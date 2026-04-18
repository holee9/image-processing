/**
 * @file test_collimation_detect.cpp
 * @brief Google Test suite for collimation detection (T-048 ~ T-051)
 *
 * Tests for SWU-2.8 Collimation ROI Detection (SPEC-XPE-P2-ADV).
 * REQ-ADV-012, REQ-ADV-041, REQ-ADV-052
 */

#include "xpe/enhance_advanced/xpe_enhance_advanced_api.h"
#include "xpe/common/xpe_common_api.h"
#include "xpe/common/xpe_types.h"
#include <gtest/gtest.h>
#include <vector>
#include <cmath>

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class CollimationDetectTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize enhance_advanced module
        XpeErrorCode initResult = xpe_enhance_advanced_init(nullptr);
        ASSERT_EQ(XPE_OK, initResult);
    }

    void TearDown() override {
        // Cleanup
        xpe_enhance_advanced_shutdown();
        xpe_shutdown();
    }

    /**
     * @brief Helper to create a FLOAT32 image buffer
     * @param width Image width
     * @param height Image height
     * @param fillValue Value to fill pixels with
     * @return XpeImageBuffer structure
     */
    XpeImageBuffer createFloatImage(int width, int height, float fillValue = 0.0f) {
        XpeImageBuffer img;
        img.width = static_cast<uint32_t>(width);
        img.height = static_cast<uint32_t>(height);
        img.format = XPE_PIXEL_FLOAT32;
        img.stride = width * sizeof(float);

        std::vector<float> buffer(width * height, fillValue);
        imageData_.push_back(std::move(buffer));
        img.data = imageData_.back().data();

        return img;
    }

    /**
     * @brief Helper to create synthetic collimation borders
     * @param width Image width
     * @param height Image height
     * @param x0 Left border position
     * @param y0 Top border position
     * @param x1 Right border position
     * @param y1 Bottom border position
     * @param edgeStrength Edge pixel intensity
     * @return XpeImageBuffer with synthetic collimation
     */
    XpeImageBuffer createSyntheticCollimation(
        int width, int height,
        int x0, int y0, int x1, int y1,
        float edgeStrength = 1000.0f) {

        XpeImageBuffer img = createFloatImage(width, height, 100.0f);

        float* data = static_cast<float*>(img.data);

        // Draw left border
        if (x0 > 0) {
            for (int y = y0; y <= y1; ++y) {
                for (int x = std::max(0, x0 - 2); x <= std::min(width - 1, x0 + 2); ++x) {
                    data[y * width + x] = edgeStrength;
                }
            }
        }

        // Draw right border
        if (x1 < width - 1) {
            for (int y = y0; y <= y1; ++y) {
                for (int x = std::max(0, x1 - 2); x <= std::min(width - 1, x1 + 2); ++x) {
                    data[y * width + x] = edgeStrength;
                }
            }
        }

        // Draw top border
        if (y0 > 0) {
            for (int y = std::max(0, y0 - 2); y <= std::min(height - 1, y0 + 2); ++y) {
                for (int x = x0; x <= x1; ++x) {
                    data[y * width + x] = edgeStrength;
                }
            }
        }

        // Draw bottom border
        if (y1 < height - 1) {
            for (int y = std::max(0, y1 - 2); y <= std::min(height - 1, y1 + 2); ++y) {
                for (int x = x0; x <= x1; ++x) {
                    data[y * width + x] = edgeStrength;
                }
            }
        }

        return img;
    }

private:
    std::vector<std::vector<float>> imageData_;  ///< Keep image data alive during tests
};

/* ============================================================================
 * T-048: Input Validation Tests (AC-COL-003)
 * ============================================================================ */

TEST_F(CollimationDetectTest, NullOutputPointerReturnsInvalidInput) {
    // Arrange
    XpeImageBuffer img = createFloatImage(100, 100);
    int32_t x0, y0, x1, y1;

    // Act & Assert
    EXPECT_EQ(xpe_detect_collimation(nullptr, &x0, &y0, &x1, &y1, nullptr),
              XPE_ERR_INVALID_INPUT);
    EXPECT_EQ(xpe_detect_collimation(&img, nullptr, &y0, &x1, &y1, nullptr),
              XPE_ERR_INVALID_INPUT);
    EXPECT_EQ(xpe_detect_collimation(&img, &x0, nullptr, &x1, &y1, nullptr),
              XPE_ERR_INVALID_INPUT);
    EXPECT_EQ(xpe_detect_collimation(&img, &x0, &y0, nullptr, &y1, nullptr),
              XPE_ERR_INVALID_INPUT);
    EXPECT_EQ(xpe_detect_collimation(&img, &x0, &y0, &x1, nullptr, nullptr),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(CollimationDetectTest, WrongPixelFormatReturnsUnsupportedFormat) {
    // Arrange
    XpeImageBuffer img = createFloatImage(100, 100);
    img.format = XPE_PIXEL_UINT8;  // Wrong format
    int32_t x0, y0, x1, y1;

    // Act & Assert
    EXPECT_EQ(xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr),
              XPE_ERR_UNSUPPORTED_FORMAT);
}

TEST_F(CollimationDetectTest, ZeroDimensionsReturnInvalidInput) {
    // Arrange
    XpeImageBuffer img1 = createFloatImage(0, 100);
    XpeImageBuffer img2 = createFloatImage(100, 0);
    int32_t x0, y0, x1, y1;

    // Act & Assert
    EXPECT_EQ(xpe_detect_collimation(&img1, &x0, &y0, &x1, &y1, nullptr),
              XPE_ERR_INVALID_INPUT);
    EXPECT_EQ(xpe_detect_collimation(&img2, &x0, &y0, &x1, &y1, nullptr),
              XPE_ERR_INVALID_INPUT);
}

/* ============================================================================
 * T-049: Synthetic Collimation Detection (AC-COL-001)
 * REQ-ADV-052: +-3 pixel accuracy
 * ============================================================================ */

TEST_F(CollimationDetectTest, SyntheticBordersDetectedWithPlusMinus3PixelAccuracy) {
    // Arrange
    const int width = 512;
    const int height = 512;
    const int x0_gt = 50;   // Ground truth left border
    const int y0_gt = 60;   // Ground truth top border
    const int x1_gt = 450;  // Ground truth right border
    const int y1_gt = 420;  // Ground truth bottom border

    XpeImageBuffer img = createSyntheticCollimation(
        width, height, x0_gt, y0_gt, x1_gt, y1_gt, 1000.0f);

    int32_t x0, y0, x1, y1;

    // Act
    XpeErrorCode result = xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr);

    // Assert
    EXPECT_EQ(XPE_OK, result);

    // REQ-ADV-052: +-3 pixel accuracy
    EXPECT_NEAR(x0, x0_gt, 3) << "Left border detection error exceeds 3 pixels";
    EXPECT_NEAR(y0, y0_gt, 3) << "Top border detection error exceeds 3 pixels";
    EXPECT_NEAR(x1, x1_gt, 3) << "Right border detection error exceeds 3 pixels";
    EXPECT_NEAR(y1, y1_gt, 3) << "Bottom border detection error exceeds 3 pixels";

    // Verify rectangle is valid
    EXPECT_LT(x0, x1) << "Left boundary should be less than right boundary";
    EXPECT_LT(y0, y1) << "Top boundary should be less than bottom boundary";
    EXPECT_GE(x0, 0) << "Left boundary should be non-negative";
    EXPECT_GE(y0, 0) << "Top boundary should be non-negative";
    EXPECT_LT(x1, width) << "Right boundary should be within image width";
    EXPECT_LT(y1, height) << "Bottom boundary should be within image height";
}

TEST_F(CollimationDetectTest, FullImageCollimationDetected) {
    // Arrange
    const int width = 256;
    const int height = 256;

    // Collimation at image edges
    XpeImageBuffer img = createSyntheticCollimation(
        width, height, 0, 0, width - 1, height - 1, 1000.0f);

    int32_t x0, y0, x1, y1;

    // Act
    XpeErrorCode result = xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr);

    // Assert
    EXPECT_EQ(XPE_OK, result);
    EXPECT_NEAR(x0, 0, 3);
    EXPECT_NEAR(y0, 0, 3);
    EXPECT_NEAR(x1, width - 1, 3);
    EXPECT_NEAR(y1, height - 1, 3);
}

/* ============================================================================
 * T-050: No-Collimation Fallback (AC-COL-002)
 * REQ-ADV-041: Confidence < 0.7 returns full extent
 * ============================================================================ */

TEST_F(CollimationDetectTest, UniformImageReturnsFullImageExtent) {
    // Arrange
    const int width = 200;
    const int height = 200;

    // Uniform image with no edges
    XpeImageBuffer img = createFloatImage(width, height, 500.0f);

    int32_t x0, y0, x1, y1;

    // Act
    XpeErrorCode result = xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr);

    // Assert
    EXPECT_EQ(XPE_OK, result);

    // REQ-ADV-041: Low confidence -> full image extent
    EXPECT_EQ(x0, 0) << "Low confidence should return x0 = 0";
    EXPECT_EQ(y0, 0) << "Low confidence should return y0 = 0";
    EXPECT_EQ(x1, width - 1) << "Low confidence should return x1 = width - 1";
    EXPECT_EQ(y1, height - 1) << "Low confidence should return y1 = height - 1";
}

TEST_F(CollimationDetectTest, LowContrastImageReturnsFullImageExtent) {
    // Arrange
    const int width = 300;
    const int height = 300;

    // Low contrast collimation (weak edges)
    XpeImageBuffer img = createSyntheticCollimation(
        width, height, 50, 50, 250, 250, 50.0f);  // Weak edge strength

    int32_t x0, y0, x1, y1;

    // Act
    XpeErrorCode result = xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr);

    // Assert
    EXPECT_EQ(XPE_OK, result);

    // Low confidence due to weak edges -> full extent
    EXPECT_EQ(x0, 0);
    EXPECT_EQ(y0, 0);
    EXPECT_EQ(x1, width - 1);
    EXPECT_EQ(y1, height - 1);
}

/* ============================================================================
 * T-051: Boundary Clipping (AC-COL-004)
 * REQ-ADV-052: Boundaries clipped to image bounds
 * ============================================================================ */

TEST_F(CollimationDetectTest, DetectedBordersClippedToImageBounds) {
    // Arrange
    const int width = 100;
    const int height = 100;

    // Create collimation that extends beyond image bounds
    XpeImageBuffer img = createSyntheticCollimation(
        width, height, -10, -10, 110, 110, 1000.0f);

    int32_t x0, y0, x1, y1;

    // Act
    XpeErrorCode result = xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr);

    // Assert
    EXPECT_EQ(XPE_OK, result);

    // REQ-ADV-052: Boundaries clipped to [0, width) and [0, height)
    EXPECT_GE(x0, 0) << "x0 should be clipped to >= 0";
    EXPECT_GE(y0, 0) << "y0 should be clipped to >= 0";
    EXPECT_LT(x1, width) << "x1 should be clipped to < width";
    EXPECT_LT(y1, height) << "y1 should be clipped to < height";
}

TEST_F(CollimationDetectTest, PartialBordersHandledCorrectly) {
    // Arrange
    const int width = 200;
    const int height = 200;

    // Only left and top borders present
    XpeImageBuffer img = createFloatImage(width, height, 100.0f);
    float* data = static_cast<float*>(img.data);

    // Draw strong left and top borders only
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < 5; ++x) {
            data[y * width + x] = 1000.0f;
        }
    }
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < width; ++x) {
            data[y * width + x] = 1000.0f;
        }
    }

    int32_t x0, y0, x1, y1;

    // Act
    XpeErrorCode result = xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr);

    // Assert
    EXPECT_EQ(XPE_OK, result);

    // Should detect partial borders and fall back to full extent
    // (insufficient lines for complete rectangle)
    EXPECT_EQ(x0, 0);
    EXPECT_EQ(y0, 0);
    EXPECT_EQ(x1, width - 1);
    EXPECT_EQ(y1, height - 1);
}

/* ============================================================================
 * Additional Tests
 * ============================================================================ */

TEST_F(CollimationDetectTest, SmallImageHandledCorrectly) {
    // Arrange
    const int width = 50;
    const int height = 50;

    XpeImageBuffer img = createSyntheticCollimation(
        width, height, 5, 5, 45, 45, 500.0f);

    int32_t x0, y0, x1, y1;

    // Act
    XpeErrorCode result = xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr);

    // Assert
    EXPECT_EQ(XPE_OK, result);

    // Should detect collimation within accuracy bounds
    EXPECT_NEAR(x0, 5, 3);
    EXPECT_NEAR(y0, 5, 3);
    EXPECT_NEAR(x1, 45, 3);
    EXPECT_NEAR(y1, 45, 3);
}

TEST_F(CollimationDetectTest, LargeImagePerformance) {
    // Arrange
    const int width = 2048;
    const int height = 2048;

    // Large image with collimation borders
    XpeImageBuffer img = createSyntheticCollimation(
        width, height, 100, 100, 1900, 1900, 800.0f);

    int32_t x0, y0, x1, y1;

    // Act
    auto start = std::chrono::high_resolution_clock::now();
    XpeErrorCode result = xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr);
    auto end = std::chrono::high_resolution_clock::now();

    // Assert
    EXPECT_EQ(XPE_OK, result);

    // REQ-ADV-062: Performance budget < 500ms for 3072x3072
    // 2048x2048 should be faster
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
    EXPECT_LT(duration.count(), 500) << "Collimation detection exceeds 500ms for 2048x2048 image";

    // Verify detection accuracy
    EXPECT_NEAR(x0, 100, 10);
    EXPECT_NEAR(y0, 100, 10);
    EXPECT_NEAR(x1, 1900, 10);
    EXPECT_NEAR(y1, 1900, 10);
}
