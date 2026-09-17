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
        xpe_preprocess_shutdown();
        fs::remove_all(dir);
    }

    std::string p(const std::string& n) const { return (dir / n).string(); }

    /** A GAIN_POLY file with `coeffs` coefficient planes, every pixel the same. */
    std::string writePoly(const std::string& name, const std::vector<float>& coeffs) {
        std::vector<float> payload(N * coeffs.size());
        for (size_t k = 0; k < coeffs.size(); ++k)
            for (size_t i = 0; i < N; ++i) payload[k * N + i] = coeffs[k];
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

// #187: the polynomial file loads (with a warning) and gain correction refuses
// with a code that names the reason.
TEST_F(GainPolyNotAppliedTest, PolynomialGainLoadsWithAWarningAndCorrectionRefuses) {
    const std::string poly = writePoly("gain_poly.xcal", {1.0f, 0.5f, 0.25f});
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(poly.c_str()))
        << "the loader accepts XCAL_TYPE_GAIN_POLY (#140 metadata round trip)";
    EXPECT_TRUE(alertContains("no correction applies G(x,y,E)"))
        << "the load must say the coefficients are not applied (#187)";

    xpe_clear_alerts();
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, correct())
        << "this function cannot apply a polynomial model; CALIB_NOT_LOADED would "
           "say the module holds no calibration, which is not the case (#187)";
    EXPECT_TRUE(alertContains("does not apply it"))
        << "the refusal must be visible to the operator, not only in the code";
    EXPECT_FLOAT_EQ(-1.0f, out[0]) << "the output buffer is left untouched";
    xpe_clear_alerts();
}

// Loading a polynomial file after a scalar map removes the working model.
TEST_F(GainPolyNotAppliedTest, PolynomialLoadReplacesAWorkingScalarMap) {
    const std::string sc = p("gain2.xcal");
    ASSERT_EQ(XPE_OK, MakeGainXCal(sc.c_str(), W, H, 2.0f));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(sc.c_str()));
    ASSERT_EQ(XPE_OK, correct());
    ASSERT_FLOAT_EQ(500.0f, out[0]);

    const std::string poly = writePoly("gain_poly2.xcal", {1.0f, 0.5f});
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(poly.c_str()));
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, correct())
        << "the scalar map is cleared on purpose (SRS-CALIB-SAFE-003): frames must "
           "not be corrected with a map the operator did not select (#187)";
    EXPECT_FLOAT_EQ(500.0f, out[0])
        << "the earlier result is left in the buffer; nothing new was written";
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
