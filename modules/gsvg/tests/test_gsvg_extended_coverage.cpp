#include <gtest/gtest.h>

#include "xpe/gsvg/gsvg_api.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace {

constexpr int kWidth = 5;
constexpr int kHeight = 3;
constexpr int kCount = kWidth * kHeight;

std::vector<uint16_t> make_gradient()
{
    std::vector<uint16_t> image(kCount);
    for (int i = 0; i < kCount; ++i) {
        image[static_cast<size_t>(i)] = static_cast<uint16_t>(2000 + i * 3);
    }
    return image;
}

} // namespace

TEST(GsvgExtendedCoverage, WhitespaceJsonBooleanParsingEnablesVignette)
{
    const auto src = make_gradient();
    const std::vector<float> gain(kCount, 0.5f);
    std::vector<uint16_t> dst(kCount, 0);

    void* handle = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&handle,
                            "{\n  \"vignette_correction\"  :  true,\n"
                            "  \"grid_suppression\" : false\n}"),
              XPE_OK);
    ASSERT_NE(handle, nullptr);

    ASSERT_EQ(xpe_gsvg_process(handle, src.data(), dst.data(), kWidth, kHeight, gain.data()),
              XPE_OK);

    for (int i = 0; i < kCount; ++i) {
        const auto expected = static_cast<uint16_t>(
            static_cast<float>(src[static_cast<size_t>(i)]) * 0.5f + 0.5f);
        EXPECT_EQ(dst[static_cast<size_t>(i)], expected) << "pixel " << i;
    }

    EXPECT_EQ(xpe_gsvg_shutdown(handle), XPE_OK);
}

TEST(GsvgExtendedCoverage, ExplicitFalseFlagsForcePassThroughEvenWithGainMap)
{
    const auto src = make_gradient();
    const std::vector<float> gain(kCount, 3.0f);
    std::vector<uint16_t> dst(kCount, 0);

    void* handle = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&handle,
                            "{\"vignette_correction\":false,"
                            "\"grid_suppression\":false}"),
              XPE_OK);
    ASSERT_NE(handle, nullptr);

    ASSERT_EQ(xpe_gsvg_process(handle, src.data(), dst.data(), kWidth, kHeight, gain.data()),
              XPE_OK);
    EXPECT_EQ(std::memcmp(dst.data(), src.data(), kCount * sizeof(uint16_t)), 0);

    EXPECT_EQ(xpe_gsvg_shutdown(handle), XPE_OK);
}

TEST(GsvgExtendedCoverage, MissingGainMapDisablesOnlyVignetteStep)
{
    const auto src = make_gradient();
    std::vector<uint16_t> dst(kCount, 0);

    void* handle = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&handle,
                            "{\"vignette_correction\":true,"
                            "\"grid_suppression\":false}"),
              XPE_OK);
    ASSERT_NE(handle, nullptr);

    ASSERT_EQ(xpe_gsvg_process(handle, src.data(), dst.data(), kWidth, kHeight, nullptr),
              XPE_OK);
    EXPECT_EQ(std::memcmp(dst.data(), src.data(), kCount * sizeof(uint16_t)), 0);

    EXPECT_EQ(xpe_gsvg_shutdown(handle), XPE_OK);
}

TEST(GsvgExtendedCoverage, VignetteAndGridEnabledOnFlatRowsPreservesDoubledImage)
{
    const std::vector<uint16_t> src(kCount, 1000);
    const std::vector<float> gain(kCount, 2.0f);
    std::vector<uint16_t> dst(kCount, 0);

    void* handle = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&handle,
                            "{\"vignette_correction\":true,"
                            "\"grid_suppression\":true}"),
              XPE_OK);
    ASSERT_NE(handle, nullptr);

    ASSERT_EQ(xpe_gsvg_process(handle, src.data(), dst.data(), kWidth, kHeight, gain.data()),
              XPE_OK);

    for (const auto pixel : dst) {
        EXPECT_EQ(pixel, 2000);
    }

    EXPECT_EQ(xpe_gsvg_shutdown(handle), XPE_OK);
}
