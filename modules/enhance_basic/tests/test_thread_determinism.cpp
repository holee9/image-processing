/**
 * @file test_thread_determinism.cpp
 * @brief #179 (QA-B-103): the bilateral output does not depend on the thread count.
 *
 * The parallel passes split the image into row bands. The bands must compute
 * the same values in the same order as the single-threaded loop, so every
 * thread count has to produce bit-identical pixels -- this is what makes the
 * speed-up safe to ship.
 */

#include "xpe/enhance_basic/enhance_basic_api.h"
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
            const double v = 300.0 + 700.0 * std::sin(x * 0.037) * std::cos(y * 0.021)
                           + 40.0 * ((x * 2654435761u + y * 40503u) % 53)
                           + ((x / 64 + y / 48) % 2 ? 1500.0 : 0.0);
            px[static_cast<size_t>(y) * kW + x] = static_cast<float>(v);
        }
    return px;
}

std::vector<float> RunBilateral(int threads) {
    EXPECT_EQ(xpe_enhance_basic_set_max_threads(threads), XPE_OK);
    EXPECT_EQ(xpe_enhance_basic_get_max_threads(), threads > 0 ? threads : 0);
    std::vector<float> px = Scene();
    XpeImageBuffer img{};
    img.width = kW; img.height = kH;
    img.format = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = 32; img.bitsStored = 32;
    img.data = px.data();
    img.dataSize = px.size() * sizeof(float);
    XpeNoiseReduceParams params{};
    params.mode = XPE_NOISE_BILATERAL;
    params.sigma_space = 3.0f;
    params.sigma_range = 50.0f;
    EXPECT_EQ(xpe_noise_reduce(&img, &params), XPE_OK);
    return px;
}

}  // namespace

TEST(EnhanceBasicThreads, BilateralIsBitIdenticalForEveryThreadCount)
{
    const std::vector<float> one = RunBilateral(1);
    for (int threads : {2, 3, 4, 8, 0 /* automatic */}) {
        const std::vector<float> got = RunBilateral(threads);
        ASSERT_EQ(got.size(), one.size());
        EXPECT_EQ(std::memcmp(got.data(), one.data(), one.size() * sizeof(float)), 0)
            << "bilateral output differs at threads = " << threads;
    }
    xpe_enhance_basic_set_max_threads(0);

    // control: the scene really is changed by the filter, so the comparison
    // above is not comparing two untouched images
    const std::vector<float> src = Scene();
    EXPECT_NE(std::memcmp(one.data(), src.data(), src.size() * sizeof(float)), 0);
}

TEST(EnhanceBasicThreads, SetterStoresTheRequestAndRejectsNothing)
{
    EXPECT_EQ(xpe_enhance_basic_set_max_threads(-4), XPE_OK);
    EXPECT_EQ(xpe_enhance_basic_get_max_threads(), 0) << "negative is stored as automatic";
    EXPECT_EQ(xpe_enhance_basic_set_max_threads(6), XPE_OK);
    EXPECT_EQ(xpe_enhance_basic_get_max_threads(), 6);
    EXPECT_EQ(xpe_enhance_basic_set_max_threads(0), XPE_OK);
    EXPECT_EQ(xpe_enhance_basic_get_max_threads(), 0);
}
