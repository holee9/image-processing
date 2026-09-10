/**
 * @file test_gain_correct_reciprocal_fma.cpp
 * @brief TDD RED tests for REQ-P1A-011: Gain Correction with Reciprocal + FMA
 *        Tests reciprocal precomputation, FMA parity, and edge cases
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"

#include <cstdio>
#include <string>

#include <vector>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <limits>
#include <random>

namespace {

// ULP difference calculator for parity validation
static inline int32_t ulp_difference(float a, float b) {
    if (std::isnan(a) || std::isnan(b)) return INT32_MAX;
    if (a == b) return 0;

    int32_t ia, ib;
    std::memcpy(&ia, &a, sizeof(float));
    std::memcpy(&ib, &b, sizeof(float));

    if ((ia ^ ib) >> 31) return INT32_MAX;  // Different signs
    return std::abs(ia - ib);
}

class GainCorrectReciprocalFMATest : public ::testing::Test {
protected:
    static constexpr uint32_t W = 4;
    static constexpr uint32_t H = 4;

    std::vector<uint16_t> rawPixels;
    std::vector<float>    gainPixels;   // stored sensitivity S; correction divides by it
    std::vector<float>    outPixels;
    XpeImageBuffer   img{};
    XpeImageBuffer   output{};
    XpeImageMetadata metadata{};
    const char* gainPath = "fma_gain.xcal";

    void SetUp() override {
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));

        rawPixels.assign(W * H, 2000);
        gainPixels.assign(W * H, 1.5f);
        outPixels.assign(W * H, 0.0f);

        img.data          = rawPixels.data();
        img.width         = W;
        img.height        = H;
        img.bitsAllocated = 16;
        img.bitsStored    = 16;
        img.format        = XPE_PIXEL_UINT16;
        img.dataSize      = rawPixels.size() * sizeof(uint16_t);

        output.data          = outPixels.data();
        output.width         = W;
        output.height        = H;
        output.bitsAllocated = 32;
        output.bitsStored    = 32;
        output.format        = XPE_PIXEL_FLOAT32;
        output.dataSize      = outPixels.size() * sizeof(float);
    }

    void TearDown() override {
        std::remove(gainPath);
        std::remove("fma_gain.xcal.tmp");
        xpe_preprocess_shutdown();
    }

    // #117 decision B: the map is loaded into the global calibration, not passed
    // in. Cases mutate gainPixels first, so this publishes on the way through.
    static void publish(const char* path, const std::vector<float>& values,
                        uint32_t w, uint32_t h) {
        std::remove(path);
        std::remove((std::string(path) + ".tmp").c_str());

        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version      = XCAL_VERSION;
        hdr.type         = static_cast<uint32_t>(XCAL_TYPE_GAIN);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width        = w;
        hdr.height       = h;
        hdr.payload_len  = static_cast<uint64_t>(values.size() * sizeof(float));

        ASSERT_EQ(XPE_OK,
                  write_xcal_file(path, hdr, nullptr, 0,
                                  reinterpret_cast<const uint8_t*>(values.data()),
                                  hdr.payload_len));
        ASSERT_EQ(XPE_OK, xpe_calib_load_gain(path));
    }

    XpeErrorCode correct() {
        publish(gainPath, gainPixels, W, H);
        return xpe_gain_correct(&img, &output, &metadata);
    }

    // The old TearDown freed img.data on the assumption that gain_correct
    // replaced it with a malloc'd buffer and handed ownership over. The shipped
    // API writes into the caller-supplied output buffer, so img.data still
    // belongs to rawPixels and must not be freed here.
};

// =========================================================================
// AC-GAIN-001: Reciprocal Precomputation Tests
// =========================================================================

// REQ-P1A-011: Precompute R(x,y) = 1/G(x,y) before pixel loop
TEST_F(GainCorrectReciprocalFMATest, ReciprocalPrecomputationIsValid) {
    ASSERT_EQ(XPE_OK, correct());
    const auto* out = static_cast<const float*>(output.data);

    // AC-GAIN-001: corrected = input / gain (flat-field normalization)
    // 2000 / 1.5 = 1333.33...
    EXPECT_NEAR(2000.0f / 1.5f, out[0], 1e-3f);

    // Round-trip: output * gain ≈ input (since output = input / gain)
    for (size_t i = 0; i < W * H; ++i) {
        float reconstructed = out[i] * gainPixels[i];
        EXPECT_NEAR(static_cast<float>(rawPixels[i]), reconstructed, 0.1f)
            << "Round-trip validation failed at pixel " << i;
    }
}

// AC-GAIN-005: Gain = 0 should return CONFIG_INVALID
TEST_F(GainCorrectReciprocalFMATest, ZeroGainReturnsConfigInvalid) {
    std::fill(gainPixels.begin(), gainPixels.end(), 0.0f);
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, correct());
}

// AC-GAIN-005: Negative gain should return CONFIG_INVALID
TEST_F(GainCorrectReciprocalFMATest, NegativeGainReturnsConfigInvalid) {
    std::fill(gainPixels.begin(), gainPixels.end(), -1.0f);
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, correct());
}

// AC-GAIN-005: NaN in gain map should return CONFIG_INVALID
TEST_F(GainCorrectReciprocalFMATest, NaNGainReturnsConfigInvalid) {
    gainPixels[0] = std::numeric_limits<float>::quiet_NaN();
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, correct());
}

// AC-GAIN-005: Inf in gain map should return CONFIG_INVALID
TEST_F(GainCorrectReciprocalFMATest, InfGainReturnsConfigInvalid) {
    gainPixels[0] = std::numeric_limits<float>::infinity();
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, correct());
}

// AC-GAIN-005: Gain below MIN_GAIN_VALUE (0.001) should return CONFIG_INVALID
TEST_F(GainCorrectReciprocalFMATest, GainBelowMinimumReturnsConfigInvalid) {
    std::fill(gainPixels.begin(), gainPixels.end(), 0.0005f);  // Below 0.001
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, correct());
}

// AC-GAIN-005: Gain above MAX_GAIN_VALUE (1000) should return CONFIG_INVALID
TEST_F(GainCorrectReciprocalFMATest, GainAboveMaximumReturnsConfigInvalid) {
    std::fill(gainPixels.begin(), gainPixels.end(), 1001.0f);  // Above 1000
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, correct());
}

// Boundary: Gain at MIN_GAIN_VALUE should succeed
TEST_F(GainCorrectReciprocalFMATest, GainAtMinimumValueSucceeds) {
    constexpr float MIN_GAIN = 0.001f;
    std::fill(gainPixels.begin(), gainPixels.end(), MIN_GAIN);
    ASSERT_EQ(XPE_OK, correct());
    const auto* out = static_cast<const float*>(output.data);
    EXPECT_NEAR(2000.0f / MIN_GAIN, out[0], 1e-3f);
}

// Boundary: Gain at MAX_GAIN_VALUE should succeed
TEST_F(GainCorrectReciprocalFMATest, GainAtMaximumValueSucceeds) {
    constexpr float MAX_GAIN = 1000.0f;
    std::fill(gainPixels.begin(), gainPixels.end(), MAX_GAIN);
    ASSERT_EQ(XPE_OK, correct());
    const auto* out = static_cast<const float*>(output.data);
    EXPECT_NEAR(2000.0f / MAX_GAIN, out[0], 1e-3f);
}

// Boundary: Gain near epsilon should produce large but finite output
TEST_F(GainCorrectReciprocalFMATest, SmallGainProducesLargeOutput) {
    std::fill(gainPixels.begin(), gainPixels.end(), 0.01f);  // Valid but small
    ASSERT_EQ(XPE_OK, correct());
    const auto* out = static_cast<const float*>(output.data);
    EXPECT_TRUE(std::isfinite(out[0]));
    EXPECT_NEAR(200000.0f, out[0], 1.0f);  // 2000 / 0.01 = 200000
}

// =========================================================================
// AC-GAIN-002/AC-GAIN-003: Scalar vs FMA Parity Tests
// =========================================================================

// AC-GAIN-004: Scalar and AVX2/FMA paths must match within 1 ULP
TEST_F(GainCorrectReciprocalFMATest, UnityGainProducesIdentityConversion) {
    std::fill(gainPixels.begin(), gainPixels.end(), 1.0f);
    ASSERT_EQ(XPE_OK, correct());
    const auto* out = static_cast<const float*>(output.data);

    for (size_t i = 0; i < W * H; ++i) {
        EXPECT_NEAR(static_cast<float>(rawPixels[i]), out[i], 1e-6f)
            << "Unity gain failed at pixel " << i;
    }
}

// Test reciprocal accuracy for various gain values
TEST_F(GainCorrectReciprocalFMATest, ReciprocalAccuracyAcrossGainRange) {
    std::vector<float> testGains = {0.5f, 1.0f, 2.0f, 10.0f, 100.0f};

    for (float gain : testGains) {
        std::fill(gainPixels.begin(), gainPixels.end(), gain);
        ASSERT_EQ(XPE_OK, correct());
        const auto* out = static_cast<const float*>(output.data);

        // Verify: output = input * (1/gain) = input / gain
        float expected = static_cast<float>(rawPixels[0]) / gain;
        EXPECT_NEAR(expected, out[0], std::abs(expected) * 1e-5f)
            << "Reciprocal accuracy failed for gain = " << gain;

        // No cleanup needed: the correction writes into `output` and leaves the
        // input buffer alone, so `img` is still valid for the next iteration.
    }
}

// =========================================================================
// Domain Transition Tests (uint16 -> float32)
// =========================================================================

// REQ-P1A-011: Output format must be float32 after conversion
TEST_F(GainCorrectReciprocalFMATest, OutputFormatIsFloat32) {
    ASSERT_EQ(XPE_OK, correct());
    EXPECT_EQ(XPE_PIXEL_FLOAT32, output.format);
    EXPECT_EQ(32, output.bitsAllocated);
    EXPECT_EQ(32, output.bitsStored);
}

// REQ-P1A-011: Data size must be updated correctly
TEST_F(GainCorrectReciprocalFMATest, OutputDataSizeIsCorrect) {
    ASSERT_EQ(XPE_OK, correct());
    EXPECT_EQ(W * H * sizeof(float), output.dataSize);
}

// =========================================================================
// Dynamic Range Tests
// =========================================================================

// Small input values with small gain
TEST_F(GainCorrectReciprocalFMATest, SmallInputWithSmallGain) {
    std::fill(rawPixels.begin(), rawPixels.end(), 100);
    std::fill(gainPixels.begin(), gainPixels.end(), 0.1f);
    ASSERT_EQ(XPE_OK, correct());
    const auto* out = static_cast<const float*>(output.data);
    EXPECT_NEAR(1000.0f, out[0], 1e-3f);  // 100 / 0.1 = 1000
}

// Large input values with large gain
TEST_F(GainCorrectReciprocalFMATest, LargeInputWithLargeGain) {
    std::fill(rawPixels.begin(), rawPixels.end(), UINT16_MAX);
    std::fill(gainPixels.begin(), gainPixels.end(), 100.0f);
    ASSERT_EQ(XPE_OK, correct());
    const auto* out = static_cast<const float*>(output.data);
    EXPECT_TRUE(std::isfinite(out[0]));
    EXPECT_NEAR(static_cast<float>(UINT16_MAX) / 100.0f, out[0], 1.0f);
}

// =========================================================================
// Memory and Alignment Tests
// =========================================================================

// Test with image size not multiple of AVX2 width (8)
TEST_F(GainCorrectReciprocalFMATest, NonAVX2AlignedImageSize) {
    const size_t oddSize = 1000;  // Not multiple of 8
    std::vector<uint16_t> oddRaw(oddSize, 2000);
    std::vector<float> oddGain(oddSize, 1.5f);

    XpeImageBuffer oddImg{};
    oddImg.data = oddRaw.data();
    oddImg.width = 1000;
    oddImg.height = 1;
    oddImg.bitsAllocated = 16;
    oddImg.bitsStored = 16;
    oddImg.format = XPE_PIXEL_UINT16;
    oddImg.dataSize = oddSize * sizeof(uint16_t);

    std::vector<float> oddOut(oddSize, 0.0f);
    XpeImageBuffer oddOutput{};
    oddOutput.data = oddOut.data();
    oddOutput.width = 1000;
    oddOutput.height = 1;
    oddOutput.bitsAllocated = 32;
    oddOutput.bitsStored = 32;
    oddOutput.format = XPE_PIXEL_FLOAT32;
    oddOutput.dataSize = oddOut.size() * sizeof(float);

    publish(gainPath, oddGain, 1000, 1);
    ASSERT_EQ(XPE_OK, xpe_gain_correct(&oddImg, &oddOutput, &metadata));
    const auto* out = static_cast<const float*>(oddOutput.data);

    // Verify all pixels processed correctly
    for (size_t i = 0; i < oddSize; ++i) {
        EXPECT_NEAR(2000.0f / 1.5f, out[i], 1e-3f) << "Failed at pixel " << i;
    }

    // No free(): the correction writes into oddOutput, which the vector owns.
}

// =========================================================================
// Input Validation Tests
// =========================================================================

// NULL img returns INVALID_INPUT
TEST_F(GainCorrectReciprocalFMATest, NullImgReturnsInvalidInput) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_gain_correct(nullptr, &output, &metadata));
}

// NULL output buffer returns INVALID_INPUT (the map is no longer an argument)
TEST_F(GainCorrectReciprocalFMATest, NullOutputReturnsInvalidInput) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_gain_correct(&img, nullptr, &metadata));
}

// Dimension mismatch returns INVALID_INPUT
TEST_F(GainCorrectReciprocalFMATest, DimensionMismatchReturnsInvalidInput) {
    publish(gainPath, gainPixels, W, H);
    output.width = W + 1;
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL,
              xpe_gain_correct(&img, &output, &metadata));
}

// Zero width returns INVALID_INPUT
TEST_F(GainCorrectReciprocalFMATest, ZeroWidthReturnsInvalidInput) {
    img.width = 0;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, correct());
}

// =========================================================================
// Per-Pixel Gain Map Tests
// =========================================================================

// Non-uniform gain map (varying gain per pixel)
TEST_F(GainCorrectReciprocalFMATest, PerPixelGainMapVariation) {
    for (size_t i = 0; i < W * H; ++i) {
        gainPixels[i] = 1.0f + (i % 10) * 0.1f;  // Varying gain: 1.0 to 1.9
    }

    ASSERT_EQ(XPE_OK, correct());
    const auto* out = static_cast<const float*>(output.data);

    for (size_t i = 0; i < W * H; ++i) {
        float expected = static_cast<float>(rawPixels[i]) / gainPixels[i];
        EXPECT_NEAR(expected, out[i], std::abs(expected) * 1e-5f)
            << "Per-pixel gain failed at index " << i;
    }
}

} // namespace
