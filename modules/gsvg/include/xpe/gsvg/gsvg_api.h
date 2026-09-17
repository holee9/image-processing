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
 *    anatomy. The grid frequency and direction (rows and/or columns) are read
 *    from the image spectrum; a recursive db4 wavelet decomposition carries
 *    the grid into detail sub-bands, where a Gaussian band-stop removes it
 *    (#180). An image with no detected grid is left unchanged.
 *
 *  - Virtual grid (#180): for images taken WITHOUT a grid, scatter is
 *    estimated from a water-equivalent thickness map and a thickness-indexed
 *    kernel table, removed iteratively, and replaced by the residual scatter a
 *    chosen grid ratio would pass. Every physical number comes from a
 *    parameter table file; there are no built-in coefficients. Grid
 *    suppression and the virtual grid are exclusive.
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
#include <stddef.h>   /* size_t for the #152 length arguments */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Returns the gsvg module version string.
 *
 * @return Null-terminated ASCII string. Lifetime: process. Never NULL.
 */
XPE_API const char* xpe_gsvg_version(void);

/**
 * @brief Initialize a GSVG correction handle.
 *
 * Reads the two feature toggles out of the config string and allocates internal
 * state.
 *
 * The config is NOT parsed by a JSON parser. Each toggle is located by scanning
 * for the quoted key followed by a colon and the token true or false; anything
 * else -- a missing key, a non-boolean value, or a string that is not JSON at
 * all -- leaves that toggle at its default of FALSE. Malformed config is
 * therefore never an error, and a typo in a key name silently disables the
 * feature it was meant to enable.
 *
 * Config JSON schema (all fields optional):
 * @code
 * {
 *   "vignette_correction": true,
 *   "grid_suppression":    false,
 *   "virtual_grid":        false
 * }
 * @endcode
 * When configJsonOrNull is NULL, every feature defaults to FALSE (pass-through).
 *
 * When "virtual_grid" is true (#180), these keys are REQUIRED -- none has a
 * default, and a missing one makes init fail:
 * @code
 *   "vg_table_path":     "path/to/table.csv",   JSON string; escape '\' as "\\"
 *   "vg_kvp":            80,        tube voltage of the exposure
 *   "vg_grid_ratio":     10,        must be a row of the table's [grid] section
 *   "vg_pixel_pitch_mm": 0.139,     detector pixel pitch
 *   "vg_air_signal":     60000,     detector signal without an object [DN]
 *   "vg_iterations":     3          thickness/scatter iterations, 1..100
 * @endcode
 * Optional post-steps (off when absent):
 * @code
 *   "vg_pyramid_levels": 6,         Laplacian pyramid levels, 4..8
 *   "vg_pyramid_gain":   1.3,       detail gain (needs vg_pyramid_levels)
 *   "vg_denoise_k":      2          soft threshold k * noise sigma on the
 *                                   finest band (needs vg_pyramid_levels)
 * @endcode
 * The table file format is described in modules/gsvg/src/virtual_grid.h.
 *
 * The vignette gain map itself is NOT supplied through the config JSON. It is
 * attached to the handle at process time by the caller via the @c gainMap
 * argument of @c xpe_gsvg_process. When @c gainMap is NULL the vignette step
 * becomes an identity no-op even if it was enabled in the config.
 *
 * @param handleOut        Output: receives the opaque handle pointer. Written
 *                         only on success.
 * @param configJsonOrNull Null-terminated config string, or NULL for defaults.
 * @return XPE_OK on success -- including when the config string was
 *         unparseable and both features fell back to FALSE.
 * @return XPE_ERR_INVALID_INPUT if handleOut is NULL.
 * @return XPE_ERR_OUT_OF_MEMORY on allocation failure.
 * @return XPE_ERR_CONFIG_INVALID when "virtual_grid" is true and a required
 *         key is missing, the table cannot be read or is malformed, or
 *         "grid_suppression" is also true. No handle is written; the reason
 *         is pushed to the alert queue.
 */
XPE_API XpeErrorCode xpe_gsvg_init(void** handleOut, const char* configJsonOrNull);

/**
 * @brief Apply the configured GSVG correction steps to a uint16 image.
 *
 * Processing order (each step is skipped if disabled or inputs missing):
 *   1. Vignette gain correction: dst[i] = clamp(src[i] * gainMap[i], 0, 65535)
 *   2. Grid shadow suppression:  wavelet sub-band band-stop on dst in-place;
 *      dst is not rewritten when no grid is detected, and images under
 *      32 pixels in either dimension are not processed.
 *   2'. Virtual grid (instead of 2): scatter estimation and removal, grid
 *      residual, optional pyramid contrast and de-noise, on dst in place.
 *      On failure dst holds the ORIGINAL src pixels -- also when dst aliases
 *      src and the vignette step already ran (REQ-GSVG-024).
 *
 * When both steps are disabled, dst ends up holding exactly the pixels of src:
 * copied when the two buffers differ, and left untouched when dst aliases src.
 *
 * @param handle  GSVG handle returned by xpe_gsvg_init. Must not be NULL.
 * @param src       Source image, width*height uint16 pixels. Must not be NULL.
 * @param srcCount  Number of uint16 ELEMENTS @p src points at.
 * @param dst       Destination image, width*height uint16 pixels. Must not be
 *                  NULL. May alias src for in-place operation.
 * @param dstCount  Number of uint16 ELEMENTS @p dst points at.
 * @param width   Image width in pixels. Must be > 0.
 * @param height  Image height in pixels. Must be > 0.
 * @param gainMap   Optional vignette gain map, width*height float32 entries.
 *                  NULL disables the vignette step regardless of config.
 * @param gainCount Number of float ENTRIES @p gainMap points at. Ignored when
 *                  @p gainMap is NULL -- pass 0 there; a buffer that does not
 *                  exist has no meaningful length, and demanding one would make
 *                  every caller that skips the vignette step invent a number.
 *
 * @par Buffer lengths are counted in ELEMENTS, not bytes (#152).
 * The three buffers have two different element types -- @p src and @p dst are
 * uint16_t, @p gainMap is float -- so a byte count would force every caller to
 * apply the right sizeof to the right argument, and a mixed-up pair would pass
 * validation while still being wrong. An element count is type-correct by
 * construction: each length is compared directly against width * height, the
 * same units @p width and @p height are already stated in. In practice a C++
 * caller passes `buf.size()`, which is the value that is right; the byte form
 * would need `buf.size() * sizeof(...)`, which is the form that gets it wrong.
 *
 * @return XPE_OK on success, including the case where every step was skipped.
 * @return XPE_ERR_INVALID_INPUT on a NULL pointer -- including a NULL handle,
 *         which is a NULL required pointer like any other -- or on a
 *         non-positive dimension. This is the module's half of the shared
 *         empty-image contract (#142): an empty image is an error everywhere in
 *         the post modules, never a silent no-op. gsvg takes loose dimensions
 *         rather than an XpeImageBuffer, so it rejects NEGATIVE dimensions too,
 *         a shape the struct-based modules cannot express.
 * @return XPE_ERR_INVALID_INPUT when a supplied buffer is SHORTER than
 *         width * height elements (#152). QA-B-52 measured what the absence of
 *         this check cost: a src half the promised length was read past its end
 *         and the call still returned XPE_OK. Documenting that -- which the
 *         header used to do -- does not stop it; a caller that misstates the
 *         dimensions gives the function no way to know, and with no length
 *         parameter there was nothing to check against.
 *         Order of judgement matches the api-spec output-buffer rule: NULL and
 *         zero are decided first, a real-but-short buffer after.
 * @return XPE_ERR_CONFIG_INVALID (virtual grid) when the exposure lies outside
 *         the table -- kVp outside a section's range, a grid ratio the table
 *         does not list -- or the settings cannot be used (e.g. an image too
 *         small for the pyramid levels). dst then holds the original pixels
 *         and the reason is pushed to the alert queue.
 *
 * Virtual grid, thickness outside the table (#180, QA-B-93): a region whose
 * estimated thickness is above the table's range is limited to the table
 * maximum and the image is processed; the share of such pixels is pushed as
 * an XPE_ALERT_WARNING. Thickness between 0 and the first kernel node uses
 * that node's kernel faded linearly to no scatter at 0 cm.
 */
XPE_API XpeErrorCode xpe_gsvg_process(void* handle,
                                      const uint16_t* src,
                                      size_t srcCount,
                                      uint16_t* dst,
                                      size_t dstCount,
                                      int width,
                                      int height,
                                      const float* gainMap,
                                      size_t gainCount);

/**
 * @brief xpe_gsvg_process with a collimation field mask (#180, QA-B-96).
 *
 * Same arguments, order of judgement and return codes as xpe_gsvg_process,
 * plus:
 *
 * @param fieldMask Optional, width*height bytes, row-major like @p src:
 *                  non-zero = inside the collimated field. gsvg does not
 *                  detect the field itself (it depends on no other XPE
 *                  module); the caller passes it, e.g. from the collimation
 *                  detection of enhance_advanced.
 * @param maskCount Number of BYTES @p fieldMask points at. Ignored when
 *                  @p fieldMask is NULL.
 *
 * The mask is used by the virtual grid only:
 *  - pixels outside the field are not scatter sources (read as 0 by the
 *    scatter estimate);
 *  - the virtual grid does not change pixels outside the field -- they keep
 *    the value they had when the step began (after the vignette step, if
 *    that ran).
 *
 * With @p fieldMask NULL the call behaves exactly like xpe_gsvg_process: when
 * the virtual grid is enabled it runs without a mask and pushes an
 * XPE_ALERT_WARNING saying so. xpe_gsvg_process pushes the same warning.
 *
 * @return XPE_ERR_INVALID_INPUT, in addition to the xpe_gsvg_process cases,
 *         when @p fieldMask is supplied and @p maskCount < width * height.
 */
XPE_API XpeErrorCode xpe_gsvg_process_masked(void* handle,
                                             const uint16_t* src,
                                             size_t srcCount,
                                             uint16_t* dst,
                                             size_t dstCount,
                                             int width,
                                             int height,
                                             const float* gainMap,
                                             size_t gainCount,
                                             const uint8_t* fieldMask,
                                             size_t maskCount);

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
