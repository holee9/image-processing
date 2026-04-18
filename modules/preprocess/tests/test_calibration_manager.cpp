/**
 * @file test_calibration_manager.cpp
 * @brief TDD RED tests for SWU-1.5: Calibration Manager (REQ-P1A-035 to REQ-P1A-040)
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include <gtest/gtest.h>
#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <vector>
#include <cstdio>
#include <filesystem>
#include <chrono>
#include <cstring>

namespace fs = std::filesystem;

namespace {

class CalibManagerTest : public ::testing::Test {
protected:
    static constexpr uint32_t W = 8;
    static constexpr uint32_t H = 8;

    fs::path tmpFile;
    std::vector<float> calibData;
    XpeImageBuffer calibMap{};

    void SetUp() override {
        calibData.assign(W * H, 1.0f);
        calibMap.data          = calibData.data();
        calibMap.width         = W;
        calibMap.height        = H;
        calibMap.bitsAllocated = 32;
        calibMap.bitsStored    = 32;
        calibMap.format        = XPE_PIXEL_FLOAT32;
        calibMap.dataSize      = calibData.size() * sizeof(float);

        tmpFile = fs::temp_directory_path() / "xpe_calib_test.xpec";
    }

    void TearDown() override {
        fs::remove(tmpFile);
    }
};

// NULL filePath inputs
TEST_F(CalibManagerTest, LoadOffsetNullPathReturnsError) {
    XpeImageBuffer out{};
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_calib_load_offset(nullptr, &out));
}

TEST_F(CalibManagerTest, LoadOffsetNullOutReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_calib_load_offset("/nonexistent.xpec", nullptr));
}

// Non-existent file returns IO error
TEST_F(CalibManagerTest, LoadOffsetNonExistentFileReturnsIoError) {
    XpeImageBuffer out{};
    EXPECT_EQ(XPE_ERR_IO_FAILED,
              xpe_calib_load_offset("/nonexistent_path/file.xpec", &out));
}

// Round-trip: save then load
TEST_F(CalibManagerTest, SaveAndLoadRoundTrip) {
    // Set expiry 1 year from now (approx)
    const uint64_t expiry = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()
    ) + 365ULL * 24 * 3600 * 1000;

    ASSERT_EQ(XPE_OK, xpe_calib_save(&calibMap, tmpFile.string().c_str(), expiry, nullptr));

    std::vector<float> loadedData(W * H, 0.0f);
    XpeImageBuffer loaded{};
    loaded.data          = loadedData.data();
    loaded.width         = W;
    loaded.height        = H;
    loaded.bitsAllocated = 32;
    loaded.bitsStored    = 32;
    loaded.format        = XPE_PIXEL_FLOAT32;
    loaded.dataSize      = loadedData.size() * sizeof(float);

    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(tmpFile.string().c_str(), &loaded));
    for (uint32_t i = 0; i < W * H; ++i)
        EXPECT_NEAR(1.0f, loadedData[i], 1e-6f) << "pixel " << i;
}

// Expired calibration file returns XPE_ERR_CALIBRATION_EXPIRED
TEST_F(CalibManagerTest, ExpiredCalibReturnsExpiryError) {
    const uint64_t expiredMs = 1000ULL; // epoch + 1 second = definitely expired
    ASSERT_EQ(XPE_OK, xpe_calib_save(&calibMap, tmpFile.string().c_str(), expiredMs, nullptr));

    XpeImageBuffer out{};
    EXPECT_EQ(XPE_ERR_CALIBRATION_EXPIRED,
              xpe_calib_load_offset(tmpFile.string().c_str(), &out));
}

// xpe_calib_check_expiry: non-existent file
TEST_F(CalibManagerTest, CheckExpiryNonExistentFileReturnsError) {
    uint64_t expiry = 0;
    EXPECT_NE(XPE_OK, xpe_calib_check_expiry("/nonexistent.xpec", &expiry));
}

// xpe_calib_generate_offset: zero frameCount returns error
TEST_F(CalibManagerTest, GenerateOffsetZeroFramesReturnsError) {
    XpeImageBuffer out{};
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_calib_generate_offset(&calibMap, 0, &out, nullptr));
}

// CRC mismatch: saved file with corrupted payload must return IO error
TEST_F(CalibManagerTest, CorruptedPayloadReturnsCrcError) {
    const uint64_t expiry = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count()
    ) + 365ULL * 24 * 3600 * 1000;

    ASSERT_EQ(XPE_OK, xpe_calib_save(&calibMap, tmpFile.string().c_str(), expiry, nullptr));

    // Flip one byte in the pixel payload (after 64-byte header) without updating CRC.
    FILE* f = std::fopen(tmpFile.string().c_str(), "r+b");
    ASSERT_NE(nullptr, f);
    std::fseek(f, 64, SEEK_SET); // skip header to reach pixel payload
    uint8_t corrupt = 0xFF;
    std::fwrite(&corrupt, 1, 1, f);
    std::fclose(f);

    std::vector<float> loadedData(W * H, 0.0f);
    XpeImageBuffer loaded{};
    loaded.data          = loadedData.data();
    loaded.width         = W;
    loaded.height        = H;
    loaded.bitsAllocated = 32;
    loaded.bitsStored    = 32;
    loaded.format        = XPE_PIXEL_FLOAT32;
    loaded.dataSize      = loadedData.size() * sizeof(float);

    EXPECT_EQ(XPE_ERR_IO_FAILED, xpe_calib_load_gain(tmpFile.string().c_str(), &loaded));
}

// xpe_calib_generate_offset: single frame produces identical map
TEST_F(CalibManagerTest, GenerateOffsetSingleFrameProducesCorrectMean) {
    std::vector<uint16_t> frameData(W * H, 500);
    XpeImageBuffer frame{};
    frame.data          = frameData.data();
    frame.width         = W;
    frame.height        = H;
    frame.bitsAllocated = 16;
    frame.bitsStored    = 16;
    frame.format        = XPE_PIXEL_UINT16;
    frame.dataSize      = frameData.size() * sizeof(uint16_t);

    std::vector<uint16_t> outData(W * H, 0);
    XpeImageBuffer out{};
    out.data          = outData.data();
    out.width         = W;
    out.height        = H;
    out.bitsAllocated = 16;
    out.bitsStored    = 16;
    out.format        = XPE_PIXEL_UINT16;
    out.dataSize      = outData.size() * sizeof(uint16_t);

    ASSERT_EQ(XPE_OK, xpe_calib_generate_offset(&frame, 1, &out, nullptr));
    EXPECT_EQ(500u, outData[0]);
}

} // namespace
