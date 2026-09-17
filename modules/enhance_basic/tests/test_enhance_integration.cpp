/**
 * @file test_enhance_integration.cpp
 * @brief TDD RED tests: integration, thread safety, P/Invoke, pipeline perf (REQ-ENH-CC-001..005)
 * SPEC: SPEC-XPE-P1B-ENH v1.0.0  IEC 62304 Class B
 */

#include <gtest/gtest.h>
#include "xpe/enhance_basic/enhance_basic_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <thread>
#include <vector>

// #181 (QA-B-93): retention is measured on the CRT heap, not on the working set.
#include "heap_growth.h"

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

static void set_body_part(XpeImageMetadata& meta, const char* bodyPart) {
    std::snprintf(meta.bodyPart, sizeof(meta.bodyPart), "%s", bodyPart ? bodyPart : "");
}

// REQ-ENH-CC-002: All functions reject null img with XPE_ERR_INVALID_INPUT
TEST(EnhanceIntegration, AllFunctions_NullImg_ReturnInvalidInput) {
    // xpe_log_transform
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_log_transform(nullptr, 1.0f));

    // xpe_log_inverse
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_log_inverse(nullptr, 1.0f));

    // xpe_noise_reduce
    XpeNoiseReduceParams nr_params{};
    nr_params.mode = XPE_NOISE_BILATERAL;
    nr_params.sigma_space = 3.0f;
    nr_params.sigma_range = 50.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_noise_reduce(nullptr, &nr_params));

    // xpe_noise_estimate_sigma
    float sigma = 0.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_noise_estimate_sigma(nullptr, &sigma));

    // xpe_contrast_enhance
    XpeClaheParams clahe_params{};
    clahe_params.clip_limit = 3.0f;
    clahe_params.tile_width = 8;
    clahe_params.tile_height = 8;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_contrast_enhance(nullptr, &clahe_params));

    // xpe_edge_enhance
    XpeUsmParams usm_params{};
    usm_params.amount = 0.5f;
    usm_params.radius = 2.0f;
    usm_params.threshold = 10.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_edge_enhance(nullptr, &usm_params));

    // xpe_calc_exposure_index
    XpeImageMetadata meta{};
    set_body_part(meta, "CHEST");
    float outEI = 0.0f, outDI = 0.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_calc_exposure_index(nullptr, &meta, &outEI, &outDI));
}

// AC-09 / REQ-ENH-CC-005: Full pipeline 3072x3072 within 200ms
TEST(EnhanceIntegration, FullPipeline_3072x3072_Within200ms) {
    auto img = make_f32(3072, 3072, 500.0f);

    // Prepare metadata for EI
    XpeImageMetadata meta{};
    set_body_part(meta, "CHEST");
    float outEI = 0.0f, outDI = 0.0f;

    // Prepare params
    XpeNoiseReduceParams nr_params{};
    nr_params.mode = XPE_NOISE_BILATERAL;
    nr_params.sigma_space = 3.0f;
    nr_params.sigma_range = 50.0f;

    XpeClaheParams clahe_params{};
    clahe_params.clip_limit = 3.0f;
    clahe_params.tile_width = 8;
    clahe_params.tile_height = 8;

    XpeUsmParams usm_params{};
    usm_params.amount = 0.5f;
    usm_params.radius = 2.0f;
    usm_params.threshold = 10.0f;

    auto t0 = std::chrono::high_resolution_clock::now();

    // Pipeline order: EI -> log -> noise -> contrast -> edge
    ASSERT_EQ(XPE_OK, xpe_calc_exposure_index(&img, &meta, &outEI, &outDI));
    ASSERT_EQ(XPE_OK, xpe_log_transform(&img, 1000.0f));
    ASSERT_EQ(XPE_OK, xpe_noise_reduce(&img, &nr_params));
    ASSERT_EQ(XPE_OK, xpe_contrast_enhance(&img, &clahe_params));
    ASSERT_EQ(XPE_OK, xpe_edge_enhance(&img, &usm_params));

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - t0).count();
    EXPECT_LE(ms, 200) << "Full pipeline took " << ms << "ms, budget 200ms";

    free_img(img);
}

// AC-08 / REQ-ENH-CC-001: Struct sizes are P/Invoke compatible
TEST(EnhanceIntegration, StructSizes_PInvokeCompatible) {
    // XpeNoiseReduceParams: enum(4) + 2*float(8) + 2*int32(8) + float(4) = 24 bytes
    // With pragma pack(8), may have padding
    // Verify minimum expected sizes
    EXPECT_GE(sizeof(XpeNoiseReduceParams), 24u)
        << "XpeNoiseReduceParams too small for P/Invoke";
    EXPECT_LE(sizeof(XpeNoiseReduceParams), 32u)
        << "XpeNoiseReduceParams too large, unexpected padding";

    // XpeClaheParams: float(4) + 2*int32(8) = 12 bytes
    EXPECT_GE(sizeof(XpeClaheParams), 12u)
        << "XpeClaheParams too small for P/Invoke";
    EXPECT_LE(sizeof(XpeClaheParams), 16u)
        << "XpeClaheParams too large, unexpected padding";

    // XpeUsmParams: 3*float(12) = 12 bytes
    EXPECT_GE(sizeof(XpeUsmParams), 12u)
        << "XpeUsmParams too small for P/Invoke";
    EXPECT_LE(sizeof(XpeUsmParams), 16u)
        << "XpeUsmParams too large, unexpected padding";
}

// AC-10 / REQ-ENH-CC-004: Thread safety -- concurrent bilateral on independent buffers
TEST(EnhanceIntegration, ThreadSafety_ConcurrentBilateral) {
    const uint32_t W = 128, H = 128;
    const int N = W * H;

    // Create two independent images with same data
    auto img1 = make_f32(W, H, 0.0f);
    auto img2 = make_f32(W, H, 0.0f);
    float* px1 = static_cast<float*>(img1.data);
    float* px2 = static_cast<float*>(img2.data);
    for (int i = 0; i < N; i++) {
        float v = 500.0f + 100.0f * std::sin((float)i * 0.05f);
        px1[i] = v;
        px2[i] = v;
    }

    // Single-threaded reference: process a copy
    auto ref = make_f32(W, H, 0.0f);
    std::memcpy(ref.data, img1.data, (size_t)N * sizeof(float));

    XpeNoiseReduceParams params{};
    params.mode = XPE_NOISE_BILATERAL;
    params.sigma_space = 3.0f;
    params.sigma_range = 50.0f;

    ASSERT_EQ(XPE_OK, xpe_noise_reduce(&ref, &params));

    // Concurrent execution
    XpeErrorCode rc1 = XPE_ERR_NOT_INITIALIZED;
    XpeErrorCode rc2 = XPE_ERR_NOT_INITIALIZED;

    std::thread t1([&]() {
        XpeNoiseReduceParams p = params;
        rc1 = xpe_noise_reduce(&img1, &p);
    });
    std::thread t2([&]() {
        XpeNoiseReduceParams p = params;
        rc2 = xpe_noise_reduce(&img2, &p);
    });

    t1.join();
    t2.join();

    ASSERT_EQ(XPE_OK, rc1);
    ASSERT_EQ(XPE_OK, rc2);

    // Results must match single-threaded reference
    float* ref_px = static_cast<float*>(ref.data);
    for (int i = 0; i < N; i++) {
        EXPECT_FLOAT_EQ(ref_px[i], px1[i]) << "Thread 1 diverged at pixel " << i;
        EXPECT_FLOAT_EQ(ref_px[i], px2[i]) << "Thread 2 diverged at pixel " << i;
    }

    free_img(img1);
    free_img(img2);
    free_img(ref);
}

// Endurance measurement constants (shared by the two heap-growth tests below).
// WARMUP exists because the first calls of a run fault in fresh heap pages and
// grow the CRT allocator arena; counting that one-time cost as "leak" would make
// the threshold a measure of startup, not of retention.  The baseline snapshot is
// taken after warm-up so only steady-state growth is scored.
// (heap_growth::kWarmup cycles; the bound is heap_growth::MaxBlocks / kMaxBytes)

// REQ-ENH-CC-003: No heap leak -- 1000 iterations of log_transform
// #105 G3: heap growth is now measured, not merely assumed from "did not crash".
TEST(EnhanceIntegration, NoHeapLeak_1000Iterations) {
#ifndef _WIN32
    GTEST_SKIP() << "CRT heap walk is Windows-only in this build";
#endif
    auto img = make_f32(64, 64, 500.0f);
    float* px = static_cast<float*>(img.data);

    auto one_cycle = [&](int i) {
        // Reset pixel values each iteration to avoid overflow
        std::fill(px, px + 64 * 64, 500.0f);
        ASSERT_EQ(XPE_OK, xpe_log_transform(&img, 1000.0f)) << "Iteration " << i << " failed";
    };
    const heap_growth::Growth g = heap_growth::Measure(one_cycle);

    // log_transform is in-place and allocates nothing, so any growth here is a
    // regression, not a tolerance.
    GTEST_LOG_(INFO) << heap_growth::Describe(g);
    EXPECT_LT(g.heap.blocks, heap_growth::MaxBlocks(g.cycles))
        << g.cycles << " log_transform cycles left blocks allocated";
    EXPECT_LT(g.heap.bytes, heap_growth::kMaxBytes)
        << g.cycles << " log_transform cycles left bytes allocated";

    // If we got here without crash/corruption, no double-free or heap corruption
    free_img(img);
}

// REQ-ENH-CC-003 (#105 G3): heap growth over the *allocating* enhance paths.
// log_transform above operates in place and never touches the heap, so on its own
// it cannot detect a retention defect.  noise_reduce / contrast_enhance /
// edge_enhance each allocate per-call std::vector scratch buffers (ring rows, LUT
// tiles, output copies); this loop is the probe that can actually fail.
static void AllocatingPathCycles(size_t leakBytes, heap_growth::Growth& out) {
    auto img = make_f32(64, 64, 500.0f);
    float* px = static_cast<float*>(img.data);

    XpeNoiseReduceParams nr_params{};
    nr_params.mode = XPE_NOISE_BILATERAL;
    nr_params.sigma_space = 3.0f;
    nr_params.sigma_range = 50.0f;

    XpeClaheParams clahe_params{};
    clahe_params.clip_limit = 3.0f;
    clahe_params.tile_width = 8;
    clahe_params.tile_height = 8;

    XpeUsmParams usm_params{};
    usm_params.amount = 0.5f;
    usm_params.radius = 2.0f;
    usm_params.threshold = 10.0f;

    auto one_cycle = [&](int i) {
        std::fill(px, px + 64 * 64, 500.0f);
        ASSERT_EQ(XPE_OK, xpe_noise_reduce(&img, &nr_params))     << "cycle " << i;
        ASSERT_EQ(XPE_OK, xpe_contrast_enhance(&img, &clahe_params)) << "cycle " << i;
        ASSERT_EQ(XPE_OK, xpe_edge_enhance(&img, &usm_params))    << "cycle " << i;
    };

    out = heap_growth::Measure(one_cycle, leakBytes);

    free_img(img);
}

TEST(EnhanceIntegration, NoHeapLeak_1000Iterations_AllocatingPaths) {
#ifndef _WIN32
    GTEST_SKIP() << "CRT heap walk is Windows-only in this build";
#endif
    heap_growth::Growth g;
    AllocatingPathCycles(0, g);
    GTEST_LOG_(INFO) << heap_growth::Describe(g);
    EXPECT_LT(g.heap.blocks, heap_growth::MaxBlocks(g.cycles))
        << g.cycles << " noise/contrast/edge cycles left blocks allocated";
    EXPECT_LT(g.heap.bytes, heap_growth::kMaxBytes)
        << g.cycles << " noise/contrast/edge cycles left bytes allocated";
}

// #181 (QA-B-93) control: the same cycle plus one unfreed 64-byte block per
// cycle must be caught by the measurement above.
TEST(EnhanceIntegration, NoHeapLeak_AllocatingPaths_ControlLeakIsCaught) {
#ifndef _WIN32
    GTEST_SKIP() << "CRT heap walk is Windows-only in this build";
#endif
    heap_growth::Growth g;
    AllocatingPathCycles(64, g);
    GTEST_LOG_(INFO) << "control 64 B/cycle: " << heap_growth::Describe(g);
    EXPECT_GE(g.heap.blocks, g.cycles * 9 / 10);
    EXPECT_GE(g.heap.bytes, 64LL * g.cycles * 9 / 10);
}

// REQ-ENH-CC-001: Version function returns non-null string
TEST(EnhanceIntegration, Version_ReturnsNonNull) {
    const char* ver = xpe_enhance_basic_version();
    ASSERT_NE(nullptr, ver);
    EXPECT_GT(std::strlen(ver), 0u);
}

} // namespace
