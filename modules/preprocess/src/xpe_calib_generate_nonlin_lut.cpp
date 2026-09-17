/**
 * @file xpe_calib_generate_nonlin_lut.cpp
 * @brief Nonlinearity LUT generation (SRS-CALIB-FUNC-006-EXT 6a), QA-A-110 (#186)
 *
 * The requirement, quoted (SRS-CALIB-001, SRS-CALIB-FUNC-006-EXT, section 6a):
 *
 *   - "LUT size: 4096 entries (covers 12-bit ADC range) or 65536 entries
 *      (16-bit full range)"
 *   - "LUT data type: uint16 (output values in ADU)"
 *   - "Lookup: I_lin = LUT[I_raw] (direct index, O(1))"
 *   - generation, verbatim:
 *       1. "Acquire flat-field images at N >= 10 dose levels spanning 5% to 95%
 *           ADC full scale"
 *       2. "For each dose level, record mean signal S_meas and reference dose D_ref"
 *       3. "Fit ideal linear response: S_ideal(D) = G_nominal x D where G_nominal
 *           is mean gain"
 *       4. "Compute correction: LUT[S_meas] = S_ideal"
 *       5. "Interpolate LUT entries between measured points using monotone cubic
 *           spline (Fritsch-Carlson 1980)"
 *       6. "Boundary conditions: LUT[0] = 0, LUT[ADC_max] = ADC_max (identity at
 *           extremes)"
 *   - "Monotonicity check: LUT[i] <= LUT[i+1] for all i (enforced; non-monotone
 *      LUT = XPE_ERR_INVALID_CALIB_DATA)"
 *
 * WHAT THIS FUNCTION TAKES, AND WHY IT IS NOT GAIN MAPS.
 * Step 2 needs the ABSOLUTE mean signal in ADU. The gain path cannot supply it:
 * xpe_calib_generate_gain() divides every pixel by the frame's mean gain
 * (xpe_calib_generate_gain.cpp, "--- Normalize to unit mean ---"), so a gain map
 * carries only the pixel-to-pixel ratio and has had the signal's scale removed.
 * This function therefore takes the flat frames themselves and computes each
 * level's mean here, in ADU, before anything is normalized. An optional dark
 * reference is subtracted first, because the pipeline order is Offset ->
 * Nonlinearity: the LUT must describe the response of an offset-corrected frame,
 * which is what it will be applied to.
 *
 * G_nominal: "mean gain" over the measured points, fitted through the origin by
 * least squares -- G = sum(D_i * S_i) / sum(D_i^2). Through the origin because
 * step 6 pins LUT[0] = 0; a fit with a free intercept would contradict it.
 *
 * NOT IN THIS FILE (QA-A-110 is stage 1): loading the LUT back into the
 * calibration state and applying it in the pipeline. That is the next card.
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xcal_format.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xcal_writer.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

/** SRS-CALIB-FUNC-006-EXT 6a: the only two entry counts the format allows. */
bool IsAllowedEntryCount(uint32_t entries) {
    return entries == 4096u || entries == 65536u;
}

/**
 * Geometry for a table of `entries` uint16 values.
 *
 * The validator's payload check is width * height * bytes_per_pixel and
 * XCAL_MAX_DIM is 4096, so 65536 entries cannot be stored as (65536, 1).
 */
void LutGeometry(uint32_t entries, uint32_t* width, uint32_t* height) {
    *width = std::min<uint32_t>(entries, XCAL_MAX_DIM);
    *height = entries / *width;
}

/** Mean of one flat frame in ADU, with the dark reference removed if given. */
bool MeanSignalAdu(const XpeImageBuffer& frame, const XpeImageBuffer* dark,
                   double* out) {
    if (frame.data == nullptr) return false;
    if (frame.format != XPE_PIXEL_UINT16) return false;
    if (frame.width == 0 || frame.height == 0) return false;
    if (dark != nullptr) {
        if (dark->data == nullptr) return false;
        if (dark->width != frame.width || dark->height != frame.height) return false;
        if (dark->format != XPE_PIXEL_UINT16 && dark->format != XPE_PIXEL_FLOAT32) {
            return false;
        }
    }

    const size_t n = static_cast<size_t>(frame.width) * frame.height;
    if (frame.dataSize < n * sizeof(uint16_t)) return false;

    const uint16_t* px = static_cast<const uint16_t*>(frame.data);
    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
        double v = static_cast<double>(px[i]);
        if (dark != nullptr) {
            if (dark->format == XPE_PIXEL_UINT16) {
                v -= static_cast<double>(static_cast<const uint16_t*>(dark->data)[i]);
            } else {
                v -= static_cast<double>(static_cast<const float*>(dark->data)[i]);
            }
        }
        sum += v;
    }
    *out = sum / static_cast<double>(n);
    return std::isfinite(*out);
}

/**
 * Fritsch-Carlson (1980) monotone cubic interpolation.
 *
 * Plain Hermite interpolation through monotone data can still overshoot between
 * knots; Fritsch-Carlson limits the tangents so it cannot. The limiter is the
 * reason this is not "cubic spline" -- a natural cubic spline through the same
 * knots overshoots and would produce a LUT that is locally decreasing, which the
 * requirement rejects outright.
 *
 * @param xs  Knot abscissae, strictly increasing.
 * @param ys  Knot ordinates, non-decreasing.
 * @param x   Where to evaluate; clamped to the knot range by the caller.
 */
double MonotoneCubic(const std::vector<double>& xs, const std::vector<double>& ys,
                     const std::vector<double>& tangents, double x) {
    // Locate the interval [xs[i], xs[i+1]] containing x.
    size_t i = 0;
    {
        size_t lo = 0, hi = xs.size() - 1;
        while (lo + 1 < hi) {
            const size_t mid = (lo + hi) / 2;
            if (xs[mid] <= x) lo = mid; else hi = mid;
        }
        i = lo;
    }
    const double h = xs[i + 1] - xs[i];
    const double t = (x - xs[i]) / h;
    const double t2 = t * t;
    const double t3 = t2 * t;

    // Hermite basis.
    const double h00 = 2.0 * t3 - 3.0 * t2 + 1.0;
    const double h10 = t3 - 2.0 * t2 + t;
    const double h01 = -2.0 * t3 + 3.0 * t2;
    const double h11 = t3 - t2;

    return h00 * ys[i] + h10 * h * tangents[i] +
           h01 * ys[i + 1] + h11 * h * tangents[i + 1];
}

/** Tangents limited per Fritsch-Carlson so the interpolant stays monotone. */
std::vector<double> MonotoneTangents(const std::vector<double>& xs,
                                     const std::vector<double>& ys) {
    const size_t n = xs.size();
    std::vector<double> delta(n - 1);
    for (size_t i = 0; i + 1 < n; ++i) {
        delta[i] = (ys[i + 1] - ys[i]) / (xs[i + 1] - xs[i]);
    }

    std::vector<double> m(n);
    m[0] = delta[0];
    m[n - 1] = delta[n - 2];
    for (size_t i = 1; i + 1 < n; ++i) {
        // A local extremum in the data forces a zero tangent, otherwise the
        // interpolant would have to leave the data's range to reach it.
        m[i] = (delta[i - 1] * delta[i] <= 0.0)
                   ? 0.0
                   : (delta[i - 1] + delta[i]) * 0.5;
    }

    // The limiter: |m| <= 3 * |delta| on both sides of each knot. Fritsch and
    // Carlson prove this is sufficient for monotonicity on every interval.
    for (size_t i = 0; i + 1 < n; ++i) {
        if (delta[i] == 0.0) {
            m[i] = 0.0;
            m[i + 1] = 0.0;
            continue;
        }
        const double a = m[i] / delta[i];
        const double b = m[i + 1] / delta[i];
        const double s = a * a + b * b;
        if (s > 9.0) {
            const double tau = 3.0 / std::sqrt(s);
            m[i] = tau * a * delta[i];
            m[i + 1] = tau * b * delta[i];
        }
    }
    return m;
}

}  // namespace

XPE_API XpeErrorCode xpe_calib_generate_nonlin_lut(const XpeImageBuffer* flat_frames,
                                                   const double* dose_levels,
                                                   int32_t num_levels,
                                                   const XpeImageBuffer* dark_reference,
                                                   uint32_t lut_entries,
                                                   const char* output_path,
                                                   const char* metadata_json) {
    if (flat_frames == nullptr || dose_levels == nullptr || output_path == nullptr) {
        return XPE_ERR_INVALID_INPUT;
    }
    // SRS-CALIB-FUNC-006-EXT 6a step 1: "N >= 10 dose levels". Fewer levels is
    // rejected rather than fitted -- the requirement states the count as a
    // precondition of the procedure, not as a recommendation.
    if (num_levels < 10) return XPE_ERR_INVALID_INPUT;
    if (!IsAllowedEntryCount(lut_entries)) return XPE_ERR_INVALID_INPUT;

    const double adc_max = static_cast<double>(lut_entries - 1u);

    // --- steps 1-2: mean signal per dose level, in ADU ---------------------
    std::vector<double> dose(static_cast<size_t>(num_levels));
    std::vector<double> meas(static_cast<size_t>(num_levels));
    for (int32_t i = 0; i < num_levels; ++i) {
        if (!std::isfinite(dose_levels[i]) || dose_levels[i] <= 0.0) {
            return XPE_ERR_INVALID_INPUT;
        }
        dose[static_cast<size_t>(i)] = dose_levels[i];
        if (!MeanSignalAdu(flat_frames[i], dark_reference,
                           &meas[static_cast<size_t>(i)])) {
            return XPE_ERR_INVALID_INPUT;
        }
    }

    // The knots must be strictly increasing in raw signal for the LUT to be a
    // function of the raw value at all. A measured response that is flat or
    // falls with dose is the "non-monotone" case the requirement names.
    for (size_t i = 0; i + 1 < dose.size(); ++i) {
        if (dose[i + 1] <= dose[i]) return XPE_ERR_INVALID_INPUT;
        if (meas[i + 1] <= meas[i]) return XPE_ERR_INVALID_CALIB_DATA;
    }
    if (meas.front() <= 0.0) return XPE_ERR_INVALID_CALIB_DATA;
    if (meas.back() >= adc_max) return XPE_ERR_INVALID_CALIB_DATA;

    // --- step 3: ideal linear response through the origin -------------------
    double num = 0.0, den = 0.0;
    for (size_t i = 0; i < dose.size(); ++i) {
        num += dose[i] * meas[i];
        den += dose[i] * dose[i];
    }
    if (den <= 0.0) return XPE_ERR_INVALID_INPUT;
    const double g_nominal = num / den;
    if (!std::isfinite(g_nominal) || g_nominal <= 0.0) {
        return XPE_ERR_INVALID_CALIB_DATA;
    }

    // --- steps 4 and 6: knots, with the two boundary conditions -------------
    std::vector<double> xs;   // measured raw ADU
    std::vector<double> ys;   // linearized ADU
    xs.reserve(dose.size() + 2);
    ys.reserve(dose.size() + 2);
    xs.push_back(0.0);
    ys.push_back(0.0);                       // "LUT[0] = 0"
    for (size_t i = 0; i < dose.size(); ++i) {
        xs.push_back(meas[i]);
        ys.push_back(g_nominal * dose[i]);   // "LUT[S_meas] = S_ideal"
    }
    xs.push_back(adc_max);
    ys.push_back(adc_max);                   // "LUT[ADC_max] = ADC_max"

    // The two pinned ends can contradict the fit: if the ideal value at the
    // highest measured point already exceeds ADC_max, no monotone curve reaches
    // the identity endpoint. That is bad calibration data, not a bug here.
    for (size_t i = 0; i + 1 < ys.size(); ++i) {
        if (ys[i + 1] <= ys[i]) return XPE_ERR_INVALID_CALIB_DATA;
    }

    // --- step 5: monotone cubic interpolation over every entry --------------
    const std::vector<double> tangents = MonotoneTangents(xs, ys);
    std::vector<uint16_t> lut(lut_entries);
    for (uint32_t idx = 0; idx < lut_entries; ++idx) {
        const double x = static_cast<double>(idx);
        double v = MonotoneCubic(xs, ys, tangents, x);
        if (!std::isfinite(v)) return XPE_ERR_INVALID_CALIB_DATA;
        v = std::max(0.0, std::min(adc_max, v));
        lut[idx] = static_cast<uint16_t>(std::lround(v));
    }
    lut[0] = 0u;
    lut[lut_entries - 1u] = static_cast<uint16_t>(adc_max);

    // Monotonicity check, quoted: "LUT[i] <= LUT[i+1] for all i (enforced;
    // non-monotone LUT = XPE_ERR_INVALID_CALIB_DATA)". Rounding to uint16 can
    // only tie neighbours, never invert them, so this fires on a genuinely
    // non-monotone interpolant -- which is exactly what it is here to catch if
    // the tangent limiter is ever weakened.
    for (uint32_t i = 0; i + 1 < lut_entries; ++i) {
        if (lut[i] > lut[i + 1]) return XPE_ERR_INVALID_CALIB_DATA;
    }

    // --- write the file -----------------------------------------------------
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version = XCAL_VERSION;
    hdr.type = static_cast<uint32_t>(XCAL_TYPE_NONLIN_LUT);
    hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_UINT16);
    LutGeometry(lut_entries, &hdr.width, &hdr.height);
    hdr.created_epoch_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    hdr.expiry_epoch_ms = 0;

    // The generation conditions travel with the table: a LUT read back without
    // them cannot be checked against the detector or the dose range it was made
    // for. Written as config_json so the 152-byte header layout is untouched.
    char cfg[512];
    const int cfg_len = std::snprintf(cfg, sizeof(cfg),
        "{\"xcal_nonlin_entries\":%u,\"xcal_nonlin_adc_max\":%.0f,"
        "\"xcal_nonlin_g_nominal\":%.9g,\"xcal_nonlin_dose_levels\":%d,"
        "\"xcal_nonlin_dose_min\":%.9g,\"xcal_nonlin_dose_max\":%.9g,"
        "\"xcal_nonlin_detector\":%s}",
        lut_entries, adc_max, g_nominal, num_levels, dose.front(), dose.back(),
        (metadata_json != nullptr && metadata_json[0] != '\0') ? metadata_json
                                                              : "null");
    if (cfg_len <= 0 || static_cast<size_t>(cfg_len) >= sizeof(cfg)) {
        return XPE_ERR_INVALID_INPUT;   // detector metadata too long for the blob
    }

    return write_xcal_file(output_path, hdr,
                           reinterpret_cast<const uint8_t*>(cfg),
                           static_cast<uint64_t>(cfg_len),
                           reinterpret_cast<const uint8_t*>(lut.data()),
                           static_cast<uint64_t>(lut.size() * sizeof(uint16_t)));
}
