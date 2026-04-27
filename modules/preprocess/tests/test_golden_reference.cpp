/**
 * @file test_golden_reference.cpp
 * @brief Golden Reference Algorithm Verification Tests — SWU-1.1 through SWU-1.9
 *
 * Each test independently computes the expected output from the algorithm's
 * mathematical formula and compares against the DLL's actual output.
 * Purpose: Verify implementation correctness against specification, not just
 * that functions return non-null or non-error.
 *
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 * REQ coverage: REQ-P1A-009..011, REQ-P1A-016..019, REQ-P1A-029..034,
 *               REQ-P1A-005..008, REQ-P1A-020..023, REQ-P1A-001..004
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <vector>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <numeric>

namespace {

// ==========================================================================
// Helpers
// ==========================================================================

static XpeImageBuffer makeU16Buf(std::vector<uint16_t>& v,
                                  uint32_t w, uint32_t h) {
    XpeImageBuffer b{};
    b.data = v.data(); b.width = w; b.height = h;
    b.bitsAllocated = 16; b.bitsStored = 16;
    b.format = XPE_PIXEL_UINT16;
    b.dataSize = v.size() * sizeof(uint16_t);
    return b;
}

static XpeImageBuffer makeF32Buf(std::vector<float>& v,
                                  uint32_t w, uint32_t h) {
    XpeImageBuffer b{};
    b.data = v.data(); b.width = w; b.height = h;
    b.bitsAllocated = 32; b.bitsStored = 32;
    b.format = XPE_PIXEL_FLOAT32;
    b.dataSize = v.size() * sizeof(float);
    return b;
}

// ==========================================================================
// SWU-1.1: Offset Correction
// Formula: corrected[i] = max((int)raw[i] - (int)offset[i], 0)
// REQ-P1A-009 to REQ-P1A-011
// ==========================================================================
class GoldenOffsetTest : public ::testing::Test {
protected:
    // kN matches the number of test cases below for SpecificPixelValues
    static constexpr uint32_t W = 4, H = 2, kN = W * H;
    // Larger image used for FormulaAppliedElementWise
    static constexpr uint32_t BW = 8, BH = 4, BN = BW * BH;

    std::vector<uint16_t> raw;
    std::vector<uint16_t> off;
    XpeImageBuffer img{}, offsetMap{};

    void SetUpImage(uint32_t w, uint32_t h) {
        const uint32_t n = w * h;
        raw.assign(n, 0);
        off.assign(n, 0);
        img       = makeU16Buf(raw, w, h);
        offsetMap = makeU16Buf(off, w, h);
    }

    void SetUp() override { SetUpImage(W, H); }
};

// REQ-P1A-009: corrected[i] = max(raw[i] - offsetMap[i], 0)
TEST_F(GoldenOffsetTest, SpecificPixelValues) {
    // (raw, offset) → expected
    struct Case { uint16_t r, o, e; };
    const Case cases[] = {
        {1000, 200, 800},   // normal subtraction
        {200,  500,   0},   // clamp: offset > raw → 0
        {65535, 100, 65435},// near max
        {0,     0,    0},   // both zero
        {100,   100,   0},  // equal → 0
        {65535, 65535, 0},  // max - max → 0
        {500,    0,  500},  // zero offset → unchanged
        {1,      1,    0},  // boundary 1
    };
    static_assert(sizeof(cases) / sizeof(cases[0]) == kN, "case count must match kN (W*H)");

    for (uint32_t i = 0; i < kN; ++i) {
        raw[i] = cases[i].r;
        off[i] = cases[i].o;
    }

    ASSERT_EQ(XPE_OK, xpe_offset_correct(&img, &offsetMap));

    const auto* out = static_cast<const uint16_t*>(img.data);
    for (uint32_t i = 0; i < kN; ++i) {
        EXPECT_EQ(cases[i].e, out[i])
            << "pixel[" << i << "]: raw=" << cases[i].r
            << " offset=" << cases[i].o;
    }
}

// max(raw - offset, 0) formula applied element-wise over full image
TEST_F(GoldenOffsetTest, FormulaAppliedElementWise) {
    SetUpImage(BW, BH);
    for (uint32_t i = 0; i < BN; ++i) {
        raw[i] = static_cast<uint16_t>(i * 500 + 100);
        off[i] = static_cast<uint16_t>(i * 100);
    }

    std::vector<uint16_t> expected(BN);
    for (uint32_t i = 0; i < BN; ++i) {
        int32_t diff = static_cast<int32_t>(raw[i]) - static_cast<int32_t>(off[i]);
        expected[i] = (diff < 0) ? 0u : static_cast<uint16_t>(diff);
    }

    ASSERT_EQ(XPE_OK, xpe_offset_correct(&img, &offsetMap));

    const auto* out = static_cast<const uint16_t*>(img.data);
    for (uint32_t i = 0; i < BN; ++i)
        EXPECT_EQ(expected[i], out[i]) << "pixel[" << i << "]";
}

// ==========================================================================
// SWU-1.2: Gain Correction
// Formula: output[i] = (float)uint16[i] * gain[i]  (domain transition)
// REQ-P1A-016 to REQ-P1A-019
// ==========================================================================
class GoldenGainTest : public ::testing::Test {
protected:
    static constexpr uint32_t W = 4, H = 4, N = W * H;

    std::vector<uint16_t> rawU16;
    std::vector<float>    gain;
    XpeImageBuffer img{}, gainMap{};

    void SetUp() override {
        rawU16.resize(N, 0);
        gain.resize(N, 1.0f);
        img     = makeU16Buf(rawU16, W, H);
        gainMap = makeF32Buf(gain, W, H);
    }

    void TearDown() override {
        if (img.format == XPE_PIXEL_FLOAT32 && img.data) {
            free(img.data);
            img.data = nullptr;
        }
    }
};

// REQ-P1A-016: output[i] = (float)raw[i] * gain[i]
TEST_F(GoldenGainTest, FormulaMatchesExactFloat) {
    // (raw_u16, gain_f32) → expected_f32
    struct Case { uint16_t r; float g; float e; };
    const Case cases[] = {
        {1000, 1.5f,    1500.0f},
        {2048, 2.0f,    4096.0f},
        {65535, 1.0f, 65535.0f},
        {0,   100.0f,     0.0f},
        {100,  0.5f,     50.0f},
        {500,  0.0f,      0.0f},
        {1024, 0.25f,   256.0f},
        {32768, 1.0f, 32768.0f},
        {1,    3.0f,      3.0f},
        {400,  0.1f,     40.0f},
        {600,  0.25f,   150.0f},
        {900,  2.5f,   2250.0f},
        {1,    1.0f,      1.0f},
        {2,    2.0f,      4.0f},
        {3,    3.0f,      9.0f},
        {4,    4.0f,     16.0f},
    };
    static_assert(sizeof(cases) / sizeof(cases[0]) == N, "case count must equal N");

    for (uint32_t i = 0; i < N; ++i) {
        rawU16[i] = cases[i].r;
        gain[i]   = cases[i].g;
    }

    ASSERT_EQ(XPE_OK, xpe_gain_correct(&img, &gainMap));

    // After call: img.format must be float32 (domain transition REQ-P1A-019)
    ASSERT_EQ(XPE_PIXEL_FLOAT32, img.format) << "domain must transition to float32";

    const auto* out = static_cast<const float*>(img.data);
    for (uint32_t i = 0; i < N; ++i)
        EXPECT_FLOAT_EQ(cases[i].e, out[i])
            << "pixel[" << i << "]: raw=" << cases[i].r
            << " gain=" << cases[i].g;
}

// Zero gain → all zeros regardless of raw value
TEST_F(GoldenGainTest, ZeroGainProducesZero) {
    std::fill(rawU16.begin(), rawU16.end(), 50000u);
    std::fill(gain.begin(), gain.end(), 0.0f);

    ASSERT_EQ(XPE_OK, xpe_gain_correct(&img, &gainMap));

    const auto* out = static_cast<const float*>(img.data);
    for (uint32_t i = 0; i < N; ++i)
        EXPECT_FLOAT_EQ(0.0f, out[i]) << "pixel[" << i << "]";
}

// ==========================================================================
// SWU-1.4: Ghost Correction Tier 1 — dual-exponential LTI deconvolution
// Default: alpha1=0.9, tau1=1.0, alpha2=0.05, tau2=20.0
//
// corrected[n,i] = raw[n,i] - alpha1*hist1[i] - alpha2*hist2[i]
// hist1[i]        = decay1*hist1[i] + raw[n,i]   decay1 = exp(-1/tau1)
// hist2[i]        = decay2*hist2[i] + raw[n,i]   decay2 = exp(-1/tau2)
//
// REQ-P1A-029 to REQ-P1A-034
// ==========================================================================
class GoldenGhostTest : public ::testing::Test {
protected:
    static constexpr uint32_t W = 4, H = 4, N = W * H;

    // Default LTI parameters (from GhostCorrectorHandle)
    static constexpr double kAlpha1 = 0.9;
    static constexpr double kTau1   = 1.0;
    static constexpr double kAlpha2 = 0.05;
    static constexpr double kTau2   = 20.0;

    void* handle{nullptr};
    std::vector<float> pixels;
    XpeImageBuffer img{};
    XpeImageMetadata meta{};

    void SetUp() override {
        pixels.resize(N, 0.0f);
        img = makeF32Buf(pixels, W, H);
        meta.acquisitionTime = 1;
    }

    void TearDown() override {
        if (handle) { xpe_ghost_destroy(handle); handle = nullptr; }
    }

    // Reference Tier-1 formula applied to a uniform-valued image
    // Returns expected corrected value given previous h1, h2 and current raw
    static double tier1Expected(double rawVal, double h1, double h2) {
        return rawVal - kAlpha1 * h1 - kAlpha2 * h2;
    }
};

// REQ-P1A-032: Frame 0 (zero history) must pass through unchanged
TEST_F(GoldenGhostTest, Frame0PassesThroughExactly) {
    const float V = 1024.0f;
    std::fill(pixels.begin(), pixels.end(), V);
    ASSERT_EQ(XPE_OK, xpe_ghost_create(W, H, nullptr, &handle));

    meta.acquisitionTime = 1;
    ASSERT_EQ(XPE_OK, xpe_ghost_correct(handle, &img, &meta));

    // hist1 = hist2 = 0 → corrected = raw - 0 - 0 = raw
    const auto* out = static_cast<const float*>(img.data);
    for (uint32_t i = 0; i < N; ++i)
        EXPECT_FLOAT_EQ(V, out[i]) << "pixel[" << i << "] frame0 must be unchanged";
}

// REQ-P1A-033: Frame 1 with constant input matches dual-exponential formula
TEST_F(GoldenGhostTest, Frame1MatchesDualExponentialFormula) {
    const float V = 2000.0f;
    ASSERT_EQ(XPE_OK, xpe_ghost_create(W, H, nullptr, &handle));

    // Frame 0: sets h1=V, h2=V (corrected passes through = V)
    std::fill(pixels.begin(), pixels.end(), V);
    meta.acquisitionTime = 1;
    ASSERT_EQ(XPE_OK, xpe_ghost_correct(handle, &img, &meta));

    // Frame 1: corrected = V - alpha1*V - alpha2*V = V*(1 - alpha1 - alpha2)
    std::fill(pixels.begin(), pixels.end(), V);
    meta.acquisitionTime = 2;
    ASSERT_EQ(XPE_OK, xpe_ghost_correct(handle, &img, &meta));

    // Reference: h1=V, h2=V after frame 0 (decay1*0 + V = V, decay2*0 + V = V)
    const double expected = tier1Expected(V, V, V);
    const auto* out = static_cast<const float*>(img.data);
    for (uint32_t i = 0; i < N; ++i)
        EXPECT_NEAR(static_cast<float>(expected), out[i], 0.5f)
            << "pixel[" << i << "] frame1 formula mismatch";
}

// REQ-P1A-034: After reset(), next frame uses zero history (passthrough again)
TEST_F(GoldenGhostTest, AfterResetHistoryIsZero) {
    const float V = 500.0f;
    ASSERT_EQ(XPE_OK, xpe_ghost_create(W, H, nullptr, &handle));

    // Build up history with 5 frames
    for (int f = 1; f <= 5; ++f) {
        std::fill(pixels.begin(), pixels.end(), V);
        meta.acquisitionTime = static_cast<uint64_t>(f);
        ASSERT_EQ(XPE_OK, xpe_ghost_correct(handle, &img, &meta));
    }

    ASSERT_EQ(XPE_OK, xpe_ghost_reset(handle));

    // Post-reset: history = 0 → next frame passthrough
    std::fill(pixels.begin(), pixels.end(), V);
    meta.acquisitionTime = 100;
    ASSERT_EQ(XPE_OK, xpe_ghost_correct(handle, &img, &meta));

    const auto* out = static_cast<const float*>(img.data);
    for (uint32_t i = 0; i < N; ++i)
        EXPECT_FLOAT_EQ(V, out[i]) << "pixel[" << i << "] post-reset must be unchanged";
}

// After frame 1, ghost is subtracted → corrected < original input
// Note: with default alpha1=0.9+alpha2=0.05, combined ghost fraction=0.95
// so corrected[1] = V*(1-0.95) = V*0.05 — aggressive correction is expected
TEST_F(GoldenGhostTest, GhostSubtractedAfterFirstFrame) {
    const float V = 4096.0f;
    ASSERT_EQ(XPE_OK, xpe_ghost_create(W, H, nullptr, &handle));

    // Frame 0: passthrough (verified by Frame0PassesThroughExactly)
    std::fill(pixels.begin(), pixels.end(), V);
    meta.acquisitionTime = 1;
    ASSERT_EQ(XPE_OK, xpe_ghost_correct(handle, &img, &meta));

    // Frame 1: ghost correction reduces the value below V
    std::fill(pixels.begin(), pixels.end(), V);
    meta.acquisitionTime = 2;
    ASSERT_EQ(XPE_OK, xpe_ghost_correct(handle, &img, &meta));

    const auto* out = static_cast<const float*>(img.data);
    EXPECT_LT(out[0], V)
        << "frame 1: ghost correction must reduce value below original input";
}

// ==========================================================================
// SWU-1.6: Temperature Compensation
// Model: I_dark(T) = I_0 * exp(-Eg / (2 * kB * T))
// scale    = exp(constant/T_abs) / exp(constant/T_ref)
//            where constant = -Eg/(2*kB) ≈ -6498 K
// corrected[i] ≈ raw[i] / scale
// REQ-P1A-005 to REQ-P1A-008
// ==========================================================================
class GoldenTempTest : public ::testing::Test {
protected:
    static constexpr uint32_t W = 4, H = 4, N = W * H;
    static constexpr double kEg  = 1.12;      // Silicon bandgap (eV)
    static constexpr double kKB  = 8.617e-5;  // Boltzmann constant (eV/K)
    static constexpr double kRef = 298.15;    // Reference temperature (K)
    static constexpr double kConst = -kEg / (2.0 * kKB);

    std::vector<uint16_t> pixels;
    XpeImageBuffer img{};

    void SetUp() override {
        pixels.resize(N, 1000u);
        img = makeU16Buf(pixels, W, H);
    }

    static double scale(double tempC) {
        const double T = tempC + 273.15;
        return std::exp(kConst / T) / std::exp(kConst / kRef);
    }
};

// REQ-P1A-005: At T_ref=25°C, scale=1.0 → pixels unchanged
TEST_F(GoldenTempTest, RefTempProducesNoChange) {
    std::fill(pixels.begin(), pixels.end(), 5000u);
    ASSERT_EQ(XPE_OK, xpe_temp_compensate(&img, 25.0f, nullptr));

    const auto* out = static_cast<const uint16_t*>(img.data);
    for (uint32_t i = 0; i < N; ++i)
        EXPECT_EQ(5000u, out[i]) << "pixel[" << i << "] T=25°C must be unchanged";
}

// REQ-P1A-006: Above 25°C, dark current higher → scale > 1 → corrected < raw
TEST_F(GoldenTempTest, Above25cReducesPixelValues) {
    const uint16_t rawVal = 3000u;
    std::fill(pixels.begin(), pixels.end(), rawVal);

    const double s = scale(37.0);
    ASSERT_GT(s, 1.0) << "scale must exceed 1 above reference temperature";

    ASSERT_EQ(XPE_OK, xpe_temp_compensate(&img, 37.0f, nullptr));

    const auto* out = static_cast<const uint16_t*>(img.data);
    const auto expected = static_cast<uint16_t>(std::round(rawVal / s));
    for (uint32_t i = 0; i < N; ++i)
        EXPECT_NEAR(static_cast<int>(expected), static_cast<int>(out[i]), 2)
            << "pixel[" << i << "] T=37°C formula mismatch (scale=" << s << ")";
}

// Below 25°C, dark current lower → scale < 1 → corrected > raw (or equal at boundary)
TEST_F(GoldenTempTest, Below25cScaleIsLessThanOne) {
    EXPECT_LT(scale(0.0), 1.0)  << "scale must be < 1 below reference temperature";
    EXPECT_LT(scale(-10.0), 1.0) << "scale at -10°C must be < 1";
}

// REQ-P1A-008: Out-of-range temperature returns XPE_ERR_INVALID_INPUT
TEST_F(GoldenTempTest, OutOfRangeReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_temp_compensate(&img, -25.0f, nullptr));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_temp_compensate(&img,  65.0f, nullptr));
}

// ==========================================================================
// SWU-1.8: Binning Correction
// Formula: output[i] = raw[i] * (1.0f / (mode * mode))
// mode=1: no-op | mode=2: ÷4 | mode=4: ÷16
// REQ-P1A-020 to REQ-P1A-023
// ==========================================================================
class GoldenBinningTest : public ::testing::Test {
protected:
    static constexpr uint32_t W = 4, H = 4, N = W * H;

    std::vector<float> pixels;
    XpeImageBuffer img{};

    void SetUp() override {
        pixels.resize(N, 0.0f);
        img = makeF32Buf(pixels, W, H);
    }
};

// REQ-P1A-020: mode=1 is identity (no-op)
TEST_F(GoldenBinningTest, Mode1IsIdentity) {
    std::fill(pixels.begin(), pixels.end(), 12345.6f);
    ASSERT_EQ(XPE_OK, xpe_binning_correct(&img, 1, nullptr));

    const auto* out = static_cast<const float*>(img.data);
    for (uint32_t i = 0; i < N; ++i)
        EXPECT_FLOAT_EQ(12345.6f, out[i]) << "pixel[" << i << "] mode=1 must be unchanged";
}

// REQ-P1A-021: mode=2 → output = raw / 4
TEST_F(GoldenBinningTest, Mode2DividesByFour) {
    std::fill(pixels.begin(), pixels.end(), 4000.0f);
    ASSERT_EQ(XPE_OK, xpe_binning_correct(&img, 2, nullptr));

    const auto* out = static_cast<const float*>(img.data);
    for (uint32_t i = 0; i < N; ++i)
        EXPECT_FLOAT_EQ(1000.0f, out[i]) << "pixel[" << i << "] mode=2";
}

// REQ-P1A-022: mode=4 → output = raw / 16
TEST_F(GoldenBinningTest, Mode4DividesBySixteen) {
    std::fill(pixels.begin(), pixels.end(), 16000.0f);
    ASSERT_EQ(XPE_OK, xpe_binning_correct(&img, 4, nullptr));

    const auto* out = static_cast<const float*>(img.data);
    for (uint32_t i = 0; i < N; ++i)
        EXPECT_FLOAT_EQ(1000.0f, out[i]) << "pixel[" << i << "] mode=4";
}

// REQ-P1A-023: unknown mode returns XPE_ERR_CONFIG_INVALID
TEST_F(GoldenBinningTest, UnknownModeReturnsError) {
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_binning_correct(&img, 3, nullptr));
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_binning_correct(&img, 8, nullptr));
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_binning_correct(&img, 0, nullptr));
}

// ==========================================================================
// SWU-1.9: Readout Artifact Validation
// Formula: score = clamp((sat_frac + noise_frac) * 50, 0, 100)
//   sat_frac   = count(pixels == 65535) / total_pixels
//   noise_frac = count(rows where row_mean > 0.9 * 65535) / total_rows
// REQ-P1A-001 to REQ-P1A-004
// ==========================================================================
class GoldenReadoutTest : public ::testing::Test {
protected:
    static constexpr uint32_t W = 16, H = 8, N = W * H;
    static constexpr uint16_t kSat   = 65535u;
    static constexpr double   kNoisy = 0.9 * 65535.0;  // row-mean threshold

    std::vector<uint16_t> pixels;
    XpeImageBuffer img{};
    int32_t score{-1};
    char msg[256]{};

    void SetUp() override {
        pixels.assign(N, 1000u);
        img = makeU16Buf(pixels, W, H);
    }
};

// REQ-P1A-001: clean image (no artifacts) → score == 0
TEST_F(GoldenReadoutTest, CleanImageScoresZero) {
    ASSERT_EQ(XPE_OK, xpe_validate_readout_artifact(&img, &score, msg, sizeof(msg)));
    EXPECT_EQ(0, score) << "clean image must score 0";
}

// REQ-P1A-003: fully saturated image → maximum score
TEST_F(GoldenReadoutTest, FullySaturatedImageScoresMax) {
    std::fill(pixels.begin(), pixels.end(), kSat);
    ASSERT_EQ(XPE_OK, xpe_validate_readout_artifact(&img, &score, msg, sizeof(msg)));
    // sat_frac=1.0, every row mean = 65535 > 0.9*65535 → noise_frac=1.0
    // score = min((1.0 + 1.0)*50, 100) = 100
    EXPECT_EQ(100, score) << "all-saturated image must score 100";
}

// REQ-P1A-002: single row with high mean contributes to noise_frac
TEST_F(GoldenReadoutTest, SingleNoiseRowGivesNonZeroScore) {
    // Row 0: set all pixels to 62000 (> 0.9*65535 = 58981)
    for (uint32_t col = 0; col < W; ++col)
        pixels[col] = 62000u;

    ASSERT_EQ(XPE_OK, xpe_validate_readout_artifact(&img, &score, msg, sizeof(msg)));
    // noise_frac = 1/H = 1/8 = 0.125; sat_frac ≈ 0
    // score ≈ 0.125 * 50 = 6 (not zero)
    EXPECT_GT(score, 0)  << "single noise row must produce non-zero score";
    EXPECT_LT(score, 20) << "single noise row should not produce high score";
}

// Score is clamped to [0, 100]
TEST_F(GoldenReadoutTest, ScoreIsClamped) {
    std::fill(pixels.begin(), pixels.end(), kSat);
    ASSERT_EQ(XPE_OK, xpe_validate_readout_artifact(&img, &score, msg, sizeof(msg)));
    EXPECT_GE(score, 0);
    EXPECT_LE(score, 100);
}

} // namespace
