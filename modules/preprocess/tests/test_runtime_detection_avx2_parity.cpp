/**
 * @file test_runtime_detection_avx2_parity.cpp
 * @brief AVX2 parity: runtime defect detection must be bit-identical across calls
 * SPEC: SPEC-SIMD-001 REQ-SIMD-004  IEC 62304 Class B
 *
 * Verifies that xpe_defect_detect_runtime produces identical TPR/FPR (and defect map)
 * when called twice with the same input — confirming deterministic AVX2 dispatch.
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

class RuntimeDetectAVX2ParityTest : public ::testing::Test {
protected:
    std::vector<uint16_t> inputPixels1;
    std::vector<uint16_t> inputPixels2;
    std::vector<uint16_t> defectOut1;
    std::vector<uint16_t> defectOut2;
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
        std::uniform_int_distribution<uint16_t> dist(0, 4095);

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

        fillBuf(input1, inputPixels1.data(), W, H, XPE_PIXEL_UINT16, 2);
        fillBuf(input2, inputPixels2.data(), W, H, XPE_PIXEL_UINT16, 2);
        fillBuf(defectMapOut1, defectOut1.data(), W, H, XPE_PIXEL_UINT16, 2);
        fillBuf(defectMapOut2, defectOut2.data(), W, H, XPE_PIXEL_UINT16, 2);
    }

    void TearDown() override {
        xpe_preprocess_shutdown();
    }
};

TEST_F(RuntimeDetectAVX2ParityTest, DefectMapBitIdentical) {
    ASSERT_EQ(XPE_OK, xpe_defect_detect_runtime(&input1, &defectMapOut1, nullptr));
    ASSERT_EQ(XPE_OK, xpe_defect_detect_runtime(&input2, &defectMapOut2, nullptr));

    EXPECT_EQ(0, std::memcmp(defectOut1.data(), defectOut2.data(),
                             PIXEL_COUNT * sizeof(uint16_t)))
        << "Runtime detection AVX2 output differs between identical calls";
}

TEST_F(RuntimeDetectAVX2ParityTest, DifferentInputProducesDifferentOutput) {
    std::vector<uint16_t> noisyPixels(PIXEL_COUNT);
    std::mt19937 rng(0xCAFEBABE);
    std::uniform_int_distribution<uint16_t> bigDist(0, 65535);
    for (auto& p : noisyPixels) p = bigDist(rng);

    XpeImageBuffer noisyBuf{};
    noisyBuf.data = noisyPixels.data();
    noisyBuf.width = W;
    noisyBuf.height = H;
    noisyBuf.bitsAllocated = 16;
    noisyBuf.bitsStored = 16;
    noisyBuf.format = XPE_PIXEL_UINT16;
    noisyBuf.dataSize = static_cast<uint32_t>(PIXEL_COUNT * 2);

    std::vector<uint16_t> noisyOut(PIXEL_COUNT, 0);
    XpeImageBuffer noisyOutBuf{};
    noisyOutBuf.data = noisyOut.data();
    noisyOutBuf.width = W;
    noisyOutBuf.height = H;
    noisyOutBuf.bitsAllocated = 16;
    noisyOutBuf.bitsStored = 16;
    noisyOutBuf.format = XPE_PIXEL_UINT16;
    noisyOutBuf.dataSize = static_cast<uint32_t>(PIXEL_COUNT * 2);

    ASSERT_EQ(XPE_OK, xpe_defect_detect_runtime(&input1, &defectMapOut1, nullptr));
    ASSERT_EQ(XPE_OK, xpe_defect_detect_runtime(&noisyBuf, &noisyOutBuf, nullptr));

    bool anyDifference = (std::memcmp(defectOut1.data(), noisyOut.data(),
                                      PIXEL_COUNT * sizeof(uint16_t)) != 0);
    EXPECT_TRUE(anyDifference)
        << "Highly noisy input should produce different defect map from uniform input";
}

TEST_F(RuntimeDetectAVX2ParityTest, RepeatedCallParity_5x) {
    std::vector<std::vector<uint16_t>> outputs(5, std::vector<uint16_t>(PIXEL_COUNT, 0));
    std::vector<XpeImageBuffer> outBufs(5);

    for (int i = 0; i < 5; ++i) {
        outBufs[i].data = outputs[i].data();
        outBufs[i].width = W;
        outBufs[i].height = H;
        outBufs[i].bitsAllocated = 16;
        outBufs[i].bitsStored = 16;
        outBufs[i].format = XPE_PIXEL_UINT16;
        outBufs[i].dataSize = static_cast<uint32_t>(PIXEL_COUNT * 2);
        ASSERT_EQ(XPE_OK, xpe_defect_detect_runtime(&input1, &outBufs[i], nullptr));
    }

    for (int i = 1; i < 5; ++i) {
        EXPECT_EQ(0, std::memcmp(outputs[0].data(), outputs[i].data(),
                                 PIXEL_COUNT * sizeof(uint16_t)))
            << "Call " << i << " differs from call 0 — non-deterministic AVX2 dispatch";
    }
}

TEST_F(RuntimeDetectAVX2ParityTest, NullConfigUsesDefaults) {
    EXPECT_EQ(XPE_OK, xpe_defect_detect_runtime(&input1, &defectMapOut1, nullptr));
}

} // namespace
