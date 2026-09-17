/**
 * @file perf_measure.h
 * @brief Measure-only timing helper for BenchmarkFreeze_*Performance* cases (#179).
 *
 * The same file exists in the enhance_basic, enhance_advanced, display and gsvg test
 * directories: each module's tests build into its own executable and share no
 * test-support library.
 *
 * It prints one grep-able line per measurement and asserts nothing about time:
 *
 *   PERFMEASURE <requirement-id> <size> min=<us> med=<us> max=<us> us (runs=<n>)
 *
 * A gate chosen on a developer machine has failed on CI before (810 ms local,
 * 1340 ms CI), so thresholds are to be set from the numbers these lines produce
 * on the benchmark runner (QA-B-84, QA-B-86).
 */
#pragma once

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "xpe/common/xpe_error.h"

namespace perf_measure {

constexpr int kRuns = 7;

/**
 * One untimed warm-up, then kRuns timed calls. prepare() restores the input
 * before every call (the functions under test work in place) and is not timed;
 * run() is the timed call and returns its XpeErrorCode.
 */
template <class Prepare, class Run>
void Measure(const char* requirementId, const char* size, Prepare prepare, Run run) {
    prepare();
    EXPECT_EQ(XPE_OK, run()) << requirementId << " warm-up";

    std::vector<long long> us;
    us.reserve(kRuns);
    for (int i = 0; i < kRuns; ++i) {
        prepare();
        const auto t0 = std::chrono::steady_clock::now();
        const XpeErrorCode rc = run();
        const auto t1 = std::chrono::steady_clock::now();
        EXPECT_EQ(XPE_OK, rc) << requirementId << " run " << i;
        us.push_back(std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count());
    }
    std::sort(us.begin(), us.end());
    const long long mn = us.front(), md = us[us.size() / 2], mx = us.back();
    std::printf("PERFMEASURE %s %s min=%lld med=%lld max=%lld us (runs=%d)\n",
                requirementId, size, mn, md, mx, kRuns);
    std::fflush(stdout);
    ::testing::Test::RecordProperty("perf_min_us", std::to_string(mn));
    ::testing::Test::RecordProperty("perf_med_us", std::to_string(md));
    ::testing::Test::RecordProperty("perf_max_us", std::to_string(mx));
}

}  // namespace perf_measure
