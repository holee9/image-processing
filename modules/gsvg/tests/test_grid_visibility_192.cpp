// #192 (QA-B-151): at the product pixel pitch, is there anything LEFT to suppress?
//
// THIS FILE DOES NOT TEST DETECTION. #192 records that a 180 lpi grid stops
// being detected when the scene moves from 0.139 mm to 0.140 mm, and the
// issue's own most important open question is whether that matters:
//
//   "If the alias folds near DC the grid pattern is not visible either, in
//    which case this is not a detection failure but nothing to suppress."
//
// Widening the detection band before answering that would make the module hunt
// for something that is not there and raise false positives on anatomy.
//
// THE CRITERION WAS FIXED BEFORE ANY NUMBER WAS TAKEN and is recorded in
// .moai/reports/lane-post/QA-B-151/criterion.md, committed ahead of this file:
// peak-to-peak modulation below 0.75 % counts as "nothing to suppress", where
// 0.75 % is one JND at 100 cd/m^2 computed from DICOM PS3.14 Equation 7-1 (the
// same equation QA-B-142/144 checked against Table B-1). It is a conservative
// threshold: human contrast sensitivity at the 0.5..5 lp/cm the alias lands in
// is BELOW its peak, so the real visibility floor is higher than one JND.
//
// THE APERTURE FORK, also written down in advance. ApplyGrid point-samples --
// GridFactor is evaluated once at the pixel centre. A real detector integrates
// over the pixel area, which multiplies a sinusoid's amplitude by
// sinc(pi*f*a). The two models can disagree, and which one is physical depends
// on the aperture fill factor -- exactly the detector data #192 is blocked on.
// Both are measured here.

#include "grid_test_tools.h"
#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace gsvg_test;

namespace {

constexpr int kN = 1024;
constexpr double kDepth = 0.05;          // the depth the suppression suite uses
constexpr double kPi = 3.14159265358979323846;

// One JND at 100 cd/m^2, from PS3.14 Eq 7-1. See criterion.md.
constexpr double kOneJndPercent = 0.75;
constexpr double kVisiblePercent = 2.0;

// Alias frequency, derived here rather than through the repository's
// AliasedFrequencyPerMm: #192's table was produced by the lead and a test that
// recomputes it through the same helper would be agreeing with itself.
double FoldedFreqPerMm(double lpi, double pitchMm) {
    const double fs = 1.0 / pitchMm;
    const double f  = lpi / 25.4;
    return std::fabs(f - fs * std::round(f / fs));
}

// Collapse to a 1-D profile along the grid axis: average across the other axis
// so anatomy and noise fall away and only the axis-aligned modulation is left.
std::vector<double> Profile(const Image& img, GridAxis axis) {
    const int n = (axis == GridAxis::Rows) ? img.height : img.width;
    const int m = (axis == GridAxis::Rows) ? img.width  : img.height;
    std::vector<double> prof(static_cast<size_t>(n), 0.0);
    for (int i = 0; i < n; ++i) {
        double s = 0.0;
        for (int k = 0; k < m; ++k) s += (axis == GridAxis::Rows) ? img.at(k, i) : img.at(i, k);
        prof[static_cast<size_t>(i)] = s / m;
    }
    return prof;
}

double ModulationPercent(const std::vector<double>& prof) {
    const auto mm = std::minmax_element(prof.begin(), prof.end());
    double mean = 0.0;
    for (double v : prof) mean += v;
    mean /= static_cast<double>(prof.size());
    if (mean <= 0.0) return 0.0;
    return 100.0 * (*mm.second - *mm.first) / mean;
}

// The dominant non-DC frequency of the profile, in cycles/mm, found by a
// direct DFT scan. This is the OBSERVED alias -- it does not consult
// FoldedFreqPerMm, so the two can be compared against each other.
double DominantFreqPerMm(const std::vector<double>& prof, double pitchMm) {
    const size_t n = prof.size();
    double mean = 0.0;
    for (double v : prof) mean += v;
    mean /= static_cast<double>(n);

    double best = 0.0, bestMag = -1.0;
    for (size_t k = 1; k < n / 2; ++k) {
        double re = 0.0, im = 0.0;
        const double w = 2.0 * kPi * static_cast<double>(k) / static_cast<double>(n);
        for (size_t i = 0; i < n; ++i) {
            const double v = prof[i] - mean;
            re += v * std::cos(w * static_cast<double>(i));
            im -= v * std::sin(w * static_cast<double>(i));
        }
        const double mag = std::sqrt(re * re + im * im);
        if (mag > bestMag) { bestMag = mag; best = static_cast<double>(k); }
    }
    // k cycles over n samples of pitchMm each.
    return best / (static_cast<double>(n) * pitchMm);
}

// A flat field with the grid applied. Flat on purpose: anatomy would add its
// own low-frequency content to the very band the alias lands in, and the
// question here is the amplitude of the grid's own residue.
// Aperture width as a FRACTION of the pitch. 1.0 = the photodiode spans the
// whole pixel; real panels are lower because of readout structure between
// cells, and the exact value is a detector property (#192, item 2).
Image FlatWithGridFill(double lpi, double pitchMm, double depth, double fillFactor) {
    Image img(kN, kN, 1000.0);
    GridSpec g;
    g.linesPerInch = lpi;
    g.pitchMm      = pitchMm;
    g.axis         = GridAxis::Rows;
    const double f = lpi / 25.4;
    const double x = kPi * f * pitchMm * fillFactor;
    g.depth = depth * ((x == 0.0) ? 1.0 : std::sin(x) / x);
    return ApplyGrid(img, g);
}

Image FlatWithGrid(double lpi, double pitchMm, double depth, bool apertureIntegrated) {
    Image img(kN, kN, 1000.0);
    GridSpec g;
    g.linesPerInch = lpi;
    g.pitchMm      = pitchMm;
    g.axis         = GridAxis::Rows;
    g.depth        = depth;

    if (apertureIntegrated) {
        // Box-average the sinusoid over one pixel width: the mean of
        // sin(2*pi*f*t + phi) over [t0, t0+a] is sinc(pi*f*a) times its value
        // at the interval centre. So a full-fill aperture is the point sample
        // with the depth scaled -- exact, no supersampling needed.
        const double f = lpi / 25.4;
        const double x = kPi * f * pitchMm;
        const double sinc = (x == 0.0) ? 1.0 : std::sin(x) / x;
        g.depth = depth * sinc;
    }
    return ApplyGrid(img, g);
}

struct Case { double lpi; double pitch; };

}  // namespace

// ---------------------------------------------------------------------------
// Premise 2 of the card: the lead's alias table, observed rather than recomputed.
// ---------------------------------------------------------------------------
TEST(GridVisibility192, ObservedAliasFrequencyMatchesTheFoldingPrediction_192) {
    const Case cases[] = {
        {170.0, 0.139}, {170.0, 0.140},
        {180.0, 0.139}, {180.0, 0.140},
        {186.0, 0.139}, {186.0, 0.140},
    };
    std::printf("GV192 alias: lpi pitch predicted_lp_per_cm observed_lp_per_cm cycles_across_field\n");
    for (const Case& c : cases) {
        const Image img = FlatWithGrid(c.lpi, c.pitch, kDepth, /*aperture=*/false);
        const std::vector<double> prof = Profile(img, GridAxis::Rows);
        const double observed = DominantFreqPerMm(prof, c.pitch);
        const double predicted = FoldedFreqPerMm(c.lpi, c.pitch);
        std::printf("GV192 alias %3.0f %.3f  %.3f  %.3f  %.1f\n",
                    c.lpi, c.pitch, predicted * 10.0, observed * 10.0,
                    observed * kN * c.pitch);
        // The DFT bin spacing is 1/(N*pitch) = 0.007 lp/mm, so agreement is
        // asserted to one bin. A mismatch here would mean the FOLDING model is
        // wrong, which is the card's third outcome and the most valuable one.
        EXPECT_NEAR(predicted, observed, 1.5 / (kN * c.pitch))
            << c.lpi << " lpi at " << c.pitch << " mm";
    }
}

// ---------------------------------------------------------------------------
// The measurement the card asks for, under both sampling models.
// ---------------------------------------------------------------------------
TEST(GridVisibility192, AliasAmplitudeAtTheProductPitch_192) {
    const Case cases[] = {
        {170.0, 0.139}, {170.0, 0.140},
        {180.0, 0.139}, {180.0, 0.140},
        {186.0, 0.139}, {186.0, 0.140},
    };

    std::printf("GV192 amp: lpi pitch point_sampled_%% aperture_integrated_%% (input depth %.1f%%)\n",
                kDepth * 100.0);
    for (const Case& c : cases) {
        const double pointPct = ModulationPercent(
            Profile(FlatWithGrid(c.lpi, c.pitch, kDepth, false), GridAxis::Rows));
        const double apPct = ModulationPercent(
            Profile(FlatWithGrid(c.lpi, c.pitch, kDepth, true), GridAxis::Rows));
        std::printf("GV192 amp %3.0f %.3f  %8.4f  %8.5f\n", c.lpi, c.pitch, pointPct, apPct);
    }

    // NO PASS/FAIL ON THE AMPLITUDES. The criterion decides an ENGINEERING
    // question (is there anything to suppress), and that verdict belongs in the
    // report and then in the SPEC -- not in a green test that would silently
    // pin whichever sampling model happens to be in the scene generator. What
    // IS asserted below is that the measurement can see anything at all.
    SUCCEED();
}

// ---------------------------------------------------------------------------
// The two controls, fixed in criterion.md before the numbers were taken.
// ---------------------------------------------------------------------------
TEST(GridVisibility192, TheAmplitudeMeasurementIsNotBlind_192) {
    // Negative control: no grid at all.
    const std::vector<double> flat = Profile(Image(kN, kN, 1000.0), GridAxis::Rows);
    const double flatPct = ModulationPercent(flat);
    std::printf("GV192 control flat-no-grid modulation=%.6f %%\n", flatPct);
    EXPECT_LT(flatPct, 1e-9) << "a field with no grid reported modulation";

    // Positive control: a frequency that is NOT near the sampling rate, so it
    // survives point sampling at full depth. Without this, a small number at
    // 180 lpi is indistinguishable from a measurement that reads zero always.
    const double lowPct = ModulationPercent(
        Profile(FlatWithGrid(60.0, 0.140, kDepth, false), GridAxis::Rows));
    std::printf("GV192 control 60lpi-0.140 modulation=%.4f %% (input depth %.1f %%)\n",
                lowPct, kDepth * 100.0);
    EXPECT_GT(lowPct, 5.0) << "the measurement cannot see a grid it should see";

    // And the aperture model must not be a constant: at 60 lpi the aperture
    // barely attenuates, at 180 lpi it attenuates hard. If both came out the
    // same the sinc factor would not be reaching the scene.
    const double apLow  = ModulationPercent(
        Profile(FlatWithGrid(60.0, 0.140, kDepth, true), GridAxis::Rows));
    const double apHigh = ModulationPercent(
        Profile(FlatWithGrid(180.0, 0.140, kDepth, true), GridAxis::Rows));
    std::printf("GV192 control aperture 60lpi=%.4f %% 180lpi=%.5f %% ratio=%.1f\n",
                apLow, apHigh, apLow / apHigh);
    EXPECT_GT(apLow / apHigh, 10.0)
        << "the aperture factor is not frequency-dependent, so it is not being applied";
}

// ---------------------------------------------------------------------------
// How much does the verdict depend on the number nobody has measured?
//
// The aperture factor is sinc(pi * f * a). At 180 lpi and 0.140 mm the grid
// frequency is 0.992 of the sampling rate, so pi*f*a sits just under pi --
// right where sinc crosses zero. The zero is at a = pitch exactly, and backing
// away from it climbs steeply, so the residue is extremely sensitive to an
// aperture width nobody in this repository has measured.
//
// This case takes no verdict. It measures how far the verdict moves when the
// unknown moves.
// ---------------------------------------------------------------------------
TEST(GridVisibility192, TheVerdictTurnsOnTheApertureFillFactor_192) {
    std::printf("GV192 fill: lpi pitch fill modulation_pct verdict (1JND=%.2f pct)\n",
                kOneJndPercent);
    for (const double lpi : {170.0, 180.0, 186.0}) {
        for (const double ff : {1.00, 0.95, 0.90, 0.85, 0.80, 0.70}) {
            const double pct = ModulationPercent(
                Profile(FlatWithGridFill(lpi, 0.140, kDepth, ff), GridAxis::Rows));
            const char* v = (pct < kOneJndPercent) ? "below"
                          : (pct < kVisiblePercent) ? "grey" : "VISIBLE";
            std::printf("GV192 fill %3.0f 0.140 %.2f  %8.4f  %s\n", lpi, ff, pct, v);
        }
    }
    SUCCEED();
}
