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

#include <algorithm>
#include <chrono>
#include <cmath>
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
 * The pattern intentionally varies along both axes: a 257-row sawtooth and a
 * 1024-column ramp. (Written for the row-mean grid suppression that #180
 * replaced; the DWT path treats the sawtooth as a periodic signal.)
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
// #105 G3 / #180 (QA-B-92): retention over 1000 handle-lifecycle cycles.
//
// Measured on the CRT heap, not on the working set. Until QA-B-92 this case
// scored the working-set delta against 1 MB (mirroring enhance_basic and
// preprocess T-010). QA-B-92 showed that number is not a leak measure:
//   - CRT heap growth over 250 / 1000 / 4000 grid-path cycles: 0 / 0 / 0 bytes
//   - working-set growth over the same runs: 86 KB / 0 / 8 KB, and 2.43 and
//     2.47 MB in 2 of 20 local repeats (CI 63af26c: 2.59 MB)
//   - a deliberate 64-byte leak per cycle: heap +250 / +994 / +4000 blocks,
//     working set -41 KB / +2.47 MB -- it does not follow the leak at all.
// The working set is what the OS has paged in and what the allocator keeps;
// the heap walk counts the blocks still allocated, which is what a leak is.
//
// With UCRT (/MD) gsvg.dll and this test allocate from the same CRT heap, so
// _heapwalk sees the module's allocations. The two ControlLeak cases below
// inject a small and a large unfreed allocation per cycle and require the
// same measurement to report them; without that, a zero here would mean
// nothing.
//
// Size: 256x256 (512x512 until QA-B-90, when the grid path made 1100 cycles
// take 22.6 s locally), not the 3072x3072 of the REQ-GSVG-019 budget case.
// Retention per lifecycle does not depend on frame size.
//
// Rows alternate 12000 / 12100 (#180, QA-B-90): a flat source has no grid, so
// the DWT path would never allocate its wavelet levels.
// ---------------------------------------------------------------------------
#ifdef _WIN32
#  include <windows.h>
#  include <psapi.h>
#  include <malloc.h>

namespace {

struct HeapUse { long long bytes = 0; long long blocks = 0; };

// Blocks currently allocated on the CRT heap.
HeapUse crt_heap_use() {
    _HEAPINFO hi{};
    hi._pentry = nullptr;
    HeapUse u;
    while (_heapwalk(&hi) == _HEAPOK) {
        if (hi._useflag == _USEDENTRY) {
            u.bytes += static_cast<long long>(hi._size);
            ++u.blocks;
        }
    }
    return u;
}

long long working_set_bytes() {
    PROCESS_MEMORY_COUNTERS pmc;
    return GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))
        ? static_cast<long long>(pmc.WorkingSetSize) : 0;
}

constexpr int       ENDURANCE_CYCLES = 1000;
constexpr int       ENDURANCE_WARMUP = 100;
// Retention bound over ENDURANCE_CYCLES: fewer than one leaked block in ten
// cycles and under 16 KB in total. The 64-byte-per-cycle control (1000 blocks,
// 64 KB) is well outside both; the measured product path is 0 and 0.
constexpr long long ENDURANCE_MAX_BLOCKS = ENDURANCE_CYCLES / 10;
constexpr long long ENDURANCE_MAX_BYTES  = 16 * 1024;

struct Growth { HeapUse heap; long long workingSet = 0; std::vector<uint16_t> dst; };

// leakBytes > 0: allocate that many bytes every leakEvery-th cycle and keep
// them until the measurement is taken (the control).
Growth measure_growth(size_t leakBytes, int leakEvery) {
    constexpr int    kW = 256;
    constexpr int    kH = 256;
    constexpr size_t kN = static_cast<size_t>(kW) * kH;

    const char* kConfig =
        R"({"vignette_correction": true, "grid_suppression": true})";

    std::vector<uint16_t> src(kN, 12000);
    for (int y = 1; y < kH; y += 2)
        std::fill_n(src.begin() + static_cast<std::ptrdiff_t>(y) * kW, kW, uint16_t{12100});
    Growth g;
    g.dst.assign(kN, 0);
    const std::vector<float> gain(kN, 1.05f);
    std::vector<void*> held;
    held.reserve(static_cast<size_t>(ENDURANCE_CYCLES));

    auto one_cycle = [&](int i, bool measured) {
        void* handle = nullptr;
        ASSERT_EQ(xpe_gsvg_init(&handle, kConfig), XPE_OK) << "init failed on cycle " << i;
        ASSERT_NE(handle, nullptr);
        ASSERT_EQ(xpe_gsvg_process(handle, src.data(), src.size(), g.dst.data(), g.dst.size(),
                                   kW, kH, gain.data(), gain.size()), XPE_OK)
            << "process failed on cycle " << i;
        ASSERT_EQ(xpe_gsvg_shutdown(handle), XPE_OK) << "shutdown failed on cycle " << i;
        if (measured && leakBytes > 0 && i % leakEvery == 0) held.push_back(std::malloc(leakBytes));
    };

    // WARMUP: the first cycles fault in fresh pages and grow the allocator's
    // arena; the baseline is taken after them.
    for (int i = 0; i < ENDURANCE_WARMUP; ++i) one_cycle(i, false);
    const HeapUse h0 = crt_heap_use();
    const long long w0 = working_set_bytes();
    for (int i = 0; i < ENDURANCE_CYCLES; ++i) one_cycle(i, true);
    const HeapUse h1 = crt_heap_use();
    g.workingSet = working_set_bytes() - w0;
    g.heap = {h1.bytes - h0.bytes, h1.blocks - h0.blocks};
    for (void* p : held) std::free(p);
    return g;
}

}  // namespace

TEST(GsvgEndurance, ThousandCycles_CrtHeapDoesNotGrow)
{
    const Growth g = measure_growth(0, 1);
    GTEST_LOG_(INFO) << "heap growth " << g.heap.bytes << " bytes / " << g.heap.blocks
                     << " blocks, working set " << g.workingSet << " bytes (not asserted)";
    EXPECT_LT(g.heap.blocks, ENDURANCE_MAX_BLOCKS)
        << ENDURANCE_CYCLES << " gsvg init/process/shutdown cycles left blocks allocated";
    EXPECT_LT(g.heap.bytes, ENDURANCE_MAX_BYTES)
        << ENDURANCE_CYCLES << " gsvg init/process/shutdown cycles left bytes allocated";

    // The loop did run the suppression: the 100 * 1.05 row alternation is gone.
    double row0 = 0.0, row1 = 0.0;
    for (int x = 0; x < 256; ++x) {
        row0 += g.dst[static_cast<size_t>(x)];
        row1 += g.dst[static_cast<size_t>(256 + x)];
    }
    EXPECT_LT(std::fabs(row1 - row0) / 256.0, 10.0)
        << "row alternation survived: the grid path was not exercised";

    RecordProperty("cycles", ENDURANCE_CYCLES);
    RecordProperty("requirement", "REQ-GSVG-021");
}

// Control: one 64-byte block per cycle is reported, and outside the bound.
TEST(GsvgEndurance, ControlLeak_SmallBlockPerCycleIsCaught)
{
    const Growth g = measure_growth(64, 1);
    GTEST_LOG_(INFO) << "control 64 B/cycle: heap " << g.heap.bytes << " bytes / "
                     << g.heap.blocks << " blocks, working set " << g.workingSet;
    EXPECT_GE(g.heap.blocks, ENDURANCE_MAX_BLOCKS);
    EXPECT_GE(g.heap.bytes, ENDURANCE_MAX_BYTES);
}

// Control: a large block (above the heap's direct-allocation size) every 100
// cycles is reported too, so a leaked image buffer would not slip past.
TEST(GsvgEndurance, ControlLeak_LargeBlockIsCaught)
{
    const Growth g = measure_growth(size_t{1} << 20, 100);
    GTEST_LOG_(INFO) << "control 1 MB/100 cycles: heap " << g.heap.bytes << " bytes / "
                     << g.heap.blocks << " blocks, working set " << g.workingSet;
    // 10 MB held; the net delta can be a few KB short when other blocks are
    // freed in the same window (QA-B-92: 10,482,616 B in 1 of 20 runs).
    EXPECT_GE(g.heap.bytes, 9LL << 20);
}

// Control: the walk sees gsvg.dll's own allocations. The two cases above leak
// from the test side; if the DLL used a separate heap (/MT), they would still
// pass while the product case reported a blind 0. xpe_gsvg_init allocates the
// handle with new inside the DLL, so a live handle must show up here.
TEST(GsvgEndurance, ControlDllHandleIsVisibleToHeapWalk)
{
    const HeapUse before = crt_heap_use();
    void* handle = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&handle, nullptr), XPE_OK);
    const HeapUse alive = crt_heap_use();
    ASSERT_EQ(xpe_gsvg_shutdown(handle), XPE_OK);
    const HeapUse after = crt_heap_use();
    GTEST_LOG_(INFO) << "live handle: +" << (alive.blocks - before.blocks) << " blocks / +"
                     << (alive.bytes - before.bytes) << " bytes; after shutdown: "
                     << (after.blocks - before.blocks) << " / " << (after.bytes - before.bytes);
    EXPECT_GE(alive.blocks - before.blocks, 1);
    EXPECT_GT(alive.bytes - before.bytes, 0);
    EXPECT_EQ(after.blocks - before.blocks, 0);
}

// #180 (QA-B-96): the exported C ABI of gsvg.dll, by name. The new entry point
// is added, nothing is removed.
TEST(GsvgAbiExports, AllEntryPointsAreExported)
{
    HMODULE dll = GetModuleHandleA("gsvg.dll");
    ASSERT_NE(dll, nullptr) << "gsvg.dll is not loaded in this process";
    for (const char* name : {"xpe_gsvg_version", "xpe_gsvg_init", "xpe_gsvg_process",
                             "xpe_gsvg_process_masked", "xpe_gsvg_process_ex",
                             "xpe_gsvg_shutdown"}) {
        EXPECT_NE(GetProcAddress(dll, name), nullptr) << name;
    }
    // control: a name that is not exported
    EXPECT_EQ(GetProcAddress(dll, "xpe_gsvg_process_unmasked"), nullptr);
}
#else
TEST(GsvgEndurance, ThousandCycles_CrtHeapDoesNotGrow)
{
    GTEST_SKIP() << "CRT heap walk is Windows-only in this build";
}
#endif
