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
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"

#include <vector>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <random>

namespace {

class DefectCorrectAVX2ParityTest : public ::testing::Test {
protected:
    std::vector<float> imgPixels1;
    std::vector<float> imgPixels2;
    std::vector<float> outputPixels1;
    std::vector<float> outputPixels2;
    std::vector<uint8_t> defectMapData;
    XpeImageBuffer img1{};
    XpeImageBuffer img2{};
    XpeImageBuffer output1{};
    XpeImageBuffer output2{};
    XpeImageMetadata metadata{};
    const char* defectPath = "test_defect_avx2_parity_defect.xcal";

    static constexpr uint32_t W = 512;
    static constexpr uint32_t H = 512;
    static constexpr size_t PIXEL_COUNT = W * H;

    void SetUp() override {
        xpe_preprocess_shutdown();
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));

        std::mt19937 rng(0x5EED);
        std::uniform_real_distribution<float> dist(0.0f, 4000.0f);

        imgPixels1.resize(PIXEL_COUNT);
        imgPixels2.resize(PIXEL_COUNT);
        outputPixels1.resize(PIXEL_COUNT);
        outputPixels2.resize(PIXEL_COUNT);
        defectMapData.resize(PIXEL_COUNT, 0);

        for (size_t i = 0; i < PIXEL_COUNT; ++i) {
            imgPixels1[i] = dist(rng);
            imgPixels2[i] = imgPixels1[i];
        }
        defectMapData[100u * W + 100u] = 1u;
        defectMapData[250u * W + 251u] = 2u;

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
        fillBuf(output1, outputPixels1.data(), W, H, XPE_PIXEL_FLOAT32, 4);
        fillBuf(output2, outputPixels2.data(), W, H, XPE_PIXEL_FLOAT32, 4);
        loadDefectMap(defectMapData, W, H);
    }

    void TearDown() override {
        std::remove(defectPath);
        std::remove("test_defect_avx2_parity_defect.xcal.tmp");
        xpe_preprocess_shutdown();
    }

    void loadDefectMap(const std::vector<uint8_t>& values, uint32_t width, uint32_t height) {
        std::remove(defectPath);
        std::remove("test_defect_avx2_parity_defect.xcal.tmp");

        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION;
        hdr.type = static_cast<uint32_t>(XCAL_TYPE_DEFECT);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_UINT8_MASK);
        hdr.width = width;
        hdr.height = height;
        hdr.payload_len = static_cast<uint64_t>(values.size() * sizeof(uint8_t));

        ASSERT_EQ(write_xcal_file(defectPath, hdr, nullptr, 0,
                                  values.data(), hdr.payload_len),
                  XPE_OK);
        ASSERT_EQ(xpe_calib_load_defect_map(defectPath), XPE_OK);
    }
};

TEST_F(DefectCorrectAVX2ParityTest, MultipleCallsAreBitIdentical) {
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&img1, &output1, &metadata));
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&img2, &output2, &metadata));

    for (size_t i = 0; i < outputPixels1.size(); ++i) {
        EXPECT_EQ(outputPixels1[i], outputPixels2[i])
            << "Pixel mismatch at index " << i;
    }
}

TEST_F(DefectCorrectAVX2ParityTest, ParityWithNonMultipleStride) {
    const size_t oddSize = 1000;
    std::vector<float> smallImg1(oddSize);
    std::vector<float> smallImg2(oddSize);
    std::vector<float> smallOut1(oddSize);
    std::vector<float> smallOut2(oddSize);
    std::vector<uint8_t> smallDefect(oddSize, 0);

    std::mt19937 rng(0x5EED);
    std::uniform_real_distribution<float> dist(0.0f, 4000.0f);
    for (size_t i = 0; i < oddSize; ++i) {
        smallImg1[i] = dist(rng);
        smallImg2[i] = smallImg1[i];
    }
    smallDefect[17] = 1u;
    smallDefect[999] = 2u;

    XpeImageBuffer in1{}, in2{}, out1{}, out2{};
    in1.data = smallImg1.data(); in1.width = 1000; in1.height = 1;
    in1.bitsAllocated = 32; in1.bitsStored = 32; in1.format = XPE_PIXEL_FLOAT32;
    in1.dataSize = oddSize * sizeof(float);

    in2 = in1; in2.data = smallImg2.data();

    out1.data = smallOut1.data(); out1.width = 1000; out1.height = 1;
    out1.bitsAllocated = 32; out1.bitsStored = 32; out1.format = XPE_PIXEL_FLOAT32;
    out1.dataSize = oddSize * sizeof(float);
    out2 = out1; out2.data = smallOut2.data();

    loadDefectMap(smallDefect, 1000, 1);

    ASSERT_EQ(XPE_OK, xpe_defect_correct(&in1, &out1, &metadata));
    ASSERT_EQ(XPE_OK, xpe_defect_correct(&in2, &out2, &metadata));

    for (size_t i = 0; i < oddSize; ++i) {
        EXPECT_EQ(smallOut1[i], smallOut2[i]);
    }
}

} // namespace
