/**
 * @file test_nonfinite_pixels.cpp
 * @brief xpe_detect_collimation refuses a non-finite pixel instead of answering rc=0 (QA-B-181f, #233).
 *
 * QA-B-181e: one +inf pixel in an otherwise ordinary image moved the detected box from the true field to the whole
 * image, with rc=0 and no sign that anything was wrong. The rule (leader decision): a non-finite input is refused
 * with XPE_ERR_INVALID_INPUT, the outputs are left untouched, and nothing is "repaired" into a plausible answer.
 *
 * +inf is the case no range comparison stops in any floating-point mode, so it cannot be refused by accident.
 */
#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <vector>

#include "xpe/enhance_advanced/xpe_enhance_advanced_api.h"

namespace {

constexpr float kInf = std::numeric_limits<float>::infinity();
constexpr float kNaN = std::numeric_limits<float>::quiet_NaN();
constexpr int kSize = 128;

std::vector<float> Field() {
    std::vector<float> px(static_cast<size_t>(kSize) * kSize, 100.0f);
    for (int y = 30; y < 98; ++y) {
        for (int x = 30; x < 98; ++x) px[static_cast<size_t>(y) * kSize + x] = 3000.0f;
    }
    return px;
}

XpeImageBuffer Wrap(std::vector<float>& px) {
    XpeImageBuffer b{};
    b.width = kSize;
    b.height = kSize;
    b.format = XPE_PIXEL_FLOAT32;
    b.bitsAllocated = 32;
    b.bitsStored = 32;
    b.data = px.data();
    b.dataSize = px.size() * sizeof(float);
    return b;
}

}  // namespace

class NonFinitePixelsCollimation : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_EQ(XPE_OK, xpe_enhance_advanced_init(nullptr)); }
    void TearDown() override { xpe_enhance_advanced_shutdown(); }
};

TEST_F(NonFinitePixelsCollimation, ARefusedImageLeavesTheOutputsAlone) {
    const float bad[] = {kNaN, kInf, -kInf};
    const char* const name[] = {"NaN", "+inf", "-inf"};
    for (int i = 0; i < 3; ++i) {
        std::vector<float> px = Field();
        px[64u * kSize + 64u] = bad[i];
        XpeImageBuffer img = Wrap(px);
        int32_t x0 = -7, y0 = -7, x1 = -7, y1 = -7;
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr)) << "pixel = " << name[i];
        EXPECT_EQ(-7, x0) << name[i];
        EXPECT_EQ(-7, y0) << name[i];
        EXPECT_EQ(-7, x1) << name[i];
        EXPECT_EQ(-7, y1) << name[i];
    }
}

TEST_F(NonFinitePixelsCollimation, TheSameImageWithoutTheBadPixelStillFindsTheField) {
    std::vector<float> px = Field();
    XpeImageBuffer img = Wrap(px);
    int32_t x0 = -7, y0 = -7, x1 = -7, y1 = -7;
    ASSERT_EQ(XPE_OK, xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr));
    EXPECT_EQ(30, x0);
    EXPECT_EQ(30, y0);
    EXPECT_EQ(97, x1);
    EXPECT_EQ(97, y1);
}

TEST_F(NonFinitePixelsCollimation, ALargeFinitePixelIsDataNotAnError) {
    std::vector<float> px = Field();
    px[64u * kSize + 64u] = 3.0e38f;
    XpeImageBuffer img = Wrap(px);
    int32_t x0 = -7, y0 = -7, x1 = -7, y1 = -7;
    EXPECT_EQ(XPE_OK, xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr));
}
