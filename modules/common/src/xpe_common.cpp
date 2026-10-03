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

#include <nlohmann/json.hpp>

#include <array>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <mutex>
#include <new>
#include <string>

/* Defined in xpe_logging.cpp -- releases the custom spdlog file sink and
 * installs a null-sink default logger. Declared here rather than in a public
 * header: it is an internal lifecycle hook, not exported from the DLL.
 * extern "C" matches its definition, which sits inside the extern "C" block
 * of xpe_logging.cpp (lines 43-158). */
extern "C" void xpe_log_internal_reset();
extern "C" void xpe_log_internal_init();

/* ============================================================================
 * Internal types
 * ============================================================================ */

struct AlertEntry {
    std::string  message;
    int32_t      severity{0};
    /* The synthetic overflow-loss alert (api-spec.md 5.17 rule 2). Marked
     * rather than matched by message text: a module could legitimately push an
     * alert whose text starts the same way, and that one must stay evictable. */
    bool         isLossAlert{false};
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

// Alert queue (bounded -- max 64 entries; overflow policy: api-spec.md 5.17)
static constexpr std::size_t kAlertQueueMax = 64;
static std::deque<AlertEntry> g_alertQueue;
// Cumulative evictions since the last xpe_clear_alerts. Drives the loss alert.
static uint64_t g_alertsDropped{0};

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

/* Alert queue overflow policy -- api-spec.md 5.17 (SRS-ALERT-007, HAZ-006).
 *
 * Callers below already hold g_mutex; the *_locked suffix marks that.
 *
 * @MX:ANCHOR: eviction order is Info -> Warning -> Error, FIFO within a class.
 * @MX:REASON: [AUTO] HAZ-006 -- a burst of Info alerts must never silently
 *             discard an Error, and no eviction may go unreported.
 * @MX:SPEC: api-spec.md 5.17
 */

/** Frees one slot by evicting the oldest evictable alert; counts the loss.
 *  Returns false when nothing is evictable (only the loss alert remains). */
static bool evict_one_locked()
{
    for (int32_t sev : {XPE_ALERT_INFO, XPE_ALERT_WARNING, XPE_ALERT_ERROR}) {
        for (auto it = g_alertQueue.begin(); it != g_alertQueue.end(); ++it) {
            if (it->isLossAlert || it->severity != sev) continue;
            g_alertQueue.erase(it);   // first match == oldest: the deque is FIFO
            ++g_alertsDropped;
            return true;
        }
    }
    return false;
}

/** Brings the loss alert in line with g_alertsDropped: updates it in place when
 *  present, otherwise makes room and appends it. Never creates a second one. */
static void sync_loss_alert_locked()
{
    if (g_alertsDropped == 0) return;

    AlertEntry* loss = nullptr;
    for (auto& e : g_alertQueue) {
        if (e.isLossAlert) { loss = &e; break; }
    }

    if (loss == nullptr) {
        // The loss alert occupies one of the 64 slots, so making room for it is
        // itself an eviction and is counted as one. This comes first: the text below states the count, and
        // these evictions are part of it.
        while (g_alertQueue.size() >= kAlertQueueMax) {
            if (!evict_one_locked()) return;  // nothing but the loss alert left
        }
    }

    char buf[64];
    std::snprintf(buf, sizeof(buf), "alert queue overflow: %llu alert(s) dropped",
                  static_cast<unsigned long long>(g_alertsDropped));

    if (loss == nullptr) {
        // QA-A-204: the entry is built before it is linked in, with room for the longest text it will ever
        // carry (so a later update is an assignment that allocates nothing). A throw while building it
        // leaves the queue as it is, with every eviction already counted: g_alertsDropped stays exact and
        // the loss alert appears on the next alert.
        AlertEntry entry;
        entry.severity    = XPE_ALERT_ERROR;
        entry.isLossAlert = true;
        entry.message.reserve(sizeof(buf));
        entry.message = buf;
        g_alertQueue.push_back(std::move(entry));
        return;
    }

    loss->message = buf;   // within the capacity reserved when the entry was made: no allocation
}

/** sync_loss_alert_locked() for callers that must not throw: a loss alert that could not be made or updated
 *  catches up on the next alert; g_alertsDropped itself is exact either way. */
static void sync_loss_alert_best_effort_locked() noexcept
{
    try {
        sync_loss_alert_locked();
    } catch (...) {
    }
}

/** The queue half of an enqueue, g_mutex held. Every alert that does not end up in the queue is counted in
 *  g_alertsDropped exactly once -- including one that could not be stored because an allocation failed. */
static void enqueue_locked(AlertEntry&& e) noexcept
{
    while (g_alertQueue.size() >= kAlertQueueMax) {
        if (!evict_one_locked()) {
            // Only the loss alert is left and the incoming alert cannot fit;
            // record it as dropped rather than displacing the loss report.
            ++g_alertsDropped;
            sync_loss_alert_best_effort_locked();
            return;
        }
    }

    try {
        g_alertQueue.push_back(std::move(e));
    } catch (...) {
        // QA-A-204: an eviction may already have been counted above; the alert that could not be stored
        // is lost too, and says so.
        ++g_alertsDropped;
    }
    sync_loss_alert_best_effort_locked();
}

/** Enqueue an alert under the 5.17 overflow policy. Never throws: an alert that cannot be built or stored is
 *  counted as dropped (QA-A-204, #233). */
static void enqueue_alert(const char* msg, int32_t severity) noexcept
{
    AlertEntry e;
    bool built = false;
    try {
        e.message  = msg ? msg : "";   // the one allocation made before the queue is touched
        e.severity = severity;
        built = true;
    } catch (...) {
    }

    try {
        std::lock_guard<std::mutex> lk(g_mutex);
        if (!built) {
            ++g_alertsDropped;
            sync_loss_alert_best_effort_locked();
            return;
        }
        enqueue_locked(std::move(e));
    } catch (...) {
        // The lock itself could not be taken: nothing was changed, nothing to account for.
    }
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
        // QA-A-204 (#233): everything that can throw is done BEFORE the first field is written. The copy of
        // the configuration used to be made after g_initialized had been set and the queue cleared, so an
        // allocation failure returned an error for a library that was already initialized.
        std::string staged;
        if (configJsonOrNull) staged = configJsonOrNull;

        {
            std::lock_guard<std::mutex> lk(g_mutex);

            // From here nothing throws: flags, clear() and swap() only.
            g_initialized   = true;
            g_logLevel      = 2;  // INFO
            g_alertQueue.clear();
            g_alertsDropped = 0;

            if (configJsonOrNull) {
                g_configJson.swap(staged);
            }
        }

        // The default logging destination: stderr at INFO (REQ-P0-011). Takes the logging mutex, not g_mutex.
        xpe_log_internal_init();

        // internal_log acquires g_mutex; must be called after releasing it. The library IS initialized by
        // now, so a failure to write this line must not turn into an error return.
        try {
            internal_log(2, "xpe_init: library initialised");
        } catch (...) {
        }
        return XPE_OK;
    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}

XPE_API void xpe_shutdown(void)
{
    try {
        {
            std::lock_guard<std::mutex> lk(g_mutex);
            if (!g_initialized) return;

            g_initialized = false;
            g_alertQueue.clear();
            g_alertsDropped = 0;
            g_configJson.clear();

            // Flush and close log file
            if (g_logFile.is_open()) {
                g_logFile.flush();
                g_logFile.close();
            }
        }

        // Release the spdlog file sink installed by xpe_log_set_file(), so the
        // host can delete or rotate the log file after shutdown. Called with
        // g_mutex released: the helper takes the logging module's own mutex and
        // the two are never held together.
        xpe_log_internal_reset();
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
    if (!jsonConfig || jsonConfig[0] == '\0') return XPE_ERR_INVALID_INPUT;

    try {
        // Full syntactic validation. accept() reports well-formedness without
        // building a DOM and without throwing, so a 64 KB malformed payload
        // costs one scan and never unwinds across the C ABI. Schema validation
        // (which keys must be present) is a separate requirement -- not here.
        if (!nlohmann::json::accept(std::string(jsonConfig))) {
            return XPE_ERR_CONFIG_INVALID;
        }

        // The configuration document must be an object, as it was before this
        // check was tightened -- a bare array or scalar is valid JSON but is
        // not a configuration.
        const char* p = jsonConfig;
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
        if (*p != '{') return XPE_ERR_CONFIG_INVALID;

        // The copy is made before the lock and the swap cannot throw, so a failure leaves the stored
        // configuration as it was (QA-A-204).
        std::string staged(jsonConfig);
        std::lock_guard<std::mutex> lk(g_mutex);
        g_configJson.swap(staged);
        return XPE_OK;
    } catch (const std::bad_alloc&) {
        // An allocation failure is not a statement about the document (QA-A-204, #233).
        return XPE_ERR_OUT_OF_MEMORY;
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
        case XPE_ERR_NOT_IMPLEMENTED:      return "Function not implemented in this version";
        case XPE_ERR_CALIB_NOT_LOADED:     return "Calibration data not loaded";
        case XPE_ERR_INVALID_CALIB_DATA:   return "Calibration data out of valid range";
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
    g_alertsDropped = 0;
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

// Logging functions implemented in xpe_logging.cpp using spdlog

/* ============================================================================
 * Alert producer (declared in xpe_error.h).
 * ============================================================================ */

extern "C" {

XPE_API void xpe_alert_push(const char* msg, int32_t severity)
{
    enqueue_alert(msg, severity);
}

} // extern "C"

#ifdef XPE_COMMON_TEST_HOOKS
/* Test-only (QA-A-204): the exact number of alerts the queue has counted as lost. Compiled only into the
 * allocation-failure executable, which defines XPE_COMMON_TEST_HOOKS; the shipped library does not have it. */
uint64_t xpe_common_alerts_dropped_for_test()
{
    std::lock_guard<std::mutex> lk(g_mutex);
    return g_alertsDropped;
}
#endif
