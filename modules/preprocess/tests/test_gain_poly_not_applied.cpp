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
 * So loading a polynomial file makes gain correction FAIL. These cases record
 * that behaviour so a later change to it is visible; they are not an
 * endorsement of it (#187). They must be updated when #187 is implemented.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xpe/preprocess/xcal_format.h"
#include "fixtures/make_xcal.hpp"

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

// #187: the polynomial file loads, and gain correction then fails.
TEST_F(GainPolyNotAppliedTest, PolynomialGainLoadsButCorrectionReportsNotLoaded) {
    const std::string poly = writePoly("gain_poly.xcal", {1.0f, 0.5f, 0.25f});
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(poly.c_str()))
        << "the loader accepts XCAL_TYPE_GAIN_POLY";
    EXPECT_EQ(XPE_ERR_CALIB_NOT_LOADED, correct())
        << "current behaviour (#187): no API applies G(x,y,E); update this case "
           "when the coefficients are wired into a correction path";
    EXPECT_FLOAT_EQ(-1.0f, out[0]) << "the output buffer is left untouched";
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
    EXPECT_EQ(XPE_ERR_CALIB_NOT_LOADED, correct())
        << "the scalar map is cleared by the polynomial load (#187)";
}
