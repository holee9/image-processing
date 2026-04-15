#ifndef XPE_PREPROCESS_API_H
#define XPE_PREPROCESS_API_H

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Returns the xpe_preprocess module version string. */
XPE_API const char* xpe_preprocess_version(void);

#ifdef __cplusplus
}
#endif

#endif /* XPE_PREPROCESS_API_H */
