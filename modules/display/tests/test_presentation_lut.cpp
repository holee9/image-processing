/**
 * @file test_presentation_lut.cpp
 * @brief Unit tests for xpe_apply_presentation_lut and xpe_gsdf_calibrate (SWU-3.3)
 * SPEC: SPEC-XPE-P1B-DISP
 * REQ-DISP-019 to REQ-DISP-028
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <chrono>
#include <cstring>
#include <algorithm>

#include "xpe/display/display_api.h"

// =============================================================================
// Test Helpers
// =============================================================================

static XpeImageBuffer make_float32_image(uint32_t w, uint32_t h, float fill_value) {
    XpeImageBuffer img{};
    img.width         = w;
    img.height        = h;
    img.format        = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = 32;
    img.bitsStored    = 32;
    img.dataSize      = w * h * sizeof(float);
    img.data          = std::malloc(img.dataSize);
    float* px = static_cast<float*>(img.data);
    for (size_t i = 0; i < (size_t)w * h; ++i) px[i] = fill_value;
    return img;
}

// After presentation LUT, img->data points to uint16 buffer (function handles realloc)
static void free_image(XpeImageBuffer& img) {
    std::free(img.data);
    img.data = nullptr;
}

// Build identity LUT: index i maps to value i (for 1024 entries, scaled to uint16 range)
static void make_identity_lut(XpePresentationLutParams& p) {
    for (int i = 0; i < 1024; ++i) {
        p.lutData[i] = static_cast<uint16_t>(i);
    }
    p.gsdfEnabled = 0;
}

// Build ramp LUT: all entries map to a fixed value
static void make_constant_lut(XpePresentationLutParams& p, uint16_t val) {
    for (int i = 0; i < 1024; ++i) {
        p.lutData[i] = val;
    }
    p.gsdfEnabled = 0;
}

// =============================================================================
// REQ-DISP-019: Domain transition float32 -> uint16
// =============================================================================

TEST(PresentationLut, DomainTransition_FormatBecomesUint16) {
    // REQ-DISP-019: after call, img->format must be XPE_PIXEL_UINT16
    XpeImageBuffer img = make_float32_image(2, 2, 0.5f);
    XpePresentationLutParams params{};
    make_identity_lut(params);

    XpeErrorCode rc = xpe_apply_presentation_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_EQ(img.format, XPE_PIXEL_UINT16);
    EXPECT_EQ(img.bitsAllocated, 16u);
    EXPECT_EQ(img.bitsStored, 16u);
    EXPECT_EQ(img.dataSize, (size_t)(2 * 2 * 2)); // 4 pixels * 2 bytes
    free_image(img);
}

// =============================================================================
// REQ-DISP-020: LUT lookup index = clamp(round(input * 1023), 0, 1023)
// =============================================================================

TEST(PresentationLut, LutLookup_HalfValue) {
    // REQ-DISP-020: input=0.5 -> index=round(0.5*1023)=512 -> lutData[512]
    XpeImageBuffer img = make_float32_image(1, 1, 0.5f);
    XpePresentationLutParams params{};
    make_identity_lut(params);
    // identity: lutData[512] = 512

    XpeErrorCode rc = xpe_apply_presentation_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    uint16_t* out = static_cast<uint16_t*>(img.data);
    EXPECT_EQ(out[0], 512u);
    free_image(img);
}

TEST(PresentationLut, LutLookup_ZeroInput) {
    // REQ-DISP-020: input=0.0 -> index=0 -> lutData[0]
    XpeImageBuffer img = make_float32_image(1, 1, 0.0f);
    XpePresentationLutParams params{};
    params.lutData[0] = 999;
    for (int i = 1; i < 1024; ++i) params.lutData[i] = 0;
    params.gsdfEnabled = 0;

    XpeErrorCode rc = xpe_apply_presentation_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_EQ(static_cast<uint16_t*>(img.data)[0], 999u);
    free_image(img);
}

TEST(PresentationLut, LutLookup_OneInput) {
    // REQ-DISP-020: input=1.0 -> index=1023 -> lutData[1023]
    XpeImageBuffer img = make_float32_image(1, 1, 1.0f);
    XpePresentationLutParams params{};
    make_constant_lut(params, 0);
    params.lutData[1023] = 65535;

    XpeErrorCode rc = xpe_apply_presentation_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_EQ(static_cast<uint16_t*>(img.data)[0], 65535u);
    free_image(img);
}

// =============================================================================
// REQ-DISP-021: Input clamped to [0.0, 1.0] before lookup
// =============================================================================

TEST(PresentationLut, InputClamp_Negative) {
    // REQ-DISP-021: negative input clamped to 0.0 -> index 0
    XpeImageBuffer img = make_float32_image(1, 1, -5.0f);
    XpePresentationLutParams params{};
    make_identity_lut(params);

    XpeErrorCode rc = xpe_apply_presentation_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_EQ(static_cast<uint16_t*>(img.data)[0], 0u);
    free_image(img);
}

TEST(PresentationLut, InputClamp_AboveOne) {
    // REQ-DISP-021: input > 1.0 clamped to 1.0 -> index 1023
    XpeImageBuffer img = make_float32_image(1, 1, 2.5f);
    XpePresentationLutParams params{};
    make_identity_lut(params);

    XpeErrorCode rc = xpe_apply_presentation_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_EQ(static_cast<uint16_t*>(img.data)[0], 1023u);
    free_image(img);
}

// =============================================================================
// REQ-DISP-022 to REQ-DISP-024: Error cases
// =============================================================================

TEST(PresentationLut, Error_NullImg) {
    // REQ-DISP-022: NULL img -> XPE_ERR_INVALID_INPUT
    XpePresentationLutParams params{};
    XpeErrorCode rc = xpe_apply_presentation_lut(nullptr, &params);
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
}

TEST(PresentationLut, Error_NullParams) {
    // REQ-DISP-022: NULL params -> XPE_ERR_INVALID_INPUT
    XpeImageBuffer img = make_float32_image(2, 2, 0.5f);
    XpeErrorCode rc = xpe_apply_presentation_lut(&img, nullptr);
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
    free_image(img);
}

TEST(PresentationLut, Error_WrongFormat) {
    // REQ-DISP-023: format != FLOAT32 -> XPE_ERR_UNSUPPORTED_FORMAT
    uint16_t buf[4] = {0};
    XpeImageBuffer img{};
    img.width = 2; img.height = 2;
    img.format = XPE_PIXEL_UINT16;
    img.bitsAllocated = 16; img.bitsStored = 16;
    img.dataSize = 8;
    img.data = buf;

    XpePresentationLutParams params{};
    XpeErrorCode rc = xpe_apply_presentation_lut(&img, &params);
    EXPECT_EQ(rc, XPE_ERR_UNSUPPORTED_FORMAT);
}

// =============================================================================
// REQ-DISP-026: xpe_gsdf_calibrate basic functionality
// =============================================================================

TEST(PresentationLut, GsdfCalibrate_BasicOutput) {
    // REQ-DISP-026: gsdf_calibrate produces 1024-entry non-decreasing LUT
    float lum[10];
    for (int i = 0; i < 10; ++i) lum[i] = 1.0f + i * 10.0f; // 1..91 cd/m^2

    XpePresentationLutParams out{};
    XpeErrorCode rc = xpe_gsdf_calibrate(lum, 10, &out);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_EQ(out.gsdfEnabled, 1);

    // LUT must be monotonically non-decreasing
    for (int i = 1; i < 1024; ++i) {
        EXPECT_GE(out.lutData[i], out.lutData[i - 1])
            << "LUT not monotone at index " << i;
    }
}

TEST(PresentationLut, GsdfCalibrate_MinCount2) {
    // REQ-DISP-027: count >= 2 required
    float lum[2] = {1.0f, 100.0f};
    XpePresentationLutParams out{};
    XpeErrorCode rc = xpe_gsdf_calibrate(lum, 2, &out);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_EQ(out.gsdfEnabled, 1);
}

TEST(PresentationLut, GsdfCalibrate_Error_NullLuminance) {
    // REQ-DISP-027: NULL luminanceValues -> XPE_ERR_INVALID_INPUT
    XpePresentationLutParams out{};
    XpeErrorCode rc = xpe_gsdf_calibrate(nullptr, 10, &out);
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
}

TEST(PresentationLut, GsdfCalibrate_Error_NullOut) {
    // REQ-DISP-027: NULL outParams -> XPE_ERR_INVALID_INPUT
    float lum[5] = {1.0f, 10.0f, 50.0f, 100.0f, 500.0f};
    XpeErrorCode rc = xpe_gsdf_calibrate(lum, 5, nullptr);
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
}

TEST(PresentationLut, GsdfCalibrate_Error_CountLessThan2) {
    // REQ-DISP-027: count < 2 -> XPE_ERR_INVALID_INPUT
    float lum[1] = {1.0f};
    XpePresentationLutParams out{};
    XpeErrorCode rc = xpe_gsdf_calibrate(lum, 1, &out);
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
}

// =============================================================================
// REQ-DISP-025: Performance test (<= 25ms for 3072x3072)
// =============================================================================

TEST(PresentationLut, Performance_3072x3072) {
    // REQ-DISP-025: presentation LUT <= 25ms for 3072x3072
    XpeImageBuffer img = make_float32_image(3072, 3072, 0.5f);
    XpePresentationLutParams params{};
    make_identity_lut(params);

    auto t0 = std::chrono::high_resolution_clock::now();
    XpeErrorCode rc = xpe_apply_presentation_lut(&img, &params);
    auto t1 = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    EXPECT_EQ(rc, XPE_OK);
    EXPECT_LE(ms, 25) << "PresentationLUT 3072x3072 took " << ms << "ms (limit 25ms)";
    free_image(img);
}
