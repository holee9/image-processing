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

#ifdef __cplusplus
}
#endif

#endif /* XPE_COMMON_API_H */
