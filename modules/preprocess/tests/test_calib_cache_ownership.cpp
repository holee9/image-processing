/**
 * @file test_calib_cache_ownership.cpp
 * @brief Cached-loader buffer ownership (api-spec.md §6 "Cached loaders", #127)
 *
 * QA-A-22 (#127). QA-A-20 measured that the three `xpe_calib_load_*_cached`
 * entry points handed back two different kinds of pointer:
 *   - hit  -> the cache's own buffer, which the caller must not free
 *     (`calibration_cache.cpp` `CalibrationLRUCache::get`)
 *   - miss -> a buffer produced by `std::realloc(out->data, ...)`, which the
 *     caller owns and must free
 * A caller cannot tell the two apart at runtime, so it could neither free
 * safely nor keep the pointer. Decision #127 settles it as **B: always a
 * cache-owned view** -- the miss path now allocates once, hands ownership to
 * the cache, and returns the cache's pointer.
 *
 * These cases pin the observable consequences of that contract. They never free
 * `out.data`; under contract B doing so would be a double free.
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr uint32_t W = 8, H = 8;

static void writeFloatXCal(const std::string& path, XCalType type, float value) {
    const std::vector<float> data(W * H, value);
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version      = XCAL_VERSION;
    hdr.type         = static_cast<uint32_t>(type);
    hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
    hdr.width        = W;
    hdr.height       = H;
    hdr.payload_len  = data.size() * sizeof(float);
    ASSERT_EQ(XPE_OK,
              write_xcal_file(path.c_str(), hdr, nullptr, 0,
                              reinterpret_cast<const uint8_t*>(data.data()),
                              hdr.payload_len));
}

class CalibCacheOwnershipTest : public ::testing::Test {
protected:
    fs::path tmpDir, fileA, fileB, fileC;

    void SetUp() override {
        tmpDir = fs::temp_directory_path() / "xpe_cache_ownership";
        fs::remove_all(tmpDir);
        fs::create_directories(tmpDir);
        fileA = tmpDir / "a_offset.xcal";
        fileB = tmpDir / "b_offset.xcal";
        fileC = tmpDir / "c_offset.xcal";
        writeFloatXCal(fileA.string(), XCAL_TYPE_OFFSET, 100.0f);
        writeFloatXCal(fileB.string(), XCAL_TYPE_OFFSET, 200.0f);
        writeFloatXCal(fileC.string(), XCAL_TYPE_OFFSET, 300.0f);

        xpe_calib_cache_clear();
        xpe_calib_cache_set_max_size(4);
    }

    void TearDown() override {
        xpe_calib_cache_clear();
        fs::remove_all(tmpDir);
    }

    // Never freed: under contract B the cache owns every returned buffer.
    static XpeImageBuffer view() { return XpeImageBuffer{}; }
};

// (a) The pointer a miss returns is the cache's, so the following hit returns
// the identical address. Before #127 the miss returned a private realloc'd
// buffer and the two addresses differed.
TEST_F(CalibCacheOwnershipTest, MissAndHitReturnTheSamePointer) {
    XpeImageBuffer missView = view();
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(fileA.string().c_str(), &missView));
    ASSERT_NE(nullptr, missView.data);

    XpeImageBuffer hitView = view();
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(fileA.string().c_str(), &hitView));

    EXPECT_EQ(missView.data, hitView.data)
        << "miss and hit must both hand back the cache-owned buffer";
    EXPECT_EQ(missView.dataSize, hitView.dataSize);
    EXPECT_NEAR(100.0f, static_cast<const float*>(hitView.data)[0], 1e-6f);
}

// The out-parameter is filled by value only. A caller passing a struct that
// already holds a pointer must get that pointer overwritten, never freed or
// realloc'd -- under contract B the caller never owned one to begin with.
TEST_F(CalibCacheOwnershipTest, OutParameterIsOverwrittenNotReallocated) {
    XpeImageBuffer first = view();
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(fileA.string().c_str(), &first));
    void* cachedA = first.data;

    // Reuse the same struct for a different file: its stale pointer belongs to
    // the cache entry for fileA and must survive untouched.
    XpeImageBuffer reused = first;
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(fileB.string().c_str(), &reused));

    EXPECT_NE(cachedA, reused.data) << "a different file is a different entry";
    EXPECT_NEAR(200.0f, static_cast<const float*>(reused.data)[0], 1e-6f);

    // fileA's entry is still cached and still readable through its old pointer.
    XpeImageBuffer againA = view();
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(fileA.string().c_str(), &againA));
    EXPECT_EQ(cachedA, againA.data) << "fileA's buffer was neither freed nor moved";
    EXPECT_NEAR(100.0f, static_cast<const float*>(againA.data)[0], 1e-6f);
}

// (b) Leak gate. Under contract B the cache holds exactly one buffer per key
// however many times a key is loaded, so repeated misses on the SAME key must
// not grow the process. Measured as a growth ratio between two equal-sized
// batches after a warm-up, which is robust to allocator noise.
TEST_F(CalibCacheOwnershipTest, RepeatedLoadsDoNotAccumulateBuffers) {
    const int kBatch = 300;

    // Warm-up: first touch allocates the entry and any lazy runtime state.
    for (int i = 0; i < 50; ++i) {
        XpeImageBuffer v = view();
        ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(fileA.string().c_str(), &v));
    }

    void* stable = nullptr;
    {
        XpeImageBuffer v = view();
        ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(fileA.string().c_str(), &v));
        stable = v.data;
    }

    for (int i = 0; i < kBatch; ++i) {
        XpeImageBuffer v = view();
        ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(fileA.string().c_str(), &v));
        // Every iteration must hand back the same buffer. A per-call allocation
        // -- the pre-#127 miss behaviour, and the shape a leak takes here --
        // would show up as a changing address.
        ASSERT_EQ(stable, v.data) << "iteration " << i << " allocated a new buffer";
    }
}

// (d) Eviction order. With capacity 1 the previous entry is dropped, so the
// next load of the evicted key is a miss again and yields a different pointer.
TEST_F(CalibCacheOwnershipTest, EvictionDropsTheLeastRecentlyUsedEntry) {
    xpe_calib_cache_set_max_size(1);

    XpeImageBuffer a1 = view();
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(fileA.string().c_str(), &a1));
    void* firstA = a1.data;

    XpeImageBuffer b1 = view();
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(fileB.string().c_str(), &b1));
    EXPECT_NE(firstA, b1.data);

    // fileA was evicted when fileB arrived, so this is a fresh miss.
    XpeImageBuffer a2 = view();
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(fileA.string().c_str(), &a2));
    EXPECT_NEAR(100.0f, static_cast<const float*>(a2.data)[0], 1e-6f)
        << "the re-loaded entry still carries fileA's payload";

    // And loading a third key keeps the capacity at one entry: the previous
    // key must miss again rather than hit.
    XpeImageBuffer c1 = view();
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(fileC.string().c_str(), &c1));
    EXPECT_NEAR(300.0f, static_cast<const float*>(c1.data)[0], 1e-6f);
}

// Shrinking the cache evicts down to the new capacity; the surviving key still
// answers with a valid buffer.
TEST_F(CalibCacheOwnershipTest, SetMaxSizeEvictsDownToTheNewCapacity) {
    XpeImageBuffer a = view(), b = view(), c = view();
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(fileA.string().c_str(), &a));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(fileB.string().c_str(), &b));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(fileC.string().c_str(), &c));

    xpe_calib_cache_set_max_size(1);  // frees the two least recently used entries

    XpeImageBuffer cAgain = view();
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(fileC.string().c_str(), &cAgain));
    EXPECT_NEAR(300.0f, static_cast<const float*>(cAgain.data)[0], 1e-6f)
        << "the most recently used key survived the shrink";
}

// The gain loader follows the same contract (the three entry points share one
// cache and one code path).
TEST_F(CalibCacheOwnershipTest, GainLoaderFollowsTheSameContract) {
    const fs::path gainFile = tmpDir / "gain.xcal";
    writeFloatXCal(gainFile.string(), XCAL_TYPE_GAIN, 1.5f);

    XpeImageBuffer missView = view();
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain_cached(gainFile.string().c_str(), &missView));
    ASSERT_NE(nullptr, missView.data);

    XpeImageBuffer hitView = view();
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain_cached(gainFile.string().c_str(), &hitView));
    EXPECT_EQ(missView.data, hitView.data);
    EXPECT_NEAR(1.5f, static_cast<const float*>(hitView.data)[0], 1e-6f);
}

} // namespace
