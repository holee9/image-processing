/**
 * @file test_thread_determinism.cpp
 * @brief #179 (QA-B-103): the fractional output does not depend on the thread count.
 *
 * The fractional passes (sanitize, the two convolutions, the gradient combine
 * and the SAF-100 overshoot limiting) run on row bands. Each band reads only
 * inputs no band writes, so every thread count must give bit-identical pixels.
 */

#include "xpe/enhance_advanced/xpe_enhance_advanced_api.h"
#include "xpe/common/xpe_types.h"

#include "gtest/gtest.h"

#include <cmath>
#include <cstring>
#include <vector>

namespace {

constexpr int kW = 512, kH = 384;   // not a multiple of the thread counts used

std::vector<float> Scene() {
    std::vector<float> px(static_cast<size_t>(kW) * kH);
    for (int y = 0; y < kH; ++y)
        for (int x = 0; x < kW; ++x) {
            const double v = 500.0 + 900.0 * std::sin(x * 0.031) * std::cos(y * 0.017)
                           + 37.0 * ((x * 2246822519u + y * 3266489917u) % 61)
                           + ((x / 57 + y / 39) % 2 ? 2500.0 : 0.0);
            px[static_cast<size_t>(y) * kW + x] = static_cast<float>(v);
        }
    return px;
}

std::vector<float> RunFractional(int threads, float order) {
    EXPECT_EQ(xpe_enhance_advanced_set_max_threads(threads), XPE_OK);
    std::vector<float> px = Scene();
    XpeImageBuffer img{};
    img.width = kW; img.height = kH;
    img.format = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = 32; img.bitsStored = 32;
    img.data = px.data();
    img.dataSize = px.size() * sizeof(float);
    EXPECT_EQ(xpe_fractional_process(&img, order, nullptr), XPE_OK);
    return px;
}

class AdvThreads : public ::testing::Test {
protected:
    void SetUp() override { ASSERT_EQ(xpe_enhance_advanced_init(nullptr), XPE_OK); }
    void TearDown() override {
        xpe_enhance_advanced_set_max_threads(0);
        xpe_enhance_advanced_shutdown();
    }
};

}  // namespace

TEST_F(AdvThreads, FractionalIsBitIdenticalForEveryThreadCount)
{
    for (float order : {0.6f, 1.0f, 1.8f}) {
        const std::vector<float> one = RunFractional(1, order);
        for (int threads : {2, 3, 4, 8, 0 /* automatic */}) {
            const std::vector<float> got = RunFractional(threads, order);
            ASSERT_EQ(got.size(), one.size());
            EXPECT_EQ(std::memcmp(got.data(), one.data(), one.size() * sizeof(float)), 0)
                << "order " << order << " differs at threads = " << threads;
        }
        const std::vector<float> src = Scene();   // control: the filter did change the image
        EXPECT_NE(std::memcmp(one.data(), src.data(), src.size() * sizeof(float)), 0) << order;
    }
}

TEST_F(AdvThreads, SetterStoresTheRequest)
{
    EXPECT_EQ(xpe_enhance_advanced_set_max_threads(-2), XPE_OK);
    EXPECT_EQ(xpe_enhance_advanced_get_max_threads(), 0);
    EXPECT_EQ(xpe_enhance_advanced_set_max_threads(5), XPE_OK);
    EXPECT_EQ(xpe_enhance_advanced_get_max_threads(), 5);
    EXPECT_EQ(xpe_enhance_advanced_set_max_threads(0), XPE_OK);
    EXPECT_EQ(xpe_enhance_advanced_get_max_threads(), 0);
}
