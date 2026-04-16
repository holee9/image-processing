/**
 * @file test_modality_lut.cpp
 * @brief Unit tests for xpe_apply_modality_lut (SWU-3.1)
 * SPEC: SPEC-XPE-P1B-DISP
 * REQ-DISP-001 to REQ-DISP-008
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
    img.width          = w;
    img.height         = h;
    img.format         = XPE_PIXEL_FLOAT32;
    img.bitsAllocated  = 32;
    img.bitsStored     = 32;
    img.dataSize       = w * h * sizeof(float);
    img.data           = std::malloc(img.dataSize);
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
// REQ-DISP-001: LINEAR mode basic rescale
// =============================================================================

TEST(ModalityLut, LinearRescale_BasicValues) {
    // REQ-DISP-001: LINEAR mode: output[i] = input[i] * slope + intercept
    XpeImageBuffer img = make_float32_image(4, 4, 1000.0f);
    XpeModalityLutParams params{};
    params.mode             = XPE_MODALITY_LUT_LINEAR;
    params.rescaleSlope     = 1.0f;
    params.rescaleIntercept = -1024.0f;

    XpeErrorCode rc = xpe_apply_modality_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    // 1000.0f * 1.0f + (-1024.0f) = -24.0f
    EXPECT_FLOAT_EQ(pixels(img)[0], -24.0f);
    free_image(img);
}

TEST(ModalityLut, LinearRescale_SlopeAndIntercept) {
    // REQ-DISP-001: verify slope != 1.0, intercept != 0
    XpeImageBuffer img = make_float32_image(2, 2, 500.0f);
    XpeModalityLutParams params{};
    params.mode             = XPE_MODALITY_LUT_LINEAR;
    params.rescaleSlope     = 0.5f;
    params.rescaleIntercept = 100.0f;

    XpeErrorCode rc = xpe_apply_modality_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    // 500.0f * 0.5f + 100.0f = 350.0f
    EXPECT_FLOAT_EQ(pixels(img)[0], 350.0f);
    free_image(img);
}

// =============================================================================
// REQ-DISP-002: TABLE mode basic lookup
// =============================================================================

TEST(ModalityLut, TableMode_BasicLookup) {
    // REQ-DISP-002: TABLE mode: output[i] = lutData[clamp(round(input[i]) - firstMapped, 0, len-1)]
    uint16_t lut[5] = {100, 200, 300, 400, 500};
    XpeImageBuffer img = make_float32_image(1, 3, 0.0f);
    float* px = pixels(img);
    px[0] = 0.0f;  // maps to index 0 -> 100
    px[1] = 2.0f;  // maps to index 2 -> 300
    px[2] = 4.0f;  // maps to index 4 -> 500

    XpeModalityLutParams params{};
    params.mode           = XPE_MODALITY_LUT_TABLE;
    params.lutData        = lut;
    params.lutLength      = 5;
    params.lutFirstMapped = 0;

    XpeErrorCode rc = xpe_apply_modality_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_FLOAT_EQ(px[0], 100.0f);
    EXPECT_FLOAT_EQ(px[1], 300.0f);
    EXPECT_FLOAT_EQ(px[2], 500.0f);
    free_image(img);
}

TEST(ModalityLut, TableMode_LutFirstMappedOffset) {
    // REQ-DISP-002: lutFirstMapped shifts the index
    uint16_t lut[3] = {10, 20, 30};
    XpeImageBuffer img = make_float32_image(1, 1, 1000.0f);
    float* px = pixels(img);
    px[0] = 1001.0f;  // index = round(1001) - 1000 = 1 -> 20

    XpeModalityLutParams params{};
    params.mode           = XPE_MODALITY_LUT_TABLE;
    params.lutData        = lut;
    params.lutLength      = 3;
    params.lutFirstMapped = 1000;

    XpeErrorCode rc = xpe_apply_modality_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_FLOAT_EQ(px[0], 20.0f);
    free_image(img);
}

TEST(ModalityLut, TableMode_ClampingBounds) {
    // REQ-DISP-002: out-of-range input clamped to [0, lutLength-1]
    uint16_t lut[3] = {10, 20, 30};
    XpeImageBuffer img = make_float32_image(1, 2, 0.0f);
    float* px = pixels(img);
    px[0] = -999.0f; // below firstMapped -> index clamped to 0 -> 10
    px[1] =  999.0f; // above last -> index clamped to 2 -> 30

    XpeModalityLutParams params{};
    params.mode           = XPE_MODALITY_LUT_TABLE;
    params.lutData        = lut;
    params.lutLength      = 3;
    params.lutFirstMapped = 0;

    XpeErrorCode rc = xpe_apply_modality_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_FLOAT_EQ(px[0], 10.0f);
    EXPECT_FLOAT_EQ(px[1], 30.0f);
    free_image(img);
}

// =============================================================================
// REQ-DISP-003 to REQ-DISP-007: Error cases
// =============================================================================

TEST(ModalityLut, Error_NullImg) {
    // REQ-DISP-003: NULL img -> XPE_ERR_INVALID_INPUT
    XpeModalityLutParams params{};
    params.mode         = XPE_MODALITY_LUT_LINEAR;
    params.rescaleSlope = 1.0f;
    XpeErrorCode rc = xpe_apply_modality_lut(nullptr, &params);
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
}

TEST(ModalityLut, Error_NullParams) {
    // REQ-DISP-003: NULL params -> XPE_ERR_INVALID_INPUT
    XpeImageBuffer img = make_float32_image(2, 2, 0.0f);
    XpeErrorCode rc = xpe_apply_modality_lut(&img, nullptr);
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
    free_image(img);
}

TEST(ModalityLut, Error_WrongFormat) {
    // REQ-DISP-004: format != FLOAT32 -> XPE_ERR_UNSUPPORTED_FORMAT
    XpeImageBuffer img{};
    img.width = 2; img.height = 2;
    img.format = XPE_PIXEL_UINT16;
    img.bitsAllocated = 16; img.bitsStored = 16;
    img.dataSize = 8;
    uint16_t buf[4] = {0};
    img.data = buf;

    XpeModalityLutParams params{};
    params.mode         = XPE_MODALITY_LUT_LINEAR;
    params.rescaleSlope = 1.0f;
    XpeErrorCode rc = xpe_apply_modality_lut(&img, &params);
    EXPECT_EQ(rc, XPE_ERR_UNSUPPORTED_FORMAT);
    // image must not be modified on error
    EXPECT_EQ(buf[0], 0);
}

TEST(ModalityLut, Error_TableNullLutData) {
    // REQ-DISP-005: TABLE mode + lutData==NULL -> XPE_ERR_INVALID_INPUT
    XpeImageBuffer img = make_float32_image(2, 2, 0.0f);
    XpeModalityLutParams params{};
    params.mode      = XPE_MODALITY_LUT_TABLE;
    params.lutData   = nullptr;
    params.lutLength = 5;
    XpeErrorCode rc = xpe_apply_modality_lut(&img, &params);
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
    free_image(img);
}

TEST(ModalityLut, Error_TableZeroLength) {
    // REQ-DISP-006: TABLE mode + lutLength==0 -> XPE_ERR_INVALID_INPUT
    uint16_t lut[1] = {0};
    XpeImageBuffer img = make_float32_image(2, 2, 0.0f);
    XpeModalityLutParams params{};
    params.mode      = XPE_MODALITY_LUT_TABLE;
    params.lutData   = lut;
    params.lutLength = 0;
    XpeErrorCode rc = xpe_apply_modality_lut(&img, &params);
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
    free_image(img);
}

TEST(ModalityLut, Error_LinearZeroSlope) {
    // REQ-DISP-007: LINEAR + rescaleSlope==0.0f -> XPE_ERR_INVALID_INPUT
    XpeImageBuffer img = make_float32_image(2, 2, 100.0f);
    XpeModalityLutParams params{};
    params.mode             = XPE_MODALITY_LUT_LINEAR;
    params.rescaleSlope     = 0.0f;
    params.rescaleIntercept = 0.0f;
    XpeErrorCode rc = xpe_apply_modality_lut(&img, &params);
    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
    // image must not be modified on error
    EXPECT_FLOAT_EQ(pixels(img)[0], 100.0f);
    free_image(img);
}

// =============================================================================
// REQ-DISP-008: Performance test (<= 20ms for 3072x3072)
// =============================================================================

TEST(ModalityLut, Performance_3072x3072_Linear) {
    // REQ-DISP-008: LINEAR mode <= 20ms for 3072x3072
    XpeImageBuffer img = make_float32_image(3072, 3072, 1000.0f);
    XpeModalityLutParams params{};
    params.mode             = XPE_MODALITY_LUT_LINEAR;
    params.rescaleSlope     = 1.0f;
    params.rescaleIntercept = -1024.0f;

    auto t0 = std::chrono::high_resolution_clock::now();
    XpeErrorCode rc = xpe_apply_modality_lut(&img, &params);
    auto t1 = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();

    EXPECT_EQ(rc, XPE_OK);
    EXPECT_LE(ms, 20) << "ModalityLUT LINEAR 3072x3072 took " << ms << "ms (limit 20ms)";
    free_image(img);
}

// =============================================================================
// REQ-DISP-034: Edge case - 1x1 image
// =============================================================================

TEST(ModalityLut, EdgeCase_1x1Image) {
    // REQ-DISP-034: Handle 1x1 image without crash
    XpeImageBuffer img = make_float32_image(1, 1, 42.0f);
    XpeModalityLutParams params{};
    params.mode             = XPE_MODALITY_LUT_LINEAR;
    params.rescaleSlope     = 2.0f;
    params.rescaleIntercept = 0.0f;

    XpeErrorCode rc = xpe_apply_modality_lut(&img, &params);
    EXPECT_EQ(rc, XPE_OK);
    EXPECT_FLOAT_EQ(pixels(img)[0], 84.0f);
    free_image(img);
}
