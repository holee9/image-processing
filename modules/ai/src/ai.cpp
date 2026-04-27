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

#include <cstdint>
#include <cstring>
#include <atomic>
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
    if (img->dataSize == 0) return XPE_ERR_INVALID_INPUT;
    // Maximum: 4096x4096x4 = 64 MB
    const size_t maxBytes = static_cast<size_t>(4096) * 4096 * 4;
    if (img->dataSize > maxBytes) return XPE_ERR_INVALID_INPUT;
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
    XpeErrorCode ec = checkInitialized();
    if (ec != XPE_OK) return ec;

    ec = checkNotNull(img);
    if (ec != XPE_OK) return ec;
    ec = checkNotNull(bodyPartOut);
    if (ec != XPE_OK) return ec;

    ec = validateImageBuffer(img);
    if (ec != XPE_OK) return ec;

    if (bufLen < 1) return XPE_ERR_BUFFER_TOO_SMALL;

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
    std::strncpy(bodyPartOut, "UNKNOWN", bufLen);
    if (bufLen > 0) {
        bodyPartOut[bufLen - 1] = '\0';
    }

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
    XpeErrorCode ec = checkInitialized();
    if (ec != XPE_OK) return ec;

    if (!parts || partCount < 2 || !stitchedOut) return XPE_ERR_INVALID_INPUT;

    // Validate all input parts
    for (uint32_t i = 0; i < partCount; ++i) {
        ec = validateImageBuffer(&parts[i]);
        if (ec != XPE_OK) return ec;
    }

    // Validate output buffer
    if (!stitchedOut->data || stitchedOut->dataSize == 0) {
        return XPE_ERR_BUFFER_TOO_SMALL;
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
    XpeErrorCode ec = checkInitialized();
    if (ec != XPE_OK) return ec;

    ec = checkNotNull(img);
    if (ec != XPE_OK) return ec;
    ec = checkNotNull(softTissueOut);
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

    // --- Stub implementation ---
    // Full implementation: send BONE_SUPPRESS over IPC, await soft-tissue image.
    // REQ-AI-050: U-Net architecture trained on DES paired data.
    // REQ-AI-051: Quality target: pulmonary nodule sensitivity +16.8%.

    (void)configJsonOrNull;

    return XPE_ERR_PROCESSING_FAILED;
}

XPE_API XpeErrorCode xpe_dl_denoise(XpeImageBuffer* img,
                                     const XpeImageMetadata* meta,
                                     const char* configJsonOrNull)
{
    // Pre-conditions
    XpeErrorCode ec = checkInitialized();
    if (ec != XPE_OK) return ec;

    ec = checkNotNull(img);
    if (ec != XPE_OK) return ec;
    ec = checkNotNull(meta);
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
    XpeErrorCode ec = checkInitialized();
    if (ec != XPE_OK) return ec;

    if (!modelId || !buf) return XPE_ERR_INVALID_INPUT;
    if (bufSize < 1) return XPE_ERR_BUFFER_TOO_SMALL;

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
