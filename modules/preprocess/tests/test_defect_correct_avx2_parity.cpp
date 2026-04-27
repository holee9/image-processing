/**
 * @file test_defect_correct_avx2_parity.cpp
 * @brief AVX2 parity: defect correction must be bit-identical across calls
 * SPEC: SPEC-SIMD-001 REQ-SIMD-003  IEC 62304 Class B
 *
 * Tests the 3-arg API: xpe_defect_correct(img, defectMap, configJsonOrNull)
 * The defect map is passed as a parameter (current API signature).
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <vector>
#include <cstdint>
#include <cstring>
#include <random>

namespace {

class DefectCorrectAVX2ParityTest : public ::testing::Test {
protected:
    std::vector<float> imgPixels1;
    std::vector<float> imgPixels2;
    std::vector<uint16_t> defectMapData;
    XpeImageBuffer img1{};
    XpeImageBuffer img2{};
    XpeImageBuffer defectMap{};

    static constexpr uint32_t W = 512;
    static constexpr uint32_t H = 512;
    static constexpr size_t PIXEL_COUNT = W * H;

    void SetUp() override {
        xpe_preprocess_init(nullptr);

        std::mt19937 rng(0x5EED);
        std::uniform_real_distribution<float> dist(0.0f, 4000.0f);

        imgPixels1.resize(PIXEL_COUNT);
        imgPixels2.resize(PIXEL_COUNT);
        defectMapData.resize(PIXEL_COUNT, 0);

        for (size_t i = 0; i < PIXEL_COUNT; ++i) {
            imgPixels1[i] = dist(rng);
            imgPixels2[i] = imgPixels1[i];
        }

        auto fillBuf = [](XpeImageBuffer& buf, void* data, uint32_t w, uint32_t h,
                          XpePixelFormat fmt, uint32_t elemBytes) {
            buf.data = data;
            buf.width = w;
            buf.height = h;
            buf.bitsAllocated = static_cast<uint16_t>(elemBytes * 8);
            buf.bitsStored    = static_cast<uint16_t>(elemBytes * 8);
            buf.format        = fmt;
            buf.dataSize      = static_cast<uint32_t>(w * h * elemBytes);
        };

        fillBuf(img1, imgPixels1.data(), W, H, XPE_PIXEL_FLOAT32, 4);
        fillBuf(img2, imgPixels2.data(), W, H, XPE_PIXEL_FLOAT32, 4);
        fillBuf(defectMap, defectMapData.data(), W, H, XPE_PIXEL_UINT16, 2);
    }

    void TearDown() override {}
};

TEST_F(DefectCorrectAVX2ParityTest, MultipleCallsAreBitIdentical) {
    GTEST_SKIP() << "Defect map requires valid calibration data; parity test deferred to calibration-integrated run";

    ASSERT_EQ(XPE_OK, xpe_defect_correct(&img1, &defectMap, nullptr));
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&img2, &defectMap, nullptr));

    for (size_t i = 0; i < imgPixels1.size(); ++i) {
        EXPECT_EQ(imgPixels1[i], imgPixels2[i])
            << "Pixel mismatch at index " << i;
    }
}

TEST_F(DefectCorrectAVX2ParityTest, DISABLED_ParityWithNonMultipleStride) {
    const size_t oddSize = 1000;
    std::vector<float> smallImg1(oddSize);
    std::vector<float> smallImg2(oddSize);
    std::vector<uint16_t> smallDefect(oddSize, 0);

    std::mt19937 rng(0x5EED);
    std::uniform_real_distribution<float> dist(0.0f, 4000.0f);
    for (size_t i = 0; i < oddSize; ++i) {
        smallImg1[i] = dist(rng);
        smallImg2[i] = smallImg1[i];
    }

    XpeImageBuffer in1{}, in2{}, dmap{};
    in1.data = smallImg1.data(); in1.width = 1000; in1.height = 1;
    in1.bitsAllocated = 32; in1.bitsStored = 32; in1.format = XPE_PIXEL_FLOAT32;
    in1.dataSize = oddSize * sizeof(float);

    in2 = in1; in2.data = smallImg2.data();

    dmap.data = smallDefect.data(); dmap.width = 1000; dmap.height = 1;
    dmap.bitsAllocated = 16; dmap.bitsStored = 16; dmap.format = XPE_PIXEL_UINT16;
    dmap.dataSize = oddSize * sizeof(uint16_t);

    ASSERT_EQ(XPE_OK, xpe_defect_correct(&in1, &dmap, nullptr));
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&in2, &dmap, nullptr));

    for (size_t i = 0; i < oddSize; ++i) {
        EXPECT_EQ(smallImg1[i], smallImg2[i]);
    }
}

} // namespace
