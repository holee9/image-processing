/**
 * @file test_bpm_lines_lof.cpp
 * @brief Validation of the line detector's LINEARITY cause against a generator that does not read the injection truth (QA-A-258, #273).
 *
 * The statistic is defined in .moai/reports/lane-pre/QA-A-258/evidence/10_definition_predictions.md.txt (committed before this file): the lack of fit of a straight line through the dose-group
 * means, weighted by the nominal variance of the means and divided by the pixel's own pooled temporal variance. These tests build their own frames from a mean response f(S) and a temporal
 * variance v(S) per region, and compute the expected misfit with an independent method (Gram-Schmidt on the weighted inner product, not the closed form of the implementation):
 *   - negative controls: the mean response is a straight line (expected misfit 0) while gain, offset, quantum efficiency and the shape of the noise change; a persistent LINEARITY group on such a
 *     line is a false alarm;
 *   - positive controls: the injection's nonlinearity f(S) = S (1 - c S / 30000), c = 0.4 and 0.2, at three noise levels, as a full row, a full column, a partial row, a pair of rows and a crossing;
 *   - separation of the axes: linear + high noise is a NOISE defect, nonlinear + low noise is a LINEARITY defect.
 * The budgets and the expected detection power are fixed in the definition file before any of these runs.
 */

#include <gtest/gtest.h>

#include "xpe_bpm_v2_internal.hpp"
#include "xpe/preprocess_api.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kW = 256, kH = 256;
constexpr size_t   kN = static_cast<size_t>(kW) * kH;
constexpr double   kDark = 3000.0, kV0 = 9.0, kKappa = 0.3, kRef = 30000.0;
constexpr double   kSignals[6] = {2836.65, 5871.31, 9193.33, 18297.17, 29178.97, 38000.0};   // the first five are the dose groups of the evaluation injection (the report of QA-A-252i); the sixth is only for designs of six groups

class Normal {
public:
    explicit Normal(uint64_t seed) : s_(seed * 2862933555777941757ull + 3037000493ull) {}
    double next() {
        const double u1 = uniform(), u2 = uniform();
        return std::sqrt(-2.0 * std::log(u1)) * std::cos(6.283185307179586 * u2);
    }

private:
    double uniform() {
        s_ = s_ * 6364136223846793005ull + 1442695040888963407ull;
        return (static_cast<double>(s_ >> 11) + 0.5) / 9007199254740992.0;
    }
    uint64_t s_;
};

/** What a region of the frame does to the nominal response and noise. mean(S) = gain S (1 - curve S / 30000) + offset; var(S) = varRead v0 + varShot kappa S (1 + varShotQuad S). */
struct Mod {
    double gain{1.0}, offset{0.0}, curve{0.0};
    double varRead{1.0}, varShot{1.0}, varShotQuad{0.0};
    double darkNoise{1.0};   // multiplies the readout sd of the dark frames
};

double meanOf(const Mod& m, double s) { return m.gain * s * (1.0 - m.curve * s / kRef) + m.offset; }
double varOf(const Mod& m, double s) { return m.varRead * kV0 + m.varShot * kKappa * s * (1.0 + m.varShotQuad * s); }

// the forms of the controls
Mod efficiency(double r) { Mod m; m.gain = r; m.varRead = r * r; m.varShot = r; return m; }               // quantum efficiency r: signal r S, variance r^2 v0 + kappa r S
Mod gainLoss(double r) { Mod m; m.gain = r; m.varRead = r * r; m.varShot = r * r; return m; }             // conversion gain r: the whole value scaled
Mod brightOffset(double b) { Mod m; m.offset = b; return m; }
Mod heteroscedastic() { Mod m; m.varShotQuad = 1.0 / kRef; return m; }                                    // a different dependence of the noise on the dose
Mod nonlinear(double c, double kappaMul) { Mod m; m.curve = c; m.varShot = kappaMul; return m; }
Mod noisyLinear() { Mod m; m.varRead = 16.0; m.varShot = 4.0; m.darkNoise = 4.0; return m; }                // linear, four times the noise sd everywhere

struct Region {
    int x0, x1, y0, y1;   // [x0, x1) x [y0, y1)
    Mod mod;
};

struct Stack {
    std::vector<std::vector<uint16_t>> dark, bright;
};

/** The frames: 8 raw dark frames (the detector skips the first) and the bright frames of the dose groups (sizes[g] frames at kSignals[g]). */
Stack makeStack(uint64_t seed, const std::vector<int>& sizes, const std::vector<Region>& regions) {
    Normal rng(seed);
    std::vector<int16_t> modOf(kN, -1);
    for (size_t r = 0; r < regions.size(); ++r)
        for (int y = regions[r].y0; y < regions[r].y1; ++y)
            for (int x = regions[r].x0; x < regions[r].x1; ++x) modOf[static_cast<size_t>(y) * kW + static_cast<size_t>(x)] = static_cast<int16_t>(r);
    const Mod base;
    auto mod = [&](size_t i) -> const Mod& { return modOf[i] < 0 ? base : regions[static_cast<size_t>(modOf[i])].mod; };
    auto clamp16 = [](double v) { return static_cast<uint16_t>(std::min(65535.0, std::max(0.0, std::floor(v + 0.5)))); };
    Stack s;
    for (int k = 0; k < 8; ++k) {
        std::vector<uint16_t> f(kN);
        for (size_t i = 0; i < kN; ++i) f[i] = clamp16(kDark + std::sqrt(kV0) * mod(i).darkNoise * rng.next());
        s.dark.push_back(std::move(f));
    }
    for (size_t g = 0; g < sizes.size(); ++g)
        for (int k = 0; k < sizes[g]; ++k) {
            std::vector<uint16_t> f(kN);
            for (size_t i = 0; i < kN; ++i) {
                const Mod& m = mod(i);
                f[i] = clamp16(kDark + meanOf(m, kSignals[g]) + std::sqrt(varOf(m, kSignals[g])) * rng.next());
            }
            s.bright.push_back(std::move(f));
        }
    return s;
}

XpeImageBuffer buf(std::vector<uint16_t>& v) {
    XpeImageBuffer b{};
    b.data = v.data();
    b.width = kW;
    b.height = kH;
    b.bitsAllocated = 16;
    b.bitsStored = 16;
    b.format = XPE_PIXEL_UINT16;
    b.dataSize = v.size() * sizeof(uint16_t);
    return b;
}

struct Found {
    std::vector<XpeBpmLine> lines;
    XpeBpmLineReport        rep{};
    XpeErrorCode            rc{XPE_OK};
};

Found detect(Stack& s) {
    std::vector<XpeImageBuffer> db, bb;
    for (auto& v : s.dark) db.push_back(buf(v));
    for (auto& v : s.bright) bb.push_back(buf(v));
    Found f;
    f.lines.assign(256, XpeBpmLine{});
    XpeBpmLineConfig lc{};
    lc.min_segment = 32;   // the test frames are 256 pixels wide
    EXPECT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    f.rc = xpe_bpm_find_lines(db.data(), static_cast<uint32_t>(db.size()), bb.data(), static_cast<uint32_t>(bb.size()), nullptr, &lc, nullptr, f.lines.data(), 256, nullptr, 0, &f.rep);
    xpe_clear_alerts();
    xpe_preprocess_shutdown();
    f.lines.resize(std::min<size_t>(f.rep.lines, 256));
    return f;
}

/** True when some group with one of the cause bits covers the line `idx` of the orientation over at least `fraction` of [from, to). */
bool covered(const Found& f, uint32_t orientation, int idx, int from, int to, uint32_t causeBits, double fraction) {
    for (const XpeBpmLine& l : f.lines) {
        if (l.orientation != orientation || !(l.causes & causeBits)) continue;
        if (static_cast<int>(l.index) > idx || idx >= static_cast<int>(l.index + l.width)) continue;
        const int lo = std::max(static_cast<int>(l.start), from), hi = std::min(static_cast<int>(l.end), to);
        if (hi - lo >= static_cast<int>(std::ceil(fraction * (to - from)))) return true;
    }
    return false;
}

int countCause(const Found& f, uint32_t causeBit) {
    int n = 0;
    for (const XpeBpmLine& l : f.lines) n += (l.causes & causeBit) ? 1 : 0;
    return n;
}

/**
 * The expected misfit of a straight line through the noiseless group means f(S_g), with the weights m_g / v(S_g): the squared norm of the part of f that is orthogonal to {1, S} under the weighted inner
 * product, found by Gram-Schmidt. This is the non-centrality of the lack-of-fit statistic. It does not use the closed form of the implementation.
 */
double expectedMisfit(const Mod& m, const std::vector<int>& sizes) {
    const size_t G = sizes.size();
    std::vector<double> w(G), f(G), one(G, 1.0), x(G);
    for (size_t g = 0; g < G; ++g) {
        w[g] = static_cast<double>(sizes[g]) / varOf(m, kSignals[g]);
        f[g] = meanOf(m, kSignals[g]);
        x[g] = kSignals[g];
    }
    auto dot = [&](const std::vector<double>& a, const std::vector<double>& b) {
        double s = 0.0;
        for (size_t g = 0; g < G; ++g) s += w[g] * a[g] * b[g];
        return s;
    };
    std::vector<double> q1 = one;
    const double n1 = std::sqrt(dot(q1, q1));
    for (double& v : q1) v /= n1;
    std::vector<double> r2 = x;
    const double c21 = dot(x, q1);
    for (size_t g = 0; g < G; ++g) r2[g] -= c21 * q1[g];
    const double n2 = std::sqrt(dot(r2, r2));
    for (double& v : r2) v /= n2;
    const double a1 = dot(f, q1), a2 = dot(f, r2);
    std::vector<double> res(G);
    for (size_t g = 0; g < G; ++g) res[g] = f[g] - a1 * q1[g] - a2 * r2[g];
    return dot(res, res);
}

const std::vector<int> kDesignA = {3, 3, 6, 3, 3};
const std::vector<int> kDesignB = {2, 3, 4, 5, 6};

}  // namespace

TEST(BpmLinesLofOracleTest, TheExpectedMisfitOfAStraightLineIsZeroForAnyNoiseAndTheCurveGivesTheNumbersOfTheDefinition) {
    for (const std::vector<int>* d : {&kDesignA, &kDesignB}) {
        EXPECT_NEAR(0.0, expectedMisfit(Mod{}, *d), 1e-6);
        EXPECT_NEAR(0.0, expectedMisfit(efficiency(0.6), *d), 1e-6);
        EXPECT_NEAR(0.0, expectedMisfit(gainLoss(0.7), *d), 1e-6);
        EXPECT_NEAR(0.0, expectedMisfit(brightOffset(150.0), *d), 1e-6);
        EXPECT_NEAR(0.0, expectedMisfit(heteroscedastic(), *d), 1e-6);
        EXPECT_NEAR(0.0, expectedMisfit(noisyLinear(), *d), 1e-6);
    }
    // the numbers of the definition (computed independently with numpy before this file was written)
    EXPECT_NEAR(16834.9, expectedMisfit(nonlinear(0.4, 0.25), kDesignA), 1.0);
    EXPECT_NEAR(4259.3, expectedMisfit(nonlinear(0.4, 1.0), kDesignA), 1.0);
    EXPECT_NEAR(1068.0, expectedMisfit(nonlinear(0.4, 4.0), kDesignA), 1.0);
    EXPECT_NEAR(4208.7, expectedMisfit(nonlinear(0.2, 0.25), kDesignA), 1.0);
    EXPECT_NEAR(1064.8, expectedMisfit(nonlinear(0.2, 1.0), kDesignA), 1.0);
    EXPECT_NEAR(267.0, expectedMisfit(nonlinear(0.2, 4.0), kDesignA), 1.0);
    // the curvature is not absorbed by more noise: the misfit falls in proportion to the variance and stays far above the degrees of freedom (G - 2 = 3)
    EXPECT_GT(expectedMisfit(nonlinear(0.2, 4.0), kDesignA), 50.0 * 3.0);
}

namespace {

struct NegativeResult {
    int         runs{0}, runsWithFalseLinearity{0}, linearityGroups{0};
    int         perForm[5] = {0, 0, 0, 0, 0};
    std::string where;
};

/** Five forms x two dose designs x `seeds` seeds; each image carries one full row (form f) and one full column (form f + 1). The seeds 0..seeds-1 of the full run are the first seeds of any shorter run. */
NegativeResult runNegative(int seeds) {
    const Mod   forms[5] = {efficiency(0.6), efficiency(0.44), gainLoss(0.7), brightOffset(150.0), heteroscedastic()};
    const char* names[5] = {"efficiency 0.6", "efficiency 0.44", "gain 0.7", "bright offset", "heteroscedastic"};
    constexpr int kRowY = 100, kColX = 150;
    NegativeResult res;
    for (int design = 0; design < 2; ++design)
        for (int f = 0; f < 5; ++f)
            for (int seed = 0; seed < seeds; ++seed) {
                const std::vector<int>& sizes = design == 0 ? kDesignA : kDesignB;
                Stack st = makeStack(1000u + static_cast<uint64_t>(design * 100 + f * 10 + seed), sizes, {{0, static_cast<int>(kW), kRowY, kRowY + 1, forms[f]}, {kColX, kColX + 1, 0, static_cast<int>(kH), forms[(f + 1) % 5]}});
                const Found r = detect(st);
                EXPECT_EQ(XPE_OK, r.rc);
                ++res.runs;
                res.linearityGroups += countCause(r, XPE_BPM_LINE_CAUSE_LINEARITY);
                const bool rowFalse = covered(r, XPE_BPM_LINE_ROW, kRowY, 0, static_cast<int>(kW), XPE_BPM_LINE_CAUSE_LINEARITY, 0.1);
                const bool colFalse = covered(r, XPE_BPM_LINE_COLUMN, kColX, 0, static_cast<int>(kH), XPE_BPM_LINE_CAUSE_LINEARITY, 0.1);
                if (rowFalse || colFalse) {
                    ++res.runsWithFalseLinearity;
                    if (rowFalse) ++res.perForm[f];
                    if (colFalse) ++res.perForm[(f + 1) % 5];
                    res.where += std::string(" [design ") + (design == 0 ? "A" : "B") + " seed " + std::to_string(seed) + (rowFalse ? std::string(" row=") + names[f] : std::string()) +
                                 (colFalse ? std::string(" col=") + names[(f + 1) % 5] : std::string()) + "]";
                }
            }
    return res;
}

struct PositiveResult {
    int         total{0}, hits{0};
    int         perCombo[2][3][5] = {};
    std::string misses;
};

/** c in {0.4, 0.2} x noise {0.25, 1, 4} x five shapes x `seeds` seeds. */
PositiveResult runPositive(int seeds) {
    const double cs[2] = {0.4, 0.2}, kms[3] = {0.25, 1.0, 4.0};
    const char*  shapeNames[5] = {"row_full", "col_full", "row_partial", "row_pair", "cross"};
    PositiveResult res;
    for (int ci = 0; ci < 2; ++ci)
        for (int ki = 0; ki < 3; ++ki)
            for (int sh = 0; sh < 5; ++sh)
                for (int seed = 0; seed < seeds; ++seed) {
                    const Mod m = nonlinear(cs[ci], kms[ki]);
                    std::vector<Region> reg;
                    switch (sh) {
                    case 0: reg = {{0, 256, 100, 101, m}}; break;
                    case 1: reg = {{150, 151, 0, 256, m}}; break;
                    case 2: reg = {{80, 176, 60, 61, m}}; break;
                    case 3: reg = {{0, 256, 180, 182, m}}; break;
                    default: reg = {{0, 256, 120, 121, m}, {70, 71, 0, 256, m}}; break;
                    }
                    Stack st = makeStack(5000u + static_cast<uint64_t>(ci * 1000 + ki * 100 + sh * 10 + seed), kDesignA, reg);
                    const Found r = detect(st);
                    EXPECT_EQ(XPE_OK, r.rc);
                    bool ok = false;
                    switch (sh) {
                    case 0: ok = covered(r, XPE_BPM_LINE_ROW, 100, 0, 256, XPE_BPM_LINE_CAUSE_LINEARITY, 0.9); break;
                    case 1: ok = covered(r, XPE_BPM_LINE_COLUMN, 150, 0, 256, XPE_BPM_LINE_CAUSE_LINEARITY, 0.9); break;
                    case 2: ok = covered(r, XPE_BPM_LINE_ROW, 60, 80, 176, XPE_BPM_LINE_CAUSE_LINEARITY, 0.9); break;
                    case 3: ok = covered(r, XPE_BPM_LINE_ROW, 180, 0, 256, XPE_BPM_LINE_CAUSE_LINEARITY, 0.9) && covered(r, XPE_BPM_LINE_ROW, 181, 0, 256, XPE_BPM_LINE_CAUSE_LINEARITY, 0.9); break;
                    default:
                        ok = covered(r, XPE_BPM_LINE_ROW, 120, 0, 256, XPE_BPM_LINE_CAUSE_LINEARITY, 0.9) && covered(r, XPE_BPM_LINE_COLUMN, 70, 0, 256, XPE_BPM_LINE_CAUSE_LINEARITY, 0.9);
                        break;
                    }
                    ++res.total;
                    res.hits += ok ? 1 : 0;
                    res.perCombo[ci][ki][sh] += ok ? 1 : 0;
                    if (!ok) res.misses += std::string(" [c ") + std::to_string(cs[ci]).substr(0, 3) + " x" + std::to_string(kms[ki]).substr(0, 4) + " " + shapeNames[sh] + " seed " + std::to_string(seed) + "]";
                }
    return res;
}

}  // namespace

// ---- The CI subset: the same forms with fewer seeds (2 per cell of the negative controls, 1 per cell of the positive controls), so that the suite stays short. The budgets scale with the number of runs
// and are fixed in the definition file (the amendment of QA-A-258 before the first run of the implementation).
TEST(BpmLinesLofNegativeTest, AStraightResponseWithMoreNoiseOrADifferentGainOrOffsetDoesNotMakeALinearityDefect) {
    const NegativeResult r = runNegative(2);
    EXPECT_EQ(20, r.runs);
    EXPECT_LE(r.runsWithFalseLinearity, 1) << "false LINEARITY on a straight-line response in 20 images: eff0.6 " << r.perForm[0] << ", eff0.44 " << r.perForm[1] << ", gain " << r.perForm[2] << ", offset "
                                           << r.perForm[3] << ", hetero " << r.perForm[4] << r.where;
    EXPECT_LE(r.linearityGroups, r.runs) << "LINEARITY groups anywhere (target: at most one per image)";
}

TEST(BpmLinesLofPositiveTest, ACurvedResponseIsALinearityDefectWhateverTheNoiseAndTheShape) {
    for (int ci = 0; ci < 2; ++ci)
        for (int ki = 0; ki < 3; ++ki) {
            const double kms[3] = {0.25, 1.0, 4.0};
            ASSERT_GT(expectedMisfit(nonlinear(ci == 0 ? 0.4 : 0.2, kms[ki]), kDesignA), 266.0) << "the design of the control: the expected misfit exceeds 266 >> G - 2 = 3 in every cell";
        }
    const PositiveResult r = runPositive(1);
    EXPECT_EQ(30, r.total);
    EXPECT_GE(r.hits, 29) << "the expected power: at least 29 of 30" << r.misses;
}

// ---- The full budgets of the definition (100 negative images, 120 positive images): disabled in the suite, run by hand (--gtest_also_run_disabled_tests) and in the measurement.
TEST(BpmLinesLofNegativeTest, DISABLED_FullBudgetOfOneHundredImages) {
    const NegativeResult r = runNegative(10);
    std::printf("[negative controls] images %d, images with a false LINEARITY group on a planted line %d (eff0.6 %d, eff0.44 %d, gain %d, offset %d, hetero %d), LINEARITY groups anywhere %d\n", r.runs,
                r.runsWithFalseLinearity, r.perForm[0], r.perForm[1], r.perForm[2], r.perForm[3], r.perForm[4], r.linearityGroups);
    EXPECT_EQ(100, r.runs);
    EXPECT_LE(r.runsWithFalseLinearity, 2) << "false LINEARITY by form: eff0.6 " << r.perForm[0] << ", eff0.44 " << r.perForm[1] << ", gain " << r.perForm[2] << ", offset " << r.perForm[3] << ", hetero " << r.perForm[4]
                                           << r.where;
    EXPECT_LE(r.linearityGroups, r.runs);
}

TEST(BpmLinesLofPositiveTest, DISABLED_FullPowerOfOneHundredTwentyImages) {
    const PositiveResult r = runPositive(4);
    int worst = 4;
    for (int ci = 0; ci < 2; ++ci)
        for (int ki = 0; ki < 3; ++ki)
            for (int sh = 0; sh < 5; ++sh) worst = std::min(worst, r.perCombo[ci][ki][sh]);
    std::printf("[positive controls] images %d, detected %d, weakest cell %d of 4 seeds\n", r.total, r.hits, worst);
    EXPECT_EQ(120, r.total);
    EXPECT_GE(r.hits, 114) << r.misses;
    for (int ci = 0; ci < 2; ++ci)
        for (int ki = 0; ki < 3; ++ki)
            for (int sh = 0; sh < 5; ++sh) EXPECT_GE(r.perCombo[ci][ki][sh], 3) << "cell c#" << ci << " noise#" << ki << " shape#" << sh;
}

TEST(BpmLinesLofSeparationTest, ALinearLineWithHighNoiseIsANoiseDefectAndANonlinearLineWithLowNoiseIsALinearityDefect) {
    int noisyLinearNoise = 0, noisyLinearLinearity = 0, curvedLowNoiseLinearity = 0, curvedLowNoiseNoise = 0;
    for (int seed = 0; seed < 4; ++seed) {
        {   // linear, four times the noise sd in the dark and the bright frames
            Stack st = makeStack(8000u + static_cast<uint64_t>(seed), kDesignA, {{0, 256, 100, 101, noisyLinear()}});
            const Found r = detect(st);
            ASSERT_EQ(XPE_OK, r.rc);
            noisyLinearNoise += covered(r, XPE_BPM_LINE_ROW, 100, 0, 256, XPE_BPM_LINE_CAUSE_NOISE, 0.9) ? 1 : 0;
            noisyLinearLinearity += covered(r, XPE_BPM_LINE_ROW, 100, 0, 256, XPE_BPM_LINE_CAUSE_LINEARITY, 0.1) ? 1 : 0;
        }
        {   // curved (c = 0.4), a quarter of the shot noise, normal readout noise
            Stack st = makeStack(8100u + static_cast<uint64_t>(seed), kDesignA, {{0, 256, 100, 101, nonlinear(0.4, 0.25)}});
            const Found r = detect(st);
            ASSERT_EQ(XPE_OK, r.rc);
            curvedLowNoiseLinearity += covered(r, XPE_BPM_LINE_ROW, 100, 0, 256, XPE_BPM_LINE_CAUSE_LINEARITY, 0.9) ? 1 : 0;
            curvedLowNoiseNoise += covered(r, XPE_BPM_LINE_ROW, 100, 0, 256, XPE_BPM_LINE_CAUSE_NOISE, 0.1) ? 1 : 0;
        }
    }
    EXPECT_GE(noisyLinearNoise, 3) << "linear + high noise: the NOISE cause in at least 3 of 4 seeds";
    EXPECT_EQ(0, noisyLinearLinearity) << "linear + high noise: no LINEARITY cause";
    EXPECT_GE(curvedLowNoiseLinearity, 3) << "curved + low noise: the LINEARITY cause in at least 3 of 4 seeds";
    EXPECT_EQ(0, curvedLowNoiseNoise) << "curved + low noise: no NOISE cause";
}

TEST(BpmLinesLofSeparationTest, AnotherAxisKeepsItsMeaning) {
    // a line whose noise is large but whose response is a straight line stays a NOISE defect (no regression of the noise axis), a line with a low response stays a RESPONSE/SLOPE defect
    int noise = 0, response = 0;
    for (int seed = 0; seed < 4; ++seed) {
        Stack a = makeStack(8200u + static_cast<uint64_t>(seed), kDesignA, {{0, 256, 100, 101, noisyLinear()}});
        const Found ra = detect(a);
        ASSERT_EQ(XPE_OK, ra.rc);
        noise += covered(ra, XPE_BPM_LINE_ROW, 100, 0, 256, XPE_BPM_LINE_CAUSE_NOISE, 0.9) ? 1 : 0;
        Stack b = makeStack(8300u + static_cast<uint64_t>(seed), kDesignA, {{0, 256, 100, 101, gainLoss(0.3)}});
        const Found rb = detect(b);
        ASSERT_EQ(XPE_OK, rb.rc);
        response += covered(rb, XPE_BPM_LINE_ROW, 100, 0, 256, XPE_BPM_LINE_CAUSE_RESPONSE | XPE_BPM_LINE_CAUSE_SLOPE, 0.9) ? 1 : 0;
    }
    EXPECT_GE(noise, 3);
    EXPECT_GE(response, 3);
}

// ---------------------------------------------------------------- the pure functions of the implementation, against an independent method
namespace {

/** The weighted residual sum of squares by Gram-Schmidt on the weighted inner product (not the normal equations of the implementation). */
double gramSchmidtResidual(const std::vector<double>& w, const std::vector<double>& x, const std::vector<double>& y) {
    const size_t G = x.size();
    auto dot = [&](const std::vector<double>& a, const std::vector<double>& b) {
        double s = 0.0;
        for (size_t g = 0; g < G; ++g) s += w[g] * a[g] * b[g];
        return s;
    };
    std::vector<double> q1(G, 1.0);
    const double        n1 = std::sqrt(dot(q1, q1));
    for (double& v : q1) v /= n1;
    std::vector<double> q2 = x;
    const double        c = dot(x, q1);
    for (size_t g = 0; g < G; ++g) q2[g] -= c * q1[g];
    const double n2 = std::sqrt(dot(q2, q2));
    for (double& v : q2) v /= n2;
    const double a1 = dot(y, q1), a2 = dot(y, q2);
    std::vector<double> res(G);
    for (size_t g = 0; g < G; ++g) res[g] = y[g] - a1 * q1[g] - a2 * q2[g];
    return dot(res, res);
}

xpe_bpm_v2_internal::LofSums sumsOf(const std::vector<double>& w, const std::vector<double>& x, const std::vector<double>& y) {
    xpe_bpm_v2_internal::LofSums s{0, 0, 0, 0, 0, 0};
    for (size_t g = 0; g < x.size(); ++g) {
        s.w0 += w[g];
        s.w1 += w[g] * x[g];
        s.w2 += w[g] * x[g] * x[g];
        s.wn += w[g] * y[g];
        s.wxn += w[g] * x[g] * y[g];
        s.wnn += w[g] * y[g] * y[g];
    }
    return s;
}

}  // namespace

TEST(BpmLinesLofFunctionTest, TheClosedFormMatchesGramSchmidtOnRandomGroupsAndAStraightLineHasNoMisfit) {
    Normal rng(4242);
    for (int t = 0; t < 1000; ++t) {
        const size_t        G = 3 + static_cast<size_t>(t % 6);
        std::vector<double> w(G), x(G), y(G);
        for (size_t g = 0; g < G; ++g) {
            x[g] = 1000.0 + 5000.0 * static_cast<double>(g) + 300.0 * rng.next();
            w[g] = 0.01 + std::exp(rng.next());
            y[g] = 40.0 + 0.8 * x[g] + 25.0 * rng.next() + (t % 2 == 0 ? 0.00002 * x[g] * x[g] : 0.0);
        }
        bool         ok = false;
        const double q = xpe_bpm_v2_internal::weighted_lack_of_fit(sumsOf(w, x, y), &ok);
        ASSERT_TRUE(ok);
        const double ref = gramSchmidtResidual(w, x, y);
        ASSERT_NEAR(ref, q, 1e-7 * std::max(1.0, ref)) << "t=" << t;
        // an exact straight line has no misfit
        std::vector<double> line(G);
        for (size_t g = 0; g < G; ++g) line[g] = 17.0 + 0.5 * x[g];
        bool ok2 = false;
        EXPECT_NEAR(0.0, xpe_bpm_v2_internal::weighted_lack_of_fit(sumsOf(w, x, line), &ok2), 1e-6 * sumsOf(w, x, line).wnn + 1e-9);
        EXPECT_TRUE(ok2);
    }
}

TEST(BpmLinesLofFunctionTest, DegenerateDosesAndNonFiniteSumsGiveNoEvidenceNotACurvature) {
    bool ok = true;
    // every group at the same dose: the determinant is zero
    std::vector<double> w = {1, 1, 1}, x = {5000, 5000, 5000}, y = {1, 2, 3};
    EXPECT_EQ(0.0, xpe_bpm_v2_internal::weighted_lack_of_fit(sumsOf(w, x, y), &ok));
    EXPECT_FALSE(ok);
    xpe_bpm_v2_internal::LofSums bad = sumsOf({1, 1, 1}, {1000, 2000, 3000}, {1, 2, 3});
    bad.wn = std::nan("");
    ok = true;
    EXPECT_EQ(0.0, xpe_bpm_v2_internal::weighted_lack_of_fit(bad, &ok));
    EXPECT_FALSE(ok);
    // the feature: no evidence (0) for a non-positive or non-finite pooled variance, a non-finite misfit or fewer than three groups
    EXPECT_EQ(0.0f, xpe_bpm_v2_internal::lack_of_fit_feature(10.0, 5, 0.0, 0.1));
    EXPECT_EQ(0.0f, xpe_bpm_v2_internal::lack_of_fit_feature(10.0, 5, -1.0, 0.1));
    EXPECT_EQ(0.0f, xpe_bpm_v2_internal::lack_of_fit_feature(10.0, 5, std::nan(""), 0.1));
    EXPECT_EQ(0.0f, xpe_bpm_v2_internal::lack_of_fit_feature(std::nan(""), 5, 1.0, 0.1));
    EXPECT_EQ(0.0f, xpe_bpm_v2_internal::lack_of_fit_feature(10.0, 2, 1.0, 0.1));
    // the floor of the pooled variance, the monotonicity, and a perfect straight line is finite (not minus infinity)
    EXPECT_EQ(xpe_bpm_v2_internal::lack_of_fit_feature(10.0, 5, 0.1, 0.1), xpe_bpm_v2_internal::lack_of_fit_feature(10.0, 5, 1e-9, 0.1)) << "a pooled variance below 0.1 of nominal is judged as 0.1";
    EXPECT_GT(xpe_bpm_v2_internal::lack_of_fit_feature(50.0, 5, 1.0, 0.1), xpe_bpm_v2_internal::lack_of_fit_feature(5.0, 5, 1.0, 0.1));
    EXPECT_LT(xpe_bpm_v2_internal::lack_of_fit_feature(10.0, 5, 4.0, 0.1), xpe_bpm_v2_internal::lack_of_fit_feature(10.0, 5, 1.0, 0.1)) << "the same misfit is weaker evidence in a noisier pixel";
    EXPECT_TRUE(std::isfinite(xpe_bpm_v2_internal::lack_of_fit_feature(0.0, 5, 1.0, 0.1)));
}

// ---------------------------------------------------------------- the failure policies, through the detector
TEST(BpmLinesLofPolicyTest, WithoutThreeDoseGroupsOfPureErrorTheLinearityCauseIsNotEvaluatedAtAll) {
    // a strongly curved row: with enough repeats it is found (the positive controls); with groups that give no pure error (single frames, or too few repeats) the cause is not evaluated and the old
    // statistic is not used in its place. Three designs: six single frames (no pure error), two groups with repeats and three single frames, three pairs with a pooled 3 degrees of freedom (< 5).
    const std::vector<std::vector<int>> designs = {{1, 1, 1, 1, 1, 1}, {3, 3, 1, 1, 1}, {2, 2, 2, 1, 1}};
    for (const auto& sizes : designs) {
        Stack st = makeStack(9100u, sizes, {{0, 256, 100, 101, nonlinear(0.4, 1.0)}});
        const Found r = detect(st);
        ASSERT_EQ(XPE_OK, r.rc);
        EXPECT_EQ(0, countCause(r, XPE_BPM_LINE_CAUSE_LINEARITY)) << "design with " << sizes.size() << " groups, first sizes " << sizes[0] << "," << sizes[1];
    }
    // and with the same curve and a design that has the pure error, the row is found
    Stack ok = makeStack(9100u, {3, 3, 6, 3, 3}, {{0, 256, 100, 101, nonlinear(0.4, 1.0)}});
    const Found rr = detect(ok);
    ASSERT_EQ(XPE_OK, rr.rc);
    EXPECT_TRUE(covered(rr, XPE_BPM_LINE_ROW, 100, 0, 256, XPE_BPM_LINE_CAUSE_LINEARITY, 0.9));
}

TEST(BpmLinesLofPolicyTest, ARowWhoseBrightValuesAreClippedAtTheTopIsNoEvidenceOfCurvature) {
    // +40000 counts in the bright frames only: the two highest doses are clipped at 65535 (a flattened, apparently curved response). Saturation is a different defect; the pixel has no evidence for the cause.
    Mod clipped;
    clipped.offset = 40000.0;
    Stack st = makeStack(9200u, kDesignA, {{0, 256, 100, 101, clipped}});
    const Found r = detect(st);
    ASSERT_EQ(XPE_OK, r.rc);
    EXPECT_FALSE(covered(r, XPE_BPM_LINE_ROW, 100, 0, 256, XPE_BPM_LINE_CAUSE_LINEARITY, 0.1)) << "a clipped row must not become a LINEARITY defect through the clipping";
}
