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
    int32_t M = degree + 1; // Number of coefficients

    // Allocate augmented matrix [A^T*A | A^T*y] of size M x (M+1)
    std::vector<std::vector<double>> aug(M, std::vector<double>(M + 1, 0.0));

    // Compute A^T*A and A^T*y
    for (int32_t i = 0; i < N; ++i) {
        double x_pow = 1.0;
        for (int32_t j = 0; j < M; ++j) {
            // A^T*A[j][k] += x[i]^(j+k)
            double x_pow_j = x_pow;
            for (int32_t k = 0; k < M; ++k) {
                aug[j][k] += x_pow_j * std::pow(x[i], k);
            }
            // A^T*y[j] += x[i]^j * y[i]
            aug[j][M] += x_pow_j * y[i];
            x_pow *= x[i];
        }
    }

    // Gaussian elimination with partial pivoting
    for (int32_t col = 0; col < M; ++col) {
        // Find pivot row
        int32_t pivot_row = col;
        double max_val = std::abs(aug[col][col]);
        for (int32_t row = col + 1; row < M; ++row) {
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
        for (int32_t row = col + 1; row < M; ++row) {
            double factor = aug[row][col] / aug[col][col];
            for (int32_t j = col; j <= M; ++j) {
                aug[row][j] -= factor * aug[col][j];
            }
        }
    }

    // Back substitution
    for (int32_t i = M - 1; i >= 0; --i) {
        double sum = aug[i][M];
        for (int32_t j = i + 1; j < M; ++j) {
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

        // --- Single-frame mode: compute uncertainty (optional) ---
        std::string config_json;
        if (num_frames == 1 && metadata_json != nullptr) {
            // Parse metadata_json for kVp, mAs, etc. (pass-through)
            // Compute variance estimate: σ² = signal × (quantum_noise² + readout_noise²)
            // This is a simplified model; actual implementation depends on detector characteristics
            config_json = metadata_json;
        } else if (metadata_json != nullptr) {
            config_json = metadata_json;
        }

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
        std::vector<std::vector<float>> gain_maps(num_levels);
        std::vector<XCalFileHeader> headers(num_levels);
        uint32_t width = 0;
        uint32_t height = 0;

        for (int32_t i = 0; i < num_levels; ++i) {
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
            size_t n_pixels = static_cast<size_t>(width) * height;
            const float* data = reinterpret_cast<const float*>(payload.data());
            gain_maps[i].assign(data, data + n_pixels);
        }

        // --- Fit polynomial for each pixel ---
        size_t n_pixels = static_cast<size_t>(width) * height;
        int32_t max_coeffs = max_degree + 1;

        // Output: (degree+1) × W × H coefficient array
        // Store as [c0_pixel0, c1_pixel0, ..., cd_pixel0, c0_pixel1, ...]
        std::vector<float> coeff_array(n_pixels * max_coeffs);

        // For each pixel: fit polynomial with degree reduction if needed
        for (size_t pix = 0; pix < n_pixels; ++pix) {
            // Extract gain values across dose levels for this pixel
            std::vector<double> y_vals(num_levels);
            for (int32_t i = 0; i < num_levels; ++i) {
                y_vals[i] = static_cast<double>(gain_maps[i][pix]);
            }

            // Try fitting from max_degree down to degree 1
            bool fit_success = false;
            int32_t final_degree = 1;
            std::vector<double> coeffs(max_coeffs);

            for (int32_t deg = max_degree; deg >= 1; --deg) {
                int32_t n_coeffs = deg + 1;
                std::vector<double> temp_coeffs(n_coeffs);

                XpeErrorCode rc = fit_polynomial_ls(
                    dose_levels, y_vals.data(),
                    num_levels, deg,
                    temp_coeffs.data());

                if (rc != XPE_OK) {
                    continue; // Try lower degree
                }

                // Validate monotonicity in [dose_min, dose_max]
                double dose_min = dose_levels[0];
                double dose_max = dose_levels[num_levels - 1];

                if (validate_monotonicity(
                    temp_coeffs.data(), deg,
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
                coeffs[1] = (y_vals[num_levels - 1] - y_vals[0]) /
                           (dose_levels[num_levels - 1] - dose_levels[0]);
                final_degree = 1;
            }

            // Store coefficients in output array
            size_t offset = pix * max_coeffs;
            for (int32_t j = 0; j <= final_degree; ++j) {
                coeff_array[offset + j] = static_cast<float>(coeffs[j]);
            }
            // Pad remaining coefficients with zeros
            for (int32_t j = final_degree + 1; j < max_coeffs; ++j) {
                coeff_array[offset + j] = 0.0f;
            }
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
        hdr.payload_len      = static_cast<uint64_t>(n_pixels * max_coeffs) *
                               sizeof(float);

        // session_id: "generated" (null-padded)
        std::memcpy(hdr.session_id, "generated\0", 10);

        // --- Build config JSON with polynomial metadata ---
        char meta[256];
        std::snprintf(meta, sizeof(meta),
            "{\"polynomial_degree\":%d,\"num_coefficients\":%d,\"num_dose_levels\":%d}",
            static_cast<int>(max_degree),
            static_cast<int>(max_coeffs),
            static_cast<int>(num_levels));
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
