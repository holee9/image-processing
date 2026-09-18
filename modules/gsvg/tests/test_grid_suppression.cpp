// #180 (QA-B-90): DWT grid suppression (grid_dwt.cpp), measured with the
// QA-B-89 tools in grid_test_tools.h.
//
// Size: 1024x1024 for the functional cases; 3072x3072 only in the measure-only
// REQ-GSVG-019 benchmark at the end (no time assertion).
//
// Thresholds were set from the QA-B-90 runs (gate.md). Where the result falls
// short of the SPEC or of the card's target, the shortfall is asserted as a
// KnownDivergence_ case so that an improvement shows up as a change.

#include <gtest/gtest.h>

#include "grid_dwt.h"
#include "grid_test_tools.h"
#include "perf_measure.h"

#include "xpe/gsvg/gsvg_api.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace gsvg_test;
namespace gd = xpe_gsvg_detail;

namespace {

constexpr int    kN = 1024;
constexpr double kPitch = 0.139;
constexpr double kDepth = 0.05;
constexpr double kLpis[] = {60.0, 103.0, 200.0};

std::vector<uint16_t> ToU16(const Image& img) {
    std::vector<uint16_t> out(img.px.size());
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = static_cast<uint16_t>(std::clamp(std::round(img.px[i]), 0.0, 65535.0));
    return out;
}

Image FromU16(const std::vector<uint16_t>& px, int w, int h) {
    Image img(w, h);
    for (size_t i = 0; i < px.size(); ++i) img.px[i] = px[i];
    return img;
}

const Image& Background(unsigned seed = 180u, double noise = 30.0) {
    static std::vector<std::pair<std::pair<unsigned, double>, Image>> cache;
    for (auto& e : cache)
        if (e.first.first == seed && e.first.second == noise) return e.second;
    cache.push_back({{seed, noise}, MakeAnatomyBackground(kN, kN, seed, noise)});
    return cache.back().second;
}

GridAxis ToolAxis(gd::Axis a) { return a == gd::Axis::Rows ? GridAxis::Rows : GridAxis::Columns; }
const char* Name(gd::Axis a) { return a == gd::Axis::Rows ? "rows" : "cols"; }

struct SuppressionResult {
    double baseline = 0.0;   // residual energy ratio of the grid-free (rounded) image
    double before = 0.0;     // with the grid
    double after = 0.0;      // after SuppressGrid
    gd::Report report;
};

SuppressionResult RunSuppression(double lpi, gd::Axis axis, const gd::Options& opt = {},
                                 double depth = kDepth) {
    GridSpec g;
    g.linesPerInch = lpi; g.pitchMm = kPitch; g.axis = ToolAxis(axis); g.depth = depth;
    const double f = AliasedFrequencyPerMm(lpi, kPitch);
    const Image truth = FromU16(ToU16(Background()), kN, kN);
    auto px = ToU16(ApplyGrid(Background(), g));
    SuppressionResult r;
    r.baseline = ResidualGridEnergy(truth, g.axis, f, kPitch).ratio;
    r.before = ResidualGridEnergy(FromU16(px, kN, kN), g.axis, f, kPitch).ratio;
    r.report = gd::SuppressGrid(px.data(), kN, kN, opt);
    r.after = ResidualGridEnergy(FromU16(px, kN, kN), g.axis, f, kPitch).ratio;
    return r;
}

// Suppression criterion: residual grid energy at least 40 dB below the input.
// Measured with the defaults: 2.2e-5 .. 8.1e-5 (QA-B-90 run 5).
constexpr double kSuppressionRatio = 1e-4;
bool Suppressed(const SuppressionResult& r) { return r.after <= kSuppressionRatio * r.before; }

struct MtfBand { double lo, hi, worstLoss; };

// Detect on edge x grid, then apply the same decisions to the grid-free edge
// and compare that with the edge itself: the blur the filtering puts on
// anatomy. The edge line is fitted on the grid-free edge and used for both --
// the band-stop leaves a ripple along each line that would otherwise pull the
// fit (QA-B-90 run 4). naive: compare the grid-free edge with processed
// edge x grid and fit each on its own, which also counts residual grid.
std::vector<MtfBand> MtfLoss(gd::Axis gridAxis, double lpi, const gd::Options& opt = {},
                             bool naive = false, gd::Report* decisionsOut = nullptr) {
    constexpr double kSigma = 0.5;
    const Image edge = MakeSlantedEdge(kN, kN, 3.0, kSigma, 8000.0, 30000.0);
    GridSpec g;
    g.linesPerInch = lpi; g.pitchMm = kPitch; g.axis = ToolAxis(gridAxis); g.depth = kDepth;
    const auto truthPx = ToU16(edge);
    auto px = ToU16(ApplyGrid(edge, g));
    const gd::Report detected = gd::SuppressGrid(px.data(), kN, kN, opt);
    if (decisionsOut) *decisionsOut = detected;
    if (!naive) {
        px = truthPx;
        gd::Options rep = opt;
        rep.replay = &detected;
        gd::SuppressGrid(px.data(), kN, kN, rep);
    }
    auto crop = [](const std::vector<uint16_t>& v) {
        constexpr int cw = 64, chh = 256;
        Image c(cw, chh);
        for (int y = 0; y < chh; ++y)
            for (int x = 0; x < cw; ++x)
                c.at(x, y) = v[static_cast<size_t>(y + kN / 2 - chh / 2) * kN + static_cast<size_t>(x + kN / 2 - cw / 2)];
        return c;
    };
    const Image truthCrop = crop(truthPx);
    const EdgeLine geom = FitEdge(truthCrop);
    const MtfCurve t = SlantedEdgeMtf(truthCrop, TiltCorrection::On, 0.25, 16.0, 1.0, 0.01, &geom);
    const MtfCurve p = SlantedEdgeMtf(crop(px), TiltCorrection::On, 0.25, 16.0, 1.0, 0.01,
                                      naive ? nullptr : &geom);
    std::vector<MtfBand> bands = {{0.0, 0.1, 0}, {0.1, 0.2, 0}, {0.2, 0.3, 0}, {0.3, 0.4, 0}, {0.4, 0.5, 0}};
    for (size_t i = 0; i < t.freq.size(); ++i)
        for (auto& b : bands)
            if (t.freq[i] >= b.lo - 1e-9 && t.freq[i] < b.hi - 1e-9)
                b.worstLoss = std::max(b.worstLoss, (t.mtf[i] - p.mtf[i]) / t.mtf[i]);
    return bands;
}

double Worst(const std::vector<MtfBand>& bands) {
    double w = 0.0;
    for (const auto& b : bands) w = std::max(w, b.worstLoss);
    return w;
}

}  // namespace

// ---------------------------------------------------------------------------
// Building blocks
// ---------------------------------------------------------------------------

TEST(GsvgGridSuppression, FftMatchesDirectDftAndInverts) {
    for (size_t n : {size_t{16}, size_t{64}, size_t{96}, size_t{45}, size_t{7}, size_t{1536}}) {
        std::vector<std::complex<double>> x(n), ref(n);
        for (size_t t = 0; t < n; ++t) x[t] = {std::sin(0.37 * t) + 0.1 * t, std::cos(1.3 * t)};
        for (size_t k = 0; k < n; ++k) {
            std::complex<double> acc = 0.0;
            for (size_t t = 0; t < n; ++t)
                acc += x[t] * std::polar(1.0, -2.0 * kPi * static_cast<double>(k * t) / static_cast<double>(n));
            ref[k] = acc;
        }
        std::vector<std::complex<double>> y = x;
        const gd::Fft fft(n);
        fft.forward(y);
        double fwdErr = 0.0, scale = 0.0;
        for (size_t k = 0; k < n; ++k) { fwdErr = std::max(fwdErr, std::abs(y[k] - ref[k])); scale = std::max(scale, std::abs(ref[k])); }
        fft.inverse(y);
        double invErr = 0.0;
        for (size_t t = 0; t < n; ++t) invErr = std::max(invErr, std::abs(y[t] - x[t]));
        std::printf("GRIDSUP fft n=%zu fwd_err=%.3g (scale %.3g) inv_err=%.3g\n", n, fwdErr, scale, invErr);
        EXPECT_LT(fwdErr, 1e-9 * scale) << n;
        EXPECT_LT(invErr, 1e-9) << n;
    }
}

// Periodic boundary; odd sizes are padded by repeating the last row/column.
TEST(GsvgGridSuppression, Db4DwtReconstructsPerfectly) {
    struct { int w, h; } sizes[] = {{kN, kN}, {97, 64}, {64, 33}, {32, 32}};
    for (auto s : sizes) {
        std::vector<double> img(static_cast<size_t>(s.w) * static_cast<size_t>(s.h));
        for (int y = 0; y < s.h; ++y)
            for (int x = 0; x < s.w; ++x)
                img[static_cast<size_t>(y) * static_cast<size_t>(s.w) + static_cast<size_t>(x)] =
                    Background().at(x, y);
        const gd::Level lv = gd::Dwt2(img, s.w, s.h);
        const auto rec = gd::Idwt2(lv);
        double err = 0.0;
        for (size_t i = 0; i < img.size(); ++i) err = std::max(err, std::fabs(rec[i] - img[i]));
        std::printf("GRIDSUP dwt %dx%d max_abs_err=%.3g\n", s.w, s.h, err);
        EXPECT_LT(err, 1e-8) << s.w << "x" << s.h;
    }
}

TEST(GsvgGridSuppression, SubbandPlacementMatchesHandValues) {
    // f (cycles/pixel) = aliased c/mm * 0.139, worked by hand:
    //   60 lpi: 2.362205*0.139 = 0.328346 -> level 1, 1 - 2*0.328346 = 0.343308
    //  103 lpi: 3.139127*0.139 = 0.436339 -> level 1, 1 - 0.872678   = 0.127322
    //  200 lpi: 0.679771*0.139 = 0.094488 -> x2 0.188976 -> x2 0.377952: level 3, 1 - 0.755904 = 0.244096
    auto p60 = gd::PlaceInSubbands(0.328346, 6);
    auto p103 = gd::PlaceInSubbands(0.436339, 6);
    auto p200 = gd::PlaceInSubbands(0.094488, 6);
    EXPECT_EQ(p60.level, 1);  EXPECT_NEAR(p60.subFreq, 0.343308, 1e-6);
    EXPECT_EQ(p103.level, 1); EXPECT_NEAR(p103.subFreq, 0.127322, 1e-6);
    EXPECT_EQ(p200.level, 3); EXPECT_NEAR(p200.subFreq, 0.244096, 1e-6);
    EXPECT_EQ(gd::PlaceInSubbands(0.001, 6).level, 0);   // folds too close to DC
}

// ---------------------------------------------------------------------------
// Suppression / invariance / MTF
// ---------------------------------------------------------------------------

TEST(GsvgGridSuppression, GridIsSuppressedByAtLeast40dB) {
    for (gd::Axis axis : {gd::Axis::Rows, gd::Axis::Columns})
        for (double lpi : kLpis) {
            const auto r = RunSuppression(lpi, axis);
            const auto& ar = axis == gd::Axis::Rows ? r.report.rows : r.report.cols;
            std::printf("GRIDSUP suppress axis=%s lpi=%.0f f=%.5f prom=%.3g level=%d filtered=%d "
                        "baseline=%.4g before=%.4g after=%.4g after/before=%.3g after/baseline=%.3g\n",
                        Name(axis), lpi, ar.input.freq, ar.input.prominence, ar.place.level,
                        ar.filteredLevels, r.baseline, r.before, r.after, r.after / r.before,
                        r.after / r.baseline);
            EXPECT_TRUE(ar.input.detected) << lpi;
            EXPECT_GE(ar.filteredLevels, 1) << lpi;
            EXPECT_TRUE(Suppressed(r)) << lpi;
        }
}

// REQ-GSVG-005, provisional regression floor (QA-B-109, #180).
// NOT a clinical pass mark — it only says "no worse than today".
// #151 실제 장비 영상 확보 시 재설정.
//
// Bound to the SYNTHETIC scene built in this file (seed 180), not to the MC
// phantoms, so replacing those does not touch this floor (QA-B-118). The
// metrics are energy ratios over a band, not extremes, and the values do not
// move between runs. What does move them is the scene: across four noise seeds
// after/before held to about 1 % while after/baseline swung 187..547 -- which
// is why the tight floor sits on after/before (QA-B-109).
//
// The card's target was "near the grid-free baseline". With the design
// document's sigma_f = 1.5 bins the residual stays 21 .. 381 x above it
// (run 5); wider band-stops reach it and cost MTF (ReportSigmaAndDomainSweep).
//
// Two floors, because the two ratios behave very differently (QA-B-109 §1):
//  * after/before is stable — across four noise seeds it moved by <= 1 %
//    (2.15e-5 .. 8.11e-5 over all axis/lpi). Floor 1.0e-4 = 1.23 x the largest
//    value measured on any seed, so the margin is ~20 x the measured spread.
//  * after/baseline swings with the scene, because `baseline` is the tiny
//    residual of a grid-free image: the same cell measured 187 .. 547 across
//    seeds (rows, 103 lpi). Floor 600 = 1.10 x the largest value measured on
//    any seed. A tighter floor here would fire on a scene change, not on a
//    regression, which is why the stable guard above carries the real weight.
TEST(GsvgGridSuppression, ProvisionalFloor_ResidualGridEnergy_REQ_GSVG_005) {
    for (gd::Axis axis : {gd::Axis::Rows, gd::Axis::Columns})
        for (double lpi : kLpis) {
            const auto r = RunSuppression(lpi, axis);
            const double toBaseline = r.after / std::max(r.baseline, 1e-12);
            std::printf("GRIDSUP floor005 axis=%s lpi=%.0f after/before=%.4g after/baseline=%.4g\n",
                        Name(axis), lpi, r.after / r.before, toBaseline);
            EXPECT_LT(r.after / r.before, 1.0e-4) << Name(axis) << " " << lpi;
            EXPECT_LT(toBaseline, 600.0) << Name(axis) << " " << lpi;
            // Recorded, not a target: the residual is still far above the
            // grid-free baseline. Kept so an improvement shows up as a change.
            EXPECT_GT(r.after, 10.0 * std::max(r.baseline, 1.0)) << Name(axis) << " " << lpi;
        }
}

TEST(GsvgGridSuppression, GridFreeImagesAreLeftByteIdentical) {
    for (unsigned seed : {1u, 2u, 3u, 180u})
        for (double noise : {0.0, 30.0, 300.0}) {
            const auto in = ToU16(Background(seed, noise));
            auto out = in;
            const auto rep = gd::SuppressGrid(out.data(), kN, kN);
            std::printf("GRIDSUP invariant seed=%u noise=%.0f prom rows=%.3g cols=%.3g detected=%d/%d\n",
                        seed, noise, rep.rows.input.prominence, rep.cols.input.prominence,
                        rep.rows.input.detected, rep.cols.input.detected);
            EXPECT_EQ(in, out) << "seed " << seed << " noise " << noise;
        }
}

// Horizontal grid lines, near-vertical edge: the band-stop runs across the
// lines, along the edge, and leaves the edge's MTF alone.
TEST(GsvgGridSuppression, MtfLossStaysUnderFivePercentForLinesAlongTheEdgeNormal) {
    for (double lpi : kLpis) {
        gd::Report dec;
        const auto bands = MtfLoss(gd::Axis::Rows, lpi, {}, false, &dec);
        std::printf("GRIDSUP mtf grid=rows lpi=%.0f decisions=%zu loss:", lpi, dec.decisions.size());
        for (const auto& b : bands) std::printf(" [%.1f,%.1f)=%.4f", b.lo, b.hi, b.worstLoss);
        std::printf("\n");
        EXPECT_GE(dec.decisions.size(), 1u) << lpi;
        for (const auto& b : bands) EXPECT_LT(b.worstLoss, 0.05) << lpi << " band " << b.lo;
    }
}

// REQ-GSVG-006, provisional regression floor (QA-B-109, #180).
// NOT a clinical pass mark — it only says "no worse than today".
// #151 실제 장비 영상 확보 시 재설정.
//
// Bound to the SYNTHETIC slanted edge built in this file, not to the MC
// phantoms (QA-B-118). Worst loss IS an extreme (a max over bands, each a max
// over frequencies), so it is the one of these floors most like 018 -- but the
// scene carries no noise, and its measurement condition, the edge angle, moved
// it only 0.1071..0.1153 over 2.5..4.0 degrees (QA-B-109).
//
// Vertical grid lines on a near-vertical edge: the band-stop runs across the
// edge. SPEC-XPE-GSVG 006 (< 5 %) is missed in the bands the notches fall in
// (0.112 / 0.114 at 60 / 103 lpi, run 5). At 200 lpi the edge keeps the
// 3-sigma sub-band check from firing, so nothing is filtered at all.
//
// Floor 0.125. The MTF scene is noiseless, so its measurement condition is the
// slanted-edge angle: over 2.5 .. 4.0 degrees the worst loss measured
// 0.1071 .. 0.1153 (QA-B-109 §1), a spread of about +-4 %. The floor is
// 1.08 x the largest of those, so it clears the measured spread twice over.
TEST(GsvgGridSuppression, ProvisionalFloor_MtfLossAcrossTheEdge_REQ_GSVG_006) {
    for (double lpi : kLpis) {
        gd::Report dec;
        const auto bands = MtfLoss(gd::Axis::Columns, lpi, {}, false, &dec);
        const auto naive = MtfLoss(gd::Axis::Columns, lpi, {}, true);
        std::printf("GRIDSUP mtf grid=cols lpi=%.0f decisions=%zu loss:", lpi, dec.decisions.size());
        for (const auto& b : bands) std::printf(" [%.1f,%.1f)=%.4f", b.lo, b.hi, b.worstLoss);
        std::printf(" naive_worst=%.4f\n", Worst(naive));
        EXPECT_LT(Worst(bands), 0.125) << lpi;
        if (lpi < 150.0) {
            EXPECT_GE(dec.decisions.size(), 1u) << lpi;
            // Recorded, not a target: 006 is still missed on this axis.
            EXPECT_GT(Worst(bands), 0.05) << lpi;
        } else {
            EXPECT_EQ(dec.decisions.size(), 0u) << lpi;
        }
    }
}

// REQ-GSVG-008, provisional regression floor (QA-B-109, #180).
// NOT a clinical pass mark — it only says "no worse than today".
// #151 실제 장비 영상 확보 시 재설정.
//
// Bound to the SYNTHETIC scene in this file, not to the MC phantoms
// (QA-B-118). Energy ratios, not extremes; unchanged between runs; across four
// noise seeds the spread was at most +-6 % (QA-B-109). The detected flags are
// discrete and would flip only if the detector's reach changed.
//
// The severely aliased band. Per-lpi floors on after/before, because the
// aliased frequency lands on a different sub-band at each line density and the
// values are three orders of magnitude apart. Measured across four noise seeds
// (QA-B-109 §1) the spread was at most +-6 % (170 lpi: 1.43e-5 .. 1.62e-5);
// each floor below is 1.2 .. 1.25 x the largest value measured on any seed.
//
// 183 lpi aliases to 0.0015 cyc/px — below the detector's reach, so nothing is
// filtered and the ratio is 1. That is recorded as the current state, not
// accepted: a floor of 1.0 only forbids it getting worse.
TEST(GsvgGridSuppression, ProvisionalFloor_SevereAliasing_REQ_GSVG_008) {
    struct Case { double lpi; double floorRatio; int detected; };
    const Case cases[] = {
        {170.0, 2.0e-5, 1},
        {175.0, 6.0e-5, 1},
        {180.0, 0.99,   1},   // filtered at level 6 only; barely moves
        {183.0, 1.0,    0},   // not detected at all
        {186.0, 0.20,   1},
    };
    for (const Case& c : cases) {
        const auto r = RunSuppression(c.lpi, gd::Axis::Rows);
        const double ratio = r.after / r.before;
        std::printf("GRIDSUP floor008 lpi=%.0f detected=%d level=%d filtered=%d after/before=%.4g\n",
                    c.lpi, r.report.rows.input.detected, r.report.rows.place.level,
                    r.report.rows.filteredLevels, ratio);
        EXPECT_EQ(r.report.rows.input.detected ? 1 : 0, c.detected) << c.lpi;
        EXPECT_LE(ratio, c.floorRatio) << c.lpi;
    }
}

// Reported: band-stop width and linear/log domain (the MTF column is for
// vertical lines on the edge; horizontal lines measured 0.0000 throughout).
TEST(GsvgGridSuppression, ReportSigmaAndDomainSweep) {
    for (double sigma : {1.5, 3.0, 6.0})
        for (bool logDomain : {false, true}) {
            gd::Options o;
            o.sigmaBins = sigma;
            o.logDomain = logDomain;
            for (gd::Axis axis : {gd::Axis::Rows, gd::Axis::Columns})
                for (double lpi : kLpis) {
                    const auto r = RunSuppression(lpi, axis, o);
                    const double mtf = axis == gd::Axis::Columns ? Worst(MtfLoss(axis, lpi, o)) : 0.0;
                    std::printf("GRIDSUP sweep sigma=%.1f log=%d axis=%s lpi=%.0f after/before=%.3g "
                                "after/baseline=%.3g mtf_worst=%.4f\n",
                                sigma, logDomain, Name(axis), lpi, r.after / r.before,
                                r.after / r.baseline, mtf);
                }
        }
}

// Reported: the smallest modulation depth the input gate picks up.
TEST(GsvgGridSuppression, ReportDetectionDepth) {
    for (double d : {0.0002, 0.0005, 0.001, 0.002, 0.005})
        for (double lpi : kLpis) {
            const auto r = RunSuppression(lpi, gd::Axis::Rows, {}, d);
            std::printf("GRIDSUP depth d=%.4f lpi=%.0f prom=%.3g amp=%.3g detected=%d after/before=%.3g\n",
                        d, lpi, r.report.rows.input.prominence, r.report.rows.input.amplitude,
                        r.report.rows.input.detected, r.after / r.before);
        }
}

// ---------------------------------------------------------------------------
// Falsification
// ---------------------------------------------------------------------------

// Band-stop off: detection and decomposition still run, nothing is filtered.
// The suppression criterion must fail; invariance must still hold.
TEST(GsvgGridSuppression, WithoutBandStopOnlySuppressionFails) {
    gd::Options off;
    off.bandStop = false;
    for (double lpi : kLpis) {
        const auto r = RunSuppression(lpi, gd::Axis::Rows, off);
        std::printf("GRIDSUP nobandstop lpi=%.0f after/before=%.3g\n", lpi, r.after / r.before);
        EXPECT_FALSE(Suppressed(r)) << lpi;
    }
    const auto in = ToU16(Background());
    auto out = in;
    gd::SuppressGrid(out.data(), kN, kN, off);
    EXPECT_EQ(in, out);
}

// Auto-stop off: keep decomposing to the last level. On a grid-free image no
// axis passes the input gate, so this switch has nothing to act on and
// invariance still holds -- recorded as the result, not hidden.
TEST(GsvgGridSuppression, WithoutAutoStopInvarianceStillHolds) {
    gd::Options off;
    off.autoStop = false;
    const auto in = ToU16(Background());
    auto out = in;
    gd::SuppressGrid(out.data(), kN, kN, off);
    EXPECT_EQ(in, out);
    const auto r = RunSuppression(103.0, gd::Axis::Rows, off);
    std::printf("GRIDSUP noautostop lpi=103 filtered=%d checks=%zu after/before=%.3g\n",
                r.report.rows.filteredLevels, r.report.rows.checks.size(), r.after / r.before);
}

// Input gate off: every level's detail band is judged by the 3-sigma rule
// alone, anywhere in its spectrum. That is what the invariance depends on.
TEST(GsvgGridSuppression, WithoutInputGateGridFreeImagesChange) {
    gd::Options off;
    off.inputGate = false;
    const auto in = ToU16(Background());
    auto out = in;
    const auto rep = gd::SuppressGrid(out.data(), kN, kN, off);
    size_t diff = 0;
    for (size_t i = 0; i < in.size(); ++i) diff += in[i] != out[i];
    std::printf("GRIDSUP noinputgate filtered rows=%d cols=%d differing=%zu\n",
                rep.rows.filteredLevels, rep.cols.filteredLevels, diff);
    EXPECT_GT(diff, 0u);
}

// ---------------------------------------------------------------------------
// Aliasing close to DC (reported, not a pass criterion)
// ---------------------------------------------------------------------------
TEST(GsvgGridSuppression, ReportSevereAliasing) {
    for (double lpi : {170.0, 175.0, 180.0, 183.0, 186.0}) {
        const auto r = RunSuppression(lpi, gd::Axis::Rows);
        std::printf("GRIDSUP alias lpi=%.0f f_cpx=%.5f detected=%d level=%d filtered=%d "
                    "baseline=%.4g before=%.4g after=%.4g\n",
                    lpi, AliasedFrequencyPerMm(lpi, kPitch) * kPitch, r.report.rows.input.detected,
                    r.report.rows.place.level, r.report.rows.filteredLevels, r.baseline, r.before, r.after);
    }
}

// ---------------------------------------------------------------------------
// REQ-GSVG-019: measure only, 3072x3072, through the public API.
// ---------------------------------------------------------------------------
TEST(GsvgGridSuppression, BenchmarkFreeze_Performance_REQ_GSVG_019_GridDwt3072) {
    constexpr int k = 3072;
    GridSpec g;   // 103 lpi, 0.139 mm, rows, d 0.05
    const auto src = ToU16(ApplyGrid(MakeAnatomyBackground(k, k, 19u, 30.0), g));
    std::vector<uint16_t> dst(src.size());
    void* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_gsvg_init(&handle, "{\"grid_suppression\": true}"));
    perf_measure::Measure("REQ-GSVG-019/xpe_gsvg_process_grid", "3072x3072", [] {},
                          [&] { return xpe_gsvg_process(handle, src.data(), src.size(), dst.data(),
                                                        dst.size(), k, k, nullptr, 0u); });
    const double f = AliasedFrequencyPerMm(g.linesPerInch, g.pitchMm);
    const double before = ResidualGridEnergy(FromU16(src, k, k), GridAxis::Rows, f, kPitch).ratio;
    const double after = ResidualGridEnergy(FromU16(dst, k, k), GridAxis::Rows, f, kPitch).ratio;
    std::printf("GRIDSUP bench3072 ratio before=%.4g after=%.4g\n", before, after);
    EXPECT_LT(after, kSuppressionRatio * before);
    xpe_gsvg_shutdown(handle);
}
