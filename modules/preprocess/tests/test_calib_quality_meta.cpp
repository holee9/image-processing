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
#include <cstdlib>
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

    // Reads one numeric value from a flat config JSON. NaN when the key is
    // absent, so a missing field fails an EXPECT_NEAR instead of reading 0.
    static double jsonNumber(const std::string& json, const std::string& key) {
        const std::string needle = "\"" + key + "\":";
        const size_t at = json.find(needle);
        if (at == std::string::npos) return std::nan("");
        return std::strtod(json.c_str() + at + needle.size(), nullptr);
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

    // Same, with the fitted degree under the caller's control. QA-A-70 needs a
    // LINEAR fit so that a deliberate deviation from a straight line cannot be
    // absorbed by curvature the fit is allowed to have.
    XpeErrorCode generatePolyDegree(const std::vector<float>& values,
                                    int32_t maxDegree,
                                    const std::string& outName) {
        const std::string tag = "d" + std::to_string(generation++) + "_";
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
            static_cast<int32_t>(values.size()), maxDegree,
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

// QA-A-70 (#140): R2 MOVES WITH FIT QUALITY, monotonically.
//
// The two gate tests below assert one side each, with different inputs. Neither
// on its own says the number RESPONDS to the data -- a function returning 1.0
// for good input and 0.5 for bad input would satisfy both while being a lookup
// table. This test holds everything fixed (five dose levels, the same doses, a
// linear fit) and varies exactly one thing: how far the series departs from a
// straight line. R2 must fall, step by step.
//
// The fit is LINEAR on purpose. With the quadratic the other tests use, a
// deviation shaped like a curve is absorbed by the fit rather than showing up as
// residual, and the series would have to be made erratic instead of merely
// less linear -- which is a coarser instrument.
//
// This is the QA-B-58 shape: change one input, hold the rest, require the output
// to move. QA-A-70 confirmed it is not redundant by injecting a constant R2 --
// see the report; the injection leaves the "pass" test green.
TEST_F(CalibQualityMetaTest, R2FallsAsTheSeriesDepartsFromTheFit) {
    const float base[5] = {1.0f, 2.0f, 3.0f, 4.0f, 5.0f};
    const float shape[5] = {0.0f, 1.0f, 0.0f, -1.0f, 0.0f};   // a linear fit cannot follow this

    double previous = 2.0;   // above any attainable R2
    for (int step = 0; step < 4; ++step) {
        const float amplitude = static_cast<float>(step) * 0.25f;
        std::vector<float> values;
        for (int i = 0; i < 5; ++i) values.push_back(base[i] + amplitude * shape[i]);

        ASSERT_EQ(XPE_OK, generatePolyDegree(values, 1,
                                             "move" + std::to_string(step) + ".xcal"));

        XpeCalibQualityMeta meta{};
        ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&meta));

        EXPECT_LT(meta.r_squared, previous)
            << "step " << step << " (amplitude " << amplitude << "): R2 " << meta.r_squared
            << " did not fall below the previous " << previous
            << " -- the score is not following the data";
        previous = meta.r_squared;
    }

    EXPECT_LT(previous, kGate)
        << "the last series should be poor enough to fail the gate, or this test"
           " never leaves the passing region and proves less than it looks";
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

// --- previous_r_squared: the store carries the prior calibration's R2 -----
//
// These two cases were named PreviousCalibration_Comparison_* until QA-A-87.
// They exercise previous_r_squared, which is NOT one of the three comparison
// metrics FUNC-033 (3) names (dark_bias_delta, prnu_delta_pct,
// defect_count_delta); none of those is implemented. Renamed so the name no
// longer reads as coverage of (3).

TEST_F(CalibQualityMetaTest, PreviousRSquared_Regression) {
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

TEST_F(CalibQualityMetaTest, PreviousRSquared_Stable) {
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

// --- QA-A-87: round trip through a GENERATED file -------------------------
//
// The hand-written-JSON round trip below never exercised the generator, so it
// could not see the generator writing one polynomial_degree to the file and
// another to the store. These cases load the file the generator produced and
// compare against what the generator recorded a moment earlier.
//
// Input where the fit must drop a degree, derived by hand. Doses 1..4, gains
// 1,4,4,4, max_degree 2. With t = dose - 2.5 the least-squares quadratic is
//   y = 3.25 + 0.9 t - 0.75 (t^2 - 1.25)
// whose slope 0.9 - 1.5 t is zero at t = 0.6, i.e. dose 3.1 -- inside [1, 4],
// so the curve falls after it and validate_monotonicity rejects degree 2.
// The linear fit y = 3.25 + 0.9 t rises, so every pixel settles on degree 1.
TEST_F(CalibQualityMetaTest, GeneratedPolyFileKeepsTheFittedDegree_WhenTheFitDropsADegree) {
    const std::string path = (tmpDir / "dropped.xcal").string();
    ASSERT_EQ(XPE_OK, generatePolyDegree({1.0f, 4.0f, 4.0f, 4.0f}, 2, "dropped.xcal"));

    XpeCalibQualityMeta generated{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&generated));
    ASSERT_EQ(1u, generated.polynomial_degree)
        << "precondition: this input must make the fit fall from degree 2 to 1";

    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(path.c_str()));
    XpeCalibQualityMeta loaded{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&loaded));

    EXPECT_EQ(generated.polynomial_degree, loaded.polynomial_degree)
        << "SRS-CALIB-FUNC-033 (1) polynomial_degree is the FITTED degree; the "
           "file must say what the store said";

    // The requested ceiling is still in the file, under a key of its own.
    const std::string json = readConfigJson(path);
    EXPECT_DOUBLE_EQ(1.0, jsonNumber(json, "polynomial_degree")) << json;
    EXPECT_DOUBLE_EQ(2.0, jsonNumber(json, "max_polynomial_degree")) << json;
}

// Control for the case above: when no pixel drops a degree, the fitted and the
// requested degree coincide and the round trip holds either way. If this one
// went red the harness itself would be wrong, not the key.
TEST_F(CalibQualityMetaTest, GeneratedPolyFileKeepsTheFittedDegree_WhenNoDegreeIsDropped) {
    const std::string path = (tmpDir / "kept.xcal").string();
    ASSERT_EQ(XPE_OK, generatePolyDegree({1.0f, 2.0f, 3.0f, 4.0f}, 2, "kept.xcal"));

    XpeCalibQualityMeta generated{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&generated));
    ASSERT_EQ(2u, generated.polynomial_degree)
        << "precondition: a straight line is monotonic at the requested degree";

    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(path.c_str()));
    XpeCalibQualityMeta loaded{};
    ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&loaded));

    EXPECT_EQ(generated.polynomial_degree, loaded.polynomial_degree);
}

// --- QA-A-87: residual fields carry the computed values -------------------
//
// Until now these two keys were only checked for presence, so any arithmetic
// in them passed. Hand-computed for gains 1,4,4,4 at doses 1..4 and a LINEAR
// fit (max_degree 1), with t = dose - 2.5:
//   fit        y = 3.25 + 0.9 t      -> 1.9, 2.8, 3.7, 4.6
//   residuals  -0.9, 1.2, 0.3, -0.6  (sum of magnitudes 3.0)
//   mean gain  3.25
//   max_residual_pct  = 1.2 / 3.25 * 100          = 36.923077
//   mean_residual_pct = (3.0 / 4) / 3.25 * 100    = 23.076923
// Every pixel holds the same series, so the per-pixel values are also the
// whole-frame values. The file prints six decimals.
TEST_F(CalibQualityMetaTest, ResidualFieldsCarryTheHandComputedValues) {
    ASSERT_EQ(XPE_OK, generatePolyDegree({1.0f, 4.0f, 4.0f, 4.0f}, 1, "resid.xcal"));

    const std::string json = readConfigJson((tmpDir / "resid.xcal").string());
    EXPECT_NEAR(36.923077, jsonNumber(json, "max_residual_pct"), 1e-5) << json;
    EXPECT_NEAR(23.076923, jsonNumber(json, "mean_residual_pct"), 1e-5) << json;
}

// Control: a series on a straight line leaves nothing to report.
TEST_F(CalibQualityMetaTest, ResidualFieldsAreZeroForAnExactFit) {
    ASSERT_EQ(XPE_OK, generatePolyDegree({1.0f, 2.0f, 3.0f, 4.0f}, 1, "exact.xcal"));

    const std::string json = readConfigJson((tmpDir / "exact.xcal").string());
    EXPECT_NEAR(0.0, jsonNumber(json, "max_residual_pct"), 1e-5) << json;
    EXPECT_NEAR(0.0, jsonNumber(json, "mean_residual_pct"), 1e-5) << json;
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
