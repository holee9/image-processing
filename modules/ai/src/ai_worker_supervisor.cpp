/**
 * @file ai_worker_supervisor.cpp
 * @brief Worker process supervision (QA-B-171B, #130). See ai_worker_supervisor.h for the policy.
 *
 * Codex audit #11 shaped three rules that run through this file:
 *   - a worker is not "gone" until its end is CONFIRMED (the wait succeeded and the exit code was
 *     read); until then its handle is kept and no second worker is started beside it;
 *   - a worker is not started "protected" unless every step that protects it succeeded: it is created
 *     suspended and only resumed once it is inside a kill-on-close job;
 *   - a worker that answers a request with something that is not the answer is not healthy, even if
 *     the bytes were a well-formed frame.
 * Win32 failures here cannot be injected from a test without a seam in product code, and none was
 * added: those paths are justified by reading, and the report says so.
 *
 * @ingroup xpe_ai
 */

#include "ai_worker_supervisor.h"

#include "ai_ipc_bridge.h"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>

extern "C" {
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

namespace xpe::ai {

namespace {

/** How long a worker that was asked to exit gets before it is terminated. */
constexpr uint32_t kShutdownGraceMs = 3000;
/** How long to wait for a terminated process to be confirmed gone. */
constexpr DWORD kKillWaitMs = 3000;
/** How long a broken pipe is given to be followed by its process exiting. */
constexpr DWORD kDeathSettleMs = 200;

HANDLE H(void* p) { return static_cast<HANDLE>(p); }

XpeAiMessageHeader MakeHeader(uint32_t type, uint32_t request_id, uint32_t payload_size) {
    XpeAiMessageHeader h{};
    h.magic = XPE_AI_MSG_MAGIC;
    h.version = (static_cast<uint32_t>(XPE_AI_PROTOCOL_VERSION_MAJOR) << 16) |
                static_cast<uint32_t>(XPE_AI_PROTOCOL_VERSION_MINOR);
    h.messageType = type;
    h.requestId = request_id;
    h.payloadSize = payload_size;
    h.timestamp = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    return h;
}

std::string JsonEscape(const std::string& text) {
    std::string out;
    for (char c : text) {
        if (c == '"' || c == '\\') out += '\\';
        if (static_cast<unsigned char>(c) < 0x20) c = ' ';
        out += c;
    }
    return out;
}

std::atomic<uint32_t> g_request_id{1};
std::atomic<uint32_t> g_pipe_serial{0};

/**
 * End a child that was created suspended and never got to run: terminate it and CONFIRM it ended
 * before its handles are released. Returns false when that could not be confirmed (the caller
 * then keeps the handle rather than leaking a process it cannot see).
 */
bool EndSuspendedChild(PROCESS_INFORMATION& pi) {
    TerminateProcess(pi.hProcess, kSupervisorKillExitCode);
    const bool confirmed = WaitForSingleObject(pi.hProcess, kKillWaitMs) == WAIT_OBJECT_0;
    CloseHandle(pi.hThread);
    if (confirmed) CloseHandle(pi.hProcess);
    return confirmed;
}

}  // namespace

WorkerSupervisor::WorkerSupervisor(WorkerSupervisorConfig config) : config_(std::move(config)) {}

WorkerSupervisor::~WorkerSupervisor() {
    Stop();
    std::lock_guard<std::mutex> lock(mtx_);
    // The job is the backstop: closing it terminates any worker still alive.
    if (job_) {
        CloseHandle(H(job_));
        job_ = nullptr;
    }
}

uint32_t WorkerSupervisor::WorkerPid() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return process_ ? pid_ : 0;
}

uint32_t WorkerSupervisor::StartCount() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return start_count_;
}

WorkerExitInfo WorkerSupervisor::LastExit() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return last_exit_;
}

std::vector<uint32_t> WorkerSupervisor::StartedPids() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return started_pids_;
}

void WorkerSupervisor::ReleaseLocked() {
    if (bridge_) {
        xpe_ai_ipc_bridge_destroy(bridge_);
        bridge_ = nullptr;
    }
    if (process_) {
        CloseHandle(H(process_));
        process_ = nullptr;
    }
    pid_ = 0;
    shutdown_requested_ = false;
    pending_kill_ = false;
}

bool WorkerSupervisor::ReapIfExitedLocked() {
    if (!process_) return false;
    if (WaitForSingleObject(H(process_), 0) != WAIT_OBJECT_0) return false;
    DWORD code = 0;
    if (!GetExitCodeProcess(H(process_), &code)) {
        code = 0xFFFFFFFFu;   // it ended, but its code could not be read: record that, not a guess
    }
    // A worker exits by itself only after being told to. An exit nobody asked for is a death
    // whatever its code; an asked-for exit is clean only at 0.
    last_exit_.kind = (shutdown_requested_ && code == 0) ? WorkerExit::kClean : WorkerExit::kDied;
    last_exit_.pid = pid_;
    last_exit_.exit_code = code;
    ReleaseLocked();
    return true;
}

bool WorkerSupervisor::KillLocked() {
    if (!process_) return true;

    // A worker that is dying closes its pipe a moment BEFORE its process handle signals. Give it
    // that moment, so a death is recorded as a death with its own exit code rather than as a kill
    // this class made.
    if (WaitForSingleObject(H(process_), kDeathSettleMs) == WAIT_OBJECT_0 && ReapIfExitedLocked()) {
        return true;
    }

    const BOOL terminated = TerminateProcess(H(process_), kSupervisorKillExitCode);
    const DWORD waited = WaitForSingleObject(H(process_), kKillWaitMs);
    DWORD code = 0;
    if (waited != WAIT_OBJECT_0 || !GetExitCodeProcess(H(process_), &code)) {
        // Not confirmed gone. Keep the handle, say so, and let nothing start a second worker
        // beside one that may still be running. The bridge is no use either way.
        pending_kill_ = true;
        if (bridge_) {
            xpe_ai_ipc_bridge_destroy(bridge_);
            bridge_ = nullptr;
        }
        return false;
    }
    // If TerminateProcess itself failed but the process is gone anyway, it ended on its own.
    last_exit_.kind = terminated ? WorkerExit::kKilled : WorkerExit::kDied;
    last_exit_.pid = pid_;
    last_exit_.exit_code = code;
    ReleaseLocked();
    return true;
}

void WorkerSupervisor::DropIfBridgeDownLocked() {
    // The bridge drops its connection after a timeout, a half transfer or a broken pipe
    // (ai_ipc_bridge.cpp). A worker whose pipe is down cannot be trusted with the next request,
    // and a stalled one is still running: end it. If that cannot be confirmed, KillLocked leaves
    // the pending state that EnsureRunningLocked refuses to start past.
    if (process_ && (!bridge_ || !bridge_->connected)) {
        KillLocked();
    }
}

XpeErrorCode WorkerSupervisor::StartLocked() {
    if (!job_) {
        job_ = CreateJobObjectA(nullptr, nullptr);
        if (!job_) return XPE_ERR_PROCESSING_FAILED;
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
        info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        if (!SetInformationJobObject(H(job_), JobObjectExtendedLimitInformation, &info,
                                     sizeof(info))) {
            CloseHandle(H(job_));
            job_ = nullptr;
            return XPE_ERR_PROCESSING_FAILED;
        }
    }

    char pipe[160];
    std::snprintf(pipe, sizeof(pipe), "\\\\.\\pipe\\xpe_ai_worker_%lu_%u",
                  static_cast<unsigned long>(GetCurrentProcessId()), g_pipe_serial.fetch_add(1));

    std::string cmd = "\"" + config_.worker_exe + "\" " + pipe;
    STARTUPINFOA si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    // Suspended, so it is inside the job before it runs a single instruction.
    if (!CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, nullptr, &si, &pi)) {
        return XPE_ERR_IO_FAILED;
    }
    // Resume only when the process is inside the kill-on-close job. A worker that could outlive the
    // host is not started: end the suspended child, confirm it, and report an explicit error.
    if (!AssignProcessToJobObject(H(job_), pi.hProcess) || ResumeThread(pi.hThread) == 0xFFFFFFFFu) {
        if (!EndSuspendedChild(pi)) {
            // Could not confirm it ended: keep its handle and refuse to start anything else.
            process_ = pi.hProcess;
            pid_ = pi.dwProcessId;
            pending_kill_ = true;
        }
        return XPE_ERR_PROCESSING_FAILED;
    }
    CloseHandle(pi.hThread);

    process_ = pi.hProcess;
    pid_ = pi.dwProcessId;
    ++start_count_;
    started_pids_.push_back(pid_);
    shutdown_requested_ = false;

    bridge_ = xpe_ai_ipc_bridge_create(pipe, config_.timeout_ms);
    if (!bridge_) {
        KillLocked();
        return XPE_ERR_OUT_OF_MEMORY;
    }

    // The worker needs a moment to create its pipe; connect() fails at once on
    // a pipe that does not exist yet, so retry until the start-up budget is spent.
    const ULONGLONG deadline = GetTickCount64() + config_.timeout_ms;
    bool connected = false;
    while (GetTickCount64() < deadline) {
        if (xpe_ai_ipc_bridge_connect(bridge_) == XPE_OK) {
            connected = true;
            break;
        }
        if (WaitForSingleObject(H(process_), 0) == WAIT_OBJECT_0) break;   // died on start
        Sleep(20);
    }
    if (!connected) {
        if (!ReapIfExitedLocked()) KillLocked();
        return XPE_ERR_PROCESSING_FAILED;
    }

    // Session start: the worker acts on model_dir.
    const std::string body = "{\"model_dir\":\"" + JsonEscape(config_.model_dir) + "\"}";
    const uint32_t id = g_request_id.fetch_add(1);
    XpeAiMessageHeader req = MakeHeader(XPE_AI_MSG_INIT, id, static_cast<uint32_t>(body.size()));
    XpeAiMessageHeader rep{};
    char buf[512];
    uint32_t got = 0;
    XpeErrorCode rc = xpe_ai_ipc_bridge_send(bridge_, &req, body.data(),
                                             static_cast<uint32_t>(body.size()));
    if (rc == XPE_OK) {
        rc = xpe_ai_ipc_bridge_receive(bridge_, &rep, buf, sizeof(buf), &got);
    }
    if (rc != XPE_OK || rep.messageType != XPE_AI_MSG_INIT_RESPONSE || rep.requestId != id) {
        KillLocked();
        return rc != XPE_OK ? rc : XPE_ERR_PROCESSING_FAILED;
    }
    return XPE_OK;
}

XpeErrorCode WorkerSupervisor::EnsureRunningLocked() {
    if (process_ && !ReapIfExitedLocked()) {
        // Noticed BEFORE anything is sent: a worker that died between calls costs a fresh
        // start, not a failed request. A worker still running whose pipe is down, or one whose
        // earlier kill was never confirmed, has to be ended first.
        if (pending_kill_ || !bridge_ || !bridge_->connected) {
            if (!KillLocked()) {
                // Do not start a second worker beside one that may still be running.
                return XPE_ERR_PROCESSING_FAILED;
            }
        }
    }
    if (process_) return XPE_OK;
    return StartLocked();
}

XpeErrorCode WorkerSupervisor::Ping() {
    std::lock_guard<std::mutex> lock(mtx_);
    XpeErrorCode rc = EnsureRunningLocked();
    if (rc != XPE_OK) return rc;

    const uint32_t id = g_request_id.fetch_add(1);
    XpeAiMessageHeader req = MakeHeader(XPE_AI_MSG_HEARTBEAT, id, 0);
    XpeAiMessageHeader rep{};
    char buf[128];
    uint32_t got = 0;
    rc = xpe_ai_ipc_bridge_send(bridge_, &req, nullptr, 0);
    if (rc == XPE_OK) {
        rc = xpe_ai_ipc_bridge_receive(bridge_, &rep, buf, sizeof(buf), &got);
    }
    if (rc == XPE_OK && (rep.messageType != XPE_AI_MSG_HEARTBEAT_ACK || rep.requestId != id)) {
        // A complete, well-formed frame that is not the answer to a heartbeat: the bridge sees
        // nothing wrong with the stream, but a worker that answers nonsense is not healthy.
        // Discard it; the next call starts a fresh one.
        KillLocked();
        return XPE_ERR_IO_FAILED;
    }
    DropIfBridgeDownLocked();
    return rc;
}

XpeErrorCode WorkerSupervisor::BoneSuppress(uint32_t width, uint32_t height,
                                            const float* pixels_in, float* pixels_out) {
    std::lock_guard<std::mutex> lock(mtx_);
    last_result_nonfinite_ = false;
    XpeErrorCode rc = EnsureRunningLocked();
    if (rc != XPE_OK) return rc;
    rc = xpe_ai_ipc_bridge_bone_suppress(bridge_, width, height, pixels_in, pixels_out);
    last_result_nonfinite_ = bridge_ != nullptr && bridge_->last_result_nonfinite;
    // A worker's error frame leaves the bridge connected and the worker in place; a transport
    // fault or a frame the bridge could not trust takes the bridge down and the worker with it.
    DropIfBridgeDownLocked();
    return rc;
}

bool WorkerSupervisor::LastResultWasNonFinite() const {
    std::lock_guard<std::mutex> lock(mtx_);
    return last_result_nonfinite_;
}

void WorkerSupervisor::Stop() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!process_) return;
    // Look at the handle first: a worker that already ended on its own must be recorded as what it
    // was, not as something this call did to it.
    if (ReapIfExitedLocked()) return;
    if (bridge_ && bridge_->connected && !pending_kill_) {
        XpeAiMessageHeader req = MakeHeader(XPE_AI_MSG_SHUTDOWN, g_request_id.fetch_add(1), 0);
        if (xpe_ai_ipc_bridge_send(bridge_, &req, nullptr, 0) == XPE_OK) {
            shutdown_requested_ = true;
            const DWORD wait = config_.timeout_ms < kShutdownGraceMs ? config_.timeout_ms
                                                                      : kShutdownGraceMs;
            if (WaitForSingleObject(H(process_), wait) == WAIT_OBJECT_0) {
                ReapIfExitedLocked();
                return;
            }
        }
    }
    KillLocked();
}

}  // namespace xpe::ai
