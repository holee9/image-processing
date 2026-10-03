/**
 * @file test_ai_worker_pipe_peer.cpp
 * @brief Who is on the other end of the worker's pipe (QA-B-198b, Codex #95, #250).
 *
 * The pipe's DACL keeps other users and Anonymous out (test_ai_worker_sandbox.cpp). A process of the SAME user passes
 * the DACL, so the two ends also check each other:
 *   - the worker accepts only a pipe client whose process id is the host's (the host passes its id as argv[2]); any
 *     other client is cut off and the worker waits for the next one, and after a bound it exits;
 *   - the worker creates its pipe with FILE_FLAG_FIRST_PIPE_INSTANCE: a name that is already served makes it exit;
 *   - the host accepts only a pipe server whose process id is the child it started, before it sends anything;
 *   - the pipe name carries 128 random bits, and the host connects at the ANONYMOUS impersonation level, so a process
 *     that serves the pipe in its place cannot act with the host's identity.
 * NOT closed: a process of the same user can debug the host or the worker, and can still make the host's start fail
 * (a denial of service, by connecting to the worker's pipe more times than the bound before the host does).
 */

#include <windows.h>

#include <gtest/gtest.h>

#include "ai_worker_supervisor.h"
#include "xpe/common/xpe_error.h"

#include <atomic>
#include <set>
#include <string>
#include <thread>

#ifndef XPE_AI_TEST_DATA_DIR
#error "XPE_AI_TEST_DATA_DIR must be defined by the build (modules/ai/CMakeLists.txt)"
#endif
#ifndef XPE_AI_WORKER_EXE
#error "XPE_AI_WORKER_EXE must be defined by the build (modules/ai/CMakeLists.txt)"
#endif
#ifndef XPE_AI_FAKE_WORKER_EXE
#error "XPE_AI_FAKE_WORKER_EXE must be defined by the build (modules/ai/CMakeLists.txt)"
#endif

extern "C" {
XpeAiIpcBridge* xpe_ai_ipc_bridge_create(const char* pipe_name, uint32_t timeout_ms);
XpeErrorCode    xpe_ai_ipc_bridge_connect(XpeAiIpcBridge* bridge);
XpeErrorCode    xpe_ai_ipc_bridge_send(XpeAiIpcBridge* bridge, const XpeAiMessageHeader* header,
                                       const void* payload, uint32_t payload_size);
void            xpe_ai_ipc_bridge_destroy(XpeAiIpcBridge* bridge);
}

namespace {

using xpe::ai::WorkerSupervisor;
using xpe::ai::WorkerSupervisorConfig;

const std::string kData = XPE_AI_TEST_DATA_DIR;
constexpr uint32_t kBudgetMs = 20000;
constexpr DWORD kNotThisProcess = 4;   // the System process: never the test's own id

struct EnvScope {
    const char* name;
    EnvScope(const char* n, const char* value) : name(n) { SetEnvironmentVariableA(name, value); }
    ~EnvScope() { SetEnvironmentVariableA(name, nullptr); }
    EnvScope(const EnvScope&) = delete;
    EnvScope& operator=(const EnvScope&) = delete;
};

std::string UniquePipeName() {
    static std::atomic<int> serial{0};
    return "\\\\.\\pipe\\xpe_ai_peer_test_" + std::to_string(GetCurrentProcessId()) + "_" + std::to_string(serial++);
}

/**
 * The REAL worker, started directly as the test's own user. @p hostArg picks the argument form (QA-B-198c):
 *   a number   the supervised form: `<pipe> <hostArg>`
 *   "diag"     the diagnostic form: `--diagnostic <pipe>`
 *   ""         the old pipe-only form `<pipe>` (now refused)
 *   "none"     no argument at all (refused)
 * @p extra is appended to whatever form was chosen (an extra argument, refused).
 */
struct PeerWorker {
    PROCESS_INFORMATION pi{};
    std::string pipe;
    PeerWorker(const std::string& hostArg, const std::string& pipeName = UniquePipeName(), const std::string& extra = "")
        : pipe(pipeName) {
        std::string args = hostArg == "diag" ? "--diagnostic " + pipe
                         : hostArg == "none" ? std::string()
                         : pipe + (hostArg.empty() ? "" : " " + hostArg);
        if (!extra.empty()) args += (args.empty() ? "" : " ") + extra;
        std::string cmd = std::string("\"") + XPE_AI_WORKER_EXE + "\" " + args;
        STARTUPINFOA si{};
        si.cb = sizeof(si);
        if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) pi = {};
    }
    bool Running() const { return pi.hProcess && WaitForSingleObject(pi.hProcess, 0) == WAIT_TIMEOUT; }
    /** The exit code after waiting up to @p ms; -1 when it is still running. */
    long ExitCodeWithin(DWORD ms) const {
        if (!pi.hProcess || WaitForSingleObject(pi.hProcess, ms) != WAIT_OBJECT_0) return -1;
        DWORD code = 0;
        GetExitCodeProcess(pi.hProcess, &code);
        return static_cast<long>(code);
    }
    ~PeerWorker() {
        if (pi.hProcess) {
            TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, 5000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }
    PeerWorker(const PeerWorker&) = delete;
    PeerWorker& operator=(const PeerWorker&) = delete;
};

/** Connects to @p pipe; retries while it does not exist yet or is busy. INVALID_HANDLE_VALUE when it never opened. */
HANDLE ConnectTo(const std::string& pipe, int attempts = 400) {
    for (int i = 0; i < attempts; ++i) {
        HANDLE h = CreateFileA(pipe.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (h != INVALID_HANDLE_VALUE) return h;
        const DWORD e = GetLastError();
        if (e != ERROR_FILE_NOT_FOUND && e != ERROR_PIPE_BUSY) return INVALID_HANDLE_VALUE;
        Sleep(25);
    }
    return INVALID_HANDLE_VALUE;
}

XpeAiMessageHeader HeartbeatRequest(uint32_t id) {
    XpeAiMessageHeader h{};
    h.magic = XPE_AI_MSG_MAGIC;
    h.version = (static_cast<uint32_t>(XPE_AI_PROTOCOL_VERSION_MAJOR) << 16) | static_cast<uint32_t>(XPE_AI_PROTOCOL_VERSION_MINOR);
    h.messageType = XPE_AI_MSG_HEARTBEAT;
    h.requestId = id;
    return h;
}

/** Sends a heartbeat on a raw pipe handle; true when the worker answered with the matching ACK. */
bool HeartbeatAnswered(HANDLE h) {
    const XpeAiMessageHeader req = HeartbeatRequest(7);
    DWORD n = 0;
    if (!WriteFile(h, &req, sizeof(req), &n, nullptr)) return false;
    XpeAiMessageHeader rep{};
    if (!ReadFile(h, &rep, sizeof(rep), &n, nullptr) || n != sizeof(rep)) return false;
    return rep.messageType == XPE_AI_MSG_HEARTBEAT_ACK && rep.requestId == 7;
}

}  // namespace

/* ---------------------------------------------------------------------- the worker checks its client */

TEST(WorkerPipePeer, ControlTheWorkerAnswersTheHostItWasToldTo) {
    PeerWorker w(std::to_string(GetCurrentProcessId()));
    ASSERT_NE(nullptr, w.pi.hProcess);
    HANDLE c = ConnectTo(w.pipe);
    ASSERT_NE(INVALID_HANDLE_VALUE, c);
    EXPECT_TRUE(HeartbeatAnswered(c)) << "the positive control: with the right host id the worker answers";
    CloseHandle(c);
}

TEST(WorkerPipePeer, TheWorkerCutsOffAClientThatIsNotTheHostAndKeepsWaiting) {
    PeerWorker w(std::to_string(kNotThisProcess));   // it expects another process: this one is the impostor
    ASSERT_NE(nullptr, w.pi.hProcess);
    HANDLE c = ConnectTo(w.pipe);
    ASSERT_NE(INVALID_HANDLE_VALUE, c) << "an impostor of the same user passes the DACL and connects";
    EXPECT_FALSE(HeartbeatAnswered(c)) << "was: answered -- the worker talked to a process that is not the host";
    CloseHandle(c);
    EXPECT_TRUE(w.Running()) << "the worker waits for the real host instead of exiting on the first impostor";
}

TEST(WorkerPipePeer, AfterTooManyForeignClientsTheWorkerExitsInsteadOfServingThem) {
    PeerWorker w(std::to_string(kNotThisProcess));
    ASSERT_NE(nullptr, w.pi.hProcess);
    int connected = 0;
    for (int i = 0; i < 200 && w.Running(); ++i) {
        HANDLE c = ConnectTo(w.pipe, 8);
        if (c != INVALID_HANDLE_VALUE) {
            ++connected;
            CloseHandle(c);
        }
    }
    EXPECT_EQ(1, w.ExitCodeWithin(10000)) << "an attack of this shape ends as a worker that did not start";
    EXPECT_GE(connected, 17) << "the worker refused its bound of foreign clients before it exited";
}

TEST(WorkerPipePeer, AWorkerStartedInTheDiagnosticFormDoesNotCheck) {
    PeerWorker w("diag");   // started by hand: the diagnostics and protocol tests; a supervisor always passes the id
    ASSERT_NE(nullptr, w.pi.hProcess);
    HANDLE c = ConnectTo(w.pipe);
    ASSERT_NE(INVALID_HANDLE_VALUE, c);
    EXPECT_TRUE(HeartbeatAnswered(c));
    CloseHandle(c);
}

TEST(WorkerPipePeer, AnExtraArgumentEndsTheWorkerInBothForms) {
    // QA-B-198c: the supervised form is exactly `<pipe> <host-pid>`, the diagnostic form `--diagnostic [<pipe>]`. An extra
    // argument is not ignored (it used to be: a fourth argument was dropped silently).
    for (const char* hostArg : {"supervised", "diag"}) {
        const bool diag = std::string(hostArg) == "diag";
        PeerWorker w(diag ? "diag" : std::to_string(GetCurrentProcessId()), UniquePipeName(), "unexpected");
        ASSERT_NE(nullptr, w.pi.hProcess);
        EXPECT_EQ(2, w.ExitCodeWithin(10000)) << hostArg << " form with an extra argument";
    }
}

TEST(WorkerPipePeer, ThePipeOnlyFormAndNoArgumentAreRefused) {
    // `<pipe>` alone used to start a worker with no host check, by accident of the argument count. It is not a form now.
    for (const char* form : {"", "none"}) {
        PeerWorker w(form);
        ASSERT_NE(nullptr, w.pi.hProcess);
        EXPECT_EQ(2, w.ExitCodeWithin(10000)) << "form '" << form << "'";
    }
}

TEST(WorkerPipePeer, ABadHostIdEndsTheWorkerInsteadOfSwitchingTheCheckOff) {
    for (const char* bad : {"0", "abc", "12x", "-5"}) {
        PeerWorker w(bad);
        ASSERT_NE(nullptr, w.pi.hProcess);
        EXPECT_EQ(1, w.ExitCodeWithin(10000)) << "argument '" << bad << "'";
    }
}

TEST(WorkerPipePeer, AWorkerCannotTakeANameThatIsAlreadyServed) {
    // Measured (build/g198b/first_instance_out.txt): a squatter that allows ONE instance already makes the worker's own
    // creation fail (error 231), but a squatter that allows more (2 or 255) lets a worker WITHOUT FILE_FLAG_FIRST_PIPE_INSTANCE
    // create a second instance on the same name, quietly. Both squatters are tried; only the flag stops the second.
    for (const DWORD squatterMax : {1u, 255u}) {
        const std::string name = UniquePipeName();
        HANDLE squatter = CreateNamedPipeA(name.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
                                           squatterMax, 4096, 4096, 0, nullptr);
        ASSERT_NE(INVALID_HANDLE_VALUE, squatter) << "squatter max " << squatterMax;
        PeerWorker w(std::to_string(GetCurrentProcessId()), name);
        ASSERT_NE(nullptr, w.pi.hProcess);
        EXPECT_EQ(1, w.ExitCodeWithin(10000))
            << "squatter max " << squatterMax << " -- was: the worker became a second server on a name someone else serves";
        CloseHandle(squatter);
    }
}

/* ---------------------------------------------------------------------- the host checks its server */

TEST(WorkerPipePeer, ControlTheSupervisorStartsAWorkerWhoseProcessServesThePipe) {
    const EnvScope off("XPE_AI_TEST_WORKER_UNRESTRICTED", "1");
    const EnvScope mode("XPE_FAKE_WORKER_MODE", "ok");
    WorkerSupervisorConfig cfg;
    cfg.worker_exe = XPE_AI_FAKE_WORKER_EXE;
    cfg.model_dir = kData + "/models_x2";
    cfg.timeout_ms = kBudgetMs;
    WorkerSupervisor sup(cfg);
    EXPECT_EQ(XPE_OK, sup.Ping());
}

TEST(WorkerPipePeer, TheSupervisorRefusesAPipeServedByAProcessItDidNotStart) {
    const EnvScope off("XPE_AI_TEST_WORKER_UNRESTRICTED", "1");
    const EnvScope mode("XPE_FAKE_WORKER_MODE", "spoof_server");   // the child it starts hands the pipe to ANOTHER process
    WorkerSupervisorConfig cfg;
    cfg.worker_exe = XPE_AI_FAKE_WORKER_EXE;
    cfg.model_dir = kData + "/models_x2";
    cfg.timeout_ms = kBudgetMs;
    WorkerSupervisor sup(cfg);
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, sup.Ping()) << "was: XPE_OK -- the host talked to a process it never started";
    EXPECT_EQ(xpe::ai::WorkerExit::kKilled, sup.LastExit().kind) << "the child that fronted the impostor is ended";
}

/* ---------------------------------------------------------------------- the name and the identity level */

TEST(WorkerPipePeer, EveryPipeNameCarries128FreshRandomBits) {
    std::set<std::string> names, randoms;
    const std::string prefix = "\\\\.\\pipe\\xpe_ai_worker_" + std::to_string(GetCurrentProcessId()) + "_";
    for (int i = 0; i < 300; ++i) {
        const std::string n = xpe::ai::NewWorkerPipeName();
        ASSERT_EQ(0u, n.find(prefix)) << n;
        const std::string tail = n.substr(n.rfind('_') + 1);
        ASSERT_EQ(32u, tail.size()) << "128 bits as 32 hex digits: " << n;
        EXPECT_EQ(std::string::npos, tail.find_first_not_of("0123456789abcdef")) << n;
        names.insert(n);
        randoms.insert(tail);
    }
    EXPECT_EQ(300u, names.size());
    EXPECT_EQ(300u, randoms.size()) << "the random part alone differs from name to name (the serial is not what makes them unique)";
}

namespace {

/** A pipe server that reads one message, impersonates its client, and reports whether the impersonation token could be opened. */
struct ImpersonationProbe {
    std::string pipe = UniquePipeName();
    DWORD openThreadTokenError = 0;
    bool opened = false;
    bool impersonated = false;
    HANDLE server = INVALID_HANDLE_VALUE;
    std::thread t;
    ImpersonationProbe() {
        server = CreateNamedPipeA(pipe.c_str(), PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1, 4096,
                                  4096, 0, nullptr);
        t = std::thread([this] {
            if (!ConnectNamedPipe(server, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) return;
            XpeAiMessageHeader h{};
            DWORD n = 0;
            if (!ReadFile(server, &h, sizeof(h), &n, nullptr)) return;   // ImpersonateNamedPipeClient needs one read first
            if (!ImpersonateNamedPipeClient(server)) return;
            impersonated = true;
            HANDLE tok = nullptr;
            if (OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, FALSE, &tok)) {
                opened = true;
                CloseHandle(tok);
            } else {
                openThreadTokenError = GetLastError();
            }
            RevertToSelf();
        });
    }
    ~ImpersonationProbe() {
        if (server != INVALID_HANDLE_VALUE) {
            CancelIoEx(server, nullptr);
            DisconnectNamedPipe(server);
        }
        if (t.joinable()) t.join();
        if (server != INVALID_HANDLE_VALUE) CloseHandle(server);
    }
    ImpersonationProbe(const ImpersonationProbe&) = delete;
    ImpersonationProbe& operator=(const ImpersonationProbe&) = delete;
};

}  // namespace

TEST(WorkerPipePeer, ControlADefaultClientCanBeImpersonatedByTheServerItConnectsTo) {
    ImpersonationProbe probe;
    ASSERT_NE(INVALID_HANDLE_VALUE, probe.server);
    HANDLE c = ConnectTo(probe.pipe);   // no SQOS flags: the default level
    ASSERT_NE(INVALID_HANDLE_VALUE, c);
    const XpeAiMessageHeader req = HeartbeatRequest(1);
    DWORD n = 0;
    ASSERT_TRUE(WriteFile(c, &req, sizeof(req), &n, nullptr));
    probe.t.join();
    EXPECT_TRUE(probe.impersonated);
    EXPECT_TRUE(probe.opened) << "the control: at the default level the server gets the client's identity (error "
                              << probe.openThreadTokenError << ")";
    CloseHandle(c);
}

TEST(WorkerPipePeer, TheHostsBridgeConnectsAtTheAnonymousLevelSoAServerCannotActAsTheHost) {
    ImpersonationProbe probe;
    ASSERT_NE(INVALID_HANDLE_VALUE, probe.server);
    XpeAiIpcBridge* bridge = xpe_ai_ipc_bridge_create(probe.pipe.c_str(), 5000);
    ASSERT_NE(nullptr, bridge);
    ASSERT_EQ(XPE_OK, xpe_ai_ipc_bridge_connect(bridge));
    const XpeAiMessageHeader req = HeartbeatRequest(1);
    ASSERT_EQ(XPE_OK, xpe_ai_ipc_bridge_send(bridge, &req, nullptr, 0));
    probe.t.join();
    EXPECT_TRUE(probe.impersonated) << "impersonation itself succeeds; what the server gets is an anonymous token";
    EXPECT_FALSE(probe.opened) << "was: the server opened the host's identity";
    EXPECT_EQ(static_cast<DWORD>(ERROR_CANT_OPEN_ANONYMOUS), probe.openThreadTokenError);
    xpe_ai_ipc_bridge_destroy(bridge);
}
