/**
 * @file test_integration.cpp
 * @brief Cross-SWU integration tests for xpe_enhance_advanced module
 *
 * Tests covering:
 *   - Full pipeline: MFP -> Fractional -> Collimation -> EI (AC-PIPE-001)
 *   - Independent function calling (AC-PIPE-002)
 *   - Exception boundary guard (REQ-ADV-030)
 *   - Multiple pipeline iterations stability
 *   - Memory endurance (20+ cycles)
 *   - Sequential processing determinism
 *
 * SPEC Reference: SPEC-XPE-P2-ADV v1.0.0
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

XpeImageMetadata MakeMeta(const char* bodyPart, float kVp = 80.0f, float mAs = 10.0f) {
    XpeImageMetadata meta{};
    std::memset(&meta, 0, sizeof(meta));
    strncpy_s(meta.bodyPart, sizeof(meta.bodyPart), bodyPart, _TRUNCATE);
    meta.kVp = kVp;
    meta.mAs = mAs;
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
// Integration Pipeline Fixture
// ============================================================================

class IntegrationPipelineTest : public ::testing::Test {
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
// AC-PIPE-001: Full Pipeline Integration
// ============================================================================

TEST_F(IntegrationPipelineTest, MfpThenFractionalPipeline) {
    const uint32_t W = 32, H = 32;
    XpeImageBuffer img = MakeConstantImage(W, H, 500.0f);
    XpeImageMetadata meta = MakeMeta("CHEST");

    // Stage 1: MFP
    ASSERT_EQ(xpe_multiscale_process(&img, &meta, nullptr), XPE_OK);

    // Verify intermediate state is finite
    float* data = static_cast<float*>(img.data);
    for (size_t i = 0; i < static_cast<size_t>(W) * H; ++i) {
        ASSERT_TRUE(std::isfinite(data[i]));
    }

    // Stage 2: Fractional enhancement
    ASSERT_EQ(xpe_fractional_process(&img, 1.0f, nullptr), XPE_OK);

    // Final verification
    for (size_t i = 0; i < static_cast<size_t>(W) * H; ++i) {
        EXPECT_TRUE(std::isfinite(data[i])) << "Non-finite after pipeline at pixel " << i;
    }

    FreeImageBuffer(img);
}

TEST_F(IntegrationPipelineTest, FullPipelineWithCollimationAndEI) {
    const uint32_t W = 64, H = 64;
    XpeImageBuffer img = MakeConstantImage(W, H, 500.0f);
    XpeImageMetadata meta = MakeMeta("CHEST");

    // Stage 1: MFP
    ASSERT_EQ(xpe_multiscale_process(&img, &meta, nullptr), XPE_OK);

    // Stage 2: Fractional enhancement
    ASSERT_EQ(xpe_fractional_process(&img, 0.5f, nullptr), XPE_OK);

    // Stage 3: Collimation detection (non-destructive read)
    int32_t x0, y0, x1, y1;
    ASSERT_EQ(xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr), XPE_OK);

    // Stage 4: Exposure index calculation (non-destructive read)
    float ei = 0.0f, di = 0.0f;
    ASSERT_EQ(xpe_calc_exposure_index(&img, &meta, &ei, &di), XPE_OK);

    // Verify outputs
    EXPECT_TRUE(std::isfinite(ei));
    EXPECT_TRUE(std::isfinite(di));
    EXPECT_GT(ei, 0.0f);

    FreeImageBuffer(img);
}

// ============================================================================
// AC-PIPE-002: Independent Function Calling
// ============================================================================

TEST_F(IntegrationPipelineTest, FunctionsCanBeCalledIndependently) {
    const uint32_t W = 64, H = 64;

    // Function 1: Only MFP
    {
        XpeImageBuffer img = MakeConstantImage(W, H, 500.0f);
        XpeImageMetadata meta = MakeMeta("CHEST");
        EXPECT_EQ(xpe_multiscale_process(&img, &meta, nullptr), XPE_OK);
        FreeImageBuffer(img);
    }

    // Function 2: Only Fractional (no prior MFP)
    {
        XpeImageBuffer img = MakeConstantImage(W, H, 500.0f);
        EXPECT_EQ(xpe_fractional_process(&img, 1.0f, nullptr), XPE_OK);
        FreeImageBuffer(img);
    }

    // Function 3: Only Collimation
    {
        XpeImageBuffer img = MakeConstantImage(W, H, 500.0f);
        int32_t x0, y0, x1, y1;
        EXPECT_EQ(xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr), XPE_OK);
        FreeImageBuffer(img);
    }

    // Function 4: Only EI
    {
        XpeImageBuffer img = MakeConstantImage(W, H, 500.0f);
        XpeImageMetadata meta = MakeMeta("CHEST");
        float ei = 0.0f, di = 0.0f;
        EXPECT_EQ(xpe_calc_exposure_index(&img, &meta, &ei, &di), XPE_OK);
        FreeImageBuffer(img);
    }
}

// ============================================================================
// REQ-ADV-030: No Exceptions Across C ABI
// ============================================================================

TEST_F(IntegrationPipelineTest, ExceptionBoundaryNoCrash) {
    const uint32_t W = 32, H = 32;
    XpeImageBuffer img = MakeConstantImage(W, H, 500.0f);
    XpeImageMetadata meta = MakeMeta("CHEST");

    // Valid calls should succeed without throwing
    EXPECT_NO_FATAL_FAILURE(xpe_multiscale_process(&img, &meta, nullptr));
    EXPECT_NO_FATAL_FAILURE(xpe_fractional_process(&img, 1.0f, nullptr));

    int32_t x0, y0, x1, y1;
    EXPECT_NO_FATAL_FAILURE(xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr));

    float ei = 0.0f, di = 0.0f;
    EXPECT_NO_FATAL_FAILURE(xpe_calc_exposure_index(&img, &meta, &ei, &di));

    FreeImageBuffer(img);
}

// ============================================================================
// Multiple Pipeline Iterations
// ============================================================================

TEST_F(IntegrationPipelineTest, MultiplePipelinesStable) {
    const uint32_t W = 32, H = 32;

    for (int i = 0; i < 5; ++i) {
        XpeImageBuffer img = MakeConstantImage(W, H, 500.0f + i * 100.0f);
        XpeImageMetadata meta = MakeMeta("CHEST");

        ASSERT_EQ(xpe_multiscale_process(&img, &meta, nullptr), XPE_OK);
        ASSERT_EQ(xpe_fractional_process(&img, 0.5f, nullptr), XPE_OK);

        float ei = 0.0f, di = 0.0f;
        ASSERT_EQ(xpe_calc_exposure_index(&img, &meta, &ei, &di), XPE_OK);
        EXPECT_TRUE(std::isfinite(ei));

        FreeImageBuffer(img);
    }
}

// ============================================================================
// REQ-ADV-031: Memory Endurance (20 cycles)
// ============================================================================

TEST_F(IntegrationPipelineTest, RepeatedProcessingNoLeak) {
    const uint32_t W = 32, H = 32;

    for (int i = 0; i < 20; ++i) {
        XpeImageBuffer img = MakeConstantImage(W, H, 500.0f);
        XpeImageMetadata meta = MakeMeta("CHEST");

        ASSERT_EQ(xpe_multiscale_process(&img, &meta, nullptr), XPE_OK);
        FreeImageBuffer(img);
    }
}

// ============================================================================
// Determinism: Same Input -> Same Output
// ============================================================================

TEST_F(IntegrationPipelineTest, SequentialProcessingDeterministic) {
    const uint32_t W = 32, H = 32;

    XpeImageBuffer img1 = MakeConstantImage(W, H, 500.0f);
    XpeImageBuffer img2 = MakeConstantImage(W, H, 500.0f);
    XpeImageMetadata meta = MakeMeta("CHEST");

    ASSERT_EQ(xpe_multiscale_process(&img1, &meta, nullptr), XPE_OK);
    ASSERT_EQ(xpe_multiscale_process(&img2, &meta, nullptr), XPE_OK);

    float* r1 = static_cast<float*>(img1.data);
    float* r2 = static_cast<float*>(img2.data);

    for (size_t i = 0; i < static_cast<size_t>(W) * H; ++i) {
        EXPECT_FLOAT_EQ(r1[i], r2[i]) << "Determinism violation at pixel " << i;
    }

    FreeImageBuffer(img1);
    FreeImageBuffer(img2);
}

// ============================================================================
// Cross-SWU Error Path Coverage
// ============================================================================

TEST_F(IntegrationPipelineTest, ErrorPathsCovered) {
    // NOT_INITIALIZED checks (module not initialized yet)
    EXPECT_EQ(xpe_multiscale_process(nullptr, nullptr, nullptr), XPE_ERR_NOT_INITIALIZED);
    EXPECT_EQ(xpe_fractional_process(nullptr, 1.0f, nullptr), XPE_ERR_NOT_INITIALIZED);

    // Initialize module
    ASSERT_EQ(xpe_enhance_advanced_init(nullptr), XPE_OK);

    // NULL pointer checks after initialization
    EXPECT_EQ(xpe_multiscale_process(nullptr, nullptr, nullptr), XPE_ERR_INVALID_INPUT);
    EXPECT_EQ(xpe_fractional_process(nullptr, 1.0f, nullptr), XPE_ERR_INVALID_INPUT);

    // Invalid format
    XpeImageBuffer img{};
    img.width = 32;
    img.height = 32;
    img.format = XPE_PIXEL_UINT16;
    EXPECT_EQ(xpe_multiscale_process(&img, nullptr, nullptr), XPE_ERR_UNSUPPORTED_FORMAT);

    // Invalid order
    XpeImageBuffer img2{};
    img2.width = 32;
    img2.height = 32;
    img2.format = XPE_PIXEL_FLOAT32;
    EXPECT_EQ(xpe_fractional_process(&img2, -1.0f, nullptr), XPE_ERR_INVALID_INPUT);
    EXPECT_EQ(xpe_fractional_process(&img2, 3.0f, nullptr), XPE_ERR_INVALID_INPUT);
}
