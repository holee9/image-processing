/**
 * @file test_bone_suppress_worker_path.cpp
 * @brief xpe_bone_suppress through the worker process, opt-in (QA-B-171C, #130, REQ-AI-092).
 *
 * `use_worker` in the xpe_ai_init config routes xpe_bone_suppress through the worker supervisor
 * (QA-B-171B). Off by default. When the worker path fails -- budget exceeded, a worker that died or
 * went silent, a wrong answer -- the failure is REPORTED (an alert) and the call falls back to the
 * in-process result: REQ-AI-092, "exceeding the budget shall trigger fallback and alert".
 *
 * WHAT THE TESTS HOLD FIXED
 *   - Off: bit-identical to the old behaviour, and no worker process exists.
 *   - On: the SAME answer as in-process, from a worker process that is a child of this one, found
 *     beside xpe_ai.dll, and gone after xpe_ai_shutdown().
 *   - A silent worker (frozen from outside, so it answers nothing): the call still returns the
 *     in-process answer, an alert names the failure, the frozen worker is ended, and the NEXT call
 *     succeeds on a new worker WITHOUT a new failure alert.
 *
 * WORKER PROCESSES ARE FOUND BY PARENTAGE, not by image name: another test's worker, or a second
 * test binary on the machine, must not be counted. A child of THIS process with the worker's name
 * is this test's worker.
 */

#include <gtest/gtest.h>

#include "xpe/ai/ai_api.h"
#include "xpe/ai/ai_onnx_session.h"
#include "xpe/common/xpe_error.h"

#include <windows.h>
#include <tlhelp32.h>

#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#ifndef XPE_AI_TEST_DATA_DIR
#error "XPE_AI_TEST_DATA_DIR must be defined by the build (modules/ai/CMakeLists.txt)"
#endif

namespace {

const std::string kDirX2 = std::string(XPE_AI_TEST_DATA_DIR) + "/models_x2";

constexpr uint32_t kW = 3, kH = 3;
constexpr size_t kN = static_cast<size_t>(kW) * kH;
constexpr float kSentinel = -12345.0f;

bool IsStub() { return xpe::ai::OnnxSession::IsStubBuild(); }

struct Img {
    std::vector<float> px;
    XpeImageBuffer buf{};
    explicit Img(float base, bool fill = true) : px(kN, kSentinel) {
        if (fill) for (size_t i = 0; i < kN; ++i) px[i] = base + static_cast<float>(i);
        buf.width = kW;
        buf.height = kH;
        buf.bitsAllocated = 32;
        buf.bitsStored = 32;
        buf.format = XPE_PIXEL_FLOAT32;
        buf.data = px.data();
        buf.dataSize = kN * sizeof(float);
    }
};

struct Result {
    XpeErrorCode rc = XPE_OK;
    std::vector<float> out;
};

Result Call(const char* config_for_init) {
    Result r;
    EXPECT_EQ(XPE_OK, xpe_ai_init(kDirX2.c_str(), config_for_init));
    xpe_clear_alerts();   // init must not have queued anything this test is about to count
    Img in(1.0f), out(0.0f, false);
    r.rc = xpe_bone_suppress(&in.buf, &out.buf, nullptr);
    r.out = out.px;
    return r;
}

/** Alerts whose text contains @p needle, with the severity of the last one. */
int CountAlerts(const char* needle, int32_t* severity = nullptr) {
    int hits = 0;
    const int32_t total = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < total; ++i) {
        char msg[512] = {0};
        int32_t sev = -1;
        if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) != XPE_OK) continue;
        if (std::string(msg).find(needle) != std::string::npos) {
            ++hits;
            if (severity) *severity = sev;
        }
    }
    return hits;
}

constexpr const char* kFailureNeedle = "AI worker";
constexpr const char* kProcessedNeedle = "AI-processed";

/** Pids of worker processes that are children of this process. */
std::vector<DWORD> ChildWorkers() {
    std::vector<DWORD> pids;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return pids;
    PROCESSENTRY32 pe{};
    pe.dwSize = sizeof(pe);
    for (BOOL ok = Process32First(snap, &pe); ok; ok = Process32Next(snap, &pe)) {
        if (pe.th32ParentProcessID == GetCurrentProcessId() &&
            _stricmp(pe.szExeFile, "xpe_ai_worker.exe") == 0) {
            pids.push_back(pe.th32ProcessID);
        }
    }
    CloseHandle(snap);
    return pids;
}

/** Wait until there are exactly @p n child workers, up to @p ms. */
bool WaitForChildWorkers(size_t n, DWORD ms) {
    for (DWORD waited = 0; waited <= ms; waited += 50) {
        if (ChildWorkers().size() == n) return true;
        Sleep(50);
    }
    return ChildWorkers().size() == n;
}

std::string DirOf(const std::string& path) {
    const size_t cut = path.find_last_of("\\/");
    return cut == std::string::npos ? std::string() : path.substr(0, cut);
}

std::string ImageDirOfProcess(DWORD pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return std::string();
    char buf[MAX_PATH * 2] = {0};
    DWORD len = sizeof(buf);
    std::string dir;
    if (QueryFullProcessImageNameA(h, 0, buf, &len)) dir = DirOf(buf);
    CloseHandle(h);
    return dir;
}

std::string LoadedDllDir(const char* name) {
    char buf[MAX_PATH * 2] = {0};
    HMODULE m = GetModuleHandleA(name);
    if (!m || !GetModuleFileNameA(m, buf, sizeof(buf))) return std::string();
    return DirOf(buf);
}

bool SameDir(std::string a, std::string b) {
    for (char& c : a) { if (c == '/') c = '\\'; c = static_cast<char>(tolower(c)); }
    for (char& c : b) { if (c == '/') c = '\\'; c = static_cast<char>(tolower(c)); }
    return !a.empty() && a == b;
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

struct WorkerPathFixture : public ::testing::Test {
    void SetUp() override { xpe_ai_shutdown(); xpe_clear_alerts(); }
    void TearDown() override {
        xpe_ai_shutdown();
        xpe_clear_alerts();
        // Nothing may be left behind by a test: the supervisor ends its worker on shutdown.
        EXPECT_TRUE(WaitForChildWorkers(0, 3000)) << "a worker outlived xpe_ai_shutdown()";
    }
};

}  // namespace

// --- off by default: nothing changes -------------------------------------------------------------

TEST_F(WorkerPathFixture, ControlTheProbesCanSeeAWorkerAndAnAlert) {
    // Without this, "no worker" and "no alert" below are passed by a broken probe just as happily as
    // by a correct implementation. A worker is started the supervisor's own way and found by parentage;
    // an alert is pushed and read back.
    xpe_alert_push("AI worker probe control", XPE_ALERT_WARNING);
    EXPECT_EQ(1, CountAlerts(kFailureNeedle));
    xpe_clear_alerts();

    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    char pipe[96];
    std::snprintf(pipe, sizeof(pipe), "\\\\.\\pipe\\xpe_ai_probe_%lu",
                  static_cast<unsigned long>(GetCurrentProcessId()));
    std::string cmd = std::string("\"") + LoadedDllDir("xpe_ai.dll") + "\\xpe_ai_worker.exe\" " + pipe;
    ASSERT_TRUE(CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                               nullptr, nullptr, &si, &pi) != 0)
        << "cannot launch the worker next to xpe_ai.dll: " << cmd;
    EXPECT_TRUE(WaitForChildWorkers(1, 3000)) << "the parentage probe cannot see a worker that exists";
    TerminateProcess(pi.hProcess, 1);
    WaitForSingleObject(pi.hProcess, 3000);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    EXPECT_TRUE(WaitForChildWorkers(0, 3000));
}

TEST_F(WorkerPathFixture, OffByDefaultMeansNoWorkerAndTheSameAnswerAsBefore) {
    const Result plain = Call(nullptr);
    EXPECT_EQ(0u, ChildWorkers().size());
    xpe_ai_shutdown();
    xpe_clear_alerts();
    const Result off = Call("{\"use_worker\": false}");
    EXPECT_EQ(0u, ChildWorkers().size()) << "use_worker=false started a worker";
    EXPECT_EQ(plain.rc, off.rc);
    ASSERT_EQ(plain.out.size(), off.out.size());
    EXPECT_EQ(0, std::memcmp(plain.out.data(), off.out.data(), plain.out.size() * sizeof(float)))
        << "use_worker=false changed the output bits";
    EXPECT_EQ(0, CountAlerts(kFailureNeedle)) << "the in-process path must not report a worker failure";
}

// --- on: the same answer from a worker that lives where it should ---------------------------------

TEST_F(WorkerPathFixture, OnGivesTheSameAnswerAsInProcessFromAWorkerBesideTheDll) {
    if (IsStub()) {
        GTEST_SKIP() << "a stub worker has no model to run; see the stub-build test below";
    }
    const Result reference = Call("{\"use_worker\": false}");
    ASSERT_EQ(XPE_OK, reference.rc);
    xpe_ai_shutdown();
    xpe_clear_alerts();

    const Result via_worker = Call("{\"use_worker\": true}");
    ASSERT_EQ(XPE_OK, via_worker.rc);
    ASSERT_EQ(reference.out.size(), via_worker.out.size());
    EXPECT_EQ(0, std::memcmp(reference.out.data(), via_worker.out.data(),
                             reference.out.size() * sizeof(float)))
        << "the worker path answered differently from the in-process path";
    for (size_t i = 0; i < kN; ++i) EXPECT_FLOAT_EQ((1.0f + static_cast<float>(i)) * 2.0f, via_worker.out[i]);

    const auto workers = ChildWorkers();
    ASSERT_EQ(1u, workers.size()) << "the answer did not come from a worker process";
    EXPECT_TRUE(SameDir(ImageDirOfProcess(workers[0]), LoadedDllDir("xpe_ai.dll")))
        << "worker at '" << ImageDirOfProcess(workers[0]) << "', dll at '" << LoadedDllDir("xpe_ai.dll")
        << "'";
    EXPECT_EQ(0, CountAlerts(kFailureNeedle)) << "a healthy worker call raised a failure alert";
    EXPECT_EQ(1, CountAlerts(kProcessedNeedle)) << "SRS-ALERT-004 must fire once for a DL result";

    xpe_ai_shutdown();
    EXPECT_TRUE(WaitForChildWorkers(0, 3000)) << "xpe_ai_shutdown() left the worker running";
}

// --- a worker that fails: reported and replaced -----------------------------------------------------

TEST_F(WorkerPathFixture, InAStubBuildTheWorkersErrorIsReportedAndTheInProcessResultStands) {
    if (!IsStub()) GTEST_SKIP() << "full build: the stall test below covers failure on a real worker";
    const Result reference = Call("{\"use_worker\": false}");
    xpe_ai_shutdown();
    xpe_clear_alerts();

    const Result r = Call("{\"use_worker\": true}");
    EXPECT_EQ(reference.rc, r.rc) << "the fallback result must be the in-process result";
    int32_t sev = -1;
    EXPECT_GE(CountAlerts(kFailureNeedle, &sev), 1) << "a failed worker call raised no alert";
    EXPECT_EQ(XPE_ALERT_WARNING, sev);
}

TEST_F(WorkerPathFixture, ASilentWorkerIsReportedReplacedByTheInProcessResultAndTheNextCallRecovers) {
    if (IsStub()) GTEST_SKIP() << "needs a worker that can serve a model: full build only";
    // 2 s budget: long enough for a cold start of the toy-model worker (measured well under 1 s),
    // short enough that the silent call does not stretch the suite.
    ASSERT_EQ(XPE_OK, xpe_ai_init(kDirX2.c_str(), "{\"use_worker\": true, \"timeout_ms\": 2000}"));
    xpe_clear_alerts();
    Img in(1.0f), out1(0.0f, false), out2(0.0f, false), out3(0.0f, false);

    ASSERT_EQ(XPE_OK, xpe_bone_suppress(&in.buf, &out1.buf, nullptr));   // healthy: starts the worker
    const auto first = ChildWorkers();
    ASSERT_EQ(1u, first.size());
    EXPECT_EQ(0, CountAlerts(kFailureNeedle));
    xpe_clear_alerts();

    Freeze(first[0]);   // alive, silent
    const DWORD t0 = GetTickCount();
    const XpeErrorCode rc2 = xpe_bone_suppress(&in.buf, &out2.buf, nullptr);
    const DWORD took = GetTickCount() - t0;

    EXPECT_EQ(XPE_OK, rc2) << "the in-process fallback must still deliver a result";
    for (size_t i = 0; i < kN; ++i) {
        EXPECT_FLOAT_EQ((1.0f + static_cast<float>(i)) * 2.0f, out2.px[i])
            << "fallback result differs from the in-process answer at " << i;
    }
    int32_t sev = -1;
    EXPECT_EQ(1, CountAlerts(kFailureNeedle, &sev)) << "the silent worker was not reported (once)";
    EXPECT_EQ(XPE_ALERT_WARNING, sev);
    EXPECT_GE(took, 1800u) << "returned before the budget: it did not wait for the worker";
    EXPECT_LE(took, 2000u + 3000u) << "took " << took << " ms";

    // The frozen worker is gone, not left running behind the fallback.
    bool frozen_gone = true;
    for (DWORD pid : ChildWorkers()) if (pid == first[0]) frozen_gone = false;
    EXPECT_TRUE(frozen_gone) << "the silent worker was left running";
    xpe_clear_alerts();

    // The NEXT call recovers on a new worker, with no new failure.
    ASSERT_EQ(XPE_OK, xpe_bone_suppress(&in.buf, &out3.buf, nullptr));
    const auto after = ChildWorkers();
    ASSERT_EQ(1u, after.size());
    EXPECT_NE(first[0], after[0]) << "the same worker answered: it was not replaced";
    EXPECT_EQ(0, CountAlerts(kFailureNeedle)) << "a recovered call raised a failure alert";
    for (size_t i = 0; i < kN; ++i) EXPECT_FLOAT_EQ((1.0f + static_cast<float>(i)) * 2.0f, out3.px[i]);
}

// QA-B-171C, alert volume: a worker that fails on EVERY call raises one alert per call. The queue holds
// 64 entries and evicts oldest-lowest-severity first (xpe_error.h), so a persistent fault fills it
// and pushes out everything else. Measured, not decided: whether to limit it is a design question
// for the owner of the alert contract.
TEST_F(WorkerPathFixture, MeasureAlertVolumeWhenTheWorkerFailsOnEveryCall) {
    if (!IsStub()) GTEST_SKIP() << "stub build only: there the worker fails every call, deterministically";
    ASSERT_EQ(XPE_OK, xpe_ai_init(kDirX2.c_str(), "{\"use_worker\": true}"));
    xpe_clear_alerts();
    xpe_alert_push("an unrelated warning raised before the failures", XPE_ALERT_WARNING);
    constexpr int kCalls = 80;
    for (int i = 0; i < kCalls; ++i) {
        Img in(1.0f), out(0.0f, false);
        xpe_bone_suppress(&in.buf, &out.buf, nullptr);
    }
    const int worker_alerts = CountAlerts(kFailureNeedle);
    const int unrelated = CountAlerts("an unrelated warning");
    std::printf("[alert-volume] %d failing calls: queue holds %d alerts, %d of them worker failures, "
                "unrelated earlier warning still queued: %s\n",
                kCalls, static_cast<int>(xpe_get_pending_alert_count()), worker_alerts,
                unrelated ? "yes" : "NO (evicted)");
    EXPECT_LE(xpe_get_pending_alert_count(), 64);
    EXPECT_GE(worker_alerts, 1);
}
