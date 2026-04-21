/**
 * @file runtime_detection.h
 * @brief Runtime defective pixel detection using Hampel 5-sigma outlier detection.
 *
 * Implements REQ-P1A-013: Statistical outlier detection for transient defect pixels
 * that appear during operation (e.g., cosmic ray hits, temperature-dependent defects).
 *
 * Algorithm:
 * 1. For each pixel, collect values in a sliding window (default 5x5)
 * 2. Compute median of window values
 * 3. Compute MAD (Median Absolute Deviation): median(|x_i - median|)
 * 4. Flag defective if: |value - median| > 5 * (1.4826 * MAD)
 *
 * The constant 1.4826 scales MAD to match standard deviation for normal distributions.
 *
 * TDD: RED-GREEN-REFACTOR methodology
 * - RED: Failing Google Test suite written first (test_runtime_detection.cpp)
 * - GREEN: Minimal implementation to pass tests
 * - REFACTOR: Optimize sliding window while maintaining test coverage
 *
 * IEC 62304 Class B -- Unit tested with >= 85% coverage requirement.
 */

#ifndef RUNTIME_DETECTION_H
#define RUNTIME_DETECTION_H

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <vector>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Default sliding window size (5x5 pixels).
 *
 * Chosen to balance spatial localization with statistical robustness.
 * Larger windows improve statistical accuracy but reduce spatial resolution.
 */
#define RUNTIME_DETECTION_DEFAULT_WINDOW_SIZE 5

/**
 * @brief Default sigma threshold for outlier detection (5-sigma).
 *
 * 5-sigma corresponds to approximately 1 in 3.5 million false positives
 * for normally distributed data, meeting the FPR < 0.001% requirement.
 */
#define RUNTIME_DETECTION_DEFAULT_SIGMA_THRESHOLD 5.0

/**
 * @brief MAD-to-sigma scaling constant.
 *
 * For normally distributed data: MAD = sigma * 0.6745
 * Therefore: sigma = MAD / 0.6745 = MAD * 1.4826
 */
#define RUNTIME_DETECTION_MAD_SCALE 1.4826f

/**
 * @brief Configuration parameters for runtime detection.
 */
struct RuntimeDetectionConfig {
    int32_t windowSize;       /**< Sliding window size (odd number: 3, 5, 7, ...) */
    float sigmaThreshold;     /**< Sigma threshold for outlier detection (default: 5.0) */
};

/**
 * @brief Default configuration initializer.
 *
 * @return RuntimeDetectionConfig with default values (5x5 window, 5-sigma)
 */
inline RuntimeDetectionConfig RuntimeDetection_DefaultConfig() {
    RuntimeDetectionConfig config;
    config.windowSize = RUNTIME_DETECTION_DEFAULT_WINDOW_SIZE;
    config.sigmaThreshold = RUNTIME_DETECTION_DEFAULT_SIGMA_THRESHOLD;
    return config;
}

#ifdef __cplusplus
}
#endif

/* ============================================================================
 * Internal C++ Implementation (Namespace-protected)
 * ============================================================================ */

namespace xpe {
namespace preprocess {
namespace internal {

/**
 * @brief Compute median of floating-point values.
 *
 * Uses nth_element for O(n) average-case performance.
 *
 * @MX:ANCHOR: [AUTO] Median computation -- REQ-P1A-013
 * @MX:REASON: Core statistical operation; called by every pixel detection
 *
 * @param values Vector of values (modified during computation)
 * @return Median value
 */
inline float ComputeMedian(std::vector<float>& values) {
    if (values.empty()) return 0.0f;

    size_t n = values.size();
    size_t mid = n / 2;

    std::nth_element(values.begin(), values.begin() + mid, values.end());

    if (n % 2 == 0) {
        // Even number of elements: average of two middle values
        float median1 = values[mid];
        float median2 = *std::max_element(values.begin(), values.begin() + mid);
        return (median1 + median2) * 0.5f;
    } else {
        // Odd number of elements: middle value
        return values[mid];
    }
}

/**
 * @brief Compute Median Absolute Deviation (MAD).
 *
 * MAD = median(|x_i - median|)
 * Scaled to sigma: sigma = MAD * 1.4826
 *
 * @param values Vector of values (modified during computation)
 * @param median Pre-computed median of values
 * @return Scaled MAD (estimate of standard deviation)
 */
inline float ComputeMAD(std::vector<float>& values, float median) {
    if (values.empty()) return 0.0f;

    // Compute absolute deviations from median
    for (size_t i = 0; i < values.size(); ++i) {
        values[i] = std::abs(values[i] - median);
    }

    float mad = ComputeMedian(values);
    return mad * RUNTIME_DETECTION_MAD_SCALE;
}

/**
 * @brief Collect pixel values in sliding window.
 *
 * Handles boundary conditions by clampling window to image edges.
 *
 * @param img Input image
 * @param centerX Center pixel X coordinate
 * @param centerY Center pixel Y coordinate
 * @param windowSize Window size (must be odd)
 * @param[out] outValues Collected pixel values
 */
inline void CollectWindowValues(const XpeImageBuffer* img,
                                uint32_t centerX,
                                uint32_t centerY,
                                int32_t windowSize,
                                std::vector<float>& outValues) {
    outValues.clear();

    int32_t halfWindow = windowSize / 2;

    // Calculate window boundaries with clamping
    int32_t startX = static_cast<int32_t>(centerX) - halfWindow;
    int32_t startY = static_cast<int32_t>(centerY) - halfWindow;
    int32_t endX = static_cast<int32_t>(centerX) + halfWindow;
    int32_t endY = static_cast<int32_t>(centerY) + halfWindow;

    // Clamp to image bounds
    startX = std::max(0, startX);
    startY = std::max(0, startY);
    endX = std::min(static_cast<int32_t>(img->width) - 1, endX);
    endY = std::min(static_cast<int32_t>(img->height) - 1, endY);

    // Collect values
    const float* pixels = static_cast<const float*>(img->data);
    for (int32_t y = startY; y <= endY; ++y) {
        for (int32_t x = startX; x <= endX; ++x) {
            outValues.push_back(pixels[y * img->width + x]);
        }
    }
}

/**
 * @brief Detect defective pixel using Hampel 5-sigma filter.
 *
 * Algorithm:
 * 1. Collect values in sliding window around pixel
 * 2. Compute median of window
 * 3. Compute MAD (Median Absolute Deviation)
 * 4. Flag defective if: |value - median| > threshold * scaled_MAD
 *
 * @MX:NOTE: [AUTO] Hampel 5-sigma outlier detection -- REQ-P1A-013
 *          Robust to up to 50% outliers in window (median-based)
 *
 * @param img Input image (float32 format)
 * @param x Pixel X coordinate
 * @param y Pixel Y coordinate
 * @param config Detection configuration
 * @return true if pixel is defective, false otherwise
 */
inline bool DetectDefectivePixel(const XpeImageBuffer* img,
                                 uint32_t x,
                                 uint32_t y,
                                 const RuntimeDetectionConfig& config) {
    // Collect window values
    std::vector<float> windowValues;
    CollectWindowValues(img, x, y, config.windowSize, windowValues);

    if (windowValues.empty()) return false;

    // Compute median
    float median = ComputeMedian(windowValues);

    // Compute MAD (Median Absolute Deviation)
    std::vector<float> deviations = windowValues;  // Copy for MAD computation
    float mad = ComputeMAD(deviations, median);

    // Get center pixel value
    const float* pixels = static_cast<const float*>(img->data);
    float centerValue = pixels[y * img->width + x];

    // Flat-field windows produce MAD == 0. In that case, any non-trivial
    // deviation from the local median is an outlier rather than noise.
    if (mad < 1e-6f) {
        return std::abs(centerValue - median) > 1e-6f;
    }

    // Hampel identifier test
    float deviation = std::abs(centerValue - median);
    float threshold = config.sigmaThreshold * mad;

    return deviation > threshold;
}

} // namespace internal
} // namespace preprocess
} // namespace xpe

#endif /* RUNTIME_DETECTION_H */
