/**
 * @file xpe_calibration.cpp
 * @brief XPE Calibration Global State definitions + xpe_validate_readout_artifact
 *
 * Defines global g_calib + g_calib_mutex (shared across all calibration functions).
 * All per-function implementations have been split into separate translation units:
 *   T-006: xpe_calib_load_offset/gain/defect_map.cpp
 *   T-007: xpe_calib_save.cpp
 *   T-008: xpe_calib_generate_offset.cpp
 *   T-009: xpe_calib_check_expiry.cpp
 *
 * REQ-P1A-031: RAII via unique_ptr for automatic cleanup
 * REQ-P1A-041: xpe_validate_readout_artifact remains here (Phase 2 misc)
 */

#include "xpe/preprocess_api.h"
#include "xpe_preprocess_internal.h"
#include <mutex>
#include <cstring>

// =============================================================================
// Internal Calibration State
// =============================================================================

// Global calibration data and mutex definitions
CalibrationData g_calib;
std::mutex g_calib_mutex;

// @MX:ANCHOR: [AUTO] Global calibration data shared across preprocessing algorithms

// =============================================================================
// Phase 4: Calibration Management Functions
// (load functions moved to xpe_calib_load_offset/gain/defect_map.cpp)
// =============================================================================

/**
 * @brief Validate and mask readout artifacts
 *
 * REQ-P1A-041: Validate readout artifacts before correction
 * REQ-P1A-030: No exceptions across C ABI boundary
 */
extern "C" XPE_API XpeErrorCode xpe_validate_readout_artifact(const XpeImageBuffer* image,
                                                             const XpeImageMetadata* metadata,
                                                             bool* has_dropped_columns,
                                                             bool* has_nonuniform_gain) {
    try {
        // Validate input
        if (image == nullptr || metadata == nullptr ||
            has_dropped_columns == nullptr || has_nonuniform_gain == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }

        // Initialize outputs
        *has_dropped_columns = false;
        *has_nonuniform_gain = false;

        // Check for dropped columns (columns with all zeros)
        if (image->format == XPE_PIXEL_UINT16) {
            const uint16_t* data = static_cast<const uint16_t*>(image->data);

            // Check each column
            for (uint32_t col = 0; col < image->width; ++col) {
                bool column_is_zero = true;

                for (uint32_t row = 0; row < image->height; ++row) {
                    if (data[row * image->width + col] != 0) {
                        column_is_zero = false;
                        break;
                    }
                }

                if (column_is_zero) {
                    *has_dropped_columns = true;
                    break;
                }
            }
        }

        // Check for nonuniform gain (simple variance check)
        // TODO: Implement more sophisticated gain uniformity check
        if (image->format == XPE_PIXEL_FLOAT32) {
            const float* data = static_cast<const float*>(image->data);

            // Calculate mean
            double sum = 0.0;
            size_t pixel_count = image->width * image->height;
            for (size_t i = 0; i < pixel_count; ++i) {
                sum += data[i];
            }
            double mean = sum / pixel_count;

            // Calculate variance
            double variance = 0.0;
            for (size_t i = 0; i < pixel_count; ++i) {
                double diff = data[i] - mean;
                variance += diff * diff;
            }
            variance /= pixel_count;

            // Check if variance exceeds threshold (5% of mean)
            double threshold = mean * 0.05;
            if (variance > threshold * threshold) {
                *has_nonuniform_gain = true;
            }
        }

        return XPE_OK;

    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
