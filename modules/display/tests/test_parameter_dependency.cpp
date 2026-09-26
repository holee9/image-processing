// #155 (QA-B-58): does each parameter reach the output?
//
// Two defects found within an hour shared one shape (#154, #155): a model is
// computed from real inputs, the result is fed into a later expression, and the
// inputs cancel out of it. The code runs, the value is right there in the
// debugger, and the answer does not depend on it.
//
// What let both through was the SHAPE of the tests. Monotonicity, range, and
// "the curve looks like this" all survive a cancelled model -- a characterisation
// test pins the output that the vanished input was supposed to produce, so it
// passes exactly as hard when the input stops mattering.
//
// So these cases assert a different property: vary ONE parameter, hold the rest,
// and require the output to move. That is the assertion neither #154 nor #155
// would have survived.
//
// Where the output does NOT move, that is the finding, and it is recorded here
// rather than fixed -- every one of these outputs is a displayed pixel value or
// a clinical index (#154 / #155 precedent).

#include <gtest/gtest.h>

#include "xpe/display/display_api.h"
#include "xpe/common/xpe_error.h"

#include <cmath>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {

constexpr uint32_t kW = 32, kH = 32;
constexpr size_t   kN = static_cast<size_t>(kW) * kH;

// A gradient rather than a flat field: a constant image hides any parameter
// whose effect varies across the input range, which is the class of parameter
// most likely to be silently ignored.
std::vector<float> Gradient(float lo, float hi) {
    std::vector<float> px(kN);
    for (size_t i = 0; i < kN; ++i) {
        px[i] = lo + (hi - lo) * (static_cast<float>(i) / static_cast<float>(kN - 1));
    }
    return px;
}

XpeImageBuffer WrapFloat32(std::vector<float>& px) {
    XpeImageBuffer img{};
    img.width         = kW;
    img.height        = kH;
    img.format        = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = 32;
    img.bitsStored    = 32;
    img.data          = px.data();
    img.dataSize      = static_cast<uint32_t>(px.size() * sizeof(float));
    return img;
}

// Largest absolute difference between two float runs; -1 when either call failed.
double MaxDiff(const std::vector<float>& a, const std::vector<float>& b) {
    double m = 0.0;
    for (size_t i = 0; i < a.size() && i < b.size(); ++i) {
        m = std::max(m, static_cast<double>(std::fabs(a[i] - b[i])));
    }
    return m;
}

}  // namespace

// ---------------------------------------------------------------------------
// xpe_apply_modality_lut -- rescaleSlope and rescaleIntercept
// ---------------------------------------------------------------------------
TEST(ParameterDependency, ModalityLut_SlopeAndInterceptBothReachTheOutput) {
    auto run = [](float slope, float intercept) {
        std::vector<float> px = Gradient(0.0f, 1000.0f);
        XpeImageBuffer img = WrapFloat32(px);
        XpeModalityLutParams p{};
        p.mode             = XPE_MODALITY_LUT_LINEAR;
        p.rescaleSlope     = slope;
        p.rescaleIntercept = intercept;
        EXPECT_EQ(XPE_OK, xpe_apply_modality_lut(&img, &p));
        return px;
    };

    const std::vector<float> base  = run(1.0f, 0.0f);
    const std::vector<float> slope = run(2.0f, 0.0f);
    const std::vector<float> inter = run(1.0f, 100.0f);

    GTEST_LOG_(INFO) << "modality slope 1->2 maxdiff=" << MaxDiff(base, slope)
                     << ", intercept 0->100 maxdiff=" << MaxDiff(base, inter);
    EXPECT_GT(MaxDiff(base, slope), 0.0) << "rescaleSlope does not reach the output";
    EXPECT_GT(MaxDiff(base, inter), 0.0) << "rescaleIntercept does not reach the output";
}

// ---------------------------------------------------------------------------
// xpe_apply_voi_lut -- mode, center, width, and the output range
// ---------------------------------------------------------------------------
TEST(ParameterDependency, VoiLut_EveryParameterReachesTheOutput) {
    auto run = [](XpeVoiLutMode mode, float center, float width,
                  float minOut, float maxOut) {
        std::vector<float> px = Gradient(-500.0f, 1500.0f);
        XpeImageBuffer img = WrapFloat32(px);
        XpeVoiLutParams p{};
        p.mode   = mode;
        p.center = center;
        p.width  = width;
        p.minOut = minOut;
        p.maxOut = maxOut;
        EXPECT_EQ(XPE_OK, xpe_apply_voi_lut(&img, &p));
        return px;
    };

    const std::vector<float> base   = run(XPE_VOI_LINEAR, 500.0f, 1000.0f, 0.0f, 1.0f);
    const std::vector<float> center = run(XPE_VOI_LINEAR, 700.0f, 1000.0f, 0.0f, 1.0f);
    const std::vector<float> width  = run(XPE_VOI_LINEAR, 500.0f,  400.0f, 0.0f, 1.0f);
    const std::vector<float> range  = run(XPE_VOI_LINEAR, 500.0f, 1000.0f, 0.0f, 255.0f);
    const std::vector<float> sigm   = run(XPE_VOI_SIGMOID, 500.0f, 1000.0f, 0.0f, 1.0f);

    GTEST_LOG_(INFO) << "voi center maxdiff=" << MaxDiff(base, center)
                     << " width="  << MaxDiff(base, width)
                     << " range="  << MaxDiff(base, range)
                     << " sigmoid=" << MaxDiff(base, sigm);

    // A threshold, not "> 0": a difference of one ulp means the parameter
    // reached a floating-point rounding step, not the answer. The LINEAR_EXACT
    // case below is exactly that trap, and it is why this margin exists.
    constexpr double kMeaningful = 1e-4;
    EXPECT_GT(MaxDiff(base, center), kMeaningful) << "center does not reach the output";
    EXPECT_GT(MaxDiff(base, width),  kMeaningful) << "width does not reach the output";
    EXPECT_GT(MaxDiff(base, range),  kMeaningful) << "minOut/maxOut do not reach the output";
    EXPECT_GT(MaxDiff(base, sigm),   kMeaningful) << "XPE_VOI_SIGMOID matches LINEAR";
}

// ---------------------------------------------------------------------------
// KnownDivergence_ (QA-B-58): XPE_VOI_LINEAR_EXACT computes the same thing as
// XPE_VOI_LINEAR. A third instance of the #154 / #155 shape, and the clearest
// one: the mode is read, the switch branches, and both branches evaluate the
// same expression.
//
//   LINEAR (voi_lut.cpp:38-41)  lo = center - width/2
//                               (x - lo)/width * range   ==  ((x - center)/width + 0.5) * range
//   EXACT  (voi_lut.cpp:48-50)  ((x - center)/width + 0.5) * range
//
// Identical, so the measured difference is float-association noise (~6e-08 on a
// [0,1] output, one ulp). REQ-DISP-010 asks for the opposite: "the full window
// maps exactly from minOut to maxOut WITHOUT the half-value offset" -- the
// offset is present in both branches.
//
// (The mis-cited requirement numbers this case once noted are fixed: QA-B-59
// corrected the EXACT branch, and QA-B-66 corrected the rest -- the citations
// from REQ-DISP-010 onward had all shifted by one, in voi_lut.cpp and
// test_voi_lut.cpp alike. 009 LINEAR / 010 LINEAR_EXACT / 011 SIGMOID /
// 012 clamp-to-range is the mapping in the SPEC.)
//
// Not fixed: changing it changes displayed pixel values for every caller that
// selects EXACT (#154 / #155 precedent).
// ---------------------------------------------------------------------------
TEST(ParameterDependency, Fixed156_VoiLinearExactDiffersFromVoiLinear) {
    auto run = [](XpeVoiLutMode mode) {
        std::vector<float> px = Gradient(-500.0f, 1500.0f);
        XpeImageBuffer img = WrapFloat32(px);
        XpeVoiLutParams p{};
        p.mode   = mode;
        p.center = 500.0f;
        p.width  = 1000.0f;
        p.minOut = 0.0f;
        p.maxOut = 1.0f;
        EXPECT_EQ(XPE_OK, xpe_apply_voi_lut(&img, &p));
        return px;
    };

    const double diff = MaxDiff(run(XPE_VOI_LINEAR), run(XPE_VOI_LINEAR_EXACT));
    GTEST_LOG_(INFO) << "LINEAR vs LINEAR_EXACT maxdiff=" << diff
                     << " (one ulp at this scale is ~6e-08)";

    // INVERTED by QA-B-149, not deleted. Until #156 was fixed this asserted
    // diff < 1e-6: the two branches computed the same expression and the mode
    // selection did nothing. The fix went into LINEAR (it had lost the
    // standard's `center - 0.5` / `width - 1` placement), so the modes now
    // separate. The one-line flip is the visible arrival.
    //
    // Threshold, not `> 0`: the branches used to sit 6e-08 apart on float
    // association alone, so `> 0` would have gone green on rounding noise.
    EXPECT_GT(diff, 1e-4)
        << "LINEAR and LINEAR_EXACT compute the same value again -- the mode "
           "selection has stopped having an effect (#156)";
}

// ---------------------------------------------------------------------------
// #156 (QA-B-66, resolved QA-B-149): the same fact, asserted against the
// STANDARD's own published example rather than against an arbitrary window.
//
// The case above pins what the code does today. That is worth having, but on its
// own it has the wrong polarity for a defect: whoever finally implements
// REQ-DISP-010 will see a RED test whose message says the code is wrong, and the
// cheapest reading of a red test is "I broke something". The risk is that the
// fix gets reverted to make the suite green again.
//
// THE STANDARD ARRIVED (leader supplied PS3.3 C.11.2, QA-B-149) and the
// DISABLED_ prefix is removed. One correction to what this comment predicted:
// the branch that needed fixing was LINEAR, not LINEAR_EXACT. EXACT already
// matched the standard's published pseudo-code; LINEAR had lost the
// `center - 0.5` / `width - 1` placement, which is what made them coincide.
//
// The expectations below are NOT re-derived through the module. They are the
// window boundaries the standard states for its own worked examples, so they
// hold whatever the implementation does:
//
//   c = 2048, w = 4096  ->  x <= 0  is minOut,  x > 4095 is maxOut
//   c = 0,    w = 100   ->  x <= -50 is minOut, x > 49   is maxOut
//
// Both are exactly (c - 0.5) +/- (w - 1)/2. Under the OLD formula the first
// example's boundaries would have been 0 and 4096, and the second's -50 and
// 50 -- so these vectors separate the two formulas by themselves.
//
// THRESHOLD: kMeaningful (1e-4 on a [0,1] output), not `> 0`. The two branches
// differ today by float-association noise of about 6e-08 -- one ulp at this
// scale -- so a `> 0` assertion would go green on rounding and report the defect
// fixed while nothing had changed. Three orders of magnitude of separation is
// what keeps "the modes differ" from meaning "the adds happened in a different
// order".
// ---------------------------------------------------------------------------
TEST(ParameterDependency, VoiLinearMatchesTheStandardsWorkedExamples_156) {
    // One pixel per probe, so a boundary is read exactly rather than sampled.
    auto at = [](float x, float center, float width) {
        // A 1x1 buffer, not WrapFloat32: that helper hardcodes kW x kH, and a
        // boundary must be read exactly rather than sampled off a gradient.
        std::vector<float> px{x};
        XpeImageBuffer img{};
        img.width         = 1;
        img.height        = 1;
        img.format        = XPE_PIXEL_FLOAT32;
        img.bitsAllocated = 32;
        img.bitsStored    = 32;
        img.data          = px.data();
        img.dataSize      = static_cast<uint32_t>(sizeof(float));
        XpeVoiLutParams p{};
        p.mode   = XPE_VOI_LINEAR;
        p.center = center;
        p.width  = width;
        p.minOut = 0.0f;
        p.maxOut = 1.0f;
        EXPECT_EQ(XPE_OK, xpe_apply_voi_lut(&img, &p));
        return px[0];
    };

    struct Example { const char* name; float c, w, loEdge, hiEdge; };
    const Example examples[] = {
        { "PS3.3 example 1", 2048.0f, 4096.0f,   0.0f, 4095.0f },
        { "PS3.3 example 2",    0.0f,  100.0f, -50.0f,   49.0f },
    };

    for (const Example& e : examples) {
        const float atLo   = at(e.loEdge, e.c, e.w);
        const float atHi   = at(e.hiEdge, e.c, e.w);
        const float below  = at(e.loEdge - 1.0f, e.c, e.w);
        const float above  = at(e.hiEdge + 1.0f, e.c, e.w);
        GTEST_LOG_(INFO) << "  " << e.name << " c=" << e.c << " w=" << e.w
                         << ": f(" << e.loEdge << ")=" << atLo
                         << "  f(" << e.hiEdge << ")=" << atHi
                         << "  f(below)=" << below << "  f(above)=" << above;

        // The standard's stated boundaries. Under the pre-#156 formula the
        // upper edge of example 1 evaluated to 0.99976 and of example 2 to
        // 0.99 -- these two numbers are what separate the formulas.
        EXPECT_NEAR(0.0f, atLo, 1e-6f) << e.name << ": lower boundary";
        EXPECT_NEAR(1.0f, atHi, 1e-6f) << e.name << ": upper boundary";
        EXPECT_FLOAT_EQ(0.0f, below)   << e.name << ": below the window";
        EXPECT_FLOAT_EQ(1.0f, above)   << e.name << ": above the window";
    }

    // width == 1 makes (width - 1) zero. REQ-DISP-015 only rejects width <= 0,
    // so this is a legal input: the standard's thresholds partition the line
    // and the interior division is never reached.
    EXPECT_FLOAT_EQ(0.0f, at(0.0f, 10.0f, 1.0f)) << "width==1, below centre";
    EXPECT_FLOAT_EQ(1.0f, at(20.0f, 10.0f, 1.0f)) << "width==1, above centre";
    EXPECT_TRUE(std::isfinite(at(9.5f, 10.0f, 1.0f))) << "width==1 divided by zero";
}

// ---------------------------------------------------------------------------
// #177 (QA-B-149): the control the (A) verdict needs.
//
// REQ-DISP-017 was revised on 2026-09-17 so the presets act on detector DN
// rather than CT HU. Reading the revised code is not evidence that the revision
// WORKS -- the original symptom (GUI-C-84: an HU window crushes raw DN to a
// single output level) was observed in the gui lane, not here. This measures
// the symptom directly, on this side, so "the presets no longer crush DN" is an
// observation rather than a reading.
//
// The measurement is the number of DISTINCT output levels produced from a raw
// DN ramp of 1024 samples spanning 0..65535.
//
//   shipped full-DN window (32768 / 65535) : 1024 levels
//   the HU window the revision removed (40 / 400, restored to measure) : 5
//
// A 205x collapse -- the reported defect. Both numbers were MEASURED, not
// predicted: this comment first said "2" on the reasoning that an HU window
// pins everything to minOut or maxOut, and the control returned 5, because the
// window still spans DN -160..240 and a few ramp samples land inside it. The
// measured number is the one that belongs in a comment.
// ---------------------------------------------------------------------------
TEST(ParameterDependency, VoiPresetDoesNotCrushRawDetectorDn_177) {
    std::vector<float> px(static_cast<size_t>(kW) * kH);
    for (size_t i = 0; i < px.size(); ++i) {
        px[i] = 65535.0f * static_cast<float>(i) / static_cast<float>(px.size() - 1);
    }
    XpeImageBuffer img = WrapFloat32(px);

    XpeVoiLutParams p{};
    ASSERT_EQ(XPE_OK, xpe_voi_preset_create(&p, XPE_BODY_ABDOMEN));
    GTEST_LOG_(INFO) << "  preset ABDOMEN: c=" << p.center << " w=" << p.width
                     << " out=[" << p.minOut << ", " << p.maxOut << "]";
    ASSERT_EQ(XPE_OK, xpe_apply_voi_lut(&img, &p));

    std::vector<float> levels = px;
    std::sort(levels.begin(), levels.end());
    levels.erase(std::unique(levels.begin(), levels.end()), levels.end());
    GTEST_LOG_(INFO) << "  distinct output levels from a 0..65535 DN ramp: "
                     << levels.size() << " (measured: HU window 40/400 gives 5)";

    EXPECT_GT(levels.size(), 64u)
        << "the preset collapsed a full-scale DN ramp to " << levels.size()
        << " levels -- an HU-domain window is back in the preset table (#177)";
}

// ---------------------------------------------------------------------------
// xpe_voi_preset_create -- today the body part does NOT change the params
//
// REQ-DISP-017 was revised on 2026-09-17 (#177): the presets are in detector
// DN, and until real detector data (#151) gives per-body-part windows, every
// body part returns the same provisional window 32768/65535. This case used to
// assert that BONE and LUNG differ; it now asserts that they are the SAME, so
// that the day per-part DN values arrive it turns red and someone updates it
// on purpose rather than the provisional equality lingering unnoticed.
// ---------------------------------------------------------------------------
TEST(ParameterDependency, KnownDivergence_VoiPresetIgnoresBodyPartUntil151) {
    auto run = [](XpeBodyPart part) {
        XpeVoiLutParams p{};
        EXPECT_EQ(XPE_OK, xpe_voi_preset_create(&p, part));
        return p;
    };

    const XpeVoiLutParams bone = run(XPE_BODY_BONE);
    const XpeVoiLutParams lung = run(XPE_BODY_LUNG);
    const XpeVoiLutParams head = run(XPE_BODY_HEAD);

    GTEST_LOG_(INFO) << "preset BONE c=" << bone.center << " w=" << bone.width
                     << " | LUNG c=" << lung.center << " w=" << lung.width
                     << " | HEAD c=" << head.center << " w=" << head.width;

    EXPECT_TRUE(bone.center == lung.center && bone.width == lung.width)
        << "BONE and LUNG now differ -- per-body-part DN windows have arrived "
           "(#151/#177); update this case and REQ-DISP-017 deliberately";
    EXPECT_TRUE(bone.center == head.center && bone.width == head.width)
        << "BONE and HEAD now differ -- per-body-part DN windows have arrived "
           "(#151/#177); update this case and REQ-DISP-017 deliberately";
}

// ---------------------------------------------------------------------------
// xpe_apply_presentation_lut -- the LUT contents must reach the output
// ---------------------------------------------------------------------------
TEST(ParameterDependency, PresentationLut_LutContentsReachTheOutput) {
    // REQ-DISP-019 has this call std::free() the float32 buffer and install a
    // std::malloc()ed uint16 one, so the input must come from std::malloc --
    // handing it vector-owned memory corrupts the heap (the QA-B-41 hazard,
    // observed again here: the first version of this case died with
    // STATUS_HEAP_CORRUPTION).
    auto run = [](bool inverted) {
        const std::vector<float> seed = Gradient(0.0f, 1.0f);
        auto* raw = static_cast<float*>(std::malloc(kN * sizeof(float)));
        EXPECT_NE(nullptr, raw);
        std::memcpy(raw, seed.data(), kN * sizeof(float));

        XpeImageBuffer img{};
        img.width         = kW;
        img.height        = kH;
        img.format        = XPE_PIXEL_FLOAT32;
        img.bitsAllocated = 32;
        img.bitsStored    = 32;
        img.data          = raw;
        img.dataSize      = static_cast<uint32_t>(kN * sizeof(float));

        XpePresentationLutParams p{};
        for (int i = 0; i < 1024; ++i) {
            const int v = inverted ? (1023 - i) : i;
            p.lutData[i] = static_cast<uint16_t>(v * 64);
        }
        p.gsdfEnabled = 0;
        EXPECT_EQ(XPE_OK, xpe_apply_presentation_lut(&img, &p));

        std::vector<uint16_t> out(kN);
        const uint16_t* src = static_cast<const uint16_t*>(img.data);
        for (size_t i = 0; i < kN; ++i) out[i] = src[i];
        std::free(img.data);          // the call replaced the buffer (REQ-DISP-019)
        return out;
    };

    const std::vector<uint16_t> rising  = run(false);
    const std::vector<uint16_t> falling = run(true);

    int differing = 0;
    for (size_t i = 0; i < kN; ++i) if (rising[i] != falling[i]) ++differing;
    GTEST_LOG_(INFO) << "presentation LUT rising vs falling: " << differing
                     << " of " << kN << " pixels differ";

    EXPECT_GT(differing, 0)
        << "inverting every LUT entry changed nothing -- lutData does not reach "
           "the output";
}

// ---------------------------------------------------------------------------
// KnownDivergence_ (#155): the one that does NOT move.
//
// Kept beside the others on purpose: this is the same measurement, run against
// the function where the answer is "no". QA-B-57 has the algebra
// (presentation_lut.cpp:131-146); this is the dependency statement of it.
// ---------------------------------------------------------------------------
TEST(ParameterDependency, Gsdf_LuminanceReachesTheOutput_155) {
    auto run = [](const float* lum, int count) {
        XpePresentationLutParams p{};
        EXPECT_EQ(XPE_OK, xpe_gsdf_calibrate(lum, count, &p));
        return p;
    };

    const float wide[5]   = {1.0f, 10.0f, 50.0f, 200.0f, 500.0f};   // ~3 decades
    const float narrow[3] = {80.0f, 100.0f, 120.0f};                // < 1 decade
    const XpePresentationLutParams a = run(wide, 5);
    const XpePresentationLutParams b = run(narrow, 3);

    int maxDelta = 0;
    for (int i = 0; i < 1024; ++i) {
        maxDelta = std::max(maxDelta,
                            std::abs(static_cast<int>(a.lutData[i]) -
                                     static_cast<int>(b.lutData[i])));
    }
    GTEST_LOG_(INFO) << "gsdf luminance wide vs narrow: largest entry difference="
                     << maxDelta << " (rounding is 1)";

    // INVERTED by QA-B-145 (#155 stage 2): this was EXPECT_LE(maxDelta, 1),
    // pinning that the luminance measurements only moved the LUT by float
    // rounding. They now set the curve the standard's luminances are inverted
    // against, so a wide and a narrow calibration must differ substantially.
    EXPECT_GT(maxDelta, 1)
        << "the luminance measurements now move the LUT by more than rounding -- "
           "#155 has been addressed; say how, and retire this case";
}
