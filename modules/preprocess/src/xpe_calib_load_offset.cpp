/**
 * @file xpe_calib_load_offset.cpp
 * @brief xpe_calib_load_offset implementation (T-006)
 *
 * REQ-P1A-014: Load XCal v1 OFFSET calibration map.
 * REQ-P1A-030: No C++ exceptions across C ABI boundary.
 * REQ-P1A-031: RAII for automatic cleanup on error.
 *
 * @MX:ANCHOR: [AUTO] Public API entry point for offset calibration load
 * @MX:REASON: Called by xpe_offset_correct (fan_in >= 3); g_calib write path
 */

#include "xpe/preprocess_api.h"
#include "xpe_preprocess_internal.h"
#include "xcal_reader.hpp"

#include <mutex>
#include <cstring>
#include <vector>
#include <chrono>

extern "C" XPE_API XpeErrorCode xpe_calib_load_offset(const char* filepath) {
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
            /*expected_type=*/XCAL_TYPE_OFFSET);
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

        // Commit under mutex (read-then-commit; no TOCTOU exposure)
        {
            std::lock_guard<std::mutex> lock(g_calib_mutex);
            g_calib.offset_map    = std::move(map);
            g_calib.offset_width  = hdr.width;
            g_calib.offset_height = hdr.height;
            g_calib.offset_timestamp = hdr.created_epoch_ms;

            // Copy session_id (null-terminated, up to 63 chars)
            std::memset(g_calib.offset_session_id, 0, sizeof(g_calib.offset_session_id));
            std::memcpy(g_calib.offset_session_id, hdr.session_id,
                        sizeof(hdr.session_id) < sizeof(g_calib.offset_session_id)
                            ? sizeof(hdr.session_id)
                            : sizeof(g_calib.offset_session_id) - 1);
        }

        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
