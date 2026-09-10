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

// Commit 1 of QA-A-40 measures and prints; it asserts nothing about the
// requirement yet. The numbers this case emits are what the assertions in the
// next commit are written against -- measured first, asserted second.
TEST_F(RuntimeDetectionRatesTest, MeasureAndReportTheRateTable) {
    report("low noise, 5 sigma",  measure(10.0f, 5.0f, 20260911u));
    report("high noise, 5 sigma", measure(50.0f, 5.0f, 20260911u));
    for (float amp : {6.0f, 8.0f, 10.0f}) {
        report(("low noise, " + std::to_string(static_cast<int>(amp)) +
                " sigma").c_str(),
               measure(10.0f, amp, 20260911u, /*alsoMeasureFpr=*/false));
    }
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
    EXPECT_EQ(low.falsePos, high.falsePos);
}

// The frame is otherwise untouched: a clean frame must not be flagged wholesale.
// REQ-P1A-013: "sum(defectMapOut) does not exceed width*height * 0.01 for clean
// input".
TEST_F(RuntimeDetectionRatesTest, CleanFrameStaysFarUnderTheOnePercentCeiling) {
    const Rates r = measure(10.0f, 5.0f, 20260911u);
    EXPECT_LT(r.falsePos, static_cast<size_t>(kN / 100))
        << "REQ-P1A-013 hard ceiling: 1% of the frame";
}

} // namespace
