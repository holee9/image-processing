/**
 * @file test_ghost_lag_contract.cpp
 * @brief The lag parameters a ghost handle accepts, and the unit they are in (QA-A-226b, #241).
 *
 * 1. A forward-unstable set is refused. The history the corrector subtracts has a steady-state gain
 *    S = alpha1/(1-exp(-1/tau1)) + alpha2/(1-exp(-1/tau2)); a constant input comes out as input*(1-S), clamped at 0
 *    (QA-A-226 measured 0.2945 = 1-0.7055 and 0.000 for S = 1.0583). A real lag y = x/(1-S) exists only for S < 1.
 *    S >= 1 (or a non-finite S) is XPE_ERR_CONFIG_INVALID at creation: no handle, nothing allocated.
 *    Only the weight-free S is checked: the tier 2/3 exposure weight has no grounded ceiling (QA-A-226 option (c)).
 * 2. tau is in FRAMES: every successful call is one step (dt = 1). acquisitionTime is not used. Before this the unit was
 *    seconds when a time was given and frames when not, and the gap between two frames was applied one frame late
 *    (the second frame's correction did not depend on the gap at all). A break in the sequence is xpe_ghost_reset().
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "preprocess_state_fixture.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr uint32_t W = 8, H = 8;
constexpr size_t N = static_cast<size_t>(W) * H;

XpeImageBuffer floatBuf(std::vector<float>& v) {
    XpeImageBuffer b{};
    b.data = v.data(); b.width = W; b.height = H; b.bitsAllocated = 32; b.bitsStored = 32;
    b.format = XPE_PIXEL_FLOAT32; b.dataSize = v.size() * sizeof(float);
    return b;
}

std::string lagConfig(int tier, double a1, double t1, double a2, double t2) {
    char buf[256];
    std::snprintf(buf, sizeof(buf), "{\"tier\":%d,\"alpha1\":%.17g,\"tau1\":%.17g,\"alpha2\":%.17g,\"tau2\":%.17g}",
                  tier, a1, t1, a2, t2);
    return buf;
}

double steadyGain(double a1, double t1, double a2, double t2) {
    return a1 / (1.0 - std::exp(-1.0 / t1)) + a2 / (1.0 - std::exp(-1.0 / t2));
}

std::vector<float> frame(unsigned seed) {
    std::mt19937 g(seed);
    std::uniform_real_distribution<float> d(1000.0f, 30000.0f);
    std::vector<float> f(N);
    for (auto& x : f) x = d(g);
    return f;
}

bool sameBytes(const std::vector<float>& a, const std::vector<float>& b) {
    return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
}

class GhostLagContract : public XpePreprocessStateFixture {
protected:
    void SetUp() override { XpePreprocessStateFixture::SetUp(); xpe_clear_alerts(); }
    void TearDown() override { xpe_clear_alerts(); XpePreprocessStateFixture::TearDown(); }

    // The corrected frames of a fixed sequence, with the timestamps given (one per frame).
    std::vector<std::vector<float>> run(const std::string& cfg, const std::vector<uint64_t>& stamps) {
        void* h = nullptr;
        EXPECT_EQ(XPE_OK, xpe_ghost_create(W, H, cfg.c_str(), &h));
        std::vector<std::vector<float>> out;
        for (size_t k = 0; k < stamps.size(); ++k) {
            std::vector<float> f = frame(40u + static_cast<unsigned>(k));
            XpeImageBuffer b = floatBuf(f);
            XpeImageMetadata meta{};
            meta.acquisitionTime = stamps[k];
            EXPECT_EQ(XPE_OK, xpe_ghost_correct(h, &b, &meta));
            out.push_back(f);
        }
        xpe_ghost_destroy(h);
        return out;
    }
};

// ---- 1. S >= 1 is refused --------------------------------------------------------------------------

TEST_F(GhostLagContract, AForwardUnstableSetIsRefusedInEveryTier) {
    const struct { const char* name; double a1, t1, a2, t2; } bad[] = {
        {"the old built-in values (S 2.449)", 0.9, 1.0, 0.05, 20.0},
        {"S 1.0583", 0.3, 3.0, 0.0, 30.0},
        {"S 1.3156", 0.2, 3.0, 0.02, 30.0},
        {"S 1.6125", 1.0, 1.0, 0.001, 30.0},
    };
    for (const auto& c : bad) {
        ASSERT_GE(steadyGain(c.a1, c.t1, c.a2, c.t2), 1.0) << c.name;
        for (int tier = 1; tier <= 3; ++tier) {
            void* h = reinterpret_cast<void*>(0x1);
            EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_ghost_create(W, H, lagConfig(tier, c.a1, c.t1, c.a2, c.t2).c_str(), &h))
                << c.name << ", tier " << tier;
            EXPECT_EQ(reinterpret_cast<void*>(0x1), h) << "a refused creation hands nothing back (the out-pointer is left alone)";
        }
    }
    EXPECT_EQ(0, xpe_get_pending_alert_count()) << "a refused creation raises no alert, like every other CONFIG_INVALID";
}

TEST_F(GhostLagContract, TheBoundaryIsSEqualsOne) {
    // alpha1 alone with tau1 = 1 and alpha2 = 0: S = alpha1/(1-e^-1), so alpha1 = (1-e^-1) is S = 1.
    const double edge = 1.0 - std::exp(-1.0);
    void* h = nullptr;
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_ghost_create(W, H, lagConfig(1, edge * 1.001, 1.0, 0.0, 20.0).c_str(), &h));
    EXPECT_EQ(XPE_OK, xpe_ghost_create(W, H, lagConfig(1, edge * 0.999, 1.0, 0.0, 20.0).c_str(), &h));
    xpe_ghost_destroy(h);
}

TEST_F(GhostLagContract, AStableSetIsAcceptedInEveryTierAndRaisesNoWarning) {
    const struct { double a1, t1, a2, t2; } good[] = {
        {0.02, 3.0, 0.003, 30.0}, {0.2, 3.0, 0.0, 30.0}, {0.1, 1.0, 0.01, 20.0},
    };
    for (const auto& c : good) {
        ASSERT_LT(steadyGain(c.a1, c.t1, c.a2, c.t2), 1.0);
        for (int tier = 1; tier <= 3; ++tier) {
            void* h = nullptr;
            EXPECT_EQ(XPE_OK, xpe_ghost_create(W, H, lagConfig(tier, c.a1, c.t1, c.a2, c.t2).c_str(), &h)) << "tier " << tier;
            xpe_ghost_destroy(h);
        }
    }
    EXPECT_EQ(0, xpe_get_pending_alert_count());
}

TEST_F(GhostLagContract, ANonFiniteGainIsRefused) {
    // tau tiny enough that 1-exp(-1/tau) rounds to 1 is fine; a huge alpha overflows S to infinity.
    void* h = nullptr;
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_ghost_create(W, H, lagConfig(1, 1.0e308, 1.0, 1.0e308, 1.0).c_str(), &h));
    // the check that is not the S >= 1 one: a negative alpha over a zero denominator is -inf, which S >= 1 lets through
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_ghost_create(W, H, lagConfig(1, -0.1, 1.0e20, 0.0, 20.0).c_str(), &h));
}

TEST_F(GhostLagContract, AZeroAlphaContributesNothingWhateverItsTauIs) {
    // tau = 1e20 makes exp(-1/tau) exactly 1.0, so the denominator of the gain is 0: 0/0 for a zero alpha, +inf for a
    // positive one. The first is a harmless "no correction" set and is accepted; the second is an infinite gain.
    void* h = nullptr;
    EXPECT_EQ(XPE_OK, xpe_ghost_create(W, H, lagConfig(1, 0.0, 1.0e20, 0.0, 1.0e20).c_str(), &h));
    xpe_ghost_destroy(h);
    EXPECT_EQ(XPE_OK, xpe_ghost_create(W, H, lagConfig(1, 0.0, 1.0e20, 0.01, 20.0).c_str(), &h));
    xpe_ghost_destroy(h);
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_ghost_create(W, H, lagConfig(1, 0.01, 1.0e20, 0.0, 20.0).c_str(), &h));
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_ghost_create(W, H, lagConfig(1, 0.0, 20.0, 1.0e-30, 1.0e20).c_str(), &h));
}

TEST_F(GhostLagContract, ASetThatIsNotCalibratedIsNotJudged) {
    // Three of the four keys: the handle is uncalibrated (passes frames through), so there is no forward system to judge.
    void* h = nullptr;
    EXPECT_EQ(XPE_OK, xpe_ghost_create(W, H, "{\"alpha1\":5.0,\"tau1\":1,\"alpha2\":5.0}", &h));
    xpe_ghost_destroy(h);
    xpe_clear_alerts();
}

// ---- 2. tau is in frames ----------------------------------------------------------------------------

TEST_F(GhostLagContract, TheOutputDoesNotDependOnTheTimes) {
    const std::vector<uint64_t> none{0, 0, 0, 0, 0, 0};
    const std::vector<std::vector<uint64_t>> others{
        {100, 101, 102, 103, 104, 105},                       // one second apart
        {100, 100, 100, 100, 100, 100},                       // the same second
        {1, 600, 601, 10000000, 10000001, 20000000},          // gaps of minutes and months
        {500, 400, 300, 200, 100, 50},                        // backwards
        {1700000000u, 1700000003u, 1700000003u, 1700000009u, 1700000010u, 1700000500u},
    };
    for (int tier = 1; tier <= 3; ++tier) {
        const std::string cfg = lagConfig(tier, 0.02, 3.0, 0.003, 30.0);
        const auto ref = run(cfg, none);
        for (size_t s = 0; s < others.size(); ++s) {
            const auto got = run(cfg, others[s]);
            for (size_t k = 0; k < ref.size(); ++k) {
                EXPECT_TRUE(sameBytes(ref[k], got[k])) << "tier " << tier << ", time series " << s << ", frame " << k;
            }
        }
    }
}

TEST_F(GhostLagContract, EachCallIsOneFrameStep) {
    // Constant 1000, a 600 s gap before frame 2: frame 2 = 1000 - a1*1000 - a2*1000 (the history is the previous frame,
    // whatever the time), and frame 3 uses the history decayed by exactly one frame step.
    const double a1 = 0.02, t1 = 3.0, a2 = 0.003, t2 = 30.0;
    void* h = nullptr;
    ASSERT_EQ(XPE_OK, xpe_ghost_create(W, H, lagConfig(1, a1, t1, a2, t2).c_str(), &h));
    const uint64_t stamps[3] = {100, 700, 701};
    double h1 = 0.0, h2 = 0.0;
    for (int k = 0; k < 3; ++k) {
        std::vector<float> f(N, 1000.0f);
        XpeImageBuffer b = floatBuf(f);
        XpeImageMetadata meta{};
        meta.acquisitionTime = stamps[k];
        ASSERT_EQ(XPE_OK, xpe_ghost_correct(h, &b, &meta));
        const double expect = 1000.0 - a1 * h1 - a2 * h2;
        for (size_t i = 0; i < N; ++i) ASSERT_NEAR(expect, f[i], 1e-2) << "frame " << k << " pixel " << i;
        h1 = std::exp(-1.0 / t1) * h1 + 1000.0;
        h2 = std::exp(-1.0 / t2) * h2 + 1000.0;
    }
    xpe_ghost_destroy(h);
}

TEST_F(GhostLagContract, AResetStartsTheSequenceOver) {
    // The documented way to mark a break: after xpe_ghost_reset the next frame has no history.
    void* h = nullptr;
    ASSERT_EQ(XPE_OK, xpe_ghost_create(W, H, lagConfig(1, 0.02, 3.0, 0.003, 30.0).c_str(), &h));
    XpeImageMetadata meta{};
    for (int k = 0; k < 3; ++k) {
        std::vector<float> f(N, 1000.0f);
        XpeImageBuffer b = floatBuf(f);
        ASSERT_EQ(XPE_OK, xpe_ghost_correct(h, &b, &meta));
    }
    ASSERT_EQ(XPE_OK, xpe_ghost_reset(h));
    std::vector<float> f(N, 1000.0f);
    XpeImageBuffer b = floatBuf(f);
    ASSERT_EQ(XPE_OK, xpe_ghost_correct(h, &b, &meta));
    for (size_t i = 0; i < N; ++i) ASSERT_EQ(1000.0f, f[i]);
    xpe_ghost_destroy(h);
}

} // namespace
