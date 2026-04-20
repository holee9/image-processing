/**
 * @file test_readout_validate.cpp
 * @brief Tests for SWU-1.9: xpe_validate_readout_artifact (REQ-P1A-001 to REQ-P1A-004)
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <vector>

namespace {

class ReadoutValidateTest : public ::testing::Test {
protected:
    static constexpr uint32_t W = 64;
    static constexpr uint32_t H = 64;

    std::vector<uint16_t> pixels;
    XpeImageBuffer img{};
    XpeImageMetadata meta{};
    bool dropped{false};
    bool nonuniform{false};

    void SetUp() override {
        pixels.assign(W * H, 32768); // mid-range clean image
        img.data          = pixels.data();
        img.width         = W;
        img.height        = H;
        img.bitsAllocated = 16;
        img.bitsStored    = 16;
        img.format        = XPE_PIXEL_UINT16;
        img.dataSize      = pixels.size() * sizeof(uint16_t);
        dropped = false;
        nonuniform = false;
    }
};

// REQ-P1A-001: clean image returns XPE_OK with no artifacts detected
TEST_F(ReadoutValidateTest, CleanImageReturnsOkNoArtifacts) {
    ASSERT_EQ(XPE_OK, xpe_validate_readout_artifact(&img, &meta, &dropped, &nonuniform));
    EXPECT_FALSE(dropped);
    EXPECT_FALSE(nonuniform);
}

// REQ-P1A-004: severely noisy row still returns XPE_OK
TEST_F(ReadoutValidateTest, HighNoiseStillReturnsOk) {
    for (uint32_t x = 0; x < W; ++x) pixels[x] = 65535;
    EXPECT_EQ(XPE_OK, xpe_validate_readout_artifact(&img, &meta, &dropped, &nonuniform));
    EXPECT_TRUE(nonuniform); // noisy row detected as nonuniform gain
}

// NULL rawImg returns error
TEST_F(ReadoutValidateTest, NullImageReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_validate_readout_artifact(nullptr, &meta, &dropped, &nonuniform));
}

// NULL metadata returns error
TEST_F(ReadoutValidateTest, NullMetadataReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_validate_readout_artifact(&img, nullptr, &dropped, &nonuniform));
}

// NULL output flags return error
TEST_F(ReadoutValidateTest, NullDroppedOutReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_validate_readout_artifact(&img, &meta, nullptr, &nonuniform));
}

TEST_F(ReadoutValidateTest, NullNonuniformOutReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_validate_readout_artifact(&img, &meta, &dropped, nullptr));
}

} // namespace
