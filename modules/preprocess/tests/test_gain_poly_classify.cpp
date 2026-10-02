/**
 * @file test_gain_poly_classify.cpp
 * @brief The polynomial gain path classifies out-of-range pixels as defective (QA-A-211, #233) -- the counterpart of
 *        test_gain_defect_classify.cpp, which holds the scalar side.
 *
 * Three places a polynomial pixel can meet the gain range [0.1, 10] (SRS-CALIB-FUNC-002):
 *   1. GENERATION, measured gain out of range (a failed pixel, the low-sensitivity edge band of CalData_6): the pixel is
 *      not fitted, all its coefficients are stored as 0, it is not in the quality figures, the count is reported, and
 *      more than 5% of the frame refuses the generation (nothing recorded, no file).
 *   2. GENERATION, every measured gain in range but the stored float32 line leaves the range at a measured dose: the same
 *      classification. A fit more than 0.1% away from what the pixel measured is NOT classified -- it is the file failing
 *      to carry the data, and it still refuses (test_gain_poly_policy.cpp).
 *   3. APPLICATION: a pixel whose float32 evaluation at its own value is outside the range -- the all-zero coefficients
 *      of 1 and 2, and also the pixel value BETWEEN the measured doses at which float32 rounding takes a stored curve
 *      out of the range (Codex #64, scaled into the range) -- gets gain 1.0 and is filled by the defect stage of the same
 *      frame; more than 5% of the frame refuses it (XPE_ERR_CONFIG_INVALID).
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"
#include "preprocess_state_fixture.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr uint32_t W = 20, H = 20;
constexpr size_t N = static_cast<size_t>(W) * H;   // 400: 5% is 20 pixels

const char* const kConfig =
    "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassOffset\":true,\"bypassNonlinearity\":true,"
    "\"bypassBinning\":true,\"bypassGhost\":true}";

std::string findAlert(const std::string& prefix) {
    const int32_t n = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < n; ++i) {
        char buf[1024];
        int32_t sev = 0;
        if (xpe_get_pending_alert(i, buf, sizeof(buf), &sev) == XPE_OK && std::string(buf).rfind(prefix, 0) == 0) return buf;
    }
    return {};
}

class GainPolyClassifyTest : public XpePreprocessStateFixture {
protected:
    void SetUp() override {
        XpePreprocessStateFixture::SetUp();
        dir_ = (fs::temp_directory_path() / ("xpe_gpc_" + std::to_string(counter_++))).string();
        fs::create_directories(dir_);
        xpe_clear_alerts();
    }
    void TearDown() override {
        xpe_clear_alerts();
        std::error_code ec;
        fs::remove_all(dir_, ec);
        XpePreprocessStateFixture::TearDown();
    }
    std::string p(const std::string& n) const { return (fs::path(dir_) / n).string(); }

    std::string writeLevel(const std::string& name, const std::vector<float>& v) {
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(XCAL_TYPE_GAIN);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width = W; hdr.height = H;
        hdr.payload_len = v.size() * sizeof(float);
        const std::string path = p(name);
        EXPECT_EQ(XPE_OK, write_xcal_file(path.c_str(), hdr, nullptr, 0, reinterpret_cast<const uint8_t*>(v.data()), hdr.payload_len));
        return path;
    }

    /** Levels: gain of pixel q at dose index l = scale(q) * gainAt[l]; `bad` pixels get the constant `badValue` at every level. */
    XpeErrorCode generate(const std::vector<double>& doses, const std::vector<float>& gainAt,
                          const std::vector<size_t>& bad, float badValue, const std::string& outName, int maxDegree = 2) {
        std::vector<std::string> paths;
        for (size_t l = 0; l < doses.size(); ++l) {
            std::vector<float> v(N);
            for (size_t q = 0; q < N; ++q) v[q] = (1.0f + 0.01f * static_cast<float>(q % 5)) * gainAt[l];
            for (size_t k : bad) v[k] = badValue;
            paths.push_back(writeLevel("lvl" + std::to_string(l) + ".xcal", v));
        }
        std::vector<const char*> cp;
        for (const auto& s : paths) cp.push_back(s.c_str());
        return xpe_calib_generate_gain_polynomial(cp.data(), doses.data(), static_cast<int32_t>(doses.size()), maxDegree,
                                                  p(outName).c_str());
    }

    void loadDefects() {
        std::vector<uint8_t> m(N, 0);
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(XCAL_TYPE_DEFECT);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_UINT8_MASK);
        hdr.width = W; hdr.height = H;
        hdr.payload_len = m.size();
        ASSERT_EQ(XPE_OK, write_xcal_file(p("defects.xcal").c_str(), hdr, nullptr, 0, m.data(), m.size()));
        ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(p("defects.xcal").c_str()));
    }

    /** Pipeline over a uint16 frame: every pixel `px`, the given pixels `other`. */
    XpeErrorCode run(uint16_t px, const std::vector<size_t>& at, uint16_t other, std::vector<float>* out) {
        std::vector<uint8_t> bytes(N * sizeof(float), 0);
        auto* v = reinterpret_cast<uint16_t*>(bytes.data());
        for (size_t i = 0; i < N; ++i) v[i] = px;
        for (size_t k : at) v[k] = other;
        XpeImageBuffer img{};
        img.data = bytes.data(); img.width = W; img.height = H; img.bitsAllocated = 16; img.bitsStored = 16;
        img.format = XPE_PIXEL_UINT16; img.dataSize = bytes.size();
        XpeImageMetadata meta{};
        const XpeErrorCode rc = xpe_preprocess_pipeline_batch(&img, 1, &meta, nullptr, nullptr, kConfig);
        if (rc == XPE_OK && out) {
            out->resize(N);
            std::memcpy(out->data(), bytes.data(), N * sizeof(float));
        }
        return rc;
    }

    std::string dir_;
    static inline int counter_ = 0;
};

const std::vector<double> kDoses = {1000.0, 2000.0, 3000.0};
const std::vector<float> kGain = {0.5f, 1.0f, 1.5f};            // a straight line in dose: R^2 is 1 for every fitted pixel
const std::vector<size_t> kBad = {5 * W + 5, 9 * W + 12, 14 * W + 3};

}  // namespace

// ---------------------------------------------------------------------------
// generation
// ---------------------------------------------------------------------------
TEST_F(GainPolyClassifyTest, MeasuredGainsOutOfRangeAreClassifiedNotFittedAndTheFileIsWritten) {
    ASSERT_EQ(XPE_OK, generate(kDoses, kGain, kBad, 0.04f, "poly.xcal")) << "three failed pixels no longer refuse the generation";
    EXPECT_TRUE(fs::exists(p("poly.xcal")));
    const std::string a = findAlert("XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT:");
    ASSERT_FALSE(a.empty());
    EXPECT_NE(std::string::npos, a.find("3 of 400 pixel(s)")) << a;
    EXPECT_NE(std::string::npos, a.find("first: " + std::to_string(kBad[0]))) << a;
}

// the second way to be classified: every measured gain is inside the range, but the least-squares LINE (the last degree
// tried; maxDegree 1 here) passes below 0.1 at the first dose -- (0.1, 0.1, 0.4) at doses 1000/2000/3000 is fitted at
// 0.05 there. The pixel cannot carry a usable gain at that dose: classified, like a measured out-of-range gain.
TEST_F(GainPolyClassifyTest, APixelWhoseLeastSquaresLineLeavesTheRangeAtAMeasuredDoseIsClassifiedToo) {
    std::vector<std::string> paths;
    const float special[3] = {0.1f, 0.1f, 0.4f};
    for (size_t l = 0; l < 3; ++l) {
        std::vector<float> v(N);
        for (size_t q = 0; q < N; ++q) v[q] = (1.0f + 0.01f * static_cast<float>(q % 5)) * kGain[l];
        v[123] = special[l];
        paths.push_back(writeLevel("sp" + std::to_string(l) + ".xcal", v));
    }
    std::vector<const char*> cp;
    for (const auto& s : paths) cp.push_back(s.c_str());
    ASSERT_EQ(XPE_OK, xpe_calib_generate_gain_polynomial(cp.data(), kDoses.data(), 3, 1, p("sp.xcal").c_str()))
        << "the pixel is classified, not a reason to refuse the generation";
    const std::string a = findAlert("XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT:");
    ASSERT_FALSE(a.empty());
    EXPECT_NE(std::string::npos, a.find("1 of 400 pixel(s)")) << a;
    EXPECT_NE(std::string::npos, a.find("first: 123")) << a;
}

// the first way to be classified is decided on what was MEASURED, before any fit: gains (1, 11, 1) are one level above the
// range, yet the least-squares line through them is the flat mean 4.33 -- inside the range at every dose, so only the
// measured value can classify this pixel. (A fit-only rule would store a line that matches none of the three levels.)
TEST_F(GainPolyClassifyTest, AMeasuredGainOutOfRangeClassifiesThePixelEvenWhenItsFitWouldBeInRange) {
    std::vector<std::string> paths;
    const float spike[3] = {1.0f, 11.0f, 1.0f};
    for (size_t l = 0; l < 3; ++l) {
        std::vector<float> v(N);
        for (size_t q = 0; q < N; ++q) v[q] = (1.0f + 0.01f * static_cast<float>(q % 5)) * kGain[l];
        v[77] = spike[l];
        paths.push_back(writeLevel("spk" + std::to_string(l) + ".xcal", v));
    }
    std::vector<const char*> cp;
    for (const auto& s : paths) cp.push_back(s.c_str());
    ASSERT_EQ(XPE_OK, xpe_calib_generate_gain_polynomial(cp.data(), kDoses.data(), 3, 1, p("spk.xcal").c_str()));
    const std::string a = findAlert("XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT:");
    ASSERT_FALSE(a.empty()) << "the measured 11 is outside [0.1, 10]";
    EXPECT_NE(std::string::npos, a.find("1 of 400 pixel(s)")) << a;
    EXPECT_NE(std::string::npos, a.find("first: 77")) << a;
}

TEST_F(GainPolyClassifyTest, TheClassifiedPixelsAreNotInTheQualityFigures) {
    ASSERT_EQ(XPE_OK, generate(kDoses, kGain, kBad, 0.04f, "poly.xcal"));
    XpeCalibQualityMeta q{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&q));
    // every fitted pixel is an exact straight line: R^2 is 1. A classified pixel scored with its zero coefficients (gain 0
    // against a measured 0.04) would pull the pooled figure away from it.
    EXPECT_GT(q.r_squared, 0.9999) << "the classified pixels are not scored";
}

TEST_F(GainPolyClassifyTest, MoreThanFivePercentClassifiedRefusesTheGenerationAndChangesNothing) {
    std::vector<size_t> many;
    for (size_t i = 0; i < 21; ++i) many.push_back(40 + i * 11);
    XpeCalibQualityMeta before{};
    (void)xpe_calib_get_quality_meta(&before);
    xpe_clear_alerts();
    EXPECT_EQ(XPE_ERR_INVALID_CALIB_DATA, generate(kDoses, kGain, many, 0.04f, "over.xcal")) << "21 of 400 is above 5%";
    EXPECT_FALSE(fs::exists(p("over.xcal")));
    const std::string a = findAlert("XPE_WARN_GAIN_PIXELS_OVER_LIMIT:");
    ASSERT_FALSE(a.empty());
    EXPECT_NE(std::string::npos, a.find("21 of 400 pixel(s)")) << a;
    EXPECT_NE(std::string::npos, a.find("not generated")) << a;
    XpeCalibQualityMeta after{};
    (void)xpe_calib_get_quality_meta(&after);
    EXPECT_EQ(0, std::memcmp(&before, &after, sizeof(before))) << "a refused generation records nothing";

    std::vector<size_t> twenty(many.begin(), many.begin() + 20);
    xpe_clear_alerts();
    EXPECT_EQ(XPE_OK, generate(kDoses, kGain, twenty, 0.04f, "at.xcal")) << "20 of 400 is exactly 5%: not above the limit";
}

TEST_F(GainPolyClassifyTest, ACleanLadderRaisesNoClassificationAlert) {
    ASSERT_EQ(XPE_OK, generate(kDoses, kGain, {}, 0.0f, "clean.xcal"));
    EXPECT_TRUE(findAlert("XPE_WARN_GAIN_PIXELS").empty());
}

// ---------------------------------------------------------------------------
// application: the classified pixels are filled by the defect stage of the same frame
// ---------------------------------------------------------------------------
TEST_F(GainPolyClassifyTest, AClassifiedPixelIsFilledFromItsNeighboursWhereTheAppliedGainIsOne) {
    ASSERT_EQ(XPE_OK, generate(kDoses, kGain, kBad, 0.04f, "poly.xcal"));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(p("poly.xcal").c_str()));
    loadDefects();   // the map marks nothing: the polynomial's own classification must do it
    std::vector<float> out;
    // pixel value 2000 -> the fitted gain there is 1.0 * scale(q); the classified pixels carry a hot value
    ASSERT_EQ(XPE_OK, run(2000, kBad, 60000, &out)) << "a frame is no longer refused because three pixels have no usable gain";
    const float ref = out[10 * W + 10];
    EXPECT_NEAR(2000.0f / (1.0f + 0.01f * static_cast<float>((10 * W + 10) % 5)), ref, 3.0f) << "control: a fitted pixel applies its polynomial";
    for (size_t k : kBad) {
        EXPECT_GT(out[k], 1500.0f) << "pixel " << k;
        EXPECT_LT(out[k], 2100.0f) << "pixel " << k << " is the neighbours' value, not 60000 at gain 1.0";
    }
    EXPECT_FALSE(findAlert("XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT: 3 pixel(s) of this frame").empty()) << "the frame says so";
}

TEST_F(GainPolyClassifyTest, CalledOnItsOwnTheGainStageKeepsGainOneAndSaysNoDefectStageFollows) {
    ASSERT_EQ(XPE_OK, generate(kDoses, kGain, kBad, 0.04f, "poly.xcal"));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(p("poly.xcal").c_str()));
    xpe_clear_alerts();
    std::vector<uint16_t> in(N, 2000);
    in[kBad[0]] = 40000;
    std::vector<float> out(N, 0.0f);
    XpeImageBuffer ib{}, ob{};
    ib.data = in.data(); ib.width = W; ib.height = H; ib.bitsAllocated = 16; ib.bitsStored = 16; ib.format = XPE_PIXEL_UINT16; ib.dataSize = N * 2;
    ob.data = out.data(); ob.width = W; ob.height = H; ob.bitsAllocated = 32; ob.bitsStored = 32; ob.format = XPE_PIXEL_FLOAT32; ob.dataSize = N * 4;
    XpeImageMetadata meta{};
    ASSERT_EQ(XPE_OK, xpe_gain_correct(&ib, &ob, &meta));
    EXPECT_FLOAT_EQ(40000.0f, out[kBad[0]]) << "gain 1.0, uncorrected";
    EXPECT_FALSE(findAlert("XPE_WARN_GAIN_PIXELS_UNCORRECTED:").empty());
}

// The shape of Codex #64 moved into the range: doses [30000, 30500, 31000], gains [0.100002766, 0.32646404, 1.00583434],
// degree 2 (the first gain found by scanning 0.1 * (1 + k * 2.1e-6) for one the generator ACCEPTS and whose stored curve
// float32 rounding takes below 0.1 at a pixel value between the doses). Every measured gain is inside the range and so is
// the stored curve AT the measured doses; at 30001 it is below 0.1. It used to refuse the frame whole; now that pixel is
// classified.
TEST_F(GainPolyClassifyTest, ACurveThatLeavesTheRangeBetweenTwoMeasuredDosesClassifiesThatPixelInsteadOfRefusingTheFrame) {
    const std::vector<double> doses = {30000.0, 30500.0, 31000.0};
    const std::vector<float> gains = {0.100002766f, 0.32646404f, 1.00583434f};
    std::vector<std::string> paths;
    for (size_t l = 0; l < 3; ++l) paths.push_back(writeLevel("c" + std::to_string(l) + ".xcal", std::vector<float>(N, gains[l])));
    std::vector<const char*> cp;
    for (const auto& s : paths) cp.push_back(s.c_str());
    ASSERT_EQ(XPE_OK, xpe_calib_generate_gain_polynomial(cp.data(), doses.data(), 3, 2, p("codex.xcal").c_str()))
        << "control: the generation accepts it (the stored curve is in range at the measured doses)";
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(p("codex.xcal").c_str()));
    loadDefects();
    xpe_clear_alerts();

    std::vector<float> out;
    ASSERT_EQ(XPE_OK, run(30000, {7 * W + 7}, 30001, &out)) << "one pixel at 30001: classified, the frame goes through";
    EXPECT_FALSE(findAlert("XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT: 1 pixel(s) of this frame").empty())
        << "control: the pixel at 30001 really evaluates below 0.1";
    EXPECT_NEAR(out[0], out[7 * W + 7], out[0] * 1e-4f) << "and it was filled from its neighbours (30000 -> gain 0.100002766)";

    // 25 of 400 (6.25%) at that value: the gain is wrong over too much of the frame -- refused, and it says why
    std::vector<size_t> at;
    for (size_t i = 0; i < 25; ++i) at.push_back(30 + i * 13);
    xpe_clear_alerts();
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, run(30000, at, 30001, nullptr));
    const std::string a = findAlert("XPE_WARN_GAIN_PIXELS_OVER_LIMIT:");
    ASSERT_FALSE(a.empty());
    EXPECT_NE(std::string::npos, a.find("25 of 400 pixel(s)")) << a;
    EXPECT_NE(std::string::npos, a.find("not applied")) << a;
}

// the 1x1 data of Codex #64 as it is (gains near 0.001): all of it is below the range, so the pixel is classified at
// generation, and a single pixel is 100% of the frame -- refused, never written as a success
TEST_F(GainPolyClassifyTest, TheCodexDataAsGivenIsRefusedAtGenerationNowThatTheRangeIsTheSrsOne) {
    const std::vector<double> doses = {30000.0, 30500.0, 31000.0};
    const std::vector<float> gains = {0.0010000730f, 0.0032646404f, 0.0100583434f};
    std::vector<std::string> paths;
    for (size_t l = 0; l < 3; ++l) {
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(XCAL_TYPE_GAIN);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width = 1; hdr.height = 1;
        hdr.payload_len = sizeof(float);
        const std::string path = p("one" + std::to_string(l) + ".xcal");
        ASSERT_EQ(XPE_OK, write_xcal_file(path.c_str(), hdr, nullptr, 0, reinterpret_cast<const uint8_t*>(&gains[l]), sizeof(float)));
        paths.push_back(path);
    }
    std::vector<const char*> cp;
    for (const auto& s : paths) cp.push_back(s.c_str());
    EXPECT_EQ(XPE_ERR_INVALID_CALIB_DATA, xpe_calib_generate_gain_polynomial(cp.data(), doses.data(), 3, 2, p("one.xcal").c_str()));
    EXPECT_FALSE(fs::exists(p("one.xcal")));
    EXPECT_FALSE(findAlert("XPE_WARN_GAIN_PIXELS_OVER_LIMIT:").empty());
}
