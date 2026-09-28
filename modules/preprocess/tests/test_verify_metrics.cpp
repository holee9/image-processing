/**
 * @file test_verify_metrics.cpp
 * @brief Unit tests for calibration verification metrics functions
 *
 * SPEC-XPE-P1A SWU-1.12 -- Verification Metrics API
 *
 * Test cases:
 *  1. VerifyOffset_PerfectCorrection: raw=dark+signal, corrected=signal → dark_bias ≈ 0, DSNU < 1%
 *  2. VerifyOffset_ClampVerification: raw < dark case → verify floor-at-zero handling
 *  3. VerifyGain_PerfectFlatField: uniform input + uniform gain → PRNU after ≈ 0
 *  4. VerifyGain_DetectsBadGain: gain map with zeros → verify invalid_gain_count > 0
 *  5. VerifyDefect_CountsDefects: image with known defects → verify defect_count matches
 *  6. VerifyPipeline_SNRImprovement: raw noisy → corrected clean → snr_improvement_db > 0
 *  7. VerifyMetrics_NullInput: null pointers → XPE_ERR_INVALID_INPUT
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <vector>
#include <random>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

namespace {

constexpr uint32_t W = 32;
constexpr uint32_t H = 32;

// Helper to create UINT16 image buffer
struct U16ImageHelper {
    std::vector<uint16_t> pixels;
    XpeImageBuffer buf{};

    explicit U16ImageHelper(uint32_t w, uint32_t h, uint16_t fill = 0) {
        pixels.assign(static_cast<size_t>(w) * h, fill);
        std::memset(&buf, 0, sizeof(buf));
        buf.width       = w;
        buf.height      = h;
        buf.format      = XPE_PIXEL_UINT16;
        buf.bitsAllocated = 16;
        buf.bitsStored    = 16;
        buf.data        = pixels.data();
        buf.dataSize    = pixels.size() * sizeof(uint16_t);
    }

    void set(uint32_t row, uint32_t col, uint16_t val) {
        pixels[row * buf.width + col] = val;
    }

    uint16_t get(uint32_t row, uint32_t col) const {
        return pixels[row * buf.width + col];
    }
};

// Helper to create FLOAT32 image buffer
struct F32ImageHelper {
    std::vector<float> pixels;
    XpeImageBuffer buf{};

    explicit F32ImageHelper(uint32_t w, uint32_t h, float fill = 0.0f) {
        pixels.assign(static_cast<size_t>(w) * h, fill);
        std::memset(&buf, 0, sizeof(buf));
        buf.width       = w;
        buf.height      = h;
        buf.format      = XPE_PIXEL_FLOAT32;
        buf.bitsAllocated = 32;
        buf.bitsStored    = 32;
        buf.data        = pixels.data();
        buf.dataSize    = pixels.size() * sizeof(float);
    }

    void set(uint32_t row, uint32_t col, float val) {
        pixels[row * buf.width + col] = val;
    }

    float get(uint32_t row, uint32_t col) const {
        return pixels[row * buf.width + col];
    }
};

// Helper to create UINT8 defect map buffer
struct U8DefectHelper {
    std::vector<uint8_t> pixels;
    XpeImageBuffer buf{};

    explicit U8DefectHelper(uint32_t w, uint32_t h, uint8_t fill = 0) {
        pixels.assign(static_cast<size_t>(w) * h, fill);
        std::memset(&buf, 0, sizeof(buf));
        buf.width       = w;
        buf.height      = h;
        buf.format      = XPE_PIXEL_UINT8;
        buf.bitsAllocated = 8;
        buf.bitsStored    = 8;
        buf.data        = pixels.data();
        buf.dataSize    = pixels.size() * sizeof(uint8_t);
    }

    void set(uint32_t row, uint32_t col, uint8_t val) {
        pixels[row * buf.width + col] = val;
    }

    uint8_t get(uint32_t row, uint32_t col) const {
        return pixels[row * buf.width + col];
    }
};

} // anonymous namespace

class VerifyMetricsTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Initialize module
        xpe_preprocess_init(nullptr);
    }

    void TearDown() override {
        // Cleanup
        xpe_preprocess_shutdown();
    }

    XpeImageMetadata createMetadata() {
        XpeImageMetadata meta{};
        std::memset(&meta, 0, sizeof(meta));
        return meta;
    }
};

// =============================================================================
// Test 1: Verify offset correction with perfect correction
// =============================================================================
/* QA-A-157 (#219): THIS CASE WAS PINNING THE DEFECT, and its fixture never
 * exercised what its name promises.
 *
 * It used a UNIFORM raw frame, which has no distinguishable dark region, so the
 * call took the unmeasurable early return (xpe_verify_metrics.cpp:254 ff) and
 * got overall_pass = true without measuring anything. The assertions below then
 * passed on a frame where dark_bias and dsnu had been set to 0 by that branch
 * rather than computed.
 *
 * That branch exists because of this case: 86d2894 "fix(calibration): 테스트-
 * 구현 불일치 4건 수정" (2026-04-26 22:10) added it 35 minutes after the
 * feature commit, summarised as "uniform 이미지 처리 개선". #219 decided the
 * branch must report false, so the fixture is corrected here to have a real
 * dark region -- which is what "perfect offset correction" needed all along.
 */
TEST_F(VerifyMetricsTest, VerifyOffset_PerfectCorrection) {
    const uint16_t dark_level = 500;
    const uint16_t signal_level = 2000;

    // The dark ROI is "raw below the 10th percentile" (xpe_verify_metrics.cpp
    // :291-300), so the dark pixels must both be a minority AND carry spread --
    // a flat dark block puts the percentile ON its own value, `<` admits
    // nothing, and the fallback silently measures the WHOLE frame instead.
    // 20% dark with a 10-ADU jitter; the rest lit. Corrected removes the dark
    // exactly, so every dark-ROI pixel lands on 0.
    U16ImageHelper raw(W, H, 0);
    U16ImageHelper corrected(W, H, 0);
    for (uint32_t i = 0; i < W * H; ++i) {
        const bool lit = (i % 5u) != 0u;              // 80% lit, 20% dark
        const uint16_t dark = static_cast<uint16_t>(dark_level - 10 + ((i / 5u) % 10u));
        raw.set(i / W, i % W, static_cast<uint16_t>(lit ? dark + signal_level : dark));
        corrected.set(i / W, i % W, static_cast<uint16_t>(lit ? signal_level : 0));
    }

    XpeImageMetadata meta = createMetadata();
    XpeCalibrationMetrics metrics{};
    std::memset(&metrics, 0, sizeof(metrics));

    XpeErrorCode rc = xpe_verify_offset(&raw.buf, &corrected.buf, &meta, &metrics);

    ASSERT_EQ(rc, XPE_OK) << "Verify offset should succeed";

    // Dark bias should be close to 0 for perfect correction
    EXPECT_NEAR(metrics.dark_bias, 0.0, 10.0) << "Dark bias should be ~0 for perfect correction";

    // DSNU should be low (< 1% of signal)
    EXPECT_LT(metrics.dsnu, signal_level * 0.01) << "DSNU should be < 1% of signal level";

    // Overall should pass
    EXPECT_TRUE(metrics.overall_pass) << "Perfect correction should pass overall";
}

// =============================================================================
// Test 2: Verify offset correction with clamping (raw < dark)
// =============================================================================
TEST_F(VerifyMetricsTest, VerifyOffset_ClampVerification) {
    const uint16_t dark_level = 1000;
    const uint16_t signal_level = 500;

    // Raw image with some pixels below dark level
    U16ImageHelper raw(W, H, dark_level + signal_level);

    // Simulate dark current variation
    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            uint16_t variation = static_cast<uint16_t>((x + y) % 100);
            raw.set(y, x, dark_level + variation);
        }
    }

    // Dark reference
    U16ImageHelper dark(W, H, dark_level);

    // Corrected image should clamp negative values to 0
    U16ImageHelper corrected(W, H, 0);
    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            int16_t val = static_cast<int16_t>(raw.get(y, x)) - static_cast<int16_t>(dark_level);
            corrected.set(y, x, static_cast<uint16_t>(std::max<int16_t>(0, val)));
        }
    }

    XpeImageMetadata meta = createMetadata();
    XpeCalibrationMetrics metrics{};
    std::memset(&metrics, 0, sizeof(metrics));

    XpeErrorCode rc = xpe_verify_offset(&raw.buf, &corrected.buf, &meta, &metrics);

    ASSERT_EQ(rc, XPE_OK) << "Verify offset with clamping should succeed";

    // Dark bias should be small (clamping prevents negative values)
    EXPECT_GE(metrics.dark_bias, 0.0) << "Dark bias should be non-negative after clamping";

    // Some pixels should be clamped to 0
    bool has_zero_pixels = false;
    for (uint32_t i = 0; i < W * H; ++i) {
        if (corrected.pixels[i] == 0) {
            has_zero_pixels = true;
            break;
        }
    }
    EXPECT_TRUE(has_zero_pixels) << "Clamping should produce some zero pixels";
}

// =============================================================================
// Test 3: Verify gain correction with perfect flat field
// =============================================================================
TEST_F(VerifyMetricsTest, VerifyGain_PerfectFlatField) {
    const float signal_level = 2000.0f;
    const float gain_value = 1.5f;

    // Before gain: uniform offset-corrected image
    U16ImageHelper before_gain(W, H, static_cast<uint16_t>(signal_level));

    // Gain map: uniform gain
    F32ImageHelper gain_map(W, H, gain_value);

    // After gain: perfectly flat (uniform / gain)
    F32ImageHelper after_gain(W, H, signal_level / gain_value);

    XpeImageMetadata meta = createMetadata();
    XpeCalibrationMetrics metrics{};
    std::memset(&metrics, 0, sizeof(metrics));

    XpeErrorCode rc = xpe_verify_gain(&before_gain.buf, &after_gain.buf, &gain_map.buf, &metrics);

    ASSERT_EQ(rc, XPE_OK) << "Verify gain should succeed";

    // PRNU after should be very low for perfect flat field
    EXPECT_LT(metrics.prnu_after, 1.0) << "PRNU after should be < 1% for perfect flat field";

    // Flatness should be high
    EXPECT_GT(metrics.flatness_pct, 90.0) << "Flatness should be > 90% for uniform image";

    // All gain values should be valid
    EXPECT_EQ(metrics.invalid_gain_count, 0) << "No invalid gain pixels expected";

    // Overall should pass
    EXPECT_TRUE(metrics.overall_pass) << "Perfect gain correction should pass";
}

// =============================================================================
// Test 4: Verify gain detects bad gain map (zeros)
// =============================================================================
TEST_F(VerifyMetricsTest, VerifyGain_DetectsBadGain) {
    const float signal_level = 2000.0f;

    // Before gain: uniform image
    U16ImageHelper before_gain(W, H, static_cast<uint16_t>(signal_level));

    // Gain map with some zeros (invalid)
    F32ImageHelper gain_map(W, H, 1.5f);
    // Inject zeros
    for (uint32_t y = 5; y < 10; ++y) {
        for (uint32_t x = 5; x < 10; ++x) {
            gain_map.set(y, x, 0.0f);
        }
    }

    // After gain: will have inf/nan at zero gain locations
    F32ImageHelper after_gain(W, H, signal_level / 1.5f);
    for (uint32_t y = 5; y < 10; ++y) {
        for (uint32_t x = 5; x < 10; ++x) {
            after_gain.set(y, x, std::numeric_limits<float>::infinity());
        }
    }

    XpeImageMetadata meta = createMetadata();
    XpeCalibrationMetrics metrics{};
    std::memset(&metrics, 0, sizeof(metrics));

    XpeErrorCode rc = xpe_verify_gain(&before_gain.buf, &after_gain.buf, &gain_map.buf, &metrics);

    ASSERT_EQ(rc, XPE_OK) << "Verify gain should succeed even with bad gain map";

    // Should detect invalid gain pixels
    EXPECT_GT(metrics.invalid_gain_count, 0) << "Should detect zero/invalid gain pixels";

    // Overall should fail
    EXPECT_FALSE(metrics.overall_pass) << "Bad gain map should cause overall failure";
}

// =============================================================================
// Test 5: Verify defect correction counts defects
// =============================================================================
TEST_F(VerifyMetricsTest, VerifyDefect_CountsDefects) {
    const float signal_level = 1000.0f;

    // Corrected image with some defects
    F32ImageHelper corrected(W, H, signal_level);

    // Inject 10 defect pixels
    const uint32_t num_defects = 10;
    for (uint32_t i = 0; i < num_defects; ++i) {
        uint32_t x = (i * 3) % W;
        uint32_t y = (i * 5) % H;
        corrected.set(y, x, 0.0f); // Dead pixel
    }

    // Defect map marking those pixels
    U8DefectHelper defect_map(W, H, 0); // All good by default
    for (uint32_t i = 0; i < num_defects; ++i) {
        uint32_t x = (i * 3) % W;
        uint32_t y = (i * 5) % H;
        defect_map.set(y, x, 1); // Mark as defect
    }

    XpeCalibrationMetrics metrics{};
    std::memset(&metrics, 0, sizeof(metrics));

    XpeErrorCode rc = xpe_verify_defect(&corrected.buf, &defect_map.buf, &metrics);

    ASSERT_EQ(rc, XPE_OK) << "Verify defect should succeed";

    // Should count the defects
    EXPECT_EQ(metrics.defect_count, num_defects) << "Should count all marked defects";

    // Defect density should match
    double expected_density = (static_cast<double>(num_defects) / (W * H)) * 100.0;
    EXPECT_NEAR(metrics.defect_density, expected_density, 0.01)
        << "Defect density should match expected value";
}

/* ---------------------------------------------------------------------------
 * QA-A-157 (#219): a frame nobody can measure must not report as passed.
 *
 * A uniform raw frame has no dark region to find, so xpe_verify_offset cannot
 * compute dark_bias or dsnu. It used to answer overall_pass = true anyway,
 * which is the shape that let a broken fixture go green during QA-A-156 -- the
 * value was wrong and the verdict said nothing, so only someone who doubted
 * the number caught it. #219 folds that state into false, matching the gain
 * path (no valid pixels -> false).
 *
 * This is the exact input that was silent.
 * ------------------------------------------------------------------------- */
TEST_F(VerifyMetricsTest, VerifyOffset_UnmeasurableFrameDoesNotPass) {
    U16ImageHelper raw(W, H, 1500);        // uniform: no distinguishable dark
    U16ImageHelper corrected(W, H, 1500);

    XpeImageMetadata meta = createMetadata();
    XpeCalibrationMetrics m{};
    std::memset(&m, 0, sizeof(m));

    ASSERT_EQ(XPE_OK, xpe_verify_offset(&raw.buf, &corrected.buf, &meta, &m))
        << "an unmeasurable frame is not an error -- the call still succeeds";
    EXPECT_FALSE(m.overall_pass)
        << "nothing was measured here, so this must not read as a pass";

    // The metrics here are the branch's placeholders, not measurements.
    // Asserted so a future reader does not mistake them for computed values.
    EXPECT_DOUBLE_EQ(0.0, m.dark_bias);
    EXPECT_DOUBLE_EQ(0.0, m.dsnu);
}

/* ---------------------------------------------------------------------------
 * QA-A-156 (#220): the estimator, pinned at the point where it used to change
 * the verdict.
 *
 * FlatResidualPct is defined on the ARITHMETIC mean -- Preprocessing-E2E-
 * Automated-Evaluation-Protocol.md:218. The implementation used to pass the
 * MEDIAN into compute_std, which moved both the centre of the deviation sum
 * and the divisor. QA-A-155 measured the gap across 23 inputs and found one
 * that straddled the 1.0% gate: 10% of pixels lifted by +3.2% gives
 *
 *     protocol  0.9569  PASS      implementation (median)  1.0118  FAIL
 *
 * so a panel the requirement accepted was rejected. This case fixes that
 * input. It fails if the median is reinstated anywhere on this path -- the
 * divisor or the deviation centre, either one.
 * ------------------------------------------------------------------------- */
TEST_F(VerifyMetricsTest, VerifyGain_FlatResidualUsesTheArithmeticMean) {
    constexpr uint32_t TW = 128, TH = 128;
    const float base = 1000.0f;

    U16ImageHelper before(TW, TH, static_cast<uint16_t>(base));
    F32ImageHelper gain(TW, TH, 1.0f);
    F32ImageHelper after(TW, TH, base);
    // 10% of the frame lifted by +3.2%: skewed, so mean and median disagree.
    for (uint32_t i = 0; i < (TW * TH) / 10u; ++i) {
        after.set(i / TW, i % TW, base * 1.032f);
    }

    XpeCalibrationMetrics m{};
    std::memset(&m, 0, sizeof(m));
    ASSERT_EQ(XPE_OK, xpe_verify_gain(&before.buf, &after.buf, &gain.buf, &m));

    EXPECT_NEAR(0.9569, m.prnu_after, 0.0050)
        << "FlatResidualPct must be std/mean*100 about the arithmetic mean; "
        << "the median form gives ~1.0118 here";
    EXPECT_LE(m.prnu_after, 1.0)
        << "SRS-CALIB-FUNC-017 accepts this panel (protocol value 0.9569%)";
}

/* QA-A-156 (#220): the same substitution on the offset path, where the centre
 * is not a ratio but the reported value itself -- DarkBias is gated directly
 * at 5 ADU (SRS-CALIB-FUNC-016, the one threshold QA-A-152 found a source for).
 * Protocol.md:203 defines it as mean(Y_dark_roi).
 *
 * The dark region here is deliberately skewed so mean and median separate. */
TEST_F(VerifyMetricsTest, VerifyOffset_DarkBiasUsesTheArithmeticMean) {
    // raw MUST vary: a uniform raw frame takes the early return at
    // xpe_verify_metrics.cpp:199, which reports dark_bias = 0 and
    // overall_pass = true without measuring (QA-A-152 reported that branch).
    // A first version of this case used a uniform raw and failed there -- the
    // assertion was right, the fixture was not.
    U16ImageHelper raw(W, H, 0);
    U16ImageHelper corrected(W, H, 0);
    for (uint32_t i = 0; i < W * H; ++i) {
        raw.set(i / W, i % W, static_cast<uint16_t>(400 + (i % 200)));
        // Every tenth pixel sits at +30 ADU, so whichever 10% slice the
        // implementation selects carries the same skew: mean 3.0, median 0.
        corrected.set(i / W, i % W, static_cast<uint16_t>((i % 10u == 0u) ? 30 : 0));
    }

    XpeImageMetadata meta = createMetadata();
    XpeCalibrationMetrics m{};
    std::memset(&m, 0, sizeof(m));
    ASSERT_EQ(XPE_OK, xpe_verify_offset(&raw.buf, &corrected.buf, &meta, &m));

    // The dark region is the bottom 10% of the raw histogram; raw is uniform
    // here, so the region is a slice of a uniform frame and the corrected
    // values in it carry the skew above. What the case pins is that the
    // reported centre is the arithmetic mean of that region, not its median.
    EXPECT_GT(m.dark_bias, 0.0)
        << "the median of this region is 0; the arithmetic mean is not";
}

/* ---------------------------------------------------------------------------
 * QA-A-154 (#218): a real improvement that still misses the requirement.
 *
 * SRS-CALIB-FUNC-017 gates xpe_verify_gain on FlatResidualPct <= 1.0%.
 * Before this gate existed, overall_pass was decided only on RELATIVE terms
 * (PRNU went down, SNR improved by >= 3 dB, coverage >= 99%), so a correction
 * that took a panel from 10% non-uniformity to 5% passed: a 6 dB improvement,
 * and five times the residual the requirement allows.
 *
 * That is the input this case builds. It is the falsification the card asked
 * for -- if no such input existed, the two axes would be measuring the same
 * thing and the new gate would be redundant.
 *
 * FlatResidualPct is not a new number: the protocol defines it as
 * 100 * PRNU_CV, and prnu_after is std/mean*100 already.
 * ------------------------------------------------------------------------- */
TEST_F(VerifyMetricsTest, VerifyGain_ImprovedButStillAboveOnePercentFails) {
    const float mean_level = 1000.0f;

    // Raw: +/-10% non-uniformity (alternating), i.e. PRNU ~ 10%.
    // Corrected: +/-5%. Genuine improvement (6 dB), still 5x the 1% allowed.
    U16ImageHelper raw(W, H, static_cast<uint16_t>(mean_level));   // the "before" buffer is UINT16
    F32ImageHelper corrected(W, H, mean_level);
    F32ImageHelper gain(W, H, 1.0f);
    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            const bool up = ((x + y) % 2) == 0;
            raw.set(y, x, static_cast<uint16_t>(mean_level * (up ? 1.10f : 0.90f)));
            corrected.set(y, x, mean_level * (up ? 1.05f : 0.95f));
        }
    }

    XpeCalibrationMetrics m{};
    std::memset(&m, 0, sizeof(m));
    ASSERT_EQ(XPE_OK, xpe_verify_gain(&raw.buf, &corrected.buf, &gain.buf, &m));

    // The relative axes are satisfied -- this is what used to carry the pass.
    EXPECT_LT(m.prnu_after, m.prnu_before) << "PRNU must actually improve";
    EXPECT_GE(m.snr_improvement_db, 3.0)   << "improvement must clear the SNR bar";
    EXPECT_GE(m.gain_coverage, 0.99)       << "gain map is all-valid here";

    // And the requirement's own criterion is not.
    EXPECT_GT(m.prnu_after, 1.0)
        << "this fixture is built to sit above FlatResidualPct 1.0% (got "
        << m.prnu_after << ")";
    EXPECT_FALSE(m.overall_pass)
        << "SRS-CALIB-FUNC-017 requires FlatResidualPct <= 1.0%; residual was "
        << m.prnu_after << "% with " << m.snr_improvement_db << " dB improvement";
}

/* ---------------------------------------------------------------------------
 * QA-A-153 (#217): the defect gate itself, at three densities.
 *
 * NOTHING ASSERTED `overall_pass` FOR THIS PATH BEFORE. The four existing
 * overall_pass assertions in this file cover offset (:161), gain (:250, :291)
 * and pipeline (:368); the defect case above checks `defect_count` and
 * `defect_density` and stops there. So the gate ran unverified, which is how a
 * unit error in it survived.
 *
 * The middle row is the one that matters: SRS-CALIB-FUNC-003 allows "Maximum
 * 5% defect density", so a panel at 1% is a panel the requirement accepts.
 * Before the QA-A-153 fix it was rejected, because the metric is a percent
 * (count/pixels*100) while the constant was a fraction (0.05) -- a 0.05% gate.
 * ------------------------------------------------------------------------- */
TEST_F(VerifyMetricsTest, VerifyDefect_GateMatchesTheFivePercentRequirement) {
    struct Case { double pct; bool expect_pass; const char* why; };
    const Case cases[] = {
        {0.01, true,  "far below the 5% tolerance"},
        {1.00, true,  "1% is inside the 5% the requirement allows"},
        {10.0, false, "10% exceeds the 5% tolerance"},
    };

    // The shared 32x32 fixture cannot express 0.01% (one pixel is 0.098%),
    // so this case sizes its own image: 200x200 makes 0.01% exactly 4 pixels.
    constexpr uint32_t TW = 200, TH = 200;

    for (const Case& c : cases) {
        const uint32_t defects = static_cast<uint32_t>(TW * TH * c.pct / 100.0);
        ASSERT_GT(defects, 0u) << "case " << c.pct << "% must mark at least one pixel";

        F32ImageHelper corrected(TW, TH, 1000.0f);
        U8DefectHelper defect_map(TW, TH, 0);
        for (uint32_t i = 0; i < defects; ++i) {
            corrected.set(i / TW, i % TW, 0.0f);
            defect_map.set(i / TW, i % TW, 1);
        }

        XpeCalibrationMetrics metrics{};
        std::memset(&metrics, 0, sizeof(metrics));
        ASSERT_EQ(XPE_OK, xpe_verify_defect(&corrected.buf, &defect_map.buf, &metrics));

        EXPECT_NEAR(c.pct, metrics.defect_density, 0.05)
            << "density is reported in percent (" << c.pct << "% case)";
        EXPECT_EQ(c.expect_pass, metrics.overall_pass)
            << "at " << c.pct << "% density: " << c.why
            << " (reported density " << metrics.defect_density << ")";
    }
}

// =============================================================================
// Test 6: Verify pipeline SNR improvement
// =============================================================================
TEST_F(VerifyMetricsTest, VerifyPipeline_SNRImprovement) {
    // Add noise to raw image
    U16ImageHelper raw(W, H, 1000);

    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<float> noise(0.0f, 50.0f); // 50 ADU noise

    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            float noisy_val = static_cast<float>(raw.get(y, x)) + noise(gen);
            raw.set(y, x, static_cast<uint16_t>(std::max(0.0f, noisy_val)));
        }
    }

    // Final processed image (clean)
    F32ImageHelper final(W, H, 1000.0f);

    XpeImageMetadata meta = createMetadata();
    XpeCalibrationMetrics metrics{};
    std::memset(&metrics, 0, sizeof(metrics));

    XpeErrorCode rc = xpe_verify_pipeline(&raw.buf, &final.buf, &meta, &metrics);

    ASSERT_EQ(rc, XPE_OK) << "Verify pipeline should succeed";

    // SNR improvement should be positive (cleaning improves SNR)
    EXPECT_GT(metrics.snr_improvement_db, 0.0) << "SNR improvement should be positive";

    // Overall should pass for good cleaning
    EXPECT_TRUE(metrics.overall_pass) << "Good pipeline should pass overall";
}

// =============================================================================
// Test 7: Null input validation
// =============================================================================
TEST_F(VerifyMetricsTest, VerifyMetrics_NullInput) {
    U16ImageHelper raw(W, H, 1000);
    U16ImageHelper corrected(W, H, 500);
    F32ImageHelper final(W, H, 1000.0f);
    F32ImageHelper gain_map(W, H, 1.5f);
    U8DefectHelper defect_map(W, H, 0);

    XpeImageMetadata meta = createMetadata();
    XpeCalibrationMetrics metrics{};
    std::memset(&metrics, 0, sizeof(metrics));

    // Test xpe_verify_offset with null
    EXPECT_EQ(xpe_verify_offset(nullptr, &corrected.buf, &meta, &metrics),
              XPE_ERR_INVALID_INPUT) << "Null raw_image should return INVALID_INPUT";

    EXPECT_EQ(xpe_verify_offset(&raw.buf, nullptr, &meta, &metrics),
              XPE_ERR_INVALID_INPUT) << "Null corrected_image should return INVALID_INPUT";

    EXPECT_EQ(xpe_verify_offset(&raw.buf, &corrected.buf, &meta, nullptr),
              XPE_ERR_INVALID_INPUT) << "Null metrics should return INVALID_INPUT";

    // Test xpe_verify_gain with null
    EXPECT_EQ(xpe_verify_gain(nullptr, &final.buf, &gain_map.buf, &metrics),
              XPE_ERR_INVALID_INPUT) << "Null before_gain should return INVALID_INPUT";

    EXPECT_EQ(xpe_verify_gain(&raw.buf, nullptr, &gain_map.buf, &metrics),
              XPE_ERR_INVALID_INPUT) << "Null after_gain should return INVALID_INPUT";

    EXPECT_EQ(xpe_verify_gain(&raw.buf, &final.buf, &gain_map.buf, nullptr),
              XPE_ERR_INVALID_INPUT) << "Null metrics should return INVALID_INPUT";

    // Test xpe_verify_defect with null
    EXPECT_EQ(xpe_verify_defect(nullptr, &defect_map.buf, &metrics),
              XPE_ERR_INVALID_INPUT) << "Null corrected_image should return INVALID_INPUT";

    EXPECT_EQ(xpe_verify_defect(&final.buf, nullptr, &metrics),
              XPE_ERR_INVALID_INPUT) << "Null defect_map should return INVALID_INPUT";

    EXPECT_EQ(xpe_verify_defect(&final.buf, &defect_map.buf, nullptr),
              XPE_ERR_INVALID_INPUT) << "Null metrics should return INVALID_INPUT";

    // Test xpe_verify_pipeline with null
    EXPECT_EQ(xpe_verify_pipeline(nullptr, &final.buf, &meta, &metrics),
              XPE_ERR_INVALID_INPUT) << "Null raw_image should return INVALID_INPUT";

    EXPECT_EQ(xpe_verify_pipeline(&raw.buf, nullptr, &meta, &metrics),
              XPE_ERR_INVALID_INPUT) << "Null final_image should return INVALID_INPUT";

    EXPECT_EQ(xpe_verify_pipeline(&raw.buf, &final.buf, &meta, nullptr),
              XPE_ERR_INVALID_INPUT) << "Null metrics should return INVALID_INPUT";
}

// =============================================================================
// Test 8: Dimension mismatch validation
// =============================================================================
TEST_F(VerifyMetricsTest, VerifyMetrics_DimensionMismatch) {
    U16ImageHelper raw(W, H, 1000);
    U16ImageHelper corrected(W + 1, H, 500); // Different width

    XpeImageMetadata meta = createMetadata();
    XpeCalibrationMetrics metrics{};
    std::memset(&metrics, 0, sizeof(metrics));

    EXPECT_EQ(xpe_verify_offset(&raw.buf, &corrected.buf, &meta, &metrics),
              XPE_ERR_BUFFER_TOO_SMALL) << "Dimension mismatch should return BUFFER_TOO_SMALL";
}
