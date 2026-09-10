/**
 * @file xpe_calib_generate_gain.cpp
 * @brief Gain map generation implementation (SWU-1.12: FUNC-026, FUNC-027)
 *
 * REQ-P1A-026: Generate flat-field gain map from flat frames
 * REQ-P1A-027: Generate dose-dependent gain polynomial
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
 *   3. Validate monotonicity in [E_min, E_max]
 *   4. If non-monotone: reduce degree, refit (min degree = 1)
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
    if (N < degree + 1) {
        return XPE_ERR_INVALID_INPUT; // Not enough points for this degree
    }

    // Build normal equations: A^T * A * coeffs = A^T * y
    // where A[i][j] = x[i]^j
    const size_t sM = static_cast<size_t>(degree + 1); // Number of coefficients (size_t)
    const size_t sN = static_cast<size_t>(N);

    // Allocate augmented matrix [A^T*A | A^T*y] of size M x (M+1)
    std::vector<std::vector<double>> aug(sM, std::vector<double>(sM + 1, 0.0));

    // Compute A^T*A and A^T*y
    for (size_t i = 0; i < sN; ++i) {
        double x_pow = 1.0;
        for (size_t j = 0; j < sM; ++j) {
            // A^T*A[j][k] += x[i]^(j+k)
            double x_pow_j = x_pow;
            for (size_t k = 0; k < sM; ++k) {
                aug[j][k] += x_pow_j * std::pow(x[i], static_cast<double>(k));
            }
            // A^T*y[j] += x[i]^j * y[i]
            aug[j][sM] += x_pow_j * y[i];
            x_pow *= x[i];
        }
    }

    // Gaussian elimination with partial pivoting
    for (size_t col = 0; col < sM; ++col) {
        // Find pivot row
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

        // Swap rows
        if (pivot_row != col) {
            std::swap(aug[col], aug[pivot_row]);
        }

        // Eliminate column
        for (size_t row = col + 1; row < sM; ++row) {
            double factor = aug[row][col] / aug[col][col];
            for (size_t j = col; j <= sM; ++j) {
                aug[row][j] -= factor * aug[col][j];
            }
        }
    }

    // Back substitution (reverse loop using size_t idiom)
    for (size_t i = sM; i-- > 0; ) {
        double sum = aug[i][sM];
        for (size_t j = i + 1; j < sM; ++j) {
            sum -= aug[i][j] * coeffs_out[j];
        }
        coeffs_out[i] = sum / aug[i][i];
    }

    return XPE_OK;
}

/**
 * @brief Validate monotonicity of polynomial in [x_min, x_max]
 *
 * @param coeffs Coefficient array, size (degree+1)
 * @param degree Polynomial degree
 * @param x_min Lower bound of interval
 * @param x_max Upper bound of interval
 * @param num_samples Number of samples to check (default: 100)
 * @return true if monotone increasing, false otherwise
 */
static bool validate_monotonicity(
    const double* coeffs,
    int32_t degree,
    double x_min,
    double x_max,
    int32_t num_samples = 100)
{
    if (num_samples < 2) {
        num_samples = 2;
    }

    double dx = (x_max - x_min) / (num_samples - 1);
    double prev_y = 0.0;
    bool first = true;

    for (int32_t i = 0; i < num_samples; ++i) {
        double x = x_min + i * dx;
        double y = coeffs[0];
        double x_pow = x;
        for (int32_t j = 1; j <= degree; ++j) {
            y += coeffs[j] * x_pow;
            x_pow *= x;
        }

        if (!first && y < prev_y) {
            return false; // Not monotonic increasing
        }
        prev_y = y;
        first = false;
    }

    return true;
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
        quality.calibration_mode   = static_cast<uint8_t>(xpe_calib_get_mode());
        quality.polynomial_degree  = 0;
        quality.num_points         = 1;
        quality.r_squared          = 1.0;
        const bool gate_passed = xpe_calib_record_quality_meta(quality);

        char meta[512];
        std::snprintf(meta, sizeof(meta),
            "{\"calibration_mode\":%d,\"actual_dose_levels\":1,"
            "\"polynomial_degree\":0,\"fit_r_squared\":1.000000000,"
            "\"max_residual_pct\":0.000000,\"mean_residual_pct\":0.000000,"
            "\"calibration_pass\":%d",
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

        // For each pixel: fit polynomial with degree reduction if needed
        const size_t sNumLevels = static_cast<size_t>(num_levels);
        const size_t sMaxDegree = static_cast<size_t>(max_degree);

        for (size_t pix = 0; pix < n_pixels; ++pix) {
            // Extract gain values across dose levels for this pixel
            std::vector<double> y_vals(sNumLevels);
            for (size_t i = 0; i < sNumLevels; ++i) {
                y_vals[i] = static_cast<double>(gain_maps[i][pix]);
            }

            // Try fitting from max_degree down to degree 1
            bool fit_success = false;
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

                // Validate monotonicity in [dose_min, dose_max]
                double dose_min = dose_levels[0];
                double dose_max = dose_levels[num_levels - 1];

                if (validate_monotonicity(
                    temp_coeffs.data(), static_cast<int32_t>(deg),
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

            if (!fit_success) {
                // Fallback: linear fit through first and last points
                coeffs[0] = y_vals[0];
                coeffs[1] = (y_vals[sNumLevels - size_t{1}] - y_vals[0]) /
                           (dose_levels[num_levels - 1] - dose_levels[0]);
                final_degree = 1;
            }

            // FUNC-033 (1): score this pixel's fit before storing it.
            {
                double y_mean = 0.0;
                for (size_t i = 0; i < sNumLevels; ++i) y_mean += y_vals[i];
                y_mean /= static_cast<double>(sNumLevels);

                for (size_t i = 0; i < sNumLevels; ++i) {
                    // Horner evaluation of the fitted polynomial at this dose.
                    double predicted = 0.0;
                    for (size_t j = final_degree + size_t{1}; j-- > size_t{0};) {
                        predicted = predicted * dose_levels[i] + coeffs[j];
                    }
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
        char meta[512];
        std::snprintf(meta, sizeof(meta),
            "{\"polynomial_degree\":%d,\"num_coefficients\":%d,\"num_dose_levels\":%d,"
            "\"calibration_mode\":%d,\"actual_dose_levels\":%d,"
            "\"fit_r_squared\":%.9f,\"max_residual_pct\":%.6f,"
            "\"mean_residual_pct\":%.6f,\"calibration_pass\":%d}",
            static_cast<int>(max_degree),
            static_cast<int>(sMaxCoeffsPoly),
            static_cast<int>(num_levels),
            static_cast<int>(xpe_calib_get_mode()),
            static_cast<int>(num_levels),
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
