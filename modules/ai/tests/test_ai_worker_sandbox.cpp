/**
 * @file test_ai_worker_sandbox.cpp
 * @brief REQ-AI-093 least privilege: what the worker process can NOT do, measured from inside it (QA-B-198 M2, #250).
 *
 * REQ-AI-093: "AI inference process shall run with minimum privilege (no network, no file write except sidecar scratch)."
 * What is held here, and what is NOT:
 *   - FILE and REGISTRY writes and CHILD PROCESSES are refused to a supervisor-started worker: its token is the host's,
 *     lowered to low integrity, and its job allows one process. A fake worker (tests/fake_worker_main.cpp, mode
 *     capability_probe_exit) TRIES each of them and exits with a code that says which ones worked -- a restricted worker
 *     cannot write a file to report with, so the exit code is the channel. The same probe with the restriction switched
 *     off (XPE_AI_TEST_WORKER_UNRESTRICTED=1, test builds only) is the positive control: it proves the probe can SEE a
 *     capability, so a refusal means refusal and not "the probe never worked".
 *   - THE NETWORK IS NOT RESTRICTED. A low-integrity process can still connect (QA-B-198 M1, measured). The test named
 *     KnownDivergence_ says so out loud: it asserts the connection WORKS, and it is the test to flip when an AppContainer
 *     (REQ-AI-093's "no network", tracked in #250) lands.
 *   - The REAL worker runs at low integrity (its token is read from outside) and still answers its requests.
 *   - The worker's pipe is open to the current user and SYSTEM only (it was Everyone + Anonymous read): the anonymous
 *     token is refused, and the pipe's DACL holds exactly two ACEs.
 */

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <aclapi.h>
#include <sddl.h>

#include <gtest/gtest.h>

#include "ai_worker_supervisor.h"
#include "xpe/ai/ai_onnx_session.h"
#include "xpe/common/xpe_error.h"

#include <cstdint>
#include <cstdio>
#include <set>
#include <string>
#include <thread>
#include <vector>

#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "advapi32.lib")

#ifndef XPE_AI_TEST_DATA_DIR
#error "XPE_AI_TEST_DATA_DIR must be defined by the build (modules/ai/CMakeLists.txt)"
#endif
#ifndef XPE_AI_WORKER_EXE
#error "XPE_AI_WORKER_EXE must be defined by the build (modules/ai/CMakeLists.txt)"
#endif
#ifndef XPE_AI_FAKE_WORKER_EXE
#error "XPE_AI_FAKE_WORKER_EXE must be defined by the build (modules/ai/CMakeLists.txt)"
#endif

namespace {

using xpe::ai::OnnxSession;
using xpe::ai::WorkerSupervisor;
using xpe::ai::WorkerSupervisorConfig;

const std::string kData = XPE_AI_TEST_DATA_DIR;
constexpr uint32_t kBudgetMs = 20000;
constexpr DWORD kProbeMarker = 0x100;   // fake_worker_main.cpp
constexpr DWORD kBitTemp = 1, kBitCwd = 2, kBitRegistry = 4, kBitChild = 8, kBitNetwork = 16;

struct EnvScope {
    const char* name;
    EnvScope(const char* n, const char* value) : name(n) { SetEnvironmentVariableA(name, value); }
    ~EnvScope() { SetEnvironmentVariableA(name, nullptr); }
    EnvScope(const EnvScope&) = delete;
    EnvScope& operator=(const EnvScope&) = delete;
};

/** A listening loopback socket: what the probe's "network" bit connects to. */
struct Listener {
    SOCKET s = INVALID_SOCKET;
    int port = 0;
    Listener() {
        WSADATA wd;
        WSAStartup(MAKEWORD(2, 2), &wd);
        s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        sockaddr_in a{};
        a.sin_family = AF_INET;
        inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
        bind(s, reinterpret_cast<sockaddr*>(&a), sizeof(a));
        listen(s, 8);
        int len = sizeof(a);
        getsockname(s, reinterpret_cast<sockaddr*>(&a), &len);
        port = ntohs(a.sin_port);
    }
    ~Listener() {
        if (s != INVALID_SOCKET) closesocket(s);
        WSACleanup();
    }
    Listener(const Listener&) = delete;
    Listener& operator=(const Listener&) = delete;
};

/**
 * What a worker started by the supervisor could still do, as the exit code of the fake worker's capability probe
 * (kProbeMarker | bits). 0 when no exit code was read.
 */
DWORD WhatTheWorkerCouldDo(bool unrestricted) {
    const Listener listener;
    const EnvScope mode("XPE_FAKE_WORKER_MODE", "capability_probe_exit");
    const EnvScope port("XPE_FAKE_WORKER_PORT", std::to_string(listener.port).c_str());
    const EnvScope off("XPE_AI_TEST_WORKER_UNRESTRICTED", unrestricted ? "1" : "0");
    WorkerSupervisorConfig cfg;
    cfg.worker_exe = XPE_AI_FAKE_WORKER_EXE;
    cfg.model_dir = kData + "/models_x2";
    cfg.timeout_ms = kBudgetMs;
    WorkerSupervisor sup(cfg);
    (void)sup.Ping();   // starts the worker (session start answered), which then probes itself and exits
    sup.Stop();         // records how it ended
    const auto exit = sup.LastExit();
    return exit.exit_code;
}

std::string CapabilityNames(DWORD code) {
    std::string s;
    if (code & kBitTemp) s += "write-temp ";
    if (code & kBitCwd) s += "write-cwd ";
    if (code & kBitRegistry) s += "write-registry ";
    if (code & kBitChild) s += "child-process ";
    if (code & kBitNetwork) s += "network ";
    return s.empty() ? "(none)" : s;
}

std::string TokenUserSid(HANDLE token) {
    DWORD n = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &n);
    std::vector<char> b(n);
    std::string out;
    if (GetTokenInformation(token, TokenUser, b.data(), n, &n)) {
        LPSTR s = nullptr;
        if (ConvertSidToStringSidA(reinterpret_cast<TOKEN_USER*>(b.data())->User.Sid, &s)) {
            out = s;
            LocalFree(s);
        }
    }
    return out;
}

/** The integrity level RID of a running process (0x1000 low, 0x2000 medium, 0x3000 high), 0 when it could not be read. */
DWORD IntegrityRidOfProcess(DWORD pid) {
    HANDLE p = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!p) return 0;
    HANDLE t = nullptr;
    DWORD rid = 0;
    if (OpenProcessToken(p, TOKEN_QUERY, &t)) {
        DWORD n = 0;
        GetTokenInformation(t, TokenIntegrityLevel, nullptr, 0, &n);
        std::vector<char> b(n);
        if (GetTokenInformation(t, TokenIntegrityLevel, b.data(), n, &n)) {
            PSID sid = reinterpret_cast<TOKEN_MANDATORY_LABEL*>(b.data())->Label.Sid;
            rid = *GetSidSubAuthority(sid, *GetSidSubAuthorityCount(sid) - 1);
        }
        CloseHandle(t);
    }
    CloseHandle(p);
    return rid;
}

/**
 * The integrity level RID of THIS process. It is read, never assumed: a developer's shell runs at medium (0x2000), but the
 * CI runner is an administrator and its processes run at high (0x3000) -- the test used to fix 0x2000 and failed there
 * (QA-B-202, #250). The restriction under test is relative to the host: the worker is made LOW whatever the host is, and
 * with the restriction switched off the worker is the HOST's own level.
 */
DWORD HostIntegrityRid() {
    const DWORD rid = IntegrityRidOfProcess(GetCurrentProcessId());
    std::printf("[ HOST     ] this test process runs at integrity level 0x%lX\n", static_cast<unsigned long>(rid));
    return rid;
}

}  // namespace

/* =========================================================================
 * What a restricted worker cannot do
 * ========================================================================= */

TEST(WorkerSandbox, ControlTheProbeSeesEveryCapabilityWhenTheRestrictionIsSwitchedOff) {
    // The positive control: without it, "refused" below could just be "the probe never ran".
    const DWORD code = WhatTheWorkerCouldDo(/*unrestricted=*/true);
    ASSERT_NE(0u, code) << "the fake worker's exit code was read";
    EXPECT_EQ(kProbeMarker, code & ~0x1Fu) << "an exit code of the probe (marker 0x100), got " << std::hex << code;
    EXPECT_EQ(kProbeMarker | kBitTemp | kBitCwd | kBitRegistry | kBitChild | kBitNetwork, code)
        << "unrestricted, the worker can: " << CapabilityNames(code);
}

TEST(WorkerSandbox, ARestrictedWorkerCannotWriteAFileOrARegistryKeyOrStartAProcess) {
    const DWORD code = WhatTheWorkerCouldDo(/*unrestricted=*/false);
    ASSERT_NE(0u, code) << "the fake worker's exit code was read";
    ASSERT_EQ(kProbeMarker, code & ~0x1Fu) << "an exit code of the probe (marker 0x100), got " << std::hex << code;
    EXPECT_EQ(0u, code & kBitTemp) << "a file in %TEMP%";
    EXPECT_EQ(0u, code & kBitCwd) << "a file in the working directory";
    EXPECT_EQ(0u, code & kBitRegistry) << "a registry key under HKCU";
    EXPECT_EQ(0u, code & kBitChild) << "a child process";
}

TEST(WorkerSandbox, KnownDivergence_TheRestrictedWorkerCanStillOpenAConnection) {
    // REQ-AI-093 says "no network". It is NOT met: a low-integrity process can connect. This test records the fact; when an
    // AppContainer is in place (#250) the connection is refused and THIS test is the one to turn around.
    const DWORD code = WhatTheWorkerCouldDo(/*unrestricted=*/false);
    ASSERT_EQ(kProbeMarker, code & ~0x1Fu) << std::hex << code;
    EXPECT_NE(0u, code & kBitNetwork) << "the restricted worker is no longer able to connect: update REQ-AI-093's status";
}

/* =========================================================================
 * The real worker: low integrity, and it still works
 * ========================================================================= */

TEST(WorkerSandbox, TheRealWorkerRunsAtLowIntegrityAndStillAnswersItsRequests) {
    const DWORD host = HostIntegrityRid();
    ASSERT_NE(0u, host) << "the integrity level of the test process was read";
    if (host <= 0x1000u) {
        GTEST_SKIP() << "the test process itself runs at low integrity (0x" << std::hex << host
                     << "): a low-integrity worker would not be a restriction relative to it";
    }
    WorkerSupervisorConfig cfg;
    cfg.worker_exe = XPE_AI_WORKER_EXE;
    cfg.model_dir = kData + "/models_x2";
    cfg.timeout_ms = kBudgetMs;
    WorkerSupervisor sup(cfg);
    ASSERT_EQ(XPE_OK, sup.Ping());
    const DWORD pid = sup.WorkerPid();
    ASSERT_NE(0u, pid);
    const DWORD worker = IntegrityRidOfProcess(pid);
    EXPECT_EQ(0x1000u, worker) << "the worker's own token, read from outside: low integrity";
    // this process, for comparison: the restriction is the worker's, not the test's -- whatever the host's level is
    EXPECT_LT(worker, host) << "the worker (0x" << std::hex << worker << ") runs below the test process (0x" << host << ")";
    EXPECT_EQ(host, IntegrityRidOfProcess(GetCurrentProcessId())) << "and the test process itself was not lowered";
    if (!OnnxSession::IsStubBuild()) {
        const std::vector<float> in(9, 1.0f);
        std::vector<float> out(9, -777.0f);
        EXPECT_EQ(XPE_OK, sup.BoneSuppress(3, 3, in.data(), out.data())) << "bone suppression, restricted";
        for (const float v : out) EXPECT_EQ(2.0f, v);
    }
}

TEST(WorkerSandbox, TheRealWorkerRecognisesABodyPartWhileRestricted) {
    if (OnnxSession::IsStubBuild()) GTEST_SKIP() << "stub build: no worker runs a model";
    WorkerSupervisorConfig cfg;
    cfg.worker_exe = XPE_AI_WORKER_EXE;
    cfg.model_dir = kData + "/models_bodypart_a";
    cfg.timeout_ms = kBudgetMs;
    WorkerSupervisor sup(cfg);
    const std::vector<float> px(16, 0.0f);
    xpe::ai::BodyPartReply reply{};
    EXPECT_EQ(XPE_OK, sup.BodyPartRecognize(4, 4, px.data(), &reply));
    EXPECT_STREQ("CHEST", reply.label);
    EXPECT_EQ(0x1000u, IntegrityRidOfProcess(sup.WorkerPid()));
}

TEST(WorkerSandbox, ControlWithTheRestrictionSwitchedOffTheRealWorkerRunsAtTheHostsIntegrity) {
    // Was "...AtMediumIntegrity" with 0x2000 fixed: the unrestricted worker is the host's own level, and the host is
    // medium in a developer shell but high on an administrator CI runner (QA-B-202).
    const DWORD host = HostIntegrityRid();
    ASSERT_NE(0u, host) << "the integrity level of the test process was read";
    const EnvScope off("XPE_AI_TEST_WORKER_UNRESTRICTED", "1");
    WorkerSupervisorConfig cfg;
    cfg.worker_exe = XPE_AI_WORKER_EXE;
    cfg.model_dir = kData + "/models_x2";
    cfg.timeout_ms = kBudgetMs;
    WorkerSupervisor sup(cfg);
    ASSERT_EQ(XPE_OK, sup.Ping());
    EXPECT_EQ(host, IntegrityRidOfProcess(sup.WorkerPid()))
        << "the control: the same read sees a worker at the host's level (0x" << std::hex << host << ")";
}

/* =========================================================================
 * The worker's pipe is open to the user and SYSTEM only (D5)
 * ========================================================================= */

namespace {

/** Starts the REAL worker directly (as the test's own user, medium integrity) on a pipe of our choosing. */
struct DirectWorker {
    PROCESS_INFORMATION pi{};
    std::string pipe;
    DirectWorker() {
        static int serial = 0;
        pipe = "\\\\.\\pipe\\xpe_ai_worker_sandbox_test_" + std::to_string(GetCurrentProcessId()) + "_" + std::to_string(serial++);
        std::string cmd = std::string("\"") + XPE_AI_WORKER_EXE + "\" --diagnostic " + pipe;
        STARTUPINFOA si{};
        si.cb = sizeof(si);
        if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) pi = {};
    }
    ~DirectWorker() {
        if (pi.hProcess) {
            TerminateProcess(pi.hProcess, 1);
            WaitForSingleObject(pi.hProcess, 5000);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
    }
    DirectWorker(const DirectWorker&) = delete;
    DirectWorker& operator=(const DirectWorker&) = delete;
};

/** Opens @p pipe for reading AS THE ANONYMOUS USER. Returns the error (0 when it opened). Retries while the pipe is not yet there. */
DWORD OpenPipeAsAnonymous(const std::string& pipe) {
    DWORD err = ERROR_FILE_NOT_FOUND;
    std::thread t([&] {
        if (!ImpersonateAnonymousToken(GetCurrentThread())) {
            err = GetLastError() ? GetLastError() : 1;
            return;
        }
        for (int i = 0; i < 200; ++i) {
            HANDLE h = CreateFileA(pipe.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, 0, nullptr);
            if (h != INVALID_HANDLE_VALUE) {
                err = 0;
                CloseHandle(h);
                break;
            }
            err = GetLastError();
            if (err != ERROR_FILE_NOT_FOUND) break;   // the pipe exists and said no (or something else): that is the answer
            Sleep(25);
        }
        RevertToSelf();
    });
    t.join();
    return err;
}

}  // namespace

TEST(WorkerPipeSecurity, AnAnonymousProcessCannotOpenTheWorkersPipe) {
    DirectWorker w;
    ASSERT_NE(nullptr, w.pi.hProcess) << "the worker started";
    EXPECT_EQ(static_cast<DWORD>(ERROR_ACCESS_DENIED), OpenPipeAsAnonymous(w.pipe))
        << "was: opened (the default pipe security grants Everyone and Anonymous read)";
}

TEST(WorkerPipeSecurity, TheHostThatStartedTheWorkerCanStillConnectAndTheDaclHoldsOnlyTheUserAndSystem) {
    DirectWorker w;
    ASSERT_NE(nullptr, w.pi.hProcess) << "the worker started";
    HANDLE client = INVALID_HANDLE_VALUE;
    DWORD lastError = 0;
    for (int i = 0; i < 200 && client == INVALID_HANDLE_VALUE; ++i) {
        client = CreateFileA(w.pipe.c_str(), GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (client == INVALID_HANDLE_VALUE) {
            lastError = GetLastError();
            if (lastError != ERROR_FILE_NOT_FOUND) break;
            Sleep(25);
        }
    }
    ASSERT_NE(INVALID_HANDLE_VALUE, client) << "the user's own process connects (error " << lastError << ")";

    PSECURITY_DESCRIPTOR sd = nullptr;
    PACL dacl = nullptr;
    ASSERT_EQ(static_cast<DWORD>(ERROR_SUCCESS),
              GetSecurityInfo(client, SE_KERNEL_OBJECT, DACL_SECURITY_INFORMATION, nullptr, nullptr, &dacl, nullptr, &sd));
    ASSERT_NE(nullptr, dacl) << "the pipe has a DACL (a NULL DACL would let everyone in)";
    ACL_SIZE_INFORMATION info{};
    ASSERT_TRUE(GetAclInformation(dacl, &info, sizeof(info), AclSizeInformation));
    std::multiset<std::string> sids;
    for (DWORD i = 0; i < info.AceCount; ++i) {
        void* ace = nullptr;
        ASSERT_TRUE(GetAce(dacl, i, &ace));
        auto* header = static_cast<ACE_HEADER*>(ace);
        EXPECT_EQ(ACCESS_ALLOWED_ACE_TYPE, header->AceType) << "only allow entries, no surprises";
        LPSTR s = nullptr;
        if (ConvertSidToStringSidA(reinterpret_cast<PSID>(&static_cast<ACCESS_ALLOWED_ACE*>(ace)->SidStart), &s)) {
            sids.insert(s);
            LocalFree(s);
        }
    }
    LocalFree(sd);
    CloseHandle(client);

    HANDLE own = nullptr;
    ASSERT_TRUE(OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &own));
    const std::string user = TokenUserSid(own);
    CloseHandle(own);
    EXPECT_EQ((std::multiset<std::string>{user, "S-1-5-18"}), sids)
        << "exactly the user and SYSTEM (was also Everyone S-1-1-0 and Anonymous S-1-5-7)";
    for (const char* never : {"S-1-1-0", "S-1-5-7", "S-1-5-11", "S-1-5-32-544", "S-1-5-32-545"}) {
        EXPECT_EQ(0u, sids.count(never)) << never << " must not be in the DACL";
    }
}
