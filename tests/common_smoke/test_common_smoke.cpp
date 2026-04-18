#include "xpe/common/xpe_common_api.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <cstring>

// ---------------------------------------------------------------------------
// xpe_init / xpe_version / xpe_shutdown
// ---------------------------------------------------------------------------

TEST(XpeCommonSmoke, InitAcceptsNullConfig) {
    EXPECT_EQ(xpe_init(nullptr), XPE_OK);
    xpe_shutdown();
}

TEST(XpeCommonSmoke, VersionIsNonNull) {
    ASSERT_NE(xpe_version(), nullptr);
}

TEST(XpeCommonSmoke, VersionIsNonEmpty) {
    ASSERT_GT(std::strlen(xpe_version()), 0u);
}

// ---------------------------------------------------------------------------
// xpe_error_string
// ---------------------------------------------------------------------------

TEST(XpeCommonSmoke, ErrorStringMapsKnownCode) {
    EXPECT_STREQ(xpe_error_string(XPE_ERR_INVALID_INPUT), "Invalid input parameter");
}

// ---------------------------------------------------------------------------
// xpe_get_param_range
// ---------------------------------------------------------------------------

TEST(XpeCommonSmoke, GetParamRangeReturnsOk) {
    float minVal = 0.0f, maxVal = 0.0f, defaultVal = 0.0f;
    EXPECT_EQ(xpe_get_param_range("CHEST", "windowWidth", &minVal, &maxVal, &defaultVal), XPE_OK);
}

TEST(XpeCommonSmoke, GetParamRangeIsOrdered) {
    float minVal = 0.0f, maxVal = 0.0f, defaultVal = 0.0f;
    ASSERT_EQ(xpe_get_param_range("CHEST", "windowWidth", &minVal, &maxVal, &defaultVal), XPE_OK);
    EXPECT_LE(minVal, defaultVal);
    EXPECT_LE(defaultVal, maxVal);
}

// ---------------------------------------------------------------------------
// xpe_alloc_image / xpe_copy_image / xpe_free_image
// ---------------------------------------------------------------------------

class XpeImageTest : public ::testing::Test {
protected:
    XpeImageBuffer source{};
    XpeImageBuffer target{};

    void SetUp() override {
        ASSERT_EQ(xpe_alloc_image(4, 4, XPE_PIXEL_UINT16, &source), XPE_OK);
        ASSERT_EQ(xpe_alloc_image(4, 4, XPE_PIXEL_UINT16, &target), XPE_OK);

        auto* pixels = static_cast<std::uint16_t*>(source.data);
        for (std::size_t i = 0; i < 16; ++i) {
            pixels[i] = static_cast<std::uint16_t>(i * 3);
        }
    }

    void TearDown() override {
        xpe_free_image(&source);
        xpe_free_image(&target);
    }
};

TEST_F(XpeImageTest, AllocSourceHasBackingBuffer) {
    EXPECT_NE(source.data, nullptr);
}

TEST_F(XpeImageTest, AllocSourceHasCorrectDataSize) {
    // 4 x 4 x sizeof(uint16_t) = 32 bytes
    EXPECT_EQ(source.dataSize, 32u);
}

TEST_F(XpeImageTest, CopyImageReturnsOk) {
    EXPECT_EQ(xpe_copy_image(&source, &target), XPE_OK);
}

TEST_F(XpeImageTest, CopyImagePreservesContents) {
    ASSERT_EQ(xpe_copy_image(&source, &target), XPE_OK);
    EXPECT_EQ(std::memcmp(source.data, target.data, source.dataSize), 0);
}

TEST_F(XpeImageTest, CopyImagePreservesDimensions) {
    ASSERT_EQ(xpe_copy_image(&source, &target), XPE_OK);
    EXPECT_EQ(target.width, source.width);
    EXPECT_EQ(target.height, source.height);
}

// ---------------------------------------------------------------------------
// xpe_get_pending_alert_count
// ---------------------------------------------------------------------------

TEST(XpeCommonSmoke, FreshRuntimeHasNoAlerts) {
    EXPECT_EQ(xpe_get_pending_alert_count(), 0u);
}
