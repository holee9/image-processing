/**
 * @file test_contrast_enhance.cpp
 * @brief TDD RED tests for SWU-2.3: Contrast Enhancement / CLAHE (REQ-ENH-013..017)
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

// Helper: compute standard deviation of a region
static float compute_stddev(const float* data, int n) {
    if (n <= 1) return 0.0f;
    double sum = 0, sum_sq = 0;
    for (int i = 0; i < n; i++) {
        sum += data[i];
        sum_sq += (double)data[i] * data[i];
    }
    double mean = sum / n;
    double var = (sum_sq / n) - (mean * mean);
    return (float)std::sqrt(std::max(0.0, var));
}

// REQ-ENH-CC-002: Null image returns XPE_ERR_INVALID_INPUT
TEST(ContrastEnhance, NullImage_ReturnsInvalidInput) {
    XpeClaheParams params{};
    params.clip_limit = 3.0f;
    params.tile_width = 8;
    params.tile_height = 8;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_contrast_enhance(nullptr, &params));
}

// REQ-ENH-013: Valid params returns XPE_OK
TEST(ContrastEnhance, ValidParams_ReturnsOk) {
    auto img = make_f32(64, 64, 500.0f);
    XpeClaheParams params{};
    params.clip_limit = 3.0f;
    params.tile_width = 8;
    params.tile_height = 8;
    EXPECT_EQ(XPE_OK, xpe_contrast_enhance(&img, &params));
    free_img(img);
}

// REQ-ENH-014: Null params uses defaults and returns XPE_OK (AC Scenario 7)
TEST(ContrastEnhance, NullParams_UsesDefaults_ReturnsOk) {
    auto img = make_f32(64, 64, 500.0f);
    EXPECT_EQ(XPE_OK, xpe_contrast_enhance(&img, nullptr));
    free_img(img);
}

// REQ-ENH-015: clip_limit < 1.0 returns XPE_ERR_INVALID_INPUT
TEST(ContrastEnhance, ClipLimitBelow1_ReturnsInvalidInput) {
    auto img = make_f32(64, 64, 500.0f);
    XpeClaheParams params{};
    params.clip_limit = 0.5f;
    params.tile_width = 8;
    params.tile_height = 8;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_contrast_enhance(&img, &params));
    free_img(img);
}

// REQ-ENH-016: tile_width < 2 returns XPE_ERR_INVALID_INPUT
TEST(ContrastEnhance, TileWidthBelow2_ReturnsInvalidInput) {
    auto img = make_f32(64, 64, 500.0f);
    XpeClaheParams params{};
    params.clip_limit = 3.0f;
    params.tile_width = 1;
    params.tile_height = 8;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_contrast_enhance(&img, &params));
    free_img(img);
}

// REQ-ENH-016: tile_height < 2 returns XPE_ERR_INVALID_INPUT
TEST(ContrastEnhance, TileHeightBelow2_ReturnsInvalidInput) {
    auto img = make_f32(64, 64, 500.0f);
    XpeClaheParams params{};
    params.clip_limit = 3.0f;
    params.tile_width = 8;
    params.tile_height = 1;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_contrast_enhance(&img, &params));
    free_img(img);
}

// REQ-ENH-016: Image smaller than 2*tile grid returns XPE_ERR_INVALID_INPUT
TEST(ContrastEnhance, ImageSmallerThanTileGrid_ReturnsInvalidInput) {
    auto img = make_f32(16, 16, 500.0f);
    XpeClaheParams params{};
    params.clip_limit = 3.0f;
    params.tile_width = 16;  // image 16 wide, need >= 2*16 = 32
    params.tile_height = 8;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_contrast_enhance(&img, &params));
    free_img(img);
}

// AC-04: Low-contrast region contrast improves by >= 20%
TEST(ContrastEnhance, LowContrastRegion_ContrastImproves) {
    const uint32_t W = 256, H = 256;
    auto img = make_f32(W, H, 0.0f);
    float* px = static_cast<float*>(img.data);

    // Fill with low-contrast values in center [495, 505]
    for (uint32_t y = 0; y < H; y++) {
        for (uint32_t x = 0; x < W; x++) {
            float val = 495.0f + 10.0f * ((float)(x + y) / (float)(W + H));
            px[y * W + x] = val;
        }
    }

    // Measure stddev in center 128x128 region before
    std::vector<float> center_before(128 * 128);
    for (int y = 64; y < 192; y++) {
        for (int x = 64; x < 192; x++) {
            center_before[(y - 64) * 128 + (x - 64)] = px[y * W + x];
        }
    }
    float stddev_before = compute_stddev(center_before.data(), 128 * 128);

    XpeClaheParams params{};
    params.clip_limit = 3.0f;
    params.tile_width = 8;
    params.tile_height = 8;

    ASSERT_EQ(XPE_OK, xpe_contrast_enhance(&img, &params));

    // Measure stddev in center region after
    std::vector<float> center_after(128 * 128);
    for (int y = 64; y < 192; y++) {
        for (int x = 64; x < 192; x++) {
            center_after[(y - 64) * 128 + (x - 64)] = px[y * W + x];
        }
    }
    float stddev_after = compute_stddev(center_after.data(), 128 * 128);

    // Contrast should improve by at least 20%
    EXPECT_GE(stddev_after, stddev_before * 1.2f)
        << "stddev_before: " << stddev_before << ", stddev_after: " << stddev_after;
    free_img(img);
}

// Edge case: Flat image should produce no artifacts
TEST(ContrastEnhance, FlatImage_NoArtifacts) {
    const uint32_t W = 64, H = 64;
    auto img = make_f32(W, H, 500.0f);

    XpeClaheParams params{};
    params.clip_limit = 3.0f;
    params.tile_width = 8;
    params.tile_height = 8;

    ASSERT_EQ(XPE_OK, xpe_contrast_enhance(&img, &params));

    float* px = static_cast<float*>(img.data);
    float first = px[0];
    for (uint32_t i = 1; i < W * H; i++) {
        EXPECT_NEAR(first, px[i], 1.0f) << "Flat image diverged at pixel " << i;
    }
    free_img(img);
}

// REQ-ENH-017: Performance <= 50ms for 3072x3072
TEST(ContrastEnhance, Performance_3072x3072_Within50ms) {
    auto img = make_f32(3072, 3072, 500.0f);
    XpeClaheParams params{};
    params.clip_limit = 3.0f;
    params.tile_width = 8;
    params.tile_height = 8;

    auto t0 = std::chrono::high_resolution_clock::now();
    ASSERT_EQ(XPE_OK, xpe_contrast_enhance(&img, &params));
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - t0).count();
    EXPECT_LE(ms, 50) << "CLAHE 3072x3072 took " << ms << "ms, budget 50ms";
    free_img(img);
}

} // namespace
