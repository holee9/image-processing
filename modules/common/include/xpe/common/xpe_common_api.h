#ifndef XPE_COMMON_API_H
#define XPE_COMMON_API_H

#include "xpe_types.h"
#include "xpe_error.h"
#include "xpe_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ==========================================================================
 * xpe_common.dll -- Library Lifecycle
 * ==========================================================================
 *
 * REQ-P0-008: xpe_common.dll SHALL export exactly 18 functions with C linkage.
 *
 * Exported function list (18 total):
 *   Lifecycle   (3): xpe_init, xpe_shutdown, xpe_version
 *   Config      (1): xpe_configure
 *   ParamRange  (1): xpe_get_param_range
 *   Error/Alert (4): xpe_error_string, xpe_get_pending_alert_count,
 *                    xpe_get_pending_alert, xpe_clear_alerts
 *   Logging     (3): xpe_log_set_level, xpe_log_set_file, xpe_log_flush
 *   AED         (3): xpe_aed_configure, xpe_aed_poll_event, xpe_aed_get_status
 *   Image Mem   (3): xpe_alloc_image, xpe_free_image, xpe_copy_image
 *                    (declared in xpe_memory.h, counted here)
 * -------------------------------------------------------------------------- */

/**
 * @brief Initialises all XPE subsystems.
 *
 * Must be called once before any other XPE function. Pass NULL to accept
 * default configuration.
 *
 * @param configJsonOrNull  UTF-8 JSON configuration string, or NULL for defaults.
 * @return XPE_OK on success, XPE_ERR_CONFIG_INVALID or XPE_ERR_OUT_OF_MEMORY on failure.
 *
 * SRS: SRS-INIT-001, SRS-INIT-002
 */
XPE_API XpeErrorCode xpe_init(const char* configJsonOrNull);

/**
 * @brief Releases all XPE subsystem resources.
 *
 * Must be the last XPE call. No XPE function may be called after this returns.
 * Calling shutdown without a preceding xpe_init is a no-op.
 *
 * SRS: SRS-INIT-003
 */
XPE_API void xpe_shutdown(void);

/**
 * @brief Returns a pointer to a static, null-terminated version string.
 *
 * Format: "X.Y.Z" (semantic versioning). The returned pointer is DLL-owned;
 * do NOT free it.
 *
 * @return Non-NULL pointer to version string. Thread-safe (read-only static).
 *
 * SRS: SRS-VER-001
 */
XPE_API const char* xpe_version(void);

/**
 * @brief Applies a runtime configuration update from a UTF-8 JSON string.
 *
 * Keys not present in the JSON are left unchanged. Unknown keys are silently
 * ignored (forward-compatible). NULL is treated as an invalid input.
 *
 * @param jsonConfig  Non-NULL UTF-8 JSON configuration string.
 * @return XPE_OK, XPE_ERR_INVALID_INPUT (NULL), XPE_ERR_CONFIG_INVALID.
 *
 * SRS: SRS-CFG-001, SRS-CFG-002
 */
XPE_API XpeErrorCode xpe_configure(const char* jsonConfig);

/**
 * @brief Retrieves the valid range and default value for a named parameter.
 *
 * Used by GUI sliders to clamp user input without hard-coding limits.
 *
 * @param bodyPart    Null-terminated body part label (e.g., "CHEST").
 * @param paramName   Null-terminated parameter name (e.g., "windowWidth").
 * @param minVal      Output: minimum valid value.
 * @param maxVal      Output: maximum valid value.
 * @param defaultVal  Output: recommended default value.
 * @return XPE_OK, XPE_ERR_INVALID_INPUT, XPE_ERR_NOT_INITIALIZED.
 *
 * SRS: SRS-SAFE-002, SRS-SAFE-005
 */
XPE_API XpeErrorCode xpe_get_param_range(const char* bodyPart, const char* paramName,
                                          float* minVal, float* maxVal, float* defaultVal);

/* ==========================================================================
 * Logging Subsystem (REQ-P0-023 .. REQ-P0-025)
 * ========================================================================== */

/**
 * @brief Sets the minimum log severity level.
 *
 * Level mapping: 0=TRACE, 1=DEBUG, 2=INFO, 3=WARN, 4=ERROR, 5=OFF.
 * Messages below this threshold are silently discarded.
 *
 * @param level  Log level integer in range [0, 5].
 * @return XPE_OK, XPE_ERR_INVALID_INPUT (level out of range).
 *
 * SRS: SRS-LOG-001
 */
XPE_API XpeErrorCode xpe_log_set_level(int32_t level);

/**
 * @brief Redirects log output to the specified file path.
 *
 * The file is opened in append mode. Pass NULL to revert to stderr.
 * If the file cannot be opened, the previous output destination is retained.
 *
 * @param filePath  UTF-8 file path, or NULL to use stderr.
 * @return XPE_OK, XPE_ERR_IO_FAILED (cannot open), XPE_ERR_INVALID_INPUT.
 *
 * SRS: SRS-LOG-002
 */
XPE_API XpeErrorCode xpe_log_set_file(const char* filePath);

/**
 * @brief Force-flushes all buffered log messages to the current destination.
 *
 * Thread-safe. Useful before process termination or crash reporting.
 *
 * SRS: SRS-LOG-003
 */
XPE_API void xpe_log_flush(void);

/* ==========================================================================
 * AED Subsystem -- Automatic Exposure Detection (REQ-P0-026 .. REQ-P0-028)
 * ========================================================================== */

/**
 * @brief Configures the Automatic Exposure Detection (AED) subsystem.
 *
 * Must be called after xpe_init(). Pass NULL to accept default configuration.
 *
 * JSON schema (default if NULL):
 * @code{.json}
 * {
 *   "aed": {
 *     "trigger_threshold_adu": 500,
 *     "settle_time_ms": 100,
 *     "min_exposure_ms": 5,
 *     "max_exposure_ms": 5000
 *   }
 * }
 * @endcode
 *
 * @param configJsonOrNull  UTF-8 JSON config string, or NULL for defaults.
 * @return XPE_OK, XPE_ERR_INVALID_INPUT, XPE_ERR_CONFIG_INVALID,
 *         XPE_ERR_NOT_INITIALIZED.
 *
 * SRS: SRS-AED-001, SRS-AED-002
 */
XPE_API XpeErrorCode xpe_aed_configure(const char* configJsonOrNull);

/**
 * @brief Polls the AED event queue for the next pending exposure event.
 *
 * Returns XPE_OK when an event was available and written to the output
 * parameters. Returns XPE_STATUS_NO_EVENT (= 1) when the queue is empty
 * (non-error). Output parameters are only written on XPE_OK.
 *
 * eventTypeOut values: 0=exposure_start, 1=exposure_end, 2=exposure_trigger.
 *
 * @param eventTypeOut   Output: event type (int32_t).
 * @param timestampOut   Output: UNIX epoch milliseconds (uint64_t).
 * @param signalLevelOut Output: normalized signal level [0.0, 1.0] (float).
 * @return XPE_OK (event read), XPE_STATUS_NO_EVENT (queue empty),
 *         XPE_ERR_INVALID_INPUT (NULL pointer), XPE_ERR_NOT_INITIALIZED.
 *
 * SRS: SRS-AED-003, SRS-AED-004
 */
XPE_API XpeErrorCode xpe_aed_poll_event(int32_t* eventTypeOut,
                                         uint64_t* timestampOut,
                                         float* signalLevelOut);

/**
 * @brief Returns the current AED state machine state.
 *
 * stateOut values: 0=IDLE, 1=ARMED, 2=TRIGGERED.
 * - IDLE:      AED not configured or between exposures.
 * - ARMED:     Configured and waiting for exposure onset.
 * - TRIGGERED: Exposure detected, event queued.
 *
 * @param stateOut  Output: current state (int32_t).
 * @return XPE_OK, XPE_ERR_INVALID_INPUT (NULL), XPE_ERR_NOT_INITIALIZED.
 *
 * SRS: SRS-AED-005
 */
XPE_API XpeErrorCode xpe_aed_get_status(int32_t* stateOut);

/* NOTE: xpe_error_string, xpe_get_pending_alert_count,
 *       xpe_get_pending_alert, xpe_clear_alerts are declared in xpe_error.h.
 *       xpe_alloc_image, xpe_free_image, xpe_copy_image are declared in xpe_memory.h.
 *       All are counted toward the 18-function total. */

#ifdef __cplusplus
}
#endif

#endif /* XPE_COMMON_API_H */
