/**
 * @file xpe_calib_load_defect_map.cpp
 * @brief xpe_calib_load_defect_map implementation (T-006)
 *
 * REQ-P1A-016: Load XCal v1 DEFECT calibration map (BPM format).
 * REQ-P1A-030: No C++ exceptions across C ABI boundary.
 * REQ-P1A-031: RAII for automatic cleanup on error.
 *
 * @MX:ANCHOR: [AUTO] Public API entry point for defect map load
 * @MX:REASON: Called by xpe_defect_correct (fan_in >= 3); g_calib write path
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include "xcal_reader.hpp"

#include <mutex>
#include <cstring>
#include <vector>

extern "C" XPE_API XpeErrorCode xpe_calib_load_defect_map(const char* filepath) {
    try {
        if (filepath == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }

        // Read and validate XCal v1 file (SHA-256 + magic + type)
        // Defect maps never expire: pass check_expiry=false
        XCalFileHeader hdr;
        std::vector<uint8_t> config_json;
        std::vector<uint8_t> payload;

        XpeErrorCode rc = read_xcal_file(
            filepath, hdr, config_json, payload,
            /*check_expiry=*/false,
            /*expected_type=*/XCAL_TYPE_DEFECT);
        if (rc != XPE_OK) {
            return rc;
        }

        // Verify payload size matches declared dimensions (uint8 per pixel)
        size_t expected = static_cast<size_t>(hdr.width) * hdr.height;
        if (payload.size() != expected) {
            return XPE_ERR_CONFIG_INVALID;
        }

        // Allocate and copy BPM data
        auto map = std::make_unique<uint8_t[]>(expected);
        std::memcpy(map.get(), payload.data(), payload.size());

        // Commit under mutex
        {
            std::lock_guard<std::mutex> lock(g_calib_mutex);
            g_calib.defect_map    = std::move(map);
            g_calib.defect_width  = hdr.width;
            g_calib.defect_height = hdr.height;
        }

        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
