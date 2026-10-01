/**
 * @file test_calib_cache_same_verdict.cpp
 * @brief A cache hit must reach the same verdict as a miss (QA-A-196, #216).
 *
 * QA-A-193 made a hit leave the module-global calibration store holding the cached map. QA-A-195
 * then measured what that opened: a map cached before its file expired, or before the file was
 * tampered with, was installed by the hit although a miss would have refused it (-5 / -4), and the
 * gain quality metadata reported another calibration's values. Rule (leader, 2026-10-01): a hit
 * gives the verdict a miss would give.
 *
 * Expiry is re-checked on every hit from the expiry stored in the entry. Integrity is guarded by
 * the file's size and last-write time: when either differs from what the entry recorded, the hit is
 * cancelled and the miss path reloads (and so re-hashes) the file. A change that keeps both the size
 * and the last-write time is NOT detected -- the cache never re-hashes on a hit; the test pinning that
 * limit is below.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#else
#  include <sys/stat.h>
#  include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

constexpr uint32_t W = 4, H = 4;
constexpr size_t N = static_cast<size_t>(W) * H;

int64_t nowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void writeFile(const char* path, uint32_t type, uint32_t fmt, const void* data, size_t bytes,
               int64_t expiryMs = 0, const std::string& json = std::string()) {
    std::remove(path);
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version = XCAL_VERSION; hdr.type = type; hdr.pixel_format = fmt;
    hdr.width = W; hdr.height = H; hdr.payload_len = bytes;
    hdr.created_epoch_ms = nowMs(); hdr.expiry_epoch_ms = expiryMs;
    ASSERT_EQ(XPE_OK, write_xcal_file(path, hdr, reinterpret_cast<const uint8_t*>(json.data()), json.size(),
                                      static_cast<const uint8_t*>(data), bytes));
}
void writeOffset(const char* path, float v, int64_t expiryMs = 0) {
    std::vector<float> m(N, v);
    writeFile(path, XCAL_TYPE_OFFSET, XCAL_FMT_FLOAT32, m.data(), m.size() * sizeof(float), expiryMs);
}
void writeGain(const char* path, float v, int64_t expiryMs = 0, const std::string& json = std::string()) {
    std::vector<float> m(N, v);
    writeFile(path, XCAL_TYPE_GAIN, XCAL_FMT_FLOAT32, m.data(), m.size() * sizeof(float), expiryMs, json);
}
void writeDefect(const char* path, bool withDefect, int64_t expiryMs = 0) {
    std::vector<uint8_t> m(N, 0);
    if (withDefect) m[1 * W + 1] = 1;
    writeFile(path, XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, m.data(), m.size(), expiryMs);
}
void flipLastByte(const char* path) {
    std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
    f.seekg(-1, std::ios::end);
    char c = 0;
    f.read(&c, 1);
    c = static_cast<char>(c ^ 0x5A);
    f.seekp(-1, std::ios::end);
    f.write(&c, 1);
}

XpeImageBuffer buf(void* d, XpePixelFormat f, uint32_t bits) {
    XpeImageBuffer b{};
    b.data = d; b.width = W; b.height = H; b.bitsAllocated = bits; b.bitsStored = bits; b.format = f;
    b.dataSize = static_cast<uint32_t>(N * (bits / 8));
    return b;
}
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
XpeErrorCode defectCorrect(float* spikePixel) {
    std::vector<float> in(N, 1000.0f), out(N, -1.0f);
    in[1 * W + 1] = 5000.0f;
    XpeImageBuffer i = buf(in.data(), XPE_PIXEL_FLOAT32, 32), o = buf(out.data(), XPE_PIXEL_FLOAT32, 32);
    XpeImageMetadata meta{};
    const XpeErrorCode rc = xpe_defect_correct(&i, &o, &meta);
    if (spikePixel) *spikePixel = out[1 * W + 1];
    return rc;
}

const char* kFiles[] = {"csv_oe.xcal", "csv_ge.xcal", "csv_de.xcal", "csv_oq.xcal", "csv_gq.xcal",
                        "csv_dq.xcal", "csv_a.xcal", "csv_poly.xcal", "csv_x.xcal"};

class CacheSameVerdict : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        xpe_calib_cache_clear();
    }
    void TearDown() override {
        xpe_calib_cache_clear();
        for (const char* p : kFiles) {
            std::remove(p);
            std::remove((std::string(p) + ".tmp").c_str());
        }
        xpe_clear_alerts();
        xpe_preprocess_shutdown();
    }
};

} // namespace

// (1) expiry --------------------------------------------------------------------------------------
TEST_F(CacheSameVerdict, AMapCachedBeforeItsFileExpiredIsRefusedLikeAMissAndNotInstalled) {
    const int64_t expiry = nowMs() + 800;
    writeOffset("csv_oe.xcal", 100.0f, expiry);
    writeGain("csv_ge.xcal", 2.0f, expiry);
    writeDefect("csv_de.xcal", true, expiry);
    writeOffset("csv_oq.xcal", 300.0f);
    writeGain("csv_gq.xcal", 4.0f);
    writeDefect("csv_dq.xcal", false);

    XpeImageBuffer v{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached("csv_oe.xcal", &v));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain_cached("csv_ge.xcal", &v));
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_cached("csv_de.xcal", &v));
    // The store now holds the Q maps, so "not installed" is observable.
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset("csv_oq.xcal"));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("csv_gq.xcal"));
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map("csv_dq.xcal"));

    std::this_thread::sleep_for(std::chrono::milliseconds(1100));

    // Control: the plain loaders (the miss path) refuse these files as expired.
    ASSERT_EQ(XPE_ERR_CALIBRATION_EXPIRED, xpe_calib_load_offset("csv_oe.xcal")) << "control: file is expired";
    ASSERT_EQ(XPE_ERR_CALIBRATION_EXPIRED, xpe_calib_load_gain("csv_ge.xcal")) << "control: file is expired";
    ASSERT_EQ(XPE_ERR_CALIBRATION_EXPIRED, xpe_calib_load_defect_map("csv_de.xcal")) << "control: file is expired";
    // The control loads were refused, but re-establish the Q maps in case a loader touched the store.
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset("csv_oq.xcal"));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("csv_gq.xcal"));
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map("csv_dq.xcal"));

    XpeImageBuffer hit{};
    EXPECT_EQ(XPE_ERR_CALIBRATION_EXPIRED, xpe_calib_load_offset_cached("csv_oe.xcal", &hit));
    EXPECT_EQ(XPE_ERR_CALIBRATION_EXPIRED, xpe_calib_load_gain_cached("csv_ge.xcal", &hit));
    EXPECT_EQ(XPE_ERR_CALIBRATION_EXPIRED, xpe_calib_load_defect_cached("csv_de.xcal", &hit));

    uint16_t o = 0; float g = 0.0f, d = 0.0f;
    ASSERT_EQ(XPE_OK, offsetCorrect(&o));
    ASSERT_EQ(XPE_OK, gainCorrect(&g));
    ASSERT_EQ(XPE_OK, defectCorrect(&d));
    EXPECT_EQ(700, o) << "the refused hit must leave the offset Q map (1000 - 300) in the store";
    EXPECT_NEAR(250.0f, g, 0.01f) << "the refused hit must leave the gain Q map (1000 / 4) in the store";
    EXPECT_NEAR(5000.0f, d, 0.01f) << "the refused hit must leave the defect Q map (no defects) in the store";
}

// (2) integrity ----------------------------------------------------------------------------------
TEST_F(CacheSameVerdict, AMapCachedBeforeItsFileWasTamperedWithIsRefusedLikeAMiss) {
    writeOffset("csv_oe.xcal", 100.0f);
    writeGain("csv_ge.xcal", 2.0f);
    writeDefect("csv_de.xcal", true);
    writeOffset("csv_oq.xcal", 300.0f);
    writeGain("csv_gq.xcal", 4.0f);
    writeDefect("csv_dq.xcal", false);

    XpeImageBuffer v{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached("csv_oe.xcal", &v));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain_cached("csv_ge.xcal", &v));
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_cached("csv_de.xcal", &v));

    // A later write time is what a real tamper leaves; the coarse system clock needs a moment.
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    flipLastByte("csv_oe.xcal");
    flipLastByte("csv_ge.xcal");
    flipLastByte("csv_de.xcal");

    // Control: the plain loaders (the miss path) refuse the tampered files; remember their verdict.
    const XpeErrorCode missO = xpe_calib_load_offset("csv_oe.xcal");
    const XpeErrorCode missG = xpe_calib_load_gain("csv_ge.xcal");
    const XpeErrorCode missD = xpe_calib_load_defect_map("csv_de.xcal");
    ASSERT_NE(XPE_OK, missO) << "control: the tampered offset file must fail the plain loader";
    ASSERT_NE(XPE_OK, missG) << "control: the tampered gain file must fail the plain loader";
    ASSERT_NE(XPE_OK, missD) << "control: the tampered defect file must fail the plain loader";

    ASSERT_EQ(XPE_OK, xpe_calib_load_offset("csv_oq.xcal"));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("csv_gq.xcal"));
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map("csv_dq.xcal"));

    XpeImageBuffer hit{};
    EXPECT_EQ(missO, xpe_calib_load_offset_cached("csv_oe.xcal", &hit));
    EXPECT_EQ(missG, xpe_calib_load_gain_cached("csv_ge.xcal", &hit));
    EXPECT_EQ(missD, xpe_calib_load_defect_cached("csv_de.xcal", &hit));

    uint16_t o = 0; float g = 0.0f, d = 0.0f;
    ASSERT_EQ(XPE_OK, offsetCorrect(&o));
    ASSERT_EQ(XPE_OK, gainCorrect(&g));
    ASSERT_EQ(XPE_OK, defectCorrect(&d));
    EXPECT_EQ(700, o);
    EXPECT_NEAR(250.0f, g, 0.01f);
    EXPECT_NEAR(5000.0f, d, 0.01f);
}

// (3) a file that changed in size or write time is reloaded, not served from the cache ---------------
TEST_F(CacheSameVerdict, AFileWhoseWriteTimeChangedIsReloaded) {
    writeOffset("csv_a.xcal", 100.0f);
    const auto original = fs::last_write_time("csv_a.xcal");
    XpeImageBuffer v{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached("csv_a.xcal", &v));
    ASSERT_FLOAT_EQ(100.0f, static_cast<const float*>(v.data)[0]);

    writeOffset("csv_a.xcal", 300.0f);                                        // same size, new content
    fs::last_write_time("csv_a.xcal", original + std::chrono::seconds(5));   // a distinct, later write time

    XpeImageBuffer after{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached("csv_a.xcal", &after));
    EXPECT_FLOAT_EQ(300.0f, static_cast<const float*>(after.data)[0])
        << "a changed write time cancels the hit; the miss path reads the file again (100 = stale hit)";
}

TEST_F(CacheSameVerdict, AFileWhoseSizeChangedIsReloadedEvenWithTheSameWriteTime) {
    std::vector<float> a(N, 100.0f), b(N, 300.0f);
    writeFile("csv_a.xcal", XCAL_TYPE_OFFSET, XCAL_FMT_FLOAT32, a.data(), a.size() * sizeof(float));
    const auto original = fs::last_write_time("csv_a.xcal");
    const auto sizeBefore = fs::file_size("csv_a.xcal");
    XpeImageBuffer v{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached("csv_a.xcal", &v));

    // New content AND a longer config block, so the file is bigger; the write time is put back.
    writeFile("csv_a.xcal", XCAL_TYPE_OFFSET, XCAL_FMT_FLOAT32, b.data(), b.size() * sizeof(float), 0,
              "{\"note\":\"a longer config block\"}");
    fs::last_write_time("csv_a.xcal", original);
    ASSERT_NE(sizeBefore, fs::file_size("csv_a.xcal")) << "precondition: the size really changed";
    ASSERT_TRUE(fs::last_write_time("csv_a.xcal") == original) << "precondition: the write time is the old one";

    XpeImageBuffer after{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached("csv_a.xcal", &after));
    EXPECT_FLOAT_EQ(300.0f, static_cast<const float*>(after.data)[0])
        << "a changed size cancels the hit (100 = stale hit)";
}

// (4) the documented limit ------------------------------------------------------------------------
TEST_F(CacheSameVerdict, AChangeThatKeepsBothSizeAndWriteTimeIsNotNoticedUntilCacheClear) {
    writeOffset("csv_a.xcal", 100.0f);
    const auto original = fs::last_write_time("csv_a.xcal");
    XpeImageBuffer v{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached("csv_a.xcal", &v));

    writeOffset("csv_a.xcal", 300.0f);                 // same size
    fs::last_write_time("csv_a.xcal", original);       // and the very same write time
    XpeImageBuffer stale{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached("csv_a.xcal", &stale));
    EXPECT_FLOAT_EQ(100.0f, static_cast<const float*>(stale.data)[0])
        << "a hit does not re-hash the file: same size and same write time = not noticed";

    xpe_calib_cache_clear();
    XpeImageBuffer fresh{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached("csv_a.xcal", &fresh));
    EXPECT_FLOAT_EQ(300.0f, static_cast<const float*>(fresh.data)[0]);
}

// (5) gain quality metadata ----------------------------------------------------------------------
TEST_F(CacheSameVerdict, AGainHitReportsTheSameQualityMetadataAsAMiss) {
    writeGain("csv_ge.xcal", 2.0f, 0, "{\"fit_r_squared\":\"0.900000\",\"polynomial_degree\":\"2\"}");
    writeGain("csv_gq.xcal", 4.0f, 0, "{\"fit_r_squared\":\"0.500000\",\"polynomial_degree\":\"1\"}");

    XpeImageBuffer v{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain_cached("csv_ge.xcal", &v));       // cached

    // Hit: the store was last loaded from Q.
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("csv_gq.xcal"));
    XpeImageBuffer hit{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain_cached("csv_ge.xcal", &hit));
    ASSERT_EQ(v.data, hit.data) << "precondition: this was a cache hit";
    XpeCalibQualityMeta afterHit{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&afterHit));

    // Miss: the same preceding state, but the entry is gone.
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("csv_gq.xcal"));
    xpe_calib_cache_clear();
    XpeImageBuffer miss{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain_cached("csv_ge.xcal", &miss));
    XpeCalibQualityMeta afterMiss{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&afterMiss));

    EXPECT_NEAR(0.9, afterMiss.r_squared, 1e-9) << "control: the miss reads the file's r_squared";
    EXPECT_EQ(2u, afterMiss.polynomial_degree) << "control: the miss reads the file's degree";
    EXPECT_EQ(0, std::memcmp(&afterMiss, &afterHit, sizeof afterMiss))
        << "hit r2=" << afterHit.r_squared << " degree=" << int(afterHit.polynomial_degree)
        << " vs miss r2=" << afterMiss.r_squared << " degree=" << int(afterMiss.polynomial_degree);
}

// (6) a gain polynomial file ---------------------------------------------------------------------
TEST_F(CacheSameVerdict, AGainPolynomialFileLoadsAndTheCachedLoaderReportsSuccess) {
    std::vector<float> coeffs(N * 2);
    for (size_t p = 0; p < N; ++p) { coeffs[p * 2] = 1.0f; coeffs[p * 2 + 1] = 0.0f; }   // pixel-major, degree 1
    writeFile("csv_poly.xcal", XCAL_TYPE_GAIN_POLY, XCAL_FMT_FLOAT32, coeffs.data(), coeffs.size() * sizeof(float));

    for (int call = 1; call <= 2; ++call) {
        XpeImageBuffer out{};
        out.data = reinterpret_cast<void*>(0x1);   // must be overwritten
        EXPECT_EQ(XPE_OK, xpe_calib_load_gain_cached("csv_poly.xcal", &out)) << "call " << call;
        EXPECT_EQ(nullptr, out.data) << "a polynomial file has no scalar map to hand back (call " << call << ")";
        float px = 0.0f;
        EXPECT_EQ(XPE_OK, gainCorrect(&px)) << "the polynomial is in the store, so the correction works (call " << call << ")";
    }
}

// (7) a map cached by one loader is not served to another --------------------------------------
// The cache key is the path string only. A file holds one kind of map, and the plain loader of
// another kind refuses it (wrong XCal type). A hit used to look at the byte count only, so a float
// offset map cached through xpe_calib_load_offset_cached went into the gain store when the same path
// was passed to xpe_calib_load_gain_cached -- past the type check and past the [0.1, 10.0] gain range.
namespace {
enum Kind { OFFSET = 0, GAIN = 1, DEFECT = 2 };
const char* kindName[] = {"offset", "gain", "defect"};
using Cached = XpeErrorCode (*)(const char*, XpeImageBuffer*);
using Plain = XpeErrorCode (*)(const char*);
const Cached kCached[] = {xpe_calib_load_offset_cached, xpe_calib_load_gain_cached, xpe_calib_load_defect_cached};
const Plain kPlain[] = {xpe_calib_load_offset, xpe_calib_load_gain, xpe_calib_load_defect_map};
void writeKind(Kind k, const char* path) {
    // Offset 100 and gain 2 on purpose: 100 is outside the gain range [0.1, 10.0].
    if (k == OFFSET) writeOffset(path, 100.0f);
    else if (k == GAIN) writeGain(path, 2.0f);
    else writeDefect(path, true);
}
}  // namespace

TEST_F(CacheSameVerdict, AMapCachedByOneLoaderIsRefusedByEveryOtherLoaderLikeAMiss) {
    for (Kind owner : {OFFSET, GAIN, DEFECT}) {
        for (Kind other : {OFFSET, GAIN, DEFECT}) {
            if (owner == other) continue;
            SCOPED_TRACE(std::string("cached by ") + kindName[owner] + ", asked by " + kindName[other]);
            xpe_calib_cache_clear();
            writeKind(owner, "csv_x.xcal");
            writeOffset("csv_oq.xcal", 300.0f);
            writeGain("csv_gq.xcal", 4.0f);
            writeDefect("csv_dq.xcal", false);

            XpeImageBuffer first{};
            ASSERT_EQ(XPE_OK, kCached[owner]("csv_x.xcal", &first));

            // Control: the plain loader of the other kind refuses this file; its code is the miss verdict.
            const XpeErrorCode missVerdict = kPlain[other]("csv_x.xcal");
            ASSERT_NE(XPE_OK, missVerdict) << "control: the file is the wrong kind for this loader";

            // The store holds the Q maps, so "not installed" is observable.
            ASSERT_EQ(XPE_OK, xpe_calib_load_offset("csv_oq.xcal"));
            ASSERT_EQ(XPE_OK, xpe_calib_load_gain("csv_gq.xcal"));
            ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map("csv_dq.xcal"));

            XpeImageBuffer wrong{};
            EXPECT_EQ(missVerdict, kCached[other]("csv_x.xcal", &wrong));

            uint16_t o = 0; float g = 0.0f, d = 0.0f;
            ASSERT_EQ(XPE_OK, offsetCorrect(&o));
            ASSERT_EQ(XPE_OK, gainCorrect(&g));
            ASSERT_EQ(XPE_OK, defectCorrect(&d));
            EXPECT_EQ(700, o) << "the refused call must leave the offset Q map in the store";
            EXPECT_NEAR(250.0f, g, 0.01f) << "the refused call must leave the gain Q map in the store";
            EXPECT_NEAR(5000.0f, d, 0.01f) << "the refused call must leave the defect Q map in the store";

            // The refusal does not evict the entry: its own loader still gets a hit.
            XpeImageBuffer again{};
            ASSERT_EQ(XPE_OK, kCached[owner]("csv_x.xcal", &again));
            EXPECT_EQ(first.data, again.data) << "the owner's entry must survive the refused call";
        }
    }
}

// (8) other checks the miss path makes, reproduced on a hit ---------------------------------------
// These three pass on arrival: they pin the table in QA-A-197's verdict (every check the plain loader
// makes, and how a hit reproduces it) so a later change cannot drop a row without a test turning red.
TEST_F(CacheSameVerdict, AFileDeletedAfterCachingIsRefusedLikeAMiss) {
    writeOffset("csv_oe.xcal", 100.0f);
    writeOffset("csv_oq.xcal", 300.0f);
    XpeImageBuffer v{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached("csv_oe.xcal", &v));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset("csv_oq.xcal"));
    ASSERT_EQ(0, std::remove("csv_oe.xcal"));

    const XpeErrorCode miss = xpe_calib_load_offset("csv_oe.xcal");
    ASSERT_EQ(XPE_ERR_IO_FAILED, miss) << "control: the plain loader cannot open a deleted file";
    XpeImageBuffer hit{};
    EXPECT_EQ(miss, xpe_calib_load_offset_cached("csv_oe.xcal", &hit));
    uint16_t o = 0;
    ASSERT_EQ(XPE_OK, offsetCorrect(&o));
    EXPECT_EQ(700, o) << "the refused call must leave the Q map in the store";
}

TEST_F(CacheSameVerdict, AHeaderCorruptedAfterCachingIsRefusedLikeAMiss) {
    writeGain("csv_ge.xcal", 2.0f);
    writeGain("csv_gq.xcal", 4.0f);
    XpeImageBuffer v{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain_cached("csv_ge.xcal", &v));
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    {
        std::fstream f("csv_ge.xcal", std::ios::in | std::ios::out | std::ios::binary);
        char c = 0;
        f.read(&c, 1);
        c = static_cast<char>(c ^ 0x5A);   // the first byte of the magic
        f.seekp(0);
        f.write(&c, 1);
    }
    const XpeErrorCode miss = xpe_calib_load_gain("csv_ge.xcal");
    ASSERT_NE(XPE_OK, miss) << "control: a broken magic is refused by the plain loader";
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("csv_gq.xcal"));
    XpeImageBuffer hit{};
    EXPECT_EQ(miss, xpe_calib_load_gain_cached("csv_ge.xcal", &hit));
    float g = 0.0f;
    ASSERT_EQ(XPE_OK, gainCorrect(&g));
    EXPECT_NEAR(250.0f, g, 0.01f) << "the refused call must leave the Q map in the store";
}

// What this pins: a scalar hit takes over from a polynomial in the store. It does NOT pin that the
// hit also clears the polynomial fields (as install_gain does, mirroring the plain loader):
// xpe_gain_correct() uses the scalar map whenever there is one, so a stale polynomial cannot be seen
// through the public API (QA-A-197 arm B4b: removing those two lines leaves every test green).
TEST_F(CacheSameVerdict, AScalarGainHitTakesOverFromAPolynomialInTheStore) {
    std::vector<float> coeffs(N * 2);
    for (size_t p = 0; p < N; ++p) { coeffs[p * 2] = 1.0f; coeffs[p * 2 + 1] = 0.0f; }
    writeFile("csv_poly.xcal", XCAL_TYPE_GAIN_POLY, XCAL_FMT_FLOAT32, coeffs.data(), coeffs.size() * sizeof(float));
    writeGain("csv_ge.xcal", 2.0f);

    XpeImageBuffer v{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain_cached("csv_ge.xcal", &v));      // cached scalar map (gain 2)
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain("csv_poly.xcal"));               // the store now holds the polynomial
    float poly = 0.0f;
    ASSERT_EQ(XPE_OK, gainCorrect(&poly));
    ASSERT_NE(500.0f, poly) << "control: with the polynomial in the store the result is not the scalar map's";

    XpeImageBuffer hit{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain_cached("csv_ge.xcal", &hit));
    ASSERT_EQ(v.data, hit.data) << "precondition: this was a cache hit";
    float g = 0.0f;
    ASSERT_EQ(XPE_OK, gainCorrect(&g));
    EXPECT_NEAR(500.0f, g, 0.01f) << "a scalar hit must put its map in the store, as a scalar load does (1000 / 2)";
}

// (9) a file whose attributes are visible but whose content cannot be opened ----------------------
// Reading the size and the write time succeeding does not mean the file can be read. A miss opens
// the file and gets IO_FAILED; a hit that only compared the attributes returned OK (Codex #20).
// Windows: a handle that denies every other reader (share mode 0) leaves the attributes readable.
// POSIX: mode 000 (not for root, which ignores it).
namespace {
class ReadDenied {
public:
    explicit ReadDenied(const char* path) : path_(path) {
#ifdef _WIN32
        h_ = CreateFileA(path, GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        ok_ = (h_ != INVALID_HANDLE_VALUE);
#else
        struct stat st {};
        if (geteuid() != 0 && ::stat(path, &st) == 0) {
            mode_ = st.st_mode & 07777;
            ok_ = (::chmod(path, 0) == 0);
        }
#endif
    }
    ~ReadDenied() { release(); }
    ReadDenied(const ReadDenied&) = delete;
    ReadDenied& operator=(const ReadDenied&) = delete;
    bool ok() const { return ok_; }
    void release() {
        if (!ok_) return;
#ifdef _WIN32
        CloseHandle(h_);
#else
        ::chmod(path_.c_str(), static_cast<mode_t>(mode_));
#endif
        ok_ = false;
    }
private:
    std::string path_;
    bool ok_{false};
#ifdef _WIN32
    HANDLE h_{INVALID_HANDLE_VALUE};
#else
    unsigned mode_{0};
#endif
};
}  // namespace

TEST_F(CacheSameVerdict, AFileWhoseAttributesAreVisibleButCannotBeOpenedIsRefusedLikeAMiss) {
    for (Kind k : {OFFSET, GAIN, DEFECT}) {
        SCOPED_TRACE(kindName[k]);
        xpe_calib_cache_clear();
        writeKind(k, "csv_x.xcal");
        writeOffset("csv_oq.xcal", 300.0f);
        writeGain("csv_gq.xcal", 4.0f);
        writeDefect("csv_dq.xcal", false);

        XpeImageBuffer first{};
        ASSERT_EQ(XPE_OK, kCached[k]("csv_x.xcal", &first));
        ASSERT_EQ(XPE_OK, xpe_calib_load_offset("csv_oq.xcal"));
        ASSERT_EQ(XPE_OK, xpe_calib_load_gain("csv_gq.xcal"));
        ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map("csv_dq.xcal"));

        ReadDenied denied("csv_x.xcal");
        if (!denied.ok()) GTEST_SKIP() << "cannot deny read access on this platform / account";

        // Controls: the attributes are still readable, and the plain loader (the miss path) fails to open.
        std::error_code ec1, ec2;
        (void)fs::file_size("csv_x.xcal", ec1);
        (void)fs::last_write_time("csv_x.xcal", ec2);
        ASSERT_FALSE(ec1) << "control: the size must stay readable: " << ec1.message();
        ASSERT_FALSE(ec2) << "control: the write time must stay readable: " << ec2.message();
        const XpeErrorCode miss = kPlain[k]("csv_x.xcal");
        ASSERT_EQ(XPE_ERR_IO_FAILED, miss) << "control: the plain loader cannot open the file";

        XpeImageBuffer refused{};
        EXPECT_EQ(miss, kCached[k]("csv_x.xcal", &refused));

        uint16_t o = 0; float g = 0.0f, d = 0.0f;
        ASSERT_EQ(XPE_OK, offsetCorrect(&o));
        ASSERT_EQ(XPE_OK, gainCorrect(&g));
        ASSERT_EQ(XPE_OK, defectCorrect(&d));
        EXPECT_EQ(700, o) << "the refused call must leave the offset Q map in the store";
        EXPECT_NEAR(250.0f, g, 0.01f) << "the refused call must leave the gain Q map in the store";
        EXPECT_NEAR(5000.0f, d, 0.01f) << "the refused call must leave the defect Q map in the store";

        // The refusal does not evict the entry: once the file can be read again, the hit is back.
        denied.release();
        XpeImageBuffer again{};
        ASSERT_EQ(XPE_OK, kCached[k]("csv_x.xcal", &again));
        EXPECT_EQ(first.data, again.data) << "the entry must survive the refused call";
    }
}
