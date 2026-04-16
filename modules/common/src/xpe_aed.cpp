/**
 * @file xpe_aed.cpp
 * @brief AED (Abnormal Event Detection) subsystem -- REQ-P0-026~028
 *
 * State machine: IDLE(0) -> ARMED(1) -> TRIGGERED(2) -> IDLE(0)
 * Thread-safe event queue for cross-module communication.
 */

#include "xpe/common/xpe_common_api.h"
#include <nlohmann/json.hpp>
#include <queue>
#include <mutex>
#include <cstdint>

// @MX:ANCHOR: [AUTO] AED state machine core -- REQ-P0-028
// @MX:REASON: Single source of truth for AED state; high fan-in from xpe_ghost, xpe_gain

namespace {

// AED State Machine States (REQ-P0-028)
typedef enum {
    AED_STATE_IDLE = 0,      // Initial state, waiting for arm
    AED_STATE_ARMED = 1,     // Monitoring active, threshold checking enabled
    AED_STATE_TRIGGERED = 2  // Event detected, waiting for cooldown/settle
} AedState;

// AED Configuration (REQ-P0-026)
struct AedConfig {
    bool enabled = true;
    float trigger_threshold_adu = 500.0f;  // Default threshold
    uint32_t settle_time_ms = 100;         // Cooldown after trigger
    void (*callback)(int32_t eventType, float signalLevel) = nullptr;
};

// Event Queue Entry
struct AedEvent {
    int32_t eventType;     // 1=Motion, 2=Saturation, 3=DetectorFault
    uint64_t timestamp;    // Unix epoch milliseconds
    float signalLevel;     // Raw ADU or normalized value
};

// Global State
std::mutex g_aedMutex;
AedState g_state = AED_STATE_IDLE;
AedConfig g_config;
std::queue<AedEvent> g_eventQueue;

// Forward declaration of library initialization flag
extern "C" {
    XPE_API bool* xpe_initialized_flag();
}

} // anonymous namespace

extern "C" {

/**
 * Configure AED subsystem from JSON.
 * Valid keys:
 *   - "enabled": bool (default true)
 *   - "trigger_threshold_adu": float (default 500)
 *   - "settle_time_ms": int (default 100)
 *   - "callback": ignored (reserved for future use)
 *
 * REQ-P0-026: JSON config parsing with error handling.
 */
XPE_API XpeErrorCode xpe_aed_configure(const char* jsonConfig) {
    std::lock_guard<std::mutex> lock(g_aedMutex);

    if (jsonConfig == nullptr) {
        // Use defaults
        g_config = AedConfig{};
        g_state = AED_STATE_ARMED; // Auto-arm on configure
        return XPE_OK;
    }

    try {
        nlohmann::json j = nlohmann::json::parse(jsonConfig);

        if (j.contains("enabled")) {
            g_config.enabled = j["enabled"].get<bool>();
        }
        if (j.contains("trigger_threshold_adu")) {
            g_config.trigger_threshold_adu = j["trigger_threshold_adu"].get<float>();
        }
        if (j.contains("settle_time_ms")) {
            g_config.settle_time_ms = j["settle_time_ms"].get<uint32_t>();
        }

        g_state = g_config.enabled ? AED_STATE_ARMED : AED_STATE_IDLE;
        return XPE_OK;
    } catch (const nlohmann::json::exception&) {
        return XPE_ERR_CONFIG_INVALID;
    }
}

/**
 * Poll for pending AED events.
 * Returns XPE_STATUS_NO_EVENT (=1) when queue is empty (REQ-P0-028a).
 * Thread-safe for concurrent callers.
 *
 * @param eventTypeOut Output: 1=Motion, 2=Saturation, 3=DetectorFault
 * @param timestampOut Output: Unix epoch milliseconds
 * @param signalLevelOut Output: Raw ADU or normalized value
 * @return XPE_OK if event retrieved, XPE_STATUS_NO_EVENT if queue empty, XPE_ERR_INVALID_INPUT on null params
 */
XPE_API XpeErrorCode xpe_aed_poll_event(int32_t* eventTypeOut,
                                         uint64_t* timestampOut,
                                         float* signalLevelOut) {
    if (!eventTypeOut || !timestampOut || !signalLevelOut)
        return XPE_ERR_INVALID_INPUT;

    std::lock_guard<std::mutex> lock(g_aedMutex);

    if (g_eventQueue.empty()) {
        return XPE_STATUS_NO_EVENT; // =1, per REQ-P0-028a
    }

    const AedEvent& evt = g_eventQueue.front();
    *eventTypeOut = evt.eventType;
    *timestampOut = evt.timestamp;
    *signalLevelOut = evt.signalLevel;

    g_eventQueue.pop();

    // Return to IDLE if queue empty and state was TRIGGERED
    if (g_eventQueue.empty() && g_state == AED_STATE_TRIGGERED) {
        g_state = AED_STATE_IDLE;
    }

    return XPE_OK;
}

/**
 * Get current AED state machine state.
 * @param stateOut Output: 0=IDLE, 1=ARMED, 2=TRIGGERED
 * @return XPE_OK on success, XPE_ERR_NOT_INITIALIZED if library not initialized
 */
XPE_API XpeErrorCode xpe_aed_get_status(int32_t* stateOut) {
    if (!stateOut)
        return XPE_ERR_INVALID_INPUT;

    std::lock_guard<std::mutex> lock(g_aedMutex);

    // Check if library has been initialized
    bool* initialized_ptr = xpe_initialized_flag();
    if (initialized_ptr && !(*initialized_ptr)) {
        return XPE_ERR_NOT_INITIALIZED;
    }

    *stateOut = static_cast<int32_t>(g_state);
    return XPE_OK;
}

// Test support functions (white-box testing)
XPE_API void xpe_test_inject_aed_event(int32_t eventType, float signalLevel) {
    std::lock_guard<std::mutex> lock(g_aedMutex);

    AedEvent evt;
    evt.eventType = eventType;
    evt.timestamp = 12345; // Fixed timestamp for testing
    evt.signalLevel = signalLevel;

    g_eventQueue.push(evt);
    g_state = AED_STATE_TRIGGERED;
}

} // extern "C"
