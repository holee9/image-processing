#ifndef GSVG_API_H
#define GSVG_API_H

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Returns the gsvg module version string. */
XPE_API const char* gsvg_version(void);

#ifdef __cplusplus
}
#endif

#endif /* GSVG_API_H */
