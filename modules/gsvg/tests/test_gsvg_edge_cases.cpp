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

// ---------------------------------------------------------------------------
// #142 (QA-B-41): the empty-image contract.
//
// Like ai, this module already held it: xpe_gsvg_process rejects width <= 0,
// height <= 0, and a NULL src or dst (gsvg.cpp:208-210). There was no RED phase
// and no fix. These cases turn the rule from "the code happens to do it" into
// an assertion.
//
// gsvg is the one post module whose entry point takes loose dimensions rather
// than an XpeImageBuffer, so "empty" here also covers NEGATIVE dimensions --
// a shape the struct-based modules cannot express, and one that would index a
// buffer backwards if it were let through.
// ---------------------------------------------------------------------------

TEST(GsvgEmptyImageContract, ZeroAndNegativeDimensionsAreInvalidInput)
{
    void* handle = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&handle, nullptr), XPE_OK);

    std::vector<uint16_t> src = make_image();
    std::vector<uint16_t> dst(static_cast<size_t>(kCount), 0);

    EXPECT_EQ(xpe_gsvg_process(handle, src.data(), dst.data(), 0, kHeight, nullptr),
              XPE_ERR_INVALID_INPUT) << "width == 0 accepted";
    EXPECT_EQ(xpe_gsvg_process(handle, src.data(), dst.data(), kWidth, 0, nullptr),
              XPE_ERR_INVALID_INPUT) << "height == 0 accepted";
    EXPECT_EQ(xpe_gsvg_process(handle, src.data(), dst.data(), -1, kHeight, nullptr),
              XPE_ERR_INVALID_INPUT) << "negative width accepted";
    EXPECT_EQ(xpe_gsvg_process(handle, src.data(), dst.data(), kWidth, -1, nullptr),
              XPE_ERR_INVALID_INPUT) << "negative height accepted";

    EXPECT_EQ(xpe_gsvg_shutdown(handle), XPE_OK);
}

TEST(GsvgEmptyImageContract, NullPixelPointersAreInvalidInput)
{
    void* handle = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&handle, nullptr), XPE_OK);

    std::vector<uint16_t> src = make_image();
    std::vector<uint16_t> dst(static_cast<size_t>(kCount), 0);

    EXPECT_EQ(xpe_gsvg_process(handle, nullptr, dst.data(), kWidth, kHeight, nullptr),
              XPE_ERR_INVALID_INPUT) << "NULL src accepted";
    EXPECT_EQ(xpe_gsvg_process(handle, src.data(), nullptr, kWidth, kHeight, nullptr),
              XPE_ERR_INVALID_INPUT) << "NULL dst accepted";

    EXPECT_EQ(xpe_gsvg_shutdown(handle), XPE_OK);
}

// The complement: without it a guard that rejected everything would satisfy
// both cases above.
TEST(GsvgEmptyImageContract, ValidImageStillAccepted)
{
    void* handle = nullptr;
    ASSERT_EQ(xpe_gsvg_init(&handle, nullptr), XPE_OK);

    std::vector<uint16_t> src = make_image();
    std::vector<uint16_t> dst(static_cast<size_t>(kCount), 0);

    EXPECT_EQ(xpe_gsvg_process(handle, src.data(), dst.data(), kWidth, kHeight, nullptr),
              XPE_OK);
    EXPECT_EQ(std::memcmp(src.data(), dst.data(), src.size() * sizeof(uint16_t)), 0)
        << "pass-through config must copy src to dst unchanged";

    EXPECT_EQ(xpe_gsvg_shutdown(handle), XPE_OK);
}
