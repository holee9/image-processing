#ifndef XPE_AI_API_H
#define XPE_AI_API_H

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Returns the xpe_ai module version string. */
XPE_API const char* xpe_ai_version(void);

#ifdef __cplusplus
}
#endif

#endif /* XPE_AI_API_H */
