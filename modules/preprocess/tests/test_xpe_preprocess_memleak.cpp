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
 *     - Count the CRT heap blocks still allocated after the cycles
 *       (heap_growth.h, #181). Until QA-A-100 this case bounded the process
 *       PrivateUsage growth by max(5% of the baseline, 2 MB); a working-set /
 *       commit bound does not follow a leak (QA-B-92), so it was replaced.
 *
 *   XpePreprocessEndurance.ControlLeakIsCaught
 *     - The same frame cycle plus a 64-byte block kept per cycle must be seen
 *       by the same measurement.
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
 *   - A warm-up (heap_growth::kWarmup) is executed first so that any one-shot allocations
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

#include "heap_growth.h"

namespace {

constexpr uint32_t W = 512;
constexpr uint32_t H = 512;

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
    EXPECT_TRUE(rc == XPE_OK || rc == XPE_ERR_NOT_INITIALIZED || rc == XPE_ERR_CALIB_NOT_LOADED)
        << "xpe_offset_correct returned " << rc;

    xpe_preprocess_shutdown();
}

/* Buffers for one frame; allocated once, before any heap snapshot. */
struct FrameFixture {
    std::vector<uint16_t> rawPixels    = std::vector<uint16_t>(W * H, 2000);
    std::vector<uint16_t> offsetPixels = std::vector<uint16_t>(W * H, 200);
    std::vector<uint16_t> goldenRaw    = rawPixels;
    XpeImageBuffer rawBuf{};
    XpeImageBuffer offsetBuf{};

    FrameFixture() {
        rawBuf.data          = rawPixels.data();
        rawBuf.width         = W;
        rawBuf.height        = H;
        rawBuf.bitsAllocated = 16;
        rawBuf.bitsStored    = 16;
        rawBuf.format        = XPE_PIXEL_UINT16;
        rawBuf.dataSize      = rawPixels.size() * sizeof(uint16_t);

        offsetBuf.data          = offsetPixels.data();
        offsetBuf.width         = W;
        offsetBuf.height        = H;
        offsetBuf.bitsAllocated = 16;
        offsetBuf.bitsStored    = 16;
        offsetBuf.format        = XPE_PIXEL_UINT16;
        offsetBuf.dataSize      = offsetPixels.size() * sizeof(uint16_t);
    }

    void Run() { run_one_frame(rawBuf, offsetBuf, goldenRaw.data(), goldenRaw.size()); }
};

/* Start with no calibration loaded (QA-A-89, #176). run_one_frame accepts
 * "no offset map" but not "an offset map of another size": a 8x8 map left
 * by CalibCacheConcurrencyTest made the first frame's xpe_offset_correct
 * return -8 (XPE_ERR_BUFFER_TOO_SMALL) under --gtest_random_seed=9. The
 * frame's own shutdown then cleared it, so only one frame failed and the
 * memory figures stayed clean -- this was never a leak.
 * init -> shutdown so the clear runs on an initialized module. */
static void ClearModule() {
    (void)xpe_preprocess_init(nullptr);
    xpe_preprocess_shutdown();
}

} /* anonymous namespace */

/* =========================================================================
 * Gate G1a -> G1b : memory endurance
 * ========================================================================= */
TEST(XpePreprocessEndurance, NoMemoryLeakAfter1000Frames) {
#ifndef _WIN32
    GTEST_SKIP() << "CRT heap walk is Windows-only in this build";
#endif
    ClearModule();
    FrameFixture f;
    const heap_growth::Growth g = heap_growth::Measure([&](int) { f.Run(); });
    GTEST_LOG_(INFO) << heap_growth::Describe(g);
    EXPECT_LT(g.heap.blocks, heap_growth::MaxBlocks(g.cycles))
        << "init/process/shutdown cycles left blocks allocated";
    EXPECT_LT(g.heap.bytes, heap_growth::kMaxBytes)
        << "init/process/shutdown cycles left bytes allocated";
}

/* #181 (QA-A-100) control for the case above. */
TEST(XpePreprocessEndurance, ControlLeakIsCaught) {
#ifndef _WIN32
    GTEST_SKIP() << "CRT heap walk is Windows-only in this build";
#endif
    ClearModule();
    FrameFixture f;
    const heap_growth::Growth g = heap_growth::Measure([&](int) { f.Run(); }, 64);
    GTEST_LOG_(INFO) << "control 64 B/cycle: " << heap_growth::Describe(g);
    EXPECT_GE(g.heap.blocks, g.cycles * 9 / 10);
    EXPECT_GE(g.heap.bytes, 64LL * g.cycles * 9 / 10);
}
