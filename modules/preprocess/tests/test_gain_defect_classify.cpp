/**
 * @file test_gain_defect_classify.cpp
 * @brief A pixel whose gain is outside [0.1, 10] is classified defective, not a reason to refuse the calibration
 *        (QA-A-211, #233; user decision of 2026-10-02 after QA-A-210f).
 *
 * QA-A-210f measured CalData_6: the scalar gain map has 39-44 thousand pixels outside the SRS-CALIB-FUNC-002 range
 * [0.1, 10], two thirds of them outside the defect map and 99.9% of them in the outer 64-pixel band (low sensitivity),
 * and the loader refused the whole map. The decision: such a pixel gets gain 1.0 and is treated as a defect pixel -- the
 * defect stage fills it from its neighbours; the count is reported; more than 5% of the frame refuses the calibration.
 *
 * This file holds the SCALAR side and the defect stage: load-time classification, the cap, the alerts, the defect stage
 * reading the union, a cache hit installing the same classification, bypassed defect stage, binning.
 * (The polynomial side is test_gain_poly_classify.cpp.)
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"
#include "fixtures/make_xcal.hpp"
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

// gain stage, defect stage; everything else off, no ghost handle
const char* const kConfig =
    "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassOffset\":true,\"bypassNonlinearity\":true,"
    "\"bypassBinning\":true,\"bypassGhost\":true}";
const char* const kConfigNoDefect =
    "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassOffset\":true,\"bypassNonlinearity\":true,"
    "\"bypassBinning\":true,\"bypassGhost\":true,\"bypassDefect\":true}";
const char* const kConfigBinning =
    "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassOffset\":true,\"bypassNonlinearity\":true,"
    "\"bypassGhost\":true,\"binningMode\":2}";

std::vector<std::string> alerts() {
    std::vector<std::string> out;
    const int32_t n = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < n; ++i) {
        char buf[1024];
        int32_t sev = 0;
        if (xpe_get_pending_alert(i, buf, sizeof(buf), &sev) == XPE_OK) out.emplace_back(buf);
    }
    return out;
}

int countAlerts(const std::string& prefix) {
    int c = 0;
    for (const auto& a : alerts()) if (a.rfind(prefix, 0) == 0) ++c;
    return c;
}

std::string findAlert(const std::string& prefix) {
    for (const auto& a : alerts()) if (a.rfind(prefix, 0) == 0) return a;
    return {};
}

class GainDefectClassifyTest : public XpePreprocessStateFixture {
protected:
    void SetUp() override {
        XpePreprocessStateFixture::SetUp();
        dir_ = (fs::temp_directory_path() / ("xpe_gdc_" + std::to_string(counter_++))).string();
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

    /** A scalar gain file: `base` everywhere, `bad` pixels set to the given values. */
    std::string writeGain(const std::string& name, float base, const std::vector<std::pair<size_t, float>>& bad) {
        std::vector<float> g(N, base);
        for (const auto& b : bad) g[b.first] = b.second;
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(XCAL_TYPE_GAIN);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width = W; hdr.height = H;
        hdr.payload_len = g.size() * sizeof(float);
        const std::string path = p(name);
        EXPECT_EQ(XPE_OK, write_xcal_file(path.c_str(), hdr, nullptr, 0,
                                          reinterpret_cast<const uint8_t*>(g.data()), hdr.payload_len));
        return path;
    }

    /** A defect map file with the given pixels marked. */
    void loadDefects(const std::vector<size_t>& marked) {
        std::vector<uint8_t> m(N, 0);
        for (size_t k : marked) m[k] = 1;
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(XCAL_TYPE_DEFECT);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_UINT8_MASK);
        hdr.width = W; hdr.height = H;
        hdr.payload_len = m.size();
        const std::string path = p("defects.xcal");
        ASSERT_EQ(XPE_OK, write_xcal_file(path.c_str(), hdr, nullptr, 0, m.data(), m.size()));
        ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(path.c_str()));
    }

    /** The frame: 1000 everywhere, `hot` (60000) at the given pixels. Runs the pipeline, returns its status. */
    XpeErrorCode run(const char* config, const std::vector<size_t>& hot, std::vector<float>* out) {
        std::vector<uint8_t> bytes(N * sizeof(float), 0);
        auto* px = reinterpret_cast<uint16_t*>(bytes.data());
        for (size_t i = 0; i < N; ++i) px[i] = 1000;
        for (size_t k : hot) px[k] = 60000;
        XpeImageBuffer img{};
        img.data = bytes.data(); img.width = W; img.height = H; img.bitsAllocated = 16; img.bitsStored = 16;
        img.format = XPE_PIXEL_UINT16; img.dataSize = bytes.size();
        XpeImageMetadata meta{};
        const XpeErrorCode rc = xpe_preprocess_pipeline_batch(&img, 1, &meta, nullptr, nullptr, config);
        if (rc == XPE_OK && out) {
            out->resize(N);
            std::memcpy(out->data(), bytes.data(), N * sizeof(float));
        }
        return rc;
    }

    std::string dir_;
    static inline int counter_ = 0;
};

// Away from the frame's edge so the interpolation neighbours are plain pixels.
const std::vector<size_t> kBad = {5 * W + 5, 9 * W + 12, 14 * W + 3};

}  // namespace

// ---------------------------------------------------------------------------
// the load classifies instead of refusing, and says how many
// ---------------------------------------------------------------------------
TEST_F(GainDefectClassifyTest, AMapWithAFewBadPixelsLoadsAndTheCountIsReported) {
    const std::string path = writeGain("few_bad.xcal", 2.0f, {{kBad[0], 0.0f}, {kBad[1], 50.0f}, {kBad[2], 0.05f}});
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(path.c_str())) << "three pixels out of range no longer refuse the map";
    const std::string a = findAlert("XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT:");
    ASSERT_FALSE(a.empty()) << "the classification is reported";
    EXPECT_NE(std::string::npos, a.find("3 of 400 pixel(s)")) << a;
    EXPECT_NE(std::string::npos, a.find("gain 1.0 is used")) << a;
    EXPECT_NE(std::string::npos, a.find("first: " + std::to_string(kBad[0]))) << a;
}

TEST_F(GainDefectClassifyTest, ANonFiniteGainIsClassifiedToo) {
    const std::string path = writeGain("nan.xcal", 2.0f, {{kBad[0], std::nanf("")}, {kBad[1], INFINITY}});
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(path.c_str()));
    EXPECT_NE(std::string::npos, findAlert("XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT:").find("2 of 400"));
}

TEST_F(GainDefectClassifyTest, AMapWithNoBadPixelRaisesNoAlertAndTheBoundsAreInclusive) {
    const std::string path = writeGain("clean.xcal", 2.0f, {{kBad[0], 0.1f}, {kBad[1], 10.0f}});
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(path.c_str()));
    EXPECT_EQ(0, countAlerts("XPE_WARN_GAIN_PIXELS")) << "0.1 and 10.0 are inside the range of SRS-CALIB-FUNC-002";
}

// ---------------------------------------------------------------------------
// the cap: exactly 5% passes, one more refuses -- and the loaded map stays
// ---------------------------------------------------------------------------
TEST_F(GainDefectClassifyTest, ExactlyFivePercentLoadsOneMoreIsRefusedAndTheLoadedMapStays) {
    std::vector<std::pair<size_t, float>> twenty, twentyOne;
    for (size_t i = 0; i < 20; ++i) twenty.push_back({i * 7 + 30, 0.0f});
    twentyOne = twenty;
    twentyOne.push_back({395, 0.0f});
    const std::string ok = writeGain("at_cap.xcal", 2.0f, twenty);
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(ok.c_str())) << "20 of 400 is exactly 5%: not above the limit";
    xpe_clear_alerts();

    const std::string over = writeGain("over_cap.xcal", 4.0f, twentyOne);
    EXPECT_EQ(XPE_ERR_INVALID_CALIB_DATA, xpe_calib_load_gain(over.c_str())) << "21 of 400 is above it";
    const std::string a = findAlert("XPE_WARN_GAIN_PIXELS_OVER_LIMIT:");
    ASSERT_FALSE(a.empty());
    EXPECT_NE(std::string::npos, a.find("21 of 400 pixel(s)")) << a;
    EXPECT_NE(std::string::npos, a.find("not loaded")) << a;

    // the refused file did not replace the loaded one: its gain was 2.0 (4.0 would halve the output again)
    loadDefects({});
    std::vector<float> out;
    ASSERT_EQ(XPE_OK, run(kConfig, {}, &out));
    EXPECT_FLOAT_EQ(500.0f, out[10 * W + 10]) << "the map that was already loaded is still the one applied";
}

TEST_F(GainDefectClassifyTest, AWholeMapOutOfRangeIsStillRefused) {
    ASSERT_EQ(XPE_OK, MakeGainXCal(p("all_bad.xcal").c_str(), W, H, 50.0f));
    EXPECT_EQ(XPE_ERR_INVALID_CALIB_DATA, xpe_calib_load_gain(p("all_bad.xcal").c_str()));
}

// ---------------------------------------------------------------------------
// the defect stage fills the classified pixels from their neighbours (gain 1.0 is not what comes out)
// ---------------------------------------------------------------------------
TEST_F(GainDefectClassifyTest, TheClassifiedPixelsAreFilledFromTheirNeighboursByTheDefectStage) {
    const std::string path = writeGain("fill.xcal", 2.0f, {{kBad[0], 0.0f}, {kBad[1], 50.0f}, {kBad[2], 0.05f}});
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(path.c_str()));
    loadDefects({});   // the defect map itself marks nothing: the classification alone must do it
    std::vector<float> out;
    // a hot input at each classified pixel: gain 1.0 alone would pass 60000 through, gain 2.0 would give 30000
    ASSERT_EQ(XPE_OK, run(kConfig, kBad, &out));
    for (size_t k : kBad) {
        EXPECT_FLOAT_EQ(500.0f, out[k]) << "pixel " << k << " is the neighbours' value, not its own";
    }
    for (size_t i = 0; i < N; ++i) {
        bool bad = false;
        for (size_t k : kBad) bad = bad || (k == i);
        if (!bad) EXPECT_FLOAT_EQ(500.0f, out[i]) << "pixel " << i << " is untouched";
    }
}

TEST_F(GainDefectClassifyTest, TheDefectMapAndTheClassificationAreBothCorrected) {
    const std::string path = writeGain("union.xcal", 2.0f, {{kBad[0], 0.0f}});
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(path.c_str()));
    const size_t mapped = 12 * W + 8;
    loadDefects({mapped});
    std::vector<float> out;
    ASSERT_EQ(XPE_OK, run(kConfig, {kBad[0], mapped}, &out));
    EXPECT_FLOAT_EQ(500.0f, out[kBad[0]]) << "the classified pixel";
    EXPECT_FLOAT_EQ(500.0f, out[mapped]) << "the pixel of the loaded defect map -- the union does not drop it";
}

// ---------------------------------------------------------------------------
// a defect stage that does not run, binning that would spread the value, the union density (D1, D4, D5)
// ---------------------------------------------------------------------------
TEST_F(GainDefectClassifyTest, WithTheDefectStageBypassedTheClassifiedPixelsPassAtGainOneAndTheFrameSaysSo) {
    const std::string path = writeGain("bypass.xcal", 2.0f, {{kBad[0], 0.0f}});
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(path.c_str()));
    xpe_clear_alerts();
    std::vector<float> out;
    ASSERT_EQ(XPE_OK, run(kConfigNoDefect, {kBad[0]}, &out)) << "a bypass is the caller's explicit choice: a warning, not a refusal";
    EXPECT_FLOAT_EQ(60000.0f, out[kBad[0]]) << "gain 1.0, uncorrected";
    EXPECT_FLOAT_EQ(500.0f, out[kBad[1]]);
    const std::string a = findAlert("XPE_WARN_GAIN_PIXELS_UNCORRECTED:");
    ASSERT_FALSE(a.empty());
    EXPECT_NE(std::string::npos, a.find("1 pixel(s)")) << a;
}

TEST_F(GainDefectClassifyTest, ClassifiedPixelsWithBinningAreRefusedBecauseBinningWouldSpreadTheUncorrectedValue) {
    const std::string path = writeGain("binning.xcal", 2.0f, {{kBad[0], 0.0f}});
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(path.c_str()));
    loadDefects({});
    xpe_clear_alerts();
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, run(kConfigBinning, {kBad[0]}, nullptr));
    EXPECT_FALSE(findAlert("XPE_WARN_GAIN_PIXELS_WITH_BINNING:").empty());
}

TEST_F(GainDefectClassifyTest, WithNothingClassifiedBinningIsNotRefusedByThisRule) {
    const std::string path = writeGain("binning_clean.xcal", 2.0f, {});
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(path.c_str()));
    loadDefects({});
    xpe_clear_alerts();
    const XpeErrorCode rc = run(kConfigBinning, {}, nullptr);
    EXPECT_NE(XPE_ERR_CONFIG_INVALID, rc) << "control: the refusal is about classified pixels, not about binning";
    EXPECT_EQ(0, countAlerts("XPE_WARN_GAIN_PIXELS_WITH_BINNING:"));
}

TEST_F(GainDefectClassifyTest, AUnionAboveFivePercentWarnsOncePerFrameAndStillCorrects) {
    const std::string path = writeGain("dense.xcal", 2.0f, {{kBad[0], 0.0f}});
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(path.c_str()));
    std::vector<size_t> many;
    for (size_t i = 0; i < 30; ++i) many.push_back(3 * W + 2 + i * 11 % 300 + 40);   // 30 mapped defects: 7.5% with the classified one
    std::sort(many.begin(), many.end());
    many.erase(std::unique(many.begin(), many.end()), many.end());
    loadDefects(many);
    xpe_clear_alerts();
    std::vector<float> out;
    ASSERT_EQ(XPE_OK, run(kConfig, {}, &out)) << "the union is a warning, never a refusal";
    EXPECT_EQ(1, countAlerts("XPE_WARN_DEFECT_UNION_OVER_LIMIT:"));
}

TEST_F(GainDefectClassifyTest, ACleanMapTakesThePathItAlwaysTookAndRaisesNothing) {
    const std::string path = writeGain("plain.xcal", 2.0f, {});
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(path.c_str()));
    loadDefects({kBad[1]});
    xpe_clear_alerts();
    std::vector<float> out;
    ASSERT_EQ(XPE_OK, run(kConfig, {kBad[1]}, &out));
    EXPECT_FLOAT_EQ(500.0f, out[kBad[1]]);
    EXPECT_EQ(0, countAlerts("XPE_WARN_"));
}

// ---------------------------------------------------------------------------
// a cache hit installs the classification the load made (the shortcut gives the verdict the slow path gives)
// ---------------------------------------------------------------------------
TEST_F(GainDefectClassifyTest, ACacheHitInstallsTheSameClassificationAsTheMiss) {
    xpe_calib_cache_clear();
    const std::string path = writeGain("cached.xcal", 2.0f, {{kBad[0], 0.0f}, {kBad[1], 50.0f}});
    XpeImageBuffer v{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain_cached(path.c_str(), &v)) << "miss";
    loadDefects({});
    std::vector<float> miss;
    ASSERT_EQ(XPE_OK, run(kConfig, {kBad[0], kBad[1]}, &miss));
    EXPECT_FLOAT_EQ(500.0f, miss[kBad[0]]);

    // another map replaces the store (and with it the classification) ...
    const std::string other = writeGain("other.xcal", 2.0f, {});
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(other.c_str()));
    std::vector<float> without;
    ASSERT_EQ(XPE_OK, run(kConfig, {kBad[0], kBad[1]}, &without));
    EXPECT_FLOAT_EQ(30000.0f, without[kBad[0]]) << "control: with the other map nothing is classified";

    // ... and the hit puts it back
    XpeImageBuffer v2{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain_cached(path.c_str(), &v2)) << "hit";
    std::vector<float> hit;
    ASSERT_EQ(XPE_OK, run(kConfig, {kBad[0], kBad[1]}, &hit));
    EXPECT_EQ(miss, hit) << "the hit corrects the same pixels the miss did";
    xpe_calib_cache_clear();
}
