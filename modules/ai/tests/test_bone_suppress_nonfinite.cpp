/**
 * @file test_bone_suppress_nonfinite.cpp
 * @brief QA-B-181h (Codex #60): xpe_bone_suppress must not report success with a non-finite result.
 *
 * The model models_x2/bone_suppress.onnx computes Y = X * 2. A FINITE pixel above FLT_MAX / 2 therefore
 * comes out as +infinity. Before this card the result went to the caller as XPE_OK, in-process and
 * through the worker. Policy (QA-B-181f): a module neither makes non-finite output from finite input nor
 * reports success on it; the refusal is XPE_ERR_INVALID_INPUT and the caller's image is not touched.
 *
 * Full ONNX build only (the stub has no model, so no result exists to judge).
 */
#include <gtest/gtest.h>

#include "xpe/ai/ai_api.h"
#include "xpe/ai/ai_onnx_session.h"

#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#ifndef XPE_AI_TEST_DATA_DIR
#error "XPE_AI_TEST_DATA_DIR must be defined by the build (modules/ai/CMakeLists.txt)"
#endif

namespace {

const std::string kDirX2 = std::string(XPE_AI_TEST_DATA_DIR) + "/models_x2";
constexpr uint32_t kW = 4, kH = 3;
constexpr size_t kN = static_cast<size_t>(kW) * kH;

struct Img {
    std::vector<float> px;
    XpeImageBuffer buf{};
    explicit Img(float fill) : px(kN, fill) { Bind(); }
    explicit Img(const std::vector<float>& v) : px(v) { Bind(); }
    void Bind() {
        buf.width = kW;
        buf.height = kH;
        buf.bitsAllocated = 32;
        buf.bitsStored = 32;
        buf.format = XPE_PIXEL_FLOAT32;
        buf.data = px.data();
        buf.dataSize = kN * sizeof(float);
    }
};

std::vector<float> Ordinary() {
    std::vector<float> v(kN);
    for (size_t i = 0; i < kN; ++i) v[i] = 1.0f + static_cast<float>(i);
    return v;
}

bool SameBits(const std::vector<float>& a, const std::vector<float>& b) {
    return a.size() == b.size() && std::memcmp(a.data(), b.data(), a.size() * sizeof(float)) == 0;
}

bool AnyNonFinite(const std::vector<float>& v) {
    for (float f : v) {
        uint32_t u;
        std::memcpy(&u, &f, sizeof(u));
        if ((u & 0x7F800000u) == 0x7F800000u) return true;
    }
    return false;
}

struct Case { const char* name; std::vector<float> in; };

std::vector<Case> ExtremeInputs() {
    std::vector<Case> c;
    for (size_t pos : {size_t{0}, kN / 2, kN - 1}) {   // first, middle and last pixel: the scan covers all
        std::vector<float> v = Ordinary();
        v[pos] = FLT_MAX;                              // finite, but x2 leaves float
        c.push_back({pos == 0 ? "FLT_MAX_first" : pos == kN - 1 ? "FLT_MAX_last" : "FLT_MAX_middle", v});
    }
    std::vector<float> neg = Ordinary();
    neg[5] = -FLT_MAX;
    c.push_back({"minus_FLT_MAX", neg});
    std::vector<float> just = Ordinary();
    just[5] = std::nextafter(FLT_MAX / 2.0f, INFINITY);   // the next float above FLT_MAX / 2
    c.push_back({"just_above_half_FLT_MAX", just});
    return c;
}

struct BoneSuppressNonFinite : public ::testing::Test {
    void SetUp() override {
        if (xpe::ai::OnnxSession::IsStubBuild()) GTEST_SKIP() << "stub build: no model, no result to judge";
        xpe_ai_shutdown();
    }
    void TearDown() override { xpe_ai_shutdown(); }
};

void RefusesExtremeInputs(const char* cfg) {
    ASSERT_EQ(XPE_OK, xpe_ai_init(kDirX2.c_str(), cfg));
    for (const Case& c : ExtremeInputs()) {
        Img in(c.in);
        Img out(-7.0f);                                // sentinel: "untouched" must be provable
        const std::vector<float> outBefore = out.px;
        const XpeErrorCode rc = xpe_bone_suppress(&in.buf, &out.buf, nullptr);
        EXPECT_NE(XPE_OK, rc) << c.name << ": a non-finite result was reported as success";
        EXPECT_FALSE(AnyNonFinite(out.px)) << c.name << ": +/-inf or NaN reached the caller's buffer";
        EXPECT_TRUE(SameBits(c.in, in.px)) << c.name << ": the input was modified";
        if (cfg == nullptr) {
            EXPECT_EQ(XPE_ERR_INVALID_INPUT, rc) << c.name;
            EXPECT_TRUE(SameBits(outBefore, out.px)) << c.name << ": output not left unchanged";
        } else {
            // The worker path's documented fallback on ANY failure: the output holds the input.
            EXPECT_TRUE(SameBits(c.in, out.px)) << c.name << ": worker-path fallback must be the input";
        }
    }
}

}  // namespace

TEST_F(BoneSuppressNonFinite, InProcessRefusesAFiniteInputWhoseResultLeavesFloat) {
    RefusesExtremeInputs(nullptr);
}

TEST_F(BoneSuppressNonFinite, WorkerPathRefusesAFiniteInputWhoseResultLeavesFloat) {
    RefusesExtremeInputs("{\"use_worker\": true}");
}

// The control: the same calls with ordinary pixels still succeed and give exactly Y = 2X, in both
// paths, bit for bit -- so the refusals above are the check firing, not the model or the harness failing.
TEST_F(BoneSuppressNonFinite, OrdinaryPixelsStillGiveExactlyTwiceTheInputInBothPaths) {
    for (const char* cfg : {static_cast<const char*>(nullptr), "{\"use_worker\": true}"}) {
        xpe_ai_shutdown();
        ASSERT_EQ(XPE_OK, xpe_ai_init(kDirX2.c_str(), cfg));
        Img in(Ordinary());
        Img out(-7.0f);
        ASSERT_EQ(XPE_OK, xpe_bone_suppress(&in.buf, &out.buf, nullptr)) << (cfg ? cfg : "in-process");
        std::vector<float> want = Ordinary();
        for (float& f : want) f *= 2.0f;
        EXPECT_TRUE(SameBits(want, out.px)) << (cfg ? cfg : "in-process");
    }
}

// The largest input that still fits: FLT_MAX / 2 doubles to exactly FLT_MAX, which is finite and must pass.
TEST_F(BoneSuppressNonFinite, ALargestFiniteResultIsNotRefused) {
    ASSERT_EQ(XPE_OK, xpe_ai_init(kDirX2.c_str(), nullptr));
    std::vector<float> v = Ordinary();
    v[3] = FLT_MAX / 2.0f;
    Img in(v);
    Img out(-7.0f);
    ASSERT_EQ(XPE_OK, xpe_bone_suppress(&in.buf, &out.buf, nullptr));
    EXPECT_EQ(FLT_MAX, out.px[3]);
}

// The two entry points that take an image buffer but have no implementation. The producer table says they
// can never hand a caller non-finite pixels because they never write any: after validation they return
// XPE_ERR_PROCESSING_FAILED unconditionally, in the stub AND in the ONNX build (their bodies hold no model
// call and no IPC message). This pins that, so a future implementation that starts writing must also
// answer to the finiteness policy -- the test then fails and the table row has to be redone.
TEST(AiStubProducers, StitchAndDenoiseNeverWriteTheirOutputInAnyBuild) {
    xpe_ai_shutdown();
    ASSERT_EQ(XPE_OK, xpe_ai_init(kDirX2.c_str(), nullptr));

    Img partA(1.0f), partB(2.0f);
    XpeImageBuffer parts[2] = {partA.buf, partB.buf};
    Img stitched(-7.0f);
    const std::vector<float> stitchedBefore = stitched.px;
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, xpe_stitch_images(parts, 2, &stitched.buf, nullptr));
    EXPECT_TRUE(SameBits(stitchedBefore, stitched.px)) << "xpe_stitch_images wrote its output";

    Img img(Ordinary());
    XpeImageMetadata meta{};
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, xpe_dl_denoise(&img.buf, &meta, nullptr));
    EXPECT_TRUE(SameBits(Ordinary(), img.px)) << "xpe_dl_denoise wrote its image";
    xpe_ai_shutdown();
}
