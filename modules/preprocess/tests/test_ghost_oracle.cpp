/**
 * @file test_ghost_oracle.cpp
 * @brief An oracle harness for the ghost-removal metrics of SRS-CALIB-FUNC-020: independent lag generators, the E2E
 *        metrics, a signal-retention companion, a clamp-free falling-step measurement, and positive / negative
 *        controls (QA-A-225 M1, #241).
 *
 * WHAT THIS FILE ASSERTS AND WHAT IT DOES NOT. The module's numbers are MEASURED and PRINTED (`[ghost-oracle]` lines);
 * no threshold is asserted on them. Three things are undecided by the requirement text and belong to the user (QA-A-225
 * design memo, D1): which blank frame k carries the 90 %, what "lag is present" means, and the pass line of the
 * companion metric. What IS asserted is mechanical:
 *   - the generators satisfy their own definitions (closed forms, not the module's formulas);
 *   - the metric code gives the answers the controls must give (none = 0 %, oracle = 100 % with retention 1, a corrector
 *     that zeroes everything scores 100 % on the blank-frame metric AND 0 retention -- the reason the blank-frame metric
 *     cannot stand alone);
 *   - the harness is deterministic, every module call succeeds and every output is finite;
 *   - a handle without calibrated lag parameters (QA-A-226) leaves every frame untouched, so its removal is exactly 0 %.
 *
 * INDEPENDENCE (QA-A-225 R4). The generators below do not use the module's formulas or its alpha/tau. T1 is the
 * discrete-time LTI impulse response of Starman et al. 2012 (Med Phys 39:6035), eq. (1), driven by the TRUE exposure,
 * with the a_n / b_n TDS-CALIB-001 4.4.2 lists for the "Varex 4030CB". T2 is a finite-trap model: this project's own
 * formulation (NOT an equation of the paper), used for its trend only -- the lag of a fixed fraction shrinks as the
 * exposure grows (the direction of the paper's Table 3). The ONE place that is not independent is the self-model
 * control, on purpose: it shows that a lag made by the module's own recursion is "corrected" 100 %, which proves nothing
 * about the module.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "ghost_oracle_harness.h"
#include "preprocess_state_fixture.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <random>
#include <string>
#include <vector>

using namespace ghost_oracle;

namespace {

class GhostOracle : public XpePreprocessStateFixture {
protected:
    void SetUp() override { XpePreprocessStateFixture::SetUp(); xpe_clear_alerts(); }
    void TearDown() override { xpe_clear_alerts(); XpePreprocessStateFixture::TearDown(); }
};

}  // namespace

// ---- the generators satisfy their own definitions ----------------------------------------------------------------

TEST_F(GhostOracle, TheLtiTruthSatisfiesItsDefinition) {
    // h(0) = b0 + sum b_n = 1 (the first frame of a lone exposure is not lagged)
    {
        std::vector<double> x(10, 0.0);
        x[3] = 1.0;
        const auto y = truthLti(x);
        EXPECT_NEAR(1.0, y[3], 1e-12);
        for (size_t k = 0; k < 3; ++k) EXPECT_EQ(0.0, y[k]) << "nothing before the exposure";
        // the tail of the impulse response: h(j) = sum b_n exp(-a_n j), positive and decreasing
        for (size_t j = 1; j < 6; ++j) {
            double want = 0.0;
            for (int n = 0; n < 4; ++n) want += kB[n] * std::exp(-kA[n] * static_cast<double>(j));
            EXPECT_NEAR(want, y[3 + j], 1e-12) << "h(" << j << ")";
            if (j > 1) EXPECT_LT(y[3 + j], y[3 + j - 1]);
        }
    }
    // steady state of a constant exposure: y_ss = x * (1 + sum b_n / (exp(a_n) - 1)) -- a closed form, not the generator's loop
    {
        std::vector<double> x(40000, 1000.0);
        const auto y = truthLti(x);
        double gain = 1.0;
        for (int n = 0; n < 4; ++n) gain += kB[n] / (std::exp(kA[n]) - 1.0);
        EXPECT_NEAR(1000.0 * gain, y.back(), 1000.0 * gain * 1e-6);
        EXPECT_GT(gain, 1.0);
    }
}

TEST_F(GhostOracle, TheTrapTruthLagShrinksRelativelyAsTheExposureGrows) {
    // T2's design claim (the direction of Starman's Table 3), asserted as a property of the generator and not of the module
    double prev = 1e9;
    for (const double level : {0.02, 0.27, 0.92}) {
        const Seq s = exposureSeq(level);
        const auto y = truthTrap(s.x);
        const double lag = 100.0 * std::fabs(y[s.nPre + s.nExp] - y[0]) / y[s.nPre + s.nExp - 1];
        std::printf("[ghost-oracle] T2 raw first blank frame lag at %2.0f%%: %.4f%% of the exposure signal\n", level * 100.0, lag);
        EXPECT_LT(lag, prev) << "level " << level;
        prev = lag;
    }
    // the LTI truth is level-independent in relative terms (what T2 adds)
    const auto a = exposureSeq(0.02), b = exposureSeq(0.92);
    const auto ya = truthLti(a.x), yb = truthLti(b.x);
    const double la = (ya[a.nPre + a.nExp] - ya[0]) / ya[a.nPre + a.nExp - 1];
    const double lb = (yb[b.nPre + b.nExp] - yb[0]) / yb[b.nPre + b.nExp - 1];
    std::printf("[ghost-oracle] T1 raw first blank frame lag: %.4f%% at 2%%, %.4f%% at 92%% (level-independent)\n", 100.0 * la, 100.0 * lb);
    EXPECT_NEAR(la, lb, 1e-9);
}

// ---- the metric code answers what the controls must answer ------------------------------------------------------

TEST_F(GhostOracle, TheControlsGiveTheAnswersTheyMust) {
    for (const bool trap : {false, true}) {
        for (const double level : {0.02, 0.27, 0.92}) {
            const Seq s = exposureSeq(level);
            const auto y = makeFrames(trap ? truthTrap(s.x) : truthLti(s.x), 0.0, 0);
            Failures f;
            const auto cs = correctors(s, &f);

            const Metrics none = measure(s, y, cs[0].fn(y));
            EXPECT_NEAR(0.0, none.k1.removal, 1e-9) << "no correction removes nothing";
            EXPECT_NEAR(0.0, none.k50.removal, 1e-9);
            EXPECT_NEAR(none.rawRetention, none.retention, 1e-12);

            const Metrics oracle = measure(s, y, cs[1].fn(y));
            EXPECT_NEAR(100.0, oracle.k1.removal, 1e-6) << "the true exposure leaves no lag";
            EXPECT_NEAR(100.0, oracle.k50.removal, 1e-6);
            EXPECT_NEAR(1.0, oracle.retention, 1e-6) << "and keeps the signal";

            // an offset added to every frame cancels against the dark reference of the SAME path: nothing removed, and the
            // retention moves by exactly the offset (this is the control that exercises DarkRef)
            Frames shifted = y;
            for (auto& frame : shifted) for (auto& px : frame) px += 100.0f;
            const Metrics off = measure(s, y, shifted);
            EXPECT_NEAR(0.0, off.k1.removal, 5e-3) << "level " << level;
            EXPECT_NEAR(0.0, off.k50.removal, 5e-3) << "level " << level;
            EXPECT_NEAR(none.retention + 100.0 / (level * kSat), off.retention, 1e-6);

            // LagRaw against the direct expression, with the exposure signal of the UNCORRECTED last exposure frame
            // (for the oracle the corrected frame differs from it, so this is where a wrong choice would show)
            {
                const auto yy = trap ? truthTrap(s.x) : truthLti(s.x);
                const double direct = 100.0 * std::fabs(yy[s.nPre + s.nExp]) / yy[s.nPre + s.nExp - 1];
                EXPECT_NEAR(direct, oracle.k1.raw, 1e-4) << "level " << level;
                // and the retention window (frames 50..end of the exposure) against the direct mean of the generator
                double sum = 0.0;
                size_t cnt = 0;
                for (size_t i = s.nPre + 49; i < s.nPre + s.nExp; ++i, ++cnt) sum += yy[i];
                EXPECT_NEAR(sum / static_cast<double>(cnt) / (level * kSat), none.retention, 1e-6) << "level " << level;
            }

            // the trap: a corrector that zeroes everything scores 100 % on the blank-frame metric and keeps NO signal
            const Metrics zeros = measure(s, y, cs[2].fn(y));
            EXPECT_NEAR(100.0, zeros.k1.removal, 1e-9);
            EXPECT_NEAR(100.0, zeros.k50.removal, 1e-9);
            EXPECT_EQ(0.0, zeros.retention) << "the companion metric is what tells it apart";
        }
    }
}

TEST_F(GhostOracle, TheStepMeasurementSeesWhatTheClampHides) {
    // 27 % -> 5 %: a corrector that zeroes everything leaves a residual of -100 % of the new level, no clamp can hide it
    const Seq s = fallingStepSeq(0.27, 0.05, 200, 100);
    const auto y = makeFrames(truthLti(s.x), 0.0, 0);
    Failures f;
    const auto cs = correctors(s, &f);
    const Step none = measureStep(s, y, cs[0].fn(y));
    EXPECT_GT(none.rawRes1, 0.0) << "the lag of the high level sits on top of the low one";
    EXPECT_NEAR(none.rawRes1, none.corRes1, 1e-12);
    const Step oracle = measureStep(s, y, cs[1].fn(y));
    EXPECT_NEAR(0.0, oracle.corRes1, 1e-6);
    EXPECT_NEAR(0.0, oracle.corRes50, 1e-6);
    const Step zeros = measureStep(s, y, cs[2].fn(y));
    EXPECT_NEAR(-100.0, zeros.corRes1, 1e-9);
    EXPECT_NEAR(-100.0, zeros.corRes50, 1e-9);
}

TEST_F(GhostOracle, ASelfModelLagIsCorrectedToTheLetterWhichProvesNothing) {
    // The positive control, on purpose NOT independent: the module's own recursion made the lag, the same configuration
    // inverts it exactly. Read it as "the metric code can say 100 %", never as evidence about the module.
    const Seq s = exposureSeq(0.27);
    const double a1 = 0.02, t1 = 3.0, a2 = 0.003, t2 = 30.0;
    const auto y = makeFrames(selfModel(s.x, a1, t1, a2, t2), 0.0, 0);
    int failures = 0;
    const auto same = runModule(lagCfg(1, a1, t1, a2, t2).c_str(), y, &failures);
    const Metrics m = measure(s, y, same);
    printRow("self", "27%", 0.0, "control:self-model, same config", m);
    EXPECT_EQ(0, failures);
    EXPECT_GE(m.k1.removal, 99.99);
    EXPECT_GE(m.k50.removal, 99.99);
    EXPECT_NEAR(1.0, m.retention, 1e-4);
    // a different configuration on the same data is NOT 100 %: the metric does discriminate
    const auto wrong = runModule(lagCfg(1, 0.01, 8.0, 0.001, 60.0).c_str(), y, &failures);
    const Metrics w = measure(s, y, wrong);
    printRow("self", "27%", 0.0, "control:self-model, other config", w);
    EXPECT_LT(w.k1.removal, 99.9);
    EXPECT_EQ(0, failures);
}

// ---- the module, measured (printed, not judged) ---------------------------------------------------------------

TEST_F(GhostOracle, MeasuresTheModuleAgainstTheIndependentTruths) {
    std::printf("[ghost-oracle] columns: truth T1 = Starman LTI, T2 = finite trap | level of saturation | sigma ADU | corrector | "
                "retention = corrected exposure mean (frames 50..200) / true exposure | LagRaw/LagResidual/GhostRemoval per blank frame k\n");
    Failures fail;
    int rows = 0;
    for (const bool trap : {false, true})
        for (const double level : {0.02, 0.27, 0.92})
            for (const double sigma : {0.0, 5.0}) {
                const Seq s = exposureSeq(level);
                const auto y = makeFrames(trap ? truthTrap(s.x) : truthLti(s.x), sigma, 7);
                for (const auto& c : correctors(s, &fail)) {
                    const Frames out = c.fn(y);
                    ASSERT_EQ(y.size(), out.size());
                    EXPECT_TRUE(allFinite(out)) << c.name;
                    const Metrics m = measure(s, y, out);
                    char lv[16];
                    std::snprintf(lv, sizeof(lv), "%.0f%%", level * 100.0);
                    printRow(trap ? "T2" : "T1", lv, sigma, c.name, m);
                    ++rows;
                    if (c.uncalibrated) {
                        // QA-A-226: no calibrated lag parameters, so no correction at all -- mechanical, not a threshold
                        EXPECT_TRUE(sameBytes(y, out)) << "an uncalibrated handle must leave every frame as it came";
                        EXPECT_NEAR(0.0, m.k1.removal, 1e-9);
                        EXPECT_NEAR(0.0, m.k50.removal, 1e-9);
                    }
                }
            }
    EXPECT_EQ(0, fail.n) << "every xpe_ghost_correct call must succeed";
    std::printf("[ghost-oracle] %d exposure rows\n", rows);
}

TEST_F(GhostOracle, MeasuresTheFallingStepWhereTheClampCannotHide) {
    std::printf("[ghost-oracle] falling step 27%% -> 5%% (200 + 100 frames): signed residual in %% of the NEW level, "
                "raw -> corrected, at the 1st and the 50th frame after the step\n");
    Failures fail;
    for (const bool trap : {false, true}) {
        const Seq s = fallingStepSeq(0.27, 0.05, 200, 100);
        const auto y = makeFrames(trap ? truthTrap(s.x) : truthLti(s.x), 0.0, 0);
        for (const auto& c : correctors(s, &fail)) {
            const Frames out = c.fn(y);
            EXPECT_TRUE(allFinite(out)) << c.name;
            const Step st = measureStep(s, y, out);
            auto removal = [](double raw, double cor) { return 100.0 * (1.0 - std::fabs(cor) / std::max(std::fabs(raw), kEps)); };
            std::printf("[ghost-oracle] %-4s step %-34s k=1 %+9.4f%% -> %+9.4f%% (removal %7.2f%%) | k=50 %+9.4f%% -> %+9.4f%% (removal %7.2f%%)\n",
                        trap ? "T2" : "T1", c.name.c_str(), st.rawRes1, st.corRes1, removal(st.rawRes1, st.corRes1),
                        st.rawRes50, st.corRes50, removal(st.rawRes50, st.corRes50));
            if (c.uncalibrated) {
                EXPECT_TRUE(sameBytes(y, out));
                EXPECT_EQ(st.rawRes1, st.corRes1);
            }
        }
    }
    EXPECT_EQ(0, fail.n);
}

TEST_F(GhostOracle, TheHarnessIsDeterministic) {
    const Seq s = exposureSeq(0.27);
    const auto y = makeFrames(truthLti(s.x), 5.0, 7);
    int failures = 0;
    const std::string cfg = lagCfg(3, 0.1, 1.0, 0.01, 20.0);
    const Metrics a = measure(s, y, runModule(cfg.c_str(), y, &failures));
    const Metrics b = measure(s, y, runModule(cfg.c_str(), y, &failures));
    EXPECT_EQ(0, failures);
    EXPECT_EQ(a.k1.removal, b.k1.removal);
    EXPECT_EQ(a.k50.removal, b.k50.removal);
    EXPECT_EQ(a.retention, b.retention);
    EXPECT_TRUE(sameBytes(makeFrames(truthLti(s.x), 5.0, 7), y)) << "the noise is seeded";
}

// QA-A-225 M4: the digest the allocation-failure executable must reproduce with its test seam at the defaults (the seam is inert).
TEST_F(GhostOracle, PrintsTheTierThreeDigestForTheSeamComparison) {
    const uint64_t d = tierThreeDigest();
    EXPECT_NE(0ull, d) << "every call succeeded";
    std::printf("[ghost-oracle] tier3-digest 0x%016llx\n", static_cast<unsigned long long>(d));
}
