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

#include <algorithm>
#include <cstdint>
#include <cstring>
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

// ---- QA-B-181g (Codex #57 sweep): xpe_multiscale_process --------------------------------------------------------
// The worst-input sweep of every image-returning function found a producer beyond the two Codex named: on images at
// +-FLT_MAX the Laplacian detail (a difference of two levels) and the gains leave float, and xpe_multiscale_process
// answered rc=0 with a non-finite image. The result is built in a separate buffer and copied at the end, so the
// refusal is exact: it fires only when the result itself is not finite, and the image is left untouched.
TEST_F(NonFinitePixelsCollimation, MultiscaleRefusesAnImageWhoseResultWouldNotBeFinite) {
    const float fltMax = std::numeric_limits<float>::max();
    for (int pat = 0; pat < 3; ++pat) {
        std::vector<float> px(64u * 64u, pat == 1 ? -fltMax : fltMax);
        if (pat == 2) {
            for (size_t i = 0; i < px.size(); ++i) px[i] = ((i % 64u + i / 64u) & 1u) ? fltMax : -fltMax;
        }
        const std::vector<float> before = px;
        XpeImageBuffer img{};
        img.width = 64; img.height = 64; img.format = XPE_PIXEL_FLOAT32; img.bitsAllocated = 32; img.bitsStored = 32;
        img.data = px.data(); img.dataSize = px.size() * sizeof(float);
        XpeImageMetadata meta{};
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_multiscale_process(&img, &meta, nullptr)) << "pattern " << pat;
        EXPECT_EQ(0, std::memcmp(before.data(), px.data(), px.size() * sizeof(float))) << "pattern " << pat << ": nothing may be written";
    }
}

TEST_F(NonFinitePixelsCollimation, MultiscaleRefusesANonFinitePixelAndStillProcessesAnOrdinaryImage) {
    const float bad[] = {std::numeric_limits<float>::quiet_NaN(), kInf, -kInf};
    for (float b : bad) {
        std::vector<float> px = Field();
        px.resize(64u * 64u, 100.0f);
        std::fill(px.begin(), px.end(), 100.0f);
        px[10] = b;
        const std::vector<float> before = px;
        XpeImageBuffer img{};
        img.width = 64; img.height = 64; img.format = XPE_PIXEL_FLOAT32; img.bitsAllocated = 32; img.bitsStored = 32;
        img.data = px.data(); img.dataSize = px.size() * sizeof(float);
        XpeImageMetadata meta{};
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_multiscale_process(&img, &meta, nullptr)) << "pixel " << b;
        EXPECT_EQ(0, std::memcmp(before.data(), px.data(), px.size() * sizeof(float))) << "pixel " << b;
    }
    std::vector<float> ok(64u * 64u);
    for (size_t i = 0; i < ok.size(); ++i) ok[i] = static_cast<float>(100 + (i * 7) % 900);
    XpeImageBuffer img{};
    img.width = 64; img.height = 64; img.format = XPE_PIXEL_FLOAT32; img.bitsAllocated = 32; img.bitsStored = 32;
    img.data = ok.data(); img.dataSize = ok.size() * sizeof(float);
    XpeImageMetadata meta{};
    ASSERT_EQ(XPE_OK, xpe_multiscale_process(&img, &meta, nullptr));
    for (float v : ok) ASSERT_TRUE(std::isfinite(v));
}
