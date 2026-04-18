/**
 * @file xpe_memory.h
 * @brief Image buffer allocation and copy utilities for XPE modules.
 *
 * Provides the three memory management functions counted toward xpe_common.dll's
 * 18-function export contract. All returned buffers must be released via
 * xpe_free_image() — do not call free() or delete directly on buffer data.
 *
 * @note All functions are thread-safe for independent XpeImageBuffer instances.
 *       Concurrent operations on the same buffer require external synchronization.
 *
 * @ingroup xpe_common
 */
#ifndef XPE_MEMORY_H
#define XPE_MEMORY_H

#include "xpe_types.h"
#include "xpe_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Allocate a new XpeImageBuffer with a heap-managed pixel array.
 *
 * Allocates contiguous pixel memory (`width * height * bytes_per_pixel`) and
 * fills all fields of @p out. The caller owns the returned buffer and must
 * release it via xpe_free_image().
 *
 * @param width   Image width in pixels (must be > 0 and ≤ 4096).
 * @param height  Image height in pixels (must be > 0 and ≤ 4096).
 * @param format  Pixel format selector (XPE_PIXEL_UINT16 or XPE_PIXEL_FLOAT32).
 * @param out     Output: receives the initialized XpeImageBuffer. Must not be NULL.
 * @return XPE_OK on success, XPE_ERR_INVALID_INPUT (bad dimensions or NULL out),
 *         XPE_ERR_OUT_OF_MEMORY if heap allocation fails.
 *
 * @see xpe_free_image
 * @ingroup xpe_common
 */
XPE_API XpeErrorCode xpe_alloc_image(uint32_t width, uint32_t height,
                                      XpePixelFormat format, XpeImageBuffer* out);

/**
 * @brief Release pixel memory owned by an XpeImageBuffer.
 *
 * Frees the pixel data buffer referenced by @p buf->data and zeroes the struct
 * fields. Calling on a zero-initialised or already-freed buffer is safe (no-op).
 * Do NOT free @p buf->data manually before calling this function.
 *
 * @param buf  Buffer whose pixel data is to be freed. Must not be NULL.
 * @return XPE_OK on success, XPE_ERR_INVALID_INPUT if @p buf is NULL.
 *
 * @see xpe_alloc_image
 * @ingroup xpe_common
 */
XPE_API XpeErrorCode xpe_free_image(XpeImageBuffer* buf);

/**
 * @brief Deep-copy pixel data from @p src to @p dst.
 *
 * @p dst must already be allocated (via xpe_alloc_image or equivalent) with
 * matching dimensions and format. Pixel data is copied byte-for-byte; no
 * conversion is performed.
 *
 * @param src  Source image buffer. Must not be NULL.
 * @param dst  Destination image buffer. Must not be NULL. Must match src dimensions
 *             and format; existing pixel data is overwritten.
 * @return XPE_OK on success, XPE_ERR_INVALID_INPUT (NULL or dimension/format mismatch).
 *
 * @ingroup xpe_common
 */
XPE_API XpeErrorCode xpe_copy_image(const XpeImageBuffer* src, XpeImageBuffer* dst);

#ifdef __cplusplus
}
#endif

#endif /* XPE_MEMORY_H */
