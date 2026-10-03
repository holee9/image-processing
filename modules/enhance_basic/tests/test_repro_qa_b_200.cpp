/**
 * @file test_repro_qa_b_200.cpp
 * @brief QA-B-200 M1: reproduction of candidate E1 of QA-B-199 (unsharp masking). NOT part of the suite.
 *
 * Every test is DISABLED_: built, never run by ctest. Run by hand with --gtest_also_run_disabled_tests; the OUTPUT is
 * the evidence (.moai/reports/lane-post/QA-B-200/). A test that FAILS reproduces a defect. The tests of a confirmed
 * candidate are enabled by the M2 that fixes it.
 *
 * E1  REQ-ENH-018: output[i] = input[i] + amount * (input[i] - blur(input)[i]) "only where abs(input[i] - blur(input)[i])
 *     >= threshold". REQ-ENH-021: "Pixel overshoot SHALL be clamped to max(original * 2.0, original + amount * threshold)".
 *     The candidate: the code clamps to original +- amount*threshold on BOTH sides, so every sharpened pixel moves by exactly
 *     amount*threshold whatever the edge, and threshold 0 changes nothing.
 *     The expected values below do not come from the module: a blur written here (separable Gaussian, sigma = radius,
 *     clamped borders) and the SPEC formulas. The module's own blur is not exposed, so a small difference between the two
 *     blurs is allowed for; the candidate's size is orders of magnitude larger.
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

TEST(ReproQaB200Enhance, DISABLED_E1_ControlTheDefaultThresholdSharpensAnEdge) {
    // The control: the harness can see sharpening at all, and the module does something at an edge.
    const std::vector<float> in = StepImage(1000.0f, 1000.0f);
    XpeErrorCode rc;
    const std::vector<float> out = ModuleUsm(in, 1.0f, 2.0f, 10.0f, &rc);
    ASSERT_EQ(XPE_OK, rc);
    const float change = MaxChange(in, out);
    std::printf("E1 CONTROL: amount 1, radius 2, threshold 10, edge 1000 -> module max |out-in| = %.3f\n", change);
    EXPECT_GT(change, 0.0f);
}

TEST(ReproQaB200Enhance, DISABLED_E1_ThresholdZeroSharpensAnEdge) {
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

TEST(ReproQaB200Enhance, DISABLED_E1_TheSharpeningFollowsTheEdgeContrastAsTheFormulaSays) {
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
