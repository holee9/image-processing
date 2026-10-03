/**
 * @file test_bone_suppress_nonfinite.cpp
 * @brief QA-B-181h (Codex #60): xpe_bone_suppress must not report success with a non-finite result.
 *
 * The model models_x2/bone_suppress.onnx computes Y = X * 2. A FINITE pixel above FLT_MAX / 2 therefore
 * comes out as +infinity. Before this card the result went to the caller as XPE_OK, in-process and
 * through the worker. Policy (QA-B-181f): a module neither makes non-finite output from finite input nor
 * reports success on it. The refusal is XPE_ERR_PROCESSING_FAILED (the input was valid; QA-B-181i changed it
 * from INVALID_INPUT) with one alert, and on the worker path it is NOT a worker fault.
 *
 * Full ONNX build only (the stub has no model, so no result exists to judge).
 */
#include <gtest/gtest.h>

#include "xpe/ai/ai_api.h"
#include "xpe/ai/ai_onnx_session.h"

#include <windows.h>
#include <tlhelp32.h>

#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include "test_signing_helper.h"

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

constexpr const char* kNonFiniteNeedle = "non-finite";
// The WHOLE text is the cross-lane contract (QA-B-181j). It states only what was observed -- the model's output
// was non-finite and this image was not AI-processed -- and says nothing about the worker: the in-process path
// has none, and the worker's state is for xpe_ai_worker_state() to report.
constexpr const char* kNonFiniteAlertText =
    "AI model output was non-finite (inf/NaN); this image was not AI-processed";

/** True when exactly one pending alert has exactly @p text, and no pending alert mentions the worker's health. */
bool OnlyTheContractAlert(const char* text, int* count) {
    *count = 0;
    bool forbidden = false;
    for (int32_t i = 0; i < xpe_get_pending_alert_count(); ++i) {
        char buf[512] = {0};
        int32_t sev = -1;
        if (xpe_get_pending_alert(i, buf, sizeof(buf), &sev) != XPE_OK) continue;
        const std::string m(buf);
        if (m == text) ++*count;
        if (m.find("healthy") != std::string::npos) forbidden = true;
    }
    return !forbidden;
}
constexpr const char* kWorkerFailedNeedle = "AI worker failed";

int CountAlerts(const char* needle) {
    int n = 0;
    for (int32_t i = 0; i < xpe_get_pending_alert_count(); ++i) {
        char buf[512] = {0};
        int32_t sev = -1;
        if (xpe_get_pending_alert(i, buf, sizeof(buf), &sev) == XPE_OK && std::string(buf).find(needle) != std::string::npos) ++n;
    }
    return n;
}

struct WState { int32_t state = -1; uint32_t failures = 777u; uint32_t ceiling = 777u; };
WState QueryState() {
    WState w;
    EXPECT_EQ(XPE_OK, xpe_ai_worker_state(&w.state, &w.failures, &w.ceiling));
    return w;
}

// Leader decision (Codex #63, QA-B-181i): a model result that is not finite is a refusal of THAT image --
// XPE_ERR_PROCESSING_FAILED (the input was valid and finite; the model could not produce a result), one
// Warning alert naming the cause -- and it is NOT a worker fault: the failure count and the worker's state
// do not move, however many such images come in a row.
void RefusesExtremeInputs(const char* cfg) {
    ASSERT_EQ(XPE_OK, xpe_ai_init(kDirX2.c_str(), cfg));
    const bool worker = cfg != nullptr;
    xpe_clear_alerts();
    int round = 0;
    for (int rep = 0; rep < 2; ++rep) {   // two passes: 5 cases x 2 = ten images in a row, more than the worker ceiling of 3
        for (const Case& c : ExtremeInputs()) {
            ++round;
            Img in(c.in);
            Img out(-7.0f);                                // sentinel: "untouched" must be provable
            const std::vector<float> outBefore = out.px;
            xpe_clear_alerts();
            const XpeErrorCode rc = xpe_bone_suppress(&in.buf, &out.buf, nullptr);
            EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, rc) << c.name << " round " << round;
            EXPECT_FALSE(AnyNonFinite(out.px)) << c.name << ": +/-inf or NaN reached the caller's buffer";
            EXPECT_TRUE(SameBits(c.in, in.px)) << c.name << ": the input was modified";
            // EVERY image went through the check: one alert naming the cause, and none of the worker-fault kind.
            EXPECT_EQ(1, CountAlerts(kNonFiniteNeedle)) << c.name << " round " << round << ": the refusal did not alert";
            int exact = 0;
            EXPECT_TRUE(OnlyTheContractAlert(kNonFiniteAlertText, &exact)) << c.name << ": the alert asserts something about the worker";
            EXPECT_EQ(1, exact) << c.name << " round " << round << ": the alert text is not the contract text";
            EXPECT_EQ(0, CountAlerts(kWorkerFailedNeedle)) << c.name << " round " << round << ": counted as a worker fault";
            if (worker) {
                // The documented fallback of the worker path: the output holds the input.
                EXPECT_TRUE(SameBits(c.in, out.px)) << c.name << ": worker-path fallback must be the input";
                const WState w = QueryState();
                EXPECT_EQ(XPE_AI_WORKER_ACTIVE, w.state) << c.name << " round " << round << ": the worker was switched off";
                EXPECT_EQ(0u, w.failures) << c.name << " round " << round << ": counted toward the ceiling";
            } else {
                EXPECT_TRUE(SameBits(outBefore, out.px)) << c.name << ": output not left unchanged";
            }
        }
    }
    // The point of it: the SAME session still processes an ordinary image.
    Img in(Ordinary());
    Img out(-7.0f);
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_bone_suppress(&in.buf, &out.buf, nullptr)) << "the session stopped serving ordinary images";
    std::vector<float> want = Ordinary();
    for (float& f : want) f *= 2.0f;
    EXPECT_TRUE(SameBits(want, out.px));
    EXPECT_EQ(0, CountAlerts(kNonFiniteNeedle));
    if (worker) {
        EXPECT_EQ(XPE_AI_WORKER_ACTIVE, QueryState().state);
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

// The contrast: a REAL worker fault (the worker cannot load its model) still counts, and the third one
// switches the worker off -- the rule the distinction above must not have loosened.
TEST_F(BoneSuppressNonFinite, WorkerFaultsStillSwitchTheWorkerOffAtTheCeiling) {
    const std::string missing = std::string(XPE_AI_TEST_DATA_DIR) + "/models_missing";
    ASSERT_EQ(XPE_OK, xpe_ai_init(missing.c_str(), "{\"use_worker\": true}"));
    for (int i = 1; i <= 3; ++i) {
        Img in(Ordinary());
        Img out(-7.0f);
        EXPECT_NE(XPE_OK, xpe_bone_suppress(&in.buf, &out.buf, nullptr));
        const WState w = QueryState();
        EXPECT_EQ(static_cast<uint32_t>(i), w.failures);
        EXPECT_EQ(i == 3 ? XPE_AI_WORKER_DISABLED : XPE_AI_WORKER_ACTIVE, w.state) << "after fault " << i;
    }
}


// --- the policy, pinned (leader decision, Codex #65): a VALID model response ends a run of worker faults -------------
//
// The ceiling counts CONSECUTIVE worker and transport faults. A response that carries a valid envelope is the
// observation "the worker works" -- the model refusing a pixel range is the model's business -- so it ends the run.
// There is no per-session total; a session that alternates faults and refusals is never switched off by them.
// This test fixes the order  fault, fault, valid-but-non-finite, fault, fault  : without the reset the fourth
// event would be the third of a run and switch the worker off; with it the count reads 2 and the worker stays on.
namespace {
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

void Freeze(DWORD pid) {   // suspend every thread of pid: alive and silent
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    THREADENTRY32 te{};
    te.dwSize = sizeof(te);
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
        if (te.th32OwnerProcessID != pid) continue;
        if (HANDLE t = OpenThread(THREAD_SUSPEND_RESUME, FALSE, te.th32ThreadID)) {
            SuspendThread(t);
            CloseHandle(t);
        }
    }
    CloseHandle(snap);
}

XpeErrorCode CallWith(const std::vector<float>& px) {
    Img in(px);
    Img out(-7.0f);
    return xpe_bone_suppress(&in.buf, &out.buf, nullptr);
}
}  // namespace

TEST_F(BoneSuppressNonFinite, AValidNonFiniteResponseEndsARunOfWorkerFaultsAndNeverSwitchesTheWorkerOff) {
    char tmp[MAX_PATH] = {0};
    GetTempPathA(sizeof(tmp), tmp);
    const std::string dir = std::string(tmp) + "xpe_ai_mix_" + std::to_string(GetCurrentProcessId());
    CreateDirectoryA(dir.c_str(), nullptr);
    const std::string model = dir + "/bone_suppress.onnx";
    DeleteFileA(model.c_str());
    ASSERT_EQ(XPE_OK, xpe_ai_init(dir.c_str(), "{\"use_worker\": true, \"timeout_ms\": 2000}"));
    xpe_clear_alerts();

    EXPECT_NE(XPE_OK, CallWith(Ordinary()));                       // fault 1: no model
    EXPECT_NE(XPE_OK, CallWith(Ordinary()));                       // fault 2
    EXPECT_EQ(2u, QueryState().failures);
    ASSERT_TRUE(xpe_test::CopyModelWithSignature(kDirX2, "bone_suppress", dir));
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, CallWith(ExtremeInputs()[0].in));   // valid response, non-finite result
    EXPECT_EQ(0u, QueryState().failures) << "a valid response must end the run of worker faults";
    EXPECT_EQ(XPE_AI_WORKER_ACTIVE, QueryState().state);

    const auto workers = ChildWorkers();
    ASSERT_EQ(1u, workers.size());
    Freeze(workers[0]);
    EXPECT_NE(XPE_OK, CallWith(Ordinary()));                       // fault 3 (a new run): a silent worker, killed
    ASSERT_TRUE(DeleteFileA(model.c_str()) != 0);
    EXPECT_NE(XPE_OK, CallWith(Ordinary()));                       // fault 4: a fresh worker, no model
    const WState w = QueryState();
    EXPECT_EQ(2u, w.failures) << "two faults after the valid response are the first two of a new run";
    EXPECT_EQ(XPE_AI_WORKER_ACTIVE, w.state) << "four faults in total, but never three in a row";
    EXPECT_EQ(4, CountAlerts("AI worker failed")) << "one alert per worker fault";
    EXPECT_EQ(1, CountAlerts(kNonFiniteNeedle)) << "and one for the refusal";
    EXPECT_EQ(0, CountAlerts("disabled"));

    xpe_ai_shutdown();
    xpe_test::RemoveModelDir(dir, "bone_suppress");
}
