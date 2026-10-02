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

XpeGainScan xpe_gain_scan_scalar(const float* values, uint32_t width, uint32_t height, std::vector<uint32_t>* idx) {
    XpeGainScan scan;
    const uint64_t n = static_cast<uint64_t>(width) * height;
    scan.total = n;
    for (uint64_t i = 0; i < n; ++i) {
        // NaN fails both comparisons, so it is classified too.
        if (values[i] >= XPE_GAIN_APPLIED_MIN && values[i] <= XPE_GAIN_APPLIED_MAX) continue;
        if (scan.count == 0) scan.first = i;
        ++scan.count;
        const uint64_t y = i / width, x = i % width;
        if (y < 64 || y + 64 >= height || x < 64 || x + 64 >= width) ++scan.inBand;
        if (idx) idx->push_back(static_cast<uint32_t>(i));
    }
    return scan;
}

bool xpe_gain_scan_over_limit(const XpeGainScan& scan) noexcept {
    return static_cast<double>(scan.count) > XPE_GAIN_DEFECT_MAX_FRACTION * static_cast<double>(scan.total);
}

void xpe_gain_alert_classified(const XpeGainScan& scan) noexcept {
    try {
        char msg[400];
        std::snprintf(msg, sizeof(msg),
            "XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT: %llu of %llu pixel(s) (%.3f%%) have a gain outside [%.1f, %.1f] and are "
            "marked defective: gain 1.0 is used and they are listed for the defect correction stage "
            "(%llu in the outermost 64-pixel band; first: %llu). Limit: %.1f%%",
            static_cast<unsigned long long>(scan.count), static_cast<unsigned long long>(scan.total),
            scan.total ? 100.0 * static_cast<double>(scan.count) / static_cast<double>(scan.total) : 0.0,
            static_cast<double>(XPE_GAIN_APPLIED_MIN), static_cast<double>(XPE_GAIN_APPLIED_MAX),
            static_cast<unsigned long long>(scan.inBand), static_cast<unsigned long long>(scan.first),
            100.0 * XPE_GAIN_DEFECT_MAX_FRACTION);
        msg[sizeof(msg) - 1] = '\0';
        xpe_alert_push(msg, XPE_ALERT_WARNING);
    } catch (...) {
        // advisory: lost under memory pressure
    }
}

void xpe_gain_alert_over_limit(const XpeGainScan& scan, const char* verb) noexcept {
    try {
        char msg[400];
        std::snprintf(msg, sizeof(msg),
            "XPE_WARN_GAIN_PIXELS_OVER_LIMIT: %llu of %llu pixel(s) (%.3f%%) have a gain outside [%.1f, %.1f], above the %.1f%% "
            "limit; the calibration was not %s",
            static_cast<unsigned long long>(scan.count), static_cast<unsigned long long>(scan.total),
            scan.total ? 100.0 * static_cast<double>(scan.count) / static_cast<double>(scan.total) : 0.0,
            static_cast<double>(XPE_GAIN_APPLIED_MIN), static_cast<double>(XPE_GAIN_APPLIED_MAX),
            100.0 * XPE_GAIN_DEFECT_MAX_FRACTION, verb);
        msg[sizeof(msg) - 1] = '\0';
        xpe_alert_push(msg, XPE_ALERT_ERROR);
    } catch (...) {
    }
}

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
        XpeConfigDoc config;   // the reader parses the config block once and hands the document over (QA-A-209c)
        XpeErrorCode rc = read_xcal_file(
            filepath, hdr, config_json, payload,
            /*check_expiry=*/true,
            /*expected_type=*/-1,
            &config);
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

        // SRS-CALIB-FUNC-002 (QA-A-211, #233): "Values shall be in range [0.1, 10.0]". A pixel outside it -- a failed
        // pixel, or the low-sensitivity edge band some detectors have (CalData_6: 99.9% of 39-44 thousand such pixels lie
        // in the outer 64-pixel band, two thirds of them outside the defect map) -- no longer refuses the whole map: it is
        // CLASSIFIED DEFECTIVE, its gain is replaced by 1.0 and its index is kept for the defect stage. The count is
        // reported, and a map with more than XPE_GAIN_DEFECT_MAX_FRACTION of its pixels classified is refused as before
        // (XPE_ERR_INVALID_CALIB_DATA). Scalar maps only: a coefficient of G(x,y,E) is not a gain value.
        // Everything that allocates is done here, before the commit.
        // Overwritten by the memcpy below; no value-initialisation (QA-A-105).
        std::shared_ptr<float[]> map(new float[n_floats]);
        std::memcpy(map.get(), payload.data(), payload.size());
        std::shared_ptr<uint32_t[]> defect_idx;
        XpeGainScan scan;
        if (!is_poly) {
            std::vector<uint32_t> idx;
            scan = xpe_gain_scan_scalar(map.get(), hdr.width, hdr.height, &idx);
            if (xpe_gain_scan_over_limit(scan)) {
                xpe_gain_alert_over_limit(scan, "loaded");
                return XPE_ERR_INVALID_CALIB_DATA;
            }
            if (scan.count > 0) {
                defect_idx.reset(new uint32_t[idx.size()]);
                std::memcpy(defect_idx.get(), idx.data(), idx.size() * sizeof(uint32_t));
                for (const uint32_t k : idx) map[k] = 1.0f;
            }
        }

        // Commit under mutex. The two gain models are alternatives: whichever
        // is loaded clears the other, so a scalar map left over from an earlier
        // file is never applied to frames the operator calibrated with a
        // polynomial (SRS-CALIB-SAFE-003: no partial / mixed calibration).

        // Everything that allocates, and so can throw, is done HERE, before the commit below. The
        // commit only moves pointers and copies plain values, and nothing after it may throw: a
        // throw once the first field was written reported OUT_OF_MEMORY for a store that had already
        // changed (QA-A-200, found by the allocation-failure sweep in test_oom_injection.cpp).
        // FUNC-033 (5): the quality metadata the file carries, so xpe_calib_get_quality_meta()
        // describes the calibration now in use. A file written before QA-A-35 has no such fields
        // and is loaded unchanged -- the call simply reports that it found none.
        XpeCalibQualityMeta quality{};
        bool quality_found = false;
        const XpeErrorCode quality_rc = xpe_calib_parse_quality_meta(config, &quality, &quality_found);
        if (quality_rc != XPE_OK) return quality_rc;   // a malformed field: nothing has been committed

        // QA-A-123 (#194): the fitted dose range, which bounds where the polynomial means anything. Present only when
        // both are top-level bare JSON numbers.
        double lo_a = -1.0, hi_a = -1.0;
        bool present = false, usable = false;
        if (is_poly) {
            const bool has_lo = config.getNumber("dose_min", &lo_a);
            const bool has_hi = config.getNumber("dose_max", &hi_a);
            present = has_lo && has_hi;
            usable  = present && (hi_a > lo_a);
        }

        StagedGain staged;
        staged.map          = std::move(map);
        staged.defectIdx    = std::move(defect_idx);
        staged.defectCount  = static_cast<uint32_t>(scan.count);
        staged.defectInBand = static_cast<uint32_t>(scan.inBand);
        staged.defectFirst  = static_cast<uint32_t>(scan.first);
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
        g_calib.gain_defect_idx.reset();
        g_calib.gain_defect_count = 0;
    } else {
        g_calib.gain_map = std::move(staged.map);
        g_calib.gain_defect_idx   = std::move(staged.defectIdx);
        g_calib.gain_defect_count = staged.defectCount;
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
    // The quality record the module serves is replaced in the same critical section as the maps (QA-A-202d, Codex
    // #32 A1) -- ALWAYS: a file with no quality metadata makes the record "none" instead of leaving the previous
    // file's values in place as if they were this gain's (QA-A-202e, Codex #38 A1).
    if (staged.qualityFound) {
        xpe_calib_commit_quality_meta_locked(staged.quality);
    } else {
        xpe_calib_commit_no_quality_locked();
    }
}

void xpe_calib_after_gain_commit(const StagedGain& staged) noexcept {
    // Committed from here on; nothing below may fail the load. (The quality metadata was committed with the
    // maps, under the lock -- see xpe_calib_commit_gain_locked.)
    const bool poly_loaded = staged.isPoly;
    const bool usable      = staged.rangeUsable;
    const bool present     = staged.rangePresent;
    const double lo_a      = staged.doseLo;
    const double hi_a      = staged.doseHi;
    if (!poly_loaded && staged.defectCount > 0) {
        XpeGainScan scan;
        scan.count = staged.defectCount; scan.inBand = staged.defectInBand; scan.first = staged.defectFirst;
        scan.total = static_cast<uint64_t>(staged.width) * staged.height;
        xpe_gain_alert_classified(scan);
    }

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
