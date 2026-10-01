/**
 * @file ai_worker_supervisor.h
 * @brief Owns the AI worker process: launch, health, kill on fault, restart (QA-B-171B, #130).
 *
 * Internal to xpe_ai (not exported; REQ-AI-003 keeps the pipe protocol private).
 *
 * WHY THIS EXISTS. Before it, nothing in the product launched xpe_ai_worker.exe,
 * so nothing could own its lifetime. The bridge (ai_ipc_bridge.cpp) now gives a
 * call a time budget (REQ-AI-092) and drops its connection after a fault -- but a
 * worker that stalled is still running, and the bridge stays down for ever.
 * SDD (XPE-SDD-002) ties "Worker crash -> process died -> Restart worker" to
 * SRS-SAFE-008 / HAZ-008; this class is that mitigation.
 *
 * POLICY (a supervisor policy, not a retry):
 *   - A fault during a call (budget exceeded, pipe broken, half transfer) fails
 *     THAT call, exactly as the bridge reported it. The worker is then killed
 *     and the NEXT call starts a fresh one. A failed call is never silently
 *     re-run -- a caller must be able to see that a call failed.
 *   - A worker that already exited between calls is noticed BEFORE the next
 *     request is sent (a liveness check, not a retry: nothing was sent yet), and
 *     a fresh one is started.
 *   - A worker that ANSWERED, even with an error frame (missing model, bad
 *     input), is healthy and is kept: that is the worker doing its job.
 *
 * SHUTDOWN_ACK IS NOT IN THE PROTOCOL (leader decision, QA-B-171). A requested
 * exit and a death look the same on the pipe; they are told apart by the exit
 * code of the process HANDLE this class keeps open. That premise -- the
 * supervisor holds the handle -- is pinned by tests/test_worker_supervisor.cpp.
 *
 * @ingroup xpe_ai
 */
#ifndef XPE_AI_WORKER_SUPERVISOR_H
#define XPE_AI_WORKER_SUPERVISOR_H

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include "xpe/ai/ai_worker_protocol.h"
#include "xpe/common/xpe_error.h"

struct XpeAiIpcBridge;

namespace xpe::ai {

/** How the most recent worker ended, as read from its process handle. */
enum class WorkerExit {
    kNone,    ///< no worker has ended yet
    kClean,   ///< the supervisor asked it to shut down and it exited with code 0
    kDied,    ///< it ended without being asked, or exited non-zero after being asked
    kKilled,  ///< the supervisor terminated it (budget exceeded or broken pipe)
};

/** The last worker exit: what happened, which process, and its exit code. */
struct WorkerExitInfo {
    WorkerExit kind = WorkerExit::kNone;
    uint32_t pid = 0;
    uint32_t exit_code = 0;
};

/** Exit code the supervisor gives a worker it terminates. */
constexpr uint32_t kSupervisorKillExitCode = 0x4B494C4Cu;  // "KILL"

struct WorkerSupervisorConfig {
    std::string worker_exe;   ///< path of xpe_ai_worker.exe
    std::string model_dir;    ///< sent to the worker in its session-start message
    /** REQ-AI-092 time budget per call and per start-up (default 5 s). */
    uint32_t timeout_ms = XPE_AI_DEFAULT_TIMEOUT_MS;
};

class WorkerSupervisor {
public:
    explicit WorkerSupervisor(WorkerSupervisorConfig config);
    /** Stops the worker (graceful, then kill): no process outlives the supervisor. */
    ~WorkerSupervisor();

    WorkerSupervisor(const WorkerSupervisor&) = delete;
    WorkerSupervisor& operator=(const WorkerSupervisor&) = delete;

    /** Health check: heartbeat round trip, starting the worker first if needed. */
    XpeErrorCode Ping();

    /** Bone suppression through the worker; see ai_worker_protocol.h for the wire layout. */
    XpeErrorCode BoneSuppress(uint32_t width, uint32_t height, const float* pixels_in,
                              float* pixels_out);

    /** Ask the worker to shut down; kill it if it does not exit within the budget. */
    void Stop();

    /** Pid of the running worker, or 0 when none is running. */
    uint32_t WorkerPid() const;
    /** Number of workers started so far. */
    uint32_t StartCount() const;
    /** How the most recent worker ended. */
    WorkerExitInfo LastExit() const;
    /** Pids of every worker this supervisor ever started (for hygiene checks). */
    std::vector<uint32_t> StartedPids() const;

private:
    XpeErrorCode EnsureRunningLocked();
    XpeErrorCode StartLocked();
    /** True when the worker process has ended; records how, and releases it. */
    bool ReapIfExitedLocked();
    void KillLocked();
    void ReleaseLocked();
    void DropIfBridgeDownLocked();

    WorkerSupervisorConfig config_;
    mutable std::mutex mtx_;

    void* process_ = nullptr;   ///< HANDLE, held open for the worker's whole life and until reaped
    void* job_ = nullptr;       ///< HANDLE of a kill-on-close job: workers die with the host
    XpeAiIpcBridge* bridge_ = nullptr;
    uint32_t pid_ = 0;
    uint32_t start_count_ = 0;
    bool shutdown_requested_ = false;
    WorkerExitInfo last_exit_;
    std::vector<uint32_t> started_pids_;
};

}  // namespace xpe::ai

#endif  // XPE_AI_WORKER_SUPERVISOR_H
