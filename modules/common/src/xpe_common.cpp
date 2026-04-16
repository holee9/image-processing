/**
 * @file xpe_common.cpp
 * @brief Core xpe_common.dll implementation -- SPEC-XPE-P0
 *
 * Provides 18 API functions for lifecycle, configuration, error handling,
 * logging, and AED (Abnormal Event Detection) subsystems.
 *
 * IEC 62304 Class B -- No C++ exceptions across C ABI boundary.
 */

#include "xpe/common/xpe_common_api.h"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <cstring>
#include <queue>
#include <mutex>
#include <string>

// @MX:NOTE: [AUTO] Static alert queue for cross-module communication
// Thread-safe for concurrent access from xpe_ghost, xpe_gain modules

// @MX:ANCHOR: [AUTO] Alert queue singleton -- REQ-P0-019~021
// @MX:REASON: Central alert state; high fan-in from all xpe modules

static const char* XPE_VERSION_STRING = "0.1.0";

// Alert Queue (REQ-P0-019~021)
namespace {
    struct AlertEntry {
        std::string message;
        int32_t severity;
    };

    std::mutex g_alertMutex;
    std::queue<AlertEntry> g_alertQueue;
    bool g_initialized = false;
}

// Export initialization flag for AED module
extern "C" {
    XPE_API bool* xpe_initialized_flag() { return &g_initialized; }
}

/* ============================================================================
 * Lifecycle (REQ-P0-011, REQ-P0-012)
 * ============================================================================ */

extern "C" {

XPE_API XpeErrorCode xpe_init(const char* configJsonOrNull) {
    std::lock_guard<std::mutex> lock(g_alertMutex);

    // Validate config parameter (empty string is invalid)
    if (configJsonOrNull != nullptr && strlen(configJsonOrNull) == 0) {
        return XPE_ERR_CONFIG_INVALID;
    }

    g_initialized = true;
    return XPE_OK;
}

XPE_API void xpe_shutdown(void) {
    std::lock_guard<std::mutex> lock(g_alertMutex);
    // Clear alert queue
    while (!g_alertQueue.empty()) {
        g_alertQueue.pop();
    }
    g_initialized = false;
}

XPE_API const char* xpe_version(void) {
    return XPE_VERSION_STRING;
}

/* ============================================================================
 * Configuration (REQ-P0-014)
 * ============================================================================ */

XPE_API XpeErrorCode xpe_configure(const char* jsonConfig) {
    if (!jsonConfig) return XPE_ERR_INVALID_INPUT;

    // Check for empty string - return INVALID_INPUT, not CONFIG_INVALID
    if (strlen(jsonConfig) == 0) return XPE_ERR_INVALID_INPUT;

    // Try to parse JSON to validate format
    try {
        (void)nlohmann::json::parse(jsonConfig);
        return XPE_OK;
    } catch (const nlohmann::json::exception&) {
        return XPE_ERR_CONFIG_INVALID;
    }
}

XPE_API XpeErrorCode xpe_get_param_range(const char* bodyPart, const char* paramName,
                                         float* minVal, float* maxVal, float* defaultVal) {
    if (!bodyPart || !paramName || !minVal || !maxVal || !defaultVal)
        return XPE_ERR_INVALID_INPUT;
    // TODO: Lookup parameter ranges from ParameterValidator
    *minVal = 0.0f;
    *maxVal = 1.0f;
    *defaultVal = 0.5f;
    return XPE_OK;
}

/* ============================================================================
 * Error Handling (REQ-P0-018)
 * ============================================================================ */

XPE_API const char* xpe_error_string(XpeErrorCode code) {
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
        default:                           return "Unknown error";
    }
}

/* ============================================================================
 * Alert Queue (REQ-P0-019~021)
 * ============================================================================ */

XPE_API int32_t xpe_get_pending_alert_count(void) {
    std::lock_guard<std::mutex> lock(g_alertMutex);
    return static_cast<int32_t>(g_alertQueue.size());
}

XPE_API XpeErrorCode xpe_get_pending_alert(int32_t index, char* msg, size_t msgLen, int32_t* severity) {
    if (!msg || msgLen == 0 || !severity)
        return XPE_ERR_INVALID_INPUT;

    std::lock_guard<std::mutex> lock(g_alertMutex);

    if (index < 0 || static_cast<size_t>(index) >= g_alertQueue.size())
        return XPE_ERR_INVALID_INPUT;

    // Copy alert at index (inefficient for large queues, but safe)
    std::queue<AlertEntry> temp = g_alertQueue;
    for (int32_t i = 0; i < index; ++i) {
        temp.pop();
    }

    const AlertEntry& entry = temp.front();
    if (entry.message.size() + 1 > msgLen)
        return XPE_ERR_BUFFER_TOO_SMALL;

    std::memcpy(msg, entry.message.c_str(), entry.message.size() + 1);
    *severity = entry.severity;

    return XPE_OK;
}

XPE_API void xpe_clear_alerts(void) {
    std::lock_guard<std::mutex> lock(g_alertMutex);
    while (!g_alertQueue.empty()) {
        g_alertQueue.pop();
    }
}

/* Test support functions (white-box testing) */
XPE_API void xpe_test_inject_alert(const char* msg, int32_t severity) {
    std::lock_guard<std::mutex> lock(g_alertMutex);
    AlertEntry entry;
    entry.message = msg ? msg : "";
    entry.severity = severity;
    g_alertQueue.push(entry);
}

} // extern "C"
