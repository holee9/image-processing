/**
 * @file test_xpe_preprocess_memleak.cpp
 * @brief Memory endurance test for XPE Preprocessing Module (Gate G1a -> G1b)
 *
 * Gate G1a -> G1b: "Memory leak test: 1000 frames without growth"
 *
 * Test case:
 *   XpePreprocessEndurance.NoMemoryLeakAfter1000Frames
 *     - Allocate a 512x512 uint16 frame buffer once (outside the loop).
 *     - Call xpe_preprocess_init / (lightweight process) / xpe_preprocess_shutdown
 *       for 1000 iterations, reusing the same frame buffer every iteration.
 *     - Measure process-private heap growth via the Windows Process Memory API
 *       (GetProcessMemoryInfo).  Assert heap does not grow beyond 5% of the
 *       post-warmup baseline (with a 2 MB absolute floor to absorb OS
 *       commit-granularity noise).
 *
 * Design notes:
 *   - We avoid xpe_gain_correct in the inner loop because that call performs an
 *     ownership transfer (allocates a new float32 buffer and stores it into
 *     img->data; caller owns the new buffer).  Using it would force the test
 *     to also exercise the caller-free contract, which is a separate concern
 *     from "does the module leak across init/shutdown cycles?".
 *   - The process path uses readout validate + temperature compensation + in-place
 *     offset correction.  All three are documented as non-allocating and
 *     operate on the caller-owned uint16 buffer in-place.
 *   - A 50-iteration warm-up is executed first so that any one-shot allocations
 *     inside the module (logger buffers, config JSON parse arenas, DLL lazy
 *     initialisation, etc.) have already occurred before the baseline is
 *     captured.
 *
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 * REQ-P1A-031 (RAII, no leaks after shutdown)
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

// Not in new public header; still exported by DLL
extern "C" XPE_API XpeErrorCode xpe_temp_compensate(XpeImageBuffer* img,
    float detectorTempC, const char* configJsonOrNull);

#include <vector>
#include <cstdint>
#include <cstring>
#include <cstdio>

#ifdef _WIN32
#  include <windows.h>
#  include <psapi.h>
#endif

namespace {

constexpr uint32_t W = 512;
constexpr uint32_t H = 512;
constexpr int      WARMUP_ITERATIONS = 50;
constexpr int      MEASURED_ITERATIONS = 1000;
constexpr double   ALLOWED_GROWTH_RATIO = 0.05;                 /* 5%     */
constexpr size_t   ABSOLUTE_FLOOR_BYTES = 2 * 1024 * 1024;      /* 2 MB   */

struct ProcessMemorySample {
    size_t workingSet;     /* RSS-like: pages resident in physical RAM         */
    size_t privateUsage;   /* Committed private bytes (heap + stacks + private */
                           /* mappings).  This is the leak-indicator of       */
                           /* interest -- WorkingSet can shrink when the OS   */
                           /* trims pages, but PrivateUsage only grows on leak */
};

static ProcessMemorySample sample_process_memory() {
    ProcessMemorySample s{0, 0};
#ifdef _WIN32
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    pmc.cb = sizeof(pmc);
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                             sizeof(pmc))) {
        s.workingSet   = static_cast<size_t>(pmc.WorkingSetSize);
        s.privateUsage = static_cast<size_t>(pmc.PrivateUsage);
    }
#endif
    return s;
}

/*
 * Run one "frame" through the module:
 *   init -> readout validate -> temp compensate -> offset correct -> shutdown
 *
 * All three processing calls operate in-place on the caller-owned uint16 buffer
 * (no allocation / no ownership transfer).  The frame contents are clobbered
 * by the inner calls, so the test restores them before every iteration.
 */
static void run_one_frame(XpeImageBuffer& rawBuf,
                          XpeImageBuffer& offsetBuf,
                          const uint16_t* goldenRaw,
                          size_t           rawElemCount) {
    /* Restore a clean input frame so every iteration does the same work */
    std::memcpy(rawBuf.data, goldenRaw, rawElemCount * sizeof(uint16_t));

    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));

    XpeImageMetadata meta{};
    bool dropped = false, nonuniform = false;
    EXPECT_EQ(XPE_OK, xpe_validate_readout_artifact(
        &rawBuf, &meta, &dropped, &nonuniform));

    EXPECT_EQ(XPE_OK, xpe_temp_compensate(&rawBuf, 25.0f, nullptr));

    // Accepts NOT_INITIALIZED when calibration not loaded (memleak test, not functional test)
    auto rc = xpe_offset_correct(&rawBuf, &offsetBuf, &meta);
    EXPECT_TRUE(rc == XPE_OK || rc == XPE_ERR_NOT_INITIALIZED);

    xpe_preprocess_shutdown();
}

} /* anonymous namespace */

/* =========================================================================
 * Gate G1a -> G1b : memory endurance
 * ========================================================================= */
TEST(XpePreprocessEndurance, NoMemoryLeakAfter1000Frames) {
#ifndef _WIN32
    GTEST_SKIP() << "Process memory measurement is Windows-specific in this build";
#endif

    /* Allocate the 512x512 uint16 frame buffer + matching offset map ONCE. */
    std::vector<uint16_t> rawPixels(W * H, 2000);
    std::vector<uint16_t> offsetPixels(W * H, 200);
    std::vector<uint16_t> goldenRaw = rawPixels; /* immutable reference copy */

    XpeImageBuffer rawBuf{};
    rawBuf.data          = rawPixels.data();
    rawBuf.width         = W;
    rawBuf.height        = H;
    rawBuf.bitsAllocated = 16;
    rawBuf.bitsStored    = 16;
    rawBuf.format        = XPE_PIXEL_UINT16;
    rawBuf.dataSize      = rawPixels.size() * sizeof(uint16_t);

    XpeImageBuffer offsetBuf{};
    offsetBuf.data          = offsetPixels.data();
    offsetBuf.width         = W;
    offsetBuf.height        = H;
    offsetBuf.bitsAllocated = 16;
    offsetBuf.bitsStored    = 16;
    offsetBuf.format        = XPE_PIXEL_UINT16;
    offsetBuf.dataSize      = offsetPixels.size() * sizeof(uint16_t);

    /* -----------------------------------------------------------------
     * Warm-up: absorb one-shot module allocations (logger ring buffers,
     * JSON parse arenas, DLL lazy initialisation, etc.)
     * ----------------------------------------------------------------- */
    for (int i = 0; i < WARMUP_ITERATIONS; ++i) {
        ASSERT_NO_FATAL_FAILURE(run_one_frame(
            rawBuf, offsetBuf, goldenRaw.data(), goldenRaw.size()));
    }

    const ProcessMemorySample before = sample_process_memory();
    ASSERT_GT(before.privateUsage, 0u)
        << "PrivateUsage sampling failed -- GetProcessMemoryInfo returned 0";

    /* -----------------------------------------------------------------
     * Measured phase: 1000 iterations
     * ----------------------------------------------------------------- */
    for (int i = 0; i < MEASURED_ITERATIONS; ++i) {
        ASSERT_NO_FATAL_FAILURE(run_one_frame(
            rawBuf, offsetBuf, goldenRaw.data(), goldenRaw.size()))
            << "frame iteration " << i;
    }

    const ProcessMemorySample after = sample_process_memory();

    /* Growth tolerance: max(5% * baseline, 2 MB floor).
     * The 2 MB floor absorbs OS-level commit granularity noise
     * (Windows commits in 4 KB pages, VirtualAlloc reserves in 64 KB chunks,
     * and the CRT heap grows in multi-page segments).
     */
    const size_t ratioBudget = static_cast<size_t>(
        static_cast<double>(before.privateUsage) * ALLOWED_GROWTH_RATIO);
    const size_t budget = (ratioBudget > ABSOLUTE_FLOOR_BYTES)
                              ? ratioBudget
                              : ABSOLUTE_FLOOR_BYTES;

    const size_t privateDelta = (after.privateUsage > before.privateUsage)
        ? (after.privateUsage - before.privateUsage) : 0u;

    std::fprintf(stderr,
        "[XpePreprocessEndurance] baseline PrivateUsage = %zu KB, "
        "after %d frames = %zu KB, delta = %zu KB "
        "(budget %zu KB = max(5%%, 2048 KB))\n",
        before.privateUsage / 1024,
        MEASURED_ITERATIONS,
        after.privateUsage / 1024,
        privateDelta / 1024,
        budget / 1024);

    EXPECT_LE(privateDelta, budget)
        << "Process PrivateUsage grew by " << (privateDelta / 1024)
        << " KB over " << MEASURED_ITERATIONS
        << " init/process/shutdown cycles "
        << "(baseline " << (before.privateUsage / 1024)
        << " KB, budget " << (budget / 1024) << " KB)";

    /* WorkingSet is reported for diagnostic context only -- it may legitimately
     * shrink under memory pressure.  We do not assert on it. */
    std::fprintf(stderr,
        "[XpePreprocessEndurance] WorkingSet before = %zu KB, after = %zu KB\n",
        before.workingSet / 1024, after.workingSet / 1024);
}
