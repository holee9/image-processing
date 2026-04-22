/**
 * @file gsvg_api.h
 * @brief GSVG (Grid Shadow and Vignette Gain) correction module API.
 *
 * GSVG addresses two post-acquisition artifacts independent of the main XPE
 * preprocess pipeline:
 *
 *  - Grid shadow suppression: the anti-scatter grid placed between patient and
 *    detector introduces a periodic (Moire-like) pattern of alternating darker
 *    and brighter stripes. These must be suppressed without softening real
 *    anatomy. A spatial row-mean subtraction is used as the baseline method.
 *
 *  - Vignette gain correction: the X-ray beam intensity is not spatially
 *    uniform across the detector — there is a cosine-style fall-off toward
 *    the edges. This is compensated by a pre-measured gain map which is
 *    multiplied pixel-wise into the image.
 *
 * Both features are selectable per init() call via a simple JSON config.
 * When a correction step is disabled, the output equals the input for that
 * step (identity). This enables DegradedMode / pass-through operation when
 * calibration data is absent or invalid.
 *
 * ABI: C linkage, handle-based lifecycle (init/process/shutdown). Thread-safe
 * for concurrent calls on independent handles. A single handle must not be
 * used by multiple threads simultaneously.
 */
#ifndef GSVG_API_H
#define GSVG_API_H

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Returns the gsvg module version string.
 *
 * @return Null-terminated ASCII string. Lifetime: process. Never NULL.
 */
XPE_API const char* gsvg_version(void);

/**
 * @brief Initialize a GSVG correction handle.
 *
 * Parses the config JSON for feature toggles and allocates internal state.
 *
 * Config JSON schema (all fields optional):
 * @code
 * {
 *   "vignette_correction": true,
 *   "grid_suppression":    false
 * }
 * @endcode
 * When configJsonOrNull is NULL, both features default to FALSE (pass-through).
 *
 * The vignette gain map itself is NOT supplied through the config JSON. It is
 * attached to the handle at process time by the caller via the @c gainMap
 * argument of @c xpe_gsvg_process. When @c gainMap is NULL the vignette step
 * becomes an identity no-op even if it was enabled in the config.
 *
 * @param handleOut        Output: receives the opaque handle pointer.
 * @param configJsonOrNull Null-terminated JSON string, or NULL for defaults.
 * @return XPE_OK on success.
 * @return XPE_ERR_INVALID_INPUT if handleOut is NULL.
 * @return XPE_ERR_OUT_OF_MEMORY on allocation failure.
 */
XPE_API XpeErrorCode xpe_gsvg_init(void** handleOut, const char* configJsonOrNull);

/**
 * @brief Apply the configured GSVG correction steps to a uint16 image.
 *
 * Processing order (each step is skipped if disabled or inputs missing):
 *   1. Vignette gain correction: dst[i] = clamp(src[i] * gainMap[i], 0, 65535)
 *   2. Grid shadow suppression:  row-mean-deviation subtraction on dst in-place.
 *
 * When both steps are disabled, dst is an exact bitwise copy of src.
 *
 * @param handle  GSVG handle returned by xpe_gsvg_init. Must not be NULL.
 * @param src     Source image, width*height uint16 pixels. Must not be NULL.
 * @param dst     Destination image, width*height uint16 pixels. Must not be
 *                NULL. May alias src for in-place operation.
 * @param width   Image width in pixels. Must be > 0.
 * @param height  Image height in pixels. Must be > 0.
 * @param gainMap Optional vignette gain map, width*height float32 pixels.
 *                NULL disables the vignette step regardless of config.
 * @return XPE_OK on success.
 * @return XPE_ERR_INVALID_INPUT on NULL pointer or non-positive dimension.
 * @return XPE_ERR_NOT_INITIALIZED if handle is NULL.
 */
XPE_API XpeErrorCode xpe_gsvg_process(void* handle,
                                      const uint16_t* src,
                                      uint16_t* dst,
                                      int width,
                                      int height,
                                      const float* gainMap);

/**
 * @brief Release all resources owned by a GSVG handle.
 *
 * After this call the handle must not be used. Passing a NULL handle is a
 * no-op and returns XPE_OK.
 *
 * @param handle GSVG handle returned by xpe_gsvg_init, or NULL.
 * @return XPE_OK always.
 */
XPE_API XpeErrorCode xpe_gsvg_shutdown(void* handle);

#ifdef __cplusplus
}
#endif

#endif /* GSVG_API_H */
