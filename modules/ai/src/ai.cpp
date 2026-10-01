/**
 * @file ai.cpp
 * @brief xpe_ai.dll skeleton implementation -- in-process C ABI proxy.
 *
 * This module is an in-process proxy that communicates with a sandboxed
 * xpe_ai_worker.exe over named pipes. The worker process hosts the ONNX
 * Runtime inference engine, providing crash isolation for GPU/native code.
 *
 * Current implementation: stub phase (ONNX Runtime not yet linked).
 * All functions validate inputs, check initialization state, and return
 * appropriate error codes. When ONNX Runtime is linked, the IPC bridge
 * will route requests to the worker process.
 *
 * REQ-AI-001: Layer 1 dependency (xpe_common only).
 * REQ-AI-002: Deterministic fallback routing for all AI functions.
 * REQ-AI-003: Worker-isolated architecture (IPC via named pipe).
 * REQ-AI-005: Opt-in activation (default off until xpe_ai_init called).
 *
 * @ingroup xpe_ai
 */

#ifndef XPE_DLL_EXPORT
#define XPE_DLL_EXPORT
#endif

#include "xpe/ai/ai_api.h"
#include "xpe/ai/ai_worker_protocol.h"
#include "xpe/ai/ai_onnx_session.h"
#include "ai_worker_supervisor.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <cstring>
#include <atomic>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

// @MX:NOTE: [AUTO] nlohmann/json included for config parsing;
//           conditional compilation avoids hard dependency.
#ifdef XPE_AI_USE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif

// @MX:NOTE: [AUTO] spdlog is a soft dependency; logging falls back to
//           no-op if not linked.
#ifdef XPE_AI_USE_SPDLOG
#include <spdlog/spdlog.h>
#define AI_LOG_TRACE(...) spdlog::trace(__VA_ARGS__)
#define AI_LOG_DEBUG(...) spdlog::debug(__VA_ARGS__)
#define AI_LOG_INFO(...)  spdlog::info(__VA_ARGS__)
#define AI_LOG_WARN(...)  spdlog::warn(__VA_ARGS__)
#define AI_LOG_ERROR(...) spdlog::error(__VA_ARGS__)
#else
#include <cstdio>
#define AI_LOG_TRACE(...) do {} while(0)
#define AI_LOG_DEBUG(...) do {} while(0)
#define AI_LOG_INFO(...)  std::printf("[AI INFO] " __VA_ARGS__); std::printf("\n")
#define AI_LOG_WARN(...)  std::printf("[AI WARN] " __VA_ARGS__); std::printf("\n")
#define AI_LOG_ERROR(...) std::printf("[AI ERROR] " __VA_ARGS__); std::printf("\n")
#endif

/* ==========================================================================
 * Internal State
 * ========================================================================== */

/**
 * @brief Internal module context (hidden from ABI).
 *
 * All mutable state is protected by a single mutex. Atomic flags are
 * used for lock-free reads where appropriate (e.g., initialized check).
 */
struct AiModuleState {
    std::mutex mtx;

    /** True after xpe_ai_init() succeeds, false after xpe_ai_shutdown(). */
    std::atomic<bool> initialized{false};

    /** True when fallback mode is active (default: true per REQ-AI-002). */
    std::atomic<bool> fallbackMode{true};

    /** Path to the model directory (set by xpe_ai_init). */
    std::string modelDirPath;

    /** Selected execution provider. */
    XpeAiExecutionProvider executionProvider{XPE_AI_EP_CPU};

    /** IPC timeout in milliseconds. */
    uint32_t timeoutMs{XPE_AI_DEFAULT_TIMEOUT_MS};

    /** Confidence threshold below which fallback is triggered. */
    float confidenceThreshold{XPE_AI_DEFAULT_CONFIDENCE_THRESHOLD};

    /** Monotonically increasing request ID for IPC. */
    std::atomic<uint32_t> nextRequestId{1};

    /**
     * Inference session for xpe_bone_suppress, owned by this module
     * (QA-B-161, #130, FUNC-038).
     *
     * Loaded lazily on the first call rather than in xpe_ai_init, because
     * init must stay cheap for callers that never run inference, and because
     * a missing model is then reported by the function that needed it rather
     * than by init. Released in xpe_ai_shutdown -- the CALLER NEVER SEES OR
     * FREES IT; nothing about it crosses the C ABI.
     *
     * Guarded by `mtx` like the rest of this struct. ONNX Runtime sessions are
     * documented as safe for concurrent Run(), but creation is not, and the
     * lazy load is a write.
     */
    std::unique_ptr<xpe::ai::OnnxSession> boneSuppressSession;

    /** Model directory the cached session was built from, so a re-init with a
     *  different directory does not silently keep serving the old model. */
    std::string boneSuppressSessionDir;

    /**
     * Opt-in (QA-B-171C): route xpe_bone_suppress through the worker process. Default OFF -- the
     * in-process path is the behaviour every caller had before and stays the default.
     */
    bool useWorker{false};

    /**
     * Owns the worker process when useWorker is set (QA-B-171B). Created lazily on the first call that
     * needs it, so init stays cheap and a caller that never infers never starts a process. Destroyed
     * in xpe_ai_shutdown, which ends the worker: no process outlives the module.
     */
    std::unique_ptr<xpe::ai::WorkerSupervisor> workerSupervisor;

    /**
     * Consecutive failures of the worker path (QA-B-171C policy, user-approved 2026-10-01,
     * docs/project/REQ-CHANGE-LOG-P3-AI.md rows 2 and 3). A success resets it to 0.
     */
    uint32_t workerConsecutiveFailures{0};

    /** Set at kWorkerFailureCeiling consecutive failures: the worker is off for the rest of the session. */
    bool workerDisabled{false};

    /**
     * What xpe_ai_worker_state reports: the two fields above as of the last COMPLETED call, packed in ONE
     * word (bit 31 = disabled, bits 0..30 = consecutive failures) and stored only when a call is done
     * with everything it does -- the count, the switch-off, ending the worker process and raising the
     * alert (Codex audit #17). The two fields above are working state, written inside a call with mtx
     * held and never read by a client. They are kept apart from this word for two reasons: a status query
     * must not wait for mtx (a call holds it for its whole time budget on a silent worker, which would
     * freeze the client's UI thread for that long), and a query that read them directly would see the
     * third failure's switch-off while that call was still running and its worker still alive. One word
     * also means a reader can never combine a count from one moment with a flag from another (QA-B-173).
     */
    std::atomic<uint32_t> workerPublished{0};

    // --- Worker process state ---
    /** PID of the worker process (0 if not running). */
    uint32_t workerPid{0};

    /** Handle to the named pipe (platform-specific). */
    void* pipeHandle{nullptr};

    // --- Model registry ---
    /** List of loaded model IDs. */
    std::vector<std::string> loadedModels;

    AiModuleState() = default;

    // Non-copyable, non-movable.
    AiModuleState(const AiModuleState&) = delete;
    AiModuleState& operator=(const AiModuleState&) = delete;
};

/**
 * @brief Singleton module state. Allocated on first init, freed on shutdown.
 *
 * Raw pointer (not unique_ptr) to avoid static destruction order issues.
 * The pointer is never freed during the process lifetime except via
 * xpe_ai_shutdown().
 */
static AiModuleState* g_aiState = nullptr;

/* ==========================================================================
 * Helper Functions
 * ========================================================================== */

/**
 * @brief Check if the module is initialized; return error if not.
 */
static inline XpeErrorCode checkInitialized() {
    if (!g_aiState || !g_aiState->initialized.load(std::memory_order_acquire)) {
        return XPE_ERR_NOT_INITIALIZED;
    }
    return XPE_OK;
}

/**
 * @brief Validate that a pointer parameter is not null.
 */
static inline XpeErrorCode checkNotNull(const void* ptr) {
    return (ptr != nullptr) ? XPE_OK : XPE_ERR_INVALID_INPUT;
}

/**
 * @brief Validate an XpeImageBuffer has valid dimensions and data.
 */
static XpeErrorCode validateImageBuffer(const XpeImageBuffer* img) {
    if (!img) return XPE_ERR_INVALID_INPUT;
    if (img->width == 0 || img->height == 0) return XPE_ERR_INVALID_INPUT;
    if (!img->data) return XPE_ERR_INVALID_INPUT;

    // api-spec "XpeImageBuffer.dataSize on input" (#123): dataSize == 0 means
    // unspecified and is accepted. This validator used to reject it (#123 B-20),
    // which contradicted the contract; both checks below are no-ops when it is 0.

    // Maximum: 4096x4096x4 = 64 MB
    const size_t maxBytes = static_cast<size_t>(4096) * 4096 * 4;
    if (img->dataSize > maxBytes) return XPE_ERR_INVALID_INPUT;

    // api-spec "XpeImageBuffer.dataSize on input" (#123): a non-zero dataSize
    // smaller than the declared dimensions cannot hold the image and is read
    // past its allocation.
    {
        uint32_t bpp = 0u;
        if (img->format == XPE_PIXEL_UINT16)       bpp = 2u;
        else if (img->format == XPE_PIXEL_FLOAT32) bpp = 4u;
        if (bpp != 0u) {
            // width * height cannot overflow 64 bits (each is below 2^32); width * height * bpp CAN:
            // 2^31 x 2^31 x 4 is 2^64, which is 0, and a required size of 0 is satisfied by any dataSize
            // (Codex audit #12). So the product is bounded by DIVISION first. The bound is the module
            // maximum applied to the DECLARED image, whatever dataSize says: dimensions that imply more
            // than 4096 x 4096 x 4 bytes cannot describe a valid buffer, and "unspecified" (dataSize 0)
            // is not a licence to trust them.
            const uint64_t pixels = static_cast<uint64_t>(img->width) *
                                    static_cast<uint64_t>(img->height);
            if (pixels > static_cast<uint64_t>(maxBytes) / bpp) return XPE_ERR_INVALID_INPUT;
            const uint64_t required = pixels * bpp;
            if (img->dataSize != 0u && static_cast<uint64_t>(img->dataSize) < required) {
                return XPE_ERR_INVALID_INPUT;
            }
        }
    }
    return XPE_OK;
}

/**
 * @brief Parse config JSON and update module state.
 *
 * Uses nlohmann/json when available; otherwise falls back to simple
 * string scanning for key parameters.
 */
static void parseConfig(AiModuleState* state, const char* configJsonOrNull) {
    if (!configJsonOrNull) return;

#ifdef XPE_AI_USE_NLOHMANN_JSON
    auto cfg = nlohmann::json::parse(configJsonOrNull, nullptr, false);
    if (cfg.is_discarded()) {
        AI_LOG_WARN("AI config JSON parse failed, using defaults");
        return;
    }

    if (cfg.contains("execution_provider") && cfg["execution_provider"].is_string()) {
        const auto ep = cfg["execution_provider"].get<std::string>();
        if (ep == "cpu")       state->executionProvider = XPE_AI_EP_CPU;
        else if (ep == "cuda")      state->executionProvider = XPE_AI_EP_CUDA;
        else if (ep == "tensorrt")  state->executionProvider = XPE_AI_EP_TENSORRT;
        else if (ep == "directml")  state->executionProvider = XPE_AI_EP_DIRECTML;
        else if (ep == "auto")      state->executionProvider = XPE_AI_EP_AUTO;
    }

    if (cfg.contains("timeout_ms") && cfg["timeout_ms"].is_number_integer()) {
        state->timeoutMs = static_cast<uint32_t>(cfg["timeout_ms"].get<int>());
    }

    if (cfg.contains("confidence_threshold") && cfg["confidence_threshold"].is_number()) {
        state->confidenceThreshold = cfg["confidence_threshold"].get<float>();
    }

    if (cfg.contains("fallback_mode") && cfg["fallback_mode"].is_boolean()) {
        state->fallbackMode.store(cfg["fallback_mode"].get<bool>(),
                                   std::memory_order_release);
    }

    // QA-B-171C: opt-in worker path for xpe_bone_suppress. Absent or false leaves the in-process path.
    if (cfg.contains("use_worker") && cfg["use_worker"].is_boolean()) {
        state->useWorker = cfg["use_worker"].get<bool>();
    }

    // #145 (QA-B-60): name the top-level keys this parser did not consume.
    //
    // Every key above is read conditionally, so a caller's typo -- or a key
    // meant for another module -- lands in none of them and the call still
    // returns XPE_OK. QA-B-59 measured the same silence in gsvg: the setting
    // has no effect and nothing says so.
    //
    // RETURN CODE UNCHANGED. Rejecting an unknown key is a behaviour change and
    // belongs to the #145 decision; this only makes the silence audible.
    //
    // A key that is present but of the WRONG TYPE is also unconsumed, and is
    // reported for the same reason -- "timeout_ms": "500" is exactly the mistake
    // this is for.
    if (cfg.is_object()) {
        static const char* const kKnownKeys[] = {
            "execution_provider", "timeout_ms", "confidence_threshold", "fallback_mode",
            "use_worker"
        };
        for (auto it = cfg.begin(); it != cfg.end(); ++it) {
            bool known = false;
            for (const char* k : kKnownKeys) {
                if (it.key() == k) { known = true; break; }
            }
            const bool consumed =
                known &&
                ((it.key() == "execution_provider"   && it.value().is_string()) ||
                 (it.key() == "timeout_ms"           && it.value().is_number_integer()) ||
                 (it.key() == "confidence_threshold" && it.value().is_number()) ||
                 (it.key() == "fallback_mode"        && it.value().is_boolean()) ||
                 (it.key() == "use_worker"           && it.value().is_boolean()));
            if (!consumed) {
                char msg[192];
                std::snprintf(msg, sizeof(msg),
                              known
                                  ? "ai config key '%s' has an unexpected type and was ignored"
                                  : "ai config key '%s' is not read by this module and has no effect",
                              it.key().c_str());
                xpe_alert_push(msg, XPE_ALERT_WARNING);
            }
        }
    }
#else
    // Minimal config parsing without nlohmann/json.
    // Only parse "timeout_ms" for basic functionality.
    const char* timeoutKey = std::strstr(configJsonOrNull, "\"timeout_ms\"");
    if (timeoutKey) {
        const char* colon = std::strchr(timeoutKey, ':');
        if (colon) {
            int val = std::atoi(colon + 1);
            if (val > 0) state->timeoutMs = static_cast<uint32_t>(val);
        }
    }
    const char* workerKey = std::strstr(configJsonOrNull, "\"use_worker\"");
    if (workerKey) {
        const char* colon = std::strchr(workerKey, ':');
        if (colon) {
            ++colon;
            while (*colon == ' ') ++colon;
            state->useWorker = std::strncmp(colon, "true", 4) == 0;
        }
    }
    AI_LOG_INFO("Config parsed (minimal parser, nlohmann/json not linked)");
#endif
}

/**
 * @brief Build a model card JSON string for a given model ID.
 *
 * Returns a stub model card when the model is recognized but full
 * metadata is not yet loaded from disk.
 */
static std::string buildStubModelCard(const std::string& modelId) {
    return std::string("{"
        "\"model_id\":\"") + modelId + "\","
        "\"model_version\":\"0.1.0-stub\","
        "\"intended_use\":\"XPE AI inference (stub -- ONNX Runtime not linked)\","
        "\"training_data_summary\":\"N/A (stub)\","
        "\"demographic_performance\":{},"
        "\"limitations\":\"This is a stub build. ONNX Runtime is not linked. "
                         "No actual inference is performed.\","
        "\"pccp_status\":\"not_applicable\","
        "\"published_date\":\"2026-04-22\","
        "\"training_data_hash\":\"N/A\","
        "\"validation_metrics\":{\"psnr\":0.0,\"ssim\":0.0}"
    "}";
}

/**
 * @brief Path of xpe_ai_worker.exe: the directory of THIS module, never PATH or the working directory.
 *
 * Deployment contract (QA-B-171C): the worker ships beside xpe_ai.dll. Looking anywhere else would
 * let a different executable answer to the worker's name in a process that handles patient images, so
 * if this module's own path cannot be read the answer is "none" and the worker path fails (reported,
 * and replaced by the in-process result) instead of searching.
 */
static std::string workerExePath() {
    HMODULE self = nullptr;
    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCSTR>(&workerExePath), &self)) {
        return std::string();
    }
    char buf[MAX_PATH * 2] = {0};
    const DWORD n = GetModuleFileNameA(self, buf, static_cast<DWORD>(sizeof(buf)));
    if (n == 0 || n >= sizeof(buf)) return std::string();
    const std::string path(buf, n);
    const size_t cut = path.find_last_of("\\/");
    if (cut == std::string::npos) return std::string();
    return path.substr(0, cut + 1) + "xpe_ai_worker.exe";
}

// Every image validateImageBuffer accepts (at most 4096 x 4096 x 4 bytes) must fit ONE worker request
// together with its length prefix and metadata (ai_ipc_bridge.cpp reserves 512 bytes for those). With
// that true the worker path needs no size exception of its own: the validator is the only gate, and
// there is no accepted image the worker path cannot carry (Codex audit #13).
static_assert(static_cast<size_t>(4096) * 4096 * 4 + 512u <= static_cast<size_t>(XPE_AI_MAX_PAYLOAD_SIZE),
              "the module maximum image must fit one worker message");

/**
 * @brief xpe_bone_suppress through the worker process (opt-in). Caller holds state->mtx.
 *
 * The result is written into @p out ONLY on success (the bridge copies pixels after it has checked
 * the reply), so a failed call leaves @p out untouched; the caller then fills it with the input
 * (the deterministic fallback), never with a half-written reply.
 */
static XpeErrorCode boneSuppressViaWorker(AiModuleState* state, const XpeImageBuffer* in,
                                          XpeImageBuffer* out) {
    try {
        if (!state->workerSupervisor) {
            xpe::ai::WorkerSupervisorConfig cfg;
            cfg.worker_exe = workerExePath();
            if (cfg.worker_exe.empty()) return XPE_ERR_IO_FAILED;
            cfg.model_dir = state->modelDirPath;
            // A budget of 0 would fail every call before the worker could answer; the requirement's
            // default applies instead.
            cfg.timeout_ms = state->timeoutMs != 0 ? state->timeoutMs : XPE_AI_DEFAULT_TIMEOUT_MS;
            state->workerSupervisor = std::make_unique<xpe::ai::WorkerSupervisor>(std::move(cfg));
        }
        return state->workerSupervisor->BoneSuppress(in->width, in->height,
                                                     static_cast<const float*>(in->data),
                                                     static_cast<float*>(out->data));
    } catch (...) {
        return XPE_ERR_OUT_OF_MEMORY;
    }
}

/**
 * Consecutive worker-path failures after which the worker is switched off for the session.
 *
 * NOT derived from the requirements, which give no number (REQ-AI-092 says "fallback and alert"; the
 * SDD says "restart worker"). It is the value the user approved on 2026-10-01 after the measurement that
 * motivated it: a worker that hangs on start costs the whole time budget plus about 250 ms on EVERY call,
 * and a fault that repeats every call fills the 64-entry alert queue and evicts unrelated warnings.
 * Recorded in docs/project/REQ-CHANGE-LOG-P3-AI.md, rows 2 and 3 (row 3 replaced row 2's alert rule).
 */
static constexpr uint32_t kWorkerFailureCeiling = 3;

#ifdef XPE_AI_TEST_HOOKS
// TEST-ONLY (QA-B-173, Codex audit #19). Compiled only when modules/ai/CMakeLists.txt defines
// XPE_AI_TEST_HOOKS, which it does only for a build that builds this module's tests; a shipped build
// (BUILD_TESTS off) has neither this variable, nor the call in xpe_bone_suppress, nor the exported setter.
// A test registers a callback that xpe_bone_suppress calls on the calling thread immediately after it has
// locked the module mutex, so the test KNOWS a call is inside its critical section (and, with a frozen
// worker, stuck there) rather than inferring it from timing.
static std::atomic<void (*)(void)> g_testMutexHeldHook{nullptr};

extern "C" XPE_API void xpe_ai_test_set_mutex_held_hook(void (*hook)(void)) {
    g_testMutexHeldHook.store(hook, std::memory_order_release);
}
#endif

/** Bit 31 of AiModuleState::workerPublished: the worker is switched off for the session. */
static constexpr uint32_t kWorkerPublishedDisabledBit = 0x80000000u;

/** Publishes the working worker state as the one snapshot xpe_ai_worker_state reads. Caller holds mtx. */
static void publishWorkerState(AiModuleState* state) {
    const uint32_t word = (state->workerConsecutiveFailures & ~kWorkerPublishedDisabledBit) |
                          (state->workerDisabled ? kWorkerPublishedDisabledBit : 0u);
    state->workerPublished.store(word, std::memory_order_release);
}

/** SRS-ALERT-004: DL processing was applied (Info). One place, so both paths say the same thing. */
static void pushAiProcessedAlert() {
    xpe_alert_push("AI-processed: bone suppression applied (SRS-ALERT-004)", XPE_ALERT_INFO);
}

/* ==========================================================================
 * Exported API Implementation
 * ========================================================================== */

// @MX:ANCHOR: [AUTO] xpe_ai_version -- SPEC-XPE-P3-AI, api-spec.md S9
// @MX:REASON: Module identity function; called by orchestrator for readiness check

extern "C" {

XPE_API const char* xpe_ai_version(void)
{
    return "0.1.0";
}

XPE_API XpeErrorCode xpe_ai_init(const char* modelDirPath,
                                  const char* configJsonOrNull)
{
    // Validate required parameter
    if (!modelDirPath) return XPE_ERR_INVALID_INPUT;

    // If already initialized, return success (idempotent)
    if (g_aiState && g_aiState->initialized.load(std::memory_order_acquire)) {
        AI_LOG_WARN("xpe_ai_init called while already initialized -- ignoring");
        return XPE_OK;
    }

    // Allocate module state
    auto* state = new (std::nothrow) AiModuleState();
    if (!state) return XPE_ERR_OUT_OF_MEMORY;

    // Store model directory
    state->modelDirPath = modelDirPath;

    // Parse optional configuration
    parseConfig(state, configJsonOrNull);

    // --- Worker process launch ---
    // Stub: In the full implementation, this would:
    //   1. Create a named pipe: \\.\pipe\xpe_ai_worker_{GetCurrentProcessId()}
    //   2. Launch xpe_ai_worker.exe with --pipe-name argument
    //   3. Wait for INIT_RESPONSE with timeout
    //   4. Verify protocol version match
    //
    // For now, we register known model IDs without actual loading.
    state->loadedModels = {
        "bodypart_cnn_v1",
        "stitch_feature_match_v1",
        "bone_suppress_unet_v1",
        "dl_denoise_ssl_v1"
    };

    // Mark as initialized
    state->initialized.store(true, std::memory_order_release);
    g_aiState = state;

    AI_LOG_INFO("xpe_ai initialized: model_dir=%s, ep=%d, timeout=%u ms",
                modelDirPath,
                static_cast<int>(state->executionProvider),
                state->timeoutMs);

    return XPE_OK;
}

XPE_API XpeErrorCode xpe_ai_worker_state(int32_t* stateOut,
                                          uint32_t* consecutiveFailuresOut,
                                          uint32_t* ceilingOut)
{
    if (!stateOut) return XPE_ERR_INVALID_INPUT;
    XpeErrorCode ec = checkInitialized();
    if (ec != XPE_OK) return ec;

    // Deliberately NO lock_guard on state->mtx: see AiModuleState::workerPublished. One load of one word:
    // the count and the flag in it were stored together, so they always belong to the same completed call.
    const AiModuleState* state = g_aiState;
    const uint32_t word = state->workerPublished.load(std::memory_order_acquire);
    const bool disabled = (word & kWorkerPublishedDisabledBit) != 0u;
    const uint32_t failures = word & ~kWorkerPublishedDisabledBit;

    int32_t st = XPE_AI_WORKER_NOT_USED;
    if (state->useWorker) {
        st = disabled ? XPE_AI_WORKER_DISABLED : XPE_AI_WORKER_ACTIVE;
    }
    *stateOut = st;
    if (consecutiveFailuresOut) *consecutiveFailuresOut = failures;
    if (ceilingOut) *ceilingOut = kWorkerFailureCeiling;
    return XPE_OK;
}

XPE_API void xpe_ai_shutdown(void)
{
    if (!g_aiState) return;

    auto* state = g_aiState;
    std::lock_guard<std::mutex> lock(state->mtx);

    // Mark as not initialized first (prevents new calls)
    state->initialized.store(false, std::memory_order_release);

    // --- Worker process shutdown ---
    // Stub: In the full implementation, this would:
    //   1. Send SHUTDOWN message over IPC
    //   2. Wait for worker process to exit (with timeout)
    //   3. Close named pipe handle
    //   4. Terminate worker process if it does not exit gracefully

    AI_LOG_INFO("xpe_ai shutdown: worker_pid=%u", state->workerPid);

    // QA-B-161: the session is this module's to free. Released before the
    // state is deleted, and before modelDirPath is cleared, so the destructor
    // still runs while the object it belongs to is intact.
    state->boneSuppressSession.reset();
    state->boneSuppressSessionDir.clear();

    // QA-B-171C: ends the worker (graceful, then terminate): nothing outlives the module.
    state->workerSupervisor.reset();

    state->loadedModels.clear();
    state->modelDirPath.clear();
    state->pipeHandle = nullptr;
    state->workerPid = 0;

    // Free state and null the global pointer
    g_aiState = nullptr;
    delete state;
}

XPE_API XpeErrorCode xpe_bodypart_recognize(const XpeImageBuffer* img,
                                             char* bodyPartOut, size_t bufLen,
                                             float* confidenceOut)
{
    // Pre-conditions
    // Required-pointer NULL checks run before the initialisation guard, per
    // the api-spec error-code precedence contract (#119). Order only; the
    // checks themselves are unchanged.
    XpeErrorCode ec = checkNotNull(img);
    if (ec != XPE_OK) return ec;
    ec = checkNotNull(bodyPartOut);
    if (ec != XPE_OK) return ec;

    ec = checkInitialized();
    if (ec != XPE_OK) return ec;

    ec = validateImageBuffer(img);
    if (ec != XPE_OK) return ec;

    // #142 (QA-B-42): a declared size of 0 means the output argument does not
    // exist -- INVALID_INPUT. BUFFER_TOO_SMALL is for a buffer that is real but
    // short, and is decided below against the label actually produced. Before
    // this, a short-but-non-zero buffer received a TRUNCATED label with no error
    // at all (the trap QA-B-39 documented).
    if (bufLen == 0) return XPE_ERR_INVALID_INPUT;

    // --- Stub implementation ---
    // Full implementation: send BODYPART_RECOGNIZE over IPC, await response.
    //
    // Fallback routing logic (REQ-AI-002):
    //   1. Send request to worker with timeout
    //   2. If worker responds with confidence < threshold:
    //      - If fallbackMode: return XPE_ERR_PROCESSING_FAILED
    //      - Else: return the low-confidence result
    //   3. If worker times out or crashes:
    //      - Return XPE_ERR_PROCESSING_FAILED (trigger caller fallback)

    // Return placeholder result indicating no inference performed.
    // Caller should fall back to deterministic body-part lookup.
    if (confidenceOut) *confidenceOut = 0.0f;

    static const char kStubLabel[] = "UNKNOWN";
    if (bufLen < sizeof(kStubLabel)) {
        return XPE_ERR_BUFFER_TOO_SMALL;
    }
    std::memcpy(bodyPartOut, kStubLabel, sizeof(kStubLabel));

    // In stub mode, we signal that AI is not available.
    // The caller should use deterministic fallback.
    return XPE_ERR_PROCESSING_FAILED;
}

XPE_API XpeErrorCode xpe_stitch_images(const XpeImageBuffer* parts,
                                        uint32_t partCount,
                                        XpeImageBuffer* stitchedOut,
                                        const char* configJsonOrNull)
{
    // Pre-conditions
    // Required-pointer NULL checks run before the initialisation guard, per
    // the api-spec error-code precedence contract (#119). Order only; the
    // checks themselves are unchanged.
    if (!parts || partCount < 2 || !stitchedOut) return XPE_ERR_INVALID_INPUT;

    XpeErrorCode ec = checkInitialized();
    if (ec != XPE_OK) return ec;

    // Validate all input parts
    for (uint32_t i = 0; i < partCount; ++i) {
        ec = validateImageBuffer(&parts[i]);
        if (ec != XPE_OK) return ec;
    }

    // Validate output buffer.
    // #142 (QA-B-42): a NULL data pointer or a declared size of 0 is a missing
    // argument, not a small one -- INVALID_INPUT, matching what
    // xpe_bone_suppress already answered for the same shape of fault. The two
    // used to disagree, so a caller needed two branches for one condition.
    // BUFFER_TOO_SMALL stays reserved for a real buffer that cannot hold the
    // result.
    if (!stitchedOut->data || stitchedOut->dataSize == 0) {
        return XPE_ERR_INVALID_INPUT;
    }

    // --- Stub implementation ---
    // Full implementation: send STITCH_IMAGES over IPC with serialized parts,
    // await response containing stitched pixel data.

    (void)configJsonOrNull; // Suppress unused parameter warning

    return XPE_ERR_PROCESSING_FAILED;
}

XPE_API XpeErrorCode xpe_stitch_estimate_size(const XpeImageBuffer* parts,
                                               uint32_t partCount,
                                               uint32_t* widthOut,
                                               uint32_t* heightOut)
{
    // Pre-conditions
    if (!parts || partCount < 2 || !widthOut || !heightOut) {
        return XPE_ERR_INVALID_INPUT;
    }

    // Validate all input parts
    for (uint32_t i = 0; i < partCount; ++i) {
        XpeErrorCode ec = validateImageBuffer(&parts[i]);
        if (ec != XPE_OK) return ec;
    }

    // --- Deterministic size estimation ---
    // Estimate based on input dimensions. For N overlapping images
    // with typical 20-30% overlap, a simple heuristic:
    //   width = max(parts.width) * (1 + 0.7 * (partCount - 1))
    //   height = max(parts.height)
    // Clamped to 4096x4096 maximum.

    uint32_t maxWidth = 0;
    uint32_t maxHeight = 0;
    for (uint32_t i = 0; i < partCount; ++i) {
        if (parts[i].width > maxWidth)  maxWidth = parts[i].width;
        if (parts[i].height > maxHeight) maxHeight = parts[i].height;
    }

    // Conservative estimate: each additional part adds ~70% width.
    const float overlapFactor = 0.7f;
    float estimatedWidth = static_cast<float>(maxWidth) *
        (1.0f + overlapFactor * static_cast<float>(partCount - 1));

    *widthOut  = static_cast<uint32_t>(estimatedWidth);
    *heightOut = maxHeight;

    // Clamp to maximum supported size.
    if (*widthOut > 4096) *widthOut = 4096;
    if (*heightOut > 4096) *heightOut = 4096;

    return XPE_OK;
}

XPE_API XpeErrorCode xpe_bone_suppress(const XpeImageBuffer* img,
                                        XpeImageBuffer* softTissueOut,
                                        const char* configJsonOrNull)
{
    // Pre-conditions
    // Required-pointer NULL checks run before the initialisation guard, per
    // the api-spec error-code precedence contract (#119). Order only; the
    // checks themselves are unchanged.
    XpeErrorCode ec = checkNotNull(img);
    if (ec != XPE_OK) return ec;
    ec = checkNotNull(softTissueOut);
    if (ec != XPE_OK) return ec;

    ec = checkInitialized();
    if (ec != XPE_OK) return ec;

    ec = validateImageBuffer(img);
    if (ec != XPE_OK) return ec;
    ec = validateImageBuffer(softTissueOut);
    if (ec != XPE_OK) return ec;

    // Dimensions must match
    if (img->width != softTissueOut->width ||
        img->height != softTissueOut->height) {
        return XPE_ERR_INVALID_INPUT;
    }

    // QA-B-161 (#130): the first C ABI function wired to a real session.
    //
    // WHY THIS ONE. It is float image in, float image out, which is what the
    // model already is -- so the "a different model gives different numbers"
    // assertion carries up from OnnxSession to the C ABI unchanged. It also
    // writes to a SEPARATE output buffer, which leaves the input intact as a
    // control: an echo is detectable. xpe_dl_denoise works in place and would
    // destroy that control; xpe_bodypart_recognize would need a float->label
    // rule the model does not supply, i.e. a second untested thing.
    //
    // REQ-AI-050: U-Net architecture trained on DES paired data.
    // REQ-AI-051: Quality target: pulmonary nodule sensitivity +16.8%.
    // NEITHER IS MET. The model here is whatever sits at
    // <modelDir>/bone_suppress.onnx; this wires the path, not the clinical
    // claim, and the tests use a toy scale model.
    (void)configJsonOrNull;

    // Float pixels only. The session speaks float32 and silently reinterpreting
    // 16-bit pixels as floats would produce numbers rather than an error.
    if (img->format != XPE_PIXEL_FLOAT32 || softTissueOut->format != XPE_PIXEL_FLOAT32) {
        return XPE_ERR_UNSUPPORTED_FORMAT;
    }

    auto* state = g_aiState;
    if (!state) return XPE_ERR_NOT_INITIALIZED;

    // The declared size, computed ONCE and overflow-checked; the buffer checks below and every copy use
    // this same number (Codex audit #12). width and height are each below 2^32, so their product fits
    // 64 bits; it is the BYTE count that can wrap, so the pixel count is bounded by division first.
    // (xpe_ai validateImageBuffer already refuses an image above the module maximum; this is the
    // function's own guarantee, and it holds without it.)
    const uint64_t pixels = static_cast<uint64_t>(img->width) * img->height;
    if (pixels == 0 || pixels > SIZE_MAX / sizeof(float)) return XPE_ERR_INVALID_INPUT;
    const size_t count = static_cast<size_t>(pixels);
    const size_t bytes = count * sizeof(float);
    if (img->dataSize < bytes || softTissueOut->dataSize < bytes) {
        return XPE_ERR_INVALID_INPUT;
    }

    std::lock_guard<std::mutex> lock(state->mtx);
#ifdef XPE_AI_TEST_HOOKS
    if (auto* hook = g_testMutexHeldHook.load(std::memory_order_acquire)) hook();   // the mutex IS held here
#endif

    // QA-B-171C (REQ-AI-092, REQ-AI-002, SDD-002 "AI worker failure -> return input unchanged"):
    // opt-in worker path. A failure of any kind -- budget exceeded, a worker that died or went silent,
    // an answer that was not one -- returns the INPUT, unchanged, with a non-OK code.
    //
    // WHY THE INPUT AND NOT THE IN-PROCESS RESULT. REQ-AI-003 runs inference in a separate process so
    // the main process is crash-immune. A worker that failed because of the MODEL would, re-run
    // in-process, bring that same risk into the process the isolation exists to protect. The
    // fallback is deterministic and cannot crash: copy the input. (The first draft of this card re-ran
    // the inference in-process; that was corrected.)
    //
    // THE RETURN CODE. XPE_OK is documented as "the AI succeeded" (ONNX build only), so a failed call
    // never returns it: the worker's own error code (or the transport's) is returned, which is the
    // documented signal to use the original image (REQ-AI-002). The copy is for a caller that uses
    // the output buffer anyway: it holds the input, never stale or half-written pixels.
    //
    // WHAT COUNTS (leader decision, Codex audit #12): EVERY non-OK result of the worker path counts toward
    // the ceiling, including an ERROR frame that a perfectly HEALTHY worker sent on purpose because the
    // model refused the request. A model that refuses three times in a row means AI is unusable for this
    // session, and counting such refusals separately would bring back an unbounded alert stream. A healthy
    // worker whose model keeps failing is therefore switched off after 3; xpe_ai_shutdown() followed by
    // xpe_ai_init() recovers it.
    //
    // THE POLICY AROUND IT (user-approved 2026-10-01, REQ-CHANGE-LOG-P3-AI.md row 3, which replaced
    // row 2): EVERY failure raises one Warning alert -- a budget overrun must alert, REQ-AI-092 -- and
    // after kWorkerFailureCeiling CONSECUTIVE failures the worker is switched off for the rest of the
    // session: its process is ended, the alert of that last failure says so, and later calls return the
    // input at once, without starting a worker and WITHOUT further alerts, with
    // XPE_ERR_PROCESSING_FAILED (the documented fallback signal). So a run of CONSECUTIVE failures raises
    // kWorkerFailureCeiling alerts and then stops; there is no per-session cap: a success resets the count,
    // so intermittent failures (fail, fail, succeed, ...) are never blocked and alert on EVERY failure.
    // xpe_ai_shutdown/xpe_ai_init begin a new session with a clean count.
    if (state->useWorker) {
        if (state->workerDisabled) {
            std::memmove(softTissueOut->data, img->data, bytes);
            return XPE_ERR_PROCESSING_FAILED;
        }
        const XpeErrorCode wrc = boneSuppressViaWorker(state, img, softTissueOut);
        if (wrc == XPE_OK) {
            state->workerConsecutiveFailures = 0;
            pushAiProcessedAlert();
            publishWorkerState(state);   // after everything the call does: the snapshot is of a finished call
            return XPE_OK;
        }
        std::memmove(softTissueOut->data, img->data, bytes);
        ++state->workerConsecutiveFailures;
        AI_LOG_WARN("bone_suppress: worker path failed (%d), input returned unchanged "
                    "(%u of %u consecutive failures)",
                    static_cast<int>(wrc), static_cast<unsigned>(state->workerConsecutiveFailures),
                    static_cast<unsigned>(kWorkerFailureCeiling));
        char msg[256];
        if (state->workerConsecutiveFailures >= kWorkerFailureCeiling) {
            state->workerDisabled = true;
            state->workerSupervisor.reset();   // ends the worker process
            std::snprintf(msg, sizeof(msg),
                          "AI worker failed (code %d, failure %u of %u) and is disabled for this "
                          "session: input images are returned unchanged (REQ-AI-002, REQ-AI-092)",
                          static_cast<int>(wrc), static_cast<unsigned>(state->workerConsecutiveFailures),
                          static_cast<unsigned>(kWorkerFailureCeiling));
        } else {
            std::snprintf(msg, sizeof(msg),
                          "AI worker failed (code %d, failure %u of %u): the input image is returned "
                          "unchanged (REQ-AI-002, REQ-AI-092)",
                          static_cast<int>(wrc), static_cast<unsigned>(state->workerConsecutiveFailures),
                          static_cast<unsigned>(kWorkerFailureCeiling));
        }
        xpe_alert_push(msg, XPE_ALERT_WARNING);
        // Published only now: the count, the switch-off, the end of the worker process and the alert are
        // all done. A status query before this point still reports the previous completed call.
        publishWorkerState(state);
        return wrc;
    }

    const std::string modelPath = state->modelDirPath.empty()
        ? std::string("bone_suppress.onnx")
        : state->modelDirPath + "/bone_suppress.onnx";

    // Lazy load, and reload when init pointed somewhere else.
    if (!state->boneSuppressSession || state->boneSuppressSessionDir != state->modelDirPath) {
        xpe::ai::OnnxSessionConfig cfg;
        cfg.model_path = modelPath;
        cfg.execution_provider = xpe::ai::ExecutionProvider::kCpu;
        cfg.num_threads = 1;

        auto created = xpe::ai::OnnxSession::Create(cfg);
        if (!created.has_value()) {
            // Three causes, three codes -- a caller that gets one code for all
            // of them cannot tell "install the model" from "the model is
            // broken" from "inference failed".
            switch (created.code) {
                case xpe::ai::OnnxErrorCode::kInvalidModelPath:
                    AI_LOG_ERROR("bone_suppress: no model at %s", modelPath.c_str());
                    return XPE_ERR_IO_FAILED;
                case xpe::ai::OnnxErrorCode::kModelLoadFailed:
                    AI_LOG_ERROR("bone_suppress: model unreadable: %s", created.message.c_str());
                    return XPE_ERR_CONFIG_INVALID;
                default:
                    AI_LOG_ERROR("bone_suppress: session failed: %s", created.message.c_str());
                    return XPE_ERR_PROCESSING_FAILED;
            }
        }
        state->boneSuppressSession = std::move(created.value);
        state->boneSuppressSessionDir = state->modelDirPath;
    }

    const float* in = static_cast<const float*>(img->data);
    const std::vector<float> input(in, in + count);

    auto out = state->boneSuppressSession->Run(input);
    if (out.code != xpe::ai::OnnxErrorCode::kOk) {
        // A stub build lands here every time (Run returns kModelLoadFailed
        // there), which keeps the documented stub outcome unchanged.
        AI_LOG_ERROR("bone_suppress: run failed: %s", out.message.c_str());
        return (out.code == xpe::ai::OnnxErrorCode::kInvalidInput)
             ? XPE_ERR_INVALID_INPUT
             : XPE_ERR_PROCESSING_FAILED;
    }
    if (out.value.size() != count) {
        AI_LOG_ERROR("bone_suppress: model returned %zu values, expected %zu",
                     out.value.size(), count);
        return XPE_ERR_PROCESSING_FAILED;
    }

    std::memcpy(softTissueOut->data, out.value.data(), bytes);

    // SRS-ALERT-004 (QA-B-168, #130): DL processing was applied -- Info,
    // "AI-processed".
    //
    // PLACEMENT IS THE CONTRACT. This sits after the memcpy, on the single
    // success exit, so it cannot fire on a path that returned an image the
    // model never touched. Every early return above -- unsupported format, no
    // model, unloadable model, a failed Run (which is EVERY call in a stub
    // build) -- leaves the queue untouched. Hoisting it earlier would make an
    // "always fires" alert that still passes a test asserting only that it
    // fires; the negative assertions in test_alert_ai_processed.cpp are what
    // pin this position.
    //
    // WHY Info AND WHY HERE. SRS-ALERT-004 reads "DL processing 적용됨 / Info /
    // AI-processed label" (XPE-SRS-001:102). It is the DL-applied, i.e. SUCCESS,
    // alert; a worker failure is not ALERT-004 (the failure alert cites
    // REQ-AI-002 and REQ-AI-092, see the use_worker path above). Severity
    // settles it on its own: every failure row in the SRS alert table is
    // Warning or Error.
    //
    // The module raises it, not the GUI: all 20 product xpe_alert_push call
    // sites live under modules/ (QA-B-167), and clients/ only reads the queue.
    pushAiProcessedAlert();
    return XPE_OK;
}

XPE_API XpeErrorCode xpe_dl_denoise(XpeImageBuffer* img,
                                     const XpeImageMetadata* meta,
                                     const char* configJsonOrNull)
{
    // Pre-conditions
    // Required-pointer NULL checks run before the initialisation guard, per
    // the api-spec error-code precedence contract (#119). Order only; the
    // checks themselves are unchanged.
    XpeErrorCode ec = checkNotNull(img);
    if (ec != XPE_OK) return ec;
    ec = checkNotNull(meta);
    if (ec != XPE_OK) return ec;

    ec = checkInitialized();
    if (ec != XPE_OK) return ec;

    ec = validateImageBuffer(img);
    if (ec != XPE_OK) return ec;

    // --- Stub implementation ---
    // Full implementation: send DL_DENOISE over IPC.
    // Model variant selected based on meta->bodyPart and meta->mAs.
    // REQ-AI-020: Self-supervised denoising (N2N, N2S, N2V, Noise2Sim).
    // REQ-AI-022: Latency target <= 500 ms on CPU, <= 100 ms on GPU.

    (void)configJsonOrNull;

    return XPE_ERR_PROCESSING_FAILED;
}

XPE_API XpeErrorCode xpe_ai_get_model_card(const char* modelId,
                                             char* buf, size_t bufSize)
{
    // Pre-conditions
    // Required-pointer NULL checks run before the initialisation guard, per
    // the api-spec error-code precedence contract (#119). Order only; the
    // checks themselves are unchanged.
    if (!modelId || !buf) return XPE_ERR_INVALID_INPUT;

    XpeErrorCode ec = checkInitialized();
    if (ec != XPE_OK) return ec;

    // #142 (QA-B-42): zero-length output buffer is a missing argument.
    if (bufSize == 0) return XPE_ERR_INVALID_INPUT;

    // Look up model in loaded models list
    auto* state = g_aiState;
    std::lock_guard<std::mutex> lock(state->mtx);

    bool found = false;
    for (const auto& id : state->loadedModels) {
        if (id == modelId) {
            found = true;
            break;
        }
    }

    // Build model card JSON
    // REQ-AI-010: Return model card with all required fields.
    // REQ-AI-011: JSON conforms to schemas/model-card.schema.json.
    std::string cardJson;
    if (found) {
        cardJson = buildStubModelCard(modelId);
    } else {
        // Model not loaded -- return minimal card indicating unavailable
        cardJson = std::string("{"
            "\"model_id\":\"") + modelId + "\","
            "\"error\":\"model_not_loaded\","
            "\"model_version\":\"N/A\","
            "\"limitations\":\"Model not found or not loaded in this session.\""
        "}";
    }

    // Copy to caller buffer
    size_t copyLen = (cardJson.size() < bufSize - 1)
                     ? cardJson.size() : bufSize - 1;
    std::memcpy(buf, cardJson.c_str(), copyLen);
    buf[copyLen] = '\0';

    if (cardJson.size() >= bufSize) {
        return XPE_ERR_BUFFER_TOO_SMALL;
    }

    return found ? XPE_OK : XPE_ERR_IO_FAILED;
}

XPE_API XpeErrorCode xpe_ai_set_fallback_mode(int32_t enable)
{
    XpeErrorCode ec = checkInitialized();
    if (ec != XPE_OK) return ec;

    auto* state = g_aiState;
    state->fallbackMode.store(enable != 0, std::memory_order_release);

    // Notify worker about fallback mode change via IPC.
    // Stub: no-op until IPC bridge is implemented.

    AI_LOG_INFO("Fallback mode %s", enable ? "enabled" : "disabled");
    return XPE_OK;
}

} // extern "C"
