/**
 * @file display_internal.h
 * @brief Internal helper declarations for xpe_display module.
 *
 * NOT part of the public ABI. Used only by display module translation units.
 * SPEC: SPEC-XPE-P1B-DISP
 */

#ifndef XPE_DISPLAY_INTERNAL_H
#define XPE_DISPLAY_INTERNAL_H

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <stddef.h>

#ifdef __cplusplus
#include <cstdint>
#include <cmath>
#include <algorithm>
extern "C" {
#endif

/* =========================================================================
 * Format Validation (C linkage)
 * ========================================================================= */

/**
 * @brief Validate that img is non-NULL and has FLOAT32 format.
 * @return XPE_OK if valid.
 * @return XPE_ERR_INVALID_INPUT if img is NULL.
 * @return XPE_ERR_UNSUPPORTED_FORMAT if img->format != XPE_PIXEL_FLOAT32.
 */
XpeErrorCode xpe_validate_float32(const XpeImageBuffer* img);

/**
 * @brief Return the number of pixels in an image (width * height).
 * @pre img is non-NULL.
 */
static
#ifdef __cplusplus
inline
#endif
size_t xpe_pixel_count(const XpeImageBuffer* img) {
    return (size_t)img->width * (size_t)img->height;
}

#ifdef __cplusplus
} /* extern "C" */

/* =========================================================================
 * C++ Inline Helpers (internal linkage — C++ only)
 * ========================================================================= */

/**
 * @brief Clamp value to [lo, hi].
 */
template<typename T>
inline T xpe_clamp(T val, T lo, T hi) {
    return val < lo ? lo : (val > hi ? hi : val);
}

/**
 * @brief Round a float and cast to int32_t.
 */
inline int32_t xpe_round_to_int(float v) {
    return static_cast<int32_t>(std::roundf(v));
}

#endif /* __cplusplus */


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

#endif /* XPE_DISPLAY_INTERNAL_H */
