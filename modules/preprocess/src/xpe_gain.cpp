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

// Forward declare calibration data from xpe_calibration.cpp
namespace {
    extern struct CalibrationData {
        std::unique_ptr<float[]> gain_map;
        uint32_t gain_width;
        uint32_t gain_height;
        // ... other fields
    } g_calib;
    extern std::mutex g_calib_mutex;
}

/**
 * @brief Execute gain correction with UINT16→FLOAT32 conversion
 *
 * REQ-P1A-011: Gain correction with format conversion
 * AC-GAIN-001: UINT16 to FLOAT32 conversion, divide by gain map
 * AC-GAIN-003: Validate NaN/Inf values
 * REQ-P1A-030: No exceptions across C ABI boundary
 */
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

        // Get gain map (thread-safe)
        std::unique_ptr<float[]> gain_map_copy;
        uint32_t gain_width, gain_height;

        {
            std::lock_guard<std::mutex> lock(g_calib_mutex);
            if (!g_calib.gain_map) {
                return XPE_ERR_NOT_INITIALIZED;
            }

            gain_width = g_calib.gain_width;
            gain_height = g_calib.gain_height;

            // Validate calibration dimensions match input
            if (gain_width != input->width || gain_height != input->height) {
                return XPE_ERR_BUFFER_TOO_SMALL;
            }

            // Copy gain map for thread-safe processing
            gain_map_copy = std::make_unique<float[]>(gain_width * gain_height);
            std::memcpy(gain_map_copy.get(), g_calib.gain_map.get(),
                       gain_width * gain_height * sizeof(float));
        }

        // Perform gain correction
        const uint16_t* input_data = static_cast<const uint16_t*>(input->data);
        float* output_data = static_cast<float*>(output->data);

        // Integer overflow prevention check
        if (input->width > 0 && input->height > 0 &&
            input->width <= (SIZE_MAX / input->height)) {
            for (size_t i = 0; i < input->width * input->height; ++i) {
            // AC-GAIN-001: I_gain = I_offset / gain_map
            float gain = gain_map_copy[i];

            // AC-GAIN-003: Handle division by zero and invalid values
            if (gain <= 0.0f || !std::isfinite(gain)) {
                output_data[i] = 0.0f;  // Default to 0 for invalid gain
            } else {
                    output_data[i] = static_cast<float>(input_data[i]) / gain;
                }
            }
        } else {
            return XPE_ERR_INVALID_INPUT;
        }

        // TODO: AC-GAIN-002: Multi-SID interpolation based on metadata->SID_mm

        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
