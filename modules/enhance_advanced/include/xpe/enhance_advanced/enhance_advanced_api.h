#ifndef XPE_ENHANCE_ADVANCED_API_H
#define XPE_ENHANCE_ADVANCED_API_H

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file enhance_advanced_api.h
 * @brief Version-only declaration for xpe_enhance_advanced.
 *
 * The module's real API surface is xpe_enhance_advanced_api.h. This header
 * declares only the version accessor, and exists because three translation
 * units include it instead.
 *
 * WARNING: this header and xpe_enhance_advanced_api.h use the SAME include
 * guard (XPE_ENHANCE_ADVANCED_API_H), so a translation unit including both
 * silently gets only whichever came first. Reported as a defect by QA-B-39 and
 * left unchanged there: renaming a guard is a code change, not a doc fix.
 */

/**
 * Returns the xpe_enhance_advanced module version string.
 * @return Null-terminated "1.0.0". Lifetime: process. Never NULL.
 */
XPE_API const char* xpe_enhance_advanced_version(void);

#ifdef __cplusplus
}
#endif

#endif /* XPE_ENHANCE_ADVANCED_API_H */
