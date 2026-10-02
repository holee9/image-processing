/**
 * @file test_bodypart_worker_path.cpp
 * @brief xpe_bodypart_recognize through the worker process: the decision, the time budget (REQ-AI-092) and the
 *        failure count it shares with xpe_bone_suppress (QA-B-191 M4c, #130, T-006).
 *
 * THE MODELS ARE TOY MODELS (tests/data/make_bodypart_models.py): they prove the path, not any recognition quality.
 *
 * The scenarios are the design's table (QA-B-191/m4_design.md section 4.2) turned into code, under the leader's
 * decision D7 "S+": the worker's failure count is ONE count shared by both functions; "the model cannot be used" is
 * not a worker failure and resets the count; a model that exists and fails to run is a failure and counts.
 *
 * Alert texts are written out LITERALLY: a test that built them with the module's formatter would agree with it
 * even when both were wrong.
 */

#include <gtest/gtest.h>

#include "xpe/ai/ai_api.h"
#include "xpe/ai/ai_onnx_session.h"
#include "xpe/common/xpe_error.h"

#include <windows.h>
#include <tlhelp32.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

#ifndef XPE_AI_TEST_DATA_DIR
#error "XPE_AI_TEST_DATA_DIR must be defined by the build (modules/ai/CMakeLists.txt)"
#endif

namespace {

const std::string kData = XPE_AI_TEST_DATA_DIR;
std::string Dir(const char* name) { return kData + "/" + name; }
bool IsStub() { return xpe::ai::OnnxSession::IsStubBuild(); }

#define REQUIRE_ONNX() \
    do { if (IsStub()) GTEST_SKIP() << "needs a worker that can serve a model: the full build only"; } while (0)

/** A float image whose XpeImageBuffer is built on demand (a stored one would dangle after a copy). */
struct Pix {
    uint32_t w, h;
    std::vector<float> v;
    XpePixelFormat format = XPE_PIXEL_FLOAT32;
    Pix(uint32_t width, uint32_t height, float base) : w(width), h(height), v(static_cast<size_t>(width) * height) {
        for (size_t i = 0; i < v.size(); ++i) v[i] = base + static_cast<float>(i);
    }
    XpeImageBuffer Buffer() const {
        XpeImageBuffer b{};
        b.width = w;
        b.height = h;
        b.bitsAllocated = format == XPE_PIXEL_FLOAT32 ? 32 : 16;
        b.bitsStored = b.bitsAllocated;
        b.format = format;
        b.data = const_cast<float*>(v.data());
        b.dataSize = v.size() * sizeof(float);
        return b;
    }
};

struct Result {
    XpeErrorCode rc;
    std::string label;
    float confidence;
};

Result Recognize(const Pix& image, size_t bufLen = 64) {
    const XpeImageBuffer b = image.Buffer();
    std::vector<char> buf(bufLen, 'x');
    float conf = -1.0f;
    Result r;
    r.rc = xpe_bodypart_recognize(&b, buf.data(), bufLen, &conf);
    r.label = std::string(buf.data(), strnlen(buf.data(), bufLen));
    r.confidence = conf;
    return r;
}

/** The bone suppression call on the standard 3x3 input; returns its code and the output pixels. */
struct BoneOut {
    XpeErrorCode rc;
    std::vector<float> px;
};
BoneOut Bone() {
    Pix in(3, 3, 1.0f), out(3, 3, 0.0f);
    std::fill(out.v.begin(), out.v.end(), -12345.0f);
    const XpeImageBuffer ib = in.Buffer();
    XpeImageBuffer ob = out.Buffer();
    BoneOut r;
    r.rc = xpe_bone_suppress(&ib, &ob, nullptr);
    r.px = out.v;
    return r;
}

std::vector<std::string> Alerts() {
    std::vector<std::string> v;
    const int32_t n = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < n; ++i) {
        char msg[512] = {0};
        int32_t sev = -1;
        if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) == XPE_OK) v.push_back(msg);
    }
    return v;
}

int CountAlerts(const char* needle) {
    int hits = 0;
    for (const std::string& a : Alerts()) {
        if (a.find(needle) != std::string::npos) ++hits;
    }
    return hits;
}

struct WState {
    XpeErrorCode rc = XPE_OK;
    int32_t state = -777;
    uint32_t failures = 777777u;
    uint32_t ceiling = 777777u;
};
WState State() {
    WState w;
    w.rc = xpe_ai_worker_state(&w.state, &w.failures, &w.ceiling);
    return w;
}

std::vector<DWORD> ChildWorkers() {
    std::vector<DWORD> pids;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return pids;
    PROCESSENTRY32 pe{};
    pe.dwSize = sizeof(pe);
    for (BOOL ok = Process32First(snap, &pe); ok; ok = Process32Next(snap, &pe)) {
        if (pe.th32ParentProcessID == GetCurrentProcessId() && _stricmp(pe.szExeFile, "xpe_ai_worker.exe") == 0) {
            pids.push_back(pe.th32ProcessID);
        }
    }
    CloseHandle(snap);
    return pids;
}

bool WaitForChildWorkers(size_t n, DWORD ms) {
    for (DWORD waited = 0; waited <= ms; waited += 50) {
        if (ChildWorkers().size() == n) return true;
        Sleep(50);
    }
    return ChildWorkers().size() == n;
}

/** Suspend every thread of @p pid: alive, silent. */
void Freeze(DWORD pid) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 te{};
    te.dwSize = sizeof(te);
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
        if (te.th32OwnerProcessID != pid) continue;
        HANDLE t = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID);
        if (t) {
            SuspendThread(t);
            CloseHandle(t);
        }
    }
    CloseHandle(snap);
}

struct BodyPartWorkerPath : public ::testing::Test {
    void SetUp() override {
        xpe_ai_shutdown();
        xpe_clear_alerts();
    }
    void TearDown() override {
        xpe_ai_shutdown();
        xpe_clear_alerts();
        EXPECT_TRUE(WaitForChildWorkers(0, 3000)) << "a worker outlived xpe_ai_shutdown()";
    }
    static void Init(const std::string& dir, const char* config = "{\"use_worker\": true}") {
        xpe_ai_shutdown();
        ASSERT_EQ(XPE_OK, xpe_ai_init(dir.c_str(), config)) << dir;
        xpe_clear_alerts();
    }
};

const char* kWorkerOn = "{\"use_worker\": true}";
// The float one step above 0.6f, as JSON that reads back as exactly that float (the model's answer is exactly 0.6f).
const char* kJustAbove06 = "{\"use_worker\": true, \"confidence_threshold\": 0.6000000834465027}";
const char* kJustAbove06FallbackOff =
    "{\"use_worker\": true, \"confidence_threshold\": 0.6000000834465027, \"fallback_mode\": false}";

const char* kLowFallbackOn =
    "AI body-part confidence 0.6 is below the threshold 0.6000001 (REQ-AI-012): UNKNOWN is returned; "
    "use the deterministic body-part lookup";
const char* kLowFallbackOff =
    "AI body-part confidence 0.6 is below the threshold 0.6000001 (REQ-AI-012): the label CHEST is returned "
    "because fallback_mode is off; an exposure parameter chosen from it may be wrong";
const char* kNonFinite = "AI model output was non-finite (inf/NaN); this image was not AI-processed";
const char* kNotProbability =
    "AI body-part model output is not a probability vector (a value outside [0, 1]); this image was not AI-classified";
const char* kUnavailable =
    "AI body-part recognition is unavailable (the AI worker has no usable body-part model): UNKNOWN is returned; "
    "use the deterministic body-part lookup (REQ-AI-002)";

std::string Failed(int failure) {
    return "AI worker failed (code -3, failure " + std::to_string(failure) +
           " of 3): body-part recognition returns UNKNOWN; use the deterministic body-part lookup "
           "(REQ-AI-002, REQ-AI-092)";
}
const char* kDisabledAtThree =
    "AI worker failed (code -3, failure 3 of 3) and is disabled for this session: body-part recognition returns "
    "UNKNOWN (REQ-AI-002, REQ-AI-092)";

}  // namespace

// ===== the answer and the decision ========================================================================

TEST_F(BodyPartWorkerPath, ControlTheModelRunsInTheWorkerAndTheAnswerComesBack) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_a"));
    const Result r = Recognize(Pix(4, 4, 0.0f));
    EXPECT_EQ(XPE_OK, r.rc);
    EXPECT_EQ("CHEST", r.label);
    EXPECT_EQ(0.6f, r.confidence);
    EXPECT_EQ(1u, ChildWorkers().size()) << "a worker process, not this one, ran the model";
    const WState s = State();
    EXPECT_EQ(XPE_OK, s.rc);
    EXPECT_EQ(XPE_AI_WORKER_ACTIVE, s.state);
    EXPECT_EQ(0u, s.failures);
    EXPECT_TRUE(Alerts().empty());
}

TEST_F(BodyPartWorkerPath, TheDifferentModelDirectoryChangesTheLabelThroughTheWorkerToo) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_b"));
    const Result r = Recognize(Pix(4, 4, 0.0f));
    EXPECT_EQ(XPE_OK, r.rc);
    EXPECT_EQ("SPINE", r.label);
}

TEST_F(BodyPartWorkerPath, ALowConfidenceFromTheWorkerGetsTheDecisionOfThisProcess) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_a"), kJustAbove06);
    const Result r = Recognize(Pix(4, 4, 0.0f));
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, r.rc);
    EXPECT_EQ("UNKNOWN", r.label);
    EXPECT_EQ(0.6f, r.confidence) << "the MEASURED confidence, as on the in-process path";
    const auto a = Alerts();
    ASSERT_EQ(1u, a.size());
    EXPECT_EQ(kLowFallbackOn, a[0]);
    EXPECT_EQ(0u, State().failures) << "a low confidence is an answer, not a worker failure";
}

TEST_F(BodyPartWorkerPath, WithFallbackModeOffTheLowConfidenceLabelIsReturnedAndTheToggleWorksAtRunTime) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_a"), kJustAbove06FallbackOff);
    const Result off = Recognize(Pix(4, 4, 0.0f));
    EXPECT_EQ(XPE_OK, off.rc);
    EXPECT_EQ("CHEST", off.label);
    const auto a = Alerts();
    ASSERT_EQ(1u, a.size());
    EXPECT_EQ(kLowFallbackOff, a[0]);

    ASSERT_EQ(XPE_OK, xpe_ai_set_fallback_mode(1));   // the state the worker cannot know: switched here, at run time
    xpe_clear_alerts();
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, Recognize(Pix(4, 4, 0.0f)).rc);
    ASSERT_EQ(1u, Alerts().size());
    EXPECT_EQ(kLowFallbackOn, Alerts()[0]);
}

TEST_F(BodyPartWorkerPath, TheModelsRefusedOutputIsAnAlertAndNeverAWorkerFailure) {
    REQUIRE_ONNX();
    struct Case { const char* dir; const char* alert; };
    for (const Case c : {Case{"models_bodypart_nonfinite", kNonFinite}, Case{"models_bodypart_range_high", kNotProbability},
                         Case{"models_bodypart_range_low", kNotProbability}}) {
        Init(Dir(c.dir));
        for (int i = 0; i < 4; ++i) {   // more than the ceiling: none of them may count
            const Result r = Recognize(Pix(4, 4, 0.0f));
            EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, r.rc) << c.dir;
            EXPECT_EQ("UNKNOWN", r.label) << c.dir;
            EXPECT_EQ(0.0f, r.confidence) << c.dir;
        }
        EXPECT_EQ(4, CountAlerts(c.alert)) << c.dir;
        for (const std::string& a : Alerts()) EXPECT_EQ(c.alert, a) << c.dir << ": only the refusal alert";
        const WState s = State();
        EXPECT_EQ(XPE_AI_WORKER_ACTIVE, s.state) << c.dir;
        EXPECT_EQ(0u, s.failures) << c.dir;
        EXPECT_EQ(1u, ChildWorkers().size()) << c.dir << ": the worker is healthy and kept";
    }
}

// ===== D7 S+: "the model cannot be used" is not a worker failure ==========================================

TEST_F(BodyPartWorkerPath, AnUnavailableBodyPartModelIsOneWarningAndNeverCountedAndNeverSwitchesTheWorkerOff) {
    REQUIRE_ONNX();
    Init(Dir("models_missing"));
    for (int i = 0; i < 5; ++i) {   // well past the ceiling of 3
        const Result r = Recognize(Pix(4, 4, 0.0f));
        EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, r.rc) << i;
        EXPECT_EQ("UNKNOWN", r.label) << i;
        EXPECT_EQ(0.0f, r.confidence) << i;
    }
    const auto a = Alerts();
    ASSERT_EQ(1u, a.size()) << "ONE Warning per session, the same as the in-process path";
    EXPECT_EQ(kUnavailable, a[0]);
    const WState s = State();
    EXPECT_EQ(XPE_AI_WORKER_ACTIVE, s.state);
    EXPECT_EQ(0u, s.failures);
    EXPECT_EQ(1u, ChildWorkers().size()) << "the worker answered every time: it is healthy and kept";
}

TEST_F(BodyPartWorkerPath, WithNoBodyPartModelBoneSuppressionIsNotSkippedByTheBodyPartCalls) {
    // The worst combination of a plain shared count (design 4.2 scenario 3): the body-part model is missing, three
    // body-part calls would switch the shared worker off, and bone suppression -- whose model is fine -- would be
    // skipped. models_x2 holds a bone model and no body-part model.
    REQUIRE_ONNX();
    Init(Dir("models_x2"));
    for (int i = 0; i < 4; ++i) EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, Recognize(Pix(4, 4, 0.0f)).rc) << i;
    EXPECT_EQ(1, CountAlerts("unavailable"));
    const BoneOut b = Bone();
    EXPECT_EQ(XPE_OK, b.rc) << "bone suppression must still be served by the worker";
    ASSERT_EQ(9u, b.px.size());
    for (size_t i = 0; i < 9; ++i) EXPECT_FLOAT_EQ((1.0f + static_cast<float>(i)) * 2.0f, b.px[i]);
    const WState s = State();
    EXPECT_EQ(XPE_AI_WORKER_ACTIVE, s.state);
    EXPECT_EQ(0u, s.failures);
}

TEST_F(BodyPartWorkerPath, AnUnavailableAnswerEndsARunOfBoneSuppressionFailures) {
    // A healthy exchange ends a run of CONSECUTIVE faults (REQ-CHANGE-LOG-P3-AI rows 5 and 6). models_missing: the
    // bone model is missing too, so bone suppression gets an error frame each time and counts.
    REQUIRE_ONNX();
    Init(Dir("models_missing"));
    (void)Bone();
    (void)Bone();
    EXPECT_EQ(2u, State().failures) << "the control: two bone failures are counted";
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, Recognize(Pix(4, 4, 0.0f)).rc);
    EXPECT_EQ(0u, State().failures) << "the unavailable body-part answer reset the run";
    xpe_clear_alerts();
    (void)Bone();
    EXPECT_EQ(1, CountAlerts("failure 1 of 3")) << "the run starts again from one, not from three";
    EXPECT_EQ(XPE_AI_WORKER_ACTIVE, State().state);
}

// ===== D7 S+: a model that exists and fails is a worker failure, and the count is shared =================

TEST_F(BodyPartWorkerPath, ControlTheFailingModelLoadsAndOnlyFailsWhenItIsRun) {
    // Without this the next test could be red or green for a reason unrelated to "a model that exists and fails":
    // the same files with the in-process path give UNKNOWN and no "unavailable" Warning (a load-time refusal would
    // have raised one), so the model loaded; and the worker path below sees it fail on every call.
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_runfail"), "{}");   // in-process
    const Result r = Recognize(Pix(4, 4, 0.0f));
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, r.rc);
    EXPECT_EQ("UNKNOWN", r.label);
    EXPECT_EQ(0, CountAlerts("unavailable")) << "the model loaded: it was not refused at load time";
}

TEST_F(BodyPartWorkerPath, AModelThatExistsAndFailsToRunIsCountedAndThirdFailureSwitchesTheWorkerOffForBothFunctions) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_runfail"));
    for (int k = 1; k <= 3; ++k) {
        const Result r = Recognize(Pix(4, 4, 0.0f));
        EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, r.rc) << k;
        EXPECT_EQ("UNKNOWN", r.label) << k;
        const auto a = Alerts();
        ASSERT_EQ(static_cast<size_t>(k), a.size()) << "one Warning per failure";
        EXPECT_EQ(k < 3 ? Failed(k) : std::string(kDisabledAtThree), a.back()) << k;
        EXPECT_EQ(static_cast<uint32_t>(k), State().failures) << k;
    }
    const WState s = State();
    EXPECT_EQ(XPE_AI_WORKER_DISABLED, s.state);
    EXPECT_EQ(3u, s.failures);
    EXPECT_EQ(3u, s.ceiling);
    EXPECT_TRUE(WaitForChildWorkers(0, 3000)) << "the switched-off worker process was ended";

    // Switched off: silent, no worker is started, for body-part recognition ...
    xpe_clear_alerts();
    const Result after = Recognize(Pix(4, 4, 0.0f));
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, after.rc);
    EXPECT_EQ("UNKNOWN", after.label);
    EXPECT_TRUE(Alerts().empty()) << "no new alert once the worker is off";
    EXPECT_EQ(0u, ChildWorkers().size());
    // ... and ONE shared worker means bone suppression is off too: its input is returned unchanged, silently.
    const BoneOut b = Bone();
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, b.rc);
    ASSERT_EQ(9u, b.px.size());
    for (size_t i = 0; i < 9; ++i) EXPECT_FLOAT_EQ(1.0f + static_cast<float>(i), b.px[i]) << "the input, unchanged";
    EXPECT_EQ(0u, ChildWorkers().size());
    EXPECT_TRUE(Alerts().empty());
}

TEST_F(BodyPartWorkerPath, BoneSuppressionFailuresSwitchOffBodyPartRecognitionToo) {
    // The other direction of the shared count. models_missing: the bone model is missing, so each bone call is an
    // error frame and counts (REQ-CHANGE-LOG-P3-AI row 4: this rule is unchanged).
    REQUIRE_ONNX();
    Init(Dir("models_missing"));
    for (int i = 0; i < 3; ++i) (void)Bone();
    EXPECT_EQ(XPE_AI_WORKER_DISABLED, State().state);
    EXPECT_TRUE(WaitForChildWorkers(0, 3000));
    xpe_clear_alerts();
    const Result r = Recognize(Pix(4, 4, 0.0f));
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, r.rc);
    EXPECT_EQ("UNKNOWN", r.label);
    EXPECT_TRUE(Alerts().empty()) << "switched off: silent";
    EXPECT_EQ(0u, ChildWorkers().size()) << "and no worker is started for it";
}

TEST_F(BodyPartWorkerPath, ShutdownThenInitRecoversAWorkerSwitchedOffByBodyPartFailures) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_runfail"));
    for (int i = 0; i < 3; ++i) (void)Recognize(Pix(4, 4, 0.0f));
    ASSERT_EQ(XPE_AI_WORKER_DISABLED, State().state);
    Init(Dir("models_bodypart_a"));
    const WState s = State();
    EXPECT_EQ(XPE_AI_WORKER_ACTIVE, s.state);
    EXPECT_EQ(0u, s.failures);
    EXPECT_EQ(XPE_OK, Recognize(Pix(4, 4, 0.0f)).rc);
}

// ===== the time budget (REQ-AI-092) =======================================================================

TEST_F(BodyPartWorkerPath, ASilentWorkerIsGivenUpOnAtTheBudgetAndTheNextCallRecoversOnANewWorker) {
    REQUIRE_ONNX();
    Init(Dir("models_bodypart_a"), "{\"use_worker\": true, \"timeout_ms\": 2000}");
    ASSERT_EQ(XPE_OK, Recognize(Pix(4, 4, 0.0f)).rc);   // healthy: starts the worker
    const auto first = ChildWorkers();
    ASSERT_EQ(1u, first.size());
    EXPECT_TRUE(Alerts().empty());

    Freeze(first[0]);   // alive, silent
    const DWORD t0 = GetTickCount();
    const Result r = Recognize(Pix(4, 4, 0.0f));
    const DWORD took = GetTickCount() - t0;
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, r.rc);
    EXPECT_EQ("UNKNOWN", r.label);
    EXPECT_EQ(0.0f, r.confidence);
    const auto a = Alerts();
    ASSERT_EQ(1u, a.size()) << "ONE alert for the failure";
    EXPECT_EQ(Failed(1), a[0]);
    EXPECT_GE(took, 1800u) << "returned before the budget: it did not wait for the worker";
    EXPECT_LE(took, 2000u + 3000u) << "took " << took << " ms";
    bool frozenGone = true;
    for (DWORD pid : ChildWorkers()) if (pid == first[0]) frozenGone = false;
    EXPECT_TRUE(frozenGone) << "the silent worker was left running";
    EXPECT_EQ(1u, State().failures);
    xpe_clear_alerts();

    const Result again = Recognize(Pix(4, 4, 0.0f));   // the NEXT call recovers on a new worker
    EXPECT_EQ(XPE_OK, again.rc);
    EXPECT_EQ("CHEST", again.label);
    const auto after = ChildWorkers();
    ASSERT_EQ(1u, after.size());
    EXPECT_NE(first[0], after[0]) << "the same worker answered: it was not replaced";
    EXPECT_TRUE(Alerts().empty());
    EXPECT_EQ(0u, State().failures) << "a success resets the count";
}

// ===== the one deliberate difference from the in-process path ============================================

TEST_F(BodyPartWorkerPath, ANonFloatImageIsRefusedBeforeTheWorkerIsAskedWhateverTheModel) {
    // In-process, a missing model gives UNKNOWN for every image (the stub's outcome, whatever the format). With
    // the worker this process does not know whether the worker has a model, so the format is judged first.
    // Both halves are asserted, so the difference is a decision on record and not an accident.
    REQUIRE_ONNX();
    Pix image(4, 4, 0.0f);
    image.format = XPE_PIXEL_UINT16;
    Init(Dir("models_missing"));
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, Recognize(image).rc);
    EXPECT_EQ(0u, ChildWorkers().size()) << "the worker was not even started";
    EXPECT_TRUE(Alerts().empty());
    Init(Dir("models_missing"), "{}");   // in-process, the control
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, Recognize(image).rc) << "the in-process path answers UNKNOWN here";
}
