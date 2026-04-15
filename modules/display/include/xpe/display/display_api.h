#ifndef XPE_DISPLAY_API_H
#define XPE_DISPLAY_API_H

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Returns the xpe_display module version string. */
XPE_API const char* xpe_display_version(void);

#ifdef __cplusplus
}
#endif

#endif /* XPE_DISPLAY_API_H */
