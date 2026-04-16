/**
 * @file test_log_transform.cpp
 * @brief TDD RED tests for SWU-2.1: Log Transform and Inverse (REQ-ENH-001..006)
 * SPEC: SPEC-XPE-P1B-ENH v1.0.0  IEC 62304 Class B
 */

#include <gtest/gtest.h>
#include "xpe/enhance_basic/enhance_basic_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <chrono>

namespace {

// Helper: create float32 image filled with a value
static XpeImageBuffer make_f32(uint32_t w, uint32_t h, float fill = 0.0f) {
    XpeImageBuffer img{};
    img.width = w;
    img.height = h;
    img.format = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = 32;
    img.bitsStored = 32;
    img.dataSize = (size_t)w * h * sizeof(float);
    img.data = malloc(img.dataSize);
    if (img.data) {
        float* px = static_cast<float*>(img.data);
        std::fill(px, px + (size_t)w * h, fill);
    }
    return img;
}

static void free_img(XpeImageBuffer& img) {
    free(img.data);
    img.data = nullptr;
}

// REQ-ENH-001: Forward transform applies output[i] = normFactor * log10(input[i] + 1.0)
TEST(LogTransform, ForwardTransform_SinglePixel_CorrectFormula) {
    auto img = make_f32(1, 1, 100.0f);
    ASSERT_EQ(XPE_OK, xpe_log_transform(&img, 1000.0f));
    float* px = static_cast<float*>(img.data);
    EXPECT_NEAR(1000.0f * std::log10(101.0f), px[0], 1e-3f);
    free_img(img);
}

// REQ-ENH-002: Negative pixels clamped to zero before log
TEST(LogTransform, NegativePixel_ClampedToZero) {
    auto img = make_f32(1, 1, -5.0f);
    ASSERT_EQ(XPE_OK, xpe_log_transform(&img, 1.0f));
    float* px = static_cast<float*>(img.data);
    EXPECT_FLOAT_EQ(0.0f, px[0]);  // log10(0+1) = 0
    free_img(img);
}

// REQ-ENH-002: Zero pixel produces log10(0+1) = 0
TEST(LogTransform, ZeroPixel_ProducesZero) {
    auto img = make_f32(1, 1, 0.0f);
    ASSERT_EQ(XPE_OK, xpe_log_transform(&img, 500.0f));
    float* px = static_cast<float*>(img.data);
    EXPECT_FLOAT_EQ(0.0f, px[0]);
    free_img(img);
}

// REQ-ENH-003: Zero normFactor returns XPE_ERR_INVALID_INPUT
TEST(LogTransform, ZeroNormFactor_ReturnsInvalidInput) {
    auto img = make_f32(4, 4, 100.0f);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_log_transform(&img, 0.0f));
    free_img(img);
}

// REQ-ENH-003: Negative normFactor returns XPE_ERR_INVALID_INPUT
TEST(LogTransform, NegativeNormFactor_ReturnsInvalidInput) {
    auto img = make_f32(4, 4, 100.0f);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_log_transform(&img, -1.0f));
    free_img(img);
}

// REQ-ENH-CC-002: Null image returns XPE_ERR_INVALID_INPUT
TEST(LogTransform, NullImage_ReturnsInvalidInput) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_log_transform(nullptr, 1.0f));
}

// REQ-ENH-004: Inverse transform applies output[i] = pow(10, input[i]/normFactor) - 1.0
TEST(LogTransform, InverseTransform_SinglePixel_CorrectFormula) {
    auto img = make_f32(1, 1, 2.0f);  // log-domain value
    ASSERT_EQ(XPE_OK, xpe_log_inverse(&img, 1.0f));
    float* px = static_cast<float*>(img.data);
    EXPECT_NEAR(std::pow(10.0f, 2.0f) - 1.0f, px[0], 1e-3f);
    free_img(img);
}

// REQ-ENH-005: Inverse with zero normFactor returns XPE_ERR_INVALID_INPUT
TEST(LogTransform, InverseZeroNormFactor_ReturnsInvalidInput) {
    auto img = make_f32(4, 4, 1.0f);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_log_inverse(&img, 0.0f));
    free_img(img);
}

// REQ-ENH-005: Inverse with negative normFactor returns XPE_ERR_INVALID_INPUT
TEST(LogTransform, InverseNegativeNormFactor_ReturnsInvalidInput) {
    auto img = make_f32(4, 4, 1.0f);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_log_inverse(&img, -1.0f));
    free_img(img);
}

// REQ-ENH-CC-002: Inverse with null image returns XPE_ERR_INVALID_INPUT
TEST(LogTransform, InverseNullImage_ReturnsInvalidInput) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_log_inverse(nullptr, 1.0f));
}

// AC-01: Round-trip restores pixel values within 1e-4 relative error
TEST(LogTransform, RoundTrip_RestoresWithin1e4RelativeError) {
    const int W = 64, H = 64;
    auto img = make_f32(W, H, 0.0f);
    float* px = static_cast<float*>(img.data);
    for (int i = 0; i < W * H; i++) px[i] = (float)(i + 1) * 10.0f;

    auto orig = make_f32(W, H, 0.0f);
    std::memcpy(orig.data, img.data, (size_t)W * H * sizeof(float));

    ASSERT_EQ(XPE_OK, xpe_log_transform(&img, 1000.0f));
    ASSERT_EQ(XPE_OK, xpe_log_inverse(&img, 1000.0f));

    float* orig_px = static_cast<float*>(orig.data);
    float* res_px = static_cast<float*>(img.data);
    for (int i = 0; i < W * H; i++) {
        if (orig_px[i] > 1e-6f) {
            EXPECT_NEAR(orig_px[i], res_px[i], orig_px[i] * 1e-4f) << " at pixel " << i;
        }
    }
    free_img(img);
    free_img(orig);
}

// REQ-ENH-006: Performance <= 15ms for 3072x3072 float32
TEST(LogTransform, Performance_3072x3072_Within15ms) {
    auto img = make_f32(3072, 3072, 1000.0f);
    auto start = std::chrono::high_resolution_clock::now();
    ASSERT_EQ(XPE_OK, xpe_log_transform(&img, 1000.0f));
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_LE(ms, 15) << "xpe_log_transform took " << ms << "ms, budget is 15ms";
    free_img(img);
}

// REQ-ENH-006: Inverse performance <= 15ms for 3072x3072 float32
TEST(LogTransform, InversePerformance_3072x3072_Within15ms) {
    auto img = make_f32(3072, 3072, 2.5f);
    auto start = std::chrono::high_resolution_clock::now();
    ASSERT_EQ(XPE_OK, xpe_log_inverse(&img, 1000.0f));
    auto end = std::chrono::high_resolution_clock::now();
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    EXPECT_LE(ms, 15) << "xpe_log_inverse took " << ms << "ms, budget is 15ms";
    free_img(img);
}

// REQ-ENH-001: Multiple pixel values transform correctly
TEST(LogTransform, MultiplePixels_AllTransformedCorrectly) {
    const uint32_t W = 4, H = 1;
    auto img = make_f32(W, H, 0.0f);
    float* px = static_cast<float*>(img.data);
    px[0] = 0.0f;
    px[1] = 1.0f;
    px[2] = 99.0f;
    px[3] = 9999.0f;

    ASSERT_EQ(XPE_OK, xpe_log_transform(&img, 1.0f));

    EXPECT_NEAR(std::log10(1.0f), px[0], 1e-5f);   // log10(0+1) = 0
    EXPECT_NEAR(std::log10(2.0f), px[1], 1e-5f);   // log10(1+1)
    EXPECT_NEAR(std::log10(100.0f), px[2], 1e-5f); // log10(99+1) = 2.0
    EXPECT_NEAR(std::log10(10000.0f), px[3], 1e-5f); // log10(9999+1) = 4.0
    free_img(img);
}

} // namespace
