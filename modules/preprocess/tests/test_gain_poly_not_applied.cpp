/**
 * @file test_gain_poly_not_applied.cpp
 * @brief Current behaviour of a loaded gain polynomial (QA-A-105, #187)
 *
 * SRS-CALIB-FUNC-027 says the polynomial fit is stored "in `.xpe_calib` with
 * `XCAL_TYPE_GAIN_POLY`", and SRS-CALIB-FUNC-005 says gain correction applies
 * `I_norm = I_corr / G(x,y)` with "Multi-gain mode with energy-dependent
 * polynomial `G(x,y,E) = Sum(c_k * E^k)` ... supported". Neither names the API
 * that applies the coefficients, and none does today:
 *
 *  - xpe_calib_load_gain() accepts a GAIN_POLY file, stores the coefficients in
 *    g_calib.gain_poly_coeffs and CLEARS the scalar map (xpe_calib_load_gain.cpp:76-90).
 *  - xpe_gain_correct() reads only the scalar map and returns
 *    XPE_ERR_CALIB_NOT_LOADED when it is absent (gain_correct.cpp:275).
 *
 * QA-A-107 (#187) changed what that failure LOOKS like, not whether it fails:
 *  - the load still succeeds (the file's FUNC-033 (5) metadata is read back --
 *    QA-A-37, #140) and now raises an XPE_ALERT_WARNING saying the coefficients
 *    are not applied;
 *  - xpe_gain_correct() answers XPE_ERR_UNSUPPORTED_FORMAT (this function
 *    cannot apply this model) with an XPE_ALERT_ERROR, instead of
 *    XPE_ERR_CALIB_NOT_LOADED, which said the module held no calibration at all.
 *
 * The QA-A-105 version of these cases expected XPE_ERR_CALIB_NOT_LOADED. That
 * expectation recorded the defect: a caller reading "not loaded" after a
 * successful load has no way to learn that the file it loaded is the reason.
 * The scalar map is still cleared on purpose -- correcting frames with a map
 * the operator did not select is what SRS-CALIB-SAFE-003 forbids.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xpe/preprocess/xcal_format.h"
#include "fixtures/make_xcal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr uint32_t W = 8, H = 8;
constexpr size_t   N = static_cast<size_t>(W) * H;

class GainPolyNotAppliedTest : public ::testing::Test {
protected:
    fs::path dir;
    std::vector<uint16_t> in;
    std::vector<float>    out;

    void SetUp() override {
        (void)xpe_preprocess_init(nullptr);
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        dir = fs::temp_directory_path() / "xpe_gain_poly_not_applied";
        fs::remove_all(dir);
        fs::create_directories(dir);
        in.assign(N, 1000u);
        out.assign(N, -1.0f);
    }

    void TearDown() override {
        // QA-A-138 (#198): leave the pending alert queue as this test found
        // it. The product drains it through this public call; nothing in
        // xpe_preprocess_shutdown() touches the common-module queue.
        xpe_clear_alerts();
        xpe_preprocess_shutdown();
        fs::remove_all(dir);
    }

    std::string p(const std::string& n) const { return (dir / n).string(); }

    /**
     * A GAIN_POLY file carrying `coeffs` for every pixel.
     *
     * PIXEL-MAJOR: coefficient j of pixel p at [p * coeffs.size() + j]. That is
     * what xpe_calib_generate_gain_polynomial() writes
     * (xpe_calib_generate_gain.cpp:561, `offset = pix * sMaxCoeffsPoly`) and
     * what the store documents (xpe_preprocess_internal.h:277-278).
     *
     * This helper used to write PLANE-major (all of c0, then all of c1). Both
     * layouts have the same byte count, so the loader accepted either and no
     * test noticed -- nothing read the coefficients back. QA-A-121 is the first
     * code that evaluates them, which is what made the mismatch observable.
     */
    std::string writePoly(const std::string& name, const std::vector<float>& coeffs) {
        std::vector<float> payload(N * coeffs.size());
        for (size_t i = 0; i < N; ++i)
            for (size_t k = 0; k < coeffs.size(); ++k)
                payload[i * coeffs.size() + k] = coeffs[k];
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(XCAL_TYPE_GAIN_POLY);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width = W; hdr.height = H;
        hdr.payload_len = payload.size() * sizeof(float);
        const std::string path = p(name);
        EXPECT_EQ(XPE_OK, write_xcal_file(path.c_str(), hdr, nullptr, 0,
                                          reinterpret_cast<const uint8_t*>(payload.data()),
                                          hdr.payload_len));
        return path;
    }

    /** true when an alert whose text contains @p needle is pending. */
    static bool alertContains(const char* needle) {
        char msg[256];
        int32_t sev = -1;
        const int32_t count = xpe_get_pending_alert_count();
        for (int32_t i = 0; i < count; ++i) {
            if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) != XPE_OK) continue;
            if (std::string(msg).find(needle) != std::string::npos) return true;
        }
        return false;
    }

    XpeErrorCode correct() {
        XpeImageBuffer ib{};
        ib.width = W; ib.height = H; ib.format = XPE_PIXEL_UINT16;
        ib.bitsAllocated = 16; ib.bitsStored = 16;
        ib.data = in.data(); ib.dataSize = N * sizeof(uint16_t);
        XpeImageBuffer ob{};
        ob.width = W; ob.height = H; ob.format = XPE_PIXEL_FLOAT32;
        ob.bitsAllocated = 32; ob.bitsStored = 32;
        ob.data = out.data(); ob.dataSize = N * sizeof(float);
        XpeImageMetadata meta{};
        meta.kVp = 80.0f;
        meta.pixelPitch_mm = 0.14f;
        return xpe_gain_correct(&ib, &ob, &meta);
    }
};

}  // namespace

// The scalar map is the model gain correction uses: the control for the cases
// below, so that a failure there cannot be read as "the test setup is wrong".
TEST_F(GainPolyNotAppliedTest, ScalarGainMapIsApplied) {
    const std::string sc = p("gain.xcal");
    ASSERT_EQ(XPE_OK, MakeGainXCal(sc.c_str(), W, H, 2.0f));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(sc.c_str()));
    ASSERT_EQ(XPE_OK, correct());
    EXPECT_FLOAT_EQ(500.0f, out[0]);   // 1000 / 2
}

// #187: the polynomial file loads and IS applied, with no alert either side.
//
// WHAT THIS CASE USED TO SAY, AND WHY IT WAS RIGHT THEN. Until QA-A-121 it
// asserted a WARNING at load ("no correction applies G(x,y,E)") and
// XPE_ERR_UNSUPPORTED_FORMAT with an ERROR alert at correction time. That
// recorded a real state of the system: the coefficients were stored and nothing
// read them. Now xpe_gain_correct() evaluates them, so both alerts describe a
// system that no longer exists -- and an alert that outlives its truth teaches
// operators to ignore the queue.
TEST_F(GainPolyNotAppliedTest, PolynomialGainLoadsAndIsAppliedWithoutAlerts) {
    const std::string poly = writePoly("gain_poly.xcal", {1.0f, 0.0005f});
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(poly.c_str()))
        << "the loader accepts XCAL_TYPE_GAIN_POLY (#140 metadata round trip)";
    EXPECT_FALSE(alertContains("no correction applies G(x,y,E)"))
        << "the load-time warning outlived the defect it described";

    ASSERT_EQ(XPE_OK, correct()) << "the polynomial model is applied now (#187)";
    EXPECT_FALSE(alertContains("does not apply it"))
        << "the refusal alert outlived the refusal";
    EXPECT_NEAR(1000.0f / 1.5f, out[0], 1e-2f)
        << "G(1000) = 1.0 + 0.0005*1000 = 1.5, and the correction divides by it";
    xpe_clear_alerts();
}

// Loading a polynomial file after a scalar map removes the working model.
TEST_F(GainPolyNotAppliedTest, PolynomialLoadReplacesAWorkingScalarMapAndAppliesInstead) {
    const std::string sc = p("gain2.xcal");
    ASSERT_EQ(XPE_OK, MakeGainXCal(sc.c_str(), W, H, 2.0f));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(sc.c_str()));
    ASSERT_EQ(XPE_OK, correct());
    ASSERT_FLOAT_EQ(500.0f, out[0]);

    const std::string poly = writePoly("gain_poly2.xcal", {1.0f, 0.0005f});
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(poly.c_str()));
    ASSERT_EQ(XPE_OK, correct())
        << "the polynomial replaces the scalar map and is applied in its place";
    EXPECT_NEAR(1000.0f / 1.5f, out[0], 1e-2f)
        << "the new model's value, not the old map's 500.0 -- the scalar map is "
           "still cleared on purpose (SRS-CALIB-SAFE-003), it is just no longer "
           "the end of the road";
    xpe_clear_alerts();
}

// ---------------------------------------------------------------------------
// SRS-CALIB-FUNC-002 (#188, QA-A-107): "Values shall be in range [0.1, 10.0];
// out-of-range values shall trigger XPE_ERR_INVALID_CALIB_DATA error."
//
// The bounds are inclusive (the requirement writes a closed interval), and the
// check lives in the loader, which is where FUNC-002 places it. The separate
// finite / non-zero guard inside xpe_gain_correct stays as a second line of
// defence; no public path reaches it any more, which its comment now records.
// ---------------------------------------------------------------------------
TEST_F(GainPolyNotAppliedTest, ScalarGainValuesInsideTheRangeLoad) {
    for (float v : {0.1f, 0.5f, 1.0f, 2.0f, 9.9f, 10.0f}) {
        const std::string path = p("in_" + std::to_string(v) + ".xcal");
        ASSERT_EQ(XPE_OK, MakeGainXCal(path.c_str(), W, H, v));
        EXPECT_EQ(XPE_OK, xpe_calib_load_gain(path.c_str())) << "gain " << v;
    }
}

TEST_F(GainPolyNotAppliedTest, ScalarGainValuesOutsideTheRangeAreRejected) {
    for (float v : {0.0f, 0.09f, 10.01f, 1000.0f, -1.0f}) {
        const std::string path = p("out_" + std::to_string(v) + ".xcal");
        ASSERT_EQ(XPE_OK, MakeGainXCal(path.c_str(), W, H, v));
        EXPECT_EQ(XPE_ERR_INVALID_CALIB_DATA, xpe_calib_load_gain(path.c_str()))
            << "gain " << v;
    }
}

// One bad pixel is enough; the rest of the map being valid does not excuse it.
TEST_F(GainPolyNotAppliedTest, ASingleOutOfRangePixelIsRejected) {
    const std::string path = p("one_bad.xcal");
    std::vector<float> data(N, 1.0f);
    data[N / 2] = 12.0f;
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version = XCAL_VERSION;
    hdr.type = static_cast<uint32_t>(XCAL_TYPE_GAIN);
    hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
    hdr.width = W; hdr.height = H;
    hdr.payload_len = data.size() * sizeof(float);
    ASSERT_EQ(XPE_OK, write_xcal_file(path.c_str(), hdr, nullptr, 0,
                                      reinterpret_cast<const uint8_t*>(data.data()),
                                      hdr.payload_len));
    EXPECT_EQ(XPE_ERR_INVALID_CALIB_DATA, xpe_calib_load_gain(path.c_str()));
}

// A rejected file must not become the active calibration.
TEST_F(GainPolyNotAppliedTest, ARejectedMapDoesNotReplaceTheLoadedOne) {
    const std::string good = p("good.xcal");
    ASSERT_EQ(XPE_OK, MakeGainXCal(good.c_str(), W, H, 2.0f));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(good.c_str()));
    ASSERT_EQ(XPE_OK, correct());
    ASSERT_FLOAT_EQ(500.0f, out[0]);

    const std::string bad = p("bad.xcal");
    ASSERT_EQ(XPE_OK, MakeGainXCal(bad.c_str(), W, H, 50.0f));
    EXPECT_EQ(XPE_ERR_INVALID_CALIB_DATA, xpe_calib_load_gain(bad.c_str()));

    out.assign(N, -1.0f);
    EXPECT_EQ(XPE_OK, correct()) << "the map that was already loaded is still there";
    EXPECT_FLOAT_EQ(500.0f, out[0]) << "and it is the one that was loaded, not the rejected file";
}

// The polynomial path is not subject to the scalar range: a coefficient of
// G(x,y,E) is not a gain value (FUNC-002 speaks of the gain coefficients a
// scalar file holds).
TEST_F(GainPolyNotAppliedTest, PolynomialCoefficientsOutsideTheScalarRangeStillLoad) {
    const std::string poly = writePoly("wide_poly.xcal", {0.0f, 25.0f, -3.0f});
    EXPECT_EQ(XPE_OK, xpe_calib_load_gain(poly.c_str()));
    xpe_clear_alerts();
}

/* =====================================================================
 * QA-A-121 (#187): the polynomial gain is APPLIED, and it is indexed by
 * each pixel's own value.
 *
 * WHY THIS SHAPE OF TEST, AND WHAT IT DISCRIMINATES.
 *
 * The lead's question is whether the model is evaluated per pixel or once
 * per frame. A test that feeds a uniform frame cannot tell those apart --
 * every pixel has the same value, so both schemes produce the same output,
 * and the test would pass either way (this repository has a name for that:
 * a test that cannot fail for the reason it exists).
 *
 * So the frame carries TWO signal levels. Under per-pixel indexing the two
 * regions sit at different points of the same curve and therefore receive
 * DIFFERENT gains; under any frame-scalar scheme they receive the same one.
 * The ratio out/in is constant across the frame in the second case and not
 * in the first, and that is the discriminator.
 *
 * The expected values are computed here from the coefficients by this
 * file's own arithmetic -- G(v) = c0 + c1*v, corrected = v / G(v) -- so a
 * wrong evaluation order, a wrong coefficient stride, or a wrong index all
 * move the measured value and leave the expectation alone.
 *
 * UNITS. The curve's abscissa is whatever xpe_calib_generate_gain_polynomial()
 * was handed as `dose_levels` ("mGy or relative units", preprocess_api.h).
 * Indexing by the pixel's own value is the consistent reading only when those
 * levels were given in the same units as pixel values -- which is what the
 * reference dataset does (tests/test_data/cyan_test: CalSet levels are named
 * by their ADU, 14037..42677). The test builds its coefficients on that
 * reading and says so here rather than leaving it implied.
 * ===================================================================== */
TEST_F(GainPolyNotAppliedTest, PolynomialGainIsAppliedPerPixelUsingThePixelsOwnValue) {
    // G(v) = 1.0 + 0.0005*v  ->  at v=1000, G=1.5; at v=3000, G=2.5.
    // Chosen so the two regions differ by much more than float noise.
    constexpr float c0 = 1.0f, c1 = 0.0005f;
    const std::string poly = writePoly("gain_poly_perpixel.xcal", {c0, c1});
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(poly.c_str()));

    // Two levels, interleaved so neither region is a contiguous block -- a
    // stride bug that reads the wrong coefficient plane cannot line up with
    // the pattern by accident.
    for (size_t i = 0; i < N; ++i) in[i] = (i % 2 == 0) ? 1000u : 3000u;
    std::fill(out.begin(), out.end(), -1.0f);

    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, correct())
        << "the polynomial model must be applied, not refused";

    for (size_t i = 0; i < N; ++i) {
        const float v = static_cast<float>(in[i]);
        const float g = c0 + c1 * v;
        EXPECT_NEAR(v / g, out[i], 1e-2f) << "pixel " << i << " at value " << v;
    }

    // The discriminator, stated as its own assertion: the two regions did NOT
    // receive the same gain. If they had, out/in would be equal for both.
    const float ratio_low = out[0] / 1000.0f;
    const float ratio_high = out[1] / 3000.0f;
    EXPECT_GT(std::fabs(ratio_low - ratio_high), 0.05f)
        << "both signal levels were corrected with the same gain (" << ratio_low
        << " vs " << ratio_high << ") -- the model is not indexed per pixel";

    // The alert said "loaded but not applied". Once it IS applied that
    // sentence is false, so it must no longer be raised.
    EXPECT_FALSE(alertContains("does not apply it"))
        << "the refusal alert survived the correction that makes it untrue";
}

/** A polynomial and a scalar map cannot both be active; the loader clears one.
 *  This pins WHICH one wins if that ever changes. */
TEST_F(GainPolyNotAppliedTest, TheModelLoadedLastIsTheOneApplied) {
    const std::string sc = p("gain_last.xcal");
    ASSERT_EQ(XPE_OK, MakeGainXCal(sc.c_str(), W, H, 2.0f));
    const std::string poly = writePoly("gain_poly_last.xcal", {1.0f, 0.0005f});

    // scalar, then polynomial -> polynomial applies
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(sc.c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(poly.c_str()));
    std::fill(in.begin(), in.end(), 1000u);
    std::fill(out.begin(), out.end(), -1.0f);
    ASSERT_EQ(XPE_OK, correct());
    EXPECT_NEAR(1000.0f / 1.5f, out[0], 1e-2f) << "the polynomial should apply";

    // polynomial, then scalar -> scalar applies
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(sc.c_str()));
    std::fill(out.begin(), out.end(), -1.0f);
    ASSERT_EQ(XPE_OK, correct());
    EXPECT_NEAR(500.0f, out[0], 1e-3f) << "the scalar map should apply";
}
