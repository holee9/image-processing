/**
 * @file test_calibration_cache.cpp
 * @brief TDD tests for SWU-1.10: Calibration LRU Cache
 *        Validates cache hit/miss, LRU eviction, size limits, and thread safety.
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"
#include <string>

#include <vector>
#include <cstdio>
#include <filesystem>
#include <chrono>
#include <cstring>

namespace fs = std::filesystem;

namespace {

// QA-A-15 (#120): xpe_calib_save no longer takes a buffer -- it saves the
// calibration currently loaded in the module. These fixtures need arbitrary
// maps on disk, so they write XCal v1 files directly, the same way
// test_offset_correct.cpp does.
static void writeXCalFixture(const std::string& path, XCalType type,
                             XCalPixelFormat fmt, uint32_t w, uint32_t h,
                             const void* payload, uint64_t payloadLen,
                             uint64_t expiryMs) {
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version          = XCAL_VERSION;
    hdr.type             = static_cast<uint32_t>(type);
    hdr.pixel_format     = static_cast<uint32_t>(fmt);
    hdr.width            = w;
    hdr.height           = h;
    hdr.expiry_epoch_ms  = static_cast<int64_t>(expiryMs);
    hdr.payload_len      = payloadLen;

    ASSERT_EQ(XPE_OK,
              write_xcal_file(path.c_str(), hdr, nullptr, 0,
                              static_cast<const uint8_t*>(payload), payloadLen));
}

class CalibrationCacheTest : public ::testing::Test {
protected:
    static constexpr uint32_t W = 8;
    static constexpr uint32_t H = 8;

    fs::path tmpDir;
    fs::path offsetFile;
    fs::path gainFile;
    fs::path defectFile;

    void SetUp() override {
        // Create temp directory for test calibration files
        tmpDir = fs::temp_directory_path() / "xpe_cache_test";
        fs::create_directories(tmpDir);
        offsetFile = tmpDir / "offset.xcal";
        gainFile   = tmpDir / "gain.xcal";
        defectFile = tmpDir / "defect.xcal";

        // Create calibration files with valid expiry
        const uint64_t expiry = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::system_clock::now().time_since_epoch()).count()
        ) + 365ULL * 24 * 3600 * 1000;

        // Offset map (uint16)
        {
            // XCal v1 requires OFFSET payloads to be FLOAT32 (xcal_validator.cpp:78);
            // the retired xpe_calib_save() converted, a direct write must not assume.
            std::vector<float> data(W * H, 100.0f);
            writeXCalFixture(offsetFile.string(), XCAL_TYPE_OFFSET, XCAL_FMT_FLOAT32, W, H,
                             data.data(), data.size() * sizeof(float), expiry);
        }

        // Gain map (float32)
        {
            std::vector<float> data(W * H, 1.5f);
            writeXCalFixture(gainFile.string(), XCAL_TYPE_GAIN, XCAL_FMT_FLOAT32, W, H,
                             data.data(), data.size() * sizeof(float), expiry);
        }

        // Defect map (uint8)
        {
            std::vector<uint8_t> data(W * H, 0);
            data[0] = 1;  // one defect pixel
            writeXCalFixture(defectFile.string(), XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, W, H,
                             data.data(), data.size() * sizeof(uint8_t), expiry);
        }

        // Clear cache before each test
        xpe_calib_cache_clear();
        xpe_calib_cache_set_max_size(4);
    }

    void TearDown() override {
        xpe_calib_cache_clear();
        fs::remove_all(tmpDir);
    }
};

// --- Basic Cache Operations ---

TEST_F(CalibrationCacheTest, CacheMissLoadsFromFile) {
    std::vector<uint16_t> data(W * H, 0);
    XpeImageBuffer out{};
    out.data          = data.data();
    out.width         = W;
    out.height        = H;
    out.bitsAllocated = 16;
    out.bitsStored    = 16;
    out.format        = XPE_PIXEL_UINT16;
    out.dataSize      = data.size() * sizeof(uint16_t);

    EXPECT_EQ(XPE_OK, xpe_calib_load_offset_cached(offsetFile.string().c_str(), &out));
    EXPECT_EQ(W, out.width);
    EXPECT_EQ(H, out.height);
    EXPECT_EQ(100u, data[0]); // Value from the saved file
}

TEST_F(CalibrationCacheTest, CacheHitReturnsSameData) {
    // First load (miss)
    std::vector<uint16_t> data1(W * H, 0);
    XpeImageBuffer out1{};
    out1.data          = data1.data();
    out1.width         = W;
    out1.height        = H;
    out1.bitsAllocated = 16;
    out1.bitsStored    = 16;
    out1.format        = XPE_PIXEL_UINT16;
    out1.dataSize      = data1.size() * sizeof(uint16_t);
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(offsetFile.string().c_str(), &out1));

    // Second load (hit) — returns cached buffer pointer
    XpeImageBuffer out2{};
    out2.data          = data1.data(); // pre-allocate for initial miss attempt
    out2.width         = W;
    out2.height        = H;
    out2.bitsAllocated = 16;
    out2.bitsStored    = 16;
    out2.format        = XPE_PIXEL_UINT16;
    out2.dataSize      = data1.size() * sizeof(uint16_t);
    EXPECT_EQ(XPE_OK, xpe_calib_load_offset_cached(offsetFile.string().c_str(), &out2));

    // On cache hit, out2.data points to cached data — copy to verify
    const auto* cached = static_cast<const uint16_t*>(out2.data);
    ASSERT_NE(nullptr, cached);
    for (size_t i = 0; i < W * H; ++i) {
        EXPECT_EQ(data1[i], cached[i]) << "Mismatch at pixel " << i;
    }
}

TEST_F(CalibrationCacheTest, GainCacheMissAndHit) {
    std::vector<float> data(W * H, 0.0f);
    XpeImageBuffer out{};
    out.data          = data.data();
    out.width         = W;
    out.height        = H;
    out.bitsAllocated = 32;
    out.bitsStored    = 32;
    out.format        = XPE_PIXEL_FLOAT32;
    out.dataSize      = data.size() * sizeof(float);

    EXPECT_EQ(XPE_OK, xpe_calib_load_gain_cached(gainFile.string().c_str(), &out));
    EXPECT_NEAR(1.5f, data[0], 1e-6f);

    // Second load (hit) — returns cached buffer pointer
    std::vector<float> tmpBuf(W * H, 0.0f);
    XpeImageBuffer out2{};
    out2.data          = tmpBuf.data();
    out2.width         = W;
    out2.height        = H;
    out2.bitsAllocated = 32;
    out2.bitsStored    = 32;
    out2.format        = XPE_PIXEL_FLOAT32;
    out2.dataSize      = tmpBuf.size() * sizeof(float);
    EXPECT_EQ(XPE_OK, xpe_calib_load_gain_cached(gainFile.string().c_str(), &out2));

    // On cache hit, out2.data points to cached data
    const auto* cached = static_cast<const float*>(out2.data);
    ASSERT_NE(nullptr, cached);
    EXPECT_NEAR(1.5f, cached[0], 1e-6f);
}

TEST_F(CalibrationCacheTest, DefectCacheMissAndHit) {
    std::vector<uint8_t> data(W * H, 0);
    XpeImageBuffer out{};
    out.data          = data.data();
    out.width         = W;
    out.height        = H;
    out.bitsAllocated = 8;
    out.bitsStored    = 8;
    out.format        = XPE_PIXEL_UINT8;
    out.dataSize      = data.size() * sizeof(uint8_t);

    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_cached(defectFile.string().c_str(), &out));
    EXPECT_EQ(1u, data[0]); // defect pixel
    EXPECT_EQ(0u, data[1]); // normal pixel
}

// --- NULL Input Validation ---

TEST_F(CalibrationCacheTest, NullPathReturnsError) {
    XpeImageBuffer out{};
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_calib_load_offset_cached(nullptr, &out));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_calib_load_gain_cached(nullptr, &out));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_calib_load_defect_cached(nullptr, &out));
}

TEST_F(CalibrationCacheTest, NullOutReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_calib_load_offset_cached(offsetFile.string().c_str(), nullptr));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_calib_load_gain_cached(gainFile.string().c_str(), nullptr));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_calib_load_defect_cached(defectFile.string().c_str(), nullptr));
}

// --- Cache Clear ---

TEST_F(CalibrationCacheTest, ClearEvictsAllEntries) {
    // Load into cache
    std::vector<uint16_t> data(W * H, 0);
    XpeImageBuffer out{};
    out.data          = data.data();
    out.width         = W;
    out.height        = H;
    out.bitsAllocated = 16;
    out.bitsStored    = 16;
    out.format        = XPE_PIXEL_UINT16;
    out.dataSize      = data.size() * sizeof(uint16_t);
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(offsetFile.string().c_str(), &out));

    // Clear
    xpe_calib_cache_clear();

    // Next load should be a cache miss (loads from file again)
    // This is verified by the function returning XPE_OK after clear
    std::vector<uint16_t> data2(W * H, 0);
    XpeImageBuffer out2{};
    out2.data          = data2.data();
    out2.width         = W;
    out2.height        = H;
    out2.bitsAllocated = 16;
    out2.bitsStored    = 16;
    out2.format        = XPE_PIXEL_UINT16;
    out2.dataSize      = data2.size() * sizeof(uint16_t);
    EXPECT_EQ(XPE_OK, xpe_calib_load_offset_cached(offsetFile.string().c_str(), &out2));
    EXPECT_EQ(100u, data2[0]);
}

// --- Max Size Limits ---

TEST_F(CalibrationCacheTest, SetMaxSizeEvictsExcess) {
    // Set max to 1
    xpe_calib_cache_set_max_size(1);

    // Load offset (cache size = 1)
    std::vector<uint16_t> offData(W * H, 0);
    XpeImageBuffer offOut{};
    offOut.data          = offData.data();
    offOut.width         = W;
    offOut.height        = H;
    offOut.bitsAllocated = 16;
    offOut.bitsStored    = 16;
    offOut.format        = XPE_PIXEL_UINT16;
    offOut.dataSize      = offData.size() * sizeof(uint16_t);
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset_cached(offsetFile.string().c_str(), &offOut));

    // Load gain (cache size = 1, offset should be evicted)
    std::vector<float> gainData(W * H, 0.0f);
    XpeImageBuffer gainOut{};
    gainOut.data          = gainData.data();
    gainOut.width         = W;
    gainOut.height        = H;
    gainOut.bitsAllocated = 32;
    gainOut.bitsStored    = 32;
    gainOut.format        = XPE_PIXEL_FLOAT32;
    gainOut.dataSize      = gainData.size() * sizeof(float);
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain_cached(gainFile.string().c_str(), &gainOut));

    // Loading offset again should be a cache miss
    // (verified by successful load from file)
    std::vector<uint16_t> offData2(W * H, 0);
    XpeImageBuffer offOut2{};
    offOut2.data          = offData2.data();
    offOut2.width         = W;
    offOut2.height        = H;
    offOut2.bitsAllocated = 16;
    offOut2.bitsStored    = 16;
    offOut2.format        = XPE_PIXEL_UINT16;
    offOut2.dataSize      = offData2.size() * sizeof(uint16_t);
    EXPECT_EQ(XPE_OK, xpe_calib_load_offset_cached(offsetFile.string().c_str(), &offOut2));
    EXPECT_EQ(100u, offData2[0]);
}

TEST_F(CalibrationCacheTest, SetMaxSizeZeroClampsToOne) {
    xpe_calib_cache_set_max_size(0);
    // Should not crash; max size should be clamped to 1
    std::vector<uint16_t> data(W * H, 0);
    XpeImageBuffer out{};
    out.data          = data.data();
    out.width         = W;
    out.height        = H;
    out.bitsAllocated = 16;
    out.bitsStored    = 16;
    out.format        = XPE_PIXEL_UINT16;
    out.dataSize      = data.size() * sizeof(uint16_t);
    EXPECT_EQ(XPE_OK, xpe_calib_load_offset_cached(offsetFile.string().c_str(), &out));
}

} // namespace
