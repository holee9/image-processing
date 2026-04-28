/**
 * @file test_gain_correct_avx2_parity.cpp
 * @brief TDD RED tests for AVX2 parity: multiple calls with same input must be bit-identical
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 *
 * Tests the new 3-arg API: xpe_gain_correct(input, output, metadata)
 * Domain transition: uint16 input -> float32 output
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"

#include <vector>
#include <cstdint>
#include <cstring>
#include <algorithm>
#include <cstdio>
#include <random>

namespace {

class GainCorrectAVX2ParityTest : public ::testing::Test {
protected:
    std::vector<uint16_t> inputPixels1;
    std::vector<uint16_t> inputPixels2;
    std::vector<float> outputPixels1;
    std::vector<float> outputPixels2;
    XpeImageBuffer input1{};
    XpeImageBuffer input2{};
    XpeImageBuffer output1{};
    XpeImageBuffer output2{};
    XpeImageMetadata metadata{};
    const char* gainPath = "test_gain_avx2_parity_gain.xcal";

    void SetUp() override {
        // Try to initialize, but don't fail if already initialized
        xpe_preprocess_init(nullptr);

        std::mt19937 rng(0x5EED);
        std::uniform_int_distribution<uint16_t> inputDist(0, 4000);

        const size_t pixelCount = 1024 * 768;
        inputPixels1.resize(pixelCount);
        inputPixels2.resize(pixelCount);
        outputPixels1.resize(pixelCount);
        outputPixels2.resize(pixelCount);

        for (size_t i = 0; i < pixelCount; ++i) {
            inputPixels1[i] = inputDist(rng);
            inputPixels2[i] = inputPixels1[i];
        }

        input1.data = inputPixels1.data();
        input1.width = 1024;
        input1.height = 768;
        input1.bitsAllocated = 16;
        input1.bitsStored = 16;
        input1.format = XPE_PIXEL_UINT16;
        input1.dataSize = inputPixels1.size() * sizeof(uint16_t);

        input2.data = inputPixels2.data();
        input2.width = 1024;
        input2.height = 768;
        input2.bitsAllocated = 16;
        input2.bitsStored = 16;
        input2.format = XPE_PIXEL_UINT16;
        input2.dataSize = inputPixels2.size() * sizeof(uint16_t);

        output1.data = outputPixels1.data();
        output1.width = 1024;
        output1.height = 768;
        output1.bitsAllocated = 32;
        output1.bitsStored = 32;
        output1.format = XPE_PIXEL_FLOAT32;
        output1.dataSize = outputPixels1.size() * sizeof(float);

        output2.data = outputPixels2.data();
        output2.width = 1024;
        output2.height = 768;
        output2.bitsAllocated = 32;
        output2.bitsStored = 32;
        output2.format = XPE_PIXEL_FLOAT32;
        output2.dataSize = outputPixels2.size() * sizeof(float);

        memset(&metadata, 0, sizeof(XpeImageMetadata));

        std::vector<float> gainMap(pixelCount);
        for (size_t i = 0; i < pixelCount; ++i) {
            gainMap[i] = 0.5f + static_cast<float>(i % 17) * 0.03125f;
        }
        loadGainMap(gainMap, 1024, 768);
    }

    void TearDown() override {
        std::remove(gainPath);
        std::remove("test_gain_avx2_parity_gain.xcal.tmp");
    }

    void loadGainMap(const std::vector<float>& values, uint32_t width, uint32_t height) {
        std::remove(gainPath);
        std::remove("test_gain_avx2_parity_gain.xcal.tmp");

        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(XCAL_TYPE_GAIN);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width = width;
        hdr.height = height;
        hdr.payload_len = static_cast<uint64_t>(values.size() * sizeof(float));

        ASSERT_EQ(write_xcal_file(gainPath, hdr, nullptr, 0,
                                  reinterpret_cast<const uint8_t*>(values.data()),
                                  hdr.payload_len),
                  XPE_OK);
        ASSERT_EQ(xpe_calib_load_gain(gainPath), XPE_OK);
    }
};

TEST_F(GainCorrectAVX2ParityTest, MultipleCallsAreBitIdentical) {
    ASSERT_EQ(XPE_OK, xpe_gain_correct(&input1, &output1, &metadata));
    ASSERT_EQ(XPE_OK, xpe_gain_correct(&input2, &output2, &metadata));

    for (size_t i = 0; i < outputPixels1.size(); ++i) {
        EXPECT_EQ(outputPixels1[i], outputPixels2[i])
            << "Pixel mismatch at index " << i;
    }
}

TEST_F(GainCorrectAVX2ParityTest, ParityWithNonMultipleStride) {
    const size_t oddSize = 1000;
    std::vector<uint16_t> smallInput1(oddSize);
    std::vector<uint16_t> smallInput2(oddSize);
    std::vector<float> smallOutput1(oddSize);
    std::vector<float> smallOutput2(oddSize);

    std::mt19937 rng(0x5EED);
    std::uniform_int_distribution<uint16_t> dist(0, 4000);
    for (size_t i = 0; i < oddSize; ++i) {
        smallInput1[i] = dist(rng);
        smallInput2[i] = smallInput1[i];
    }

    XpeImageBuffer in1{}, in2{}, out1{}, out2{};
    in1.data = smallInput1.data(); in1.width = 1000; in1.height = 1;
    in1.bitsAllocated = 16; in1.bitsStored = 16; in1.format = XPE_PIXEL_UINT16;
    in1.dataSize = oddSize * sizeof(uint16_t);

    in2 = in1; in2.data = smallInput2.data();
    out1.data = smallOutput1.data(); out1.width = 1000; out1.height = 1;
    out1.bitsAllocated = 32; out1.bitsStored = 32; out1.format = XPE_PIXEL_FLOAT32;
    out1.dataSize = oddSize * sizeof(float);
    out2 = out1; out2.data = smallOutput2.data();

    std::vector<float> gainMap(oddSize);
    for (size_t i = 0; i < oddSize; ++i) {
        gainMap[i] = 0.75f + static_cast<float>(i % 11) * 0.0625f;
    }
    loadGainMap(gainMap, 1000, 1);

    ASSERT_EQ(XPE_OK, xpe_gain_correct(&in1, &out1, &metadata));
    ASSERT_EQ(XPE_OK, xpe_gain_correct(&in2, &out2, &metadata));

    for (size_t i = 0; i < oddSize; ++i) {
        EXPECT_EQ(smallOutput1[i], smallOutput2[i]);
    }
}

} // namespace
