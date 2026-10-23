/**
 * @file test_defect_xcal_v2.cpp
 * @brief XCal DEFECT file, format version 2 (QA-A-254, SRS-CALIB-FUNC-039): round trip, old files, damaged files with the reason of each refusal
 *
 * Every refusal is checked for its error code AND for the one alert that says why ("XPE_ERR_XCAL_<KIND>: ..."). The fixture drains the alert queue before and after each test.
 */

#include <gtest/gtest.h>

#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include "defect_ext.hpp"
#include "fixtures/make_xcal.hpp"
#include "xcal_reader.hpp"
#include "xcal_writer.hpp"
#include "xpe/common/xpe_common_api.h"
#include "xpe/preprocess_api.h"

namespace fs = std::filesystem;

namespace {

constexpr uint32_t kW = 64, kH = 48;

std::vector<uint8_t> readBytes(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::vector<uint8_t>((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

void writeBytes(const fs::path& p, const std::vector<uint8_t>& b) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
}

std::string alertText(int32_t i = 0) {
    char buf[1024] = {0};
    int32_t sev = -1;
    if (xpe_get_pending_alert(i, buf, sizeof buf, &sev) != XPE_OK) return "";
    return buf;
}

class DefectV2Test : public ::testing::Test {
protected:
    fs::path dir;
    std::vector<uint8_t> map, area;
    DefectExt ext;

    void SetUp() override {
        xpe_clear_alerts();
        dir = fs::temp_directory_path() / ("xpe_defect_v2_" + std::to_string(::testing::UnitTest::GetInstance()->random_seed()) + "_" + ::testing::UnitTest::GetInstance()->current_test_info()->name());
        fs::remove_all(dir);
        fs::create_directories(dir);
        map.assign(static_cast<size_t>(kW) * kH, 0);
        area.assign(map.size(), 1);
        for (uint32_t y = 0; y < kH; ++y)
            for (uint32_t x = 0; x < kW; ++x)
                if (x < 2 || x == kW - 1 || y == 0 || y == kH - 1) area[static_cast<size_t>(y) * kW + x] = 0;
        map[10 * kW + 10] = 1;
        map[11 * kW + 30] = 2;
        map[12 * kW + 40] = 3;
        for (uint32_t x = 8; x < 20; ++x) map[20 * kW + x] = 1;
        for (uint32_t x = 30; x < 40; ++x) map[20 * kW + x] = 1;
        DefectExtLine row;
        row.orientation = 0; row.index = 20; row.width = 1; row.start = 8; row.end = 40; row.causes = 0x01 | 0x08; row.flags = 0; row.correction = XPE_DEFECT_LINE_CORRECT_INTERPOLATE;
        row.strength = 12.5f; row.persistence = -1.0f;
        row.segments = {{20, 8, 20}, {20, 30, 40}};
        DefectExtLine col;
        col.orientation = 1; col.index = 50; col.width = 2; col.start = 4; col.end = 30; col.causes = 0x04; col.flags = 0x01; col.correction = XPE_DEFECT_LINE_CORRECT_INTERPOLATE;
        col.strength = 5.5f; col.persistence = 0.75f;
        col.segments = {{50, 4, 30}, {51, 10, 20}};
        DefectExtLine cand;
        cand.orientation = 0; cand.index = 5; cand.width = 1; cand.start = 0; cand.end = kW; cand.causes = 0x02; cand.reason = 1; cand.correction = 0; cand.strength = 3.0f; cand.persistence = -1.0f;
        ext.lines = {row, col};
        ext.candidates = {cand};
        ext.blobs = {{3, 3, 5, 4, 5}};
    }

    void TearDown() override {
        xpe_clear_alerts();
        std::error_code ec;
        fs::remove_all(dir, ec);
    }

    DefectXcalWrite job() const {
        DefectXcalWrite w;
        w.width = kW;
        w.height = kH;
        w.map = map.data();
        w.area = area.data();
        w.ext = &ext;
        w.session_id = "defect-v2-test";
        return w;
    }

    /** A valid version-2 file with a hand-built payload: lets a test damage one part and keep the hash right. */
    XpeErrorCode handmade(const fs::path& p, const std::vector<uint8_t>& payload, const std::string& config, uint32_t version = XCAL_VERSION_DEFECT_EXT) {
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = version;
        hdr.type = XCAL_TYPE_DEFECT;
        hdr.pixel_format = XCAL_FMT_UINT8_MASK;
        hdr.width = kW;
        hdr.height = kH;
        std::memcpy(hdr.session_id, "hand", 4);
        return write_xcal_file(p.string().c_str(), hdr, reinterpret_cast<const uint8_t*>(config.data()), config.size(), payload.data(), payload.size());
    }

    std::vector<uint8_t> goodExtBytes() {
        std::vector<uint8_t> e;
        std::string why;
        EXPECT_TRUE(defect_ext_encode(ext, kW, kH, &e, &why)) << why;
        return e;
    }

    std::vector<uint8_t> payloadOf(const std::vector<uint8_t>& extBytes) const {
        std::vector<uint8_t> p(map);
        p.insert(p.end(), area.begin(), area.end());
        p.insert(p.end(), extBytes.begin(), extBytes.end());
        return p;
    }

    static std::string cfgFor(size_t extLen) { return "{\"defect_format\":2,\"defect_ext_len\":" + std::to_string(extLen) + "}"; }

    void expectRefused(const fs::path& p, XpeErrorCode code, const char* kind, const char* wording) {
        DefectXcal d;
        xpe_clear_alerts();
        EXPECT_EQ(code, read_defect_xcal(p.string().c_str(), false, &d)) << kind;
        ASSERT_EQ(1, xpe_get_pending_alert_count()) << "one alert says why: " << alertText();
        const std::string a = alertText();
        EXPECT_NE(std::string::npos, a.find(std::string("XPE_ERR_XCAL_") + kind)) << a;
        EXPECT_NE(std::string::npos, a.find(wording)) << a;
        EXPECT_EQ(0u, d.map.size()) << "nothing is handed out for a refused file";
        xpe_clear_alerts();
    }
};

void expectSameLine(const DefectExtLine& a, const DefectExtLine& b) {
    EXPECT_EQ(a.orientation, b.orientation);
    EXPECT_EQ(a.index, b.index);
    EXPECT_EQ(a.width, b.width);
    EXPECT_EQ(a.start, b.start);
    EXPECT_EQ(a.end, b.end);
    EXPECT_EQ(a.causes, b.causes);
    EXPECT_EQ(a.flags, b.flags);
    EXPECT_EQ(a.reason, b.reason);
    EXPECT_EQ(a.correction, b.correction);
    EXPECT_EQ(a.strength, b.strength);
    EXPECT_EQ(a.persistence, b.persistence);
    ASSERT_EQ(a.segments.size(), b.segments.size());
    for (size_t k = 0; k < a.segments.size(); ++k) {
        EXPECT_EQ(a.segments[k].line, b.segments[k].line);
        EXPECT_EQ(a.segments[k].start, b.segments[k].start);
        EXPECT_EQ(a.segments[k].end, b.segments[k].end);
    }
}

} // namespace

TEST_F(DefectV2Test, ARoundTripKeepsEveryLayerExactlyCompressedOrNot) {
    for (const bool compress : {true, false}) {
        const fs::path p = dir / (compress ? "c.xcal" : "u.xcal");
        DefectXcalWrite w = job();
        w.compress = compress;
        ASSERT_EQ(XPE_OK, write_defect_xcal(p.string().c_str(), w));
        DefectXcal d;
        ASSERT_EQ(XPE_OK, read_defect_xcal(p.string().c_str(), false, &d)) << alertText();
        EXPECT_EQ(2u, d.format_version);
        EXPECT_EQ(kW, d.width);
        EXPECT_EQ(kH, d.height);
        EXPECT_EQ(map, d.map) << "the defect plane is the version-1 payload, byte for byte (pixel codes 1, 2, 3 included)";
        EXPECT_TRUE(d.has_area);
        EXPECT_EQ(area, d.area);
        EXPECT_TRUE(d.has_ext);
        ASSERT_EQ(ext.lines.size(), d.ext.lines.size());
        for (size_t k = 0; k < ext.lines.size(); ++k) expectSameLine(ext.lines[k], d.ext.lines[k]);
        ASSERT_EQ(1u, d.ext.candidates.size());
        expectSameLine(ext.candidates[0], d.ext.candidates[0]);
        ASSERT_EQ(1u, d.ext.blobs.size());
        EXPECT_EQ(5u, d.ext.blobs[0].size);
        EXPECT_EQ(4u, d.ext.blobs[0].y1);
        EXPECT_EQ("defect-v2-test", d.session_id);
        EXPECT_NE(std::string::npos, d.config_json.find("\"defect_ext_len\":"));
    }
}

TEST_F(DefectV2Test, TheCandidatesAreADiagnosticAppendixNotPartOfTheDefectLayer) {
    // the defect layer is the map plane and the confirmed lines; a candidate is listed apart and paints nothing
    const fs::path p = dir / "a.xcal";
    ASSERT_EQ(XPE_OK, write_defect_xcal(p.string().c_str(), job()));
    DefectXcal d;
    ASSERT_EQ(XPE_OK, read_defect_xcal(p.string().c_str(), false, &d));
    EXPECT_EQ(0, d.map[5 * kW + 20]) << "the candidate on row 5 is not in the defect plane";
    EXPECT_EQ(1u, d.ext.candidates.size());
    EXPECT_EQ(1u, d.ext.candidates[0].reason);
    EXPECT_TRUE(d.ext.candidates[0].segments.empty());
}

TEST_F(DefectV2Test, AVersionOneFileStillReadsAndHasNoAreaOrExtension) {
    const fs::path p = dir / "old.xcal";
    ASSERT_EQ(XPE_OK, MakeDefectXCal(p.string().c_str(), kW, kH));
    DefectXcal d;
    ASSERT_EQ(XPE_OK, read_defect_xcal(p.string().c_str(), false, &d)) << alertText();
    EXPECT_EQ(1u, d.format_version);
    EXPECT_FALSE(d.has_area);
    EXPECT_TRUE(d.area.empty());
    EXPECT_FALSE(d.has_ext);
    EXPECT_EQ(static_cast<size_t>(kW) * kH, d.map.size());
    EXPECT_EQ(0, xpe_get_pending_alert_count());
}

TEST_F(DefectV2Test, TheRuntimeLoaderTakesBothVersionsAndRefusesADamagedOne) {
    const fs::path v1 = dir / "v1.xcal", v2 = dir / "v2.xcal", bad = dir / "bad.xcal";
    ASSERT_EQ(XPE_OK, MakeDefectXCal(v1.string().c_str(), kW, kH));
    ASSERT_EQ(XPE_OK, write_defect_xcal(v2.string().c_str(), job()));
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_map(v1.string().c_str())) << alertText();
    EXPECT_EQ(XPE_OK, xpe_calib_load_defect_map(v2.string().c_str())) << alertText();
    std::vector<uint8_t> e = goodExtBytes();
    e[32 + 4] = 200;   // the index of line 0: far outside the frame
    ASSERT_EQ(XPE_OK, handmade(bad, payloadOf(e), cfgFor(e.size())));
    xpe_clear_alerts();
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_calib_load_defect_map(bad.string().c_str()));
    EXPECT_NE(std::string::npos, alertText().find("XPE_ERR_XCAL_EXTENSION"));
    xpe_clear_alerts();
    xpe_preprocess_shutdown();
}

TEST_F(DefectV2Test, ATruncatedFileIsRefusedAndTheAlertSaysItEndsEarly) {
    const fs::path p = dir / "t.xcal";
    DefectXcalWrite w = job();
    w.compress = false;
    ASSERT_EQ(XPE_OK, write_defect_xcal(p.string().c_str(), w));
    std::vector<uint8_t> b = readBytes(p);
    b.resize(b.size() / 2);
    writeBytes(p, b);
    expectRefused(p, XPE_ERR_IO_FAILED, "TRUNCATED", "ends early");
    // shorter than the header
    b.resize(100);
    writeBytes(p, b);
    expectRefused(p, XPE_ERR_IO_FAILED, "TRUNCATED", "shorter than the 152-byte header");
    // the runtime loader says so too
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    xpe_clear_alerts();
    EXPECT_EQ(XPE_ERR_IO_FAILED, xpe_calib_load_defect_map(p.string().c_str()));
    EXPECT_NE(std::string::npos, alertText().find("XPE_ERR_XCAL_TRUNCATED"));
    xpe_clear_alerts();
    xpe_preprocess_shutdown();
}

TEST_F(DefectV2Test, ALineRecordOutsideTheFrameIsRefusedWithTheRecordNamed) {
    std::vector<uint8_t> e = goodExtBytes();
    e[32 + 4] = 200;   // line 0: index 200 on a frame of 48 rows
    const fs::path p = dir / "range.xcal";
    ASSERT_EQ(XPE_OK, handmade(p, payloadOf(e), cfgFor(e.size())));
    expectRefused(p, XPE_ERR_CONFIG_INVALID, "EXTENSION", "line 0");
}

TEST_F(DefectV2Test, ASegmentOutsideItsLineIsRefused) {
    DefectExt bad = ext;
    bad.lines[0].segments[1].end = 41;   // beyond the record's own range 8..40
    std::vector<uint8_t> e;
    std::string why;
    EXPECT_FALSE(defect_ext_encode(bad, kW, kH, &e, &why)) << "the writer refuses it";
    EXPECT_NE(std::string::npos, why.find("segment 1")) << why;
    // and a file that carries it anyway: patch the encoded block (segment table at the end, 12 bytes per segment; line 0's second segment is the 2nd entry)
    std::vector<uint8_t> good = goodExtBytes();
    const size_t segBase = good.size() - 4 * 12;   // four segments in total
    good[segBase + 12 + 8] = 90;                   // end of segment 1 of line 0 -> 90, past the 64-pixel line
    const fs::path p = dir / "seg.xcal";
    ASSERT_EQ(XPE_OK, handmade(p, payloadOf(good), cfgFor(good.size())));
    expectRefused(p, XPE_ERR_CONFIG_INVALID, "EXTENSION", "segment 1");
}

TEST_F(DefectV2Test, AnUnsupportedVersionNumberIsRefusedAndNamed) {
    const fs::path p = dir / "v.xcal";
    ASSERT_EQ(XPE_OK, write_defect_xcal(p.string().c_str(), job()));
    std::vector<uint8_t> b = readBytes(p);
    b[4] = 3;   // the version field (little endian): 3, which no reader knows. The header is not part of the hash, so the file is otherwise intact
    writeBytes(p, b);
    expectRefused(p, XPE_ERR_CONFIG_INVALID, "VERSION", "file version 3");
    b[4] = 0;
    writeBytes(p, b);
    expectRefused(p, XPE_ERR_CONFIG_INVALID, "VERSION", "file version 0");
}

TEST_F(DefectV2Test, VersionTwoBelongsToDefectFilesOnly) {
    const fs::path p = dir / "o.xcal";
    ASSERT_EQ(XPE_OK, MakeOffsetXCal(p.string().c_str(), kW, kH));
    std::vector<uint8_t> b = readBytes(p);
    b[4] = 2;
    writeBytes(p, b);
    XCalFileHeader hdr;
    std::vector<uint8_t> cfg, pay;
    xpe_clear_alerts();
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, read_xcal_file(p.string().c_str(), hdr, cfg, pay, false, -1));
    EXPECT_NE(std::string::npos, alertText().find("XPE_ERR_XCAL_VERSION"));
}

TEST_F(DefectV2Test, TheImagingAreaPlaneHoldsZeroAndOneOnly) {
    std::vector<uint8_t> e = goodExtBytes();
    std::vector<uint8_t> payload = payloadOf(e);
    payload[map.size() + 100] = 2;
    const fs::path p = dir / "area.xcal";
    ASSERT_EQ(XPE_OK, handmade(p, payload, cfgFor(e.size())));
    expectRefused(p, XPE_ERR_CONFIG_INVALID, "AREA", "value 2 at pixel 100");
    DefectXcalWrite w = job();
    std::vector<uint8_t> badArea = area;
    badArea[7] = 5;
    w.area = badArea.data();
    xpe_clear_alerts();
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, write_defect_xcal((dir / "w.xcal").string().c_str(), w));
    EXPECT_NE(std::string::npos, alertText().find("XPE_ERR_XCAL_AREA"));
}

TEST_F(DefectV2Test, TheDeclaredExtensionLengthMustMatchThePayload) {
    std::vector<uint8_t> e = goodExtBytes();
    const fs::path longer = dir / "long.xcal", missing = dir / "missing.xcal";
    ASSERT_EQ(XPE_OK, handmade(longer, payloadOf(e), cfgFor(e.size() + 8)));
    expectRefused(longer, XPE_ERR_CONFIG_INVALID, "LAYOUT", "defect_ext_len");
    ASSERT_EQ(XPE_OK, handmade(missing, payloadOf(e), "{\"defect_format\":2}"));
    expectRefused(missing, XPE_ERR_CONFIG_INVALID, "LAYOUT", "must give defect_ext_len");
}

TEST_F(DefectV2Test, ADamagedExtensionBlockNamesWhatIsWrongWithIt) {
    const fs::path p = dir / "x.xcal";
    std::vector<uint8_t> e = goodExtBytes();
    std::vector<uint8_t> magic = e;
    magic[0] = 'Y';
    ASSERT_EQ(XPE_OK, handmade(p, payloadOf(magic), cfgFor(magic.size())));
    expectRefused(p, XPE_ERR_CONFIG_INVALID, "EXTENSION", "does not start with XDEX");
    std::vector<uint8_t> reserved = e;
    reserved[24] = 1;
    ASSERT_EQ(XPE_OK, handmade(p, payloadOf(reserved), cfgFor(reserved.size())));
    expectRefused(p, XPE_ERR_CONFIG_INVALID, "EXTENSION", "reserved fields");
    std::vector<uint8_t> shortBlock(e.begin(), e.end() - 5);
    ASSERT_EQ(XPE_OK, handmade(p, payloadOf(shortBlock), cfgFor(shortBlock.size())));
    expectRefused(p, XPE_ERR_CONFIG_INVALID, "EXTENSION", "truncated or padded");
    std::vector<uint8_t> format = e;
    format[4] = 9;
    ASSERT_EQ(XPE_OK, handmade(p, payloadOf(format), cfgFor(format.size())));
    expectRefused(p, XPE_ERR_CONFIG_INVALID, "EXTENSION", "format 9");
}

TEST_F(DefectV2Test, ATamperedPayloadFailsTheHashLikeEveryOtherFile) {
    const fs::path p = dir / "h.xcal";
    DefectXcalWrite w = job();
    w.compress = false;
    ASSERT_EQ(XPE_OK, write_defect_xcal(p.string().c_str(), w));
    std::vector<uint8_t> b = readBytes(p);
    b[b.size() - 3] ^= 0x01;
    writeBytes(p, b);
    DefectXcal d;
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, read_defect_xcal(p.string().c_str(), false, &d));
    xpe_clear_alerts();
}

TEST_F(DefectV2Test, TheWriterRefusesARecordOutsideTheGeometryAndWritesNothing) {
    DefectXcalWrite w = job();
    DefectExt bad = ext;
    bad.blobs[0].x1 = 99;
    w.ext = &bad;
    const fs::path p = dir / "w.xcal";
    xpe_clear_alerts();
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, write_defect_xcal(p.string().c_str(), w));
    EXPECT_NE(std::string::npos, alertText().find("XPE_ERR_XCAL_EXTENSION"));
    EXPECT_NE(std::string::npos, alertText().find("blob 0"));
    EXPECT_FALSE(fs::exists(p));
    xpe_clear_alerts();
}

TEST_F(DefectV2Test, AFileWithoutRecordsCarriesNoExtensionBlock) {
    DefectXcalWrite w = job();
    DefectExt none;
    w.ext = &none;
    const fs::path p = dir / "e.xcal";
    ASSERT_EQ(XPE_OK, write_defect_xcal(p.string().c_str(), w));
    DefectXcal d;
    ASSERT_EQ(XPE_OK, read_defect_xcal(p.string().c_str(), false, &d));
    EXPECT_FALSE(d.has_ext);
    EXPECT_TRUE(d.has_area);
    EXPECT_NE(std::string::npos, d.config_json.find("\"defect_ext_len\":0"));
}
