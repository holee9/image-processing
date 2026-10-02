/**
 * @file test_bodypart_inference.cpp
 * @brief xpe_bodypart_recognize on a real session, and its resize helper (QA-B-191 M2, #130, T-006).
 *
 * THE MODELS BEHIND THESE TESTS ARE NOT CLASSIFIERS. They are hand-written Flatten/MatMul/Add graphs
 * (tests/data/make_bodypart_models.py) whose outputs are numbers chosen in advance. What is tested is the
 * WIRING: a number the model produces reaches the label and the confidence, a different model or a different
 * image gives a different answer, and every way the model can be unusable ends in the documented fallback
 * outcome. NOTHING HERE SAYS ANYTHING ABOUT HOW WELL A REAL MODEL RECOGNISES A BODY PART, HOW ACCURATE IT IS
 * OR HOW FAST IT RUNS. SRS-AI-010's "CNN classifier" is not met by this card.
 *
 * THE LOAD-BEARING TESTS are ADifferentModelDirectoryChangesTheLabel and TheImageChangesTheLabel. An echo, a
 * constant, an ignored image or the stub cannot pass both: the first needs the answer to come from the model,
 * the second needs it to come from THIS image.
 *
 * The threshold, the low-confidence event and fallback_mode (M3, REQ-AI-012) are the last section.
 */

#include <gtest/gtest.h>

#include "xpe/ai/ai_api.h"
#include "xpe/ai/ai_onnx_session.h"
#include "xpe/common/xpe_error.h"
#include "ai_bodypart.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

#ifndef XPE_AI_TEST_DATA_DIR
#error "XPE_AI_TEST_DATA_DIR must be defined by the build (modules/ai/CMakeLists.txt)"
#endif
const std::string kData = XPE_AI_TEST_DATA_DIR;
std::string Dir(const char* name) { return kData + "/" + name; }

bool IsStub() { return xpe::ai::OnnxSession::IsStubBuild(); }

/** A single-channel float image of w x h, each pixel computed from its row and column. */
struct Img {
    std::vector<float> px;
    XpeImageBuffer buf{};
    template <typename F>
    Img(uint32_t w, uint32_t h, F value) : px(static_cast<size_t>(w) * h) {
        for (uint32_t y = 0; y < h; ++y) {
            for (uint32_t x = 0; x < w; ++x) px[static_cast<size_t>(y) * w + x] = value(x, y);
        }
        buf.width = w;
        buf.height = h;
        buf.bitsAllocated = 32;
        buf.bitsStored = 32;
        buf.format = XPE_PIXEL_FLOAT32;
        buf.data = px.data();
        buf.dataSize = px.size() * sizeof(float);
    }
};

// 0.75 and 0 are exactly representable, so the means below are exact in float arithmetic.
Img TopBright(uint32_t w = 4, uint32_t h = 4) { return Img(w, h, [h](uint32_t, uint32_t y) { return y < h / 2 ? 0.75f : 0.0f; }); }
Img BottomBright(uint32_t w = 4, uint32_t h = 4) { return Img(w, h, [h](uint32_t, uint32_t y) { return y >= h / 2 ? 0.75f : 0.0f; }); }
Img Flat(float v, uint32_t w = 4, uint32_t h = 4) { return Img(w, h, [v](uint32_t, uint32_t) { return v; }); }

struct Result {
    XpeErrorCode rc;
    std::string label;
    float confidence;
};

Result Recognize(const Img& img, size_t bufLen = 64) {
    std::vector<char> buf(bufLen, 'x');   // 'x' fill: a byte the function did not write stays visible
    float conf = -1.0f;
    Result r;
    r.rc = xpe_bodypart_recognize(&img.buf, buf.data(), bufLen, &conf);
    r.label = std::string(buf.data(), strnlen(buf.data(), bufLen));
    r.confidence = conf;
    return r;
}

struct Alert {
    std::string text;
    int32_t severity;
};

std::vector<Alert> Alerts() {
    std::vector<Alert> v;
    const int32_t n = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < n; ++i) {
        char msg[512] = {0};
        int32_t sev = -1;
        if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) == XPE_OK) v.push_back({msg, sev});
    }
    return v;
}

int CountAlerts(const char* needle) {
    int hits = 0;
    for (const Alert& a : Alerts()) {
        if (a.text.find(needle) != std::string::npos) ++hits;
    }
    return hits;
}

struct BodyPart : public ::testing::Test {
    void SetUp() override {
        xpe_ai_shutdown();
        xpe_clear_alerts();
    }
    void TearDown() override { xpe_ai_shutdown(); }
    static void Init(const std::string& dir, const char* config = nullptr) {
        xpe_ai_shutdown();
        ASSERT_EQ(XPE_OK, xpe_ai_init(dir.c_str(), config)) << dir;
        xpe_clear_alerts();
    }
};

#define REQUIRE_ONNX() \
    do { if (IsStub()) GTEST_SKIP() << "needs ONNX Runtime: the full build only"; } while (0)

}  // namespace

// ===== fixtures, or nothing below means anything ==========================================================

TEST_F(BodyPart, ModelDirectoriesArePresent) {
    for (const char* d : {"models_bodypart_a", "models_bodypart_b", "models_bodypart_dep", "models_bodypart_nhwc"}) {
        std::ifstream m(Dir(d) + "/bodypart.onnx", std::ios::binary);
        std::ifstream j(Dir(d) + "/bodypart.json");
        EXPECT_TRUE(m.good()) << d << " has no model; run tests/data/make_bodypart_models.py";
        EXPECT_TRUE(j.good()) << d << " has no label sidecar";
    }
    std::ifstream none(Dir("models_bodypart_no_labels") + "/bodypart.json");
    EXPECT_FALSE(none.good()) << "models_bodypart_no_labels must NOT have a sidecar -- it pins that failure";
    std::ifstream missing(Dir("models_missing") + "/bodypart.onnx", std::ios::binary);
    EXPECT_FALSE(missing.good()) << "models_missing must NOT hold a body-part model";
}

// ===== the wiring: the answer comes from the model =========================================================

TEST_F(BodyPart, ConstantModelGivesItsLabelAndItsProbability) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_a"));
    const Result r = Recognize(Flat(0.0f));
    EXPECT_EQ(XPE_OK, r.rc);
    EXPECT_EQ("CHEST", r.label);
    EXPECT_EQ(0.6f, r.confidence) << "exactly the number the model emits";
    EXPECT_TRUE(Alerts().empty()) << "a usable answer raises no alert in M2";
}

TEST_F(BodyPart, ADifferentModelDirectoryChangesTheLabel) {
    REQUIRE_ONNX();
    const Img image = Flat(0.0f);
    Init(Dir("models_bodypart_a"));
    const Result a = Recognize(image);
    Init(Dir("models_bodypart_b"));
    const Result b = Recognize(image);
    ASSERT_EQ(XPE_OK, a.rc);
    ASSERT_EQ(XPE_OK, b.rc);
    EXPECT_EQ("CHEST", a.label);
    EXPECT_EQ("SPINE", b.label);
    EXPECT_NE(a.label, b.label) << "the same image through a different model must give a different answer";
}

TEST_F(BodyPart, AConstantModelIgnoresTheImage) {
    // The control for the next test: if a model that ignores its input gave different answers, the image-dependent
    // result below would prove nothing.
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_a"));
    const Result z = Recognize(Flat(0.0f));
    const Result o = Recognize(Flat(1.0f));
    const Result t = Recognize(TopBright());
    EXPECT_EQ(z.label, o.label);
    EXPECT_EQ(z.label, t.label);
    EXPECT_EQ(z.confidence, o.confidence);
    EXPECT_EQ(z.confidence, t.confidence);
}

TEST_F(BodyPart, TheImageChangesTheLabel) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_dep"));
    const Result top = Recognize(TopBright());
    const Result bottom = Recognize(BottomBright());
    ASSERT_EQ(XPE_OK, top.rc);
    ASSERT_EQ(XPE_OK, bottom.rc);
    EXPECT_EQ("CHEST", top.label);
    EXPECT_EQ(0.75f, top.confidence);
    EXPECT_EQ("ABDOMEN", bottom.label);
    EXPECT_EQ(0.75f, bottom.confidence);
}

TEST_F(BodyPart, ATieGoesToTheFirstClassAndAFullScaleProbabilityIsAccepted) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_dep"));
    const Result r = Recognize(Flat(1.0f));   // the model gives 1 1 0
    EXPECT_EQ(XPE_OK, r.rc);
    EXPECT_EQ("CHEST", r.label) << "on a tie the first class wins";
    EXPECT_EQ(1.0f, r.confidence);
    EXPECT_TRUE(Alerts().empty()) << "1.0 is inside [0, 1]";
}

TEST_F(BodyPart, ANhwcModelGivesTheSameAnswersAsTheNchwOne) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_nhwc"));
    const Result top = Recognize(TopBright());
    const Result bottom = Recognize(BottomBright());
    EXPECT_EQ(XPE_OK, top.rc);
    EXPECT_EQ("CHEST", top.label);
    EXPECT_EQ(0.75f, top.confidence);
    EXPECT_EQ("ABDOMEN", bottom.label);
}

TEST_F(BodyPart, AnImageOfAnotherSizeIsResizedToTheModelsInputBeforeItIsRun) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_dep"));   // the model wants 4 x 4
    // Larger, non-square, and not a multiple of 4 on either side: only a real resize makes this answerable.
    const Result top = Recognize(TopBright(1000, 602));
    const Result bottom = Recognize(BottomBright(1000, 602));
    EXPECT_EQ(XPE_OK, top.rc);
    EXPECT_EQ("CHEST", top.label);
    EXPECT_EQ(0.75f, top.confidence) << "the top half is uniformly bright, so every top sample averages to 0.75";
    EXPECT_EQ("ABDOMEN", bottom.label);
    // And smaller than the model's input.
    const Result small = Recognize(TopBright(2, 2));
    EXPECT_EQ(XPE_OK, small.rc);
    EXPECT_EQ("CHEST", small.label);
}

// ===== a model that cannot be used: the stub's outcome, and ONE warning per session ================================

namespace {
void ExpectUnavailable(const std::string& dir, const char* reasonNeedle) {
    xpe_ai_shutdown();
    ASSERT_EQ(XPE_OK, xpe_ai_init(dir.c_str(), nullptr));
    xpe_clear_alerts();
    const Img image = Flat(0.5f);
    const Result first = Recognize(image);
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, first.rc) << dir;
    EXPECT_EQ("UNKNOWN", first.label) << dir;
    EXPECT_EQ(0.0f, first.confidence) << dir;
    const std::vector<Alert> after1 = Alerts();
    ASSERT_EQ(1u, after1.size()) << dir << ": one Warning on the first failed call";
    EXPECT_EQ(XPE_ALERT_WARNING, after1[0].severity);
    EXPECT_NE(std::string::npos, after1[0].text.find("body-part recognition is unavailable")) << after1[0].text;
    EXPECT_NE(std::string::npos, after1[0].text.find(reasonNeedle)) << dir << ": " << after1[0].text;

    const Result second = Recognize(image);
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, second.rc);
    EXPECT_EQ("UNKNOWN", second.label);
    EXPECT_EQ(1u, Alerts().size()) << dir << ": the second call must not alert again (a per-call alert fills the queue)";

    // A new session alerts again.
    xpe_ai_shutdown();
    ASSERT_EQ(XPE_OK, xpe_ai_init(dir.c_str(), nullptr));
    xpe_clear_alerts();
    (void)Recognize(image);
    EXPECT_EQ(1u, Alerts().size()) << dir << ": a new init starts a new session, so it warns again";
}
}  // namespace

TEST_F(BodyPart, NoModelFileIsTheStubsOutcomeWithOneWarning) {
    REQUIRE_ONNX();
    ExpectUnavailable(Dir("models_missing"), "no model file");
}
TEST_F(BodyPart, ABrokenModelFileIsTheStubsOutcomeWithOneWarning) {
    REQUIRE_ONNX();
    ExpectUnavailable(Dir("models_bodypart_broken"), "cannot be loaded");
}
TEST_F(BodyPart, MissingLabelsAreTheStubsOutcomeWithOneWarning) {
    REQUIRE_ONNX();
    ExpectUnavailable(Dir("models_bodypart_no_labels"), "not found");
}
TEST_F(BodyPart, MoreOutputsThanLabelsAreTheStubsOutcomeWithOneWarning) {
    REQUIRE_ONNX();
    ExpectUnavailable(Dir("models_bodypart_labels_mismatch"), "differs from the number of labels");
}
TEST_F(BodyPart, ARankTwoInputIsTheStubsOutcomeWithOneWarning) {
    REQUIRE_ONNX();
    ExpectUnavailable(Dir("models_bodypart_rank2"), "input shape");
}
TEST_F(BodyPart, ADynamicInputIsTheStubsOutcomeWithOneWarning) {
    REQUIRE_ONNX();
    ExpectUnavailable(Dir("models_bodypart_dynamic"), "input shape");
}

TEST_F(BodyPart, WithoutAModelNoImageFormatChangesTheOutcome) {
    // A caller that never had a model must not start getting UNSUPPORTED_FORMAT: the stub never looked at the format.
    REQUIRE_ONNX();
    Init(Dir("models_missing"));
    std::vector<uint16_t> raw(16, 7);
    XpeImageBuffer img{};
    img.width = 4;
    img.height = 4;
    img.bitsAllocated = 16;
    img.bitsStored = 16;
    img.format = XPE_PIXEL_UINT16;
    img.data = raw.data();
    img.dataSize = raw.size() * sizeof(uint16_t);
    char label[64] = {};
    float conf = -1.0f;
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, xpe_bodypart_recognize(&img, label, sizeof(label), &conf));
    EXPECT_STREQ("UNKNOWN", label);
    EXPECT_EQ(0.0f, conf);
}

// ===== a model that answers, but not with a probability vector =====================================================

TEST_F(BodyPart, ANonFiniteModelResultIsRefusedWithTheNonFiniteAlert) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_nonfinite"));
    for (float v : {1.0f, 0.0f}) {   // x * inf = inf, 0 * inf = NaN: both are refused
        const Img image = Flat(v);
        xpe_clear_alerts();
        const Result r = Recognize(image);
        EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, r.rc);
        EXPECT_EQ("UNKNOWN", r.label);
        EXPECT_EQ(0.0f, r.confidence) << "the model's value must not leak out";
        const std::vector<Alert> a = Alerts();
        ASSERT_EQ(1u, a.size());
        EXPECT_EQ(XPE_ALERT_WARNING, a[0].severity);
        EXPECT_NE(std::string::npos, a[0].text.find("non-finite")) << a[0].text;
    }
}

TEST_F(BodyPart, AProbabilityAboveOneIsRefused) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_range_high"));
    const Result r = Recognize(Flat(0.0f));
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, r.rc);
    EXPECT_EQ("UNKNOWN", r.label);
    EXPECT_EQ(0.0f, r.confidence);
    EXPECT_EQ(1, CountAlerts("not a probability vector"));
}

TEST_F(BodyPart, ANegativeValueIsRefusedEvenWhenTheLargestIsInRange) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_range_low"));   // 0.5 0.2 -0.1: the maximum, 0.5, is fine; the vector is not
    const Result r = Recognize(Flat(0.0f));
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, r.rc);
    EXPECT_EQ("UNKNOWN", r.label);
    EXPECT_EQ(1, CountAlerts("not a probability vector"));
}

// ===== argument and format contract on a working model ======================================================

TEST_F(BodyPart, ANonFloatImageIsUnsupportedWhenAModelIsThere) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_a"));
    std::vector<uint16_t> raw(16, 7);
    XpeImageBuffer img{};
    img.width = 4;
    img.height = 4;
    img.bitsAllocated = 16;
    img.bitsStored = 16;
    img.format = XPE_PIXEL_UINT16;
    img.data = raw.data();
    img.dataSize = raw.size() * sizeof(uint16_t);
    char label[64];
    std::memset(label, 'x', sizeof(label));
    float conf = -1.0f;
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, xpe_bodypart_recognize(&img, label, sizeof(label), &conf));
    EXPECT_EQ('x', label[0]) << "nothing is written for a refused format";
    EXPECT_EQ(0.0f, conf);
}

TEST_F(BodyPart, ALabelThatDoesNotFitIsBufferTooSmallAndNeverTruncated) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_a"));   // "CHEST" needs 6 bytes with its terminator
    const Result tight = Recognize(Flat(0.0f), 6);
    EXPECT_EQ(XPE_OK, tight.rc);
    EXPECT_EQ("CHEST", tight.label);
    const Result tooShort = Recognize(Flat(0.0f), 5);
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL, tooShort.rc);
    EXPECT_EQ("xxxxx", tooShort.label) << "the buffer must be untouched, not holding a truncated label";
    EXPECT_EQ(0.0f, tooShort.confidence);
}

TEST_F(BodyPart, WithoutAModelAShortBufferIsStillBufferTooSmall) {
    REQUIRE_ONNX();
    Init(Dir("models_missing"));
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL, Recognize(Flat(0.0f), 7).rc) << "\"UNKNOWN\" needs 8 bytes, as in the stub";
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, Recognize(Flat(0.0f), 8).rc);
}

// ===== the resize helper, directly =================================================================================

TEST(BodyPartResize, ASameSizeImageIsCopiedBitForBit) {
    const Img image(7, 5, [](uint32_t x, uint32_t y) { return 0.1f * static_cast<float>(x) - 3.3f * static_cast<float>(y); });
    const std::vector<float> out = xpe::ai::ResizeImageFloat(image.px.data(), 7, 5, 7, 5);
    ASSERT_EQ(image.px.size(), out.size());
    EXPECT_EQ(0, std::memcmp(image.px.data(), out.data(), out.size() * sizeof(float)));
}

TEST(BodyPartResize, ShrinkingAveragesTheArea) {
    // 4x4 -> 2x2: each output is the mean of a 2x2 block.
    const float v[16] = {1, 3, 5, 7,
                         1, 3, 5, 7,
                         0, 2, 4, 6,
                         0, 2, 4, 6};
    const std::vector<float> out = xpe::ai::ResizeImageFloat(v, 4, 4, 2, 2);
    ASSERT_EQ(4u, out.size());
    EXPECT_EQ(2.0f, out[0]);
    EXPECT_EQ(6.0f, out[1]);
    EXPECT_EQ(1.0f, out[2]);
    EXPECT_EQ(5.0f, out[3]);
    // 4x4 -> 1x1 is the mean of everything.
    const std::vector<float> one = xpe::ai::ResizeImageFloat(v, 4, 4, 1, 1);
    EXPECT_EQ(3.5f, one[0]);
}

TEST(BodyPartResize, ShrinkingByAFractionWeightsTheEdgeSamples) {
    // 3 -> 2: output 0 covers [0, 1.5), output 1 covers [1.5, 3): (1*1 + 2*0.5)/1.5 and (2*0.5 + 3*1)/1.5.
    const float v[3] = {1.0f, 2.0f, 3.0f};
    const std::vector<float> out = xpe::ai::ResizeImageFloat(v, 3, 1, 2, 1);
    ASSERT_EQ(2u, out.size());
    EXPECT_NEAR((1.0 + 1.0) / 1.5, out[0], 1e-6);
    EXPECT_NEAR((1.0 + 3.0) / 1.5, out[1], 1e-6);
}

TEST(BodyPartResize, EnlargingInterpolatesAndStaysInsideTheSourceRange) {
    const float v[4] = {0.0f, 1.0f, 2.0f, 3.0f};   // 2x2
    const std::vector<float> out = xpe::ai::ResizeImageFloat(v, 2, 2, 4, 4);
    ASSERT_EQ(16u, out.size());
    EXPECT_EQ(0.0f, out[0]) << "the first sample sits on the first source pixel (edge clamped)";
    EXPECT_EQ(3.0f, out[15]) << "and the last on the last";
    for (float x : out) {
        EXPECT_GE(x, 0.0f);
        EXPECT_LE(x, 3.0f);
    }
    EXPECT_LT(out[0], out[1]) << "interpolating, not repeating";
}

TEST(BodyPartResize, AConstantImageStaysConstantForAnySizes) {
    for (uint32_t sw : {1u, 3u, 7u, 16u}) {
        for (uint32_t dw : {1u, 2u, 5u, 9u}) {
            const Img image(sw, 5, [](uint32_t, uint32_t) { return 0.3125f; });
            const std::vector<float> out = xpe::ai::ResizeImageFloat(image.px.data(), sw, 5, dw, 3);
            for (float x : out) EXPECT_NEAR(0.3125f, x, 1e-7f) << sw << " -> " << dw;
        }
    }
}

TEST(BodyPartResize, ShrinkingKeepsTheMean) {
    const Img image(37, 23, [](uint32_t x, uint32_t y) { return static_cast<float>((x * 7 + y * 13) % 17); });
    double src = 0.0;
    for (float x : image.px) src += x;
    const std::vector<float> out = xpe::ai::ResizeImageFloat(image.px.data(), 37, 23, 5, 4);
    double dst = 0.0;
    for (float x : out) dst += x;
    EXPECT_NEAR(src / image.px.size(), dst / out.size(), 1e-4) << "area averaging preserves the mean";
}

TEST(BodyPartResize, TheModuleMaximumSizeResizesWithoutIncident) {
    const Img big(4096, 4096, [](uint32_t x, uint32_t) { return static_cast<float>(x & 1u); });
    const std::vector<float> out = xpe::ai::ResizeImageFloat(big.px.data(), 4096, 4096, 512, 512);
    ASSERT_EQ(static_cast<size_t>(512) * 512, out.size());
    EXPECT_NEAR(0.5f, out[0], 1e-6f);
}

TEST(BodyPartInputSize, TheShapesTheModuleWillFeed) {
    uint32_t h = 0, w = 0;
    EXPECT_TRUE(xpe::ai::BodyPartInputSize({1, 1, 4, 6}, &h, &w));
    EXPECT_EQ(4u, h);
    EXPECT_EQ(6u, w);
    EXPECT_TRUE(xpe::ai::BodyPartInputSize({1, 4, 6, 1}, &h, &w));
    EXPECT_EQ(4u, h);
    EXPECT_EQ(6u, w);
    EXPECT_TRUE(xpe::ai::BodyPartInputSize({1, 1, 4096, 4096}, &h, &w));
}

TEST(BodyPartInputSize, TheShapesItWillNot) {
    uint32_t h = 7, w = 7;
    EXPECT_FALSE(xpe::ai::BodyPartInputSize({1, 16}, &h, &w)) << "rank 2";
    EXPECT_FALSE(xpe::ai::BodyPartInputSize({1, 1, 1, 4, 4}, &h, &w)) << "rank 5";
    EXPECT_FALSE(xpe::ai::BodyPartInputSize({2, 1, 4, 4}, &h, &w)) << "a batch of two";
    EXPECT_FALSE(xpe::ai::BodyPartInputSize({1, 3, 4, 4}, &h, &w)) << "three channels";
    EXPECT_FALSE(xpe::ai::BodyPartInputSize({1, 1, -1, 4}, &h, &w)) << "a dynamic axis";
    EXPECT_FALSE(xpe::ai::BodyPartInputSize({1, 1, 0, 4}, &h, &w)) << "a zero side";
    EXPECT_FALSE(xpe::ai::BodyPartInputSize({1, 1, 4097, 4}, &h, &w)) << "above the module maximum";
    EXPECT_EQ(7u, h) << "a refusal leaves the outputs alone";
    EXPECT_EQ(7u, w);
}

// ===== M3: the threshold, the low-confidence event (REQ-AI-012) and fallback_mode ====================================
//
// models_bodypart_a answers 0.6 / 0.3 / 0.1 for any image, so its confidence is exactly 0.6f, the default
// threshold. The alert texts below are written out LITERALLY, not built the way the module builds them: a test that
// rebuilt the text with the module's own formatter would agree with it even when both were wrong.

namespace {
// The float one step above 0.6f, as JSON that reads back as exactly that float.
constexpr const char* kJustAbove06 = "{\"confidence_threshold\": 0.6000000834465027}";
constexpr const char* kJustAbove06FallbackOff =
    "{\"confidence_threshold\": 0.6000000834465027, \"fallback_mode\": false}";

const char* kLowFallbackOn =
    "AI body-part confidence 0.6 is below the threshold 0.6000001 (REQ-AI-012): UNKNOWN is returned; "
    "use the deterministic body-part lookup";
const char* kLowFallbackOff =
    "AI body-part confidence 0.6 is below the threshold 0.6000001 (REQ-AI-012): the label CHEST is returned "
    "because fallback_mode is off; an exposure parameter chosen from it may be wrong";

void ExpectOneAlert(const char* exactText) {
    const std::vector<Alert> a = Alerts();
    ASSERT_EQ(1u, a.size());
    EXPECT_EQ(XPE_ALERT_WARNING, a[0].severity);
    EXPECT_EQ(std::string(exactText), a[0].text);
}
}  // namespace

TEST_F(BodyPart, AConfidenceExactlyAtTheThresholdPasses) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_a"));   // confidence 0.6f, default threshold 0.6f
    const Result r = Recognize(Flat(0.0f));
    EXPECT_EQ(XPE_OK, r.rc);
    EXPECT_EQ("CHEST", r.label);
    EXPECT_EQ(0.6f, r.confidence);
    EXPECT_TRUE(Alerts().empty()) << "equal is not below";
}

TEST_F(BodyPart, AConfidenceOneFloatBelowTheThresholdIsLowAndFallsBack) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_a"), kJustAbove06);
    const Result r = Recognize(Flat(0.0f));
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, r.rc);
    EXPECT_EQ("UNKNOWN", r.label);
    EXPECT_EQ(0.6f, r.confidence) << "the MEASURED confidence is reported, not 0.0";
    ExpectOneAlert(kLowFallbackOn);
}

TEST_F(BodyPart, WithFallbackModeOffTheLowConfidenceLabelIsReturnedWithAWarning) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_a"), kJustAbove06FallbackOff);
    const Result r = Recognize(Flat(0.0f));
    EXPECT_EQ(XPE_OK, r.rc);
    EXPECT_EQ("CHEST", r.label);
    EXPECT_EQ(0.6f, r.confidence);
    ExpectOneAlert(kLowFallbackOff);
}

TEST_F(BodyPart, FallbackModeCanBeToggledAtRunTimeAndTheNextCallFollows) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_a"), kJustAbove06);
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, Recognize(Flat(0.0f)).rc) << "default: fallback on";
    ASSERT_EQ(XPE_OK, xpe_ai_set_fallback_mode(0));
    xpe_clear_alerts();
    const Result off = Recognize(Flat(0.0f));
    EXPECT_EQ(XPE_OK, off.rc);
    EXPECT_EQ("CHEST", off.label);
    ExpectOneAlert(kLowFallbackOff);
    ASSERT_EQ(XPE_OK, xpe_ai_set_fallback_mode(1));
    xpe_clear_alerts();
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, Recognize(Flat(0.0f)).rc) << "switched back on";
    ExpectOneAlert(kLowFallbackOn);
}

TEST_F(BodyPart, TheThresholdIsTheConfiguredOneNotAConstant) {
    REQUIRE_ONNX();
    const Img flat = Flat(0.0f);   // the dep model gives 0 0 0 for it: confidence 0.0
    Init(Dir("models_bodypart_dep"));
    const Result low = Recognize(flat);
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, low.rc) << "0.0 is below the default 0.6";
    EXPECT_EQ(0.0f, low.confidence);
    EXPECT_EQ(1u, Alerts().size());
    Init(Dir("models_bodypart_dep"), "{\"confidence_threshold\": 0.0}");
    const Result pass = Recognize(flat);
    EXPECT_EQ(XPE_OK, pass.rc) << "0.0 is not below a threshold of 0.0";
    EXPECT_EQ("CHEST", pass.label);
    EXPECT_TRUE(Alerts().empty());
}

TEST_F(BodyPart, AThresholdOfOneAcceptsOnlyAFullScaleConfidence) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_dep"), "{\"confidence_threshold\": 1.0}");
    EXPECT_EQ(XPE_OK, Recognize(Flat(1.0f)).rc) << "the model gives 1 1 0: confidence 1.0 is not below 1.0";
    EXPECT_TRUE(Alerts().empty());
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, Recognize(TopBright()).rc) << "0.75 is below 1.0";
    EXPECT_EQ(1u, Alerts().size());
}

TEST_F(BodyPart, EveryLowConfidenceImageRaisesItsOwnEventButAPassingOneRaisesNone) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_dep"), "{\"confidence_threshold\": 0.8}");
    (void)Recognize(TopBright());      // 0.75: low
    (void)Recognize(BottomBright());   // 0.75: low
    (void)Recognize(Flat(1.0f));       // 1.0: passes
    const std::vector<Alert> a = Alerts();
    ASSERT_EQ(2u, a.size()) << "one event per low-confidence image";
    for (const Alert& x : a) {
        EXPECT_EQ(XPE_ALERT_WARNING, x.severity);
        EXPECT_NE(std::string::npos, x.text.find("below the threshold 0.8")) << x.text;
    }
}

TEST_F(BodyPart, ALowConfidenceIsNotAnUnavailableModelAndDoesNotUseUpItsOneWarning) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_dep"));
    (void)Recognize(Flat(0.0f));   // low confidence: 0.0
    EXPECT_EQ(0, CountAlerts("unavailable")) << "the model is fine; it was just not sure";
    // The "unavailable" warning is still owed to a session whose model turns out to be unusable.
    Init(Dir("models_missing"));
    (void)Recognize(Flat(0.0f));
    EXPECT_EQ(1, CountAlerts("unavailable"));
}

TEST_F(BodyPart, ALowConfidenceFallbackNeedsRoomForUnknownAndStillRaisesTheEvent) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_a"), kJustAbove06);
    const Result tooShort = Recognize(Flat(0.0f), 7);   // "UNKNOWN" needs 8 bytes
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL, tooShort.rc);
    EXPECT_EQ("xxxxxxx", tooShort.label) << "nothing written";
    EXPECT_EQ(0.0f, tooShort.confidence);
    ExpectOneAlert(kLowFallbackOn);   // the event is the image's, not the buffer's
    xpe_clear_alerts();
    const Result fits = Recognize(Flat(0.0f), 8);
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, fits.rc);
    EXPECT_EQ("UNKNOWN", fits.label);
    EXPECT_EQ(0.6f, fits.confidence);
}

TEST_F(BodyPart, WithFallbackModeOffALabelThatDoesNotFitIsStillBufferTooSmall) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_a"), kJustAbove06FallbackOff);
    const Result r = Recognize(Flat(0.0f), 5);   // "CHEST" needs 6
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL, r.rc);
    EXPECT_EQ("xxxxx", r.label);
    ExpectOneAlert(kLowFallbackOff);
}

TEST_F(BodyPart, ANonNumericThresholdInTheConfigIsIgnoredAndTheDefaultApplies) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_a"), "{\"confidence_threshold\": \"0.99\"}");   // a string: not a number
    const Result r = Recognize(Flat(0.0f));
    EXPECT_EQ(XPE_OK, r.rc) << "0.6 against the default 0.6 passes; a 0.99 threshold would have refused it";
}

// ===== M3b: what the resize costs (a measurement, not a requirement) ===============================================
//
// PRD (xpe-ai-prd.md, SWU-2.7) asks for <= 300 ms on a 3072 x 3072 image INCLUDING resize and inference. This
// measures the part this repository owns, the resize, at the sizes the PRD names. THE INFERENCE TIME PRINTED FOR
// THE TOY MODEL IS NOT A CLAIM ABOUT ANY REAL MODEL: the toy takes a 4 x 4 input and does one matrix product, so
// it says nothing about a 512 x 512 MobileNet. The test asserts no time limit (a limit measured on one machine
// would fail on another, #214); it asserts only that the measurement is not blind -- the call really ran, and the
// cost grows with the area.

namespace {
using Clock = std::chrono::steady_clock;

double Ms(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

struct Stats {
    double min;
    double median;
};

template <typename F>
Stats Time(int runs, F&& f) {
    std::vector<double> t;
    for (int i = 0; i < runs; ++i) {
        const auto a = Clock::now();
        f();
        t.push_back(Ms(a, Clock::now()));
    }
    std::sort(t.begin(), t.end());
    return {t.front(), t[t.size() / 2]};
}
}  // namespace

TEST_F(BodyPart, MeasureResizeAndRecognizeLatency) {
    REQUIRE_ONNX();
    constexpr int kRuns = 12;
    std::printf("BP-LATENCY runs=%d (min and median of %d), RelWithDebInfo, single thread\n", kRuns, kRuns);
    std::printf("BP-LATENCY resize only, target 512x512 (the PRD's input size), no model: side min_ms median_ms\n");
    double resizeMin1024 = 0.0, resizeMin3072 = 0.0;
    for (const uint32_t side : {1024u, 3072u}) {
        const Img image = TopBright(side, side);
        std::vector<float> sink;
        const Stats s = Time(kRuns, [&] { sink = xpe::ai::ResizeImageFloat(image.px.data(), side, side, 512, 512); });
        EXPECT_EQ(static_cast<size_t>(512) * 512, sink.size());
        EXPECT_EQ(0.75f, sink[0]) << "the measured call really resized (top rows are bright)";
        EXPECT_EQ(0.0f, sink[static_cast<size_t>(511) * 512]) << "and the bottom rows are not";
        std::printf("BP-LATENCY resize %4u %8.2f %8.2f\n", side, s.min, s.median);
        (side == 1024u ? resizeMin1024 : resizeMin3072) = s.min;
    }
    EXPECT_GT(resizeMin3072, resizeMin1024) << "nine times the pixels must cost more";

    std::printf("BP-LATENCY whole call through the C ABI with the TOY model (4x4 input, NOT a real model): "
                "side min_ms median_ms\n");
    Init(Dir("models_bodypart_dep"));
    for (const uint32_t side : {1024u, 3072u}) {
        const Img image = TopBright(side, side);
        Result r{};
        const Stats s = Time(kRuns, [&] { r = Recognize(image); });
        EXPECT_EQ(XPE_OK, r.rc);
        EXPECT_EQ("CHEST", r.label) << "the measured call really answered";
        std::printf("BP-LATENCY recognize %4u %8.2f %8.2f\n", side, s.min, s.median);
    }
    std::printf("BP-LATENCY the toy model's own inference at 4x4, for scale (image already 4x4): ");
    const Img tiny = TopBright();
    Result r{};
    const Stats s = Time(kRuns, [&] { r = Recognize(tiny); });
    EXPECT_EQ(XPE_OK, r.rc);
    std::printf("min %.3f ms median %.3f ms\n", s.min, s.median);
}
