/**
 * @file test_edge_enhance.cpp
 * @brief TDD RED tests for SWU-2.4: Edge Enhancement / USM (REQ-ENH-018..022)
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
#include <vector>

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

// REQ-ENH-CC-002: Null image returns XPE_ERR_INVALID_INPUT
TEST(EdgeEnhance, NullImage_ReturnsInvalidInput) {
    XpeUsmParams params{};
    params.amount = 0.5f;
    params.radius = 2.0f;
    params.threshold = 10.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_edge_enhance(nullptr, &params));
}

// REQ-ENH-019: Null params uses defaults and returns XPE_OK
TEST(EdgeEnhance, NullParams_UsesDefaults_ReturnsOk) {
    auto img = make_f32(64, 64, 500.0f);
    EXPECT_EQ(XPE_OK, xpe_edge_enhance(&img, nullptr));
    free_img(img);
}

// REQ-ENH-020: amount > 5.0 returns XPE_ERR_INVALID_INPUT
TEST(EdgeEnhance, AmountTooLarge_ReturnsInvalidInput) {
    auto img = make_f32(32, 32, 500.0f);
    XpeUsmParams params{};
    params.amount = 6.0f;
    params.radius = 2.0f;
    params.threshold = 10.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_edge_enhance(&img, &params));
    free_img(img);
}

// REQ-ENH-020: amount < 0.0 returns XPE_ERR_INVALID_INPUT
TEST(EdgeEnhance, AmountNegative_ReturnsInvalidInput) {
    auto img = make_f32(32, 32, 500.0f);
    XpeUsmParams params{};
    params.amount = -0.1f;
    params.radius = 2.0f;
    params.threshold = 10.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_edge_enhance(&img, &params));
    free_img(img);
}

// REQ-ENH-020: radius < 0.5 returns XPE_ERR_INVALID_INPUT
TEST(EdgeEnhance, RadiusTooSmall_ReturnsInvalidInput) {
    auto img = make_f32(32, 32, 500.0f);
    XpeUsmParams params{};
    params.amount = 1.0f;
    params.radius = 0.1f;
    params.threshold = 10.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_edge_enhance(&img, &params));
    free_img(img);
}

// REQ-ENH-020: radius > 10.0 returns XPE_ERR_INVALID_INPUT
TEST(EdgeEnhance, RadiusTooLarge_ReturnsInvalidInput) {
    auto img = make_f32(32, 32, 500.0f);
    XpeUsmParams params{};
    params.amount = 1.0f;
    params.radius = 11.0f;
    params.threshold = 10.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_edge_enhance(&img, &params));
    free_img(img);
}

// REQ-ENH-020: negative threshold returns XPE_ERR_INVALID_INPUT
TEST(EdgeEnhance, NegativeThreshold_ReturnsInvalidInput) {
    auto img = make_f32(32, 32, 500.0f);
    XpeUsmParams params{};
    params.amount = 1.0f;
    params.radius = 2.0f;
    params.threshold = -1.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_edge_enhance(&img, &params));
    free_img(img);
}

// REQ-ENH-018: amount=0 means no sharpening, output equals input
TEST(EdgeEnhance, ZeroAmount_ImageUnchanged) {
    const uint32_t W = 64, H = 64;
    auto img = make_f32(W, H, 0.0f);
    float* px = static_cast<float*>(img.data);
    for (uint32_t i = 0; i < W * H; i++) px[i] = (float)(i % 256) * 2.5f;

    // Save original
    std::vector<float> orig((size_t)W * H);
    std::memcpy(orig.data(), img.data, (size_t)W * H * sizeof(float));

    XpeUsmParams params{};
    params.amount = 0.0f;
    params.radius = 2.0f;
    params.threshold = 10.0f;

    ASSERT_EQ(XPE_OK, xpe_edge_enhance(&img, &params));

    for (uint32_t i = 0; i < W * H; i++) {
        EXPECT_FLOAT_EQ(orig[i], px[i]) << "pixel " << i << " changed with amount=0";
    }
    free_img(img);
}

// AC-05 / REQ-ENH-021: Overshoot clamp -- no pixel exceeds bound
TEST(EdgeEnhance, OvershootClamp_NeverExceedsBound) {
    const uint32_t W = 256, H = 1;
    auto img = make_f32(W, H, 0.0f);
    float* px = static_cast<float*>(img.data);

    // Step edge: left half = 100.0, right half = 1000.0
    for (uint32_t x = 0; x < W; x++) {
        px[x] = (x < W / 2) ? 100.0f : 1000.0f;
    }

    // Save original
    std::vector<float> orig((size_t)W * H);
    std::memcpy(orig.data(), img.data, (size_t)W * H * sizeof(float));

    XpeUsmParams params{};
    params.amount = 2.0f;
    params.radius = 2.0f;
    params.threshold = 5.0f;

    ASSERT_EQ(XPE_OK, xpe_edge_enhance(&img, &params));

    for (uint32_t i = 0; i < W * H; i++) {
        float bound = std::max(orig[i] * 2.0f, orig[i] + params.amount * params.threshold);
        EXPECT_LE(px[i], bound + 0.01f)
            << "pixel " << i << ": value=" << px[i] << " exceeds bound=" << bound;
    }
    free_img(img);
}

// REQ-ENH-018: Threshold gating -- flat region remains unchanged
TEST(EdgeEnhance, ThresholdGating_FlatRegion_Unchanged) {
    const uint32_t W = 64, H = 64;
    auto img = make_f32(W, H, 500.0f);  // perfectly flat

    std::vector<float> orig((size_t)W * H, 500.0f);

    XpeUsmParams params{};
    params.amount = 2.0f;
    params.radius = 2.0f;
    params.threshold = 10.0f;

    ASSERT_EQ(XPE_OK, xpe_edge_enhance(&img, &params));

    float* px = static_cast<float*>(img.data);
    for (uint32_t i = 0; i < W * H; i++) {
        EXPECT_NEAR(500.0f, px[i], 0.1f) << "flat pixel " << i << " changed by USM";
    }
    free_img(img);
}

// REQ-ENH-022: Performance <= 20ms for 3072x3072
TEST(EdgeEnhance, Performance_3072x3072_Within20ms) {
    auto img = make_f32(3072, 3072, 500.0f);
    XpeUsmParams params{};
    params.amount = 0.5f;
    params.radius = 2.0f;
    params.threshold = 10.0f;

    auto t0 = std::chrono::high_resolution_clock::now();
    ASSERT_EQ(XPE_OK, xpe_edge_enhance(&img, &params));
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - t0).count();
    EXPECT_LE(ms, 20) << "USM 3072x3072 took " << ms << "ms, budget 20ms";
    free_img(img);
}

} // namespace
