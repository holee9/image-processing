/**
 * @file xpe_calib_load_gain.cpp
 * @brief xpe_calib_load_gain implementation (T-006)
 *
 * REQ-P1A-015: Load XCal v1 GAIN calibration map.
 * REQ-P1A-030: No C++ exceptions across C ABI boundary.
 * REQ-P1A-031: RAII for automatic cleanup on error.
 *
 * @MX:ANCHOR: [AUTO] Public API entry point for gain calibration load
 * @MX:REASON: Called by xpe_gain_correct (fan_in >= 3); g_calib write path
 */

#include "xpe/preprocess_api.h"
#include "xpe_preprocess_internal.h"
#include "xcal_reader.hpp"

#include <mutex>
#include <cstring>
#include <vector>

extern "C" XPE_API XpeErrorCode xpe_calib_load_gain(const char* filepath) {
    try {
        if (filepath == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }

        // Read and validate XCal v1 file (SHA-256 + magic + type + expiry)
        XCalFileHeader hdr;
        std::vector<uint8_t> config_json;
        std::vector<uint8_t> payload;

        XpeErrorCode rc = read_xcal_file(
            filepath, hdr, config_json, payload,
            /*check_expiry=*/true,
            /*expected_type=*/XCAL_TYPE_GAIN);
        if (rc != XPE_OK) {
            return rc;
        }

        // Verify payload size matches declared dimensions
        size_t expected = static_cast<size_t>(hdr.width) * hdr.height * sizeof(float);
        if (payload.size() != expected) {
            return XPE_ERR_CONFIG_INVALID;
        }

        // Allocate and copy pixel data
        auto map = std::make_unique<float[]>(static_cast<size_t>(hdr.width) * hdr.height);
        std::memcpy(map.get(), payload.data(), payload.size());

        // Commit under mutex
        {
            std::lock_guard<std::mutex> lock(g_calib_mutex);
            g_calib.gain_map    = std::move(map);
            g_calib.gain_width  = hdr.width;
            g_calib.gain_height = hdr.height;
            g_calib.gain_timestamp = hdr.created_epoch_ms;

            std::memset(g_calib.gain_session_id, 0, sizeof(g_calib.gain_session_id));
            std::memcpy(g_calib.gain_session_id, hdr.session_id,
                        sizeof(hdr.session_id) < sizeof(g_calib.gain_session_id)
                            ? sizeof(hdr.session_id)
                            : sizeof(g_calib.gain_session_id) - 1);
        }

        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
