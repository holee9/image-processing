/**
 * @file test_ghost_mix_compare.cpp
 * @brief QA-A-225 M4: the tier-3 blend (0.7 corrected + 0.3 mean of the incoming 3x3) kept versus removed, measured through a
 *        test-only seam (#238). Allocation-failure executable only (XPE_CACHE_TEST_HOOKS): the shipped library has neither the
 *        seam nor a use of it.
 *
 * The decision rules were fixed BEFORE this code and these results existed: `.moai/reports/lane-pre/QA-A-225-M4/preregistration.md`
 * (its own commit). The comparison below follows it to the letter and prints K1..K4 and the label; it asserts none of them. #238
 * is not decided here: until real-device data exists this is measurement evidence only. What IS asserted is mechanical: the
 * analytic values the preregistration computed first (the Nyquist response, the noise ratio, the ring step), that the seam
 * touches tier 3 and nothing else, and that its defaults are the shipped constants.
 */

#include <gtest/gtest.h>

#include "ghost_oracle_harness.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace ghost_oracle;

namespace {

/** Sets the seam for one scope and restores the shipped constants on exit. */
struct Mix {
    explicit Mix(bool blendOn) { xpe_ghost_tier3_mix = blendOn ? XpeGhostTier3Mix{0.7f, 0.3f} : XpeGhostTier3Mix{1.0f, 0.0f}; }
    ~Mix() { xpe_ghost_tier3_mix = XpeGhostTier3Mix{0.7f, 0.3f}; }
};

Frames runTier(int tier, const Lag& l, const Frames& in) {
    int failures = 0;
    const Frames out = runModule(lagCfg(tier, l).c_str(), in, &failures);
    EXPECT_EQ(0, failures);
    return out;
}

// The preregistered rules (preregistration section 4) as functions, so that they can be tested on synthetic inputs.
/** K1, one row: +1 = blend worse (|residual| larger by at least 1.0 point), -1 = blend better (smaller by at least 1.0 point), 0 = no difference. */
int k1Row(double residualBlend, double residualNoBlend) {
    const double d = std::fabs(residualBlend) - std::fabs(residualNoBlend);
    return d >= 1.0 ? 1 : (d <= -1.0 ? -1 : 0);
}
bool k1BlendWorse(int better, int worse) { return better == 0 && worse >= 6; }
bool k1BlendBetter(int better, int worse) { return better >= 6 && worse == 0; }
/** K2, one configuration: the ring step (percent of the border mean) is larger with the blend by at least 0.1. */
bool k2BlendWorse(double stepBlend, double stepNoBlend) { return std::fabs(stepBlend) - std::fabs(stepNoBlend) >= 0.1; }
/** K3: both the stripes and the checkerboard response are lower with the blend by more than 0.02. */
bool k3BlendWorse(const double* blend, const double* noBlend) { return blend[0] < noBlend[0] - 0.02 && blend[1] < noBlend[1] - 0.02; }
/** K4: the noise ratio with the blend is below 0.9 times the one without. */
bool k4BlendBetter(double blend, double noBlend) { return blend < 0.9 * noBlend; }
/** The overall label: A = no measurement for the blend and a cost on K2 and K3; B = K1 for the blend; C = otherwise. */
char labelOf(bool k1Better, bool k2Worse, bool k3Worse) { return (!k1Better && k2Worse && k3Worse) ? 'A' : (k1Better ? 'B' : 'C'); }

struct NamedLag { std::string name; Lag lag; };

std::vector<NamedLag> configs() {
    std::vector<NamedLag> v;
    v.push_back({"A 0.02/3/0.003/30", Lag{0.02, 3.0, 0.003, 30.0}});
    for (const bool trap : {false, true}) {
        const Seq s = exposureSeq(0.27);
        const auto y = trap ? truthTrap(s.x) : truthLti(s.x);
        v.push_back({trap ? "C derived T2@27 +-0.1" : "B derived T1@27 +-0.1", deriveLag(s, y, Band{0.999, 1.001})});
    }
    return v;
}

struct Ring { double border{0}, interior{0}; };

Ring ringOf(const Frame& f) {
    Ring r;
    double sb = 0.0, si = 0.0;
    size_t nb = 0, ni = 0;
    for (uint32_t y = 0; y < H; ++y)
        for (uint32_t x = 0; x < W; ++x) {
            const bool border = (x == 0 || y == 0 || x == W - 1 || y == H - 1);
            (border ? sb : si) += f[y * W + x];
            ++(border ? nb : ni);
        }
    r.border = sb / static_cast<double>(nb);
    r.interior = si / static_cast<double>(ni);
    return r;
}

/** Nyquist response on the interior of a FIRST frame (zero history, so the correction is the identity before the blend). */
double nyquist(bool checker, const Lag& l) {
    const double level = 10000.0, amp = 1000.0;
    Frame f(N);
    for (uint32_t y = 0; y < H; ++y)
        for (uint32_t x = 0; x < W; ++x) {
            const int sign = checker ? (((x + y) & 1u) ? -1 : 1) : ((x & 1u) ? -1 : 1);
            f[y * W + x] = static_cast<float>(level + sign * amp);
        }
    const Frames out = runTier(3, l, Frames{f});
    double sum = 0.0;
    size_t n = 0;
    for (uint32_t y = 1; y + 1 < H; ++y)
        for (uint32_t x = 1; x + 1 < W; ++x, ++n) {
            const int sign = checker ? (((x + y) & 1u) ? -1 : 1) : ((x & 1u) ? -1 : 1);
            sum += sign * (out[0][y * W + x] - level) / amp;
        }
    return sum / static_cast<double>(n);
}

/** Interior standard deviation ratio (output / input) of a flat noisy FIRST frame, pooled over 100 independent frames. */
double noiseRatio(const Lag& l) {
    const double level = 10000.0;
    double sin2 = 0.0, sout2 = 0.0;
    for (unsigned seed = 1; seed <= 100; ++seed) {
        const Frames in = makeFrames({level}, 5.0, seed);
        const Frames out = runTier(3, l, in);
        for (uint32_t y = 1; y + 1 < H; ++y)
            for (uint32_t x = 1; x + 1 < W; ++x) {
                const double a = in[0][y * W + x] - level, b = out[0][y * W + x] - level;
                sin2 += a * a;
                sout2 += b * b;
            }
    }
    return std::sqrt(sout2 / sin2);
}

/** The preregistered M-noise value: ONE flat noisy first frame (mean 10000, sigma 5, seed 7), interior std ratio output/input. */
double noiseRatioSeed7(const Lag& l) {
    const double level = 10000.0;
    const Frames in = makeFrames({level}, 5.0, 7);
    const Frames out = runTier(3, l, in);
    double sin2 = 0.0, sout2 = 0.0;
    for (uint32_t y = 1; y + 1 < H; ++y)
        for (uint32_t x = 1; x + 1 < W; ++x) {
            const double a = in[0][y * W + x] - level, b = out[0][y * W + x] - level;
            sin2 += a * a;
            sout2 += b * b;
        }
    return std::sqrt(sout2 / sin2);
}

}  // namespace

TEST(GhostMixCompare, TheSeamDefaultsAreTheShippedConstantsAndTouchOnlyTierThree) {
    EXPECT_EQ(0.7f, xpe_ghost_tier3_mix.keep);
    EXPECT_EQ(0.3f, xpe_ghost_tier3_mix.local);
    const uint64_t d = tierThreeDigest();
    EXPECT_NE(0ull, d);
    std::printf("[ghost-oracle] tier3-digest 0x%016llx\n", static_cast<unsigned long long>(d));

    // tiers 1 and 2 do not read the seam: their output is bit-identical with the blend on and off
    const Seq s = exposureSeq(0.27);
    const auto y = makeFrames(truthLti(s.x), 5.0, 7);
    const Lag l{0.02, 3.0, 0.003, 30.0};
    for (const int tier : {1, 2}) {
        Frames on, off;
        { Mix m(true); on = runTier(tier, l, y); }
        { Mix m(false); off = runTier(tier, l, y); }
        EXPECT_TRUE(sameBytes(on, off)) << "tier " << tier;
    }
    // and tier 3 does: the two runs differ
    Frames on3, off3;
    { Mix m(true); on3 = runTier(3, l, y); }
    { Mix m(false); off3 = runTier(3, l, y); }
    EXPECT_FALSE(sameBytes(on3, off3));
    EXPECT_EQ(d, digestOf(on3)) << "the default mix reproduces the digest";
    EXPECT_EQ(0.7f, xpe_ghost_tier3_mix.keep) << "the guard restored the defaults";
}

TEST(GhostMixCompare, TheAnalyticValuesOfThePreregistrationHold) {
    const Lag l{0.02, 3.0, 0.003, 30.0};
    for (const bool blend : {true, false}) {
        Mix m(blend);
        const double stripes = nyquist(false, l), checker = nyquist(true, l), noise = noiseRatio(l);
        std::printf("[ghost-oracle-m4] analytic check blend=%d: stripes %.5f (predicted %.4f) checkerboard %.5f (predicted %.4f) noise ratio %.4f (predicted %.4f)\n",
                    blend ? 1 : 0, stripes, blend ? 0.6 : 1.0, checker, blend ? 0.7333333 : 1.0, noise, blend ? 0.7393691 : 1.0);
        // The preregistration lists 0.8 for the stripes; that value is WRONG (erratum in the M4 report): in a column-alternating
        // pattern the two neighbouring columns of a column have the OPPOSITE sign, so the 3x3 mean is -A/3 and the response is
        // (1-m) - m/3 = 0.6 for m = 0.3. The checkerboard (1/9) and the noise ratio were right. K3 compares the two runs, not this value.
        EXPECT_NEAR(blend ? 0.6 : 1.0, stripes, 1e-3);
        EXPECT_NEAR(blend ? 0.7333333 : 1.0, checker, 1e-3);
        EXPECT_NEAR(blend ? 0.7393691 : 1.0, noise, 0.03);
    }
}

TEST(GhostMixCompare, TheRingStepIsTheMixTimesTheRemovedLag) {
    // uniform frame (T1 27 %, no noise): interior = keep*border + local*raw  =>  step = local*(raw - border)
    const Seq s = exposureSeq(0.27);
    const auto yv = truthLti(s.x);
    const auto y = makeFrames(yv, 0.0, 0);
    const size_t i = s.nPre + 150;
    for (const auto& c : configs()) {
        for (const bool blend : {true, false}) {
            Mix m(blend);
            const Frames out = runTier(3, c.lag, y);
            const Ring r = ringOf(out[i]);
            const double raw = meanOf(y[i]);
            const double predicted = (blend ? 0.7 : 1.0) * r.border + (blend ? 0.3 : 0.0) * raw;
            std::printf("[ghost-oracle-m4] ring %-24s blend=%d: border %.2f interior %.2f step %+.2f ADU (%+.3f%% of the border) | predicted interior %.2f\n",
                        c.name.c_str(), blend ? 1 : 0, r.border, r.interior, r.interior - r.border,
                        100.0 * (r.interior - r.border) / r.border, predicted);
            EXPECT_NEAR(predicted, r.interior, 0.05) << c.name;
            if (!blend) EXPECT_NEAR(r.border, r.interior, 0.05) << c.name << ": no blend, no ring";
        }
    }
}

TEST(GhostMixCompare, ThePreregisteredRulesGiveTheirVerdictsOnSyntheticInputs) {
    // K1 per row: the thresholds are inclusive at 1.0 point
    EXPECT_EQ(1, k1Row(-12.0, -11.0));    // |.| larger by exactly 1.0 -> blend worse
    EXPECT_EQ(-1, k1Row(-11.0, -12.0));   // smaller by exactly 1.0 -> blend better
    EXPECT_EQ(0, k1Row(-11.5, -12.0));    // 0.5 -> no difference
    EXPECT_EQ(0, k1Row(2.0, -2.5));       // the sign does not matter, the size does
    // K1 over the rows: 6 and 0 decide, 5 do not, a single opposite row makes it mixed
    EXPECT_TRUE(k1BlendWorse(0, 6));
    EXPECT_FALSE(k1BlendWorse(0, 5));
    EXPECT_FALSE(k1BlendWorse(1, 11));
    EXPECT_TRUE(k1BlendBetter(6, 0));
    EXPECT_FALSE(k1BlendBetter(5, 0));
    EXPECT_FALSE(k1BlendBetter(6, 2));   // the situation of the first run: 6 better and 2 worse is not "better"
    // K2: 0.1 percent of the border mean, inclusive
    EXPECT_TRUE(k2BlendWorse(0.6, 0.0));
    EXPECT_TRUE(k2BlendWorse(-0.1, 0.0));
    EXPECT_FALSE(k2BlendWorse(0.05, 0.0));
    // K3: both patterns, strictly more than 0.02 below
    const double on[2] = {0.6, 0.7333}, off[2] = {1.0, 1.0}, nearOn[2] = {0.99, 0.99}, oneOnly[2] = {0.6, 0.99};
    EXPECT_TRUE(k3BlendWorse(on, off));
    EXPECT_FALSE(k3BlendWorse(nearOn, off));
    EXPECT_FALSE(k3BlendWorse(oneOnly, off));
    // K4: more than 10 percent
    EXPECT_TRUE(k4BlendBetter(0.74, 1.0));
    EXPECT_FALSE(k4BlendBetter(0.95, 1.0));
    EXPECT_FALSE(k4BlendBetter(0.9, 1.0));   // exactly 0.9 times is not below
    // the label
    EXPECT_EQ('A', labelOf(false, true, true));
    EXPECT_EQ('B', labelOf(true, true, true));
    EXPECT_EQ('B', labelOf(true, false, false));
    EXPECT_EQ('C', labelOf(false, true, false));
    EXPECT_EQ('C', labelOf(false, false, true));
    EXPECT_EQ('C', labelOf(false, false, false));
}

// ---- the comparison, exactly as the preregistration fixes it (printed, none of it asserted) --------------------------

TEST(GhostMixCompare, ComparesTheBlendKeptAndRemovedAsPreregistered) {
    const auto cfgs = configs();
    for (const auto& c : cfgs)
        std::printf("[ghost-oracle-m4] config %-24s alpha1 %.6g tau1 %.6g alpha2 %.6g tau2 %.6g S %.4f\n", c.name.c_str(), c.lag.a1, c.lag.t1, c.lag.a2, c.lag.t2, steadyGain(c.lag));

    // K1: the falling step (12 rows), signed residual in % of the new level
    int better = 0, worse = 0, equal = 0, rows = 0;
    for (const auto& c : cfgs)
        for (const bool trap : {false, true}) {
            const Seq s = fallingStepSeq(0.27, 0.05, 200, 100);
            const auto y = makeFrames(trap ? truthTrap(s.x) : truthLti(s.x), 0.0, 0);
            Step on, off;
            { Mix m(true); on = measureStep(s, y, runTier(3, c.lag, y)); }
            { Mix m(false); off = measureStep(s, y, runTier(3, c.lag, y)); }
            const double res[2][2] = {{on.corRes1, off.corRes1}, {on.corRes50, off.corRes50}};
            for (int k = 0; k < 2; ++k) {
                const double delta = std::fabs(res[k][0]) - std::fabs(res[k][1]);
                const int row = k1Row(res[k][0], res[k][1]);
                const char* verdict = row > 0 ? "blend worse" : (row < 0 ? "blend better" : "no difference");
                if (row > 0) ++worse; else if (row < 0) ++better; else ++equal;
                ++rows;
                std::printf("[ghost-oracle-m4] K1 %-24s %s k=%-2d residual blend %+8.3f%% | no blend %+8.3f%% | delta %+7.3f pt -> %s\n",
                            c.name.c_str(), trap ? "T2" : "T1", k == 0 ? 1 : 50, res[k][0], res[k][1], delta, verdict);
            }
        }
    const bool k1Worse = k1BlendWorse(better, worse), k1Better = k1BlendBetter(better, worse);

    // K2: the ring step on a uniform exposure frame, % of the border mean
    int ringWorse = 0;
    {
        const Seq s = exposureSeq(0.27);
        const auto y = makeFrames(truthLti(s.x), 0.0, 0);
        const size_t i = s.nPre + 150;
        for (const auto& c : cfgs) {
            double step[2];
            for (int b = 0; b < 2; ++b) {
                Mix m(b == 0);
                const Ring r = ringOf(runTier(3, c.lag, y)[i]);
                step[b] = 100.0 * (r.interior - r.border) / r.border;
            }
            const bool bad = k2BlendWorse(step[0], step[1]);
            if (bad) ++ringWorse;
            std::printf("[ghost-oracle-m4] K2 %-24s ring step blend %+8.3f%% | no blend %+8.3f%% -> %s\n", c.name.c_str(), step[0], step[1], bad ? "blend worse" : "no difference");
        }
    }
    const bool k2Worse = (ringWorse == static_cast<int>(cfgs.size()));

    // K3: the Nyquist response of a first frame
    double nyq[2][2];   // [blend on/off][stripes, checker]
    for (int b = 0; b < 2; ++b) {
        Mix m(b == 0);
        nyq[b][0] = nyquist(false, cfgs[0].lag);
        nyq[b][1] = nyquist(true, cfgs[0].lag);
    }
    const bool k3Worse = k3BlendWorse(nyq[0], nyq[1]);
    std::printf("[ghost-oracle-m4] K3 Nyquist response: stripes blend %.4f | no blend %.4f ; checkerboard blend %.4f | no blend %.4f ; reference 0.98 (NFR-206, requirement status unverified): blend %s, no blend %s\n",
                nyq[0][0], nyq[1][0], nyq[0][1], nyq[1][1],
                (nyq[0][0] >= 0.98 && nyq[0][1] >= 0.98) ? "meets" : "below", (nyq[1][0] >= 0.98 && nyq[1][1] >= 0.98) ? "meets" : "below");

    // K4: noise
    // the preregistered value is the single seed-7 frame; the pooled value (100 frames) is printed beside it because one 196-pixel
    // frame has a sampling error of a few percent (the analytic assertion uses the pooled one; the K4 rule uses the seed-7 frame)
    double nz[2], nzPooled[2];
    for (int b = 0; b < 2; ++b) { Mix m(b == 0); nz[b] = noiseRatioSeed7(cfgs[0].lag); nzPooled[b] = noiseRatio(cfgs[0].lag); }
    const bool k4Better = k4BlendBetter(nz[0], nz[1]);
    std::printf("[ghost-oracle-m4] K4 interior noise ratio output/input (seed-7 frame, as preregistered): blend %.4f | no blend %.4f -> %s ; pooled over 100 frames: blend %.4f | no blend %.4f\n",
                nz[0], nz[1], k4Better ? "blend better" : "no difference", nzPooled[0], nzPooled[1]);

    // reported, not judged: the E2E blank-frame removal and the retention, the rows of M1 on the tier-3 corrector
    for (const auto& c : cfgs)
        for (const bool trap : {false, true})
            for (const double level : {0.02, 0.27, 0.92})
                for (const double sigma : {0.0, 5.0}) {
                    const Seq s = exposureSeq(level);
                    const auto y = makeFrames(trap ? truthTrap(s.x) : truthLti(s.x), sigma, 7);
                    char lv[16];
                    std::snprintf(lv, sizeof(lv), "%.0f%%", level * 100.0);
                    for (const bool b : {true, false}) {
                        Mix m(b);
                        printRow(trap ? "T2" : "T1", lv, sigma, std::string("m4 tier3 ") + (b ? "blend    " : "no-blend ") + c.name, measure(s, y, runTier(3, c.lag, y)));
                    }
                }

    const char* k1 = k1Worse ? "blend worse" : (k1Better ? "blend better" : "mixed or no difference");
    const char label[2] = {labelOf(k1Better, k2Worse, k3Worse), '\0'};
    std::printf("[ghost-oracle-m4] K1 rows: blend better %d, blend worse %d, no difference %d (of %d) => %s\n", better, worse, equal, rows, k1);
    std::printf("[ghost-oracle-m4] K2 => %s | K3 => %s | K4 => %s\n", k2Worse ? "blend worse" : "mixed or no difference", k3Worse ? "blend worse" : "mixed or no difference",
                k4Better ? "blend better" : "no difference");
    std::printf("[ghost-oracle-m4] LABEL %s (preregistration section 4; measurement summary of these inputs, not a decision on #238)\n", label);
    SUCCEED();
}
