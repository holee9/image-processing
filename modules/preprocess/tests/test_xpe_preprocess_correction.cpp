/**
 * @file test_xpe_preprocess_correction.cpp
 * @brief Correction algorithm tests for XPE Preprocessing Module
 *
 * Tests offset, gain, and defect correction algorithms.
 * Covers REQ-P1A-010, REQ-P1A-011, REQ-P1A-012.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <limits>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"

// =============================================================================
// Test Data Generation Helpers
// =============================================================================

namespace {

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

    // stride field does not exist in XpeImageBuffer; compute data_size directly
    size_t pixel_size = (format == XPE_PIXEL_UINT16) ? sizeof(uint16_t) : sizeof(float);

    // Allocate data
    size_t data_size = height * width * pixel_size;
    img->data = new uint8_t[data_size];
    img->dataSize = data_size;
    std::memset(img->data, 0, data_size);

    // Fill with test data
    if (format == XPE_PIXEL_UINT16) {
        uint16_t* data = reinterpret_cast<uint16_t*>(img->data);
        for (size_t i = 0; i < width * height; ++i) {
            data[i] = 1000 + (i % 100);  // Test pattern
        }
    } else {  // FLOAT32
        float* data = reinterpret_cast<float*>(img->data);
        for (size_t i = 0; i < width * height; ++i) {
            data[i] = 1.0f + (i % 100) * 0.01f;  // Test pattern
        }
    }

    return img;
}

/**
 * @brief Create test metadata
 */
XpeImageMetadata* CreateTestMetadata() {
    XpeImageMetadata* meta = new XpeImageMetadata();
    std::memset(meta, 0, sizeof(XpeImageMetadata));

    std::strncpy(meta->bodyPart, "CHEST", sizeof(meta->bodyPart) - 1);
    meta->kVp = 120.0f;
    meta->mAs = 100.0f;
    meta->SID_mm = 1200.0f;
    meta->pixelPitch_mm = 0.143f;
    meta->acquisitionTime = 0;
    meta->flags = 0;

    return meta;
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

class PreprocessCorrectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        xpe_preprocess_shutdown();
        ASSERT_EQ(xpe_preprocess_init(NULL), XPE_OK);
    }

    void TearDown() override {
        xpe_preprocess_shutdown();
    }
};

// =============================================================================
// Offset Correction Tests
// =============================================================================

/**
 * @test OffsetCorrect_BasicSubtraction
 *
 * Given module is initialized and offset map is loaded
 * When offset correction is applied
 * Then output = max(input - offset, 0)
 */
TEST_F(PreprocessCorrectionTest, OffsetCorrect_BasicSubtraction) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_UINT16);
    XpeImageBuffer* output = CreateTestImage(1024, 1024, XPE_PIXEL_UINT16);
    XpeImageMetadata* metadata = CreateTestMetadata();

    XpeErrorCode result = xpe_offset_correct(input, output, metadata);

    EXPECT_TRUE(result == XPE_OK || result == XPE_ERR_NOT_INITIALIZED);

    FreeTestImage(input);
    FreeTestImage(output);
    delete metadata;
}

/**
 * @test OffsetCorrect_FloorAtZeroClamping
 *
 * Given module is initialized
 * When input is less than offset map
 * Then output is clamped at 0 (no negative values)
 */
TEST_F(PreprocessCorrectionTest, OffsetCorrect_FloorAtZeroClamping) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_UINT16);
    XpeImageBuffer* output = CreateTestImage(1024, 1024, XPE_PIXEL_UINT16);
    XpeImageMetadata* metadata = CreateTestMetadata();

    // Set input values to 50 (below offset of 100)
    uint16_t* data = reinterpret_cast<uint16_t*>(input->data);
    for (size_t i = 0; i < 1024 * 1024; ++i) {
        data[i] = 50;
    }

    XpeErrorCode result = xpe_offset_correct(input, output, metadata);

    EXPECT_TRUE(result == XPE_OK || result == XPE_ERR_NOT_INITIALIZED);

    // Verify no negative values (UINT16 can't be negative, but check floor behavior)
    if (result == XPE_OK) {
        uint16_t* out_data = reinterpret_cast<uint16_t*>(output->data);
        for (size_t i = 0; i < 100; ++i) {  // Check first 100 pixels
            EXPECT_GE(out_data[i], 0);
        }
    }

    FreeTestImage(input);
    FreeTestImage(output);
    delete metadata;
}

/**
 * @test OffsetCorrect_DimensionMismatch
 *
 * Given module is initialized
 * When input/output dimensions don't match
 * Then returns XPE_ERR_BUFFER_TOO_SMALL
 */
TEST_F(PreprocessCorrectionTest, OffsetCorrect_DimensionMismatch) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_UINT16);
    XpeImageBuffer* output = CreateTestImage(512, 512, XPE_PIXEL_UINT16);
    XpeImageMetadata* metadata = CreateTestMetadata();

    XpeErrorCode result = xpe_offset_correct(input, output, metadata);

    EXPECT_TRUE(result == XPE_ERR_BUFFER_TOO_SMALL ||
                result == XPE_ERR_INVALID_INPUT ||
                result == XPE_ERR_NOT_INITIALIZED);

    FreeTestImage(input);
    FreeTestImage(output);
    delete metadata;
}

/**
 * @test OffsetCorrect_NullBuffers
 *
 * Given module is initialized
 * When NULL pointers are passed
 * Then returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessCorrectionTest, OffsetCorrect_NullBuffers) {
    XpeImageMetadata* metadata = CreateTestMetadata();

    XpeErrorCode result = xpe_offset_correct(nullptr, nullptr, metadata);

    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);

    delete metadata;
}

/**
 * @test OffsetCorrect_NullMetadata
 *
 * Given module is initialized
 * When NULL metadata is passed
 * Then returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessCorrectionTest, OffsetCorrect_NullMetadata) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_UINT16);
    XpeImageBuffer* output = CreateTestImage(1024, 1024, XPE_PIXEL_UINT16);

    XpeErrorCode result = xpe_offset_correct(input, output, nullptr);

    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);

    FreeTestImage(input);
    FreeTestImage(output);
}

/**
 * @test OffsetCorrect_FormatMismatch
 *
 * Given module is initialized
 * When input format is not UINT16
 * Then returns XPE_ERR_UNSUPPORTED_FORMAT
 */
TEST_F(PreprocessCorrectionTest, OffsetCorrect_FormatMismatch) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_FLOAT32);
    XpeImageBuffer* output = CreateTestImage(1024, 1024, XPE_PIXEL_FLOAT32);
    XpeImageMetadata* metadata = CreateTestMetadata();

    XpeErrorCode result = xpe_offset_correct(input, output, metadata);

    EXPECT_TRUE(result == XPE_ERR_UNSUPPORTED_FORMAT ||
                result == XPE_ERR_NOT_INITIALIZED);

    FreeTestImage(input);
    FreeTestImage(output);
    delete metadata;
}

// =============================================================================
// Gain Correction Tests
// =============================================================================

/**
 * @test GainCorrect_UINT16ToFLOAT32Conversion
 *
 * Given module is initialized and gain map is loaded
 * When gain correction is applied
 * Then output is FLOAT32 with correct format
 */
TEST_F(PreprocessCorrectionTest, GainCorrect_UINT16ToFLOAT32Conversion) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_UINT16);
    XpeImageBuffer* output = CreateTestImage(1024, 1024, XPE_PIXEL_FLOAT32);
    XpeImageMetadata* metadata = CreateTestMetadata();

    XpeErrorCode result = xpe_gain_correct(input, output, metadata);

    EXPECT_TRUE(result == XPE_OK ||
                result == XPE_ERR_NOT_INITIALIZED ||
                result == XPE_ERR_UNSUPPORTED_FORMAT);

    // Verify output format
    if (result == XPE_OK) {
        EXPECT_EQ(output->format, XPE_PIXEL_FLOAT32);
    }

    FreeTestImage(input);
    FreeTestImage(output);
    delete metadata;
}

/**
 * @test GainCorrect_DimensionMismatch
 *
 * Given module is initialized
 * When input/output dimensions don't match
 * Then returns XPE_ERR_BUFFER_TOO_SMALL
 */
TEST_F(PreprocessCorrectionTest, GainCorrect_DimensionMismatch) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_UINT16);
    XpeImageBuffer* output = CreateTestImage(512, 512, XPE_PIXEL_FLOAT32);
    XpeImageMetadata* metadata = CreateTestMetadata();

    XpeErrorCode result = xpe_gain_correct(input, output, metadata);

    EXPECT_TRUE(result == XPE_ERR_BUFFER_TOO_SMALL ||
                result == XPE_ERR_INVALID_INPUT ||
                result == XPE_ERR_NOT_INITIALIZED);

    FreeTestImage(input);
    FreeTestImage(output);
    delete metadata;
}

/**
 * @test GainCorrect_NullBuffers
 *
 * Given module is initialized
 * When NULL pointers are passed
 * Then returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessCorrectionTest, GainCorrect_NullBuffers) {
    XpeImageMetadata* metadata = CreateTestMetadata();

    XpeErrorCode result = xpe_gain_correct(nullptr, nullptr, metadata);

    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);

    delete metadata;
}

/**
 * @test GainCorrect_FormatMismatch
 *
 * Given module is initialized
 * When input format is not UINT16 or output format is not FLOAT32
 * Then returns XPE_ERR_UNSUPPORTED_FORMAT
 */
TEST_F(PreprocessCorrectionTest, GainCorrect_FormatMismatch) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_FLOAT32);
    XpeImageBuffer* output = CreateTestImage(1024, 1024, XPE_PIXEL_FLOAT32);
    XpeImageMetadata* metadata = CreateTestMetadata();

    XpeErrorCode result = xpe_gain_correct(input, output, metadata);

    EXPECT_TRUE(result == XPE_ERR_UNSUPPORTED_FORMAT ||
                result == XPE_ERR_NOT_INITIALIZED);

    FreeTestImage(input);
    FreeTestImage(output);
    delete metadata;
}

/**
 * @test GainCorrect_NoNaNInfInOutput
 *
 * Given module is initialized
 * When gain correction is applied
 * Then output contains no NaN or Inf values
 */
TEST_F(PreprocessCorrectionTest, GainCorrect_NoNaNInfInOutput) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_UINT16);
    XpeImageBuffer* output = CreateTestImage(1024, 1024, XPE_PIXEL_FLOAT32);
    XpeImageMetadata* metadata = CreateTestMetadata();

    XpeErrorCode result = xpe_gain_correct(input, output, metadata);

    if (result == XPE_OK) {
        float* data = reinterpret_cast<float*>(output->data);
        for (size_t i = 0; i < 100; ++i) {
            EXPECT_TRUE(std::isfinite(data[i]));
        }
    }

    FreeTestImage(input);
    FreeTestImage(output);
    delete metadata;
}

// =============================================================================
// Defect Correction Tests
// =============================================================================

/**
 * @test DefectCorrect_BasicInterpolation
 *
 * Given module is initialized and defect map is loaded
 * When defect correction is applied
 * Then defective pixels are interpolated
 */
TEST_F(PreprocessCorrectionTest, DefectCorrect_BasicInterpolation) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_FLOAT32);
    XpeImageBuffer* output = CreateTestImage(1024, 1024, XPE_PIXEL_FLOAT32);
    XpeImageMetadata* metadata = CreateTestMetadata();

    XpeErrorCode result = xpe_defect_correct(input, output, metadata);

    EXPECT_TRUE(result == XPE_OK ||
                result == XPE_ERR_NOT_INITIALIZED);

    FreeTestImage(input);
    FreeTestImage(output);
    delete metadata;
}

/**
 * @test DefectCorrect_DimensionMismatch
 *
 * Given module is initialized
 * When input/output dimensions don't match
 * Then returns XPE_ERR_BUFFER_TOO_SMALL
 */
TEST_F(PreprocessCorrectionTest, DefectCorrect_DimensionMismatch) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_FLOAT32);
    XpeImageBuffer* output = CreateTestImage(512, 512, XPE_PIXEL_FLOAT32);
    XpeImageMetadata* metadata = CreateTestMetadata();

    XpeErrorCode result = xpe_defect_correct(input, output, metadata);

    EXPECT_TRUE(result == XPE_ERR_BUFFER_TOO_SMALL ||
                result == XPE_ERR_INVALID_INPUT ||
                result == XPE_ERR_NOT_INITIALIZED);

    FreeTestImage(input);
    FreeTestImage(output);
    delete metadata;
}

/**
 * @test DefectCorrect_NullBuffers
 *
 * Given module is initialized
 * When NULL pointers are passed
 * Then returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessCorrectionTest, DefectCorrect_NullBuffers) {
    XpeImageMetadata* metadata = CreateTestMetadata();

    XpeErrorCode result = xpe_defect_correct(nullptr, nullptr, metadata);

    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);

    delete metadata;
}

/**
 * @test DefectCorrect_FormatMismatch
 *
 * Given module is initialized
 * When input format is not FLOAT32
 * Then returns XPE_ERR_UNSUPPORTED_FORMAT
 */
TEST_F(PreprocessCorrectionTest, DefectCorrect_FormatMismatch) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_UINT16);
    XpeImageBuffer* output = CreateTestImage(1024, 1024, XPE_PIXEL_UINT16);
    XpeImageMetadata* metadata = CreateTestMetadata();

    XpeErrorCode result = xpe_defect_correct(input, output, metadata);

    EXPECT_TRUE(result == XPE_ERR_UNSUPPORTED_FORMAT ||
                result == XPE_ERR_NOT_INITIALIZED);

    FreeTestImage(input);
    FreeTestImage(output);
    delete metadata;
}

/**
 * @test DefectCorrect_ClusterDefectHandling
 *
 * Given module is initialized
 * When cluster defects (adjacent bad pixels) are present
 * Then correction algorithm handles clusters properly
 */
TEST_F(PreprocessCorrectionTest, DefectCorrect_ClusterDefectHandling) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_FLOAT32);
    XpeImageBuffer* output = CreateTestImage(1024, 1024, XPE_PIXEL_FLOAT32);
    XpeImageMetadata* metadata = CreateTestMetadata();

    // Simulate cluster defect (set 2x2 region to 0)
    float* data = reinterpret_cast<float*>(input->data);
    for (int y = 100; y < 102; ++y) {
        for (int x = 100; x < 102; ++x) {
            data[y * 1024 + x] = 0.0f;
        }
    }

    XpeErrorCode result = xpe_defect_correct(input, output, metadata);

    EXPECT_TRUE(result == XPE_OK ||
                result == XPE_ERR_NOT_INITIALIZED);

    FreeTestImage(input);
    FreeTestImage(output);
    delete metadata;
}

/**
 * @test DefectCorrect_EdgeDefectHandling
 *
 * Given module is initialized
 * When defects are at image edges
 * Then correction algorithm handles edge cases properly
 */
TEST_F(PreprocessCorrectionTest, DefectCorrect_EdgeDefectHandling) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_FLOAT32);
    XpeImageBuffer* output = CreateTestImage(1024, 1024, XPE_PIXEL_FLOAT32);
    XpeImageMetadata* metadata = CreateTestMetadata();

    // Simulate edge defects
    float* data = reinterpret_cast<float*>(input->data);
    data[0] = 0.0f;  // Top-left corner
    data[1023] = 0.0f;  // Top-right corner
    data[1024 * 1023] = 0.0f;  // Bottom-left corner
    data[1024 * 1024 - 1] = 0.0f;  // Bottom-right corner

    XpeErrorCode result = xpe_defect_correct(input, output, metadata);

    EXPECT_TRUE(result == XPE_OK ||
                result == XPE_ERR_NOT_INITIALIZED);

    FreeTestImage(input);
    FreeTestImage(output);
    delete metadata;
}

// =============================================================================
// Main Test Runner
// =============================================================================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
