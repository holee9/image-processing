/**
 * @file test_calib_mode_enforcement.cpp
 * @brief SRS-CALIB-FUNC-031 (3)(4)(5)(8): mode caps and AUTO selection (QA-A-101, #169)
 *
 * Boundary table (levels N, requested degree D) under AUTO -- the smallest
 * mode whose max_points >= N and poly_degree >= D:
 *
 *   N   D   AUTO resolves to       explicit mode with the same N
 *   0   0   INVALID_INPUT          INVALID_INPUT (every mode)
 *   1   0   SINGLE_POINT (1, 0)    all modes accept
 *   2   1   DUAL_POINT   (2, 1)    SINGLE_POINT rejects
 *   3   2   MULTI_POINT_5 (5, 2)   DUAL_POINT rejects
 *   5   2   MULTI_POINT_5          MULTI_POINT_5 accepts (at its cap)
 *   6   2   MULTI_POINT_8 (8, 3)   MULTI_POINT_5 rejects (cap + 1)
 *   8   2   MULTI_POINT_8          MULTI_POINT_8 accepts (at its cap)
 *   9   2   MULTI_POINT_10 (10, 4) MULTI_POINT_8 rejects (cap + 1)
 *  10   2   MULTI_POINT_10         MULTI_POINT_10 accepts (hard cap)
 *  11   2   INVALID_INPUT          MULTI_POINT_10 rejects (cap + 1)
 *   3   3   MULTI_POINT_8          MULTI_POINT_5 rejects (degree 3 > 2)
 *   3   4   MULTI_POINT_10         MULTI_POINT_8 rejects (degree 4 > 3)
 *
 * QA-A-336: the gain polynomial generator (FUNC-027) was removed, and with it every row of the table that a
 * public generator could reach except N = 1: the single-dose generator (one level, degree 0) is the only one
 * left. The table stays as the definition of the mode caps (xpe_calib_resolve_mode is unchanged); the rows
 * with N >= 3 are no longer driven here.
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
        xpe_calib_set_mode(XPE_CALIB_MULTI_POINT_8);   // module default
        xpe_preprocess_shutdown();
        fs::remove_all(tmpDir);
    }

    std::string writeGainLevel(const std::string& name, float value) {
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
                  write_xcal_file(path.c_str(), hdr, nullptr, 0,
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

// FUNC-031 (8) for the single-dose generator: one level, degree 0.
TEST_F(CalibModeEnforcementTest, SingleDoseGeneratorUnderAutoRecordsSinglePoint) {
    const Generated a = single(XPE_CALIB_AUTO);
    const Generated e = single(XPE_CALIB_SINGLE_POINT);
    ASSERT_EQ(XPE_OK, e.rc);
    ASSERT_EQ(XPE_OK, a.rc);
    EXPECT_EQ(e.payload, a.payload);
    EXPECT_EQ(static_cast<int>(XPE_CALIB_SINGLE_POINT), a.qualityMode);
    EXPECT_NE(std::string::npos, a.json.find("{" + ModeKey(XPE_CALIB_SINGLE_POINT))) << a.json;
    EXPECT_NE(std::string::npos, a.json.find(RequestedKey(XPE_CALIB_AUTO))) << a.json;
}

// Every explicit mode accepts the single-dose generator (one level, degree 0).
TEST_F(CalibModeEnforcementTest, SingleDoseGeneratorIsAcceptedByEveryMode) {
    for (int m = XPE_CALIB_SINGLE_POINT; m < XPE_CALIB_AUTO; ++m) {
        const Generated g = single(static_cast<XpeCalibrationMode>(m));
        EXPECT_EQ(XPE_OK, g.rc) << "mode=" << m;
        EXPECT_EQ(m, g.qualityMode);
    }
}

// The selection is reported on the alert queue as well (card: log the mode).
TEST_F(CalibModeEnforcementTest, AutoResolutionIsReportedOnTheAlertQueue) {
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, single(XPE_CALIB_AUTO).rc);
    bool seen = false;
    char msg[256];
    int32_t severity = -1;
    const int32_t count = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < count; ++i) {
        ASSERT_EQ(XPE_OK, xpe_get_pending_alert(i, msg, sizeof(msg), &severity));
        if (std::string(msg) == "XPE_CALIB_AUTO resolved to mode 0" &&
            severity == XPE_ALERT_INFO) {
            seen = true;
        }
    }
    xpe_clear_alerts();
    EXPECT_TRUE(seen);
}
