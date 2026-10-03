/**
 * @file test_worker_supervisor.cpp
 * @brief The worker supervisor against the REAL worker process (QA-B-171B, #130).
 *
 * The questions, each asked of a real xpe_ai_worker.exe:
 *   - does a call reuse a healthy worker, and start one when none runs?
 *   - after the worker is killed from outside, does the NEXT call succeed, on a
 *     NEW pid?                                    (HAZ-008: crash -> restart)
 *   - a worker that stalls: does the call fail, the worker get killed, and the
 *     next call start a fresh one?               (REQ-AI-092 + HAZ-008)
 *   - is a requested exit told apart from a death by the exit code of the
 *     handle the supervisor HOLDS?   (the premise behind not adding SHUTDOWN_ACK)
 *   - does any worker outlive the supervisor?
 *
 * EVIDENCE IS INDEPENDENT. The test opens its OWN handle to each worker, kills
 * through it, and compares what the supervisor reports with what that handle
 * says. A supervisor that merely echoed back the code the test gave it would be
 * indistinguishable from one that read its handle; this is how they differ: the
 * kill is done with a code the supervisor could not know in advance, and the
 * stall is injected by suspending the process, which leaves no exit code at all
 * until the supervisor itself ends it.
 *
 * NOT A RETRY. A call that hits a fault FAILS (the tests assert that). The
 * restart is the supervisor's policy for the NEXT call; no test here wraps a
 * call in a loop to make it pass.
 *
 * HYGIENE. Every worker started is recorded by the supervisor, and a test that
 * cares holds a handle to each so it can assert, after the supervisor is gone,
 * that the process is gone too. The supervisor's destructor and a kill-on-close
 * job do the cleanup; nothing here relies on a trailing kill in a test body.
 */

#include <gtest/gtest.h>

#include "ai_worker_supervisor.h"
#include "xpe/ai/ai_onnx_session.h"
#include "xpe/ai/ai_worker_protocol.h"
#include "xpe/common/xpe_error.h"

#include <windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

extern "C" {
typedef struct XpeAiIpcBridge XpeAiIpcBridge;
XpeAiIpcBridge* xpe_ai_ipc_bridge_create(const char* pipe_name, uint32_t timeout_ms);
XpeErrorCode    xpe_ai_ipc_bridge_connect(XpeAiIpcBridge* bridge);
XpeErrorCode    xpe_ai_ipc_bridge_send(XpeAiIpcBridge* bridge, const XpeAiMessageHeader* header,
                                       const void* payload, uint32_t payload_size);
XpeErrorCode    xpe_ai_ipc_bridge_receive(XpeAiIpcBridge* bridge, XpeAiMessageHeader* header,
                                          void* payload, uint32_t payload_capacity,
                                          uint32_t* payload_size);
void            xpe_ai_ipc_bridge_destroy(XpeAiIpcBridge* bridge);
}

#ifndef XPE_AI_WORKER_EXE
#error "XPE_AI_WORKER_EXE must be defined by the build (modules/ai/CMakeLists.txt)"
#endif
#ifndef XPE_AI_TEST_DATA_DIR
#error "XPE_AI_TEST_DATA_DIR must be defined by the build (modules/ai/CMakeLists.txt)"
#endif

using xpe::ai::WorkerExit;
using xpe::ai::WorkerSupervisor;
using xpe::ai::WorkerSupervisorConfig;

namespace {

/** The supervisor's budget in the stall test. Short, so a pass is quick. */
constexpr uint32_t kStallBudgetMs = 800;
/**
 * The stall test spends its budget twice: the FIRST Ping starts a worker (a cold start, measured at up to
 * about 730 ms in a full build, QA-B-171C), and only then is the worker frozen so the next call can time
 * out. A budget of 800 ms left no margin for the first and failed once in a full-suite run on a busy
 * machine; this one is 2.7 times the worst cold start seen.
 */
constexpr uint32_t kStallTestBudgetMs = 2000;
/** Everywhere else: generous, because worker start-up is what is being timed. */
constexpr uint32_t kBudgetMs = 3000;
/** Slack for process noise on a loaded machine. */
constexpr DWORD kSlackMs = 2500;

WorkerSupervisorConfig Cfg(uint32_t budget_ms, const std::string& model_dir = std::string()) {
    WorkerSupervisorConfig c;
    c.worker_exe = XPE_AI_WORKER_EXE;
    c.model_dir = model_dir;
    c.timeout_ms = budget_ms;
    return c;
}

/** The test's own handle to a worker, independent of the supervisor's. */
class Proc {
public:
    explicit Proc(uint32_t pid) {
        h_ = OpenProcess(SYNCHRONIZE | PROCESS_TERMINATE | PROCESS_QUERY_LIMITED_INFORMATION |
                             PROCESS_SUSPEND_RESUME,
                         FALSE, pid);
    }
    ~Proc() { if (h_) CloseHandle(h_); }
    Proc(const Proc&) = delete;
    Proc& operator=(const Proc&) = delete;

    bool opened() const { return h_ != nullptr; }
    bool Alive() const { return h_ && WaitForSingleObject(h_, 0) == WAIT_TIMEOUT; }
    bool GoneWithin(DWORD ms) const { return h_ && WaitForSingleObject(h_, ms) == WAIT_OBJECT_0; }
    /** Kill with a code the supervisor cannot have predicted. */
    void Kill(uint32_t code) const {
        if (h_) TerminateProcess(h_, code);
        GoneWithin(3000);
    }
    long ExitCode() const {
        DWORD c = 0;
        return (h_ && GetExitCodeProcess(h_, &c) && c != STILL_ACTIVE) ? static_cast<long>(c) : -1;
    }
    /** Suspend every thread: the process stays alive and answers nothing. */
    void Freeze(uint32_t pid) const {
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

private:
    HANDLE h_ = nullptr;
};

bool IsStub() { return xpe::ai::OnnxSession::IsStubBuild(); }

}  // namespace

// --- reuse and start --------------------------------------------------------------

TEST(WorkerSupervisor, FirstCallStartsAWorkerAndTheSecondReusesIt) {
    WorkerSupervisor sup(Cfg(kBudgetMs));
    ASSERT_EQ(0u, sup.WorkerPid()) << "no worker should run before the first call";
    ASSERT_EQ(XPE_OK, sup.Ping());
    const uint32_t pid = sup.WorkerPid();
    ASSERT_NE(0u, pid);
    Proc p(pid);
    ASSERT_TRUE(p.opened());
    EXPECT_TRUE(p.Alive());
    ASSERT_EQ(XPE_OK, sup.Ping());
    EXPECT_EQ(pid, sup.WorkerPid()) << "a healthy worker must be reused, not restarted per call";
    EXPECT_EQ(1u, sup.StartCount());
}

// --- HAZ-008: the worker dies, the next call succeeds on a new pid -------------------

TEST(WorkerSupervisor, AfterTheWorkerIsKilledTheNextCallSucceedsOnANewPid) {
    WorkerSupervisor sup(Cfg(kBudgetMs));
    ASSERT_EQ(XPE_OK, sup.Ping());
    const uint32_t first = sup.WorkerPid();
    Proc p(first);
    ASSERT_TRUE(p.opened());

    p.Kill(99);
    ASSERT_TRUE(p.GoneWithin(0)) << "the kill did not take";
    ASSERT_EQ(99, p.ExitCode());

    ASSERT_EQ(XPE_OK, sup.Ping()) << "the call after a kill must succeed";
    const uint32_t second = sup.WorkerPid();
    EXPECT_NE(0u, second);
    EXPECT_NE(first, second) << "same pid: the dead worker was not replaced";
    EXPECT_EQ(2u, sup.StartCount());
}

TEST(WorkerSupervisor, TheSupervisorReadsTheExitCodeOfTheHandleItHolds) {
    // The premise behind leaving SHUTDOWN_ACK out of the protocol: the one who
    // launched the worker keeps its process handle, so it can read how it ended.
    WorkerSupervisor sup(Cfg(kBudgetMs));
    ASSERT_EQ(XPE_OK, sup.Ping());
    const uint32_t pid = sup.WorkerPid();
    Proc p(pid);
    ASSERT_TRUE(p.opened());
    p.Kill(99);                       // a code the supervisor was never told
    ASSERT_EQ(XPE_OK, sup.Ping());    // noticing the death happens on the next call

    const auto last = sup.LastExit();
    EXPECT_EQ(WorkerExit::kDied, last.kind);
    EXPECT_EQ(pid, last.pid);
    EXPECT_EQ(99u, last.exit_code) << "the supervisor did not read the code from its own handle";
}

TEST(WorkerSupervisor, AnUnrequestedExitIsADeathEvenWithExitCodeZero) {
    // A worker never exits by itself, so an exit nobody asked for is a death
    // whatever its code. Code 0 is the case that would fool a code-only check.
    WorkerSupervisor sup(Cfg(kBudgetMs));
    ASSERT_EQ(XPE_OK, sup.Ping());
    Proc p(sup.WorkerPid());
    ASSERT_TRUE(p.opened());
    p.Kill(0);
    ASSERT_EQ(XPE_OK, sup.Ping());
    EXPECT_EQ(WorkerExit::kDied, sup.LastExit().kind);
    EXPECT_EQ(0u, sup.LastExit().exit_code);
}

TEST(WorkerSupervisor, ARequestedExitIsCleanAndReadAsExitCodeZero) {
    WorkerSupervisor sup(Cfg(kBudgetMs));
    ASSERT_EQ(XPE_OK, sup.Ping());
    const uint32_t pid = sup.WorkerPid();
    Proc p(pid);
    ASSERT_TRUE(p.opened());

    sup.Stop();

    EXPECT_EQ(0u, sup.WorkerPid());
    EXPECT_TRUE(p.GoneWithin(0)) << "Stop() returned with the worker still running";
    EXPECT_EQ(0, p.ExitCode()) << "a requested shutdown should exit 0";
    const auto last = sup.LastExit();
    EXPECT_EQ(WorkerExit::kClean, last.kind);
    EXPECT_EQ(pid, last.pid);
    EXPECT_EQ(0u, last.exit_code);
    // And the supervisor is not finished: a later call starts a fresh worker.
    ASSERT_EQ(XPE_OK, sup.Ping());
    EXPECT_NE(pid, sup.WorkerPid());
}

// --- REQ-AI-092 + HAZ-008: a worker that stalls ----------------------------------------

TEST(WorkerSupervisor, AStalledWorkerFailsThatCallIsKilledAndTheNextCallStartsAFreshOne) {
    WorkerSupervisor sup(Cfg(kStallTestBudgetMs));
    ASSERT_EQ(XPE_OK, sup.Ping());
    const uint32_t stalled = sup.WorkerPid();
    Proc p(stalled);
    ASSERT_TRUE(p.opened());
    p.Freeze(stalled);   // alive, answers nothing, no exit code yet
    ASSERT_TRUE(p.Alive());

    const DWORD t0 = GetTickCount();
    const XpeErrorCode rc = sup.Ping();
    const DWORD took = GetTickCount() - t0;

    // The call FAILS: the supervisor does not hide the fault by re-running it.
    EXPECT_NE(XPE_OK, rc);
    EXPECT_GE(took, kStallTestBudgetMs - 100) << "returned before the budget: it did not wait";
    EXPECT_LE(took, kStallTestBudgetMs + kSlackMs) << "took " << took << " ms";

    // The stalled worker is gone: killed by the supervisor, not left running.
    EXPECT_TRUE(p.GoneWithin(3000)) << "the stalled worker is still running (a leaked process)";
    EXPECT_EQ(0u, sup.WorkerPid());
    const auto last = sup.LastExit();
    EXPECT_EQ(WorkerExit::kKilled, last.kind);
    EXPECT_EQ(stalled, last.pid);
    EXPECT_EQ(xpe::ai::kSupervisorKillExitCode, last.exit_code);

    // The NEXT call starts a fresh worker and succeeds.
    ASSERT_EQ(XPE_OK, sup.Ping());
    EXPECT_NE(0u, sup.WorkerPid());
    EXPECT_NE(stalled, sup.WorkerPid());
    EXPECT_EQ(2u, sup.StartCount());
}

// --- an error answer is a healthy worker ------------------------------------------------

TEST(WorkerSupervisor, AWorkerThatAnswersWithAnErrorIsKeptNotRestarted) {
    // A model directory with no model: the worker answers with an error frame
    // (the same code the in-process path gives). That is the worker doing its
    // job; restarting it would turn every bad request into a process churn.
    const std::string missing = std::string(XPE_AI_TEST_DATA_DIR) + "/models_missing";
    WorkerSupervisor sup(Cfg(kBudgetMs, missing));
    const float in[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    float out[9];
    for (float& v : out) v = -777.0f;

    EXPECT_EQ(XPE_ERR_IO_FAILED, sup.BoneSuppress(3, 3, in, out));
    const uint32_t pid = sup.WorkerPid();
    EXPECT_NE(0u, pid);
    EXPECT_EQ(XPE_ERR_IO_FAILED, sup.BoneSuppress(3, 3, in, out));
    EXPECT_EQ(pid, sup.WorkerPid()) << "an error answer must not restart the worker";
    EXPECT_EQ(1u, sup.StartCount());
    for (float v : out) EXPECT_EQ(-777.0f, v) << "output written on a failed call";
}

TEST(WorkerSupervisor, AfterARestartTheWorkerStillServesBoneSuppress) {
    if (IsStub()) {
        GTEST_SKIP() << "a stub worker has no model to run; restart is covered by Ping above";
    }
    const std::string dir = std::string(XPE_AI_TEST_DATA_DIR) + "/models_x2";
    WorkerSupervisor sup(Cfg(kBudgetMs, dir));
    const float in[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    float out[9] = {0};
    ASSERT_EQ(XPE_OK, sup.BoneSuppress(3, 3, in, out));
    for (int i = 0; i < 9; ++i) EXPECT_FLOAT_EQ(in[i] * 2.0f, out[i]);
    const uint32_t first = sup.WorkerPid();
    Proc p(first);
    ASSERT_TRUE(p.opened());
    p.Kill(99);

    float out2[9] = {0};
    ASSERT_EQ(XPE_OK, sup.BoneSuppress(3, 3, in, out2)) << "the call after a kill must succeed";
    EXPECT_NE(first, sup.WorkerPid());
    for (int i = 0; i < 9; ++i) EXPECT_FLOAT_EQ(in[i] * 2.0f, out2[i])
        << "the restarted worker loaded its model again (session start carried model_dir)";
}

// --- hygiene: no worker outlives the supervisor ------------------------------------------

TEST(WorkerSupervisor, NoWorkerOutlivesTheSupervisor) {
    std::vector<uint32_t> pids;
    std::vector<Proc*> held;
    {
        WorkerSupervisor sup(Cfg(kBudgetMs));
        ASSERT_EQ(XPE_OK, sup.Ping());
        for (int round = 0; round < 2; ++round) {
            Proc* p = new Proc(sup.WorkerPid());
            ASSERT_TRUE(p->opened());
            held.push_back(p);
            pids.push_back(sup.WorkerPid());
            if (round == 0) {
                p->Kill(7);                 // one died...
                ASSERT_EQ(XPE_OK, sup.Ping());   // ...and was replaced
            }
        }
        Proc* last = new Proc(sup.WorkerPid());   // the one still running at scope exit
        held.push_back(last);
        pids.push_back(sup.WorkerPid());
        ASSERT_TRUE(last->Alive());
    }   // supervisor destroyed here
    int still_running = 0;
    for (Proc* p : held) {
        if (!p->GoneWithin(3000)) ++still_running;
        delete p;
    }
    EXPECT_EQ(0, still_running) << still_running << " of " << pids.size()
                                << " workers outlived the supervisor";
}

// --- the baseline this card exists for -----------------------------------------------------

TEST(WorkerSupervisor, BaselineWithoutASupervisorAStalledWorkerStaysAliveAndTheBridgeStaysDown) {
    // The measurement taken before the supervisor existed: the bridge alone.
    // After a stall the bridge drops its connection (QA-B-171) -- but nobody
    // ends the worker and nobody starts another. Kept as a test so the reason
    // for the supervisor stays observable.
    char pipe[128];
    std::snprintf(pipe, sizeof(pipe), "\\\\.\\pipe\\xpe_ai_sup_base_%lu",
                  static_cast<unsigned long>(GetCurrentProcessId()));
    std::string cmd = std::string("\"") + XPE_AI_WORKER_EXE + "\" --diagnostic " + pipe;
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    ASSERT_TRUE(CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                               nullptr, nullptr, &si, &pi) != 0);
    Proc p(pi.dwProcessId);
    XpeAiIpcBridge* b = xpe_ai_ipc_bridge_create(pipe, 600);
    bool connected = false;
    for (int i = 0; b && i < 60 && !connected; ++i) {
        connected = xpe_ai_ipc_bridge_connect(b) == XPE_OK;
        if (!connected) Sleep(50);
    }
    ASSERT_TRUE(connected);
    p.Freeze(pi.dwProcessId);

    XpeAiMessageHeader req{};
    req.magic = XPE_AI_MSG_MAGIC;
    req.version = (XPE_AI_PROTOCOL_VERSION_MAJOR << 16) | XPE_AI_PROTOCOL_VERSION_MINOR;
    req.messageType = XPE_AI_MSG_HEARTBEAT;
    req.requestId = 1;
    xpe_ai_ipc_bridge_send(b, &req, nullptr, 0);
    XpeAiMessageHeader rep{};
    char buf[64];
    uint32_t got = 0;
    const XpeErrorCode first = xpe_ai_ipc_bridge_receive(b, &rep, buf, sizeof(buf), &got);
    const XpeErrorCode second = xpe_ai_ipc_bridge_send(b, &req, nullptr, 0);
    const bool alive = p.Alive();

    std::printf("[baseline] after a stall: first receive rc=%d, next send rc=%d, worker alive=%d\n",
                static_cast<int>(first), static_cast<int>(second), alive ? 1 : 0);
    EXPECT_NE(XPE_OK, first);
    EXPECT_EQ(XPE_ERR_NOT_INITIALIZED, second) << "the bridge stays down";
    EXPECT_TRUE(alive) << "the stalled worker is left running when nothing supervises it";

    // Cleanup that does not depend on the assertions above.
    xpe_ai_ipc_bridge_destroy(b);
    TerminateProcess(pi.hProcess, 1);
    WaitForSingleObject(pi.hProcess, 3000);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
}

// --- Codex audit #11 --------------------------------------------------------------------------

#ifdef XPE_AI_FAKE_WORKER_EXE
namespace {

/** Sets XPE_FAKE_WORKER_MODE for the workers a supervisor starts, and clears it afterwards. */
class FakeMode {
public:
    explicit FakeMode(const char* mode) { _putenv_s("XPE_FAKE_WORKER_MODE", mode); }
    ~FakeMode() { _putenv_s("XPE_FAKE_WORKER_MODE", ""); }
};

WorkerSupervisorConfig FakeCfg() {
    WorkerSupervisorConfig c;
    c.worker_exe = XPE_AI_FAKE_WORKER_EXE;
    c.timeout_ms = kBudgetMs;
    return c;
}

}  // namespace

TEST(WorkerSupervisor, ControlTheFakeWorkerInOkModeBehavesLikeTheRealOne) {
    // Without this, the two tests below could be red for a reason that has nothing to do with
    // the supervisor: a fake worker that cannot even start or be pinged.
    FakeMode m("ok");
    WorkerSupervisor sup(FakeCfg());
    ASSERT_EQ(XPE_OK, sup.Ping());
    const uint32_t pid = sup.WorkerPid();
    ASSERT_EQ(XPE_OK, sup.Ping());
    EXPECT_EQ(pid, sup.WorkerPid());
    EXPECT_EQ(1u, sup.StartCount());
}

// QA-B-181j (Codex #65): the supervisor reports "the last refusal was the MODEL's" only for a reply with a valid
// success envelope. An empty envelope with the same NaN pixels is a protocol fault -- IO_FAILED, the worker
// discarded with its connection, and NOT the exemption that keeps a call out of the failure count.
TEST(WorkerSupervisor, AValidEnvelopeWithNaNPixelsIsAModelRefusalAndKeepsTheWorker) {
    FakeMode m("bone_valid_nan");
    WorkerSupervisor sup(FakeCfg());
    const float in[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    float out[9];
    for (float& v : out) v = -777.0f;
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, sup.BoneSuppress(3, 3, in, out));
    EXPECT_TRUE(sup.LastResultWasNonFinite());
    for (float v : out) EXPECT_EQ(-777.0f, v);
    EXPECT_NE(0u, sup.WorkerPid()) << "a valid response must keep the worker";
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, sup.BoneSuppress(3, 3, in, out));   // and the next call is served by it
    EXPECT_EQ(1u, sup.StartCount());
}

TEST(WorkerSupervisor, AGarbageEnvelopeWithNaNPixelsIsAProtocolFaultNotAModelRefusal) {
    FakeMode m("bone_garbage_nan");
    WorkerSupervisor sup(FakeCfg());
    const float in[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    float out[9];
    for (float& v : out) v = -777.0f;
    EXPECT_EQ(XPE_ERR_IO_FAILED, sup.BoneSuppress(3, 3, in, out));
    EXPECT_FALSE(sup.LastResultWasNonFinite()) << "a broken envelope must not borrow the model-refusal exemption";
    for (float v : out) EXPECT_EQ(-777.0f, v);
    EXPECT_EQ(0u, sup.WorkerPid()) << "a worker with a broken envelope was kept";
}

TEST(WorkerSupervisor, AHeartbeatAnswerOfTheWrongTypeDiscardsTheWorker) {
    // The answer is a complete frame, so the bridge keeps its connection; only the supervisor can
    // see that it is not an answer to a heartbeat. A worker that answers nonsense is not healthy.
    FakeMode m("wrong_type");
    WorkerSupervisor sup(FakeCfg());
    EXPECT_NE(XPE_OK, sup.Ping());
    const uint32_t first = sup.LastExit().pid;   // the worker that was discarded, if any
    EXPECT_EQ(0u, sup.WorkerPid()) << "a worker that gave a wrong answer was kept";
    EXPECT_EQ(WorkerExit::kKilled, sup.LastExit().kind);
    EXPECT_NE(XPE_OK, sup.Ping());               // same mode, so it fails again...
    EXPECT_EQ(2u, sup.StartCount()) << "...but on a fresh worker, not the one that answered wrongly";
    EXPECT_NE(first, sup.LastExit().pid);
}

TEST(WorkerSupervisor, AHeartbeatAnswerToSomeoneElsesRequestDiscardsTheWorker) {
    FakeMode m("wrong_id");
    WorkerSupervisor sup(FakeCfg());
    EXPECT_NE(XPE_OK, sup.Ping());
    EXPECT_EQ(0u, sup.WorkerPid());
    EXPECT_EQ(WorkerExit::kKilled, sup.LastExit().kind);
    EXPECT_NE(XPE_OK, sup.Ping());
    EXPECT_EQ(2u, sup.StartCount());
}

// QA-B-171C, restart policy: what does a worker that keeps failing cost? The supervisor restarts on
// every call after a fault and has NO limit. This does not decide whether it should have one (the
// requirements give no number, and none is invented here); it measures what "no limit" costs so the
// decision can be made on that.
TEST(WorkerSupervisor, MeasureTheCostOfRepeatedFailuresWhenAWorkerHangsOrDiesOnStart) {
    auto cost_of = [](const char* mode, std::vector<DWORD>* per_call, uint32_t* starts) {
        FakeMode m(mode);
        WorkerSupervisorConfig c = FakeCfg();
        c.timeout_ms = kStallBudgetMs;
        WorkerSupervisor sup(c);
        for (int i = 0; i < 3; ++i) {
            const DWORD t0 = GetTickCount();
            EXPECT_NE(XPE_OK, sup.Ping()) << mode;
            per_call->push_back(GetTickCount() - t0);
        }
        *starts = sup.StartCount();
    };
    std::vector<DWORD> hang, die;
    uint32_t hang_starts = 0, die_starts = 0;
    cost_of("no_pipe", &hang, &hang_starts);
    cost_of("exit_on_start", &die, &die_starts);
    std::printf("[restart-cost] budget %u ms. hangs on start: calls took %lu / %lu / %lu ms, %u starts for 3 calls\n",
                kStallBudgetMs, static_cast<unsigned long>(hang[0]), static_cast<unsigned long>(hang[1]),
                static_cast<unsigned long>(hang[2]), hang_starts);
    std::printf("[restart-cost] budget %u ms. dies on start: calls took %lu / %lu / %lu ms, %u starts for 3 calls\n",
                kStallBudgetMs, static_cast<unsigned long>(die[0]), static_cast<unsigned long>(die[1]),
                static_cast<unsigned long>(die[2]), die_starts);
    // What is pinned: every failed call starts another worker (no limit today), and a hanging one
    // costs the whole budget each time.
    EXPECT_EQ(3u, hang_starts);
    EXPECT_EQ(3u, die_starts);
    for (DWORD ms : hang) EXPECT_GE(ms, kStallBudgetMs - 100u);
}
#endif  // XPE_AI_FAKE_WORKER_EXE

TEST(WorkerSupervisor, StopAfterAnUnnoticedExitRecordsADeathNotAKill) {
    // The worker ended on its own with code 0 and nothing has called the supervisor since. Stop()
    // must look at the handle first: it did not terminate this worker, so it must not say it did.
    WorkerSupervisor sup(Cfg(kBudgetMs));
    ASSERT_EQ(XPE_OK, sup.Ping());
    const uint32_t pid = sup.WorkerPid();
    Proc p(pid);
    ASSERT_TRUE(p.opened());
    p.Kill(0);

    sup.Stop();

    const auto last = sup.LastExit();
    EXPECT_EQ(WorkerExit::kDied, last.kind) << "an exit nobody asked for was recorded as a kill";
    EXPECT_EQ(pid, last.pid);
    EXPECT_EQ(0u, last.exit_code);
}

// --- QA-B-171C measurement: does a cold worker fit the default time budget? --------------------
//
// REQ-AI-092's default is 5 s (XPE_AI_DEFAULT_TIMEOUT_MS). That budget bounds worker start-up AND the
// first request, and the first request is where the model is loaded (the worker loads lazily). So
// "start-up fits the budget" has two parts, timed separately and together, ten cold starts each:
//   start  = CreateProcess + pipe connect + session-start message (Ping)
//   first  = the first BoneSuppress, i.e. the model load plus one run
// THE MODEL IS A TOY (3x3 scale, a few hundred bytes). A clinical U-Net is orders of magnitude
// larger; this measures the fixed costs (process, ONNX Runtime DLL load, session creation), not
// the cost of loading that model, which is unmeasured and which these numbers cannot bound.

namespace {
std::vector<DWORD> Sorted(std::vector<DWORD> v) {
    std::sort(v.begin(), v.end());
    return v;
}
}  // namespace

TEST(WorkerSupervisor, MeasureColdStartAgainstTheDefaultBudget) {
    constexpr int kRuns = 10;
    std::string dir = std::string(XPE_AI_TEST_DATA_DIR) + "/models_x2";
    {
        // A scratch model directory can be named here to see how load time scales with model size.
        // Measurement only: unset, the test uses the checked-in toy model.
        char buf[512] = {0};
        size_t len = 0;
        if (getenv_s(&len, buf, sizeof(buf), "XPE_AI_COLD_MODEL_DIR") == 0 && len > 1) dir = buf;
    }
    std::vector<DWORD> start_ms, first_ms, total_ms;
    for (int i = 0; i < kRuns; ++i) {
        WorkerSupervisor sup(Cfg(XPE_AI_DEFAULT_TIMEOUT_MS, dir));
        const DWORD t0 = GetTickCount();
        ASSERT_EQ(XPE_OK, sup.Ping());
        const DWORD t1 = GetTickCount();
        start_ms.push_back(t1 - t0);
        if (!IsStub()) {
            const float in[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
            float out[9] = {0};
            ASSERT_EQ(XPE_OK, sup.BoneSuppress(3, 3, in, out));
            first_ms.push_back(GetTickCount() - t1);
            total_ms.push_back(GetTickCount() - t0);
        }
    }
    auto line = [](const char* name, const std::vector<DWORD>& v, int runs) {
        if (v.empty()) { std::printf("[cold-start] %-6s n/a (stub build: no model to load)\n", name); return; }
        const auto s = Sorted(v);
        std::printf("[cold-start] %-6s min=%lu median=%lu max=%lu ms over %d cold starts\n", name,
                    static_cast<unsigned long>(s.front()), static_cast<unsigned long>(s[s.size() / 2]),
                    static_cast<unsigned long>(s.back()), runs);
    };
    WIN32_FILE_ATTRIBUTE_DATA fa{};
    unsigned long long model_bytes = 0;
    if (GetFileAttributesExA((dir + "/bone_suppress.onnx").c_str(), GetFileExInfoStandard, &fa)) {
        model_bytes = (static_cast<unsigned long long>(fa.nFileSizeHigh) << 32) | fa.nFileSizeLow;
    }
    std::printf("[cold-start] budget %u ms, build=%s, model_dir=%s, model_file=%llu bytes\n",
                XPE_AI_DEFAULT_TIMEOUT_MS, IsStub() ? "stub" : "full", dir.c_str(), model_bytes);
    line("start", start_ms, kRuns);
    line("first", first_ms, kRuns);
    line("total", total_ms, kRuns);
    // What is asserted: the toy-model fixed costs fit with room to spare. It is NOT a claim about a
    // clinical model, and says so in its name and its output.
    EXPECT_LT(Sorted(start_ms).back(), XPE_AI_DEFAULT_TIMEOUT_MS);
    if (!total_ms.empty()) EXPECT_LT(Sorted(total_ms).back(), XPE_AI_DEFAULT_TIMEOUT_MS);
}
