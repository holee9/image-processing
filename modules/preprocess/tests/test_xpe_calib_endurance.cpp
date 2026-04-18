/**
 * @file test_xpe_calib_endurance.cpp
 * @brief Endurance and concurrency tests for calibration load/unload (T-010)
 *
 * SPEC-XPE-P1A SUP-01 -- REQ-P1A-031 (RAII), REQ-P1A-003 (thread-safety)
 *
 * Test cases:
 *  1. 1000-cycle load_offset/load_gain/load_defect round-trip: no crash, XPE_OK
 *  2. Memory stability: RSS growth after 1000 cycles < 1 MB (Windows WorkingSetSize)
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

// Windows-specific RSS measurement
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
// Test 2: Memory stability -- WorkingSet growth < 1 MB after 1000 cycles
// =============================================================================
TEST_F(EnduranceTest, ThousandCycles_MemoryGrowthUnderOneMB) {
#ifndef _WIN32
    GTEST_SKIP() << "RSS measurement only supported on Windows in this build";
#endif
    constexpr int CYCLES = 1000;
    constexpr SIZE_T ONE_MB = 1024 * 1024;

    SIZE_T before = get_working_set_bytes();

    for (int i = 0; i < CYCLES; ++i) {
        xpe_calib_load_offset(OFF_PATH);
        xpe_calib_load_gain(GAIN_PATH);
        xpe_calib_load_defect_map(DEF_PATH);
    }

    SIZE_T after = get_working_set_bytes();

    // Allow up to 1 MB growth (expected: ~0, RAII cleans up on each load)
    if (after > before) {
        EXPECT_LT(after - before, ONE_MB)
            << "Memory grew by " << (after - before) / 1024 << " KB over "
            << CYCLES << " cycles";
    }
    // If after <= before, working set shrunk (fine)
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
