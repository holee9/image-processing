/**
 * @file test_verify_gain_semantics.cpp
 * @brief xpe_verify_gain with the gain semantics argument (QA-A-192, #220 section 3).
 *
 * SRS-CALIB-FUNC-017: "Phase 1 acceptance shall require `FlatResidualPct <= 1.0%` and target
 * `<= 0.5%` for release-hardening fixtures where gain semantics are known."
 * SRS-CALIB-FUNC-018: "System shall record calibration gain semantics as `normalized_gain`,
 * `reciprocal_gain`, or `unknown`. Unknown semantics may run exploratory validation but shall not
 * pass release gates."
 *
 * Decided 2026-10-01 (#220): xpe_verify_gain takes the semantics as an argument. When they are
 * known (normalized or reciprocal) the FlatResidualPct line is 0.5%; when they are unknown it is
 * 1.0%, as before.
 *
 * Every frame here improves clearly (about 10% before, under 1.5% after) and the gain map is
 * all-valid, so the relative axes of overall_pass are satisfied and the FlatResidualPct line is
 * the only thing that can decide the verdict.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace {

constexpr uint32_t W = 128, H = 128;
constexpr size_t N = static_cast<size_t>(W) * H;
constexpr float kLevel = 1000.0f;

XpeImageBuffer buf(void* data, XpePixelFormat fmt, uint32_t bits) {
    XpeImageBuffer b{};
    b.data = data; b.width = W; b.height = H; b.bitsAllocated = bits; b.bitsStored = bits; b.format = fmt;
    b.dataSize = static_cast<uint32_t>(N * (bits / 8));
    return b;
}

/** A frame pair whose "after" frame sits `residualPct` percent away from its mean (alternating +/-). */
struct Case {
    std::vector<uint16_t> before = std::vector<uint16_t>(N);
    std::vector<float> after = std::vector<float>(N);
    std::vector<float> gain = std::vector<float>(N, 1.0f);
    explicit Case(double residualPct) {
        for (uint32_t y = 0; y < H; ++y)
            for (uint32_t x = 0; x < W; ++x) {
                const bool up = ((x + y) % 2) == 0;
                before[y * W + x] = static_cast<uint16_t>(kLevel * (up ? 1.10f : 0.90f));
                after[y * W + x] = static_cast<float>(kLevel * (1.0 + (up ? 1.0 : -1.0) * residualPct / 100.0));
            }
    }
    XpeErrorCode run(XpeGainSemantics s, XpeCalibrationMetrics* m) {
        XpeImageBuffer b = buf(before.data(), XPE_PIXEL_UINT16, 16), a = buf(after.data(), XPE_PIXEL_FLOAT32, 32),
                       g = buf(gain.data(), XPE_PIXEL_FLOAT32, 32);
        return xpe_verify_gain(&b, &a, &g, s, m);
    }
};

struct Row { double residualPct; bool knownPasses; bool unknownPasses; };

} // namespace

class GainSemantics : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr)); }
    void TearDown() override { xpe_clear_alerts(); xpe_preprocess_shutdown(); }
};

// The line the verdict is held to depends on whether the semantics are known.
TEST_F(GainSemantics, FlatResidualLineIsHalfAPercentWhenKnownAndOnePercentWhenUnknown) {
    // known: pass iff FlatResidualPct <= 0.5 ; unknown: pass iff FlatResidualPct <= 1.0
    const Row rows[] = {
        {0.40, true,  true },
        {0.45, true,  true },
        {0.55, false, true },
        {0.80, false, true },
        {1.20, false, false},
    };
    const XpeGainSemantics known[] = {XPE_GAIN_SEMANTICS_NORMALIZED, XPE_GAIN_SEMANTICS_RECIPROCAL};
    for (const Row& r : rows) {
        Case c(r.residualPct);
        for (XpeGainSemantics s : known) {
            XpeCalibrationMetrics m{};
            ASSERT_EQ(XPE_OK, c.run(s, &m));
            // the relative axes hold, so only the FlatResidualPct line can fail this frame
            EXPECT_LT(m.prnu_after, m.prnu_before);
            EXPECT_GE(m.snr_improvement_db, 3.0);
            EXPECT_GE(m.gain_coverage, 0.99);
            EXPECT_EQ(r.knownPasses, m.overall_pass)
                << "semantics=" << s << " residual=" << m.prnu_after << "% (known semantics: line is 0.5%)";
        }
        XpeCalibrationMetrics mu{};
        ASSERT_EQ(XPE_OK, c.run(XPE_GAIN_SEMANTICS_UNKNOWN, &mu));
        EXPECT_EQ(r.unknownPasses, mu.overall_pass)
            << "semantics=unknown residual=" << mu.prnu_after << "% (unknown semantics: line is 1.0%)";
    }
}

// The argument selects the line only; it does not change what is measured.
TEST_F(GainSemantics, TheMeasuredValuesDoNotDependOnTheSemantics) {
    Case c(0.80);
    XpeCalibrationMetrics u{}, n{}, r{};
    ASSERT_EQ(XPE_OK, c.run(XPE_GAIN_SEMANTICS_UNKNOWN, &u));
    ASSERT_EQ(XPE_OK, c.run(XPE_GAIN_SEMANTICS_NORMALIZED, &n));
    ASSERT_EQ(XPE_OK, c.run(XPE_GAIN_SEMANTICS_RECIPROCAL, &r));
    EXPECT_DOUBLE_EQ(u.prnu_after, n.prnu_after);
    EXPECT_DOUBLE_EQ(u.prnu_after, r.prnu_after);
    EXPECT_DOUBLE_EQ(u.prnu_before, n.prnu_before);
    EXPECT_DOUBLE_EQ(u.snr_improvement_db, r.snr_improvement_db);
    EXPECT_EQ(u.measured_mask, n.measured_mask);
}

// A value outside the enumeration is a caller error and leaves the output untouched (REQ-P1A-005).
TEST_F(GainSemantics, AnUnknownEnumerationValueIsRejectedWithoutTouchingTheOutput) {
    Case c(0.40);
    for (int bad : {-1, 3, 99}) {
        XpeCalibrationMetrics m;
        std::memset(&m, 0xAB, sizeof m);
        XpeCalibrationMetrics before = m;
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, c.run(static_cast<XpeGainSemantics>(bad), &m)) << "value " << bad;
        EXPECT_EQ(0, std::memcmp(&before, &m, sizeof m)) << "metrics were modified for value " << bad;
    }
}
