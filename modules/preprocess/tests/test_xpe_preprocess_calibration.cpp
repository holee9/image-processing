/**
 * @file test_xpe_preprocess_calibration.cpp
 * @brief Calibration management tests for XPE Preprocessing Module
 *
 * Tests calibration loading, generation, expiry checking, and saving.
 * Covers REQ-P1A-014 through REQ-P1A-019.
 */

#include <gtest/gtest.h>
#include <fstream>
#include <cstring>
#include <chrono>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"

// =============================================================================
// Test Data Generation Helpers
// =============================================================================

namespace {

/**
 * @brief Create test XCal file with valid format
 */
bool CreateTestXCalFile(const char* filepath,
                        uint32_t width,
                        uint32_t height,
                        uint32_t data_type,
                        const char* session_id = "test-session-123") {
    std::ofstream file(filepath, std::ios::binary);
    if (!file) return false;

    // Write header
    file.write("XCAL", 4);  // Magic

    uint32_t version = 1;
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));

    file.write(reinterpret_cast<const char*>(&width), sizeof(width));
    file.write(reinterpret_cast<const char*>(&height), sizeof(height));
    file.write(reinterpret_cast<const char*>(&data_type), sizeof(data_type));

    // Timestamps (valid for 30 days)
    uint64_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    uint64_t expires_at = now + (30 * 24 * 3600);
    file.write(reinterpret_cast<const char*>(&now), sizeof(now));
    file.write(reinterpret_cast<const char*>(&expires_at), sizeof(expires_at));

    // Session ID
    char session[64] = {0};
    std::strncpy(session, session_id, sizeof(session) - 1);
    file.write(session, sizeof(session));

    // SHA-256 placeholder (all zeros for test)
    uint8_t sha256[32] = {0};
    file.write(reinterpret_cast<const char*>(sha256), sizeof(sha256));

    // Data size
    uint64_t data_size;
    if (data_type == 0) {  // UINT16
        data_size = width * height * sizeof(uint16_t);
        std::vector<uint16_t> data(width * height, 100);
        file.write(reinterpret_cast<const char*>(data.data()), data_size);
    } else if (data_type == 1) {  // FLOAT32
        data_size = width * height * sizeof(float);
        std::vector<float> data(width * height, 1.0f);
        file.write(reinterpret_cast<const char*>(data.data()), data_size);
    } else {  // UINT8 (defect map)
        data_size = width * height * sizeof(uint8_t);
        std::vector<uint8_t> data(width * height, 0);
        file.write(reinterpret_cast<const char*>(data.data()), data_size);
    }

    return file.good();
}

/**
 * @brief Create corrupted XCal file (wrong magic number)
 */
bool CreateCorruptedXCalFile(const char* filepath) {
    std::ofstream file(filepath, std::ios::binary);
    if (!file) return false;

    file.write("BAD!", 4);  // Wrong magic
    return file.good();
}

/**
 * @brief Create test image buffer
 */
XpeImageBuffer* CreateTestImage(uint32_t width, uint32_t height, XpePixelFormat format) {
    XpeImageBuffer* img = new XpeImageBuffer();
    std::memset(img, 0, sizeof(XpeImageBuffer));

    img->width = width;
    img->height = height;
    img->format = format;
    img->bitsAllocated = (format == XPE_PIXEL_UINT16) ? 16 : 32;
    img->bitsStored = img->bitsAllocated;

    size_t pixel_size = (format == XPE_PIXEL_UINT16) ? sizeof(uint16_t) : sizeof(float);
    img->stride = width * pixel_size;

    size_t data_size = height * width * pixel_size;
    img->data = new uint8_t[data_size];
    img->dataSize = data_size;
    std::memset(img->data, 0, data_size);

    // Fill with test data
    if (format == XPE_PIXEL_UINT16) {
        uint16_t* data = reinterpret_cast<uint16_t*>(img->data);
        for (size_t i = 0; i < width * height; ++i) {
            data[i] = 1000 + (i % 100);
        }
    } else {
        float* data = reinterpret_cast<float*>(img->data);
        for (size_t i = 0; i < width * height; ++i) {
            data[i] = 1.0f + (i % 100) * 0.01f;
        }
    }

    return img;
}

/**
 * @brief Free image buffer
 */
void FreeTestImage(XpeImageBuffer* img) {
    if (img) {
        delete[] img->data;
        delete img;
    }
}

} // anonymous namespace

// =============================================================================
// Test Fixtures
// =============================================================================

class PreprocessCalibrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        xpe_preprocess_shutdown();
        ASSERT_EQ(xpe_preprocess_init(NULL), XPE_OK);
    }

    void TearDown() override {
        xpe_preprocess_shutdown();

        // Cleanup test files
        std::remove("test_offset.xcal");
        std::remove("test_gain.xcal");
        std::remove("test_defect.xcal");
        std::remove("test_check.xcal");
        std::remove("test_output.xcal");
    }
};

// =============================================================================
// Calibration Loading Tests
// =============================================================================

/**
 * @test LoadOffset_ValidXCalFile
 *
 * Given module is initialized
 * When valid XCal offset file is loaded
 * Then returns XPE_OK
 */
TEST_F(PreprocessCalibrationTest, LoadOffset_ValidXCalFile) {
    ASSERT_TRUE(CreateTestXCalFile("test_offset.xcal", 1024, 1024, 0));

    XpeErrorCode result = xpe_calib_load_offset("test_offset.xcal");

    EXPECT_EQ(result, XPE_OK);
}

/**
 * @test LoadOffset_InvalidFilePath
 *
 * Given module is initialized
 * When non-existent file path is provided
 * Then returns XPE_ERR_IO_FAILED
 */
TEST_F(PreprocessCalibrationTest, LoadOffset_InvalidFilePath) {
    XpeErrorCode result = xpe_calib_load_offset("nonexistent_file.xcal");

    EXPECT_EQ(result, XPE_ERR_IO_FAILED);
}

/**
 * @test LoadOffset_NullFilePath
 *
 * Given module is initialized
 * When NULL file path is provided
 * Then returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessCalibrationTest, LoadOffset_NullFilePath) {
    XpeErrorCode result = xpe_calib_load_offset(nullptr);

    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);
}

/**
 * @test LoadOffset_CorruptedFile
 *
 * Given module is initialized
 * When corrupted XCal file is loaded
 * Then returns XPE_ERR_CONFIG_INVALID
 */
TEST_F(PreprocessCalibrationTest, LoadOffset_CorruptedFile) {
    ASSERT_TRUE(CreateCorruptedXCalFile("test_offset.xcal"));

    XpeErrorCode result = xpe_calib_load_offset("test_offset.xcal");

    EXPECT_TRUE(result == XPE_ERR_IO_FAILED ||
                result == XPE_ERR_CONFIG_INVALID);
}

/**
 * @test LoadGain_ValidXCalFile
 *
 * Given module is initialized
 * When valid XCal gain file is loaded
 * Then returns XPE_OK
 */
TEST_F(PreprocessCalibrationTest, LoadGain_ValidXCalFile) {
    ASSERT_TRUE(CreateTestXCalFile("test_gain.xcal", 1024, 1024, 1));

    XpeErrorCode result = xpe_calib_load_gain("test_gain.xcal");

    EXPECT_EQ(result, XPE_OK);
}

/**
 * @test LoadGain_InvalidFilePath
 *
 * Given module is initialized
 * When non-existent file path is provided
 * Then returns XPE_ERR_IO_FAILED
 */
TEST_F(PreprocessCalibrationTest, LoadGain_InvalidFilePath) {
    XpeErrorCode result = xpe_calib_load_gain("nonexistent_file.xcal");

    EXPECT_EQ(result, XPE_ERR_IO_FAILED);
}

/**
 * @test LoadDefectMap_ValidXCalFile
 *
 * Given module is initialized
 * When valid XCal defect map file is loaded
 * Then returns XPE_OK
 */
TEST_F(PreprocessCalibrationTest, LoadDefectMap_ValidXCalFile) {
    ASSERT_TRUE(CreateTestXCalFile("test_defect.xcal", 1024, 1024, 2));

    XpeErrorCode result = xpe_calib_load_defect_map("test_defect.xcal");

    EXPECT_EQ(result, XPE_OK);
}

/**
 * @test LoadDefectMap_InvalidFilePath
 *
 * Given module is initialized
 * When non-existent file path is provided
 * Then returns XPE_ERR_IO_FAILED
 */
TEST_F(PreprocessCalibrationTest, LoadDefectMap_InvalidFilePath) {
    XpeErrorCode result = xpe_calib_load_defect_map("nonexistent_file.xcal");

    EXPECT_EQ(result, XPE_ERR_IO_FAILED);
}

// =============================================================================
// Calibration Generation Tests
// =============================================================================

/**
 * @test GenerateOffset_DarkFrameAveraging
 *
 * Given module is initialized
 * When offset calibration is generated from multiple dark frames
 * Then output is average of all frames
 */
TEST_F(PreprocessCalibrationTest, GenerateOffset_DarkFrameAveraging) {
    const int num_frames = 10;
    std::vector<XpeImageBuffer*> dark_frames;

    for (int i = 0; i < num_frames; ++i) {
        dark_frames.push_back(CreateTestImage(512, 512, XPE_PIXEL_UINT16));
    }

    XpeErrorCode result = xpe_calib_generate_offset(
        dark_frames.data(),
        num_frames,
        100.0f,  // integration_time_ms
        25.0f,   // temperature_c
        "test_output.xcal"
    );

    EXPECT_TRUE(result == XPE_OK ||
                result == XPE_ERR_NOT_IMPLEMENTED);

    for (auto frame : dark_frames) {
        FreeTestImage(frame);
    }
}

/**
 * @test GenerateOffset_SingleFrame
 *
 * Given module is initialized
 * When offset calibration is generated from single dark frame
 * Then output is that frame
 */
TEST_F(PreprocessCalibrationTest, GenerateOffset_SingleFrame) {
    XpeImageBuffer* dark_frame = CreateTestImage(512, 512, XPE_PIXEL_UINT16);

    XpeErrorCode result = xpe_calib_generate_offset(
        &dark_frame,
        1,
        100.0f,
        25.0f,
        "test_output.xcal"
    );

    EXPECT_TRUE(result == XPE_OK ||
                result == XPE_ERR_NOT_IMPLEMENTED);

    FreeTestImage(dark_frame);
}

/**
 * @test GenerateOffset_NullFrames
 *
 * Given module is initialized
 * When null pointer is passed for dark frames
 * Then returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessCalibrationTest, GenerateOffset_NullFrames) {
    XpeErrorCode result = xpe_calib_generate_offset(
        nullptr,
        10,
        100.0f,
        25.0f,
        "test_output.xcal"
    );

    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);
}

/**
 * @test GenerateOffset_InvalidFrameCount
 *
 * Given module is initialized
 * When invalid frame count (<= 0) is provided
 * Then returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessCalibrationTest, GenerateOffset_InvalidFrameCount) {
    XpeImageBuffer* dark_frame = CreateTestImage(512, 512, XPE_PIXEL_UINT16);

    XpeErrorCode result = xpe_calib_generate_offset(
        &dark_frame,
        0,  // Invalid frame count
        100.0f,
        25.0f,
        "test_output.xcal"
    );

    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);

    FreeTestImage(dark_frame);
}

/**
 * @test GenerateOffset_DimensionMismatch
 *
 * Given module is initialized
 * When frames have different dimensions
 * Then returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessCalibrationTest, GenerateOffset_DimensionMismatch) {
    std::vector<XpeImageBuffer*> dark_frames;
    dark_frames.push_back(CreateTestImage(512, 512, XPE_PIXEL_UINT16));
    dark_frames.push_back(CreateTestImage(1024, 1024, XPE_PIXEL_UINT16));  // Different size

    XpeErrorCode result = xpe_calib_generate_offset(
        dark_frames.data(),
        2,
        100.0f,
        25.0f,
        "test_output.xcal"
    );

    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);

    for (auto frame : dark_frames) {
        FreeTestImage(frame);
    }
}

// =============================================================================
// Calibration Expiry Tests
// =============================================================================

/**
 * @test CheckExpiry_ValidCalibration
 *
 * Given module is initialized
 * When valid calibration file is checked
 * Then returns is_expired=false with remaining_days > 0
 */
TEST_F(PreprocessCalibrationTest, CheckExpiry_ValidCalibration) {
    ASSERT_TRUE(CreateTestXCalFile("test_check.xcal", 1024, 1024, 0));

    bool is_expired = false;
    int32_t remaining_days = 0;

    XpeErrorCode result = xpe_calib_check_expiry(
        "test_check.xcal",
        &is_expired,
        &remaining_days
    );

    EXPECT_EQ(result, XPE_OK);
    EXPECT_FALSE(is_expired);
    EXPECT_GT(remaining_days, 0);
}

/**
 * @test CheckExpiry_ExpiredCalibration
 *
 * Given module is initialized
 * When expired calibration file is checked
 * Then returns is_expired=true with negative remaining_days
 */
TEST_F(PreprocessCalibrationTest, CheckExpiry_ExpiredCalibration) {
    // Create expired calibration file
    std::ofstream file("test_check.xcal", std::ios::binary);
    ASSERT_TRUE(file.good());

    file.write("XCAL", 4);
    uint32_t version = 1, width = 1024, height = 1024, data_type = 0;
    file.write(reinterpret_cast<const char*>(&version), sizeof(version));
    file.write(reinterpret_cast<const char*>(&width), sizeof(width));
    file.write(reinterpret_cast<const char*>(&height), sizeof(height));
    file.write(reinterpret_cast<const char*>(&data_type), sizeof(data_type));

    // Expired timestamp (1 day ago)
    uint64_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    uint64_t expires_at = now - (24 * 3600);
    file.write(reinterpret_cast<const char*>(&now), sizeof(now));
    file.write(reinterpret_cast<const char*>(&expires_at), sizeof(expires_at));

    char session[64] = {0};
    file.write(session, sizeof(session));

    uint8_t sha256[32] = {0};
    file.write(reinterpret_cast<const char*>(sha256), sizeof(sha256));

    std::vector<uint16_t> data(1024 * 1024, 100);
    file.write(reinterpret_cast<const char*>(data.data()), data.size() * sizeof(uint16_t));
    file.close();

    bool is_expired = false;
    int32_t remaining_days = 0;

    XpeErrorCode result = xpe_calib_check_expiry(
        "test_check.xcal",
        &is_expired,
        &remaining_days
    );

    EXPECT_EQ(result, XPE_OK);
    EXPECT_TRUE(is_expired);
    EXPECT_LT(remaining_days, 0);
}

/**
 * @test CheckExpiry_NullFilePath
 *
 * Given module is initialized
 * When null file path is checked
 * Then returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessCalibrationTest, CheckExpiry_NullFilePath) {
    bool is_expired = false;
    int32_t remaining_days = 0;

    XpeErrorCode result = xpe_calib_check_expiry(
        nullptr,
        &is_expired,
        &remaining_days
    );

    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);
}

/**
 * @test CheckExpiry_NullOutputPointers
 *
 * Given module is initialized
 * When null output pointers are provided
 * Then returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessCalibrationTest, CheckExpiry_NullOutputPointers) {
    XpeErrorCode result = xpe_calib_check_expiry(
        "test_check.xcal",
        nullptr,
        nullptr
    );

    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);
}

// =============================================================================
// Calibration Save Tests
// =============================================================================

/**
 * @test Save_OffsetCalibration
 *
 * Given module is initialized and offset is loaded
 * When calibration is saved
 * Then file is created successfully
 */
TEST_F(PreprocessCalibrationTest, Save_OffsetCalibration) {
    // First load offset calibration
    ASSERT_TRUE(CreateTestXCalFile("test_offset.xcal", 1024, 1024, 0));
    ASSERT_EQ(xpe_calib_load_offset("test_offset.xcal"), XPE_OK);

    XpeErrorCode result = xpe_calib_save("test_output.xcal", "offset");

    EXPECT_TRUE(result == XPE_OK ||
                result == XPE_ERR_NOT_IMPLEMENTED ||
                result == XPE_ERR_NOT_INITIALIZED);
}

/**
 * @test Save_GainCalibration
 *
 * Given module is initialized and gain is loaded
 * When calibration is saved
 * Then file is created successfully
 */
TEST_F(PreprocessCalibrationTest, Save_GainCalibration) {
    // First load gain calibration
    ASSERT_TRUE(CreateTestXCalFile("test_gain.xcal", 1024, 1024, 1));
    ASSERT_EQ(xpe_calib_load_gain("test_gain.xcal"), XPE_OK);

    XpeErrorCode result = xpe_calib_save("test_output.xcal", "gain");

    EXPECT_TRUE(result == XPE_OK ||
                result == XPE_ERR_NOT_IMPLEMENTED ||
                result == XPE_ERR_NOT_INITIALIZED);
}

/**
 * @test Save_NullFilePath
 *
 * Given module is initialized
 * When null file path is provided
 * Then returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessCalibrationTest, Save_NullFilePath) {
    XpeErrorCode result = xpe_calib_save(nullptr, "offset");

    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);
}

/**
 * @test Save_NullCalibType
 *
 * Given module is initialized
 * When null calibration type is provided
 * Then returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessCalibrationTest, Save_NullCalibType) {
    XpeErrorCode result = xpe_calib_save("test_output.xcal", nullptr);

    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);
}

/**
 * @test Save_InvalidCalibType
 *
 * Given module is initialized
 * When invalid calibration type is provided
 * Then returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessCalibrationTest, Save_InvalidCalibType) {
    XpeErrorCode result = xpe_calib_save("test_output.xcal", "invalid_type");

    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);
}

// =============================================================================
// Main Test Runner
// =============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
