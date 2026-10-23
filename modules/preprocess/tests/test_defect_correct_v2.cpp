/**
 * @file test_defect_correct_v2.cpp
 * @brief QA-A-255: defect correction, second edition (SRS-CALIB-FUNC-039 correction part)
 *
 * A version-1 map takes the path it always took (the bytes below are those of the code before QA-A-255); a version-2 map brings the lines, the blobs and the imaging area, and the
 * correction uses them: a line is interpolated from the nearest valid pixels across it, a blob is filled from the outside inward, the pixels outside the imaging area are neither a source
 * nor corrected.
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"
#include "defect_ext.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

constexpr uint32_t W = 96, H = 80;

uint64_t fnv1a(const void* data, size_t bytes) {
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint64_t       h = 1469598103934665603ull;
    for (size_t i = 0; i < bytes; ++i) {
        h ^= p[i];
        h *= 1099511628211ull;
    }
    return h;
}

std::vector<float> texturedFrame() {
    std::vector<float> f(static_cast<size_t>(W) * H);
    for (uint32_t y = 0; y < H; ++y)
        for (uint32_t x = 0; x < W; ++x) f[y * W + x] = 1000.0f + 3.0f * static_cast<float>(x) + 2.0f * static_cast<float>(y) + static_cast<float>((x * 7 + y * 13) % 11);
    return f;
}

// a plane with no texture: a pure ramp, on which linear interpolation across a line is exact
std::vector<float> rampFrame() {
    std::vector<float> f(static_cast<size_t>(W) * H);
    for (uint32_t y = 0; y < H; ++y)
        for (uint32_t x = 0; x < W; ++x) f[y * W + x] = 1000.0f + 3.0f * static_cast<float>(x) + 2.0f * static_cast<float>(y);
    return f;
}

// the version-1 golden map: isolated pixels, a pair, an L, a 4 x 4 block, a row line (1 wide), a column line (2 wide), a corner pixel and an edge pixel
std::vector<uint8_t> goldenMap() {
    std::vector<uint8_t> m(static_cast<size_t>(W) * H, 0);
    auto set = [&](uint32_t x, uint32_t y) { m[y * W + x] = 1; };
    set(10, 10);
    set(50, 40);
    set(20, 20);
    set(21, 20);
    set(40, 30);
    set(41, 30);
    set(41, 31);
    for (uint32_t y = 50; y < 54; ++y)
        for (uint32_t x = 30; x < 34; ++x) set(x, y);
    for (uint32_t x = 5; x < 91; ++x) set(x, 65);
    for (uint32_t y = 5; y < 60; ++y) {
        set(70, y);
        set(71, y);
    }
    set(0, 0);
    set(95, 40);
    return m;
}

struct LineSpec {
    uint32_t orientation;   // 0 row, 1 column
    uint32_t index;
    uint32_t width;
    uint32_t start, end;    // along the line, end exclusive; the same stretch on every line of the group
    int      reject = 0;    // 1 = the correction method of the group is REJECT whatever its width
};

class DefectCorrectV2Test : public ::testing::Test {
protected:
    const char* path = "test_defect_correct_v2.xcal";
    const char* path2 = "test_defect_correct_v2_b.xcal";
    void SetUp() override { ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr)); }
    void TearDown() override {
        std::remove(path);
        std::remove(path2);
        xpe_clear_alerts();
        xpe_preprocess_shutdown();
    }

    void loadV1(const std::vector<uint8_t>& map) {
        std::remove(path);
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version      = XCAL_VERSION;
        hdr.type         = static_cast<uint32_t>(XCAL_TYPE_DEFECT);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_UINT8_MASK);
        hdr.width        = W;
        hdr.height       = H;
        hdr.payload_len  = static_cast<uint64_t>(map.size());
        ASSERT_EQ(XPE_OK, write_xcal_file(path, hdr, nullptr, 0, map.data(), hdr.payload_len));
        ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(path));
    }

    // a version-2 file: the map (the line pixels are in it, as the generator merges them), the imaging area (empty = all inside) and the lines
    void writeV2(const char* file, const std::vector<uint8_t>& map, const std::vector<uint8_t>& area, const std::vector<LineSpec>& lines) {
        std::remove(file);
        DefectExt ext;
        for (const LineSpec& l : lines) {
            DefectExtLine r;
            r.orientation = l.orientation;
            r.index = l.index;
            r.width = l.width;
            r.start = l.start;
            r.end = l.end;
            r.causes = 1u;
            r.correction = (l.width >= 3u || l.reject) ? XPE_DEFECT_LINE_CORRECT_REJECT : XPE_DEFECT_LINE_CORRECT_INTERPOLATE;
            for (uint32_t k = 0; k < l.width; ++k) r.segments.push_back({l.index + k, l.start, l.end});
            ext.lines.push_back(r);
        }
        std::vector<uint8_t> inside(map.size(), 1);
        DefectXcalWrite      w;
        w.width = W;
        w.height = H;
        w.map = map.data();
        w.area = area.empty() ? inside.data() : area.data();
        w.ext = &ext;
        w.session_id = "t";
        ASSERT_EQ(XPE_OK, write_defect_xcal(file, w));
    }

    void loadV2(const std::vector<uint8_t>& map, const std::vector<uint8_t>& area, const std::vector<LineSpec>& lines) {
        writeV2(path, map, area, lines);
        ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(path));
    }

    static XpeImageBuffer bufferOf(std::vector<float>& v) {
        XpeImageBuffer a{};
        a.data = v.data();
        a.width = W;
        a.height = H;
        a.bitsAllocated = 32;
        a.bitsStored = 32;
        a.format = XPE_PIXEL_FLOAT32;
        a.dataSize = v.size() * sizeof(float);
        return a;
    }

    std::vector<float> correct(const std::vector<float>& in, XpeDefectCorrectionReport* report = nullptr, std::vector<uint8_t>* low = nullptr) {
        std::vector<float> out(in.size(), 0.0f), src = in;
        XpeImageBuffer     a = bufferOf(src), b = bufferOf(out);
        XpeImageMetadata   meta{};
        if (!low) {
            EXPECT_EQ(XPE_OK, xpe_defect_correct_ex(&a, &b, &meta, report, nullptr));
        } else {
            low->assign(in.size(), 7);
            XpeImageBuffer l{};
            l.data = low->data();
            l.width = W;
            l.height = H;
            l.bitsAllocated = 8;
            l.bitsStored = 8;
            l.format = XPE_PIXEL_UINT8;
            l.dataSize = low->size();
            EXPECT_EQ(XPE_OK, xpe_defect_correct_ex(&a, &b, &meta, report, &l));
        }
        return out;
    }
};

TEST_F(DefectCorrectV2Test, AVersionOneMapGivesTheBytesOfTheCodeBeforeQAA255) {
    loadV1(goldenMap());
    const std::vector<float> out = correct(texturedFrame());
    EXPECT_EQ(0xfa3a2284911ddbe8ull, fnv1a(out.data(), out.size() * sizeof(float))) << "the golden value was taken from the code of 1db8fc28, before the second edition";
}

TEST_F(DefectCorrectV2Test, AVersionOneMapReportsNoLayersAndNoLowConfidence) {
    loadV1(goldenMap());
    XpeDefectCorrectionReport rep{};
    std::vector<uint8_t>      low;
    correct(texturedFrame(), &rep, &low);
    EXPECT_EQ(sizeof(XpeDefectCorrectionReport), rep.size);
    EXPECT_EQ(0u, rep.layers_used);
    EXPECT_EQ(0u, rep.low_confidence_pixels);
    EXPECT_EQ(0, std::count(low.begin(), low.end(), static_cast<uint8_t>(1)));
}

TEST_F(DefectCorrectV2Test, AVersionTwoMapOfIsolatedPixelsGivesTheBytesOfTheVersionOneMap) {
    // only single pixels: the second edition keeps FUNC-007 for them, so the two editions agree to the bit
    std::vector<uint8_t> m(static_cast<size_t>(W) * H, 0);
    m[10 * W + 10] = 1;
    m[40 * W + 50] = 1;
    m[70 * W + 90] = 1;
    loadV1(m);
    const std::vector<float> v1 = correct(texturedFrame());
    loadV2(m, {}, {});
    XpeDefectCorrectionReport rep{};
    const std::vector<float>  v2 = correct(texturedFrame(), &rep);
    EXPECT_EQ(1u, rep.layers_used);
    EXPECT_EQ(3u, rep.isolated_pixels);
    EXPECT_EQ(0, std::memcmp(v1.data(), v2.data(), v1.size() * sizeof(float)));
}

TEST_F(DefectCorrectV2Test, ARowLineIsInterpolatedFromTheLinesAboveAndBelow) {
    std::vector<uint8_t> m(static_cast<size_t>(W) * H, 0);
    for (uint32_t x = 10; x < 81; ++x) m[40 * W + x] = 1;
    std::vector<float> in = rampFrame();
    for (uint32_t x = 10; x < 81; ++x) in[40 * W + x] = 5000.0f;   // a stuck line
    loadV1(m);
    const std::vector<float> v1 = correct(in);
    loadV2(m, {}, {{0, 40, 1, 10, 81}});
    XpeDefectCorrectionReport rep{};
    const std::vector<float>  v2 = correct(in, &rep);
    const std::vector<float>  truth = rampFrame();
    double e1 = 0, e2 = 0;
    for (uint32_t x = 10; x < 81; ++x) {
        e1 = std::max(e1, static_cast<double>(std::fabs(v1[40 * W + x] - truth[40 * W + x])));
        e2 = std::max(e2, static_cast<double>(std::fabs(v2[40 * W + x] - truth[40 * W + x])));
    }
    EXPECT_LT(e2, 1e-3) << "linear interpolation across the line is exact on a ramp";
    EXPECT_GT(e1, e2) << "the first edition's median of the six neighbours is not (largest error " << e1 << ")";
    EXPECT_EQ(71u, rep.line_pixels);
    EXPECT_EQ(1u, rep.line_groups);
    EXPECT_EQ(0u, rep.low_confidence_pixels);
}

TEST_F(DefectCorrectV2Test, ATwoLineGroupIsBridgedFromItsTwoOuterNeighbours) {
    std::vector<uint8_t> m(static_cast<size_t>(W) * H, 0);
    for (uint32_t y = 40; y < 42; ++y)
        for (uint32_t x = 10; x < 81; ++x) m[y * W + x] = 1;
    std::vector<float> in = rampFrame();
    for (uint32_t y = 40; y < 42; ++y)
        for (uint32_t x = 10; x < 81; ++x) in[y * W + x] = 9000.0f;
    loadV2(m, {}, {{0, 40, 2, 10, 81}});
    XpeDefectCorrectionReport rep{};
    const std::vector<float>  out = correct(in, &rep);
    const std::vector<float>  truth = rampFrame();
    for (uint32_t y = 40; y < 42; ++y)
        for (uint32_t x = 10; x < 81; ++x) ASSERT_NEAR(truth[y * W + x], out[y * W + x], 1e-3) << x << "," << y;
    EXPECT_EQ(142u, rep.line_pixels);
    EXPECT_EQ(1u, rep.line_groups);
    EXPECT_EQ(0u, rep.low_confidence_pixels);
}

TEST_F(DefectCorrectV2Test, AColumnLineIsInterpolatedFromTheLinesAtItsSides) {
    std::vector<uint8_t> m(static_cast<size_t>(W) * H, 0);
    for (uint32_t y = 10; y < 70; ++y) m[y * W + 50] = 1;
    std::vector<float> in = rampFrame();
    for (uint32_t y = 10; y < 70; ++y) in[y * W + 50] = 0.0f;
    loadV2(m, {}, {{1, 50, 1, 10, 70}});
    const std::vector<float> out = correct(in);
    const std::vector<float> truth = rampFrame();
    for (uint32_t y = 10; y < 70; ++y) ASSERT_NEAR(truth[y * W + 50], out[y * W + 50], 1e-3) << y;
}

TEST_F(DefectCorrectV2Test, AGroupOfThreeLinesIsCorrectedAndMarkedLowConfidence) {
    std::vector<uint8_t> m(static_cast<size_t>(W) * H, 0);
    for (uint32_t y = 40; y < 43; ++y)
        for (uint32_t x = 10; x < 31; ++x) m[y * W + x] = 1;
    loadV2(m, {}, {{0, 40, 3, 10, 31}});
    XpeDefectCorrectionReport rep{};
    std::vector<uint8_t>      low;
    const std::vector<float>  out = correct(rampFrame(), &rep, &low);
    const std::vector<float>  truth = rampFrame();
    for (uint32_t y = 40; y < 43; ++y)
        for (uint32_t x = 10; x < 31; ++x) {
            EXPECT_NEAR(truth[y * W + x], out[y * W + x], 1e-3);
            EXPECT_EQ(1, low[y * W + x]) << "three lines are a panel rejection condition: the correction is marked";
        }
    EXPECT_EQ(63u, rep.low_confidence_pixels);
    EXPECT_EQ(1u, rep.low_confidence_regions);
    EXPECT_EQ(63, std::count(low.begin(), low.end(), static_cast<uint8_t>(1))) << "no pixel outside the group is marked";
    ASSERT_GE(xpe_get_pending_alert_count(), 1);
    char    msg[512] = {0};
    int32_t sev = 0;
    bool    found = false;
    for (int32_t i = 0; i < xpe_get_pending_alert_count(); ++i)
        if (xpe_get_pending_alert(i, msg, sizeof msg, &sev) == XPE_OK && std::string(msg).find("XPE_WARN_DEFECT_LOW_CONFIDENCE") != std::string::npos) found = true;
    EXPECT_TRUE(found) << "one warning states that low-confidence pixels exist";
}

TEST_F(DefectCorrectV2Test, ABlobIsFilledFromTheOutsideInAndNothingStaysUnfilled) {
    std::vector<uint8_t> m(static_cast<size_t>(W) * H, 0);
    for (uint32_t y = 30; y < 36; ++y)
        for (uint32_t x = 40; x < 46; ++x) m[y * W + x] = 1;
    std::vector<float> in(static_cast<size_t>(W) * H, 1234.0f);
    for (uint32_t y = 30; y < 36; ++y)
        for (uint32_t x = 40; x < 46; ++x) in[y * W + x] = 0.0f;
    loadV2(m, {}, {});
    XpeDefectCorrectionReport rep{};
    std::vector<uint8_t>      low;
    const std::vector<float>  out = correct(in, &rep, &low);
    for (uint32_t y = 30; y < 36; ++y)
        for (uint32_t x = 40; x < 46; ++x) ASSERT_EQ(1234.0f, out[y * W + x]) << "the six-by-six block is filled layer by layer with the value around it: " << x << "," << y;
    EXPECT_EQ(36u, rep.blob_pixels);
    EXPECT_EQ(0u, rep.low_confidence_pixels);
    EXPECT_EQ(0u, rep.unfilled_pixels);
}

TEST_F(DefectCorrectV2Test, TheImagingAreaIsNoSourceAndItsPixelsAreLeftAsTheyWere) {
    // columns 0..5 are outside the imaging area (one of them carries a value that would spoil any fill); a defect at x = 6 has its left neighbour outside: the fill must not read it
    std::vector<uint8_t> area(static_cast<size_t>(W) * H, 1), m(static_cast<size_t>(W) * H, 0);
    for (uint32_t y = 0; y < H; ++y)
        for (uint32_t x = 0; x < 6; ++x) area[y * W + x] = 0;
    m[20 * W + 6] = 1;      // an isolated defect next to the strip
    m[30 * W + 2] = 1;      // a flagged pixel INSIDE the strip: it must not be corrected
    std::vector<float> in = rampFrame();
    for (uint32_t y = 0; y < H; ++y)
        for (uint32_t x = 0; x < 6; ++x) in[y * W + x] = 777777.0f;
    in[20 * W + 6] = 55555.0f;
    loadV2(m, area, {});
    XpeDefectCorrectionReport rep{};
    const std::vector<float>  out = correct(in, &rep);
    for (uint32_t y = 0; y < H; ++y)
        for (uint32_t x = 0; x < 6; ++x) ASSERT_EQ(777777.0f, out[y * W + x]) << "outside the imaging area nothing is touched: " << x << "," << y;
    // the three valid neighbours of (6, 20): (7, 20), (6, 19), (6, 21) -- their mean, no 777777 in it
    const std::vector<float> ramp = rampFrame();
    const float              expect = (ramp[20 * W + 7] + ramp[19 * W + 6] + ramp[21 * W + 6]) / 3.0f;
    EXPECT_NEAR(expect, out[20 * W + 6], 1e-3);
    EXPECT_EQ(6u * H, rep.out_of_area_pixels);
    EXPECT_EQ(1u, rep.isolated_pixels);
    EXPECT_EQ(H, out.size() / W) << "the frame is not cropped";
}

TEST_F(DefectCorrectV2Test, ABlobEnclosedByTheOutsideOfTheAreaTakesTheRingFillAndIsMarked) {
    // a 2 x 2 blob with a ring of three pixels outside the area around it: no valid pixel touches it, the ring search finds one farther out
    std::vector<uint8_t> area(static_cast<size_t>(W) * H, 1), m(static_cast<size_t>(W) * H, 0);
    for (uint32_t y = 25; y < 36; ++y)
        for (uint32_t x = 40; x < 51; ++x) area[y * W + x] = 0;
    for (uint32_t y = 29; y < 32; ++y)
        for (uint32_t x = 44; x < 47; ++x) area[y * W + x] = 1;   // the inside of the ring: area again
    for (uint32_t y = 30; y < 32; ++y)
        for (uint32_t x = 45; x < 47; ++x) m[y * W + x] = 1;       // the blob
    std::vector<float> in(static_cast<size_t>(W) * H, 500.0f);
    loadV2(m, area, {});
    XpeDefectCorrectionReport rep{};
    std::vector<uint8_t>      low;
    const std::vector<float>  out = correct(in, &rep, &low);
    // pixels of the inner square that are not defective are valid pixels: the blob touches them, so this layout is NOT enclosed. The fill reads them; no low confidence
    EXPECT_EQ(0u, rep.low_confidence_pixels);
    for (uint32_t y = 30; y < 32; ++y)
        for (uint32_t x = 45; x < 47; ++x) EXPECT_EQ(500.0f, out[y * W + x]);
    // now remove the valid inner pixels: the whole inner square is the blob
    std::fill(m.begin(), m.end(), 0);
    for (uint32_t y = 29; y < 32; ++y)
        for (uint32_t x = 44; x < 47; ++x) m[y * W + x] = 1;
    loadV2(m, area, {});
    XpeDefectCorrectionReport rep2{};
    std::vector<uint8_t>      low2;
    const std::vector<float>  out2 = correct(in, &rep2, &low2);
    EXPECT_EQ(9u, rep2.low_confidence_pixels) << "no valid pixel touches the blob: the ring fill of the first edition, marked";
    EXPECT_EQ(1u, rep2.low_confidence_regions);
    for (uint32_t y = 29; y < 32; ++y)
        for (uint32_t x = 44; x < 47; ++x) {
            EXPECT_EQ(500.0f, out2[y * W + x]);
            EXPECT_EQ(1, low2[y * W + x]);
        }
}

TEST_F(DefectCorrectV2Test, ABlobWithNoValidPixelWithinSixteenKeepsItsValueAndIsMarked) {
    std::vector<uint8_t> area(static_cast<size_t>(W) * H, 0), m(static_cast<size_t>(W) * H, 0);   // everything outside the area but the blob itself
    for (uint32_t y = 30; y < 32; ++y)
        for (uint32_t x = 45; x < 47; ++x) {
            area[y * W + x] = 1;
            m[y * W + x] = 1;
        }
    std::vector<float> in(static_cast<size_t>(W) * H, 500.0f);
    for (uint32_t y = 30; y < 32; ++y)
        for (uint32_t x = 45; x < 47; ++x) in[y * W + x] = 4242.0f;
    loadV2(m, area, {});
    XpeDefectCorrectionReport rep{};
    std::vector<uint8_t>      low;
    const std::vector<float>  out = correct(in, &rep, &low);
    for (uint32_t y = 30; y < 32; ++y)
        for (uint32_t x = 45; x < 47; ++x) {
            EXPECT_EQ(4242.0f, out[y * W + x]) << "never 0, never invented: the input value stays";
            EXPECT_EQ(1, low[y * W + x]);
        }
    EXPECT_EQ(4u, rep.unfilled_pixels);
    EXPECT_EQ(4u, rep.low_confidence_pixels);
}

TEST_F(DefectCorrectV2Test, InPlaceMatchesOutOfPlaceForAVersionTwoMap) {
    std::vector<uint8_t> m(static_cast<size_t>(W) * H, 0);
    for (uint32_t x = 10; x < 81; ++x) m[40 * W + x] = 1;
    for (uint32_t y = 50; y < 54; ++y)
        for (uint32_t x = 30; x < 34; ++x) m[y * W + x] = 1;
    m[10 * W + 10] = 1;
    loadV2(m, {}, {{0, 40, 1, 10, 81}});
    const std::vector<float> expected = correct(texturedFrame());
    std::vector<float>       both = texturedFrame();
    XpeImageBuffer           buf = bufferOf(both);
    XpeImageMetadata         meta{};
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&buf, &buf, &meta));
    EXPECT_EQ(0, std::memcmp(expected.data(), both.data(), both.size() * sizeof(float)));
}

TEST_F(DefectCorrectV2Test, TheCachedLoaderKeepsTheLayersAndALaterVersionOneFileClearsThem) {
    std::vector<uint8_t> m(static_cast<size_t>(W) * H, 0);
    for (uint32_t x = 10; x < 81; ++x) m[40 * W + x] = 1;
    writeV2(path2, m, {}, {{0, 40, 1, 10, 81}});
    XpeImageBuffer view{};
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_cached(path2, &view));   // a miss: loads the file
    XpeDefectCorrectionReport r1{};
    correct(rampFrame(), &r1);
    EXPECT_EQ(1u, r1.layers_used);
    EXPECT_EQ(71u, r1.line_pixels);
    loadV1(goldenMap());   // a version-1 file replaces the map and, with it, the layers
    XpeDefectCorrectionReport r2{};
    correct(rampFrame(), &r2);
    EXPECT_EQ(0u, r2.layers_used);
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_cached(path2, &view));   // a hit: installs the map and the layers
    XpeDefectCorrectionReport r3{};
    correct(rampFrame(), &r3);
    EXPECT_EQ(1u, r3.layers_used) << "a cache hit installs the layers the load made";
    EXPECT_EQ(71u, r3.line_pixels);
}

TEST_F(DefectCorrectV2Test, TheLowConfidenceBufferIsCheckedLikeAnyOutput) {
    loadV1(goldenMap());
    std::vector<float> in = rampFrame(), out(in.size());
    XpeImageBuffer     a = bufferOf(in), b = bufferOf(out);
    XpeImageMetadata   meta{};
    std::vector<uint8_t> small(10, 0);
    XpeImageBuffer     l{};
    l.data = small.data();
    l.width = W;
    l.height = H;
    l.format = XPE_PIXEL_UINT8;
    l.dataSize = small.size();
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL, xpe_defect_correct_ex(&a, &b, &meta, nullptr, &l));
    l.dataSize = W * H;
    l.format = XPE_PIXEL_FLOAT32;
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, xpe_defect_correct_ex(&a, &b, &meta, nullptr, &l));
    EXPECT_EQ(XPE_OK, xpe_defect_correct_ex(&a, &b, &meta, nullptr, nullptr));
}

// QA-A-255b (Codex #223): the low-confidence state of a pixel belongs to its group, not to the way it was corrected. The per-pixel mask, the report count and the warning must agree.
struct LowCheck {
    uint32_t pixels;
    uint32_t regions;
};

class LowConfidenceTest : public DefectCorrectV2Test {
protected:
    void expectConsistent(const XpeDefectCorrectionReport& rep, const std::vector<uint8_t>& low, const LowCheck& want) {
        const uint32_t inMask = static_cast<uint32_t>(std::count(low.begin(), low.end(), static_cast<uint8_t>(1)));
        EXPECT_EQ(want.pixels, inMask) << "the pixels marked in the mask";
        EXPECT_EQ(want.pixels, rep.low_confidence_pixels) << "the report counts the same pixels";
        EXPECT_EQ(want.regions, rep.low_confidence_regions);
        const std::string expectText = "XPE_WARN_DEFECT_LOW_CONFIDENCE: " + std::to_string(want.pixels) + " pixel(s) in " + std::to_string(want.regions) + " region(s)";
        char              msg[512] = {0};
        int32_t           sev = 0;
        bool              found = false;
        for (int32_t i = 0; i < xpe_get_pending_alert_count(); ++i)
            if (xpe_get_pending_alert(i, msg, sizeof msg, &sev) == XPE_OK && std::string(msg).find(expectText) != std::string::npos) found = true;
        EXPECT_EQ(want.pixels > 0, found) << "one warning that carries the same two numbers, and none when nothing is low";
    }
};

TEST_F(LowConfidenceTest, ARowGroupOfThreeCrossedByAFullColumnKeepsEveryPixelLow) {
    // Codex #223 input: 213 pixels in the row group, three of them (x = 40) in the column line too. The row stage cannot interpolate those three (the column is masked above and below beyond its reach)
    // and used to skip them without the mark: 210 of 213.
    std::vector<uint8_t> m(static_cast<size_t>(W) * H, 0);
    for (uint32_t y = 20; y < 23; ++y)
        for (uint32_t x = 10; x < 81; ++x) m[y * W + x] = 1;
    for (uint32_t y = 0; y < H; ++y) m[y * W + 40] = 1;
    loadV2(m, {}, {{0, 20, 3, 10, 81}, {1, 40, 1, 0, H}});
    XpeDefectCorrectionReport rep{};
    std::vector<uint8_t>      low;
    correct(rampFrame(), &rep, &low);
    expectConsistent(rep, low, {213, 1});
    for (uint32_t y = 20; y < 23; ++y)
        for (uint32_t x = 10; x < 81; ++x) ASSERT_EQ(1, low[y * W + x]) << x << "," << y;
    EXPECT_EQ(0, low[10 * W + 40]) << "the one-line column outside the row group is not low";
}

TEST_F(LowConfidenceTest, APixelTheOtherDirectionCorrectedFirstKeepsTheMarkOfItsGroup) {
    // a one-line row (handled first) crosses a column group of three whose reach is short: the row stage interpolates the three crossing pixels, and the column stage then skips them as done
    std::vector<uint8_t> m(static_cast<size_t>(W) * H, 0);
    for (uint32_t x = 10; x < 81; ++x) m[40 * W + x] = 1;
    for (uint32_t y = 35; y < 45; ++y)
        for (uint32_t x = 40; x < 43; ++x) m[y * W + x] = 1;
    loadV2(m, {}, {{0, 40, 1, 10, 81}, {1, 40, 3, 35, 45}});
    XpeDefectCorrectionReport rep{};
    std::vector<uint8_t>      low;
    correct(rampFrame(), &rep, &low);
    expectConsistent(rep, low, {30, 1});
    for (uint32_t y = 35; y < 45; ++y)
        for (uint32_t x = 40; x < 43; ++x) ASSERT_EQ(1, low[y * W + x]) << x << "," << y;
}

TEST_F(LowConfidenceTest, AGroupThatStraddlesTheEdgeOfTheImagingAreaMarksOnlyItsPixelsInsideIt) {
    std::vector<uint8_t> area(static_cast<size_t>(W) * H, 1), m(static_cast<size_t>(W) * H, 0);
    for (uint32_t y = 0; y < H; ++y)
        for (uint32_t x = 0; x < 20; ++x) area[y * W + x] = 0;
    for (uint32_t y = 40; y < 43; ++y)
        for (uint32_t x = 10; x < 31; ++x) m[y * W + x] = 1;
    loadV2(m, area, {{0, 40, 3, 10, 31}});
    XpeDefectCorrectionReport rep{};
    std::vector<uint8_t>      low;
    correct(rampFrame(), &rep, &low);
    expectConsistent(rep, low, {33, 1});   // x = 20..30 on three rows
    for (uint32_t y = 40; y < 43; ++y)
        for (uint32_t x = 10; x < 20; ++x) ASSERT_EQ(0, low[y * W + x]) << "outside the imaging area nothing is marked: " << x << "," << y;
}

TEST_F(LowConfidenceTest, ASingleLineWhoseCorrectionMethodIsRejectIsLowToo) {
    // the low state also follows the method recorded for the group, not only its width
    std::vector<uint8_t> m(static_cast<size_t>(W) * H, 0);
    for (uint32_t x = 10; x < 81; ++x) m[40 * W + x] = 1;
    loadV2(m, {}, {{0, 40, 1, 10, 81, 1}});
    XpeDefectCorrectionReport rep{};
    std::vector<uint8_t>      low;
    correct(rampFrame(), &rep, &low);
    expectConsistent(rep, low, {71, 1});
}

TEST_F(LowConfidenceTest, AVersionOneMapReportsNoCountsBeyondTheContract) {
    // the contract in the header: for a version-1 file only size is set (layers_used and out_of_area_pixels are 0), every other count is 0, and the first edition's bytes are unchanged
    loadV1(goldenMap());
    XpeDefectCorrectionReport rep{};
    std::vector<uint8_t>      low;
    const std::vector<float>  out = correct(texturedFrame(), &rep, &low);
    EXPECT_EQ(0xfa3a2284911ddbe8ull, fnv1a(out.data(), out.size() * sizeof(float)));
    EXPECT_EQ(sizeof(XpeDefectCorrectionReport), rep.size);
    EXPECT_EQ(0u, rep.layers_used);
    EXPECT_EQ(0u, rep.isolated_pixels);
    EXPECT_EQ(0u, rep.blob_pixels);
    EXPECT_EQ(0u, rep.line_pixels);
    EXPECT_EQ(0u, rep.line_groups);
    EXPECT_EQ(0u, rep.one_sided_line_pixels);
    EXPECT_EQ(0u, rep.low_confidence_pixels);
    EXPECT_EQ(0u, rep.low_confidence_regions);
    EXPECT_EQ(0u, rep.unfilled_pixels);
    EXPECT_EQ(0u, rep.out_of_area_pixels);
}

}  // namespace
