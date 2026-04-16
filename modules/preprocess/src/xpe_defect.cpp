/**
 * @file xpe_defect.cpp
 * @brief XPE Defect Correction Implementation (Phase 3)
 *
 * REQ-P1A-012: Defect correction with 5x5 neighborhood
 * AC-DEF-001: Edge-aware bilinear interpolation excluding center
 * AC-DEF-002: Static BPM priority over runtime detection
 * AC-DEF-003: Runtime transient defect detection
 */

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_memory.h"
#include <cstring>

// Calibration data is defined in xpe_calibration.cpp
// Access through proper API functions (to be implemented)
// For now, use stub implementation

/**
 * @brief Execute defect correction using edge-aware interpolation
 *
 * REQ-P1A-012: Defect correction with 5x5 neighborhood
 * AC-DEF-001: Edge-aware bilinear interpolation excluding center
 * AC-DEF-002: Static BPM priority over runtime detection
 * REQ-P1A-030: No exceptions across C ABI boundary
 */
extern "C" XPE_API XpeErrorCode xpe_defect_correct(const XpeImageBuffer* input,
                                                   XpeImageBuffer* output,
                                                   const XpeImageMetadata* metadata) {
    try {
        // Validate input
        if (input == nullptr || output == nullptr || metadata == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }

        // Validate format
        if (input->format != XPE_PIXEL_FLOAT32 || output->format != XPE_PIXEL_FLOAT32) {
            return XPE_ERR_UNSUPPORTED_FORMAT;
        }

        // Validate dimensions
        if (input->width != output->width || input->height != output->height) {
            return XPE_ERR_BUFFER_TOO_SMALL;
        }

        // Phase 1 stub: Copy input to output (no defect correction yet)
        std::memcpy(output->data, input->data,
                   input->width * input->height * sizeof(float));

        // TODO: AC-DEF-001: Edge-aware bilinear interpolation excluding center
        // TODO: AC-DEF-002: Static BPM priority over runtime detection
        // TODO: AC-DEF-003: Runtime transient defect detection

        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
