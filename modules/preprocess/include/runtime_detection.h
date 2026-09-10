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
 * @brief Default neighbourhood size (3x3 pixels, centre excluded -> 8 values).
 *
 * QA-A-42 (#143): SPEC-XPE-P1A REQ-P1A-013, algorithm step 1, verbatim:
 *
 *   "For each pixel p(x,y), compute local median m(x,y) over 3x3 neighborhood
 *    excluding center (8 values)"
 *
 * This was 5 with the centre INCLUDED (25 values), which diverged from the SPEC
 * in two ways at once. Including the centre is the more consequential half: a
 * defective pixel contributes to the median and the MAD it is then compared
 * against, pulling both toward itself and masking the defect.
 */
#define RUNTIME_DETECTION_DEFAULT_WINDOW_SIZE 3

/**
 * @brief Minimum neighbours required before a pixel is judged.
 *
 * REQ-P1A-013 Pixel Accuracy, verbatim: "Edge-of-image pixels (where 3x3
 * neighborhood is incomplete): processed with available subset; at least 5
 * neighbors required or pixel is skipped (defectMapOut = 0)".
 *
 * Load-bearing only now: with the old 5x5 window even a corner had 8 remaining
 * samples, so the rule never bit. Under 3x3-excluding-centre a corner has 3.
 */
#define RUNTIME_DETECTION_MIN_NEIGHBORS 5

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
    size_t mid = n / 2u;

    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(mid), values.end());

    if (n % 2u == 0u) {
        // Even number of elements: average of two middle values
        float median1 = values[mid];
        float median2 = *std::max_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(mid));
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
            outValues.push_back(pixels[static_cast<uint32_t>(y) * img->width + static_cast<uint32_t>(x)]);
        }
    }
}

/**
 * @brief Collect the neighbourhood of a pixel, EXCLUDING the centre.
 *
 * REQ-P1A-013 step 1 counts 8 values for a 3x3 neighbourhood, which is the
 * window minus its own centre. Edge pixels get the available subset, per the
 * Pixel Accuracy clause.
 *
 * @param img Input image
 * @param centerX Centre pixel X coordinate
 * @param centerY Centre pixel Y coordinate
 * @param windowSize Window size (must be odd)
 * @param[out] outValues Collected neighbour values, centre omitted
 */
inline void CollectNeighborValues(const XpeImageBuffer* img,
                                  uint32_t centerX,
                                  uint32_t centerY,
                                  int32_t windowSize,
                                  std::vector<float>& outValues) {
    outValues.clear();

    const int32_t halfWindow = windowSize / 2;
    int32_t startX = std::max(0, static_cast<int32_t>(centerX) - halfWindow);
    int32_t startY = std::max(0, static_cast<int32_t>(centerY) - halfWindow);
    int32_t endX = std::min(static_cast<int32_t>(img->width) - 1,
                            static_cast<int32_t>(centerX) + halfWindow);
    int32_t endY = std::min(static_cast<int32_t>(img->height) - 1,
                            static_cast<int32_t>(centerY) + halfWindow);

    const float* pixels = static_cast<const float*>(img->data);
    for (int32_t y = startY; y <= endY; ++y) {
        for (int32_t x = startX; x <= endX; ++x) {
            if (static_cast<uint32_t>(x) == centerX &&
                static_cast<uint32_t>(y) == centerY) {
                continue;  // the centre is the sample under test, not a neighbour
            }
            outValues.push_back(
                pixels[static_cast<uint32_t>(y) * img->width + static_cast<uint32_t>(x)]);
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
    // QA-A-42 (#143): neighbours only, per REQ-P1A-013 step 1.
    std::vector<float> windowValues;
    CollectNeighborValues(img, x, y, config.windowSize, windowValues);

    // REQ-P1A-013: "at least 5 neighbors required or pixel is skipped".
    if (windowValues.size() < RUNTIME_DETECTION_MIN_NEIGHBORS) return false;

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
