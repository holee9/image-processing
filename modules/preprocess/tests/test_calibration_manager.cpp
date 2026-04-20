/**
 * @file test_calibration_manager.cpp
 * @brief Tests for SWU-1.5: Calibration Manager — new XCal v1 API
 *        REQ-P1A-014..016, REQ-P1A-017..019, REQ-P1A-018
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <vector>
#include <cstring>
#include <cstdio>
#include <filesystem>

namespace fs = std::filesystem;

namespace {

class CalibManagerTest : public ::testing::Test {
protected:
    static constexpr uint32_t W = 8;
    static constexpr uint32_t H = 8;

    fs::path tmpFile;

    void SetUp() override {
        tmpFile = fs::temp_directory_path() / "xpe_calib_test.xcal";
    }

    void TearDown() override {
        fs::remove(tmpFile);
        // Also clean up possible .tmp file
        fs::remove(fs::path(tmpFile.string() + ".tmp"));
    }
};

// --- xpe_calib_load_offset (1-arg new API) ---

TEST_F(CalibManagerTest, LoadOffsetNullPathReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_calib_load_offset(nullptr));
}

TEST_F(CalibManagerTest, LoadOffsetNonExistentFileReturnsIoError) {
    EXPECT_EQ(XPE_ERR_IO_FAILED,
              xpe_calib_load_offset("/nonexistent_path/file.xcal"));
}

// --- xpe_calib_load_gain (1-arg new API) ---

TEST_F(CalibManagerTest, LoadGainNullPathReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_calib_load_gain(nullptr));
}

TEST_F(CalibManagerTest, LoadGainNonExistentFileReturnsIoError) {
    EXPECT_EQ(XPE_ERR_IO_FAILED,
              xpe_calib_load_gain("/nonexistent_path/file.xcal"));
}

// --- xpe_calib_load_defect_map (1-arg new API) ---

TEST_F(CalibManagerTest, LoadDefectMapNullPathReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_calib_load_defect_map(nullptr));
}

TEST_F(CalibManagerTest, LoadDefectMapNonExistentFileReturnsIoError) {
    EXPECT_EQ(XPE_ERR_IO_FAILED,
              xpe_calib_load_defect_map("/nonexistent_path/file.xcal"));
}

// --- xpe_calib_check_expiry (3-arg new API) ---

TEST_F(CalibManagerTest, CheckExpiryNullPathReturnsError) {
    bool expired = false;
    int32_t days = 0;
    EXPECT_NE(XPE_OK, xpe_calib_check_expiry(nullptr, &expired, &days));
}

TEST_F(CalibManagerTest, CheckExpiryNonExistentFileReturnsError) {
    bool expired = false;
    int32_t days = 0;
    EXPECT_NE(XPE_OK, xpe_calib_check_expiry("/nonexistent.xcal", &expired, &days));
}

// --- xpe_calib_generate_offset (5-arg new API) ---

TEST_F(CalibManagerTest, GenerateOffsetNullFramesReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_calib_generate_offset(nullptr, 1, 10.0f, 20.0f,
                                         tmpFile.string().c_str()));
}

TEST_F(CalibManagerTest, GenerateOffsetZeroFramesReturnsError) {
    std::vector<uint16_t> frameData(W * H, 500u);
    XpeImageBuffer frame{};
    frame.data          = frameData.data();
    frame.width         = W;
    frame.height        = H;
    frame.bitsAllocated = 16;
    frame.bitsStored    = 16;
    frame.format        = XPE_PIXEL_UINT16;
    frame.dataSize      = frameData.size() * sizeof(uint16_t);

    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_calib_generate_offset(&frame, 0, 10.0f, 20.0f,
                                         tmpFile.string().c_str()));
}

TEST_F(CalibManagerTest, GenerateOffsetNullOutputPathReturnsError) {
    std::vector<uint16_t> frameData(W * H, 500u);
    XpeImageBuffer frame{};
    frame.data          = frameData.data();
    frame.width         = W;
    frame.height        = H;
    frame.bitsAllocated = 16;
    frame.bitsStored    = 16;
    frame.format        = XPE_PIXEL_UINT16;
    frame.dataSize      = frameData.size() * sizeof(uint16_t);

    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_calib_generate_offset(&frame, 1, 10.0f, 20.0f, nullptr));
}

// --- xpe_calib_save (2-arg new API) ---

TEST_F(CalibManagerTest, CalibSaveNullPathReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_calib_save(nullptr, "offset"));
}

TEST_F(CalibManagerTest, CalibSaveNullTypeReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_calib_save(tmpFile.string().c_str(), nullptr));
}

TEST_F(CalibManagerTest, CalibSaveInvalidTypeReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_calib_save(tmpFile.string().c_str(), "unknown_type"));
}

TEST_F(CalibManagerTest, CalibSaveUnloadedOffsetReturnsError) {
    // g_calib has no offset loaded → save should fail
    xpe_preprocess_shutdown();
    xpe_preprocess_init(nullptr);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_calib_save(tmpFile.string().c_str(), "offset"));
    xpe_preprocess_shutdown();
}

} // namespace
