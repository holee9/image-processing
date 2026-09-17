/**
 * @file test_calib_overwrite.cpp
 * @brief Writing a calibration file over an existing one (QA-A-102, #169)
 *
 * SRS-CALIB-001 FUNC-033 (3) names the case -- "When overwriting an existing
 * calibration file, system shall compute and log ..." -- and the field
 * recalibration flow (CONCEPT-DIAGRAMS.md "VALIDATE --> OVERWRITE: 새 파일로
 * 덮어쓰기") ends in it. The writer documents an atomic replace: write
 * <path>.tmp, then rename onto <path> (xcal_writer.hpp).
 *
 * Until QA-A-102 the rename was std::rename, which on Windows refuses an
 * existing destination, so every second write to the same path returned
 * XPE_ERR_IO_FAILED (-9) and left the old file in place.
 *
 * Each case writes twice to one path with different content and checks that
 * the second call succeeds, the file holds the second content, and no .tmp is
 * left behind. All four file-writing entry points go through write_xcal_file.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xpe/preprocess/xcal_format.h"
#include "fixtures/make_xcal.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr uint32_t W = 8;
constexpr uint32_t H = 8;
constexpr size_t   N = static_cast<size_t>(W) * H;

class CalibOverwriteTest : public ::testing::Test {
protected:
    fs::path dir;

    void SetUp() override {
        (void)xpe_preprocess_init(nullptr);
        dir = fs::temp_directory_path() / "xpe_calib_overwrite";
        fs::remove_all(dir);
        fs::create_directories(dir);
    }

    void TearDown() override {
        xpe_preprocess_shutdown();
        fs::remove_all(dir);
    }

    std::string path(const std::string& name) const { return (dir / name).string(); }

    // First payload float of an XCal file (all payloads here are float32 or
    // uint8; for uint8 the caller reads the first byte instead).
    static std::vector<uint8_t> payload(const std::string& p) {
        std::ifstream f(p, std::ios::binary);
        EXPECT_TRUE(f.is_open()) << p;
        XCalFileHeader hdr{};
        f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
        f.seekg(static_cast<std::streamoff>(sizeof(hdr) + hdr.config_json_len));
        std::vector<uint8_t> out(static_cast<size_t>(hdr.payload_len));
        if (!out.empty()) {
            f.read(reinterpret_cast<char*>(out.data()),
                   static_cast<std::streamsize>(out.size()));
        }
        return out;
    }
    static float firstFloat(const std::vector<uint8_t>& b) {
        float v = 0.0f;
        if (b.size() >= sizeof(float)) std::memcpy(&v, b.data(), sizeof(float));
        return v;
    }

    std::vector<std::vector<uint16_t>> store;
    std::vector<XpeImageBuffer> frames(uint16_t value, int count = 4) {
        store.assign(static_cast<size_t>(count), std::vector<uint16_t>(N, value));
        std::vector<XpeImageBuffer> out(static_cast<size_t>(count));
        for (int i = 0; i < count; ++i) {
            XpeImageBuffer& b = out[static_cast<size_t>(i)];
            b.data = store[static_cast<size_t>(i)].data();
            b.width = W; b.height = H;
            b.bitsAllocated = 16; b.bitsStored = 16;
            b.format = XPE_PIXEL_UINT16;
            b.dataSize = static_cast<uint32_t>(N * sizeof(uint16_t));
        }
        return out;
    }

    void expectNoTmp(const std::string& p) {
        EXPECT_FALSE(fs::exists(p + ".tmp")) << p << ".tmp left behind";
    }
};

}  // namespace

TEST_F(CalibOverwriteTest, GenerateOffsetReplacesAnExistingFile) {
    const std::string out = path("offset.xcal");
    auto a = frames(100);
    ASSERT_EQ(XPE_OK, xpe_calib_generate_offset(a.data(), 4, 100.0f, 25.0f, out.c_str(), nullptr));
    const float first = firstFloat(payload(out));
    auto b = frames(200);
    ASSERT_EQ(XPE_OK, xpe_calib_generate_offset(b.data(), 4, 100.0f, 25.0f, out.c_str(), nullptr));
    const float second = firstFloat(payload(out));
    EXPECT_FLOAT_EQ(100.0f, first);
    EXPECT_FLOAT_EQ(200.0f, second);
    expectNoTmp(out);
}

TEST_F(CalibOverwriteTest, GenerateGainReplacesAnExistingFile) {
    const std::string out = path("gain.xcal");
    auto a = frames(1000);
    ASSERT_EQ(XPE_OK, xpe_calib_generate_gain(a.data(), 4, nullptr, out.c_str(), nullptr));
    const std::vector<uint8_t> first = payload(out);
    auto b = frames(3000);
    ASSERT_EQ(XPE_OK, xpe_calib_generate_gain(b.data(), 4, nullptr, out.c_str(),
                                              "{\"second\":1}"));
    // A uniform flat normalises to 1.0 either way, so the payload is the same;
    // the config JSON tells the two files apart.
    std::ifstream f(out, std::ios::binary);
    XCalFileHeader hdr{};
    f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
    std::string json(static_cast<size_t>(hdr.config_json_len), '\0');
    f.read(&json[0], static_cast<std::streamsize>(json.size()));
    EXPECT_NE(std::string::npos, json.find("\"second\":1")) << json;
    EXPECT_EQ(first.size(), payload(out).size());
    expectNoTmp(out);
}

TEST_F(CalibOverwriteTest, GenerateGainPolynomialReplacesAnExistingFile) {
    std::vector<std::string> lv;
    std::vector<const char*> ptrs;
    std::vector<double> doses;
    for (int i = 0; i < 3; ++i) {
        lv.push_back(path("lvl" + std::to_string(i) + ".xcal"));
        ASSERT_EQ(XPE_OK, MakeGainXCal(lv.back().c_str(), W, H, 1.0f + 0.5f * i));
        doses.push_back(1.0 + i);
    }
    for (const auto& s : lv) ptrs.push_back(s.c_str());
    const std::string out = path("poly.xcal");
    ASSERT_EQ(XPE_OK, xpe_calib_generate_gain_polynomial(ptrs.data(), doses.data(), 3, 1, out.c_str()));
    const size_t firstSize = payload(out).size();
    ASSERT_EQ(XPE_OK, xpe_calib_generate_gain_polynomial(ptrs.data(), doses.data(), 3, 2, out.c_str()));
    // degree 1 -> 2 coefficients per pixel, degree 2 -> 3.
    EXPECT_EQ(2u * N * sizeof(float), firstSize);
    EXPECT_EQ(3u * N * sizeof(float), payload(out).size());
    expectNoTmp(out);
}

TEST_F(CalibOverwriteTest, CalibSaveReplacesAnExistingFile) {
    const std::string src1 = path("src1.xcal");
    const std::string src2 = path("src2.xcal");
    const std::string out  = path("saved.xcal");
    ASSERT_EQ(XPE_OK, MakeOffsetXCal(src1.c_str(), W, H, 1.5f));
    ASSERT_EQ(XPE_OK, MakeOffsetXCal(src2.c_str(), W, H, 7.5f));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(src1.c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_save(out.c_str(), "offset", 0));
    const float first = firstFloat(payload(out));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(src2.c_str()));
    ASSERT_EQ(XPE_OK, xpe_calib_save(out.c_str(), "offset", 0));
    EXPECT_FLOAT_EQ(1.5f, first);
    EXPECT_FLOAT_EQ(7.5f, firstFloat(payload(out)));
    expectNoTmp(out);
}
