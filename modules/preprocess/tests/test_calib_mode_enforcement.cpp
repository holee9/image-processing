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
 * The polynomial generator also requires N >= 3 (FUNC-027) and the single-dose
 * generator is N = 1, so the rows N = 0 and N = 2 are documented here but not
 * reachable through the public API.
 *
 * The AUTO results are compared with a run under the explicit mode the table
 * names: same return code, same coefficient payload, same recorded mode. The
 * explicit run is the independent reference.
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

    // A smooth, increasing gain-vs-dose series with curvature, so the fitted
    // degree matters: g(i) = 1 + 0.1 i + 0.01 i^2.
    Generated poly(XpeCalibrationMode mode, int32_t levels, int32_t degree) {
        EXPECT_EQ(XPE_OK, xpe_calib_set_mode(mode));
        const std::string tag = "p" + std::to_string(generation++) + "_";
        std::vector<std::string> paths;
        std::vector<const char*> ptrs;
        std::vector<double> doses;
        for (int32_t i = 0; i < levels; ++i) {
            const float v = 1.0f + 0.1f * static_cast<float>(i) +
                            0.01f * static_cast<float>(i * i);
            paths.push_back(writeGainLevel(tag + std::to_string(i) + ".xcal", v));
            doses.push_back(static_cast<double>(i + 1));
        }
        for (const auto& s : paths) ptrs.push_back(s.c_str());
        const std::string out = (tmpDir / (tag + "out.xcal")).string();
        Generated g;
        g.rc = xpe_calib_generate_gain_polynomial(
            ptrs.empty() ? nullptr : ptrs.data(), doses.data(), levels, degree, out.c_str());
        if (g.rc == XPE_OK) readBack(out, g);
        return g;
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

// --- the table, through the public generators ---------------------------------
//
// xpe_calib_resolve_mode is internal and not exported from the DLL, so every row
// is driven through xpe_calib_generate_gain_polynomial (N >= 3) or
// xpe_calib_generate_gain (N = 1). The rows N = 0 and N = 2 cannot be reached
// through either generator (FUNC-026 is one level, FUNC-027 needs N >= 3).

TEST_F(CalibModeEnforcementTest, AutoResolvesToTheSmallestFittingMode) {
    struct Row { int32_t n, d; XpeErrorCode rc; int mode; };
    const Row rows[] = {
        { 3, 2, XPE_OK, XPE_CALIB_MULTI_POINT_5},
        { 5, 2, XPE_OK, XPE_CALIB_MULTI_POINT_5},
        { 6, 2, XPE_OK, XPE_CALIB_MULTI_POINT_8},
        { 8, 2, XPE_OK, XPE_CALIB_MULTI_POINT_8},
        { 9, 2, XPE_OK, XPE_CALIB_MULTI_POINT_10},
        {10, 2, XPE_OK, XPE_CALIB_MULTI_POINT_10},
        {11, 2, XPE_ERR_INVALID_INPUT, -1},
        { 3, 3, XPE_OK, XPE_CALIB_MULTI_POINT_8},
        { 3, 4, XPE_OK, XPE_CALIB_MULTI_POINT_10},
        { 8, 4, XPE_OK, XPE_CALIB_MULTI_POINT_10},
    };
    for (const Row& r : rows) {
        const Generated g = poly(XPE_CALIB_AUTO, r.n, r.d);
        EXPECT_EQ(r.rc, g.rc) << "N=" << r.n << " D=" << r.d;
        if (r.rc == XPE_OK) EXPECT_EQ(r.mode, g.qualityMode) << "N=" << r.n << " D=" << r.d;
    }
    const Generated one = single(XPE_CALIB_AUTO);
    EXPECT_EQ(XPE_OK, one.rc);
    EXPECT_EQ(static_cast<int>(XPE_CALIB_SINGLE_POINT), one.qualityMode);
}

// --- through the public generators -------------------------------------------

// FUNC-031 (4)(8): the cap is enforced by the polynomial generator.
TEST_F(CalibModeEnforcementTest, PolynomialGeneratorRefusesLevelsAboveTheExplicitCap) {
    EXPECT_EQ(XPE_OK, poly(XPE_CALIB_MULTI_POINT_5, 5, 2).rc);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, poly(XPE_CALIB_MULTI_POINT_5, 6, 2).rc);
    EXPECT_EQ(XPE_OK, poly(XPE_CALIB_MULTI_POINT_8, 8, 3).rc);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, poly(XPE_CALIB_MULTI_POINT_8, 9, 3).rc);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, poly(XPE_CALIB_MULTI_POINT_8, 3, 4).rc);
    EXPECT_EQ(XPE_OK, poly(XPE_CALIB_MULTI_POINT_10, 10, 4).rc);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, poly(XPE_CALIB_MULTI_POINT_10, 11, 2).rc);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, poly(XPE_CALIB_MULTI_POINT_5, 3, 3).rc);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, poly(XPE_CALIB_DUAL_POINT, 3, 1).rc);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, poly(XPE_CALIB_SINGLE_POINT, 3, 1).rc);
}

// FUNC-031 (5): at every boundary, AUTO does exactly what the explicit mode
// named by the table does -- same code, same coefficients, same recorded mode.
TEST_F(CalibModeEnforcementTest, AutoMatchesTheExplicitModeAtEveryBoundary) {
    struct Row { int32_t n, d; XpeCalibrationMode expected; };
    const Row rows[] = {
        { 3, 2, XPE_CALIB_MULTI_POINT_5},
        { 5, 2, XPE_CALIB_MULTI_POINT_5},
        { 6, 2, XPE_CALIB_MULTI_POINT_8},
        { 8, 2, XPE_CALIB_MULTI_POINT_8},
        { 9, 2, XPE_CALIB_MULTI_POINT_10},
        {10, 2, XPE_CALIB_MULTI_POINT_10},
        { 3, 3, XPE_CALIB_MULTI_POINT_8},
        { 4, 4, XPE_CALIB_MULTI_POINT_10},
    };
    for (const Row& r : rows) {
        SCOPED_TRACE("N=" + std::to_string(r.n) + " D=" + std::to_string(r.d));
        const Generated a = poly(XPE_CALIB_AUTO, r.n, r.d);
        const Generated e = poly(r.expected, r.n, r.d);
        ASSERT_EQ(XPE_OK, e.rc);
        ASSERT_EQ(e.rc, a.rc);
        EXPECT_EQ(e.payload, a.payload);
        EXPECT_EQ(static_cast<int>(r.expected), a.qualityMode);
        EXPECT_EQ(e.qualityMode, a.qualityMode);
        EXPECT_NE(std::string::npos, a.json.find(ModeKey(r.expected))) << a.json;
        EXPECT_NE(std::string::npos, a.json.find(RequestedKey(XPE_CALIB_AUTO))) << a.json;
        EXPECT_NE(std::string::npos, e.json.find(RequestedKey(r.expected))) << e.json;
    }
}

TEST_F(CalibModeEnforcementTest, AutoAboveTheHardCapFailsLikeMultiPoint10) {
    const Generated a = poly(XPE_CALIB_AUTO, 11, 2);
    const Generated e = poly(XPE_CALIB_MULTI_POINT_10, 11, 2);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, e.rc);
    EXPECT_EQ(e.rc, a.rc);
}

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
    ASSERT_EQ(XPE_OK, poly(XPE_CALIB_AUTO, 6, 2).rc);
    bool seen = false;
    char msg[256];
    int32_t severity = -1;
    const int32_t count = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < count; ++i) {
        ASSERT_EQ(XPE_OK, xpe_get_pending_alert(i, msg, sizeof(msg), &severity));
        if (std::string(msg) == "XPE_CALIB_AUTO resolved to mode 3" &&
            severity == XPE_ALERT_INFO) {
            seen = true;
        }
    }
    xpe_clear_alerts();
    EXPECT_TRUE(seen);
}
