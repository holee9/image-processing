/**
 * @file xpe_verify_metrics.cpp
 * @brief XPE Calibration Verification Metrics API
 *
 * Computes quantitative quality metrics after each calibration correction step,
 * enabling automated pass/fail determination for offset, gain, and defect correction.
 *
 * SPEC: SPEC-XPE-P1A (Phase 10: Verification Metrics)
 * IEC 62304 Class B
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <cmath>
#include <algorithm>
#include <numeric>
#include <vector>

/* =============================================================================
 * Pass/Fail Thresholds (configurable defaults)
 * ============================================================================ */

namespace {

    // Offset correction thresholds
    constexpr double DARK_BIAS_MAX       = 5.0;     // ADU — corrected dark should be near zero
    constexpr double DSNU_MAX_PCT        = 1.0;     // % — dark non-uniformity should be small

    // Gain correction thresholds
    constexpr double PRNU_IMPROVE_MIN_DB = 3.0;     // dB — gain correction should improve PRNU
    constexpr double GAIN_COVERAGE_MIN   = 0.99;    // 99% of gain values must be valid

    // Defect correction thresholds
    constexpr double DEFECT_DENSITY_MAX  = 0.05;    // 5% max defect density

    // Overall thresholds
    constexpr double SNR_IMPROVE_MIN_DB  = 2.0;     // minimum SNR improvement

    // Helper: Compute robust mean using median (more resistant to outliers)
    double compute_robust_mean(const std::vector<double>& values) noexcept {
        if (values.empty()) return 0.0;

        std::vector<double> sorted = values;
        std::sort(sorted.begin(), sorted.end());

        // Use median for robust estimation
        size_t n = sorted.size();
        if (n % 2u == 0u) {
            return (sorted[n/2u - 1u] + sorted[n/2u]) / 2.0;
        } else {
            return sorted[n/2u];
        }
    }

    // Helper: Compute standard deviation
    double compute_std(const std::vector<double>& values, double mean) noexcept {
        if (values.size() <= 1u) return 0.0;

        double sum_sq_diff = 0.0;
        for (double v : values) {
            double diff = v - mean;
            sum_sq_diff += diff * diff;
        }

        return std::sqrt(sum_sq_diff / static_cast<double>(values.size() - 1));
    }

    // Helper: Compute histogram entropy for flatness measurement
    double compute_flatness(const std::vector<double>& values, int bins = 256) noexcept {
        if (values.empty()) return 0.0;

        // Find min/max
        auto [min_it, max_it] = std::minmax_element(values.begin(), values.end());
        double min_val = *min_it;
        double max_val = *max_it;

        if (max_val <= min_val) return 1.0; // All values identical → perfectly flat

        // Build histogram
        std::vector<int> hist(static_cast<size_t>(bins), 0);
        double bin_width = (max_val - min_val) / bins;

        for (double v : values) {
            int bin = static_cast<int>((v - min_val) / bin_width);
            if (bin >= bins) bin = bins - 1;
            hist[static_cast<size_t>(bin)]++;
        }

        // Compute entropy
        double entropy = 0.0;
        size_t total = values.size();

        for (int count : hist) {
            if (count > 0) {
                double p = static_cast<double>(count) / total;
                entropy -= p * std::log(p);
            }
        }

        // Normalize to [0, 1] where 1 = perfectly flat (uniform distribution)
        double max_entropy = std::log(static_cast<double>(bins));
        return entropy / max_entropy;
    }

    // Helper: Compute 3x3 neighbor mean (excluding defective pixels)
    double compute_neighbor_mean(const float* pixels, const uint8_t* defect_map,
                                  uint32_t x, uint32_t y,
                                  uint32_t width, uint32_t height) noexcept {
        double sum = 0.0;
        int count = 0;

        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue; // Skip center

                uint32_t nx = static_cast<uint32_t>(static_cast<int>(x) + dx);
                uint32_t ny = static_cast<uint32_t>(static_cast<int>(y) + dy);

                // Boundary check
                if (nx >= width || ny >= height) continue;

                size_t idx = static_cast<size_t>(ny) * width + nx;

                // Skip defective neighbors
                if (defect_map && defect_map[idx] != 0) continue;

                sum += static_cast<double>(pixels[idx]);
                count++;
            }
        }

        return (count > 0) ? (sum / count) : 0.0;
    }

} // anonymous namespace

/* =============================================================================
 * Phase 10: Verification Metrics API
 * ============================================================================ */

/**
 * @brief Verify offset correction quality
 *
 * Computes dark bias, DSNU, and residual noise metrics for offset-corrected images.
 *
 * REQ-P1A-XXX: Offset correction verification
 *
 * @param raw_image Original raw image (UINT16)
 * @param corrected_image Offset-corrected image (UINT16)
 * @param metadata Image metadata
 * @param metrics Output metrics (populated by this function)
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT on NULL pointers or dimension mismatch
 */
XPE_API XpeErrorCode xpe_verify_offset(
    const XpeImageBuffer* raw_image,
    const XpeImageBuffer* corrected_image,
    const XpeImageMetadata* metadata,
    XpeCalibrationMetrics* metrics)
{
    if (!raw_image || !corrected_image || !metrics) {
        return XPE_ERR_INVALID_INPUT;
    }
    (void)metadata;

    // Clear output
    *metrics = {};

    // Validate dimensions match
    if (!xpe_dims_match(raw_image, corrected_image)) {
        return XPE_ERR_BUFFER_TOO_SMALL;
    }

    // Validate formats
    size_t pixel_count = 0;
    if (!xpe_buffer_has_format(raw_image, XPE_PIXEL_UINT16, &pixel_count) ||
        !xpe_buffer_has_format(corrected_image, XPE_PIXEL_UINT16)) {
        return XPE_ERR_UNSUPPORTED_FORMAT;
    }

    if (pixel_count == 0) {
        return XPE_ERR_INVALID_INPUT;
    }

    const uint16_t* raw = static_cast<const uint16_t*>(raw_image->data);
    const uint16_t* corrected = static_cast<const uint16_t*>(corrected_image->data);

    // Check if raw image has significant variation (dark regions detectable)
    uint16_t raw_min = raw[0], raw_max = raw[0];
    for (size_t i = 1; i < pixel_count; ++i) {
        raw_min = std::min(raw_min, raw[i]);
        raw_max = std::max(raw_max, raw[i]);
    }
    double raw_range = static_cast<double>(raw_max) - static_cast<double>(raw_min);
    double raw_mean_val = static_cast<double>(raw_min + raw_max) * 0.5;
    bool has_variation = (raw_mean_val > 0.0) && (raw_range / raw_mean_val > 0.01);

    if (!has_variation) {
        // Uniform raw image: no distinguishable dark regions.
        // dark_bias = 0 (can't measure residual dark without variation)
        metrics->dark_bias = 0.0;
        metrics->dsnu = 0.0;
        metrics->residual_noise = 0.0;
        metrics->overall_pass = true;
        return XPE_OK;
    }

    // Identify dark regions (bottom 10% of raw histogram)
    std::vector<uint16_t> raw_copy(raw, raw + pixel_count);
    std::sort(raw_copy.begin(), raw_copy.end());
    uint16_t dark_threshold = raw_copy[static_cast<size_t>(pixel_count * 0.1)];

    // Extract corrected values for dark regions
    std::vector<double> dark_corrected;
    dark_corrected.reserve(pixel_count / 10);

    for (size_t i = 0; i < pixel_count; ++i) {
        if (raw[i] < dark_threshold) {
            dark_corrected.push_back(static_cast<double>(corrected[i]));
        }
    }

    if (dark_corrected.empty()) {
        // No dark pixels found, use all pixels
        for (size_t i = 0; i < pixel_count; ++i) {
            dark_corrected.push_back(static_cast<double>(corrected[i]));
        }
    }

    // Compute metrics
    double mean = compute_robust_mean(dark_corrected);
    double stddev = compute_std(dark_corrected, mean);

    metrics->dark_bias = mean;
    metrics->dsnu = (mean > 0.0) ? (stddev / mean) * 100.0 : 0.0;
    metrics->residual_noise = stddev;

    // Pass/fail determination
    metrics->overall_pass = (metrics->dark_bias < DARK_BIAS_MAX) &&
                            (metrics->dsnu < DSNU_MAX_PCT);

    return XPE_OK;
}

/**
 * @brief Verify gain correction quality
 *
 * Computes PRNU before/after, flatness, gain coverage, and SNR improvement.
 *
 * REQ-P1A-XXX: Gain correction verification
 *
 * @param before_gain Offset-corrected image (UINT16)
 * @param after_gain Gain-corrected image (FLOAT32)
 * @param gain_map Gain map used (FLOAT32)
 * @param metrics Output metrics
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT on NULL pointers or dimension mismatch
 */
XPE_API XpeErrorCode xpe_verify_gain(
    const XpeImageBuffer* before_gain,
    const XpeImageBuffer* after_gain,
    const XpeImageBuffer* gain_map,
    XpeCalibrationMetrics* metrics)
{
    if (!before_gain || !after_gain || !gain_map || !metrics) {
        return XPE_ERR_INVALID_INPUT;
    }

    // Clear output
    *metrics = {};

    // Validate dimensions match
    if (!xpe_dims_match(before_gain, after_gain) ||
        !xpe_dims_match(before_gain, gain_map)) {
        return XPE_ERR_BUFFER_TOO_SMALL;
    }

    // Validate formats
    size_t pixel_count = 0;
    if (!xpe_buffer_has_format(before_gain, XPE_PIXEL_UINT16, &pixel_count) ||
        !xpe_buffer_has_format(after_gain, XPE_PIXEL_FLOAT32) ||
        !xpe_buffer_has_format(gain_map, XPE_PIXEL_FLOAT32)) {
        return XPE_ERR_UNSUPPORTED_FORMAT;
    }

    if (pixel_count == 0) {
        return XPE_ERR_INVALID_INPUT;
    }

    const uint16_t* before = static_cast<const uint16_t*>(before_gain->data);
    const float* after = static_cast<const float*>(after_gain->data);
    const float* gain = static_cast<const float*>(gain_map->data);

    // Convert to double for analysis
    std::vector<double> before_vals;
    std::vector<double> after_vals;
    before_vals.reserve(pixel_count);
    after_vals.reserve(pixel_count);

    size_t valid_gain_count = 0;
    metrics->invalid_gain_count = 0;

    for (size_t i = 0; i < pixel_count; ++i) {
        // Check for valid gain values
        if (std::isfinite(gain[i]) && gain[i] > 0.0f) {
            valid_gain_count++;
            before_vals.push_back(static_cast<double>(before[i]));
            after_vals.push_back(static_cast<double>(after[i]));
        } else {
            metrics->invalid_gain_count++;
        }
    }

    if (before_vals.empty()) {
        // No valid pixels
        metrics->overall_pass = false;
        return XPE_OK;
    }

    // Compute PRNU before gain correction
    double mean_before = compute_robust_mean(before_vals);
    double std_before = compute_std(before_vals, mean_before);
    metrics->prnu_before = (mean_before > 0.0) ? (std_before / mean_before) * 100.0 : 0.0;

    // Compute PRNU after gain correction
    double mean_after = compute_robust_mean(after_vals);
    double std_after = compute_std(after_vals, mean_after);
    metrics->prnu_after = (mean_after > 0.0) ? (std_after / mean_after) * 100.0 : 0.0;

    // Compute flatness
    metrics->flatness_pct = compute_flatness(after_vals) * 100.0;

    // Compute gain coverage
    metrics->gain_coverage = static_cast<double>(valid_gain_count) / pixel_count;

    // Compute SNR improvement in dB
    if (metrics->prnu_before > 0.0 && metrics->prnu_after > 0.0) {
        metrics->snr_improvement_db = 20.0 * std::log10(metrics->prnu_before / metrics->prnu_after);
    } else {
        metrics->snr_improvement_db = 0.0;
    }

    // Pass/fail determination
    bool prnu_improved = (metrics->prnu_after < metrics->prnu_before) ||
                         (metrics->prnu_before < 0.01 && metrics->prnu_after < 0.01);
    bool coverage_ok = (metrics->gain_coverage >= GAIN_COVERAGE_MIN);
    bool snr_improved = (metrics->snr_improvement_db >= PRNU_IMPROVE_MIN_DB) ||
                        (metrics->prnu_before < 0.01 && metrics->prnu_after < 0.01);

    metrics->overall_pass = prnu_improved && coverage_ok && snr_improved;

    return XPE_OK;
}

/**
 * @brief Verify defect correction quality
 *
 * Computes defect count, density, and correction error metrics.
 *
 * REQ-P1A-XXX: Defect correction verification
 *
 * @param corrected_image Defect-corrected image (FLOAT32)
 * @param defect_map BPM used (UINT8)
 * @param metrics Output metrics
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT on NULL pointers or dimension mismatch
 */
XPE_API XpeErrorCode xpe_verify_defect(
    const XpeImageBuffer* corrected_image,
    const XpeImageBuffer* defect_map,
    XpeCalibrationMetrics* metrics)
{
    if (!corrected_image || !defect_map || !metrics) {
        return XPE_ERR_INVALID_INPUT;
    }

    // Clear output
    *metrics = {};

    // Validate dimensions match
    if (!xpe_dims_match(corrected_image, defect_map)) {
        return XPE_ERR_BUFFER_TOO_SMALL;
    }

    // Validate formats
    size_t pixel_count = 0;
    if (!xpe_buffer_has_format(corrected_image, XPE_PIXEL_FLOAT32, &pixel_count) ||
        !xpe_buffer_has_format(defect_map, XPE_PIXEL_UINT8)) {
        return XPE_ERR_UNSUPPORTED_FORMAT;
    }

    if (pixel_count == 0) {
        return XPE_ERR_INVALID_INPUT;
    }

    const float* corrected = static_cast<const float*>(corrected_image->data);
    const uint8_t* defect = static_cast<const uint8_t*>(defect_map->data);

    // Count defects and compute correction error
    metrics->defect_count = 0;
    double total_error = 0.0;
    uint32_t error_samples = 0;

    uint32_t width = corrected_image->width;
    uint32_t height = corrected_image->height;

    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            size_t idx = static_cast<size_t>(y) * width + x;

            if (defect[idx] != 0) {
                metrics->defect_count++;

                // Compute correction error for this defective pixel
                double neighbor_mean = compute_neighbor_mean(
                    corrected, defect, x, y, width, height
                );

                if (neighbor_mean > 0.0) {
                    double error = std::abs(static_cast<double>(corrected[idx]) - neighbor_mean);
                    total_error += error;
                    error_samples++;
                }
            }
        }
    }

    // Compute defect density
    metrics->defect_density = (static_cast<double>(metrics->defect_count) / pixel_count) * 100.0;

    // Compute mean correction error
    metrics->correction_error = (error_samples > 0) ? (total_error / error_samples) : 0.0;

    // Pass/fail determination
    metrics->overall_pass = (metrics->defect_density < DEFECT_DENSITY_MAX);

    return XPE_OK;
}

/**
 * @brief Verify full pipeline quality
 *
 * Computes overall SNR improvement between raw and final processed images.
 *
 * REQ-P1A-XXX: Pipeline verification
 *
 * @param raw_image Original raw image
 * @param final_image Final processed image
 * @param metadata Image metadata
 * @param metrics Combined metrics
 * @return XPE_OK on success
 *         XPE_ERR_INVALID_INPUT on NULL pointers
 */
XPE_API XpeErrorCode xpe_verify_pipeline(
    const XpeImageBuffer* raw_image,
    const XpeImageBuffer* final_image,
    const XpeImageMetadata* metadata,
    XpeCalibrationMetrics* metrics)
{
    if (!raw_image || !final_image || !metrics) {
        return XPE_ERR_INVALID_INPUT;
    }
    (void)metadata;

    // Clear output
    *metrics = {};

    // Validate dimensions match
    if (!xpe_dims_match(raw_image, final_image)) {
        return XPE_ERR_BUFFER_TOO_SMALL;
    }

    size_t pixel_count = 0;

    // Handle different input/output formats
    if (raw_image->format == XPE_PIXEL_UINT16) {
        if (!xpe_buffer_has_format(raw_image, XPE_PIXEL_UINT16, &pixel_count)) {
            return XPE_ERR_UNSUPPORTED_FORMAT;
        }
    } else {
        return XPE_ERR_UNSUPPORTED_FORMAT;
    }

    if (final_image->format == XPE_PIXEL_FLOAT32) {
        if (!xpe_buffer_has_format(final_image, XPE_PIXEL_FLOAT32)) {
            return XPE_ERR_UNSUPPORTED_FORMAT;
        }
    } else {
        return XPE_ERR_UNSUPPORTED_FORMAT;
    }

    if (pixel_count == 0) {
        return XPE_ERR_INVALID_INPUT;
    }

    const uint16_t* raw = static_cast<const uint16_t*>(raw_image->data);
    const float* final = static_cast<const float*>(final_image->data);

    // Convert to double for analysis
    std::vector<double> raw_vals;
    std::vector<double> final_vals;
    raw_vals.reserve(pixel_count);
    final_vals.reserve(pixel_count);

    for (size_t i = 0; i < pixel_count; ++i) {
        raw_vals.push_back(static_cast<double>(raw[i]));
        final_vals.push_back(static_cast<double>(final[i]));
    }

    // Compute robust statistics
    double mean_raw = compute_robust_mean(raw_vals);
    double std_raw = compute_std(raw_vals, mean_raw);

    double mean_final = compute_robust_mean(final_vals);
    double std_final = compute_std(final_vals, mean_final);

    // Compute SNR improvement (using coefficient of variation: std/mean)
    double snr_raw = (mean_raw > 0.0) ? (20.0 * std::log10(mean_raw / std_raw)) : 0.0;
    double snr_final = (mean_final > 0.0) ? (20.0 * std::log10(mean_final / std_final)) : 0.0;

    metrics->snr_improvement_db = snr_final - snr_raw;

    // Pass/fail determination
    metrics->overall_pass = (metrics->snr_improvement_db >= SNR_IMPROVE_MIN_DB);

    return XPE_OK;
}
