#ifndef XPE_ENHANCE_BASIC_INTERNAL_H
#define XPE_ENHANCE_BASIC_INTERNAL_H

/* Internal helpers for xpe_enhance_basic -- not exported. */

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include <cstdint>

/* Validate that img is non-null, format==FLOAT32, and data is non-null. */
/**
 * @brief api-spec "XpeImageBuffer.dataSize on input" size-consistency check (#123).
 *
 * `dataSize == 0` means unspecified and is accepted (legacy callers do not
 * populate the field). A non-zero `dataSize` smaller than
 * width * height * bytesPerPixel(format) means the buffer cannot hold the image
 * it declares, and reading it overruns the allocation. A larger value is fine.
 *
 * Header-inline: one definition per module without adding a xpe_common export
 * (REQ-P0-008 fixes that surface at 16 symbols).
 */
static inline int xpe_data_size_is_consistent(const XpeImageBuffer* img) {
    uint64_t required;
    uint32_t bpp;
    if (img == NULL || img->dataSize == 0) return 1;
    switch (img->format) {
        case XPE_PIXEL_UINT16:  bpp = 2u; break;
        case XPE_PIXEL_FLOAT32: bpp = 4u; break;
        default:                return 1;  /* unknown format is the format check's business */
    }
    required = (uint64_t)img->width * (uint64_t)img->height * (uint64_t)bpp;
    return (uint64_t)img->dataSize >= required;
}

inline XpeErrorCode validate_float32_image(const XpeImageBuffer* img) {
    if (!img || !img->data) return XPE_ERR_INVALID_INPUT;
    // #142 D1: one empty-image answer for the whole module. Three entry points
    // used to treat a zero-sized image as "nothing to do" and return XPE_OK
    // while two others rejected it, so the same input got different answers
    // depending on which function the caller reached for (QA-B-39). Checked
    // here, before the format test, because a dimension is a property of the
    // descriptor rather than of the pixel type -- an empty UINT16 image is
    // reported as empty, not as the wrong format.
    if (img->width == 0 || img->height == 0) return XPE_ERR_INVALID_INPUT;
    if (img->format != XPE_PIXEL_FLOAT32) return XPE_ERR_UNSUPPORTED_FORMAT;
    // api-spec "XpeImageBuffer.dataSize on input" (#123). Every enhance_basic
    // entry point routes through here, so the module keeps one definition of
    // the check rather than seven copies.
    if (!xpe_data_size_is_consistent(img)) return XPE_ERR_INVALID_INPUT;
    return XPE_OK;
}

inline float* float_pixels(XpeImageBuffer* img) {
    return static_cast<float*>(img->data);
}

inline const float* const_float_pixels(const XpeImageBuffer* img) {
    return static_cast<const float*>(img->data);
}

/*
 * Alerts are posted with xpe_alert_push(), declared in xpe/common/xpe_error.h
 * (included above). The local extern declaration that used to sit here dated
 * from when the symbol had no public declaration; #111 gave it one, so the
 * duplicate is gone -- do not reintroduce it.
 */


#endif /* XPE_ENHANCE_BASIC_INTERNAL_H */
