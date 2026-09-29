/**
 * @file test_nonlin_poly_apply.cpp
 * @brief SRS-CALIB-FUNC-006-EXT 6b/6c -- polynomial nonlinearity correction
 *        and the method selector. QA-A-160 (#186)
 *
 * WHAT IS UNDER TEST AND WHAT IS NOT.
 *
 * 6b splits into a factory step (fit the polynomial; the requirement names
 * numpy.polyfit, i.e. offline) and a runtime step (evaluate it, enforce
 * monotonicity, apply). This module owns the RUNTIME step, so that is what is
 * pinned here: the coefficients arrive from the profile and the test checks the
 * module evaluates them, rejects a non-monotone one, and honours the selector.
 *
 * THE EXPECTATION IS NOT A SECOND COPY OF HORNER'S METHOD. The fixture defines
 * the linearization f() FIRST and derives everything from it, so the expected
 * value for a pixel is f(raw) computed here from the same coefficients the
 * module is handed. A wrong evaluation order, a wrong clamp, or a dropped term
 * moves the measured value and not the expectation.
 *
 * AND THE FIXTURE PROVES IT HAS SOMETHING TO CORRECT. Every residual check is
 * paired with the UNCORRECTED gap |raw - f(raw)|, asserted to exceed the same
 * 0.3% budget. Without that pair a frame with no nonlinearity in it would pass
 * the residual gate while proving nothing (QA-A-160 §3, first falsification).
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "preprocess_state_fixture.h"

#include <cmath>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr double kAdcMax = 4095.0;   // 12-bit panel, as in 6c's table
constexpr double kResidualBudget = 0.003 * kAdcMax;  // "max residual <= 0.3% ADU"

// A monotone degree-2 linearization: mild at the bottom, ~7% at mid scale.
// Derivative 0.85 + 8e-5*x > 0 everywhere, so it is monotone by construction --
// the non-monotone case gets its own coefficients below.
constexpr double kC0 = 0.0, kC1 = 0.85, kC2 = 4.0e-5, kC3 = 0.0, kC4 = 0.0;

double linearize(double x) {
    return kC0 + x * (kC1 + x * (kC2 + x * (kC3 + x * kC4)));
}

std::string profile(const std::string& extra) {
    return std::string("{\"panel.linear\":\"false\"") + extra + "}";
}

const std::string kPolyCoeffs =
    ",\"panel.nonlin_poly_c0\":0.0"
    ",\"panel.nonlin_poly_c1\":0.85"
    ",\"panel.nonlin_poly_c2\":4.0e-5"
    ",\"panel.adc_max\":4095";

struct Frame {
    std::vector<uint16_t> px;
    XpeImageBuffer buf{};

    explicit Frame(const std::vector<uint16_t>& values) : px(values) {
        std::memset(&buf, 0, sizeof(buf));
        buf.width  = static_cast<uint32_t>(px.size());
        buf.height = 1;
        buf.format = XPE_PIXEL_UINT16;
        buf.bitsAllocated = 16;
        buf.bitsStored    = 16;
        buf.data     = px.data();
        buf.dataSize = px.size() * sizeof(uint16_t);
    }
};

// Raw values spanning 5%..95% of full scale, the range 6b's step 1 calibrates.
std::vector<uint16_t> sample_raws() {
    std::vector<uint16_t> v;
    for (int pct = 5; pct <= 95; pct += 5) {
        v.push_back(static_cast<uint16_t>(kAdcMax * pct / 100.0));
    }
    return v;
}

class NonlinPolyTest : public XpePreprocessStateFixture {};

} // namespace

// 6b + the one numeric acceptance the requirement gives.
TEST_F(NonlinPolyTest, PolynomialCorrectionMeetsTheZeroPointThreePercentResidual) {
    const std::vector<uint16_t> raws = sample_raws();
    Frame frame(raws);

    // The fixture must HAVE nonlinearity, or the residual gate proves nothing.
    double worst_uncorrected = 0.0;
    for (uint16_t r : raws) {
        worst_uncorrected = std::max(worst_uncorrected,
                                     std::abs(static_cast<double>(r) - linearize(r)));
    }
    ASSERT_GT(worst_uncorrected, kResidualBudget)
        << "precondition: an uncorrected frame must MISS the 0.3% budget, "
           "otherwise passing it after correction shows nothing";

    const std::vector<uint16_t> before = frame.px;
    ASSERT_EQ(XPE_OK, xpe_nonlinearity_correct(&frame.buf,
                                               profile(std::string(",\"panel.nonlinearity_mode\":\"POLY\"")
                                                       + kPolyCoeffs).c_str()));
    EXPECT_NE(before, frame.px) << "pixels must actually have changed";

    double worst_residual = 0.0;
    for (size_t i = 0; i < raws.size(); ++i) {
        const double expected = linearize(raws[i]);
        worst_residual = std::max(worst_residual,
                                  std::abs(static_cast<double>(frame.px[i]) - expected));
    }
    EXPECT_LE(worst_residual, kResidualBudget)
        << "SRS-CALIB-FUNC-006: max residual <= 0.3% ADU (" << kResidualBudget << " ADU)";
}

// 6b step 5: non-monotone is REJECTED, and rejection means fall back to 6a.
// With no LUT loaded and panel.linear = false, the 6a path reports exactly that
// -- so this return value is evidence the fallback was taken, not skipped.
TEST_F(NonlinPolyTest, NonMonotonePolynomialIsRejectedAndFallsBackToLut) {
    Frame frame(sample_raws());
    const std::vector<uint16_t> before = frame.px;

    // c1 negative with a positive c2: decreasing at the bottom of the range.
    const std::string cfg = profile(
        ",\"panel.nonlinearity_mode\":\"POLY\""
        ",\"panel.nonlin_poly_c0\":100.0"
        ",\"panel.nonlin_poly_c1\":-1.0"
        ",\"panel.nonlin_poly_c2\":1.0e-3"
        ",\"panel.adc_max\":4095");

    const XpeErrorCode rc = xpe_nonlinearity_correct(&frame.buf, cfg.c_str());

    EXPECT_EQ(XPE_ERR_CALIB_NOT_LOADED, rc)
        << "the 6a path's answer -- reached only because 6b rejected and fell back";
    EXPECT_EQ(before, frame.px) << "a rejected polynomial must not have touched pixels";
}

// 6c: "AUTO" selects the polynomial only for MCU/FPGA targets.
TEST_F(NonlinPolyTest, AutoSelectsPolynomialOnlyForEmbeddedTargets) {
    {   // FPGA -> 6b runs
        Frame frame(sample_raws());
        const std::vector<uint16_t> before = frame.px;
        ASSERT_EQ(XPE_OK, xpe_nonlinearity_correct(&frame.buf,
                  profile(std::string(",\"panel.nonlinearity_mode\":\"AUTO\""
                                      ",\"panel.target_platform\":\"FPGA\"") + kPolyCoeffs).c_str()));
        EXPECT_NE(before, frame.px) << "AUTO on an FPGA target must take the polynomial path";
    }
    {   // CPU -> 6a, which without a loaded LUT says so
        Frame frame(sample_raws());
        const std::vector<uint16_t> before = frame.px;
        EXPECT_EQ(XPE_ERR_CALIB_NOT_LOADED,
                  xpe_nonlinearity_correct(&frame.buf,
                      profile(std::string(",\"panel.nonlinearity_mode\":\"AUTO\""
                                          ",\"panel.target_platform\":\"CPU\"") + kPolyCoeffs).c_str()));
        EXPECT_EQ(before, frame.px)
            << "AUTO on a CPU target must NOT evaluate the polynomial";
    }
}

// panel.linear governs enable/disable -- both directions, per QA-A-160 §3.
TEST_F(NonlinPolyTest, PanelLinearGovernsEnableAndDisable) {
    const std::string tail = std::string(",\"panel.nonlinearity_mode\":\"POLY\"") + kPolyCoeffs;

    {   // linear = true -> skipped entirely, even with usable coefficients
        Frame frame(sample_raws());
        const std::vector<uint16_t> before = frame.px;
        ASSERT_EQ(XPE_OK, xpe_nonlinearity_correct(&frame.buf,
                  (std::string("{\"panel.linear\":\"true\"") + tail + "}").c_str()));
        EXPECT_EQ(before, frame.px)
            << "a linear panel needs no correction, so no pixel may change";
    }
    {   // linear = false -> it runs
        Frame frame(sample_raws());
        const std::vector<uint16_t> before = frame.px;
        ASSERT_EQ(XPE_OK, xpe_nonlinearity_correct(&frame.buf,
                  (std::string("{\"panel.linear\":\"false\"") + tail + "}").c_str()));
        EXPECT_NE(before, frame.px);
    }
}

// A profile that names POLY without coefficients must not silently apply the
// zero polynomial, which would flatten the frame to 0.
TEST_F(NonlinPolyTest, PolyModeWithoutCoefficientsDoesNotZeroTheFrame) {
    Frame frame(sample_raws());
    const std::vector<uint16_t> before = frame.px;

    EXPECT_EQ(XPE_ERR_CALIB_NOT_LOADED,
              xpe_nonlinearity_correct(&frame.buf,
                  profile(",\"panel.nonlinearity_mode\":\"POLY\"").c_str()));
    EXPECT_EQ(before, frame.px);
}
