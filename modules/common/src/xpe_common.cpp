/**
 * @file xpe_common.cpp
 * @brief xpe_common.dll implementation -- lifecycle, config, param range,
 *        error strings, alert queue, logging, and AED subsystem.
 *
 * IEC 62304 Class B -- Software Unit Implementation.
 * All exported symbols are extern "C"; no C++ exceptions cross the C ABI.
 */

#ifndef XPE_DLL_EXPORT
#define XPE_DLL_EXPORT
#endif

#include "xpe/common/xpe_common_api.h"

#include <array>
#include <atomic>
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

// @MX:NOTE: AED state: 0=IDLE, 1=ARMED, 2=TRIGGERED (REQ-P0-028)
struct AedEvent {
    int32_t  eventType{0};
    uint64_t timestamp{0};
    float    signalLevel{0.0f};
};

struct AedConfig {
    bool     enabled{true};
    int32_t  triggerThresholdAdu{500};
    int32_t  settleTimeMs{100};
    int32_t  minExposureMs{5};
    int32_t  maxExposureMs{5000};
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

// AED subsystem
static AedConfig         g_aedConfig;
static std::atomic<int32_t> g_aedState{0};   // 0=IDLE, 1=ARMED, 2=TRIGGERED
static std::deque<AedEvent>  g_aedEvents;
static std::mutex            g_aedMutex;
static bool                  g_aedConfigured{false};

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

        // Reset AED subsystem (separate mutex, safe outside g_mutex)
        {
            std::lock_guard<std::mutex> aedLk(g_aedMutex);
            g_aedConfigured = false;
            g_aedState.store(0);
            g_aedEvents.clear();
            g_aedConfig = AedConfig{};
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

        // Reset AED
        {
            std::lock_guard<std::mutex> aedLk(g_aedMutex);
            g_aedConfigured = false;
            g_aedState.store(0);
            g_aedEvents.clear();
        }

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
        case XPE_STATUS_NO_EVENT:          return "No pending event";
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
 * ============================================================================ */

XPE_API XpeErrorCode xpe_log_set_level(int32_t level)
{
    if (level < 0 || level > 5) return XPE_ERR_INVALID_INPUT;

    std::lock_guard<std::mutex> lk(g_mutex);
    g_logLevel = level;
    return XPE_OK;
}

XPE_API XpeErrorCode xpe_log_set_file(const char* filePath)
{
    std::lock_guard<std::mutex> lk(g_mutex);

    // Close existing file sink
    if (g_logFile.is_open()) {
        g_logFile.flush();
        g_logFile.close();
    }

    if (!filePath) {
        // Revert to stderr
        g_logFilePath.clear();
        return XPE_OK;
    }

    g_logFile.open(filePath, std::ios::app);
    if (!g_logFile.is_open()) {
        g_logFilePath.clear();
        return XPE_ERR_IO_FAILED;
    }

    g_logFilePath = filePath;
    return XPE_OK;
}

XPE_API void xpe_log_flush(void)
{
    std::lock_guard<std::mutex> lk(g_mutex);
    if (g_logFile.is_open()) {
        g_logFile.flush();
    } else {
        std::fflush(stderr);
    }
}

/* ============================================================================
 * AED subsystem (REQ-P0-026 .. REQ-P0-028)
 * ============================================================================ */

// @MX:ANCHOR: xpe_aed_configure is the AED entry point; called from C# P/Invoke.
// @MX:REASON: [AUTO] fan_in >= 3 callers expected (GUI, test harness, batch pipeline).
XPE_API XpeErrorCode xpe_aed_configure(const char* configJsonOrNull)
{
    {
        std::lock_guard<std::mutex> lk(g_mutex);
        if (!g_initialized) return XPE_ERR_NOT_INITIALIZED;
    }

    try {
        std::lock_guard<std::mutex> aedLk(g_aedMutex);

        AedConfig cfg;  // start from defaults

        if (configJsonOrNull) {
            // Minimal JSON validation
            const char* p = configJsonOrNull;
            while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
            if (*p != '{') return XPE_ERR_CONFIG_INVALID;

            // Simple key extraction without pulling in nlohmann/json for
            // this minimal implementation. For production, use nlohmann::json.
            auto extract_int = [](const char* json, const char* key, int32_t& out) -> bool {
                const char* pos = std::strstr(json, key);
                if (!pos) return false;
                pos = std::strchr(pos, ':');
                if (!pos) return false;
                ++pos;
                while (*pos == ' ') ++pos;
                char* end = nullptr;
                long v = std::strtol(pos, &end, 10);
                if (end == pos) return false;
                out = static_cast<int32_t>(v);
                return true;
            };

            int32_t val = 0;
            if (extract_int(configJsonOrNull, "\"trigger_threshold_adu\"", val))
                cfg.triggerThresholdAdu = val;
            if (extract_int(configJsonOrNull, "\"settle_time_ms\"", val))
                cfg.settleTimeMs = val;
            if (extract_int(configJsonOrNull, "\"min_exposure_ms\"", val))
                cfg.minExposureMs = val;
            if (extract_int(configJsonOrNull, "\"max_exposure_ms\"", val))
                cfg.maxExposureMs = val;
        }

        // Validate ranges
        if (cfg.triggerThresholdAdu < 0 ||
            cfg.settleTimeMs < 0          ||
            cfg.minExposureMs < 0         ||
            cfg.maxExposureMs < cfg.minExposureMs) {
            return XPE_ERR_CONFIG_INVALID;
        }

        g_aedConfig     = cfg;
        g_aedConfigured = true;
        g_aedState.store(1);  // ARMED
        g_aedEvents.clear();

        return XPE_OK;
    } catch (...) {
        return XPE_ERR_CONFIG_INVALID;
    }
}

XPE_API XpeErrorCode xpe_aed_poll_event(int32_t* eventTypeOut,
                                          uint64_t* timestampOut,
                                          float* signalLevelOut)
{
    if (!eventTypeOut || !timestampOut || !signalLevelOut)
        return XPE_ERR_INVALID_INPUT;

    {
        std::lock_guard<std::mutex> lk(g_mutex);
        if (!g_initialized) return XPE_ERR_NOT_INITIALIZED;
    }

    std::lock_guard<std::mutex> aedLk(g_aedMutex);

    if (g_aedEvents.empty()) {
        return XPE_STATUS_NO_EVENT;
    }

    const AedEvent& ev = g_aedEvents.front();
    *eventTypeOut   = ev.eventType;
    *timestampOut   = ev.timestamp;
    *signalLevelOut = ev.signalLevel;
    g_aedEvents.pop_front();

    // Transition back to ARMED if queue is now empty
    if (g_aedEvents.empty() && g_aedState.load() == 2) {
        g_aedState.store(1);  // back to ARMED
    }

    return XPE_OK;
}

XPE_API XpeErrorCode xpe_aed_get_status(int32_t* stateOut)
{
    if (!stateOut) return XPE_ERR_INVALID_INPUT;

    {
        std::lock_guard<std::mutex> lk(g_mutex);
        if (!g_initialized) return XPE_ERR_NOT_INITIALIZED;
    }

    *stateOut = g_aedState.load();
    return XPE_OK;
}

/* ============================================================================
 * Internal test-support: inject AED event (not exported, used from unit tests
 * via white-box linkage).
 * ============================================================================ */

/** @cond INTERNAL */
extern "C" {

XPE_API void xpe_test_inject_aed_event(int32_t eventType, float signalLevel)
{
    std::lock_guard<std::mutex> aedLk(g_aedMutex);
    AedEvent ev;
    ev.eventType   = eventType;
    ev.timestamp   = unix_epoch_ms();
    ev.signalLevel = signalLevel;
    g_aedEvents.push_back(ev);
    g_aedState.store(2);  // TRIGGERED
}

XPE_API void xpe_test_inject_alert(const char* msg, int32_t severity)
{
    enqueue_alert(msg, severity);
}

} // extern "C"
/** @endcond */
