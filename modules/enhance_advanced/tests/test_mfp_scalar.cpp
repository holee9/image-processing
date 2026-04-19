#include <gtest/gtest.h>
#include "xpe/enhance_advanced/xpe_enhance_advanced_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include <vector>
#include <cmath>
#include <algorithm>

/**
 * Test fixture for Multiscale Frequency Processing (MFP)
 * SWU-2.5: Laplacian pyramid decomposition
 * REQ-ADV-010: MFP execution
 * AC-MFP-001~AC-MFP-006: MFP acceptance criteria
 */
class MfpScalarTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize module before each test
        ASSERT_EQ(xpe_enhance_advanced_init(nullptr), XPE_OK);

        // Create test image (256x256 FLOAT32)
        width_ = 256;
        height_ = 256;
        testData_.resize(width_ * height_);

        // Create gradient pattern for testing
        for (int y = 0; y < height_; ++y) {
            for (int x = 0; x < width_; ++x) {
                testData_[y * width_ + x] = static_cast<float>(x + y) * 0.01f;
            }
        }

        // Setup image buffer
        img_.width = width_;
        img_.height = height_;
        img_.format = XPE_PIXEL_FLOAT32;
        img_.data = testData_.data();

        // Setup metadata
        meta_.kVp = 120.0f;
        strncpy_s(meta_.bodyPart, sizeof(meta_.bodyPart), "CHEST", _TRUNCATE);
    }

    void TearDown() override {
        xpe_enhance_advanced_shutdown();
    }

    int width_;
    int height_;
    std::vector<float> testData_;
    XpeImageBuffer img_;
    XpeImageMetadata meta_;
};

/**
 * T-201: RED Phase - Test basic MFP execution (AC-MFP-001)
 *
 * Given: Valid initialized module with FLOAT32 image
 * When: xpe_multiscale_process() is called with default config
 * Then: Function returns XPE_OK
 * And: Image data is modified (enhanced)
 */
TEST_F(MfpScalarTest, BasicMfpExecution) {
    // Create a copy of original data for comparison
    std::vector<float> original = testData_;

    // Execute MFP with default config
    XpeErrorCode result = xpe_multiscale_process(&img_, &meta_, nullptr);

    // RED: This will fail until MFP is implemented
    EXPECT_EQ(result, XPE_OK);

    // Verify image was modified (at least some pixels changed)
    bool dataModified = false;
    for (size_t i = 0; i < testData_.size(); ++i) {
        if (std::abs(testData_[i] - original[i]) > 1e-6f) {
            dataModified = true;
            break;
        }
    }
    EXPECT_TRUE(dataModified);
}

/**
 * T-202: Test MFP with NULL image buffer (REQ-ADV-022)
 *
 * Given: Module is initialized
 * When: xpe_multiscale_process() is called with NULL image
 * Then: Function returns XPE_ERR_INVALID_INPUT
 */
TEST_F(MfpScalarTest, NullImageBuffer) {
    XpeErrorCode result = xpe_multiscale_process(nullptr, &meta_, nullptr);
    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);
}

/**
 * T-203: Test MFP with NULL metadata (REQ-ADV-022)
 *
 * Given: Module is initialized
 * When: xpe_multiscale_process() is called with NULL metadata
 * Then: Function returns XPE_ERR_INVALID_INPUT
 */
TEST_F(MfpScalarTest, NullMetadata) {
    XpeErrorCode result = xpe_multiscale_process(&img_, nullptr, nullptr);
    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);
}

/**
 * T-204: Test MFP with unsupported pixel format (REQ-ADV-071)
 *
 * Given: Module is initialized
 * When: xpe_multiscale_process() is called with non-FLOAT32 image
 * Then: Function returns XPE_ERR_UNSUPPORTED_FORMAT
 */
TEST_F(MfpScalarTest, UnsupportedPixelFormat) {
    // Change format to UINT16
    img_.format = XPE_PIXEL_UINT16;

    XpeErrorCode result = xpe_multiscale_process(&img_, &meta_, nullptr);
    EXPECT_EQ(result, XPE_ERR_UNSUPPORTED_FORMAT);
}

/**
 * T-205: Test MFP with invalid dimensions (REQ-ADV-070)
 *
 * Given: Module is initialized
 * When: xpe_multiscale_process() is called with zero width/height
 * Then: Function returns XPE_ERR_INVALID_INPUT
 */
TEST_F(MfpScalarTest, InvalidDimensions) {
    img_.width = 0;
    XpeErrorCode result = xpe_multiscale_process(&img_, &meta_, nullptr);
    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);

    img_.width = width_;
    img_.height = 0;
    result = xpe_multiscale_process(&img_, &meta_, nullptr);
    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);
}

/**
 * T-206: Test MFP with custom configuration (AC-MFP-002)
 *
 * Given: Module is initialized
 * When: xpe_multiscale_process() is called with custom JSON config
 * Then: Function returns XPE_OK
 * And: Enhancement uses custom parameters
 */
TEST_F(MfpScalarTest, CustomConfiguration) {
    const char* customConfig = R"({
        "mfp": {
            "num_levels": 4,
            "edge_gain": 1.5,
            "noise_threshold": 0.02
        }
    })";

    std::vector<float> original = testData_;
    XpeErrorCode result = xpe_multiscale_process(&img_, &meta_, customConfig);

    EXPECT_EQ(result, XPE_OK);

    // Verify enhancement is different from default
    // TODO: Run with default config first for full comparison
}

/**
 * T-207: Test MFP identity reconstruction (REQ-ADV-050, AC-MFP-003)
 *
 * Given: Module is initialized
 * When: MFP is applied with unity gain coefficients (no enhancement)
 * Then: Output should equal input (identity)
 */
TEST_F(MfpScalarTest, IdentityReconstruction) {
    const char* identityConfig = R"({
        "mfp": {
            "edge_gain": 0.0,
            "texture_gain": 0.0,
            "flat_gain": 1.0,
            "noise_threshold": 0.0
        }
    })";

    std::vector<float> original = testData_;
    XpeErrorCode result = xpe_multiscale_process(&img_, &meta_, identityConfig);

    EXPECT_EQ(result, XPE_OK);

    // Verify output equals input (within floating point tolerance)
    for (size_t i = 0; i < testData_.size(); ++i) {
        EXPECT_NEAR(testData_[i], original[i], 1e-5f)
            << "Pixel mismatch at index " << i;
    }
}

/**
 * T-208: Test MFP with small image (edge case)
 *
 * Given: Module is initialized with 64x64 image
 * When: xpe_multiscale_process() is called
 * Then: Function returns XPE_OK
 * And: No memory corruption occurs
 */
TEST_F(MfpScalarTest, SmallImage) {
    std::vector<float> smallData(64 * 64, 1.0f);

    XpeImageBuffer smallImg = {};
    smallImg.width = 64;
    smallImg.height = 64;
    smallImg.format = XPE_PIXEL_FLOAT32;
    smallImg.data = smallData.data();

    XpeErrorCode result = xpe_multiscale_process(&smallImg, &meta_, nullptr);
    EXPECT_EQ(result, XPE_OK);
}

/**
 * T-209: Test MFP with non-power-of-2 dimensions
 *
 * Given: Module is initialized with 100x150 image
 * When: xpe_multiscale_process() is called
 * Then: Function returns XPE_OK
 * And: Pyramid handles non-power-of-2 sizes correctly
 */
TEST_F(MfpScalarTest, NonPowerOf2Dimensions) {
    std::vector<float> np2Data(100 * 150, 1.0f);

    XpeImageBuffer np2Img = {};
    np2Img.width = 100;
    np2Img.height = 150;
    np2Img.format = XPE_PIXEL_FLOAT32;
    np2Img.data = np2Data.data();

    XpeErrorCode result = xpe_multiscale_process(&np2Img, &meta_, nullptr);
    EXPECT_EQ(result, XPE_OK);
}

/**
 * T-210: Test MFP invalid JSON configuration
 *
 * Given: Module is initialized
 * When: xpe_multiscale_process() is called with invalid JSON
 * Then: Function returns XPE_ERR_CONFIG_INVALID
 */
TEST_F(MfpScalarTest, InvalidJsonConfig) {
    const char* invalidJson = "{invalid json}";

    XpeErrorCode result = xpe_multiscale_process(&img_, &meta_, invalidJson);
    EXPECT_EQ(result, XPE_ERR_CONFIG_INVALID);
}

/**
 * T-211: Test MFP preserves mean pixel value (AC-MFP-004)
 *
 * Given: Module is initialized
 * When: MFP is applied
 * Then: Mean pixel value should not change significantly
 */
TEST_F(MfpScalarTest, PreservesMeanValue) {
    // Calculate original mean
    float originalMean = 0.0f;
    for (float pixel : testData_) {
        originalMean += pixel;
    }
    originalMean /= static_cast<float>(testData_.size());

    // Apply MFP
    XpeErrorCode result = xpe_multiscale_process(&img_, &meta_, nullptr);
    ASSERT_EQ(result, XPE_OK);

    // Calculate new mean
    float newMean = 0.0f;
    for (float pixel : testData_) {
        newMean += pixel;
    }
    newMean /= static_cast<float>(testData_.size());

    // Mean should not change more than 5%
    float meanChange = std::abs(newMean - originalMean) / (std::abs(originalMean) + 1e-6f);
    EXPECT_LT(meanChange, 0.05f);
}
