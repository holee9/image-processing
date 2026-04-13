#ifndef XPE_ERROR_H
#define XPE_ERROR_H

#include "xpe_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t XpeErrorCode;

#define XPE_OK                       0
#define XPE_ERR_INVALID_INPUT       -1
#define XPE_ERR_OUT_OF_MEMORY       -2
#define XPE_ERR_PROCESSING_FAILED   -3
#define XPE_ERR_CONFIG_INVALID      -4
#define XPE_ERR_CALIBRATION_EXPIRED -5
#define XPE_ERR_NOT_INITIALIZED     -6
#define XPE_ERR_UNSUPPORTED_FORMAT  -7
#define XPE_ERR_BUFFER_TOO_SMALL    -8
#define XPE_ERR_IO_FAILED           -9
#define XPE_ERR_NETWORK_FAILED      -10

/* Alert severity levels */
typedef enum XpeAlertSeverity {
    XPE_ALERT_INFO    = 0,
    XPE_ALERT_WARNING = 1,
    XPE_ALERT_ERROR   = 2
} XpeAlertSeverity;

/* Get human-readable error description */
XPE_API const char* xpe_error_string(XpeErrorCode code);

/* Alert polling (SRS-ALERT-001~006, safe for P/Invoke) */
XPE_API int32_t     xpe_get_pending_alert_count(void);
XPE_API XpeErrorCode xpe_get_pending_alert(int32_t index, char* msg, size_t msgLen, int32_t* severity);
XPE_API void         xpe_clear_alerts(void);

#ifdef __cplusplus
}
#endif

#endif /* XPE_ERROR_H */
