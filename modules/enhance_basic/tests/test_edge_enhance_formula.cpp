/**
 * @file test_edge_enhance_formula.cpp
 * @brief QA-B-200 M2b (E1): xpe_edge_enhance follows REQ-ENH-018 and REQ-ENH-021 as written.
 *
 * REQ-ENH-018: output[i] = input[i] + amount * (input[i] - blur(input)[i]) "only where abs(input[i] - blur(input)[i])
 * >= threshold". REQ-ENH-021: "Pixel overshoot SHALL be clamped to max(original * 2.0, original + amount * threshold)".
 * Until M2b the code clamped to original +- amount*threshold on BOTH sides, so every sharpened pixel moved by exactly
 * amount*threshold whatever the edge, and threshold 0 changed nothing (QA-B-199 candidate E1, reproduced by QA-B-200 M1).
 * The expected values do not come from the module: a blur written here (separable Gaussian, sigma = radius, clamped
 * borders) and the SPEC formulas. The module's own blur is not exposed, so a small difference between the two blurs is
 * allowed for; the defect was orders of magnitude larger.
 */

#include <gtest/gtest.h>

#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xpe/enhance_basic/enhance_basic_api.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

namespace {

constexpr int kW = 64, kH = 64;

/** Left half `low`, right half `low + contrast`: a vertical step edge between columns 31 and 32. */
std::vector<float> StepImage(float low, float contrast) {
    std::vector<float> px(static_cast<size_t>(kW) * kH);
    for (int y = 0; y < kH; ++y)
        for (int x = 0; x < kW; ++x) px[static_cast<size_t>(y) * kW + x] = x < kW / 2 ? low : low + contrast;
    return px;
}

/** Independent reference blur: separable Gaussian, sigma, kernel radius ceil(4 sigma), borders clamped, weights normalised. */
std::vector<float> ReferenceBlur(const std::vector<float>& in, float sigma) {
    const int r = static_cast<int>(std::ceil(4.0f * sigma));
    std::vector<double> k(static_cast<size_t>(2 * r + 1));
    double sum = 0;
    for (int i = -r; i <= r; ++i) {
        k[static_cast<size_t>(i + r)] = std::exp(-0.5 * (static_cast<double>(i) / sigma) * (static_cast<double>(i) / sigma));
        sum += k[static_cast<size_t>(i + r)];
    }
    for (double& v : k) v /= sum;
    std::vector<double> tmp(in.size());
    for (int y = 0; y < kH; ++y)
        for (int x = 0; x < kW; ++x) {
            double a = 0;
            for (int i = -r; i <= r; ++i) a += k[static_cast<size_t>(i + r)] * in[static_cast<size_t>(y) * kW + std::clamp(x + i, 0, kW - 1)];
            tmp[static_cast<size_t>(y) * kW + x] = a;
        }
    std::vector<float> out(in.size());
    for (int y = 0; y < kH; ++y)
        for (int x = 0; x < kW; ++x) {
            double a = 0;
            for (int i = -r; i <= r; ++i) a += k[static_cast<size_t>(i + r)] * tmp[static_cast<size_t>(std::clamp(y + i, 0, kH - 1)) * kW + x];
            out[static_cast<size_t>(y) * kW + x] = static_cast<float>(a);
        }
    return out;
}

/** REQ-ENH-018 and REQ-ENH-021 evaluated from the SPEC text on the reference blur. */
std::vector<float> SpecUsm(const std::vector<float>& in, float amount, float radius, float threshold) {
    const std::vector<float> blur = ReferenceBlur(in, radius);
    std::vector<float> out(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        const float diff = in[i] - blur[i];
        float v = in[i];
        if (std::fabs(diff) >= threshold) {
            v = in[i] + amount * diff;                                   // REQ-ENH-018
            const float bound = std::max(in[i] * 2.0f, in[i] + amount * threshold);   // REQ-ENH-021 (overshoot upper bound)
            if (v > bound) v = bound;
        }
        out[i] = v;
    }
    return out;
}

/** The module's output on @p in. */
std::vector<float> ModuleUsm(const std::vector<float>& in, float amount, float radius, float threshold, XpeErrorCode* rc) {
    XpeImageBuffer img{};
    img.width = kW;
    img.height = kH;
    img.format = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = img.bitsStored = 32;
    img.dataSize = in.size() * sizeof(float);
    img.data = std::malloc(img.dataSize);
    std::copy(in.begin(), in.end(), static_cast<float*>(img.data));
    XpeUsmParams p{amount, radius, threshold};
    *rc = xpe_edge_enhance(&img, &p);
    std::vector<float> out(static_cast<float*>(img.data), static_cast<float*>(img.data) + in.size());
    std::free(img.data);
    return out;
}

float MaxChange(const std::vector<float>& a, const std::vector<float>& b) {
    float m = 0;
    for (size_t i = 0; i < a.size(); ++i) m = std::max(m, std::fabs(a[i] - b[i]));
    return m;
}

}  // namespace

TEST(EdgeEnhanceFormula, E1_ControlTheDefaultThresholdSharpensAnEdge) {
    // The control: the harness can see sharpening at all, and the module does something at an edge.
    const std::vector<float> in = StepImage(1000.0f, 1000.0f);
    XpeErrorCode rc;
    const std::vector<float> out = ModuleUsm(in, 1.0f, 2.0f, 10.0f, &rc);
    ASSERT_EQ(XPE_OK, rc);
    const float change = MaxChange(in, out);
    std::printf("E1 CONTROL: amount 1, radius 2, threshold 10, edge 1000 -> module max |out-in| = %.3f\n", change);
    EXPECT_GT(change, 0.0f);
}

TEST(EdgeEnhanceFormula, E1_ThresholdZeroSharpensAnEdge) {
    // REQ-ENH-020 allows threshold 0 (only a negative one is refused); REQ-ENH-018 then sharpens every pixel.
    const std::vector<float> in = StepImage(1000.0f, 1000.0f);
    XpeErrorCode rc;
    const std::vector<float> out = ModuleUsm(in, 1.0f, 2.0f, 0.0f, &rc);
    ASSERT_EQ(XPE_OK, rc) << "threshold 0 is valid (REQ-ENH-020)";
    const std::vector<float> spec = SpecUsm(in, 1.0f, 2.0f, 0.0f);
    const float moduleChange = MaxChange(in, out);
    const float specChange = MaxChange(in, spec);
    std::printf("E1 THRESHOLD 0: SPEC (reference blur) max |out-in| = %.3f; module max |out-in| = %.3f\n", specChange, moduleChange);
    EXPECT_GT(moduleChange, 0.1f * specChange) << "was: the module changed nothing at threshold 0";
}

TEST(EdgeEnhanceFormula, E1_TheSharpeningFollowsTheEdgeContrastAsTheFormulaSays) {
    // amount 0.5, radius 2, threshold 10, a step edge of contrast C on a base of 1000: at the edge pixel the SPEC adds
    // amount * (input - blur) -- proportional to C. The module adds amount*threshold = 5 whatever C is.
    struct Row {
        float contrast;
        float spec, module;
    } rows[2] = {{100.0f, 0, 0}, {1000.0f, 0, 0}};
    for (Row& r : rows) {
        const std::vector<float> in = StepImage(1000.0f, r.contrast);
        XpeErrorCode rc;
        const std::vector<float> out = ModuleUsm(in, 0.5f, 2.0f, 10.0f, &rc);
        ASSERT_EQ(XPE_OK, rc);
        const std::vector<float> spec = SpecUsm(in, 0.5f, 2.0f, 10.0f);
        // the edge pixel on the bright side: row 32, column 32; and on the dark side: column 31
        const size_t bright = 32u * kW + 32u, dark = 32u * kW + 31u;
        r.spec = spec[bright] - in[bright];
        r.module = out[bright] - in[bright];
        std::printf("E1 CONTRAST %6.0f: bright-side edge pixel moves by SPEC %.3f, module %.3f; dark side SPEC %.3f, module %.3f\n",
                    r.contrast, r.spec, r.module, spec[dark] - in[dark], out[dark] - in[dark]);
    }
    std::printf("E1 RATIO (contrast 1000 / contrast 100) at the bright edge pixel: SPEC %.2f, module %.2f\n",
                rows[1].spec / rows[0].spec, rows[1].module / rows[0].module);
    EXPECT_NEAR(rows[0].module, rows[0].spec, 0.15f * std::fabs(rows[0].spec)) << "contrast 100";
    EXPECT_NEAR(rows[1].module, rows[1].spec, 0.15f * std::fabs(rows[1].spec)) << "contrast 1000";
}

TEST(EdgeEnhanceFormula, E1_TheOvershootIsBoundedByTwiceTheOriginalAndNotByAmountTimesThreshold) {
    // A dark base (10) next to a very bright side (10010), amount 5: the sharpened value on the bright side is far above
    // 2 * original, so REQ-ENH-021's bound max(original * 2, original + amount * threshold) = 2 * original is what limits it.
    const std::vector<float> in = StepImage(10.0f, 10000.0f);
    XpeErrorCode rc;
    const std::vector<float> out = ModuleUsm(in, 5.0f, 2.0f, 10.0f, &rc);
    ASSERT_EQ(XPE_OK, rc);
    const std::vector<float> spec = SpecUsm(in, 5.0f, 2.0f, 10.0f);
    const size_t bright = 32u * kW + 32u;
    std::printf("E1 BOUND: bright-side edge pixel original %.1f, SPEC %.1f, module %.1f, 2*original %.1f, original+amount*threshold %.1f\n",
                in[bright], spec[bright], out[bright], 2.0f * in[bright], in[bright] + 5.0f * 10.0f);
    ASSERT_FLOAT_EQ(2.0f * in[bright], spec[bright]) << "precondition: the reference hits the 2*original bound here";
    EXPECT_FLOAT_EQ(2.0f * in[bright], out[bright]) << "was original + amount*threshold = " << in[bright] + 50.0f;
    for (size_t i = 0; i < in.size(); ++i) {
        const float bound = std::max(in[i] * 2.0f, in[i] + 5.0f * 10.0f);
        EXPECT_LE(out[i], bound * (1.0f + 1e-6f)) << "pixel " << i << " overshoots the REQ-ENH-021 bound";
    }
}

TEST(EdgeEnhanceFormula, E1_APixelWhoseDifferenceIsBelowTheThresholdIsLeftAlone) {
    // A step of 8 with threshold 10: the largest |input - blur| is about 4, below the threshold, so nothing changes.
    // The control above (step 1000) shows the same call does change pixels once the difference reaches the threshold.
    const std::vector<float> in = StepImage(1000.0f, 8.0f);
    XpeErrorCode rc;
    const std::vector<float> out = ModuleUsm(in, 1.0f, 2.0f, 10.0f, &rc);
    ASSERT_EQ(XPE_OK, rc);
    EXPECT_EQ(0.0f, MaxChange(in, out));
    const std::vector<float> spec = SpecUsm(in, 1.0f, 2.0f, 10.0f);
    EXPECT_EQ(0.0f, MaxChange(in, spec)) << "precondition: the reference agrees that nothing reaches the threshold";
}

TEST(EdgeEnhanceFormula, E1_ADarkPixelMayOvershootByAmountTimesThresholdWhenThatIsAboveTwiceItsValue) {
    // One pixel of 30 on a base of 0, amount 5, threshold 10: sharpened = 30 + 5 * (30 - blur) is about 170, and the bound is
    // max(2 * 30, 30 + 5 * 10) = max(60, 80) = 80. The second term is the larger one here; without it the pixel stops at 60.
    std::vector<float> in(static_cast<size_t>(kW) * kH, 0.0f);
    const size_t dot = 32u * kW + 32u;
    in[dot] = 30.0f;
    XpeErrorCode rc;
    const std::vector<float> out = ModuleUsm(in, 5.0f, 2.0f, 10.0f, &rc);
    ASSERT_EQ(XPE_OK, rc);
    const std::vector<float> spec = SpecUsm(in, 5.0f, 2.0f, 10.0f);
    std::printf("E1 DARK DOT: original %.1f, SPEC %.1f, module %.1f (2*original 60, original+amount*threshold 80)\n", in[dot], spec[dot], out[dot]);
    ASSERT_FLOAT_EQ(80.0f, spec[dot]) << "precondition: the reference is held by the amount*threshold term here";
    EXPECT_FLOAT_EQ(80.0f, out[dot]);
}

// ---------------------------------------------------------------------------------------------------------------------
// QA-B-201 M3: a sharpened pixel is never below 0 (user decision, #251). The upper bound is unchanged.
// ---------------------------------------------------------------------------------------------------------------------

TEST(EdgeEnhanceFloor, ASharpenedPixelBesideADarkRegionIsNeverBelowZero) {
    // The dark side of a strong edge: the formula (REQ-ENH-018) goes far below 0 -- the reference shows it -- and the
    // module cuts it at 0. Two bases: 0 (a collimated strip) and 30 (a value an "orig-based" floor would keep).
    struct Scene {
        float low, contrast, amount;
    } scenes[] = {{0.0f, 4000.0f, 5.0f}, {30.0f, 4000.0f, 5.0f}, {0.0f, 4000.0f, 0.5f}, {0.0f, 4000.0f, 1.0f}};
    for (const Scene& sc : scenes) {
        const std::vector<float> in = StepImage(sc.low, sc.contrast);
        XpeErrorCode rc;
        const std::vector<float> out = ModuleUsm(in, sc.amount, 2.0f, 10.0f, &rc);
        ASSERT_EQ(XPE_OK, rc);
        const std::vector<float> spec = SpecUsm(in, sc.amount, 2.0f, 10.0f);
        const size_t dark = 32u * kW + 31u;   // the dark-side edge pixel
        float moduleMin = out[0], specMin = spec[0];
        size_t negatives = 0;
        for (size_t i = 0; i < out.size(); ++i) {
            moduleMin = std::min(moduleMin, out[i]);
            specMin = std::min(specMin, spec[i]);
            if (out[i] < 0.0f) ++negatives;
        }
        std::printf("M3 FLOOR: base %.0f, contrast %.0f, amount %.1f: reference min %.1f, module min %.1f, module negatives %zu\n",
                    sc.low, sc.contrast, sc.amount, specMin, moduleMin, negatives);
        ASSERT_LT(spec[dark], 0.0f) << "precondition: the formula alone goes below 0 here (so the floor is what is tested)";
        EXPECT_EQ(0u, negatives) << "base " << sc.low << " amount " << sc.amount;
        EXPECT_FLOAT_EQ(0.0f, out[dark]) << "the dark-side edge pixel is cut at 0, not at its input (" << in[dark] << ")";
    }
}

TEST(EdgeEnhanceFloor, TheFloorDoesNotChangeAnyPixelThatTheFormulaLeavesAtOrAboveZero) {
    // The control for the test above and the statement that the 16-bit result does not change: where the reference stays
    // >= 0 the module equals the formula (within the blur difference), so the floor touched only what was below 0.
    const std::vector<float> in = StepImage(1000.0f, 1000.0f);   // nothing goes below 0 here
    XpeErrorCode rc;
    const std::vector<float> out = ModuleUsm(in, 1.0f, 2.0f, 10.0f, &rc);
    ASSERT_EQ(XPE_OK, rc);
    const std::vector<float> spec = SpecUsm(in, 1.0f, 2.0f, 10.0f);
    float specMin = spec[0];
    for (float v : spec) specMin = std::min(specMin, v);
    ASSERT_GT(specMin, 0.0f) << "precondition";
    const size_t dark = 32u * kW + 31u;
    EXPECT_NEAR(spec[dark], out[dark], 0.02f * std::fabs(spec[dark] - in[dark])) << "the undershoot above 0 is kept";
    EXPECT_LT(out[dark], in[dark]) << "and it is still an undershoot";
}

TEST(EdgeEnhanceFloor, TheUpperBoundIsUnchangedByTheFloor) {
    // The same scene as the REQ-ENH-021 test above: the bound is max(2 * original, original + amount * threshold).
    const std::vector<float> in = StepImage(10.0f, 10000.0f);
    XpeErrorCode rc;
    const std::vector<float> out = ModuleUsm(in, 5.0f, 2.0f, 10.0f, &rc);
    ASSERT_EQ(XPE_OK, rc);
    const size_t bright = 32u * kW + 32u;
    EXPECT_FLOAT_EQ(2.0f * in[bright], out[bright]);
    for (size_t i = 0; i < in.size(); ++i) {
        EXPECT_LE(out[i], std::max(in[i] * 2.0f, in[i] + 5.0f * 10.0f) * (1.0f + 1e-6f)) << "pixel " << i;
        EXPECT_GE(out[i], 0.0f) << "pixel " << i;
    }
}

TEST(EdgeEnhanceFloor, APixelThatIsNotSharpenedKeepsItsInputValueEvenWhenItIsNegative) {
    // The floor belongs to the sharpened value. A flat image has no difference to the blur, so nothing is sharpened and
    // every pixel is returned as it came, a negative one included -- a negative INPUT is outside what the pipelines
    // produce (the log stage cuts it, REQ-ENH-002) and is not this stage's to rewrite.
    const std::vector<float> in(static_cast<size_t>(kW) * kH, -5.0f);
    XpeErrorCode rc;
    const std::vector<float> out = ModuleUsm(in, 5.0f, 2.0f, 10.0f, &rc);
    ASSERT_EQ(XPE_OK, rc);
    EXPECT_EQ(0.0f, MaxChange(in, out));
    EXPECT_FLOAT_EQ(-5.0f, out[100]);
}
