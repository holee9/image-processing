/**
 * @file test_gain_poly_dose_range.cpp
 * @brief The gain polynomial is evaluated only inside its fitted dose range
 *        (QA-A-123, #194).
 *
 * WHAT THIS PINS, AND WHY THE NUMBERS ARE THE ONES THEY ARE.
 *
 * QA-A-122 measured what a pixel above the fitted range actually received.
 * On a ladder whose gain turns up sharply at the top knot (0.95 -> 1.40 over
 * dose levels 14037..42677 ADU), a degree-3 fit gave a saturated pixel (65535)
 * a gain of 3.8955 -- 2.78x the gain at D_max. The size was not the worst of
 * it: the corrected output was 16823, which is LOWER than the output of a
 * D_max pixel (30485). A brighter pixel came out darker. Monotonicity
 * inverted, and it inverted exactly where direct-exposure, metal and
 * saturation live.
 *
 * That measured pair is this file's falsification. Before the clamp the
 * saturated pixel receives 3.8955; after it, it receives the D_max gain
 * (1.3999) and is no longer darker than the D_max pixel. If the clamp is
 * removed, the ordering assertion fails -- it is not a restatement of the
 * clamp, it is the property the clamp exists to restore.
 *
 * The coefficients are NOT hand-written here. The file is produced by the
 * public generator, so the test exercises the whole chain the change touches:
 * the generator records the range, the loader reads it back, and the
 * correction clamps to it. A hand-built file would test only the last step.
 *
 * TWO PATHS. A file generated before the range field existed carries no
 * bounds. It is loaded, not rejected -- rejecting would retire every existing
 * calibration -- and it raises one alert saying the clamp does not apply to
 * it. Both paths are pinned below; the second is built by writing the SAME
 * generated coefficients back out through write_xcal_file() with no
 * config_json, so it is a genuinely valid old file rather than a mock -- and
 * because the coefficients are unchanged, the measured 3.8955 is still the
 * right expectation for it.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xpe/preprocess/xcal_format.h"
#include "fixtures/make_xcal.hpp"
#include "preprocess_state_fixture.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr uint32_t W = 8, H = 8;
constexpr size_t   N = static_cast<size_t>(W) * H;

// The QA-A-122 ladder: cyan_test's CalSet levels in ADU, with a gain that
// turns up sharply at the top. This is the shape that produced the inversion.
const std::vector<double> kDoses = {14037.0, 17285.0, 20985.0, 30868.0, 42677.0};
const std::vector<float>  kGains = {0.95f,   0.96f,   0.98f,   1.05f,   1.40f};

constexpr uint16_t kDoseMax   = 42677u;  // the top knot
constexpr uint16_t kSaturated = 65535u;  // what a saturated 16-bit pixel reads

class GainPolyDoseRangeTest : public XpePreprocessStateFixture {
protected:
    fs::path dir;

    void SetUp() override {
        XpePreprocessStateFixture::SetUp();
        dir = fs::temp_directory_path() / "xpe_a123_dose_range";
        fs::remove_all(dir);
        fs::create_directories(dir);
    }

    void TearDown() override {
        std::error_code ec;
        fs::remove_all(dir, ec);
        XpePreprocessStateFixture::TearDown();
    }

    std::string p(const char* name) const { return (dir / name).string(); }

    /** Generate a real GAIN_POLY file from the ladder above. */
    std::string generatePoly(const char* name, int32_t degree) {
        std::vector<std::string> paths;
        std::vector<const char*> ptrs;
        for (size_t i = 0; i < kDoses.size(); ++i) {
            const std::string lvl = p(("a123_lvl" + std::to_string(i) + ".xcal").c_str());
            EXPECT_EQ(XPE_OK, MakeGainXCal(lvl.c_str(), W, H, kGains[i]));
            paths.push_back(lvl);
        }
        for (const auto& s : paths) ptrs.push_back(s.c_str());

        const std::string out = p(name);
        EXPECT_EQ(XPE_OK, xpe_calib_generate_gain_polynomial(
            ptrs.data(), kDoses.data(), static_cast<int32_t>(kDoses.size()),
            degree, out.c_str()));
        return out;
    }

    /** Run one uniform frame through gain correction; return the output. */
    float correctedValue(uint16_t raw, XpeErrorCode* rc_out = nullptr) {
        std::vector<uint16_t> in(N, raw);
        std::vector<float>    out(N, -1.0f);
        XpeImageBuffer ib{}, ob{};
        ib.data = in.data(); ib.width = W; ib.height = H;
        ib.bitsAllocated = 16; ib.bitsStored = 16; ib.format = XPE_PIXEL_UINT16;
        ib.dataSize = static_cast<uint32_t>(in.size() * sizeof(uint16_t));
        ob.data = out.data(); ob.width = W; ob.height = H;
        ob.bitsAllocated = 32; ob.bitsStored = 32; ob.format = XPE_PIXEL_FLOAT32;
        ob.dataSize = static_cast<uint32_t>(out.size() * sizeof(float));
        XpeImageMetadata meta{};
        const XpeErrorCode rc = xpe_gain_correct(&ib, &ob, &meta);
        if (rc_out) *rc_out = rc;
        return out[0];
    }

    static bool alertContains(const char* needle) {
        char msg[512];
        int32_t sev = -1;
        const int32_t count = xpe_get_pending_alert_count();
        for (int32_t i = 0; i < count; ++i) {
            if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) != XPE_OK) continue;
            if (std::string(msg).find(needle) != std::string::npos) return true;
        }
        return false;
    }

    static int32_t alertsContaining(const char* needle) {
        char msg[512];
        int32_t sev = -1;
        int32_t hits = 0;
        const int32_t count = xpe_get_pending_alert_count();
        for (int32_t i = 0; i < count; ++i) {
            if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) != XPE_OK) continue;
            if (std::string(msg).find(needle) != std::string::npos) ++hits;
        }
        return hits;
    }

    /** Rewrite a generated file's coefficients into a fresh XCal file that
     *  carries no config_json -- the shape of a polynomial written before the
     *  range field existed.
     *
     *  Editing the original's bytes in place was tried first and the loader
     *  refused it with XPE_ERR_CONFIG_INVALID: the file is covered by a
     *  SHA-256, so a hand-patched one is a corrupt file, not an old one. Going
     *  back through write_xcal_file() produces a genuinely valid old file, and
     *  keeping the SAME coefficients is what lets the measured 3.8955 stand as
     *  this path's expectation. */
    std::string rewriteWithoutRange(const std::string& src, const char* name) {
        std::ifstream f(src, std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(f)),
                                 std::istreambuf_iterator<char>());

        XCalFileHeader hdr{};
        EXPECT_GE(bytes.size(), sizeof(hdr));
        std::memcpy(&hdr, bytes.data(), sizeof(hdr));

        const size_t payload_at = sizeof(hdr) + static_cast<size_t>(hdr.config_json_len);
        EXPECT_GE(bytes.size(), payload_at + hdr.payload_len);

        XCalFileHeader out_hdr{};
        std::memcpy(out_hdr.magic, XCAL_MAGIC, 4);
        out_hdr.version      = XCAL_VERSION;
        out_hdr.type         = hdr.type;
        out_hdr.pixel_format = hdr.pixel_format;
        out_hdr.width        = hdr.width;
        out_hdr.height       = hdr.height;
        out_hdr.payload_len  = hdr.payload_len;

        const std::string out = p(name);
        EXPECT_EQ(XPE_OK, write_xcal_file(
            out.c_str(), out_hdr, nullptr, 0,
            reinterpret_cast<const uint8_t*>(bytes.data() + payload_at),
            out_hdr.payload_len));
        return out;
    }
};

}  // namespace

/* ---------------------------------------------------------------------------
 * Path 1: the file carries a range -> the clamp applies.
 * ------------------------------------------------------------------------- */

/** The generator records the range it already computes for the monotonicity
 *  check. Without this the file carries no boundary and nothing downstream can
 *  separate "inside the fit" from "extrapolated". */
TEST_F(GainPolyDoseRangeTest, TheGeneratedFileCarriesTheFittedDoseRange) {
    const std::string poly = generatePoly("a123_range.xcal", 3);

    std::ifstream f(poly, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(f)),
                             std::istreambuf_iterator<char>());

    EXPECT_NE(std::string::npos, bytes.find("\"dose_min\":14037"))
        << "config_json must record the lowest dose level the fit used";
    EXPECT_NE(std::string::npos, bytes.find("\"dose_max\":42677"))
        << "config_json must record the highest dose level the fit used";
}

/** THE FALSIFICATION (QA-A-123 (d)). The measured out-of-range gain was
 *  3.8955; clamping must replace it with the D_max gain, 1.3999. Both numbers
 *  come from the QA-A-122 probe against this exact ladder and degree. */
TEST_F(GainPolyDoseRangeTest, ASaturatedPixelReceivesTheDMaxGainInsteadOfTheExtrapolatedOne) {
    const std::string poly = generatePoly("a123_clamp.xcal", 3);
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(poly.c_str()));

    const float at_max = correctedValue(kDoseMax);
    const float at_sat = correctedValue(kSaturated);

    const double gain_at_max = static_cast<double>(kDoseMax) / at_max;
    const double gain_at_sat = static_cast<double>(kSaturated) / at_sat;

    EXPECT_NEAR(1.3999, gain_at_max, 0.01)
        << "the gain at the top knot is the calibrated one";
    EXPECT_NEAR(1.3999, gain_at_sat, 0.01)
        << "a pixel above the range must be evaluated at the range edge, not "
           "extrapolated to 3.8955 (QA-A-122 measurement)";
}

/** THE PROPERTY THE CLAMP EXISTS FOR. Brighter in, brighter out. Before the
 *  clamp the saturated pixel came out at 16823 against the D_max pixel's
 *  30485 -- darker, in exactly the region an operator reads as signal. */
TEST_F(GainPolyDoseRangeTest, ASaturatedPixelIsNotDarkerThanADMaxPixel) {
    const std::string poly = generatePoly("a123_mono.xcal", 3);
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(poly.c_str()));

    const float at_max = correctedValue(kDoseMax);
    const float at_sat = correctedValue(kSaturated);

    EXPECT_GT(at_sat, at_max)
        << "monotonicity inverted: raw " << kSaturated << " -> " << at_sat
        << " came out darker than raw " << kDoseMax << " -> " << at_max;
}

/** One alert for the frame, carrying the count -- not one per pixel. All 64
 *  pixels are out of range here, so a per-pixel push would be 64 lines. */
TEST_F(GainPolyDoseRangeTest, ClampingRaisesExactlyOneAlertCarryingTheCount) {
    const std::string poly = generatePoly("a123_alert.xcal", 3);
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(poly.c_str()));

    xpe_clear_alerts();
    (void)correctedValue(kSaturated);

    EXPECT_EQ(1, alertsContaining("fell outside the gain polynomial"))
        << "the clamp must report once per frame, not once per pixel";
    EXPECT_TRUE(alertContains("64 pixel(s)"))
        << "the alert must carry how many pixels were clamped";
}

/** A frame entirely inside the range must be silent -- an alert that fires on
 *  ordinary frames is one operators learn to ignore. */
TEST_F(GainPolyDoseRangeTest, AFrameInsideTheRangeRaisesNoClampAlert) {
    const std::string poly = generatePoly("a123_quiet.xcal", 3);
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(poly.c_str()));

    xpe_clear_alerts();
    (void)correctedValue(20985u);  // a knot, well inside

    EXPECT_FALSE(alertContains("fell outside the gain polynomial"))
        << "no pixel was out of range, so nothing should be reported";
}

/* ---------------------------------------------------------------------------
 * Path 2: the file carries no range -> loaded, one alert, no clamp.
 * ------------------------------------------------------------------------- */

/** A pre-QA-A-123 file must still load. Rejecting it would retire every
 *  calibration made before the field existed, which is the harder thing to
 *  undo -- and the operator would have no working gain correction at all. */
TEST_F(GainPolyDoseRangeTest, AFileWithoutARangeStillLoadsAndSaysWhy) {
    const std::string poly = generatePoly("a123_src.xcal", 3);
    const std::string old  = rewriteWithoutRange(poly, "a123_old.xcal");

    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(old.c_str()))
        << "an older calibration must not be rejected";

    EXPECT_TRUE(alertContains("without a dose range"))
        << "loading it silently would hide today's behaviour forever";
    EXPECT_TRUE(alertContains("before the range field existed"))
        << "the alert must say what to do about it, not only that it happened";
}

/** And it is NOT clamped: the old file's saturated pixel still receives the
 *  extrapolated gain. This is the behaviour the alert above describes, pinned
 *  so the two can never drift apart. */
TEST_F(GainPolyDoseRangeTest, AFileWithoutARangeIsNotClamped) {
    const std::string poly = generatePoly("a123_src2.xcal", 3);
    const std::string old  = rewriteWithoutRange(poly, "a123_old2.xcal");
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(old.c_str()));

    xpe_clear_alerts();
    const float at_sat = correctedValue(kSaturated);
    const double gain_at_sat = static_cast<double>(kSaturated) / at_sat;

    EXPECT_NEAR(3.8955, gain_at_sat, 0.05)
        << "without a range there is no boundary to clamp to, so the "
           "extrapolated gain is what the pixel gets (QA-A-122 measurement)";
    EXPECT_FALSE(alertContains("fell outside the gain polynomial"))
        << "nothing was clamped, so no clamp alert should be raised";
}
