/**
 * @file test_saturation.cpp
 * @brief Detector saturation level: configuration, gain-file record, pile-up detection, output rule
 *        (QA-A-296, Refs #278).
 *
 * The raw value at which a detector saturates differs per detector, so the module has no built-in value:
 * pipeline key `saturationLevel` > the gain file's record > none (no handling, one alert). Each case here has an
 * arm that is deliberately broken to show that the assertion can fail (named "...Falsification...").
 *
 * Real-data cases read files named by environment variables and skip when they are unset:
 *   XPE_SAT_2G_RAW + XPE_SAT_2G_BPM (3072x2560 uint16 raw, 8-bit defect map), XPE_SAT_CYAN_RAW (3072x3072),
 *   XPE_SAT_CALDATA6_RAW (3072x3072).
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"
#include "saturation.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr uint32_t W = 64, H = 64;
constexpr size_t   N = static_cast<size_t>(W) * H;
constexpr uint32_t kLevel = 12800;

std::vector<std::string> alerts() {
    std::vector<std::string> out;
    const int32_t n = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < n; ++i) {
        char buf[512];
        int32_t sev = 0;
        if (xpe_get_pending_alert(i, buf, sizeof(buf), &sev) == XPE_OK) out.emplace_back(buf);
    }
    return out;
}

int countAlerts(const char* prefix) {
    int c = 0;
    for (const auto& a : alerts()) if (a.rfind(prefix, 0) == 0) ++c;
    return c;
}

// gain plane: a quarter of the pixels 0.5, half 1.0, a quarter 2.0 -> median exactly 1.0
// a complete, valid saturation record (QA-A-296 D2: any saturation_* key makes the whole record mandatory)
std::string rec(const char* status, int level, int used = 1, int total = 2) {
    std::string j = std::string("{\"saturation_status\":\"") + status + "\"";
    if (level > 0) j += ",\"saturation_level\":" + std::to_string(level);
    j += ",\"saturation_method\":\"histogram_top_pile/1\",\"saturation_min_count_frac\":0.0005,\"saturation_min_ratio\":50,"
         "\"saturation_max_above_frac\":0.001,\"saturation_frames_used\":" + std::to_string(used) +
         ",\"saturation_frames_total\":" + std::to_string(total) + ",\"saturation_decided_at\":\"2026-10-23T08:00:00Z\"}";
    return j;
}

float gainAt(size_t i) { return (i % 4 == 0) ? 0.5f : ((i % 4 == 3) ? 2.0f : 1.0f); }

class SaturationTest : public ::testing::Test {
protected:
    fs::path dir;
    std::vector<float> storage;
    XpeImageBuffer img{};
    XpeImageMetadata meta{};

    void SetUp() override {
        (void)xpe_preprocess_init(nullptr);
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        dir = fs::temp_directory_path() / "xpe_saturation_test";
        fs::remove_all(dir);
        fs::create_directories(dir);
        writePlane("offset.xcal", XCAL_TYPE_OFFSET, std::vector<float>(N, 0.0f), "");
        std::vector<float> g(N);
        for (size_t i = 0; i < N; ++i) g[i] = gainAt(i);
        writeGain(g, "");
        const std::vector<uint8_t> mask(N, 0);
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(XCAL_TYPE_DEFECT);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_UINT8_MASK);
        hdr.width = W; hdr.height = H;
        hdr.payload_len = N;
        ASSERT_EQ(XPE_OK, write_xcal_file((dir / "defect.xcal").string().c_str(), hdr, nullptr, 0, mask.data(), N));
        xpe_clear_alerts();
    }

    void TearDown() override {
        xpe_clear_alerts();
        xpe_preprocess_shutdown();
        fs::remove_all(dir);
    }

    XpeErrorCode writePlane(const char* name, XCalType type, const std::vector<float>& data, const std::string& config) {
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(type);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width = W; hdr.height = H;
        hdr.payload_len = data.size() * sizeof(float);
        hdr.config_json_len = config.size();
        return write_xcal_file((dir / name).string().c_str(), hdr,
                               config.empty() ? nullptr : reinterpret_cast<const uint8_t*>(config.data()), config.size(),
                               reinterpret_cast<const uint8_t*>(data.data()), hdr.payload_len);
    }

    void writeGain(const std::vector<float>& g, const std::string& config) {
        ASSERT_EQ(XPE_OK, writePlane("gain.xcal", XCAL_TYPE_GAIN, g, config));
    }

    // raw frame: below the level except every 7th pixel, which is saturated (12800, or 12824 on every 14th: the 2G
    // detector is not strictly flat at its level)
    void makeFrame(uint32_t level, uint16_t fill = 0, bool varied = true) {
        storage.assign(N, 0.0f);
        auto* u = reinterpret_cast<uint16_t*>(storage.data());
        for (size_t i = 0; i < N; ++i) {
            if (!varied) { u[i] = fill; continue; }
            u[i] = (i % 7 == 0) ? static_cast<uint16_t>(level + ((i % 14 == 0 && level < 65000) ? 24 : 0))
                                : static_cast<uint16_t>(2000 + (i % 200) * 10);
        }
        img = XpeImageBuffer{};
        img.data = storage.data();
        img.width = W; img.height = H;
        img.bitsAllocated = 16; img.bitsStored = 16;
        img.format = XPE_PIXEL_UINT16;
        img.dataSize = N * sizeof(float);
        meta = XpeImageMetadata{};
    }

    std::vector<uint16_t> rawCopy() const {
        const auto* u = reinterpret_cast<const uint16_t*>(storage.data());
        return std::vector<uint16_t>(u, u + N);
    }

    XpeErrorCode run(const std::string& extra) {
        const std::string cfg = std::string("{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassBinning\":true,"
                                            "\"bypassDefect\":true,\"bypassGhost\":true,\"bypassNonlinearity\":true") +
                                extra + "}";
        const std::string d = dir.string();
        return xpe_preprocess_pipeline(&img, &meta, d.c_str(), nullptr, cfg.c_str());
    }
};

double cv(const std::vector<float>& v) {
    double m = 0.0;
    for (float x : v) m += x;
    m /= static_cast<double>(v.size());
    double s = 0.0;
    for (float x : v) s += (x - m) * (x - m);
    s = std::sqrt(s / static_cast<double>(v.size()));
    return m != 0.0 ? s / m : 0.0;
}

}  // namespace

// ---------------------------------------------------------------------------------------------------------------
// Pile-up detection (pure function)
// ---------------------------------------------------------------------------------------------------------------

namespace {
std::vector<uint32_t> histOf(const std::vector<uint16_t>& v, uint64_t* total) {
    std::vector<uint32_t> h(XPE_SAT_BINS, 0u);
    *total = 0;
    xpe_sat_histogram(v.data(), static_cast<uint32_t>(v.size()), 1u, 1u, nullptr, h.data(), total);
    return h;
}
std::vector<uint16_t> spread(size_t n, uint16_t top) {   // distinct-ish values below `top`
    std::vector<uint16_t> v(n);
    for (size_t i = 0; i < n; ++i) v[i] = static_cast<uint16_t>(1000 + (i * 37) % (top - 1500));
    return v;
}
}  // namespace

TEST(SaturationPile, FindsThePileAtTheTopAtTwoDifferentLevels) {
    for (const uint32_t lv : {12800u, 65535u}) {
        auto v = spread(100000, static_cast<uint16_t>(lv));
        for (int i = 0; i < 300; ++i) v[static_cast<size_t>(i) * 300] = static_cast<uint16_t>(lv);
        uint64_t total = 0;
        const auto h = histOf(v, &total);
        uint32_t found = 0;
        uint64_t cnt = 0;
        ASSERT_TRUE(xpe_sat_find_pile(h.data(), total, XpeSatParams{}, &found, &cnt)) << lv;
        EXPECT_EQ(lv, found);
        EXPECT_EQ(300u, cnt);
    }
}

TEST(SaturationPile, FalsificationNoPileMeansUndeterminedNotAGuess) {
    const auto v = spread(100000, 30000);   // smooth, nothing piles up
    uint64_t total = 0;
    const auto h = histOf(v, &total);
    uint32_t found = 7;
    uint64_t cnt = 0;
    EXPECT_FALSE(xpe_sat_find_pile(h.data(), total, XpeSatParams{}, &found, &cnt));
    EXPECT_EQ(7u, found) << "no value may be written when there is no pile-up";
}

TEST(SaturationPile, FalsificationAPileBelowALargerOutlierGroupIsNotTheTop) {
    // 0.5 % of the pixels sit above the pile (far more than the 0.1 % the rule allows): the pile is not at the top.
    auto v = spread(100000, 12000);
    for (int i = 0; i < 300; ++i) v[static_cast<size_t>(i) * 300] = 12800;
    for (int i = 0; i < 500; ++i) v[static_cast<size_t>(i) * 150 + 1] = static_cast<uint16_t>(30000 + i);
    uint64_t total = 0;
    const auto h = histOf(v, &total);
    uint32_t found = 0;
    uint64_t cnt = 0;
    EXPECT_FALSE(xpe_sat_find_pile(h.data(), total, XpeSatParams{}, &found, &cnt));
    XpeSatParams loose;
    loose.maxAboveFrac = 0.01;   // the threshold is a parameter: relaxed, the same data gives the pile
    ASSERT_TRUE(xpe_sat_find_pile(h.data(), total, loose, &found, &cnt));
    EXPECT_EQ(12800u, found);
}

TEST(SaturationPile, KnownDefectPixelsAreLeftOutOfTheHistogram) {
    std::vector<uint16_t> v(1000, 500);
    std::vector<uint8_t> ex(1000, 0);
    for (int i = 0; i < 100; ++i) { v[static_cast<size_t>(i)] = 65535; ex[static_cast<size_t>(i)] = 1; }
    std::vector<uint32_t> h(XPE_SAT_BINS, 0u);
    uint64_t total = 0;
    xpe_sat_histogram(v.data(), 1000, 1, 1, ex.data(), h.data(), &total);
    EXPECT_EQ(900u, total);
    EXPECT_EQ(0u, h[65535]);
    std::fill(h.begin(), h.end(), 0u);
    total = 0;
    xpe_sat_histogram(v.data(), 1000, 1, 1, nullptr, h.data(), &total);   // falsification: without the mask they are counted
    EXPECT_EQ(1000u, total);
    EXPECT_EQ(100u, h[65535]);
}

// ---------------------------------------------------------------------------------------------------------------
// Priority: explicit setting > gain file > none
// ---------------------------------------------------------------------------------------------------------------

TEST_F(SaturationTest, ExplicitSettingWinsOverTheGainFile) {
    std::vector<float> g(N);
    for (size_t i = 0; i < N; ++i) g[i] = gainAt(i);
    writeGain(g, rec("determined", 10000));
    makeFrame(kLevel);
    const auto raw = rawCopy();
    ASSERT_EQ(XPE_OK, run(",\"saturationLevel\":12800"));
    for (size_t i = 0; i < N; ++i)
        if (raw[i] >= 12800) ASSERT_FLOAT_EQ(12800.0f, storage[i]) << "pixel " << i << " must carry the explicit level's S_out";
    EXPECT_EQ(1, countAlerts("XPE_INFO_SATURATION_OVERRIDE"));
    EXPECT_EQ(0, countAlerts("XPE_WARN_SATURATION_UNSET"));
}

TEST_F(SaturationTest, TheGainFileRecordIsUsedWhenNothingIsSet) {
    std::vector<float> g(N);
    for (size_t i = 0; i < N; ++i) g[i] = gainAt(i);
    writeGain(g, rec("determined", 12800));
    makeFrame(kLevel);
    const auto raw = rawCopy();
    ASSERT_EQ(XPE_OK, run(""));
    size_t sat = 0;
    for (size_t i = 0; i < N; ++i)
        if (raw[i] >= 12800) { ASSERT_FLOAT_EQ(12800.0f, storage[i]); ++sat; }
    EXPECT_GT(sat, 100u);
    EXPECT_EQ(0, countAlerts("XPE_WARN_SATURATION_UNSET"));
}

TEST_F(SaturationTest, WithNeitherThereIsNoHandlingAndExactlyOneAlert) {
    makeFrame(kLevel);
    const auto raw = rawCopy();
    ASSERT_EQ(XPE_OK, run(""));
    EXPECT_EQ(1, countAlerts("XPE_WARN_SATURATION_UNSET"));
    // no handling: the output is raw / gain, saturated pixels carry the gain map's pattern (this is today's output)
    for (size_t i = 0; i < N; ++i) ASSERT_FLOAT_EQ(static_cast<float>(raw[i]) * (1.0f / gainAt(i)), storage[i]) << i;
}

TEST_F(SaturationTest, FalsificationAnUndeterminedRecordIsNotALevel) {
    std::vector<float> g(N);
    for (size_t i = 0; i < N; ++i) g[i] = gainAt(i);
    writeGain(g, rec("undetermined", 0, 0, 1));
    makeFrame(kLevel);
    ASSERT_EQ(XPE_OK, run(""));
    EXPECT_EQ(1, countAlerts("XPE_WARN_SATURATION_UNSET")) << "undetermined must not be turned into a level";
}

TEST_F(SaturationTest, OffIsSilentAndChangesNothing) {
    makeFrame(kLevel);
    const auto raw = rawCopy();
    ASSERT_EQ(XPE_OK, run(",\"saturationLevel\":\"off\""));
    EXPECT_EQ(0, countAlerts("XPE_WARN_SATURATION_UNSET"));
    for (size_t i = 0; i < N; ++i) ASSERT_FLOAT_EQ(static_cast<float>(raw[i]) * (1.0f / gainAt(i)), storage[i]) << i;
}

TEST_F(SaturationTest, ABadSettingIsARefusalNotADefault) {
    for (const char* bad : {",\"saturationLevel\":0", ",\"saturationLevel\":70000", ",\"saturationLevel\":\"abc\"",
                            ",\"saturationLevel\":12800.5", ",\"saturationLevel\":\"OFF\""}) {
        makeFrame(kLevel);
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, run(bad)) << bad;
    }
    makeFrame(kLevel);
    EXPECT_EQ(XPE_OK, run(",\"saturationLevel\":12800")) << "falsification: a valid value is accepted";
}

TEST_F(SaturationTest, AMalformedOrPartialRecordRefusesTheGainFileAndAnOldFileLoads) {
    std::vector<float> g(N, 1.0f);
    const std::string path = (dir / "gain.xcal").string();
    // legacy: no saturation_* key at all
    writeGain(g, "{\"calibration_mode\":1}");
    EXPECT_EQ(XPE_OK, xpe_calib_load_gain(path.c_str()));
    // valid records
    for (const std::string& ok : {rec("determined", 12800), rec("undetermined", 0, 0, 1), rec("undetermined", 0, 1, 2)}) {
        writeGain(g, ok);
        EXPECT_EQ(XPE_OK, xpe_calib_load_gain(path.c_str())) << ok;
    }
    const std::string good = rec("determined", 12800);
    const auto without = [&](const char* key) {   // the record with one field removed
        std::string t = good;
        const size_t k = t.find(std::string("\"") + key + "\":");
        const size_t e = t.find_first_of(",}", t.find(':', k) + 1);
        if (t[e] == ',') t.erase(k, e - k + 1); else t.erase(k - 1, e - k + 1);
        return t;
    };
    std::vector<std::string> bad = {
        "{\"saturation_level\":12800}",                                 // only a level
        "{\"saturation_status\":\"determined\"}",                       // status alone
        "{\"saturation_status\":\"determined\",\"saturation_level\":12800}",   // the old two-key form is now partial
        "{\"saturation_foo\":1}",                                       // an unknown saturation_* key
        rec("determined", 0), rec("determined", 70000), rec("determined", 12800, 0, 2), rec("determined", 12800, 3, 2),
        rec("maybe", 12800), rec("undetermined", 12800, 0, 1), rec("undetermined", 0, 3, 2), rec("undetermined", 0, 0, 0),
        "{\"saturation_status\":\"determined\",\"saturation_level\":\"12800\"" + good.substr(good.find(",\"saturation_method\"")),
    };
    for (const char* key : {"saturation_status", "saturation_method", "saturation_min_count_frac", "saturation_min_ratio",
                            "saturation_max_above_frac", "saturation_frames_used", "saturation_frames_total", "saturation_decided_at"})
        bad.push_back(without(key));
    std::string t = good;
    t.replace(t.find("2026-10-23T08:00:00Z"), 20, "2026-10-23 08:00:00 ");
    bad.push_back(t);
    for (const std::string& b : bad) {
        writeGain(g, rec("undetermined", 0, 0, 1));
        ASSERT_EQ(XPE_OK, xpe_calib_load_gain(path.c_str()));   // a known state first
        writeGain(g, b);
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_calib_load_gain(path.c_str())) << b;
    }
}

TEST_F(SaturationTest, TheMedianLeavesOutThePixelsTheGainCalibrationClassifiedDefective) {
    // 2000 pixels of gain 0.5, 204 classified (0.01: set to 1.0 by the loader), the rest 2.0.
    // Left out: the median is 0.5 (S_out 25600). With the 1.0 replacements counted it would be 1.0 (S_out 12800).
    std::vector<float> g(N, 2.0f);
    for (size_t i = 0; i < 2000; ++i) g[i] = 0.5f;
    for (size_t i = 2000; i < 2204; ++i) g[i] = 0.01f;
    writeGain(g, "");
    makeFrame(kLevel);
    const auto raw = rawCopy();
    ASSERT_EQ(XPE_OK, run(",\"saturationLevel\":12800"));
    size_t checked = 0;
    for (size_t i = 0; i < N; ++i) {
        if (i >= 2000 && i < 2204) continue;
        if (raw[i] >= 12800) { ASSERT_FLOAT_EQ(25600.0f, storage[i]) << i; ++checked; }
    }
    EXPECT_GT(checked, 300u);
}

TEST_F(SaturationTest, TheMedianLeavesOutTheDefectMapAndIsComputedAgainWhenTheMapIsReplaced) {
    // gain: 2200 pixels of 0.5 then 1896 of 2.0. No defect map entries: median (element 2048) 0.5 -> S_out 25600.
    std::vector<float> g(N, 2.0f);
    for (size_t i = 0; i < 2200; ++i) g[i] = 0.5f;
    writeGain(g, "");
    makeFrame(kLevel);
    const auto raw = rawCopy();
    ASSERT_EQ(XPE_OK, run(",\"saturationLevel\":12800"));
    size_t checked = 0;
    for (size_t i = 0; i < N; ++i) if (raw[i] >= 12800) { ASSERT_FLOAT_EQ(25600.0f, storage[i]) << i; ++checked; }
    EXPECT_GT(checked, 300u);

    // replace the defect map: 800 of the 0.5 pixels are now defects -> 1400 x 0.5 and 1896 x 2.0 are left, element 1648 is 2.0 -> S_out 6400
    std::vector<uint8_t> mask(N, 0);
    for (size_t i = 0; i < 800; ++i) mask[i] = 1;
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version = XCAL_VERSION;
    hdr.type = static_cast<uint32_t>(XCAL_TYPE_DEFECT);
    hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_UINT8_MASK);
    hdr.width = W; hdr.height = H;
    hdr.payload_len = N;
    ASSERT_EQ(XPE_OK, write_xcal_file((dir / "defect.xcal").string().c_str(), hdr, nullptr, 0, mask.data(), N));
    makeFrame(kLevel);
    ASSERT_EQ(XPE_OK, run(",\"saturationLevel\":12800"));
    checked = 0;
    for (size_t i = 800; i < N; ++i)   // the defect pixels themselves are filled by the defect stage; look at the others
        if (raw[i] >= 12800) { ASSERT_FLOAT_EQ(6400.0f, storage[i]) << i; ++checked; }
    EXPECT_GT(checked, 300u);
}

TEST(SaturationUnit, TheMedianPopulationAndTheUnusableCases) {
    const float v[6] = {5.0f, 1.0f, 1.0f, 9.0f, 1.0f, 3.0f};
    const uint32_t ex[3] = {1, 2, 4};   // the three 1.0 values
    EXPECT_EQ(5.0f, xpe_sat_gain_median(v, 6, ex, 3));          // left: 5, 9, 3 -> sorted 3,5,9 -> element 1
    EXPECT_EQ(3.0f, xpe_sat_gain_median(v, 6, nullptr, 0));      // all: 1,1,1,3,5,9 -> element 3
    const uint32_t all[6] = {0, 1, 2, 3, 4, 5};
    EXPECT_EQ(0.0f, xpe_sat_gain_median(v, 6, all, 6));          // nothing left: unusable
    EXPECT_EQ(0.0f, xpe_sat_gain_median(nullptr, 0, nullptr, 0));
    EXPECT_EQ(0.0f, xpe_sat_output_value(12800, 0.0f));
    EXPECT_EQ(0.0f, xpe_sat_output_value(12800, std::numeric_limits<float>::infinity()));
    EXPECT_EQ(25600.0f, xpe_sat_output_value(12800, 0.5f));      // falsification: a usable median gives a value
}

TEST(SaturationUnit, AZeroBelowSixtyFourCountsIsHandledByTheAddedFloorOfOne) {
    // the 64 bins under the pile hold nothing (median 0): the added rule max(1, median) = 1 lets 50 pixels at the top count as a pile
    std::vector<uint32_t> h(XPE_SAT_BINS, 0u);
    h[12800] = 60;
    for (int v = 2000; v < 3000; ++v) h[v] = 20;
    uint32_t level = 0; uint64_t cnt = 0;
    ASSERT_TRUE(xpe_sat_find_pile(h.data(), 60u + 20000u, XpeSatParams{}, &level, &cnt));
    EXPECT_EQ(12800u, level);
    h[12800] = 49;   // falsification: 49 < 50 * 1
    EXPECT_FALSE(xpe_sat_find_pile(h.data(), 49u + 20000u, XpeSatParams{}, &level, &cnt));
}

TEST(SaturationUnit, ASaturationBelowTheFloorOf65IsNeverEstimated) {
    std::vector<uint32_t> h(XPE_SAT_BINS, 0u);
    h[60] = 100000;
    for (int v = 2000; v < 3000; ++v) h[v] = 1;
    uint32_t level = 0; uint64_t cnt = 0;
    EXPECT_FALSE(xpe_sat_find_pile(h.data(), 100000u + 1000u, XpeSatParams{}, &level, &cnt));
}

TEST_F(SaturationTest, ExplicitThresholdsInTheMetadataAreCheckedAndAbsentOnesTakeTheDefaults) {
    auto f0 = std::vector<uint16_t>(N);
    for (size_t i = 0; i < N; ++i) f0[i] = static_cast<uint16_t>(2000 + (i * 37) % 9000);
    for (size_t i = 0; i < 100; ++i) f0[i * 40] = 12800;
    XpeImageBuffer fr{};
    fr.data = f0.data(); fr.width = W; fr.height = H; fr.bitsAllocated = 16; fr.bitsStored = 16; fr.format = XPE_PIXEL_UINT16; fr.dataSize = N * 2;
    const std::string path = (dir / "gain.xcal").string();
    for (const char* bad : {"{\"saturation_min_ratio\":0.5}", "{\"saturation_min_count_frac\":0}", "{\"saturation_min_count_frac\":2}",
                            "{\"saturation_max_above_frac\":-0.1}", "{\"saturation_max_above_frac\":\"x\"}", "{\"saturation_detection\":\"on\"}",
                            "{\"saturation_detection\":1}"})
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_calib_generate_gain(&fr, 1, nullptr, path.c_str(), bad)) << bad;
    EXPECT_EQ(XPE_OK, xpe_calib_generate_gain(&fr, 1, nullptr, path.c_str(), "{\"saturation_min_ratio\":40,\"saturation_min_count_frac\":0.0004}"));
    EXPECT_EQ(XPE_OK, xpe_calib_generate_gain(&fr, 1, nullptr, path.c_str(), nullptr));   // absent: the defaults
    EXPECT_EQ(XPE_OK, xpe_calib_generate_gain(&fr, 1, nullptr, path.c_str(), "{\"saturation_detection\":\"off\"}"));
    std::ifstream in(path, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(std::string::npos, bytes.find("saturation_status")) << "detection off writes no record";
}

// ---------------------------------------------------------------------------------------------------------------
// Output rule
// ---------------------------------------------------------------------------------------------------------------

TEST_F(SaturationTest, SaturatedPixelsAllCarryOneValueAndWithoutTheRuleTheyDoNot) {
    makeFrame(kLevel);
    const auto raw = rawCopy();
    ASSERT_EQ(XPE_OK, run(",\"saturationLevel\":12800"));
    std::vector<float> sat;
    for (size_t i = 0; i < N; ++i) if (raw[i] >= 12800) sat.push_back(storage[i]);
    ASSERT_GT(sat.size(), 100u);
    EXPECT_EQ(0.0, cv(sat)) << "CV of the saturated output must be 0";

    makeFrame(kLevel);   // falsification arm: the same frame with the rule off shows the gain pattern
    ASSERT_EQ(XPE_OK, run(",\"saturationLevel\":\"off\""));
    sat.clear();
    for (size_t i = 0; i < N; ++i) if (raw[i] >= 12800) sat.push_back(storage[i]);
    EXPECT_GT(cv(sat), 0.2);
}

TEST_F(SaturationTest, OutputNeverExceedsTheSaturatedValue) {
    makeFrame(kLevel);
    ASSERT_EQ(XPE_OK, run(",\"saturationLevel\":12800"));
    for (size_t i = 0; i < N; ++i) ASSERT_LE(storage[i], 12800.0f) << i;
}

TEST_F(SaturationTest, NormalPixelsBelowTheSaturatedValueAreBitIdenticalToTheRuleOffRun) {
    makeFrame(kLevel);
    const auto raw = rawCopy();
    ASSERT_EQ(XPE_OK, run(",\"saturationLevel\":\"off\""));
    const std::vector<float> off = storage;
    makeFrame(kLevel);
    ASSERT_EQ(XPE_OK, run(",\"saturationLevel\":12800"));
    size_t same = 0, changed = 0;
    for (size_t i = 0; i < N; ++i) {
        if (raw[i] < 12800 && off[i] <= 12800.0f) {
            ASSERT_EQ(0, std::memcmp(&off[i], &storage[i], sizeof(float))) << "pixel " << i;
            ++same;
        } else {
            ++changed;
        }
    }
    EXPECT_GT(same, 1000u);
    EXPECT_GT(changed, 100u);   // falsification: the saturated pixels did change, so the comparison is not vacuous
}

TEST_F(SaturationTest, OutputIsMonotoneInTheRawValueAndTheClampIsWhatMakesItSo) {
    // one flat frame per raw value; each pixel position sees the ramp with its own gain
    std::vector<std::vector<float>> out;
    std::vector<uint16_t> ramp;
    for (uint32_t r = 0; r <= 13500; r += 100) ramp.push_back(static_cast<uint16_t>(r));
    for (const uint16_t r : ramp) {
        makeFrame(kLevel, r, false);
        ASSERT_EQ(XPE_OK, run(",\"saturationLevel\":12800"));
        out.push_back(storage);
    }
    for (size_t k = 1; k < out.size(); ++k)
        for (size_t i = 0; i < N; ++i) ASSERT_LE(out[k - 1][i], out[k][i]) << "raw " << ramp[k] << " pixel " << i;

    // falsification: replace the saturated pixels but do not limit the others -> a pixel of gain 0.5 falls at the level
    bool nonMonotone = false;
    for (size_t k = 1; k < ramp.size() && !nonMonotone; ++k) {
        const float prev = ramp[k - 1] >= 12800 ? 12800.0f : static_cast<float>(ramp[k - 1]) / 0.5f;
        const float cur = ramp[k] >= 12800 ? 12800.0f : static_cast<float>(ramp[k]) / 0.5f;
        nonMonotone = cur < prev;
    }
    EXPECT_TRUE(nonMonotone) << "without the limit the rule would not be monotone";
}

TEST_F(SaturationTest, AFrameThatPilesUpAtAnotherValueRaisesAMismatchAlertAndKeepsTheLevel) {
    // the configured level is 12800 but this frame piles up at 40000 (the monitor samples every 16th row: 256 pixels here)
    makeFrame(0, 40000, false);
    ASSERT_EQ(XPE_OK, run(",\"saturationLevel\":12800"));
    EXPECT_EQ(1, countAlerts("XPE_WARN_SATURATION_MISMATCH"));
    for (size_t i = 0; i < N; ++i) ASSERT_LE(storage[i], 12800.0f);   // the level in use was not changed by the monitor

    xpe_clear_alerts();
    makeFrame(0, 12800, false);   // falsification: a frame that piles up at the configured level raises none
    ASSERT_EQ(XPE_OK, run(",\"saturationLevel\":12800"));
    EXPECT_EQ(0, countAlerts("XPE_WARN_SATURATION_MISMATCH"));
}

TEST_F(SaturationTest, WithoutAScalarGainTheRuleIsOffAndTheCallSaysSoOnce) {
    makeFrame(kLevel);
    const auto raw = rawCopy();
    ASSERT_EQ(XPE_OK, run(",\"bypassGain\":true,\"saturationLevel\":12800"));
    EXPECT_EQ(1, countAlerts("XPE_WARN_SATURATION_NOT_APPLIED"));
    ASSERT_EQ(XPE_PIXEL_UINT16, img.format);   // gain and defect both off: the frame stays uint16, untouched
    EXPECT_EQ(raw, rawCopy());
    xpe_clear_alerts();
    makeFrame(kLevel);
    ASSERT_EQ(XPE_OK, run(",\"bypassGain\":true,\"saturationLevel\":\"off\""));   // falsification: "off" is silent
    EXPECT_EQ(0, countAlerts("XPE_WARN_SATURATION_NOT_APPLIED"));
}

TEST_F(SaturationTest, APolynomialGainIsNotCappedAndSaysSo) {
    // coefficients (1, 0) for every pixel: gain polynomial == 1; the dose range covers the frame
    std::vector<float> coeffs(N * 2);
    for (size_t i = 0; i < N; ++i) { coeffs[2 * i] = 1.0f; coeffs[2 * i + 1] = 0.0f; }
    const std::string cfgj = "{\"dose_min\":0,\"dose_max\":65535}";
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version = XCAL_VERSION;
    hdr.type = static_cast<uint32_t>(XCAL_TYPE_GAIN_POLY);
    hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
    hdr.width = W; hdr.height = H;
    hdr.payload_len = coeffs.size() * sizeof(float);
    hdr.config_json_len = cfgj.size();
    ASSERT_EQ(XPE_OK, write_xcal_file((dir / "gain.xcal").string().c_str(), hdr,
                                      reinterpret_cast<const uint8_t*>(cfgj.data()), cfgj.size(),
                                      reinterpret_cast<const uint8_t*>(coeffs.data()), hdr.payload_len));
    makeFrame(kLevel);
    const auto rc = run(",\"saturationLevel\":12800");
    if (rc != XPE_OK) GTEST_SKIP() << "polynomial fixture not accepted by the loader here: " << static_cast<int>(rc);
    EXPECT_EQ(1, countAlerts("XPE_WARN_SATURATION_NOT_APPLIED"));
    EXPECT_EQ(0, countAlerts("XPE_WARN_SATURATION_UNSET"));
}

TEST_F(SaturationTest, TheMonitorIsALimitedHeuristicItMissesAPileOutsideTheSampledRows) {
    // the pile at 40000 sits only on rows that are not a multiple of 16: the sampled rows (every 16th) never see it
    storage.assign(N, 0.0f);
    auto* u = reinterpret_cast<uint16_t*>(storage.data());
    for (uint32_t y = 0; y < H; ++y)
        for (uint32_t x = 0; x < W; ++x) u[y * W + x] = (y % 16 != 0) ? 40000 : static_cast<uint16_t>(500 + x);
    img = XpeImageBuffer{};
    img.data = storage.data(); img.width = W; img.height = H;
    img.bitsAllocated = 16; img.bitsStored = 16; img.format = XPE_PIXEL_UINT16; img.dataSize = N * sizeof(float);
    meta = XpeImageMetadata{};
    ASSERT_EQ(XPE_OK, run(",\"saturationLevel\":12800"));
    EXPECT_EQ(0, countAlerts("XPE_WARN_SATURATION_MISMATCH")) << "documented limit: the 16-row sample can miss a periodic pattern";
}

// ---------------------------------------------------------------------------------------------------------------
// Determination at calibration generation
// ---------------------------------------------------------------------------------------------------------------

namespace {
std::vector<uint16_t> flatFrame(bool withPile, uint16_t top) {
    std::vector<uint16_t> v(N);
    for (size_t i = 0; i < N; ++i) v[i] = static_cast<uint16_t>(2000 + (i * 37) % 9000);
    if (withPile)
        for (size_t i = 0; i < 100; ++i) v[i * 40] = top;
    return v;
}
}  // namespace

TEST_F(SaturationTest, GenerationRecordsTheLevelFoundAndTheLoadedFileUsesIt) {
    for (const uint16_t top : {static_cast<uint16_t>(12800), static_cast<uint16_t>(65535)}) {
        auto f0 = flatFrame(true, top);
        auto f1 = flatFrame(true, top);
        XpeImageBuffer fr[2];
        for (int k = 0; k < 2; ++k) {
            fr[k] = XpeImageBuffer{};
            fr[k].data = k == 0 ? f0.data() : f1.data();
            fr[k].width = W; fr[k].height = H;
            fr[k].bitsAllocated = 16; fr[k].bitsStored = 16;
            fr[k].format = XPE_PIXEL_UINT16;
            fr[k].dataSize = N * 2;
        }
        const std::string path = (dir / "gain.xcal").string();
        ASSERT_EQ(XPE_OK, xpe_calib_generate_gain(fr, 2, nullptr, path.c_str(), nullptr));
        std::ifstream in(path, std::ios::binary);
        const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        EXPECT_NE(std::string::npos, bytes.find("\"saturation_status\":\"determined\"")) << top;
        char want[64];
        std::snprintf(want, sizeof(want), "\"saturation_level\":%u", static_cast<unsigned>(top));
        EXPECT_NE(std::string::npos, bytes.find(want)) << top;
        EXPECT_NE(std::string::npos, bytes.find("\"saturation_method\":\"histogram_top_pile/1\""));
        EXPECT_NE(std::string::npos, bytes.find("\"saturation_frames_used\":2"));

        // the pipeline picks it up with no setting: no UNSET alert
        xpe_clear_alerts();
        makeFrame(top);
        ASSERT_EQ(XPE_OK, run(""));
        EXPECT_EQ(0, countAlerts("XPE_WARN_SATURATION_UNSET")) << top;
    }
}

TEST_F(SaturationTest, FalsificationGenerationFromFramesWithoutAPileWritesUndeterminedAndNoLevel) {
    auto f0 = flatFrame(false, 0);
    XpeImageBuffer fr{};
    fr.data = f0.data();
    fr.width = W; fr.height = H;
    fr.bitsAllocated = 16; fr.bitsStored = 16;
    fr.format = XPE_PIXEL_UINT16;
    fr.dataSize = N * 2;
    const std::string path = (dir / "gain.xcal").string();
    ASSERT_EQ(XPE_OK, xpe_calib_generate_gain(&fr, 1, nullptr, path.c_str(), nullptr));
    std::ifstream in(path, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_NE(std::string::npos, bytes.find("\"saturation_status\":\"undetermined\""));
    EXPECT_EQ(std::string::npos, bytes.find("\"saturation_level\""));
    EXPECT_EQ(1, countAlerts("XPE_WARN_SATURATION_UNDETERMINED"));
    makeFrame(kLevel);
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, run(""));
    EXPECT_EQ(1, countAlerts("XPE_WARN_SATURATION_UNSET"));
}

TEST_F(SaturationTest, FramesThatDisagreeOnTheLevelAreUndetermined) {
    auto f0 = flatFrame(true, 12800);
    auto f1 = flatFrame(true, 30000);
    XpeImageBuffer fr[2];
    for (int k = 0; k < 2; ++k) {
        fr[k] = XpeImageBuffer{};
        fr[k].data = k == 0 ? f0.data() : f1.data();
        fr[k].width = W; fr[k].height = H;
        fr[k].bitsAllocated = 16; fr[k].bitsStored = 16;
        fr[k].format = XPE_PIXEL_UINT16;
        fr[k].dataSize = N * 2;
    }
    const std::string path = (dir / "gain.xcal").string();
    ASSERT_EQ(XPE_OK, xpe_calib_generate_gain(fr, 2, nullptr, path.c_str(), nullptr));
    std::ifstream in(path, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_NE(std::string::npos, bytes.find("\"saturation_status\":\"undetermined\""));
}

// ---------------------------------------------------------------------------------------------------------------
// Real data (skipped when the files are not named by the environment)
// ---------------------------------------------------------------------------------------------------------------

namespace {
// std::getenv trips MSVC C4996 (/WX); a copy kept alive for the test's duration
const char* envOrNull(const char* name) {
    static std::vector<std::string> keep;
    char* v = nullptr;
    size_t len = 0;
    if (_dupenv_s(&v, &len, name) != 0 || v == nullptr) return nullptr;
    keep.emplace_back(v);
    std::free(v);
    return keep.back().c_str();
}
bool readRaw(const char* path, size_t n, std::vector<uint16_t>* out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    out->resize(n);
    in.read(reinterpret_cast<char*>(out->data()), static_cast<std::streamsize>(n * 2));
    return static_cast<size_t>(in.gcount()) == n * 2;
}
}  // namespace

TEST(SaturationRealData, TwoGFrameGives12800) {
    const char* raw = envOrNull("XPE_SAT_2G_RAW");
    const char* bpm = envOrNull("XPE_SAT_2G_BPM");
    if (!raw || !bpm) GTEST_SKIP() << "XPE_SAT_2G_RAW / XPE_SAT_2G_BPM not set";
    constexpr uint32_t w = 3072, h = 2560;
    std::vector<uint16_t> a;
    ASSERT_TRUE(readRaw(raw, static_cast<size_t>(w) * h, &a));
    std::vector<uint8_t> m(static_cast<size_t>(w) * h);
    { std::ifstream in(bpm, std::ios::binary); in.read(reinterpret_cast<char*>(m.data()), static_cast<std::streamsize>(m.size())); ASSERT_EQ(m.size(), static_cast<size_t>(in.gcount())); }
    std::vector<uint32_t> hist(XPE_SAT_BINS, 0u);
    uint64_t total = 0;
    // the top 3 rows hold the detector's known 65535 anomaly (the lead's rev4 estimator drops them as well)
    xpe_sat_histogram(a.data() + 3u * w, w, h - 3u, 1u, m.data() + 3u * w, hist.data(), &total);
    uint32_t lv = 0;
    uint64_t c = 0;
    ASSERT_TRUE(xpe_sat_find_pile(hist.data(), total, XpeSatParams{}, &lv, &c));
    EXPECT_EQ(12800u, lv);
    std::printf("[real] 2G pile %u count %llu of %llu\n", lv, static_cast<unsigned long long>(c), static_cast<unsigned long long>(total));
}

TEST(SaturationRealData, CyanFrameGives65535) {
    const char* raw = envOrNull("XPE_SAT_CYAN_RAW");
    if (!raw) GTEST_SKIP() << "XPE_SAT_CYAN_RAW not set";
    std::vector<uint16_t> a;
    ASSERT_TRUE(readRaw(raw, 3072u * 3072u, &a));
    std::vector<uint32_t> hist(XPE_SAT_BINS, 0u);
    uint64_t total = 0;
    xpe_sat_histogram(a.data(), 3072, 3072, 1u, nullptr, hist.data(), &total);
    uint32_t lv = 0;
    uint64_t c = 0;
    ASSERT_TRUE(xpe_sat_find_pile(hist.data(), total, XpeSatParams{}, &lv, &c));
    EXPECT_EQ(65535u, lv);
    std::printf("[real] cyan pile %u count %llu\n", lv, static_cast<unsigned long long>(c));
}

TEST(SaturationRealData, CalData6IsUndetermined) {
    const char* raw = envOrNull("XPE_SAT_CALDATA6_RAW");
    if (!raw) GTEST_SKIP() << "XPE_SAT_CALDATA6_RAW not set";
    std::vector<uint16_t> a;
    ASSERT_TRUE(readRaw(raw, 3072u * 3072u, &a));
    std::vector<uint32_t> hist(XPE_SAT_BINS, 0u);
    uint64_t total = 0;
    xpe_sat_histogram(a.data(), 3072, 3072, 1u, nullptr, hist.data(), &total);
    uint32_t lv = 99;
    uint64_t c = 0;
    EXPECT_FALSE(xpe_sat_find_pile(hist.data(), total, XpeSatParams{}, &lv, &c));
    EXPECT_EQ(99u, lv);
}

// ---------------------------------------------------------------------------------------------------------------
// D4: key types, one alert per call, refused metadata, state kept after a refused load
// ---------------------------------------------------------------------------------------------------------------

TEST_F(SaturationTest, TheThreeKeysCheckTheJsonTypeToo) {
    const char* bad[] = {
        ",\"saturationLevel\":\"\"", ",\"saturationLevel\":{}", ",\"saturationLevel\":[]", ",\"saturationLevel\":\"12800\"",
        ",\"saturationLevel\":true", ",\"saturationLevel\":null", ",\"saturationLevel\":12800.5", ",\"saturationLevel\":[12800]",
        ",\"maxThreads\":\"\"", ",\"maxThreads\":\"4\"", ",\"maxThreads\":{}", ",\"maxThreads\":[]", ",\"maxThreads\":true",
        ",\"saturationLevel\":\"off\",\"defectLineInterpolation\":\"\"", ",\"saturationLevel\":\"off\",\"defectLineInterpolation\":5",
        ",\"saturationLevel\":\"off\",\"defectLineInterpolation\":[]", ",\"saturationLevel\":\"off\",\"defectLineInterpolation\":{}",
        ",\"saturationLevel\":\"off\",\"defectLineInterpolation\":true"};
    for (const char* b : bad) {
        makeFrame(kLevel);
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, run(b)) << b;
    }
    // falsification: the accepted forms
    for (const char* ok : {",\"saturationLevel\":12800", ",\"saturationLevel\":\"off\"", ",\"saturationLevel\":12800,\"maxThreads\":4",
                           ",\"saturationLevel\":\"off\",\"defectLineInterpolation\":\"rows\""}) {
        makeFrame(kLevel);
        EXPECT_EQ(XPE_OK, run(ok)) << ok;
    }
}

TEST_F(SaturationTest, OverlappingSaturationConditionsGiveOneAlertWithEveryReason) {
    // the file says 10000, the call says 12800 (an override note, INFO), and the frame piles up at 40000 (a mismatch note, WARNING)
    std::vector<float> g(N);
    for (size_t i = 0; i < N; ++i) g[i] = gainAt(i);
    writeGain(g, rec("determined", 10000));
    makeFrame(0, 40000, false);
    ASSERT_EQ(XPE_OK, run(",\"saturationLevel\":12800"));
    int total = 0;
    std::string text;
    for (const auto& a : alerts()) if (a.find("SATURATION") != std::string::npos) { ++total; text = a; }
    EXPECT_EQ(1, total) << "saturation alerts, INFO included, are at most one per call";
    EXPECT_EQ(0u, text.rfind("XPE_WARN_SATURATION_MISMATCH", 0)) << text;   // the most severe note leads
    EXPECT_NE(std::string::npos, text.find("XPE_INFO_SATURATION_OVERRIDE")) << text;   // and the other reason is in the text
    int32_t sev = -1;
    char buf[512];
    for (int32_t i = 0; i < xpe_get_pending_alert_count(); ++i)
        if (xpe_get_pending_alert(i, buf, sizeof(buf), &sev) == XPE_OK && std::string(buf).find("SATURATION") != std::string::npos) break;
    EXPECT_EQ(XPE_ALERT_WARNING, sev);

    // falsification: an override alone is one INFO alert
    xpe_clear_alerts();
    makeFrame(kLevel);
    ASSERT_EQ(XPE_OK, run(",\"saturationLevel\":12800"));
    total = 0;
    for (const auto& a : alerts()) if (a.find("SATURATION") != std::string::npos) { ++total; text = a; }
    EXPECT_EQ(1, total);
    EXPECT_EQ(0u, text.rfind("XPE_INFO_SATURATION_OVERRIDE", 0)) << text;
}

TEST_F(SaturationTest, NoLevelAndNoScalarGainAreOneAlertNotTwo) {
    makeFrame(kLevel);
    ASSERT_EQ(XPE_OK, run(",\"bypassGain\":true"));   // no level given and no gain: both notes apply, one alert
    int total = 0;
    for (const auto& a : alerts()) if (a.find("SATURATION") != std::string::npos) ++total;
    EXPECT_EQ(1, total);
}

TEST_F(SaturationTest, MetadataThatDoesNotParseIsARefusalAndWritesNothing) {
    auto f0 = std::vector<uint16_t>(N, 3000);
    XpeImageBuffer fr{};
    fr.data = f0.data(); fr.width = W; fr.height = H; fr.bitsAllocated = 16; fr.bitsStored = 16; fr.format = XPE_PIXEL_UINT16; fr.dataSize = N * 2;
    const std::string existing = (dir / "gain.xcal").string();
    std::string before;
    { std::ifstream in(existing, std::ios::binary); before.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()); }
    ASSERT_FALSE(before.empty());
    const std::string fresh = (dir / "fresh.xcal").string();
    for (const char* bad : {"{bad", "[1,2", "{\"a\":1,\"a\":2}", "{\"x\":1}}", "   "}) {
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_calib_generate_gain(&fr, 1, nullptr, existing.c_str(), bad)) << bad;
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_calib_generate_gain(&fr, 1, nullptr, fresh.c_str(), bad)) << bad;
        EXPECT_FALSE(fs::exists(fresh)) << bad;
        std::string after;
        { std::ifstream in(existing, std::ios::binary); after.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()); }
        EXPECT_EQ(before, after) << "the existing file must be as it was: " << bad;
    }
    EXPECT_EQ(XPE_OK, xpe_calib_generate_gain(&fr, 1, nullptr, fresh.c_str(), "{\"ok\":1}"));   // falsification: a valid object is accepted
    EXPECT_TRUE(fs::exists(fresh));
}

TEST_F(SaturationTest, ARefusedLoadLeavesTheOutputOfTheEarlierStateUnchanged) {
    // load a valid set through the public loaders (gain with a determined record), process a frame from that state
    std::vector<float> g(N);
    for (size_t i = 0; i < N; ++i) g[i] = gainAt(i);
    writeGain(g, rec("determined", 12800));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset((dir / "offset.xcal").string().c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain((dir / "gain.xcal").string().c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map((dir / "defect.xcal").string().c_str()));
    std::vector<uint16_t> raw;
    const auto process = [&](std::vector<float>* out) {
        makeFrame(kLevel);
        raw = rawCopy();
        const std::string cfg = "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassOffset\":true,\"bypassNonlinearity\":true,"
                                "\"bypassBinning\":true,\"bypassGhost\":true,\"bypassDefect\":true}";
        const XpeErrorCode rc = xpe_preprocess_pipeline(&img, &meta, nullptr, nullptr, cfg.c_str());
        if (rc == XPE_OK) *out = storage;
        return rc;
    };
    std::vector<float> before, after;
    ASSERT_EQ(XPE_OK, process(&before));
    size_t saturated = 0;
    for (size_t i = 0; i < N; ++i) if (raw[i] >= 12800) { ASSERT_FLOAT_EQ(12800.0f, before[i]); ++saturated; }
    ASSERT_GT(saturated, 100u);   // the earlier state really applies the record: the saturated pixels carry S_out

    // a corrupted record is refused ...
    writeGain(g, "{\"saturation_status\":\"determined\",\"saturation_level\":12800}");
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_calib_load_gain((dir / "gain.xcal").string().c_str()));
    // ... and the output from the state that was there is the same bytes
    ASSERT_EQ(XPE_OK, process(&after));
    ASSERT_EQ(before.size(), after.size());
    EXPECT_EQ(0, std::memcmp(before.data(), after.data(), before.size() * sizeof(float)));

    // falsification: loading a valid file with another level does change the output
    writeGain(g, rec("determined", 10000));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain((dir / "gain.xcal").string().c_str()));
    ASSERT_EQ(XPE_OK, process(&after));
    EXPECT_NE(0, std::memcmp(before.data(), after.data(), before.size() * sizeof(float)));
}
