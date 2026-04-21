/**
 * @file test_gsvg_degraded.cpp
 * @brief DegradedMode GTest for GSVG — BP-06..BP-09 coverage.
 *
 * Verifies that GSVG correctly enters a pass-through / identity-like mode
 * under degraded inputs:
 *   - BP-07-DEG: null vignette map passed to process → identity output.
 *   - BP-08-DEG: all-ones vignette map → output == input.
 *   - BP-09-DEG: grid suppression disabled, vignette only → output matches
 *                a standalone vignette application with no grid change.
 *
 * Each test uses a small 64x64 synthetic image and enforces a 50 ms wall
 * clock upper bound per process call.
 */

#include <gtest/gtest.h>

#include "xpe/gsvg/gsvg_api.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

constexpr int kWidth  = 64;
constexpr int kHeight = 64;
constexpr int kCount  = kWidth * kHeight;
constexpr int kMaxMs  = 50;

/**
 * @brief Create a deterministic synthetic image that is NOT uniform.
 *
 * Row y and column x contribute so that row means differ (worst case for
 * the row-mean-deviation grid suppression baseline). Values stay well within
 * the uint16 range for safe arithmetic checks.
 */
std::vector<uint16_t> make_synthetic_image()
{
    std::vector<uint16_t> img(kCount);
    for (int y = 0; y < kHeight; ++y) {
        for (int x = 0; x < kWidth; ++x) {
            // Deterministic pattern: 1000 + 10*y + x. Row means will differ.
            const int v = 1000 + 10 * y + x;
            img[static_cast<size_t>(y) * kWidth + x] = static_cast<uint16_t>(v);
        }
    }
    return img;
}

} // namespace

// ---------------------------------------------------------------------------
// BP-07-DEG: null vignette map → identity output.
// When vignette_correction is enabled but gainMap is NULL, the vignette step
// becomes a no-op and the output equals the input.
// ---------------------------------------------------------------------------
TEST(GsvgDegradedMode, BP07_NullVignetteMap_IdentityOutput)
{
    const auto src = make_synthetic_image();
    std::vector<uint16_t> dst(kCount, 0xBEEF); // Sentinel to prove dst was written.

    void* handle = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&handle, "{\"vignette_correction\":true,\"grid_suppression\":false}"), XPE_OK);
    ASSERT_NE(handle, nullptr);

    const auto start = std::chrono::steady_clock::now();
    const auto rc = xpe_gsvg_process(handle,
                                     src.data(), dst.data(),
                                     kWidth, kHeight,
                                     /*gainMap=*/nullptr);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    EXPECT_EQ(rc, XPE_OK);
    EXPECT_LT(elapsed.count(), kMaxMs)
        << "BP-07-DEG exceeded " << kMaxMs << " ms wall clock budget.";

    // Identity: dst byte-equal to src.
    EXPECT_EQ(std::memcmp(dst.data(), src.data(), kCount * sizeof(uint16_t)), 0);

    // Sanity: no pixel was left unwritten (sentinel gone).
    for (int i = 0; i < kCount; ++i) {
        EXPECT_EQ(dst[i], src[i]) << "mismatch at pixel " << i;
        if (dst[i] != src[i]) break;
    }

    EXPECT_EQ(xpe_gsvg_shutdown(handle), XPE_OK);
    RecordProperty("BP", "BP-07-DEG");
    RecordProperty("max_ms", kMaxMs);
}

// ---------------------------------------------------------------------------
// BP-08-DEG: all-ones vignette map → output == input.
// A gain map of exactly 1.0 everywhere is the algebraic identity for the
// vignette step; output must equal input to the uint16.
// ---------------------------------------------------------------------------
TEST(GsvgDegradedMode, BP08_AllOnesVignetteMap_OutputEqualsInput)
{
    const auto src = make_synthetic_image();
    std::vector<uint16_t> dst(kCount, 0);
    const std::vector<float> gain(kCount, 1.0f);

    void* handle = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&handle, "{\"vignette_correction\":true,\"grid_suppression\":false}"), XPE_OK);
    ASSERT_NE(handle, nullptr);

    const auto start = std::chrono::steady_clock::now();
    const auto rc = xpe_gsvg_process(handle,
                                     src.data(), dst.data(),
                                     kWidth, kHeight,
                                     gain.data());
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    EXPECT_EQ(rc, XPE_OK);
    EXPECT_LT(elapsed.count(), kMaxMs)
        << "BP-08-DEG exceeded " << kMaxMs << " ms wall clock budget.";

    // All pixels must stay inside the uint16 range (they were by construction,
    // and gain 1.0 preserves that).
    for (int i = 0; i < kCount; ++i) {
        EXPECT_LE(dst[i], 65535);
    }

    // Strict equality: gain 1.0 is the multiplicative identity.
    EXPECT_EQ(std::memcmp(dst.data(), src.data(), kCount * sizeof(uint16_t)), 0);

    EXPECT_EQ(xpe_gsvg_shutdown(handle), XPE_OK);
    RecordProperty("BP", "BP-08-DEG");
    RecordProperty("max_ms", kMaxMs);
}

// ---------------------------------------------------------------------------
// BP-09-DEG: grid suppression disabled → output reflects vignette only.
// With grid_suppression=false, applying a uniform gain of 2.0 must exactly
// double each source pixel (clamped to 65535). No row-mean correction must
// be applied regardless of the input row-mean structure.
// ---------------------------------------------------------------------------
TEST(GsvgDegradedMode, BP09_GridDisabled_VignetteOnly)
{
    const auto src = make_synthetic_image();
    std::vector<uint16_t> dst(kCount, 0);
    const std::vector<float> gain(kCount, 2.0f);

    void* handle = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&handle, "{\"vignette_correction\":true,\"grid_suppression\":false}"), XPE_OK);
    ASSERT_NE(handle, nullptr);

    const auto start = std::chrono::steady_clock::now();
    const auto rc = xpe_gsvg_process(handle,
                                     src.data(), dst.data(),
                                     kWidth, kHeight,
                                     gain.data());
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    EXPECT_EQ(rc, XPE_OK);
    EXPECT_LT(elapsed.count(), kMaxMs)
        << "BP-09-DEG exceeded " << kMaxMs << " ms wall clock budget.";

    // Vignette-only expectation: dst[i] == clamp(src[i] * 2, 0..65535).
    // Synthetic pixels top out at 1000 + 10*63 + 63 = 1693 so 2x fits.
    for (int i = 0; i < kCount; ++i) {
        const int expected = static_cast<int>(src[i]) * 2;
        const int clamped  = expected > 65535 ? 65535 : expected;
        EXPECT_EQ(dst[i], static_cast<uint16_t>(clamped))
            << "mismatch at pixel " << i;
    }

    EXPECT_EQ(xpe_gsvg_shutdown(handle), XPE_OK);
    RecordProperty("BP", "BP-09-DEG");
    RecordProperty("max_ms", kMaxMs);
}

// ---------------------------------------------------------------------------
// Additional lifecycle / defensive input coverage. These do not have a BP ID
// but guard the ABI surface that the benchmark cases depend on.
// ---------------------------------------------------------------------------
TEST(GsvgDegradedMode, InitWithNullConfig_DefaultsToPassThrough)
{
    const auto src = make_synthetic_image();
    std::vector<uint16_t> dst(kCount, 0);

    void* handle = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&handle, nullptr), XPE_OK);
    ASSERT_NE(handle, nullptr);

    ASSERT_EQ(xpe_gsvg_process(handle, src.data(), dst.data(),
                               kWidth, kHeight, /*gainMap=*/nullptr),
              XPE_OK);

    // Defaults should be full pass-through (both steps OFF).
    EXPECT_EQ(std::memcmp(dst.data(), src.data(), kCount * sizeof(uint16_t)), 0);

    EXPECT_EQ(xpe_gsvg_shutdown(handle), XPE_OK);
}

TEST(GsvgDegradedMode, ProcessWithNullHandle_ReturnsNotInitialized)
{
    const auto src = make_synthetic_image();
    std::vector<uint16_t> dst(kCount, 0);
    EXPECT_EQ(xpe_gsvg_process(nullptr, src.data(), dst.data(),
                               kWidth, kHeight, nullptr),
              XPE_ERR_NOT_INITIALIZED);
}

TEST(GsvgDegradedMode, ShutdownNullHandle_NoOp)
{
    EXPECT_EQ(xpe_gsvg_shutdown(nullptr), XPE_OK);
}
