/**
 * @file test_defect_fill_interior.cpp
 * @brief The interior of a defect cluster is filled from the nearest valid pixels, never with 0 (QA-A-211b, #233,
 *        Codex #71), plus the two other holds of Codex #71 on QA-A-211.
 *
 * 1. median_filter_cluster returned 0.0f when a cluster pixel had no valid pixel in its 3x3. The "ring 1..3" fallback
 *    existed only for single defects. On CalData_6, 66.7% of the defect map's own pixels (and 72.5% of defect-map-or-
 *    gain-classified ones) have no valid pixel in their 3x3, at most 10 pixels from the nearest valid one -- so the
 *    real defect map was producing zeros long before the gain classification added more. The search now widens ring
 *    by ring (radius 2..16) and uses the median of the nearest ring that holds a valid pixel; a pixel with none keeps
 *    its input value and the frame says so (XPE_WARN_DEFECT_NO_VALID_NEIGHBOUR).
 * 2. With the gain stage bypassed the stored classification still reaches the defect stage, so binning must be
 *    refused there too (it mixes the original values into the neighbours first).
 * 3. The alerts raised at load, generation and application no longer say the defect stage "fills" the pixels: it may
 *    not run. They say the pixels are marked and listed for it. The full texts are pinned below (cross-lane contract).
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"
#include "fixtures/make_xcal.hpp"
#include "preprocess_state_fixture.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

std::string findAlert(const std::string& prefix) {
    const int32_t n = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < n; ++i) {
        char buf[2048];
        int32_t sev = 0;
        if (xpe_get_pending_alert(i, buf, sizeof(buf), &sev) == XPE_OK && std::string(buf).rfind(prefix, 0) == 0) return buf;
    }
    return {};
}

const char* const kGainAndDefect =
    "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassOffset\":true,\"bypassNonlinearity\":true,"
    "\"bypassBinning\":true,\"bypassGhost\":true}";
const char* const kDefectOnly =
    "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassOffset\":true,\"bypassNonlinearity\":true,"
    "\"bypassBinning\":true,\"bypassGhost\":true,\"bypassGain\":true}";
const char* const kBinningNoGain =
    "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassOffset\":true,\"bypassNonlinearity\":true,"
    "\"bypassGhost\":true,\"bypassGain\":true,\"binningMode\":2}";

class DefectFillTest : public XpePreprocessStateFixture {
protected:
    void SetUp() override {
        XpePreprocessStateFixture::SetUp();
        dir_ = (fs::temp_directory_path() / ("xpe_dfi_" + std::to_string(counter_++))).string();
        fs::create_directories(dir_);
        xpe_clear_alerts();
    }
    void TearDown() override {
        xpe_clear_alerts();
        std::error_code ec;
        fs::remove_all(dir_, ec);
        XpePreprocessStateFixture::TearDown();
    }
    std::string p(const std::string& n) const { return (fs::path(dir_) / n).string(); }

    void writeGain(const char* name, uint32_t w, uint32_t h, float base, const std::vector<size_t>& bad, float badValue) {
        std::vector<float> g(static_cast<size_t>(w) * h, base);
        for (size_t k : bad) g[k] = badValue;
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION; hdr.type = static_cast<uint32_t>(XCAL_TYPE_GAIN); hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width = w; hdr.height = h; hdr.payload_len = g.size() * sizeof(float);
        ASSERT_EQ(XPE_OK, write_xcal_file(p(name).c_str(), hdr, nullptr, 0, reinterpret_cast<const uint8_t*>(g.data()), hdr.payload_len));
        ASSERT_EQ(XPE_OK, xpe_calib_load_gain(p(name).c_str()));
    }
    void loadDefects(uint32_t w, uint32_t h, const std::vector<size_t>& marked) {
        std::vector<uint8_t> m(static_cast<size_t>(w) * h, 0);
        for (size_t k : marked) m[k] = 1;
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION; hdr.type = static_cast<uint32_t>(XCAL_TYPE_DEFECT); hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_UINT8_MASK);
        hdr.width = w; hdr.height = h; hdr.payload_len = m.size();
        ASSERT_EQ(XPE_OK, write_xcal_file(p("defects.xcal").c_str(), hdr, nullptr, 0, m.data(), m.size()));
        ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(p("defects.xcal").c_str()));
    }
    /** The block [r0, r0+rows) x [c0, c0+cols) of a w-wide frame, as pixel indices. */
    static std::vector<size_t> block(uint32_t w, uint32_t r0, uint32_t c0, uint32_t rows, uint32_t cols) {
        std::vector<size_t> v;
        for (uint32_t r = r0; r < r0 + rows; ++r) for (uint32_t c = c0; c < c0 + cols; ++c) v.push_back(static_cast<size_t>(r) * w + c);
        return v;
    }
    /** uint16 frame: `valid` everywhere, `hot` at the listed pixels; through the pipeline. */
    XpeErrorCode run(const char* config, uint32_t w, uint32_t h, uint16_t valid, const std::vector<size_t>& at, uint16_t hot,
                     std::vector<float>* out) {
        const size_t n = static_cast<size_t>(w) * h;
        std::vector<uint8_t> bytes(n * sizeof(float), 0);
        auto* v = reinterpret_cast<uint16_t*>(bytes.data());
        for (size_t i = 0; i < n; ++i) v[i] = valid;
        for (size_t k : at) v[k] = hot;
        XpeImageBuffer img{};
        img.data = bytes.data(); img.width = w; img.height = h; img.bitsAllocated = 16; img.bitsStored = 16;
        img.format = XPE_PIXEL_UINT16; img.dataSize = bytes.size();
        XpeImageMetadata meta{};
        const XpeErrorCode rc = xpe_preprocess_pipeline_batch(&img, 1, &meta, nullptr, nullptr, config);
        if (rc == XPE_OK && out) { out->resize(n); std::memcpy(out->data(), bytes.data(), n * sizeof(float)); }
        return rc;
    }
    static size_t countZero(const std::vector<float>& v) { size_t z = 0; for (float x : v) z += (x == 0.0f); return z; }

    std::string dir_;
    static inline int counter_ = 0;
};

}  // namespace

// ---- 1. the interior of a cluster ------------------------------------------------------------------------------

// Codex's reproduction: 20x20, every pixel 100, defect map empty, the gain map zero in the middle 3x3 only (2.25%, under the cap).
TEST_F(DefectFillTest, ACentre3x3ClassifiedByTheGainMapIsFilledFromTheRingNotWithZero) {
    constexpr uint32_t W = 20, H = 20;
    writeGain("g.xcal", W, H, 2.0f, block(W, 9, 9, 3, 3), 0.0f);
    loadDefects(W, H, {});
    std::vector<float> out;
    ASSERT_EQ(XPE_OK, run(kGainAndDefect, W, H, 100, {}, 100, &out));
    EXPECT_EQ(0u, countZero(out)) << "no pixel of the block was written as 0";
    for (size_t i = 0; i < out.size(); ++i) EXPECT_FLOAT_EQ(50.0f, out[i]) << "pixel " << i << " (100 / gain 2, the block from the valid ring)";
}

TEST_F(DefectFillTest, AClusterOfTheDefectMapItselfIsFilledTheSameWay) {
    constexpr uint32_t W = 20, H = 20;
    writeGain("g.xcal", W, H, 2.0f, {}, 0.0f);
    loadDefects(W, H, block(W, 8, 8, 5, 5));   // a 5x5 block: the centre is 3 pixels from the nearest valid one
    std::vector<float> out;
    ASSERT_EQ(XPE_OK, run(kGainAndDefect, W, H, 100, block(W, 8, 8, 5, 5), 9000, &out));
    EXPECT_EQ(0u, countZero(out));
    for (size_t i = 0; i < out.size(); ++i) EXPECT_FLOAT_EQ(50.0f, out[i]) << "pixel " << i;
}

TEST_F(DefectFillTest, TheNearestRingDecidesNotAWiderOne) {
    // a 1-pixel valid dot 2 pixels from the block's centre must win over the far valid background
    constexpr uint32_t W = 30, H = 30;
    writeGain("g.xcal", W, H, 1.0f, {}, 0.0f);
    std::vector<size_t> blk = block(W, 10, 10, 9, 9);   // rows/cols 10..18, centre (14,14)
    // make (14, 16) valid again: remove it from the mask -> it is the nearest valid pixel of the centre (distance 2)
    const size_t dot = 14u * W + 16u;
    blk.erase(std::remove(blk.begin(), blk.end(), dot), blk.end());
    loadDefects(W, H, blk);
    std::vector<uint8_t> bytes(static_cast<size_t>(W) * H * sizeof(float), 0);
    auto* px = reinterpret_cast<uint16_t*>(bytes.data());
    for (size_t i = 0; i < static_cast<size_t>(W) * H; ++i) px[i] = 100;
    px[dot] = 700;                                           // the dot has its own value
    XpeImageBuffer img{};
    img.data = bytes.data(); img.width = W; img.height = H; img.bitsAllocated = 16; img.bitsStored = 16; img.format = XPE_PIXEL_UINT16; img.dataSize = bytes.size();
    XpeImageMetadata meta{};
    ASSERT_EQ(XPE_OK, xpe_preprocess_pipeline_batch(&img, 1, &meta, nullptr, nullptr, kGainAndDefect));
    const float* out = reinterpret_cast<const float*>(bytes.data());
    EXPECT_FLOAT_EQ(700.0f, out[14u * W + 14u]) << "the centre of the block takes the nearest valid pixel (the dot, distance 2), not the 100 background";
}

TEST_F(DefectFillTest, InPlaceAndOutOfPlaceAgreeOnTheRingPathAndNeitherIsZero) {
    constexpr uint32_t W = 24, H = 24;
    loadDefects(W, H, block(W, 8, 8, 7, 7));   // a 7x7 block: the centre is 4 pixels from a valid one
    std::vector<float> src(static_cast<size_t>(W) * H, 100.0f);
    for (size_t k : block(W, 8, 8, 7, 7)) src[k] = 9999.0f;
    for (size_t i = 0; i < src.size(); ++i) if (src[i] == 100.0f) src[i] = 100.0f + static_cast<float>(i % 7);   // valid pixels differ
    auto frame = [&](std::vector<float>& d) { XpeImageBuffer b{}; b.data = d.data(); b.width = W; b.height = H; b.bitsAllocated = 32; b.bitsStored = 32; b.format = XPE_PIXEL_FLOAT32; b.dataSize = d.size() * 4; return b; };
    std::vector<float> inplace = src, out(src.size(), -1.0f), in2 = src;
    XpeImageBuffer ib = frame(inplace), i2 = frame(in2), ob = frame(out);
    XpeImageMetadata meta{};
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&ib, &ib, &meta));
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&i2, &ob, &meta));
    EXPECT_EQ(inplace, out) << "the same result in place and out of place";
    EXPECT_EQ(in2, src) << "the input was not modified by the out-of-place call";
    for (size_t k : block(W, 8, 8, 7, 7)) {
        EXPECT_GE(out[k], 100.0f) << "pixel " << k;
        EXPECT_LE(out[k], 106.0f) << "pixel " << k << " is a valid neighbour's value, not 0 and not the 9999 it came in with";
    }
}

TEST_F(DefectFillTest, APixelWithNoValidPixelWithinTheSearchRadiusKeepsItsValueAndTheFrameSaysSo) {
    constexpr uint32_t W = 40, H = 40;
    // a 36x36 block: the 4x4 pixels in its middle are 17 or 18 pixels from the nearest valid one -- beyond radius 16
    const std::vector<size_t> blk = block(W, 2, 2, 36, 36);
    loadDefects(W, H, blk);
    std::vector<float> src(static_cast<size_t>(W) * H, 100.0f), out(src.size(), -1.0f);
    for (size_t k : blk) src[k] = 7777.0f;
    XpeImageBuffer ib{}, ob{};
    ib.data = src.data(); ib.width = W; ib.height = H; ib.bitsAllocated = 32; ib.bitsStored = 32; ib.format = XPE_PIXEL_FLOAT32; ib.dataSize = src.size() * 4;
    ob = ib; ob.data = out.data();
    XpeImageMetadata meta{};
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&ib, &ob, &meta));
    size_t kept = 0, filled = 0;
    for (size_t k : blk) { if (out[k] == 7777.0f) ++kept; else if (out[k] == 100.0f) ++filled; }
    EXPECT_EQ(16u, kept) << "the 4x4 middle (distance 17..18) keeps the input value";
    EXPECT_EQ(blk.size() - 16u, filled) << "everything within radius 16 is filled from the valid background";
    EXPECT_EQ(0u, countZero(out)) << "and nothing is 0";
    const std::string a = findAlert("XPE_WARN_DEFECT_NO_VALID_NEIGHBOUR:");
    ASSERT_FALSE(a.empty());
    EXPECT_EQ("XPE_WARN_DEFECT_NO_VALID_NEIGHBOUR: 16 masked pixel(s) have no valid pixel within 16 pixels to fill them from and keep their input value", a);
}

TEST_F(DefectFillTest, AFrameWhoseMaskedPixelsAllHaveAValidNeighbourRaisesNothing) {
    constexpr uint32_t W = 20, H = 20;
    loadDefects(W, H, block(W, 8, 8, 4, 4));
    std::vector<float> src(static_cast<size_t>(W) * H, 100.0f), out(src.size());
    XpeImageBuffer ib{}, ob{};
    ib.data = src.data(); ib.width = W; ib.height = H; ib.bitsAllocated = 32; ib.bitsStored = 32; ib.format = XPE_PIXEL_FLOAT32; ib.dataSize = src.size() * 4;
    ob = ib; ob.data = out.data();
    XpeImageMetadata meta{};
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&ib, &ob, &meta));
    EXPECT_TRUE(findAlert("XPE_WARN_DEFECT_NO_VALID_NEIGHBOUR:").empty());
}

// ---- 2. binning with the gain stage bypassed --------------------------------------------------------------------

TEST_F(DefectFillTest, ABypassedGainStageDoesNotLetBinningMixInAStoredClassification) {
    constexpr uint32_t W = 20, H = 20;
    writeGain("g.xcal", W, H, 2.0f, {5u * W + 5u}, 0.0f);   // one classified pixel (0.25%)
    loadDefects(W, H, {});
    xpe_clear_alerts();
    std::vector<float> out;
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, run(kBinningNoGain, W, H, 100, {5u * W + 5u}, 9000, &out));
    EXPECT_FALSE(findAlert("XPE_WARN_GAIN_PIXELS_WITH_BINNING:").empty());
}

TEST_F(DefectFillTest, WithNothingClassifiedTheSameConfigIsNotRefusedByThisRule) {
    constexpr uint32_t W = 20, H = 20;
    writeGain("g.xcal", W, H, 2.0f, {}, 0.0f);
    loadDefects(W, H, {});
    xpe_clear_alerts();
    const XpeErrorCode rc = run(kBinningNoGain, W, H, 100, {}, 100, nullptr);
    EXPECT_NE(XPE_ERR_CONFIG_INVALID, rc) << "control: the refusal is about the classification";
    EXPECT_TRUE(findAlert("XPE_WARN_GAIN_PIXELS_WITH_BINNING:").empty());
}

TEST_F(DefectFillTest, WithTheDefectStageBypassedTooTheStoredListIsNotDeliveredAndBinningIsLeftAlone) {
    constexpr uint32_t W = 20, H = 20;
    writeGain("g.xcal", W, H, 2.0f, {5u * W + 5u}, 0.0f);
    const char* cfg =
        "{\"bypassReadout\":true,\"bypassTemp\":true,\"bypassOffset\":true,\"bypassNonlinearity\":true,"
        "\"bypassGhost\":true,\"bypassGain\":true,\"bypassDefect\":true,\"binningMode\":2}";
    xpe_clear_alerts();
    const XpeErrorCode rc = run(cfg, W, H, 100, {}, 100, nullptr);
    EXPECT_NE(XPE_ERR_CONFIG_INVALID, rc) << "nothing reads the list when neither the gain nor the defect stage runs";
}

// ---- 3. the alert texts ------------------------------------------------------------------------------------------

TEST_F(DefectFillTest, TheLoadAlertSaysMarkedAndListedNotFilled) {
    constexpr uint32_t W = 20, H = 20;
    std::vector<float> g(static_cast<size_t>(W) * H, 2.0f);
    g[0] = 0.0f; g[7u * W + 7u] = 50.0f;
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version = XCAL_VERSION; hdr.type = static_cast<uint32_t>(XCAL_TYPE_GAIN); hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
    hdr.width = W; hdr.height = H; hdr.payload_len = g.size() * sizeof(float);
    ASSERT_EQ(XPE_OK, write_xcal_file(p("t.xcal").c_str(), hdr, nullptr, 0, reinterpret_cast<const uint8_t*>(g.data()), hdr.payload_len));
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(p("t.xcal").c_str()));
    EXPECT_EQ("XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT: 2 of 400 pixel(s) (0.500%) have a gain outside [0.1, 10.0] and are marked defective: "
              "gain 1.0 is used and they are listed for the defect correction stage (2 in the outermost 64-pixel band; first: 0). Limit: 5.0%",
              findAlert("XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT:"));
}

TEST_F(DefectFillTest, TheApplicationAlertOfAPolynomialSaysMarkedAndListedNotFilled) {
    constexpr uint32_t W = 20, H = 20;
    std::vector<float> c(static_cast<size_t>(W) * H * 2, 0.0f);
    for (size_t q = 0; q < static_cast<size_t>(W) * H; ++q) c[q * 2] = (q == 37) ? 0.0f : 2.0f;
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version = XCAL_VERSION; hdr.type = static_cast<uint32_t>(XCAL_TYPE_GAIN_POLY); hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
    hdr.width = W; hdr.height = H; hdr.payload_len = c.size() * sizeof(float);
    ASSERT_EQ(XPE_OK, write_xcal_file(p("poly.xcal").c_str(), hdr, nullptr, 0, reinterpret_cast<const uint8_t*>(c.data()), hdr.payload_len));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(p("poly.xcal").c_str()));
    loadDefects(W, H, {});
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, run(kGainAndDefect, W, H, 100, {}, 100, nullptr));
    EXPECT_EQ("XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT: 1 pixel(s) of this frame evaluate to a gain outside [0.1, 10.0] and are marked defective "
              "(gain 1.0) and listed for the defect correction stage",
              findAlert("XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT:"));
}
