/**
 * @file test_defect_lines.cpp
 * @brief Long runs of masked pixels in a version-1 defect map are line defects: interpolated across the line
 *        (QA-A-296, Refs #278).
 *
 * A run of at least 8 masked pixels along a row is interpolated linearly from the nearest valid pixel above and below
 * (fractions kept, no flooring); everything else -- isolated pixels, short runs, clusters -- is corrected as before.
 * Everything is off by default [QA-A-296 D2]; rows are on with "rows"; columns (key `defectLineInterpolation`: "off" | "rows" | "rows_cols"). Each case that asserts a
 * value has a falsification arm: the same frame with the interpolation off, which must give something else.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr uint32_t W = 48, H = 40;
constexpr size_t   N = static_cast<size_t>(W) * H;

float valueAt(uint32_t x, uint32_t y) {   // the frame: a ramp with a small texture, integers
    return static_cast<float>(1000u + 7u * x + 13u * y + ((x * 31u + y * 17u) % 11u));
}

class DefectLineTest : public ::testing::Test {
protected:
    fs::path dir;
    std::vector<uint8_t> mask;
    std::vector<float> storage;
    XpeImageBuffer img{};
    XpeImageMetadata meta{};

    void SetUp() override {
        (void)xpe_preprocess_init(nullptr);
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        dir = fs::temp_directory_path() / "xpe_defect_lines_test";
        fs::remove_all(dir);
        fs::create_directories(dir);
        mask.assign(N, 0);
        writePlane("offset.xcal", XCAL_TYPE_OFFSET);
        writePlane("gain.xcal", XCAL_TYPE_GAIN);
        xpe_clear_alerts();
    }

    void TearDown() override {
        xpe_clear_alerts();
        xpe_preprocess_shutdown();
        fs::remove_all(dir);
    }

    void writePlane(const char* name, XCalType type) {
        const std::vector<float> data(N, type == XCAL_TYPE_OFFSET ? 0.0f : 1.0f);
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(type);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width = W; hdr.height = H;
        hdr.payload_len = data.size() * sizeof(float);
        ASSERT_EQ(XPE_OK, write_xcal_file((dir / name).string().c_str(), hdr, nullptr, 0,
                                          reinterpret_cast<const uint8_t*>(data.data()), hdr.payload_len));
    }

    void writeMask() {
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(XCAL_TYPE_DEFECT);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_UINT8_MASK);
        hdr.width = W; hdr.height = H;
        hdr.payload_len = N;
        ASSERT_EQ(XPE_OK, write_xcal_file((dir / "defect.xcal").string().c_str(), hdr, nullptr, 0, mask.data(), N));
    }

    void setRun(uint32_t y, uint32_t x0, uint32_t x1) { for (uint32_t x = x0; x <= x1; ++x) mask[y * W + x] = 1; }
    void setCol(uint32_t x, uint32_t y0, uint32_t y1) { for (uint32_t y = y0; y <= y1; ++y) mask[y * W + x] = 1; }

    // runs the defect stage alone (the gain map is ones and bypassed) on the frame valueAt(); returns the float frame
    std::vector<float> run(const std::string& lineKey, XpeErrorCode* rc = nullptr) {
        writeMask();
        storage.assign(N, 0.0f);
        auto* u = reinterpret_cast<uint16_t*>(storage.data());
        for (uint32_t y = 0; y < H; ++y)
            for (uint32_t x = 0; x < W; ++x) u[y * W + x] = static_cast<uint16_t>(valueAt(x, y));
        img = XpeImageBuffer{};
        img.data = storage.data();
        img.width = W; img.height = H;
        img.bitsAllocated = 16; img.bitsStored = 16;
        img.format = XPE_PIXEL_UINT16;
        img.dataSize = N * sizeof(float);
        meta = XpeImageMetadata{};
        const std::string cfg = std::string("{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassOffset\":true,"
                                            "\"bypassNonlinearity\":true,\"bypassBinning\":true,\"bypassGhost\":true,"
                                            "\"bypassGain\":true,\"saturationLevel\":\"off\"") + lineKey + "}";
        const std::string d = dir.string();
        const XpeErrorCode r = xpe_preprocess_pipeline(&img, &meta, d.c_str(), nullptr, cfg.c_str());
        if (rc) *rc = r; else EXPECT_EQ(XPE_OK, r);
        return storage;
    }
};

const char* const kRows = ",\"defectLineInterpolation\":\"rows\"";

bool sameBits(float a, float b) { return std::memcmp(&a, &b, sizeof(float)) == 0; }

float lerp(float a, float b, double posA, double posB, double at) {
    return static_cast<float>(static_cast<double>(a) + (static_cast<double>(b) - static_cast<double>(a)) * (at - posA) / (posB - posA));
}

}  // namespace

TEST_F(DefectLineTest, AOneRowRunIsInterpolatedLinearlyAcrossItWithTheFractionKept) {
    setRun(10, 5, 30);
    const auto out = run(kRows);
    bool sawHalf = false;
    for (uint32_t x = 5; x <= 30; ++x) {
        const float expect = lerp(valueAt(x, 9), valueAt(x, 11), 9, 11, 10);
        ASSERT_TRUE(sameBits(expect, out[10 * W + x])) << "x " << x << " got " << out[10 * W + x] << " want " << expect;
        if (expect != std::floor(expect)) sawHalf = true;
    }
    EXPECT_TRUE(sawHalf) << "the frame must contain a case where the mean is not an integer, or 'no flooring' is untested";

    const auto off = run(",\"defectLineInterpolation\":\"off\"");   // falsification: the interpolation off gives something else
    int differs = 0;
    for (uint32_t x = 5; x <= 30; ++x) differs += sameBits(off[10 * W + x], lerp(valueAt(x, 9), valueAt(x, 11), 9, 11, 10)) ? 0 : 1;
    EXPECT_GT(differs, 0);
}

TEST_F(DefectLineTest, AThreeRowLineIsInterpolatedBetweenTheRowsOutsideIt) {
    setRun(10, 5, 30); setRun(11, 5, 30); setRun(12, 5, 30);
    const auto out = run(kRows);
    for (uint32_t y = 10; y <= 12; ++y)
        for (uint32_t x = 5; x <= 30; ++x) {
            const float expect = lerp(valueAt(x, 9), valueAt(x, 13), 9, 13, y);
            ASSERT_TRUE(sameBits(expect, out[y * W + x])) << "x " << x << " y " << y;
        }
}

TEST_F(DefectLineTest, EverythingThatIsNotALongRunIsCorrectedAsBefore) {
    setRun(10, 5, 30);                  // the line
    setRun(20, 5, 11);                  // a run of 7: below the minimum
    mask[25 * W + 25] = 1;              // an isolated pixel
    mask[30 * W + 30] = mask[30 * W + 31] = mask[31 * W + 30] = mask[31 * W + 31] = 1;   // a 2x2 cluster
    const auto withLines = run(kRows);
    const auto off = run(",\"defectLineInterpolation\":\"off\"");
    for (size_t i = 0; i < N; ++i) {
        const uint32_t y = static_cast<uint32_t>(i / W);
        if (y == 10) continue;
        ASSERT_TRUE(sameBits(withLines[i], off[i])) << "pixel " << i << " (row " << y << ") differs between lines on and off";
    }
    int changed = 0;
    for (uint32_t x = 5; x <= 30; ++x) changed += sameBits(withLines[10 * W + x], off[10 * W + x]) ? 0 : 1;
    EXPECT_GT(changed, 0) << "falsification: the line row itself does change";
}

TEST_F(DefectLineTest, ARunAlongTheTopEdgeCopiesTheNearestValidRow) {
    setRun(0, 5, 30);
    const auto out = run(kRows);
    for (uint32_t x = 5; x <= 30; ++x) ASSERT_TRUE(sameBits(valueAt(x, 1), out[x])) << x;
}

TEST_F(DefectLineTest, ColumnsAreOffByDefaultAndOnWithRowsCols) {
    setCol(15, 5, 30);
    const auto def = run("");
    const auto off = run(",\"defectLineInterpolation\":\"off\"");
    for (size_t i = 0; i < N; ++i) ASSERT_TRUE(sameBits(def[i], off[i])) << "default must leave a column run to the old path: pixel " << i;
    const auto cols = run(",\"defectLineInterpolation\":\"rows_cols\"");
    int differs = 0;
    for (uint32_t y = 5; y <= 30; ++y) {
        const float expect = lerp(valueAt(14, y), valueAt(16, y), 14, 16, 15);
        ASSERT_TRUE(sameBits(expect, cols[y * W + 15])) << "y " << y;
        differs += sameBits(def[y * W + 15], expect) ? 0 : 1;
    }
    EXPECT_GT(differs, 0) << "falsification: without the key the column is not interpolated across";
}

TEST_F(DefectLineTest, TheShippedDefaultIsOffRowsIncluded) {
    setRun(10, 5, 30);
    const auto def = run("");
    const auto off = run(",\"defectLineInterpolation\":\"off\"");
    for (size_t i = 0; i < N; ++i) ASSERT_TRUE(sameBits(def[i], off[i])) << i;
    const auto rows = run(kRows);   // falsification: "rows" does change the line
    int differs = 0;
    for (uint32_t x = 5; x <= 30; ++x) differs += sameBits(rows[10 * W + x], def[10 * W + x]) ? 0 : 1;
    EXPECT_GT(differs, 0);
}

TEST_F(DefectLineTest, AClusterTouchingALineIsTheIntendedlyAffectedSetAndStaysFinite) {
    setRun(10, 5, 30);
    mask[11 * W + 40] = 1; mask[12 * W + 40] = 1;   // apart from the line: a small cluster, unchanged by the line pass
    mask[11 * W + 6] = 1;                            // touching the line (row 10, x 6): a pixel whose cluster status the line pass changes
    const auto on = run(kRows);
    const auto off = run(",\"defectLineInterpolation\":\"off\"");
    EXPECT_TRUE(sameBits(on[11 * W + 40], off[11 * W + 40]));
    EXPECT_TRUE(sameBits(on[12 * W + 40], off[12 * W + 40]));
    for (size_t i = 0; i < N; ++i) ASSERT_TRUE(std::isfinite(on[i])) << i;
}

TEST_F(DefectLineTest, ABadKeyIsARefusal) {
    setRun(10, 5, 30);
    XpeErrorCode rc = XPE_OK;
    (void)run(",\"defectLineInterpolation\":\"diagonal\"", &rc);
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, rc);
    (void)run(",\"defectLineInterpolation\":5", &rc);
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, rc);
    (void)run(",\"defectLineInterpolation\":\"rows\"", &rc);
    EXPECT_EQ(XPE_OK, rc);
}

TEST_F(DefectLineTest, ARowAndAColumnThatCrossAreBothHandledAndNothingIsRevisited) {
    setRun(10, 5, 40); setCol(20, 3, 30);
    const auto on = run(",\"defectLineInterpolation\":\"rows_cols\"");
    for (uint32_t x = 5; x <= 40; ++x) {
        if (x == 20) continue;
        if (x >= 12 && x <= 28) {}   // near the crossing the column pixels are masked too; the row pixels still see rows 9 and 11
        const float expect = lerp(valueAt(x, 9), valueAt(x, 11), 9, 11, 10);
        ASSERT_TRUE(sameBits(expect, on[10 * W + x])) << "row pixel " << x;
    }
    for (uint32_t y = 3; y <= 30; ++y) {
        if (y == 10) continue;
        const float expect = lerp(valueAt(19, y), valueAt(21, y), 19, 21, 20);
        ASSERT_TRUE(sameBits(expect, on[y * W + 20])) << "column pixel " << y;
    }
    for (size_t i = 0; i < N; ++i) ASSERT_TRUE(std::isfinite(on[i])) << i;
}

TEST_F(DefectLineTest, ABandWiderThanTheReachIsLeftToTheOldPath) {
    for (uint32_t y = 10; y <= 30; ++y) setRun(y, 5, 30);   // 21 rows: no valid pixel within 8 of row 20
    const auto on = run(kRows);
    const auto off = run(",\"defectLineInterpolation\":\"off\"");
    for (uint32_t x = 5; x <= 30; ++x) ASSERT_TRUE(sameBits(on[20 * W + x], off[20 * W + x])) << x;   // the middle row has no pixel across: unchanged
}

TEST_F(DefectLineTest, ARunAtTheLeftEdgeColumnIsATwoSidedColumnOnlyWhenColumnsAreOn) {
    setCol(0, 5, 30);
    const auto def = run("");
    const auto off = run(",\"defectLineInterpolation\":\"off\"");
    for (size_t i = 0; i < N; ++i) ASSERT_TRUE(sameBits(def[i], off[i])) << i;
    const auto cols = run(",\"defectLineInterpolation\":\"rows_cols\"");
    for (uint32_t y = 5; y <= 30; ++y) ASSERT_TRUE(sameBits(valueAt(1, y), cols[y * W]));   // only the right side exists: its value
}
