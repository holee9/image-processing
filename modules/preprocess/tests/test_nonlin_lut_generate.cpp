/**
 * @file test_nonlin_lut_generate.cpp
 * @brief xpe_calib_generate_nonlin_lut -- SRS-CALIB-FUNC-006-EXT 6a, QA-A-110 (#186)
 *
 * HOW THE EXPECTED VALUES ARE OBTAINED, AND WHY IT IS NOT THE PRODUCTION CODE.
 *
 * The detector response is SIMULATED with a closed-form power law
 *
 *     S_meas(D) = adc_max * (D / D_max)^gamma
 *
 * so its inverse is also closed form:
 *
 *     D(S) = D_max * (S / adc_max)^(1/gamma)
 *
 * and the value the LUT must hold at raw index r is therefore
 *
 *     LUT[r] = G_nominal * D(r)
 *
 * where G_nominal is the through-origin least-squares slope, recomputed here
 * from the dose/signal pairs with its own two-line summation. Nothing in this
 * expectation touches the Fritsch-Carlson code, the knot construction, or the
 * writer -- a bug in any of them changes the measured value and not the expected
 * one. (Comparing against a value the same spline produced would pass while the
 * spline is consistently wrong, which is the failure shape this file exists to
 * avoid.)
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xcal_format.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xcal_reader.hpp"

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

namespace {

constexpr double kGamma = 1.35;      // a clearly non-linear, monotone response
constexpr uint32_t kEntries = 4096u;
constexpr double kAdcMax = 4095.0;
constexpr double kDoseMax = 100.0;
constexpr int kLevels = 12;          // >= 10, per the requirement

/** One dose ladder and the frames that a detector with kGamma would produce. */
struct Ladder {
    std::vector<double> dose;
    std::vector<double> signal;                  // exact, integral ADU
    std::vector<std::vector<uint16_t>> pixels;
    std::vector<XpeImageBuffer> frames;
    double g_nominal = 0.0;

    static constexpr uint32_t kW = 8, kH = 8;

    /** Independent expectation: linearized ADU for a raw value. */
    double Expected(double raw) const {
        const double d = kDoseMax * std::pow(raw / kAdcMax, 1.0 / kGamma);
        return g_nominal * d;
    }
};

/**
 * Builds the ladder. The signals are chosen first, as integers spanning 5% to
 * 95% of full scale (the requirement's span), and each dose is then derived by
 * inverting the power law -- so the simulated relation holds EXACTLY at every
 * knot with no rounding slack, and a frame of identical uint16 pixels has
 * exactly that mean.
 */
Ladder MakeLadder(double gamma = kGamma) {
    Ladder L;
    L.dose.resize(kLevels);
    L.signal.resize(kLevels);
    L.pixels.resize(kLevels);
    L.frames.resize(kLevels);

    for (int i = 0; i < kLevels; ++i) {
        const double frac = 0.05 + 0.90 * static_cast<double>(i) /
                                    static_cast<double>(kLevels - 1);
        const double s = std::round(frac * kAdcMax);
        L.signal[static_cast<size_t>(i)] = s;
        L.dose[static_cast<size_t>(i)] =
            kDoseMax * std::pow(s / kAdcMax, 1.0 / gamma);

        L.pixels[static_cast<size_t>(i)].assign(
            static_cast<size_t>(Ladder::kW) * Ladder::kH,
            static_cast<uint16_t>(s));

        XpeImageBuffer& b = L.frames[static_cast<size_t>(i)];
        b = XpeImageBuffer{};
        b.data = L.pixels[static_cast<size_t>(i)].data();
        b.width = Ladder::kW;
        b.height = Ladder::kH;
        b.bitsAllocated = 16;
        b.bitsStored = 16;
        b.format = XPE_PIXEL_UINT16;
        b.dataSize = static_cast<uint32_t>(
            L.pixels[static_cast<size_t>(i)].size() * sizeof(uint16_t));
    }

    // Through-origin least squares, computed here rather than read back.
    double num = 0.0, den = 0.0;
    for (int i = 0; i < kLevels; ++i) {
        num += L.dose[static_cast<size_t>(i)] * L.signal[static_cast<size_t>(i)];
        den += L.dose[static_cast<size_t>(i)] * L.dose[static_cast<size_t>(i)];
    }
    L.g_nominal = num / den;
    return L;
}

std::string TempPath(const char* name) {
    return std::string(::testing::TempDir()) + "/a110_" + name + ".xcal";
}

/** Reads the table back out of the written file. */
std::vector<uint16_t> ReadLut(const std::string& path, XCalFileHeader* hdr_out) {
    XCalFileHeader hdr{};
    std::vector<uint8_t> cfg, payload;
    EXPECT_EQ(XPE_OK, read_xcal_file(path.c_str(), hdr, cfg, payload, false,
                                     XCAL_TYPE_NONLIN_LUT));
    if (hdr_out != nullptr) *hdr_out = hdr;
    std::vector<uint16_t> lut(payload.size() / sizeof(uint16_t));
    std::memcpy(lut.data(), payload.data(), payload.size());
    return lut;
}

XpeErrorCode Generate(const Ladder& L, const std::string& path,
                      int32_t levels = kLevels, uint32_t entries = kEntries) {
    return xpe_calib_generate_nonlin_lut(L.frames.data(), L.dose.data(), levels,
                                         nullptr, entries, path.c_str(),
                                         nullptr);
}

}  // namespace

// The load-bearing test: the table inverts the simulated nonlinearity.
TEST(NonlinLutGenerateTest, LutMatchesTheIndependentlyComputedIdealAtEveryKnot) {
    const Ladder L = MakeLadder();
    const std::string path = TempPath("knots");
    ASSERT_EQ(XPE_OK, Generate(L, path));

    const std::vector<uint16_t> lut = ReadLut(path, nullptr);
    ASSERT_EQ(kEntries, lut.size());

    for (int i = 0; i < kLevels; ++i) {
        const double raw = L.signal[static_cast<size_t>(i)];
        const double expected = L.Expected(raw);
        const double actual = static_cast<double>(lut[static_cast<size_t>(raw)]);
        // Interpolation passes through its knots, so only uint16 rounding
        // separates the two -- one ADU.
        EXPECT_NEAR(expected, actual, 1.0)
            << "knot " << i << " at raw " << raw;
    }
}

// The requirement's accuracy clause: "Maximum interpolation error requirement:
// <= 0.3% of ADC full scale at any input value".
//
// SCOPE: the measured range, lowest to highest measured point. The SPEC
// correction of 2026-09-18 (#186) makes that scope explicit -- "<= 0.3% 요구는
// 측정 구간 안에서만 판정합니다" -- and the extension above it is covered by the
// next test, which asserts monotonicity and claims no accuracy.
//
// WHY THIS CHANGED: QA-A-110 could only assert the clause up to the
// SECOND-highest point, because the withdrawn identity pin distorted the top
// measured interval. With the pin gone the whole measured range is in scope.
TEST(NonlinLutGenerateTest, InterpolationAcrossTheMeasuredRangeStaysWithinThreeTenthsOfAPercent) {
    const Ladder L = MakeLadder();
    const std::string path = TempPath("between");
    ASSERT_EQ(XPE_OK, Generate(L, path));
    const std::vector<uint16_t> lut = ReadLut(path, nullptr);

    const double tolerance = 0.003 * kAdcMax;   // 12.3 ADU
    const double lo = L.signal.front();
    const double hi = L.signal.back();
    double worst = 0.0;
    for (double raw = lo; raw <= hi; raw += 1.0) {
        const double err = std::fabs(L.Expected(raw) -
                                     static_cast<double>(lut[static_cast<size_t>(raw)]));
        if (err > worst) worst = err;
    }
    EXPECT_LE(worst, tolerance) << "worst interpolation error " << worst << " ADU";
}

/**
 * ABOVE THE HIGHEST MEASURED POINT THE TABLE IS EXTENDED, NOT PINNED.
 *
 * QA-A-110 asserted the opposite shape here: that the top interval carried a
 * large error, because step 6 pinned `LUT[ADC_max] = ADC_max` while step 3
 * fitted a line that put full scale far below it. That expectation was correct
 * about the CODE and wrong about the REQUIREMENT -- the SPEC correction (#186,
 * SPEC c293ad9) withdrew the pin: "검출기의 full scale 이 이상 직선과 같아야 할
 * 물리적 이유가 없습니다."
 *
 * What is asserted now is what the corrected requirement actually promises in
 * the extension: continuity at the hand-over, monotonicity throughout, and NO
 * accuracy claim. The file records where the extension starts so a reader can
 * tell the two regions apart without re-deriving them.
 */
TEST(NonlinLutGenerateTest, AboveTheMeasuredRangeTheTableIsExtendedAndSaysSo) {
    const Ladder L = MakeLadder();
    const std::string path = TempPath("extension");
    ASSERT_EQ(XPE_OK, Generate(L, path));

    XCalFileHeader hdr{};
    std::vector<uint8_t> cfg, payload;
    ASSERT_EQ(XPE_OK, read_xcal_file(path.c_str(), hdr, cfg, payload, false,
                                     XCAL_TYPE_NONLIN_LUT));
    std::vector<uint16_t> lut(payload.size() / sizeof(uint16_t));
    std::memcpy(lut.data(), payload.data(), payload.size());

    // The boundary is recorded, and it is just above the highest measured point.
    const std::string config(reinterpret_cast<const char*>(cfg.data()), cfg.size());
    const std::string key = "\"xcal_nonlin_extension_start\":";
    const size_t at = config.find(key);
    ASSERT_NE(std::string::npos, at);
    const uint32_t ext = static_cast<uint32_t>(
        std::stoul(config.substr(at + key.size())));
    EXPECT_EQ(static_cast<uint32_t>(L.signal.back()) + 1u, ext);

    // The upper end is NOT pinned to full scale any more; it continues the
    // fitted response, which for a compressive detector lands below it.
    EXPECT_LT(lut.back(), static_cast<uint16_t>(kAdcMax))
        << "the withdrawn identity pin appears to be back";

    // Continuity at the hand-over: the first extended entry is one step above
    // the last measured one, not a jump.
    const double step = static_cast<double>(lut[ext]) -
                        static_cast<double>(lut[ext - 1u]);
    EXPECT_GT(step, 0.0);
    EXPECT_LT(step, 0.01 * kAdcMax) << "hand-over step " << step << " ADU";

    // Monotone all the way to the end -- the only thing claimed up there.
    for (size_t i = ext; i + 1 < lut.size(); ++i) {
        ASSERT_LE(lut[i], lut[i + 1]) << "fell at index " << i;
    }
}

// "Boundary conditions: LUT[0] = 0" -- the only pin the corrected SPEC keeps.
TEST(NonlinLutGenerateTest, TheDarkEndIsPinnedToZero) {
    const Ladder L = MakeLadder();
    const std::string path = TempPath("bounds");
    ASSERT_EQ(XPE_OK, Generate(L, path));
    const std::vector<uint16_t> lut = ReadLut(path, nullptr);

    EXPECT_EQ(0u, lut.front());
}

// "Monotonicity check: LUT[i] <= LUT[i+1] for all i (enforced ...)"
TEST(NonlinLutGenerateTest, EveryEntryIsNonDecreasing) {
    const Ladder L = MakeLadder();
    const std::string path = TempPath("mono");
    ASSERT_EQ(XPE_OK, Generate(L, path));
    const std::vector<uint16_t> lut = ReadLut(path, nullptr);

    for (size_t i = 0; i + 1 < lut.size(); ++i) {
        ASSERT_LE(lut[i], lut[i + 1]) << "fell at index " << i;
    }
}

// A measured response that does not rise with dose is the requirement's
// non-monotone case: "non-monotone LUT = XPE_ERR_INVALID_CALIB_DATA".
TEST(NonlinLutGenerateTest, NonMonotoneMeasuredResponseIsRejected) {
    Ladder L = MakeLadder();
    // Swap two neighbouring frames' contents so the signal dips at one dose.
    L.pixels[5].swap(L.pixels[6]);
    L.frames[5].data = L.pixels[5].data();
    L.frames[6].data = L.pixels[6].data();

    const std::string path = TempPath("nonmono");
    EXPECT_EQ(XPE_ERR_INVALID_CALIB_DATA, Generate(L, path));
}

// "Acquire flat-field images at N >= 10 dose levels"
TEST(NonlinLutGenerateTest, FewerThanTenDoseLevelsIsRejected) {
    const Ladder L = MakeLadder();
    const std::string path = TempPath("few");
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, Generate(L, path, 9));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, Generate(L, path, 0));
}

// "LUT size: 4096 entries ... or 65536 entries"
TEST(NonlinLutGenerateTest, OnlyTheTwoSpecifiedEntryCountsAreAccepted) {
    const Ladder L = MakeLadder();
    const std::string path = TempPath("entries");
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, Generate(L, path, kLevels, 256u));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, Generate(L, path, kLevels, 1024u));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, Generate(L, path, kLevels, 65535u));
    EXPECT_EQ(XPE_OK, Generate(L, path, kLevels, 4096u));
}

// 65536 entries cannot be (65536, 1): XCAL_MAX_DIM is 4096 and the validator's
// payload check is width * height * bpp.
TEST(NonlinLutGenerateTest, SixtyFiveThousandEntriesUseA4096By16Geometry) {
    Ladder L = MakeLadder();
    // Re-scale the ladder to 16-bit full scale so the top knot stays below it.
    for (int i = 0; i < kLevels; ++i) {
        const double s = std::round(L.signal[static_cast<size_t>(i)] * 16.0);
        L.signal[static_cast<size_t>(i)] = s;
        L.pixels[static_cast<size_t>(i)].assign(
            static_cast<size_t>(Ladder::kW) * Ladder::kH,
            static_cast<uint16_t>(s));
        L.frames[static_cast<size_t>(i)].data = L.pixels[static_cast<size_t>(i)].data();
    }

    const std::string path = TempPath("wide");
    ASSERT_EQ(XPE_OK, Generate(L, path, kLevels, 65536u));

    XCalFileHeader hdr{};
    const std::vector<uint16_t> lut = ReadLut(path, &hdr);
    EXPECT_EQ(4096u, hdr.width);
    EXPECT_EQ(16u, hdr.height);
    EXPECT_EQ(65536u, lut.size());
    EXPECT_EQ(0u, lut.front());
}

// Write -> read -> the same bytes, and the generation conditions travel along.
TEST(NonlinLutGenerateTest, FileRoundTripPreservesEntriesAndRecordsTheConditions) {
    const Ladder L = MakeLadder();
    const std::string path = TempPath("roundtrip");
    ASSERT_EQ(XPE_OK, Generate(L, path));

    XCalFileHeader hdr{};
    std::vector<uint8_t> cfg, payload;
    ASSERT_EQ(XPE_OK, read_xcal_file(path.c_str(), hdr, cfg, payload, false,
                                     XCAL_TYPE_NONLIN_LUT));
    EXPECT_EQ(static_cast<uint32_t>(XCAL_TYPE_NONLIN_LUT), hdr.type);
    EXPECT_EQ(static_cast<uint32_t>(XCAL_FMT_UINT16), hdr.pixel_format);
    EXPECT_EQ(kEntries * sizeof(uint16_t), payload.size());

    const std::string config(reinterpret_cast<const char*>(cfg.data()), cfg.size());
    EXPECT_NE(std::string::npos, config.find("\"xcal_nonlin_entries\":4096"));
    EXPECT_NE(std::string::npos, config.find("\"xcal_nonlin_dose_levels\":12"));
    EXPECT_NE(std::string::npos, config.find("\"xcal_nonlin_g_nominal\""));

    // Reading twice gives the same table -- the payload is not re-derived.
    const std::vector<uint16_t> a = ReadLut(path, nullptr);
    const std::vector<uint16_t> b = ReadLut(path, nullptr);
    EXPECT_EQ(a, b);
}

// A reader that asks for a different type must not accept this file.
TEST(NonlinLutGenerateTest, TheFileIsNotReadableAsAnotherCalibrationType) {
    const Ladder L = MakeLadder();
    const std::string path = TempPath("type");
    ASSERT_EQ(XPE_OK, Generate(L, path));

    XCalFileHeader hdr{};
    std::vector<uint8_t> cfg, payload;
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID,
              read_xcal_file(path.c_str(), hdr, cfg, payload, false, XCAL_TYPE_GAIN));
}

// Null arguments and a dose ladder that does not rise.
TEST(NonlinLutGenerateTest, NullArgumentsAndNonIncreasingDoseAreRejected) {
    Ladder L = MakeLadder();
    const std::string path = TempPath("args");

    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_calib_generate_nonlin_lut(nullptr, L.dose.data(), kLevels,
                                            nullptr, kEntries, path.c_str(), nullptr));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_calib_generate_nonlin_lut(L.frames.data(), nullptr, kLevels,
                                            nullptr, kEntries, path.c_str(), nullptr));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_calib_generate_nonlin_lut(L.frames.data(), L.dose.data(), kLevels,
                                            nullptr, kEntries, nullptr, nullptr));

    L.dose[7] = L.dose[6];   // flat dose step
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, Generate(L, path));
}
