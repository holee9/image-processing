/**
 * @file xpe_calib_generate_gain.cpp
 * @brief Gain map generation implementation (SWU-1.12: FUNC-026, FUNC-027)
 *
 * SRS-CALIB-FUNC-026: Generate flat-field gain map from flat frames
 * SRS-CALIB-FUNC-027: Generate dose-dependent gain polynomial
 *
 * Algorithm (FUNC-026):
 *   1. Validate input dimensions consistency
 *   2. Dark subtraction: flat_corr[i] = flat_frames[i] - dark_reference
 *   3. Pixel-wise mean: G_raw(x,y) = mean(flat_corr[0..N-1][x,y])
 *   4. Normalize: G(x,y) = G_raw(x,y) / mean(G_raw)
 *   5. Single-frame mode: compute uncertainty σ², store in metadata
 *   6. Write via xcal_writer (XCAL_TYPE_GAIN format)
 *
 * Algorithm (FUNC-027):
 *   1. Load N gain maps from FUNC-026 output files
 *   2. For each pixel: fit polynomial G(x,y,E) = c0 + c1*E + c2*E² + ...
 *   3. Validate monotonicity in [E_min, E_max] (non-decreasing OR non-increasing -- QA-A-210c)
 *   4. If non-monotone: reduce degree, refit; degree 1 is the least-squares line and always passes (min degree = 1)
 *   5. Store coefficient array: (d+1) × W × H
 *   6. Write via xcal_writer (XCAL_TYPE_GAIN_POLY format)
 *
 * @MX:ANCHOR: [AUTO] xpe_calib_generate_gain – flat-field gain calibration
 * @MX:REASON: Critical offline calibration path; dark subtraction + normalization mandatory
 * @MX:SPEC: SWU-1.12 FUNC-026, FUNC-027
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include "xcal_writer.hpp"
#include "xcal_reader.hpp"

#include <cstring>
#include <cmath>
#include <memory>
#include <chrono>
#include <vector>
#include <cstdint>
#include <algorithm>

// =============================================================================
// Internal helper: least-squares polynomial fit
// =============================================================================

/**
 * @brief Fit polynomial y = c0 + c1*x + c2*x² + ... + cd*x^d using least squares
 *
 * @param x Independent variable values (dose levels), length N
 * @param y Dependent variable values (gain values), length N
 * @param N Number of data points
 * @param degree Polynomial degree (1 ≤ degree ≤ 4)
 * @param coeffs_out Output coefficient array, size (degree+1)
 * @return XPE_OK on success, XPE_ERR_PROCESSING_FAILED on singular matrix
 */
static XpeErrorCode fit_polynomial_ls(
    const double* x,
    const double* y,
    int32_t N,
    int32_t degree,
    double* coeffs_out)
{
    // QA-A-210d (#233): the fit is computed CENTRED AND SCALED, t = (x - mean) / half-range in [-1, 1], and the
    // coefficients are then expanded back to the raw dose that the file stores and the applier evaluates. The normal
    // equations in the raw dose have a condition number of about (x / span)^2: for doses 1000 .. 1000.00004 that is
    // 6e14 against a double precision of 1e-16, so the slope was lost to cancellation (and for doses below ~1e-3 the
    // fixed pivot threshold below called the system singular). In t the matrix is well scaled whatever the doses.
    if (N < degree + 1) {
        return XPE_ERR_INVALID_INPUT; // Not enough points for this degree
    }

    const size_t sM = static_cast<size_t>(degree + 1); // Number of coefficients (size_t)
    const size_t sN = static_cast<size_t>(N);

    double xm = 0.0;
    for (size_t i = 0; i < sN; ++i) xm += x[i];
    xm /= static_cast<double>(sN);
    double h = 0.0;
    for (size_t i = 0; i < sN; ++i) h = std::max(h, std::abs(x[i] - xm));
    if (!(h > 0.0) || !std::isfinite(h) || !std::isfinite(xm)) {
        return XPE_ERR_PROCESSING_FAILED; // all doses equal, or not finite
    }

    // Build normal equations in t: A^T * A * b = A^T * y, A[i][j] = t[i]^j
    std::vector<std::vector<double>> aug(sM, std::vector<double>(sM + 1, 0.0));
    std::vector<double> pw(2 * sM - 1, 0.0);
    for (size_t i = 0; i < sN; ++i) {
        const double t = (x[i] - xm) / h;
        pw[0] = 1.0;
        for (size_t k = 1; k < pw.size(); ++k) pw[k] = pw[k - 1] * t;
        for (size_t j = 0; j < sM; ++j) {
            for (size_t k = 0; k < sM; ++k) aug[j][k] += pw[j + k];
            aug[j][sM] += pw[j] * y[i];
        }
    }

    // Gaussian elimination with partial pivoting
    for (size_t col = 0; col < sM; ++col) {
        size_t pivot_row = col;
        double max_val = std::abs(aug[col][col]);
        for (size_t row = col + 1; row < sM; ++row) {
            if (std::abs(aug[row][col]) > max_val) {
                max_val = std::abs(aug[row][col]);
                pivot_row = row;
            }
        }

        if (max_val < 1e-12) {
            return XPE_ERR_PROCESSING_FAILED; // Singular matrix
        }

        if (pivot_row != col) {
            std::swap(aug[col], aug[pivot_row]);
        }

        for (size_t row = col + 1; row < sM; ++row) {
            double factor = aug[row][col] / aug[col][col];
            for (size_t j = col; j <= sM; ++j) {
                aug[row][j] -= factor * aug[col][j];
            }
        }
    }

    // Back substitution: b[j] multiplies t^j
    double b[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
    for (size_t i = sM; i-- > 0; ) {
        double sum = aug[i][sM];
        for (size_t j = i + 1; j < sM; ++j) {
            sum -= aug[i][j] * b[j];
        }
        b[i] = sum / aug[i][i];
    }

    // Back to the raw dose: sum_k b_k ((x - xm)/h)^k = sum_j c_j x^j, c_j = sum_{k>=j} b_k h^-k C(k,j) (-xm)^(k-j)
    static const double kBinom[5][5] = {
        {1, 0, 0, 0, 0}, {1, 1, 0, 0, 0}, {1, 2, 1, 0, 0}, {1, 3, 3, 1, 0}, {1, 4, 6, 4, 1}};
    for (size_t j = 0; j < sM; ++j) {
        double cj = 0.0;
        for (size_t k = j; k < sM; ++k) {
            cj += b[k] / std::pow(h, static_cast<double>(k)) * kBinom[k][j] * std::pow(-xm, static_cast<double>(k - j));
        }
        if (!std::isfinite(cj)) {
            return XPE_ERR_PROCESSING_FAILED;
        }
        coeffs_out[j] = cj;
    }

    return XPE_OK;
}

/**
 * @brief Whether the polynomial is monotone over the WHOLE closed interval [x_min, x_max], in either direction
 *
 * QA-A-210d (#233): decided ANALYTICALLY, not by sampling. The earlier rule looked at 100 sample points; a quadratic or
 * higher curve can turn between two samples (Codex #58: G(E) = (E - 1001)^2 / 1e6 over doses 1000 .. 2000 falls from
 * 1000 to 1001 and rises after, and the first sample gap is ~10 ADU), while the guarantee the code, the header and
 * SRS-CALIB-FUNC-027 give is for the whole measured range.
 *
 * Degree <= 4, so P' has degree <= 3 and its turning points are the real roots of P'' (degree <= 2, closed forms).
 * Between two consecutive such points P' is monotone, so it keeps one sign on that piece exactly when it has the same
 * sign (or zero) at the piece's two ends. P is therefore monotone iff P' >= 0 at every end point of every piece, or
 * P' <= 0 at every one. The caller passes the coefficients AS THE FILE STORES THEM (float32 values), so the verdict is
 * about the polynomial the applier will evaluate. A non-finite value is "not monotone".
 *
 * @param c      Coefficient array, size (degree+1), raw dose basis
 * @param degree Polynomial degree, 1..4
 */
static bool validate_monotonicity(
    const double* c,
    int32_t degree,
    double x_min,
    double x_max)
{
    double d1[4] = {0.0, 0.0, 0.0, 0.0};     // P'(x)  = d1[0] + d1[1] x + d1[2] x^2 + d1[3] x^3
    double d2[3] = {0.0, 0.0, 0.0};          // P''(x) = d2[0] + d2[1] x + d2[2] x^2
    for (int32_t j = 1; j <= degree; ++j) d1[j - 1] = static_cast<double>(j) * c[j];
    for (int32_t j = 2; j <= degree; ++j) d2[j - 2] = static_cast<double>(j * (j - 1)) * c[j];

    double pts[4];
    size_t np = 0;
    pts[np++] = x_min;
    pts[np++] = x_max;
    double roots[2];
    size_t nr = 0;
    if (d2[2] != 0.0) {
        const double disc = d2[1] * d2[1] - 4.0 * d2[2] * d2[0];
        if (disc >= 0.0) {
            const double q = -0.5 * (d2[1] + std::copysign(std::sqrt(disc), d2[1]));
            roots[nr++] = q / d2[2];
            roots[nr++] = (q != 0.0) ? d2[0] / q : q / d2[2];
        }
    } else if (d2[1] != 0.0) {
        roots[nr++] = -d2[0] / d2[1];
    }
    for (size_t i = 0; i < nr; ++i) {
        if (roots[i] > x_min && roots[i] < x_max) pts[np++] = roots[i];
    }

    bool non_negative = true;
    bool non_positive = true;
    for (size_t i = 0; i < np; ++i) {
        const double x = pts[i];
        const double v = ((d1[3] * x + d1[2]) * x + d1[1]) * x + d1[0];
        if (!std::isfinite(v)) return false;
        if (v < 0.0) non_negative = false;
        if (v > 0.0) non_positive = false;
    }
    return non_negative || non_positive;
}

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

        // FUNC-031 (8): the mode applies here too. This entry point acquires
        // at one dose level with no fitted curve (degree 0), which every
        // explicit mode accepts; AUTO resolves to SINGLE_POINT (#169).
        XpeCalibrationMode mode = XPE_CALIB_SINGLE_POINT;
        {
            const XpeErrorCode mrc = xpe_calib_resolve_mode(1, 0, &mode);
            if (mrc != XPE_OK) return mrc;
        }
        if (xpe_calib_get_mode() == XPE_CALIB_AUTO) {
            char note[96];
            std::snprintf(note, sizeof(note),
                          "XPE_CALIB_AUTO resolved to mode %d", static_cast<int>(mode));
            xpe_alert_push(note, XPE_ALERT_INFO);
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

// =============================================================================
// FUNC-027: xpe_calib_generate_gain_polynomial
// =============================================================================

extern "C" XPE_API XpeErrorCode xpe_calib_generate_gain_polynomial(
    const char** gain_file_paths,
    const double* dose_levels,
    int32_t       num_levels,
    int32_t       max_degree,
    const char*   output_path)
{
    try {
        // --- Input validation ---
        if (gain_file_paths == nullptr || dose_levels == nullptr ||
            output_path == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }
        if (num_levels < 3) {
            return XPE_ERR_INVALID_INPUT; // Need at least 3 points for polynomial
        }
        if (max_degree < 1 || max_degree > 4) {
            return XPE_ERR_INVALID_INPUT; // Restrict to degree 1-4
        }
        // QA-A-124 (#194): dose_levels must be strictly ascending.
        //
        // dose_levels[0] and dose_levels[n-1] are taken as the range minimum
        // and maximum below. That reading is only true of a sorted array, and
        // nothing checked it. The assumption used to be cheap -- the two
        // values fed only the monotonicity check, so a wrong pair made the
        // check look at the wrong interval and nothing else. QA-A-123 turned
        // the same pair into the clamp boundary, which changes output pixel
        // values, so the assumption now decides whether the image is right.
        //
        // Unsorted input is REFUSED, not sorted here. Each dose level is
        // paired with the gain map at the same index; if the caller shuffled
        // the doses, the pairing may already be wrong, and silently reordering
        // one side would fit a curve through mismatched points and write it to
        // a file that looks fine. An equal pair is refused too -- two maps at
        // one dose give the fit two y values for one x.
        // QA-A-210d (Codex #58): every dose must be FINITE, checked first. A last dose of +infinity passed the ordering
        // test below (inf > 3), reached the normal equations as NaN coefficients, and the monotonicity test accepted
        // them because every comparison with NaN is false. Refused here -- before the mode is resolved, any file is
        // opened, and the quality metadata can change.
        for (int32_t i = 0; i < num_levels; ++i) {
            if (!std::isfinite(dose_levels[i])) {
                return XPE_ERR_INVALID_INPUT;
            }
        }
        for (int32_t i = 1; i < num_levels; ++i) {
            if (!(dose_levels[i] > dose_levels[i - 1])) {
                return XPE_ERR_INVALID_INPUT;
            }
        }

        // FUNC-031 (3)(4)(5)(8): an explicit mode caps the level count and the
        // degree; AUTO picks the smallest mode that accepts both (#169).
        XpeCalibrationMode mode = XPE_CALIB_MULTI_POINT_10;
        {
            const XpeErrorCode mrc = xpe_calib_resolve_mode(num_levels, max_degree, &mode);
            if (mrc != XPE_OK) return mrc;
        }
        if (xpe_calib_get_mode() == XPE_CALIB_AUTO) {
            char note[96];
            std::snprintf(note, sizeof(note),
                          "XPE_CALIB_AUTO resolved to mode %d", static_cast<int>(mode));
            xpe_alert_push(note, XPE_ALERT_INFO);
        }

        // --- Load all gain maps ---
        std::vector<std::vector<float>> gain_maps(static_cast<size_t>(num_levels));
        std::vector<XCalFileHeader> headers(static_cast<size_t>(num_levels));
        uint32_t width = 0;
        uint32_t height = 0;

        const size_t sLevels = static_cast<size_t>(num_levels);
        for (size_t i = 0; i < sLevels; ++i) {
            if (gain_file_paths[i] == nullptr) {
                return XPE_ERR_INVALID_INPUT;
            }

            std::vector<uint8_t> config, payload;
            XpeErrorCode rc = read_xcal_file(
                gain_file_paths[i],
                headers[i],
                config,
                payload,
                false, // Don't check expiry for calibration generation
                static_cast<int>(XCAL_TYPE_GAIN));

            if (rc != XPE_OK) {
                return rc;
            }

            // Validate dimensions consistency
            if (i == 0) {
                width = headers[i].width;
                height = headers[i].height;
            } else {
                if (headers[i].width != width || headers[i].height != height) {
                    return XPE_ERR_INVALID_INPUT;
                }
            }

            // Extract float32 payload
            size_t n_pixels_frame = static_cast<size_t>(width) * height;
            const float* data = reinterpret_cast<const float*>(payload.data());
            gain_maps[i].assign(data, data + n_pixels_frame);

            // QA-A-210d: a non-finite gain value is bad calibration data. Fitted, it gave NaN coefficients that the
            // monotonicity test then passed (comparisons with NaN are false) and the file stored.
            for (const float v : gain_maps[i]) {
                if (!std::isfinite(v)) {
                    return XPE_ERR_INVALID_CALIB_DATA;
                }
            }
        }

        // --- Fit polynomial for each pixel ---
        size_t n_pixels = static_cast<size_t>(width) * height;
        const size_t sMaxCoeffsPoly = static_cast<size_t>(max_degree) + size_t{1};

        // Output: (degree+1) × W × H coefficient array
        // Store as [c0_pixel0, c1_pixel0, ..., cd_pixel0, c0_pixel1, ...]
        std::vector<float> coeff_array(n_pixels * sMaxCoeffsPoly);

        // FUNC-033 (1): fit-quality accumulators, filled as each pixel is fitted.
        //   R2 = 1 - SS_res / SS_tot, pooled over every pixel and dose level.
        //   residual_pct is |residual| as a percentage of that pixel's mean gain.
        double   ss_res_total   = 0.0;
        double   ss_tot_total   = 0.0;
        double   residual_pct_sum = 0.0;
        double   max_residual_pct = 0.0;
        size_t   residual_count = 0;
        uint32_t highest_degree = 0;

        // QA-A-210e (Codex #61): what the applier does with a stored fit decides whether the fit is accepted and what
        // the file reports. kApplyTolerance is the largest relative difference accepted between the gain the applier's
        // float32 arithmetic computes at a measured dose and the gain the least-squares fit intended there: 0.1% -- a
        // tenth of the 1% flat-field residual the gain verification allows (xpe_verify_gain), and below the 0.3%
        // level-to-level scatter of the real calibration data (QA-A-210b). A larger departure means the stored
        // polynomial is not the fit.
        constexpr double kApplyTolerance = 1e-3;
        size_t applier_reject_count = 0;   // pixels no degree can be stored for
        size_t applier_reject_first = 0;

        // QA-A-211 (#233): a pixel whose gain is outside [XPE_GAIN_APPLIED_MIN, XPE_GAIN_APPLIED_MAX] is not refused any
        // more -- it is CLASSIFIED DEFECTIVE: all its coefficients are stored as 0, which the applier evaluates to a gain
        // outside the range and so classifies at application time too (gain 1.0, the defect stage fills it). Two ways
        // to be classified: a MEASURED gain of the pixel is outside the range (a failed pixel, a low-sensitivity edge
        // band), or no degree down to the least-squares line evaluates inside the range at the measured doses. A
        // classified pixel is not fitted, so it is not in the quality figures. More than XPE_GAIN_DEFECT_MAX_FRACTION of
        // the frame classified refuses the generation. (A fit that is in range but more than kApplyTolerance from what
        // the pixel measured is NOT classified: that is the file failing to carry the data, and refuses as before.)
        XpeGainScan classified;
        classified.total = n_pixels;
        const auto classify_pixel = [&](size_t pix) {
            if (classified.count == 0) classified.first = pix;
            ++classified.count;
            const size_t py = pix / width, px = pix % width;
            if (py < 64 || py + 64 >= height || px < 64 || px + 64 >= width) ++classified.inBand;
            // the coefficients stay as the zeros coeff_array was created with
        };

        // For each pixel: fit polynomial with degree reduction if needed
        const size_t sNumLevels = static_cast<size_t>(num_levels);
        const size_t sMaxDegree = static_cast<size_t>(max_degree);

        for (size_t pix = 0; pix < n_pixels; ++pix) {
            // Extract gain values across dose levels for this pixel
            std::vector<double> y_vals(sNumLevels);
            for (size_t i = 0; i < sNumLevels; ++i) {
                y_vals[i] = static_cast<double>(gain_maps[i][pix]);
            }

            // QA-A-211: a measured gain outside the range classifies the pixel before any fit is tried.
            {
                bool measured_out_of_range = false;
                for (size_t i = 0; i < sNumLevels; ++i) {
                    if (!xpe_gain_value_valid(static_cast<float>(y_vals[i]))) { measured_out_of_range = true; break; }
                }
                if (measured_out_of_range) {
                    classify_pixel(pix);
                    continue;
                }
            }

            // Try fitting from max_degree down to degree 1
            bool deg1_range_failed = false;   // at degree 1 the float32 evaluation left the range (QA-A-211)
            bool fit_success = false;
            bool degree1_solved = false;     // the least-squares line itself could be solved (QA-A-210e)
            size_t final_degree = 1;
            std::vector<double> coeffs(sMaxCoeffsPoly);

            for (size_t deg = sMaxDegree; deg >= size_t{1}; --deg) {
                size_t n_coeffs = deg + size_t{1};
                std::vector<double> temp_coeffs(n_coeffs);

                XpeErrorCode rc = fit_polynomial_ls(
                    dose_levels, y_vals.data(),
                    num_levels, static_cast<int32_t>(deg),
                    temp_coeffs.data());

                if (rc != XPE_OK) {
                    continue; // Try lower degree
                }
                if (deg == 1) degree1_solved = true;

                // QA-A-210d: the coefficients are decided on AS THEY WILL BE STORED (float32), and a non-finite one fails the
                // degree. The analytic monotonicity test then covers the whole range [dose_min, dose_max] for the
                // polynomial the applier will really evaluate.
                double stored[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
                bool stored_finite = true;
                for (size_t j = 0; j < n_coeffs; ++j) {
                    const float f = static_cast<float>(temp_coeffs[j]);
                    if (!std::isfinite(f)) stored_finite = false;
                    stored[j] = static_cast<double>(f);
                }
                if (!stored_finite) {
                    continue; // Try lower degree
                }

                double dose_min = dose_levels[0];
                double dose_max = dose_levels[num_levels - 1];

                // QA-A-210e (Codex #61): and the fit must survive the APPLIER's arithmetic. The applier evaluates the stored
                // float32 coefficients in float32 Horner at the pixel value and refuses a gain outside its range; a large
                // intercept and slope that cancel in double can fail to cancel in float32 (doses 1000 .. 1000.00004,
                // gain 0.001 + 5 (dose - 1000): stored [-5000, 5.00000095], gain at 1000 evaluates to 0.00097656, below the
                // applier's floor). Checked at every measured dose -- the curve is monotone, so its extremes over the
                // range are at the first and last dose -- against the double-precision value of the same fit: the float32
                // evaluation must be a gain the applier accepts, within kApplyTolerance of the intended one. A fit that
                // fails lowers the degree like a non-monotone one; at degree 1 the pixel is refused (below).
                bool apply_ok = true;
                bool range_failed = false;
                {
                    float stored_f[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
                    for (size_t j = 0; j < n_coeffs; ++j) stored_f[j] = static_cast<float>(stored[j]);
                    for (int32_t i = 0; i < num_levels && apply_ok; ++i) {
                        const float applied = xpe_gain_poly_eval_f32(stored_f, static_cast<uint32_t>(n_coeffs),
                                                                     static_cast<float>(dose_levels[i]));
                        double intended = 0.0;
                        for (size_t j = n_coeffs; j-- > size_t{0};) intended = intended * dose_levels[i] + temp_coeffs[j];
                        if (!xpe_gain_value_valid(applied)) {
                            apply_ok = false;
                            range_failed = true;
                        } else if (std::abs(static_cast<double>(applied) - intended) > kApplyTolerance * std::abs(intended)) {
                            apply_ok = false;
                        }
                    }
                }
                if (deg == 1 && range_failed) deg1_range_failed = true;

                if (apply_ok && validate_monotonicity(
                    stored, static_cast<int32_t>(deg),
                    dose_min, dose_max))
                {
                    // Success: copy coefficients
                    std::memcpy(coeffs.data(), temp_coeffs.data(),
                               n_coeffs * sizeof(double));
                    final_degree = deg;
                    fit_success = true;
                    break;
                }
            }

            if (!fit_success && degree1_solved && deg1_range_failed) {
                // QA-A-211: the least-squares line itself evaluates outside the gain range at a measured dose although
                // every measured gain is inside it (a pixel at the edge of the range): classified, like a measured
                // out-of-range gain.
                classify_pixel(pix);
                continue;
            }
            if (!fit_success && degree1_solved) {
                // Every degree down to the line was solved, and none can be applied by the applier's arithmetic: this
                // pixel cannot be represented. Counted; the generation is refused after the loop with every such pixel
                // reported, before the quality record or the file is touched.
                if (applier_reject_count == 0) applier_reject_first = pix;
                ++applier_reject_count;
                continue;
            }
            if (!fit_success) {
                // Degree 1 is the last stop and always passes validate_monotonicity (a straight line is monotone
                // whichever way it points), so this is reached only when the least-squares system itself cannot be
                // solved -- doses so close together that the normal equations are singular. QA-A-210c (#233): the
                // straight line through the first and last measurement that used to stand here is gone; it was not
                // a least-squares solution, so a pixel on it could score worse than its own mean (R^2 < 0), and on
                // real data half the pixels were on it.
                return XPE_ERR_PROCESSING_FAILED;
            }

            // FUNC-033 (1): score this pixel's fit before storing it.
            {
                double y_mean = 0.0;
                for (size_t i = 0; i < sNumLevels; ++i) y_mean += y_vals[i];
                y_mean /= static_cast<double>(sNumLevels);

                // QA-A-210e: scored in the APPLIER's arithmetic -- the stored float32 coefficients, float32 Horner, the
                // dose as a float -- so the quality the file reports is the one of the correction it will perform.
                // (It used to be the double coefficients in double arithmetic, which agree on well-conditioned data and
                // part where float32 cannot carry the fit.)
                float stored_cf[5] = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f};
                for (size_t j = 0; j <= final_degree; ++j) stored_cf[j] = static_cast<float>(coeffs[j]);
                for (size_t i = 0; i < sNumLevels; ++i) {
                    const double predicted = static_cast<double>(xpe_gain_poly_eval_f32(
                        stored_cf, static_cast<uint32_t>(final_degree + size_t{1}), static_cast<float>(dose_levels[i])));
                    const double residual = y_vals[i] - predicted;
                    ss_res_total += residual * residual;
                    const double dev = y_vals[i] - y_mean;
                    ss_tot_total += dev * dev;

                    if (y_mean != 0.0) {
                        const double pct = std::abs(residual / y_mean) * 100.0;
                        residual_pct_sum += pct;
                        if (pct > max_residual_pct) max_residual_pct = pct;
                        ++residual_count;
                    }
                }
                if (static_cast<uint32_t>(final_degree) > highest_degree) {
                    highest_degree = static_cast<uint32_t>(final_degree);
                }
            }

            // Store coefficients in output array
            size_t offset = pix * sMaxCoeffsPoly;
            for (size_t j = 0; j <= final_degree; ++j) {
                coeff_array[offset + j] = static_cast<float>(coeffs[j]);
            }
            // Pad remaining coefficients with zeros
            for (size_t j = final_degree + size_t{1}; j < sMaxCoeffsPoly; ++j) {
                coeff_array[offset + j] = 0.0f;
            }
        }

        // QA-A-210e: a pixel for which no degree can be stored refuses the whole generation -- no quality record, no file.
        // INVALID_CALIB_DATA, like the other "this data cannot make a calibration" refusals (a non-finite gain value,
        // an unusable fit), with an alert that names the cause and how many pixels.
        if (applier_reject_count > 0) {
            char reject_msg[400];
            std::snprintf(reject_msg, sizeof(reject_msg),
                          "XPE_WARN_GAIN_POLY_NOT_APPLICABLE: %zu pixel(s) (first: %zu) have no gain polynomial whose float32 "
                          "coefficients the applier can use -- evaluated in float32 at a measured dose it is more than %.1f%% "
                          "from the fit. The doses are too close together for coefficients stored in the raw dose; "
                          "no file was written",
                          applier_reject_count, applier_reject_first,
                          kApplyTolerance * 100.0);
            reject_msg[sizeof(reject_msg) - 1] = '\0';
            xpe_alert_push(reject_msg, XPE_ALERT_ERROR);
            return XPE_ERR_INVALID_CALIB_DATA;
        }

        // QA-A-211 (#233): the classified pixels. Over the cap the generation is refused (nothing recorded, no file); under
        // it the count is reported with the cause hint (how many lie in the outer 64-pixel band) and the file is written.
        if (classified.count > 0) {
            if (xpe_gain_scan_over_limit(classified)) {
                xpe_gain_alert_over_limit(classified, "generated");
                return XPE_ERR_INVALID_CALIB_DATA;
            }
            xpe_gain_alert_classified(classified);
        }

        // --- FUNC-033: score the whole fit and record the metadata ---
        //
        // SRS-CALIB-001 SRS-CALIB-FUNC-033 (2): "If fit_r_squared < 0.999 after
        // fitting, system shall log XPE_WARN_CALIB_POOR_FIT and include
        // recommendation to increase dose levels or check detector stability."
        // No XPE_WARN_* code exists in xpe_error.h, so the warning is raised on
        // the alert queue -- the module's only operator-visible channel -- with
        // that identifier as the message prefix (QA-A-35 note).
        //
        // A perfectly flat input has SS_tot == 0: every sample equals the mean,
        // the fit reproduces it exactly, and R2 is 1.0 by definition rather than
        // 0/0.
        const double r_squared = (ss_tot_total > 0.0)
                                 ? (1.0 - ss_res_total / ss_tot_total)
                                 : 1.0;

        // Named `quality`, not `meta`: the config-JSON buffer below already
        // owns that name in this scope.
        XpeCalibQualityMeta quality{};
        quality.calibration_mode  = static_cast<uint8_t>(mode);
        quality.polynomial_degree = static_cast<uint8_t>(highest_degree);
        quality.num_points        = static_cast<uint8_t>(num_levels);
        quality.r_squared         = r_squared;

        const bool gate_passed = xpe_calib_record_quality_meta(quality);
        const double mean_residual_pct =
            (residual_count > 0) ? (residual_pct_sum / static_cast<double>(residual_count)) : 0.0;

        if (!gate_passed) {
            // The SRS asks for the recommendation to travel with the warning, so
            // the residual figures go in the message: they are what tells the
            // operator whether a few pixels or the whole field is the problem.
            char warn[256];
            std::snprintf(warn, sizeof(warn),
                          "XPE_WARN_CALIB_POOR_FIT: fit_r_squared=%.6f below %.3f "
                          "(max_residual=%.3f%%, mean_residual=%.3f%%); "
                          "increase dose levels or check detector stability",
                          r_squared, XPE_CALIB_R_SQUARED_GATE,
                          max_residual_pct, mean_residual_pct);
            xpe_alert_push(warn, XPE_ALERT_WARNING);
        }

        // --- Build XCal v1 header ---
        using namespace std::chrono;
        int64_t now_ms = duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()).count();

        XCalFileHeader hdr;
        std::memset(&hdr, 0, sizeof(hdr));
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version          = XCAL_VERSION;
        hdr.type             = static_cast<uint32_t>(XCAL_TYPE_GAIN_POLY);
        hdr.pixel_format     = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width            = width;
        hdr.height           = height;
        hdr.created_epoch_ms = now_ms;
        hdr.expiry_epoch_ms  = 0; // never expires
        hdr.payload_len      = static_cast<uint64_t>(n_pixels * sMaxCoeffsPoly) * sizeof(float);

        // session_id: "generated" (null-padded)
        std::memcpy(hdr.session_id, "generated\0", 10);

        // --- Build config JSON with polynomial metadata ---
        // FUNC-033 (5): the quality metadata rides in the XCal config JSON.
        // Field names follow the SRS wording (fit_r_squared, actual_dose_levels,
        // max_residual_pct, mean_residual_pct), not the C struct's member names.
        //
        // polynomial_degree is the FITTED degree (SRS-CALIB-FUNC-033 (1)), the
        // same value recorded in the store above. It used to carry max_degree,
        // the requested ceiling, so a fit that dropped a degree reported one
        // value right after generation and another after the file was loaded
        // (QA-A-87, #140). The ceiling keeps its own key; num_coefficients is
        // the payload stride and still follows the ceiling, because every
        // pixel is stored at max_degree + 1 coefficients.
        // QA-A-123 (#194): record the dose range the fit was validated over.
        // These are not new numbers -- they are the same two values the
        // monotonicity check above already takes (`:507-508`) and then drops.
        // Without them the file carries no boundary, so nothing downstream can
        // tell "inside the fit" from "extrapolated": a pixel far above the top
        // knot was evaluated on the same polynomial and, on a steeply curved
        // ladder, received 2.78x the top-knot gain -- enough to make a
        // saturated pixel come out DARKER than a D_max one (QA-A-122 probe).
        char meta[640];
        std::snprintf(meta, sizeof(meta),
            "{\"polynomial_degree\":%d,\"max_polynomial_degree\":%d,"
            "\"num_coefficients\":%d,\"num_dose_levels\":%d,"
            "\"calibration_mode\":%d,\"requested_calibration_mode\":%d,"
            "\"actual_dose_levels\":%d,"
            "\"dose_min\":%.6f,\"dose_max\":%.6f,"
            "\"fit_r_squared\":%.9f,\"max_residual_pct\":%.6f,"
            "\"mean_residual_pct\":%.6f,\"calibration_pass\":%d}",
            static_cast<int>(highest_degree),
            static_cast<int>(max_degree),
            static_cast<int>(sMaxCoeffsPoly),
            static_cast<int>(num_levels),
            static_cast<int>(mode),
            static_cast<int>(xpe_calib_get_mode()),
            static_cast<int>(num_levels),
            dose_levels[0], dose_levels[num_levels - 1],
            r_squared, max_residual_pct, mean_residual_pct,
            gate_passed ? 1 : 0);
        meta[sizeof(meta) - 1] = '\0';

        std::string config_json = meta;

        // --- Write via xcal_writer ---
        const uint8_t* config_ptr = reinterpret_cast<const uint8_t*>(config_json.data());
        uint64_t config_len = static_cast<uint64_t>(config_json.size());
        hdr.config_json_len = config_len;

        return write_xcal_file(
            output_path, hdr,
            config_ptr, config_len,
            reinterpret_cast<const uint8_t*>(coeff_array.data()),
            hdr.payload_len);

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
