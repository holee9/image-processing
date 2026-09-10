/**
 * @file test_calib_generate_offset_config.cpp
 * @brief QA-A-39 (#138 #97): xpe_calib_generate_offset's config_json_or_null.
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 *
 * Until QA-A-39 the public entry point parsed a null config unconditionally, so
 * it always ran the Mean method. XPE-ALG-001 §9.8's SigmaClip and Median, and
 * the §9.8.2.1 N_min defect marking QA-A-38 implemented, were unreachable from
 * any shipped entry point -- the marking code existed and nothing could run it.
 *
 * These cases drive everything through the PUBLIC C API, which is the point:
 * QA-A-38 could only observe the mask and the bit-level OR, because the global
 * store is DLL-internal and its test binary compiles the generation TU
 * directly. Here the store is reached the way a real caller reaches it -- run
 * the generator, then read the map back with xpe_calib_save(..., "defect", ...).
 * That closes QA-A-38's Gap 1 and Gap 2.
 *
 * The sample series is A-26's: {100, 110, 105, 108, 500}.
 *   mean       -> 184.6
 *   sigma_clip -> 106.5 at kappa = 1.0, converging on |S| = 2
 *   N = 5      -> N_min = max(3, floor(5/4)) = 3, and 2 < 3, so the pixel is
 *                 marked a static defect while keeping its 106.5 value.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr uint32_t W = 2, H = 2;
constexpr size_t   N = static_cast<size_t>(W) * H;

class GenerateOffsetConfigTest : public ::testing::Test {
protected:
    fs::path tmpDir;

    void SetUp() override {
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        tmpDir = fs::temp_directory_path() / "xpe_offset_config";
        fs::remove_all(tmpDir);
        fs::create_directories(tmpDir);
    }

    void TearDown() override {
        // Clears the global calibration store as well, so each case starts with
        // no defect map (preprocess.cpp:71 assigns a fresh CalibrationData).
        xpe_preprocess_shutdown();
        fs::remove_all(tmpDir);
    }

    /** Runs the public generator over @p values, one frame per value. */
    XpeErrorCode generate(const std::vector<uint16_t>& values,
                          const std::string& outName,
                          const char* configJson) {
        frameStore.clear();
        frameStore.reserve(values.size());
        for (uint16_t v : values) frameStore.emplace_back(N, v);

        std::vector<XpeImageBuffer> bufs(values.size());
        for (size_t i = 0; i < values.size(); ++i) {
            XpeImageBuffer& b = bufs[i];
            b.data = frameStore[i].data();
            b.width = W; b.height = H;
            b.bitsAllocated = 16; b.bitsStored = 16;
            b.format = XPE_PIXEL_UINT16;
            b.dataSize = static_cast<uint32_t>(N * sizeof(uint16_t));
        }
        return xpe_calib_generate_offset(
            bufs.data(), static_cast<int32_t>(bufs.size()), 100.0f, 25.0f,
            (tmpDir / outName).string().c_str(), configJson);
    }

    /** Reads the FLOAT32 payload of an XCal file written by the generator. */
    std::vector<float> offsetPayload(const std::string& name) {
        std::ifstream f(tmpDir / name, std::ios::binary);
        EXPECT_TRUE(f.is_open());
        XCalFileHeader hdr{};
        f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
        f.seekg(static_cast<std::streamoff>(hdr.config_json_len), std::ios::cur);
        std::vector<float> payload(static_cast<size_t>(hdr.payload_len) / sizeof(float));
        if (!payload.empty()) {
            f.read(reinterpret_cast<char*>(payload.data()),
                   static_cast<std::streamsize>(hdr.payload_len));
        }
        return payload;
    }

    /** Saves the global defect map and returns its payload. */
    std::vector<uint8_t> savedDefectMask(const std::string& name) {
        const std::string path = (tmpDir / name).string();
        EXPECT_EQ(XPE_OK, xpe_calib_save(path.c_str(), "defect", 0));

        std::ifstream f(path, std::ios::binary);
        EXPECT_TRUE(f.is_open());
        XCalFileHeader hdr{};
        f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
        f.seekg(static_cast<std::streamoff>(hdr.config_json_len), std::ios::cur);
        std::vector<uint8_t> mask(static_cast<size_t>(hdr.payload_len), 0);
        if (!mask.empty()) {
            f.read(reinterpret_cast<char*>(mask.data()),
                   static_cast<std::streamsize>(mask.size()));
        }
        return mask;
    }

    std::vector<std::vector<uint16_t>> frameStore;
};

// The parameter has to actually reach the method selector: the same frames must
// come out differently under sigma clipping than under the mean.
TEST_F(GenerateOffsetConfigTest, SigmaClipProducesADifferentMapThanMean) {
    const std::vector<uint16_t> series = {100, 110, 105, 108, 500};

    ASSERT_EQ(XPE_OK, generate(series, "mean.xcal", nullptr));
    ASSERT_EQ(XPE_OK, generate(series, "clip.xcal",
                               "{\"method\":\"sigma_clip\",\"sigma\":1.0}"));

    const std::vector<float> mean = offsetPayload("mean.xcal");
    const std::vector<float> clip = offsetPayload("clip.xcal");
    ASSERT_EQ(N, mean.size());
    ASSERT_EQ(N, clip.size());

    EXPECT_FLOAT_EQ(184.6f, mean[0]) << "plain mean of the five frames";
    EXPECT_FLOAT_EQ(106.5f, clip[0]) << "mean of {105, 108} after clipping";
}

// NULL must behave exactly as the function did before the parameter existed.
TEST_F(GenerateOffsetConfigTest, NullConfigIsTheSameAsExplicitMean) {
    const std::vector<uint16_t> series = {100, 110, 105, 108, 500};

    ASSERT_EQ(XPE_OK, generate(series, "null.xcal", nullptr));
    ASSERT_EQ(XPE_OK, generate(series, "explicit.xcal", "{\"method\":\"mean\"}"));

    EXPECT_EQ(offsetPayload("null.xcal"), offsetPayload("explicit.xcal"));
}

// Median is reachable too -- the parameter is not a sigma-clip-only switch.
TEST_F(GenerateOffsetConfigTest, MedianIsReachable) {
    ASSERT_EQ(XPE_OK, generate({100, 110, 105, 108, 500}, "median.xcal",
                               "{\"method\":\"median\"}"));

    const std::vector<float> median = offsetPayload("median.xcal");
    ASSERT_EQ(N, median.size());
    EXPECT_FLOAT_EQ(108.0f, median[0]) << "median of {100,105,108,110,500}";
}

// A bad config is reported, not silently defaulted -- otherwise a typo in the
// method name would quietly produce a mean map the caller never asked for.
TEST_F(GenerateOffsetConfigTest, MalformedConfigIsReported) {
    const std::vector<uint16_t> series = {100, 110, 105};

    EXPECT_EQ(XPE_ERR_CONFIG_INVALID,
              generate(series, "bad1.xcal", "{\"method\":\"sigma_clipp\"}"));
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID,
              generate(series, "bad2.xcal",
                       "{\"method\":\"sigma_clip\",\"sigma\":0}"));
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID,
              generate(series, "bad3.xcal",
                       "{\"method\":\"sigma_clip\",\"max_iter\":0}"));

    // The failure happens before any file is written.
    EXPECT_FALSE(fs::exists(tmpDir / "bad1.xcal"));
}

// QA-A-38 Gap 1 and Gap 2, closed: the §9.8.2.1 marks reach the global defect
// map through the shipped entry point, and a real caller can read them back.
TEST_F(GenerateOffsetConfigTest, NMinMarksLandInTheGlobalDefectMap) {
    ASSERT_EQ(XPE_OK, generate({100, 110, 105, 108, 500}, "marked.xcal",
                               "{\"method\":\"sigma_clip\",\"sigma\":1.0}"));

    const std::vector<uint8_t> mask = savedDefectMask("defect.xcal");
    ASSERT_EQ(N, mask.size());
    for (size_t i = 0; i < N; ++i) {
        EXPECT_EQ(1u, mask[i]) << "pixel " << i << ": |S| = 2 < N_min = 3";
    }

    // And the offset value is untouched by the mark (XPE-ALG-001 9.8.3).
    const std::vector<float> offset = offsetPayload("marked.xcal");
    ASSERT_EQ(N, offset.size());
    EXPECT_FLOAT_EQ(106.5f, offset[0]);
}

// A run that marks nothing must not create a defect map -- neither the mean
// path nor a sigma-clip run that keeps every frame.
TEST_F(GenerateOffsetConfigTest, RunsThatMarkNothingCreateNoDefectMap) {
    ASSERT_EQ(XPE_OK, generate({100, 110, 105, 108, 500}, "m.xcal", nullptr));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_calib_save((tmpDir / "d1.xcal").string().c_str(), "defect", 0))
        << "the mean method has no N_min clause";

    ASSERT_EQ(XPE_OK, generate({100, 102, 104, 106, 108}, "c.xcal",
                               "{\"method\":\"sigma_clip\"}"));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_calib_save((tmpDir / "d2.xcal").string().c_str(), "defect", 0))
        << "nothing was clipped, so |S| = 5 and no pixel is below the floor";
}

} // namespace
