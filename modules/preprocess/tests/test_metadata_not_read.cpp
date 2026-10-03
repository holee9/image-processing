/**
 * @file test_metadata_not_read.cpp
 * @brief What the `metadata` argument does today: xpe_defect_correct, xpe_defect_detect_runtime,
 *        xpe_validate_readout_artifact, xpe_verify_offset and xpe_verify_pipeline never read its fields
 *        (QA-A-232 M2, #245)
 *
 * The header used to say the metadata gave these functions "dose-dependent threshold", "dose information"
 * and "acquisition context". The source reads none of kVp, mAs, SID_mm, pixelPitch_mm, acquisitionTime or
 * bodyPart for them (a grep of the sources found no such read; the pipeline only WRITES meta->flags).
 * The requirement (REQ-P1A-013, dose-dependent runtime threshold) is kept; the implementation is not there.
 * The header now says so. Each case below runs one function with several very different metadata blocks and
 * asserts the output is the same every time -- so if a dose-dependent threshold is ever implemented these
 * fail, and the header sentences have to change with the code.
 *
 * What each function does with the POINTER is part of the same fact and is pinned here too:
 *   xpe_defect_correct, xpe_validate_readout_artifact  NULL metadata -> XPE_ERR_INVALID_INPUT (a null check)
 *   xpe_defect_detect_runtime, xpe_verify_offset, xpe_verify_pipeline  NULL metadata is accepted
 * (xpe_offset_correct and xpe_gain_correct are pinned in test_xpe_preprocess_correction.cpp and
 * test_gain_poly_not_applied.cpp.)
 */

#include <gtest/gtest.h>

#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {

constexpr uint32_t W = 64;
constexpr uint32_t H = 64;

// Metadata blocks that differ in every field a dose-dependent or acquisition-dependent implementation
// could read. The first is an ordinary chest exposure.
std::vector<XpeImageMetadata> metadataVariants() {
    struct Row { const char* body; float kvp, mas, sid, pitch; uint64_t when; };
    const Row rows[] = {
        {"CHEST", 120.0f, 2.5f, 1800.0f, 0.14f, 1700000000ull},
        {"HAND", 40.0f, 0.1f, 600.0f, 0.1f, 1ull},
        {"", 0.0f, 0.0f, 0.0f, 0.0f, 0ull},
        {"PELVIS", 150.0f, 400.0f, 1200.0f, 0.2f, 99999999999999ull},
    };
    std::vector<XpeImageMetadata> v;
    for (const Row& r : rows) {
        XpeImageMetadata m{};
        for (size_t i = 0; r.body[i] != 0 && i + 1 < sizeof(m.bodyPart); ++i) m.bodyPart[i] = r.body[i];
        m.kVp = r.kvp; m.mAs = r.mas; m.SID_mm = r.sid; m.pixelPitch_mm = r.pitch;
        m.acquisitionTime = r.when;
        v.push_back(m);
    }
    return v;
}

XpeImageBuffer bufferOf(void* data, size_t bytes, XpePixelFormat fmt, uint16_t bits) {
    XpeImageBuffer b{};
    b.data = data; b.dataSize = bytes; b.width = W; b.height = H;
    b.format = fmt; b.bitsAllocated = b.bitsStored = bits;
    return b;
}

class MetadataNotReadTest : public ::testing::Test {
protected:
    const char* defectPath = "qa_a_232_meta_defect.xcal";
    void SetUp() override { ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr)); }
    void TearDown() override {
        std::remove(defectPath);
        xpe_preprocess_shutdown();
    }
};

}  // namespace

// xpe_defect_correct: header said "Image metadata for dose-dependent threshold".
TEST_F(MetadataNotReadTest, DefectCorrectOutputIsTheSameForAnyMetadata) {
    std::vector<uint8_t> map(W * H, 0);
    map[10 * W + 10] = 1;
    map[10 * W + 11] = 1;
    map[40 * W + 7] = 1;
    XCalFileHeader hdr{};
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version = XCAL_VERSION;
    hdr.type = static_cast<uint32_t>(XCAL_TYPE_DEFECT);
    hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_UINT8_MASK);
    hdr.width = W; hdr.height = H;
    hdr.payload_len = static_cast<uint64_t>(map.size());
    std::remove(defectPath);
    ASSERT_EQ(XPE_OK, write_xcal_file(defectPath, hdr, nullptr, 0, map.data(), hdr.payload_len));
    ASSERT_EQ(XPE_OK, xpe_calib_load_defect_map(defectPath));

    std::vector<float> in(W * H);
    for (size_t i = 0; i < in.size(); ++i) in[i] = 1000.0f + static_cast<float>((i * 31u) % 97u);
    for (size_t i = 0; i < map.size(); ++i) if (map[i]) in[i] = 60000.0f;  // the defects are far from their surroundings

    std::vector<float> first;
    for (const XpeImageMetadata& meta : metadataVariants()) {
        std::vector<float> out(W * H, -1.0f);
        XpeImageBuffer ib = bufferOf(in.data(), in.size() * sizeof(float), XPE_PIXEL_FLOAT32, 32);
        XpeImageBuffer ob = bufferOf(out.data(), out.size() * sizeof(float), XPE_PIXEL_FLOAT32, 32);
        ASSERT_EQ(XPE_OK, xpe_defect_correct(&ib, &ob, &meta));
        if (first.empty()) {
            first = out;
            // Control: the defects were corrected, so "the same every time" is not the same untouched frame.
            for (size_t i = 0; i < map.size(); ++i)
                if (map[i]) ASSERT_LT(out[i], 10000.0f) << "defect pixel " << i << " was not corrected";
        } else {
            EXPECT_EQ(first, out);
        }
    }

    // The pointer: a null check and nothing else.
    std::vector<float> out(W * H);
    XpeImageBuffer ib = bufferOf(in.data(), in.size() * sizeof(float), XPE_PIXEL_FLOAT32, 32);
    XpeImageBuffer ob = bufferOf(out.data(), out.size() * sizeof(float), XPE_PIXEL_FLOAT32, 32);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_defect_correct(&ib, &ob, nullptr));
}

// xpe_defect_detect_runtime: header said "Image metadata for dose information".
TEST_F(MetadataNotReadTest, DetectRuntimeMapIsTheSameForAnyMetadata) {
    std::vector<float> in(W * H);
    uint32_t s = 12345u;
    for (float& p : in) {
        s = s * 1664525u + 1013904223u;
        p = 1000.0f + static_cast<float>((s >> 16) % 7u);  // small noise
    }
    in[20 * W + 20] = 9000.0f;
    in[33 * W + 5] = 9000.0f;
    in[50 * W + 60] = 9000.0f;

    std::vector<uint8_t> first;
    auto run = [&](const XpeImageMetadata* meta, std::vector<uint8_t>& map) {
        map.assign(W * H, 0xFF);
        XpeImageBuffer ib = bufferOf(in.data(), in.size() * sizeof(float), XPE_PIXEL_FLOAT32, 32);
        XpeImageBuffer ob = bufferOf(map.data(), map.size(), XPE_PIXEL_UINT8, 8);
        return xpe_defect_detect_runtime(&ib, meta, &ob);
    };
    for (const XpeImageMetadata& meta : metadataVariants()) {
        std::vector<uint8_t> map;
        ASSERT_EQ(XPE_OK, run(&meta, map));
        if (first.empty()) {
            first = map;
            // Control: the detector found the planted outliers and not the whole frame.
            size_t found = 0;
            for (uint8_t v : map) found += (v != 0);
            ASSERT_GE(found, 3u);
            ASSERT_LT(found, size_t{64});
        } else {
            EXPECT_EQ(first, map);
        }
    }
    // The pointer: NULL is accepted (the argument is not looked at).
    std::vector<uint8_t> map;
    ASSERT_EQ(XPE_OK, run(nullptr, map));
    EXPECT_EQ(first, map);
}

// xpe_validate_readout_artifact: header said "Image metadata (acquisition context)".
TEST_F(MetadataNotReadTest, ValidateReadoutFlagsAreTheSameForAnyMetadata) {
    std::vector<uint16_t> px(W * H, 20000);
    for (uint32_t y = 0; y < H; ++y) px[static_cast<size_t>(y) * W + 13] = 0;                  // a dropped column
    for (uint32_t x = 0; x < W; ++x)                                                           // a bright row
        if (x != 13) px[static_cast<size_t>(40) * W + x] = 65535;                              // (not through the dropped column)
    XpeImageBuffer ib = bufferOf(px.data(), px.size() * sizeof(uint16_t), XPE_PIXEL_UINT16, 16);

    for (const XpeImageMetadata& meta : metadataVariants()) {
        bool dropped = false, bright = false;
        ASSERT_EQ(XPE_OK, xpe_validate_readout_artifact(&ib, &meta, &dropped, &bright));
        EXPECT_TRUE(dropped);  // control: both artifacts are there, so "the same" is not "both false"
        EXPECT_TRUE(bright);
    }
    // The pointer: a null check.
    bool dropped = false, bright = false;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_validate_readout_artifact(&ib, nullptr, &dropped, &bright));
}

// xpe_verify_offset: the header gave metadata no role and the source ignores it; NULL is accepted.
TEST_F(MetadataNotReadTest, VerifyOffsetMetricsAreTheSameForAnyMetadata) {
    std::vector<uint16_t> raw(W * H), corrected(W * H);
    for (size_t i = 0; i < raw.size(); ++i) {
        const bool lit = (i % 5u) != 0u;
        const uint16_t dark = static_cast<uint16_t>(90u + (i / 5u) % 10u);
        raw[i] = static_cast<uint16_t>(lit ? dark + 20000u : dark);
        corrected[i] = static_cast<uint16_t>(lit ? 20000u : 0u);
    }
    XpeImageBuffer rb = bufferOf(raw.data(), raw.size() * sizeof(uint16_t), XPE_PIXEL_UINT16, 16);
    XpeImageBuffer cb = bufferOf(corrected.data(), corrected.size() * sizeof(uint16_t), XPE_PIXEL_UINT16, 16);

    auto run = [&](const XpeImageMetadata* meta) {
        XpeCalibrationMetrics m{};
        std::memset(&m, 0, sizeof(m));
        EXPECT_EQ(XPE_OK, xpe_verify_offset(&rb, &cb, meta, &m));
        return m;
    };
    const auto variants = metadataVariants();
    const XpeCalibrationMetrics first = run(&variants[0]);
    EXPECT_NE(0u, first.measured_mask);  // control: something was measured
    for (const XpeImageMetadata& meta : variants) {
        const XpeCalibrationMetrics m = run(&meta);
        EXPECT_EQ(0, std::memcmp(&first, &m, sizeof(m)));
    }
    const XpeCalibrationMetrics viaNull = run(nullptr);
    EXPECT_EQ(0, std::memcmp(&first, &viaNull, sizeof(viaNull)));
}

// xpe_verify_pipeline: same.
TEST_F(MetadataNotReadTest, VerifyPipelineMetricsAreTheSameForAnyMetadata) {
    std::vector<uint16_t> raw(W * H);
    std::vector<float> fin(W * H);
    for (uint32_t y = 0; y < H; ++y) {
        for (uint32_t x = 0; x < W; ++x) {
            const bool up = ((x + y) % 2) == 0;
            raw[static_cast<size_t>(y) * W + x] = static_cast<uint16_t>(up ? 1300 : 700);
            fin[static_cast<size_t>(y) * W + x] = up ? 1020.0f : 980.0f;
        }
    }
    XpeImageBuffer rb = bufferOf(raw.data(), raw.size() * sizeof(uint16_t), XPE_PIXEL_UINT16, 16);
    XpeImageBuffer fb = bufferOf(fin.data(), fin.size() * sizeof(float), XPE_PIXEL_FLOAT32, 32);

    auto run = [&](const XpeImageMetadata* meta) {
        XpeCalibrationMetrics m{};
        std::memset(&m, 0, sizeof(m));
        EXPECT_EQ(XPE_OK, xpe_verify_pipeline(&rb, &fb, meta, &m));
        return m;
    };
    const auto variants = metadataVariants();
    const XpeCalibrationMetrics first = run(&variants[0]);
    EXPECT_NE(0u, first.measured_mask);
    for (const XpeImageMetadata& meta : variants) {
        const XpeCalibrationMetrics m = run(&meta);
        EXPECT_EQ(0, std::memcmp(&first, &m, sizeof(m)));
    }
    const XpeCalibrationMetrics viaNull = run(nullptr);
    EXPECT_EQ(0, std::memcmp(&first, &viaNull, sizeof(viaNull)));
}
