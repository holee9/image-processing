#ifndef XPE_ENHANCE_ADVANCED_API_H
#define XPE_ENHANCE_ADVANCED_API_H

#include "xpe/common/xpe_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Lifecycle Management (REQ-ADV-001, REQ-ADV-020)
 * ============================================================================ */

/**
 * Initialize the enhance_advanced module with default or custom configuration
 *
 * @param configJsonOrNull Optional JSON configuration string (NULL for defaults)
 * @return XPE_OK on success, error code on failure
 *
 * REQ-ADV-001: Module initialization
 * AC-LC-001: Initialization with default config
 */
XPE_API XpeErrorCode xpe_enhance_advanced_init(const char* configJsonOrNull);

/**
 * Shutdown the enhance_advanced module and release all resources
 *
 * REQ-ADV-020: Not-initialized guard
 * AC-LC-003: Shutdown after init
 */
XPE_API void xpe_enhance_advanced_shutdown(void);

/**
 * Get the enhance_advanced module version string
 *
 * @return Version string (e.g., "1.0.0")
 */
XPE_API const char* xpe_enhance_advanced_version(void);

/* ============================================================================
 * Multiscale Frequency Processing (SWU-2.5, REQ-ADV-010)
 * ============================================================================ */

/**
 * Apply multiscale frequency processing using Laplacian pyramid decomposition
 *
 * @param img Input/output image buffer (FLOAT32 format required)
 * @param meta Image metadata including body part information
 * @param configJsonOrNull Optional JSON configuration for enhancement coefficients
 * @return XPE_OK on success, error code on failure
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
 * @param img Input/output image buffer (FLOAT32 format required)
 * @param order Fractional derivative order in range [0.0, 2.0]
 *              - Near 1.0: Preserves edges
 *              - Near 2.0: Emphasizes fine texture
 * @param configJsonOrNull Optional JSON configuration
 * @return XPE_OK on success, error code on failure
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
 * @param img Input image buffer (FLOAT32 format required)
 * @param x0Out Output left boundary pixel coordinate
 * @param y0Out Output top boundary pixel coordinate
 * @param x1Out Output right boundary pixel coordinate
 * @param y1Out Output bottom boundary pixel coordinate
 * @param configJsonOrNull Optional JSON configuration
 * @return XPE_OK on success, error code on failure
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
 * @param img Input detector-domain image buffer (FLOAT32 format required)
 * @param meta Image metadata including body part and acquisition parameters
 * @param eiOut Output calculated Exposure Index
 * @param deviationIndexOut Output calculated Deviation Index
 * @return XPE_OK on success, error code on failure
 *
 * REQ-ADV-013: Exposure index calculation
 * AC-EI-001~AC-EI-004: Exposure index acceptance criteria
 */
XPE_API XpeErrorCode xpe_calc_exposure_index(
    const XpeImageBuffer* img,
    const XpeImageMetadata* meta,
    float* eiOut,
    float* deviationIndexOut);

#ifdef __cplusplus
}
#endif

#endif /* XPE_ENHANCE_ADVANCED_API_H */
