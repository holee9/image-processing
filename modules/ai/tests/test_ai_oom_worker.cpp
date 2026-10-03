/**
 * @file test_ai_oom_worker.cpp
 * @brief A session that runs out of memory INSIDE THE WORKER, through a real worker (QA-B-194b, #130).
 *
 * The worker answers a shortage of memory with XPE_ERR_OUT_OF_MEMORY (-2) -- it used to answer a bone request with
 * PROCESSING_FAILED (-3) and a body-part request with "the model is unavailable". The worker is told to run out of
 * memory at session creation by XPE_AI_TEST_FAIL_SESSION_CREATE=oom, which a worker built with XPE_AI_TEST_HOOKS honors
 * (test builds only) and which it inherits from this process.
 *
 * WHAT THE HOST DOES WITH THAT ANSWER (observed here, not assumed): the shortage is NOT "the model is unavailable", so
 * it is counted toward switching the worker off exactly like any other failure of the worker path -- the same rule the
 * bone-suppression path has always had; only a signature refusal is exempt (QA-B-195 D6).
 */

#include <gtest/gtest.h>

#include "xpe/ai/ai_api.h"
#include "xpe/ai/ai_onnx_session.h"
#include "xpe/common/xpe_error.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#ifndef XPE_AI_TEST_DATA_DIR
#error "XPE_AI_TEST_DATA_DIR must be defined by the build (modules/ai/CMakeLists.txt)"
#endif

namespace {

const std::string kData = XPE_AI_TEST_DATA_DIR;
const char* const kVar = "XPE_AI_TEST_FAIL_SESSION_CREATE";

struct EnvScope {
    explicit EnvScope(const char* value) { SetEnvironmentVariableA(kVar, value); }
    ~EnvScope() { SetEnvironmentVariableA(kVar, nullptr); }
    EnvScope(const EnvScope&) = delete;
    EnvScope& operator=(const EnvScope&) = delete;
};

std::vector<std::string> Alerts() {
    std::vector<std::string> out;
    const int32_t n = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < n; ++i) {
        char msg[512] = {0};
        int32_t sev = -1;
        if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) == XPE_OK) out.push_back(msg);
    }
    return out;
}

XpeErrorCode Bone(std::vector<float>* out) {
    std::vector<float> in(9, 1.0f);
    out->assign(9, -777.0f);
    XpeImageBuffer ib{}, ob{};
    ib.width = ib.height = ob.width = ob.height = 3;
    ib.bitsAllocated = ib.bitsStored = ob.bitsAllocated = ob.bitsStored = 32;
    ib.format = ob.format = XPE_PIXEL_FLOAT32;
    ib.data = in.data();
    ob.data = out->data();
    ib.dataSize = ob.dataSize = in.size() * sizeof(float);
    return xpe_bone_suppress(&ib, &ob, nullptr);
}

XpeErrorCode Part(std::string* label) {
    std::vector<float> px(16, 0.0f);
    XpeImageBuffer ib{};
    ib.width = ib.height = 4;
    ib.bitsAllocated = ib.bitsStored = 32;
    ib.format = XPE_PIXEL_FLOAT32;
    ib.data = px.data();
    ib.dataSize = px.size() * sizeof(float);
    char buf[64];
    std::memset(buf, 'x', sizeof(buf));
    float conf = -1.0f;
    const XpeErrorCode rc = xpe_bodypart_recognize(&ib, buf, sizeof(buf), &conf);
    *label = std::string(buf, strnlen(buf, sizeof(buf)));
    return rc;
}

}  // namespace

TEST(WorkerOom, ABoneSessionThatRunsOutOfMemoryInTheWorkerIsOutOfMemoryAndCountsAsAWorkerFailure) {
    if (xpe::ai::OnnxSession::IsStubBuild()) GTEST_SKIP() << "stub build: no worker builds a session";
    const std::string dir = kData + "/models_x2";
    xpe_ai_shutdown();
    xpe_clear_alerts();
    {
        const EnvScope oom("oom");
        ASSERT_EQ(XPE_OK, xpe_ai_init(dir.c_str(), "{\"use_worker\": true}"));
        for (int i = 0; i < 4; ++i) {
            std::vector<float> out;
            const XpeErrorCode rc = Bone(&out);
            int32_t state = -1;
            uint32_t failures = 0;
            ASSERT_EQ(XPE_OK, xpe_ai_worker_state(&state, &failures, nullptr));
            if (i < 3) {
                EXPECT_EQ(XPE_ERR_OUT_OF_MEMORY, rc) << "call " << i << ": the worker's own answer, -2, not -3";
                EXPECT_EQ(static_cast<uint32_t>(i + 1), failures) << "call " << i << ": counted";
            } else {
                EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, rc) << "the 4th call: the worker is off";
                EXPECT_EQ(XPE_AI_WORKER_DISABLED, state);
            }
        }
    }
    xpe_ai_shutdown();
    // the control: the same directory without the variable works through the worker
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_ai_init(dir.c_str(), "{\"use_worker\": true}"));
    std::vector<float> out;
    EXPECT_EQ(XPE_OK, Bone(&out));
    for (const float v : out) EXPECT_EQ(2.0f, v);
    xpe_ai_shutdown();
    xpe_clear_alerts();
}

TEST(WorkerOom, ABodyPartSessionThatRunsOutOfMemoryInTheWorkerIsAFailureNotAnUnavailableModel) {
    if (xpe::ai::OnnxSession::IsStubBuild()) GTEST_SKIP() << "stub build: xpe_bodypart_recognize answers before it looks for a model";
    const std::string dir = kData + "/models_bodypart_a";
    xpe_ai_shutdown();
    xpe_clear_alerts();
    {
        const EnvScope oom("oom");
        ASSERT_EQ(XPE_OK, xpe_ai_init(dir.c_str(), "{\"use_worker\": true}"));
        std::string label;
        EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, Part(&label)) << "the documented fallback signal of the worker path";
        EXPECT_EQ("UNKNOWN", label);
        int32_t state = -1;
        uint32_t failures = 0;
        ASSERT_EQ(XPE_OK, xpe_ai_worker_state(&state, &failures, nullptr));
        EXPECT_EQ(1u, failures) << "a shortage of memory is a failure of the worker path, not 'the model is unavailable'";
        const std::vector<std::string> a = Alerts();
        ASSERT_EQ(1u, a.size());
        EXPECT_NE(std::string::npos, a[0].find("code -2")) << a[0];
        EXPECT_EQ(std::string::npos, a[0].find("unavailable")) << a[0];
    }
    xpe_ai_shutdown();
    xpe_clear_alerts();
}
