/**
 * @file test_ipc_deadline.cpp
 * @brief The bridge's time budget against a worker that stops answering (QA-B-171, #130).
 *
 * WHY THIS FILE EXISTS. QA-B-170's five falsification arms (fixed value, echo,
 * cache, capability, error code) were all conditions under which the worker
 * ANSWERS. The independent audit (#2) found that the receive path has no time
 * limit -- timeout_ms bounded only the connect wait -- and that no arm injected
 * a worker that does not answer. This file injects exactly that.
 *
 * REQ-AI-092 (SRS-AI-SEC-002): "AI inference shall enforce a configurable time
 * budget (default 5 s). Exceeding the budget shall trigger fallback and alert."
 * The budget here is the bridge's own timeout_ms -- the configurable value
 * ai.cpp already parses and XPE_AI_DEFAULT_TIMEOUT_MS (5000) already defaults.
 * The tests use a SHORT budget so a pass takes well under a second; what they
 * assert is that the caller gets control back after about that long, not what
 * the number is.
 *
 * THE FAKE WORKER. A named-pipe server inside the test process, in the same
 * message mode the real worker uses. It lets a test choose a behaviour the real
 * worker never exhibits on purpose (stall, half a reply). The real worker is
 * used where the question is about the real process (exit vs death).
 *
 * THE TEST'S OWN WATCHDOG IS NOT THE PRODUCT'S DEFENCE. A call that does not
 * return is detected by the harness and released from outside, so a hang shows
 * up as a FAILURE with a number instead of a stuck suite. Passing because that
 * watchdog fired would hide the defect inside the test forever, so a released
 * call is reported as "hung" and fails.
 */

#include <gtest/gtest.h>

#include "xpe/ai/ai_worker_protocol.h"
#include "xpe/common/xpe_error.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <string>
#include <thread>
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
XpeErrorCode    xpe_ai_ipc_bridge_bone_suppress(XpeAiIpcBridge* bridge, uint32_t width,
                                                uint32_t height, const float* pixels_in,
                                                float* pixels_out);
void            xpe_ai_ipc_bridge_destroy(XpeAiIpcBridge* bridge);
}

#ifndef XPE_AI_WORKER_EXE
#error "XPE_AI_WORKER_EXE must be defined by the build (modules/ai/CMakeLists.txt)"
#endif

namespace {

/** The bridge's configured budget in these tests. Short on purpose. */
constexpr uint32_t kBudgetMs = 400;
/** A call still blocked this long has no deadline; the harness frees it. */
constexpr DWORD kHangDetectMs = 4000;
/** Slack for scheduling and process noise on a loaded machine. */
constexpr DWORD kSlackMs = 1500;

std::string UniquePipe(const char* tag) {
    static std::atomic<unsigned> n{0};
    char buf[128];
    std::snprintf(buf, sizeof(buf), "\\\\.\\pipe\\xpe_ai_dl_%s_%lu_%u", tag,
                  static_cast<unsigned long>(GetCurrentProcessId()), n.fetch_add(1));
    return buf;
}

XpeAiMessageHeader Header(uint32_t type, uint32_t request_id, uint32_t payload_size = 0,
                          uint32_t flags = 0) {
    XpeAiMessageHeader h{};
    h.magic = XPE_AI_MSG_MAGIC;
    h.version = (XPE_AI_PROTOCOL_VERSION_MAJOR << 16) | XPE_AI_PROTOCOL_VERSION_MINOR;
    h.messageType = type;
    h.requestId = request_id;
    h.payloadSize = payload_size;
    h.flags = flags;
    return h;
}

/**
 * A pipe server in the test process. The behaviour runs on its own thread after
 * a client connects. It always reads the whole request first, so the CLIENT's
 * writes complete and the question is only about the reply.
 */
class FakeWorker {
public:
    using Behaviour = std::function<void(FakeWorker&)>;

    explicit FakeWorker(Behaviour behaviour) : pipe_(UniquePipe("fake")) {
        release_ = CreateEventA(nullptr, TRUE, FALSE, nullptr);
        handle_ = CreateNamedPipeA(pipe_.c_str(), PIPE_ACCESS_DUPLEX,
                                   PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1,
                                   XPE_AI_PIPE_BUFFER_SIZE, XPE_AI_PIPE_BUFFER_SIZE, 0, nullptr);
        if (handle_ == INVALID_HANDLE_VALUE) return;
        thread_ = std::thread([this, behaviour] {
            if (!ConnectNamedPipe(handle_, nullptr) && GetLastError() != ERROR_PIPE_CONNECTED) {
                return;
            }
            behaviour(*this);
        });
    }

    ~FakeWorker() {
        Release();
        if (thread_.joinable()) thread_.join();
        if (handle_ != INVALID_HANDLE_VALUE) {
            DisconnectNamedPipe(handle_);
            CloseHandle(handle_);
        }
        if (release_) CloseHandle(release_);
    }
    FakeWorker(const FakeWorker&) = delete;
    FakeWorker& operator=(const FakeWorker&) = delete;

    bool ok() const { return handle_ != INVALID_HANDLE_VALUE; }
    const std::string& pipe() const { return pipe_; }

    /** Wake a behaviour that is waiting, and cut the pipe so a blocked client
     *  read fails instead of waiting for ever. Harness use only. */
    void Release() {
        if (release_) SetEvent(release_);
    }
    void CutPipe() {
        if (handle_ != INVALID_HANDLE_VALUE) DisconnectNamedPipe(handle_);
    }

    /** Read one request: header, then its payload. */
    bool ReadRequest(XpeAiMessageHeader& h, std::vector<char>& payload) {
        DWORD got = 0;
        if (!ReadFile(handle_, &h, sizeof(h), &got, nullptr) || got != sizeof(h)) return false;
        payload.assign(h.payloadSize, 0);
        if (h.payloadSize > 0) {
            if (!ReadFile(handle_, payload.data(), h.payloadSize, &got, nullptr) ||
                got != h.payloadSize) {
                return false;
            }
        }
        return true;
    }
    bool Write(const void* p, DWORD n) {
        DWORD w = 0;
        return WriteFile(handle_, p, n, &w, nullptr) && w == n;
    }
    /** Wait until released (the test finished) or @p ms passed. */
    void WaitReleased(DWORD ms) { WaitForSingleObject(release_, ms); }

private:
    std::string pipe_;
    HANDLE handle_ = INVALID_HANDLE_VALUE;
    HANDLE release_ = nullptr;
    std::thread thread_;
};

/** One bridge call on a worker thread, so a hang is a measurement, not a stuck suite. */
struct Outcome {
    bool hung = false;
    XpeErrorCode rc = XPE_OK;
    DWORD elapsed_ms = 0;
};

Outcome RunBoundedFreeing(const std::function<void()>& free_it,
                          const std::function<XpeErrorCode()>& call) {
    std::atomic<bool> done{false};
    Outcome out;
    const DWORD t0 = GetTickCount();
    std::thread t([&] {
        out.rc = call();
        out.elapsed_ms = GetTickCount() - t0;
        done = true;
    });
    while (!done && GetTickCount() - t0 < kHangDetectMs) Sleep(10);
    if (!done) {
        out.hung = true;
        // Free it from OUTSIDE the product: this is the harness, not a defence.
        free_it();
    }
    t.join();
    if (out.hung) out.elapsed_ms = GetTickCount() - t0;
    return out;
}

Outcome RunBounded(FakeWorker& fw, const std::function<XpeErrorCode()>& call) {
    return RunBoundedFreeing([&fw] { fw.Release(); fw.CutPipe(); }, call);
}

/** A connected bridge to @p fw with the given budget. Freed on every exit path. */
struct Client {
    XpeAiIpcBridge* b = nullptr;
    Client(FakeWorker& fw, uint32_t budget_ms) {
        b = xpe_ai_ipc_bridge_create(fw.pipe().c_str(), budget_ms);
        if (!b) return;
        for (int i = 0; i < 40; ++i) {
            if (xpe_ai_ipc_bridge_connect(b) == XPE_OK) return;
            Sleep(25);
        }
        xpe_ai_ipc_bridge_destroy(b);
        b = nullptr;
    }
    ~Client() { if (b) xpe_ai_ipc_bridge_destroy(b); }
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
};

std::string Describe(const Outcome& o) {
    char buf[160];
    std::snprintf(buf, sizeof(buf), "hung=%d rc=%d elapsed=%lu ms (budget %u ms)", o.hung ? 1 : 0,
                  static_cast<int>(o.rc), static_cast<unsigned long>(o.elapsed_ms), kBudgetMs);
    return buf;
}

/** A behaviour that takes the request and then says nothing. */
void Stall(FakeWorker& fw) {
    XpeAiMessageHeader h;
    std::vector<char> p;
    fw.ReadRequest(h, p);
    fw.WaitReleased(30000);
}

}  // namespace

// --- control: the harness can tell an answer from silence ---------------------

TEST(IpcDeadline, ControlAWorkerThatAnswersIsAnsweredWithinTheBudget) {
    FakeWorker fw([](FakeWorker& w) {
        XpeAiMessageHeader h;
        std::vector<char> p;
        if (!w.ReadRequest(h, p)) return;
        XpeAiMessageHeader r = Header(XPE_AI_MSG_HEARTBEAT_ACK, h.requestId);
        w.Write(&r, sizeof(r));
        w.WaitReleased(30000);
    });
    ASSERT_TRUE(fw.ok());
    Client c(fw, kBudgetMs);
    ASSERT_NE(nullptr, c.b) << "bridge never connected";
    XpeAiMessageHeader req = Header(XPE_AI_MSG_HEARTBEAT, 7);
    ASSERT_EQ(XPE_OK, xpe_ai_ipc_bridge_send(c.b, &req, nullptr, 0));
    XpeAiMessageHeader rep{};
    uint8_t buf[64];
    uint32_t got = 0;
    const Outcome o = RunBounded(fw, [&] {
        return xpe_ai_ipc_bridge_receive(c.b, &rep, buf, sizeof(buf), &got);
    });
    EXPECT_FALSE(o.hung) << Describe(o);
    EXPECT_EQ(XPE_OK, o.rc) << Describe(o);
    EXPECT_EQ(7u, rep.requestId);
}

// --- arm 1: stop (the worker takes the request and never answers) --------------

TEST(IpcDeadline, AWorkerThatNeverAnswersReturnsControlAfterTheBudget) {
    FakeWorker fw(Stall);
    ASSERT_TRUE(fw.ok());
    Client c(fw, kBudgetMs);
    ASSERT_NE(nullptr, c.b) << "bridge never connected";
    XpeAiMessageHeader req = Header(XPE_AI_MSG_HEARTBEAT, 1);
    ASSERT_EQ(XPE_OK, xpe_ai_ipc_bridge_send(c.b, &req, nullptr, 0));

    XpeAiMessageHeader rep{};
    uint8_t buf[64];
    uint32_t got = 0;
    const Outcome o = RunBounded(fw, [&] {
        return xpe_ai_ipc_bridge_receive(c.b, &rep, buf, sizeof(buf), &got);
    });
    EXPECT_FALSE(o.hung) << "receive has no time limit: " << Describe(o);
    // PROCESSING_FAILED is the documented fallback signal (REQ-AI-002) and what
    // the bridge already returned for "no worker connection".
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, o.rc) << Describe(o);
    // Not instant: returning at once would mean it never waited for the worker.
    EXPECT_GE(o.elapsed_ms, kBudgetMs - 100) << Describe(o);
    EXPECT_LE(o.elapsed_ms, kBudgetMs + kSlackMs) << Describe(o);
}

TEST(IpcDeadline, TheBudgetIsTheConfiguredValueNotAConstant) {
    // Two bridges, two budgets: the longer one must wait measurably longer. A
    // hard-coded timeout would make both the same.
    auto elapsed_for = [](uint32_t budget) {
        FakeWorker fw(Stall);
        EXPECT_TRUE(fw.ok());
        Client c(fw, budget);
        EXPECT_NE(nullptr, c.b);
        if (!c.b) return DWORD(0);
        XpeAiMessageHeader req = Header(XPE_AI_MSG_HEARTBEAT, 1);
        xpe_ai_ipc_bridge_send(c.b, &req, nullptr, 0);
        XpeAiMessageHeader rep{};
        uint8_t buf[64];
        uint32_t got = 0;
        const Outcome o = RunBounded(fw, [&] {
            return xpe_ai_ipc_bridge_receive(c.b, &rep, buf, sizeof(buf), &got);
        });
        return o.hung ? DWORD(kHangDetectMs) : o.elapsed_ms;
    };
    const DWORD shortMs = elapsed_for(300);
    const DWORD longMs = elapsed_for(1200);
    EXPECT_GE(longMs, shortMs + 500) << "300 ms budget took " << shortMs << " ms, 1200 ms budget took "
                                     << longMs << " ms";
}

TEST(IpcDeadline, BoneSuppressWithAStalledWorkerFailsAfterTheBudgetAndLeavesTheOutputAlone) {
    FakeWorker fw(Stall);
    ASSERT_TRUE(fw.ok());
    Client c(fw, kBudgetMs);
    ASSERT_NE(nullptr, c.b);
    const float in[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    float out[9];
    for (float& v : out) v = -777.0f;   // sentinel: untouched means the call did not "succeed"
    const Outcome o = RunBounded(fw, [&] {
        return xpe_ai_ipc_bridge_bone_suppress(c.b, 3, 3, in, out);
    });
    EXPECT_FALSE(o.hung) << Describe(o);
    EXPECT_NE(XPE_OK, o.rc) << Describe(o);
    EXPECT_LE(o.elapsed_ms, kBudgetMs + kSlackMs) << Describe(o);
    for (float v : out) EXPECT_EQ(-777.0f, v) << "output was written although the call failed";
}

// --- arm 2: interruption (half a reply, then silence or a closed pipe) ----------

namespace {
/** Header promising @p promised payload bytes, then only @p sent of them. */
void HalfReply(FakeWorker& w, uint32_t promised, uint32_t sent, bool then_stall) {
    XpeAiMessageHeader h;
    std::vector<char> p;
    if (!w.ReadRequest(h, p)) return;
    XpeAiMessageHeader r = Header(XPE_AI_MSG_BONE_SUPPRESS_RESP, h.requestId, promised,
                                  XPE_AI_FLAG_HAS_BINARY_PAYLOAD);
    w.Write(&r, sizeof(r));
    std::vector<char> half(sent, 1);
    if (sent > 0) w.Write(half.data(), sent);
    if (then_stall) w.WaitReleased(30000);
}
}  // namespace

TEST(IpcDeadline, AReplyThatStopsHalfWayAndStallsIsNotCountedAsSuccess) {
    FakeWorker fw([](FakeWorker& w) { HalfReply(w, 4 + 40 + 36, 20, true); });
    ASSERT_TRUE(fw.ok());
    Client c(fw, kBudgetMs);
    ASSERT_NE(nullptr, c.b);
    const float in[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    float out[9];
    for (float& v : out) v = -777.0f;
    const Outcome o = RunBounded(fw, [&] {
        return xpe_ai_ipc_bridge_bone_suppress(c.b, 3, 3, in, out);
    });
    EXPECT_FALSE(o.hung) << Describe(o);
    EXPECT_NE(XPE_OK, o.rc) << Describe(o);
    EXPECT_LE(o.elapsed_ms, kBudgetMs + kSlackMs) << Describe(o);
    for (float v : out) EXPECT_EQ(-777.0f, v) << "a partial reply was copied into the output";
}

TEST(IpcDeadline, AReplyThatStopsHalfWayAndClosesIsNotCountedAsSuccess) {
    FakeWorker fw([](FakeWorker& w) {
        HalfReply(w, 4 + 40 + 36, 20, false);
        w.CutPipe();
    });
    ASSERT_TRUE(fw.ok());
    Client c(fw, kBudgetMs);
    ASSERT_NE(nullptr, c.b);
    const float in[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    float out[9];
    for (float& v : out) v = -777.0f;
    const Outcome o = RunBounded(fw, [&] {
        return xpe_ai_ipc_bridge_bone_suppress(c.b, 3, 3, in, out);
    });
    EXPECT_FALSE(o.hung) << Describe(o);
    EXPECT_NE(XPE_OK, o.rc) << Describe(o);
    for (float v : out) EXPECT_EQ(-777.0f, v) << "a partial reply was copied into the output";
}

// --- arms 3 and 4: the REAL worker process dies, or exits on request -----------
//
// The card asks whether a normal exit can be told from a death by the pipe
// closing. These tests record what the bridge and the process handle each say
// in the two cases. They assert the MEASURED relationship (the pipe-visible
// result is the same; the exit code differs), because adding SHUTDOWN_ACK to
// the header is only justified by what this shows -- see the QA-B-171 report.

namespace {

/** A real worker process plus a connected bridge; freed on every exit path. */
struct RealWorker {
    std::string pipe = UniquePipe("real");
    PROCESS_INFORMATION pi{};
    bool launched = false;
    XpeAiIpcBridge* bridge = nullptr;

    RealWorker() {
        std::string cmd = std::string("\"") + XPE_AI_WORKER_EXE + "\" " + pipe;
        STARTUPINFOA si{};
        si.cb = sizeof(si);
        launched = CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW,
                                  nullptr, nullptr, &si, &pi) != 0;
        if (!launched) return;
        bridge = xpe_ai_ipc_bridge_create(pipe.c_str(), kBudgetMs);
        for (int i = 0; bridge && i < 60; ++i) {
            if (xpe_ai_ipc_bridge_connect(bridge) == XPE_OK) return;
            Sleep(50);
        }
    }
    ~RealWorker() {
        if (bridge) xpe_ai_ipc_bridge_destroy(bridge);
        if (launched) {
            TerminateProcess(pi.hProcess, 1);   // a no-op if it already exited
            WaitForSingleObject(pi.hProcess, 2000);
            CloseHandle(pi.hThread);
            CloseHandle(pi.hProcess);
        }
    }
    RealWorker(const RealWorker&) = delete;
    RealWorker& operator=(const RealWorker&) = delete;

    /** Exit code, or -1 if still running after @p wait_ms. */
    long ExitCode(DWORD wait_ms) {
        if (WaitForSingleObject(pi.hProcess, wait_ms) != WAIT_OBJECT_0) return -1;
        DWORD code = 0;
        GetExitCodeProcess(pi.hProcess, &code);
        return static_cast<long>(code);
    }
};

struct PipeView {
    XpeErrorCode rc;
    bool hung;
};

PipeView ReceiveAfter(RealWorker& w) {
    XpeAiMessageHeader rep{};
    uint8_t buf[64];
    uint32_t got = 0;
    const Outcome o = RunBoundedFreeing(
        [&w] { TerminateProcess(w.pi.hProcess, 3); },
        [&] { return xpe_ai_ipc_bridge_receive(w.bridge, &rep, buf, sizeof(buf), &got); });
    return {o.rc, o.hung};
}

}  // namespace

TEST(IpcDeadline, ControlTheRealWorkerAnswersAHeartbeatBeforeAnyKill) {
    RealWorker w;
    ASSERT_TRUE(w.launched);
    ASSERT_NE(nullptr, w.bridge) << "bridge never connected";
    XpeAiMessageHeader req = Header(XPE_AI_MSG_HEARTBEAT, 11);
    ASSERT_EQ(XPE_OK, xpe_ai_ipc_bridge_send(w.bridge, &req, nullptr, 0));
    XpeAiMessageHeader rep{};
    uint8_t buf[64];
    uint32_t got = 0;
    ASSERT_EQ(XPE_OK, xpe_ai_ipc_bridge_receive(w.bridge, &rep, buf, sizeof(buf), &got));
    EXPECT_EQ(static_cast<uint32_t>(XPE_AI_MSG_HEARTBEAT_ACK), rep.messageType);
    EXPECT_EQ(-1, w.ExitCode(0)) << "the worker is expected to be running";
}

TEST(IpcDeadline, ADeadWorkerIsReportedPromptlyNotAwaitedForever) {
    RealWorker w;
    ASSERT_TRUE(w.launched);
    ASSERT_NE(nullptr, w.bridge);
    TerminateProcess(w.pi.hProcess, 99);
    ASSERT_EQ(99, w.ExitCode(2000)) << "the kill did not take";
    const PipeView v = ReceiveAfter(w);
    EXPECT_FALSE(v.hung);
    EXPECT_NE(XPE_OK, v.rc);
}

TEST(IpcDeadline, PipeClosureAloneDoesNotSeparateAnExitOnRequestFromADeath) {
    // Arm "exit on request": ask the worker to shut down, then read.
    PipeView exited{};
    long exited_code = -2;
    {
        RealWorker w;
        ASSERT_TRUE(w.launched);
        ASSERT_NE(nullptr, w.bridge);
        XpeAiMessageHeader req = Header(XPE_AI_MSG_SHUTDOWN, 21);
        ASSERT_EQ(XPE_OK, xpe_ai_ipc_bridge_send(w.bridge, &req, nullptr, 0));
        exited_code = w.ExitCode(3000);
        exited = ReceiveAfter(w);
    }
    // Arm "death": kill it, then read.
    PipeView died{};
    long died_code = -2;
    {
        RealWorker w;
        ASSERT_TRUE(w.launched);
        ASSERT_NE(nullptr, w.bridge);
        TerminateProcess(w.pi.hProcess, 99);
        died_code = w.ExitCode(2000);
        died = ReceiveAfter(w);
    }
    std::printf("[measure] exit-on-request: bridge rc=%d hung=%d process exit code=%ld\n"
                "[measure] death:           bridge rc=%d hung=%d process exit code=%ld\n",
                static_cast<int>(exited.rc), exited.hung, exited_code,
                static_cast<int>(died.rc), died.hung, died_code);
    // What the pipe says is the same in both cases...
    EXPECT_EQ(exited.rc, died.rc)
        << "if these differ, the pipe alone CAN tell them apart and SHUTDOWN_ACK has no basis";
    // ...what the process handle says is not: that is the signal that exists today.
    EXPECT_EQ(0, exited_code) << "a requested shutdown should exit 0";
    EXPECT_EQ(99, died_code);
}

// --- the converse: slow is not the same as dead ----------------------------------
//
// Reading until the whole payload has arrived (instead of trusting one ReadFile
// to return it all) also has to keep a LARGE healthy reply working. 1024x1024
// float32 is 4 MiB -- sixty-four times the pipe buffer -- delivered in pieces
// with small gaps, well inside a generous budget.

namespace {
// Namespace scope, not locals: the fake worker's lambda below reads them, and a
// lambda with no capture cannot use a constexpr local (MSVC C3493).
constexpr uint32_t W = 1024, H = 1024;
constexpr size_t kBytes = size_t(W) * H * sizeof(float);
}  // namespace

TEST(IpcDeadline, ALargeReplyThatArrivesInPiecesIsReadWhole) {
    FakeWorker fw([](FakeWorker& w) {
        XpeAiMessageHeader h;
        std::vector<char> req;
        if (!w.ReadRequest(h, req)) return;
        // Echo: reply payload = length prefix + JSON + the request's own pixels.
        const char json[] = "{\"success\":true,\"width\":1024,\"height\":1024,\"format\":\"float32\"}";
        const uint32_t jn = static_cast<uint32_t>(sizeof(json) - 1);
        uint32_t reqjson = 0;
        std::memcpy(&reqjson, req.data(), sizeof(reqjson));
        const char* pixels = req.data() + sizeof(uint32_t) + reqjson;
        std::vector<char> reply(sizeof(uint32_t) + jn + kBytes);
        std::memcpy(reply.data(), &jn, sizeof(jn));
        std::memcpy(reply.data() + sizeof(jn), json, jn);
        std::memcpy(reply.data() + sizeof(jn) + jn, pixels, kBytes);
        XpeAiMessageHeader r = Header(XPE_AI_MSG_BONE_SUPPRESS_RESP, h.requestId,
                                      static_cast<uint32_t>(reply.size()),
                                      XPE_AI_FLAG_HAS_BINARY_PAYLOAD);
        w.Write(&r, sizeof(r));
        // In pieces: a message-mode pipe delivers each write as its own message.
        for (size_t off = 0; off < reply.size(); off += 65536) {
            const DWORD n = static_cast<DWORD>(std::min<size_t>(65536, reply.size() - off));
            if (!w.Write(reply.data() + off, n)) return;
            Sleep(1);
        }
        w.WaitReleased(30000);
    });
    ASSERT_TRUE(fw.ok());
    Client c(fw, 10000);
    ASSERT_NE(nullptr, c.b);
    std::vector<float> in(size_t(W) * H);
    for (size_t i = 0; i < in.size(); ++i) in[i] = static_cast<float>(i % 251);
    std::vector<float> out(in.size(), -777.0f);
    const Outcome o = RunBounded(fw, [&] {
        return xpe_ai_ipc_bridge_bone_suppress(c.b, W, H, in.data(), out.data());
    });
    ASSERT_FALSE(o.hung) << "hung=" << o.hung;
    EXPECT_EQ(XPE_OK, o.rc) << "a healthy 4 MiB reply was refused, rc=" << o.rc;
    EXPECT_EQ(0, std::memcmp(in.data(), out.data(), kBytes)) << "pixels did not arrive intact";
}

// --- Codex audit #10: a connection that cannot be trusted is dropped ------------------------
//
// A frame consumed in part, a wrong magic, a reply to someone else's request, or
// a reply of the wrong shape leaves the byte stream at a position the bridge can
// no longer know. The next call would read whatever follows as a header. Every
// such path must drop the connection, so the second call fails loudly as "not
// connected". A worker's own ERROR frame is the opposite case: a complete,
// well-formed answer, after which the connection is fine -- the control below
// keeps the fix from becoming "drop on every error".

namespace {

/** Header (with a chosen magic) plus @p body in one go. */
void ReplyFrame(FakeWorker& w, uint32_t type, uint32_t request_id, uint32_t flags,
                const std::vector<char>& body, uint32_t magic = XPE_AI_MSG_MAGIC) {
    XpeAiMessageHeader r = Header(type, request_id, static_cast<uint32_t>(body.size()), flags);
    r.magic = magic;
    w.Write(&r, sizeof(r));
    if (!body.empty()) w.Write(body.data(), static_cast<DWORD>(body.size()));
}

/** A well-formed 3x3 BONE_SUPPRESS_RESP body: length prefix, JSON, 36 pixel bytes. */
std::vector<char> GoodBoneBody(size_t pixel_bytes = 36) {
    const char json[] = "{\"success\":true,\"width\":3,\"height\":3,\"format\":\"float32\"}";
    const uint32_t jn = static_cast<uint32_t>(sizeof(json) - 1);
    std::vector<char> b(sizeof(uint32_t) + jn + pixel_bytes, 0);
    std::memcpy(b.data(), &jn, sizeof(jn));
    std::memcpy(b.data() + sizeof(jn), json, jn);
    return b;
}

/** Serve one request with @p reply, then keep reading so later sends complete. */
FakeWorker::Behaviour ServeOnce(std::function<void(FakeWorker&, uint32_t)> reply) {
    return [reply](FakeWorker& w) {
        XpeAiMessageHeader h;
        std::vector<char> p;
        if (!w.ReadRequest(h, p)) return;
        reply(w, h.requestId);
        while (w.ReadRequest(h, p)) {}   // a second request must be ACCEPTED by the server
    };
}

/** After the first call, is the bridge still usable for a plain send? */
XpeErrorCode SecondSend(XpeAiIpcBridge* b) {
    XpeAiMessageHeader req = Header(XPE_AI_MSG_HEARTBEAT, 99);
    return xpe_ai_ipc_bridge_send(b, &req, nullptr, 0);
}

const float kIn3x3[9] = {1, 2, 3, 4, 5, 6, 7, 8, 9};

}  // namespace

TEST(IpcDeadline, ABodyLongerThanTheBufferDropsTheConnection) {
    FakeWorker fw(ServeOnce([](FakeWorker& w, uint32_t id) {
        ReplyFrame(w, XPE_AI_MSG_HEARTBEAT_ACK, id, 0, std::vector<char>(200, 7));
    }));
    ASSERT_TRUE(fw.ok());
    Client c(fw, 2000);
    ASSERT_NE(nullptr, c.b);
    XpeAiMessageHeader req = Header(XPE_AI_MSG_HEARTBEAT, 1);
    ASSERT_EQ(XPE_OK, xpe_ai_ipc_bridge_send(c.b, &req, nullptr, 0));
    XpeAiMessageHeader rep{};
    uint8_t tiny[64];
    uint32_t got = 0;
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL,
              xpe_ai_ipc_bridge_receive(c.b, &rep, tiny, sizeof(tiny), &got));
    // The header was consumed and 200 body bytes are still in the pipe: the next
    // receive would read them as a header.
    EXPECT_EQ(XPE_ERR_NOT_INITIALIZED, SecondSend(c.b))
        << "the connection survived a half-consumed frame";
}

TEST(IpcDeadline, ABodyWithNoBufferAtAllDropsTheConnection) {
    FakeWorker fw(ServeOnce([](FakeWorker& w, uint32_t id) {
        ReplyFrame(w, XPE_AI_MSG_HEARTBEAT_ACK, id, 0, std::vector<char>(16, 7));
    }));
    ASSERT_TRUE(fw.ok());
    Client c(fw, 2000);
    ASSERT_NE(nullptr, c.b);
    XpeAiMessageHeader req = Header(XPE_AI_MSG_HEARTBEAT, 1);
    ASSERT_EQ(XPE_OK, xpe_ai_ipc_bridge_send(c.b, &req, nullptr, 0));
    XpeAiMessageHeader rep{};
    uint32_t got = 0;
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL, xpe_ai_ipc_bridge_receive(c.b, &rep, nullptr, 0, &got));
    EXPECT_EQ(XPE_ERR_NOT_INITIALIZED, SecondSend(c.b));
}

TEST(IpcDeadline, AWrongMagicDropsTheConnection) {
    FakeWorker fw(ServeOnce([](FakeWorker& w, uint32_t id) {
        ReplyFrame(w, XPE_AI_MSG_HEARTBEAT_ACK, id, 0, {}, 0xDEADBEEFu);
    }));
    ASSERT_TRUE(fw.ok());
    Client c(fw, 2000);
    ASSERT_NE(nullptr, c.b);
    XpeAiMessageHeader req = Header(XPE_AI_MSG_HEARTBEAT, 1);
    ASSERT_EQ(XPE_OK, xpe_ai_ipc_bridge_send(c.b, &req, nullptr, 0));
    XpeAiMessageHeader rep{};
    uint8_t buf[64];
    uint32_t got = 0;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_ai_ipc_bridge_receive(c.b, &rep, buf, sizeof(buf), &got));
    EXPECT_EQ(XPE_ERR_NOT_INITIALIZED, SecondSend(c.b));
}

TEST(IpcDeadline, AReplyToSomeoneElsesRequestDropsTheConnection) {
    FakeWorker fw(ServeOnce([](FakeWorker& w, uint32_t id) {
        ReplyFrame(w, XPE_AI_MSG_BONE_SUPPRESS_RESP, id + 1000, XPE_AI_FLAG_HAS_BINARY_PAYLOAD,
                   GoodBoneBody());
    }));
    ASSERT_TRUE(fw.ok());
    Client c(fw, 2000);
    ASSERT_NE(nullptr, c.b);
    float out[9];
    EXPECT_EQ(XPE_ERR_IO_FAILED, xpe_ai_ipc_bridge_bone_suppress(c.b, 3, 3, kIn3x3, out));
    EXPECT_EQ(XPE_ERR_NOT_INITIALIZED, SecondSend(c.b));
}

TEST(IpcDeadline, AReplyOfTheWrongTypeDropsTheConnection) {
    FakeWorker fw(ServeOnce([](FakeWorker& w, uint32_t id) {
        ReplyFrame(w, XPE_AI_MSG_HEARTBEAT_ACK, id, XPE_AI_FLAG_HAS_BINARY_PAYLOAD, GoodBoneBody());
    }));
    ASSERT_TRUE(fw.ok());
    Client c(fw, 2000);
    ASSERT_NE(nullptr, c.b);
    float out[9];
    EXPECT_EQ(XPE_ERR_IO_FAILED, xpe_ai_ipc_bridge_bone_suppress(c.b, 3, 3, kIn3x3, out));
    EXPECT_EQ(XPE_ERR_NOT_INITIALIZED, SecondSend(c.b));
}

TEST(IpcDeadline, AReplyWithTheWrongPixelLengthDropsTheConnection) {
    FakeWorker fw(ServeOnce([](FakeWorker& w, uint32_t id) {
        ReplyFrame(w, XPE_AI_MSG_BONE_SUPPRESS_RESP, id, XPE_AI_FLAG_HAS_BINARY_PAYLOAD,
                   GoodBoneBody(20));   // 20 pixel bytes where 3x3 float32 needs 36
    }));
    ASSERT_TRUE(fw.ok());
    Client c(fw, 2000);
    ASSERT_NE(nullptr, c.b);
    float out[9];
    for (float& v : out) v = -777.0f;
    EXPECT_EQ(XPE_ERR_IO_FAILED, xpe_ai_ipc_bridge_bone_suppress(c.b, 3, 3, kIn3x3, out));
    for (float v : out) EXPECT_EQ(-777.0f, v);
    EXPECT_EQ(XPE_ERR_NOT_INITIALIZED, SecondSend(c.b));
}

TEST(IpcDeadline, ControlAWorkersOwnErrorFrameKeepsTheConnection) {
    // A complete, well-formed answer that says "no": nothing is left in the pipe
    // and the worker is fine. Dropping here would turn every bad request into a
    // reconnect, which is the over-fix this control exists to catch.
    FakeWorker fw(ServeOnce([](FakeWorker& w, uint32_t id) {
        const std::string body = "{\"error_code\":-9,\"error_message\":\"no model\"}";
        ReplyFrame(w, XPE_AI_MSG_ERROR, id, 0, std::vector<char>(body.begin(), body.end()));
    }));
    ASSERT_TRUE(fw.ok());
    Client c(fw, 2000);
    ASSERT_NE(nullptr, c.b);
    float out[9];
    EXPECT_EQ(XPE_ERR_IO_FAILED, xpe_ai_ipc_bridge_bone_suppress(c.b, 3, 3, kIn3x3, out));
    EXPECT_EQ(XPE_OK, SecondSend(c.b)) << "an error FRAME is a healthy answer; the connection stays";
}

// --- Codex audit #11 (med): an ERROR frame the bridge cannot read is not a healthy answer ------------
//
// A worker's ERROR frame keeps the connection only when it carries a usable error code. A frame with
// no code, a code that is not a number, or one outside the range the XPE_ERR_* codes occupy is a
// worker speaking garbage; the stream may be fine, but nothing else it says can be trusted.

namespace {
void ExpectErrorFrameDrops(const char* body_text, const char* what) {
    FakeWorker fw(ServeOnce([body_text](FakeWorker& w, uint32_t id) {
        const std::string body = body_text;
        ReplyFrame(w, XPE_AI_MSG_ERROR, id, 0, std::vector<char>(body.begin(), body.end()));
    }));
    ASSERT_TRUE(fw.ok());
    Client c(fw, 2000);
    ASSERT_NE(nullptr, c.b);
    float out[9];
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, xpe_ai_ipc_bridge_bone_suppress(c.b, 3, 3, kIn3x3, out))
        << what;
    EXPECT_EQ(XPE_ERR_NOT_INITIALIZED, SecondSend(c.b)) << what << ": the connection was kept";
}
}  // namespace

TEST(IpcDeadline, AnErrorFrameWithNoCodeDropsTheConnection) {
    ExpectErrorFrameDrops("{}", "empty object");
}
TEST(IpcDeadline, AnErrorFrameWithANonNumericCodeDropsTheConnection) {
    ExpectErrorFrameDrops("{\"error_code\":\"bad\"}", "string code");
}
TEST(IpcDeadline, AnErrorFrameWithACodeOutsideTheErrorRangeDropsTheConnection) {
    ExpectErrorFrameDrops("{\"error_code\":0}", "zero is success, not an error");
    ExpectErrorFrameDrops("{\"error_code\":5}", "positive");
    ExpectErrorFrameDrops("{\"error_code\":-100000}", "far outside the XPE_ERR_* range");
    ExpectErrorFrameDrops("{\"error_code\":-9.5}", "not an integer");
}
