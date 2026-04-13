#ifndef XPE_MEMORY_H
#define XPE_MEMORY_H

#include "xpe_types.h"
#include "xpe_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Image buffer allocation/deallocation (caller allocates, caller frees) */
XPE_API XpeErrorCode xpe_alloc_image(uint32_t width, uint32_t height,
                                      XpePixelFormat format, XpeImageBuffer* out);
XPE_API XpeErrorCode xpe_free_image(XpeImageBuffer* buf);

/* Copy image buffer contents */
XPE_API XpeErrorCode xpe_copy_image(const XpeImageBuffer* src, XpeImageBuffer* dst);

#ifdef __cplusplus
}
#endif

#endif /* XPE_MEMORY_H */
