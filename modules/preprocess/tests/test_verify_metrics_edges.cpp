/**
 * @file test_verify_metrics_edges.cpp
 * @brief Verification-metric edge paths (QA-A-32, #120)
 *
 * `xpe_verify_metrics.cpp` measured 191/230 (0.83) in coverage run 34479945843.
 * The 39 uncovered lines are the edges rather than the happy paths: the format
 * rejections (179, 183), the "no dark pixels found" fallback (226-228), the
 * flatness/entropy helper's identical-values shortcut and histogram loop
 * (81-103), and the corresponding rejection arms in `xpe_verify_gain` (276-288),
 * `xpe_verify_defect` (383-394) and `xpe_verify_pipeline` (471-494).
 *
 * The existing suite covers the four entry points' success paths plus NULL and
 * dimension-mismatch; what it never supplies is a wrong PIXEL FORMAT, a
 * zero-sized buffer, or an image whose content drives the alternate branch.
 *
 * Per-entry-point formats, measured from the implementation rather than assumed
 * (xpe_verify_metrics.cpp:177-178, 281-283, 388-389, 478-486):
 *
 *   xpe_verify_offset    raw UINT16   corrected UINT16
 *   xpe_verify_gain      before UINT16  after FLOAT32   gain_map FLOAT32
 *   xpe_verify_defect    corrected FLOAT32              defect_map UINT8
 *   xpe_verify_pipeline  raw UINT16     final FLOAT32
 *
 * The corrected image is FLOAT32 from the gain stage onward, which is why only
 * the offset metric takes UINT16 on both sides. A first draft of this file
 * assumed UINT16 throughout and measured -7 (XPE_ERR_UNSUPPORTED_FORMAT) on
 * five cases.
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <cstdint>
#include <vector>

namespace {

constexpr uint32_t W = 8, H = 8;
constexpr size_t   N = static_cast<size_t>(W) * H;

XpeImageBuffer u16Buf(std::vector<uint16_t>& v, uint32_t w = W, uint32_t h = H) {
    XpeImageBuffer b{};
    b.data = v.data(); b.width = w; b.height = h;
    b.bitsAllocated = 16; b.bitsStored = 16;
    b.format = XPE_PIXEL_UINT16;
    b.dataSize = v.size() * sizeof(uint16_t);
    return b;
}

XpeImageBuffer f32Buf(std::vector<float>& v, uint32_t w = W, uint32_t h = H) {
    XpeImageBuffer b{};
    b.data = v.data(); b.width = w; b.height = h;
    b.bitsAllocated = 32; b.bitsStored = 32;
    b.format = XPE_PIXEL_FLOAT32;
    b.dataSize = v.size() * sizeof(float);
    return b;
}

XpeImageBuffer u8Buf(std::vector<uint8_t>& v, uint32_t w = W, uint32_t h = H) {
    XpeImageBuffer b{};
    b.data = v.data(); b.width = w; b.height = h;
    b.bitsAllocated = 8; b.bitsStored = 8;
    b.format = XPE_PIXEL_UINT8;
    b.dataSize = v.size();
    return b;
}

class VerifyMetricsEdgeTest : public ::testing::Test {};

// xpe_verify_offset wants UINT16 on both sides. A FLOAT32 buffer of matching
// dimensions passes the dimension check and is then rejected on format.
TEST_F(VerifyMetricsEdgeTest, VerifyOffsetRejectsFloatInput) {
    std::vector<float>    rawF(N, 1000.0f);
    std::vector<uint16_t> corrected(N, 900u);
    XpeImageBuffer raw = f32Buf(rawF);
    XpeImageBuffer cor = u16Buf(corrected);
    XpeCalibrationMetrics metrics{};

    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT,
              xpe_verify_offset(&raw, &cor, nullptr, &metrics));
}

// The corrected side is checked too, not only the raw side.
TEST_F(VerifyMetricsEdgeTest, VerifyOffsetRejectsFloatCorrectedImage) {
    std::vector<uint16_t> rawU(N, 1000u);
    std::vector<float>    correctedF(N, 900.0f);
    XpeImageBuffer raw = u16Buf(rawU);
    XpeImageBuffer cor = f32Buf(correctedF);
    XpeCalibrationMetrics metrics{};

    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT,
              xpe_verify_offset(&raw, &cor, nullptr, &metrics));
}

// A uniformly bright raw frame has no dark region, so the dark-pixel selection
// finds nothing and the implementation falls back to using every pixel
// (xpe_verify_metrics.cpp:226-228) rather than dividing by zero.
TEST_F(VerifyMetricsEdgeTest, VerifyOffsetFallsBackWhenNoDarkPixelsExist) {
    std::vector<uint16_t> rawU(N, 60000u);        // uniformly bright
    std::vector<uint16_t> corrected(N, 59000u);
    XpeImageBuffer raw = u16Buf(rawU);
    XpeImageBuffer cor = u16Buf(corrected);
    XpeCalibrationMetrics metrics{};

    const XpeErrorCode rc = xpe_verify_offset(&raw, &cor, nullptr, &metrics);
    EXPECT_EQ(XPE_OK, rc) << "the fallback must produce metrics, not an error";
}

// An entirely uniform corrected frame is the flatness helper's shortcut:
// max <= min means "perfectly flat" without running the histogram.
TEST_F(VerifyMetricsEdgeTest, VerifyGainHandlesPerfectlyUniformOutput) {
    std::vector<uint16_t> before(N, 1000u);
    std::vector<float>    after(N, 1000.0f);      // identical everywhere
    std::vector<float>    gain(N, 1.0f);
    XpeImageBuffer b = u16Buf(before), a = f32Buf(after), g = f32Buf(gain);
    XpeCalibrationMetrics metrics{};

    EXPECT_EQ(XPE_OK, xpe_verify_gain(&b, &a, &g, &metrics));
}

// A varied corrected frame drives the histogram/entropy path instead.
TEST_F(VerifyMetricsEdgeTest, VerifyGainComputesFlatnessOverVariedOutput) {
    std::vector<uint16_t> before(N, 1000u);
    std::vector<float>    after(N);
    for (size_t i = 0; i < N; ++i)
        after[i] = 500.0f + static_cast<float>(i) * 20.0f;
    std::vector<float> gain(N, 1.0f);
    XpeImageBuffer b = u16Buf(before), a = f32Buf(after), g = f32Buf(gain);
    XpeCalibrationMetrics metrics{};

    EXPECT_EQ(XPE_OK, xpe_verify_gain(&b, &a, &g, &metrics));
}

// Wrong format on the gain map is rejected as such.
TEST_F(VerifyMetricsEdgeTest, VerifyGainRejectsWrongGainMapFormat) {
    std::vector<uint16_t> before(N, 1000u), gainU16(N, 1u);
    std::vector<float>    after(N, 1000.0f);
    XpeImageBuffer b = u16Buf(before), a = f32Buf(after), g = u16Buf(gainU16);
    XpeCalibrationMetrics metrics{};

    EXPECT_NE(XPE_OK, xpe_verify_gain(&b, &a, &g, &metrics))
        << "a UINT16 gain map is not the FLOAT32 the metric expects";
}

// xpe_verify_defect wants a UINT8 mask; a UINT16 buffer is rejected.
TEST_F(VerifyMetricsEdgeTest, VerifyDefectRejectsWrongMaskFormat) {
    std::vector<float>    corrected(N, 1000.0f);
    std::vector<uint16_t> maskU16(N, 0u);
    XpeImageBuffer c = f32Buf(corrected), m = u16Buf(maskU16);
    XpeCalibrationMetrics metrics{};

    EXPECT_NE(XPE_OK, xpe_verify_defect(&c, &m, &metrics));
}

// A mask with no defect at all is the zero-count branch.
TEST_F(VerifyMetricsEdgeTest, VerifyDefectHandlesMaskWithNoDefects) {
    std::vector<float>   corrected(N, 1000.0f);
    std::vector<uint8_t> mask(N, 0u);             // nothing marked
    XpeImageBuffer c = f32Buf(corrected), m = u8Buf(mask);
    XpeCalibrationMetrics metrics{};

    EXPECT_EQ(XPE_OK, xpe_verify_defect(&c, &m, &metrics));
}

// Every pixel marked defective is the opposite extreme.
TEST_F(VerifyMetricsEdgeTest, VerifyDefectHandlesFullyDefectiveMask) {
    std::vector<float>   corrected(N, 1000.0f);
    std::vector<uint8_t> mask(N, 1u);
    XpeImageBuffer c = f32Buf(corrected), m = u8Buf(mask);
    XpeCalibrationMetrics metrics{};

    EXPECT_EQ(XPE_OK, xpe_verify_defect(&c, &m, &metrics));
}

// xpe_verify_pipeline rejects a wrong format the same way its siblings do.
TEST_F(VerifyMetricsEdgeTest, VerifyPipelineRejectsFloatInput) {
    std::vector<float> rawF(N, 1000.0f);
    std::vector<float> finalF(N, 900.0f);
    XpeImageBuffer raw = f32Buf(rawF), fin = f32Buf(finalF);
    XpeCalibrationMetrics metrics{};

    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT,
              xpe_verify_pipeline(&raw, &fin, nullptr, &metrics));
}

// A final image with no variation exercises the pipeline metric's
// zero-deviation branch (SNR from a flat frame).
TEST_F(VerifyMetricsEdgeTest, VerifyPipelineHandlesFlatFinalImage) {
    std::vector<uint16_t> rawU(N);
    for (size_t i = 0; i < N; ++i) rawU[i] = static_cast<uint16_t>(900u + i);
    std::vector<float> finalF(N, 1000.0f);        // perfectly flat
    XpeImageBuffer raw = u16Buf(rawU), fin = f32Buf(finalF);
    XpeCalibrationMetrics metrics{};

    EXPECT_EQ(XPE_OK, xpe_verify_pipeline(&raw, &fin, nullptr, &metrics));
}

// An odd pixel count exercises the median helper's odd branch; the existing
// suite only ever passes even-sized frames (8x8 = 64).
TEST_F(VerifyMetricsEdgeTest, OddPixelCountUsesTheOddMedianBranch) {
    constexpr uint32_t OW = 3, OH = 3;             // 9 pixels
    std::vector<uint16_t> rawU(9, 1000u), corrected(9, 900u);
    for (size_t i = 0; i < 9; ++i) corrected[i] = static_cast<uint16_t>(800u + i);
    XpeImageBuffer raw = u16Buf(rawU, OW, OH);
    XpeImageBuffer cor = u16Buf(corrected, OW, OH);
    XpeCalibrationMetrics metrics{};

    EXPECT_EQ(XPE_OK, xpe_verify_offset(&raw, &cor, nullptr, &metrics));
}

} // namespace
