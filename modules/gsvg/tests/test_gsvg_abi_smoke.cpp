/**
 * @file test_gsvg_abi_smoke.cpp
 * @brief R2 ABI smoke test for the GSVG module (Gate G2 blocker resolution).
 *
 * Validates the public C ABI surface of gsvg.dll on a clinically realistic
 * 3072x3072 uint16 frame. The test exercises the full lifecycle with each
 * combination of correction-step toggles to prove that the DLL boundary
 * accepts large-frame work without crashing, returns the documented error
 * codes for invalid inputs, and produces deterministic output for the
 * pass-through configuration.
 *
 * Coverage map:
 *   - REQ-GSVG-019 (3072x3072 frame processing budget)
 *   - REQ-GSVG-021 (no leaks across many init/process/shutdown cycles)
 *   - REQ-GSVG-022 (input buffer is never mutated when src and dst differ)
 *   - REQ-GSVG-024 (handle returns safely from each error path)
 *   - REQ-GSVG-026 (output values stay within 0..65535)
 *
 * SPEC: SPEC-XPE-GSVG v1.0.0 Acceptance Criterion "Readiness Level R2".
 */

#include <gtest/gtest.h>

#include "xpe/gsvg/gsvg_api.h"

#include <chrono>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr int kAbiWidth   = 3072;
constexpr int kAbiHeight  = 3072;
constexpr size_t kAbiCount =
    static_cast<size_t>(kAbiWidth) * static_cast<size_t>(kAbiHeight);

// Generous wall-clock ceiling for a debug/RelWithDebInfo CI runner. The
// performance budget (REQ-GSVG-019, 1000 ms typical) is asserted by the
// dedicated benchmark suite; here we only enforce that the smoke call
// terminates in well under the harness timeout.
constexpr int kAbiMaxMs = 8000;

/**
 * @brief Build a deterministic 3072x3072 frame with a non-uniform pattern.
 *
 * The pattern intentionally varies along both axes so that the row-mean
 * grid suppression branch cannot collapse to an all-zero deviation.
 */
std::vector<uint16_t> make_large_frame()
{
    std::vector<uint16_t> img(kAbiCount);
    // Use a small linear ramp + per-row offset. The ramp keeps values well
    // inside the uint16 range and the offset gives each row a distinct mean.
    for (int y = 0; y < kAbiHeight; ++y) {
        const uint32_t base = 4000u + static_cast<uint32_t>(y % 257);
        uint16_t* row = img.data() + static_cast<size_t>(y) * kAbiWidth;
        for (int x = 0; x < kAbiWidth; ++x) {
            const uint32_t v = base + static_cast<uint32_t>(x % 1024);
            row[x] = static_cast<uint16_t>(v);
        }
    }
    return img;
}

/**
 * @brief Build a benign vignette gain map (1.0 +/- 5%).
 *
 * Emulates a real vignette correction without forcing pixel saturation.
 * Output values therefore stay well within the uint16 range.
 */
std::vector<float> make_gain_map()
{
    std::vector<float> gain(kAbiCount);
    const float center_x = static_cast<float>(kAbiWidth)  * 0.5f;
    const float center_y = static_cast<float>(kAbiHeight) * 0.5f;
    const float radius   = std::sqrt(center_x * center_x + center_y * center_y);
    for (int y = 0; y < kAbiHeight; ++y) {
        for (int x = 0; x < kAbiWidth; ++x) {
            const float dx = static_cast<float>(x) - center_x;
            const float dy = static_cast<float>(y) - center_y;
            const float r  = std::sqrt(dx * dx + dy * dy) / radius;
            // 1.00 at the center, ~1.05 at the corners. Always >= 1.0 so the
            // vignette correction "boosts" the periphery slightly.
            gain[static_cast<size_t>(y) * kAbiWidth + x] = 1.0f + 0.05f * r;
        }
    }
    return gain;
}

} // namespace

// ---------------------------------------------------------------------------
// Lifecycle: init -> process(3072x3072) -> shutdown.
// Confirms the DLL accepts a clinically sized frame and the pass-through
// configuration produces a byte-equal copy of the source.
// ---------------------------------------------------------------------------
TEST(GsvgAbiSmoke, Lifecycle3072_PassThroughIsByteEqual)
{
    const auto src = make_large_frame();
    std::vector<uint16_t> dst(kAbiCount, 0);

    void* handle = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&handle, /*configJsonOrNull=*/nullptr), XPE_OK);
    ASSERT_NE(handle, nullptr);

    const auto start = std::chrono::steady_clock::now();
    const auto rc = xpe_gsvg_process(handle, src.data(), src.size(), dst.data(), dst.size(), kAbiWidth, kAbiHeight, nullptr, 0);
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    (void)elapsed;  // budget asserted in Lifecycle3072_PerformanceBudget (#120)

    EXPECT_EQ(rc, XPE_OK);

    // Pass-through: every pixel must be a byte-perfect copy. memcmp gives
    // O(N) coverage with a single assertion message on mismatch.
    EXPECT_EQ(std::memcmp(dst.data(),
                          src.data(),
                          kAbiCount * sizeof(uint16_t)), 0);

    EXPECT_EQ(xpe_gsvg_shutdown(handle), XPE_OK);
    RecordProperty("readiness_level", "R2");
    RecordProperty("frame_pixels", static_cast<int>(kAbiCount));
}

// ---------------------------------------------------------------------------
// Vignette + Grid combined. Asserts no crash, output stays within uint16
// range, and the source buffer is unchanged (REQ-GSVG-022).
// ---------------------------------------------------------------------------
TEST(GsvgAbiSmoke, Lifecycle3072_VignetteAndGrid_OutputClampedAndSourceIntact)
{
    const auto src = make_large_frame();
    const auto src_copy = src;     // Snapshot for post-process equality check.
    const auto gain = make_gain_map();
    std::vector<uint16_t> dst(kAbiCount, 0);

    void* handle = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&handle,
                            "{\"vignette_correction\":true,"
                            "\"grid_suppression\":true}"),
              XPE_OK);
    ASSERT_NE(handle, nullptr);

    const auto start = std::chrono::steady_clock::now();
    const auto rc = xpe_gsvg_process(handle, src.data(), src.size(), dst.data(), dst.size(), kAbiWidth, kAbiHeight, gain.data(), gain.size());
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    (void)elapsed;  // budget asserted in Lifecycle3072_PerformanceBudget (#120)

    EXPECT_EQ(rc, XPE_OK);

    // REQ-GSVG-022: the source buffer is never mutated when src != dst.
    EXPECT_EQ(std::memcmp(src.data(),
                          src_copy.data(),
                          kAbiCount * sizeof(uint16_t)), 0)
        << "Source buffer was mutated during processing.";

    // REQ-GSVG-026: every output pixel must stay inside the uint16 range.
    // The uint16_t storage type enforces the upper bound by construction.
    // We additionally verify that the pipeline does not introduce silent
    // saturation by checking that a representative population of pixels
    // remained strictly below the 65535 ceiling and strictly above the
    // 0 floor (the input data has no zero/65535 pixels, and gain factors
    // stay in [1.00, 1.05], so saturation would indicate corruption).
    int saturated_high = 0;
    int saturated_low  = 0;
    constexpr int kSampleStride = 137;  // Co-prime with the row pattern.
    int sampled = 0;
    for (size_t i = 0; i < kAbiCount; i += kSampleStride) {
        ++sampled;
        if (dst[i] == 0)     ++saturated_low;
        if (dst[i] == 65535) ++saturated_high;
    }
    EXPECT_EQ(saturated_low,  0) << "unexpected zero pixels in vignette+grid output.";
    EXPECT_EQ(saturated_high, 0) << "unexpected 65535 pixels in vignette+grid output.";
    EXPECT_GT(sampled, 0);

    EXPECT_EQ(xpe_gsvg_shutdown(handle), XPE_OK);
    RecordProperty("readiness_level", "R2");
}

// ---------------------------------------------------------------------------
// Error path: NULL handle on process. A NULL required pointer is
// INVALID_INPUT, matching dicom and the api-spec precedence contract (#119).
// ---------------------------------------------------------------------------
TEST(GsvgAbiSmoke, ProcessRejectsNullHandle)
{
    const std::vector<uint16_t> src(16, 1000);
    std::vector<uint16_t> dst(16, 0);
    EXPECT_EQ(xpe_gsvg_process(/*handle=*/nullptr, src.data(), src.size(), dst.data(), dst.size(), 4, 4, nullptr, 0),
              XPE_ERR_INVALID_INPUT);
}

// ---------------------------------------------------------------------------
// Error path: NULL output pointer on init.
// ---------------------------------------------------------------------------
TEST(GsvgAbiSmoke, InitRejectsNullHandlePointer)
{
    EXPECT_EQ(xpe_gsvg_init(/*handleOut=*/nullptr, "{}"), XPE_ERR_INVALID_INPUT);
}

// ---------------------------------------------------------------------------
// Lifecycle stress: 32 cycles of init + small process + shutdown.
// REQ-GSVG-021 requires 100 frames in batch mode without leaks; here we
// run a smaller loop on the smoke path and rely on the build's ASan/leak
// sanitiser (when enabled) to catch handle-level allocations that escape.
// ---------------------------------------------------------------------------
TEST(GsvgAbiSmoke, RepeatedLifecycleDoesNotLeakOrCrash)
{
    constexpr int kCycles = 32;
    constexpr int kSmallW = 256;
    constexpr int kSmallH = 256;
    constexpr size_t kSmallN = kSmallW * kSmallH;

    const std::vector<uint16_t> src(kSmallN, 12000);

    for (int i = 0; i < kCycles; ++i) {
        std::vector<uint16_t> dst(kSmallN, 0);

        void* handle = nullptr;
        ASSERT_EQ(xpe_gsvg_init(&handle, /*configJsonOrNull=*/nullptr), XPE_OK)
            << "init failed on cycle " << i;
        ASSERT_NE(handle, nullptr);

        ASSERT_EQ(xpe_gsvg_process(handle, src.data(), src.size(), dst.data(), dst.size(), kSmallW, kSmallH, nullptr, 0),
                  XPE_OK) << "process failed on cycle " << i;

        ASSERT_EQ(xpe_gsvg_shutdown(handle), XPE_OK)
            << "shutdown failed on cycle " << i;
    }

    RecordProperty("cycles", kCycles);
    RecordProperty("requirement", "REQ-GSVG-021");
}

// ---------------------------------------------------------------------------
// Version probe contract. Confirms the version string is non-empty and
// obeys SemVer-like formatting (digit, dot, digit). Provides a minimal
// guard against accidental empty-string regressions.
// ---------------------------------------------------------------------------
TEST(GsvgAbiSmoke, VersionStringLooksLikeSemver)
{
    const char* v = xpe_gsvg_version();
    ASSERT_NE(v, nullptr);
    const std::string s(v);
    ASSERT_FALSE(s.empty());
    ASSERT_NE(s.find('.'), std::string::npos)
        << "version string '" << s << "' missing a dot separator.";
}

// ---------------------------------------------------------------------------
// Time budget for both 3072x3072 lifecycle paths, separated from the functional
// cases above (#120). Mixing the two meant a slow build erased the byte-equality
// and clamping assertions, which are the part that says the module is correct.
// ---------------------------------------------------------------------------
TEST(GsvgAbiSmoke, Lifecycle3072_PerformanceBudget)
{
    const auto src = make_large_frame();
    std::vector<uint16_t> dst(kAbiCount, 0);

    {   // pass-through (no config)
        void* handle = nullptr;
        ASSERT_EQ(xpe_gsvg_init(&handle, /*configJsonOrNull=*/nullptr), XPE_OK);
        const auto start = std::chrono::steady_clock::now();
        ASSERT_EQ(xpe_gsvg_process(handle, src.data(), src.size(), dst.data(), dst.size(), kAbiWidth, kAbiHeight, nullptr, 0),
                  XPE_OK);
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        EXPECT_LT(elapsed.count(), kAbiMaxMs)
            << "ABI smoke pass-through exceeded " << kAbiMaxMs << " ms ceiling.";
        EXPECT_EQ(xpe_gsvg_shutdown(handle), XPE_OK);
    }

    {   // vignette + grid
        const auto gain = make_gain_map();
        void* handle = nullptr;
        ASSERT_EQ(xpe_gsvg_init(&handle,
                                "{\"vignette_correction\":true,"
                                "\"grid_suppression\":true}"),
                  XPE_OK);
        const auto start = std::chrono::steady_clock::now();
        ASSERT_EQ(xpe_gsvg_process(handle, src.data(), src.size(), dst.data(), dst.size(), kAbiWidth, kAbiHeight, gain.data(), gain.size()),
                  XPE_OK);
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        EXPECT_LT(elapsed.count(), kAbiMaxMs)
            << "ABI smoke vignette+grid exceeded " << kAbiMaxMs << " ms ceiling.";
        EXPECT_EQ(xpe_gsvg_shutdown(handle), XPE_OK);
    }
}

// ---------------------------------------------------------------------------
// #105 G3: working-set measurement, mirroring enhance_basic
// test_enhance_integration.cpp (92bcf17) and preprocess T-010. Duplicated per
// module rather than exported: xpe_common's surface is fixed at 16 symbols
// (REQ-P0-008).
// ---------------------------------------------------------------------------
#ifdef _WIN32
#  include <windows.h>
#  include <psapi.h>
static SIZE_T get_working_set_bytes() {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
}
#else
static size_t get_working_set_bytes() { return 0; }
#endif

// WARMUP exists because the first cycles fault in fresh heap pages and grow the
// CRT allocator arena; counting that one-time cost as "leak" would make the
// threshold a measure of startup, not of retention. The baseline is snapshotted
// after warm-up so only steady-state growth is scored.
constexpr int    ENDURANCE_CYCLES = 1000;
constexpr int    ENDURANCE_WARMUP = 100;
constexpr size_t ENDURANCE_ONE_MB = 1024u * 1024u;

// ---------------------------------------------------------------------------
// #105 G3 (QA-B-23): heap growth over 1000 handle-lifecycle cycles.
//
// RepeatedLifecycleDoesNotLeakOrCrash above runs 32 cycles and measures
// nothing -- it proves the module survives, not that it releases. This case
// takes the same init/process/shutdown cycle, runs it 1000 times, and scores
// the working-set delta against the 1 MB bound used by preprocess T-010,
// enhance_basic and the four modules of QA-B-22.
//
// Size: 512x512, not the 3072x3072 of the REQ-GSVG-019 budget case. The gate
// measures retention per lifecycle, which does not depend on frame size, and
// 1100 cycles at 3072x3072 would dominate the suite's wall time. Recorded here
// rather than left implicit.
//
// Both processing stages are switched ON deliberately. With a NULL config and a
// NULL gain map, xpe_gsvg_process reduces to a memcpy (gsvg.cpp:218-228:
// vignette_correction and grid_suppression both default to false), so the cycle
// would have covered the handle lifecycle and nothing else. The config below
// plus a real gain map puts apply_vignette_scalar and suppress_grid_row_mean
// inside the measured loop.
// ---------------------------------------------------------------------------
TEST(GsvgEndurance, ThousandCycles_MemoryGrowthUnderOneMB)
{
#ifndef _WIN32
    GTEST_SKIP() << "Working-set measurement is Windows-only in this build";
#endif
    constexpr int    kW = 512;
    constexpr int    kH = 512;
    constexpr size_t kN = static_cast<size_t>(kW) * kH;

    const char* kConfig =
        R"({"vignette_correction": true, "grid_suppression": true})";

    const std::vector<uint16_t> src(kN, 12000);
    std::vector<uint16_t>       dst(kN, 0);
    const std::vector<float>    gain(kN, 1.05f);

    auto one_cycle = [&](int i) {
        void* handle = nullptr;
        ASSERT_EQ(xpe_gsvg_init(&handle, kConfig), XPE_OK)
            << "init failed on cycle " << i;
        ASSERT_NE(handle, nullptr);

        ASSERT_EQ(xpe_gsvg_process(handle, src.data(), src.size(), dst.data(), dst.size(), kW, kH, gain.data(), gain.size()), XPE_OK)
            << "process failed on cycle " << i;

        ASSERT_EQ(xpe_gsvg_shutdown(handle), XPE_OK)
            << "shutdown failed on cycle " << i;
    };

    for (int i = 0; i < ENDURANCE_WARMUP; ++i) one_cycle(i);

    const auto before = get_working_set_bytes();
    for (int i = 0; i < ENDURANCE_CYCLES; ++i) one_cycle(i);
    const auto after = get_working_set_bytes();

    if (after > before) {
        EXPECT_LT(after - before, ENDURANCE_ONE_MB)
            << "Working set grew by " << (after - before) / 1024 << " KB over "
            << ENDURANCE_CYCLES << " gsvg init/process/shutdown cycles";
    }

    RecordProperty("cycles", ENDURANCE_CYCLES);
    RecordProperty("requirement", "REQ-GSVG-021");
}
