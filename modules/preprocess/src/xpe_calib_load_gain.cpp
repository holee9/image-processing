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

#include <cstdio>
#include <mutex>
#include <cstring>
#include <string>
#include <vector>

XpeErrorCode xpe_calib_stage_gain(const char* filepath, StagedGain* out) noexcept {
    try {
        if (filepath == nullptr || out == nullptr) {
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

        // Everything that allocates, and so can throw, is done HERE, before the commit below. The
        // commit only moves pointers and copies plain values, and nothing after it may throw: a
        // throw once the first field was written reported OUT_OF_MEMORY for a store that had already
        // changed (QA-A-200, found by the allocation-failure sweep in test_oom_injection.cpp).
        const std::string config_copy(config_json.begin(), config_json.end());

        // FUNC-033 (5): the quality metadata the file carries, so xpe_calib_get_quality_meta()
        // describes the calibration now in use. A file written before QA-A-35 has no such fields
        // and is loaded unchanged -- the call simply reports that it found none.
        XpeCalibQualityMeta quality{};
        bool quality_found = false;
        const XpeErrorCode quality_rc = xpe_calib_parse_quality_meta_json(config_copy.c_str(), &quality, &quality_found);
        if (quality_rc != XPE_OK) return quality_rc;   // a malformed field: nothing has been committed

        // QA-A-123 (#194): the fitted dose range, which bounds where the polynomial means anything.
        // Absence is detected by asking twice with different defaults rather than by matching text --
        // a key that is genuinely present answers the same both times.
        double lo_a = -1.0, hi_a = -1.0;
        bool present = false, usable = false;
        if (is_poly) {
            lo_a = xpe_json_get_double(config_copy.c_str(), "dose_min", -1.0);
            const double lo_b = xpe_json_get_double(config_copy.c_str(), "dose_min", -2.0);
            hi_a = xpe_json_get_double(config_copy.c_str(), "dose_max", -1.0);
            const double hi_b = xpe_json_get_double(config_copy.c_str(), "dose_max", -2.0);
            present = (lo_a == lo_b) && (hi_a == hi_b);
            usable  = present && (hi_a > lo_a);
        }

        StagedGain staged;
        staged.map          = std::move(map);
        staged.isPoly       = is_poly;
        staged.numCoeffs    = static_cast<uint32_t>(num_coeffs);
        staged.width        = hdr.width;
        staged.height       = hdr.height;
        staged.timestamp    = hdr.created_epoch_ms;
        staged.expiryMs     = hdr.expiry_epoch_ms;
        staged.quality      = quality;
        staged.qualityFound = quality_found;
        staged.doseLo       = lo_a;
        staged.doseHi       = hi_a;
        staged.rangePresent = present;
        staged.rangeUsable  = usable;
        std::memcpy(staged.sessionId, hdr.session_id,
                    sizeof(hdr.session_id) < sizeof(staged.sessionId)
                        ? sizeof(hdr.session_id)
                        : sizeof(staged.sessionId) - 1);

        *out = std::move(staged);
        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}

// The two gain models are alternatives: whichever is loaded clears the other, so a scalar map left over from
// an earlier file is never applied to frames the operator calibrated with a polynomial (SRS-CALIB-SAFE-003:
// no partial / mixed calibration).
void xpe_calib_commit_gain_locked(StagedGain& staged) noexcept {
    if (staged.isPoly) {
        g_calib.gain_poly_coeffs     = std::move(staged.map);
        g_calib.gain_poly_num_coeffs = staged.numCoeffs;
        g_calib.gain_map.reset();
    } else {
        g_calib.gain_map = std::move(staged.map);
        g_calib.gain_poly_coeffs.reset();
        g_calib.gain_poly_num_coeffs = 0;
    }
    // The range belongs to whichever polynomial is current; setting it on every load keeps
    // a previous file's bounds from surviving into the next one (QA-A-123, #194).
    g_calib.gain_poly_has_range = (staged.isPoly && staged.rangeUsable);
    g_calib.gain_poly_dose_min  = (staged.isPoly && staged.rangeUsable) ? staged.doseLo : 0.0;
    g_calib.gain_poly_dose_max  = (staged.isPoly && staged.rangeUsable) ? staged.doseHi : 0.0;
    g_calib.gain_width       = staged.width;
    g_calib.gain_height      = staged.height;
    g_calib.gain_timestamp   = staged.timestamp;
    g_calib.gain_expiry_ms   = staged.expiryMs;
    g_calib.gain_quality     = staged.quality;
    g_calib.gain_has_quality = staged.qualityFound;
    std::memcpy(g_calib.gain_session_id, staged.sessionId, sizeof(g_calib.gain_session_id));
}

void xpe_calib_after_gain_commit(const StagedGain& staged) noexcept {
    // Committed from here on; nothing below may fail the load. The metadata copy cannot throw.
    if (staged.qualityFound) xpe_calib_commit_quality_meta(staged.quality);

    const bool poly_loaded = staged.isPoly;
    const bool usable      = staged.rangeUsable;
    const bool present     = staged.rangePresent;
    const double lo_a      = staged.doseLo;
    const double hi_a      = staged.doseHi;

        // The alerts are advisory: raising one allocates, and an allocation failure there must not
        // turn a load that has succeeded and been committed into an error.
        try {
            if (poly_loaded) {
                if (usable) {
                    // QA-A-143 (#194 item 2): THE UNIT IS DOCUMENTED, SO SAY
                    // SOMETHING WHEN THE NUMBERS DISAGREE WITH IT.
                    //
                    // preprocess_api.h pins dose_levels to pixel values (ADU),
                    // but nothing enforces it: the file carries no unit field,
                    // so a ladder fitted in mGy loads, applies, and yields a
                    // wrong image with no error anywhere. Documentation alone
                    // leaves that path open; this closes it as far as it can
                    // be closed without a format change.
                    //
                    // IT IS A MAGNITUDE CHECK, NOT A UNIT CHECK, and the
                    // difference matters: it cannot read a unit, only notice
                    // that the numbers are nowhere near the pixel domain they
                    // are supposed to index. A wrong unit whose values happen
                    // to land in range passes silently, and that limit is why
                    // the header DEBT marker stays.
                    //
                    // WHERE THE THRESHOLD COMES FROM -- measured in this
                    // repository, not chosen for roundness:
                    //
                    //   smallest ADU dose the fixture generator emits   8000
                    //     (xpe_calib_fixture_gen.cpp: 20000 * 0.40)
                    //   smallest ADU dose in the reference dataset     14037
                    //     (tests/test_data/cyan_test/README.md:186)
                    //   pixel domain                              0..65535
                    //   an mGy ladder, for contrast                  1..100
                    //
                    // 1000 sits 8x below the smallest real ADU ladder seen and
                    // 10x above where an mGy ladder tops out, so both sides
                    // have an order of magnitude of slack. A genuinely
                    // low-dose ADU calibration would have to fall below an
                    // eighth of anything measured here to reach it.
                    //
                    // ALERT, NOT REJECTION. Refusing would discard a
                    // calibration on a heuristic; an operator who meant it can
                    // ignore one line, and one who did not has the only clue
                    // this format can give them.
                    constexpr double kMinPlausibleAduDoseMax = 1000.0;
                    if (hi_a < kMinPlausibleAduDoseMax) {
                        char msg[320];
                        std::snprintf(msg, sizeof(msg),
                            "gain polynomial dose levels span [%.3f, %.3f], far "
                            "below the pixel-value range they index (0..65535). "
                            "CHECK THE UNIT OF THIS CALIBRATION'S dose_levels: "
                            "they must be pixel values (ADU), and a ladder fitted "
                            "in mGy loads without error while producing a wrong "
                            "image. If the unit is right and the calibration is "
                            "simply very dark, this warning can be ignored. This "
                            "is a magnitude check, not a unit check -- the file "
                            "carries no unit field (issue #194)",
                            lo_a, hi_a);
                        msg[sizeof(msg) - 1] = '\0';
                        xpe_alert_push(msg, XPE_ALERT_WARNING);
                    }
                } else if (!present) {
                    // Not rejected: refusing would retire every calibration
                    // made before this field existed, which is the harder
                    // thing to undo. Not silent either, or today's behaviour
                    // stays invisible forever.
                    xpe_alert_push(
                        "gain polynomial loaded without a dose range: this file "
                        "was generated before the range field existed, so the "
                        "out-of-range clamp does not apply to it and pixel "
                        "values beyond the fitted levels are extrapolated; "
                        "regenerate the calibration to enable the clamp "
                        "(issue #194)",
                        XPE_ALERT_WARNING);
                } else {
                    // QA-A-124 (#194): the keys ARE there and the interval is
                    // inverted or empty. Saying "generated before the range
                    // field existed" here would be false, and an alert that
                    // misdescribes the file sends the reader after the wrong
                    // thing. Measured: a reversed dose ladder written with the
                    // generator's sort guard disabled recorded
                    // dose_min=42677, dose_max=14037.
                    char msg[320];
                    std::snprintf(msg, sizeof(msg),
                        "gain polynomial carries an inverted dose range "
                        "[%.1f, %.1f]: the clamp is not applied and pixel "
                        "values are extrapolated. The file's dose levels were "
                        "not ascending when it was generated, so its "
                        "coefficients may be fitted against mispaired gain "
                        "maps -- regenerate it (issue #194)",
                        lo_a, hi_a);
                    msg[sizeof(msg) - 1] = '\0';
                    xpe_alert_push(msg, XPE_ALERT_WARNING);
                }
            }
        } catch (...) {
            // The warning is lost under memory pressure; the calibration itself is loaded.
        }

}

extern "C" XPE_API XpeErrorCode xpe_calib_load_gain(const char* filepath) {
    try {
        StagedGain staged;
        const XpeErrorCode rc = xpe_calib_stage_gain(filepath, &staged);
        if (rc != XPE_OK) {
            return rc;
        }

        {
            std::lock_guard<std::mutex> lock(g_calib_mutex);
            xpe_calib_commit_gain_locked(staged);
        }
        xpe_calib_after_gain_commit(staged);
        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
