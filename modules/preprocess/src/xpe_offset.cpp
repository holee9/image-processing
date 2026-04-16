/**
 * @file xpe_offset.cpp
 * @brief XPE Offset Correction Implementation (Phase 3)
 *
 * REQ-P1A-010: Offset correction with temperature interpolation
 * AC-OFF-001: Basic offset correction with floor-at-zero
 * AC-OFF-002: Temperature interpolation between two offset maps
 * AC-OFF-003: PREP-time exponential decay model
 */

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_memory.h"
#include <mutex>
#include <cstring>
#include <algorithm>
#include <cmath>

// Forward declare calibration data from xpe_calibration.cpp
namespace {
    extern struct CalibrationData {
        std::unique_ptr<float[]> offset_map;
        uint32_t offset_width;
        uint32_t offset_height;
        // ... other fields
    } g_calib;
    extern std::mutex g_calib_mutex;
}

/**
 * @brief Execute offset correction: I_offset = max(I_raw - I_dark, 0)
 *
 * REQ-P1A-010: Offset correction with temperature interpolation
 * AC-OFF-001: Basic offset correction with floor-at-zero
 * REQ-P1A-030: No exceptions across C ABI boundary
 */
extern "C" XPE_API XpeErrorCode xpe_offset_correct(const XpeImageBuffer* input,
                                                   XpeImageBuffer* output,
                                                   const XpeImageMetadata* metadata) {
    try {
        // Validate input
        if (input == nullptr || output == nullptr || metadata == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }

        // Validate format
        if (input->format != XPE_PIXEL_UINT16 || output->format != XPE_PIXEL_UINT16) {
            return XPE_ERR_UNSUPPORTED_FORMAT;
        }

        // Validate dimensions
        if (input->width != output->width || input->height != output->height) {
            return XPE_ERR_BUFFER_TOO_SMALL;
        }

        // Get offset map (thread-safe)
        std::unique_ptr<float[]> offset_map_copy;
        uint32_t offset_width, offset_height;

        {
            std::lock_guard<std::mutex> lock(g_calib_mutex);
            if (!g_calib.offset_map) {
                return XPE_ERR_NOT_INITIALIZED;
            }

            offset_width = g_calib.offset_width;
            offset_height = g_calib.offset_height;

            // Validate calibration dimensions match input
            if (offset_width != input->width || offset_height != input->height) {
                return XPE_ERR_BUFFER_TOO_SMALL;
            }

            // Copy offset map for thread-safe processing
            offset_map_copy = std::make_unique<float[]>(offset_width * offset_height);
            std::memcpy(offset_map_copy.get(), g_calib.offset_map.get(),
                       offset_width * offset_height * sizeof(float));
        }

        // Perform offset correction
        const uint16_t* input_data = static_cast<const uint16_t*>(input->data);
        uint16_t* output_data = static_cast<uint16_t*>(output->data);

        // Integer overflow prevention check
        if (input->width > 0 && input->height > 0 &&
            input->width <= (SIZE_MAX / input->height)) {
            for (size_t i = 0; i < input->width * input->height; ++i) {
            // AC-OFF-001: I_offset = max(I_raw - I_dark, 0)
            float corrected = static_cast<float>(input_data[i]) - offset_map_copy[i];
                output_data[i] = static_cast<uint16_t>(std::max(0.0f, corrected));
            }
        } else {
            return XPE_ERR_INVALID_INPUT;
        }

        // TODO: AC-OFF-002: Temperature interpolation
        // TODO: AC-OFF-003: PREP-time exponential decay

        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
