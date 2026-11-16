/**
 * @file test_gain_poly_not_applied.cpp
 * @brief The scalar gain map: what is applied, the FUNC-002 range at load, kVp/SID not used (QA-A-105, QA-A-107, QA-A-229)
 *
 * QA-A-336: the dose-dependent gain polynomial was removed by user decision, and with it the cases of this file that
 * loaded and applied one (the refusal of such a file is pinned in test_calib_gain_poly_load.cpp). The file and the
 * suite keep their names because the public header cites GainPolyNotAppliedTest.KvpAndSidDoNotChangeTheCorrection.
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xpe/preprocess/xcal_format.h"
#include "fixtures/make_xcal.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace {

constexpr uint32_t W = 8, H = 8;
constexpr size_t   N = static_cast<size_t>(W) * H;

class GainPolyNotAppliedTest : public ::testing::Test {
protected:
    fs::path dir;
    std::vector<uint16_t> in;
    std::vector<float>    out;

    void SetUp() override {
        (void)xpe_preprocess_init(nullptr);
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
        dir = fs::temp_directory_path() / "xpe_gain_poly_not_applied";
        fs::remove_all(dir);
        fs::create_directories(dir);
        in.assign(N, 1000u);
        out.assign(N, -1.0f);
    }

    void TearDown() override {
        // QA-A-138 (#198): leave the pending alert queue as this test found
        // it. The product drains it through this public call; nothing in
        // xpe_preprocess_shutdown() touches the common-module queue.
        xpe_clear_alerts();
        xpe_preprocess_shutdown();
        fs::remove_all(dir);
    }

    std::string p(const std::string& n) const { return (dir / n).string(); }

    XpeErrorCode correct() {
        XpeImageBuffer ib{};
        ib.width = W; ib.height = H; ib.format = XPE_PIXEL_UINT16;
        ib.bitsAllocated = 16; ib.bitsStored = 16;
        ib.data = in.data(); ib.dataSize = N * sizeof(uint16_t);
        XpeImageBuffer ob{};
        ob.width = W; ob.height = H; ob.format = XPE_PIXEL_FLOAT32;
        ob.bitsAllocated = 32; ob.bitsStored = 32;
        ob.data = out.data(); ob.dataSize = N * sizeof(float);
        XpeImageMetadata meta{};
        meta.kVp = 80.0f;
        meta.pixelPitch_mm = 0.14f;
        return xpe_gain_correct(&ib, &ob, &meta);
    }
};

}  // namespace

// The scalar map is the model gain correction uses.
TEST_F(GainPolyNotAppliedTest, ScalarGainMapIsApplied) {
    const std::string sc = p("gain.xcal");
    ASSERT_EQ(XPE_OK, MakeGainXCal(sc.c_str(), W, H, 2.0f));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(sc.c_str()));
    ASSERT_EQ(XPE_OK, correct());
    EXPECT_FLOAT_EQ(500.0f, out[0]);   // 1000 / 2
}

// ---------------------------------------------------------------------------
// SRS-CALIB-FUNC-002 (#188, QA-A-107): "Values shall be in range [0.1, 10.0];
// out-of-range values shall trigger XPE_ERR_INVALID_CALIB_DATA error."
//
// The bounds are inclusive (the requirement writes a closed interval), and the
// check lives in the loader, which is where FUNC-002 places it. The separate
// finite / non-zero guard inside xpe_gain_correct stays as a second line of
// defence; no public path reaches it any more, which its comment now records.
// ---------------------------------------------------------------------------
TEST_F(GainPolyNotAppliedTest, ScalarGainValuesInsideTheRangeLoad) {
    for (float v : {0.1f, 0.5f, 1.0f, 2.0f, 9.9f, 10.0f}) {
        const std::string path = p("in_" + std::to_string(v) + ".xcal");
        ASSERT_EQ(XPE_OK, MakeGainXCal(path.c_str(), W, H, v));
        EXPECT_EQ(XPE_OK, xpe_calib_load_gain(path.c_str())) << "gain " << v;
    }
}

TEST_F(GainPolyNotAppliedTest, ScalarGainValuesOutsideTheRangeAreRejected) {
    for (float v : {0.0f, 0.09f, 10.01f, 1000.0f, -1.0f}) {
        const std::string path = p("out_" + std::to_string(v) + ".xcal");
        ASSERT_EQ(XPE_OK, MakeGainXCal(path.c_str(), W, H, v));
        EXPECT_EQ(XPE_ERR_INVALID_CALIB_DATA, xpe_calib_load_gain(path.c_str()))
            << "gain " << v;
    }
}

// QA-A-211 (#233): ONE bad pixel no longer refuses the map -- it is classified defective (gain 1.0, index kept for the
// defect stage). It used to be refused ("the rest of the map being valid does not excuse it"); what refuses a map now is
// more than 5% of it being out of range (test_gain_defect_classify.cpp). An 8x8 map: 1 of 64 is 1.6%.
TEST_F(GainPolyNotAppliedTest, ASingleOutOfRangePixelIsClassifiedDefectiveNotRejected) {
    const std::string path = p("one_bad.xcal");
    std::vector<float> data(N, 1.0f);
    data[N / 2] = 12.0f;
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version = XCAL_VERSION;
    hdr.type = static_cast<uint32_t>(XCAL_TYPE_GAIN);
    hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
    hdr.width = W; hdr.height = H;
    hdr.payload_len = data.size() * sizeof(float);
    ASSERT_EQ(XPE_OK, write_xcal_file(path.c_str(), hdr, nullptr, 0,
                                      reinterpret_cast<const uint8_t*>(data.data()),
                                      hdr.payload_len));
    EXPECT_EQ(XPE_OK, xpe_calib_load_gain(path.c_str()));
    xpe_clear_alerts();
}

// A rejected file must not become the active calibration.
TEST_F(GainPolyNotAppliedTest, ARejectedMapDoesNotReplaceTheLoadedOne) {
    const std::string good = p("good.xcal");
    ASSERT_EQ(XPE_OK, MakeGainXCal(good.c_str(), W, H, 2.0f));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(good.c_str()));
    ASSERT_EQ(XPE_OK, correct());
    ASSERT_FLOAT_EQ(500.0f, out[0]);

    const std::string bad = p("bad.xcal");
    ASSERT_EQ(XPE_OK, MakeGainXCal(bad.c_str(), W, H, 50.0f));
    EXPECT_EQ(XPE_ERR_INVALID_CALIB_DATA, xpe_calib_load_gain(bad.c_str()));

    out.assign(N, -1.0f);
    EXPECT_EQ(XPE_OK, correct()) << "the map that was already loaded is still there";
    EXPECT_FLOAT_EQ(500.0f, out[0]) << "and it is the one that was loaded, not the rejected file";
}

/** QA-A-229 (#245, REQ-P1A-015): the header says kVp and SID are NOT used to choose a gain map -- there is
 *  one map and no interpolation across kVp or SID. This pins that statement: the same frame corrected
 *  under very different kVp / SID metadata yields the same pixels. If kVp-specific gain is ever
 *  implemented, this fails and the header sentence must change with it. */
TEST_F(GainPolyNotAppliedTest, KvpAndSidDoNotChangeTheCorrection) {
    const std::string sc = p("gain_kvp.xcal");
    ASSERT_EQ(XPE_OK, MakeGainXCal(sc.c_str(), W, H, 2.0f));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(sc.c_str()));
    for (size_t i = 0; i < N; ++i) in[i] = static_cast<uint16_t>(500 + (i * 37u) % 3000u);

    const float kvp[] = {80.0f, 40.0f, 150.0f, 0.0f};
    const float sid[] = {1000.0f, 1800.0f, 600.0f, 0.0f};
    std::vector<float> first;
    for (size_t c = 0; c < 4; ++c) {
        XpeImageBuffer ib{};
        ib.width = W; ib.height = H; ib.format = XPE_PIXEL_UINT16;
        ib.bitsAllocated = 16; ib.bitsStored = 16;
        ib.data = in.data(); ib.dataSize = N * sizeof(uint16_t);
        XpeImageBuffer ob{};
        ob.width = W; ob.height = H; ob.format = XPE_PIXEL_FLOAT32;
        ob.bitsAllocated = 32; ob.bitsStored = 32;
        std::fill(out.begin(), out.end(), -1.0f);
        ob.data = out.data(); ob.dataSize = N * sizeof(float);
        XpeImageMetadata meta{};
        meta.kVp = kvp[c];
        meta.SID_mm = sid[c];
        meta.pixelPitch_mm = 0.14f;
        ASSERT_EQ(XPE_OK, xpe_gain_correct(&ib, &ob, &meta)) << "case " << c;
        for (size_t i = 0; i < N; ++i)
            ASSERT_FLOAT_EQ(static_cast<float>(in[i]) / 2.0f, out[i]) << "case " << c << " pixel " << i;
        if (c == 0) first = out;
        else EXPECT_EQ(0, std::memcmp(first.data(), out.data(), N * sizeof(float))) << "case " << c;
    }
}
