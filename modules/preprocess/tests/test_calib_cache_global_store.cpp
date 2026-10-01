/**
 * @file test_calib_cache_global_store.cpp
 * @brief Cached loaders and the module-global calibration store (QA-A-193, #216).
 *
 * Decided 2026-10-01 (#216):
 *   (1) A cache HIT must leave the module-global calibration store holding that map, exactly as a
 *       miss does, so that xpe_offset_correct / xpe_gain_correct / xpe_defect_correct called right
 *       after a successful *_cached load do not report XPE_ERR_CALIB_NOT_LOADED.
 *   (2) xpe_preprocess_shutdown() empties the cache, as api-spec.md section 6 says
 *       ("valid until ... module shutdown").
 *   (3) The cache key is the path string. A file that changes on disk is not noticed until
 *       xpe_calib_cache_clear() (or shutdown). No code change; pinned here as the contract.
 *
 * How a hit is told from a miss without peeking inside the cache: a map is loaded through the cache
 * from path A, then a DIFFERENT map is loaded straight into the global store from path B with the
 * plain loader, then A is requested through the cache again. That second request is a hit (the cache
 * still holds A) and the global store holds B -- so the correction that follows shows whose map the
 * store holds.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr uint32_t W = 4, H = 4;
constexpr size_t N = static_cast<size_t>(W) * H;

void writeMap(const char* path, uint32_t type, uint32_t fmt, const void* data, size_t bytes) {
    std::remove(path);
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version = XCAL_VERSION; hdr.type = type; hdr.pixel_format = fmt;
    hdr.width = W; hdr.height = H; hdr.payload_len = bytes;
    ASSERT_EQ(XPE_OK, write_xcal_file(path, hdr, nullptr, 0, static_cast<const uint8_t*>(data), bytes));
}
void writeOffset(const char* path, float v) {
    std::vector<float> m(N, v);
    writeMap(path, XCAL_TYPE_OFFSET, XCAL_FMT_FLOAT32, m.data(), m.size() * sizeof(float));
}
void writeGain(const char* path, float v) {
    std::vector<float> m(N, v);
    writeMap(path, XCAL_TYPE_GAIN, XCAL_FMT_FLOAT32, m.data(), m.size() * sizeof(float));
}
/** Defect map with exactly one defective pixel at (1,1) when `withDefect`, otherwise none. */
void writeDefect(const char* path, bool withDefect) {
    std::vector<uint8_t> m(N, 0);
    if (withDefect) m[1 * W + 1] = 1;
    writeMap(path, XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, m.data(), m.size());
}

XpeImageBuffer buf(void* d, XpePixelFormat f, uint32_t bits) {
    XpeImageBuffer b{};
    b.data = d; b.width = W; b.height = H; b.bitsAllocated = bits; b.bitsStored = bits; b.format = f;
    b.dataSize = static_cast<uint32_t>(N * (bits / 8));
    return b;
}

/** Offset-corrects a constant 1000 frame; returns the code and the first output pixel. */
XpeErrorCode offsetCorrect(uint16_t* first) {
    std::vector<uint16_t> in(N, 1000), out(N, 0);
    XpeImageBuffer i = buf(in.data(), XPE_PIXEL_UINT16, 16), o = buf(out.data(), XPE_PIXEL_UINT16, 16);
    XpeImageMetadata meta{};
    const XpeErrorCode rc = xpe_offset_correct(&i, &o, &meta);
    if (first) *first = out[0];
    return rc;
}
XpeErrorCode gainCorrect(float* first) {
    std::vector<uint16_t> in(N, 1000);
    std::vector<float> out(N, -1.0f);
    XpeImageBuffer i = buf(in.data(), XPE_PIXEL_UINT16, 16), o = buf(out.data(), XPE_PIXEL_FLOAT32, 32);
    XpeImageMetadata meta{};
    const XpeErrorCode rc = xpe_gain_correct(&i, &o, &meta);
    if (first) *first = out[0];
    return rc;
}
/** Defect-corrects a frame of 1000 with a 5000 spike at (1,1); returns the code and pixel (1,1). */
XpeErrorCode defectCorrect(float* spikePixel) {
    std::vector<float> in(N, 1000.0f), out(N, -1.0f);
    in[1 * W + 1] = 5000.0f;
    XpeImageBuffer i = buf(in.data(), XPE_PIXEL_FLOAT32, 32), o = buf(out.data(), XPE_PIXEL_FLOAT32, 32);
    XpeImageMetadata meta{};
    const XpeErrorCode rc = xpe_defect_correct(&i, &o, &meta);
    if (spikePixel) *spikePixel = out[1 * W + 1];
    return rc;
}

class CacheGlobalStore : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        xpe_calib_cache_clear();
    }
    void TearDown() override {
        xpe_calib_cache_clear();
        for (const char* p : {"cgs_a.xcal", "cgs_b.xcal", "cgs_c.xcal"}) {
            std::remove(p);
            std::remove((std::string(p) + ".tmp").c_str());
        }
        xpe_clear_alerts();
        xpe_preprocess_shutdown();
    }
};

} // namespace

// (1) offset ---------------------------------------------------------------
TEST_F(CacheGlobalStore, OffsetCacheHitLeavesThatMapInTheGlobalStore) {
    writeOffset("cgs_a.xcal", 100.0f);
    writeOffset("cgs_b.xcal", 300.0f);
    XpeImageBuffer v{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached("cgs_a.xcal", &v));      // miss: caches A, store = A
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset("cgs_b.xcal"));                 // store = B, cache still holds A
    uint16_t px = 0;
    ASSERT_EQ(XPE_OK, offsetCorrect(&px));
    ASSERT_EQ(700u, px) << "precondition: the store holds B (1000 - 300)";

    XpeImageBuffer hit{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached("cgs_a.xcal", &hit));    // HIT
    EXPECT_EQ(v.data, hit.data) << "precondition: this was a cache hit (same cache-owned pointer)";
    ASSERT_EQ(XPE_OK, offsetCorrect(&px)) << "a successful cached load must leave a map the correction can use";
    EXPECT_EQ(900u, px) << "after the hit the store must hold A (1000 - 100), not B";
}

// (1) gain -----------------------------------------------------------------
TEST_F(CacheGlobalStore, GainCacheHitLeavesThatMapInTheGlobalStore) {
    writeGain("cgs_a.xcal", 2.0f);
    writeGain("cgs_b.xcal", 4.0f);
    XpeImageBuffer v{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain_cached("cgs_a.xcal", &v));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("cgs_b.xcal"));
    float px = 0.0f;
    ASSERT_EQ(XPE_OK, gainCorrect(&px));
    ASSERT_NEAR(250.0f, px, 0.01f) << "precondition: the store holds B (1000 / 4)";

    XpeImageBuffer hit{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain_cached("cgs_a.xcal", &hit));      // HIT
    EXPECT_EQ(v.data, hit.data) << "precondition: this was a cache hit";
    ASSERT_EQ(XPE_OK, gainCorrect(&px));
    EXPECT_NEAR(500.0f, px, 0.01f) << "after the hit the store must hold A (1000 / 2), not B";
}

// (1) defect ---------------------------------------------------------------
TEST_F(CacheGlobalStore, DefectCacheHitLeavesThatMapInTheGlobalStore) {
    writeDefect("cgs_a.xcal", true);     // A flags (1,1)
    writeDefect("cgs_b.xcal", false);    // B flags nothing
    XpeImageBuffer v{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_cached("cgs_a.xcal", &v));
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map("cgs_b.xcal"));
    float px = 0.0f;
    ASSERT_EQ(XPE_OK, defectCorrect(&px));
    ASSERT_FLOAT_EQ(5000.0f, px) << "precondition: the store holds B, the spike is not repaired";

    XpeImageBuffer hit{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_cached("cgs_a.xcal", &hit));    // HIT
    EXPECT_EQ(v.data, hit.data) << "precondition: this was a cache hit";
    ASSERT_EQ(XPE_OK, defectCorrect(&px));
    EXPECT_NEAR(1000.0f, px, 0.01f) << "after the hit the store must hold A, which repairs the spike at (1,1)";
}

// (1) the scenario from the decision: shutdown + init, then the cached load, then the correction ---
TEST_F(CacheGlobalStore, AfterShutdownAndInitACachedLoadLeavesAMapTheCorrectionCanUse) {
    writeOffset("cgs_a.xcal", 100.0f);
    writeGain("cgs_b.xcal", 2.0f);
    writeDefect("cgs_c.xcal", false);
    XpeImageBuffer v{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached("cgs_a.xcal", &v));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain_cached("cgs_b.xcal", &v));
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_cached("cgs_c.xcal", &v));

    xpe_preprocess_shutdown();
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));

    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached("cgs_a.xcal", &v));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain_cached("cgs_b.xcal", &v));
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_cached("cgs_c.xcal", &v));
    uint16_t o = 0; float g = 0.0f, d = 0.0f;
    EXPECT_EQ(XPE_OK, offsetCorrect(&o)) << "offset: a successful cached load after init must not leave CALIB_NOT_LOADED";
    EXPECT_EQ(XPE_OK, gainCorrect(&g)) << "gain";
    EXPECT_EQ(XPE_OK, defectCorrect(&d)) << "defect";
}

// (2) shutdown empties the cache ------------------------------------------
TEST_F(CacheGlobalStore, ShutdownEmptiesTheCache) {
    writeOffset("cgs_a.xcal", 100.0f);
    XpeImageBuffer v{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached("cgs_a.xcal", &v));
    ASSERT_FLOAT_EQ(100.0f, static_cast<const float*>(v.data)[0]);

    writeOffset("cgs_a.xcal", 300.0f);       // same path, new content, written while the cache holds the old map
    xpe_preprocess_shutdown();
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));

    XpeImageBuffer after{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached("cgs_a.xcal", &after));
    EXPECT_FLOAT_EQ(300.0f, static_cast<const float*>(after.data)[0])
        << "api-spec.md section 6: a cache-owned view is valid until module shutdown, so a load after "
           "shutdown must read the file again (100 here means the old entry survived)";
}

// (3) the key is the path string; a changed file is not noticed until the cache is cleared --------
TEST_F(CacheGlobalStore, ACacheKeyIsThePathStringSoAChangedFileNeedsCacheClear) {
    writeOffset("cgs_a.xcal", 100.0f);
    XpeImageBuffer v{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached("cgs_a.xcal", &v));
    ASSERT_FLOAT_EQ(100.0f, static_cast<const float*>(v.data)[0]);

    writeOffset("cgs_a.xcal", 300.0f);
    XpeImageBuffer stale{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached("cgs_a.xcal", &stale));
    EXPECT_FLOAT_EQ(100.0f, static_cast<const float*>(stale.data)[0])
        << "the key is the path string: the rewritten file is not noticed";

    xpe_calib_cache_clear();
    XpeImageBuffer fresh{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached("cgs_a.xcal", &fresh));
    EXPECT_FLOAT_EQ(300.0f, static_cast<const float*>(fresh.data)[0])
        << "xpe_calib_cache_clear() makes the next load read the file";
}
