/**
 * @file test_ghost_oracle_derive.cpp
 * @brief QA-A-225 M3: (a) why GhostRemovalPct at k=50 is negative in noisy rows, and (b) a lag configuration DERIVED from a
 *        calibration sequence and judged on other sequences (the hold-out rule, design memo R5). #241.
 *
 * Same stance as test_ghost_oracle.cpp: the module's numbers are measured and printed (`[ghost-oracle]` lines), no threshold is
 * asserted on them (D1 is the user's, waiting for real-device data). What is asserted is mechanical:
 *   (a) the CAUSE of the negative removals, pinned as an identity: every noisy row whose k=50 removal is negative has a corrected
 *       blank frame that is entirely 0 (the module clamps at 0), so its LagResidual is exactly the rectified dark reference of
 *       the same path, which grows with the noise and has nothing to do with the lag; the module never outputs a negative pixel;
 *   (b) the derivation is deterministic, produces a configuration the module accepts, meets its own retention constraint on the
 *       calibration sequence, and its double-precision replica of tier 1 agrees with the real module.
 */

#include <gtest/gtest.h>

#include "ghost_oracle_harness.h"
#include "preprocess_state_fixture.h"

#include <cstdio>
#include <string>
#include <vector>

using namespace ghost_oracle;

namespace {

class GhostOracleDerive : public XpePreprocessStateFixture {
protected:
    void SetUp() override { XpePreprocessStateFixture::SetUp(); xpe_clear_alerts(); }
    void TearDown() override { xpe_clear_alerts(); XpePreprocessStateFixture::TearDown(); }
};

bool allNonNegative(const Frames& fs) {
    for (const auto& f : fs)
        for (float v : f)
            if (v < 0.0f) return false;
    return true;
}

double spatialStd(const Frame& f) {
    const double m = meanOf(f);
    double s = 0.0;
    for (float v : f) s += (v - m) * (v - m);
    return std::sqrt(s / static_cast<double>(f.size()));
}

}  // namespace

// ---- (a) the negative k=50 removal under noise ---------------------------------------------------------------------

TEST_F(GhostOracleDerive, ANegativeNoisyRemovalIsTheRectifiedDarkReferenceNotTheLag) {
    Failures fail;
    int rows = 0, negative = 0, negativeRawDark = 0;
    std::printf("[ghost-oracle] M3-a: noisy rows (sigma 5 ADU), k=50: removal by the E2E definition (DarkRef = corrected dark frames), "
                "by DarkRef = UNcorrected dark frames, share of the corrected blank frame that is exactly 0, mean of the corrected dark frames\n");
    for (const bool trap : {false, true})
        for (const double level : {0.02, 0.27, 0.92}) {
            const Seq s = exposureSeq(level);
            const auto y = makeFrames(trap ? truthTrap(s.x) : truthLti(s.x), 5.0, 7);
            const double exposure = meanOf(y[s.nPre + s.nExp - 1]);
            for (const auto& c : correctors(s, &fail)) {
                if (c.control || c.uncalibrated) continue;
                const Frames out = c.fn(y);
                ASSERT_TRUE(allNonNegative(out)) << c.name << ": the module clamps at 0, no pixel is negative";
                const Metrics m = measure(s, y, out);
                ++rows;
                if (m.k50.removal < 0.0) ++negative;
                if (m.k50.removalRawDark < 0.0) ++negativeRawDark;
                std::printf("[ghost-oracle] %-3s %2.0f%% %-30s k50 LagRaw %.4f%% | removal %8.2f%% | removal(DarkRef raw) %8.2f%% | "
                            "blank pixels at 0: %.3f | dark_raw %7.4f dark_cor %7.4f\n",
                            trap ? "T2" : "T1", level * 100.0, c.name.c_str(), m.k50.raw, m.k50.removal, m.k50.removalRawDark,
                            m.k50.clampedShare, m.darkRaw, m.darkCor);
                // the mechanism, pinned: a negative removal means a fully clamped corrected blank frame ...
                if (m.k50.removal < 0.0) EXPECT_EQ(1.0, m.k50.clampedShare) << c.name;
                // ... whose residual is then exactly the rectified dark reference, whatever the lag was
                if (m.k50.clampedShare == 1.0) {
                    EXPECT_NEAR(100.0 * std::fabs(m.darkCor) / exposure, m.k50.res, 1e-9) << c.name;
                }
                // the corrected dark frames are rectified: their mean is above the (about 0) mean of the raw ones
                EXPECT_GT(m.darkCor, m.darkRaw) << c.name;
            }
        }
    std::printf("[ghost-oracle] M3-a: %d noisy calibrated-module rows, %d with a negative k=50 removal (E2E definition), %d with DarkRef = uncorrected dark\n",
                rows, negative, negativeRawDark);
    EXPECT_EQ(0, fail.n);
    EXPECT_GT(rows, 0);
}

TEST_F(GhostOracleDerive, TheRectifiedDarkReferenceGrowsWithTheNoiseAndNotWithTheLag) {
    const Seq s = exposureSeq(0.27);
    Failures fail;
    const std::string cfg = lagCfg(1, 0.02, 3.0, 0.003, 30.0);
    double ratioMin = 1e9, ratioMax = 0.0;
    std::printf("[ghost-oracle] M3-a: sigma sweep, T1 27%%, tier 1 0.02/3/0.003/30: mean of the corrected dark frames, and per sigma\n");
    for (const double sigma : {1.0, 2.0, 5.0, 10.0, 20.0}) {
        const auto y = makeFrames(truthLti(s.x), sigma, 7);
        int failures = 0;
        const Frames out = runModule(cfg.c_str(), y, &failures);
        EXPECT_EQ(0, failures);
        const Metrics m = measure(s, y, out);
        const double perSigma = m.darkCor / sigma;
        ratioMin = std::min(ratioMin, perSigma);
        ratioMax = std::max(ratioMax, perSigma);
        std::printf("[ghost-oracle] sigma %5.1f: corrected dark mean %.4f ADU (%.4f per sigma; 1/sqrt(2*pi) = %.4f for a zero history)\n",
                    sigma, m.darkCor, perSigma, 1.0 / std::sqrt(2.0 * 3.14159265358979323846));
    }
    EXPECT_LT(ratioMax / ratioMin, 1.15) << "the bias is proportional to the noise (within the sampling spread of five frames)";
    (void)fail;
}

TEST_F(GhostOracleDerive, MeasuresWhetherTheCorrectorAmplifiesTheNoise) {
    // spatial standard deviation of an exposure frame (no clamp involved) before and after correction, and of its INTERIOR only
    // (tier 3 treats the border pixels differently, so the whole-frame figure mixes two things)
    const Seq s = exposureSeq(0.27);
    const auto y = makeFrames(truthLti(s.x), 5.0, 7);
    const size_t i = s.nPre + 150;
    auto interiorStd = [](const Frame& f) {
        double sum = 0.0;
        size_t n = 0;
        for (uint32_t r = 1; r + 1 < H; ++r)
            for (uint32_t c = 1; c + 1 < W; ++c) { sum += f[r * W + c]; ++n; }
        const double m = sum / static_cast<double>(n);
        double v = 0.0;
        for (uint32_t r = 1; r + 1 < H; ++r)
            for (uint32_t c = 1; c + 1 < W; ++c) v += (f[r * W + c] - m) * (f[r * W + c] - m);
        return std::sqrt(v / static_cast<double>(n));
    };
    Failures fail;
    std::printf("[ghost-oracle] M3-a: noise of exposure frame 150 (T1 27%%, sigma 5): raw whole %.3f interior %.3f\n", spatialStd(y[i]), interiorStd(y[i]));
    for (const auto& c : correctors(s, &fail)) {
        if (c.control || c.uncalibrated) continue;
        const Frames out = c.fn(y);
        ASSERT_TRUE(allFinite(out));
        std::printf("[ghost-oracle] %-34s corrected whole-frame std %9.3f (x%.2f) | interior std %7.3f (x%.2f)\n", c.name.c_str(),
                    spatialStd(out[i]), spatialStd(out[i]) / spatialStd(y[i]), interiorStd(out[i]), interiorStd(out[i]) / interiorStd(y[i]));
    }
    EXPECT_EQ(0, fail.n);
}

// ---- (b) a configuration derived from a calibration sequence ---------------------------------------------------------

TEST_F(GhostOracleDerive, TheReplicaOfTierOneMatchesTheRealModule) {
    // the replica is used only to search; it must agree with what it stands for, clamp included
    const Seq s = exposureSeq(0.27);
    const auto yv = truthLti(s.x);
    const auto frames = makeFrames(yv, 0.0, 0);
    const Lag sets[] = {{0.02, 3.0, 0.003, 30.0}, {0.1, 1.0, 0.01, 20.0}, {0.05, 0.5, 0.0, 10.0}};
    for (const Lag& l : sets) {
        int failures = 0;
        const auto mod = meansOf(runModule(lagCfg(1, l).c_str(), frames, &failures));
        const auto rep = replicaTier1(yv, l);
        EXPECT_EQ(0, failures);
        ASSERT_EQ(mod.size(), rep.size());
        double worst = 0.0;
        for (size_t k = 0; k < rep.size(); ++k) worst = std::max(worst, std::fabs(mod[k] - rep[k]));
        EXPECT_LT(worst, 0.05) << "alpha1 " << l.a1 << ": replica vs module, ADU";
    }
}

TEST_F(GhostOracleDerive, TheDerivationIsDeterministicValidAndMeetsItsOwnConstraint) {
    for (const bool trap : {false, true})
        for (const Band band : {Band{0.995, 1.005}, Band{0.999, 1.001}}) {
            const Seq s = exposureSeq(0.27);
            const auto y = trap ? truthTrap(s.x) : truthLti(s.x);
            const Lag a = deriveLag(s, y, band);
            const Lag b = deriveLag(s, y, band);
            EXPECT_EQ(a.a1, b.a1); EXPECT_EQ(a.t1, b.t1); EXPECT_EQ(a.a2, b.a2); EXPECT_EQ(a.t2, b.t2);
            EXPECT_TRUE(lagValid(a));
            const double r = deriveRetention(s, y, a);
            EXPECT_GE(r, band.lo) << (trap ? "T2" : "T1");
            EXPECT_LE(r, band.hi) << (trap ? "T2" : "T1");
            // the objective is not satisfied by the clamp: read WITHOUT the clamp at 0, the derived set leaves a blank residual that is
            // well below the uncorrected one (a first version scored exactly 0 ADU on the clamped value by over-subtracting; its signed
            // residual was as large as the raw lag)
            {
                const auto sc = replicaTier1(y, a, /*clamp=*/false);
                double sig = 0.0, raw = 0.0;
                for (size_t i = s.nPre + s.nExp; i < s.nPre + s.nExp + s.nBlank; ++i) { sig += std::fabs(sc[i]); raw += std::fabs(y[i]); }
                EXPECT_LT(sig, raw / 5.0) << (trap ? "T2" : "T1") << ": signed blank residual " << sig / static_cast<double>(s.nBlank)
                                          << " ADU vs uncorrected " << raw / static_cast<double>(s.nBlank);
            }
            for (int tier = 1; tier <= 3; ++tier) {
                void* h = nullptr;
                EXPECT_EQ(XPE_OK, xpe_ghost_create(W, H, lagCfg(tier, a).c_str(), &h)) << "the module accepts the derived set, tier " << tier;
                xpe_ghost_destroy(h);
            }
        }
}

TEST_F(GhostOracleDerive, MeasuresDerivedConfigurationsOnSequencesTheyWereNotDerivedFrom) {
    struct Derived { std::string label; Lag lag; bool trap; double bandHalf; };
    std::vector<Derived> derived;
    for (const bool trap : {false, true})
        for (const double half : {0.005, 0.001}) {
            const Seq s = exposureSeq(0.27);
            const auto y = trap ? truthTrap(s.x) : truthLti(s.x);
            const Lag l = deriveLag(s, y, Band{1.0 - half, 1.0 + half});
            char lab[64];
            std::snprintf(lab, sizeof(lab), "cal=%s@27 band=%.1f", trap ? "T2" : "T1", half * 100.0);
            derived.push_back({lab, l, trap, half});
            std::printf("[ghost-oracle] M3-b derived %-22s alpha1 %.6g tau1 %.6g alpha2 %.6g tau2 %.6g | S %.4f | calibration retention %.5f, mean |blank| %.4f ADU\n",
                        lab, l.a1, l.t1, l.a2, l.t2, steadyGain(l), deriveRetention(s, y, l),
                        deriveCost(s, y, l, Band{1.0 - half, 1.0 + half}));
        }
    Failures fail;
    for (const auto& d : derived)
        for (const bool trap : {false, true})
            for (const double level : {0.02, 0.27, 0.92})
                for (const double sigma : {0.0, 5.0}) {
                    const Seq s = exposureSeq(level);
                    const auto y = makeFrames(trap ? truthTrap(s.x) : truthLti(s.x), sigma, 7);
                    const bool inSample = (trap == d.trap) && level == 0.27 && sigma == 0.0;
                    for (const auto& c : derivedCorrectors(d.label + (inSample ? " in-sample" : ""), d.lag, &fail)) {
                        const Frames out = c.fn(y);
                        ASSERT_EQ(y.size(), out.size());
                        EXPECT_TRUE(allFinite(out)) << c.name;
                        char lv[16];
                        std::snprintf(lv, sizeof(lv), "%.0f%%", level * 100.0);
                        printRow(trap ? "T2" : "T1", lv, sigma, c.name, measure(s, y, out));
                    }
                }
    std::printf("[ghost-oracle] M3-b falling step 27%% -> 5%%: signed residual in %% of the new level, raw -> corrected\n");
    for (const auto& d : derived)
        for (const bool trap : {false, true}) {
            const Seq s = fallingStepSeq(0.27, 0.05, 200, 100);
            const auto y = makeFrames(trap ? truthTrap(s.x) : truthLti(s.x), 0.0, 0);
            for (const auto& c : derivedCorrectors(d.label, d.lag, &fail)) {
                const Frames out = c.fn(y);
                EXPECT_TRUE(allFinite(out)) << c.name;
                const Step st = measureStep(s, y, out);
                auto removal = [](double raw, double cor) { return 100.0 * (1.0 - std::fabs(cor) / std::max(std::fabs(raw), kEps)); };
                std::printf("[ghost-oracle] %-4s step %-34s k=1 %+9.4f%% -> %+9.4f%% (removal %7.2f%%) | k=50 %+9.4f%% -> %+9.4f%% (removal %7.2f%%)\n",
                            trap ? "T2" : "T1", c.name.c_str(), st.rawRes1, st.corRes1, removal(st.rawRes1, st.corRes1),
                            st.rawRes50, st.corRes50, removal(st.rawRes50, st.corRes50));
            }
        }
    EXPECT_EQ(0, fail.n) << "every xpe_ghost_correct call must succeed";
}
