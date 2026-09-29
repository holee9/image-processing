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
#include <cstddef>
#include <algorithm>
#include <numeric>
#include <vector>

/* =============================================================================
 * ABI LOCK for XpeCalibrationMetrics -- SRS-CALIB-FUNC-037, QA-A-159 (#223)
 *
 * #223 widened this struct. FUNC-037 allows that only by APPENDING: every field
 * that existed before keeps its name, its type, and its BYTE OFFSET. A caller
 * built against the old header must be able to read every old field from a
 * struct filled in by the new library.
 *
 * These assertions are what makes that a check rather than an intention. They
 * fail at COMPILE TIME if a field is inserted, reordered, resized, or removed
 * -- the three edits that look harmless in a diff and break binaries silently.
 * Verified to actually fire: QA-A-159 §5 inserted a double before `dsnu` and
 * every assertion from that point down went red, then reverted.
 * ============================================================================ */

static_assert(offsetof(XpeCalibrationMetrics, dark_bias)          ==   0, "ABI: dark_bias moved");
static_assert(offsetof(XpeCalibrationMetrics, dsnu)               ==   8, "ABI: dsnu moved");
static_assert(offsetof(XpeCalibrationMetrics, residual_noise)     ==  16, "ABI: residual_noise moved");
static_assert(offsetof(XpeCalibrationMetrics, prnu_before)        ==  24, "ABI: prnu_before moved");
static_assert(offsetof(XpeCalibrationMetrics, prnu_after)         ==  32, "ABI: prnu_after moved");
static_assert(offsetof(XpeCalibrationMetrics, flatness_pct)       ==  40, "ABI: flatness_pct moved");
static_assert(offsetof(XpeCalibrationMetrics, gain_coverage)      ==  48, "ABI: gain_coverage moved");
static_assert(offsetof(XpeCalibrationMetrics, invalid_gain_count) ==  56, "ABI: invalid_gain_count moved");
static_assert(offsetof(XpeCalibrationMetrics, defect_count)       ==  60, "ABI: defect_count moved");
static_assert(offsetof(XpeCalibrationMetrics, defect_density)     ==  64, "ABI: defect_density moved");
static_assert(offsetof(XpeCalibrationMetrics, correction_error)   ==  72, "ABI: correction_error moved");
static_assert(offsetof(XpeCalibrationMetrics, snr_improvement_db) ==  80, "ABI: snr_improvement_db moved");
static_assert(offsetof(XpeCalibrationMetrics, overall_pass)       ==  88, "ABI: overall_pass moved");

// Appended by #223 -- these three must stay AFTER everything above.
static_assert(offsetof(XpeCalibrationMetrics, dark_reduction_db)  >  offsetof(XpeCalibrationMetrics, overall_pass),
              "ABI: appended fields must follow the pre-#223 fields");
static_assert(offsetof(XpeCalibrationMetrics, dsnu_adu)           >  offsetof(XpeCalibrationMetrics, dark_reduction_db),
              "ABI: appended fields must keep their order");
static_assert(offsetof(XpeCalibrationMetrics, measured_mask)      >  offsetof(XpeCalibrationMetrics, dsnu_adu),
              "ABI: appended fields must keep their order");

/* =============================================================================
 * Pass/Fail Thresholds (configurable defaults)
 * ============================================================================ */

namespace {

    /* THRESHOLD PROVENANCE -- QA-A-152 (#216).
     * These constants decide `overall_pass`, so each one is a pass/fail
     * criterion for a Class B function. Two have a requirement behind them and
     * four do not; the search scope for the four is docs/ (SRS-CALIB-001,
     * RTM-CALIB-001) plus .moai/specs/SPEC-XPE-P1A/spec.md, searched by value
     * (3 dB, 0.99, 99%, 2 dB) and by concept (PRNU, coverage, SNR
     * improvement). docs/quality-eval/ is excluded on purpose: its own README
     * states it documents a separate Python production-line QA tool, "안전
     * 등급: 해당 없음 (… 진단 소프트웨어 아님)", so it is reference material,
     * not a requirement source for this module. */

    // Offset correction thresholds
    constexpr double DARK_BIAS_MAX       = 5.0;     // ADU. SRS-CALIB-FUNC-016:
                                                    // "acceptance shall require abs(DarkBias) <= 5 ADU"
    constexpr double DARK_REDUCTION_MIN_DB = 10.0;  // dB. SRS-CALIB-FUNC-016:147 /
                                                    // Protocol.md:211, the `or` alternative to
                                                    // DARK_BIAS_MAX -- QA-A-158 (#222).
    constexpr double DSNU_MAX_PCT        = 1.0;     // % -- NO REQUIREMENT FOUND (scope above).
                                                    // QA-A-158 (#222) REMOVED THIS FROM THE GATE:
                                                    // FUNC-016 does not name it, and an unsourced
                                                    // threshold was blocking passes the requirement
                                                    // allows. The value is still reported in
                                                    // metrics->dsnu; only the gate stopped reading it.
                                                    // docs/quality-eval/01_Noise_...:1185 carries
                                                    // "DSNU RMS < 1% of full scale", but that is a
                                                    // DIFFERENT QUANTITY: this metric is
                                                    // stddev/mean*100 over the dark region (:236),
                                                    // i.e. a coefficient of variation, not a
                                                    // fraction of full scale. Same numeral, different
                                                    // denominator -- do not adopt it as the source.

    // Gain correction thresholds
    constexpr double PRNU_IMPROVE_MIN_DB = 3.0;     // dB -- NO REQUIREMENT FOUND (scope above).
                                                    // Also misnamed: it is compared against
                                                    // snr_improvement_db (:348), not PRNU.
    constexpr double GAIN_COVERAGE_MIN   = 0.99;    // -- NO REQUIREMENT FOUND (scope above)
    constexpr double FLAT_RESIDUAL_MAX_PCT = 1.0;   // %. SRS-CALIB-FUNC-017: "Phase 1 acceptance
                                                    // shall require FlatResidualPct <= 1.0%"
                                                    // (target <= 0.5% for release hardening).
                                                    // The quantity is already computed as
                                                    // metrics->prnu_after -- see QA-A-154 note below.

    // Defect correction thresholds
    constexpr double DEFECT_DENSITY_MAX  = 5.0;     // %. SRS-CALIB-FUNC-003: "Maximum 5% defect
                                                    // density tolerance".
                                                    // QA-A-153 (#217): was 0.05. The metric is a
                                                    // PERCENT (count/pixels*100, below), so the
                                                    // fraction made this a 0.05 % gate -- 1/100 of
                                                    // what the requirement allows, rejecting panels
                                                    // the requirement accepts. Correcting the unit
                                                    // is not a product decision: the requirement and
                                                    // the comment both already said 5 %; the value
                                                    // was the only thing disagreeing.
                                                    // The metric stays in percent because its
                                                    // consumers are (tests, reports, GUI) -- the one
                                                    // wrong thing was this comparison.

    // Overall thresholds
    constexpr double SNR_IMPROVE_MIN_DB  = 2.0;     // dB -- NO REQUIREMENT FOUND (scope above)

    // Helper: Compute robust mean using median (more resistant to outliers)
    /* QA-A-156 (#220): the canonical metrics are defined on the ARITHMETIC mean.
     * Preprocessing-E2E-Automated-Evaluation-Protocol.md:203 `DarkBias =
     * mean(Y_dark_roi)`, :204 `DSNU_ADU = std(Y_dark_roi)`, :218 `PRNU_CV =
     * std(Y_flat_roi) / max(mean(Y_flat_roi), eps)`.
     *
     * compute_robust_mean below returns the MEDIAN, and compute_std takes the
     * centre as an argument -- so passing the median moved BOTH the centre of
     * the deviation sum and the divisor. Median + RMS is not a known estimator
     * pair (a robust design pairs the median with MAD), which is what decided
     * #220: the substitution was partial, so it is not the canon. */
    double compute_mean(const std::vector<double>& values) noexcept {
        if (values.empty()) return 0.0;
        double sum = 0.0;
        for (double v : values) sum += v;
        return sum / static_cast<double>(values.size());
    }

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
 * SRS-CALIB-FUNC-016 / REQ-P1A-010: Offset correction verification
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
        //
        // UNMEASURABLE REPORTS AS FAILURE -- SRS-CALIB-FUNC-036.
        //
        // The requirement states this, and QA-A-158 (#221) promoted this
        // comment from a decision to a citation -- it was decided in QA-A-157
        // (#219) before the requirement existed, and FUNC-036 was written from
        // that finding.
        //
        // THE FIELD NOW EXISTS (measured_mask, #223) AND THE false STAYS. The
        // requirement's "until a dedicated status field exists" phrasing reads
        // as if one replaces the other; it does not. They answer different
        // callers: the mask tells a caller that asks WHICH metric is
        // trustworthy, the false protects the caller that only reads
        // overall_pass and never learns a mask was added. Dropping the false
        // once the mask landed would re-open #219 for exactly that caller --
        // silently, because their code still compiles and still says "passed".
        //
        // This used to set overall_pass = true, which made a frame nobody could
        // measure indistinguishable from a frame that passed. QA-A-152 reported
        // it; #219 decided (a): match the gain path, which answers the same
        // situation with false (no valid pixels -> false, below).
        //
        // WHY false AND NOT true. The two mistakes are not symmetric. A caller
        // that believes a false "pass" ships an unverified frame; a caller that
        // believes a false "fail" looks at a good frame once more. Only the
        // first one is silent.
        //
        // WHICH SIDE WAS THE DESIGN: the gain path's false came with the
        // feature (b6c19b8, 2026-04-26 21:35). This branch was added 35 minutes
        // later by 86d2894 "fix(calibration): 테스트-구현 불일치 4건 수정",
        // whose own summary calls it "uniform 이미지 처리 개선" -- it was
        // written so a uniform-image test would stop failing, not as a product
        // judgement. That is why the gain path was the thing to match.
        //
        // LIMIT, AND IT IS REAL: overall_pass is one bool for three states --
        // passed, failed, could not be measured. (a) folds the third into the
        // second because that is the safe fold, and a caller still cannot tell
        // them apart. A separate status field is the actual fix; it needs a
        // requirement first, so it is not made here (#219).
        // measured_mask keeps DARK_BIAS and DARK_REDUCTION CLEAR: these are
        // placeholders, not measurements -- SRS-CALIB-FUNC-036. The false below
        // stays for the caller who never reads the mask (see the field comment).
        metrics->dark_bias = 0.0;
        metrics->dsnu = 0.0;
        metrics->dsnu_adu = 0.0;
        metrics->residual_noise = 0.0;
        metrics->dark_reduction_db = 0.0;
        metrics->overall_pass = false;
        return XPE_OK;
    }

    // DARK ROI SELECTION -- SRS-CALIB-FUNC-035. The rule is stated here rather
    // than left to be inferred from the variable name: Y_dark_roi is the
    // DARKEST DECILE OF THE RAW FRAME, `raw < percentile10(raw)`, and it needs
    // at least `dark_roi_min_pixels = 1% of pixel_count` members.
    std::vector<uint16_t> raw_copy(raw, raw + pixel_count);
    std::sort(raw_copy.begin(), raw_copy.end());
    uint16_t dark_threshold = raw_copy[static_cast<size_t>(pixel_count * 0.1)];

    std::vector<double> dark_corrected;   // Y_dark_roi -- after correction
    std::vector<double> dark_raw;         // R_dark_roi -- before, SAME pixels
    dark_corrected.reserve(pixel_count / 10);
    dark_raw.reserve(pixel_count / 10);

    for (size_t i = 0; i < pixel_count; ++i) {
        if (raw[i] < dark_threshold) {
            dark_corrected.push_back(static_cast<double>(corrected[i]));
            dark_raw.push_back(static_cast<double>(raw[i]));
        }
    }

    const size_t dark_roi_min_pixels =
        static_cast<size_t>(static_cast<double>(pixel_count) * 0.01);

    if (dark_corrected.size() < dark_roi_min_pixels) {
        // TOO FEW CANDIDATES -> UNMEASURABLE, NOT A SUBSTITUTE POPULATION
        // -- QA-A-158 (#221), SRS-CALIB-FUNC-035 + FUNC-036.
        //
        // This used to fall back to "no dark pixels found, use all pixels",
        // with no signal to the caller. A dark residual measured over the whole
        // frame is a DIFFERENT QUANTITY from one measured over dark pixels, and
        // the FUNC-016 gate is written for the latter -- so the fallback did
        // not degrade the measurement, it silently answered a different
        // question. Observed: DarkBias reported as 1000 and 1599.6 ADU.
        //
        // WHY THE STRICT `<` IS KEPT (measured, QA-A-158 §2). Ties are what
        // empty this set: on a frame whose values repeat, percentile10 lands ON
        // a repeated value and `<` admits far fewer than a decile -- zero in the
        // limit. Relaxing to `<=` is NOT the fix: measured on the same inputs it
        // swings the other way, admitting 100% / 50% / 20% of the frame, which
        // is no longer "the darkest decile". On the real fixtures (CalData_6,
        // 7 frames, 9.4 Mpx) `<` selects 8.6-10.0%, an order of magnitude above
        // this 1% floor. So the strict form is right and the floor is the guard.
        // measured_mask keeps DARK_BIAS and DARK_REDUCTION CLEAR: these are
        // placeholders, not measurements -- SRS-CALIB-FUNC-036. The false below
        // stays for the caller who never reads the mask (see the field comment).
        metrics->dark_bias = 0.0;
        metrics->dsnu = 0.0;
        metrics->dsnu_adu = 0.0;
        metrics->residual_noise = 0.0;
        metrics->dark_reduction_db = 0.0;
        metrics->overall_pass = false;
        return XPE_OK;
    }

    // Compute metrics
    // Protocol.md:203 `DarkBias = mean(Y_dark_roi)`, :204 `DSNU_ADU = std(...)`.
    // QA-A-156 (#220): was compute_robust_mean (median).
    double mean = compute_mean(dark_corrected);
    double stddev = compute_std(dark_corrected, mean);

    metrics->dark_bias = mean;
    metrics->dsnu = (mean > 0.0) ? (stddev / mean) * 100.0 : 0.0;
    // residual_noise and dsnu_adu are the SAME quantity, Protocol.md:204
    // `DSNU_ADU = std(Y_dark_roi)`. Written from one variable, adjacent, so the
    // canonical name and the historical name cannot drift -- QA-A-159 (#223).
    metrics->residual_noise = stddev;
    metrics->dsnu_adu = stddev;
    metrics->measured_mask |= XPE_METRIC_DARK_BIAS;

    // Protocol.md:205 `DarkReduction_dB = 20*log10(std(R_dark_roi) /
    // max(std(Y_dark_roi), epsilon))` -- R is before correction, Y after, over
    // the SAME ROI. QA-A-158 (#222): FUNC-016 names this metric and gates on
    // it, and it did not exist anywhere in modules/ (0 hits; controls
    // `dark_bias` 4, `dsnu` 5).
    constexpr double DARK_REDUCTION_EPS = 1e-9;
    const double raw_mean_roi   = compute_mean(dark_raw);
    const double raw_stddev_roi = compute_std(dark_raw, raw_mean_roi);
    const double dark_reduction_db =
        20.0 * std::log10(raw_stddev_roi / std::max(stddev, DARK_REDUCTION_EPS));
    // QA-A-159 (#223): the gate reads this local; the caller reads the field.
    // They are the same value by construction -- one assignment, no recompute.
    metrics->dark_reduction_db = dark_reduction_db;
    metrics->measured_mask |= XPE_METRIC_DARK_REDUCTION;

    // PASS/FAIL -- SRS-CALIB-FUNC-016:147 and Protocol.md:211, verbatim:
    //     `abs(DarkBias) <= 5 ADU` OR `DarkReduction_dB >= 10 dB`
    //
    // QA-A-158 (#222) brought three things to the requirement:
    //  - the `or` alternative existed in the requirement but not in the code;
    //  - `dsnu < DSNU_MAX_PCT` was ANDed in, a threshold the requirement does
    //    not name (QA-A-152 marked DSNU_MAX_PCT as having no source). It is
    //    removed from the GATE only -- `metrics->dsnu` is still reported;
    //  - `<` was `<=` in the requirement.
    //
    // `std::abs` is a no-op today: `corrected` is `const uint16_t*` (:242), so
    // dark_bias cannot be negative. It is written anyway because the day that
    // type changes is the day its absence becomes a defect, silently.
    metrics->overall_pass = (std::abs(metrics->dark_bias) <= DARK_BIAS_MAX) ||
                            (dark_reduction_db >= DARK_REDUCTION_MIN_DB);

    return XPE_OK;
}

/**
 * @brief Verify gain correction quality
 *
 * Computes PRNU before/after, flatness, gain coverage, and SNR improvement.
 *
 * SRS-CALIB-FUNC-017 / REQ-P1A-011: Gain correction verification
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
    // Protocol.md:218 `PRNU_CV = std(Y_flat_roi) / max(mean(Y_flat_roi), eps)`.
    // QA-A-156 (#220): was compute_robust_mean (median).
    double mean_before = compute_mean(before_vals);
    double std_before = compute_std(before_vals, mean_before);
    metrics->prnu_before = (mean_before > 0.0) ? (std_before / mean_before) * 100.0 : 0.0;

    // Compute PRNU after gain correction
    // Protocol.md:218, same as above -- this is the value FUNC-017 gates.
    double mean_after = compute_mean(after_vals);
    double std_after = compute_std(after_vals, mean_after);
    metrics->prnu_after = (mean_after > 0.0) ? (std_after / mean_after) * 100.0 : 0.0;

    // Compute flatness
    metrics->flatness_pct = compute_flatness(after_vals) * 100.0;

    // Compute gain coverage
    metrics->gain_coverage = static_cast<double>(valid_gain_count) / pixel_count;
    metrics->measured_mask |= XPE_METRIC_PRNU | XPE_METRIC_GAIN_COVERAGE | XPE_METRIC_SNR;

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

    /* QA-A-154 (#218): THE REQUIREMENT'S OWN CRITERION, which was missing.
     *
     * SRS-CALIB-FUNC-017 gates this function on `FlatResidualPct <= 1.0%`.
     * Until now overall_pass was decided entirely on a different axis --
     * relative improvement (prnu_improved, snr_improved) plus coverage -- so a
     * correction that improved a bad panel to a still-bad one passed. 10% ->
     * 5% is a 6 dB improvement and five times the residual the requirement
     * allows; it used to pass.
     *
     * No new calculation was needed. Preprocessing-E2E-Automated-Evaluation-
     * Protocol.md:218-219 defines `PRNU_CV = std/mean` and `FlatResidualPct =
     * 100 * PRNU_CV`, and prnu_after is std/mean*100 (:370) -- the percent one.
     * The metric was already here under the other name; only the gate was
     * absent. (XPE-GUI-CALIB-001:167 calls FlatResidualPct "same as PRNU_CV
     * (alias)", which is right about the quantity and silent about the 100x;
     * the protocol is the one that states the relation.)
     *
     * ABSOLUTE AND RELATIVE ARE BOTH KEPT. Dropping the improvement checks
     * would let a panel that is already flat-but-uncorrected pass, and
     * dropping this one lets a badly-corrected panel pass. They fail different
     * things. */
    bool flat_residual_ok = (metrics->prnu_after <= FLAT_RESIDUAL_MAX_PCT);

    metrics->overall_pass = prnu_improved && coverage_ok && snr_improved && flat_residual_ok;

    return XPE_OK;
}

/**
 * @brief Verify defect correction quality
 *
 * Computes defect count, density, and correction error metrics.
 *
 * SRS-CALIB-FUNC-019 / REQ-P1A-012: Defect correction verification
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
    metrics->measured_mask |= XPE_METRIC_DEFECT;

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
 * SRS-CALIB-FUNC-015 / SRS-CALIB-FUNC-021 / REQ-P1A-041..047: Pipeline verification
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
    metrics->measured_mask |= XPE_METRIC_SNR;

    // Pass/fail determination
    metrics->overall_pass = (metrics->snr_improvement_db >= SNR_IMPROVE_MIN_DB);

    return XPE_OK;
}
