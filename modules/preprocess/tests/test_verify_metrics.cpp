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
    XpeImageBuffer buf;

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
    XpeImageBuffer buf;

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
    XpeImageBuffer buf;

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
TEST_F(VerifyMetricsTest, VerifyOffset_PerfectCorrection) {
    const uint16_t dark_level = 500;
    const uint16_t signal_level = 2000;

    // Raw image = dark + signal
    U16ImageHelper raw(W, H, dark_level + signal_level);

    // Dark reference
    U16ImageHelper dark(W, H, dark_level);

    // Corrected image = signal (perfect offset correction)
    U16ImageHelper corrected(W, H, signal_level);

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
              XPE_ERR_INVALID_INPUT) << "Dimension mismatch should return INVALID_INPUT";
}
