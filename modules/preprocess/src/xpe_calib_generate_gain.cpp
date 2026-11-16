/**
 * @file xpe_calib_generate_gain.cpp
 * @brief Gain map generation implementation (SWU-1.12: FUNC-026)
 *
 * SRS-CALIB-FUNC-026: Generate flat-field gain map from flat frames
 * (QA-A-336: the dose-dependent gain polynomial generator, FUNC-027, was removed.)
 *
 * Algorithm (FUNC-026):
 *   1. Validate input dimensions consistency
 *   2. Dark subtraction: flat_corr[i] = flat_frames[i] - dark_reference
 *   3. Pixel-wise mean: G_raw(x,y) = mean(flat_corr[0..N-1][x,y])
 *   4. Normalize: G(x,y) = G_raw(x,y) / mean(G_raw)
 *   5. Single-frame mode: compute uncertainty σ², store in metadata
 *   6. Write via xcal_writer (XCAL_TYPE_GAIN format)
 *
 * @MX:ANCHOR: [AUTO] xpe_calib_generate_gain – flat-field gain calibration
 * @MX:REASON: Critical offline calibration path; dark subtraction + normalization mandatory
 * @MX:SPEC: SWU-1.12 FUNC-026
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include "xcal_writer.hpp"
#include "xcal_reader.hpp"
#include "saturation.hpp"

#include <cstring>
#include <cmath>
#include <memory>
#include <chrono>
#include <vector>
#include <cstdint>
#include <algorithm>

// =============================================================================
// FUNC-026: xpe_calib_generate_gain
// =============================================================================

extern "C" XPE_API XpeErrorCode xpe_calib_generate_gain(
    const XpeImageBuffer* flat_frames,
    int32_t               num_frames,
    const XpeImageBuffer* dark_reference,
    const char*           output_path,
    const char*           metadata_json)
{
    try {
        // --- Input validation ---
        if (flat_frames == nullptr || output_path == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }
        if (num_frames <= 0) {
            return XPE_ERR_INVALID_INPUT;
        }

        // FUNC-031 (8): this entry point acquires at one dose level with no fitted curve (degree 0): SINGLE_POINT, the
        // only mode (QA-A-336; it recorded the then default MULTI_POINT_8 = 3 before, which was false about the file).
        XpeCalibrationMode mode = XPE_CALIB_SINGLE_POINT;
        {
            const XpeErrorCode mrc = xpe_calib_resolve_mode(1, 0, &mode);
            if (mrc != XPE_OK) return mrc;
        }

        // Dimensions from first frame
        uint32_t width  = flat_frames[0].width;
        uint32_t height = flat_frames[0].height;

        if (width == 0 || height == 0 ||
            width > XCAL_MAX_DIM || height > XCAL_MAX_DIM) {
            return XPE_ERR_INVALID_INPUT;
        }

        // Validate dark reference dimensions (if provided)
        if (dark_reference != nullptr) {
            if (dark_reference->width != width ||
                dark_reference->height != height) {
                return XPE_ERR_INVALID_INPUT;
            }
            if (dark_reference->format != XPE_PIXEL_UINT16) {
                return XPE_ERR_UNSUPPORTED_FORMAT;
            }
        }

        // --- Allocate double accumulator for dark-subtracted values ---
        size_t n_pixels = static_cast<size_t>(width) * height;
        auto accum = std::make_unique<double[]>(n_pixels);
        std::memset(accum.get(), 0, n_pixels * sizeof(double));

        const uint16_t* dark_ptr = dark_reference != nullptr
            ? static_cast<const uint16_t*>(dark_reference->data)
            : nullptr;

        // --- Accumulate dark-subtracted flat frames ---
        for (int32_t i = 0; i < num_frames; ++i) {
            const XpeImageBuffer& frame = flat_frames[i];

            // Validate consistency
            if (frame.width != width || frame.height != height) {
                return XPE_ERR_INVALID_INPUT;
            }
            if (frame.data == nullptr) {
                return XPE_ERR_INVALID_INPUT;
            }
            if (frame.format != XPE_PIXEL_UINT16) {
                return XPE_ERR_UNSUPPORTED_FORMAT;
            }

            const uint16_t* src = static_cast<const uint16_t*>(frame.data);
            for (size_t j = 0; j < n_pixels; ++j) {
                double flat_val = static_cast<double>(src[j]);
                double dark_val = dark_ptr != nullptr
                    ? static_cast<double>(dark_ptr[j])
                    : 0.0;
                double corrected = flat_val - dark_val;
                // Floor at zero (no negative signal)
                accum[j] += (corrected > 0.0) ? corrected : 0.0;
            }
        }

        // --- Compute mean gain map ---
        auto gain_raw = std::make_unique<float[]>(n_pixels);
        double inv_n = 1.0 / static_cast<double>(num_frames);
        double sum_all = 0.0;

        for (size_t j = 0; j < n_pixels; ++j) {
            float v = static_cast<float>(accum[j] * inv_n);
            gain_raw[j] = std::isfinite(v) ? v : 0.0f;
            sum_all += gain_raw[j];
        }

        // --- Normalize to unit mean ---
        double mean_gain = sum_all / static_cast<double>(n_pixels);
        if (mean_gain <= 0.0 || !std::isfinite(mean_gain)) {
            return XPE_ERR_PROCESSING_FAILED; // Invalid mean
        }

        auto gain_normalized = std::make_unique<float[]>(n_pixels);
        for (size_t j = 0; j < n_pixels; ++j) {
            gain_normalized[j] = gain_raw[j] / static_cast<float>(mean_gain);
            // Guard against division artifacts
            if (!std::isfinite(gain_normalized[j])) {
                gain_normalized[j] = 1.0f; // Fallback to neutral gain
            }
        }

        // --- FUNC-033 (1): quality metadata for a single-dose calibration ---
        //
        // SRS-CALIB-001 SRS-CALIB-FUNC-033 (1): "Every generated XCal gain file
        // shall include: calibration_mode, actual_dose_levels, polynomial_degree
        // (fitted polynomial degree; 0 for single-point), fit_r_squared
        // (coefficient of determination; 1.0 for single-point by definition),
        // max_residual_pct, mean_residual_pct, ...".
        //
        // This entry point acquires at ONE dose level (num_frames frames of the
        // same exposure are averaged), so actual_dose_levels is 1 and the degree
        // is 0. No curve is fitted, so there is no residual to report: the SRS
        // states no single-point value for the two residual fields, and 0 is the
        // arithmetic consequence of an exact one-point fit rather than an
        // invented threshold.
        //
        // acquisition_duration_s and detector_temperature_c are also named by
        // FUNC-033 (1) and are NOT written here -- neither value reaches this
        // function through its current signature. Recorded as residual on #140.
        XpeCalibQualityMeta quality{};
        quality.calibration_mode   = static_cast<uint8_t>(mode);
        quality.polynomial_degree  = 0;
        quality.num_points         = 1;
        quality.r_squared          = 1.0;
        const bool gate_passed = xpe_calib_record_quality_meta(quality);

        char meta[512];
        std::snprintf(meta, sizeof(meta),
            "{\"calibration_mode\":%d,\"requested_calibration_mode\":%d,"
            "\"actual_dose_levels\":1,"
            "\"polynomial_degree\":0,\"fit_r_squared\":1.000000000,"
            "\"max_residual_pct\":0.000000,\"mean_residual_pct\":0.000000,"
            "\"calibration_pass\":%d",
            static_cast<int>(mode),
            static_cast<int>(xpe_calib_get_mode()),
            gate_passed ? 1 : 0);
        meta[sizeof(meta) - 1] = '\0';

        std::string config_json = meta;

        // QA-A-296 (Refs #278): the detector's saturation level, found from the pile-up at the top of the histogram of
        // the flat frames (the raw frames, known defect pixels left out). A frame set with no frame exposed to saturation
        // has no pile-up: the file then says "undetermined" -- a value is never guessed. The thresholds are the defaults of
        // XpeSatParams, tuned on 2G data only; the caller's metadata (top-level numbers saturation_min_count_frac,
        // saturation_min_ratio, saturation_max_above_frac) overrides them, and "saturation_detection":"off" skips the record.
        {
            XpeSatParams sp;
            bool detect = true;
            if (metadata_json != nullptr && metadata_json[0] != '\0') {
                XpeConfigDoc md;
                // QA-A-296 (D4): metadata that is not one valid JSON object without a repeated member name is a refusal, and nothing is written
                if (xpe_config_parse(metadata_json, &md) != XPE_OK) return XPE_ERR_CONFIG_INVALID;
                {
                    // a key that is absent keeps the documented default; a key that is present must be a number in range (QA-A-296: no silent fallback)
                    double v = 0.0;
                    std::string t;
                    bool q = false;
                    if (md.find("saturation_min_count_frac")) { if (!md.getNumber("saturation_min_count_frac", &v) || !(v > 0.0 && v <= 1.0)) return XPE_ERR_CONFIG_INVALID; sp.minCountFrac = v; }
                    if (md.find("saturation_min_ratio")) { if (!md.getNumber("saturation_min_ratio", &v) || !(v >= 1.0)) return XPE_ERR_CONFIG_INVALID; sp.minRatio = v; }
                    if (md.find("saturation_max_above_frac")) { if (!md.getNumber("saturation_max_above_frac", &v) || !(v >= 0.0 && v <= 1.0)) return XPE_ERR_CONFIG_INVALID; sp.maxAboveFrac = v; }
                    if (md.find("saturation_detection")) { if (!md.getString("saturation_detection", &t, &q) || !q || t != "off") return XPE_ERR_CONFIG_INVALID; detect = false; }
                }
            }
            if (detect) {
                const CalibSnapshot snap = xpe_calib_snapshot();   // the defect map as one consistent set, not read under a load
                const uint8_t* excl = (snap.defect_map && snap.defect_width == width && snap.defect_height == height)
                                          ? snap.defect_map.get() : nullptr;
                std::vector<uint32_t> hist(XPE_SAT_BINS);
                uint32_t used = 0;
                bool conflict = false;
                uint32_t level = 0;
                for (int32_t i = 0; i < num_frames; ++i) {
                    std::fill(hist.begin(), hist.end(), 0u);
                    uint64_t counted = 0;
                    xpe_sat_histogram(static_cast<const uint16_t*>(flat_frames[i].data), width, height, 1u, excl,
                                      hist.data(), &counted);
                    uint32_t lv = 0;
                    uint64_t cnt = 0;
                    if (!xpe_sat_find_pile(hist.data(), counted, sp, &lv, &cnt)) continue;
                    if (used > 0 && lv != level) conflict = true;
                    level = lv;
                    ++used;
                }
                const bool determined = used > 0 && !conflict;
                xpe_sat_record_append_json(&config_json, determined, determined ? level : 0u, sp, used,
                                           static_cast<uint32_t>(num_frames));
                if (!determined) {
                    xpe_alert_push(conflict
                        ? "XPE_WARN_SATURATION_UNDETERMINED: the flat frames pile up at different raw values; the gain calibration records the saturation level as undetermined"
                        : "XPE_WARN_SATURATION_UNDETERMINED: no flat frame shows a pile-up at the top of its histogram (no frame was exposed to saturation); the gain calibration records the saturation level as undetermined",
                        XPE_ALERT_INFO);
                }
            }
        }

        if (metadata_json != nullptr && metadata_json[0] != '\0') {
            // The caller's metadata used to BE the whole config JSON. It is
            // nested rather than merged so it survives verbatim whatever shape
            // it has, and so a caller key can never shadow a FUNC-033 field.
            config_json += ",\"source_metadata\":";
            config_json += metadata_json;
        }
        config_json += "}";

        // --- Build XCal v1 header ---
        using namespace std::chrono;
        int64_t now_ms = duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()).count();

        XCalFileHeader hdr;
        std::memset(&hdr, 0, sizeof(hdr));
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version          = XCAL_VERSION;
        hdr.type             = static_cast<uint32_t>(XCAL_TYPE_GAIN);
        hdr.pixel_format     = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width            = width;
        hdr.height           = height;
        hdr.created_epoch_ms = now_ms;
        hdr.expiry_epoch_ms  = 0; // never expires
        hdr.payload_len      = static_cast<uint64_t>(n_pixels) * sizeof(float);

        // session_id: "generated" (null-padded)
        std::memcpy(hdr.session_id, "generated\0", 10);

        // --- Write via xcal_writer ---
        const uint8_t* config_ptr = config_json.empty()
            ? nullptr
            : reinterpret_cast<const uint8_t*>(config_json.data());
        uint64_t config_len = static_cast<uint64_t>(config_json.size());
        hdr.config_json_len = config_len;

        return write_xcal_file(
            output_path, hdr,
            config_ptr, config_len,
            reinterpret_cast<const uint8_t*>(gain_normalized.get()),
            hdr.payload_len);

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
