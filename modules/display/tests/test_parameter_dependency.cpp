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
// (The EXACT branch's comment also cites REQ-DISP-011, which is the sigmoid
// requirement; REQ-DISP-010 is the one it implements. Noted, not fixed.)
//
// Not fixed: changing it changes displayed pixel values for every caller that
// selects EXACT (#154 / #155 precedent).
// ---------------------------------------------------------------------------
TEST(ParameterDependency, KnownDivergence_VoiLinearExactEqualsVoiLinear) {
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

    EXPECT_LT(diff, 1e-6)
        << "LINEAR_EXACT now differs from LINEAR -- REQ-DISP-010 has been "
           "implemented; say how, and retire this case";
}

// ---------------------------------------------------------------------------
// xpe_voi_preset_create -- the body part must reach the params
// ---------------------------------------------------------------------------
TEST(ParameterDependency, VoiPreset_BodyPartReachesTheParams) {
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

    EXPECT_TRUE(bone.center != lung.center || bone.width != lung.width)
        << "BONE and LUNG produce the same window";
    EXPECT_TRUE(bone.center != head.center || bone.width != head.width)
        << "BONE and HEAD produce the same window";
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
TEST(ParameterDependency, KnownDivergence_GsdfLuminanceDoesNotReachTheOutput) {
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

    EXPECT_LE(maxDelta, 1)
        << "the luminance measurements now move the LUT by more than rounding -- "
           "#155 has been addressed; say how, and retire this case";
}
