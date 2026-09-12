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
