#ifndef XPE_ENHANCE_BASIC_INTERNAL_H
#define XPE_ENHANCE_BASIC_INTERNAL_H

/* Internal helpers for xpe_enhance_basic -- not exported. */

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include <cstdint>

/* Validate that img is non-null, format==FLOAT32, and data is non-null. */
inline XpeErrorCode validate_float32_image(const XpeImageBuffer* img) {
    if (!img || !img->data) return XPE_ERR_INVALID_INPUT;
    if (img->format != XPE_PIXEL_FLOAT32) return XPE_ERR_UNSUPPORTED_FORMAT;
    return XPE_OK;
}

inline float* float_pixels(XpeImageBuffer* img) {
    return static_cast<float*>(img->data);
}

inline const float* const_float_pixels(const XpeImageBuffer* img) {
    return static_cast<const float*>(img->data);
}

/*
 * Post an alert to the xpe_common alert queue.
 * xpe_test_inject_alert is exported from xpe_common.dll but not declared
 * in a public header. We declare it here for internal use by enhance_basic.
 */
extern "C" XPE_API void xpe_test_inject_alert(const char* msg, int32_t severity);

#endif /* XPE_ENHANCE_BASIC_INTERNAL_H */
