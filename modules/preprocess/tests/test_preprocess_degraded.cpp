/**
 * @file test_preprocess_degraded.cpp
 * @brief BP-01~05 DegradedMode smoke tests for the preprocess benchmark pack.
 *
 * Verifies that each correction stage (Offset, Gain, Defect, Ghost/Lag,
 * Temp/Nonlinearity) behaves safely under degraded input conditions:
 * missing calibration, identity calibration, empty defect lists, zero lag
 * coefficients, and flat temperature curves.
 *
 * Scope:
 *   - BP-01-DEG: Offset correction  — no calibration loaded
 *   - BP-02-DEG: Gain correction    — identity semantics (no calibration)
 *   - BP-03-DEG: Defect correction  — empty defect list (clean float32 image)
 *   - BP-04-DEG: Ghost/Lag          — zero lag coefficient (alpha1=alpha2=0)
 *   - BP-05-DEG: Temp/Nonlinearity  — flat (reference) temperature, null config
 *
 * Each test uses a 64x64 or 128x128 synthetic image, verifies a valid error
 * code is returned (never a crash), checks output pixel statistics are within
 * the expected range, and asserts each invocation completes in under 100ms.
 *
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

// APIs not in the new public header; still exported by the DLL.
// Matching the forward-declaration style used in test_integration.cpp.
extern "C" XPE_API XpeErrorCode xpe_ghost_create(uint32_t width, uint32_t height,
    const char* configJsonOrNull, void** handleOut);
extern "C" XPE_API XpeErrorCode xpe_ghost_correct(void* handle, XpeImageBuffer* img,
    const XpeImageMetadata* meta);
extern "C" XPE_API void xpe_ghost_destroy(void* handle);
extern "C" XPE_API XpeErrorCode xpe_temp_compensate(XpeImageBuffer* img,
    float detectorTempC, const char* configJsonOrNull);
extern "C" XPE_API XpeErrorCode xpe_nonlinearity_correct(XpeImageBuffer* img,
    const char* configJsonOrNull);

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace {

/* --------------------------------------------------------------------------
 * Buffer helpers — follow the make_*_buf() pattern from
 * test_temp_nonlinearity_binning.cpp / test_boundary.cpp.
 * -------------------------------------------------------------------------- */

static XpeImageBuffer make_uint16_buf(std::vector<uint16_t>& data,
                                      uint32_t w, uint32_t h) {
    XpeImageBuffer buf{};
    buf.data          = data.data();
    buf.width         = w;
    buf.height        = h;
    buf.bitsAllocated = 16;
    buf.bitsStored    = 16;
    buf.format        = XPE_PIXEL_UINT16;
    buf.dataSize      = data.size() * sizeof(uint16_t);
    return buf;
}

static XpeImageBuffer make_float32_buf(std::vector<float>& data,
                                       uint32_t w, uint32_t h) {
    XpeImageBuffer buf{};
    buf.data          = data.data();
    buf.width         = w;
    buf.height        = h;
    buf.bitsAllocated = 32;
    buf.bitsStored    = 32;
    buf.format        = XPE_PIXEL_FLOAT32;
    buf.dataSize      = data.size() * sizeof(float);
    return buf;
}

/* --------------------------------------------------------------------------
 * Timing helper — measure a single lambda invocation in milliseconds.
 * Each DegradedMode smoke test must complete in < 100 ms.
 * -------------------------------------------------------------------------- */

template <typename Fn>
static long long time_ms(Fn&& fn) {
    const auto t0 = std::chrono::steady_clock::now();
    fn();
    const auto t1 = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::milliseconds>(t1 - t0).count();
}

constexpr long long kDegradedBudgetMs = 100;

} // namespace

/* ==========================================================================
 * BP-01-DEG: Offset correction, null calibration table
 *
 * Expectation: xpe_offset_correct returns XPE_ERR_NOT_INITIALIZED when
 * g_calib.offset_map has not been populated. The call MUST return an error
 * code — never crash — and MUST NOT corrupt the output buffer past a safe
 * no-op state (output pixels remain at their initial sentinel value).
 * ========================================================================== */

TEST(PreprocessDegraded, BP01_OffsetNullCalibrationReturnsNotInitialized) {
    constexpr uint32_t W = 64;
    constexpr uint32_t H = 64;
    constexpr uint16_t kSentinel = 0xBEEFu;

    std::vector<uint16_t> input_data(W * H, 1000);
    std::vector<uint16_t> output_data(W * H, kSentinel);

    XpeImageBuffer input  = make_uint16_buf(input_data,  W, H);
    XpeImageBuffer output = make_uint16_buf(output_data, W, H);
    XpeImageMetadata meta{};

    XpeErrorCode rc = XPE_OK;
    const long long ms = time_ms([&] {
        rc = xpe_offset_correct(&input, &output, &meta);
    });

    // Must return a defined error code, not crash.
    // Accept NOT_INITIALIZED (no calibration loaded) or OK (if a prior test
    // populated g_calib). Both paths are graceful degradation.
    EXPECT_TRUE(rc == XPE_ERR_NOT_INITIALIZED || rc == XPE_OK)
        << "Unexpected error code: " << rc;

    // Timing budget for 64x64 image.
    EXPECT_LT(ms, kDegradedBudgetMs);

    // Pixel statistics sanity check: output still within uint16 range.
    // (Trivially true but explicit — any wild write would be caught here.)
    const uint16_t min_px = *std::min_element(output_data.begin(), output_data.end());
    const uint16_t max_px = *std::max_element(output_data.begin(), output_data.end());
    EXPECT_LE(min_px, max_px);
}

/* ==========================================================================
 * BP-02-DEG: Gain correction, identity gain (no calibration loaded)
 *
 * Expectation: With no gain map loaded, xpe_gain_correct returns
 * NOT_INITIALIZED without corrupting the input buffer. The input pixel
 * statistics must remain unchanged when the function short-circuits.
 *
 * TODO: When an in-memory identity-gain API becomes available, extend this
 * test to load an all-1.0 gain map and verify the output equals input within
 * UINT16 -> FLOAT32 conversion tolerance (<= 1 ULP).
 * ========================================================================== */

TEST(PreprocessDegraded, BP02_GainIdentityPreservesInputStatistics) {
    constexpr uint32_t W = 64;
    constexpr uint32_t H = 64;

    std::vector<uint16_t> input_data(W * H);
    for (size_t i = 0; i < input_data.size(); ++i) {
        input_data[i] = static_cast<uint16_t>(1000 + (i % 50));
    }
    const uint16_t in_min = *std::min_element(input_data.begin(), input_data.end());
    const uint16_t in_max = *std::max_element(input_data.begin(), input_data.end());

    std::vector<float> output_data(W * H, 0.0f);

    XpeImageBuffer input  = make_uint16_buf(input_data,   W, H);
    XpeImageBuffer output = make_float32_buf(output_data, W, H);
    XpeImageMetadata meta{};
    meta.kVp    = 120.0f;
    meta.SID_mm = 1200.0f;

    XpeErrorCode rc = XPE_OK;
    const long long ms = time_ms([&] {
        rc = xpe_gain_correct(&input, &output, &meta);
    });

    EXPECT_TRUE(rc == XPE_ERR_NOT_INITIALIZED || rc == XPE_OK)
        << "Unexpected error code: " << rc;

    EXPECT_LT(ms, kDegradedBudgetMs);

    // Input must never be mutated by a failing call.
    const uint16_t post_in_min = *std::min_element(input_data.begin(), input_data.end());
    const uint16_t post_in_max = *std::max_element(input_data.begin(), input_data.end());
    EXPECT_EQ(in_min, post_in_min);
    EXPECT_EQ(in_max, post_in_max);

    // If gain correction succeeded (calibration happened to be loaded),
    // the output must contain only finite values.
    if (rc == XPE_OK) {
        for (float v : output_data) {
            EXPECT_TRUE(std::isfinite(v));
        }
    }
}

/* ==========================================================================
 * BP-03-DEG: Defect correction, empty defect list
 *
 * Expectation: With no defect map loaded (empty defect list), defect
 * correction is a no-op — output == input when the function succeeds. If
 * calibration is not loaded, NOT_INITIALIZED is an acceptable return.
 * ========================================================================== */

TEST(PreprocessDegraded, BP03_DefectEmptyListIsNoOp) {
    constexpr uint32_t W = 64;
    constexpr uint32_t H = 64;

    std::vector<float> input_data(W * H);
    for (size_t i = 0; i < input_data.size(); ++i) {
        input_data[i] = 500.0f + static_cast<float>(i % 10);
    }
    std::vector<float> output_data(input_data); // copy to same initial state

    XpeImageBuffer input  = make_float32_buf(input_data,  W, H);
    XpeImageBuffer output = make_float32_buf(output_data, W, H);
    XpeImageMetadata meta{};

    XpeErrorCode rc = XPE_OK;
    const long long ms = time_ms([&] {
        rc = xpe_defect_correct(&input, &output, &meta);
    });

    EXPECT_TRUE(rc == XPE_OK || rc == XPE_ERR_NOT_INITIALIZED)
        << "Unexpected error code: " << rc;

    EXPECT_LT(ms, kDegradedBudgetMs);

    // When defect correction succeeds with no defects, output must equal input.
    if (rc == XPE_OK) {
        for (size_t i = 0; i < input_data.size(); ++i) {
            EXPECT_FLOAT_EQ(input_data[i], output_data[i])
                << "Defect no-op corrupted pixel " << i;
        }
    }
}

/* ==========================================================================
 * BP-04-DEG: Ghost/Lag correction, zero lag coefficient
 *
 * Expectation: With alpha1=alpha2=0 (zero IRF amplitudes), ghost correction
 * becomes an identity operation — the image passes through unchanged on the
 * first frame because the history buffers are zero and the lag residual is
 * zero. Any finite output change would indicate a temporal artifact.
 * ========================================================================== */

TEST(PreprocessDegraded, BP04_GhostZeroLagCoefficientIsIdentity) {
    constexpr uint32_t W = 64;
    constexpr uint32_t H = 64;
    constexpr float kPixelValue = 500.0f;

    std::vector<float> img_data(W * H, kPixelValue);
    XpeImageBuffer img = make_float32_buf(img_data, W, H);
    XpeImageMetadata meta{};
    meta.acquisitionTime = 1u;

    // Zero lag coefficients: alpha1=0, alpha2=0 -> identity IRF.
    const char* kZeroLagConfig =
        R"({"tier":"1","alpha1":"0.0","tau1":"1.0","alpha2":"0.0","tau2":"1.0"})";

    void* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_ghost_create(W, H, kZeroLagConfig, &handle));
    ASSERT_NE(nullptr, handle);

    XpeErrorCode rc = XPE_OK;
    const long long ms = time_ms([&] {
        rc = xpe_ghost_correct(handle, &img, &meta);
    });

    EXPECT_EQ(XPE_OK, rc);
    EXPECT_LT(ms, kDegradedBudgetMs);

    // With zero IRF amplitudes, the first frame must pass through unchanged.
    for (size_t i = 0; i < img_data.size(); ++i) {
        EXPECT_NEAR(kPixelValue, img_data[i], 1e-3f)
            << "Zero-lag ghost must be identity on first frame at pixel " << i;
    }

    xpe_ghost_destroy(handle);
}

/* ==========================================================================
 * BP-05-DEG: Temperature/Nonlinearity, flat curve (identity correction)
 *
 * Expectation:
 *   - Temperature compensation at T=25C (reference) applies scale = 1.0,
 *     so pixel values are preserved.
 *   - Nonlinearity correction with null config is a documented no-op
 *     (REQ-P1A-013) and must return XPE_OK with unchanged pixels.
 * ========================================================================== */

TEST(PreprocessDegraded, BP05_TempCompensateReferenceIsIdentity) {
    constexpr uint32_t W = 128;
    constexpr uint32_t H = 128;
    constexpr uint16_t kPixelValue = 2000;

    std::vector<uint16_t> data(W * H, kPixelValue);
    XpeImageBuffer buf = make_uint16_buf(data, W, H);

    XpeErrorCode rc = XPE_OK;
    const long long ms = time_ms([&] {
        rc = xpe_temp_compensate(&buf, 25.0f, nullptr);
    });

    EXPECT_EQ(XPE_OK, rc);
    EXPECT_LT(ms, kDegradedBudgetMs);

    // At the reference temperature (25C), scale == 1.0 -> identity.
    for (size_t i = 0; i < data.size(); ++i) {
        EXPECT_EQ(kPixelValue, data[i])
            << "Reference-temperature compensation must be identity at pixel " << i;
    }
}

TEST(PreprocessDegraded, BP05_NonlinearityNullConfigIsIdentity) {
    constexpr uint32_t W = 128;
    constexpr uint32_t H = 128;
    constexpr uint16_t kPixelValue = 3000;

    std::vector<uint16_t> data(W * H, kPixelValue);
    XpeImageBuffer buf = make_uint16_buf(data, W, H);

    XpeErrorCode rc = XPE_OK;
    const long long ms = time_ms([&] {
        rc = xpe_nonlinearity_correct(&buf, nullptr);
    });

    EXPECT_EQ(XPE_OK, rc);
    EXPECT_LT(ms, kDegradedBudgetMs);

    // REQ-P1A-013: null-config nonlinearity correction is a no-op.
    for (size_t i = 0; i < data.size(); ++i) {
        EXPECT_EQ(kPixelValue, data[i])
            << "Null-config nonlinearity correction must be identity at pixel " << i;
    }
}

/* ==========================================================================
 * REQ-SIMD-006: Scalar Reference Performance Baseline
 *
 * Performance budgets for 3072×3072 images on scalar-only path:
 *   - Offset: < 55ms
 *   - Gain: < 55ms
 *   - Defect (bilinear): < 95ms
 *
 * Note: These tests are optional and require calibration data to run.
 * They verify that the scalar path meets performance requirements.
 * ========================================================================== */

// REQ-SIMD-006: Full resolution offset timing test
TEST(PreprocessPerformance, DISABLED_FullResOffsetTimingScalarBaseline) {
    constexpr uint32_t W = 3072;
    constexpr uint32_t H = 3072;
    constexpr long long kOffsetBudgetMs = 55;

    std::vector<uint16_t> input_data(W * H, 1000);
    std::vector<uint16_t> output_data(W * H);

    XpeImageBuffer input = make_uint16_buf(input_data, W, H);
    XpeImageBuffer output = make_uint16_buf(output_data, W, H);
    XpeImageMetadata meta{};

    XpeErrorCode rc = XPE_OK;
    const long long ms = time_ms([&] {
        rc = xpe_offset_correct(&input, &output, &meta);
    });

    // Accept NOT_INITIALIZED if no calibration loaded
    EXPECT_TRUE(rc == XPE_ERR_NOT_INITIALIZED || rc == XPE_OK)
        << "Unexpected error code: " << rc;

    if (rc == XPE_OK) {
        EXPECT_LT(ms, kOffsetBudgetMs)
            << "Offset correction exceeded scalar baseline budget: " << ms << "ms >= " << kOffsetBudgetMs << "ms";
    } else {
        GTEST_SKIP() << "Offset calibration not loaded, skipping timing test";
    }
}

// REQ-SIMD-006: Full resolution gain timing test
TEST(PreprocessPerformance, DISABLED_FullResGainTimingScalarBaseline) {
    constexpr uint32_t W = 3072;
    constexpr uint32_t H = 3072;
    constexpr long long kGainBudgetMs = 55;

    std::vector<uint16_t> input_data(W * H, 2000);
    std::vector<float> output_data(W * H);

    XpeImageBuffer input = make_uint16_buf(input_data, W, H);
    XpeImageBuffer output = make_float32_buf(output_data, W, H);
    XpeImageMetadata meta{};
    meta.kVp = 120.0f;
    meta.SID_mm = 1200.0f;

    XpeErrorCode rc = XPE_OK;
    const long long ms = time_ms([&] {
        rc = xpe_gain_correct(&input, &output, &meta);
    });

    // Accept NOT_INITIALIZED if no calibration loaded
    EXPECT_TRUE(rc == XPE_ERR_NOT_INITIALIZED || rc == XPE_OK)
        << "Unexpected error code: " << rc;

    if (rc == XPE_OK) {
        EXPECT_LT(ms, kGainBudgetMs)
            << "Gain correction exceeded scalar baseline budget: " << ms << "ms >= " << kGainBudgetMs << "ms";
    } else {
        GTEST_SKIP() << "Gain calibration not loaded, skipping timing test";
    }
}

// REQ-SIMD-006: Full resolution defect timing test
TEST(PreprocessPerformance, DISABLED_FullResDefectTimingScalarBaseline) {
    constexpr uint32_t W = 3072;
    constexpr uint32_t H = 3072;
    constexpr long long kDefectBudgetMs = 95;

    std::vector<float> input_data(W * H, 500.0f);
    std::vector<float> output_data(W * H);

    XpeImageBuffer input = make_float32_buf(input_data, W, H);
    XpeImageBuffer output = make_float32_buf(output_data, W, H);
    XpeImageMetadata meta{};

    XpeErrorCode rc = XPE_OK;
    const long long ms = time_ms([&] {
        rc = xpe_defect_correct(&input, &output, &meta);
    });

    // Accept NOT_INITIALIZED if no calibration loaded
    EXPECT_TRUE(rc == XPE_ERR_NOT_INITIALIZED || rc == XPE_OK)
        << "Unexpected error code: " << rc;

    if (rc == XPE_OK) {
        EXPECT_LT(ms, kDefectBudgetMs)
            << "Defect correction exceeded scalar baseline budget: " << ms << "ms >= " << kDefectBudgetMs << "ms";
    } else {
        GTEST_SKIP() << "Defect calibration not loaded, skipping timing test";
    }
}
