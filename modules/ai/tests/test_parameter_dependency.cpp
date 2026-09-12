// #156 (QA-B-59): does each parameter reach the output? -- ai.
//
// The sweep that found #156 asked one question of every parameter: change it,
// hold the rest, does the answer move? On ai that question needs a distinction
// the other modules did not:
//
//   "does not reach the output"  -- the input is read, used, and cancels out.
//                                   That is the #154 / #155 / #156 defect.
//   "never gets that far"        -- this is a stub build, and the inference
//                                   entry points return
//                                   XPE_ERR_PROCESSING_FAILED after argument
//                                   validation and before any model runs
//                                   (REQ-AI-002 fallback routing).
//
// The second is not a defect and must not be recorded as one. It is also not
// evidence of correctness: an input that never reaches a model cannot be shown
// to affect it either way, so those entry points are named here as unmeasurable
// in this build rather than quietly left out of the table.
//
// What IS measurable in the stub: the entry points whose answer is arithmetic
// over the arguments (xpe_stitch_estimate_size), and the module state that the
// stub itself consults (fallback mode).

#include <gtest/gtest.h>

#include "xpe/ai/ai_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <cstdint>
#include <cstring>
#include <string>
#include <tuple>
#include <vector>

namespace {

XpeImageBuffer MakePart(std::vector<float>& px, uint32_t w, uint32_t h) {
    px.assign(static_cast<size_t>(w) * h, 1000.0f);
    XpeImageBuffer img{};
    img.width         = w;
    img.height        = h;
    img.format        = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = 32;
    img.bitsStored    = 32;
    img.data          = px.data();
    img.dataSize      = static_cast<uint32_t>(px.size() * sizeof(float));
    return img;
}

class AiParamDependency : public ::testing::Test {
protected:
    void SetUp() override    { ASSERT_EQ(XPE_OK, xpe_ai_init("dummy_model_dir", nullptr)); }
    void TearDown() override { xpe_ai_shutdown(); }
};

}  // namespace

// xpe_stitch_estimate_size is arithmetic over the arguments and runs fully in the
// stub build, so both of its inputs can be measured.
TEST_F(AiParamDependency, StitchEstimateSize_PartDimensionsReachTheOutput) {
    auto run = [](uint32_t w, uint32_t h, uint32_t partCount) {
        std::vector<float> a, b, c;
        std::vector<XpeImageBuffer> parts;
        parts.push_back(MakePart(a, w, h));
        parts.push_back(MakePart(b, w, h));
        if (partCount > 2) parts.push_back(MakePart(c, w, h));
        uint32_t outW = 0, outH = 0;
        EXPECT_EQ(XPE_OK, xpe_stitch_estimate_size(parts.data(), partCount, &outW, &outH));
        return std::pair<uint32_t, uint32_t>{outW, outH};
    };

    const auto base   = run(256, 128, 2);
    const auto wider  = run(512, 128, 2);
    const auto taller = run(256, 256, 2);
    const auto more   = run(256, 128, 3);

    GTEST_LOG_(INFO) << "estimate base=" << base.first << "x" << base.second
                     << " wider=" << wider.first << "x" << wider.second
                     << " taller=" << taller.first << "x" << taller.second
                     << " 3 parts=" << more.first << "x" << more.second;

    EXPECT_NE(base.first,  wider.first)  << "part width does not reach the estimate";
    EXPECT_NE(base.second, taller.second) << "part height does not reach the estimate";
    EXPECT_NE(base.first,  more.first)
        << "partCount does not reach the estimate -- the documented heuristic "
           "adds ~70% width per additional part";
}

// Fallback mode is module state the stub consults. Whether it changes any
// observable answer is measured, not assumed: if both settings produce the same
// result for every reachable call, the setter has no observable effect in this
// build, and that is worth stating plainly rather than leaving implied.
TEST_F(AiParamDependency, FallbackMode_IsAcceptedAndObservableEffectIsRecorded) {
    std::vector<float> px;
    XpeImageBuffer img = MakePart(px, 64, 64);

    auto probe = [&img](int32_t enable) {
        EXPECT_EQ(XPE_OK, xpe_ai_set_fallback_mode(enable));
        char part[64] = {0};
        float conf = -1.0f;
        const XpeErrorCode rc = xpe_bodypart_recognize(&img, part, sizeof(part), &conf);
        return std::tuple<XpeErrorCode, std::string, float>{rc, std::string(part), conf};
    };

    const auto on  = probe(1);
    const auto off = probe(0);

    GTEST_LOG_(INFO) << "fallback ON  rc=" << std::get<0>(on)
                     << " label='" << std::get<1>(on) << "' conf=" << std::get<2>(on);
    GTEST_LOG_(INFO) << "fallback OFF rc=" << std::get<0>(off)
                     << " label='" << std::get<1>(off) << "' conf=" << std::get<2>(off);

    // The setter must at least be accepted in both directions; what it changes
    // downstream is a property of the ONNX build, which this one is not.
    SUCCEED() << "recorded above; see the QA-B-59 report for the reading";
}

// ---------------------------------------------------------------------------
// Unmeasurable in this build -- named, not omitted.
//
// These entry points validate their arguments and then return
// XPE_ERR_PROCESSING_FAILED without running a model (stub build; the ONNX path
// is #130, still unverified). A parameter sweep over them would measure the
// validation order, not the model, so "no effect" here would mean "never got
// that far" -- the distinction this file exists to keep.
// ---------------------------------------------------------------------------
TEST_F(AiParamDependency, InferenceEntryPointsStopBeforeTheModel) {
    std::vector<float> px;
    XpeImageBuffer img = MakePart(px, 64, 64);

    char part[64] = {0};
    float conf = 0.0f;
    const XpeErrorCode bodypart = xpe_bodypart_recognize(&img, part, sizeof(part), &conf);

    std::vector<float> outPx;
    XpeImageBuffer out = MakePart(outPx, 64, 64);
    const XpeErrorCode bone = xpe_bone_suppress(&img, &out, nullptr);

    XpeImageMetadata meta{};
    const XpeErrorCode denoise = xpe_dl_denoise(&img, &meta, nullptr);

    GTEST_LOG_(INFO) << "stub returns: bodypart=" << bodypart
                     << " bone_suppress=" << bone
                     << " dl_denoise=" << denoise
                     << " (XPE_ERR_PROCESSING_FAILED is " << XPE_ERR_PROCESSING_FAILED << ")";

    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, bodypart)
        << "this entry point now produces a result in the stub build -- its "
           "parameters have become measurable and belong in the sweep above";
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, bone);
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, denoise);
}
