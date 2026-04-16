/**
 * @file xpe_gain.cpp
 * @brief XPE Gain Correction Implementation (Phase 3)
 *
 * REQ-P1A-011: Gain correction with format conversion
 * AC-GAIN-001: UINT16 to FLOAT32 conversion, divide by gain map
 * AC-GAIN-002: Multi-SID gain interpolation
 * AC-GAIN-003: Validate NaN/Inf values
 */

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_memory.h"
#include <mutex>
#include <cstring>
#include <cmath>

// Calibration data is defined in xpe_calibration.cpp
// Access through proper API functions (to be implemented)
// For now, use stub implementation

/**
 * @brief Execute gain correction with UINT16→FLOAT32 conversion
 *
 * REQ-P1A-011: Gain correction with format conversion
 * AC-GAIN-001: UINT16 to FLOAT32 conversion, divide by gain map
 * AC-GAIN-003: Validate NaN/Inf values
 * REQ-P1A-030: No exceptions across C ABI boundary
 */
// Stub implementation for Phase 1
// Full implementation will be added in Phase 3 with proper calibration data access
extern "C" XPE_API XpeErrorCode xpe_gain_correct(const XpeImageBuffer* input,
                                                 XpeImageBuffer* output,
                                                 const XpeImageMetadata* metadata) {
    try {
        // Validate input
        if (input == nullptr || output == nullptr || metadata == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }

        // Validate format
        if (input->format != XPE_PIXEL_UINT16 || output->format != XPE_PIXEL_FLOAT32) {
            return XPE_ERR_UNSUPPORTED_FORMAT;
        }

        // Validate dimensions
        if (input->width != output->width || input->height != output->height) {
            return XPE_ERR_BUFFER_TOO_SMALL;
        }

        // Phase 1 stub: Convert UINT16 to FLOAT32 (no gain correction yet)
        const uint16_t* input_data = static_cast<const uint16_t*>(input->data);
        float* output_data = static_cast<float*>(output->data);

        // Integer overflow prevention check
        if (input->width > 0 && input->height > 0 &&
            input->width <= (SIZE_MAX / input->height)) {
            for (size_t i = 0; i < input->width * input->height; ++i) {
                output_data[i] = static_cast<float>(input_data[i]);
            }
        } else {
            return XPE_ERR_INVALID_INPUT;
        }

        // TODO: AC-GAIN-001: I_gain = I_offset / gain_map
        // TODO: AC-GAIN-002: Multi-SID interpolation based on metadata->SID_mm
        // TODO: AC-GAIN-003: Handle division by zero and invalid values

        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
