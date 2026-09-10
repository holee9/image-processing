/**
 * @file xpe_calib_load_gain.cpp
 * @brief xpe_calib_load_gain implementation (T-006)
 *
 * REQ-P1A-015: Load XCal v1 GAIN calibration map.
 * QA-A-37 (#140): also accepts XCAL_TYPE_GAIN_POLY coefficient files.
 * REQ-P1A-030: No C++ exceptions across C ABI boundary.
 * REQ-P1A-031: RAII for automatic cleanup on error.
 *
 * @MX:ANCHOR: [AUTO] Public API entry point for gain calibration load
 * @MX:REASON: Called by xpe_gain_correct (fan_in >= 3); g_calib write path
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include "xcal_reader.hpp"

#include <mutex>
#include <cstring>
#include <string>
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

        // QA-A-37 (#140): both gain representations are accepted here --
        // XCAL_TYPE_GAIN (one scalar plane) and XCAL_TYPE_GAIN_POLY
        // ((degree+1) coefficient planes, written by
        // xpe_calib_generate_gain_polynomial). The reader takes a single
        // expected type, so the type check is done here instead; the code for
        // a mismatch is the one the reader would have returned,
        // XPE_ERR_CONFIG_INVALID, so the existing contract is unchanged.
        XpeErrorCode rc = read_xcal_file(
            filepath, hdr, config_json, payload,
            /*check_expiry=*/true,
            /*expected_type=*/-1);
        if (rc != XPE_OK) {
            return rc;
        }

        const bool is_poly = (hdr.type == static_cast<uint32_t>(XCAL_TYPE_GAIN_POLY));
        if (!is_poly && hdr.type != static_cast<uint32_t>(XCAL_TYPE_GAIN)) {
            return XPE_ERR_CONFIG_INVALID;
        }

        // Verify payload size matches declared dimensions. A polynomial file
        // holds a whole number of planes; validate_xcal_header already refused
        // a ragged or empty payload, so the division here is exact.
        const size_t plane = static_cast<size_t>(hdr.width) * hdr.height * sizeof(float);
        if (plane == 0) {
            return XPE_ERR_CONFIG_INVALID;
        }
        if (is_poly) {
            if (payload.size() == 0 || payload.size() % plane != 0) {
                return XPE_ERR_CONFIG_INVALID;
            }
        } else if (payload.size() != plane) {
            return XPE_ERR_CONFIG_INVALID;
        }

        const size_t num_coeffs = is_poly ? (payload.size() / plane) : 1;
        const size_t n_floats   = payload.size() / sizeof(float);

        // Allocate and copy pixel data
        auto map = std::make_unique<float[]>(n_floats);
        std::memcpy(map.get(), payload.data(), payload.size());

        // Commit under mutex. The two gain models are alternatives: whichever
        // is loaded clears the other, so xpe_gain_correct() reports
        // CALIB_NOT_LOADED after a POLY load rather than applying a scalar map
        // left over from an earlier file.
        {
            std::lock_guard<std::mutex> lock(g_calib_mutex);
            if (is_poly) {
                g_calib.gain_poly_coeffs     = std::move(map);
                g_calib.gain_poly_num_coeffs = static_cast<uint32_t>(num_coeffs);
                g_calib.gain_map.reset();
            } else {
                g_calib.gain_map = std::move(map);
                g_calib.gain_poly_coeffs.reset();
                g_calib.gain_poly_num_coeffs = 0;
            }
            g_calib.gain_width  = hdr.width;
            g_calib.gain_height = hdr.height;
            g_calib.gain_timestamp = hdr.created_epoch_ms;

            std::memset(g_calib.gain_session_id, 0, sizeof(g_calib.gain_session_id));
            std::memcpy(g_calib.gain_session_id, hdr.session_id,
                        sizeof(hdr.session_id) < sizeof(g_calib.gain_session_id)
                            ? sizeof(hdr.session_id)
                            : sizeof(g_calib.gain_session_id) - 1);
        }

        // FUNC-033 (5): restore the quality metadata the file carries, so
        // xpe_calib_get_quality_meta() describes the calibration now in use.
        // A file written before QA-A-35 has no such fields and is loaded
        // unchanged -- the call simply reports that it found none.
        {
            std::string json(config_json.begin(), config_json.end());
            xpe_calib_apply_quality_meta_json(json.c_str());
        }

        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
