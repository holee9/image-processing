/**
 * @file perf_budget.h
 * @brief Judge a wall-clock budget by the MEDIAN of several timed calls (QA-B-175, #179).
 *
 * WHY. The budget tests used to time ONE call and assert on it, so one scheduling stall on a loaded
 * machine failed the test: T608 reported 121 ms for a call that measured 6-10 ms (median 7) in 36 other
 * runs (QA-B-174). The statistic is now the median of kRuns timed calls after one untimed warm-up:
 *   - one stall (or two, out of five) does not move the median;
 *   - code that is slow on every call, or on most of them, still does -- the budget still means
 *     something. test_perf_budget_helper.cpp pins both directions with fake workloads.
 *
 * WHAT THIS IS NOT. It does not choose a budget. The budgets in the tests are the values they always had;
 * where one is not derived from a requirement the test says so. And it is not perf_measure.h, which is a
 * measure-only helper that asserts nothing about time and exists as a byte-identical copy in three test
 * directories; this one judges, so it lives only here.
 *
 * Test-only injection (falsification arms, inert unless set): XPE_PERF_INJECT_DELAY_MS=<ms> delays the
 * timed region, XPE_PERF_INJECT_MODE=first|all (default first) picks the first timed sample only or every
 * timed sample. The warm-up is never delayed. It lets the REAL budget tests be run with an injected
 * outlier, and with every sample slow, without editing them.
 */
#pragma once

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include "xpe/common/xpe_error.h"

namespace perf_budget {

/** Timed calls per measurement. Odd, so the median is one of the samples; 5 tolerates 2 outliers. */
constexpr int kRuns = 5;

/** The median; for an even count, the mean of the two middle values. */
inline long long MedianOf(std::vector<long long> v) {
    std::sort(v.begin(), v.end());
    const size_t n = v.size();
    if (n == 0) return 0;
    return (n % 2 == 1) ? v[n / 2] : (v[n / 2 - 1] + v[n / 2]) / 2;
}

struct Result {
    long long medianUs = 0;
    long long minUs = 0;
    long long maxUs = 0;
    std::vector<long long> samplesUs;   // in call order, not sorted
};

namespace detail {

inline std::string EnvOrEmpty(const char* name) {
    char* value = nullptr;
    size_t len = 0;
    if (_dupenv_s(&value, &len, name) != 0 || value == nullptr) return std::string();
    std::string out(value);
    std::free(value);
    return out;
}

/** Microseconds of test-injected delay for timed sample @p index (0-based); 0 when not injecting. */
inline long long InjectedDelayUs(int index) {
    const std::string ms = EnvOrEmpty("XPE_PERF_INJECT_DELAY_MS");
    if (ms.empty()) return 0;
    const long v = std::strtol(ms.c_str(), nullptr, 10);
    if (v <= 0) return 0;
    std::string mode = EnvOrEmpty("XPE_PERF_INJECT_MODE");
    if (mode.empty()) mode = "first";
    if (mode == "all" || (mode == "first" && index == 0)) return static_cast<long long>(v) * 1000;
    return 0;
}

}  // namespace detail

/**
 * One untimed warm-up, then @p runs timed calls. @p prepare restores the input before every call (the
 * functions under test work in place) and is not timed; @p run is the timed call and returns its
 * XpeErrorCode, which must be XPE_OK every time. Prints one grep-able line:
 *   PERFBUDGET <what> median=<us> min=<us> max=<us> us (runs=<n>)
 */
template <class Prepare, class Run>
Result Measure(const char* what, Prepare prepare, Run run, int runs = kRuns) {
    prepare();
    EXPECT_EQ(XPE_OK, run()) << what << " warm-up";

    Result r;
    r.samplesUs.reserve(static_cast<size_t>(runs));
    for (int i = 0; i < runs; ++i) {
        prepare();
        const long long injectedUs = detail::InjectedDelayUs(i);
        const auto t0 = std::chrono::steady_clock::now();
        if (injectedUs > 0) std::this_thread::sleep_for(std::chrono::microseconds(injectedUs));
        const XpeErrorCode rc = run();
        const auto t1 = std::chrono::steady_clock::now();
        EXPECT_EQ(XPE_OK, rc) << what << " run " << i;
        r.samplesUs.push_back(std::chrono::duration_cast<std::chrono::microseconds>(t1 - t0).count());
    }
    r.medianUs = MedianOf(r.samplesUs);
    r.minUs = *std::min_element(r.samplesUs.begin(), r.samplesUs.end());
    r.maxUs = *std::max_element(r.samplesUs.begin(), r.samplesUs.end());
    std::printf("PERFBUDGET %s median=%lld min=%lld max=%lld us (runs=%d)\n", what, r.medianUs, r.minUs,
                r.maxUs, runs);
    std::fflush(stdout);
    return r;
}

/** "median 7.1 ms of [7.0 7.2 ...] ms": for an assertion message that shows the samples, not just the verdict. */
inline std::string Describe(const Result& r) {
    char buf[64];
    std::string s = "median ";
    std::snprintf(buf, sizeof(buf), "%.2f", static_cast<double>(r.medianUs) / 1000.0);
    s += buf;
    s += " ms of [";
    for (size_t i = 0; i < r.samplesUs.size(); ++i) {
        std::snprintf(buf, sizeof(buf), "%s%.2f", i ? " " : "", static_cast<double>(r.samplesUs[i]) / 1000.0);
        s += buf;
    }
    s += "] ms";
    return s;
}

}  // namespace perf_budget
