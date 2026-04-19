/**
 * @file xpe_common.cpp
 * @brief xpe_common.dll implementation -- lifecycle, config, param range,
 *        error strings, alert queue, and logging.
 *
 * IEC 62304 Class B -- Software Unit Implementation.
 * All exported symbols are extern "C"; no C++ exceptions cross the C ABI.
 */

#ifndef XPE_DLL_EXPORT
#define XPE_DLL_EXPORT
#endif

#include "xpe/common/xpe_common_api.h"

#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <mutex>
#include <string>

/* ============================================================================
 * Internal types
 * ============================================================================ */

struct AlertEntry {
    std::string  message;
    int32_t      severity{0};
};

/* ============================================================================
 * Global state
 * @MX:WARN: Global mutable state -- all access must be protected by g_mutex
 * @MX:REASON: [AUTO] DLL has single-process lifetime; static state is required
 *             for P/Invoke bridge which cannot carry context pointers.
 * ============================================================================ */

static std::mutex        g_mutex;
static bool              g_initialized{false};
static std::string       g_configJson;

// Alert queue (bounded ring -- max 64 entries)
static constexpr std::size_t kAlertQueueMax = 64;
static std::deque<AlertEntry> g_alertQueue;

// Logging
static int32_t           g_logLevel{2};      // default INFO
static std::string       g_logFilePath;
static std::ofstream     g_logFile;

/* Version string -- semantic versioning */
static const char* kVersionString = "0.1.0";

/* ============================================================================
 * Internal helpers
 * ============================================================================ */

/** Simple log sink -- writes to g_logFile if open, else stderr. */
static void internal_log(int32_t level, const char* msg)
{
    if (level < g_logLevel) return;

    const char* levelStr[] = {"TRACE", "DEBUG", "INFO ", "WARN ", "ERROR", "OFF  "};
    const char* tag = (level >= 0 && level <= 5) ? levelStr[level] : "?????";

    char buf[512];
    std::snprintf(buf, sizeof(buf), "[XPE][%s] %s\n", tag, msg);

    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_logFile.is_open()) {
        g_logFile << buf;
    } else {
        std::fputs(buf, stderr);
    }
}

/** Enqueue an alert, evicting oldest entry when queue is full. */
static void enqueue_alert(const char* msg, int32_t severity)
{
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_alertQueue.size() >= kAlertQueueMax) {
        g_alertQueue.pop_front();
    }
    AlertEntry e;
    e.message  = msg ? msg : "";
    e.severity = severity;
    g_alertQueue.push_back(std::move(e));
}

/** Returns current UNIX epoch milliseconds. */
static uint64_t unix_epoch_ms()
{
    using namespace std::chrono;
    return static_cast<uint64_t>(
        duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

/* ============================================================================
 * Lifecycle
 * ============================================================================ */

// @MX:ANCHOR: xpe_init is the root initialisation entry point for all subsystems.
// @MX:REASON: [AUTO] Called by C# GUI on startup (REQ-P0-031); fan_in >= 3.
XPE_API XpeErrorCode xpe_init(const char* configJsonOrNull)
{
    // Validate config before acquiring lock to avoid partial state on error
    if (configJsonOrNull && configJsonOrNull[0] == '\0') {
        return XPE_ERR_CONFIG_INVALID;
    }

    try {
        {
            std::lock_guard<std::mutex> lk(g_mutex);

            g_initialized   = true;
            g_logLevel      = 2;  // INFO
            g_alertQueue.clear();

            if (configJsonOrNull) {
                g_configJson = configJsonOrNull;
            }
        }

        // internal_log acquires g_mutex; must be called after releasing it
        internal_log(2, "xpe_init: library initialised");
        return XPE_OK;
    } catch (...) {
        return XPE_ERR_OUT_OF_MEMORY;
    }
}

XPE_API void xpe_shutdown(void)
{
    try {
        std::lock_guard<std::mutex> lk(g_mutex);
        if (!g_initialized) return;

        g_initialized = false;
        g_alertQueue.clear();
        g_configJson.clear();

        // Flush and close log file
        if (g_logFile.is_open()) {
            g_logFile.flush();
            g_logFile.close();
        }
    } catch (...) {
        /* no-op -- shutdown must not throw */
    }
}

XPE_API const char* xpe_version(void)
{
    return kVersionString;
}

XPE_API XpeErrorCode xpe_configure(const char* jsonConfig)
{
    if (!jsonConfig) return XPE_ERR_INVALID_INPUT;

    try {
        // Minimal JSON validity check: must start with '{'
        const char* p = jsonConfig;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
        if (*p != '{') return XPE_ERR_CONFIG_INVALID;

        std::lock_guard<std::mutex> lk(g_mutex);
        g_configJson = jsonConfig;
        return XPE_OK;
    } catch (...) {
        return XPE_ERR_CONFIG_INVALID;
    }
}

XPE_API XpeErrorCode xpe_get_param_range(const char* bodyPart, const char* paramName,
                                          float* minVal, float* maxVal, float* defaultVal)
{
    if (!bodyPart || !paramName || !minVal || !maxVal || !defaultVal)
        return XPE_ERR_INVALID_INPUT;

    // @MX:ANCHOR: [AUTO] Init guard — REQ-GUI-IT-040
    // @MX:REASON: Parameter range lookup must reject pre-init calls
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        if (!g_initialized) return XPE_ERR_NOT_INITIALIZED;
    }

    // Body-part whitelist (REQ-GUI-IT-026): reject unknown anatomy strings
    // TODO: Replace with ParameterValidator backed by JSON catalog
    static const char* const kKnownBodyParts[] = {
        "CHEST", "ABDOMEN", "PELVIS", "SPINE", "SKULL", "HEAD", "EXTREMITY"
    };
    bool validBodyPart = false;
    for (const char* known : kKnownBodyParts) {
        if (std::strcmp(bodyPart, known) == 0) { validBodyPart = true; break; }
    }
    if (!validBodyPart) return XPE_ERR_INVALID_INPUT;

    // Static parameter range table (SRS-SAFE-002, SRS-SAFE-005)
    struct ParamRange { const char* param; float mn; float mx; float def; };
    static const ParamRange kRanges[] = {
        { "windowWidth",   100.0f, 10000.0f, 2000.0f },
        { "windowCenter",   50.0f,  5000.0f, 1000.0f },
        { "gamma",           0.1f,     3.0f,    1.0f },
        { "brightness",     -1.0f,     1.0f,    0.0f },
        { "contrast",        0.1f,     5.0f,    1.0f },
        { nullptr,           0.0f,     0.0f,    0.0f }
    };

    for (const ParamRange* r = kRanges; r->param; ++r) {
        if (std::strcmp(r->param, paramName) == 0) {
            (void)bodyPart; // body-part scoping reserved for future use
            *minVal     = r->mn;
            *maxVal     = r->mx;
            *defaultVal = r->def;
            return XPE_OK;
        }
    }

    // Unknown parameter -- return generic defaults
    *minVal     = 0.0f;
    *maxVal     = 1.0f;
    *defaultVal = 0.5f;
    return XPE_OK;
}

/* ============================================================================
 * Error string
 * ============================================================================ */

XPE_API const char* xpe_error_string(XpeErrorCode code)
{
    switch (code) {
        case XPE_OK:                       return "Success";
        case XPE_ERR_INVALID_INPUT:        return "Invalid input parameter";
        case XPE_ERR_OUT_OF_MEMORY:        return "Out of memory";
        case XPE_ERR_PROCESSING_FAILED:    return "Processing failed";
        case XPE_ERR_CONFIG_INVALID:       return "Invalid configuration";
        case XPE_ERR_CALIBRATION_EXPIRED:  return "Calibration data expired";
        case XPE_ERR_NOT_INITIALIZED:      return "Module not initialized";
        case XPE_ERR_UNSUPPORTED_FORMAT:   return "Unsupported pixel format";
        case XPE_ERR_BUFFER_TOO_SMALL:     return "Buffer too small";
        case XPE_ERR_IO_FAILED:            return "I/O operation failed";
        case XPE_ERR_NETWORK_FAILED:       return "Network operation failed";
        case XPE_ERR_SAFETY_VIOLATION:     return "Safety violation";
        case XPE_ERR_INTERNAL:             return "Internal processing error";
        case XPE_ERR_DICOM_INVALID:        return "Invalid or malformed DICOM file";
        case XPE_ERR_DICOM_CONFORMANCE:    return "DICOM conformance validation failed";
        default:                           return "Unknown error";
    }
}

/* ============================================================================
 * Alert queue
 * ============================================================================ */

XPE_API int32_t xpe_get_pending_alert_count(void)
{
    std::lock_guard<std::mutex> lk(g_mutex);
    return static_cast<int32_t>(g_alertQueue.size());
}

XPE_API XpeErrorCode xpe_get_pending_alert(int32_t index, char* msg, size_t msgLen,
                                            int32_t* severity)
{
    if (!msg || msgLen == 0 || !severity || index < 0)
        return XPE_ERR_INVALID_INPUT;

    std::lock_guard<std::mutex> lk(g_mutex);

    if (static_cast<std::size_t>(index) >= g_alertQueue.size())
        return XPE_ERR_INVALID_INPUT;

    const AlertEntry& e = g_alertQueue[static_cast<std::size_t>(index)];

    if (e.message.size() + 1 > msgLen)
        return XPE_ERR_BUFFER_TOO_SMALL;

    strncpy_s(msg, msgLen, e.message.c_str(), msgLen - 1);
    *severity = e.severity;
    return XPE_OK;
}

XPE_API void xpe_clear_alerts(void)
{
    std::lock_guard<std::mutex> lk(g_mutex);
    g_alertQueue.clear();
}

/* ============================================================================
 * Logging subsystem (REQ-P0-023 .. REQ-P0-025)
 *
 * NOTE: The public logging API (xpe_log_set_level, xpe_log_set_file,
 * xpe_log_flush) is now implemented in xpe_logging.cpp using spdlog.
 * Duplicate definitions were removed here to fix LNK2005 multi-definition
 * errors at link time. The internal globals (g_logLevel, g_logFilePath,
 * g_logFile) remain in this translation unit because internal_log() and
 * xpe_shutdown() still reference them for lifecycle + alert-queue diagnostics.
 * ============================================================================ */

/* ============================================================================
 * Internal test-support helpers (white-box linkage for unit tests).
 * ============================================================================ */

/** @cond INTERNAL */
extern "C" {

XPE_API void xpe_test_inject_alert(const char* msg, int32_t severity)
{
    enqueue_alert(msg, severity);
}

} // extern "C"
/** @endcond */
