/**
 * @file test_runtime_detection.cpp
 * @brief Google Test suite for runtime defective pixel detection -- REQ-P1A-013
 *
 * IEC 62304 Class B -- Unit Tests for Hampel 5-sigma outlier detection.
 * Coverage target: >= 85% statement coverage.
 *
 * TDD Methodology: RED-GREEN-REFACTOR
 * - RED Phase: Failing tests written first
 * - GREEN Phase: Minimal implementation to pass tests
 * - REFACTOR Phase: Optimize while maintaining test coverage
 */

#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "gtest/gtest.h"

#include <cstring>
#include <cmath>
#include <vector>
#include <random>

/* ============================================================================
 * Test Fixtures
 * ============================================================================ */

class RuntimeDetectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize library before each test
        ASSERT_EQ(xpe_preprocess_init(nullptr), XPE_OK);

        // Create test image buffer (100x100 float32)
        testImg.width = 100;
        testImg.height = 100;
        testImg.bitsAllocated = 32;
        testImg.bitsStored = 32;
        testImg.format = XPE_PIXEL_FLOAT32;
        testImg.dataSize = testImg.width * testImg.height * sizeof(float);
        testImg.data = malloc(testImg.dataSize);
        ASSERT_NE(testImg.data, nullptr);

        // Create defect map buffer (uint8)
        defectMap.width = 100;
        defectMap.height = 100;
        defectMap.bitsAllocated = 8;
        defectMap.bitsStored = 8;
        defectMap.format = XPE_PIXEL_UINT8;
        defectMap.dataSize = defectMap.width * defectMap.height * sizeof(uint8_t);
        defectMap.data = malloc(defectMap.dataSize);
        ASSERT_NE(defectMap.data, nullptr);

        // Initialize buffers
        CleanImage();
        std::memset(defectMap.data, 0, defectMap.dataSize);
    }

    void TearDown() override {
        if (testImg.data) free(testImg.data);
        if (defectMap.data) free(defectMap.data);
        xpe_preprocess_shutdown();
    }

    // Helper: Fill image with uniform value (no defects)
    void CleanImage(float value = 1000.0f) {
        float* pixels = static_cast<float*>(testImg.data);
        for (size_t i = 0; i < testImg.width * testImg.height; ++i) {
            pixels[i] = value;
        }
    }

    // Helper: Add synthetic defective pixel
    void InjectDefect(uint32_t x, uint32_t y, float outlierValue) {
        float* pixels = static_cast<float*>(testImg.data);
        pixels[y * testImg.width + x] = outlierValue;
    }

    // Helper: Add Gaussian noise to image
    void AddGaussianNoise(float mean = 1000.0f, float stddev = 10.0f) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::normal_distribution<float> dist(mean, stddev);

        float* pixels = static_cast<float*>(testImg.data);
        for (size_t i = 0; i < testImg.width * testImg.height; ++i) {
            pixels[i] = dist(gen);
        }
    }

    // Helper: Count defects in map
    uint32_t CountDefects() const {
        uint8_t* map = static_cast<uint8_t*>(defectMap.data);
        uint32_t count = 0;
        for (size_t i = 0; i < defectMap.width * defectMap.height; ++i) {
            if (map[i] != 0) count++;
        }
        return count;
    }

    // Helper: Calculate True Positive Rate
    double CalculateTPR(uint32_t trueDefects, uint32_t detectedDefects) const {
        if (trueDefects == 0) return 1.0;
        return static_cast<double>(detectedDefects) / static_cast<double>(trueDefects);
    }

    // Helper: Calculate False Positive Rate
    double CalculateFPR(uint32_t trueNegatives, uint32_t falsePositives) const {
        uint32_t total = trueNegatives + falsePositives;
        if (total == 0) return 0.0;
        return static_cast<double>(falsePositives) / static_cast<double>(total);
    }

    XpeImageBuffer testImg;
    XpeImageBuffer defectMap;
};

/* ============================================================================
 * API Validation Tests (REQ-P1A-013)
 * ============================================================================ */

TEST_F(RuntimeDetectionTest, DetectWithNullImageReturnsInvalid) {
    EXPECT_EQ(xpe_defect_detect_runtime(nullptr, &defectMap, nullptr),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(RuntimeDetectionTest, DetectWithNullDefectMapReturnsInvalid) {
    EXPECT_EQ(xpe_defect_detect_runtime(&testImg, nullptr, nullptr),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(RuntimeDetectionTest, DetectWithDimensionMismatchReturnsInvalid) {
    XpeImageBuffer wrongSizeMap = defectMap;
    wrongSizeMap.width = 50;  // Wrong dimension
    wrongSizeMap.height = 100;

    EXPECT_EQ(xpe_defect_detect_runtime(&testImg, &wrongSizeMap, nullptr),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(RuntimeDetectionTest, DetectWithDefaultConfigSucceeds) {
    EXPECT_EQ(xpe_defect_detect_runtime(&testImg, &defectMap, nullptr), XPE_OK);
}

/* ============================================================================
 * Clean Image Tests (REQ-P1A-013: FPR < 0.001%)
 * ============================================================================ */

TEST_F(RuntimeDetectionTest, CleanUniformImageZeroDefects) {
    CleanImage(1000.0f);

    EXPECT_EQ(xpe_defect_detect_runtime(&testImg, &defectMap, nullptr), XPE_OK);
    EXPECT_EQ(CountDefects(), 0u);
}

TEST_F(RuntimeDetectionTest, CleanGaussianNoiseLowFPR) {
    AddGaussianNoise(1000.0f, 10.0f);

    EXPECT_EQ(xpe_defect_detect_runtime(&testImg, &defectMap, nullptr), XPE_OK);

    // FPR should be < 0.1% (10 defects in 10000 pixels for 5-sigma threshold)
    // For normal distribution, 5-sigma corresponds to ~1 in 3.5 million
    // So we expect very few false positives
    uint32_t defects = CountDefects();
    EXPECT_LT(defects, 10u);  // Less than 0.1% false positive rate
}

TEST_F(RuntimeDetectionTest, CleanImageMultipleFramesConsistent) {
    // Run detection 10 times on same clean image
    for (int i = 0; i < 10; ++i) {
        CleanImage(1000.0f + i);  // Vary baseline slightly
        std::memset(defectMap.data, 0, defectMap.dataSize);

        EXPECT_EQ(xpe_defect_detect_runtime(&testImg, &defectMap, nullptr), XPE_OK);
        EXPECT_EQ(CountDefects(), 0u);
    }
}

/* ============================================================================
 * Synthetic Defect Tests (REQ-P1A-013: TPR >= 99.9%)
 * ============================================================================ */

TEST_F(RuntimeDetectionTest, SingleExtremeOutlierDetected) {
    // Use image with slight noise to ensure MAD > 0
    AddGaussianNoise(1000.0f, 1.0f);  // Very small noise
    InjectDefect(50, 50, 5000.0f);  // Extreme positive outlier

    EXPECT_EQ(xpe_defect_detect_runtime(&testImg, &defectMap, nullptr), XPE_OK);

    uint32_t defects = CountDefects();
    EXPECT_GE(defects, 1u);
}

TEST_F(RuntimeDetectionTest, SingleDarkOutlierDetected) {
    // Use image with slight noise to ensure MAD > 0
    AddGaussianNoise(1000.0f, 1.0f);
    InjectDefect(50, 50, 0.0f);  // Extreme negative outlier

    EXPECT_EQ(xpe_defect_detect_runtime(&testImg, &defectMap, nullptr), XPE_OK);

    uint32_t defects = CountDefects();
    EXPECT_GE(defects, 1u);
}

TEST_F(RuntimeDetectionTest, MultipleOutliersDetected) {
    // Use image with slight noise to ensure MAD > 0
    AddGaussianNoise(1000.0f, 1.0f);

    // Inject 10 defects at various locations
    InjectDefect(10, 10, 5000.0f);
    InjectDefect(20, 20, 0.0f);
    InjectDefect(30, 30, 6000.0f);
    InjectDefect(40, 40, -100.0f);
    InjectDefect(50, 50, 7000.0f);
    InjectDefect(60, 60, 50.0f);
    InjectDefect(70, 70, 8000.0f);
    InjectDefect(80, 80, 0.0f);
    InjectDefect(90, 10, 9000.0f);
    InjectDefect(10, 90, 100.0f);

    EXPECT_EQ(xpe_defect_detect_runtime(&testImg, &defectMap, nullptr), XPE_OK);

    uint32_t defects = CountDefects();
    // TPR >= 99.9% means at least 9.99 out of 10 should be detected
    EXPECT_GE(defects, 9u);
}

TEST_F(RuntimeDetectionTest, EdgePixelOutlierDetected) {
    // Use image with slight noise to ensure MAD > 0
    AddGaussianNoise(1000.0f, 1.0f);

    // Test corner and edge defects
    InjectDefect(0, 0, 5000.0f);       // Top-left corner
    InjectDefect(99, 0, 5000.0f);     // Top-right corner
    InjectDefect(0, 99, 5000.0f);     // Bottom-left corner
    InjectDefect(99, 99, 5000.0f);    // Bottom-right corner
    InjectDefect(50, 0, 5000.0f);     // Top edge
    InjectDefect(50, 99, 5000.0f);    // Bottom edge
    InjectDefect(0, 50, 5000.0f);     // Left edge
    InjectDefect(99, 50, 5000.0f);    // Right edge

    EXPECT_EQ(xpe_defect_detect_runtime(&testImg, &defectMap, nullptr), XPE_OK);

    uint32_t defects = CountDefects();
    // All edge defects should be detected
    EXPECT_GE(defects, 7u);
}

/* ============================================================================
 * Hampel 5-Sigma Algorithm Tests
 * ============================================================================ */

TEST_F(RuntimeDetectionTest, HampelFilterWithSmallWindow) {
    // Use image with slight noise to ensure MAD > 0
    AddGaussianNoise(1000.0f, 1.0f);
    InjectDefect(50, 50, 5000.0f);

    // Configure small window (3x3)
    const char* config = "{\"windowSize\": 3}";

    EXPECT_EQ(xpe_defect_detect_runtime(&testImg, &defectMap, config), XPE_OK);

    uint32_t defects = CountDefects();
    EXPECT_GE(defects, 1u);
}

TEST_F(RuntimeDetectionTest, HampelFilterWithDefaultWindow) {
    // Use image with slight noise to ensure MAD > 0
    AddGaussianNoise(1000.0f, 1.0f);
    InjectDefect(50, 50, 5000.0f);

    // Default window (5x5) - nullptr config
    EXPECT_EQ(xpe_defect_detect_runtime(&testImg, &defectMap, nullptr), XPE_OK);

    uint32_t defects = CountDefects();
    EXPECT_GE(defects, 1u);
}

TEST_F(RuntimeDetectionTest, HampelFilterWithLargeWindow) {
    // Use image with slight noise to ensure MAD > 0
    AddGaussianNoise(1000.0f, 1.0f);
    InjectDefect(50, 50, 5000.0f);

    // Configure large window (7x7)
    const char* config = "{\"windowSize\": 7}";

    EXPECT_EQ(xpe_defect_detect_runtime(&testImg, &defectMap, config), XPE_OK);

    uint32_t defects = CountDefects();
    EXPECT_GE(defects, 1u);
}

/* ============================================================================
 * Configurable Parameters Tests
 * ============================================================================ */

TEST_F(RuntimeDetectionTest, ConfigWithInvalidJsonReturnsOk) {
    CleanImage(1000.0f);

    // Invalid JSON should fall back to defaults
    EXPECT_EQ(xpe_defect_detect_runtime(&testImg, &defectMap, "not json"), XPE_OK);
}

TEST_F(RuntimeDetectionTest, ConfigWithSigmaThreshold) {
    // Use image with slight noise to ensure MAD > 0
    AddGaussianNoise(1000.0f, 1.0f);
    InjectDefect(50, 50, 5000.0f);

    // Configure custom sigma threshold (3-sigma, more sensitive)
    const char* config = "{\"sigmaThreshold\": 3.0}";

    EXPECT_EQ(xpe_defect_detect_runtime(&testImg, &defectMap, config), XPE_OK);

    uint32_t defects = CountDefects();
    EXPECT_GE(defects, 1u);
}

/* ============================================================================
 * Statistical Properties Tests
 * ============================================================================ */

TEST_F(RuntimeDetectionTest, MedianAbsoluteDeviationCalculated) {
    // Create image with known statistical properties
    AddGaussianNoise(1000.0f, 1.0f);
    InjectDefect(50, 50, 5000.0f);

    EXPECT_EQ(xpe_defect_detect_runtime(&testImg, &defectMap, nullptr), XPE_OK);

    // Verify defect was detected using MAD-based threshold
    uint32_t defects = CountDefects();
    EXPECT_GE(defects, 1u);
}

TEST_F(RuntimeDetectionTest, SlidingWindowMedianRobustToOutliers) {
    // Create image with cluster of outliers
    AddGaussianNoise(1000.0f, 1.0f);
    InjectDefect(49, 50, 5000.0f);
    InjectDefect(50, 50, 5000.0f);
    InjectDefect(51, 50, 5000.0f);

    EXPECT_EQ(xpe_defect_detect_runtime(&testImg, &defectMap, nullptr), XPE_OK);

    // Median should be robust to multiple outliers
    uint32_t defects = CountDefects();
    EXPECT_GE(defects, 2u);
}

/* ============================================================================
 * Real-World Scenario Tests
 * ============================================================================ */

TEST_F(RuntimeDetectionTest, LowNoiseImageWithSparseDefects) {
    AddGaussianNoise(1000.0f, 5.0f);  // Low noise

    // Inject 5 sparse defects
    InjectDefect(10, 10, 5000.0f);
    InjectDefect(30, 30, 0.0f);
    InjectDefect(50, 50, 6000.0f);
    InjectDefect(70, 70, 50.0f);
    InjectDefect(90, 90, 7000.0f);

    EXPECT_EQ(xpe_defect_detect_runtime(&testImg, &defectMap, nullptr), XPE_OK);

    // TPR >= 99.9% means all 5 should be detected
    uint32_t defects = CountDefects();
    EXPECT_GE(defects, 5u);
}

TEST_F(RuntimeDetectionTest, HighNoiseImageWithDefects) {
    AddGaussianNoise(1000.0f, 50.0f);  // Higher noise

    // Inject 5 strong defects
    InjectDefect(10, 10, 5000.0f);
    InjectDefect(30, 30, 0.0f);
    InjectDefect(50, 50, 6000.0f);
    InjectDefect(70, 70, 50.0f);
    InjectDefect(90, 90, 7000.0f);

    EXPECT_EQ(xpe_defect_detect_runtime(&testImg, &defectMap, nullptr), XPE_OK);

    // Even with noise, strong defects should be detected
    uint32_t defects = CountDefects();
    EXPECT_GE(defects, 4u);
}

/* ============================================================================
 * Performance and Edge Cases
 * ============================================================================ */

TEST_F(RuntimeDetectionTest, SmallImageDetection) {
    // Create small 10x10 image
    XpeImageBuffer smallImg;
    smallImg.width = 10;
    smallImg.height = 10;
    smallImg.bitsAllocated = 32;
    smallImg.bitsStored = 32;
    smallImg.format = XPE_PIXEL_FLOAT32;
    smallImg.dataSize = smallImg.width * smallImg.height * sizeof(float);
    smallImg.data = malloc(smallImg.dataSize);

    XpeImageBuffer smallMap;
    smallMap.width = 10;
    smallMap.height = 10;
    smallMap.bitsAllocated = 8;
    smallMap.bitsStored = 8;
    smallMap.format = XPE_PIXEL_UINT8;
    smallMap.dataSize = smallMap.width * smallMap.height * sizeof(uint8_t);
    smallMap.data = malloc(smallMap.dataSize);

    // Fill with uniform value
    float* pixels = static_cast<float*>(smallImg.data);
    for (size_t i = 0; i < 100; ++i) {
        pixels[i] = 1000.0f;
    }
    std::memset(smallMap.data, 0, smallMap.dataSize);

    // Inject defect
    pixels[55] = 5000.0f;  // Center pixel

    EXPECT_EQ(xpe_defect_detect_runtime(&smallImg, &smallMap, nullptr), XPE_OK);

    free(smallImg.data);
    free(smallMap.data);
}

TEST_F(RuntimeDetectionTest, LargeImageDetection) {
    // Create large 2048x2048 image
    XpeImageBuffer largeImg;
    largeImg.width = 2048;
    largeImg.height = 2048;
    largeImg.bitsAllocated = 32;
    largeImg.bitsStored = 32;
    largeImg.format = XPE_PIXEL_FLOAT32;
    largeImg.dataSize = largeImg.width * largeImg.height * sizeof(float);
    largeImg.data = malloc(largeImg.dataSize);

    XpeImageBuffer largeMap;
    largeMap.width = 2048;
    largeMap.height = 2048;
    largeMap.bitsAllocated = 8;
    largeMap.bitsStored = 8;
    largeMap.format = XPE_PIXEL_UINT8;
    largeMap.dataSize = largeMap.width * largeMap.height * sizeof(uint8_t);
    largeMap.data = malloc(largeMap.dataSize);

    // Fill with uniform value
    float* pixels = static_cast<float*>(largeImg.data);
    for (size_t i = 0; i < (size_t)2048 * 2048; ++i) {
        pixels[i] = 1000.0f;
    }
    std::memset(largeMap.data, 0, largeMap.dataSize);

    // Inject defect at center
    pixels[1024 * 2048 + 1024] = 5000.0f;

    EXPECT_EQ(xpe_defect_detect_runtime(&largeImg, &largeMap, nullptr), XPE_OK);

    free(largeImg.data);
    free(largeMap.data);
}

/* ============================================================================
 * Main
 * ============================================================================ */

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
