/**
 * @file enhance_advanced_api.h
 * @brief Compatibility header for xpe_enhance_advanced.
 *
 * The module's API surface lives in xpe_enhance_advanced_api.h. This header
 * exists because several translation units include it by this name; it now
 * forwards there rather than declaring a subset.
 *
 * Why it forwards instead of re-declaring (QA-B-40, D2 of #142): both headers
 * used the SAME include guard, XPE_ENHANCE_ADVANCED_API_H. A translation unit
 * that included both silently received only whichever came first -- and since
 * this one declared only the version accessor, including it first hid the
 * entire rest of the API with no diagnostic. Forwarding removes the hazard at
 * its root: there is now one set of declarations, reachable under either name,
 * and this file carries a guard of its own.
 */
#ifndef XPE_ENHANCE_ADVANCED_API_COMPAT_H
#define XPE_ENHANCE_ADVANCED_API_COMPAT_H

#include "xpe/enhance_advanced/xpe_enhance_advanced_api.h"

#endif /* XPE_ENHANCE_ADVANCED_API_COMPAT_H */
