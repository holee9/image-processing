#include <gtest/gtest.h>

#include "xpe/gsvg/gsvg_api.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace {

constexpr int kWidth = 4;
constexpr int kHeight = 4;
constexpr int kCount = kWidth * kHeight;

std::vector<uint16_t> make_image()
{
    std::vector<uint16_t> image(kCount);
    for (int i = 0; i < kCount; ++i) {
        image[static_cast<size_t>(i)] = static_cast<uint16_t>(100 + i);
    }
    return image;
}

} // namespace

TEST(GsvgEdgeCases, InitRejectsNullOutputHandle)
{
    EXPECT_EQ(xpe_gsvg_init(nullptr, "{}"), XPE_ERR_INVALID_INPUT);
}

TEST(GsvgEdgeCases, ProcessRejectsNullImagePointers)
{
    void* handle = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&handle, "{}"), XPE_OK);
    ASSERT_NE(handle, nullptr);

    auto src = make_image();
    std::vector<uint16_t> dst(kCount, 0);

    EXPECT_EQ(xpe_gsvg_process(handle, nullptr, dst.data(), kWidth, kHeight, nullptr),
              XPE_ERR_INVALID_INPUT);
    EXPECT_EQ(xpe_gsvg_process(handle, src.data(), nullptr, kWidth, kHeight, nullptr),
              XPE_ERR_INVALID_INPUT);

    EXPECT_EQ(xpe_gsvg_shutdown(handle), XPE_OK);
}

TEST(GsvgEdgeCases, ProcessRejectsNonPositiveDimensions)
{
    void* handle = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&handle, "{}"), XPE_OK);
    ASSERT_NE(handle, nullptr);

    auto src = make_image();
    std::vector<uint16_t> dst(kCount, 0);

    EXPECT_EQ(xpe_gsvg_process(handle, src.data(), dst.data(), 0, kHeight, nullptr),
              XPE_ERR_INVALID_INPUT);
    EXPECT_EQ(xpe_gsvg_process(handle, src.data(), dst.data(), kWidth, 0, nullptr),
              XPE_ERR_INVALID_INPUT);
    EXPECT_EQ(xpe_gsvg_process(handle, src.data(), dst.data(), -1, kHeight, nullptr),
              XPE_ERR_INVALID_INPUT);
    EXPECT_EQ(xpe_gsvg_process(handle, src.data(), dst.data(), kWidth, -1, nullptr),
              XPE_ERR_INVALID_INPUT);

    EXPECT_EQ(xpe_gsvg_shutdown(handle), XPE_OK);
}

TEST(GsvgEdgeCases, MalformedConfigFallsBackToPassThrough)
{
    const auto src = make_image();
    std::vector<uint16_t> dst(kCount, 0);

    void* handle = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&handle, "{\"vignette_correction\":maybe}"), XPE_OK);
    ASSERT_NE(handle, nullptr);

    ASSERT_EQ(xpe_gsvg_process(handle, src.data(), dst.data(), kWidth, kHeight, nullptr),
              XPE_OK);
    EXPECT_EQ(std::memcmp(dst.data(), src.data(), kCount * sizeof(uint16_t)), 0);

    EXPECT_EQ(xpe_gsvg_shutdown(handle), XPE_OK);
}

TEST(GsvgEdgeCases, InPlaceVignetteProcessingIsSupported)
{
    std::vector<uint16_t> image = {1, 2, 3, 4};
    const std::vector<float> gain(image.size(), 2.0f);

    void* handle = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&handle, "{\"vignette_correction\":true}"), XPE_OK);
    ASSERT_NE(handle, nullptr);

    ASSERT_EQ(xpe_gsvg_process(handle,
                               image.data(),
                               image.data(),
                               static_cast<int>(image.size()),
                               1,
                               gain.data()),
              XPE_OK);

    const std::vector<uint16_t> expected = {2, 4, 6, 8};
    EXPECT_EQ(image, expected);

    EXPECT_EQ(xpe_gsvg_shutdown(handle), XPE_OK);
}
