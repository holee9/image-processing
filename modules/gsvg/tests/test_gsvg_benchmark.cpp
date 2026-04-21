#include <gtest/gtest.h>

#include "xpe/gsvg/gsvg_api.h"

#include <chrono>

TEST(BenchmarkFreeze, BP06_GsvgVersionProbeBaseline)
{
    constexpr auto kIterations = 1024;
    constexpr auto kMaxTotalUs = 5000;

    const char* version = nullptr;
    auto start = std::chrono::steady_clock::now();
    for (int i = 0; i < kIterations; ++i) {
        version = gsvg_version();
    }
    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::steady_clock::now() - start);

    ASSERT_NE(version, nullptr);
    EXPECT_STRNE(version, "");
    EXPECT_LT(elapsed.count(), kMaxTotalUs)
        << "BP-06 gsvg version-probe baseline exceeded.";
    RecordProperty("BP", "BP-06");
    RecordProperty("baseline_total_us_max", kMaxTotalUs);
    RecordProperty("iterations", kIterations);
}
