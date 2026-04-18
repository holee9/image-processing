/**
 * @file test_noise_reduce.cpp
 * @brief TDD RED tests for SWU-2.2: Noise Reduction (REQ-ENH-007..012)
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
#include <random>

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

// Helper: compute SNR in dB between clean signal and noisy/denoised image
static float compute_snr_db(const float* signal, const float* noisy, int n) {
    double sig_pow = 0, noise_pow = 0;
    for (int i = 0; i < n; i++) {
        sig_pow += (double)signal[i] * signal[i];
        noise_pow += (double)(noisy[i] - signal[i]) * (noisy[i] - signal[i]);
    }
    if (noise_pow < 1e-10) return 999.0f;
    return (float)(10.0 * std::log10(sig_pow / noise_pow));
}

// REQ-ENH-009: Null params returns XPE_ERR_INVALID_INPUT
TEST(NoiseReduce, NullParams_ReturnsInvalidInput) {
    auto img = make_f32(16, 16, 500.0f);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_noise_reduce(&img, nullptr));
    free_img(img);
}

// REQ-ENH-CC-002: Null image returns XPE_ERR_INVALID_INPUT
TEST(NoiseReduce, NullImage_ReturnsInvalidInput) {
    XpeNoiseReduceParams params{};
    params.mode = XPE_NOISE_BILATERAL;
    params.sigma_space = 3.0f;
    params.sigma_range = 50.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_noise_reduce(nullptr, &params));
}

// REQ-ENH-007: Bilateral on flat image is a no-op
TEST(NoiseReduce, BilateralFilter_FlatImage_NoChange) {
    const uint32_t W = 32, H = 32;
    auto img = make_f32(W, H, 500.0f);

    XpeNoiseReduceParams params{};
    params.mode = XPE_NOISE_BILATERAL;
    params.sigma_space = 3.0f;
    params.sigma_range = 50.0f;

    ASSERT_EQ(XPE_OK, xpe_noise_reduce(&img, &params));

    float* px = static_cast<float*>(img.data);
    for (uint32_t i = 0; i < W * H; i++) {
        EXPECT_NEAR(500.0f, px[i], 1.0f) << "pixel " << i << " changed on flat image";
    }
    free_img(img);
}

// AC-02: Bilateral filter SNR improvement >= 3 dB
TEST(NoiseReduce, BilateralFilter_NoisyImage_SNRImproves) {
    const uint32_t W = 256, H = 256;
    const int N = W * H;

    // Create clean image with known pattern
    auto clean = make_f32(W, H, 0.0f);
    float* clean_px = static_cast<float*>(clean.data);
    for (int i = 0; i < N; i++) {
        clean_px[i] = 500.0f + 200.0f * std::sin((float)i * 0.01f);
    }

    // Create noisy copy
    auto noisy = make_f32(W, H, 0.0f);
    std::memcpy(noisy.data, clean.data, (size_t)N * sizeof(float));
    float* noisy_px = static_cast<float*>(noisy.data);

    std::mt19937 rng(42);
    std::normal_distribution<float> noise(0.0f, 30.0f);
    for (int i = 0; i < N; i++) {
        noisy_px[i] += noise(rng);
    }

    float snr_before = compute_snr_db(clean_px, noisy_px, N);

    XpeNoiseReduceParams params{};
    params.mode = XPE_NOISE_BILATERAL;
    params.sigma_space = 3.0f;
    params.sigma_range = 50.0f;

    ASSERT_EQ(XPE_OK, xpe_noise_reduce(&noisy, &params));

    float snr_after = compute_snr_db(clean_px, noisy_px, N);
    EXPECT_GE(snr_after - snr_before, 3.0f)
        << "SNR before: " << snr_before << " dB, SNR after: " << snr_after << " dB";

    free_img(clean);
    free_img(noisy);
}

// REQ-ENH-010: Negative sigma_space returns XPE_ERR_INVALID_INPUT
TEST(NoiseReduce, BilateralFilter_NegativeSigmaSpace_ReturnsInvalidInput) {
    auto img = make_f32(16, 16, 500.0f);
    XpeNoiseReduceParams params{};
    params.mode = XPE_NOISE_BILATERAL;
    params.sigma_space = -1.0f;
    params.sigma_range = 50.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_noise_reduce(&img, &params));
    free_img(img);
}

// REQ-ENH-010: Negative sigma_range returns XPE_ERR_INVALID_INPUT
TEST(NoiseReduce, BilateralFilter_NegativeSigmaRange_ReturnsInvalidInput) {
    auto img = make_f32(16, 16, 500.0f);
    XpeNoiseReduceParams params{};
    params.mode = XPE_NOISE_BILATERAL;
    params.sigma_space = 3.0f;
    params.sigma_range = -1.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_noise_reduce(&img, &params));
    free_img(img);
}

// REQ-ENH-010: Zero sigma_space returns XPE_ERR_INVALID_INPUT
TEST(NoiseReduce, BilateralFilter_ZeroSigmaSpace_ReturnsInvalidInput) {
    auto img = make_f32(16, 16, 500.0f);
    XpeNoiseReduceParams params{};
    params.mode = XPE_NOISE_BILATERAL;
    params.sigma_space = 0.0f;
    params.sigma_range = 50.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_noise_reduce(&img, &params));
    free_img(img);
}

// REQ-ENH-008: NLM with valid small image returns XPE_OK
TEST(NoiseReduce, NLM_ValidSmallImage_ReturnsOk) {
    auto img = make_f32(64, 64, 500.0f);
    XpeNoiseReduceParams params{};
    params.mode = XPE_NOISE_NLM;
    params.search_window = 21;
    params.patch_size = 7;
    params.h_param = 10.0f;
    EXPECT_EQ(XPE_OK, xpe_noise_reduce(&img, &params));
    free_img(img);
}

// REQ-ENH-008: NLM with even search_window -- implementation may correct or reject
TEST(NoiseReduce, NLM_EvenWindowSize_HandledGracefully) {
    auto img = make_f32(64, 64, 500.0f);
    XpeNoiseReduceParams params{};
    params.mode = XPE_NOISE_NLM;
    params.search_window = 20;  // even -- may be corrected to 21 or rejected
    params.patch_size = 7;
    params.h_param = 10.0f;
    XpeErrorCode rc = xpe_noise_reduce(&img, &params);
    // Either XPE_OK (corrected) or XPE_ERR_INVALID_INPUT (rejected) is acceptable
    EXPECT_TRUE(rc == XPE_OK || rc == XPE_ERR_INVALID_INPUT)
        << "Expected XPE_OK or XPE_ERR_INVALID_INPUT, got " << rc;
    free_img(img);
}

// REQ-ENH-011: Sigma estimation on known noise
// AC Scenario 5: estimated sigma within +/- 20% of true sigma
TEST(NoiseReduce, SigmaEstimation_KnownNoise) {
    const uint32_t W = 256, H = 256;
    auto img = make_f32(W, H, 500.0f);
    float* px = static_cast<float*>(img.data);

    std::mt19937 rng(123);
    std::normal_distribution<float> noise(0.0f, 25.0f);
    for (uint32_t i = 0; i < W * H; i++) {
        px[i] += noise(rng);
    }

    float outSigma = 0.0f;
    ASSERT_EQ(XPE_OK, xpe_noise_estimate_sigma(&img, &outSigma));
    EXPECT_GE(outSigma, 20.0f) << "Estimated sigma too low: " << outSigma;
    EXPECT_LE(outSigma, 30.0f) << "Estimated sigma too high: " << outSigma;
    free_img(img);
}

// REQ-ENH-011: Null output pointer returns XPE_ERR_INVALID_INPUT
TEST(NoiseReduce, SigmaEstimation_NullOutput_ReturnsInvalidInput) {
    auto img = make_f32(32, 32, 500.0f);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_noise_estimate_sigma(&img, nullptr));
    free_img(img);
}

// REQ-ENH-CC-002: Sigma estimation with null image returns XPE_ERR_INVALID_INPUT
TEST(NoiseReduce, SigmaEstimation_NullImage_ReturnsInvalidInput) {
    float outSigma = 0.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_noise_estimate_sigma(nullptr, &outSigma));
}

// REQ-ENH-012: Performance <= 100ms for 512x512 bilateral (smaller for test speed)
TEST(NoiseReduce, Performance_Bilateral_Small_Within100ms) {
    auto img = make_f32(512, 512, 500.0f);
    XpeNoiseReduceParams params{};
    params.mode = XPE_NOISE_BILATERAL;
    params.sigma_space = 3.0f;
    params.sigma_range = 50.0f;

    auto t0 = std::chrono::high_resolution_clock::now();
    ASSERT_EQ(XPE_OK, xpe_noise_reduce(&img, &params));
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - t0).count();
    EXPECT_LE(ms, 100) << "Bilateral 512x512 took " << ms << "ms, budget 100ms";
    free_img(img);
}

} // namespace
