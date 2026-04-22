/**
 * @file test_defect_correct_avx2_parity.cpp
 * @brief TDD RED tests for AVX2 parity: defect correction must be bit-identical across calls
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 *
 * Tests the new 3-arg API: xpe_defect_correct(img, defectMap, configJsonOrNull)
 */

#include <gtest/gtest.h>
#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <vector>
#include <cstdint>
#include <cstring>
#include <random>

namespace {

class DefectCorrectAVX2ParityTest : public ::testing::Test {
protected:
    std::vector<float> inputPixels1;
    std::vector<float> inputPixels2;
    std::vector<uint16_t> defectMap1;
    std::vector<uint16_t> defectMap2;
    XpeImageBuffer input1{};
    XpeImageBuffer input2{};
    XpeImageBuffer defectMapBuf1{};
    XpeImageBuffer defectMapBuf2{};
    XpeImageMetadata metadata{};

    void SetUp() override {
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));

        std::mt19937 rng(0x5EED);
        std::uniform_int_distribution<uint16_t> inputDist(0, 4000);
        std::uniform_int_distribution<uint16_t> defectDist(0, 100);

        const size_t pixelCount = 1024 * 768;
        inputPixels1.resize(pixelCount);
        inputPixels2.resize(pixelCount);
        defectMap1.resize(pixelCount);
        defectMap2.resize(pixelCount);

        for (size_t i = 0; i < pixelCount; ++i) {
            inputPixels1[i] = static_cast<float>(inputDist(rng));
            inputPixels2[i] = inputPixels1[i];
            defectMap1[i] = (defectDist(rng) < 5) ? 1 : 0;
            defectMap2[i] = defectMap1[i];
        }

        input1.data = inputPixels1.data();
        input1.width = 1024;
        input1.height = 768;
        input1.bitsAllocated = 32;
        input1.bitsStored = 32;
        input1.format = XPE_PIXEL_FLOAT32;
        input1.dataSize = inputPixels1.size() * sizeof(float);

        input2.data = inputPixels2.data();
        input2.width = 1024;
        input2.height = 768;
        input2.bitsAllocated = 32;
        input2.bitsStored = 32;
        input2.format = XPE_PIXEL_FLOAT32;
        input2.dataSize = inputPixels2.size() * sizeof(float);

        defectMapBuf1.data = defectMap1.data();
        defectMapBuf1.width = 1024;
        defectMapBuf1.height = 768;
        defectMapBuf1.bitsAllocated = 16;
        defectMapBuf1.bitsStored = 16;
        defectMapBuf1.format = XPE_PIXEL_UINT16;
        defectMapBuf1.dataSize = defectMap1.size() * sizeof(uint16_t);

        defectMapBuf2.data = defectMap2.data();
        defectMapBuf2.width = 1024;
        defectMapBuf2.height = 768;
        defectMapBuf2.bitsAllocated = 16;
        defectMapBuf2.bitsStored = 16;
        defectMapBuf2.format = XPE_PIXEL_UINT16;
        defectMapBuf2.dataSize = defectMap2.size() * sizeof(uint16_t);

        memset(&metadata, 0, sizeof(XpeImageMetadata));
    }

    void TearDown() override {
        xpe_preprocess_shutdown();
    }
};

TEST_F(DefectCorrectAVX2ParityTest, BilinearMode_MultipleCallsAreBitIdentical) {
    const char* config = "{\"mode\": \"bilinear\"}";

    ASSERT_EQ(XPE_OK, xpe_defect_correct(&input1, &defectMapBuf1, config));
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&input2, &defectMapBuf2, config));

    for (size_t i = 0; i < inputPixels1.size(); ++i) {
        EXPECT_EQ(inputPixels1[i], inputPixels2[i])
            << "Bilinear mismatch at index " << i;
    }
}

TEST_F(DefectCorrectAVX2ParityTest, MedianMode_MultipleCallsAreBitIdentical) {
    const char* config = "{\"mode\": \"median\"}";

    ASSERT_EQ(XPE_OK, xpe_defect_correct(&input1, &defectMapBuf1, config));
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&input2, &defectMapBuf2, config));

    for (size_t i = 0; i < inputPixels1.size(); ++i) {
        EXPECT_EQ(inputPixels1[i], inputPixels2[i])
            << "Median mismatch at index " << i;
    }
}

TEST_F(DefectCorrectAVX2ParityTest, NullConfig_MultipleCallsAreBitIdentical) {
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&input1, &defectMapBuf1, nullptr));
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&input2, &defectMapBuf2, nullptr));

    for (size_t i = 0; i < inputPixels1.size(); ++i) {
        EXPECT_EQ(inputPixels1[i], inputPixels2[i]);
    }
}

} // namespace
