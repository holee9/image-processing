/**
 * @file test_bone_suppress_abi.cpp
 * @brief xpe_bone_suppress across the C ABI, on a real session (QA-B-161, #130).
 *
 * REQ-AI-050 / REQ-AI-051 name a U-Net and a sensitivity target. NEITHER IS
 * TESTED HERE and neither is met: the model is a toy scale graph. What is
 * tested is that the C ABI actually reaches a model and that its failures are
 * distinguishable -- the wiring, not the clinical claim.
 *
 * THE LOAD-BEARING TEST is ADifferentModelDirectoryChangesTheOutput. The same
 * pixels are pushed through two model directories that differ only in a scale
 * constant, and the numbers that come back must differ. An echo, an identity,
 * a constant, or a stub cannot pass it. The C ABI is where this matters most:
 * OnnxSession could be perfect while xpe_bone_suppress ignored it, and no test
 * below the ABI would notice (that is what #205 was).
 *
 * WHY xpe_bone_suppress AND NOT ANOTHER FUNCTION: float image in, float image
 * out matches what the model is, so the assertion carries up unchanged; and it
 * writes to a SEPARATE buffer, so the input survives as a control and an echo
 * is detectable. xpe_dl_denoise works in place and would destroy that control.
 *
 * ERROR CODES. The card asked that "no model", "model broken" and "inference
 * failed" not collapse into one code, so three directories exist:
 * models_x2 / models_x3 (loadable), models_missing (no file), models_broken
 * (a file that is not a model).
 */

#include <gtest/gtest.h>

#include "xpe/ai/ai_api.h"
#include "xpe/ai/ai_onnx_session.h"

#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace {

// gtest_discover_tests sets WORKING_DIRECTORY to modules/ai (CMakeLists).
const char* kDirX2      = "tests/data/models_x2";
const char* kDirX3      = "tests/data/models_x3";
const char* kDirMissing = "tests/data/models_missing";
const char* kDirBroken  = "tests/data/models_broken";

constexpr uint32_t kW = 3, kH = 3;
constexpr size_t   kN = static_cast<size_t>(kW) * kH;

struct Img {
    std::vector<float> px;
    XpeImageBuffer buf{};
    explicit Img(float base) : px(kN) {
        for (size_t i = 0; i < kN; ++i) px[i] = base + static_cast<float>(i);
        buf.width = kW;
        buf.height = kH;
        buf.bitsAllocated = 32;
        buf.bitsStored = 32;
        buf.format = XPE_PIXEL_FLOAT32;
        buf.data = px.data();
        buf.dataSize = kN * sizeof(float);
    }
};

// Each test owns the whole init/shutdown cycle: the session is cached on the
// module state, so a test that inherited another test's model directory would
// be asserting about the previous test's model.
struct BoneSuppressAbi : public ::testing::Test {
    void TearDown() override { xpe_ai_shutdown(); }
    static void Init(const char* dir) {
        xpe_ai_shutdown();
        ASSERT_EQ(XPE_OK, xpe_ai_init(dir, nullptr)) << dir;
    }
};

bool IsStub() { return xpe::ai::OnnxSession::IsStubBuild(); }

}  // namespace

// --- fixtures, or nothing below means anything -----------------------------

TEST_F(BoneSuppressAbi, ModelDirectoriesArePresent) {
    for (const char* d : {kDirX2, kDirX3, kDirBroken}) {
        const std::string p = std::string(d) + "/bone_suppress.onnx";
        std::ifstream f(p, std::ios::binary);
        EXPECT_TRUE(f.good()) << p << " is missing; run tests/data/make_min_models.py";
    }
    std::ifstream absent(std::string(kDirMissing) + "/bone_suppress.onnx", std::ios::binary);
    EXPECT_FALSE(absent.good())
        << kDirMissing << " must NOT contain a model -- it pins the not-found code";
}

// --- THE falsification, at the C ABI ---------------------------------------

TEST_F(BoneSuppressAbi, ADifferentModelDirectoryChangesTheOutput) {
    if (IsStub()) {
        GTEST_SKIP() << "stub build: no model is loaded, so there is nothing to "
                        "swap. Build the ci-ai preset (ONNX linked) to run this. "
                        "A job that expects the full build should set "
                        "XPE_AI_EXPECT_ONNX=1 so this skip cannot pass silently.";
    }
    Img in(1.0f);
    Img out2(0.0f), out3(0.0f);

    Init(kDirX2);
    ASSERT_EQ(XPE_OK, xpe_bone_suppress(&in.buf, &out2.buf, nullptr));

    Init(kDirX3);
    ASSERT_EQ(XPE_OK, xpe_bone_suppress(&in.buf, &out3.buf, nullptr));

    // The numbers, not merely "they differ".
    for (size_t i = 0; i < kN; ++i) {
        EXPECT_FLOAT_EQ(in.px[i] * 2.0f, out2.px[i]) << "i=" << i;
        EXPECT_FLOAT_EQ(in.px[i] * 3.0f, out3.px[i]) << "i=" << i;
    }
    EXPECT_NE(out2.px, out3.px)
        << "both model directories produced identical pixels -- the model is not "
           "deciding the output, the defect #154/#155 recorded";
    EXPECT_NE(in.px, out2.px) << "output equals input: this is an echo, not inference";
}

TEST_F(BoneSuppressAbi, TheInputImageIsNotModified) {
    if (IsStub()) GTEST_SKIP() << "stub build";
    Img in(1.0f);
    const std::vector<float> before = in.px;
    Img out(0.0f);

    Init(kDirX2);
    ASSERT_EQ(XPE_OK, xpe_bone_suppress(&in.buf, &out.buf, nullptr));
    EXPECT_EQ(before, in.px) << "the input buffer is const in the signature";
}

TEST_F(BoneSuppressAbi, DifferentPixelsGiveDifferentOutput) {
    if (IsStub()) GTEST_SKIP() << "stub build";
    Init(kDirX2);
    Img a(1.0f), b(10.0f);
    Img outA(0.0f), outB(0.0f);
    ASSERT_EQ(XPE_OK, xpe_bone_suppress(&a.buf, &outA.buf, nullptr));
    ASSERT_EQ(XPE_OK, xpe_bone_suppress(&b.buf, &outB.buf, nullptr));
    EXPECT_NE(outA.px, outB.px) << "the same model returned the same pixels for "
                                   "different inputs -- output ignores the image";
}

// --- the stub contract, stated separately ----------------------------------

TEST_F(BoneSuppressAbi, StubBuildFailsAndLeavesTheOutputAlone) {
    if (!IsStub()) {
        GTEST_SKIP() << "full build: the stub contract does not apply.";
    }
    Init(kDirX2);
    Img in(1.0f);
    Img out(0.0f);
    const std::vector<float> outBefore = out.px;

    const XpeErrorCode rc = xpe_bone_suppress(&in.buf, &out.buf, nullptr);
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, rc)
        << "a stub build has no model; XPE_OK here would let a stub read as a "
           "working inference path (#205)";
    EXPECT_EQ(outBefore, out.px)
        << "a failed call must not leave half-written pixels, and must not echo";
    EXPECT_NE(in.px, out.px) << "the stub must not copy the input into the output";
}

// --- three causes, three codes ---------------------------------------------

TEST_F(BoneSuppressAbi, MissingModelAndBrokenModelGiveDifferentCodes) {
    if (IsStub()) {
        GTEST_SKIP() << "stub build: no file is ever opened, so both directories "
                        "return the same code. That is itself why a stub cannot "
                        "verify model loading.";
    }
    Img in(1.0f);
    Img out(0.0f);

    Init(kDirMissing);
    const XpeErrorCode missing = xpe_bone_suppress(&in.buf, &out.buf, nullptr);

    Init(kDirBroken);
    const XpeErrorCode broken = xpe_bone_suppress(&in.buf, &out.buf, nullptr);

    EXPECT_EQ(XPE_ERR_IO_FAILED, missing) << "no model file at all";
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, broken) << "the file is there but unreadable";
    EXPECT_NE(missing, broken)
        << "one code for both leaves the caller unable to tell 'install the model' "
           "from 'the model you installed is broken'";
}

TEST_F(BoneSuppressAbi, NonFloatPixelsAreRejectedRatherThanReinterpreted) {
    Init(kDirX2);
    std::vector<uint16_t> raw(kN, 1000);
    XpeImageBuffer u16{};
    u16.width = kW; u16.height = kH;
    u16.bitsAllocated = 16; u16.bitsStored = 16;
    u16.format = XPE_PIXEL_UINT16;
    u16.data = raw.data();
    u16.dataSize = raw.size() * sizeof(uint16_t);

    Img out(0.0f);
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, xpe_bone_suppress(&u16, &out.buf, nullptr))
        << "16-bit pixels read as floats would return numbers instead of an error";
}

TEST_F(BoneSuppressAbi, WithoutInitItIsNotInitialized) {
    xpe_ai_shutdown();
    Img in(1.0f);
    Img out(0.0f);
    EXPECT_EQ(XPE_ERR_NOT_INITIALIZED, xpe_bone_suppress(&in.buf, &out.buf, nullptr));
}

TEST_F(BoneSuppressAbi, NullArgumentsOutrankEverythingElse) {
    Init(kDirX2);
    Img real(1.0f);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_bone_suppress(nullptr, &real.buf, nullptr));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_bone_suppress(&real.buf, nullptr, nullptr));
}

TEST_F(BoneSuppressAbi, MismatchedDimensionsAreInvalidInput) {
    Init(kDirX2);
    Img in(1.0f);
    std::vector<float> small(4, 0.0f);
    XpeImageBuffer out{};
    out.width = 2; out.height = 2;
    out.bitsAllocated = 32; out.bitsStored = 32;
    out.format = XPE_PIXEL_FLOAT32;
    out.data = small.data();
    out.dataSize = small.size() * sizeof(float);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_bone_suppress(&in.buf, &out, nullptr));
}

// --- session lifetime: the contract the header states ----------------------

TEST_F(BoneSuppressAbi, ReInitWithADifferentDirectorySwitchesTheModel) {
    if (IsStub()) GTEST_SKIP() << "stub build";
    Img in(1.0f);
    Img first(0.0f), second(0.0f), third(0.0f);

    Init(kDirX2);
    ASSERT_EQ(XPE_OK, xpe_bone_suppress(&in.buf, &first.buf, nullptr));
    // Same directory again: the cached session is reused, same answer.
    ASSERT_EQ(XPE_OK, xpe_bone_suppress(&in.buf, &second.buf, nullptr));
    EXPECT_EQ(first.px, second.px);

    Init(kDirX3);
    ASSERT_EQ(XPE_OK, xpe_bone_suppress(&in.buf, &third.buf, nullptr));
    EXPECT_NE(first.px, third.px)
        << "re-init pointed at another model directory but the old session kept "
           "serving -- a cache that survives its own key";
}

TEST_F(BoneSuppressAbi, ShutdownThenUseIsRejectedNotCrashed) {
    Init(kDirX2);
    Img in(1.0f);
    Img out(0.0f);
    if (!IsStub()) ASSERT_EQ(XPE_OK, xpe_bone_suppress(&in.buf, &out.buf, nullptr));

    xpe_ai_shutdown();
    EXPECT_EQ(XPE_ERR_NOT_INITIALIZED, xpe_bone_suppress(&in.buf, &out.buf, nullptr))
        << "the session is freed by shutdown; a later call must be refused, not "
           "reach a dangling session";

    xpe_ai_shutdown();   // idempotent, and must not double-free the session
}
