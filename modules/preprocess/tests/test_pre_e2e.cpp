/** @file test_pre_e2e.cpp
 * FUNC-015/019/021: measured evidence, oracle populations, weights and safety caps (Refs #245).
 * Changing a weight, promoting missing evidence, or dropping either cap must fail.
 */
#include <gtest/gtest.h>
#include "xpe_pre_e2e_report.hpp"
#include "xpe_pre_e2e.hpp"
#include "xpe_pre_stripe.hpp"
#include "xpe_pre_defect_metrics.hpp"
#include "xpe_pre_e2e_path.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"
#include "defect_ext.hpp"
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <limits>
#include <vector>

namespace {
using namespace xpe::pre::e2e;

ScoreEvidence measuredScores() {
    ScoreEvidence e;
    e.components.fill(1.0);
    e.rawProof = e.inputPreserved = e.gainKnown = e.finiteOutput = true;
    return e;
}

TEST(PreDefectOracle, IndependentTruthCountsAndExistingNeighbourResidual) {
    std::vector<float> before(9, 10.0f), after(9, 10.0f);
    std::vector<uint8_t> truth(9, 0), prediction(9, 0);
    truth[4] = 255; prediction[4] = prediction[0] = 1;
    before[4] = 1000.0f; after[4] = 12.0f;
    const auto m = measureDefectOracle(before, after, truth, prediction, 3, 3);
    ASSERT_TRUE(m.valid); ASSERT_TRUE(m.predictionMeasured);
    EXPECT_EQ(1u, m.tp); EXPECT_EQ(1u, m.fp); EXPECT_EQ(0u, m.fn); EXPECT_EQ(7u, m.tn);
    ASSERT_TRUE(m.recall); EXPECT_DOUBLE_EQ(1.0, *m.recall);
    ASSERT_TRUE(m.fpr); EXPECT_DOUBLE_EQ(0.125, *m.fpr);
    ASSERT_TRUE(m.residual); EXPECT_DOUBLE_EQ(2.0, *m.residual);
    EXPECT_EQ(1u, m.residualPixels);
    ASSERT_TRUE(m.p99Linear); EXPECT_DOUBLE_EQ(0.0, *m.p99Linear);
    EXPECT_STREQ("fail", defectSelectionVerdict(m, true));
    EXPECT_STREQ("reported", defectSelectionVerdict(m, false));
    EXPECT_STREQ("pass", goodPixelVerdict(m));
    EXPECT_EQ(1000.0f, before[4]); EXPECT_EQ(255u, truth[4]);
}

TEST(PreDefectOracle, KeepsBothExistingQuantilesAndDoesNotChooseAWinningConvention) {
    std::vector<float> before(100, 0.0f), after(100, 0.0f);
    std::vector<uint8_t> truth(100, 0), prediction(100, 0);
    after[98] = 0.5f; after[99] = 100.0f;
    const auto m = measureDefectOracle(before, after, truth, prediction, 10, 10);
    ASSERT_TRUE(m.p99Linear); ASSERT_TRUE(m.p99Nearest);
    EXPECT_NEAR(1.495, *m.p99Linear, 1e-10);
    EXPECT_DOUBLE_EQ(0.5, *m.p99Nearest);
    EXPECT_STREQ("unmeasured", goodPixelVerdict(m));
    EXPECT_FALSE(m.recall); // no truth-positive denominator
}

TEST(PreDefectOracle, EmptyPopulationsAndUnsupportedPredictionAreNotPerfectScores) {
    std::vector<float> pixels(9, 10.0f);
    std::vector<uint8_t> truth(9, 1), prediction(9, 1);
    auto m = measureDefectOracle(pixels, pixels, truth, prediction, 3, 3);
    ASSERT_TRUE(m.valid); EXPECT_FALSE(m.residual); EXPECT_FALSE(m.fpr); EXPECT_FALSE(m.p99Linear);
    EXPECT_STREQ("unmeasured", defectSelectionVerdict(m, true));
    truth[0] = 0;
    m = measureDefectOracle(pixels, pixels, truth, {}, 3, 3);
    EXPECT_FALSE(m.predictionMeasured); EXPECT_FALSE(m.recall); EXPECT_FALSE(m.fpr);
    ASSERT_TRUE(m.residual); EXPECT_DOUBLE_EQ(0.0, *m.residual);
}

TEST(PreDefectOracle, NonfiniteStagePixelsAndWrongShapesCannotSilentlyPass) {
    std::vector<float> before(9, 10.0f), after(before);
    std::vector<uint8_t> truth(9, 0), prediction(9, 0);
    truth[4] = prediction[4] = 1;
    after[0] = std::numeric_limits<float>::quiet_NaN();
    const auto m = measureDefectOracle(before, after, truth, prediction, 3, 3);
    EXPECT_EQ(1u, m.nonfinitePixels); EXPECT_FALSE(m.residual); EXPECT_FALSE(m.p99Linear);
    EXPECT_STREQ("unmeasured", goodPixelVerdict(m));
    EXPECT_FALSE(measureDefectOracle(before, after, {1}, prediction, 3, 3).valid);
    EXPECT_FALSE(measureDefectOracle(before, after, truth, prediction, 0, 3).valid);
}

TEST(PreDefectOracle, SyntheticFprIsStrictAndDoesNotGateReferenceDataAsSynthetic) {
    std::vector<float> pixels(100001, 10.0f);
    std::vector<uint8_t> truth(pixels.size(), 0), prediction(pixels.size(), 0);
    truth[0] = prediction[0] = prediction[1] = 1;
    auto m = measureDefectOracle(pixels, pixels, truth, prediction, 100001, 1);
    ASSERT_TRUE(m.fpr); EXPECT_DOUBLE_EQ(0.00001, *m.fpr);
    EXPECT_STREQ("fail", defectSelectionVerdict(m, true));
    EXPECT_STREQ("reported", defectSelectionVerdict(m, false));
    pixels.push_back(10.0f); truth.push_back(0); prediction.push_back(0);
    m = measureDefectOracle(pixels, pixels, truth, prediction, 100002, 1);
    EXPECT_STREQ("pass", defectSelectionVerdict(m, true));
}

TEST(PreStripe, MatchesAnIndependentTwoPassFourByFourOracleAndTranspose) {
    const std::vector<float> pixels{100,102,101,105, 104,103,108,102, 110,107,109,112, 111,115,113,117};
    const std::vector<float> gains(16, 1.0f);
    const auto s = measureStripe(pixels, gains, 4, 4);
    ASSERT_TRUE(s.ratio);
    EXPECT_EQ(16u, s.roiPixels);
    EXPECT_EQ(12u, s.horizontalPairs);
    EXPECT_EQ(12u, s.verticalPairs);
    // Explicit row means 102/104.25/109.5/114; column means 106.25/106.75/107.75/109.
    EXPECT_NEAR(std::sqrt(21.76171875), s.rowStd, 1e-12);
    EXPECT_NEAR(std::sqrt(1.10546875), s.columnStd, 1e-12);
    EXPECT_NEAR(std::sqrt(10.909722222222221 / 2.0), s.sigmaHorizontal, 1e-12);
    EXPECT_NEAR(std::sqrt(11.833333333333334 / 2.0), s.sigmaVertical, 1e-12);
    EXPECT_NEAR(std::sqrt(10.909722222222221 / 2.0) * std::sqrt(0.75) / 2.0, s.rowExpected, 1e-12);
    std::vector<float> transpose(16);
    for (size_t y = 0; y < 4; ++y)
        for (size_t x = 0; x < 4; ++x) transpose[x * 4 + y] = pixels[y * 4 + x];
    const auto t = measureStripe(transpose, gains, 4, 4);
    ASSERT_TRUE(t.ratio);
    EXPECT_NEAR(*s.ratio, *t.ratio, 1e-12);
    EXPECT_NEAR(*s.rowRatio, *t.columnRatio, 1e-12);
    EXPECT_NEAR(*s.columnRatio, *t.rowRatio, 1e-12);
}

TEST(PreStripe, UsesPositiveGainRoiAndOnlyOriginalNeighbourPairs) {
    std::vector<float> pixels{100,102,101,105, 104,103,108,102, 110,107,109,112, 111,115,113,117};
    std::vector<float> gains(16, 1.0f);
    for (size_t i = 4; i < 8; ++i) { gains[i] = 0.0f; pixels[i] = std::numeric_limits<float>::quiet_NaN(); }
    gains[0] = 20.0f; // finite-positive ROI includes this even though application would reject it
    const auto s = measureStripe(pixels, gains, 4, 4);
    ASSERT_TRUE(s.ratio);
    EXPECT_EQ(12u, s.roiPixels);
    EXPECT_EQ(3u, s.rows);
    EXPECT_EQ(4u, s.columns);
    EXPECT_EQ(9u, s.horizontalPairs);
    EXPECT_EQ(4u, s.verticalPairs); // no adjacency across the excluded row
}

TEST(PreStripe, DegenerateOrNonfiniteEvidenceIsUnmeasured) {
    std::vector<float> pixels(16, 4000.0f), gains(16, 1.0f);
    EXPECT_FALSE(measureStripe(pixels, gains, 4, 4).ratio); // zero expected noise: undefined 0/0
    pixels[0] = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(measureStripe(pixels, gains, 4, 4).ratio);
    EXPECT_FALSE(measureStripe(pixels, gains, 0, 4).ratio);
    EXPECT_FALSE(measureStripe(pixels, gains, 8, 4).ratio);
}

TEST(PreStripe, UsesTheTruncatedThirtyThreeSampleWindowOnLongProfiles) {
    std::vector<float> image(160), gain(160, 1.0f);
    const float columnPattern[] = {0, 1, 4, 7};
    for (size_t y = 0; y < 40; ++y)
        for (size_t x = 0; x < 4; ++x)
            image[y * 4 + x] = 100.0f + columnPattern[x] + (y == 20 ? 10.0f : 0.0f);
    const auto s = measureStripe(image, gain, 4, 40);
    ASSERT_TRUE(s.ratio);
    // Direct two-pass oracle: impulse 10 at index20, 40-long profile, clipped +/-16 windows.
    EXPECT_NEAR(1.568521563742523, s.rowStd, 1e-12);
}

TEST(PreE2EReport, UsesTheEightProtocolWeightsRatherThanAnUnweightedAverage) {
    auto e = measuredScores();
    e.components = {1.0, 0.5, 0.25, 0.75, 0.0, 1.0, 1.0, 0.5};
    const auto s = calculateScore(e);
    ASSERT_TRUE(s.value.has_value());
    EXPECT_NEAR(61.5, *s.value, 1e-12);
    EXPECT_TRUE(s.complete);
    EXPECT_FALSE(s.phase1Pass);
}

TEST(PreE2EReport, ClampReviewUsesTheExistingStrictFivePercentBoundary) {
    EXPECT_STREQ("reported", clampReviewVerdict(0.04999));
    EXPECT_STREQ("reported", clampReviewVerdict(0.05));
    EXPECT_STREQ("review_required", clampReviewVerdict(0.05001));
    EXPECT_STREQ("unmeasured", clampReviewVerdict(std::nullopt));
    EXPECT_STREQ("unmeasured", clampReviewVerdict(std::numeric_limits<double>::quiet_NaN()));
}

TEST(PreE2EReport, CompleteMeasuredEvidenceCanReachTheSpecifiedPromotionGates) {
    const auto s = calculateScore(measuredScores());
    ASSERT_TRUE(s.value);
    EXPECT_DOUBLE_EQ(100.0, *s.value);
    EXPECT_TRUE(s.phase1Pass);
    EXPECT_TRUE(s.releasePass);
}

TEST(PreE2EReport, MissingComponentsAreNullAndCannotBePromoted) {
    auto e = measuredScores();
    e.components[4].reset(); // no real lag evidence must not become a perfect zero residual
    const auto s = calculateScore(e);
    EXPECT_FALSE(s.value);
    EXPECT_FALSE(s.complete);
    EXPECT_FALSE(s.phase1Pass);
    const auto j = scoreJson(s);
    ASSERT_TRUE(j.contains("calibration_effect_score"));
    EXPECT_TRUE(j.at("calibration_effect_score").is_null());
}

TEST(PreE2EReport, UnknownSemanticsAndMissingRawProofAreCappedAtSeventy) {
    for (int which = 0; which != 3; ++which) {
        auto e = measuredScores();
        if (which == 0) e.gainKnown = false;
        if (which == 1) e.rawProof = false;
        if (which == 2) e.finiteOutput = false;
        const auto s = calculateScore(e);
        ASSERT_TRUE(s.value);
        EXPECT_DOUBLE_EQ(70.0, *s.value);
        EXPECT_FALSE(s.phase1Pass);
    }
}

TEST(PreE2EReport, SafetyFailureTakesTheStricterCapAndCannotBeHiddenByGoodScores) {
    auto e = measuredScores();
    e.blockingSafetyFailure = true;
    e.gainKnown = false;
    const auto s = calculateScore(e);
    ASSERT_TRUE(s.value);
    EXPECT_DOUBLE_EQ(60.0, *s.value);
    EXPECT_FALSE(s.phase1Pass);
    EXPECT_FALSE(s.releasePass);
}

TEST(PreE2EReport, ProvenRawMutationIsABlockingSafetyFailure) {
    auto e = measuredScores();
    e.inputPreserved = false;
    const auto s = calculateScore(e);
    ASSERT_TRUE(s.value);
    EXPECT_DOUBLE_EQ(60.0, *s.value);
    EXPECT_FALSE(s.phase1Pass);
}

TEST(PreE2EReport, NonfiniteScoresAreUnmeasuredButFiniteScoresAreClamped) {
    for (const double v : {std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity()}) {
        auto e = measuredScores();
        e.components[0] = v;
        EXPECT_FALSE(calculateScore(e).value);
    }
    auto e = measuredScores();
    e.components[0] = 2.0;
    e.components[1] = -1.0;
    const auto s = calculateScore(e);
    ASSERT_TRUE(s.value);
    EXPECT_NEAR(82.0, *s.value, 1e-12);
}

TEST(PreE2EReport, MarkdownLabelsUnmeasuredMetricsAndIncompleteReadiness) {
    const nlohmann::json report = {
        {"schema", "xpe-pre-e2e-report-v1"},
        {"image_path", "image.raw"},
        {"metrics", {{"dark_bias_adu", nullptr}, {"flat_residual_pct", 0.0},
                     {"calibration_effect_score", nullptr}}},
        {"degraded_evidence", true},
        {"gates", {{{"id", "PRE-E2E-COMPLETE"}, {"result", "unmeasured"}}}}
    };
    const auto text = markdown(report);
    EXPECT_NE(std::string::npos, text.find("unmeasured"));
    EXPECT_NE(std::string::npos, text.find("dark_bias_adu"));
    EXPECT_NE(std::string::npos, text.find("flat_residual_pct"));
    EXPECT_NE(std::string::npos, text.find("degraded"));
}

TEST(PreE2EReport, PromotionBoundariesIncludeEightyFiveAndNinetyTwoExactly) {
    for (const double score : {84.999, 85.0, 85.001, 91.999, 92.0, 92.001}) {
        auto e = measuredScores();
        e.components.fill(score / 100.0);
        const auto s = calculateScore(e);
        ASSERT_TRUE(s.value);
        EXPECT_NEAR(score, *s.value, 1e-10);
        EXPECT_EQ(score >= 85.0, s.phase1Pass) << score;
        EXPECT_EQ(score >= 92.0, s.releasePass) << score;
    }
}

int runE2ECli(const std::string& args) {
    std::string cmd = std::string("\"") + XPE_PRE_E2E_EXE + "\" " + args;
#ifdef _WIN32
    cmd = "\"" + cmd + "\"";
#endif
    return std::system(cmd.c_str());
}

class PreE2ERun : public ::testing::Test {
protected:
    std::filesystem::path dir;
    Options options;

    void SetUp() override {
        dir = std::filesystem::temp_directory_path() /
            ("xpe_e2e_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        ASSERT_TRUE(std::filesystem::create_directory(dir));
        options.raw = dir / "input.raw";
        options.calibration = dir;
        options.width = options.height = 16;
        options.gainSemantics = "normalized_gain";
        options.gitSha = "test-source";
        const std::vector<uint16_t> raw(256, 1000);
        std::ofstream out(options.raw, std::ios::binary);
        out.write(reinterpret_cast<const char*>(raw.data()), static_cast<std::streamsize>(raw.size() * 2));
        out.close();
        for (const auto& row : {std::pair<const char*, float>{"offset.xcal", 100.0f}, {"gain.xcal", 2.0f}}) {
            const std::vector<float> pixels(256, row.second);
            XCalFileHeader hdr{};
            std::memcpy(hdr.magic, XCAL_MAGIC, 4);
            hdr.version = XCAL_VERSION;
            hdr.type = row.second == 100.0f ? XCAL_TYPE_OFFSET : XCAL_TYPE_GAIN;
            hdr.pixel_format = XCAL_FMT_FLOAT32;
            hdr.width = hdr.height = 16;
            hdr.payload_len = pixels.size() * sizeof(float);
            ASSERT_EQ(XPE_OK, write_xcal_file((dir / row.first).string().c_str(), hdr, nullptr, 0,
                reinterpret_cast<const uint8_t*>(pixels.data()), hdr.payload_len));
        }
        const std::vector<uint8_t> mask(256, 0);
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = XCAL_TYPE_DEFECT;
        hdr.pixel_format = XCAL_FMT_UINT8_MASK;
        hdr.width = hdr.height = 16;
        hdr.payload_len = mask.size();
        ASSERT_EQ(XPE_OK, write_xcal_file((dir / "defect.xcal").string().c_str(), hdr, nullptr, 0,
                                        mask.data(), hdr.payload_len));
    }

    void TearDown() override {
        std::error_code ec;
        std::filesystem::remove_all(dir, ec); // only the unique directory this fixture created
    }

    void setRaw(const std::vector<uint16_t>& raw) {
        std::ofstream out(options.raw, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(raw.data()), static_cast<std::streamsize>(raw.size() * sizeof(uint16_t)));
    }

    void setGain(const std::vector<float>& pixels) {
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = XCAL_TYPE_GAIN;
        hdr.pixel_format = XCAL_FMT_FLOAT32;
        hdr.width = hdr.height = 16;
        hdr.payload_len = pixels.size() * sizeof(float);
        ASSERT_EQ(XPE_OK, write_xcal_file((dir / "gain.xcal").string().c_str(), hdr, nullptr, 0,
            reinterpret_cast<const uint8_t*>(pixels.data()), hdr.payload_len));
    }

    std::string cliArgs(const std::filesystem::path& output) const {
        return "--raw \"" + options.raw.string() + "\" --calib \"" + dir.string() +
            "\" --width 16 --height 16 --out \"" + output.string() + "\"";
    }
};

TEST_F(PreE2ERun, ReferenceMeasuresFullPopulationWithoutPromotionAndPreservesInput) {
    options.referenceF32 = dir / "reference.f32";
    // Fixture default gain application is division: (1000 - 100) / 2 = 450 ADU.
    std::vector<float> reference(256, 450.0f);
    reference[0] = 447.0f; reference[1] = 446.0f;
    { std::ofstream f(options.referenceF32, std::ios::binary); f.write(reinterpret_cast<const char*>(reference.data()), 1024); }
    const auto j = evaluate(options);
    ASSERT_TRUE(j.at("run_succeeded").get<bool>()) << j.dump();
    ASSERT_TRUE(j.contains("reference"));
    EXPECT_EQ(256, j.at("reference").at("pixels"));
    EXPECT_EQ(j.at("reference").at("sha256_before"), j.at("reference").at("sha256_after"));
    EXPECT_TRUE(j.at("reference").at("preserved").get<bool>());
    EXPECT_DOUBLE_EQ(5.0 / 16.0, j.at("metrics").at("rmse_reference").get<double>());
    EXPECT_DOUBLE_EQ(7.0 / 256.0, j.at("metrics").at("mae_reference_adu").get<double>());
    EXPECT_DOUBLE_EQ(4.0, j.at("metrics").at("max_abs_reference_adu").get<double>());
    EXPECT_NEAR(20.0 * std::log10(65535.0 / (5.0 / 16.0)), j.at("metrics").at("psnr_reference_db").get<double>(), 1e-12);
    EXPECT_TRUE(j.at("score_components").at("reference").is_null());
    EXPECT_FALSE(j.at("release_pass").get<bool>());
}

TEST_F(PreE2ERun, ReferenceExactMatchDoesNotInventEpsilonAndCliConnectsInput) {
    options.referenceF32 = dir / "reference.f32";
    const std::vector<float> reference(256, 450.0f);
    { std::ofstream f(options.referenceF32, std::ios::binary); f.write(reinterpret_cast<const char*>(reference.data()), 1024); }
    const auto output = dir / "reference-cli";
    ASSERT_EQ(0, runE2ECli(cliArgs(output) + " --reference-f32 \"" + options.referenceF32.string() + "\""));
    std::ifstream f(output / "report.json"); const auto j = nlohmann::json::parse(f);
    ASSERT_TRUE(j.contains("reference"));
    EXPECT_TRUE(j.at("reference").at("exact_match").get<bool>());
    EXPECT_DOUBLE_EQ(0.0, j.at("metrics").at("rmse_reference").get<double>());
    EXPECT_TRUE(j.at("metrics").at("psnr_reference_db").is_null());
    EXPECT_EQ("epsilon_unspecified_for_exact_match", j.at("reference").at("psnr_status"));
}

TEST_F(PreE2ERun, ReferenceRejectsWrongLengthAndNonfiniteWithoutDroppingPixels) {
    options.referenceF32 = dir / "reference.f32";
    { std::ofstream f(options.referenceF32, std::ios::binary); f.put(0); }
    EXPECT_FALSE(evaluate(options).at("run_succeeded").get<bool>());
    std::vector<float> reference(256, 450.0f); reference[99] = std::numeric_limits<float>::infinity();
    { std::ofstream f(options.referenceF32, std::ios::binary | std::ios::trunc); f.write(reinterpret_cast<const char*>(reference.data()), 1024); }
    const auto j = evaluate(options);
    EXPECT_FALSE(j.at("run_succeeded").get<bool>());
    EXPECT_TRUE(j.at("metrics").at("rmse_reference").is_null());
    EXPECT_TRUE(j.at("metrics").at("psnr_reference_db").is_null());
}

TEST_F(PreE2ERun, OracleIsIndependentFromTheLoadedPredictionAndIsPreserved) {
    options.oracleBpm = dir / "truth.mask";
    options.oracleKind = "synthetic";
    std::vector<uint8_t> truth(256, 0); truth[100] = 255;
    { std::ofstream f(options.oracleBpm, std::ios::binary); f.write(reinterpret_cast<const char*>(truth.data()), 256); }
    const auto j = evaluate(options); // loaded map is empty: the oracle must not become its prediction
    ASSERT_TRUE(j.at("run_succeeded").get<bool>()) << j.dump();
    ASSERT_TRUE(j.contains("oracle"));
    EXPECT_EQ(j.at("oracle").at("sha256_before"), j.at("oracle").at("sha256_after"));
    EXPECT_DOUBLE_EQ(0.0, j.at("metrics").at("defect_recall").get<double>());
    EXPECT_EQ(1, j.at("oracle").at("fn"));
    EXPECT_TRUE(j.at("metrics").at("calibration_effect_score").is_null());
}

TEST_F(PreE2ERun, OracleMeasuresActualStageFramesAndHandlesVersionTwoAreaWithoutDroppingTruth) {
    options.oracleBpm = dir / "truth.mask"; options.oracleKind = "synthetic";
    std::vector<uint8_t> truth(256, 0), area(256, 1);
    truth[100] = truth[101] = 1; area[101] = 0;
    { std::ofstream f(options.oracleBpm, std::ios::binary); f.write(reinterpret_cast<const char*>(truth.data()), 256); }
    DefectXcalWrite job; job.width = job.height = 16; job.map = truth.data(); job.area = area.data();
    ASSERT_EQ(XPE_OK, write_defect_xcal((dir / "defect.xcal").string().c_str(), job));
    std::vector<uint16_t> raw(256, 1000); raw[100] = raw[101] = 40000; setRaw(raw);
    const auto j = evaluate(options);
    ASSERT_TRUE(j.at("run_succeeded").get<bool>()) << j.dump();
    ASSERT_TRUE(j.contains("oracle"));
    EXPECT_EQ(1, j.at("oracle").at("tp")); EXPECT_EQ(1, j.at("oracle").at("fn"));
    EXPECT_EQ(1, j.at("oracle").at("truth_bad_outside_area"));
    EXPECT_DOUBLE_EQ(0.5, j.at("metrics").at("defect_recall").get<double>());
    EXPECT_DOUBLE_EQ(0.0, j.at("metrics").at("good_pixel_delta_p99_linear_adu").get<double>());
    EXPECT_GT(j.at("metrics").at("defect_residual_adu").get<double>(), 0.0);
}

TEST_F(PreE2ERun, OraclePredictionIncludesOriginalGainClassificationRatherThanNeutralizedGain) {
    options.oracleBpm = dir / "truth.mask"; options.oracleKind = "synthetic";
    std::vector<uint8_t> truth(256, 0); truth[100] = 1;
    { std::ofstream f(options.oracleBpm, std::ios::binary); f.write(reinterpret_cast<const char*>(truth.data()), 256); }
    std::vector<float> gains(256, 2.0f); gains[100] = 0.0f; setGain(gains);
    const auto j = evaluate(options);
    ASSERT_TRUE(j.at("run_succeeded").get<bool>()) << j.dump();
    EXPECT_EQ(1, j.at("oracle").at("tp")); EXPECT_EQ(0, j.at("oracle").at("fn"));
    EXPECT_DOUBLE_EQ(1.0, j.at("metrics").at("defect_recall").get<double>());
    EXPECT_DOUBLE_EQ(0.0, j.at("metrics").at("defect_residual_adu").get<double>());
}

TEST_F(PreE2ERun, PolynomialOracleKeepsUnsupportedSelectionNullButMeasuresActualStageFrames) {
    options.oracleBpm = dir / "truth.mask";
    std::vector<uint8_t> truth(256, 0); truth[100] = 1;
    { std::ofstream f(options.oracleBpm, std::ios::binary); f.write(reinterpret_cast<const char*>(truth.data()), 256); }
    std::vector<float> coefficients(512, 0.0f);
    for (size_t i = 0; i < 256; ++i) coefficients[2 * i] = 2.0f;
    const std::string config = "{\"dose_min\":0,\"dose_max\":65535,\"polynomial_degree\":1}";
    XCalFileHeader hdr{}; std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version = XCAL_VERSION; hdr.type = XCAL_TYPE_GAIN_POLY; hdr.pixel_format = XCAL_FMT_FLOAT32;
    hdr.width = hdr.height = 16;
    ASSERT_EQ(XPE_OK, write_xcal_file((dir / "gain.xcal").string().c_str(), hdr,
        reinterpret_cast<const uint8_t*>(config.data()), config.size(),
        reinterpret_cast<const uint8_t*>(coefficients.data()), coefficients.size() * sizeof(float)));
    const auto j = evaluate(options);
    ASSERT_TRUE(j.at("run_succeeded").get<bool>()) << j.dump();
    EXPECT_FALSE(j.at("oracle").at("prediction_measured").get<bool>());
    EXPECT_TRUE(j.at("metrics").at("defect_recall").is_null());
    EXPECT_TRUE(j.at("metrics").at("defect_fpr").is_null());
    EXPECT_DOUBLE_EQ(0.0, j.at("metrics").at("defect_residual_adu").get<double>());
    EXPECT_DOUBLE_EQ(0.0, j.at("metrics").at("good_pixel_delta_p99_linear_adu").get<double>());
}

TEST_F(PreE2ERun, OracleValidatesLengthAndKindAndCliActuallyConnectsIt) {
    options.oracleBpm = dir / "truth.mask";
    { std::ofstream f(options.oracleBpm, std::ios::binary); f.put(0); }
    EXPECT_FALSE(evaluate(options).at("run_succeeded").get<bool>());
    const std::vector<uint8_t> truth(256, 0);
    { std::ofstream f(options.oracleBpm, std::ios::binary | std::ios::trunc); f.write(reinterpret_cast<const char*>(truth.data()), 256); }
    options.oracleKind = "guess";
    EXPECT_FALSE(evaluate(options).at("run_succeeded").get<bool>());
    const auto output = dir / "oracle-cli";
    EXPECT_EQ(0, runE2ECli(cliArgs(output) + " --oracle-bpm \"" + options.oracleBpm.string() + "\" --oracle-kind reference"));
    std::ifstream f(output / "report.json");
    const auto j = nlohmann::json::parse(f);
    ASSERT_TRUE(j.contains("oracle")); EXPECT_EQ(256, j.at("oracle").at("truth_good"));
}

TEST_F(PreE2ERun, MeasuresTheRealBasicStagesAndPreservesRawAndMapProvenance) {
    const auto j = evaluate(options);
    ASSERT_TRUE(j.contains("schema"));
    EXPECT_EQ("xpe-pre-e2e-report-v1", j.at("schema"));
    ASSERT_TRUE(j.at("run_succeeded").get<bool>()) << j.dump();
    EXPECT_TRUE(j.at("input_preserved").get<bool>());
    EXPECT_EQ(j.at("raw_sha256_before"), j.at("raw_sha256_after"));
    EXPECT_EQ(3u, j.at("calibration_files").size());
    EXPECT_DOUBLE_EQ(450.0, j.at("output_summary").at("min").get<double>());
    EXPECT_DOUBLE_EQ(450.0, j.at("output_summary").at("max").get<double>());
    EXPECT_TRUE(j.at("matches_shipped_pipeline").get<bool>());
    ASSERT_EQ(3u, j.at("stages").size());
    for (const auto& stage : j.at("stages")) {
        EXPECT_TRUE(stage.at("applied").get<bool>());
        EXPECT_GE(stage.at("time_ms").get<double>(), 0.0);
    }
    EXPECT_TRUE(j.at("metrics").at("dark_bias_adu").is_null()) << "an image is not a verified dark frame";
    EXPECT_TRUE(j.at("metrics").at("calibration_effect_score").is_null());
    EXPECT_FALSE(j.at("phase1_pass").get<bool>());
    std::vector<uint16_t> raw(256);
    std::ifstream in(options.raw, std::ios::binary);
    in.read(reinterpret_cast<char*>(raw.data()), 512);
    EXPECT_EQ(std::vector<uint16_t>(256, 1000), raw);
}

TEST_F(PreE2ERun, StripeFailureIsReportedEvenBelowTheResidualJudgingSignalLevel) {
    options.inputKind = "flat";
    std::vector<uint16_t> raw(256);
    for (size_t i = 0; i < raw.size(); ++i)
        raw[i] = static_cast<uint16_t>(1000 + (i / 16 % 2) * 200 + (i * 37 % 13));
    setRaw(raw);
    const auto j = evaluate(options);
    ASSERT_TRUE(j.at("run_succeeded").get<bool>());
    ASSERT_TRUE(j.at("metrics").at("stripe_noise_ratio").is_number());
    EXPECT_GT(j.at("metrics").at("stripe_noise_ratio").get<double>(), 3.0);
    EXPECT_FALSE(j.at("flat_statistics").at("minimum_signal_met").get<bool>());
    bool foundStripe = false, foundFlat = false;
    for (const auto& gate : j.at("gates")) {
        if (gate.at("id") == "PRE-E2E-STRIPE-QUALITY") { foundStripe = true; EXPECT_EQ("fail", gate.at("result")); }
        if (gate.at("id") == "PRE-E2E-FLAT-QUALITY") { foundFlat = true; EXPECT_EQ("fail", gate.at("result")); }
    }
    EXPECT_TRUE(foundStripe && foundFlat);
    EXPECT_TRUE(j.at("score_components").at("flat").is_null());
}

TEST_F(PreE2ERun, HighClampRateRequestsReviewWithoutInventingASignedBiasPassLimit) {
    options.inputKind = "dark";
    setRaw(std::vector<uint16_t>(256, 98));
    const auto j = evaluate(options);
    ASSERT_TRUE(j.at("run_succeeded").get<bool>());
    ASSERT_TRUE(j.contains("clamp_review_required"));
    EXPECT_TRUE(j.at("clamp_review_required").get<bool>());
    EXPECT_DOUBLE_EQ(1.0, j.at("metrics").at("clamp_rate").get<double>());
    bool found = false;
    for (const auto& gate : j.at("gates")) {
        if (gate.at("id") == "PRE-E2E-CLAMP-REVIEW") { found = true; EXPECT_EQ("review_required", gate.at("result")); }
        EXPECT_NE("PRE-E2E-SIGNED-BIAS-PASS", gate.at("id"));
    }
    EXPECT_TRUE(found);
}

TEST_F(PreE2ERun, ReplaysTheSameCpuPipelineForDeterminismEvidence) {
    std::vector<uint16_t> raw(256);
    for (size_t i = 0; i < raw.size(); ++i) raw[i] = static_cast<uint16_t>(1000 + (i * 7 % 13));
    setRaw(raw);
    const auto j = evaluate(options);
    ASSERT_TRUE(j.at("run_succeeded").get<bool>());
    ASSERT_TRUE(j.at("metrics").contains("determinism_rmse"));
    ASSERT_TRUE(j.at("metrics").at("determinism_rmse").is_number());
    EXPECT_DOUBLE_EQ(0.0, j.at("metrics").at("determinism_rmse").get<double>());
    ASSERT_TRUE(j.contains("determinism"));
    EXPECT_EQ(2, j.at("determinism").at("pipeline_runs"));
    bool found = false;
    for (const auto& gate : j.at("gates"))
        if (gate.at("id") == "PRE-E2E-CPU-DETERMINISM") { found = true; EXPECT_EQ("pass", gate.at("result")); }
    EXPECT_TRUE(found);
    EXPECT_TRUE(j.at("input_preserved").get<bool>());
}

TEST_F(PreE2ERun, StripePassDoesNotPromoteABelowSignalFlatFrame) {
    options.inputKind = "flat";
    setGain(std::vector<float>(256, 4.0f));
    std::vector<uint16_t> raw(256);
    uint32_t state = 12345;
    for (auto& pixel : raw) {
        state = state * 1664525u + 1013904223u;
        pixel = static_cast<uint16_t>(5000 + (state >> 16) % 41);
    }
    setRaw(raw);
    const auto j = evaluate(options);
    ASSERT_TRUE(j.at("run_succeeded").get<bool>());
    ASSERT_TRUE(j.at("metrics").at("stripe_noise_ratio").is_number());
    EXPECT_LE(j.at("metrics").at("stripe_noise_ratio").get<double>(), 3.0);
    EXPECT_FALSE(j.at("flat_statistics").at("minimum_signal_met").get<bool>());
    for (const auto& gate : j.at("gates"))
        if (gate.at("id") == "PRE-E2E-FLAT-QUALITY") EXPECT_EQ("unmeasured", gate.at("result"));
}

TEST_F(PreE2ERun, DefinedFlatGatesCanBeMeasuredWithoutInventingATotalScore) {
    options.inputKind = "flat";
    setGain(std::vector<float>(256, 4.0f));
    std::vector<uint16_t> raw(256);
    uint32_t state = 12345;
    for (auto& pixel : raw) {
        state = state * 1664525u + 1013904223u;
        pixel = static_cast<uint16_t>(20000 + (state >> 16) % 41);
    }
    setRaw(raw);
    const auto j = evaluate(options);
    ASSERT_TRUE(j.at("run_succeeded").get<bool>());
    EXPECT_TRUE(j.at("flat_statistics").at("minimum_signal_met").get<bool>());
    size_t flatGates = 0;
    for (const auto& gate : j.at("gates"))
        if (gate.at("id") == "PRE-E2E-FLAT-QUALITY" || gate.at("id") == "PRE-E2E-FLAT-RELEASE-QUALITY") {
            ++flatGates;
            EXPECT_EQ("pass", gate.at("result"));
        }
    EXPECT_EQ(2u, flatGates);
    EXPECT_TRUE(j.at("metrics").at("calibration_effect_score").is_null());
    EXPECT_FALSE(j.at("phase1_pass").get<bool>());
}

TEST_F(PreE2ERun, FlatMetricsAreMeasuredOnTheGainStageAndNotInventedForOtherKinds) {
    options.inputKind = "flat";
    const auto j = evaluate(options);
    ASSERT_TRUE(j.contains("metrics"));
    ASSERT_TRUE(j.at("run_succeeded").get<bool>()) << j.dump();
    ASSERT_TRUE(j.at("metrics").at("flat_residual_pct").is_number());
    EXPECT_DOUBLE_EQ(0.0, j.at("metrics").at("flat_residual_pct").get<double>());
    EXPECT_TRUE(j.at("metrics").at("defect_recall").is_null()) << "a mask is not defect ground truth";
    EXPECT_TRUE(j.at("metrics").at("rmse_reference").is_null());
}

TEST_F(PreE2ERun, AUniformDarkFrameReportsUnmeasurableInsteadOfZeroBias) {
    options.inputKind = "dark";
    const auto j = evaluate(options);
    ASSERT_TRUE(j.contains("metrics"));
    EXPECT_TRUE(j.at("metrics").at("dark_bias_adu").is_null());
    EXPECT_FALSE(j.at("phase1_pass").get<bool>());
}

TEST_F(PreE2ERun, RejectsWrongSizeAndMissingCalibrationWithoutAQualityPass) {
    options.width = 17;
    auto j = evaluate(options);
    ASSERT_TRUE(j.contains("run_succeeded"));
    EXPECT_FALSE(j.at("run_succeeded").get<bool>());
    EXPECT_FALSE(j.at("phase1_pass").get<bool>());
    options.width = 16;
    std::filesystem::remove(dir / "gain.xcal");
    j = evaluate(options);
    EXPECT_FALSE(j.at("run_succeeded").get<bool>());
    EXPECT_FALSE(j.at("phase1_pass").get<bool>());
}

TEST_F(PreE2ERun, WritesJsonAndMarkdownOnlyInANewDirectoryAndNeverOverwritesEvidence) {
    const auto report = evaluate(options);
    const auto destination = dir / "report";
    ASSERT_TRUE(writeReports(destination, report));
    ASSERT_TRUE(std::filesystem::exists(destination / "report.json"));
    ASSERT_TRUE(std::filesystem::exists(destination / "report.md"));
    std::ifstream input(destination / "report.json");
    const auto saved = nlohmann::json::parse(input);
    EXPECT_EQ(report, saved);
    EXPECT_FALSE(writeReports(destination, nlohmann::json{{"overwritten", true}}));
    EXPECT_FALSE(writeReports(dir, report)) << "the raw/calibration directory is already populated";
}

TEST_F(PreE2ERun, SignedDarkBiasAndClampRateAreMeasuredWithoutANewSignedPassGate) {
    std::vector<uint16_t> raw(256);
    for (size_t i = 0; i < raw.size(); ++i) raw[i] = static_cast<uint16_t>(98u + i % 16u);
    std::ofstream out(options.raw, std::ios::binary | std::ios::trunc);
    out.write(reinterpret_cast<const char*>(raw.data()), 512);
    out.close();
    options.inputKind = "dark";
    const auto j = evaluate(options);
    ASSERT_TRUE(j.at("run_succeeded").get<bool>()) << j.dump();
    EXPECT_DOUBLE_EQ(-2.0, j.at("metrics").at("dark_bias_signed_adu").get<double>());
    EXPECT_DOUBLE_EQ(0.0, j.at("metrics").at("dark_bias_adu").get<double>());
    ASSERT_TRUE(j.at("metrics").contains("clamp_rate"));
    EXPECT_DOUBLE_EQ(0.125, j.at("metrics").at("clamp_rate").get<double>());
    for (const auto& g : j.at("gates")) EXPECT_EQ(std::string::npos, g.at("id").get<std::string>().find("SIGNED"));
}

TEST_F(PreE2ERun, InvalidGainCoverageUsesOriginalFileRatherThanTheSanitizedLoadedMap) {
    std::vector<float> pixels(256, 2.0f);
    pixels[0] = 0.0f; // accepted load: one defective gain pixel, below the provisional 5% cap
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version = XCAL_VERSION;
    hdr.type = XCAL_TYPE_GAIN;
    hdr.pixel_format = XCAL_FMT_FLOAT32;
    hdr.width = hdr.height = 16;
    hdr.payload_len = pixels.size() * sizeof(float);
    ASSERT_EQ(XPE_OK, write_xcal_file((dir / "gain.xcal").string().c_str(), hdr, nullptr, 0,
        reinterpret_cast<const uint8_t*>(pixels.data()), hdr.payload_len));
    options.inputKind = "flat";
    const auto j = evaluate(options);
    ASSERT_TRUE(j.at("run_succeeded").get<bool>()) << j.dump();
    ASSERT_TRUE(j.at("metrics").contains("invalid_gain_count"));
    EXPECT_EQ(1, j.at("metrics").at("invalid_gain_count"));
}

TEST_F(PreE2ERun, ReportsOnlyTheObservedConfiguredGhostBypass) {
    const auto j = evaluate(options);
    ASSERT_TRUE(j.at("run_succeeded").get<bool>()) << j.dump();
    ASSERT_TRUE(j.contains("ghost"));
    EXPECT_TRUE(j.at("ghost_bypassed").get<bool>());
    EXPECT_EQ("configured_bypass", j.at("ghost").at("bypass_reason"));
    EXPECT_FALSE(j.at("ghost").at("corrected_flag").get<bool>());
    EXPECT_TRUE(j.at("ghost").at("tier").is_null());
    EXPECT_TRUE(j.at("ghost").at("history_length").is_null());
    const auto md = markdown(j);
    EXPECT_NE(std::string::npos, md.find("configured_bypass"));
    EXPECT_NE(std::string::npos, md.find("no ghost handle"));
}

TEST_F(PreE2ERun, RejectedInputsDoNotClaimThatGhostWasExecutedOrBypassed) {
    options.width = 0;
    const auto j = evaluate(options);
    EXPECT_FALSE(j.at("run_succeeded").get<bool>());
    ASSERT_TRUE(j.contains("ghost_bypassed"));
    EXPECT_TRUE(j.at("ghost_bypassed").is_null());
    EXPECT_EQ("not_executed", j.at("ghost").at("bypass_reason"));
    EXPECT_TRUE(j.at("ghost").at("corrected_flag").is_null());
}

TEST_F(PreE2ERun, CorruptedMapsCannotProduceASuccessfulRun) {
    {
        std::fstream f(dir / "gain.xcal", std::ios::binary | std::ios::in | std::ios::out);
        f.seekp(-1, std::ios::end);
        const char wrong = '\x7f';
        f.write(&wrong, 1);
    }
    const auto j = evaluate(options);
    EXPECT_FALSE(j.at("run_succeeded").get<bool>());
    EXPECT_FALSE(j.at("phase1_pass").get<bool>());
}

TEST_F(PreE2ERun, ExpiredMapsCannotProduceASuccessfulRun) {
    // Change only the expiry field of this fixture's offset map.
    // The reader's integrity digest covers config+payload; expiry has its own header contract.
    {
        std::fstream f(dir / "offset.xcal", std::ios::binary | std::ios::in | std::ios::out);
        f.seekp(static_cast<std::streamoff>(offsetof(XCalFileHeader, expiry_epoch_ms)));
        const int64_t expired = 1;
        f.write(reinterpret_cast<const char*>(&expired), sizeof(expired));
    }
    const auto j = evaluate(options);
    EXPECT_FALSE(j.at("run_succeeded").get<bool>());
    EXPECT_FALSE(j.at("release_pass").get<bool>());
}

TEST_F(PreE2ERun, RejectsOverflowAndUnrecognizedEvidenceLabels) {
    options.width = options.height = std::numeric_limits<uint32_t>::max();
    EXPECT_FALSE(evaluate(options).at("run_succeeded").get<bool>());
    options.width = options.height = 16;
    options.inputKind = "assume-dark";
    EXPECT_FALSE(evaluate(options).at("run_succeeded").get<bool>());
    options.inputKind = "image";
    options.gainSemantics = "guess";
    EXPECT_FALSE(evaluate(options).at("run_succeeded").get<bool>());
}

TEST_F(PreE2ERun, FpnReductionUsesRawAndFinalStandardDeviationsNotPrnuImprovement) {
    std::vector<uint16_t> raw(256);
    for (size_t i = 0; i < raw.size(); ++i) raw[i] = static_cast<uint16_t>(1000u + i % 16u);
    setRaw(raw);
    options.inputKind = "flat";
    const auto j = evaluate(options);
    ASSERT_TRUE(j.at("run_succeeded").get<bool>());
    ASSERT_TRUE(j.at("metrics").at("fpn_reduction_db").is_number());
    EXPECT_NEAR(6.020599913279624, j.at("metrics").at("fpn_reduction_db").get<double>(), 1e-10);
    ASSERT_TRUE(j.at("metrics").contains("gain_stage_prnu_improvement_db"));
    EXPECT_NEAR(0.0, j.at("metrics").at("gain_stage_prnu_improvement_db").get<double>(), 1e-10);
}

TEST_F(PreE2ERun, AMeanChangeWithoutAVarianceChangeDoesNotBecomeFpnReduction) {
    std::vector<uint16_t> raw(256);
    std::vector<float> gain(256);
    for (size_t i = 0; i < raw.size(); ++i) {
        const auto v = static_cast<uint16_t>(i % 16u);
        raw[i] = static_cast<uint16_t>(1000u + v);
        gain[i] = (900.0f + v) / (450.0f + v);
    }
    setRaw(raw);
    setGain(gain);
    options.inputKind = "flat";
    const auto j = evaluate(options);
    ASSERT_TRUE(j.at("run_succeeded").get<bool>()) << j.dump();
    EXPECT_NEAR(0.0, j.at("metrics").at("fpn_reduction_db").get<double>(), 1e-3);
}

TEST_F(PreE2ERun, VerificationRoiCoverageAndGainApplicationRangeAreDistinct) {
    options.inputKind = "flat";
    for (const float value : {20.0f, 0.05f, 0.1f, 10.0f}) {
        std::vector<float> pixels(256, 2.0f);
        pixels[0] = value;
        setGain(pixels);
        const auto j = evaluate(options);
        ASSERT_TRUE(j.at("run_succeeded").get<bool>()) << j.dump();
        EXPECT_EQ(0, j.at("metrics").at("invalid_gain_count")) << "all values are finite and positive for the protocol ROI";
        ASSERT_TRUE(j.at("metrics").contains("gain_application_invalid_count"));
        const int invalid = value == 20.0f || value == 0.05f ? 1 : 0;
        EXPECT_EQ(invalid, j.at("metrics").at("gain_application_invalid_count"));
        EXPECT_DOUBLE_EQ(1.0 - invalid / 256.0, j.at("metrics").at("gain_application_coverage").get<double>());
    }
}

TEST_F(PreE2ERun, CliExitCodesDistinguishArgumentsExecutionAndOutputRefusal) {
    const auto output = dir / "cli-report";
    EXPECT_EQ(2, runE2ECli("--width 4x"));
    EXPECT_EQ(4, runE2ECli(cliArgs(dir)));
    EXPECT_EQ(0, runE2ECli(cliArgs(output)));
    std::ifstream in(output / "report.json");
    const auto j = nlohmann::json::parse(in);
    EXPECT_FALSE(j.at("phase1_pass").get<bool>()) << "execution success is not quality approval";
    std::filesystem::remove(options.raw);
    EXPECT_EQ(3, runE2ECli(cliArgs(dir / "cli-error")));
    EXPECT_TRUE(std::filesystem::exists(dir / "cli-error" / "report.json"));
}

TEST_F(PreE2ERun, OutputFailureDoesNotPublishACompleteJsonReport) {
    const auto impossible = options.raw / "child";
    EXPECT_FALSE(writeReports(impossible, evaluate(options)));
    const auto invalidTextDir = dir / "invalid-json";
    const nlohmann::json invalid = {{"bad_utf8", std::string(1, static_cast<char>(0xff))}};
    EXPECT_FALSE(writeReports(invalidTextDir, invalid));
    EXPECT_FALSE(std::filesystem::exists(invalidTextDir / "report.json"));
}
} // namespace
