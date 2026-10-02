/**
 * @file test_nonfinite_inputs.cpp
 * @brief xpe_binning_correct and xpe_defect_detect_runtime refuse a non-finite frame and write nothing (QA-A-215, #233).
 *
 * The rule of QA-A-214b: a consumer refuses a NaN or an infinity at its entrance with XPE_ERR_INVALID_INPUT and leaves
 * its output alone (an in-place function: its input).
 *   - binning used to scale pixel by pixel and give up with XPE_ERR_PROCESSING_FAILED at the first non-finite result,
 *     after every pixel before it was already divided. The only way its scaling produces a non-finite value is a
 *     non-finite input (the factor is 1/4 or 1/16), so the frame is now checked first.
 *   - runtime defect detection did not look: a NaN pixel was reported as GOOD (every comparison with NaN is false), an
 *     infinity as defective, neither disturbing the other pixels (measured, QA-A-215).
 * Neither stage can be handed such a frame by the pipeline (its input is the gain stage's finite output).
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "preprocess_state_fixture.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <random>
#include <string>
#include <vector>

namespace {

std::string findAlert(const std::string& prefix) {
    const int32_t n = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < n; ++i) {
        char buf[1024];
        int32_t sev = 0;
        if (xpe_get_pending_alert(i, buf, sizeof(buf), &sev) == XPE_OK && std::string(buf).rfind(prefix, 0) == 0) return buf;
    }
    return {};
}

XpeImageBuffer f32(std::vector<float>& v, uint32_t w, uint32_t h) {
    XpeImageBuffer b{};
    b.data = v.data(); b.width = w; b.height = h; b.bitsAllocated = 32; b.bitsStored = 32; b.format = XPE_PIXEL_FLOAT32; b.dataSize = v.size() * 4;
    return b;
}

const float kNaN = std::numeric_limits<float>::quiet_NaN();
const float kInf = std::numeric_limits<float>::infinity();

class NonFiniteInputTest : public XpePreprocessStateFixture {
protected:
    void SetUp() override { XpePreprocessStateFixture::SetUp(); xpe_clear_alerts(); }
    void TearDown() override { xpe_clear_alerts(); XpePreprocessStateFixture::TearDown(); }
};

}  // namespace

// ---- binning ---------------------------------------------------------------------------------------------------

TEST_F(NonFiniteInputTest, BinningRefusesANonFiniteFrameAndLeavesEveryPixelAsItWas) {
    struct Case { const char* name; size_t at; float v; };
    const Case cases[] = {{"NaN in the middle", 20, kNaN}, {"+inf in the middle", 20, kInf}, {"-inf in the middle", 20, -kInf},
                          {"NaN at the first pixel", 0, kNaN}, {"+inf at the last pixel", 39, kInf}};
    for (const int mode : {2, 4}) {
        for (const Case& c : cases) {
            SCOPED_TRACE(std::string(c.name) + ", mode " + std::to_string(mode));
            std::vector<float> v(40);
            for (size_t i = 0; i < v.size(); ++i) v[i] = 100.0f + static_cast<float>(i);
            v[c.at] = c.v;
            const std::vector<float> before = v;
            XpeImageBuffer b = f32(v, 8, 5);
            xpe_clear_alerts();
            EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_binning_correct(&b, mode, nullptr));
            EXPECT_EQ(0, std::memcmp(before.data(), v.data(), v.size() * sizeof(float))) << "no pixel before the bad one was divided";
            EXPECT_FALSE(findAlert("XPE_WARN_BINNING_INPUT_NOT_FINITE:").empty());
        }
    }
}

TEST_F(NonFiniteInputTest, BinningTheAlertNamesTheCountAndTheFirstPixel) {
    std::vector<float> v(40, 50.0f);
    v[13] = kInf; v[30] = kNaN;
    XpeImageBuffer b = f32(v, 8, 5);
    xpe_clear_alerts();
    ASSERT_EQ(XPE_ERR_INVALID_INPUT, xpe_binning_correct(&b, 2, nullptr));
    EXPECT_EQ("XPE_WARN_BINNING_INPUT_NOT_FINITE: 2 pixel(s) of the input frame are NaN or infinite (first: index 13, x=5, y=1); "
              "the frame was not binned and the buffer was not changed",
              findAlert("XPE_WARN_BINNING_INPUT_NOT_FINITE:"));
}

TEST_F(NonFiniteInputTest, BinningOfAFiniteFrameIsTheScalingItAlwaysWasEvenAtTheEdgesOfTheFloatRange) {
    // the only way scaling by 1/4 or 1/16 could give a non-finite value is a non-finite input: the largest float, the
    // smallest normal, denormals, zeros and negatives all scale to finite values, and the call says OK
    const float vals[] = {std::numeric_limits<float>::max(), std::numeric_limits<float>::lowest(), std::numeric_limits<float>::min(),
                          std::numeric_limits<float>::denorm_min(), 1e-40f, 0.0f, -0.0f, 1.0f, -3.5f, 65535.0f};
    for (const int mode : {2, 4}) {
        std::vector<float> v(std::begin(vals), std::end(vals));
        std::vector<float> want(v.size());
        const float norm = 1.0f / static_cast<float>(mode * mode);
        for (size_t i = 0; i < v.size(); ++i) want[i] = v[i] * norm;
        XpeImageBuffer b = f32(v, 10, 1);
        xpe_clear_alerts();
        ASSERT_EQ(XPE_OK, xpe_binning_correct(&b, mode, nullptr)) << "mode " << mode;
        EXPECT_EQ(0, std::memcmp(want.data(), v.data(), v.size() * sizeof(float))) << "mode " << mode << ": the same scaling, bit for bit";
        for (float x : v) EXPECT_TRUE(std::isfinite(x));
        EXPECT_EQ(0, xpe_get_pending_alert_count());
    }
}

TEST_F(NonFiniteInputTest, BinningKeepsItsOlderErrorsAheadOfThePixels) {
    std::vector<float> v(40, 1.0f);
    v[3] = kNaN;
    XpeImageBuffer b = f32(v, 8, 5);
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, xpe_binning_correct(&b, 3, nullptr)) << "a bad mode is answered before the pixels are looked at";
    EXPECT_EQ(XPE_OK, xpe_binning_correct(&b, 1, nullptr)) << "mode 1 is a no-op: nothing is processed, so nothing is refused";
    b.format = XPE_PIXEL_UINT16;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_binning_correct(&b, 2, nullptr)) << "the format check comes first";
}

// ---- runtime defect detection ---------------------------------------------------------------------------------------

namespace {
constexpr uint32_t W = 48, H = 40;

std::vector<float> noiseFrame() {
    std::mt19937 g(11);
    std::normal_distribution<float> nd(1000.0f, 20.0f);
    std::vector<float> f(W * H);
    for (auto& x : f) x = nd(g);
    return f;
}
XpeErrorCode detect(std::vector<float>& img, std::vector<uint8_t>& map) {
    XpeImageBuffer ib = f32(img, W, H), mb{};
    mb.data = map.data(); mb.width = W; mb.height = H; mb.bitsAllocated = 8; mb.bitsStored = 8; mb.format = XPE_PIXEL_UINT8; mb.dataSize = map.size();
    XpeImageMetadata meta{};
    return xpe_defect_detect_runtime(&ib, &meta, &mb);
}
}  // namespace

TEST_F(NonFiniteInputTest, RuntimeDetectionRefusesANonFiniteFrameAndLeavesTheMapAlone) {
    struct Case { const char* name; size_t at; float v; };
    const Case cases[] = {{"NaN", 20 * W + 20, kNaN}, {"+inf", 20 * W + 20, kInf}, {"-inf", 20 * W + 20, -kInf}, {"NaN at pixel 0", 0, kNaN},
                          {"+inf at the last pixel", W * H - 1, kInf}};
    for (const Case& c : cases) {
        SCOPED_TRACE(c.name);
        std::vector<float> img = noiseFrame();
        img[c.at] = c.v;
        const std::vector<float> imgBefore = img;
        std::vector<uint8_t> map(W * H, 77);   // a pattern the call must not clear
        xpe_clear_alerts();
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, detect(img, map));
        for (uint8_t x : map) ASSERT_EQ(77u, x) << "the defect map keeps its bytes";
        EXPECT_EQ(0, std::memcmp(imgBefore.data(), img.data(), img.size() * sizeof(float)));
        EXPECT_FALSE(findAlert("XPE_WARN_RUNTIME_DETECT_INPUT_NOT_FINITE:").empty());
    }
}

TEST_F(NonFiniteInputTest, RuntimeDetectionTheAlertNamesTheCountAndTheFirstPixel) {
    std::vector<float> img = noiseFrame();
    img[3 * W + 7] = kNaN; img[9 * W + 1] = -kInf; img[39 * W + 47] = kInf;
    std::vector<uint8_t> map(W * H, 0);
    xpe_clear_alerts();
    ASSERT_EQ(XPE_ERR_INVALID_INPUT, detect(img, map));
    EXPECT_EQ("XPE_WARN_RUNTIME_DETECT_INPUT_NOT_FINITE: 3 pixel(s) of the input frame are NaN or infinite (first: index 151, x=7, y=3); "
              "no detection was run and the defect map was not written",
              findAlert("XPE_WARN_RUNTIME_DETECT_INPUT_NOT_FINITE:"));
}

TEST_F(NonFiniteInputTest, RuntimeDetectionOfAFiniteFrameStillFindsTheOutlierAndRaisesNothing) {
    std::vector<float> img = noiseFrame();
    img[20 * W + 20] = 60000.0f;   // a hot pixel, far outside the noise
    std::vector<uint8_t> map(W * H, 77);
    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, detect(img, map));
    EXPECT_NE(0u, map[20 * W + 20]) << "the finite outlier is flagged, as before";
    size_t flagged = 0; for (uint8_t x : map) flagged += (x != 0);
    EXPECT_EQ(1u, flagged);
    EXPECT_EQ(0, xpe_get_pending_alert_count());
}

TEST_F(NonFiniteInputTest, RuntimeDetectionKeepsItsOlderErrorsAheadOfThePixels) {
    std::vector<float> img = noiseFrame();
    img[5] = kNaN;
    std::vector<uint8_t> small(10, 0);
    XpeImageBuffer ib = f32(img, W, H), mb{};
    mb.data = small.data(); mb.width = W; mb.height = H; mb.bitsAllocated = 8; mb.bitsStored = 8; mb.format = XPE_PIXEL_UINT8; mb.dataSize = small.size();
    XpeImageMetadata meta{};
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL, xpe_defect_detect_runtime(&ib, &meta, &mb)) << "a map too small is answered before the pixels are read";
}
