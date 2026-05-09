#include <gtest/gtest.h>

#include "xpe/gsvg/gsvg_api.h"

#include <algorithm>
#include <cstdint>
#include <vector>

namespace {

constexpr int kWidth = 8;
constexpr int kHeight = 6;
constexpr int kCount = kWidth * kHeight;

std::vector<uint16_t> make_periodic_row_shadow()
{
    std::vector<uint16_t> image(kCount);
    for (int y = 0; y < kHeight; ++y) {
        const int rowOffset = (y % 2 == 0) ? 20 : -20;
        for (int x = 0; x < kWidth; ++x) {
            image[static_cast<size_t>(y) * kWidth + x] =
                static_cast<uint16_t>(1000 + rowOffset + x);
        }
    }
    return image;
}

double row_mean(const std::vector<uint16_t>& image, int y)
{
    double acc = 0.0;
    for (int x = 0; x < kWidth; ++x) {
        acc += image[static_cast<size_t>(y) * kWidth + x];
    }
    return acc / static_cast<double>(kWidth);
}

} // namespace

TEST(GsvgCoverage, VersionIsStableAndNonEmpty)
{
    const char* version = xpe_gsvg_version();
    ASSERT_NE(version, nullptr);
    EXPECT_STRNE(version, "");
    EXPECT_STREQ(version, xpe_gsvg_version());
}

TEST(GsvgCoverage, GridSuppressionReducesPeriodicRowMeanDeviation)
{
    const auto src = make_periodic_row_shadow();
    std::vector<uint16_t> dst(kCount, 0);

    void* handle = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&handle, "{\"grid_suppression\":true}"), XPE_OK);
    ASSERT_NE(handle, nullptr);

    ASSERT_EQ(xpe_gsvg_process(handle, src.data(), dst.data(), kWidth, kHeight, nullptr),
              XPE_OK);

    const double beforeDeviation =
        std::abs(row_mean(src, 0) - row_mean(src, 1));
    const double afterDeviation =
        std::abs(row_mean(dst, 0) - row_mean(dst, 1));

    EXPECT_GT(beforeDeviation, 30.0);
    EXPECT_LE(afterDeviation, 1.0);

    EXPECT_EQ(xpe_gsvg_shutdown(handle), XPE_OK);
}

TEST(GsvgCoverage, VignetteGainRoundsAndClampsToUint16Range)
{
    const std::vector<uint16_t> src = {0, 1, 2, 100, 32768, 65000};
    const std::vector<float> gain = {-1.0f, 1.0f, 1.25f, 0.5f, 2.0f, 2.0f};
    std::vector<uint16_t> dst(src.size(), 0);

    void* handle = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&handle, "{\"vignette_correction\":true}"), XPE_OK);
    ASSERT_NE(handle, nullptr);

    ASSERT_EQ(xpe_gsvg_process(handle,
                               src.data(),
                               dst.data(),
                               static_cast<int>(src.size()),
                               1,
                               gain.data()),
              XPE_OK);

    const std::vector<uint16_t> expected = {0, 1, 3, 50, 65535, 65535};
    EXPECT_EQ(dst, expected);

    EXPECT_EQ(xpe_gsvg_shutdown(handle), XPE_OK);
}
