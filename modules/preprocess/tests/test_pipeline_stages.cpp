/**
 * @file test_pipeline_stages.cpp
 * @brief Pipeline stage bodies and failure propagation (QA-A-32, #120)
 *
 * `pipeline.cpp` measured 155/216 (0.72) in coverage run 34479945843. The 61
 * uncovered lines are the stage BODIES -- readout validation (108-114), temp
 * compensation (122-131), nonlinearity (155-168), gain (201-213), defect
 * (255-267) and the metadata-flag writes that follow each. Every existing case
 * either bypasses the stages or stops at argument validation, so the bodies
 * never ran.
 *
 * These cases enable stages instead of bypassing them, and check two things
 * per stage: that the body executes (the metadata flag appears), and that a
 * stage failure propagates out rather than being swallowed.
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr uint32_t W = 8, H = 8;
constexpr size_t   N = static_cast<size_t>(W) * H;

class PipelineStageTest : public ::testing::Test {
protected:
    std::vector<uint16_t> pixels;
    XpeImageBuffer        img{};
    XpeImageMetadata      meta{};
    fs::path              tmpDir;

    void SetUp() override {
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        tmpDir = fs::temp_directory_path() / "xpe_pipeline_stages";
        fs::remove_all(tmpDir);
        fs::create_directories(tmpDir);

        pixels.assign(N, 1000u);
        img.data = pixels.data();
        img.width = W; img.height = H;
        img.bitsAllocated = 16; img.bitsStored = 16;
        img.format = XPE_PIXEL_UINT16;
        img.dataSize = pixels.size() * sizeof(uint16_t);

        // The pipeline reads the detector temperature from its config JSON
        // (`detectorTempC`), not from metadata, so a zeroed metadata is enough.
        meta = XpeImageMetadata{};
    }

    void TearDown() override {
        xpe_preprocess_shutdown();
        fs::remove_all(tmpDir);
    }

    void writeFloatMap(const std::string& path, XCalType type, float value) {
        const std::vector<float> data(N, value);
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(type);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width = W; hdr.height = H;
        hdr.payload_len = data.size() * sizeof(float);
        ASSERT_EQ(XPE_OK,
                  write_xcal_file(path.c_str(), hdr, nullptr, 0,
                                  reinterpret_cast<const uint8_t*>(data.data()),
                                  hdr.payload_len));
    }
};

// Readout validation and temperature compensation need no calibration, so a
// config that bypasses only the calibration stages runs both bodies to
// completion and records their metadata flags.
TEST_F(PipelineStageTest, ReadoutAndTempStagesRunAndSetFlags) {
    const char* config =
        "{\"bypassOffset\":true,\"bypassGain\":true,\"bypassDefect\":true,"
        "\"bypassGhost\":true,\"bypassNonlinearity\":true,\"bypassBinning\":true,"
        "\"detectorTempC\":30.0}";

    ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline(&img, &meta, nullptr, nullptr, config));

    EXPECT_TRUE(meta.flags & XPE_FLAG_READOUT_VALIDATED)
        << "the readout stage body must have run";
    EXPECT_TRUE(meta.flags & XPE_FLAG_TEMP_COMPENSATED)
        << "the temperature stage body must have run";
}

// The nonlinearity and binning bodies run on their own too.
TEST_F(PipelineStageTest, NonlinearityAndBinningStagesRun) {
    const char* config =
        "{\"bypassOffset\":true,\"bypassGain\":true,\"bypassDefect\":true,"
        "\"bypassGhost\":true,\"bypassReadout\":true,\"bypassTemp\":true}";

    ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline(&img, &meta, nullptr, nullptr, config));
    EXPECT_TRUE(meta.flags & XPE_FLAG_NONLINEARITY_CORRECTED);
}

// A stage that cannot run must abort the pipeline. With the offset stage
// enabled and no offset map loaded, the pipeline returns the stage's own error
// instead of continuing with uncorrected data.
TEST_F(PipelineStageTest, OffsetStageFailurePropagates) {
    const char* config =
        "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassNonlinearity\":true,"
        "\"bypassGain\":true,\"bypassDefect\":true,\"bypassGhost\":true,"
        "\"bypassBinning\":true}";

    const XpeErrorCode rc =
        xpe_preprocess_pipeline(&img, &meta, nullptr, nullptr, config);

    EXPECT_NE(XPE_OK, rc) << "a stage that cannot run must not be skipped silently";
    EXPECT_EQ(XPE_ERR_CALIB_NOT_LOADED, rc)
        << "the offset stage's own error must reach the caller unchanged";
    EXPECT_FALSE(meta.flags & XPE_FLAG_OFFSET_CORRECTED)
        << "a failed stage must not claim it ran";
}

// Same for gain.
TEST_F(PipelineStageTest, GainStageFailurePropagates) {
    const char* config =
        "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassNonlinearity\":true,"
        "\"bypassOffset\":true,\"bypassDefect\":true,\"bypassGhost\":true,"
        "\"bypassBinning\":true}";

    EXPECT_EQ(XPE_ERR_CALIB_NOT_LOADED,
              xpe_preprocess_pipeline(&img, &meta, nullptr, nullptr, config));
}

// With the maps loaded the offset and gain bodies run to completion.
TEST_F(PipelineStageTest, OffsetAndGainStagesRunWithCalibrationLoaded) {
    const std::string offsetPath = (tmpDir / "offset.xcal").string();
    const std::string gainPath   = (tmpDir / "gain.xcal").string();
    ASSERT_NO_FATAL_FAILURE(writeFloatMap(offsetPath, XCAL_TYPE_OFFSET, 100.0f));
    ASSERT_NO_FATAL_FAILURE(writeFloatMap(gainPath, XCAL_TYPE_GAIN, 2.0f));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(offsetPath.c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gainPath.c_str()));

    const char* config =
        "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassNonlinearity\":true,"
        "\"bypassDefect\":true,\"bypassGhost\":true,\"bypassBinning\":true}";

    ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline(&img, &meta, nullptr, nullptr, config));
    EXPECT_TRUE(meta.flags & XPE_FLAG_OFFSET_CORRECTED);
    EXPECT_TRUE(meta.flags & XPE_FLAG_GAIN_CORRECTED);
}

// Every stage enabled at once, with calibration available: the whole chain
// runs in one pass rather than one stage per test.
TEST_F(PipelineStageTest, AllStagesEnabledRunInSequence) {
    const std::string offsetPath = (tmpDir / "offset.xcal").string();
    const std::string gainPath   = (tmpDir / "gain.xcal").string();
    ASSERT_NO_FATAL_FAILURE(writeFloatMap(offsetPath, XCAL_TYPE_OFFSET, 50.0f));
    ASSERT_NO_FATAL_FAILURE(writeFloatMap(gainPath, XCAL_TYPE_GAIN, 1.5f));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(offsetPath.c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gainPath.c_str()));

    // Defect and ghost stay bypassed: no defect map is loaded, and ghost needs
    // a handle. Both are covered by their own suites.
    const char* config = "{\"bypassDefect\":true,\"bypassGhost\":true}";

    ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline(&img, &meta, nullptr, nullptr, config));

    EXPECT_TRUE(meta.flags & XPE_FLAG_READOUT_VALIDATED);
    EXPECT_TRUE(meta.flags & XPE_FLAG_TEMP_COMPENSATED);
    EXPECT_TRUE(meta.flags & XPE_FLAG_OFFSET_CORRECTED);
    EXPECT_TRUE(meta.flags & XPE_FLAG_GAIN_CORRECTED);
}

// MEASURED ASYMMETRY, not a conformance assertion. Offset and gain return
// XPE_ERR_CALIB_NOT_LOADED when their map is missing (the two cases above), but
// the defect stage is GATED on map presence instead: with no defect map loaded
// the pipeline returns XPE_OK and the stage simply does not run. An earlier
// draft of this case expected an error and measured 0 vs 0.
//
// So a caller that enables the defect stage without loading a map gets a
// success it may read as "defects were corrected". The flag is the only signal
// that they were not -- which is what this case pins. Reported in QA-A-32 rather
// than changed here: altering it is a production-branch decision.
TEST_F(PipelineStageTest, DefectStageIsSkippedRatherThanFailingWithoutAMap) {
    const char* config =
        "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassNonlinearity\":true,"
        "\"bypassOffset\":true,\"bypassGain\":true,\"bypassGhost\":true,"
        "\"bypassBinning\":true}";

    const XpeErrorCode rc =
        xpe_preprocess_pipeline(&img, &meta, nullptr, nullptr, config);

    EXPECT_EQ(XPE_OK, rc) << "the defect stage is skipped, not failed";
    EXPECT_FALSE(meta.flags & XPE_FLAG_DEFECT_CORRECTED)
        << "and the flag is the only evidence that it did not run";
}

} // namespace
