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
 * @brief Fraction of the frame-wide robust sigma used as a per-pixel floor.
 *
 * QA-A-43 (#143): the SPEC algorithm clause (3x3 excluding centre, 8 values)
 * and the SPEC Pixel Accuracy clause (clean input flags <= 1% of the frame)
 * could not both hold without this. Eight samples estimate sigma coarsely, and
 * every under-estimate becomes a false flag: measured 1.72% of a clean 1024x1024
 * frame, against a 1% ceiling. Flooring the local estimate at 0.8x the
 * frame-wide robust sigma brings it to 1.06e-4 -- 1/94 of the ceiling -- while
 * keeping the SPEC's neighbourhood rule and the same runtime.
 *
 * 0.8 rather than 1.0: at 1.0 the floor dominates the local estimate almost
 * everywhere and TPR at 5 sigma drops from 0.64 to 0.38 (QA-A-41/42 sweeps).
 * 0.8 keeps 91% of the detection rate. Evidence:
 * .moai/reports/lane-pre/QA-A-43/.
 *
 * A floor of 0 disables the mechanism, which is the default for
 * RuntimeDetectionConfig so that direct callers see the unfloored rule.
 */
#define RUNTIME_DETECTION_GLOBAL_SIGMA_FLOOR 0.8f

/**
 * @brief Multiple of the frame-wide robust sigma used as a per-pixel CEILING.
 *
 * QA-A-46 (#143), the mirror of RUNTIME_DETECTION_GLOBAL_SIGMA_FLOOR.
 *
 * QA-A-45 examined every injected 10-sigma transient the detector missed and
 * found one cause for all thirteen: the local MAD, built from eight samples,
 * came out 1.60x to 2.63x the true sigma at those sites, so kappa * sigma rose
 * above the transient. Not clustering, not a truncated neighbourhood, not
 * saturation -- all three measured zero. The floor cannot help, because the
 * floor bounds the estimate from BELOW and these sites strayed ABOVE. A ceiling
 * is the matching device, and it costs nothing extra: the frame-wide sigma is
 * already computed for the floor, so this adds one comparison per pixel.
 *
 * DISABLED BY DEFAULT (0.0f). A ceiling lowers the threshold wherever the local
 * estimate is high, so it trades false negatives for false positives -- and
 * QA-A-43 spent a whole card getting the false-positive rate under the SPEC's
 * 1% ceiling. Enabling this without measuring that trade would undo it.
 * QA-A-46's table measures it; the value stays 0 until someone decides on the
 * evidence. Bound from the QA-A-45 data: catching all thirteen needs
 * beta < 78.76 / (5 * 10.023838) = 1.5714.
 */
#define RUNTIME_DETECTION_GLOBAL_SIGMA_CAP 0.0f

/**
 * @brief Configuration parameters for runtime detection.
 */
struct RuntimeDetectionConfig {
    int32_t windowSize;       /**< Sliding window size (odd number: 3, 5, 7, ...) */
    float sigmaThreshold;     /**< Sigma threshold for outlier detection (default: 5.0) */
    float globalSigmaFloor;   /**< Lower bound on the local sigma estimate; 0 = none */
    float globalSigmaCap;     /**< Upper bound on the local sigma estimate; 0 = none */
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
    // 0 by default: the floor is a frame-wide quantity, so only a caller that
    // has seen the whole frame can fill it in. xpe_defect_detect_runtime does.
    config.globalSigmaFloor = 0.0f;
    config.globalSigmaCap = 0.0f;
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
 * @brief Frame-wide robust noise sigma, estimated from adjacent-pixel differences.
 *
 * QA-A-48 (#148). This used to be the MAD of the pixel VALUES. On a frame with
 * no structure that equals the noise, which is why it looked correct for four
 * cards. On a frame WITH structure it measures the structure: QA-A-47 measured
 * 24.95x the true noise on a brightness ramp and 164.51x across an intensity
 * step, which drove the floor so high that every injected 10-sigma defect was
 * missed -- silently, with an empty defect map and no error.
 *
 * The fix is to difference before measuring. A difference of two neighbours
 * cancels whatever the signal does smoothly at the one-pixel scale and leaves
 * the difference of two independent noise draws:
 *
 *     d = I(x+1,y) - I(x,y) = [s(x+1,y) - s(x,y)] + [n1 - n2]
 *     Var(n1 - n2) = 2 * sigma^2      ->  sd(d) = sqrt(2) * sigma
 *     sigma_hat    = MAD(d) * 1.4826 / sqrt(2)
 *
 * Both constants are load-bearing and neither is cosmetic:
 *   - 1.4826 = 1 / Phi^-1(0.75) converts a MAD to a Gaussian sigma
 *     (RUNTIME_DETECTION_MAD_SCALE, already used for the local estimate).
 *   - sqrt(2) undoes the variance doubling above. Omitting it overestimates
 *     sigma by 41%, which raises every threshold by 41% and loses detections --
 *     and nothing else in the system would look wrong.
 *
 * Horizontal and vertical differences are measured separately and the SMALLER
 * is taken. A row artefact (grid lines, a detector row) inflates the vertical
 * statistic and leaves the horizontal one clean; a column artefact does the
 * reverse. Taking the minimum picks whichever direction the structure did not
 * corrupt. Measured on the QA-A-47 simulations (value-MAD -> this estimator):
 *
 *     uniform  1.00x -> 1.00x      scatter  24.95x -> 0.99x
 *     lines    1.19x -> 1.00x      edge    164.51x -> 1.39x
 *
 * The residual 1.39x on the step frame is not an estimator defect: that frame
 * genuinely has two noise levels (12 and 25) and no single global number
 * represents both.
 *
 * Cost: two selection passes over one difference array. Peak memory is one
 * array of width*height floats -- the same as the copy the previous version
 * made. Returns 0 for an empty or malformed frame, which disables the floor
 * rather than fabricating one.
 */
inline float ComputeGlobalSigma(const XpeImageBuffer* img) {
    if (img == nullptr || img->data == nullptr) return 0.0f;
    const size_t w = img->width;
    const size_t h = img->height;
    if (w < 2u && h < 2u) return 0.0f;

    const float* pixels = static_cast<const float*>(img->data);

    // MAD of a difference array, already converted to a sigma.
    auto madSigma = [](std::vector<float>& d) -> float {
        if (d.empty()) return 0.0f;
        const size_t mid = d.size() / 2u;
        std::nth_element(d.begin(), d.begin() + static_cast<std::ptrdiff_t>(mid), d.end());
        const float median = d[mid];
        for (size_t i = 0; i < d.size(); ++i) d[i] = std::abs(d[i] - median);
        std::nth_element(d.begin(), d.begin() + static_cast<std::ptrdiff_t>(mid), d.end());
        // 1.4826 : MAD -> sigma.   1/sqrt(2) : undo Var(n1 - n2) = 2 sigma^2.
        return d[mid] * RUNTIME_DETECTION_MAD_SCALE * 0.70710678f;
    };

    std::vector<float> diff;
    diff.reserve(w * h);

    float sigmaH = 0.0f;
    if (w >= 2u) {
        for (size_t y = 0; y < h; ++y) {
            const float* row = pixels + y * w;
            for (size_t x = 0; x + 1u < w; ++x) diff.push_back(row[x + 1u] - row[x]);
        }
        sigmaH = madSigma(diff);
    }

    float sigmaV = 0.0f;
    if (h >= 2u) {
        diff.clear();
        for (size_t y = 0; y + 1u < h; ++y) {
            const float* row = pixels + y * w;
            for (size_t x = 0; x < w; ++x) diff.push_back(row[x + w] - row[x]);
        }
        sigmaV = madSigma(diff);
    }

    if (sigmaH <= 0.0f) return sigmaV;
    if (sigmaV <= 0.0f) return sigmaH;
    return (sigmaH < sigmaV) ? sigmaH : sigmaV;
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
                                 const RuntimeDetectionConfig& config,
                                 std::vector<float>& windowValues,
                                 std::vector<float>& deviations) {
    // QA-A-42 (#143): neighbours only, per REQ-P1A-013 step 1.
    CollectNeighborValues(img, x, y, config.windowSize, windowValues);

    // REQ-P1A-013: "at least 5 neighbors required or pixel is skipped".
    if (windowValues.size() < RUNTIME_DETECTION_MIN_NEIGHBORS) return false;

    // Compute median
    float median = ComputeMedian(windowValues);

    // Compute MAD (Median Absolute Deviation)
    deviations.assign(windowValues.begin(), windowValues.end());
    float mad = ComputeMAD(deviations, median);

    // Get center pixel value
    const float* pixels = static_cast<const float*>(img->data);
    float centerValue = pixels[y * img->width + x];

    // QA-A-43 (#143): floor the local estimate at a fraction of the frame-wide
    // robust sigma. Eight samples under-estimate sigma often enough to breach
    // the SPEC's 1% clean-input ceiling; the floor removes those flags without
    // touching the neighbourhood rule. Zero floor = mechanism off.
    float sigmaEstimate = mad;
    if (config.globalSigmaFloor > sigmaEstimate) {
        sigmaEstimate = config.globalSigmaFloor;
    }
    // QA-A-46 (#143): and cap it from above. Disabled (0) unless a caller sets
    // it -- see RUNTIME_DETECTION_GLOBAL_SIGMA_CAP for why the default is off.
    if (config.globalSigmaCap > 0.0f && sigmaEstimate > config.globalSigmaCap) {
        sigmaEstimate = config.globalSigmaCap;
    }

    // Flat-field windows produce MAD == 0. In that case, any non-trivial
    // deviation from the local median is an outlier rather than noise.
    if (sigmaEstimate < 1e-6f) {
        return std::abs(centerValue - median) > 1e-6f;
    }

    // Hampel identifier test
    float deviation = std::abs(centerValue - median);
    float threshold = config.sigmaThreshold * sigmaEstimate;

    return deviation > threshold;
}

/**
 * @brief Allocating convenience form of DetectDefectivePixel.
 *
 * QA-A-42 (#144): the buffer-taking overload above exists because these two
 * vectors were being constructed and destroyed once per pixel. Measured on a
 * 1024x1024 frame with the old 5x5 window: 706 ms with per-pixel allocation
 * against 412 ms with reused buffers -- 42% of the run was allocator traffic,
 * with no change to the rule. Hot loops take the overload; one-off callers and
 * tests keep this form.
 */
inline bool DetectDefectivePixel(const XpeImageBuffer* img,
                                 uint32_t x,
                                 uint32_t y,
                                 const RuntimeDetectionConfig& config) {
    std::vector<float> windowValues;
    std::vector<float> deviations;
    return DetectDefectivePixel(img, x, y, config, windowValues, deviations);
}

} // namespace internal
} // namespace preprocess
} // namespace xpe

#endif /* RUNTIME_DETECTION_H */
