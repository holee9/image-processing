/**
 * @file test_ai_worker_boundary.cpp
 * @brief A shortage of memory (or any exception) inside the worker is an ANSWER, never a dead worker (QA-B-195b, #130).
 *
 * Codex #88 found it: loading a model reads the whole file into one buffer, and that allocation, with the signature and
 * sidecar reads, sat BEFORE the catch that maps a shortage of memory to a code; the in-process path was saved by the
 * C ABI's outer catch, but the worker has no such catch, so it died of an unhandled bad_alloc and the host saw a broken
 * pipe instead of XPE_ERR_OUT_OF_MEMORY. Two layers now hold, and each is tested ALONE:
 *   1. OnnxSession::Create turns a bad_alloc anywhere in loading into kOutOfMemory (an exception never leaves it);
 *   2. the worker's request boundary turns a bad_alloc or any other exception into an ERROR frame, and the worker goes on.
 * The worker is made to fail by environment variables it reads in a test build only (XPE_AI_TEST_HOOKS):
 *   XPE_AI_TEST_FAIL_MODEL_READ=<n>   the first n file reads of loading fail like a shortage of memory
 *   XPE_AI_TEST_FAIL_WORKER_REQUEST=oom|std|payload   ONE request fails at the boundary
 * What "survives" means is observed, not assumed: the supervisor's worker PID and start count are the same after the
 * failed request, and the NEXT request to the same worker succeeds.
 */

#include <gtest/gtest.h>

#include "ai_worker_supervisor.h"
#include "ai_bodypart.h"
#include "xpe/ai/ai_api.h"
#include "xpe/ai/ai_onnx_session.h"
#include "xpe/common/xpe_error.h"

#include <windows.h>

#include <cstdint>
#include <cstring>
#include <new>
#include <string>
#include <vector>

#ifndef XPE_AI_TEST_DATA_DIR
#error "XPE_AI_TEST_DATA_DIR must be defined by the build (modules/ai/CMakeLists.txt)"
#endif
#ifndef XPE_AI_WORKER_EXE
#error "XPE_AI_WORKER_EXE must be defined by the build (modules/ai/CMakeLists.txt)"
#endif

#ifdef XPE_AI_TEST_HOOKS
namespace xpe::ai {
void TestSetBeforeFileReadHook(void (*hook)());   // ai_onnx_session.cpp, test builds only
}
#endif

namespace {

using xpe::ai::BodyPartReply;
using xpe::ai::OnnxErrorCode;
using xpe::ai::OnnxSession;
using xpe::ai::OnnxSessionConfig;
using xpe::ai::WorkerSupervisor;
using xpe::ai::WorkerSupervisorConfig;

const std::string kData = XPE_AI_TEST_DATA_DIR;
constexpr uint32_t kBudgetMs = 20000;

struct EnvScope {
    const char* name;
    EnvScope(const char* n, const char* value) : name(n) { SetEnvironmentVariableA(name, value); }
    ~EnvScope() { SetEnvironmentVariableA(name, nullptr); }
    EnvScope(const EnvScope&) = delete;
    EnvScope& operator=(const EnvScope&) = delete;
};

WorkerSupervisorConfig Cfg(const char* dir) {
    WorkerSupervisorConfig c;
    c.worker_exe = XPE_AI_WORKER_EXE;
    c.model_dir = kData + "/" + dir;
    c.timeout_ms = kBudgetMs;
    return c;
}

struct Bone {
    XpeErrorCode rc;
    std::vector<float> out;
};
Bone AskBone(WorkerSupervisor& sup) {
    const std::vector<float> in(9, 1.0f);
    Bone b;
    b.out.assign(9, -777.0f);
    b.rc = sup.BoneSuppress(3, 3, in.data(), b.out.data());
    return b;
}

struct Part {
    XpeErrorCode rc;
    BodyPartReply reply;
};
Part AskPart(WorkerSupervisor& sup) {
    const std::vector<float> px(16, 0.0f);
    Part p{};
    p.rc = sup.BodyPartRecognize(4, 4, px.data(), &p.reply);
    return p;
}

bool IsStub() { return OnnxSession::IsStubBuild(); }

/** The worker was told to fail one request one way; it answered with @p expected and is the SAME worker afterwards. */
void ExpectBoneSurvives(const char* var, const char* value, XpeErrorCode expected) {
    const EnvScope env(var, value);
    WorkerSupervisor sup(Cfg("models_x2"));
    const Bone failed = AskBone(sup);
    EXPECT_EQ(expected, failed.rc) << var << "=" << value << ": the worker's own answer";
    for (const float v : failed.out) EXPECT_EQ(-777.0f, v) << "a failed call leaves the output untouched";
    const uint32_t pid = sup.WorkerPid();
    EXPECT_NE(0u, pid) << "the worker is still there";
    const Bone next = AskBone(sup);
    EXPECT_EQ(XPE_OK, next.rc) << "the next request to the same worker succeeds";
    for (const float v : next.out) EXPECT_EQ(2.0f, v);
    EXPECT_EQ(pid, sup.WorkerPid()) << "the same process";
    EXPECT_EQ(1u, sup.StartCount()) << "the worker was started once and never restarted";
}

void ExpectPartSurvives(const char* var, const char* value, XpeErrorCode expected) {
    const EnvScope env(var, value);
    WorkerSupervisor sup(Cfg("models_bodypart_a"));
    const Part failed = AskPart(sup);
    EXPECT_EQ(expected, failed.rc) << var << "=" << value << ": the worker's own answer";
    EXPECT_FALSE(sup.LastModelUnavailable()) << "a shortage of memory is not 'the model is unavailable'";
    const uint32_t pid = sup.WorkerPid();
    EXPECT_NE(0u, pid) << "the worker is still there";
    const Part next = AskPart(sup);
    EXPECT_EQ(XPE_OK, next.rc) << "the next request to the same worker succeeds";
    EXPECT_STREQ("CHEST", next.reply.label);
    EXPECT_EQ(pid, sup.WorkerPid()) << "the same process";
    EXPECT_EQ(1u, sup.StartCount());
}

}  // namespace

/* =========================================================================
 * Layer 1: OnnxSession::Create never lets an exception out
 * ========================================================================= */

#ifdef XPE_AI_TEST_HOOKS
namespace {
int g_readsUntilFailure = 0;   // the k-th read fails (1 = the model, 2 = the sidecar, 3 = the signature)
void FailTheKthRead() {
    if (--g_readsUntilFailure == 0) throw std::bad_alloc();
}
struct ReadFailureScope {
    explicit ReadFailureScope(int k) {
        g_readsUntilFailure = k;
        xpe::ai::TestSetBeforeFileReadHook(&FailTheKthRead);
    }
    ~ReadFailureScope() { xpe::ai::TestSetBeforeFileReadHook(nullptr); }
    ReadFailureScope(const ReadFailureScope&) = delete;
    ReadFailureScope& operator=(const ReadFailureScope&) = delete;
};
}  // namespace

TEST(ModelReadOom, ControlTheModelLoadsWhenNoReadFails) {
    OnnxSessionConfig c;
    c.model_path = kData + "/models_x2/bone_suppress.onnx";
    EXPECT_EQ(OnnxErrorCode::kOk, OnnxSession::Create(c).code);
}

TEST(ModelReadOom, ABadAllocAtAnyOfTheThreeFileReadsIsOutOfMemoryAndNeverAnException) {
    // The model, its sidecar and its signature are each read into a buffer the size of the file; the sweep fails each in turn.
    for (int k = 1; k <= 3; ++k) {
        OnnxSessionConfig c;
        c.model_path = kData + "/models_x2/bone_suppress.onnx";
        bool escaped = false;
        OnnxErrorCode code = OnnxErrorCode::kOk;
        {
            const ReadFailureScope fail(k);
            try {
                code = OnnxSession::Create(c).code;
            } catch (...) {
                escaped = true;
            }
        }
        EXPECT_FALSE(escaped) << "read " << k << ": an exception left OnnxSession::Create";
        EXPECT_EQ(OnnxErrorCode::kOutOfMemory, code) << "read " << k;
        EXPECT_EQ(OnnxErrorCode::kOk, OnnxSession::Create(c).code) << "read " << k << ": usable afterwards";
    }
}
#endif  // XPE_AI_TEST_HOOKS

/* =========================================================================
 * Layer 2: the worker's request boundary, through a REAL worker
 * ========================================================================= */

TEST(WorkerBoundary, ControlWithNoFaultTheWorkerAnswersBothRequestTypes) {
    if (IsStub()) GTEST_SKIP() << "stub build: no worker runs a model";
    WorkerSupervisor sup(Cfg("models_x2"));
    EXPECT_EQ(XPE_OK, AskBone(sup).rc);
    WorkerSupervisor part(Cfg("models_bodypart_a"));
    EXPECT_EQ(XPE_OK, AskPart(part).rc);
}

TEST(WorkerBoundary, AShortageOfMemoryWhileReadingTheModelIsAnErrorFrameAndTheWorkerSurvives) {
    if (IsStub()) GTEST_SKIP() << "stub build: no worker runs a model";
    ExpectBoneSurvives("XPE_AI_TEST_FAIL_MODEL_READ", "1", XPE_ERR_OUT_OF_MEMORY);
    ExpectPartSurvives("XPE_AI_TEST_FAIL_MODEL_READ", "1", XPE_ERR_OUT_OF_MEMORY);
}

TEST(WorkerBoundary, ABadAllocInsideARequestHandlerIsAnErrorFrameAndTheWorkerSurvives) {
    if (IsStub()) GTEST_SKIP() << "stub build: no worker runs a model";
    ExpectBoneSurvives("XPE_AI_TEST_FAIL_WORKER_REQUEST", "oom", XPE_ERR_OUT_OF_MEMORY);
    ExpectPartSurvives("XPE_AI_TEST_FAIL_WORKER_REQUEST", "oom", XPE_ERR_OUT_OF_MEMORY);
}

TEST(WorkerBoundary, AnyOtherExceptionInsideARequestHandlerIsAProcessingFailureAndTheWorkerSurvives) {
    if (IsStub()) GTEST_SKIP() << "stub build: no worker runs a model";
    ExpectBoneSurvives("XPE_AI_TEST_FAIL_WORKER_REQUEST", "std", XPE_ERR_PROCESSING_FAILED);
    ExpectPartSurvives("XPE_AI_TEST_FAIL_WORKER_REQUEST", "std", XPE_ERR_PROCESSING_FAILED);
}

TEST(WorkerBoundary, AShortageOfMemoryAtThePayloadAllocationIsAnsweredAndTheNextFrameIsStillInStep) {
    // The payload's bytes are in the pipe when its buffer cannot be allocated: they must be read and thrown away, or the
    // next request would be decoded from the middle of this one. The next request succeeding is the proof.
    if (IsStub()) GTEST_SKIP() << "stub build: no worker runs a model";
    ExpectBoneSurvives("XPE_AI_TEST_FAIL_WORKER_REQUEST", "payload", XPE_ERR_OUT_OF_MEMORY);
    ExpectPartSurvives("XPE_AI_TEST_FAIL_WORKER_REQUEST", "payload", XPE_ERR_OUT_OF_MEMORY);
}

TEST(WorkerBoundary, ThroughTheCAbiTheShortageIsCountedAsAWorkerFailureAndTheNextSuccessResetsTheCount) {
    // 194b's rule, unchanged: a shortage of memory in the worker is a failure of the worker path (counted), not "the model
    // is unavailable". What changes is that it is now an ANSWER, so the count is 1 and the worker keeps serving.
    if (IsStub()) GTEST_SKIP() << "stub build: no worker runs a model";
    const std::string dir = kData + "/models_x2";
    xpe_ai_shutdown();
    xpe_clear_alerts();
    {
        const EnvScope env("XPE_AI_TEST_FAIL_MODEL_READ", "1");
        ASSERT_EQ(XPE_OK, xpe_ai_init(dir.c_str(), "{\"use_worker\": true}"));
        std::vector<float> in(9, 1.0f), out(9, -777.0f);
        XpeImageBuffer ib{}, ob{};
        ib.width = ib.height = ob.width = ob.height = 3;
        ib.bitsAllocated = ib.bitsStored = ob.bitsAllocated = ob.bitsStored = 32;
        ib.format = ob.format = XPE_PIXEL_FLOAT32;
        ib.data = in.data();
        ob.data = out.data();
        ib.dataSize = ob.dataSize = in.size() * sizeof(float);
        EXPECT_EQ(XPE_ERR_OUT_OF_MEMORY, xpe_bone_suppress(&ib, &ob, nullptr));
        int32_t state = -1;
        uint32_t failures = 0;
        ASSERT_EQ(XPE_OK, xpe_ai_worker_state(&state, &failures, nullptr));
        EXPECT_EQ(1u, failures) << "counted";
        EXPECT_EQ(XPE_AI_WORKER_ACTIVE, state);
        std::fill(out.begin(), out.end(), -777.0f);
        EXPECT_EQ(XPE_OK, xpe_bone_suppress(&ib, &ob, nullptr)) << "the same worker serves the next call";
        for (const float v : out) EXPECT_EQ(2.0f, v);
        ASSERT_EQ(XPE_OK, xpe_ai_worker_state(&state, &failures, nullptr));
        EXPECT_EQ(0u, failures) << "a success resets the count";
    }
    xpe_ai_shutdown();
    xpe_clear_alerts();
}
