/**
 * @file test_offset_correct.cpp
 * @brief Tests for SWU-1.1: xpe_offset_correct (REQ-P1A-009 to REQ-P1A-011)
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

class OffsetCorrectTest : public ::testing::Test {
protected:
    static constexpr uint32_t W = 4;
    static constexpr uint32_t H = 4;

    std::vector<uint16_t> rawPixels;
    std::vector<uint16_t> outputPixels;
    std::vector<float> offsetPixels;
    XpeImageBuffer input{};
    XpeImageBuffer output{};
    XpeImageMetadata metadata{};
    const char* offsetPath = "test_offset_correct_offset.xcal";

    void SetUp() override {
        xpe_preprocess_init(nullptr);

        rawPixels.assign(W * H, 1000);
        outputPixels.assign(W * H, 0);
        offsetPixels.assign(W * H, 200.0f);

        input.data = rawPixels.data();
        input.width = W;
        input.height = H;
        input.bitsAllocated = 16;
        input.bitsStored = 16;
        input.format = XPE_PIXEL_UINT16;
        input.dataSize = rawPixels.size() * sizeof(uint16_t);

        output.data = outputPixels.data();
        output.width = W;
        output.height = H;
        output.bitsAllocated = 16;
        output.bitsStored = 16;
        output.format = XPE_PIXEL_UINT16;
        output.dataSize = outputPixels.size() * sizeof(uint16_t);

        loadOffsetMap(offsetPixels);
    }

    void TearDown() override {
        std::remove(offsetPath);
        std::remove("test_offset_correct_offset.xcal.tmp");
        xpe_preprocess_shutdown();
    }

    void loadOffsetMap(const std::vector<float>& values) {
        std::remove(offsetPath);
        std::remove("test_offset_correct_offset.xcal.tmp");

        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(XCAL_TYPE_OFFSET);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width = W;
        hdr.height = H;
        hdr.payload_len = static_cast<uint64_t>(values.size() * sizeof(float));

        ASSERT_EQ(write_xcal_file(offsetPath, hdr, nullptr, 0,
                                  reinterpret_cast<const uint8_t*>(values.data()),
                                  hdr.payload_len),
                  XPE_OK);
        ASSERT_EQ(xpe_calib_load_offset(offsetPath), XPE_OK);
    }
};

TEST_F(OffsetCorrectTest, SubtractsOffsetFromRawPixels) {
    ASSERT_EQ(XPE_OK, xpe_offset_correct(&input, &output, &metadata));
    const auto* out = static_cast<const uint16_t*>(output.data);
    EXPECT_EQ(800u, out[0]);
}

TEST_F(OffsetCorrectTest, RoundsHalfUpAfterFloatOffsetSubtraction) {
    offsetPixels[0] = 199.6f;
    offsetPixels[1] = 199.5f;
    loadOffsetMap(offsetPixels);

    ASSERT_EQ(XPE_OK, xpe_offset_correct(&input, &output, &metadata));
    const auto* out = static_cast<const uint16_t*>(output.data);
    EXPECT_EQ(800u, out[0]);
    EXPECT_EQ(801u, out[1]);
}

TEST_F(OffsetCorrectTest, ClampsUnderflowToZero) {
    std::fill(rawPixels.begin(), rawPixels.end(), 100);
    std::fill(offsetPixels.begin(), offsetPixels.end(), 500.0f);
    loadOffsetMap(offsetPixels);

    ASSERT_EQ(XPE_OK, xpe_offset_correct(&input, &output, &metadata));
    const auto* out = static_cast<const uint16_t*>(output.data);
    for (uint32_t i = 0; i < W * H; ++i) {
        EXPECT_EQ(0u, out[i]) << "pixel " << i << " should clamp to 0";
    }
}

TEST_F(OffsetCorrectTest, NullInputReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_offset_correct(nullptr, &output, &metadata));
}

TEST_F(OffsetCorrectTest, NullOutputReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_offset_correct(&input, nullptr, &metadata));
}

TEST_F(OffsetCorrectTest, DimensionMismatchReturnsError) {
    XpeImageBuffer badOutput = output;
    badOutput.width = W + 1;
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL, xpe_offset_correct(&input, &badOutput, &metadata));
}

TEST_F(OffsetCorrectTest, ZeroOffsetLeavesPixelsUnchanged) {
    std::fill(offsetPixels.begin(), offsetPixels.end(), 0.0f);
    loadOffsetMap(offsetPixels);

    ASSERT_EQ(XPE_OK, xpe_offset_correct(&input, &output, &metadata));
    const auto* out = static_cast<const uint16_t*>(output.data);
    EXPECT_EQ(1000u, out[0]);
}

} // namespace
