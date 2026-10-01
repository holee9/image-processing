/**
 * @file ai_worker_supervisor.cpp
 * @brief Worker process supervision (QA-B-171B, #130). See ai_worker_supervisor.h for the policy.
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
/** How long to wait for a terminated process to be gone. */
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
}

bool WorkerSupervisor::ReapIfExitedLocked() {
    if (!process_) return false;
    if (WaitForSingleObject(H(process_), 0) != WAIT_OBJECT_0) return false;
    DWORD code = 0;
    GetExitCodeProcess(H(process_), &code);
    // A worker exits by itself only after being told to. An exit nobody asked
    // for is a death whatever its code; an asked-for exit is clean only at 0.
    last_exit_.kind = (shutdown_requested_ && code == 0) ? WorkerExit::kClean : WorkerExit::kDied;
    last_exit_.pid = pid_;
    last_exit_.exit_code = code;
    ReleaseLocked();
    return true;
}

void WorkerSupervisor::KillLocked() {
    if (!process_) return;
    TerminateProcess(H(process_), kSupervisorKillExitCode);
    WaitForSingleObject(H(process_), kKillWaitMs);
    DWORD code = 0;
    GetExitCodeProcess(H(process_), &code);
    last_exit_.kind = WorkerExit::kKilled;
    last_exit_.pid = pid_;
    last_exit_.exit_code = code;
    ReleaseLocked();
}

void WorkerSupervisor::DropIfBridgeDownLocked() {
    // The bridge drops its connection after a timeout, a half transfer or a
    // broken pipe (ai_ipc_bridge.cpp). A worker whose pipe is down cannot be
    // trusted with the next request, and a stalled one is still running: kill it.
    if (process_ && (!bridge_ || !bridge_->connected)) {
        // A worker that is dying closes its pipe a moment BEFORE its process
        // handle signals. Give it that moment, so a death is recorded as a
        // death with its own exit code rather than as a kill this class made.
        if (WaitForSingleObject(H(process_), kDeathSettleMs) == WAIT_OBJECT_0) {
            ReapIfExitedLocked();
        } else {
            KillLocked();
        }
    }
}

XpeErrorCode WorkerSupervisor::StartLocked() {
    if (!job_) {
        job_ = CreateJobObjectA(nullptr, nullptr);
        if (job_) {
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION info{};
            info.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            SetInformationJobObject(H(job_), JobObjectExtendedLimitInformation, &info, sizeof(info));
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
    if (job_) AssignProcessToJobObject(H(job_), pi.hProcess);   // best effort; Stop() still kills
    ResumeThread(pi.hThread);
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
    if (process_) {
        // Noticed BEFORE anything is sent: a worker that died between calls
        // costs a fresh start, not a failed request.
        if (!ReapIfExitedLocked()) {
            DropIfBridgeDownLocked();
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
        rc = XPE_ERR_IO_FAILED;
    }
    DropIfBridgeDownLocked();
    return rc;
}

XpeErrorCode WorkerSupervisor::BoneSuppress(uint32_t width, uint32_t height,
                                            const float* pixels_in, float* pixels_out) {
    std::lock_guard<std::mutex> lock(mtx_);
    XpeErrorCode rc = EnsureRunningLocked();
    if (rc != XPE_OK) return rc;
    rc = xpe_ai_ipc_bridge_bone_suppress(bridge_, width, height, pixels_in, pixels_out);
    // An error frame from the worker leaves the bridge connected and the worker
    // in place; a transport fault takes the bridge down and the worker with it.
    DropIfBridgeDownLocked();
    return rc;
}

void WorkerSupervisor::Stop() {
    std::lock_guard<std::mutex> lock(mtx_);
    if (!process_) return;
    if (bridge_ && bridge_->connected) {
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
