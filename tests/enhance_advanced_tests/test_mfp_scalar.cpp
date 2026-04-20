/**
 * @file test_mfp_scalar.cpp
 * @brief Google Test suite for SWU-2.5: Multiscale Frequency Processing (MFP)
 *
 * Tests the xpe_multiscale_process() C ABI function covering:
 *   - Identity reconstruction fidelity (REQ-ADV-050)
 *   - Frequency band enhancement
 *   - Body-part adaptive gain
 *   - NULL pointer guards (REQ-ADV-022)
 *   - Dimension validation (REQ-ADV-070)
 *   - Pixel format validation (REQ-ADV-071)
 *   - Config parsing (T-206, T-207)
 *   - Reproducibility
 *   - NaN/Inf output guard (REQ-ADV-032)
 *
 * SPEC Reference: SPEC-XPE-P2-ADV v1.0.0
 * SWU: 2.5
 */

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <memory>
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

XpeImageBuffer MakeGradientImage(uint32_t w, uint32_t h) {
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
            p[y * w + x] = static_cast<float>(x) / static_cast<float>(w - 1);
    return buf;
}

XpeImageBuffer MakeCheckerboardImage(uint32_t w, uint32_t h, uint32_t blockSize) {
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
        for (uint32_t x = 0; x < w; ++x) {
            bool white = ((x / blockSize) + (y / blockSize)) % 2 == 0;
            p[y * w + x] = white ? 1.0f : 0.0f;
        }
    return buf;
}

XpeImageMetadata MakeMeta(const char* bodyPart) {
    XpeImageMetadata meta{};
    std::memset(&meta, 0, sizeof(meta));
    strncpy_s(meta.bodyPart, sizeof(meta.bodyPart), bodyPart, _TRUNCATE);
    meta.kVp = 80.0f;
    meta.mAs = 10.0f;
    meta.SID_mm = 1800.0f;
    meta.pixelPitch_mm = 0.139f;
    return meta;
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

class MfpScalarTest : public ::testing::Test {
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

TEST_F(MfpScalarTest, NullImageReturnsInvalidInput) {
    XpeImageMetadata meta = MakeMeta("CHEST");
    EXPECT_EQ(xpe_multiscale_process(nullptr, &meta, nullptr), XPE_ERR_INVALID_INPUT);
}

TEST_F(MfpScalarTest, NullMetaReturnsInvalidInput) {
    XpeImageBuffer img = MakeConstantImage(64, 64, 0.5f);
    EXPECT_EQ(xpe_multiscale_process(&img, nullptr, nullptr), XPE_ERR_INVALID_INPUT);
    FreeImageBuffer(img);
}

TEST_F(MfpScalarTest, NullImageAndMetaReturnsInvalidInput) {
    EXPECT_EQ(xpe_multiscale_process(nullptr, nullptr, nullptr), XPE_ERR_INVALID_INPUT);
}

// ============================================================================
// REQ-ADV-070: Dimension Validation
// ============================================================================

TEST_F(MfpScalarTest, ZeroWidthReturnsInvalidInput) {
    XpeImageBuffer img{};
    img.width = 0;
    img.height = 64;
    img.format = XPE_PIXEL_FLOAT32;
    XpeImageMetadata meta = MakeMeta("CHEST");
    EXPECT_EQ(xpe_multiscale_process(&img, &meta, nullptr), XPE_ERR_INVALID_INPUT);
}

TEST_F(MfpScalarTest, ZeroHeightReturnsInvalidInput) {
    XpeImageBuffer img{};
    img.width = 64;
    img.height = 0;
    img.format = XPE_PIXEL_FLOAT32;
    XpeImageMetadata meta = MakeMeta("CHEST");
    EXPECT_EQ(xpe_multiscale_process(&img, &meta, nullptr), XPE_ERR_INVALID_INPUT);
}

// ============================================================================
// REQ-ADV-071: Format Validation
// ============================================================================

TEST_F(MfpScalarTest, Uint16FormatReturnsUnsupportedFormat) {
    XpeImageBuffer img{};
    img.width = 64;
    img.height = 64;
    img.format = XPE_PIXEL_UINT16;
    img.dataSize = 64 * 64 * sizeof(uint16_t);
    img.data = new uint16_t[64 * 64]();
    XpeImageMetadata meta = MakeMeta("CHEST");
    EXPECT_EQ(xpe_multiscale_process(&img, &meta, nullptr), XPE_ERR_UNSUPPORTED_FORMAT);
    delete[] static_cast<uint16_t*>(img.data);
}

// ============================================================================
// REQ-ADV-050: Identity Reconstruction Fidelity
// ============================================================================

TEST_F(MfpScalarTest, IdentityReconstructionConstantImage) {
    const uint32_t W = 128, H = 128;
    XpeImageBuffer img = MakeConstantImage(W, H, 1000.0f);
    float* original = new float[W * H];
    std::memcpy(original, img.data, W * H * sizeof(float));

    XpeImageMetadata meta = MakeMeta("CHEST");

    // Identity config: all gains = 1.0, noise threshold = 0
    const char* identityConfig =
        "{\"mfp\":{\"num_levels\":4,\"edge_gain\":1.0,\"texture_gain\":1.0,"
        "\"flat_gain\":1.0,\"noise_threshold\":0.0}}";

    XpeErrorCode err = xpe_multiscale_process(&img, &meta, identityConfig);
    ASSERT_EQ(err, XPE_OK);

    float* result = static_cast<float*>(img.data);
    float maxErr = 0.0f;
    for (size_t i = 0; i < static_cast<size_t>(W) * H; ++i) {
        float diff = std::fabs(result[i] - original[i]);
        maxErr = std::max(maxErr, diff);
    }
    EXPECT_LT(maxErr, 1e-3f) << "Identity reconstruction max error: " << maxErr;

    delete[] original;
    FreeImageBuffer(img);
}

TEST_F(MfpScalarTest, IdentityReconstructionGradientImage) {
    const uint32_t W = 64, H = 64;
    XpeImageBuffer img = MakeGradientImage(W, H);
    float* original = new float[W * H];
    std::memcpy(original, img.data, W * H * sizeof(float));

    XpeImageMetadata meta = MakeMeta("ABDOMEN");

    const char* identityConfig =
        "{\"mfp\":{\"num_levels\":3,\"edge_gain\":0.0,\"texture_gain\":0.0,"
        "\"flat_gain\":1.0,\"noise_threshold\":0.0}}";

    XpeErrorCode err = xpe_multiscale_process(&img, &meta, identityConfig);
    ASSERT_EQ(err, XPE_OK);

    float* result = static_cast<float*>(img.data);
    float maxErr = 0.0f;
    for (size_t i = 0; i < static_cast<size_t>(W) * H; ++i) {
        float diff = std::fabs(result[i] - original[i]);
        maxErr = std::max(maxErr, diff);
    }
    EXPECT_LT(maxErr, 1e-1f) << "Identity reconstruction gradient max error: " << maxErr;

    delete[] original;
    FreeImageBuffer(img);
}

// ============================================================================
// REQ-ADV-010: Frequency Band Enhancement (Non-Identity)
// ============================================================================

TEST_F(MfpScalarTest, NonIdentityConfigModifiesOutput) {
    const uint32_t W = 64, H = 64;
    XpeImageBuffer img = MakeGradientImage(W, H);
    float* original = new float[W * H];
    std::memcpy(original, img.data, W * H * sizeof(float));

    XpeImageMetadata meta = MakeMeta("CHEST");

    const char* enhanceConfig =
        "{\"mfp\":{\"num_levels\":4,\"edge_gain\":2.0,\"texture_gain\":1.5,"
        "\"flat_gain\":0.8,\"noise_threshold\":1.0}}";

    XpeErrorCode err = xpe_multiscale_process(&img, &meta, enhanceConfig);
    ASSERT_EQ(err, XPE_OK);

    float* result = static_cast<float*>(img.data);
    int diffCount = 0;
    for (size_t i = 0; i < static_cast<size_t>(W) * H; ++i) {
        if (std::fabs(result[i] - original[i]) > 1e-6f) diffCount++;
    }
    EXPECT_GT(diffCount, 0) << "Non-identity config should modify output";

    delete[] original;
    FreeImageBuffer(img);
}

// ============================================================================
// Body-Part Adaptive Gain
// ============================================================================

TEST_F(MfpScalarTest, MultipleBodyPartsSucceed) {
    const uint32_t W = 32, H = 32;
    const char* bodyParts[] = {"CHEST", "ABDOMEN", "EXTREMITY", "SKULL", "SPINE"};

    for (const char* bp : bodyParts) {
        XpeImageBuffer img = MakeConstantImage(W, H, 500.0f);
        XpeImageMetadata meta = MakeMeta(bp);
        EXPECT_EQ(xpe_multiscale_process(&img, &meta, nullptr), XPE_OK)
            << "Failed for body part: " << bp;
        FreeImageBuffer(img);
    }
}

// ============================================================================
// Config Parsing
// ============================================================================

TEST_F(MfpScalarTest, NullConfigUsesDefaults) {
    XpeImageBuffer img = MakeConstantImage(32, 32, 500.0f);
    XpeImageMetadata meta = MakeMeta("CHEST");
    EXPECT_EQ(xpe_multiscale_process(&img, &meta, nullptr), XPE_OK);
    FreeImageBuffer(img);
}

TEST_F(MfpScalarTest, EmptyConfigStringUsesDefaults) {
    XpeImageBuffer img = MakeConstantImage(32, 32, 500.0f);
    XpeImageMetadata meta = MakeMeta("CHEST");
    EXPECT_EQ(xpe_multiscale_process(&img, &meta, ""), XPE_OK);
    FreeImageBuffer(img);
}

TEST_F(MfpScalarTest, MalformedJsonConfigReturnsConfigInvalid) {
    XpeImageBuffer img = MakeConstantImage(32, 32, 500.0f);
    XpeImageMetadata meta = MakeMeta("CHEST");
    EXPECT_EQ(xpe_multiscale_process(&img, &meta, "{invalid json"), XPE_ERR_CONFIG_INVALID);
    FreeImageBuffer(img);
}

TEST_F(MfpScalarTest, CustomLevelsConfigSucceeds) {
    XpeImageBuffer img = MakeConstantImage(32, 32, 500.0f);
    XpeImageMetadata meta = MakeMeta("CHEST");
    EXPECT_EQ(xpe_multiscale_process(&img, &meta, "{\"mfp\":{\"num_levels\":3}}"), XPE_OK);
    FreeImageBuffer(img);
}

TEST_F(MfpScalarTest, SmallImageSucceeds) {
    XpeImageBuffer img = MakeConstantImage(4, 4, 100.0f);
    XpeImageMetadata meta = MakeMeta("CHEST");
    EXPECT_EQ(xpe_multiscale_process(&img, &meta, "{\"mfp\":{\"num_levels\":2}}"), XPE_OK);
    FreeImageBuffer(img);
}

// ============================================================================
// REQ-ADV-032: NaN/Inf Output Guard
// ============================================================================

TEST_F(MfpScalarTest, NoNaNOrInfInOutput) {
    const uint32_t W = 64, H = 64;
    XpeImageBuffer img = MakeCheckerboardImage(W, H, 8);
    XpeImageMetadata meta = MakeMeta("CHEST");

    const char* config =
        "{\"mfp\":{\"num_levels\":4,\"edge_gain\":1.8,\"texture_gain\":1.2,"
        "\"flat_gain\":0.9,\"noise_threshold\":0.5}}";

    XpeErrorCode err = xpe_multiscale_process(&img, &meta, config);
    ASSERT_EQ(err, XPE_OK);

    float* result = static_cast<float*>(img.data);
    for (size_t i = 0; i < static_cast<size_t>(W) * H; ++i) {
        EXPECT_TRUE(std::isfinite(result[i]))
            << "Non-finite value at pixel " << i << ": " << result[i];
    }
    FreeImageBuffer(img);
}

// ============================================================================
// Reproducibility Test
// ============================================================================

TEST_F(MfpScalarTest, IdenticalInputProducesIdenticalOutput) {
    const uint32_t W = 32, H = 32;
    XpeImageBuffer img1 = MakeGradientImage(W, H);
    XpeImageBuffer img2 = MakeGradientImage(W, H);
    XpeImageMetadata meta = MakeMeta("CHEST");

    const char* config =
        "{\"mfp\":{\"num_levels\":3,\"edge_gain\":1.5,\"texture_gain\":1.0,"
        "\"flat_gain\":0.8,\"noise_threshold\":2.0}}";

    ASSERT_EQ(xpe_multiscale_process(&img1, &meta, config), XPE_OK);
    ASSERT_EQ(xpe_multiscale_process(&img2, &meta, config), XPE_OK);

    float* r1 = static_cast<float*>(img1.data);
    float* r2 = static_cast<float*>(img2.data);
    for (size_t i = 0; i < static_cast<size_t>(W) * H; ++i) {
        EXPECT_FLOAT_EQ(r1[i], r2[i])
            << "Reproducibility violation at pixel " << i;
    }

    FreeImageBuffer(img1);
    FreeImageBuffer(img2);
}

// ============================================================================
// Sequential Processing Stability
// ============================================================================

TEST_F(MfpScalarTest, MultipleSequentialCallsStable) {
    const uint32_t W = 32, H = 32;
    for (int i = 0; i < 10; ++i) {
        XpeImageBuffer img = MakeConstantImage(W, H, 100.0f + i * 50.0f);
        XpeImageMetadata meta = MakeMeta("CHEST");
        ASSERT_EQ(xpe_multiscale_process(&img, &meta, nullptr), XPE_OK);
        FreeImageBuffer(img);
    }
}
