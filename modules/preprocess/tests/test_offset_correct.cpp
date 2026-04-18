/**
 * @file test_offset_correct.cpp
 * @brief TDD RED tests for SWU-1.1: xpe_offset_correct (REQ-P1A-009 to REQ-P1A-011)
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include <gtest/gtest.h>
#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <vector>
#include <cstdint>

namespace {

// Test fixture with a 4x4 uint16 image and matching offset map
class OffsetCorrectTest : public ::testing::Test {
protected:
    static constexpr uint32_t W = 4;
    static constexpr uint32_t H = 4;

    std::vector<uint16_t> rawPixels;
    std::vector<uint16_t> offsetPixels;
    XpeImageBuffer img{};
    XpeImageBuffer offsetMap{};

    void SetUp() override {
        rawPixels.assign(W * H, 1000);
        offsetPixels.assign(W * H, 200);

        img.data          = rawPixels.data();
        img.width         = W;
        img.height        = H;
        img.bitsAllocated = 16;
        img.bitsStored    = 16;
        img.format        = XPE_PIXEL_UINT16;
        img.dataSize      = rawPixels.size() * sizeof(uint16_t);

        offsetMap.data          = offsetPixels.data();
        offsetMap.width         = W;
        offsetMap.height        = H;
        offsetMap.bitsAllocated = 16;
        offsetMap.bitsStored    = 16;
        offsetMap.format        = XPE_PIXEL_UINT16;
        offsetMap.dataSize      = offsetPixels.size() * sizeof(uint16_t);
    }
};

// REQ-P1A-009: corrected[i] = clamp(raw[i] - offsetMap[i], 0)
TEST_F(OffsetCorrectTest, SubtractsOffsetFromRawPixels) {
    ASSERT_EQ(XPE_OK, xpe_offset_correct(&img, &offsetMap));
    const auto* out = static_cast<const uint16_t*>(img.data);
    EXPECT_EQ(800u, out[0]);  // 1000 - 200 = 800
}

// REQ-P1A-011: clamp — no pixel underflows below 0
TEST_F(OffsetCorrectTest, ClampsUnderflowToZero) {
    std::fill(rawPixels.begin(), rawPixels.end(), 100);
    std::fill(offsetPixels.begin(), offsetPixels.end(), 500); // offset > raw
    ASSERT_EQ(XPE_OK, xpe_offset_correct(&img, &offsetMap));
    const auto* out = static_cast<const uint16_t*>(img.data);
    for (uint32_t i = 0; i < W * H; ++i)
        EXPECT_EQ(0u, out[i]) << "pixel " << i << " should clamp to 0";
}

// NULL img returns XPE_ERR_INVALID_INPUT
TEST_F(OffsetCorrectTest, NullImgReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_offset_correct(nullptr, &offsetMap));
}

// NULL offsetMap returns XPE_ERR_INVALID_INPUT
TEST_F(OffsetCorrectTest, NullOffsetMapReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_offset_correct(&img, nullptr));
}

// Dimension mismatch returns XPE_ERR_INVALID_INPUT (REQ-P1A-009)
TEST_F(OffsetCorrectTest, DimensionMismatchReturnsError) {
    XpeImageBuffer badMap = offsetMap;
    badMap.width = W + 1;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_offset_correct(&img, &badMap));
}

// Zero offset map: pixels unchanged
TEST_F(OffsetCorrectTest, ZeroOffsetLeavesPixelsUnchanged) {
    std::fill(offsetPixels.begin(), offsetPixels.end(), 0);
    ASSERT_EQ(XPE_OK, xpe_offset_correct(&img, &offsetMap));
    const auto* out = static_cast<const uint16_t*>(img.data);
    EXPECT_EQ(1000u, out[0]);
}

} // namespace
