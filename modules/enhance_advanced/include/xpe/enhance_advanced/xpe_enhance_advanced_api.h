#ifndef XPE_ENHANCE_ADVANCED_API_H
#define XPE_ENHANCE_ADVANCED_API_H

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Lifecycle Management (REQ-ADV-001, REQ-ADV-020)
 * ============================================================================ */

/**
 * Initialize the enhance_advanced module
 *
 * NOTE: a supplied configuration string is currently only checked for being
 * parseable JSON; its values are not stored and do not affect any processing
 * function (xpe_enhance_advanced.cpp:46-54 carries the TODO). Per-call
 * configuration is passed through each processing function's own
 * configJsonOrNull argument.
 *
 * @param configJsonOrNull Optional JSON configuration string, or NULL. An empty
 *                         string is rejected -- pass NULL instead.
 * @return XPE_OK on success; XPE_ERR_CONFIG_INVALID if the string is empty or
 *         is not parseable JSON; otherwise whatever xpe_init() returned, except
 *         that XPE_ERR_NOT_INITIALIZED from xpe_init() is treated as success.
 *
 * REQ-ADV-001: Module initialization
 * AC-LC-001: Initialization with default config
 */
XPE_API XpeErrorCode xpe_enhance_advanced_init(const char* configJsonOrNull);

/**
 * Shutdown the enhance_advanced module
 *
 * Clears the initialized flag under the init mutex. The module owns no
 * resources of its own to release, and it does not shut down xpe_common.
 * Calling it when the module was never initialized is a no-op, not an error.
 *
 * REQ-ADV-020: Not-initialized guard
 * AC-LC-003: Shutdown after init
 */
XPE_API void xpe_enhance_advanced_shutdown(void);

/**
 * Get the enhance_advanced module version string
 *
 * @return Null-terminated version string ("1.0.0", from
 *         XPE_ENHANCE_ADVANCED_VERSION). Lifetime: process. Never NULL.
 *         Callable before init.
 */
XPE_API const char* xpe_enhance_advanced_version(void);

/* ============================================================================
 * Multiscale Frequency Processing (SWU-2.5, REQ-ADV-010)
 * ============================================================================ */

/**
 * Apply multiscale frequency processing using Laplacian pyramid decomposition
 *
 * @param img Input/output image buffer (FLOAT32 format required). NULL, a NULL
 *            data pointer, a zero width or height, or a dataSize inconsistent
 *            with the declared dimensions (#123) are all rejected.
 * @param meta Image metadata including body part information. NULL is rejected.
 * @param configJsonOrNull Optional JSON configuration for enhancement coefficients
 * @return XPE_OK on success;
 *         XPE_ERR_INVALID_INPUT for a NULL img/meta, NULL img->data, zero
 *         dimensions, or inconsistent dataSize;
 *         XPE_ERR_NOT_INITIALIZED if xpe_enhance_advanced_init() has not been
 *         called -- checked after the NULL guard but before the buffer contents;
 *         XPE_ERR_UNSUPPORTED_FORMAT if img is not FLOAT32;
 *         XPE_ERR_CONFIG_INVALID if configJsonOrNull does not parse;
 *         XPE_ERR_INTERNAL if processing throws.
 *
 * REQ-ADV-010: MFP execution
 * REQ-ADV-050: Identity reconstruction fidelity
 * AC-MFP-001~AC-MFP-006: MFP acceptance criteria
 */
XPE_API XpeErrorCode xpe_multiscale_process(
    XpeImageBuffer* img,
    const XpeImageMetadata* meta,
    const char* configJsonOrNull);

/* ============================================================================
 * Fractional-Order Edge Enhancement (SWU-2.6, REQ-ADV-011)
 * ============================================================================ */

/**
 * Apply fractional-order differentiation for edge enhancement
 *
 * @param img Input/output image buffer (FLOAT32 format required). NULL, a NULL
 *            data pointer, a zero width or height, or a dataSize inconsistent
 *            with the declared dimensions (#123) are all rejected.
 * @param order Fractional derivative order in range [0.0, 2.0]
 *              - Near 1.0: Preserves edges
 *              - Near 2.0: Emphasizes fine texture
 * @param configJsonOrNull Optional JSON configuration
 * @return XPE_OK on success;
 *         XPE_ERR_INVALID_INPUT for a NULL img, NULL img->data, zero
 *         dimensions, inconsistent dataSize, or order outside [0.0, 2.0];
 *         XPE_ERR_NOT_INITIALIZED if the module was not initialized;
 *         XPE_ERR_UNSUPPORTED_FORMAT if img is not FLOAT32;
 *         XPE_ERR_CONFIG_INVALID if configJsonOrNull does not parse;
 *         XPE_ERR_SAFETY_VIOLATION if the configuration would disable overshoot
 *         limiting, or if the limiter reports a violation (SAF-100);
 *         XPE_ERR_INTERNAL if processing throws.
 *
 * REQ-ADV-011: Fractional-order process execution
 * REQ-ADV-021: Invalid order parameter guard
 * REQ-ADV-051: Mandatory overshoot limiting (SAF-100)
 * AC-EDGE-001~AC-EDGE-005: Edge enhancement acceptance criteria
 */
XPE_API XpeErrorCode xpe_fractional_process(
    XpeImageBuffer* img,
    float order,
    const char* configJsonOrNull);

/* ============================================================================
 * Collimation ROI Detection (SWU-2.8, REQ-ADV-012)
 * ============================================================================ */

/**
 * Detect collimation boundaries using Hough transform
 *
 * @param img Input image buffer (FLOAT32 format required). NULL, a NULL data
 *            pointer, a zero width or height, or a dataSize inconsistent with
 *            the declared dimensions (#123) are all rejected.
 * @param x0Out Output left boundary pixel coordinate. NULL is rejected.
 * @param y0Out Output top boundary pixel coordinate. NULL is rejected.
 * @param x1Out Output right boundary pixel coordinate. NULL is rejected.
 * @param y1Out Output bottom boundary pixel coordinate. NULL is rejected.
 * @param configJsonOrNull Optional JSON configuration
 * @return XPE_OK on success -- including the low-confidence fallback, which
 *         reports the full image extent rather than an error (REQ-ADV-041);
 *         XPE_ERR_INVALID_INPUT for a NULL img or any NULL output pointer, NULL
 *         img->data, zero dimensions, or inconsistent dataSize;
 *         XPE_ERR_NOT_INITIALIZED if the module was not initialized;
 *         XPE_ERR_UNSUPPORTED_FORMAT if img is not FLOAT32;
 *         XPE_ERR_CONFIG_INVALID if configJsonOrNull does not parse;
 *         XPE_ERR_INTERNAL if detection throws.
 *
 * REQ-ADV-012: Collimation detection execution
 * REQ-ADV-041: Confidence-based fallback
 * REQ-ADV-052: Collimation detection accuracy
 * AC-COL-001~AC-COL-004: Collimation acceptance criteria
 */
XPE_API XpeErrorCode xpe_detect_collimation(
    const XpeImageBuffer* img,
    int32_t* x0Out,
    int32_t* y0Out,
    int32_t* x1Out,
    int32_t* y1Out,
    const char* configJsonOrNull);

/* ============================================================================
 * Exposure Index Calculation (SWU-2.10, REQ-ADV-013)
 * ============================================================================ */

/**
 * Calculate IEC 62494-1 Exposure Index (EI) and Deviation Index (DI)
 *
 * RENAMED 2026-09-12 (#153). This entry point was called
 * xpe_calc_exposure_index, the same name xpe_enhance_basic exports. The note
 * that used to stand here said the two were separate implementations with
 * different contracts and that a consumer loading both had to resolve
 * explicitly. That was accurate and it did not help: with one name there was
 * nothing for a C++ caller to resolve BETWEEN. A translation unit including
 * both headers compiled, linked and called whichever the linker picked, with no
 * diagnostic (tests/e2e_post_pipeline/test_e2e_full_pipeline.cpp is such a
 * unit). QA-B-54 measured the cost: the two return EI 200 and EI 100000 for the
 * same uniform input -- a factor of 500 decided by link order.
 *
 * The basic export kept its name because the C# bindings and the GUI
 * RequiredExports list name xpe_calc_exposure_index and all of them mean basic;
 * renaming there would have broken working consumers to fix a C++ hazard.
 *
 * The two implementations still DISAGREE -- the rename made the choice visible,
 * not the answers equal. Which one satisfies REQ-ENH-030 versus REQ-ADV-013 is
 * a separate question; until it is settled, call the one you mean by name.
 * This one requires xpe_enhance_advanced_init(); the enhance_basic one does not.
 *
 * @param img Input detector-domain image buffer (FLOAT32 format required).
 *            NULL, NULL data, zero dimensions, or a dataSize inconsistent with
 *            the declared dimensions (#123) are all rejected.
 * @param meta Image metadata including body part and acquisition parameters.
 *             NULL is rejected.
 * @param eiOut Output calculated Exposure Index. NULL is rejected.
 * @param deviationIndexOut Output calculated Deviation Index. NULL is rejected.
 * @return XPE_OK on success;
 *         XPE_ERR_INVALID_INPUT for a NULL argument, zero dimensions, or
 *         inconsistent dataSize;
 *         XPE_ERR_NOT_INITIALIZED if the module was not initialized;
 *         XPE_ERR_UNSUPPORTED_FORMAT if img is not FLOAT32;
 *         XPE_ERR_INTERNAL if the computation throws.
 *
 * REQ-ADV-013: Exposure index calculation
 * AC-EI-001~AC-EI-004: Exposure index acceptance criteria
 */
XPE_API XpeErrorCode xpe_adv_calc_exposure_index(
    const XpeImageBuffer* img,
    const XpeImageMetadata* meta,
    float* eiOut,
    float* deviationIndexOut);

#ifdef __cplusplus
}
#endif

#endif /* XPE_ENHANCE_ADVANCED_API_H */
