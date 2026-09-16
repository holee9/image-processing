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
#include <cstring>
#include <memory>
#include <thread>
#include <utility>
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
    /**
     * Number of worker threads. 1 (the default) keeps the single-threaded path.
     *
     * QA-A-61 (#144). This is an ARGUMENT, never module state: nothing here is
     * static, thread_local, or remembered between calls, so REQ-P1A-003
     * re-entrancy holds and two callers may run different thread counts at once.
     * The module never reads hardware concurrency -- the caller knows what else
     * is running in the pipeline and this module does not.
     *
     * Values below 1 are treated as 1.
     */
    int32_t threadCount;
};

/**
 * @brief Default configuration initializer.
 *
 * @return RuntimeDetectionConfig with default values (5x5 window, 5-sigma)
 */
inline RuntimeDetectionConfig RuntimeDetection_DefaultConfig() {
    RuntimeDetectionConfig config;
    config.threadCount = 1;
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
inline float ComputeMedianGeneric(std::vector<float>& values) {
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
 * @brief One compare-exchange: after it, a <= b. Branchless on MSVC (vminss/vmaxss).
 *
 * @param a First value; on return the smaller of the two.
 * @param b Second value; on return the larger of the two.
 */
inline void MedianSortCE(float& a, float& b) {
    const bool swap = (b < a);
    const float lo = swap ? b : a;
    const float hi = swap ? a : b;
    a = lo;
    b = hi;
}

/**
 * @brief Exact median of exactly 8 values, via a sorting network.
 *
 * QA-A-54 (#144). An interior pixel of the 3x3 window has exactly 8 neighbours,
 * so this is the shape the detector asks for on all but the frame border. The
 * generic path pays std::nth_element plus std::max_element -- two passes with
 * branches and, on MSVC, an insertion sort underneath for a range this small.
 * A Batcher odd-even network is 19 compare-exchanges with no data-dependent
 * branches and the whole array in registers.
 *
 * This is NOT an approximation. The network fully sorts the eight values, so
 * v[3] and v[4] are the same two order statistics the generic path selects
 * (values[mid] and the maximum of the lower half), and the returned expression
 * is the same sum times the same constant. For every input on which the generic
 * path is defined, this returns the identical float --
 * test_runtime_detection_median8_parity.cpp asserts it bit for bit.
 *
 * The caveat is NaN: a NaN makes `<` inconsistent, which already breaks
 * std::nth_element's strict-weak-ordering precondition, so the generic path is
 * undefined there rather than merely different. Neither path is trustworthy on
 * NaN input, and the parity claim is scoped to inputs where the old one was
 * defined.
 *
 * @param v Pointer to exactly 8 readable floats. Not modified; the network runs
 *          on copies held in registers.
 * @return The mean of the 4th and 5th smallest of the eight values, i.e.
 *         (v[3] + v[4]) * 0.5f after a full sort -- the even-count median
 *         convention this file uses everywhere.
 */
inline float MedianOfEight(const float* v) {
    float a0 = v[0], a1 = v[1], a2 = v[2], a3 = v[3];
    float a4 = v[4], a5 = v[5], a6 = v[6], a7 = v[7];

    MedianSortCE(a0, a1); MedianSortCE(a2, a3); MedianSortCE(a4, a5); MedianSortCE(a6, a7);
    MedianSortCE(a0, a2); MedianSortCE(a1, a3); MedianSortCE(a4, a6); MedianSortCE(a5, a7);
    MedianSortCE(a1, a2); MedianSortCE(a5, a6);
    MedianSortCE(a0, a4); MedianSortCE(a1, a5); MedianSortCE(a2, a6); MedianSortCE(a3, a7);
    MedianSortCE(a2, a4); MedianSortCE(a3, a5);
    MedianSortCE(a1, a2); MedianSortCE(a3, a4); MedianSortCE(a5, a6);

    return (a3 + a4) * 0.5f;
}

/**
 * @brief Median, with an exact fast path for the 8-value case.
 *
 * QA-A-54 (#144): the two selections per pixel (median, then median of the
 * absolute deviations) measured 91.7% of the per-pixel loop at 3072x3072, which
 * is itself 77% of the shipped entry point. This dispatch is the whole change --
 * the rule, the window, the threshold and the sigma floor are untouched, so the
 * QA-A-42..A-50 tables stay valid.
 *
 * Note the fast path does NOT permute the input, where the generic one does.
 * No caller depends on that side effect (every one copies first or discards),
 * and std::nth_element's own post-state is unspecified beyond the k-th element.
 *
 * @param values Values to take the median of. MAY BE REORDERED: the generic path
 *               partitions in place, the 8-value fast path leaves it untouched.
 *               Treat the order afterwards as unspecified either way.
 * @return The median: the middle value for an odd count, the mean of the two
 *         middle values for an even count, and 0.0f for an empty input.
 */
inline float ComputeMedian(std::vector<float>& values) {
    if (values.size() == 8u) return MedianOfEight(values.data());
    return ComputeMedianGeneric(values);
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
/**
 * @brief Order-preserving map from a float to a uint32 sort key.
 *
 * QA-A-55 (#144). For every non-NaN pair a, b:  a < b  <=>  key(a) < key(b).
 * Positives keep their bit pattern with the sign bit set; negatives are
 * inverted, which reverses their descending bit order into ascending.
 *
 * ONE deliberate difference from float comparison: -0.0 and +0.0 compare EQUAL
 * as floats but map to DIFFERENT keys, with -0.0 ordered first. Where that can
 * matter is argued at the call site (SelectKthSmallest) -- it does not change
 * any value this file returns.
 *
 * @param f Value to map. NaN is out of scope: it has no consistent order.
 * @return The sort key. For non-NaN a, b: a < b if and only if
 *         FloatSortKey(a) < FloatSortKey(b), with -0.0 ordered before +0.0.
 */
inline uint32_t FloatSortKey(float f) {
    uint32_t bits = 0u;
    std::memcpy(&bits, &f, sizeof(bits));
    return (bits & 0x80000000u) ? ~bits : (bits | 0x80000000u);
}

/**
 * @brief Inverse of FloatSortKey.
 *
 * @param key A key previously produced by FloatSortKey.
 * @return The float that produced it, bit for bit.
 */
inline float SortKeyToFloat(uint32_t key) {
    const uint32_t bits = (key & 0x80000000u) ? (key & 0x7FFFFFFFu) : ~key;
    float f = 0.0f;
    std::memcpy(&f, &bits, sizeof(f));
    return f;
}

/**
 * @brief The k-th smallest value (0-based), by two-level radix histogram.
 *
 * QA-A-55 (#144): std::nth_element over the 9.4-million-element difference array
 * measured 54.6% + 36.5% of ComputeGlobalSigma's work at 3072x3072 -- the two
 * selections together were 91.1% of it, while building the array was 5.9%. So
 * the selection is the item worth attacking, and this is the scalar way to do
 * it: two linear passes with 16-bit histograms instead of nth_element's repeated
 * partitioning.
 *
 * EXACT, not approximate. The first pass counts keys by their high 16 bits and
 * finds the bucket the k-th key falls in; the second counts the low 16 bits
 * within that bucket. The concatenation is the k-th key exactly, and the value
 * is recovered by inverting the map. No sampling, no interpolation.
 *
 * Equivalence to the std::nth_element it replaces, and its one gap:
 *   - For distinct values the k-th smallest is unique, so both agree exactly.
 *   - For repeated values every copy is bit-identical, so which copy is "the"
 *     k-th cannot be observed.
 *   - The gap is -0.0 vs +0.0: they compare equal, so nth_element may return
 *     either, while this always orders -0.0 first. Both callers here are safe --
 *     the first selection's result is used only as `x - median` (and
 *     x - (-0.0) == x - (+0.0) for every x), and the second selection runs on
 *     absolute deviations, which contain no -0.0 that a +0.0 could shadow.
 *     test_runtime_detection_radix_select_parity.cpp pins both claims.
 *   - NaN is out of scope, as it already was: NaN breaks the strict weak
 *     ordering std::nth_element requires, so there is no prior behaviour to match.
 *
 * @param values Values to select from; NOT modified.
 * @param n      Number of readable elements at @p values.
 * @param k      0-BASED rank: k = 0 selects the smallest value. A k at or beyond
 *               @p n is clamped to n-1 rather than treated as an error.
 * @return The k-th smallest value, or 0.0f when @p values is null or @p n is 0.
 */
inline float SelectKthSmallest(const float* values, size_t n, size_t k) {
    if (values == nullptr || n == 0u) return 0.0f;
    if (k >= n) k = n - 1u;

    constexpr size_t kBuckets = 1u << 16;
    std::vector<uint32_t> hist(kBuckets, 0u);

    for (size_t i = 0; i < n; ++i) {
        ++hist[FloatSortKey(values[i]) >> 16];
    }

    size_t seen = 0;
    uint32_t high = 0u;
    for (size_t b = 0; b < kBuckets; ++b) {
        if (seen + hist[b] > k) { high = static_cast<uint32_t>(b); break; }
        seen += hist[b];
    }

    std::fill(hist.begin(), hist.end(), 0u);
    for (size_t i = 0; i < n; ++i) {
        const uint32_t key = FloatSortKey(values[i]);
        if ((key >> 16) == high) ++hist[key & 0xFFFFu];
    }

    const size_t rank = k - seen;
    size_t inner = 0;
    uint32_t low = 0u;
    for (size_t b = 0; b < kBuckets; ++b) {
        if (inner + hist[b] > rank) { low = static_cast<uint32_t>(b); break; }
        inner += hist[b];
    }

    return SortKeyToFloat((high << 16) | low);
}

/**
 * @brief Vector form. Delegates to the pointer core so both share one implementation.
 *
 * @param values Values to select from; NOT modified.
 * @param k      0-based rank, clamped to values.size() - 1 when it is larger.
 * @return The k-th smallest value, or 0.0f for an empty input.
 */
inline float SelectKthSmallest(const std::vector<float>& values, size_t k) {
    return SelectKthSmallest(values.data(), values.size(), k);
}

/**
 * @brief Frame-wide robust noise sigma, from adjacent-pixel differences.
 *
 * Takes the MAD of the horizontal adjacent differences and of the vertical
 * ones, converts each to a sigma (x 1.4826 for MAD, x 1/sqrt(2) to undo
 * Var(n1 - n2) = 2 sigma^2), and returns the SMALLER of the two. Differencing
 * removes structure that the raw values would otherwise contribute, and taking
 * the minimum picks whichever direction the structure disturbed less -- see
 * QA-A-48/A-49 (#148) for why the value-MAD it replaced read structure as noise.
 *
 * @param img Input frame. Must be XPE_PIXEL_FLOAT32; the caller has already
 *            validated the format by the time this is reached.
 * @return The frame's robust sigma estimate. Returns 0.0f when @p img or its
 *         data is null, or when the frame has fewer than 2 pixels in BOTH
 *         directions; a frame that is 1 pixel wide (or tall) is still measured
 *         along the other direction alone.
 */
inline float ComputeGlobalSigma(const XpeImageBuffer* img) {
    if (img == nullptr || img->data == nullptr) return 0.0f;
    const size_t w = img->width;
    const size_t h = img->height;
    if (w < 2u && h < 2u) return 0.0f;

    const float* pixels = static_cast<const float*>(img->data);

    // MAD of a difference array, already converted to a sigma.
    //
    // QA-A-55 (#144): the two selections were 91.1% of this function at 3072x3072.
    // Both are now SelectKthSmallest -- an exact two-pass radix selection rather
    // than std::nth_element's repeated partitioning. Same rank, same value; see
    // that function for the equivalence argument and its one -0.0/+0.0 gap.
    auto madSigma = [](float* d, size_t n) -> float {
        if (d == nullptr || n == 0u) return 0.0f;
        const size_t mid = n / 2u;
        const float median = SelectKthSmallest(d, n, mid);
        for (size_t i = 0; i < n; ++i) d[i] = std::abs(d[i] - median);
        // 1.4826 : MAD -> sigma.   1/sqrt(2) : undo Var(n1 - n2) = 2 sigma^2.
        return SelectKthSmallest(d, n, mid) * RUNTIME_DETECTION_MAD_SCALE * 0.70710678f;
    };

    // QA-A-59 (#144): one buffer, sized once, written by index.
    //
    // This used to be reserve() + push_back(). Two costs came with that. The
    // visible one is per-element: push_back re-checks size against capacity and
    // bumps the size member for every one of ~9.4 million writes, work an
    // indexed store does not do. The structural one is that a shared vector
    // grown by push_back cannot be filled by several threads -- QA-A-58
    // measured this stage as fully splittable in principle and blocked in
    // practice, and named push_back as the blocker. This change removes both,
    // and is worth making whichever way the thread question is decided.
    //
    // new float[n] rather than std::vector<float>(n): vector value-initialises,
    // which would add a full zero-fill pass over 37.75 MB that the previous
    // reserve() never paid. Every element is written before it is read, so
    // default-initialised storage is correct here and costs nothing.
    const size_t hCount = (w >= 2u) ? h * (w - 1u) : 0u;
    const size_t vCount = (h >= 2u) ? (h - 1u) * w : 0u;
    const size_t maxCount = (hCount > vCount) ? hCount : vCount;
    if (maxCount == 0u) return 0.0f;
    std::unique_ptr<float[]> diff(new float[maxCount]);

    float sigmaH = 0.0f;
    if (hCount > 0u) {
        float* out = diff.get();
        for (size_t y = 0; y < h; ++y) {
            const float* row = pixels + y * w;
            float* dst = out + y * (w - 1u);
            for (size_t x = 0; x + 1u < w; ++x) dst[x] = row[x + 1u] - row[x];
        }
        sigmaH = madSigma(diff.get(), hCount);
    }

    float sigmaV = 0.0f;
    if (vCount > 0u) {
        float* out = diff.get();
        for (size_t y = 0; y + 1u < h; ++y) {
            const float* row = pixels + y * w;
            float* dst = out + y * w;
            for (size_t x = 0; x < w; ++x) dst[x] = row[x + w] - row[x];
        }
        sigmaV = madSigma(diff.get(), vCount);
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
 * @param windowValues Scratch buffer for the neighbour gather. Contents on entry
 *                     are discarded; on return it holds this pixel's neighbours
 *                     in unspecified order. Reused across pixels so the gather
 *                     costs no allocation.
 * @param deviations Second scratch buffer, for the absolute deviations the MAD
 *                   is taken over. Same contract as @p windowValues.
 * @return true if pixel is defective, false otherwise. Also false when the
 *         window yields fewer than RUNTIME_DETECTION_MIN_NEIGHBORS neighbours,
 *         which is the SPEC's "skip rather than judge" rule at the border.
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
 *
 * @param img Input image (float32 format)
 * @param x Pixel X coordinate
 * @param y Pixel Y coordinate
 * @param config Detection configuration
 * @return true if pixel is defective, false otherwise -- identical to the
 *         buffer-taking overload, which this one calls with fresh buffers.
 */
inline bool DetectDefectivePixel(const XpeImageBuffer* img,
                                 uint32_t x,
                                 uint32_t y,
                                 const RuntimeDetectionConfig& config) {
    std::vector<float> windowValues;
    std::vector<float> deviations;
    return DetectDefectivePixel(img, x, y, config, windowValues, deviations);
}


/* ------------------------------------------------------------- QA-A-61 */
//
// Caller-specified threading. Two properties are load-bearing and both hold by
// CONSTRUCTION rather than by luck -- the parity tests then check that the
// construction is what actually shipped:
//
//   1. Every pixel's verdict reads only the input frame and the config. No pixel
//      reads another pixel's verdict, and each writes its own map byte. Splitting
//      rows therefore cannot change any value.
//   2. The global sigma is computed ONCE over the whole frame before any split,
//      and its own parallel form sums integer histogram counts -- exact, and
//      addition of counts is associative, so the merged table is identical to the
//      single-threaded one for any thread count.
//
// What is NOT claimed: that threading makes this fast enough. QA-A-58 measured
// 20 threads still 1.44x short of the 60 ms target; that gap is algorithmic and
// belongs to another card.

/** Clamps a caller-supplied thread count to something runnable. */
inline uint32_t RuntimeDetection_NormalizeThreads(int32_t requested) {
    return (requested < 1) ? 1u : static_cast<uint32_t>(requested);
}

/**
 * @brief Frame-wide robust sigma, split across @p threadCount workers.
 *
 * Bit-identical to ComputeGlobalSigma for every thread count: the differences
 * are the same values at the same positions, the per-thread histograms are
 * summed exactly, and the selection reads one merged table.
 *
 * @param img Input frame (XPE_PIXEL_FLOAT32).
 * @param threadCount Workers to use; values below 1 are treated as 1.
 * @return The frame's robust sigma estimate, or 0.0f on the same conditions
 *         ComputeGlobalSigma returns 0.0f.
 */
inline float ComputeGlobalSigmaThreaded(const XpeImageBuffer* img, int32_t threadCount) {
    const uint32_t T = RuntimeDetection_NormalizeThreads(threadCount);
    if (T == 1u) return ComputeGlobalSigma(img);
    if (img == nullptr || img->data == nullptr) return 0.0f;

    const size_t w = img->width;
    const size_t h = img->height;
    if (w < 2u && h < 2u) return 0.0f;
    const float* pixels = static_cast<const float*>(img->data);

    const size_t hCount = (w >= 2u) ? h * (w - 1u) : 0u;
    const size_t vCount = (h >= 2u) ? (h - 1u) * w : 0u;
    const size_t maxCount = (hCount > vCount) ? hCount : vCount;
    if (maxCount == 0u) return 0.0f;
    std::unique_ptr<float[]> diff(new float[maxCount]);

    constexpr size_t kBuckets = 1u << 16;
    std::vector<uint32_t> tables(static_cast<size_t>(T) * kBuckets, 0u);

    auto rowsOf = [&](uint32_t t, size_t rows) {
        const size_t y0 = (rows * t) / T;
        const size_t y1 = (rows * (t + 1u)) / T;
        return std::pair<size_t, size_t>(y0, y1);
    };

    // Exact selection over `n` values, with the counting split across threads.
    auto selectKth = [&](const float* d, size_t n, size_t k) -> float {
        if (n == 0u) return 0.0f;
        if (k >= n) k = n - 1u;
        std::fill(tables.begin(), tables.end(), 0u);

        auto countHigh = [d, &tables](size_t i0, size_t i1, uint32_t* table) {
            (void)tables;
            for (size_t i = i0; i < i1; ++i) ++table[FloatSortKey(d[i]) >> 16];
        };
        {
            std::vector<std::thread> pool;
            pool.reserve(T);
            for (uint32_t t = 0; t < T; ++t) {
                pool.emplace_back(countHigh, (n * t) / T, (n * (t + 1u)) / T,
                                  tables.data() + static_cast<size_t>(t) * kBuckets);
            }
            for (std::thread& th : pool) th.join();
        }
        for (uint32_t t = 1; t < T; ++t) {
            const uint32_t* src = tables.data() + static_cast<size_t>(t) * kBuckets;
            uint32_t* dst = tables.data();
            for (size_t b = 0; b < kBuckets; ++b) dst[b] += src[b];
        }
        size_t seen = 0;
        uint32_t high = 0u;
        for (size_t b = 0; b < kBuckets; ++b) {
            if (seen + tables[b] > k) { high = static_cast<uint32_t>(b); break; }
            seen += tables[b];
        }

        std::fill(tables.begin(), tables.end(), 0u);
        auto countLow = [d, high](size_t i0, size_t i1, uint32_t* table) {
            for (size_t i = i0; i < i1; ++i) {
                const uint32_t key = FloatSortKey(d[i]);
                if ((key >> 16) == high) ++table[key & 0xFFFFu];
            }
        };
        {
            std::vector<std::thread> pool;
            pool.reserve(T);
            for (uint32_t t = 0; t < T; ++t) {
                pool.emplace_back(countLow, (n * t) / T, (n * (t + 1u)) / T,
                                  tables.data() + static_cast<size_t>(t) * kBuckets);
            }
            for (std::thread& th : pool) th.join();
        }
        for (uint32_t t = 1; t < T; ++t) {
            const uint32_t* src = tables.data() + static_cast<size_t>(t) * kBuckets;
            uint32_t* dst = tables.data();
            for (size_t b = 0; b < kBuckets; ++b) dst[b] += src[b];
        }
        const size_t rank = k - seen;
        size_t inner = 0;
        uint32_t low = 0u;
        for (size_t b = 0; b < kBuckets; ++b) {
            if (inner + tables[b] > rank) { low = static_cast<uint32_t>(b); break; }
            inner += tables[b];
        }
        return SortKeyToFloat((high << 16) | low);
    };

    auto madSigma = [&](float* d, size_t n) -> float {
        if (n == 0u) return 0.0f;
        const size_t mid = n / 2u;
        const float median = selectKth(d, n, mid);
        {
            std::vector<std::thread> pool;
            pool.reserve(T);
            for (uint32_t t = 0; t < T; ++t) {
                const size_t i0 = (n * t) / T;
                const size_t i1 = (n * (t + 1u)) / T;
                pool.emplace_back([d, i0, i1, median]() {
                    for (size_t i = i0; i < i1; ++i) d[i] = std::abs(d[i] - median);
                });
            }
            for (std::thread& th : pool) th.join();
        }
        return selectKth(d, n, mid) * RUNTIME_DETECTION_MAD_SCALE * 0.70710678f;
    };

    float sigmaH = 0.0f;
    if (hCount > 0u) {
        float* out = diff.get();
        std::vector<std::thread> pool;
        pool.reserve(T);
        for (uint32_t t = 0; t < T; ++t) {
            const auto r = rowsOf(t, h);
            pool.emplace_back([pixels, out, w, r]() {
                for (size_t y = r.first; y < r.second; ++y) {
                    const float* row = pixels + y * w;
                    float* dst = out + y * (w - 1u);
                    for (size_t x = 0; x + 1u < w; ++x) dst[x] = row[x + 1u] - row[x];
                }
            });
        }
        for (std::thread& th : pool) th.join();
        sigmaH = madSigma(diff.get(), hCount);
    }

    float sigmaV = 0.0f;
    if (vCount > 0u) {
        float* out = diff.get();
        std::vector<std::thread> pool;
        pool.reserve(T);
        for (uint32_t t = 0; t < T; ++t) {
            const auto r = rowsOf(t, h - 1u);
            pool.emplace_back([pixels, out, w, r]() {
                for (size_t y = r.first; y < r.second; ++y) {
                    const float* row = pixels + y * w;
                    float* dst = out + y * w;
                    for (size_t x = 0; x < w; ++x) dst[x] = row[x + w] - row[x];
                }
            });
        }
        for (std::thread& th : pool) th.join();
        sigmaV = madSigma(diff.get(), vCount);
    }

    if (sigmaH <= 0.0f) return sigmaV;
    if (sigmaV <= 0.0f) return sigmaH;
    return (sigmaH < sigmaV) ? sigmaH : sigmaV;
}

/**
 * @brief Runs the per-pixel rule over a whole frame, optionally across threads.
 *
 * The sigma floor and cap are derived here, once, from the whole frame -- before
 * any split, because they are frame-wide quantities. Workers then take disjoint
 * row ranges and write disjoint map bytes.
 *
 * @param img Input frame (XPE_PIXEL_FLOAT32).
 * @param config Detection configuration; config.threadCount selects the split.
 *               The floor and cap fields are OVERWRITTEN from the frame's own
 *               sigma, matching what the shipped entry point does.
 * @param map Output map, one byte per pixel, zero-filled by the caller.
 *            1 marks a defective pixel; untouched bytes keep their prior value.
 */
inline void DetectFrame(const XpeImageBuffer* img,
                        RuntimeDetectionConfig config,
                        uint8_t* map) {
    if (img == nullptr || img->data == nullptr || map == nullptr) return;
    const uint32_t T = RuntimeDetection_NormalizeThreads(config.threadCount);
    const uint32_t w = img->width;
    const uint32_t h = img->height;

    const float sigmaGlobal = ComputeGlobalSigmaThreaded(img, config.threadCount);
    config.globalSigmaFloor = RUNTIME_DETECTION_GLOBAL_SIGMA_FLOOR * sigmaGlobal;
    config.globalSigmaCap = RUNTIME_DETECTION_GLOBAL_SIGMA_CAP * sigmaGlobal;

    auto runRows = [img, &config, map, w](uint32_t y0, uint32_t y1) {
        std::vector<float> windowValues;
        std::vector<float> deviations;
        windowValues.reserve(64);
        deviations.reserve(64);
        for (uint32_t y = y0; y < y1; ++y) {
            for (uint32_t x = 0; x < w; ++x) {
                if (DetectDefectivePixel(img, x, y, config, windowValues, deviations)) {
                    map[static_cast<size_t>(y) * w + x] = 1u;
                }
            }
        }
    };

    if (T == 1u) { runRows(0u, h); return; }

    std::vector<std::thread> pool;
    pool.reserve(T);
    for (uint32_t t = 0; t < T; ++t) {
        const uint32_t y0 = static_cast<uint32_t>((static_cast<uint64_t>(h) * t) / T);
        const uint32_t y1 = static_cast<uint32_t>((static_cast<uint64_t>(h) * (t + 1u)) / T);
        pool.emplace_back(runRows, y0, y1);
    }
    for (std::thread& th : pool) th.join();
}

} // namespace internal
} // namespace preprocess
} // namespace xpe

#endif /* RUNTIME_DETECTION_H */
