// #156 (QA-B-59): does each config flag reach the output? -- gsvg.
//
// Same question as the B-58 sweep, on the modules it did not reach. The shape
// being hunted is the one behind #154, #155 and #156: an input is read, used,
// and then has no effect on the answer.
//
// gsvg's whole config surface is two boolean leaves -- the parser reads nothing
// else by design ("only boolean leaves are needed for GSVG config",
// gsvg.cpp:49):
//
//     vignette_correction   gsvg.cpp:190   default false
//     grid_suppression      gsvg.cpp:191   default false
//
// BOTH DEFAULT TO FALSE, which makes this module the easiest place in the tree
// to mis-measure. QA-B-52 nearly recorded "the header is wrong about the gain
// map" when the truth was that the vignette step never ran, because the flag it
// needs is off unless a config says otherwise. So each case below turns its flag
// ON explicitly and keeps a flag-off run beside it.
//
// The second floor is the fixture. Grid suppression subtracts a per-row mean
// deviation and skips any row whose deviation is under 1.0 code value
// (gsvg.cpp, suppress_grid_row_mean, kDeviationThreshold). A uniform image has
// per-row means exactly equal to the global mean -- deviation 0, branch skipped,
// output unchanged. Measuring the flag against a flat field would produce "no
// effect" from a function that was never given anything to do: the QA-B-58 §3
// trap. The fixture here carries actual grid lines.

#include <gtest/gtest.h>

#include "xpe/gsvg/gsvg_api.h"
#include "xpe/common/xpe_error.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr int    kW = 64, kH = 64;
constexpr size_t kN = static_cast<size_t>(kW) * kH;

// Rows alternate between two levels, so per-row means differ from the global
// mean by ~50 code values -- far above the 1.0 threshold the suppression uses.
std::vector<uint16_t> GridLines() {
    std::vector<uint16_t> px(kN);
    for (int y = 0; y < kH; ++y) {
        const uint16_t level = (y % 2 == 0) ? 1000 : 1100;
        for (int x = 0; x < kW; ++x) {
            px[static_cast<size_t>(y) * kW + x] =
                static_cast<uint16_t>(level + (x % 7));   // slight texture
        }
    }
    return px;
}

std::vector<float> GainRamp() {
    std::vector<float> g(kN);
    for (size_t i = 0; i < kN; ++i) {
        g[i] = 0.5f + 1.0f * (static_cast<float>(i) / static_cast<float>(kN - 1));
    }
    return g;
}

int CountDiffering(const std::vector<uint16_t>& a, const std::vector<uint16_t>& b) {
    int n = 0;
    for (size_t i = 0; i < a.size(); ++i) if (a[i] != b[i]) ++n;
    return n;
}

std::vector<uint16_t> Process(const char* config, bool withGainMap) {
    void* handle = nullptr;
    EXPECT_EQ(XPE_OK, xpe_gsvg_init(&handle, config));
    std::vector<uint16_t> src = GridLines();
    std::vector<uint16_t> dst(kN, 0);
    std::vector<float>    gain = GainRamp();

    EXPECT_EQ(XPE_OK, xpe_gsvg_process(handle,
                                       src.data(), src.size(),
                                       dst.data(), dst.size(),
                                       kW, kH,
                                       withGainMap ? gain.data() : nullptr,
                                       withGainMap ? gain.size() : 0u));
    xpe_gsvg_shutdown(handle);
    return dst;
}

}  // namespace

// The fixture floor, asserted before anything is concluded from a difference --
// or from the absence of one. If the rows do not deviate, grid suppression has
// nothing to act on and every result below is about the fixture.
TEST(GsvgParameterDependency, FixtureRowsDeviateAboveTheSuppressionThreshold) {
    const std::vector<uint16_t> px = GridLines();
    double globalAcc = 0.0;
    for (uint16_t v : px) globalAcc += v;
    const double globalMean = globalAcc / static_cast<double>(kN);

    double maxDev = 0.0;
    for (int y = 0; y < kH; ++y) {
        double rowAcc = 0.0;
        for (int x = 0; x < kW; ++x) rowAcc += px[static_cast<size_t>(y) * kW + x];
        maxDev = std::max(maxDev, std::fabs(rowAcc / kW - globalMean));
    }
    GTEST_LOG_(INFO) << "fixture max row-mean deviation=" << maxDev
                     << " (suppression threshold is 1.0)";
    ASSERT_GT(maxDev, 1.0)
        << "the fixture has nothing for grid suppression to remove, so a null "
           "result would say nothing about the flag";
}

TEST(GsvgParameterDependency, GridSuppressionFlagReachesTheOutput) {
    const std::vector<uint16_t> off = Process("{\"grid_suppression\": false}", false);
    const std::vector<uint16_t> on  = Process("{\"grid_suppression\": true}",  false);

    const int differing = CountDiffering(off, on);
    GTEST_LOG_(INFO) << "grid_suppression off vs on: " << differing
                     << " of " << kN << " pixels differ";
    EXPECT_GT(differing, 0) << "grid_suppression does not reach the output";
}

TEST(GsvgParameterDependency, VignetteFlagReachesTheOutput) {
    // Both runs supply the gain map; only the flag differs. That isolates the
    // flag from the map, which is the pair QA-B-52 had to separate.
    const std::vector<uint16_t> off = Process("{\"vignette_correction\": false}", true);
    const std::vector<uint16_t> on  = Process("{\"vignette_correction\": true}",  true);

    const int differing = CountDiffering(off, on);
    GTEST_LOG_(INFO) << "vignette_correction off vs on (gain map supplied both times): "
                     << differing << " of " << kN << " pixels differ";
    EXPECT_GT(differing, 0) << "vignette_correction does not reach the output";
}

TEST(GsvgParameterDependency, GainMapContentsReachTheOutput) {
    // Flag on in both runs; the map itself is what changes.
    auto run = [](bool rampUp) {
        void* handle = nullptr;
        EXPECT_EQ(XPE_OK, xpe_gsvg_init(&handle, "{\"vignette_correction\": true}"));
        std::vector<uint16_t> src = GridLines();
        std::vector<uint16_t> dst(kN, 0);
        std::vector<float>    gain = GainRamp();
        if (!rampUp) {
            for (size_t i = 0; i < gain.size(); ++i) gain[i] = 2.0f - gain[i];
        }
        EXPECT_EQ(XPE_OK, xpe_gsvg_process(handle, src.data(), src.size(),
                                           dst.data(), dst.size(), kW, kH,
                                           gain.data(), gain.size()));
        xpe_gsvg_shutdown(handle);
        return dst;
    };

    const int differing = CountDiffering(run(true), run(false));
    GTEST_LOG_(INFO) << "gain map rising vs falling: " << differing
                     << " of " << kN << " pixels differ";
    EXPECT_GT(differing, 0) << "the gain map contents do not reach the output";
}

// An unknown key must not silently turn anything on. The parser reads boolean
// leaves by name (gsvg.cpp:56), so a key it does not know should leave the
// defaults alone -- and the defaults are both false, meaning pass-through.
TEST(GsvgParameterDependency, UnknownConfigKeyLeavesTheDefaults) {
    const std::vector<uint16_t> unknownKey =
        Process("{\"grid_frequency_lp_per_mm\": 4.0, \"virtual_grid_enabled\": 1}", true);
    const std::vector<uint16_t> noConfig = Process(nullptr, true);

    const int differing = CountDiffering(unknownKey, noConfig);
    GTEST_LOG_(INFO) << "unknown-key config vs no config: " << differing
                     << " of " << kN << " pixels differ";
    EXPECT_EQ(0, differing)
        << "a config naming only keys this parser does not read changed the "
           "output, so something is being switched on by accident";
}
