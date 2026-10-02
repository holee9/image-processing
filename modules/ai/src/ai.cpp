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
#include "ai_finite.h"
#include "ai_bodypart.h"
#include "ai_bodypart_decision.h"
#include "ai_bodypart_model.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <cstdint>
#include <cmath>
#include <cstring>
#include <atomic>
#include <charconv>
#include <fstream>
#include <memory>
#include <mutex>
#include <new>
#ifdef XPE_AI_TEST_HOOKS
#include <thread>   // the before-state-delete probe (test hook only)
#endif
#include <string>
#include <vector>

// @MX:NOTE: [AUTO] nlohmann/json included for config parsing;
//           conditional compilation avoids hard dependency.
#ifdef XPE_AI_USE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif

#include "ai_log.h"   // AI_LOG_* (spdlog when XPE_AI_USE_SPDLOG, else printf)
#ifdef XPE_AI_TEST_LOG_CAPTURE
#include <spdlog/sinks/callback_sink.h>
#include <algorithm>
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
using xpe::ai::BodyPartModel;   // defined in ai_bodypart_model.h, shared with the worker

struct AiModuleState {
    std::mutex mtx;

    /** True after xpe_ai_init() succeeds, false after xpe_ai_shutdown(). */
    std::atomic<bool> initialized{false};

    /** True when fallback mode is active (default: true per REQ-AI-002). */
    std::atomic<bool> fallbackMode{true};

    /** Path to the model directory (set by xpe_ai_init). */
    std::string modelDirPath;

    /** The configuration text the session was started with ("" for NULL): what a second xpe_ai_init is compared to (D8). */
    std::string configJson;

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

    /** The body-part model (QA-B-191); null until the first call that needs it succeeds. Guarded by `mtx`. */
    std::unique_ptr<BodyPartModel> bodyPart;

    /** Directory `bodyPart` was built from (same reason as boneSuppressSessionDir). */
    std::string bodyPartDir;

    /**
     * True once this session has told the operator that the body-part model is unusable. ONE Warning per
     * xpe_ai_init, on the first such call (QA-B-191 D4): a user who switched AI on with no model must be able
     * to find out, and a missing model repeats on every call so a per-call alert would fill the queue.
     */
    bool bodyPartUnavailableWarned{false};

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
        // QA-B-194 M1 (REQ-AI-090): every format the enum has gets its pixel size, UINT8 included -- it used to
        // fall through with bpp 0, so a UINT8 image declaring 4000 x 4000 and supplying one byte passed (measured).
        // A value that is not one of the three formats is not an image at all.
        uint32_t bpp = 0u;
        if (img->format == XPE_PIXEL_UINT8)        bpp = 1u;
        else if (img->format == XPE_PIXEL_UINT16)  bpp = 2u;
        else if (img->format == XPE_PIXEL_FLOAT32) bpp = 4u;
        if (bpp == 0u) return XPE_ERR_INVALID_INPUT;
        {
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
 * @brief Judge the acquisition metadata a caller hands to xpe_dl_denoise (QA-B-194 M3, REQ-AI-090, design D3).
 *
 * The metadata chooses the model variant and scales the noise estimate ("mAs"), so a NaN or a negative dose is not
 * a harmless label: it would pick a variant, or scale a model, by garbage. What is checked, and no more:
 *   - bodyPart is a NUL-terminated C string within its 64 bytes (the fixed-size field is read as a string);
 *   - kVp, mAs, SID_mm and pixelPitch_mm are finite and not negative. Zero means "unknown", as everywhere else in
 *     the metadata (xpe_types.h), so zero is accepted.
 * acquisitionTime and flags are not judged: any 64-bit time and any bit pattern is a value they can hold (0 means
 * unknown for the time), and the flags are written by the stages, not read as input here.
 * No range is invented either (a "plausible kVp" is clinical knowledge nobody gave): finite and non-negative is the
 * whole contract.
 */
static XpeErrorCode validateDenoiseMetadata(const XpeImageMetadata* meta) {
    if (std::memchr(meta->bodyPart, '\0', sizeof(meta->bodyPart)) == nullptr) return XPE_ERR_INVALID_INPUT;
    const float physical[] = {meta->kVp, meta->mAs, meta->SID_mm, meta->pixelPitch_mm};
    for (const float v : physical) {
        if (!std::isfinite(v) || v < 0.0f) return XPE_ERR_INVALID_INPUT;
    }
    return XPE_OK;
}

/**
 * @brief Is @p id a model identifier: 1 to 64 characters of [A-Za-z0-9._-] (QA-B-194 M3, design D4).
 *
 * The identifier is copied verbatim into the JSON the model card returns (both the real card and the "model not
 * loaded" card), so a quote or a backslash in it made the card invalid JSON (measured). The grammar is the file
 * names the model directory holds, and it is judged before the identifier is used for anything. The scan stops at
 * the 65th byte, so it never reads further into a caller's string than a legal identifier plus its terminator.
 */
static bool isValidModelId(const char* id) {
    size_t n = 0;
    for (; n <= 64 && id[n] != '\0'; ++n) {
        const char ch = id[n];
        const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
                        ch == '.' || ch == '_' || ch == '-';
        if (!ok) return false;
    }
    return n >= 1 && n <= 64;
}

/**
 * @brief Validate the parts of one stitch call: each is a valid image, and they are all the SAME pixel format.
 *
 * QA-B-194 M2 (REQ-AI-090, design D6). The parts of one stitch are one kind of image; a UINT16 part among float
 * parts (measured: accepted, estimate 19 x 8) has no meaning for any stitching. A SIZE rule between parts is NOT
 * checked: how the parts relate in size depends on the stitching algorithm, which does not exist yet, and a rule
 * made up now would be a requirement nobody gave. The count is judged by the caller BEFORE this runs (it must not
 * read an element of an array it cannot trust to hold that many).
 */
static XpeErrorCode validateStitchParts(const XpeImageBuffer* parts, uint32_t partCount) {
    for (uint32_t i = 0; i < partCount; ++i) {
        const XpeErrorCode ec = validateImageBuffer(&parts[i]);
        if (ec != XPE_OK) return ec;
    }
    for (uint32_t i = 1; i < partCount; ++i) {
        if (parts[i].format != parts[0].format) return XPE_ERR_INVALID_INPUT;
    }
    return XPE_OK;
}

/**
 * @brief Refuse a float image that holds NaN or infinity, BEFORE anything is written (QA-B-194 M1, REQ-AI-090).
 *
 * The consumer's rule, the same as the preprocess entry points (api-spec "non-finite input"): one non-finite pixel
 * and the call returns XPE_ERR_INVALID_INPUT with every output untouched and ONE XPE_ALERT_ERROR that names the
 * count and the first pixel and says the INPUT is at fault. Before this the image went on to the model, which
 * produced a non-finite result, and the alert that came out blamed the MODEL's output ("AI model output was
 * non-finite") for a fault that was the caller's.
 *
 * Only XPE_PIXEL_FLOAT32 can hold one (integer formats cannot). The image has been through validateImageBuffer, so
 * width * height * 4 bytes are readable. The scan is the branch-free AllFinite the module already runs on every
 * model output (3072 x 3072: about 1.4 ms with the pixels in cache, QA-B-194 design section 3); the count and the
 * position are found by a second pass over the failing frame only.
 *
 * @param prefix  the alert's fixed tag, e.g. "XPE_WARN_BONE_SUPPRESS_INPUT_NOT_FINITE:"
 * @param what    "the input frame" or "input part 2"
 * @param tail    what the call did NOT do, per function
 * CROSS-LANE CONTRACT: clients may match these texts; ai_api.h records them.
 */
static XpeErrorCode checkImageFinite(const XpeImageBuffer* img, const char* prefix, const char* what,
                                     const char* tail) {
    if (img->format != XPE_PIXEL_FLOAT32) return XPE_OK;
    const size_t n = static_cast<size_t>(img->width) * img->height;
    if (xpe::ai::AllFinite(img->data, n)) return XPE_OK;
    size_t count = 0, first = 0;
    const unsigned char* b = static_cast<const unsigned char*>(img->data);
    for (size_t i = 0; i < n; ++i) {
        uint32_t u;
        std::memcpy(&u, b + i * sizeof(uint32_t), sizeof(u));
        if ((u & 0x7F800000u) == 0x7F800000u) {
            if (count == 0) first = i;
            ++count;
        }
    }
    char msg[320];
    std::snprintf(msg, sizeof(msg), "%s %zu pixel(s) of %s are NaN or infinite (first: index %zu, x=%zu, y=%zu); %s",
                  prefix, count, what, first, first % img->width, first / img->width, tail);
    xpe_alert_push(msg, XPE_ALERT_ERROR);
    return XPE_ERR_INVALID_INPUT;
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
        // QA-B-194 M4 (D7): the log line alone is silence to the caller; the alert is what a client can show.
        // RETURN CODE UNCHANGED (the #145 line: a config that cannot be used is a warning, not an error).
        xpe_alert_push("ai config is not valid JSON and was ignored: every setting uses its default",
                       XPE_ALERT_WARNING);
        return;
    }
    if (!cfg.is_object()) {
        // Valid JSON that is not an object ([], 5, "x", null, true): there is no key to read, and before M4 it was
        // accepted without a word.
        xpe_alert_push("ai config is valid JSON but not an object and was ignored: every setting uses its default",
                       XPE_ALERT_WARNING);
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
        // QA-B-194 M4 (D7): the value is a number of milliseconds that becomes a uint32_t deadline. The old
        // get<int>() + static_cast turned -1 into 4294967295 ms (about 49 days) and silently truncated anything
        // above int range. Accepted: 0 (= the default, as everywhere timeoutMs is used) up to 2^31 - 1; anything
        // else is ignored with an alert and the default stays.
        const nlohmann::json& t = cfg["timeout_ms"];
        constexpr int64_t kMaxTimeoutMs = 2147483647;
        bool inRange = false;
        int64_t value = 0;
        if (t.is_number_unsigned()) {
            const uint64_t u = t.get<uint64_t>();
            inRange = u <= static_cast<uint64_t>(kMaxTimeoutMs);
            value = static_cast<int64_t>(u);
        } else {
            value = t.get<int64_t>();
            inRange = value >= 0 && value <= kMaxTimeoutMs;
        }
        if (inRange) {
            state->timeoutMs = static_cast<uint32_t>(value);
        } else {
            char msg[192];
            std::snprintf(msg, sizeof(msg),
                          "ai config key 'timeout_ms' is out of range (0 to 2147483647 ms) and was ignored: "
                          "the default is used (value: %s)",
                          t.dump().c_str());
            xpe_alert_push(msg, XPE_ALERT_WARNING);
        }
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
                                          XpeImageBuffer* out, bool* nonFiniteResult) {
    *nonFiniteResult = false;
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
        const XpeErrorCode rc = state->workerSupervisor->BoneSuppress(
            in->width, in->height, static_cast<const float*>(in->data), static_cast<float*>(out->data));
        *nonFiniteResult = rc != XPE_OK && state->workerSupervisor->LastResultWasNonFinite();
        return rc;
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

#ifdef XPE_AI_TEST_LOG_CAPTURE
// TEST-ONLY (QA-B-177). Compiled only with XPE_AI_TEST_HOOKS and spdlog (modules/ai/CMakeLists.txt defines
// XPE_AI_TEST_LOG_CAPTURE for exactly that); a build with the XPE_AI_TEST_HOOKS option OFF has neither this
// code nor the exported setter. The DLL carries its OWN spdlog, so a test cannot reach its default logger
// from the test process: this adds a callback sink to it, which sees each message exactly as spdlog
// formatted it -- what a log file would contain -- not as the macro was asked to.
static std::shared_ptr<spdlog::sinks::sink> g_testLogSink;

extern "C" XPE_API void xpe_ai_test_set_log_capture(void (*cb)(int level, const char* message)) {
    auto logger = spdlog::default_logger();
    if (!logger) return;   // no default logger (after shutdown/drop_all): nothing to attach to
    auto& sinks = logger->sinks();
    if (g_testLogSink) {
        sinks.erase(std::remove(sinks.begin(), sinks.end(), g_testLogSink), sinks.end());
        g_testLogSink.reset();
    }
    if (cb) {
        g_testLogSink = std::make_shared<spdlog::sinks::callback_sink_mt>(
            [cb](const spdlog::details::log_msg& m) {
                const std::string text(m.payload.data(), m.payload.size());
                cb(static_cast<int>(m.level), text.c_str());
            });
        sinks.push_back(g_testLogSink);
    }
}
#endif

#ifdef XPE_AI_TEST_HOOKS
// TEST-ONLY (QA-B-173, Codex audit #19). Compiled only when modules/ai/CMakeLists.txt defines
// XPE_AI_TEST_HOOKS, i.e. when the XPE_AI_TEST_HOOKS option is ON. Its default is ON whenever the module's
// tests are built, and every preset builds them, so a build whose DLL is DELIVERED must turn it OFF
// (-DXPE_AI_TEST_HOOKS=OFF, e.g. set in the release preset). With the option OFF the DLL has neither this
// variable, nor the call in xpe_bone_suppress, nor the exported setter.
// A test registers a callback that xpe_bone_suppress and xpe_ai_get_model_card call on the calling thread
// immediately after they have locked the module mutex (QA-B-181: a hook that THROWS places an exception inside the
// critical section deterministically, with no dependence on the allocator or the code page), so the test KNOWS a call is inside its critical section (and, with a frozen
// worker, stuck there) rather than inferring it from timing.
static std::atomic<void (*)(void)> g_testMutexHeldHook{nullptr};

extern "C" XPE_API void xpe_ai_test_set_mutex_held_hook(void (*hook)(void)) {
    g_testMutexHeldHook.store(hook, std::memory_order_release);
}

// TEST-ONLY (QA-B-178). Called by xpe_ai_shutdown immediately BEFORE it deletes the module state, with
// whether the state's mutex is still locked at that moment. A locked mutex means a lock_guard that is still
// in scope will unlock it AFTER the delete (undefined behaviour, even single-threaded). The probe runs on a
// helper thread because try_lock by the owning thread is itself undefined for std::mutex. Compiled only with
// the same XPE_AI_TEST_HOOKS option as the hook above.
static std::atomic<void (*)(int)> g_testBeforeStateDeleteHook{nullptr};

extern "C" XPE_API void xpe_ai_test_set_before_state_delete_hook(void (*hook)(int mutexStillHeld)) {
    g_testBeforeStateDeleteHook.store(hook, std::memory_order_release);
}

// TEST-ONLY (QA-B-195 M3). The model loader calls this after it has verified a model's signature and before it builds
// the session from the verified bytes (OnnxSession::TestSetAfterVerifyHook, ai_onnx_session.cpp). A test registers a
// callback that changes the files on disk at exactly that point, to show end to end that what was verified is what is
// used -- the model AND the sidecar the body-part labels come from. Same XPE_AI_TEST_HOOKS option as the hooks above.
namespace xpe::ai {
void TestSetAfterVerifyHook(void (*hook)(const std::string& modelPath));
}
static std::atomic<void (*)(const char*)> g_testAfterVerifyHook{nullptr};
static void afterVerifyTrampoline(const std::string& modelPath) {
    if (auto* hook = g_testAfterVerifyHook.load(std::memory_order_acquire)) hook(modelPath.c_str());
}
extern "C" XPE_API void xpe_ai_test_set_after_verify_hook(void (*hook)(const char* modelPath)) {
    g_testAfterVerifyHook.store(hook, std::memory_order_release);
    xpe::ai::TestSetAfterVerifyHook(hook ? &afterVerifyTrampoline : nullptr);
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

/**
 * The Warning that says the worker is switched off for the session (QA-B-191 M4e, leader decision replacing D10).
 * The count is shared by xpe_bone_suppress and xpe_bodypart_recognize, so BOTH stop using the worker whichever of
 * them failed the third time; the text says both effects and which function's failure caused it, because an alert
 * that named only the failing function would leave the other function's change unexplained.
 * CROSS-LANE CONTRACT: clients may match it.
 */
static void pushWorkerDisabledAlert(XpeErrorCode code, uint32_t failures, const char* cause) {
    char msg[320];
    std::snprintf(msg, sizeof(msg),
                  "AI worker failed (code %d, failure %u of %u) during %s and is disabled for this session: "
                  "body-part recognition returns UNKNOWN, bone suppression returns the input image unchanged "
                  "(REQ-AI-002, REQ-AI-092)",
                  static_cast<int>(code), static_cast<unsigned>(failures), static_cast<unsigned>(kWorkerFailureCeiling),
                  cause);
    xpe_alert_push(msg, XPE_ALERT_WARNING);
}

/** SRS-ALERT-004: DL processing was applied (Info). One place, so both paths say the same thing. */
static void pushNonFiniteResultAlert() {
    // CROSS-LANE CONTRACT (QA-B-181i, reworded in 181j): clients may match this text. Only what was observed:
    // the model's output was non-finite and this image was not AI-processed. Nothing about the worker -- the
    // in-process path has none, and its state is reported by xpe_ai_worker_state().
    xpe_alert_push("AI model output was non-finite (inf/NaN); this image was not AI-processed",
                   XPE_ALERT_WARNING);
}

static void pushAiProcessedAlert() {
    xpe_alert_push("AI-processed: bone suppression applied (SRS-ALERT-004)", XPE_ALERT_INFO);
}

/* ==========================================================================
 * Body-part recognition helpers (QA-B-191, #130, T-006)
 *
 * THE MODELS THE TESTS USE ARE NOT CLASSIFIERS (tests/data/make_bodypart_models.py). What is wired and tested here
 * is the path: a number the model produces becomes the label, the confidence and the decision. Nothing here says
 * how well any model recognises a body part.
 * ========================================================================== */

/** The label written when there is no usable answer; it is the stub's label, so a caller sees one outcome. */
static const char kBodyPartUnknown[] = "UNKNOWN";

/** Write "UNKNOWN" and report the documented fallback signal. A buffer too short for it is BUFFER_TOO_SMALL, as in the stub. */
static XpeErrorCode bodyPartUnknown(char* bodyPartOut, size_t bufLen) {
    if (bufLen < sizeof(kBodyPartUnknown)) return XPE_ERR_BUFFER_TOO_SMALL;
    std::memcpy(bodyPartOut, kBodyPartUnknown, sizeof(kBodyPartUnknown));
    return XPE_ERR_PROCESSING_FAILED;
}

/** Build the model for this session's model directory; the loading rules are shared with the worker (ai_bodypart_model.h). */
static const char* loadBodyPartModel(AiModuleState* state, std::unique_ptr<BodyPartModel>* out) {
    std::string detail;
    const char* why = xpe::ai::LoadBodyPartModel(state->modelDirPath, out, nullptr, &detail);
    if (why && !detail.empty()) AI_LOG_ERROR("bodypart: %s", detail.c_str());
    return why;
}

/**
 * REQ-AI-012: the low-confidence event. ONE Warning per image whose confidence is below the threshold (leader
 * decision QA-B-191 D3: Warning and not Info, because with fallback_mode off the low-confidence label is used
 * and an exposure parameter chosen from a wrong body part can be wrong; one per image is not a flood).
 * @p labelUsed is the label that is returned anyway (fallback_mode off), or null when UNKNOWN is returned.
 * The text is xpe::ai::LowConfidenceAlertText, shared with the worker path (CROSS-LANE CONTRACT: clients may
 * match it).
 */
static void pushLowConfidenceAlert(float confidence, float threshold, const char* labelUsed) {
    const std::string msg = xpe::ai::LowConfidenceAlertText(confidence, threshold, labelUsed);
    xpe_alert_push(msg.c_str(), XPE_ALERT_WARNING);
}

/** The model's output was not a probability vector. One text for both paths. CROSS-LANE CONTRACT (QA-B-191): clients may match it. */
static void pushBodyPartNotProbabilityAlert() {
    xpe_alert_push("AI body-part model output is not a probability vector (a value outside [0, 1]); "
                   "this image was not AI-classified", XPE_ALERT_WARNING);
}

/** Tell the operator once per session that body-part recognition has no usable model (D4). The caller holds state->mtx. */
static void warnBodyPartUnavailableOnce(AiModuleState* state, const char* reason) {
    if (state->bodyPartUnavailableWarned) return;
    state->bodyPartUnavailableWarned = true;
    char msg[256];
    std::snprintf(msg, sizeof(msg),
                  "AI body-part recognition is unavailable (%s): UNKNOWN is returned; use the deterministic body-part "
                  "lookup (REQ-AI-002)", reason);
    xpe_alert_push(msg, XPE_ALERT_WARNING);
}

/**
 * What the module does with an ANSWER of the model (REQ-AI-012 / REQ-AI-002): a confidence below the threshold
 * is a low-confidence EVENT and, while fallback_mode is on (the default), the documented fallback outcome.
 * `>=` passes: a confidence exactly at the threshold is not low. The event does not depend on the caller's
 * buffer: it is posted before any label is written. The in-process path and the worker path both end here, so
 * the same answer gets the same decision whichever process computed it. Caller holds state->mtx.
 */
static XpeErrorCode decideBodyPart(AiModuleState* state, const std::string& label, float confidence,
                                   char* bodyPartOut, size_t bufLen, float* confidenceOut) {
    const float threshold = state->confidenceThreshold;
    if (confidence < threshold) {
        const bool fallbackOn = state->fallbackMode.load(std::memory_order_acquire);
        pushLowConfidenceAlert(confidence, threshold, fallbackOn ? nullptr : label.c_str());
        if (fallbackOn) {
            // The caller is told why it must fall back: UNKNOWN, and the confidence that was actually measured.
            const XpeErrorCode rc = bodyPartUnknown(bodyPartOut, bufLen);
            if (rc == XPE_ERR_PROCESSING_FAILED && confidenceOut) *confidenceOut = confidence;
            return rc;
        }
        // fallback_mode off: the low-confidence label is returned, with its confidence, and the Warning above.
    }

    if (label.size() + 1 > bufLen) return XPE_ERR_BUFFER_TOO_SMALL;   // never truncated silently

    std::memcpy(bodyPartOut, label.c_str(), label.size() + 1);
    if (confidenceOut) *confidenceOut = confidence;
    return XPE_OK;
}

/**
 * @brief xpe_bodypart_recognize through the worker process (opt-in `use_worker`, QA-B-191 M4c). Caller holds state->mtx.
 *
 * The WORKER runs the model and says what it said (label and confidence, or that the output was refused); THIS
 * process applies the threshold and fallback_mode, because those are its state (decideBodyPart, shared with the
 * in-process path). Outcomes and what each does to the shared failure count (leader decision D7 "S+",
 * QA-B-191 M4: ONE count for the worker, shared with xpe_bone_suppress, with one exception):
 *
 *   a valid answer                         -> count reset to 0, then the decision
 *   the model's output refused (inf/NaN,   -> count reset to 0 (the worker answered correctly), the same alert as
 *   outside [0, 1])                           the in-process path, UNKNOWN
 *   "the model cannot be used" (no file,   -> NOT a worker failure and NOT counted; count reset to 0 (a healthy
 *   unreadable, bad labels or shape)          exchange ends a run of faults); UNKNOWN + the one-per-session
 *                                             "unavailable" Warning, as in the in-process path
 *   anything else (no answer within the    -> counted; a Warning per failure naming the count; the worker is
 *   budget, the process died, a bad reply,    switched off for the session at the ceiling
 *   a model that exists but fails to run)
 *
 * The exception exists because the worker is shared: a deployment with a body-part model missing must not get
 * bone suppression switched off by a configuration state. A model that IS there and fails is a real fault and
 * counts, exactly as for bone suppression (REQ-CHANGE-LOG-P3-AI row 4); bone suppression's own rule is unchanged.
 *
 * One difference from the in-process path, on purpose: a non-FLOAT32 image is refused (UNSUPPORTED_FORMAT) before
 * the worker is asked, whether or not a model exists -- this process does not know whether the worker has one.
 */
static XpeErrorCode bodyPartViaWorker(AiModuleState* state, const XpeImageBuffer* img, char* bodyPartOut,
                                      size_t bufLen, float* confidenceOut) {
    if (img->format != XPE_PIXEL_FLOAT32) return XPE_ERR_UNSUPPORTED_FORMAT;
    // Switched off for the session (by either function's failures): no worker is tried and nothing is raised.
    if (state->workerDisabled) return bodyPartUnknown(bodyPartOut, bufLen);

    xpe::ai::BodyPartReply reply;
    XpeErrorCode rc = XPE_ERR_PROCESSING_FAILED;
    bool unavailable = false;
    try {
        if (!state->workerSupervisor) {
            xpe::ai::WorkerSupervisorConfig cfg;
            cfg.worker_exe = workerExePath();
            if (cfg.worker_exe.empty()) {
                rc = XPE_ERR_IO_FAILED;
            } else {
                cfg.model_dir = state->modelDirPath;
                cfg.timeout_ms = state->timeoutMs != 0 ? state->timeoutMs : XPE_AI_DEFAULT_TIMEOUT_MS;
                state->workerSupervisor = std::make_unique<xpe::ai::WorkerSupervisor>(std::move(cfg));
            }
        }
        if (state->workerSupervisor) {
            rc = state->workerSupervisor->BodyPartRecognize(img->width, img->height,
                                                            static_cast<const float*>(img->data), &reply);
            unavailable = rc != XPE_OK && state->workerSupervisor->LastModelUnavailable();
        }
    } catch (...) {
        rc = XPE_ERR_OUT_OF_MEMORY;
    }

    if (rc == XPE_OK) {
        state->workerConsecutiveFailures = 0;
        if (reply.judgement == xpe::ai::BodyPartJudgement::kNonFinite) {
            AI_LOG_ERROR("bodypart: the worker reports a non-finite model result");
            pushNonFiniteResultAlert();
            publishWorkerState(state);
            return bodyPartUnknown(bodyPartOut, bufLen);
        }
        if (reply.judgement != xpe::ai::BodyPartJudgement::kOk) {
            AI_LOG_ERROR("bodypart: the worker reports a model result outside [0, 1]");
            pushBodyPartNotProbabilityAlert();
            publishWorkerState(state);
            return bodyPartUnknown(bodyPartOut, bufLen);
        }
        publishWorkerState(state);
        return decideBodyPart(state, std::string(reply.label), reply.confidence, bodyPartOut, bufLen, confidenceOut);
    }

    if (unavailable) {
        state->workerConsecutiveFailures = 0;   // the worker answered: healthy, and its model is a state, not a fault
        warnBodyPartUnavailableOnce(state, "the AI worker has no usable body-part model");
        publishWorkerState(state);
        return bodyPartUnknown(bodyPartOut, bufLen);
    }

    ++state->workerConsecutiveFailures;
    AI_LOG_WARN("bodypart: worker path failed (%d), UNKNOWN returned (%u of %u consecutive failures)",
                static_cast<int>(rc), static_cast<unsigned>(state->workerConsecutiveFailures),
                static_cast<unsigned>(kWorkerFailureCeiling));
    if (state->workerConsecutiveFailures >= kWorkerFailureCeiling) {
        state->workerDisabled = true;
        state->workerSupervisor.reset();   // ends the worker process
        pushWorkerDisabledAlert(rc, state->workerConsecutiveFailures, "body-part recognition");
    } else {
        char msg[256];
        std::snprintf(msg, sizeof(msg),
                      "AI worker failed (code %d, failure %u of %u): body-part recognition returns UNKNOWN; "
                      "use the deterministic body-part lookup (REQ-AI-002, REQ-AI-092)",
                      static_cast<int>(rc), static_cast<unsigned>(state->workerConsecutiveFailures),
                      static_cast<unsigned>(kWorkerFailureCeiling));
        xpe_alert_push(msg, XPE_ALERT_WARNING);
    }
    // Published last: the count, the switch-off, the end of the worker process and the alert are all done.
    publishWorkerState(state);
    return bodyPartUnknown(bodyPartOut, bufLen);
}

/* ==========================================================================
 * Exported API Implementation
 * ========================================================================== */

// @MX:ANCHOR: [AUTO] xpe_ai_version -- SPEC-XPE-P3-AI, api-spec.md S9
// @MX:REASON: Module identity function; called by orchestrator for readiness check

extern "C" {

// QA-B-181 (QA-B-179, #233): no exception may leave an exported function, and a lock held when one is thrown must
// be released. Under /EHsc an `extern "C"` function that has no try region of its own is compiled as if it never
// throws: when a callee throws, the unwinder does not run that function's destructors, so a std::lock_guard in
// it stays LOCKED for the life of the process (measured: eh_exp, QA-B-181 report 4; the same function with ANY
// try/catch in it, or built with /EHs, releases the lock). The body therefore lives in an ordinary C++ function
// that owns the lock, and the exported function is only a try/catch around the call: the lock is released while
// the exception travels from the helper to the catch. The handlers are empty of work on purpose -- they only
// return a code and allocate nothing.
// The helpers MUST be declared `extern "C++"`: a function declared inside this extern "C" block has C language
// linkage even when it is static, gets the same "never throws" treatment, and the try/catch around a call to it
// is optimised away (measured: with plain `static` helpers the exception still escaped and the lock stayed held).
extern "C++" static XpeErrorCode xpe_bone_suppress_impl(const XpeImageBuffer* img,
                                                         XpeImageBuffer* softTissueOut,
                                                         const char* configJsonOrNull);
extern "C++" static XpeErrorCode xpe_ai_get_model_card_impl(const char* modelId, char* buf, size_t bufSize);
extern "C++" static XpeErrorCode xpe_ai_init_impl(const char* modelDirPath, const char* configJsonOrNull);
extern "C++" static XpeErrorCode xpe_bodypart_recognize_impl(const XpeImageBuffer* img, char* bodyPartOut,
                                                              size_t bufLen, float* confidenceOut);

XPE_API const char* xpe_ai_version(void)
{
    return "0.1.0";
}

XPE_API XpeErrorCode xpe_ai_init(const char* modelDirPath,
                                  const char* configJsonOrNull)
{
    // QA-B-194 M4: no exception crosses the C ABI. The body is an ordinary C++ function (see the note above the
    // xpe_*_impl prototypes) and this is only the try/catch around it.
    try {
        return xpe_ai_init_impl(modelDirPath, configJsonOrNull);
    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}

extern "C++" static XpeErrorCode xpe_ai_init_impl(const char* modelDirPath,
                                                  const char* configJsonOrNull)
{
    // Validate required parameter
    if (!modelDirPath) return XPE_ERR_INVALID_INPUT;

    // QA-B-194 M4 (D1): an empty directory is a missing argument, like a zero-length buffer (#142). It used to be
    // accepted, and the model was then looked up relative to the working directory -- the opposite of the worker's
    // "next to the DLL, never the working directory" rule. Judged before the already-initialised short-circuit
    // below: an argument is wrong whatever the module's state is.
    if (modelDirPath[0] == '\0') return XPE_ERR_INVALID_INPUT;

    // If already initialized, return success (idempotent)
    if (g_aiState && g_aiState->initialized.load(std::memory_order_acquire)) {
        AI_LOG_WARN("xpe_ai_init called while already initialized -- ignoring");
        // QA-B-194 M4 (D8): the call stays OK and ignored (a client test holds "ignored" as the contract), but when
        // it asked for something DIFFERENT the silence hid that the new settings did not take effect. Different means
        // not byte-identical: the directory text or the config text.
        const std::string newConfig = configJsonOrNull ? configJsonOrNull : "";
        if (g_aiState->modelDirPath != modelDirPath || g_aiState->configJson != newConfig) {
            xpe_alert_push("xpe_ai_init was called again with a different model directory or config while the module "
                           "is already initialised: the call was ignored and the first settings stay in effect "
                           "(call xpe_ai_shutdown first to change them)",
                           XPE_ALERT_WARNING);
        }
        return XPE_OK;
    }

    // Allocate module state. QA-B-194 M4: owned by a unique_ptr until the very last step, so an allocation failure
    // or an exception in parseConfig / the string copies frees it and leaves g_aiState exactly as it was (before,
    // such an exception both leaked the state and left the caller with an exception through a C ABI).
    std::unique_ptr<AiModuleState> state(new (std::nothrow) AiModuleState());
    if (!state) return XPE_ERR_OUT_OF_MEMORY;

    // Store model directory and the config the session starts with
    state->modelDirPath = modelDirPath;
    state->configJson = configJsonOrNull ? configJsonOrNull : "";

    // Parse optional configuration
    parseConfig(state.get(), configJsonOrNull);

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

    AI_LOG_INFO("xpe_ai initialized: model_dir=%s, ep=%d, timeout=%u ms",
                modelDirPath,
                static_cast<int>(state->executionProvider),
                state->timeoutMs);

    // The one step that cannot throw, and the last: from here the module is initialised.
    g_aiState = state.release();
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
    {   // The lock's scope ENDS before the state is deleted: a lock_guard still alive at `delete state` would
        // unlock a destroyed mutex when the function returns (QA-B-178, Codex #25).
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
    state->bodyPart.reset();
    state->bodyPartDir.clear();

    // QA-B-171C: ends the worker (graceful, then terminate): nothing outlives the module.
    state->workerSupervisor.reset();

    state->loadedModels.clear();
    state->modelDirPath.clear();
    state->pipeHandle = nullptr;
    state->workerPid = 0;

    // Null the global pointer while still locked (nothing can reach the state through it any more) ...
    g_aiState = nullptr;
    }   // ... release the lock, and only then destroy the mutex that guarded it.

#ifdef XPE_AI_TEST_HOOKS
    if (auto* hook = g_testBeforeStateDeleteHook.load(std::memory_order_acquire)) {
        int held = 0;
        std::thread probe([state, &held] {
            held = state->mtx.try_lock() ? 0 : 1;
            if (!held) state->mtx.unlock();
        });
        probe.join();
        hook(held);
    }
#endif
    delete state;
}

XPE_API XpeErrorCode xpe_bodypart_recognize(const XpeImageBuffer* img,
                                             char* bodyPartOut, size_t bufLen,
                                             float* confidenceOut)
{
    // Same shape as xpe_ai_get_model_card: the body is an ordinary C++ function that owns the lock, and this
    // exported function is only the try/catch (see the note above the forward declarations).
    try {
        return xpe_bodypart_recognize_impl(img, bodyPartOut, bufLen, confidenceOut);
    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}

extern "C++" static XpeErrorCode xpe_bodypart_recognize_impl(const XpeImageBuffer* img,
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

    // QA-B-194 M1 (D2): a non-finite input is the caller's fault -- INVALID_INPUT with NOTHING written: not the
    // label "UNKNOWN" (that is the documented fallback for "no usable answer", a different thing) and not the
    // confidence. Judged before the stub return and before the model, so it does not depend on the build or on
    // whether a model exists.
    ec = checkImageFinite(img, "XPE_WARN_BODYPART_INPUT_NOT_FINITE:", "the input frame",
                          "the image was not classified and the label and confidence were not written");
    if (ec != XPE_OK) return ec;

    // Every outcome below that is not a result leaves the confidence at 0.0, as the stub always did.
    if (confidenceOut) *confidenceOut = 0.0f;

    // A stub build has no runtime to ask. Its outcome is the stub's, unchanged and silent: "UNKNOWN", 0.0,
    // PROCESSING_FAILED -- the documented signal for the caller to use the deterministic body-part lookup.
    if (xpe::ai::OnnxSession::IsStubBuild()) return bodyPartUnknown(bodyPartOut, bufLen);

    AiModuleState* state = g_aiState;
    std::lock_guard<std::mutex> lock(state->mtx);

    // use_worker (opt-in): the model runs in the worker process; this process keeps the decision.
    if (state->useWorker) {
        return bodyPartViaWorker(state, img, bodyPartOut, bufLen, confidenceOut);
    }

    // The model, built on the first call that needs it. A model that is not there, cannot be read, has no usable
    // labels or has an input this module will not feed is NOT a call error: it is the documented fallback outcome,
    // with ONE Warning per session so the operator can find out.
    if (!state->bodyPart || state->bodyPartDir != state->modelDirPath) {
        std::unique_ptr<BodyPartModel> built;
        if (const char* why = loadBodyPartModel(state, &built)) {
            state->bodyPart.reset();
            warnBodyPartUnavailableOnce(state, why);
            return bodyPartUnknown(bodyPartOut, bufLen);
        }
        state->bodyPart = std::move(built);
        state->bodyPartDir = state->modelDirPath;
    }
    BodyPartModel& model = *state->bodyPart;

    // Float pixels only (the same refusal as xpe_bone_suppress): the session speaks float32, and reading
    // 16-bit pixels as floats would produce numbers rather than an error. Judged AFTER the model is known to be
    // usable, on purpose: without a model the outcome is the stub's for every image (the stub never looked at the
    // pixel format), and a caller that never had a model must not start getting a new error code from this call.
    if (img->format != XPE_PIXEL_FLOAT32) return XPE_ERR_UNSUPPORTED_FORMAT;

    // Resize to the size the model declares. No intensity normalisation: the caller supplies the model's scale.
    const std::vector<float> input = xpe::ai::ResizeImageFloat(static_cast<const float*>(img->data), img->width,
                                                               img->height, model.inputWidth, model.inputHeight);
    const auto out = model.session->Run(input);
    if (out.code != xpe::ai::OnnxErrorCode::kOk) {
        AI_LOG_ERROR("bodypart: run failed: %s", out.message.c_str());
        return bodyPartUnknown(bodyPartOut, bufLen);
    }
    if (out.value.size() != model.labels.size()) {
        AI_LOG_ERROR("bodypart: the model returned %zu values for %zu labels", out.value.size(), model.labels.size());
        warnBodyPartUnavailableOnce(state, "the model output size differs from the number of labels");
        return bodyPartUnknown(bodyPartOut, bufLen);
    }

    // The module applies no softmax: the model emits probabilities, and a vector that is not one is refused.
    // Judged on the whole result BEFORE anything is written, so a refusal leaves the label as UNKNOWN.
    const xpe::ai::BodyPartVerdict verdict = xpe::ai::JudgeBodyPartOutput(out.value.data(), out.value.size());
    if (verdict.judgement == xpe::ai::BodyPartJudgement::kNonFinite) {
        AI_LOG_ERROR("bodypart: the model result contains a non-finite value");
        pushNonFiniteResultAlert();
        return bodyPartUnknown(bodyPartOut, bufLen);
    }
    if (verdict.judgement != xpe::ai::BodyPartJudgement::kOk) {
        AI_LOG_ERROR("bodypart: the model result has a value outside [0, 1]");
        pushBodyPartNotProbabilityAlert();
        return bodyPartUnknown(bodyPartOut, bufLen);
    }

    // The most probable class; on a tie the first (decided in JudgeBodyPartOutput, shared with the worker path).
    const std::string& label = model.labels[verdict.best];
    const float confidence = verdict.confidence;

    return decideBodyPart(state, label, confidence, bodyPartOut, bufLen, confidenceOut);
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
    // QA-B-194 M2: an upper bound on partCount, judged here with the other arguments and before any parts[i] is read.
    if (!parts || partCount < 2 || partCount > XPE_AI_MAX_STITCH_PARTS || !stitchedOut) return XPE_ERR_INVALID_INPUT;

    XpeErrorCode ec = checkInitialized();
    if (ec != XPE_OK) return ec;

    // Validate all input parts: each valid, all the same format
    ec = validateStitchParts(parts, partCount);
    if (ec != XPE_OK) return ec;

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

    // QA-B-194 M1: every part is scanned before anything is written; the alert names the first part that fails.
    for (uint32_t i = 0; i < partCount; ++i) {
        char what[40];
        std::snprintf(what, sizeof(what), "input part %u", static_cast<unsigned>(i));
        ec = checkImageFinite(&parts[i], "XPE_WARN_STITCH_INPUT_NOT_FINITE:", what,
                              "the parts were not stitched and the output buffer was not written");
        if (ec != XPE_OK) return ec;
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
    // QA-B-194 M2: the same upper bound as xpe_stitch_images, before any parts[i] is read.
    if (!parts || partCount < 2 || partCount > XPE_AI_MAX_STITCH_PARTS || !widthOut || !heightOut) {
        return XPE_ERR_INVALID_INPUT;
    }

    // Validate all input parts: each valid, all the same format
    {
        const XpeErrorCode ec = validateStitchParts(parts, partCount);
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

    // QA-B-181d (Codex #48 census): limited as a float BEFORE it becomes a uint32. The estimate can exceed
    // UINT32_MAX (it is up to 2^32 * 1.7 * ...), where the conversion is undefined and wrapped on x86-64
    // (4294967808 came out as 512). maxWidth is an integer, so the estimate is finite and positive.
    *widthOut  = estimatedWidth >= 4096.0f ? 4096u : static_cast<uint32_t>(estimatedWidth);
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
    try {
        return xpe_bone_suppress_impl(img, softTissueOut, configJsonOrNull);
    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}

extern "C++" static XpeErrorCode xpe_bone_suppress_impl(const XpeImageBuffer* img,
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

    // QA-B-194 M1: a non-finite input is the CALLER's fault, refused here, before the lock, the model or the
    // worker, with the output buffer untouched.
    ec = checkImageFinite(img, "XPE_WARN_BONE_SUPPRESS_INPUT_NOT_FINITE:", "the input frame",
                          "the image was not processed and the output buffer was not changed");
    if (ec != XPE_OK) return ec;

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
    // WHAT COUNTS (leader decisions, Codex audit #12, then #63/#65): the ceiling counts CONSECUTIVE worker and
    // transport faults -- a non-OK result of the worker path, including an ERROR frame that a worker sent on
    // purpose because the model refused the request, which still counts (a model that refuses three times in
    // a row means AI is unusable for this session, and counting such refusals separately would bring back an
    // unbounded alert stream). ONE kind of result is not a fault: a reply with a VALID success envelope whose
    // pixels are non-finite. The bridge validated the envelope (success, size, format) before judging the
    // pixels, so this is the observation "the worker works; the model's result for this image left the finite
    // range". That refuses the image (one alert) and ENDS a run of faults, like any other valid response, so
    // extreme images can never switch AI off -- and a reply with a broken envelope is a protocol fault and
    // counts. There is no per-session total: a session that alternates faults and valid responses is not
    // switched off by them. A worker switched off after 3 consecutive faults is recovered by xpe_ai_shutdown()
    // followed by xpe_ai_init().
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
        bool nonFiniteResult = false;
        const XpeErrorCode wrc = boneSuppressViaWorker(state, img, softTissueOut, &nonFiniteResult);
        if (nonFiniteResult) {
            // QA-B-181i (Codex #63): the worker answered correctly; the MODEL's result for this image left the
            // finite range. That refuses this image and nothing else: the output holds the input (this path's
            // documented fallback), one alert names the cause, and the failure count neither grows nor keeps
            // an earlier run of real faults alive -- a healthy exchange ends a "consecutive" run. A few extreme
            // images must not switch AI off for the rest of the session.
            std::memmove(softTissueOut->data, img->data, bytes);
            state->workerConsecutiveFailures = 0;
            AI_LOG_WARN("bone_suppress: the model result is non-finite, input returned unchanged "
                        "(not counted as a worker failure)");
            pushNonFiniteResultAlert();
            publishWorkerState(state);
            return XPE_ERR_PROCESSING_FAILED;
        }
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
        if (state->workerConsecutiveFailures >= kWorkerFailureCeiling) {
            state->workerDisabled = true;
            state->workerSupervisor.reset();   // ends the worker process
            pushWorkerDisabledAlert(wrc, state->workerConsecutiveFailures, "bone suppression");
        } else {
            char msg[256];
            std::snprintf(msg, sizeof(msg),
                          "AI worker failed (code %d, failure %u of %u): the input image is returned "
                          "unchanged (REQ-AI-002, REQ-AI-092)",
                          static_cast<int>(wrc), static_cast<unsigned>(state->workerConsecutiveFailures),
                          static_cast<unsigned>(kWorkerFailureCeiling));
            xpe_alert_push(msg, XPE_ALERT_WARNING);
        }
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
        cfg.role = "bone_suppress";   // part of what the signature covers (QA-B-195)
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
                case xpe::ai::OnnxErrorCode::kOutOfMemory:
                    // QA-B-194 M5: a shortage of memory is named as one, not as a failed inference.
                    return XPE_ERR_OUT_OF_MEMORY;
                case xpe::ai::OnnxErrorCode::kModelNotTrusted:
                    // QA-B-195 M3: the model or its sidecar failed signature verification. Nothing was loaded. The
                    // same code as "model unreadable": the caller's answer is the same (no model to use). M4 makes
                    // the reason visible to the operator.
                    AI_LOG_ERROR("bone_suppress: %s", created.message.c_str());
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

    // QA-B-181h (Codex #60): a model result that is not finite is not a success. A finite input can
    // still overflow (Y = 2X on a pixel above FLT_MAX / 2), and a model may emit NaN on its own. The
    // judgment is made on the whole result BEFORE the copy, so a refusal leaves softTissueOut exactly as
    // the caller passed it. XPE_ERR_PROCESSING_FAILED (QA-B-181i, Codex #63): the input was valid and finite, so
    // INVALID_INPUT would blame the caller; the model could not give a result. Same code on the worker path.
    if (!xpe::ai::AllFinite(out.value.data(), count)) {
        AI_LOG_ERROR("bone_suppress: the model result contains a non-finite value; output left unchanged");
        pushNonFiniteResultAlert();
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

    // QA-B-194 M3 (D3): the metadata is input too -- judged before the pixels are scanned, and before anything runs.
    ec = validateDenoiseMetadata(meta);
    if (ec != XPE_OK) return ec;

    // QA-B-194 M1: the frame is denoised IN PLACE, so refusing it before anything is read is what keeps it intact.
    ec = checkImageFinite(img, "XPE_WARN_DL_DENOISE_INPUT_NOT_FINITE:", "the input frame",
                          "the image was not denoised and the buffer was not changed");
    if (ec != XPE_OK) return ec;

    // --- Stub implementation ---
    // Full implementation: send DL_DENOISE over IPC.
    // Model variant selected based on meta->bodyPart and meta->mAs.
    // REQ-AI-020: Self-supervised denoising. REQ-AI-021 names the strategies:
    // Noise2Noise, Noise2Self, Neighbor2Neighbor, Noise2Sim.
    // REQ-AI-022: Latency target <= 500 ms on CPU, <= 100 ms on GPU.

    (void)configJsonOrNull;

    return XPE_ERR_PROCESSING_FAILED;
}

XPE_API XpeErrorCode xpe_ai_get_model_card(const char* modelId,
                                             char* buf, size_t bufSize)
{
    try {
        return xpe_ai_get_model_card_impl(modelId, buf, bufSize);
    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}

extern "C++" static XpeErrorCode xpe_ai_get_model_card_impl(const char* modelId, char* buf, size_t bufSize)
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

    // QA-B-194 M3 (D4): the identifier is echoed into JSON, so it must be one a JSON string can carry unescaped.
    if (!isValidModelId(modelId)) return XPE_ERR_INVALID_INPUT;

    // Look up model in loaded models list
    auto* state = g_aiState;
    std::lock_guard<std::mutex> lock(state->mtx);
#ifdef XPE_AI_TEST_HOOKS
    if (auto* hook = g_testMutexHeldHook.load(std::memory_order_acquire)) hook();   // the mutex IS held here
#endif

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
