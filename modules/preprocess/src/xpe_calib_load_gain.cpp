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

        // SRS-CALIB-FUNC-002 (#188, QA-A-107): "Values shall be in range
        // [0.1, 10.0]; out-of-range values shall trigger
        // XPE_ERR_INVALID_CALIB_DATA error." Checked here, at load, which is
        // where FUNC-002 places it. Scalar maps only: a coefficient of
        // G(x,y,E) is not a gain value.
        if (!is_poly) {
            const float* values = reinterpret_cast<const float*>(payload.data());
            for (size_t i = 0; i < n_floats; ++i) {
                if (!(values[i] >= XPE_CALIB_GAIN_MIN && values[i] <= XPE_CALIB_GAIN_MAX)) {
                    return XPE_ERR_INVALID_CALIB_DATA;
                }
            }
        }

        // Allocate and copy pixel data
        // Overwritten by the memcpy below; no value-initialisation (QA-A-105).
        std::unique_ptr<float[]> map(new float[n_floats]);
        std::memcpy(map.get(), payload.data(), payload.size());

        // Commit under mutex. The two gain models are alternatives: whichever
        // is loaded clears the other, so a scalar map left over from an earlier
        // file is never applied to frames the operator calibrated with a
        // polynomial (SRS-CALIB-SAFE-003: no partial / mixed calibration).
        bool poly_loaded = false;
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
            poly_loaded = is_poly;

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

        // QA-A-107 (#187): the file is read and its metadata is available, but no
        // correction path applies the coefficients. Say so here -- the load is
        // where the operator can still act on it.
        if (poly_loaded) {
            xpe_alert_push("gain polynomial loaded (XCAL_TYPE_GAIN_POLY): metadata is "
                           "available but no correction applies G(x,y,E) yet (issue #187); "
                           "gain correction will refuse until a scalar gain map is loaded",
                           XPE_ALERT_WARNING);
        }

        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
