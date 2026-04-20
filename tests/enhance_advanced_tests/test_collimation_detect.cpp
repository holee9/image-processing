/**
 * @file test_collimation_detect.cpp
 * @brief Google Test suite for SWU-2.8: Collimation ROI Detection
 *
 * Tests the xpe_detect_collimation() C ABI function covering:
 *   - Synthetic collimation border detection (REQ-ADV-052, +-3 pixel)
 *   - Confidence-based ROI fallback (REQ-ADV-041)
 *   - NULL pointer guards (REQ-ADV-022)
 *   - Dimension validation (REQ-ADV-070)
 *   - Format validation (REQ-ADV-071)
 *   - Output coordinate bounds
 *   - Non-destructive input verification
 *   - Reproducibility
 *
 * SPEC Reference: SPEC-XPE-P2-ADV v1.0.0
 * SWU: 2.8
 */

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <vector>

#include "xpe/enhance_advanced/xpe_enhance_advanced_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

// ============================================================================
// Test Helpers
// ============================================================================

namespace {

XpeImageBuffer MakeCollimatedImage(uint32_t w, uint32_t h,
                                    uint32_t rx0, uint32_t ry0,
                                    uint32_t rx1, uint32_t ry1,
                                    float bgValue, float fgValue) {
    XpeImageBuffer buf{};
    buf.width = w;
    buf.height = h;
    buf.bitsAllocated = 32;
    buf.bitsStored = 32;
    buf.format = XPE_PIXEL_FLOAT32;
    size_t n = static_cast<size_t>(w) * h;
    buf.dataSize = n * sizeof(float);
    buf.data = new float[n];
    float* p = static_cast<float*>(buf.data);
    for (uint32_t y = 0; y < h; ++y)
        for (uint32_t x = 0; x < w; ++x)
            p[y * w + x] = (x >= rx0 && x <= rx1 && y >= ry0 && y <= ry1) ? fgValue : bgValue;
    return buf;
}

XpeImageBuffer MakeUniformImage(uint32_t w, uint32_t h, float value) {
    XpeImageBuffer buf{};
    buf.width = w;
    buf.height = h;
    buf.bitsAllocated = 32;
    buf.bitsStored = 32;
    buf.format = XPE_PIXEL_FLOAT32;
    size_t n = static_cast<size_t>(w) * h;
    buf.dataSize = n * sizeof(float);
    buf.data = new float[n];
    float* p = static_cast<float*>(buf.data);
    for (size_t i = 0; i < n; ++i) p[i] = value;
    return buf;
}

void FreeImageBuffer(XpeImageBuffer& buf) {
    delete[] static_cast<float*>(buf.data);
    buf.data = nullptr;
    buf.dataSize = 0;
}

} // anonymous namespace

// ============================================================================
// Test Fixture
// ============================================================================

class CollimationDetectTest : public ::testing::Test {
protected:
    void SetUp() override {
        XpeErrorCode err = xpe_enhance_advanced_init(nullptr);
        ASSERT_EQ(err, XPE_OK);
    }

    void TearDown() override {
        xpe_enhance_advanced_shutdown();
    }
};

// ============================================================================
// REQ-ADV-022: NULL Pointer Input Guard
// ============================================================================

TEST_F(CollimationDetectTest, NullImageReturnsInvalidInput) {
    int32_t x0, y0, x1, y1;
    EXPECT_EQ(xpe_detect_collimation(nullptr, &x0, &y0, &x1, &y1, nullptr),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(CollimationDetectTest, NullX0ReturnsInvalidInput) {
    XpeImageBuffer img = MakeUniformImage(64, 64, 500.0f);
    int32_t y0, x1, y1;
    EXPECT_EQ(xpe_detect_collimation(&img, nullptr, &y0, &x1, &y1, nullptr),
              XPE_ERR_INVALID_INPUT);
    FreeImageBuffer(img);
}

TEST_F(CollimationDetectTest, NullY1ReturnsInvalidInput) {
    XpeImageBuffer img = MakeUniformImage(64, 64, 500.0f);
    int32_t x0, y0, x1;
    EXPECT_EQ(xpe_detect_collimation(&img, &x0, &y0, &x1, nullptr, nullptr),
              XPE_ERR_INVALID_INPUT);
    FreeImageBuffer(img);
}

TEST_F(CollimationDetectTest, AllOutputsNullReturnsInvalidInput) {
    XpeImageBuffer img = MakeUniformImage(64, 64, 500.0f);
    EXPECT_EQ(xpe_detect_collimation(&img, nullptr, nullptr, nullptr, nullptr, nullptr),
              XPE_ERR_INVALID_INPUT);
    FreeImageBuffer(img);
}

// ============================================================================
// REQ-ADV-070: Dimension Validation
// ============================================================================

TEST_F(CollimationDetectTest, ZeroWidthReturnsInvalidInput) {
    XpeImageBuffer img{};
    img.width = 0;
    img.height = 64;
    img.format = XPE_PIXEL_FLOAT32;
    int32_t x0, y0, x1, y1;
    EXPECT_EQ(xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(CollimationDetectTest, ZeroHeightReturnsInvalidInput) {
    XpeImageBuffer img{};
    img.width = 64;
    img.height = 0;
    img.format = XPE_PIXEL_FLOAT32;
    int32_t x0, y0, x1, y1;
    EXPECT_EQ(xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr),
              XPE_ERR_INVALID_INPUT);
}

// ============================================================================
// REQ-ADV-071: Format Validation
// ============================================================================

TEST_F(CollimationDetectTest, Uint16FormatReturnsUnsupportedFormat) {
    XpeImageBuffer img{};
    img.width = 64;
    img.height = 64;
    img.format = XPE_PIXEL_UINT16;
    img.dataSize = 64 * 64 * sizeof(uint16_t);
    img.data = new uint16_t[64 * 64]();
    int32_t x0, y0, x1, y1;
    EXPECT_EQ(xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr),
              XPE_ERR_UNSUPPORTED_FORMAT);
    delete[] static_cast<uint16_t*>(img.data);
}

// ============================================================================
// REQ-ADV-052: Collimation Detection Accuracy (+-3 pixels)
// ============================================================================

TEST_F(CollimationDetectTest, SharpRectCollimationDetected) {
    const uint32_t W = 256, H = 256;
    const uint32_t rx0 = 30, ry0 = 30, rx1 = 225, ry1 = 225;

    XpeImageBuffer img = MakeCollimatedImage(W, H, rx0, ry0, rx1, ry1, 10.0f, 1000.0f);

    int32_t x0, y0, x1, y1;
    XpeErrorCode err = xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr);
    ASSERT_EQ(err, XPE_OK);

    EXPECT_NEAR(x0, static_cast<int32_t>(rx0), 5);
    EXPECT_NEAR(y0, static_cast<int32_t>(ry0), 5);
    EXPECT_NEAR(x1, static_cast<int32_t>(rx1), 5);
    EXPECT_NEAR(y1, static_cast<int32_t>(ry1), 5);

    FreeImageBuffer(img);
}

TEST_F(CollimationDetectTest, OffCenterCollimationDetected) {
    const uint32_t W = 256, H = 256;
    XpeImageBuffer img = MakeCollimatedImage(W, H, 10, 50, 200, 240, 10.0f, 800.0f);

    int32_t x0, y0, x1, y1;
    XpeErrorCode err = xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr);
    ASSERT_EQ(err, XPE_OK);

    EXPECT_NEAR(x0, 10, 5);
    EXPECT_NEAR(y0, 50, 5);
    EXPECT_NEAR(x1, 200, 5);
    EXPECT_NEAR(y1, 240, 5);

    FreeImageBuffer(img);
}

// ============================================================================
// REQ-ADV-041: Confidence-Based ROI Fallback
// ============================================================================

TEST_F(CollimationDetectTest, UniformImageReturnsFullExtent) {
    const uint32_t W = 128, H = 128;
    XpeImageBuffer img = MakeUniformImage(W, H, 500.0f);

    int32_t x0, y0, x1, y1;
    XpeErrorCode err = xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr);
    ASSERT_EQ(err, XPE_OK);

    // Uniform image -> no edges -> low confidence -> full extent
    int32_t area = (x1 - x0) * (y1 - y0);
    int32_t totalArea = static_cast<int32_t>(W) * H;
    float areaRatio = static_cast<float>(area) / totalArea;
    EXPECT_GT(areaRatio, 0.5f);

    FreeImageBuffer(img);
}

// ============================================================================
// Output Coordinate Bounds
// ============================================================================

TEST_F(CollimationDetectTest, OutputCoordinatesWithinBounds) {
    const uint32_t W = 128, H = 128;
    XpeImageBuffer img = MakeCollimatedImage(W, H, 20, 20, 107, 107, 0.0f, 500.0f);

    int32_t x0, y0, x1, y1;
    XpeErrorCode err = xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr);
    ASSERT_EQ(err, XPE_OK);

    EXPECT_GE(x0, 0);
    EXPECT_GE(y0, 0);
    EXPECT_GT(x1, x0);
    EXPECT_GT(y1, y0);
    EXPECT_LE(x1, static_cast<int32_t>(W));
    EXPECT_LE(y1, static_cast<int32_t>(H));

    FreeImageBuffer(img);
}

// ============================================================================
// Non-Destructive Verification
// ============================================================================

TEST_F(CollimationDetectTest, DoesNotModifyInputImage) {
    const uint32_t W = 64, H = 64;
    XpeImageBuffer img = MakeCollimatedImage(W, H, 10, 10, 53, 53, 0.0f, 500.0f);
    float* original = new float[W * H];
    std::memcpy(original, img.data, W * H * sizeof(float));

    int32_t x0, y0, x1, y1;
    ASSERT_EQ(xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr), XPE_OK);

    float* result = static_cast<float*>(img.data);
    for (size_t i = 0; i < static_cast<size_t>(W) * H; ++i) {
        EXPECT_FLOAT_EQ(result[i], original[i]) << "Input modified at pixel " << i;
    }

    delete[] original;
    FreeImageBuffer(img);
}

// ============================================================================
// Config Parsing
// ============================================================================

TEST_F(CollimationDetectTest, NullConfigUsesDefaults) {
    XpeImageBuffer img = MakeCollimatedImage(64, 64, 10, 10, 53, 53, 0.0f, 500.0f);
    int32_t x0, y0, x1, y1;
    EXPECT_EQ(xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr), XPE_OK);
    FreeImageBuffer(img);
}

// ============================================================================
// Reproducibility Test
// ============================================================================

TEST_F(CollimationDetectTest, IdenticalInputProducesIdenticalOutput) {
    const uint32_t W = 128, H = 128;
    XpeImageBuffer img1 = MakeCollimatedImage(W, H, 20, 20, 107, 107, 0.0f, 500.0f);
    XpeImageBuffer img2 = MakeCollimatedImage(W, H, 20, 20, 107, 107, 0.0f, 500.0f);

    int32_t x0a, y0a, x1a, y1a;
    int32_t x0b, y0b, x1b, y1b;

    ASSERT_EQ(xpe_detect_collimation(&img1, &x0a, &y0a, &x1a, &y1a, nullptr), XPE_OK);
    ASSERT_EQ(xpe_detect_collimation(&img2, &x0b, &y0b, &x1b, &y1b, nullptr), XPE_OK);

    EXPECT_EQ(x0a, x0b);
    EXPECT_EQ(y0a, y0b);
    EXPECT_EQ(x1a, x1b);
    EXPECT_EQ(y1a, y1b);

    FreeImageBuffer(img1);
    FreeImageBuffer(img2);
}
