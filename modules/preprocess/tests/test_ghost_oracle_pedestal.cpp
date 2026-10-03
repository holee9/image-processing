/**
 * @file test_ghost_oracle_pedestal.cpp
 * @brief QA-A-225 M6: blank frames that stay ABOVE 0 after correction, so the zero clamp cannot saturate the metric. #241.
 *
 * M5 left a limit: in all 36 noisy rows the corrected blank frame was 100 % clamped, so GhostRemovalPct read 98.9-100 % whatever
 * the lag was. This file gives the blank frames a sustained signal (a pedestal P of 0.4 of saturation that is part of the TRUE
 * exposure, so the lag generators produce lag for it too and an exact corrector returns P on the blank frames), long enough
 * (1500 pre frames) for the slow Starman tail to settle, and checks whether the metric tells the correctors apart.
 *
 * Same stance as test_ghost_oracle.cpp: module values are measured and printed (`[ghost-oracle-m6]` lines), no threshold is
 * asserted on them (D1 is the user's). What IS asserted is the metric on CONTROLS built from the generator by arithmetic alone
 * (never from the module): what each control must score, and therefore what the metric can and cannot separate.
 */

#include <gtest/gtest.h>

#include "ghost_oracle_harness.h"
#include "preprocess_state_fixture.h"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace ghost_oracle;

namespace {

class GhostOraclePedestal : public XpePreprocessStateFixture {
protected:
    void SetUp() override { XpePreprocessStateFixture::SetUp(); xpe_clear_alerts(); }
    void TearDown() override { xpe_clear_alerts(); XpePreprocessStateFixture::TearDown(); }
};

constexpr double kPedestal = 0.4;   // of saturation

/** nPre frames of the pedestal, nExp frames of pedestal + level, nBlank frames of the pedestal again. */
Seq pedestalSeq(double level) {
    Seq s;
    s.nPre = 1500; s.nExp = 200; s.nBlank = 50; s.level = level;
    const double p = kPedestal * kSat;
    s.x.assign(s.nPre, p);
    s.x.insert(s.x.end(), s.nExp, p + level * kSat);
    s.x.insert(s.x.end(), s.nBlank, p);
    return s;
}

/** A control: c = y - f * (lag part of y), the lag part being (generator value - true x) per frame; the noise of y stays. */
Frames scaledLagRemoved(const Seq& s, const std::vector<double>& truth, const Frames& y, double f) {
    Frames out = y;
    for (size_t i = 0; i < out.size(); ++i)
        for (auto& px : out[i]) px = static_cast<float>(px - f * (truth[i] - s.x[i]));
    return out;
}

/** Signed blank residual of frame k against the CORRECTED dark reference, in percent of the exposure amplitude
 *  (the raw exposure frame minus the raw dark): positive = lag left, negative = over-subtracted. */
double signedPct(const Seq& s, const Frames& y, const Frames& c, size_t k) {
    double darkCor = 0.0, darkRaw = 0.0;
    for (size_t i = 0; i < s.nPre; ++i) { darkCor += meanOf(c[i]); darkRaw += meanOf(y[i]); }
    darkCor /= static_cast<double>(s.nPre); darkRaw /= static_cast<double>(s.nPre);
    const double amp = meanOf(y[s.nPre + s.nExp - 1]) - darkRaw;
    return 100.0 * (meanOf(c[s.nPre + s.nExp + k - 1]) - darkCor) / amp;
}

bool allNonNegative(const Frames& fs) {
    for (const auto& f : fs)
        for (float v : f)
            if (!(v >= 0.0f)) return false;
    return true;
}

}  // namespace

TEST_F(GhostOraclePedestal, TheControlsSayWhatTheMetricCanAndCannotSeparate) {
    for (const bool trap : {false, true})
        for (const double level : {0.02, 0.27, 0.5}) {
            const Seq s = pedestalSeq(level);
            const auto truth = trap ? truthTrap(s.x) : truthLti(s.x);
            const Frames y = makeFrames(truth, 0.0, 0);
            const Metrics none = measure(s, y, y);
            const Metrics oracle = measure(s, y, makeFrames(s.x, 0.0, 0));
            const struct { const char* name; double f; } ctl[] = {{"under 0.5x", 0.5}, {"over 1.5x", 1.5}, {"over 2x", 2.0}, {"over 3x", 3.0}};
            std::printf("[ghost-oracle-m6] %s %2.0f%% pedestal %.0f%% | dark_raw %.2f dark_cor(oracle) %.2f | none: removal(M5) %.2f%% removal(M1) %.2f%% | oracle: removal(M5) %.2f%% removal(M1) %.2f%% | blank pixels at 0 (oracle k1) %.3f\n",
                        trap ? "T2" : "T1", level * 100.0, kPedestal * 100.0, none.darkRaw, oracle.darkCor, none.k1.removal,
                        none.k1.removalCorDark, oracle.k1.removal, oracle.k1.removalCorDark, oracle.k1.clampedShare);

            // the design's premise: no control row touches the clamp, on a blank frame or a dark frame
            EXPECT_EQ(0.0, none.k1.clampedShare); EXPECT_EQ(0.0, oracle.k1.clampedShare); EXPECT_EQ(0.0, oracle.k50.clampedShare);

            EXPECT_NEAR(0.0, none.k1.removal, 1e-9);
            EXPECT_NEAR(0.0, none.k50.removalCorDark, 1e-9);
            // an exact corrector scores 100 % when the reference is the CORRECTED dark frames (nothing is rectified here) ...
            EXPECT_NEAR(100.0, oracle.k1.removalCorDark, 1e-6) << "k=1";
            EXPECT_NEAR(100.0, oracle.k50.removalCorDark, 1e-6) << "k=50";
            // ... but NOT when the reference is the UNcorrected dark frames: those carry the steady-state lag of the pedestal
            // itself, which the exact corrector removes from the blank frames and so lands that far below the reference
            EXPECT_GT(none.darkRaw, kPedestal * kSat) << "the raw dark frames sit above the pedestal by its own lag";
            EXPECT_NEAR(100.0 * std::fabs(kPedestal * kSat - none.darkRaw) / meanOf(y[s.nPre + s.nExp - 1]), oracle.k1.res, 1e-6);
            EXPECT_LT(oracle.k1.removal, 100.0 - 1e-3) << "removal(M5) of the exact corrector is below 100 % on a pedestal";

            // the frames are float32 at 26000-44000 (one step is about 0.002-0.004 ADU) while the k=50 lag is a few ADU, so a
            // control lands within a few thousandths of a percentage point of its arithmetic value, not exactly on it
            // the tolerance is that step over the lag the row has to measure: 100 * 0.004 ADU / |lag in ADU|, in percentage points
            const auto quant = [&](size_t k) {
                const double lagAdu = std::fabs(meanOf(y[s.nPre + s.nExp + k - 1]) - none.darkRaw);
                return 100.0 * 0.004 / std::max(lagAdu, 1e-9) + 1e-6;
            };
            const double kQuant = quant(1), kQuant50 = quant(50);
            for (const auto& c : ctl) {
                const Frames cf = scaledLagRemoved(s, truth, y, c.f);
                const Metrics m = measure(s, y, cf);
                const double want = 100.0 * (1.0 - std::fabs(1.0 - c.f));   // |residual| = |1-f| * lag: the unsigned metric
                std::printf("[ghost-oracle-m6]   %s %2.0f%% control %-10s k1: removal(M1) %7.2f%% (expected %7.2f%%) signed %+8.3f%% | blank pixels at 0 %.3f\n",
                            trap ? "T2" : "T1", level * 100.0, c.name, m.k1.removalCorDark, want, signedPct(s, y, cf, 1), m.k1.clampedShare);
                EXPECT_EQ(0.0, m.k1.clampedShare) << c.name;
                EXPECT_NEAR(want, m.k1.removalCorDark, kQuant) << c.name << " k=1";
                EXPECT_NEAR(want, m.k50.removalCorDark, kQuant50) << c.name << " k=50";
                // the sign is in the SIGNED residual: positive under-corrected, negative over-corrected
                EXPECT_EQ(c.f < 1.0, signedPct(s, y, cf, 1) > 0.0) << c.name;
            }
            // what the unsigned metric cannot see: removing twice the lag leaves a residual of the same size as removing none
            const Metrics twice = measure(s, y, scaledLagRemoved(s, truth, y, 2.0));
            EXPECT_NEAR(none.k1.removalCorDark, twice.k1.removalCorDark, kQuant) << "over 2x scores like no correction on the unsigned metric";
            EXPECT_NEAR(-signedPct(s, y, y, 1), signedPct(s, y, scaledLagRemoved(s, truth, y, 2.0), 1), 1e-3)
                << "while the signed residual shows the two apart (equal size, opposite sign)";
        }
}

TEST_F(GhostOraclePedestal, TheModuleRowsThatNeverTouchTheClamp) {
    Failures fail;
    int rows = 0, kept = 0;
    // the "accurate" configuration: derived (M3) from a pedestal-free calibration sequence of the T1 truth, retention band +-0.5 %
    const Seq cal = exposureSeq(0.27);
    const Lag derived = deriveLag(cal, meansOf(makeFrames(truthLti(cal.x), 0.0, 0)), Band{0.995, 1.005});
    std::printf("[ghost-oracle-m6] derived configuration (calibration T1 27%%, no pedestal): alpha1 %.5f tau1 %.3f alpha2 %.5f tau2 %.3f, steady-state gain S %.4f (the generator's own: %.4f)\n",
                derived.a1, derived.t1, derived.a2, derived.t2, steadyGain(derived), [] { double t = 0; for (int n = 0; n < 4; ++n) t += kB[n] / (std::exp(kA[n]) - 1.0); return t; }());
    std::printf("[ghost-oracle-m6] module rows: removal with the corrected dark (M1) / the uncorrected dark (M5), signed residual (%% of the exposure amplitude), blank pixels at 0 (k1 k10 k50), dark pixels at 0\n");
    for (const double sigma : {0.0, 5.0})
        for (const bool trap : {false, true})
            for (const double level : {0.02, 0.27, 0.5}) {
                const Seq s = pedestalSeq(level);
                const Frames y = makeFrames(trap ? truthTrap(s.x) : truthLti(s.x), sigma, 7);
                std::vector<Named> who = correctors(s, &fail);
                for (const auto& d : derivedCorrectors("T1@27", derived, &fail)) who.push_back(d);
                for (const auto& c : who) {
                    if (c.control || c.uncalibrated) continue;
                    const Frames out = c.fn(y);
                    ASSERT_TRUE(allNonNegative(out));
                    const Metrics m = measure(s, y, out);
                    size_t darkZeros = 0;
                    for (size_t i = 0; i < s.nPre; ++i) for (float v : out[i]) if (v == 0.0f) ++darkZeros;
                    const bool clean = m.k1.clampedShare == 0.0 && m.k10.clampedShare == 0.0 && m.k50.clampedShare == 0.0 && darkZeros == 0;
                    ++rows;
                    if (!clean) { std::printf("[ghost-oracle-m6]   EXCLUDED (touches the clamp) %s %2.0f%% sigma %g %s: blank at 0 %.3f %.3f %.3f, dark at 0 %zu\n",
                                              trap ? "T2" : "T1", level * 100.0, sigma, c.name.c_str(), m.k1.clampedShare, m.k10.clampedShare, m.k50.clampedShare, darkZeros);
                                  continue; }
                    ++kept;
                    std::printf("[ghost-oracle-m6]   %s %2.0f%% sigma %g %-30s | k1 removal(M1) %8.2f%% (M5) %8.2f%% signed %+8.3f%% | k10 removal(M1) %8.2f%% signed %+8.3f%% | k50 removal(M1) %8.2f%% signed %+8.3f%% | LagRaw k1 %.3f%%\n",
                                trap ? "T2" : "T1", level * 100.0, sigma, c.name.c_str(), m.k1.removalCorDark, m.k1.removal,
                                signedPct(s, y, out, 1), m.k10.removalCorDark, signedPct(s, y, out, 10), m.k50.removalCorDark, signedPct(s, y, out, 50), m.k1.raw);
                }
            }
    std::printf("[ghost-oracle-m6] module rows kept (blank and dark frames never at 0): %d of %d\n", kept, rows);
    // the positive control of the filter: the clamp detector must find rows that DO touch the clamp (the over-correcting tier-2 set
    // at 50 % does), otherwise "no excluded row" would only mean the detector reads nothing
    EXPECT_LT(kept, rows) << "some module row must touch the clamp, or the filter proves nothing";
    EXPECT_GT(kept, 0);
    EXPECT_EQ(0, fail.n);
}
