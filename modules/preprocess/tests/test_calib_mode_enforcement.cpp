/**
 * @file test_calib_mode_enforcement.cpp
 * @brief SRS-CALIB-FUNC-031: the calibration mode a gain file records, and older files that record another one
 *
 * QA-A-336 (lead decision): SINGLE_POINT is the only mode. The multi-point modes and AUTO (QA-A-101, #169) were removed
 * with the gain polynomial -- the single-dose generator recorded the default MULTI_POINT_8 (3, "8 points, cubic fit"),
 * which was false about a one-dose, degree-0 file. It records 0 now. A single-map gain file written before carries
 * 1..4 in its config block and must still load: the loader reads the value as a record only.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr uint32_t W = 4;
constexpr uint32_t H = 4;
constexpr size_t   N = static_cast<size_t>(W) * H;

struct Generated {
    XpeErrorCode         rc = XPE_OK;
    std::string          json;
    std::vector<uint8_t> payload;
    int                  qualityMode = -1;
};

class CalibModeEnforcementTest : public ::testing::Test {
protected:
    fs::path tmpDir;
    int      generation = 0;
    std::vector<std::vector<uint16_t>> flatStore;

    void SetUp() override {
        (void)xpe_preprocess_init(nullptr);
        tmpDir = fs::temp_directory_path() / "xpe_calib_mode_enforcement";
        fs::remove_all(tmpDir);
        fs::create_directories(tmpDir);
    }

    void TearDown() override {
        // QA-A-138 (#198): leave the pending alert queue as this test found
        // it. The product drains it through this public call; nothing in
        // xpe_preprocess_shutdown() touches the common-module queue.
        xpe_clear_alerts();
        xpe_calib_set_mode(XPE_CALIB_SINGLE_POINT);   // module default
        xpe_preprocess_shutdown();
        fs::remove_all(tmpDir);
    }

    /** A scalar gain file, every pixel @p value, with @p json as its config block (none when empty). */
    std::string writeGainLevel(const std::string& name, float value, const std::string& json = std::string()) {
        const std::string path = (tmpDir / name).string();
        const std::vector<float> data(N, value);
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version      = XCAL_VERSION;
        hdr.type         = static_cast<uint32_t>(XCAL_TYPE_GAIN);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width = W; hdr.height = H;
        hdr.payload_len  = data.size() * sizeof(float);
        EXPECT_EQ(XPE_OK,
                  write_xcal_file(path.c_str(), hdr,
                                  json.empty() ? nullptr : reinterpret_cast<const uint8_t*>(json.data()), json.size(),
                                  reinterpret_cast<const uint8_t*>(data.data()),
                                  hdr.payload_len));
        return path;
    }

    // Reads back what the generator wrote, plus the quality store.
    void readBack(const std::string& path, Generated& g) {
        XpeCalibQualityMeta meta{};
        EXPECT_EQ(XPE_OK, xpe_calib_get_quality_meta(&meta));
        g.qualityMode = meta.calibration_mode;
        std::ifstream f(path, std::ios::binary);
        ASSERT_TRUE(f.is_open()) << path;
        XCalFileHeader hdr{};
        f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
        g.json.assign(static_cast<size_t>(hdr.config_json_len), '\0');
        if (hdr.config_json_len > 0) f.read(&g.json[0], hdr.config_json_len);
        g.payload.assign(static_cast<size_t>(hdr.payload_len), 0);
        if (hdr.payload_len > 0) {
            f.read(reinterpret_cast<char*>(g.payload.data()),
                   static_cast<std::streamsize>(hdr.payload_len));
        }
    }

    Generated single(XpeCalibrationMode mode) {
        EXPECT_EQ(XPE_OK, xpe_calib_set_mode(mode));
        const int32_t count = 4;
        flatStore.assign(count, std::vector<uint16_t>(N, 3000));
        std::vector<XpeImageBuffer> frames(count);
        for (int32_t i = 0; i < count; ++i) {
            XpeImageBuffer& b = frames[static_cast<size_t>(i)];
            b.data = flatStore[static_cast<size_t>(i)].data();
            b.width = W; b.height = H;
            b.bitsAllocated = 16; b.bitsStored = 16;
            b.format = XPE_PIXEL_UINT16;
            b.dataSize = static_cast<uint32_t>(N * sizeof(uint16_t));
        }
        const std::string out =
            (tmpDir / ("s" + std::to_string(generation++) + ".xcal")).string();
        Generated g;
        g.rc = xpe_calib_generate_gain(frames.data(), count, nullptr, out.c_str(), nullptr);
        if (g.rc == XPE_OK) readBack(out, g);
        return g;
    }
};

std::string ModeKey(int mode) {
    return "\"calibration_mode\":" + std::to_string(mode) + ",";
}
std::string RequestedKey(int mode) {
    return "\"requested_calibration_mode\":" + std::to_string(mode) + ",";
}

}  // namespace

// The single-dose generator records SINGLE_POINT: 0 in the file (both keys) and in the quality record.
TEST_F(CalibModeEnforcementTest, SingleDoseGeneratorRecordsSinglePoint) {
    const Generated g = single(XPE_CALIB_SINGLE_POINT);
    ASSERT_EQ(XPE_OK, g.rc);
    EXPECT_EQ(static_cast<int>(XPE_CALIB_SINGLE_POINT), g.qualityMode);
    EXPECT_NE(std::string::npos, g.json.find("{" + ModeKey(0))) << g.json;
    EXPECT_NE(std::string::npos, g.json.find(RequestedKey(0))) << g.json;
    EXPECT_EQ(std::string::npos, g.json.find(ModeKey(3))) << g.json;
}

// QA-A-336: an existing single-map gain file that records a removed mode (3, the old default; 1, 2 and 4 when they were
// set) loads and corrects exactly like the same map recording 0. The value is a record only: it reaches the quality
// record as written and selects nothing.
TEST_F(CalibModeEnforcementTest, AScalarGainFileRecordingARemovedModeLoadsAndCorrectsLikeModeZero) {
    auto quality = [](int mode) {
        return "{\"calibration_mode\":" + std::to_string(mode) + ",\"requested_calibration_mode\":" + std::to_string(mode) +
               ",\"actual_dose_levels\":1,\"polynomial_degree\":0,\"fit_r_squared\":1.000000000}";
    };
    auto correct = [](std::vector<float>* out) {
        std::vector<uint16_t> in(N);
        for (size_t i = 0; i < N; ++i) in[i] = static_cast<uint16_t>(1000 + 37 * i);
        out->assign(N, -1.0f);
        XpeImageBuffer ib{}, ob{};
        ib.data = in.data(); ib.width = W; ib.height = H; ib.bitsAllocated = 16; ib.bitsStored = 16;
        ib.format = XPE_PIXEL_UINT16; ib.dataSize = N * sizeof(uint16_t);
        ob.data = out->data(); ob.width = W; ob.height = H; ob.bitsAllocated = 32; ob.bitsStored = 32;
        ob.format = XPE_PIXEL_FLOAT32; ob.dataSize = N * sizeof(float);
        XpeImageMetadata meta{};
        return xpe_gain_correct(&ib, &ob, &meta);
    };
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(writeGainLevel("mode0.xcal", 2.0f, quality(0)).c_str()));
    std::vector<float> reference;
    ASSERT_EQ(XPE_OK, correct(&reference));
    ASSERT_FLOAT_EQ(500.0f, reference[0]) << "control: 1000 / 2";

    for (const int mode : {3, 1, 2, 4}) {
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        const std::string path = writeGainLevel("mode" + std::to_string(mode) + ".xcal", 2.0f, quality(mode));
        ASSERT_EQ(XPE_OK, xpe_calib_load_gain(path.c_str())) << "mode " << mode;
        XpeCalibQualityMeta meta{};
        ASSERT_EQ(XPE_OK, xpe_calib_get_quality_meta(&meta));
        EXPECT_EQ(mode, static_cast<int>(meta.calibration_mode)) << "the value is kept as the record the file gives";
        std::vector<float> out;
        ASSERT_EQ(XPE_OK, correct(&out)) << "mode " << mode;
        EXPECT_EQ(0, std::memcmp(reference.data(), out.data(), N * sizeof(float))) << "mode " << mode << " corrects differently";
    }
}
