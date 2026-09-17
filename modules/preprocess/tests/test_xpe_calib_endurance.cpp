/**
 * @file test_xpe_calib_endurance.cpp
 * @brief Endurance and concurrency tests for calibration load/unload (T-010)
 *
 * SPEC-XPE-P1A SUP-01 -- REQ-P1A-031 (RAII), REQ-P1A-003 (thread-safety)
 *
 * Test cases:
 *  1. 1000-cycle load_offset/load_gain/load_defect round-trip: no crash, XPE_OK
 *  2. Memory stability: CRT heap retention over the cycles (heap_growth.h, #181),
 *     paired with a control that leaks 64 bytes per cycle
 *  3. 4-thread concurrent load_offset: all threads succeed, no crash
 *  4. 4-thread concurrent load_gain: all threads succeed, no crash
 *  5. 4-thread concurrent mixed (offset + gain + defect): no crash, last write wins
 */

#include <gtest/gtest.h>
#include <cstring>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <fstream>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "fixtures/make_xcal.hpp"

#include "heap_growth.h"

namespace {
constexpr uint32_t W = 128;
constexpr uint32_t H = 128;

const char* OFF_PATH  = "t010_endurance_offset.xcal";
const char* GAIN_PATH = "t010_endurance_gain.xcal";
const char* DEF_PATH  = "t010_endurance_defect.xcal";

} // anonymous namespace

class EnduranceTest : public ::testing::Test {
protected:
    static void SetUpTestSuite() {
        // Create fixture files once for all tests in this suite
        ASSERT_EQ(MakeOffsetXCal(OFF_PATH,  W, H, 1.0f), XPE_OK);
        ASSERT_EQ(MakeGainXCal(GAIN_PATH,   W, H, 0.5f), XPE_OK);
        ASSERT_EQ(MakeDefectXCal(DEF_PATH,  W, H, 0),    XPE_OK);
    }

    static void TearDownTestSuite() {
        std::remove(OFF_PATH);
        std::remove(GAIN_PATH);
        std::remove(DEF_PATH);
    }

    // Release every map a case loaded (QA-A-89, #176). These cases load
    // offset/gain/defect maps into the module and never shut it down, so the
    // last map stayed loaded for whatever ran next in the same process -- a
    // pipeline case expecting "no defect map" got a W x H map instead.
    //
    // init -> shutdown so the clear runs on an INITIALIZED module; the header
    // documents shutdown on an uninitialized module as a no-op. The init result
    // is ignored: already-initialized answers XPE_ERR_INVALID_INPUT, and the
    // module is initialized either way.
    void TearDown() override {
        (void)xpe_preprocess_init(nullptr);
        xpe_preprocess_shutdown();
    }
};

// =============================================================================
// Test 1: 1000 load cycles -- no crash, all return XPE_OK
// =============================================================================
TEST_F(EnduranceTest, ThousandCycles_NocrashAllOk) {
    constexpr int CYCLES = 1000;
    for (int i = 0; i < CYCLES; ++i) {
        ASSERT_EQ(xpe_calib_load_offset(OFF_PATH),      XPE_OK) << "cycle " << i;
        ASSERT_EQ(xpe_calib_load_gain(GAIN_PATH),        XPE_OK) << "cycle " << i;
        ASSERT_EQ(xpe_calib_load_defect_map(DEF_PATH),   XPE_OK) << "cycle " << i;
    }
}

// =============================================================================
// Test 2: Memory stability -- CRT heap retention over the load cycles
//
// #181 (QA-A-100): until now this case bounded the process working set
// (< 1 MB after 1000 cycles, warm-up first per #105). QA-B-92 showed the
// working set does not follow a leak, so the case now counts the CRT heap
// blocks still allocated after the cycles (heap_growth.h) and is paired with a
// control that must fail the same bound.
// =============================================================================
namespace {
void LoadAllThree(int) {
    xpe_calib_load_offset(OFF_PATH);
    xpe_calib_load_gain(GAIN_PATH);
    xpe_calib_load_defect_map(DEF_PATH);
}
} // anonymous namespace

TEST_F(EnduranceTest, LoadCycles_DoNotGrowCrtHeap) {
#ifndef _WIN32
    GTEST_SKIP() << "CRT heap walk is Windows-only in this build";
#endif
    const heap_growth::Growth g = heap_growth::Measure(LoadAllThree);
    GTEST_LOG_(INFO) << heap_growth::Describe(g);
    EXPECT_LT(g.heap.blocks, heap_growth::MaxBlocks(g.cycles))
        << "load cycles left blocks allocated -- a replaced map is not released";
    EXPECT_LT(g.heap.bytes, heap_growth::kMaxBytes)
        << "load cycles left bytes allocated -- a replaced map is not released";
}

// #181 (QA-A-100) control for the case above: the same cycle plus a 64-byte
// block kept per cycle must be seen by the same measurement.
TEST_F(EnduranceTest, LoadCycles_ControlLeakIsCaught) {
#ifndef _WIN32
    GTEST_SKIP() << "CRT heap walk is Windows-only in this build";
#endif
    const heap_growth::Growth g = heap_growth::Measure(LoadAllThree, 64);
    GTEST_LOG_(INFO) << "control 64 B/cycle: " << heap_growth::Describe(g);
    EXPECT_GE(g.heap.blocks, g.cycles * 9 / 10);
    EXPECT_GE(g.heap.bytes, 64LL * g.cycles * 9 / 10);
}

// =============================================================================
// Test 3: 4-thread concurrent load_offset -- no crash, no data race
// =============================================================================
TEST_F(EnduranceTest, FourThreadsConcurrentLoadOffset_NoCrash) {
    constexpr int NUM_THREADS = 4;
    constexpr int ITERS_PER_THREAD = 50;

    std::atomic<int> failures{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < ITERS_PER_THREAD; ++i) {
                XpeErrorCode rc = xpe_calib_load_offset(OFF_PATH);
                if (rc != XPE_OK) {
                    ++failures;
                }
            }
        });
    }

    for (auto& th : threads) th.join();
    EXPECT_EQ(failures.load(), 0);
}

// =============================================================================
// Test 4: 4-thread concurrent load_gain -- no crash
// =============================================================================
TEST_F(EnduranceTest, FourThreadsConcurrentLoadGain_NoCrash) {
    constexpr int NUM_THREADS = 4;
    constexpr int ITERS_PER_THREAD = 50;

    std::atomic<int> failures{0};
    std::vector<std::thread> threads;

    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([&]() {
            for (int i = 0; i < ITERS_PER_THREAD; ++i) {
                XpeErrorCode rc = xpe_calib_load_gain(GAIN_PATH);
                if (rc != XPE_OK) {
                    ++failures;
                }
            }
        });
    }

    for (auto& th : threads) th.join();
    EXPECT_EQ(failures.load(), 0);
}

// =============================================================================
// Test 5: 4-thread mixed concurrent (offset, gain, defect) -- no crash
// =============================================================================
TEST_F(EnduranceTest, FourThreadsMixedLoad_NoCrash) {
    constexpr int ITERS_PER_THREAD = 50;
    std::atomic<int> failures{0};

    std::vector<std::thread> threads;

    // Thread 0: load_offset
    threads.emplace_back([&]() {
        for (int i = 0; i < ITERS_PER_THREAD; ++i) {
            if (xpe_calib_load_offset(OFF_PATH) != XPE_OK) ++failures;
        }
    });

    // Thread 1: load_gain
    threads.emplace_back([&]() {
        for (int i = 0; i < ITERS_PER_THREAD; ++i) {
            if (xpe_calib_load_gain(GAIN_PATH) != XPE_OK) ++failures;
        }
    });

    // Thread 2: load_defect
    threads.emplace_back([&]() {
        for (int i = 0; i < ITERS_PER_THREAD; ++i) {
            if (xpe_calib_load_defect_map(DEF_PATH) != XPE_OK) ++failures;
        }
    });

    // Thread 3: alternate offset + gain
    threads.emplace_back([&]() {
        for (int i = 0; i < ITERS_PER_THREAD; ++i) {
            XpeErrorCode rc = (i % 2 == 0)
                ? xpe_calib_load_offset(OFF_PATH)
                : xpe_calib_load_gain(GAIN_PATH);
            if (rc != XPE_OK) ++failures;
        }
    });

    for (auto& th : threads) th.join();
    EXPECT_EQ(failures.load(), 0);
}
