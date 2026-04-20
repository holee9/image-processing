/**
 * @file test_edge_enhancement.cpp
 * @brief Google Test suite for SWU-2.6: Fractional-Order Edge Enhancement
 *
 * Tests the xpe_fractional_process() C ABI function covering:
 *   - Order range validation [0.0, 2.0] (REQ-ADV-021)
 *   - SAF-100 overshoot limiting enforcement (REQ-ADV-051)
 *   - NULL pointer guards (REQ-ADV-022)
 *   - Dimension validation (REQ-ADV-070)
 *   - Format validation (REQ-ADV-071)
 *   - NaN/Inf output guard (REQ-ADV-032)
 *   - Safety violation on config attempts to disable overshoot
 *   - Reproducibility
 *
 * SPEC Reference: SPEC-XPE-P2-ADV v1.0.0
 * SWU: 2.6
 */

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

#include "xpe/enhance_advanced/xpe_enhance_advanced_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

// ============================================================================
// Test Helpers
// ============================================================================

namespace {

XpeImageBuffer MakeConstantImage(uint32_t w, uint32_t h, float value) {
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

XpeImageBuffer MakeStepEdgeImage(uint32_t w, uint32_t h) {
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
            p[y * w + x] = (x < w / 2) ? 100.0f : 1000.0f;
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

class EdgeEnhancementTest : public ::testing::Test {
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

TEST_F(EdgeEnhancementTest, NullImageReturnsInvalidInput) {
    EXPECT_EQ(xpe_fractional_process(nullptr, 1.0f, nullptr), XPE_ERR_INVALID_INPUT);
}

// ============================================================================
// REQ-ADV-021: Order Range Validation
// ============================================================================

TEST_F(EdgeEnhancementTest, OrderZeroSucceeds) {
    XpeImageBuffer img = MakeConstantImage(32, 32, 100.0f);
    float* original = new float[32 * 32];
    std::memcpy(original, img.data, 32 * 32 * sizeof(float));

    EXPECT_EQ(xpe_fractional_process(&img, 0.0f, nullptr), XPE_OK);

    // Order 0.0 = identity (no enhancement)
    float* result = static_cast<float*>(img.data);
    float maxErr = 0.0f;
    for (size_t i = 0; i < 32 * 32; ++i) {
        maxErr = std::max(maxErr, std::fabs(result[i] - original[i]));
    }
    EXPECT_LT(maxErr, 1.0f) << "Order 0.0 should be near-identity";

    delete[] original;
    FreeImageBuffer(img);
}

TEST_F(EdgeEnhancementTest, OrderOneSucceeds) {
    XpeImageBuffer img = MakeStepEdgeImage(32, 32);
    EXPECT_EQ(xpe_fractional_process(&img, 1.0f, nullptr), XPE_OK);
    FreeImageBuffer(img);
}

TEST_F(EdgeEnhancementTest, OrderTwoSucceeds) {
    XpeImageBuffer img = MakeStepEdgeImage(32, 32);
    EXPECT_EQ(xpe_fractional_process(&img, 2.0f, nullptr), XPE_OK);
    FreeImageBuffer(img);
}

TEST_F(EdgeEnhancementTest, OrderHalfSucceeds) {
    XpeImageBuffer img = MakeStepEdgeImage(32, 32);
    EXPECT_EQ(xpe_fractional_process(&img, 0.5f, nullptr), XPE_OK);
    FreeImageBuffer(img);
}

TEST_F(EdgeEnhancementTest, OrderNegativeReturnsInvalidInput) {
    XpeImageBuffer img = MakeConstantImage(32, 32, 100.0f);
    EXPECT_EQ(xpe_fractional_process(&img, -0.1f, nullptr), XPE_ERR_INVALID_INPUT);
    FreeImageBuffer(img);
}

TEST_F(EdgeEnhancementTest, OrderAboveTwoReturnsInvalidInput) {
    XpeImageBuffer img = MakeConstantImage(32, 32, 100.0f);
    EXPECT_EQ(xpe_fractional_process(&img, 2.1f, nullptr), XPE_ERR_INVALID_INPUT);
    FreeImageBuffer(img);
}

TEST_F(EdgeEnhancementTest, OrderInfinityReturnsInvalidInput) {
    XpeImageBuffer img = MakeConstantImage(32, 32, 100.0f);
    EXPECT_EQ(xpe_fractional_process(&img, std::numeric_limits<float>::infinity(), nullptr),
              XPE_ERR_INVALID_INPUT);
    FreeImageBuffer(img);
}

TEST_F(EdgeEnhancementTest, OrderLargeNegativeReturnsInvalidInput) {
    XpeImageBuffer img = MakeConstantImage(32, 32, 100.0f);
    EXPECT_EQ(xpe_fractional_process(&img, -100.0f, nullptr), XPE_ERR_INVALID_INPUT);
    FreeImageBuffer(img);
}

TEST_F(EdgeEnhancementTest, OrderLargePositiveReturnsInvalidInput) {
    XpeImageBuffer img = MakeConstantImage(32, 32, 100.0f);
    EXPECT_EQ(xpe_fractional_process(&img, 100.0f, nullptr), XPE_ERR_INVALID_INPUT);
    FreeImageBuffer(img);
}

// ============================================================================
// REQ-ADV-070: Dimension Validation
// ============================================================================

TEST_F(EdgeEnhancementTest, ZeroDimensionReturnsInvalidInput) {
    XpeImageBuffer img{};
    img.width = 0;
    img.height = 64;
    img.format = XPE_PIXEL_FLOAT32;
    EXPECT_EQ(xpe_fractional_process(&img, 1.0f, nullptr), XPE_ERR_INVALID_INPUT);
}

// ============================================================================
// REQ-ADV-071: Format Validation
// ============================================================================

TEST_F(EdgeEnhancementTest, Uint16FormatReturnsUnsupportedFormat) {
    XpeImageBuffer img{};
    img.width = 32;
    img.height = 32;
    img.format = XPE_PIXEL_UINT16;
    img.dataSize = 32 * 32 * sizeof(uint16_t);
    img.data = new uint16_t[32 * 32]();
    EXPECT_EQ(xpe_fractional_process(&img, 1.0f, nullptr), XPE_ERR_UNSUPPORTED_FORMAT);
    delete[] static_cast<uint16_t*>(img.data);
}

// ============================================================================
// REQ-ADV-051: SAF-100 Overshoot Limiting
// ============================================================================

TEST_F(EdgeEnhancementTest, OvershootLimitingEnforced) {
    const uint32_t W = 64, H = 64;
    XpeImageBuffer img = MakeStepEdgeImage(W, H);
    float* original = new float[W * H];
    std::memcpy(original, img.data, W * H * sizeof(float));

    XpeErrorCode err = xpe_fractional_process(&img, 1.5f, nullptr);
    ASSERT_EQ(err, XPE_OK);

    float* result = static_cast<float*>(img.data);

    for (uint32_t y = 1; y < H - 1; ++y) {
        for (uint32_t x = 1; x < W - 1; ++x) {
            size_t idx = y * W + x;
            float boost = result[idx] - original[idx];

            // Compute 3x3 local stddev
            float sum = 0.0f;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx)
                    sum += original[(y + dy) * W + (x + dx)];
            float mean = sum / 9.0f;
            float varSum = 0.0f;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    float d = original[(y + dy) * W + (x + dx)] - mean;
                    varSum += d * d;
                }
            float sigma = std::sqrt(varSum / 9.0f);
            float limit = (sigma < 1e-6f) ? 0.1f : 3.0f * sigma;

            EXPECT_LE(std::fabs(boost), limit + 1e-3f)
                << "SAF-100 violation at (" << x << "," << y << ")";
        }
    }

    delete[] original;
    FreeImageBuffer(img);
}

TEST_F(EdgeEnhancementTest, UniformImagePreserved) {
    const uint32_t W = 32, H = 32;
    XpeImageBuffer img = MakeConstantImage(W, H, 500.0f);
    float* original = new float[W * H];
    std::memcpy(original, img.data, W * H * sizeof(float));

    ASSERT_EQ(xpe_fractional_process(&img, 1.0f, nullptr), XPE_OK);

    float* result = static_cast<float*>(img.data);
    float maxErr = 0.0f;
    for (size_t i = 0; i < static_cast<size_t>(W) * H; ++i)
        maxErr = std::max(maxErr, std::fabs(result[i] - original[i]));

    // Uniform input: derivative ~0, overshoot limiting keeps deviation small
    EXPECT_LT(maxErr, 10.0f) << "Uniform image should be preserved";

    delete[] original;
    FreeImageBuffer(img);
}

// ============================================================================
// SAF-100: Config Disabling Attempts Must Fail
// ============================================================================

TEST_F(EdgeEnhancementTest, DisableOvershootViaConfigRejected) {
    const uint32_t W = 64, H = 64;
    const char* forbiddenConfigs[] = {
        "{\"overshoot_limiting\": false}",
        "{\"overshoot_limit\": false}",
        "{\"overshoot_factor\": 10.0}",
        "{\"disable_overshoot_limit\": true}"
    };

    for (const char* config : forbiddenConfigs) {
        XpeImageBuffer img = MakeConstantImage(W, H, 0.5f);
        XpeErrorCode err = xpe_fractional_process(&img, 1.0f, config);
        EXPECT_EQ(err, XPE_ERR_SAFETY_VIOLATION)
            << "Config should be rejected: " << config;
        FreeImageBuffer(img);
    }
}

// ============================================================================
// REQ-ADV-032: NaN/Inf Output Guard
// ============================================================================

TEST_F(EdgeEnhancementTest, NoNaNOrInfInOutput) {
    XpeImageBuffer img = MakeStepEdgeImage(32, 32);
    ASSERT_EQ(xpe_fractional_process(&img, 1.5f, nullptr), XPE_OK);

    float* result = static_cast<float*>(img.data);
    for (size_t i = 0; i < 32 * 32; ++i) {
        EXPECT_TRUE(std::isfinite(result[i]))
            << "Non-finite at pixel " << i << ": " << result[i];
    }
    FreeImageBuffer(img);
}

TEST_F(EdgeEnhancementTest, NaNInputHandledGracefully) {
    XpeImageBuffer img = MakeConstantImage(32, 32, 100.0f);
    float* data = static_cast<float*>(img.data);
    data[0] = std::nanf("");
    data[32 * 32 / 2] = std::nanf("");

    XpeErrorCode err = xpe_fractional_process(&img, 1.0f, nullptr);
    if (err == XPE_OK) {
        for (size_t i = 0; i < 32 * 32; ++i) {
            EXPECT_TRUE(std::isfinite(static_cast<float*>(img.data)[i]));
        }
    }
    FreeImageBuffer(img);
}

// ============================================================================
// Repeated Application
// ============================================================================

TEST_F(EdgeEnhancementTest, MultipleIterationsSucceed) {
    XpeImageBuffer img = MakeStepEdgeImage(32, 32);
    EXPECT_EQ(xpe_fractional_process(&img, 1.0f, "{\"iterations\":3}"), XPE_OK);
    FreeImageBuffer(img);
}

// ============================================================================
// Config Parsing
// ============================================================================

TEST_F(EdgeEnhancementTest, NullConfigUsesDefaults) {
    XpeImageBuffer img = MakeConstantImage(32, 32, 500.0f);
    EXPECT_EQ(xpe_fractional_process(&img, 1.0f, nullptr), XPE_OK);
    FreeImageBuffer(img);
}

TEST_F(EdgeEnhancementTest, MalformedJsonReturnsConfigInvalid) {
    XpeImageBuffer img = MakeConstantImage(32, 32, 500.0f);
    EXPECT_EQ(xpe_fractional_process(&img, 1.0f, "{broken"), XPE_ERR_CONFIG_INVALID);
    FreeImageBuffer(img);
}

// ============================================================================
// Reproducibility Test
// ============================================================================

TEST_F(EdgeEnhancementTest, IdenticalInputProducesIdenticalOutput) {
    const uint32_t W = 32, H = 32;
    XpeImageBuffer img1 = MakeStepEdgeImage(W, H);
    XpeImageBuffer img2 = MakeStepEdgeImage(W, H);

    ASSERT_EQ(xpe_fractional_process(&img1, 1.0f, nullptr), XPE_OK);
    ASSERT_EQ(xpe_fractional_process(&img2, 1.0f, nullptr), XPE_OK);

    float* r1 = static_cast<float*>(img1.data);
    float* r2 = static_cast<float*>(img2.data);
    for (size_t i = 0; i < static_cast<size_t>(W) * H; ++i) {
        EXPECT_FLOAT_EQ(r1[i], r2[i]) << "Reproducibility violation at pixel " << i;
    }

    FreeImageBuffer(img1);
    FreeImageBuffer(img2);
}
