/**
 * @file xpe_error.h
 * @brief Error codes and alert subsystem for all XPE modules.
 *
 * Defines the signed 32-bit return code type (@c XpeErrorCode) used by every
 * exported XPE API function, the full set of return code constants, alert
 * severity levels, and the alert polling API.
 *
 * @note Return code convention: non-negative values indicate success or
 *       informational status; negative values indicate errors.
 * @note Alert API (xpe_get_pending_alert, etc.) is safe for P/Invoke from C#
 *       and may be called from any thread after module initialization.
 *       Internally the alert queue is protected by a critical section.
 *
 * @ingroup xpe_common
 */
#ifndef XPE_ERROR_H
#define XPE_ERROR_H

#include "xpe_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Signed 32-bit return code used by all XPE API functions.
 *
 * Non-negative values indicate success or informational status.
 * Negative values indicate errors. Use @c xpe_error_string() to obtain a
 * human-readable description.
 *
 * @see XPE_OK, XPE_ERR_INVALID_INPUT, and the other @c XPE_ERR_* defines.
 * @ingroup xpe_common
 */
typedef int32_t XpeErrorCode;

/** @name Return Codes
 *  All XPE API functions return an XpeErrorCode.
 *
 *  Non-negative values indicate success or informational status.
 *  Negative values indicate errors.
 *  Callers should test `code == XPE_OK` for a success check, or
 *  compare against specific values for precise error handling.
 *  @{
 */
#define XPE_OK                       0   /**< Success — operation completed without error */
#define XPE_ERR_INVALID_INPUT       -1   /**< NULL pointer, wrong pixel format, or invalid parameter value */
#define XPE_ERR_OUT_OF_MEMORY       -2   /**< Heap allocation failure */
#define XPE_ERR_PROCESSING_FAILED   -3   /**< Internal algorithm failure (e.g. singular matrix, zero-mean image) */
#define XPE_ERR_CONFIG_INVALID      -4   /**< Configuration file missing, malformed, or version mismatch */
#define XPE_ERR_CALIBRATION_EXPIRED -5   /**< Calibration data present but outside its valid-use window */
#define XPE_ERR_NOT_INITIALIZED     -6   /**< Module-level init function has not been called */
#define XPE_ERR_UNSUPPORTED_FORMAT  -7   /**< Pixel format or image dimensions not supported by this function */
#define XPE_ERR_BUFFER_TOO_SMALL    -8   /**< Caller-supplied output buffer is smaller than required */
#define XPE_ERR_IO_FAILED           -9   /**< File read or write failure */
#define XPE_ERR_NETWORK_FAILED      -10  /**< Network communication failure (DICOM send/receive) */
#define XPE_ERR_SAFETY_VIOLATION    -11  /**< Safety violation detected */
#define XPE_ERR_INTERNAL            -12  /**< Internal processing error */
#define XPE_ERR_DICOM_INVALID       -13  /**< DICOM file is malformed, truncated, or not a valid DICOM file */
#define XPE_ERR_DICOM_CONFORMANCE   -14  /**< DICOM conformance violation: unsupported SOP class, transfer syntax, or mandatory attribute missing */
/** @} */

/**
 * @brief Severity level for entries in the XPE alert queue.
 *
 * Alerts are posted to an internal thread-safe queue by processing functions
 * when they detect conditions worth reporting that do not warrant a hard
 * error return. Retrieve alerts via xpe_get_pending_alert().
 */
typedef enum XpeAlertSeverity {
    XPE_ALERT_INFO    = 0,  /**< Informational message — no action required */
    XPE_ALERT_WARNING = 1,  /**< Warning — image quality may be affected; operator review recommended */
    XPE_ALERT_ERROR   = 2   /**< Non-fatal error — a stage was skipped or degraded; check flags in XpeImageMetadata */
} XpeAlertSeverity;

/**
 * @brief Return a human-readable description of an XpeErrorCode.
 *
 * The returned string is a static constant owned by the DLL. Callers must not
 * free or modify it. The string remains valid for the lifetime of the process.
 *
 * @param code  Any XpeErrorCode value, including unknown codes.
 * @return      Null-terminated ASCII string. Never returns NULL.
 *              Returns "Unknown error code" for unrecognized values.
 */
XPE_API const char* xpe_error_string(XpeErrorCode code);

/**
 * @brief Return the number of pending alerts in the internal alert queue.
 *
 * The count reflects all alerts posted since the last xpe_clear_alerts() call.
 * This function is thread-safe and non-blocking.
 *
 * @return Non-negative count of pending alerts.
 *
 * @note Poll this after each processing call if your integration requires
 *       operator-visible quality feedback (SRS-ALERT-001).
 */
XPE_API int32_t xpe_get_pending_alert_count(void);

/**
 * @brief Retrieve a single alert by index from the internal alert queue.
 *
 * Copies the alert message and severity into caller-supplied buffers.
 * The alert queue is not modified; call xpe_clear_alerts() to drain it.
 *
 * @param index    Zero-based index into the pending alert queue.
 *                 Valid range: [0, xpe_get_pending_alert_count() - 1].
 * @param msg      Caller-supplied buffer to receive the null-terminated message.
 *                 If the message is longer than @p msgLen - 1 bytes, it is
 *                 truncated and null-terminated. Must not be NULL.
 * @param msgLen   Size of the @p msg buffer in bytes. Recommended: >= 256.
 * @param severity Output: receives the @c XpeAlertSeverity of the alert.
 *                 May be NULL if the caller does not need severity.
 * @return XPE_OK on success.
 * @return XPE_ERR_INVALID_INPUT if @p msg is NULL or @p index is out of range.
 *
 * @note This function is thread-safe (SRS-ALERT-003).
 * @note Designed for direct P/Invoke from C# (SRS-ALERT-006).
 */
XPE_API XpeErrorCode xpe_get_pending_alert(int32_t index, char* msg, size_t msgLen, int32_t* severity);

/**
 * @brief Clear all pending alerts from the internal alert queue.
 *
 * After this call, xpe_get_pending_alert_count() returns 0.
 * Typically called by the host after processing and displaying all alerts.
 *
 * @note Thread-safe. Safe to call even when the queue is already empty.
 * @note SRS-ALERT-005.
 */
XPE_API void xpe_clear_alerts(void);

#ifdef __cplusplus
}
#endif

#endif /* XPE_ERROR_H */
