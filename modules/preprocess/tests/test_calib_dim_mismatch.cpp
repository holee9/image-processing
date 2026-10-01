/**
 * @file test_calib_dim_mismatch.cpp
 * @brief REQ-P1A-021 (QA-A-185, #232): a calibration map whose dimensions differ from the input.
 *
 * Decided 2026-10-01 (#232): "input and map buffer dimensions differ" returns
 * XPE_ERR_INVALID_INPUT and leaves the output buffer untouched. An OUTPUT buffer whose
 * dimensions differ from the input stays XPE_ERR_BUFFER_TOO_SMALL -- that is covered by
 * Offset/Gain/DefectCorrectTest.DimensionMismatchReturnsError and is deliberately not
 * repeated here.
 *
 * One test per map-versus-input return site:
 *   offset_correct.cpp  offset map
 *   gain_correct.cpp    scalar gain map, and the per-pixel polynomial map
 *   defect_correct.cpp  defect map
 *
 * Each test loads a 4x4 map and calls with a 5x4 input and a 5x4 output, so the output
 * dimensions agree with the input and only the map disagrees.
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

constexpr uint32_t MW = 4, MH = 4;   // calibration map
constexpr uint32_t IW = 5, IH = 4;   // input and output
constexpr float kSentinel = -123.0f;

XpeImageBuffer buffer(void* data, XpePixelFormat fmt, uint32_t bits) {
    XpeImageBuffer b{};
    b.data = data; b.width = IW; b.height = IH;
    b.bitsAllocated = bits; b.bitsStored = bits; b.format = fmt;
    b.dataSize = static_cast<uint32_t>(static_cast<size_t>(IW) * IH * (bits / 8));
    return b;
}

class CalibDimMismatch : public ::testing::Test {
protected:
    XpeImageMetadata meta{};
    const char* path = "test_calib_dim_mismatch.xcal";

    void SetUp() override { ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr)); }
    void TearDown() override {
        xpe_clear_alerts();   // loading a gain polynomial queues an alert; leave the queue as found
        std::remove(path);
        std::remove("test_calib_dim_mismatch.xcal.tmp");
        xpe_preprocess_shutdown();
    }

    void writeMap(uint32_t type, uint32_t fmt, const void* data, size_t bytes) {
        std::remove(path);
        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version = XCAL_VERSION; hdr.type = type; hdr.pixel_format = fmt;
        hdr.width = MW; hdr.height = MH; hdr.payload_len = bytes;
        ASSERT_EQ(XPE_OK, write_xcal_file(path, hdr, nullptr, 0,
                                          static_cast<const uint8_t*>(data), bytes));
    }
};

} // namespace

TEST_F(CalibDimMismatch, OffsetMapDifferentFromInputReturnsInvalidInput) {
    const std::vector<float> map(MW * MH, 100.0f);
    writeMap(XCAL_TYPE_OFFSET, XCAL_FMT_FLOAT32, map.data(), map.size() * sizeof(float));
    ASSERT_EQ(XPE_OK, xpe_calib_load_offset(path));

    std::vector<uint16_t> in(IW * IH, 1000), out(IW * IH, 777);
    XpeImageBuffer i = buffer(in.data(), XPE_PIXEL_UINT16, 16), o = buffer(out.data(), XPE_PIXEL_UINT16, 16);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_offset_correct(&i, &o, &meta));
    EXPECT_TRUE(std::all_of(out.begin(), out.end(), [](uint16_t v) { return v == 777; }))
        << "the output buffer must be left untouched";
}

TEST_F(CalibDimMismatch, GainMapDifferentFromInputReturnsInvalidInput) {
    const std::vector<float> map(MW * MH, 1.5f);
    writeMap(XCAL_TYPE_GAIN, XCAL_FMT_FLOAT32, map.data(), map.size() * sizeof(float));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(path));

    std::vector<uint16_t> in(IW * IH, 1000);
    std::vector<float> out(IW * IH, kSentinel);
    XpeImageBuffer i = buffer(in.data(), XPE_PIXEL_UINT16, 16), o = buffer(out.data(), XPE_PIXEL_FLOAT32, 32);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_gain_correct(&i, &o, &meta));
    EXPECT_TRUE(std::all_of(out.begin(), out.end(), [](float v) { return v == kSentinel; }))
        << "the output buffer must be left untouched";
}

TEST_F(CalibDimMismatch, GainPolynomialMapDifferentFromInputReturnsInvalidInput) {
    constexpr size_t kCoeffs = 2;   // c0 + c1*x, the same for every pixel
    std::vector<float> payload(static_cast<size_t>(MW) * MH * kCoeffs);
    for (size_t p = 0; p < static_cast<size_t>(MW) * MH; ++p) { payload[p * kCoeffs] = 1.0f; payload[p * kCoeffs + 1] = 0.0f; }
    writeMap(XCAL_TYPE_GAIN_POLY, XCAL_FMT_FLOAT32, payload.data(), payload.size() * sizeof(float));
    ASSERT_EQ(XPE_OK, xpe_calib_load_gain(path));

    std::vector<uint16_t> in(IW * IH, 1000);
    std::vector<float> out(IW * IH, kSentinel);
    XpeImageBuffer i = buffer(in.data(), XPE_PIXEL_UINT16, 16), o = buffer(out.data(), XPE_PIXEL_FLOAT32, 32);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_gain_correct(&i, &o, &meta));
    EXPECT_TRUE(std::all_of(out.begin(), out.end(), [](float v) { return v == kSentinel; }))
        << "the output buffer must be left untouched";
}

TEST_F(CalibDimMismatch, DefectMapDifferentFromInputReturnsInvalidInput) {
    const std::vector<uint8_t> map(MW * MH, 0);
    writeMap(XCAL_TYPE_DEFECT, XCAL_FMT_UINT8_MASK, map.data(), map.size());
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(path));

    std::vector<float> in(IW * IH, 1000.0f), out(IW * IH, kSentinel);
    XpeImageBuffer i = buffer(in.data(), XPE_PIXEL_FLOAT32, 32), o = buffer(out.data(), XPE_PIXEL_FLOAT32, 32);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_defect_correct(&i, &o, &meta));
    EXPECT_TRUE(std::all_of(out.begin(), out.end(), [](float v) { return v == kSentinel; }))
        << "the output buffer must be left untouched";
}
