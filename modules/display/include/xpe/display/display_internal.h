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
#include <cstring>
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
 * @brief REQ-DISP-031: log the entry of a public display function at DEBUG level (xpe_common's logger).
 * Never throws; a logging failure changes nothing about the call.
 */
void xpe_display_log_enter(const char* fn);

/**
 * @brief REQ-DISP-031: log the outcome of a public display function and hand @p rc back unchanged: exit at DEBUG level
 * when it is XPE_OK, an error condition at ERROR level otherwise. Never throws.
 */
XpeErrorCode xpe_display_log_exit(const char* fn, XpeErrorCode rc);

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
 * @brief True when `v` is neither NaN nor an infinity (QA-B-181f, #233).
 *
 * A bit test on the exponent, not a range comparison: whether `x <= 0` refuses NaN depends on /fp:fast against
 * /fp:precise (QA-B-181e measured the same check giving opposite answers), and a bit test does not. Same helper as
 * enhance_basic_internal.h; each module keeps its own copy (see xpe_data_size_is_consistent below).
 */
inline bool xpe_float_is_finite(float v) {
    uint32_t u;
    std::memcpy(&u, &v, sizeof u);
    return (u & 0x7F800000u) != 0x7F800000u;
}

/** True when every one of the `n` floats is finite. One pass, no early exit, so it vectorizes. */
inline bool xpe_all_finite(const float* p, size_t n) {
    uint32_t bad = 0;
    for (size_t i = 0; i < n; ++i) {
        uint32_t u;
        std::memcpy(&u, p + i, sizeof u);
        bad |= static_cast<uint32_t>((u & 0x7F800000u) == 0x7F800000u);
    }
    return bad == 0;
}

/**
 * @brief xpe_all_finite plus the smallest and largest value. `*lo` and `*hi` are only meaningful when it returns
 * true. For callers that must know whether a monotone function of the image stays finite: the extremes of the
 * result are the results of the extremes.
 *
 * The extremes are taken on integer keys that sort like the floats (the sign bit folded into the low bits), not
 * with float compares: an integer min/max reduction vectorizes, a float one does not without /fp:fast (the first
 * version here, with float compares, made a 3072x3072 modality LINEAR pass 5x slower, 2 -> 10 ms).
 */
inline bool xpe_scan_finite(const float* p, size_t n, float* lo, float* hi) {
    int32_t mn = INT32_MAX, mx = INT32_MIN;
    uint32_t bad = 0;
    for (size_t i = 0; i < n; ++i) {
        uint32_t u;
        std::memcpy(&u, p + i, sizeof u);
        bad |= static_cast<uint32_t>((u & 0x7F800000u) == 0x7F800000u);
        const int32_t s = static_cast<int32_t>(u);
        const int32_t key = s ^ ((s >> 31) & 0x7FFFFFFF);
        mn = key < mn ? key : mn;
        mx = key > mx ? key : mx;
    }
    auto decode = [](int32_t k) {
        const uint32_t u = static_cast<uint32_t>(k ^ ((k >> 31) & 0x7FFFFFFF));
        float f;
        std::memcpy(&f, &u, sizeof f);
        return f;
    };
    *lo = n ? decode(mn) : 0.0f;
    *hi = n ? decode(mx) : 0.0f;
    return bad == 0;
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
