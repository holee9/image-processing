/**
 * @file test_calib_quality_meta.cpp
 * @brief FUNC-033 calibration quality metadata (QA-A-35, #140 #120)
 *
 * SRS-CALIB-001 SRS-CALIB-FUNC-033, quoted where it is asserted:
 *   (1) every generated XCal gain file carries calibration_mode,
 *       actual_dose_levels, polynomial_degree, fit_r_squared,
 *       max_residual_pct, mean_residual_pct, ...
 *   (2) "If fit_r_squared < 0.999 after fitting, system shall log
 *       XPE_WARN_CALIB_POOR_FIT and include recommendation to increase dose
 *       levels or check detector stability."
 *   (5) "All metadata shall be stored in XCal file header section
 *       (JSON-encoded in reserved header bytes)."
 *
 * QA-A-34 deleted four cases that carried these names and tested nothing —
 * they compared local constants to each other and never entered production
 * code. The names are reused here deliberately; every case below drives the
 * shipped generator or loader and reads the result back through
 * `xpe_calib_get_quality_meta`.
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr uint32_t W = 4, H = 4;
constexpr size_t   N = static_cast<size_t>(W) * H;
constexpr double   kGate = 0.999;      // SRS-CALIB-FUNC-033 (2), quoted

class CalibQualityMetaTest : public ::testing::Test {
protected:
    fs::path tmpDir;

    void SetUp() override {
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        tmpDir = fs::temp_directory_path() / "xpe_func033";
        fs::remove_all(tmpDir);
        fs::create_directories(tmpDir);
        xpe_clear_alerts();
    }

    void TearDown() override {
        xpe_clear_alerts();
        xpe_preprocess_shutdown();
        fs::remove_all(tmpDir);
    }

    // Writes one GAIN XCal file whose pixels all hold `value`.
    std::string writeGainLevel(const std::string& name, float value) {
        const std::string path = (tmpDir / name).string();
        const std::vector<float> data(N, value);
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(XCAL_TYPE_GAIN);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width = W; hdr.height = H;
        hdr.payload_len = data.size() * sizeof(float);
        EXPECT_EQ(XPE_OK,
                  write_xcal_file(path.c_str(), hdr, nullptr, 0,
                                  reinterpret_cast<const uint8_t*>(data.data()),
                                  hdr.payload_len));
        return path;
    }

    // Writes a GAIN file carrying an explicit config JSON.
    std::string writeGainWithJson(const std::string& name, const std::string& json) {
        const std::string path = (tmpDir / name).string();
        const std::vector<float> data(N, 1.0f);
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(XCAL_TYPE_GAIN);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width = W; hdr.height = H;
        hdr.payload_len = data.size() * sizeof(float);
        hdr.config_json_len = json.size();
        EXPECT_EQ(XPE_OK,
                  write_xcal_file(path.c_str(), hdr,
                                  reinterpret_cast<const uint8_t*>(json.data()),
                                  json.size(),
                                  reinterpret_cast<const uint8_t*>(data.data()),
                                  hdr.payload_len));
        return path;
    }

    // Reads the config JSON block back out of an XCal file.
    std::string readConfigJson(const std::string& path) {
        std::ifstream f(path, std::ios::binary);
        EXPECT_TRUE(f.is_open());
        XCalFileHeader hdr{};
        f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
        std::string json(static_cast<size_t>(hdr.config_json_len), '\0');
        if (hdr.config_json_len > 0) f.read(&json[0], hdr.config_json_len);
        return json;
    }

    bool alertQueueMentions(const char* needle) {
        const int32_t n = xpe_get_pending_alert_count();
        for (int32_t i = 0; i < n; ++i) {
            char buf[512] = {0};
            int32_t sev = 0;
            if (xpe_get_pending_alert(i, buf, sizeof(buf), &sev) == XPE_OK &&
                std::string(buf).find(needle) != std::string::npos) {
                return true;
            }
        }
        return false;
    }

    // Generates a polynomial gain file from `values`, one per dose level.
    //
    // The level files are named per invocation. Reusing one name across two
    // calls in the same case made the second write fail with -9 (IO_FAILED):
    // write_xcal_file renames a .tmp into place, and the leftover from the
    // first call blocked it. Measured, not anticipated.
    int generation = 0;
    XpeErrorCode generatePoly(const std::vector<float>& values,
                              const std::string& outName) {
        const std::string tag = "g" + std::to_string(generation++) + "_";
        std::vector<std::string> paths;
        std::vector<const char*> pathPtrs;
        std::vector<double>      doses;
        for (size_t i = 0; i < values.size(); ++i) {
            paths.push_back(writeGainLevel(tag + "level" + std::to_string(i) + ".xcal",
                                           values[i]));
            doses.push_back(static_cast<double>(i + 1));
        }
        for (const auto& s : paths) pathPtrs.push_back(s.c_str());

        return xpe_calib_generate_gain_polynomial(
            pathPtrs.data(), doses.data(),
            static_cast<int32_t>(values.size()), 2,
            (tmpDir / outName).string().c_str());
    }
};

// --- FUNC-033 (1): the fields exist and describe the fit -------------------

// A perfectly linear gain-vs-dose series is fitted exactly, so R2 is 1 and the
// gate passes. Hand-computed: values 1,2,3,4 at doses 1,2,3,4 lie on a line, so
// every residual is 0, SS_res is 0, and R2 = 1 - 0/SS_tot = 1.
TEST_F(CalibQualityMetaTest, R2QualityGate_Pass_WhenAboveThreshold) {
    ASSERT_EQ(XPE_OK, generatePoly({1.0f, 2.0f, 3.0f, 4.0f}, "poly_ok.xcal"));

    XpeCalibQualityMeta meta{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&meta));

    EXPECT_NEAR(1.0, meta.r_squared, 1e-9) << "an exact fit scores 1.0";
    EXPECT_GE(meta.r_squared, kGate);
    EXPECT_EQ(1u, meta.calibration_pass);
    EXPECT_EQ(4u, meta.num_points) << "actual_dose_levels";
    EXPECT_GT(meta.calibration_timestamp, 0u) << "the record is stamped";
}

// --- FUNC-033 (2): the gate and its warning --------------------------------

// A series that no low-degree polynomial can follow drives R2 below the gate.
TEST_F(CalibQualityMetaTest, R2QualityGate_Fail_WhenBelowThreshold) {
    // Deliberately erratic across dose: a quadratic cannot track this.
    ASSERT_EQ(XPE_OK,
              generatePoly({1.0f, 9.0f, 2.0f, 8.0f, 3.0f}, "poly_bad.xcal"));

    XpeCalibQualityMeta meta{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&meta));

    EXPECT_LT(meta.r_squared, kGate) << "an erratic series cannot fit well";
    EXPECT_EQ(0u, meta.calibration_pass) << "the gate must record the failure";

    EXPECT_TRUE(alertQueueMentions("XPE_WARN_CALIB_POOR_FIT"))
        << "SRS-CALIB-FUNC-033 (2) requires the warning to be logged";
    EXPECT_TRUE(alertQueueMentions("increase dose levels"))
        << "and the recommendation to travel with it";
}

// The gate verdict follows the measurement, not the other way round: a passing
// run leaves no warning behind.
TEST_F(CalibQualityMetaTest, GoodFitRaisesNoWarning) {
    ASSERT_EQ(XPE_OK, generatePoly({2.0f, 4.0f, 6.0f, 8.0f}, "poly_clean.xcal"));

    EXPECT_FALSE(alertQueueMentions("XPE_WARN_CALIB_POOR_FIT"));
}

// --- FUNC-033 (3): comparison with the previous calibration ----------------

TEST_F(CalibQualityMetaTest, PreviousCalibration_Comparison_Regression) {
    ASSERT_EQ(XPE_OK, generatePoly({1.0f, 2.0f, 3.0f, 4.0f}, "first.xcal"));
    XpeCalibQualityMeta firstMeta{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&firstMeta));
    const double firstR2 = firstMeta.r_squared;

    ASSERT_EQ(XPE_OK, generatePoly({1.0f, 9.0f, 2.0f, 8.0f, 3.0f}, "second.xcal"));
    XpeCalibQualityMeta secondMeta{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&secondMeta));

    EXPECT_DOUBLE_EQ(firstR2, secondMeta.previous_r_squared)
        << "the previous run's R2 must be carried forward for comparison";
    EXPECT_LT(secondMeta.r_squared, secondMeta.previous_r_squared)
        << "this pair is a regression, which is what makes the field useful";
}

TEST_F(CalibQualityMetaTest, PreviousCalibration_Comparison_Stable) {
    ASSERT_EQ(XPE_OK, generatePoly({1.0f, 2.0f, 3.0f, 4.0f}, "stable1.xcal"));
    ASSERT_EQ(XPE_OK, generatePoly({2.0f, 4.0f, 6.0f, 8.0f}, "stable2.xcal"));

    XpeCalibQualityMeta meta{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&meta));

    EXPECT_NEAR(meta.previous_r_squared, meta.r_squared, 0.01)
        << "two clean fits in a row are stable";
    EXPECT_EQ(1u, meta.calibration_pass);
}

// --- FUNC-033 (5): the metadata is in the XCal config JSON -----------------

TEST_F(CalibQualityMetaTest, GeneratedFileCarriesTheMetadataFields) {
    ASSERT_EQ(XPE_OK, generatePoly({1.0f, 2.0f, 3.0f, 4.0f}, "fields.xcal"));

    const std::string json = readConfigJson((tmpDir / "fields.xcal").string());
    for (const char* key : {"fit_r_squared", "actual_dose_levels",
                            "polynomial_degree", "calibration_mode",
                            "max_residual_pct", "mean_residual_pct",
                            "calibration_pass"}) {
        EXPECT_NE(std::string::npos, json.find(key))
            << "SRS-CALIB-FUNC-033 (1) field missing from the file: " << key;
    }
}

// Round trip: a file's metadata is restored on load, not recomputed from
// whatever happened to be in the store.
TEST_F(CalibQualityMetaTest, MetadataSurvivesSaveAndLoad) {
    const std::string path = writeGainWithJson(
        "roundtrip.xcal",
        "{\"calibration_mode\":3,\"actual_dose_levels\":5,"
        "\"polynomial_degree\":2,\"fit_r_squared\":0.99950000}");

    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(path.c_str()));

    XpeCalibQualityMeta meta{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&meta));
    EXPECT_NEAR(0.9995, meta.r_squared, 1e-9);
    EXPECT_EQ(5u, meta.num_points);
    EXPECT_EQ(2u, meta.polynomial_degree);
    EXPECT_EQ(1u, meta.calibration_pass) << "0.9995 >= 0.999";
}

// The gate is re-derived on load. A file claiming it passed with a failing R2
// does not get to say so.
TEST_F(CalibQualityMetaTest, LoadedGateVerdictIsRecomputedNotTrusted) {
    const std::string path = writeGainWithJson(
        "liar.xcal",
        "{\"fit_r_squared\":0.50000000,\"calibration_pass\":1,"
        "\"actual_dose_levels\":3}");

    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(path.c_str()));

    XpeCalibQualityMeta meta{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&meta));
    EXPECT_NEAR(0.5, meta.r_squared, 1e-9);
    EXPECT_EQ(0u, meta.calibration_pass)
        << "the file said pass=1; 0.5 is below the gate, so the verdict is 0";
}

// Backward compatibility: a file written before QA-A-35 has none of these keys
// and must still load, leaving the metadata at its no-data values.
TEST_F(CalibQualityMetaTest, LegacyFileWithoutMetadataStillLoads) {
    const std::string path = writeGainLevel("legacy.xcal", 1.0f);  // no config JSON

    EXPECT_EQ(XPE_OK, xpe_calib_load_gain(path.c_str()))
        << "an older file must not be rejected for lacking FUNC-033 fields";
}

// A file carrying an unrelated config JSON is also fine — the parser reports
// "no fields found" rather than misreading it.
TEST_F(CalibQualityMetaTest, UnrelatedConfigJsonIsIgnored) {
    const std::string path = writeGainWithJson(
        "unrelated.xcal", "{\"kVp\":80,\"mAs\":2.5}");

    EXPECT_EQ(XPE_OK, xpe_calib_load_gain(path.c_str()));
}

} // namespace
