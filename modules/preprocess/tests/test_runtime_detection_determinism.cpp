/**
 * @file test_runtime_detection_determinism.cpp
 * @brief Runtime defect detection is deterministic across repeated calls.
 * SPEC: SPEC-SIMD-001 REQ-SIMD-004  IEC 62304 Class B
 *
 * Renamed by QA-A-42 (#144). The file was called
 * test_runtime_detection_avx2_parity.cpp and its own description claimed to
 * confirm "deterministic AVX2 dispatch", but there is no AVX2 dispatch to
 * confirm: xpe_defect_detect_runtime calls the scalar DetectDefectivePixel in a
 * double loop, and a grep for avx2 / __m256 / immintrin across
 * src/runtime_detection.cpp and include/runtime_detection.h returns nothing
 * (measured, QA-A-41: reports/lane-pre/QA-A-41/a41-avx2-evidence.txt).
 *
 * What the four cases actually assert is worth keeping: the same input produces
 * the same defect map every time, and different inputs produce different maps.
 * That is a determinism contract, and it is what the name now says. If an AVX2
 * path is ever added, THIS suite becomes the natural place to assert scalar/AVX2
 * parity -- but it must then compare the two paths, not one path against itself.
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

class RuntimeDetectDeterminismTest : public ::testing::Test {
protected:
    std::vector<float>   inputPixels1;
    std::vector<float>   inputPixels2;
    std::vector<uint8_t> defectOut1;
    std::vector<uint8_t> defectOut2;
    XpeImageBuffer input1{};
    XpeImageBuffer input2{};
    XpeImageBuffer defectMapOut1{};
    XpeImageBuffer defectMapOut2{};

    static constexpr uint32_t W = 512;
    static constexpr uint32_t H = 512;
    static constexpr size_t PIXEL_COUNT = W * H;

    void SetUp() override {
        ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));

        std::mt19937 rng(0xDEAD1234);
        std::uniform_real_distribution<float> dist(0.0f, 4095.0f);

        inputPixels1.resize(PIXEL_COUNT);
        inputPixels2.resize(PIXEL_COUNT);
        defectOut1.resize(PIXEL_COUNT, 0);
        defectOut2.resize(PIXEL_COUNT, 0);

        for (size_t i = 0; i < PIXEL_COUNT; ++i) {
            inputPixels1[i] = dist(rng);
            inputPixels2[i] = inputPixels1[i];
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

        fillBuf(input1,      inputPixels1.data(), W, H, XPE_PIXEL_FLOAT32, 4);
        fillBuf(input2,      inputPixels2.data(), W, H, XPE_PIXEL_FLOAT32, 4);
        fillBuf(defectMapOut1, defectOut1.data(), W, H, XPE_PIXEL_UINT8,   1);
        fillBuf(defectMapOut2, defectOut2.data(), W, H, XPE_PIXEL_UINT8,   1);
    }

    void TearDown() override {
        xpe_preprocess_shutdown();
    }
};

TEST_F(RuntimeDetectDeterminismTest, DefectMapBitIdentical) {
    ASSERT_EQ(XPE_OK, xpe_defect_detect_runtime(&input1, nullptr, &defectMapOut1));
    ASSERT_EQ(XPE_OK, xpe_defect_detect_runtime(&input2, nullptr, &defectMapOut2));

    EXPECT_EQ(0, std::memcmp(defectOut1.data(), defectOut2.data(),
                             PIXEL_COUNT * sizeof(uint8_t)))
        << "Runtime detection AVX2 output differs between identical calls";
}

TEST_F(RuntimeDetectDeterminismTest, DifferentInputProducesDifferentOutput) {
    // Flat image with sparse extreme outliers: Hampel window median=2000, MAD≈0,
    // so extreme pixels (65535) are always flagged regardless of threshold.
    // Regular input1 is uniform [0,4095] — Hampel threshold >>2047 so no defects.
    std::vector<float> noisyPixels(PIXEL_COUNT, 2000.0f);
    std::mt19937 rng(0xCAFEBABE);
    std::uniform_int_distribution<size_t> posDist(0, PIXEL_COUNT - 1);
    for (int j = 0; j < 500; ++j)
        noisyPixels[posDist(rng)] = 65535.0f;

    XpeImageBuffer noisyBuf{};
    noisyBuf.data = noisyPixels.data();
    noisyBuf.width = W;
    noisyBuf.height = H;
    noisyBuf.bitsAllocated = 32;
    noisyBuf.bitsStored = 32;
    noisyBuf.format = XPE_PIXEL_FLOAT32;
    noisyBuf.dataSize = static_cast<uint32_t>(PIXEL_COUNT * 4);

    std::vector<uint8_t> noisyOut(PIXEL_COUNT, 0);
    XpeImageBuffer noisyOutBuf{};
    noisyOutBuf.data = noisyOut.data();
    noisyOutBuf.width = W;
    noisyOutBuf.height = H;
    noisyOutBuf.bitsAllocated = 8;
    noisyOutBuf.bitsStored = 8;
    noisyOutBuf.format = XPE_PIXEL_UINT8;
    noisyOutBuf.dataSize = static_cast<uint32_t>(PIXEL_COUNT * 1);

    ASSERT_EQ(XPE_OK, xpe_defect_detect_runtime(&input1, nullptr, &defectMapOut1));
    ASSERT_EQ(XPE_OK, xpe_defect_detect_runtime(&noisyBuf, nullptr, &noisyOutBuf));

    bool anyDifference = (std::memcmp(defectOut1.data(), noisyOut.data(),
                                      PIXEL_COUNT * sizeof(uint8_t)) != 0);
    EXPECT_TRUE(anyDifference)
        << "Flat image with extreme outliers should produce different defect map from uniform input";
}

TEST_F(RuntimeDetectDeterminismTest, RepeatedCallParity_5x) {
    std::vector<std::vector<uint8_t>> outputs(5, std::vector<uint8_t>(PIXEL_COUNT, 0));
    std::vector<XpeImageBuffer> outBufs(5);

    for (int i = 0; i < 5; ++i) {
        outBufs[i].data = outputs[i].data();
        outBufs[i].width = W;
        outBufs[i].height = H;
        outBufs[i].bitsAllocated = 8;
        outBufs[i].bitsStored = 8;
        outBufs[i].format = XPE_PIXEL_UINT8;
        outBufs[i].dataSize = static_cast<uint32_t>(PIXEL_COUNT * 1);
        ASSERT_EQ(XPE_OK, xpe_defect_detect_runtime(&input1, nullptr, &outBufs[i]));
    }

    for (int i = 1; i < 5; ++i) {
        EXPECT_EQ(0, std::memcmp(outputs[0].data(), outputs[i].data(),
                                 PIXEL_COUNT * sizeof(uint8_t)))
            << "Call " << i << " differs from call 0 — non-deterministic AVX2 dispatch";
    }
}

TEST_F(RuntimeDetectDeterminismTest, NullConfigUsesDefaults) {
    EXPECT_EQ(XPE_OK, xpe_defect_detect_runtime(&input1, nullptr, &defectMapOut1));
}

} // namespace
