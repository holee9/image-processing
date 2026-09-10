/**
 * @file test_runtime_detection_rates.cpp
 * @brief QA-A-40 (#120 #112): REQ-P1A-013 TPR / FPR quantitative harness.
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 *
 * SPEC-XPE-P1A REQ-P1A-013, "Pixel Accuracy (research.md v2.0.0 Section 8.3)",
 * verbatim:
 *
 *   - True-positive rate (TPR) on injected 5-sigma transients: >= 99.9%
 *   - False-positive rate (FPR) on clean clinical frames: < 0.001%
 *     (< 9 false pixels per 3072x3072)
 *   - Edge-of-image pixels (where 3x3 neighborhood is incomplete): processed
 *     with available subset; at least 5 neighbors required or pixel is skipped
 *     (defectMapOut = 0)
 *
 * The two rates are measured on DIFFERENT images, exactly as the SPEC words
 * them: TPR on a frame with injected transients, FPR on a clean frame. Mixing
 * them would let a defect's own neighbourhood perturbation count as a false
 * positive and understate the detector.
 *
 * What comes from the document and what does not:
 *
 *   - The thresholds (0.999, 1e-5) and the defect definition ("5-sigma
 *     transients") are quoted above.
 *   - The noise levels, the frame size and the injected count are experimental
 *     conditions of THIS harness, not spec values. They are stated in the
 *     report rather than presented as requirements.
 *   - The SPEC gives no separate hot / dead / spike amplitude table. "5-sigma
 *     transients" is the only amplitude the requirement names, so that is what
 *     the assertion uses; the sweep at 6, 8 and 10 sigma is reported alongside
 *     to show where the detector actually saturates.
 *
 * Detector under test: xpe_defect_detect_runtime, which runs
 * RuntimeDetection_DefaultConfig() -- a 5x5 window at 5.0 sigma, with
 * sigma estimated as MAD * 1.4826 (runtime_detection.h:44-59).
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

namespace {

constexpr uint32_t kW = 1024;
constexpr uint32_t kH = 1024;
constexpr size_t   kN = static_cast<size_t>(kW) * kH;

// The requirement's own numbers.
constexpr double kTprFloor = 0.999;    // >= 99.9%
constexpr double kFprCap   = 0.00001;  // < 0.001%

/** One measurement: how many injected pixels were found, and how many were not. */
struct Rates {
    size_t injected   = 0;
    size_t truePos    = 0;
    size_t falseNeg   = 0;
    size_t falsePos   = 0;   // clean-frame flags
    size_t cleanTotal = 0;
    double elapsedMs  = 0.0;

    double tpr() const {
        return injected ? static_cast<double>(truePos) / static_cast<double>(injected) : 0.0;
    }
    double fpr() const {
        return cleanTotal ? static_cast<double>(falsePos) / static_cast<double>(cleanTotal) : 0.0;
    }
};

/**
 * Injected coordinates: a fixed lattice, well clear of the border and spaced so
 * no two defects share a 5x5 window. 32-pixel stride over a 1024 frame with a
 * 16-pixel margin gives 31 x 31 = 961 sites; the count is reported rather than
 * rounded to a target.
 */
std::vector<size_t> defectSites() {
    std::vector<size_t> sites;
    for (uint32_t y = 16; y < kH - 16; y += 32) {
        for (uint32_t x = 16; x < kW - 16; x += 32) {
            sites.push_back(static_cast<size_t>(y) * kW + x);
        }
    }
    return sites;
}

/** Deterministic Gaussian frame: mean 3000 ADU, standard deviation @p sigma. */
std::vector<float> cleanFrame(float sigma, uint32_t seed) {
    std::mt19937 rng(seed);
    std::normal_distribution<float> noise(3000.0f, sigma);
    std::vector<float> frame(kN);
    for (size_t i = 0; i < kN; ++i) frame[i] = noise(rng);
    return frame;
}

/** Runs the detector and returns the flagged map. */
std::vector<uint8_t> detect(std::vector<float>& frame, double* elapsedMsOut) {
    XpeImageBuffer img{};
    img.data = frame.data();
    img.width = kW; img.height = kH;
    img.bitsAllocated = 32; img.bitsStored = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.dataSize = static_cast<uint32_t>(kN * sizeof(float));

    std::vector<uint8_t> map(kN, 0);
    XpeImageBuffer out{};
    out.data = map.data();
    out.width = kW; out.height = kH;
    out.bitsAllocated = 8; out.bitsStored = 8;
    out.format = XPE_PIXEL_UINT8;
    out.dataSize = static_cast<uint32_t>(kN);

    XpeImageMetadata meta{};
    const auto t0 = std::chrono::steady_clock::now();
    const XpeErrorCode rc = xpe_defect_detect_runtime(&img, &meta, &out);
    const auto t1 = std::chrono::steady_clock::now();
    EXPECT_EQ(XPE_OK, rc);

    if (elapsedMsOut) {
        *elapsedMsOut = std::chrono::duration<double, std::milli>(t1 - t0).count();
    }
    return map;
}

/**
 * Measures TPR at @p amplitudeSigma on a noisy frame, and FPR on the clean
 * frame built from the same seed and noise level.
 */
Rates measure(float noiseSigma, float amplitudeSigma, uint32_t seed,
              bool alsoMeasureFpr = true) {
    Rates r;
    const std::vector<size_t> sites = defectSites();
    r.injected = sites.size();

    // --- TPR frame: clean noise plus one positive transient per site.
    std::vector<float> withDefects = cleanFrame(noiseSigma, seed);
    for (size_t s : sites) withDefects[s] += amplitudeSigma * noiseSigma;

    double msDefect = 0.0;
    const std::vector<uint8_t> flaggedDefect = detect(withDefects, &msDefect);
    for (size_t s : sites) {
        if (flaggedDefect[s]) ++r.truePos; else ++r.falseNeg;
    }

    // --- FPR frame: the same generator, no injection. "clean clinical frames".
    double msClean = 0.0;
    if (alsoMeasureFpr) {
        std::vector<float> clean = cleanFrame(noiseSigma, seed);
        const std::vector<uint8_t> flaggedClean = detect(clean, &msClean);
        for (size_t i = 0; i < kN; ++i) if (flaggedClean[i]) ++r.falsePos;
        r.cleanTotal = kN;
    }

    r.elapsedMs = msDefect + msClean;
    return r;
}

void report(const char* label, const Rates& r) {
    std::printf("[rates] %-28s injected=%zu TP=%zu FN=%zu  TPR=%.6f | "
                "FP=%zu/%zu FPR=%.9f | %.1f ms\n",
                label, r.injected, r.truePos, r.falseNeg, r.tpr(),
                r.falsePos, r.cleanTotal, r.fpr(), r.elapsedMs);
}

class RuntimeDetectionRatesTest : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr)); }
    void TearDown() override { xpe_preprocess_shutdown(); }
};

// --- The requirement, asserted at the amplitude it names --------------------
//
// KnownDivergence_ prefix: the measured TPR at exactly 5 sigma does NOT reach
// 99.9%, and the assertion is deliberately NOT relaxed to make it pass. The
// case pins the CURRENT numbers so a later algorithm change is visible, exactly
// as test_sigma_clip_conformance.cpp did for the N_min clause before QA-A-38.
//
// Why 5 sigma is the hard case: the detector flags when
// |value - median| > 5.0 * (MAD * 1.4826), and MAD * 1.4826 estimates the local
// standard deviation. A transient of exactly 5 sigma therefore sits ON the
// threshold, and the local sigma estimate fluctuates around the true value from
// the 24 neighbours in the window -- so roughly half the sites fall on either
// side. The requirement's ">= 99.9% on injected 5-sigma transients" is only
// reachable if the transient clears the threshold by a margin.
//
// The exact figures are recorded in the report; the bounds asserted here are
// loose brackets around the measurement so the case fails if the behaviour
// moves materially, not on run-to-run noise.
TEST_F(RuntimeDetectionRatesTest, KnownDivergence_TprAtFiveSigmaIsBelowTheRequirement) {
    const Rates low  = measure(10.0f, 5.0f, 20260911u);
    const Rates high = measure(50.0f, 5.0f, 20260911u);
    report("low noise, 5 sigma", low);
    report("high noise, 5 sigma", high);

    EXPECT_LT(low.tpr(), kTprFloor)
        << "REQ-P1A-013 asks for >= 0.999 at 5 sigma; this records that it is not met";
    EXPECT_LT(high.tpr(), kTprFloor);

    // Brackets, not equalities: the measurement is deterministic for a fixed
    // seed, but pinning an exact ratio would break on any compiler-level
    // floating-point difference.
    // 0.500520 (pre-QA-A-42) -> 0.610822 (QA-A-42) -> 0.553590 (QA-A-43).
    // The sigma floor costs 9% of the detection rate and buys a 168x reduction
    // in false positives. Still nowhere near 0.999 -- the lambda = 5 vs
    // TPR-at-5-sigma contradiction in REQ-P1A-013 is untouched by this card and
    // is awaiting a decision.
    EXPECT_GT(low.tpr(), 0.40) << "and it is not near-zero either";
    EXPECT_LT(low.tpr(), 0.90);

    RecordProperty("spec_clause", "REQ-P1A-013 TPR >= 99.9% at 5 sigma");
    RecordProperty("measured_tpr_low_noise", std::to_string(low.tpr()));
    RecordProperty("measured_tpr_high_noise", std::to_string(high.tpr()));
}

// FPR is measured on the clean frame the SPEC names, and is NOT met.
//
// Measured: 496 flagged pixels out of 1,048,576 -> FPR = 4.73e-4, which is
// 0.047% against a 0.001% requirement -- 47x over. The header's own comment
// (runtime_detection.h:47-51) reasons that "5-sigma corresponds to
// approximately 1 in 3.5 million false positives for normally distributed
// data"; that holds for a KNOWN sigma, but the detector estimates sigma from
// 24 neighbours per pixel, and the estimate's own spread is what produces
// these flags. The assertion is not relaxed to fit; the number is pinned.
TEST_F(RuntimeDetectionRatesTest, KnownDivergence_FprOnCleanFramesExceedsTheRequirement) {
    const Rates low  = measure(10.0f, 5.0f, 20260911u);
    const Rates high = measure(50.0f, 5.0f, 20260911u);
    report("low noise, clean FPR", low);
    report("high noise, clean FPR", high);

    EXPECT_GT(low.fpr(), kFprCap)
        << "REQ-P1A-013 asks for < 1e-5; this records that it is not met";
    EXPECT_GT(high.fpr(), kFprCap);

    // Three measurements: 4.730e-4 (pre-QA-A-42) -> 1.718e-2 (QA-A-42, SPEC
    // neighbourhood rule) -> 1.020e-4 (QA-A-43, global sigma floor). The floor
    // put the 1% ceiling back within reach -- see
    // CleanFrameStaysUnderTheOnePercentCeiling -- but the REQUIREMENT here is
    // 1e-5, and 1.02e-4 is still an order of magnitude above it. Still a
    // divergence, with a much smaller gap.
    EXPECT_GT(low.fpr(), kFprCap) << "1.02e-4 is still above the 1e-5 requirement";
    EXPECT_LT(low.fpr(), 0.001)   << "bracket: an order below the old 1.7e-2";
    EXPECT_GT(high.fpr(), kFprCap);
    EXPECT_LT(high.fpr(), 0.001);

    RecordProperty("spec_clause", "REQ-P1A-013 FPR < 0.001% on clean frames");
    RecordProperty("measured_fpr_low_noise", std::to_string(low.fpr()));
    RecordProperty("measured_fpr_high_noise", std::to_string(high.fpr()));
    RecordProperty("false_positives_low_noise", std::to_string(low.falsePos));
}

// How far the amplitude has to go before the requirement is met -- and the
// finding that it is not met even at 10 sigma.
//
// Measured (low noise, 961 sites): 6 sigma -> 0.7118, 8 sigma -> 0.9553,
// 10 sigma -> 0.9990 (960 of 961). The last is 0.998959, just under the 0.999
// floor: one site short. So no amplitude in this sweep satisfies
// "TPR >= 99.9%", and the requirement as written is not met at any tested
// amplitude, not merely at 5 sigma.
TEST_F(RuntimeDetectionRatesTest, KnownDivergence_TprStaysBelowTheFloorThroughTenSigma) {
    double best = 0.0;
    for (float amp : {6.0f, 8.0f, 10.0f}) {
        const Rates r = measure(10.0f, amp, 20260911u, /*alsoMeasureFpr=*/false);
        report(("low noise, " + std::to_string(static_cast<int>(amp)) +
                " sigma").c_str(), r);
        RecordProperty("tpr_at_" + std::to_string(static_cast<int>(amp)) + "_sigma",
                       std::to_string(r.tpr()));
        if (r.tpr() > best) best = r.tpr();
    }

    // QA-A-42 (#143) then QA-A-43: 6 sigma 0.711759 -> 0.761707 -> 0.753382,
    // 8 sigma 0.955255 -> 0.927159 -> 0.927159, 10 sigma 0.998959 -> 0.986472
    // -> 0.986472. The floor costs a little at 6 sigma and nothing above it:
    // a transient that clears 8 sigma clears the floored threshold too.
    // The low end improves and the high end gets WORSE: eight samples make the
    // sigma estimate noisier, which helps a marginal transient clear the
    // threshold and hurts one that should clear it comfortably.
    EXPECT_LT(best, kTprFloor)
        << "no amplitude up to 10 sigma reaches 99.9%";
    EXPECT_GT(best, 0.95) << "but 10 sigma is close";
}

// The two noise levels are NOT independent evidence, and saying so is part of
// the result: the detector is scale-invariant. Both the transient amplitude and
// the MAD-derived threshold scale with sigma, so a 5-sigma transient in 10 ADU
// noise and in 50 ADU noise present the detector with the same problem. The
// rates come out identical, which is a property worth pinning rather than two
// rows to be read as a wider sweep.
TEST_F(RuntimeDetectionRatesTest, RatesAreInvariantUnderNoiseScaling) {
    const Rates low  = measure(10.0f, 5.0f, 20260911u);
    const Rates high = measure(50.0f, 5.0f, 20260911u);

    EXPECT_EQ(low.truePos, high.truePos);

    // QA-A-42 (#143): the false-positive counts are no longer EXACTLY equal
    // (18011 vs 18009 measured). Scale invariance is a property of the rule,
    // not of float32 arithmetic: with eight samples the median and MAD land on
    // different rounding boundaries at 10 ADU than at 50 ADU. The invariance
    // still holds to about one part in ten thousand.
    const double rel = std::fabs(static_cast<double>(low.falsePos) -
                                 static_cast<double>(high.falsePos)) /
                       static_cast<double>(low.falsePos);
    EXPECT_LT(rel, 1e-3) << "low=" << low.falsePos << " high=" << high.falsePos;
}

// REQ-P1A-013, verbatim: "Output is boolean-like UINT8 (0 or 1); guaranteed
// sum(defectMapOut) does not exceed width*height * 0.01 for clean input".
//
// Three measurements, in order:
//   pre-QA-A-42 (5x5, centre included)   496 of 1,048,576  = 0.047%   under
//   QA-A-42     (3x3, SPEC rule)      18,011 of 1,048,576  = 1.72%    BREACH
//   QA-A-43     (+ global sigma floor)   107 of 1,048,576  = 0.0102%  under
//
// QA-A-42's KnownDivergence_ form was not a wrong expectation being fixed here;
// it recorded the tree as it stood between the two commits. The floor removed
// the breach, so the normal expectation is restored under its original name.
TEST_F(RuntimeDetectionRatesTest, CleanFrameStaysUnderTheOnePercentCeiling) {
    const Rates r = measure(10.0f, 5.0f, 20260911u);
    EXPECT_LT(r.falsePos, static_cast<size_t>(kN / 100))
        << "REQ-P1A-013 hard ceiling: 1% of the frame";
    RecordProperty("flagged_clean", std::to_string(r.falsePos));
}

} // namespace
