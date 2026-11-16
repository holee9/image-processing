/**
 * @file xpe_calib_load_gain.cpp
 * @brief xpe_calib_load_gain implementation (T-006)
 *
 * REQ-P1A-015: Load XCal v1 GAIN calibration map.
 * QA-A-336: a gain polynomial file (type 3, the removed XCAL_TYPE_GAIN_POLY) is refused by the header check
 *           (XPE_ERR_CONFIG_INVALID, unknown type) -- never read as a scalar map.
 * REQ-P1A-030: No C++ exceptions across C ABI boundary.
 * REQ-P1A-031: RAII for automatic cleanup on error.
 *
 * @MX:ANCHOR: [AUTO] Public API entry point for gain calibration load
 * @MX:REASON: Called by xpe_gain_correct (fan_in >= 3); g_calib write path
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include "xcal_reader.hpp"
#include "saturation.hpp"

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
        // [no-throw-boundary] advisory: lost under memory pressure
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
        // [no-throw-boundary] the alert is advisory; losing it (allocation failure) must not change the result of the call that raised it
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

        // Only XCAL_TYPE_GAIN (one scalar plane). A file of any other type -- including the type-3 gain polynomial this
        // module no longer has (QA-A-336) -- is refused with XPE_ERR_CONFIG_INVALID: the header check rejects a type above
        // the highest one it knows, and the check below refuses the other known types.
        XpeConfigDoc config;   // the reader parses the config block once and hands the document over (QA-A-209c)
        // QA-A-235b: an uncompressed scalar gain payload is read straight into the map this function
        // keeps (no zero-filled vector, no second copy). The map is local until the commit below, so a refused file
        // leaves the module-global store as it was. Anything the sink does not take (another type, a ragged length,
        // a compressed file) is declined and goes the old way.
        std::shared_ptr<float[]> map;
        XCalPayloadSink sink;
        sink.ctx = &map;
        sink.acquire = [](void* ctx, const XCalFileHeader& h, uint64_t len) -> uint8_t* {
            if (h.type != static_cast<uint32_t>(XCAL_TYPE_GAIN)) return nullptr;
            const uint64_t plane = static_cast<uint64_t>(h.width) * h.height * sizeof(float);
            if (plane == 0 || len != plane) return nullptr;
            auto* m = static_cast<std::shared_ptr<float[]>*>(ctx);
            m->reset(new float[static_cast<size_t>(len / sizeof(float))]);
            return reinterpret_cast<uint8_t*>(m->get());
        };
        XpeErrorCode rc = read_xcal_file(
            filepath, hdr, config_json, payload,
            /*check_expiry=*/true,
            /*expected_type=*/-1,
            &config, &sink);
        if (rc != XPE_OK) {
            return rc;
        }

        if (hdr.type != static_cast<uint32_t>(XCAL_TYPE_GAIN)) {
            return XPE_ERR_CONFIG_INVALID;
        }

        // Verify payload size matches declared dimensions.
        // (the payload is in `map` when the sink took it, otherwise in `payload`)
        const size_t payload_size = sink.filled ? static_cast<size_t>(hdr.payload_len) : payload.size();
        const size_t plane = static_cast<size_t>(hdr.width) * hdr.height * sizeof(float);
        if (plane == 0) {
            return XPE_ERR_CONFIG_INVALID;
        }
        if (payload_size != plane) {
            return XPE_ERR_CONFIG_INVALID;
        }

        const size_t n_floats   = payload_size / sizeof(float);

        // SRS-CALIB-FUNC-002 (QA-A-211, #233): "Values shall be in range [0.1, 10.0]". A pixel outside it -- a failed
        // pixel, or the low-sensitivity edge band some detectors have (CalData_6: 99.9% of 39-44 thousand such pixels lie
        // in the outer 64-pixel band, two thirds of them outside the defect map) -- no longer refuses the whole map: it is
        // CLASSIFIED DEFECTIVE, its gain is replaced by 1.0 and its index is kept for the defect stage. The count is
        // reported, and a map with more than XPE_GAIN_DEFECT_MAX_FRACTION of its pixels classified is refused as before
        // (XPE_ERR_INVALID_CALIB_DATA).
        // Everything that allocates is done here, before the commit.
        // Overwritten by the memcpy below; no value-initialisation (QA-A-105).
        if (!sink.filled) {
            map.reset(new float[n_floats]);
            std::memcpy(map.get(), payload.data(), payload.size());
        }
        std::shared_ptr<uint32_t[]> defect_idx;
        XpeGainScan scan;
        {
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

        // Commit under mutex.

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

        // QA-A-296: the saturation record in the config block (absent in a file written before it existed) and the
        // median of the scalar plane. A present but malformed record refuses the file: nothing has been committed.
        XpeGainSat sat;
        {
            XpeSatRecord rec;
            if (!xpe_sat_record_from_config(config, &rec)) return XPE_ERR_CONFIG_INVALID;
            sat.known = rec.known;
            sat.determined = rec.determined;
            sat.level = rec.level;
        }

        StagedGain staged;
        staged.sat          = sat;
        staged.map          = std::move(map);
        staged.defectIdx    = std::move(defect_idx);
        staged.defectCount  = static_cast<uint32_t>(scan.count);
        staged.defectInBand = static_cast<uint32_t>(scan.inBand);
        staged.defectFirst  = static_cast<uint32_t>(scan.first);
        staged.width        = hdr.width;
        staged.height       = hdr.height;
        staged.timestamp    = hdr.created_epoch_ms;
        staged.expiryMs     = hdr.expiry_epoch_ms;
        staged.quality      = quality;
        staged.qualityFound = quality_found;
        if (!config_json.empty())
            staged.config = std::make_shared<const std::vector<uint8_t>>(std::move(config_json));
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

void xpe_calib_commit_gain_locked(StagedGain& staged) noexcept {
    g_calib.gain_map = std::move(staged.map);
    g_calib.gain_defect_idx   = std::move(staged.defectIdx);
    g_calib.gain_defect_count = staged.defectCount;
    g_calib.gain_sat = staged.sat;
    g_calib.gain_width       = staged.width;
    g_calib.gain_height      = staged.height;
    g_calib.gain_timestamp   = staged.timestamp;
    g_calib.gain_expiry_ms   = staged.expiryMs;
    xpe_calib_note_expiry_locked(CalibMapKind::Gain, staged.expiryMs);
    g_calib.gain_quality     = staged.quality;
    g_calib.gain_has_quality = staged.qualityFound;
    g_calib.gain_config      = std::move(staged.config);
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
    if (staged.defectCount > 0) {
        XpeGainScan scan;
        scan.count = staged.defectCount; scan.inBand = staged.defectInBand; scan.first = staged.defectFirst;
        scan.total = static_cast<uint64_t>(staged.width) * staged.height;
        xpe_gain_alert_classified(scan);
    }
}

extern "C" XPE_API XpeErrorCode xpe_calib_load_gain(const char* filepath) {
    try {
        StagedGain staged;
        const XpeErrorCode rc = xpe_calib_stage_gain(filepath, &staged);
        if (rc != XPE_OK) {
            return rc;
        }

        bool warnSession = false;
        {
            std::lock_guard<std::mutex> lock(g_calib_mutex);
            // QA-A-229 M4: refused BEFORE anything changes -- the loaded maps stay, this one is rejected.
            const XpeErrorCode src = xpe_calib_session_check_locked(CalibMapKind::Gain, staged.sessionId, &warnSession);
            if (src != XPE_OK) return src;
            xpe_calib_commit_gain_locked(staged);
        }
        xpe_calib_session_warn(warnSession);
        xpe_calib_after_gain_commit(staged);
        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
