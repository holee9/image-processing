/**
 * @file test_voi_lut.cpp
 * @brief Unit tests for xpe_apply_voi_lut and xpe_voi_preset_create (SWU-3.2)
 * SPEC: SPEC-XPE-P1B-DISP
 * REQ-DISP-009 to REQ-DISP-018
 */

#include <gtest/gtest.h>
#include <cmath>
#include <cstdlib>
#include <chrono>

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

static void free_image(XpeImageBuffer& img) {
    std::free(img.data);
    img.data = nullptr;
}

static float* pixels(XpeImageBuffer& img) {
    return static_cast<float*>(img.data);
}

// =============================================================================
// REQ-DISP-009: LINEAR windowing
// =============================================================================

TEST(VoiLut, Linear_CenterWindow) {
    // REQ-DISP-009: LINEAR: output[i] = clamp((input[i] - (center - width/2)) / width * range + minOut, minOut, maxOut)
    // center=500, width=1000, minOut=0, maxOut=255
    // input = center -> output = 0 + (500 - (500-500)) / 1000 * 255 = 127.5
    XpeImageBuffer img = make_float32_image(1, 1, 500.0f);
    XpeVoiLutParams params{};
    params.mode   = XPE_VOI_LINEAR;
    params.center = 500.0f;
    params.width  = 1000.0f;
    params.minOut = 0.0f;
    params.maxOut = 255.0f;

    XpeErrorCode rc = xpe_apply_voi_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    // (500 - (500 - 500)) / 1000 * 255 + 0 = 127.5
    EXPECT_NEAR(pixels(img)[0], 127.5f, 0.5f);
    free_image(img);
}

TEST(VoiLut, Linear_ClampMin) {
    // REQ-DISP-010: output clamped to minOut
    XpeImageBuffer img = make_float32_image(1, 1, -9999.0f);
    XpeVoiLutParams params{};
    params.mode   = XPE_VOI_LINEAR;
    params.center = 0.0f;
    params.width  = 1000.0f;
    params.minOut = 0.0f;
    params.maxOut = 255.0f;

    XpeErrorCode rc = xpe_apply_voi_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_FLOAT_EQ(pixels(img)[0], 0.0f);
    free_image(img);
}

TEST(VoiLut, Linear_ClampMax) {
    // REQ-DISP-010: output clamped to maxOut
    XpeImageBuffer img = make_float32_image(1, 1, 9999.0f);
    XpeVoiLutParams params{};
    params.mode   = XPE_VOI_LINEAR;
    params.center = 0.0f;
    params.width  = 1000.0f;
    params.minOut = 0.0f;
    params.maxOut = 255.0f;

    XpeErrorCode rc = xpe_apply_voi_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_FLOAT_EQ(pixels(img)[0], 255.0f);
    free_image(img);
}

// =============================================================================
// REQ-DISP-011: LINEAR_EXACT windowing (DICOM PS3.3 C.11.2.1.3)
// =============================================================================

TEST(VoiLut, LinearExact_CenterValue) {
    // REQ-DISP-011: LINEAR_EXACT center maps to midpoint of [minOut, maxOut]
    XpeImageBuffer img = make_float32_image(1, 1, 40.0f);
    XpeVoiLutParams params{};
    params.mode   = XPE_VOI_LINEAR_EXACT;
    params.center = 40.0f;
    params.width  = 80.0f;
    params.minOut = 0.0f;
    params.maxOut = 255.0f;

    XpeErrorCode rc = xpe_apply_voi_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    // center input -> output = (minOut + maxOut) / 2 = 127.5
    EXPECT_NEAR(pixels(img)[0], 127.5f, 1.0f);
    free_image(img);
}

// =============================================================================
// REQ-DISP-012: SIGMOID windowing
// =============================================================================

TEST(VoiLut, Sigmoid_CenterValue) {
    // REQ-DISP-012: SIGMOID center -> output near midpoint
    // sigmoid(0) = 0.5, so center -> (maxOut - minOut) * 0.5 + minOut
    XpeImageBuffer img = make_float32_image(1, 1, 500.0f);
    XpeVoiLutParams params{};
    params.mode   = XPE_VOI_SIGMOID;
    params.center = 500.0f;
    params.width  = 1000.0f;
    params.minOut = 0.0f;
    params.maxOut = 255.0f;

    XpeErrorCode rc = xpe_apply_voi_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    // At center: sigmoid(0) = 0.5 -> output = 127.5
    EXPECT_NEAR(pixels(img)[0], 127.5f, 1.0f);
    free_image(img);
}

TEST(VoiLut, Sigmoid_OutputClampedToRange) {
    // REQ-DISP-010: SIGMOID output still clamped to [minOut, maxOut]
    // Extreme high value approaches maxOut
    XpeImageBuffer img = make_float32_image(1, 1, 99999.0f);
    XpeVoiLutParams params{};
    params.mode   = XPE_VOI_SIGMOID;
    params.center = 0.0f;
    params.width  = 1000.0f;
    params.minOut = 0.0f;
    params.maxOut = 255.0f;

    XpeErrorCode rc = xpe_apply_voi_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_LE(pixels(img)[0], 255.0f);
    EXPECT_GE(pixels(img)[0], 0.0f);
    free_image(img);
}

// =============================================================================
// REQ-DISP-013 to REQ-DISP-014: Error cases
// =============================================================================

TEST(VoiLut, Error_NullImg) {
    // REQ-DISP-013: NULL img -> XPE_ERR_INVALID_INPUT
    XpeVoiLutParams params{};
    params.mode  = XPE_VOI_LINEAR;
    params.width = 1000.0f;
    XpeErrorCode rc = xpe_apply_voi_lut(nullptr, &params);
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
}

TEST(VoiLut, Error_NullParams) {
    // REQ-DISP-013: NULL params -> XPE_ERR_INVALID_INPUT
    XpeImageBuffer img = make_float32_image(2, 2, 0.0f);
    XpeErrorCode rc = xpe_apply_voi_lut(&img, nullptr);
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
    free_image(img);
}

TEST(VoiLut, Error_WrongFormat) {
    // REQ-DISP-014: format != FLOAT32 -> XPE_ERR_UNSUPPORTED_FORMAT
    uint16_t buf[4] = {100, 200, 300, 400};
    XpeImageBuffer img{};
    img.width = 2; img.height = 2;
    img.format = XPE_PIXEL_UINT16;
    img.bitsAllocated = 16; img.bitsStored = 16;
    img.dataSize = 8;
    img.data = buf;

    XpeVoiLutParams params{};
    params.mode  = XPE_VOI_LINEAR;
    params.width = 1000.0f;
    XpeErrorCode rc = xpe_apply_voi_lut(&img, &params);
    EXPECT_EQ(rc, XPE_ERR_UNSUPPORTED_FORMAT);
    EXPECT_EQ(buf[0], 100); // unchanged
}

TEST(VoiLut, Error_ZeroWidth) {
    // REQ-DISP-015: width <= 0 -> XPE_ERR_INVALID_INPUT; image unchanged
    XpeImageBuffer img = make_float32_image(2, 2, 42.0f);
    XpeVoiLutParams params{};
    params.mode   = XPE_VOI_LINEAR;
    params.width  = 0.0f;
    params.minOut = 0.0f;
    params.maxOut = 255.0f;
    XpeErrorCode rc = xpe_apply_voi_lut(&img, &params);
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
    EXPECT_FLOAT_EQ(pixels(img)[0], 42.0f); // must not be modified
    free_image(img);
}

TEST(VoiLut, Error_NegativeWidth) {
    // REQ-DISP-015: negative width also invalid
    XpeImageBuffer img = make_float32_image(1, 1, 42.0f);
    XpeVoiLutParams params{};
    params.mode  = XPE_VOI_LINEAR;
    params.width = -100.0f;
    XpeErrorCode rc = xpe_apply_voi_lut(&img, &params);
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
    free_image(img);
}

// =============================================================================
// REQ-DISP-017: Presets
// =============================================================================

TEST(VoiLut, Preset_Bone) {
    // REQ-DISP-017: BONE preset center=500, width=2000
    XpeVoiLutParams params{};
    XpeErrorCode rc = xpe_voi_preset_create(&params, XPE_BODY_BONE);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_FLOAT_EQ(params.center, 500.0f);
    EXPECT_FLOAT_EQ(params.width, 2000.0f);
}

TEST(VoiLut, Preset_Lung) {
    // REQ-DISP-017: LUNG preset center=-600, width=1600
    XpeVoiLutParams params{};
    XpeErrorCode rc = xpe_voi_preset_create(&params, XPE_BODY_LUNG);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_FLOAT_EQ(params.center, -600.0f);
    EXPECT_FLOAT_EQ(params.width, 1600.0f);
}

TEST(VoiLut, Preset_Abdomen) {
    // REQ-DISP-017: ABDOMEN preset center=40, width=400
    XpeVoiLutParams params{};
    XpeErrorCode rc = xpe_voi_preset_create(&params, XPE_BODY_ABDOMEN);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_FLOAT_EQ(params.center, 40.0f);
    EXPECT_FLOAT_EQ(params.width, 400.0f);
}

TEST(VoiLut, Preset_Head) {
    // REQ-DISP-017: HEAD preset center=40, width=80
    XpeVoiLutParams params{};
    XpeErrorCode rc = xpe_voi_preset_create(&params, XPE_BODY_HEAD);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_FLOAT_EQ(params.center, 40.0f);
    EXPECT_FLOAT_EQ(params.width, 80.0f);
}

TEST(VoiLut, Preset_InvalidBodyPart) {
    // REQ-DISP-018: invalid bodyPart -> XPE_ERR_INVALID_INPUT
    XpeVoiLutParams params{};
    XpeErrorCode rc = xpe_voi_preset_create(&params, static_cast<XpeBodyPart>(999));
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
}

TEST(VoiLut, Preset_NullParams) {
    // REQ-DISP-018: NULL params -> XPE_ERR_INVALID_INPUT
    XpeErrorCode rc = xpe_voi_preset_create(nullptr, XPE_BODY_BONE);
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
}

// =============================================================================
// REQ-DISP-016: Performance test (<= 16ms for 3072x3072)
// =============================================================================

TEST(VoiLut, Performance_3072x3072) {
    // REQ-DISP-016: LINEAR windowing <= 16ms for 3072x3072
    XpeImageBuffer img = make_float32_image(3072, 3072, 500.0f);
    XpeVoiLutParams params{};
    params.mode   = XPE_VOI_LINEAR;
    params.center = 500.0f;
    params.width  = 1000.0f;
    params.minOut = 0.0f;
    params.maxOut = 255.0f;

    auto t0 = std::chrono::high_resolution_clock::now();
    XpeErrorCode rc = xpe_apply_voi_lut(&img, &params);
    auto t1 = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    EXPECT_EQ(rc, XPE_OK);
    EXPECT_LE(ms, 16) << "VoiLUT LINEAR 3072x3072 took " << ms << "ms (limit 16ms)";
    free_image(img);
}
