/**
 * @file test_perf_budget_helper.cpp
 * @brief Pins what perf_budget::Measure discriminates (QA-B-175, #179).
 *
 * The budget tests (T308, T508, LargeImagePerformance, T603b, T608) used to time ONE call with the wall
 * clock and assert on it, so a single scheduling stall on a loaded machine failed the test (T608: 121 ms
 * against 7 ms measured 36 times, QA-B-174). They now judge the MEDIAN of several timed calls after an
 * untimed warm-up. These tests show, with deterministic fake workloads, that the median is the right
 * statistic: it ignores one outlier, and it still fails when the code really is slow.
 */
#include <gtest/gtest.h>

#include <chrono>
#include <cstdlib>
#include <string>
#include <thread>

#include "perf_budget.h"

namespace {

using std::chrono::milliseconds;

constexpr long long kSlowUs = 100 * 1000;   // an injected delay: 100 ms, far above any fast call
constexpr long long kHalfSlowUs = kSlowUs / 2;

void SleepMs(int ms) { std::this_thread::sleep_for(milliseconds(ms)); }

/** Sets an environment variable for the lifetime of the object (Windows: _putenv_s). */
class ScopedEnv {
public:
    ScopedEnv(const char* name, const char* value) : name_(name) { _putenv_s(name, value); }
    ~ScopedEnv() { _putenv_s(name_.c_str(), ""); }   // an empty value removes it
    ScopedEnv(const ScopedEnv&) = delete;
    ScopedEnv& operator=(const ScopedEnv&) = delete;

private:
    std::string name_;
};

}  // namespace

TEST(PerfBudgetHelper, MedianOfAnOddAndAnEvenCount) {
    EXPECT_EQ(30, perf_budget::MedianOf({50, 10, 30, 20, 40}));
    EXPECT_EQ(25, perf_budget::MedianOf({10, 20, 30, 40}));   // the mean of the two middle values
    EXPECT_EQ(7, perf_budget::MedianOf({7}));
}

TEST(PerfBudgetHelper, OneSlowSampleDoesNotMoveTheMedian) {
    int call = 0;
    const auto r = perf_budget::Measure("fake", [] {}, [&] {
        if (++call == 2) SleepMs(100);   // call 1 is the warm-up; call 2 is the FIRST timed sample
        return XPE_OK;
    });
    ASSERT_EQ(perf_budget::kRuns, static_cast<int>(r.samplesUs.size()));
    EXPECT_GE(r.maxUs, kSlowUs - 5000) << "the injected outlier was not measured at all";
    EXPECT_LT(r.medianUs, kHalfSlowUs) << "ONE slow sample out of " << perf_budget::kRuns
                                       << " moved the median: this is the single-shot flaw again";
}

TEST(PerfBudgetHelper, TwoSlowSamplesOutOfFiveStillDoNotMoveTheMedian) {
    int call = 0;
    const auto r = perf_budget::Measure("fake", [] {}, [&] {
        ++call;
        if (call == 2 || call == 4) SleepMs(100);
        return XPE_OK;
    });
    EXPECT_LT(r.medianUs, kHalfSlowUs);
}

TEST(PerfBudgetHelper, EverySampleSlowIsStillSlow) {
    const auto r = perf_budget::Measure("fake", [] {}, [] {
        SleepMs(100);
        return XPE_OK;
    });
    EXPECT_GE(r.medianUs, kSlowUs - 5000) << "code that is slow on EVERY call must not pass";
    EXPECT_GE(r.minUs, kSlowUs - 5000);
}

TEST(PerfBudgetHelper, ThreeSlowSamplesOutOfFiveMoveTheMedian) {
    int call = 0;
    const auto r = perf_budget::Measure("fake", [] {}, [&] {
        ++call;
        if (call >= 2 && call <= 4) SleepMs(100);   // three of the five timed samples
        return XPE_OK;
    });
    EXPECT_GE(r.medianUs, kSlowUs - 5000) << "a majority of slow samples is a real slowdown";
}

// These checks are made on a LARGE injected delay (300 ms) with a threshold far below it, never on a
// small absolute bound for an ordinary call: this test runs in CI on shared runners, and "a trivial call
// stayed under 30 ms" would fail there on one scheduling stall -- the flaw this helper exists to remove.
TEST(PerfBudgetHelper, TheWarmUpIsNotTimed) {
    int call = 0;
    const auto r = perf_budget::Measure("fake", [] {}, [&] {
        if (++call == 1) SleepMs(300);   // slow only on the very first call: the warm-up
        return XPE_OK;
    });
    EXPECT_EQ(perf_budget::kRuns + 1, call) << "one warm-up plus the timed runs";
    EXPECT_LT(r.maxUs, 250 * 1000) << "the cold first call leaked into the timed samples";
}

TEST(PerfBudgetHelper, PrepareIsNotTimed) {
    // prepare() sleeps 60 ms before every call; if it were timed every sample would be >= 60 ms. The
    // MEDIAN is judged, so a stall in one or two samples cannot fail this.
    const auto r = perf_budget::Measure("fake", [] { SleepMs(60); }, [] { return XPE_OK; });
    EXPECT_LT(r.medianUs, 30 * 1000) << "restoring the input must stay outside the timed region";
}

// The same discrimination, driven the way the real budget tests are driven: through the environment, so
// the real tests can be run with an injected outlier without editing them (QA-B-175 falsification arms).
TEST(PerfBudgetHelper, InjectionOfTheFirstSampleOnlyIsIgnoredByTheMedian) {
    ScopedEnv delay("XPE_PERF_INJECT_DELAY_MS", "100");
    ScopedEnv mode("XPE_PERF_INJECT_MODE", "first");
    const auto r = perf_budget::Measure("fake", [] {}, [] { return XPE_OK; });
    EXPECT_GE(r.maxUs, kSlowUs - 5000) << "the injection did not reach the timed region";
    EXPECT_LT(r.medianUs, kHalfSlowUs);
}

TEST(PerfBudgetHelper, InjectionOfEverySampleMovesTheMedian) {
    ScopedEnv delay("XPE_PERF_INJECT_DELAY_MS", "100");
    ScopedEnv mode("XPE_PERF_INJECT_MODE", "all");
    const auto r = perf_budget::Measure("fake", [] {}, [] { return XPE_OK; });
    EXPECT_GE(r.medianUs, kSlowUs - 5000);
}

TEST(PerfBudgetHelper, WithoutInjectionNothingIsDelayed) {
    // An injection would be >= 100 ms; the bound sits just below that, not at the size of an ordinary call.
    const auto r = perf_budget::Measure("fake", [] {}, [] { return XPE_OK; });
    EXPECT_LT(r.maxUs, 90 * 1000);
}
