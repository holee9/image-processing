#include <gtest/gtest.h>

#include "xpe/gsvg/gsvg_api.h"

#include <chrono>

#include "perf_budget.h"

TEST(BenchmarkFreeze, BP06_GsvgVersionProbeBaseline)
{
    constexpr auto kIterations = 1024;
    constexpr auto kMaxTotalUs = 5000;

    // Judged by the MEDIAN of five timed passes of the 1024 calls, after one untimed warm-up pass
    // (perf_budget.h, QA-B-176, #179). The ceiling is 5 ms for a whole pass, so one scheduling quantum
    // (a context switch can cost 10-15 ms) used to fail a pass that normally takes microseconds.
    //
    // WHERE 5000 us COMES FROM: SPEC-BENCH-POST REQ-BPOST-001 ("1024 probes < 5000 us"), a ceiling that
    // spec froze on 2026-04-22 (measured 0 ms then). It is not derived from a product performance
    // requirement. REQ-BPOST-006 makes this test a CI gate, which is why one stall must not fail it.
    // QA-B-176 changed HOW the time is judged, not the value.
    const char* version = nullptr;
    const auto m = perf_budget::Measure("BP06_gsvg_version_probe_1024", [] {}, [&] {
        for (int i = 0; i < kIterations; ++i) {
            version = xpe_gsvg_version();
        }
        return XPE_OK;
    });

    ASSERT_NE(version, nullptr);
    EXPECT_STRNE(version, "");
    EXPECT_LT(m.medianUs, kMaxTotalUs)
        << "BP-06 gsvg version-probe baseline exceeded: " << perf_budget::Describe(m);
    RecordProperty("BP", "BP-06");
    RecordProperty("baseline_total_us_max", kMaxTotalUs);
    RecordProperty("iterations", kIterations);
}
