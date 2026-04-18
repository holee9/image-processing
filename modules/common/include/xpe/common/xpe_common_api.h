#ifndef XPE_COMMON_API_H
#define XPE_COMMON_API_H

#include "xpe_types.h"
#include "xpe_error.h"
#include "xpe_memory.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Library initialization / shutdown */
XPE_API XpeErrorCode xpe_init(const char* configJsonOrNull);
XPE_API void         xpe_shutdown(void);
XPE_API const char*  xpe_version(void);

/* Configuration (JSON string, forward-compatible) */
XPE_API XpeErrorCode xpe_configure(const char* jsonConfig);

/* Parameter range query for GUI sliders (SRS-SAFE-002, SRS-SAFE-005) */
XPE_API XpeErrorCode xpe_get_param_range(const char* bodyPart, const char* paramName,
                                          float* minVal, float* maxVal, float* defaultVal);

/* Logging subsystem (REQ-P0-023~025) */
XPE_API XpeErrorCode xpe_log_set_level(int level);
XPE_API XpeErrorCode xpe_log_set_file(const char* filePath);
XPE_API void         xpe_log_flush(void);

/* AED subsystem (REQ-P0-026~028) */
XPE_API XpeErrorCode xpe_aed_configure(const char* jsonConfig);
XPE_API XpeErrorCode xpe_aed_poll_event(int32_t* eventTypeOut, uint64_t* timestampOut, float* signalLevelOut);
XPE_API XpeErrorCode xpe_aed_get_status(int32_t* stateOut);

#ifdef __cplusplus
}
#endif

#endif /* XPE_COMMON_API_H */
