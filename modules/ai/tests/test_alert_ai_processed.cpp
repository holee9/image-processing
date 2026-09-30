/**
 * @file test_alert_ai_processed.cpp
 * @brief SRS-ALERT-004 -- the "AI-processed" Info alert (QA-B-168, #130).
 *
 * THE POINT OF THIS FILE IS THE NEGATIVE HALF. A success alert fails in one
 * direction only: by firing when nothing was applied. A suite that asserts
 * "the alert appears" is passed by an implementation that raises it
 * unconditionally at the top of the function, so the assertions that actually
 * constrain the code are the ones below that require the queue to stay EMPTY.
 *
 * Both halves are here, and they are deliberately asymmetric in what they
 * need:
 *
 *   POSITIVE -- needs a real model, so it runs in a FULL build only. It does
 *   not skip silently: DlAppliedRaisesTheAlert_ProvableWhenTheCallerSaysSo
 *   turns the skip into a red when XPE_AI_EXPECT_ONNX=1, the same guard
 *   test_onnx_session.cpp uses. A skip nobody can see is how "modules/ai is
 *   green" survived while nothing ran (#205).
 *
 *   NEGATIVE -- needs no model at all, so it runs in BOTH builds. That is the
 *   trap this file is written against: AiIpcBridgeTest gives byte-identical
 *   results in stub and full, which is precisely why it asserts nothing about
 *   the real path (QA-B-167). Here the two configurations are SUPPOSED to
 *   differ, and DlNotAppliedInAStubBuildRaisesNothing is where that shows.
 *
 * WHICH NEGATIVE PATHS EXIST. The card named enabled=0 pass-through, non-chest
 * skip and low-confidence return. NONE OF THE THREE IS IN THIS FUNCTION today:
 * xpe_bone_suppress ignores configJsonOrNull entirely ((void)configJsonOrNull,
 * ai.cpp) -- there is no enable flag, no body-part gate and no confidence
 * gate to exercise. Asserting on them would be asserting on code that does not
 * exist. The negative paths that DO exist are the early returns, and every one
 * of them is covered below.
 */

#include <gtest/gtest.h>

#include "xpe/ai/ai_api.h"
#include "xpe/ai/ai_onnx_session.h"
#include "xpe/common/xpe_error.h"

#include <cstdlib>
#include <string>
#include <vector>

namespace {

#ifndef XPE_AI_TEST_DATA_DIR
#error "XPE_AI_TEST_DATA_DIR must be defined by the build (modules/ai/CMakeLists.txt)"
#endif
const std::string kDataDir    = XPE_AI_TEST_DATA_DIR;
const std::string kDirX2      = kDataDir + "/models_x2";
const std::string kDirMissing = kDataDir + "/models_missing";
const std::string kDirBroken  = kDataDir + "/models_broken";

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

bool IsStub() { return xpe::ai::OnnxSession::IsStubBuild(); }

/** True when the caller declared it expects a full ONNX build. */
bool CallerExpectsOnnx() {
    const char* v = std::getenv("XPE_AI_EXPECT_ONNX");
    return v && v[0] == '1' && v[1] == '\0';
}

/** Number of queued alerts whose text contains @p needle. */
int CountAlertsContaining(const char* needle, int32_t* severityOut) {
    int hits = 0;
    const int32_t total = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < total; ++i) {
        char msg[512] = {0};
        int32_t sev = -1;
        if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) != XPE_OK) continue;
        if (std::string(msg).find(needle) != std::string::npos) {
            ++hits;
            if (severityOut) *severityOut = sev;
        }
    }
    return hits;
}

constexpr const char* kNeedle = "AI-processed";

// The queue is process-global, so a test that inherited another test's alerts
// would be counting them. Clear on both ends, and drop the session too: it is
// cached on the module state and carries the previous test's model directory.
struct AiProcessedAlert : public ::testing::Test {
    void SetUp() override { xpe_ai_shutdown(); xpe_clear_alerts(); }
    void TearDown() override { xpe_ai_shutdown(); xpe_clear_alerts(); }
    static void Init(const std::string& dir) {
        ASSERT_EQ(XPE_OK, xpe_ai_init(dir.c_str(), nullptr)) << dir;
        // init itself must not have queued anything, or every count below is
        // measuring the wrong thing.
        xpe_clear_alerts();
    }
};

}  // namespace

// --- the probe can see an alert at all -------------------------------------
//
// Without this, every "count == 0" below is passed by a broken reader just as
// happily as by a correct implementation. Absence needs a control.

TEST_F(AiProcessedAlert, TheProbeItselfCanSeeAQueuedInfoAlert) {
    xpe_alert_push("AI-processed: control entry", XPE_ALERT_INFO);
    int32_t sev = -1;
    EXPECT_EQ(1, CountAlertsContaining(kNeedle, &sev));
    EXPECT_EQ(XPE_ALERT_INFO, sev);
}

// --- positive: applied -> the alert fires ----------------------------------

TEST_F(AiProcessedAlert, DlAppliedRaisesTheInfoAlert) {
    if (IsStub()) {
        GTEST_SKIP() << "stub build: inference never succeeds, so the applied "
                        "path is unreachable. DlAppliedRaisesTheAlert_"
                        "ProvableWhenTheCallerSaysSo turns this skip red under "
                        "XPE_AI_EXPECT_ONNX=1 so it cannot pass silently.";
    }
    Init(kDirX2);
    Img in(1.0f), out(0.0f);
    ASSERT_EQ(XPE_OK, xpe_bone_suppress(&in.buf, &out.buf, nullptr));

    int32_t sev = -1;
    EXPECT_EQ(1, CountAlertsContaining(kNeedle, &sev))
        << "DL processing was applied; SRS-ALERT-004 must be queued";
    EXPECT_EQ(XPE_ALERT_INFO, sev)
        << "SRS-ALERT-004 is Info -- every failure row in the SRS alert table "
           "is Warning or Error, which is what settles the polarity";
}

TEST_F(AiProcessedAlert, OneCallRaisesExactlyOneAlert) {
    if (IsStub()) GTEST_SKIP() << "stub build";
    Init(kDirX2);
    Img in(1.0f), out(0.0f);
    ASSERT_EQ(XPE_OK, xpe_bone_suppress(&in.buf, &out.buf, nullptr));
    EXPECT_EQ(1, CountAlertsContaining(kNeedle, nullptr))
        << "one application, one alert -- a per-tile or per-row push would "
           "flood the 64-entry queue and evict real warnings (SRS-ALERT-007)";
}

TEST_F(AiProcessedAlert, TwoCallsRaiseTwoAlerts) {
    if (IsStub()) GTEST_SKIP() << "stub build";
    Init(kDirX2);
    Img in(1.0f), out(0.0f);
    ASSERT_EQ(XPE_OK, xpe_bone_suppress(&in.buf, &out.buf, nullptr));
    ASSERT_EQ(XPE_OK, xpe_bone_suppress(&in.buf, &out.buf, nullptr));
    EXPECT_EQ(2, CountAlertsContaining(kNeedle, nullptr))
        << "the alert reports an application, not a session -- a once-only "
           "push would leave later images untagged";
}

// --- negative: NOT applied -> nothing is queued ----------------------------
//
// These are the assertions that pin the push to the success exit. An
// implementation that raises the alert on entry passes every positive test
// above and fails every one of these.

TEST_F(AiProcessedAlert, UnsupportedFormatRaisesNothing) {
    Init(kDirX2);
    Img in(1.0f), out(0.0f);
    in.buf.format = XPE_PIXEL_UINT16;   // rejected before any model is touched
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT,
              xpe_bone_suppress(&in.buf, &out.buf, nullptr));
    EXPECT_EQ(0, CountAlertsContaining(kNeedle, nullptr))
        << "no image was processed, so nothing is AI-processed";
}

TEST_F(AiProcessedAlert, MissingModelRaisesNothing) {
    Init(kDirMissing);
    Img in(1.0f), out(0.0f);
    EXPECT_NE(XPE_OK, xpe_bone_suppress(&in.buf, &out.buf, nullptr));
    EXPECT_EQ(0, CountAlertsContaining(kNeedle, nullptr));
}

TEST_F(AiProcessedAlert, BrokenModelRaisesNothing) {
    Init(kDirBroken);
    Img in(1.0f), out(0.0f);
    EXPECT_NE(XPE_OK, xpe_bone_suppress(&in.buf, &out.buf, nullptr));
    EXPECT_EQ(0, CountAlertsContaining(kNeedle, nullptr));
}

TEST_F(AiProcessedAlert, MismatchedDimensionsRaiseNothing) {
    Init(kDirX2);
    Img in(1.0f), out(0.0f);
    out.buf.width = kW + 1;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_bone_suppress(&in.buf, &out.buf, nullptr));
    EXPECT_EQ(0, CountAlertsContaining(kNeedle, nullptr));
}

TEST_F(AiProcessedAlert, WithoutInitRaisesNothing) {
    xpe_ai_shutdown();
    xpe_clear_alerts();
    Img in(1.0f), out(0.0f);
    EXPECT_EQ(XPE_ERR_NOT_INITIALIZED,
              xpe_bone_suppress(&in.buf, &out.buf, nullptr));
    EXPECT_EQ(0, CountAlertsContaining(kNeedle, nullptr));
}

// --- the two configurations must NOT agree ---------------------------------

TEST_F(AiProcessedAlert, DlNotAppliedInAStubBuildRaisesNothing) {
    if (!IsStub()) {
        GTEST_SKIP() << "full build: inference succeeds, so this contract -- "
                        "'a stub applies nothing, therefore tags nothing' -- "
                        "does not apply. The full-build counterpart is "
                        "DlAppliedRaisesTheInfoAlert.";
    }
    Init(kDirX2);
    Img in(1.0f), out(0.0f);
    EXPECT_NE(XPE_OK, xpe_bone_suppress(&in.buf, &out.buf, nullptr))
        << "a stub build must not report success";
    EXPECT_EQ(0, CountAlertsContaining(kNeedle, nullptr))
        << "a stub applied no DL processing, so tagging the image AI-processed "
           "would be a false claim about the image the caller holds";
}

TEST_F(AiProcessedAlert, DlAppliedRaisesTheAlert_ProvableWhenTheCallerSaysSo) {
    if (!CallerExpectsOnnx()) {
        GTEST_SKIP() << "XPE_AI_EXPECT_ONNX is not 1: the caller has not "
                        "declared which build this is, so a skip above is not "
                        "evidence either way.";
    }
    ASSERT_FALSE(IsStub())
        << "XPE_AI_EXPECT_ONNX=1 says a full build was expected, but this is a "
           "stub build -- the positive SRS-ALERT-004 assertion above was "
           "skipped, not passed.";
}
