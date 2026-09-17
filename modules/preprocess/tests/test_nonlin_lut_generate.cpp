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
// SCOPE: between the measured points, which is what step 5 interpolates. The
// interval above the HIGHEST measured point is a separate matter and has its own
// test below -- see it for why the two cannot be checked together.
TEST(NonlinLutGenerateTest, InterpolationBetweenMeasuredPointsStaysWithinThreeTenthsOfAPercent) {
    const Ladder L = MakeLadder();
    const std::string path = TempPath("between");
    ASSERT_EQ(XPE_OK, Generate(L, path));
    const std::vector<uint16_t> lut = ReadLut(path, nullptr);

    const double tolerance = 0.003 * kAdcMax;   // 12.3 ADU
    const double lo = L.signal.front();
    const double hi = L.signal[static_cast<size_t>(kLevels - 2)];
    double worst = 0.0;
    for (double raw = lo; raw <= hi; raw += 1.0) {
        const double err = std::fabs(L.Expected(raw) -
                                     static_cast<double>(lut[static_cast<size_t>(raw)]));
        if (err > worst) worst = err;
    }
    EXPECT_LE(worst, tolerance) << "worst interpolation error " << worst << " ADU";
}

/**
 * THE TWO BOUNDARY CONDITIONS CONTRADICT THE LINEAR FIT, AND THIS RECORDS IT.
 *
 * Step 3 fits `S_ideal = G_nominal * D`; step 6 pins `LUT[ADC_max] = ADC_max`.
 * For a detector whose response is compressive, those disagree: the fit says
 * full scale should linearize to G_nominal * D_max, which is BELOW ADC_max. The
 * whole disagreement then has to be absorbed in the span above the highest
 * measured point (95% of full scale per step 1), and the tangent at the last
 * measured knot is pulled toward the identity endpoint, so the top measured
 * interval carries part of it too.
 *
 * Measured here (gamma = 1.35, 12 levels): the fit puts full scale at about 3697
 * ADU against the pinned 4095 -- roughly 10% of full scale to absorb in the last
 * 5% of the range.
 *
 * This test asserts the SHAPE of that consequence, not a tuned number: the error
 * in the top interval exceeds the error among the inner intervals. It exists so
 * the conflict cannot be mistaken for the 0.3% clause being met everywhere.
 * QA-A-110 reports it; which side gives is not this card's decision.
 */
TEST(NonlinLutGenerateTest, TheIdentityEndpointDisagreesWithTheFitAndTheTopIntervalCarriesIt) {
    const Ladder L = MakeLadder();
    const std::string path = TempPath("endpoint");
    ASSERT_EQ(XPE_OK, Generate(L, path));
    const std::vector<uint16_t> lut = ReadLut(path, nullptr);

    // The disagreement itself: what the fit says full scale should become.
    const double ideal_at_full_scale = L.Expected(kAdcMax);
    EXPECT_LT(ideal_at_full_scale, kAdcMax - 100.0)
        << "this ladder is not compressive enough to exercise the conflict";
    EXPECT_EQ(static_cast<uint16_t>(kAdcMax), lut.back()) << "step 6 still wins at the pin";

    auto worst_between = [&](double a, double b) {
        double w = 0.0;
        for (double raw = a; raw <= b; raw += 1.0) {
            const double e = std::fabs(L.Expected(raw) -
                                       static_cast<double>(lut[static_cast<size_t>(raw)]));
            if (e > w) w = e;
        }
        return w;
    };
    const double inner = worst_between(L.signal.front(),
                                       L.signal[static_cast<size_t>(kLevels - 2)]);
    const double top = worst_between(L.signal[static_cast<size_t>(kLevels - 2)],
                                     L.signal.back());
    EXPECT_GT(top, inner)
        << "inner " << inner << " ADU, top interval " << top << " ADU";
}

// "Boundary conditions: LUT[0] = 0, LUT[ADC_max] = ADC_max (identity at extremes)"
TEST(NonlinLutGenerateTest, BoundaryConditionsAreIdentityAtBothEnds) {
    const Ladder L = MakeLadder();
    const std::string path = TempPath("bounds");
    ASSERT_EQ(XPE_OK, Generate(L, path));
    const std::vector<uint16_t> lut = ReadLut(path, nullptr);

    EXPECT_EQ(0u, lut.front());
    EXPECT_EQ(static_cast<uint16_t>(kAdcMax), lut.back());
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
    EXPECT_EQ(65535u, lut.back());
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
