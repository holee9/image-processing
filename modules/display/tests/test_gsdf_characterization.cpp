// #142 (QA-B-57): what the GSDF calibration test does NOT check.
//
// REQ-DISP-025 requires xpe_gsdf_calibrate to "compute a GSDF-compliant
// Presentation LUT that maps JND (Just Noticeable Difference) indices to
// display digital driving levels". The existing case,
// PresentationLut.GsdfCalibrate_BasicOutput, asserts three things: the return
// code, gsdfEnabled == 1, and that the 1024 entries are monotonically
// non-decreasing.
//
// Monotonic non-decreasing is necessary and nowhere near sufficient. A straight
// ramp 0, 64, 128, ... passes it, and a straight ramp is precisely what GSDF is
// not: the point of the standard's curve is that equal steps in index are equal
// steps in PERCEIVED brightness, which is non-linear in luminance. So the
// strongest existing assertion would survive the calibration being replaced by
// a linear interpolation -- the same shape of gap QA-B-45 closed on the DICOM
// side by comparing pixels byte-for-byte instead of checking a return code.
//
// These cases were written to characterise the curve actually produced. The
// measurement came back as follows (_gsdf.log):
//
//     max deviation from the straight line through its own endpoints: 0.5
//     of 65535 -- 0.0008% of span
//     step size across all 1023 steps: min 64, max 65
//
// THE LUT IS A LINEAR RAMP. The cause is visible in the source and is exact
// rather than approximate (presentation_lut.cpp:131-146):
//
//     target_jnd = jnd_min + (i/1023) * jnd_range
//     t          = (target_jnd - jnd_min) / jnd_range        ==  i/1023
//     log_L      = log_lmin + t * (log_lmax - log_lmin)
//     ddl        = ((log_L - log_lmin)/(log_lmax - log_lmin)) * 65535  ==  t * 65535
//
// t is reconstructed from the value it was just used to build, so the JND model
// -- the whole cubic in log10(L) at :113-118 -- cancels out and never reaches
// the output. ddl = round(i/1023 * 65535), which is what 0.5 and 64/65 are.
// The luminance measurements cancel with it: they enter only through log_lmin
// and log_lmax, which appear in numerator and denominator alike.
//
// This is the QA-B-56 shape again. There, EIT was looked up and then cancelled
// out of DI; here, a perceptual model is computed and then cancelled out of the
// LUT. In both cases the code that does the work is present, runs, and has no
// effect on the answer -- which is why reading it convinces and only arithmetic
// catches it.
//
// NOTHING IS FIXED HERE. The presentation LUT decides displayed pixel values,
// so changing it changes what a radiologist sees; QA-B-56 established that such
// a change is a decision, not a test. These cases pin today's output so the
// decision arrives as a visible change, and they do NOT assert conformance to
// IEC 62563-1 / DICOM PS3.14 -- the standard's tabulated curve is not in this
// repository, and QA-B-56 recorded what it costs to argue from a formula nobody
// has checked against its source.

#include <gtest/gtest.h>

#include "xpe/display/display_api.h"
#include "xpe/common/xpe_error.h"

#include <cmath>
#include <cstdint>
#include <vector>

namespace {

XpePresentationLutParams CalibrateOverDecades() {
    // 1 .. 500 cd/m^2 -- a bit under three decades, the range a diagnostic
    // monitor calibration actually spans.
    const float lum[5] = {1.0f, 10.0f, 50.0f, 200.0f, 500.0f};
    XpePresentationLutParams out{};
    EXPECT_EQ(XPE_OK, xpe_gsdf_calibrate(lum, 5, &out));
    return out;
}

}  // namespace

// KnownDivergence_ (QA-B-57): the produced LUT is a straight ramp, so the
// perceptual curve REQ-DISP-025 asks for is not in the output.
//
// GsdfCalibrate_BasicOutput passes on this, because monotonically
// non-decreasing is exactly what a ramp is. That assertion would survive the
// calibration being deleted and replaced with a loop that writes i*64.
TEST(GsdfCharacterization, KnownDivergence_LutIsALinearRamp) {
    const XpePresentationLutParams lut = CalibrateOverDecades();

    const float first = static_cast<float>(lut.lutData[0]);
    const float last  = static_cast<float>(lut.lutData[1023]);
    ASSERT_LT(first, last) << "the LUT is flat; there is no curve to characterise";

    float maxDev = 0.0f;
    int   maxAt  = 0;
    for (int i = 0; i < 1024; ++i) {
        const float straight = first + (last - first) * (static_cast<float>(i) / 1023.0f);
        const float dev = std::fabs(static_cast<float>(lut.lutData[i]) - straight);
        if (dev > maxDev) { maxDev = dev; maxAt = i; }
    }
    const float span = last - first;
    GTEST_LOG_(INFO) << "GSDF LUT: first=" << first << " last=" << last
                     << " max deviation from straight line=" << maxDev
                     << " (" << (100.0f * maxDev / span) << "% of span) at index " << maxAt;

    // Rounding alone accounts for half an output unit. Anything a perceptual
    // curve did would be orders of magnitude larger.
    EXPECT_LE(maxDev, 1.0f)
        << "the LUT now departs from a straight line -- a perceptual curve has "
           "reached the output; say how, and retire this case";
}

// The same statement as a step size: a ramp has one step, give or take rounding.
TEST(GsdfCharacterization, KnownDivergence_StepSizeIsConstantUpToRounding) {
    const XpePresentationLutParams lut = CalibrateOverDecades();

    int minStep = INT32_MAX, maxStep = 0;
    for (int i = 1; i < 1024; ++i) {
        const int step = static_cast<int>(lut.lutData[i]) - static_cast<int>(lut.lutData[i - 1]);
        if (step < minStep) minStep = step;
        if (step > maxStep) maxStep = step;
    }
    GTEST_LOG_(INFO) << "GSDF LUT step size: min=" << minStep << " max=" << maxStep;

    // 65535/1023 = 64.06, so a pure ramp alternates 64 and 65 and nothing else.
    EXPECT_LE(maxStep - minStep, 1)
        << "step size now varies by more than rounding -- the curve is no longer "
           "linear; say how, and retire this case";
}

// And the luminance measurements barely reach the output: only the rounding
// pattern moves when the calibration range changes by two decades.
TEST(GsdfCharacterization, KnownDivergence_MeasurementsBarelyChangeTheCurve) {
    const XpePresentationLutParams wide = CalibrateOverDecades();

    const float narrow_lum[3] = {80.0f, 100.0f, 120.0f};   // well under one decade
    XpePresentationLutParams narrow{};
    ASSERT_EQ(XPE_OK, xpe_gsdf_calibrate(narrow_lum, 3, &narrow));

    int differing = 0;
    int maxDelta = 0;
    for (int i = 0; i < 1024; ++i) {
        const int d = std::abs(static_cast<int>(wide.lutData[i]) -
                               static_cast<int>(narrow.lutData[i]));
        if (d != 0) ++differing;
        if (d > maxDelta) maxDelta = d;
    }
    GTEST_LOG_(INFO) << "entries differing between a ~3-decade and a sub-decade "
                        "calibration: " << differing << " of 1024"
                     << ", largest difference=" << maxDelta;

    // Both calibrations reduce to the same ramp; what differs is float rounding,
    // never more than one output unit.
    EXPECT_LE(maxDelta, 1)
        << "the luminance measurements now move the curve by more than rounding "
           "-- they are reaching the output; say how, and retire this case";
}

// ===========================================================================
// #155 (QA-B-141): the one-axis sweep, and the CONTROL the earlier cases lack.
//
// KnownDivergence_MeasurementsBarelyChangeTheCurve above compares TWO luminance
// inputs and finds them a rounding unit apart. That is an ABSENCE assertion,
// and an absence assertion with no control passes just as happily when the
// measurement is blind -- the QA-B-58 lesson, and the QA-B-139 one. Two things
// are added here and nothing is fixed:
//
//   (1) a sweep over SIX luminance configurations, one axis, so "it does not
//       move" is read off a range rather than off a single pair; and
//
//   (2) a control that shows the metric CAN see a difference, by building the
//       LUT the code's own model would produce if the model reached the output,
//       and measuring the distance to the shipped one.
//
// THE CONTROL IS NOT A CONFORMANCE REFERENCE. It inverts the cubic THIS FILE'S
// SOURCE already computes (presentation_lut.cpp:113-118) and then keeps the
// code's own final mapping (log10 L linear onto [0, 65535]) unchanged. It is
// the minimal edit that would make the computed model reach the answer -- so
// the distance to it is what the cancellation costs, in output counts, and
// nothing more. Whether DICOM PS3.14 wants THAT curve is not decided here; the
// standard's tabulated values are still not in this repository (file header).
//
// The cubic is strictly increasing, so the inverse is well defined and a
// bisection finds it: p'(x) = 214.494x^2 - 189.186x + 41.912 has discriminant
// 189.186^2 - 4*214.494*41.912 = -173 < 0, so p' has no real root and never
// changes sign; p'(0) = 41.912 > 0 makes that sign positive.
// ===========================================================================
namespace {

float JndFromLog10L(float x) {
    return 71.498f * x * x * x - 94.593f * x * x + 41.912f * x + 9.8212f;
}

// log10(L) such that JndFromLog10L(log10 L) == target, searched inside
// [lo, hi]. Strictly increasing, so bisection converges.
float Log10LFromJnd(float target, float lo, float hi) {
    for (int it = 0; it < 200; ++it) {
        const float mid = 0.5f * (lo + hi);
        if (JndFromLog10L(mid) < target) lo = mid; else hi = mid;
    }
    return 0.5f * (lo + hi);
}

// The LUT the shipped code would emit if the JND model reached the output:
// same target-JND schedule, same final log10 L -> DDL mapping, but the inverse
// is the REAL inverse instead of the identity the source reconstructs.
std::vector<uint16_t> ModelRespectingLut(float lumMin, float lumMax) {
    const float logLo = std::log10(lumMin), logHi = std::log10(lumMax);
    const float jndLo = JndFromLog10L(logLo), jndHi = JndFromLog10L(logHi);
    std::vector<uint16_t> out(1024);
    for (int i = 0; i < 1024; ++i) {
        const float target = jndLo + (static_cast<float>(i) / 1023.0f) * (jndHi - jndLo);
        const float logL = Log10LFromJnd(target, logLo, logHi);
        const float ddl = ((logL - logLo) / (logHi - logLo)) * 65535.0f;
        int v = static_cast<int>(std::lround(ddl));
        if (v < 0) v = 0;
        if (v > 65535) v = 65535;
        out[static_cast<size_t>(i)] = static_cast<uint16_t>(v);
    }
    return out;
}

int MaxDeviationFromStraightRamp(const XpePresentationLutParams& lut) {
    int worst = 0;
    for (int i = 0; i < 1024; ++i) {
        const int ramp = static_cast<int>(std::lround(
            static_cast<double>(i) / 1023.0 * 65535.0));
        worst = std::max(worst, std::abs(static_cast<int>(lut.lutData[i]) - ramp));
    }
    return worst;
}

}  // namespace

TEST(GsdfCharacterization, KnownDivergence_LuminanceSweepDoesNotMoveTheCurve_155) {
    struct Case { const char* name; std::vector<float> lum; };
    const std::vector<Case> cases = {
        { "sub-decade  80..120",     {80.0f, 100.0f, 120.0f} },
        { "1 decade    10..100",     {10.0f, 30.0f, 100.0f} },
        { "3 decades   1..500",      {1.0f, 10.0f, 50.0f, 200.0f, 500.0f} },
        { "4 decades   0.5..5000",   {0.5f, 5.0f, 50.0f, 500.0f, 5000.0f} },
        { "6 decades   0.01..10000", {0.01f, 1.0f, 100.0f, 10000.0f} },
        { "dim pair    0.05..0.5",   {0.05f, 0.2f, 0.5f} },
    };

    std::vector<XpePresentationLutParams> luts;
    GTEST_LOG_(INFO) << "#155 luminance sweep -- one axis, everything else fixed";
    for (const Case& c : cases) {
        XpePresentationLutParams p{};
        ASSERT_EQ(XPE_OK, xpe_gsdf_calibrate(c.lum.data(),
                                             static_cast<uint32_t>(c.lum.size()), &p))
            << c.name;
        const int dev = MaxDeviationFromStraightRamp(p);
        GTEST_LOG_(INFO) << "  " << c.name
                         << "  max|LUT - straight ramp| = " << dev << " of 65535";
        EXPECT_LE(dev, 1)
            << c.name << ": the curve has left the straight ramp (deviation "
            << dev << ") -- the model is reaching the output; say how and retire this";
        luts.push_back(p);
    }

    int worstPair = 0;
    size_t wi = 0, wj = 0;
    for (size_t a = 0; a < luts.size(); ++a)
        for (size_t b = a + 1; b < luts.size(); ++b)
            for (int i = 0; i < 1024; ++i) {
                const int d = std::abs(static_cast<int>(luts[a].lutData[i]) -
                                       static_cast<int>(luts[b].lutData[i]));
                if (d > worstPair) { worstPair = d; wi = a; wj = b; }
            }
    GTEST_LOG_(INFO) << "largest difference between ANY two of the six LUTs: "
                     << worstPair << " (" << cases[wi].name << " vs " << cases[wj].name << ")";
    EXPECT_LE(worstPair, 1)
        << "two calibrations now differ by more than rounding -- the luminance "
           "measurements reach the output; say how and retire this case";
}

// The control. Without it the case above is an absence assertion with nothing
// proving the metric can fire.
TEST(GsdfCharacterization, KnownDivergence_WhatTheCancelledModelWouldHaveCost_155) {
    const float lum[5] = {1.0f, 10.0f, 50.0f, 200.0f, 500.0f};   // 1..500 cd/m^2
    XpePresentationLutParams shipped{};
    ASSERT_EQ(XPE_OK, xpe_gsdf_calibrate(lum, 5, &shipped));

    const std::vector<uint16_t> modelled = ModelRespectingLut(1.0f, 500.0f);

    int worst = 0, worstAt = -1;
    long long sumAbs = 0;
    for (int i = 0; i < 1024; ++i) {
        const int d = std::abs(static_cast<int>(shipped.lutData[i]) -
                               static_cast<int>(modelled[static_cast<size_t>(i)]));
        sumAbs += d;
        if (d > worst) { worst = d; worstAt = i; }
    }

    int modelDev = 0;
    for (int i = 0; i < 1024; ++i) {
        const int ramp = static_cast<int>(std::lround(
            static_cast<double>(i) / 1023.0 * 65535.0));
        modelDev = std::max(modelDev,
                            std::abs(static_cast<int>(modelled[static_cast<size_t>(i)]) - ramp));
    }

    GTEST_LOG_(INFO) << "1..500 cd/m^2:"
                     << "  shipped vs model-respecting: max=" << worst
                     << " at index " << worstAt
                     << ", mean=" << (static_cast<double>(sumAbs) / 1024.0)
                     << "  |  model-respecting vs straight ramp: max=" << modelDev;

    // THE CONTROL: a LUT built from the same inputs, differing only in whether
    // the model reaches the output, is far from the shipped one. So "the
    // shipped LUT does not move" is a measured fact, not a blind metric.
    EXPECT_GT(worst, 100)
        << "the model-respecting LUT is within " << worst << " counts of the "
           "shipped ramp -- then the comparison above proves nothing, and this "
           "control must be rebuilt before the absence claim is believed";

    // And the same number stated the other way: the model's own curve is NOT a
    // ramp, which is why its cancellation is visible at all.
    EXPECT_GT(modelDev, 100)
        << "inverting the cubic produced a straight ramp too -- then the model "
           "was never capable of bending the curve and #155's framing is wrong";
}

// ===========================================================================
// #155 (QA-B-142): the control, rebuilt from DICOM PS3.14 instead of from the
// code's own arithmetic.
//
// QA-B-141's control inverted the cubic THIS MODULE computes. That proved the
// cancellation is real and cost ~45 % of full scale, but it could not say what
// the curve SHOULD be -- it was the module's own opinion, applied.
//
// Source: DICOM PS3.14 Grayscale Standard Display Function.
//   https://dicom.nema.org/medical/dicom/current/output/chtml/part14/chapter_7.html
//   https://dicom.nema.org/medical/dicom/current/output/chtml/part14/chapter_B.html
// Defined over L = 0.05 .. 4000 cd/m^2, carrying JND indices j = 1 .. 1023.
//
// THE FORMS BELOW WERE NOT TAKEN ON TRUST. The equations are images in the
// standard's HTML, so only the COEFFICIENTS could be read as text. Two shapes
// were tried and judged against Table B-1 and against each other, never against
// anyone's say-so:
//
//   Eq 7-1  log10 L(j): rational in ln(j), numerator a,c,e,g,m over
//           denominator 1,b,d,f,h,k -- matches all ten Table B-1 points to the
//           4 decimals the table prints (worst relative deviation 0.042 %),
//           and gives L(1023) = 3993 against the standard's stated 4000 ceiling.
//   Eq 7-2  j(L): a degree-8 POLYNOMIAL in log10(L), A + B*y + ... + I*y^8.
//           A rational shape was tried FIRST here and FAILED the round trip by
//           up to 1022 JND; the polynomial closes it to 0.09 JND of 1023. The
//           table and the round trip made that call, not the shape's pedigree.
//
// Both checks are asserted below, so a wrong transcription fails here rather
// than travelling into the comparison as a "standard" that is not one.
//
// NOTHING IN presentation_lut.cpp IS TOUCHED. Whether REQ-DISP-* asks for this
// curve is a SPEC question and is not decided here (QA-B-141, same boundary).
// ===========================================================================
namespace {

// Eq 7-1 coefficients (natural log of j).
constexpr double kA71 = -1.3011877,   kB71 = -2.5840191e-2, kC71 = 8.0242636e-2;
constexpr double kD71 = -1.0320229e-1, kE71 = 1.3646699e-1, kF71 = 2.8745620e-2;
constexpr double kG71 = -2.5468404e-2, kH71 = -3.1978977e-3, kK71 = 1.2992634e-4;
constexpr double kM71 = 1.3635334e-3;

// Eq 7-2 coefficients (log10 of L).
constexpr double kA72 = 71.498068, kB72 = 94.593053,   kC72 = 41.912053;
constexpr double kD72 = 9.8247004, kE72 = 0.28175407,  kF72 = -1.1878455;
constexpr double kG72 = -0.18014349, kH72 = 0.14710899, kI72 = -0.017046845;

// Luminance in cd/m^2 for JND index j (1..1023). Double precision, as the
// standard recommends.
double GsdfLuminance(double j) {
    const double x = std::log(j);
    const double num = kA71 + kC71 * x + kE71 * x * x + kG71 * x * x * x
                     + kM71 * x * x * x * x;
    const double den = 1.0 + kB71 * x + kD71 * x * x + kF71 * x * x * x
                     + kH71 * x * x * x * x + kK71 * x * x * x * x * x;
    return std::pow(10.0, num / den);
}

// JND index for a luminance, the standard's own inverse.
double GsdfJndIndex(double lum) {
    const double y = std::log10(lum);
    double p = kI72;
    p = p * y + kH72;
    p = p * y + kG72;
    p = p * y + kF72;
    p = p * y + kE72;
    p = p * y + kD72;
    p = p * y + kC72;
    p = p * y + kB72;
    p = p * y + kA72;
    return p;
}

// The module's own cubic, copied from presentation_lut.cpp:113-118 so the two
// can be compared without touching the product file.
double ModuleCubicJnd(double lum) {
    const double y = std::log10(lum);
    return 71.498 * y * y * y - 94.593 * y * y + 41.912 * y + 9.8212;
}

enum class DdlScale { Log10Luminance, LinearLuminance };

// The LUT the module would emit if it used the STANDARD's functions, with the
// module's own schedule kept: equal steps in JND index across the measured
// display range. `scale` is the one remaining free choice -- see the report.
std::vector<uint16_t> StandardLut(double lumMin, double lumMax, DdlScale scale) {
    const double jLo = GsdfJndIndex(lumMin), jHi = GsdfJndIndex(lumMax);
    const double logLo = std::log10(lumMin), logHi = std::log10(lumMax);
    std::vector<uint16_t> out(1024);
    for (int i = 0; i < 1024; ++i) {
        const double j = jLo + (static_cast<double>(i) / 1023.0) * (jHi - jLo);
        const double lum = GsdfLuminance(j);
        const double frac = (scale == DdlScale::Log10Luminance)
            ? (std::log10(lum) - logLo) / (logHi - logLo)
            : (lum - lumMin) / (lumMax - lumMin);
        long v = std::lround(frac * 65535.0);
        if (v < 0) v = 0;
        if (v > 65535) v = 65535;
        out[static_cast<size_t>(i)] = static_cast<uint16_t>(v);
    }
    return out;
}

struct Diff { int maxAbs; int atIndex; double mean; };

Diff Compare(const XpePresentationLutParams& lut, const std::vector<uint16_t>& ref) {
    Diff d{0, -1, 0.0};
    long long sum = 0;
    for (int i = 0; i < 1024; ++i) {
        const int v = std::abs(static_cast<int>(lut.lutData[i]) -
                               static_cast<int>(ref[static_cast<size_t>(i)]));
        sum += v;
        if (v > d.maxAbs) { d.maxAbs = v; d.atIndex = i; }
    }
    d.mean = static_cast<double>(sum) / 1024.0;
    return d;
}

}  // namespace

// The standard's own numbers, checked before anything is built on them.
TEST(GsdfCharacterization, StandardEquationsMatchTableB1_155) {
    // Annex B Table B-1, ten points, as published (four decimals).
    const struct { int j; double lum; } kTable[] = {
        {1, 0.0500}, {2, 0.0547}, {3, 0.0594}, {10, 0.0991}, {20, 0.1750},
        {30, 0.2752}, {40, 0.4019}, {50, 0.5574}, {60, 0.7440}, {70, 0.9640},
    };

    double worstRel = 0.0;
    for (const auto& t : kTable) {
        const double v = GsdfLuminance(t.j);
        // The table prints four decimals, so agreement is judged at that
        // precision: the computed value must round to the printed one.
        EXPECT_NEAR(std::round(v * 10000.0) / 10000.0, t.lum, 1e-9)
            << "Eq 7-1 disagrees with Table B-1 at j=" << t.j
            << " (computed " << v << ") -- the transcribed FORM is wrong, not the table";
        worstRel = std::max(worstRel, std::abs(v - t.lum) / t.lum);
    }
    GTEST_LOG_(INFO) << "Eq 7-1 vs Table B-1 (10 points): worst relative deviation "
                     << (worstRel * 100.0) << " %";

    // The standard's stated range ceiling, as a second independent check.
    const double top = GsdfLuminance(1023.0);
    GTEST_LOG_(INFO) << "L(j=1023) = " << top << " cd/m^2 (standard states ~4000)";
    EXPECT_NEAR(top, 4000.0, 40.0) << "the top of the JND scale is not the stated range";

    // Round trip: 7-2 must return the index 7-1 was given. This is what caught
    // a wrong shape for 7-2 -- a rational form failed here by up to 1022 JND.
    double worstJnd = 0.0;
    for (const int j : {1, 2, 3, 10, 50, 100, 300, 700, 1023}) {
        const double back = GsdfJndIndex(GsdfLuminance(j));
        worstJnd = std::max(worstJnd, std::abs(back - j));
    }
    GTEST_LOG_(INFO) << "round trip 7-1 -> 7-2: worst error " << worstJnd << " JND of 1023";
    EXPECT_LT(worstJnd, 1.0)
        << "the two equations do not invert each other -- one of the transcribed "
           "forms is wrong, and no comparison below can be trusted";
}

// The comparison the card asks for: shipped LUT against the STANDARD curve.
TEST(GsdfCharacterization, KnownDivergence_ShippedLutAgainstTheStandardCurve_155) {
    const float lum[5] = {1.0f, 10.0f, 50.0f, 200.0f, 500.0f};   // 1..500 cd/m^2
    XpePresentationLutParams shipped{};
    ASSERT_EQ(XPE_OK, xpe_gsdf_calibrate(lum, 5, &shipped));

    const std::vector<uint16_t> stdLog = StandardLut(1.0, 500.0, DdlScale::Log10Luminance);
    const std::vector<uint16_t> stdLin = StandardLut(1.0, 500.0, DdlScale::LinearLuminance);

    const Diff dLog = Compare(shipped, stdLog);
    const Diff dLin = Compare(shipped, stdLin);

    GTEST_LOG_(INFO) << "1..500 cd/m^2, shipped LUT vs the STANDARD curve:";
    GTEST_LOG_(INFO) << "  DDL linear in log10 L (the module's own final mapping): max="
                     << dLog.maxAbs << " (" << (100.0 * dLog.maxAbs / 65535.0)
                     << " % of full scale) at index " << dLog.atIndex
                     << ", mean=" << dLog.mean;
    GTEST_LOG_(INFO) << "  DDL linear in L: max=" << dLin.maxAbs << " ("
                     << (100.0 * dLin.maxAbs / 65535.0) << " %) at index "
                     << dLin.atIndex << ", mean=" << dLin.mean;

    // Both readings must be far from the shipped ramp, or the comparison is not
    // measuring what it claims.
    EXPECT_GT(dLog.maxAbs, 100)
        << "the standard curve and the shipped ramp agree to " << dLog.maxAbs
        << " counts -- then #155's premise is wrong, or this control is blind";
}

// Separate observation, kept apart from the comparison above because it is a
// different claim: the module's cubic is not a "simplified Barten model" at
// all. Its four coefficients are Eq 7-2's A, B, C, D -- in REVERSE positional
// order, with signs alternated. A is the standard's CONSTANT term and the
// module uses it on y^3; D is the standard's y^3 term and the module uses it as
// the constant. So the curve was not simplified, it was transcribed backwards.
TEST(GsdfCharacterization, KnownDivergence_ModuleCubicIsEq72Reversed_155) {
    // The pairing, stated as numbers rather than as a story.
    GTEST_LOG_(INFO) << "module cubic coefficients vs DICOM Eq 7-2:";
    GTEST_LOG_(INFO) << "  module y^3 = 71.498   <-> standard A (constant) = " << kA72;
    GTEST_LOG_(INFO) << "  module y^2 = -94.593  <-> standard B (y^1)      = " << kB72;
    GTEST_LOG_(INFO) << "  module y^1 = 41.912   <-> standard C (y^2)      = " << kC72;
    GTEST_LOG_(INFO) << "  module y^0 = 9.8212   <-> standard D (y^3)      = " << kD72;

    EXPECT_NEAR(71.498, kA72, 1e-3);
    EXPECT_NEAR(94.593, kB72, 1e-3);
    EXPECT_NEAR(41.912, kC72, 1e-3);
    EXPECT_NEAR(9.8212, kD72, 4e-3)
        << "the constant term does not match Eq 7-2's D -- the pairing claim is wrong";

    // And what the reversal costs, where it matters. Reported, not asserted as
    // a bound: the module's cubic is never used for anything (it cancels), so
    // this is the size of a defect that is currently unreachable.
    GTEST_LOG_(INFO) << "JND index for a luminance -- standard vs the module's cubic:";
    for (const double l : {0.05, 1.0, 50.0, 500.0, 4000.0}) {
        GTEST_LOG_(INFO) << "  L=" << l << "  standard j=" << GsdfJndIndex(l)
                         << "   module cubic=" << ModuleCubicJnd(l);
    }
    // At L = 1 cd/m^2 (log10 L = 0) every power term vanishes, so the two
    // reduce to their constants: the standard's A = 71.5, the module's 9.82.
    EXPECT_NEAR(GsdfJndIndex(1.0), kA72, 1e-9);
    EXPECT_NEAR(ModuleCubicJnd(1.0), 9.8212, 1e-9);
}
