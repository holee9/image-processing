/**
 * @file test_exposure_index.cpp
 * @brief TDD RED tests for SWU-2.10: Exposure Index / Deviation Index (REQ-ENH-023..030)
 * SPEC: SPEC-XPE-P1B-ENH v1.0.0  IEC 62304 Class B  IEC 62494-1
 */

#include <gtest/gtest.h>
#include "xpe/enhance_basic/enhance_basic_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <algorithm>
#include <chrono>

namespace {

// Must match the EIT table in exposure_index.cpp
static constexpr float S0_REFERENCE = 1000.0f;
static constexpr float EIT_CHEST = 200.0f;
static constexpr float EIT_DEFAULT = 200.0f;

// Helper: create float32 image filled with a value
static XpeImageBuffer make_f32(uint32_t w, uint32_t h, float fill = 0.0f) {
    XpeImageBuffer img{};
    img.width = w;
    img.height = h;
    img.format = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = 32;
    img.bitsStored = 32;
    img.dataSize = (size_t)w * h * sizeof(float);
    img.data = malloc(img.dataSize);
    if (img.data) {
        float* px = static_cast<float*>(img.data);
        std::fill(px, px + (size_t)w * h, fill);
    }
    return img;
}

static void free_img(XpeImageBuffer& img) {
    free(img.data);
    img.data = nullptr;
}

static XpeImageMetadata make_meta(const char* bodyPart) {
    XpeImageMetadata meta{};
    if (bodyPart) {
        std::snprintf(meta.bodyPart, sizeof(meta.bodyPart), "%s", bodyPart);
    }
    meta.kVp = 80.0f;
    meta.mAs = 5.0f;
    meta.SID_mm = 1000.0f;
    meta.pixelPitch_mm = 0.139f;
    meta.acquisitionTime = 0;
    meta.flags = 0;
    return meta;
}

// REQ-ENH-027: Null img returns XPE_ERR_INVALID_INPUT
TEST(ExposureIndex, NullImg_ReturnsInvalidInput) {
    auto meta = make_meta("CHEST");
    float outEI = -1.0f, outDI = -1.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_calc_exposure_index(nullptr, &meta, &outEI, &outDI));
}

// REQ-ENH-027: Null meta returns XPE_ERR_INVALID_INPUT
TEST(ExposureIndex, NullMeta_ReturnsInvalidInput) {
    auto img = make_f32(64, 64, 500.0f);
    float outEI = -1.0f, outDI = -1.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_calc_exposure_index(&img, nullptr, &outEI, &outDI));
    free_img(img);
}

// REQ-ENH-027: Null outEI returns XPE_ERR_INVALID_INPUT
TEST(ExposureIndex, NullOutEI_ReturnsInvalidInput) {
    auto img = make_f32(64, 64, 500.0f);
    auto meta = make_meta("CHEST");
    float outDI = -1.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_calc_exposure_index(&img, &meta, nullptr, &outDI));
    free_img(img);
}

// REQ-ENH-027: Null outDI returns XPE_ERR_INVALID_INPUT
TEST(ExposureIndex, NullOutDI_ReturnsInvalidInput) {
    auto img = make_f32(64, 64, 500.0f);
    auto meta = make_meta("CHEST");
    float outEI = -1.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_calc_exposure_index(&img, &meta, &outEI, nullptr));
    free_img(img);
}

// REQ-ENH-028: Width == 0 returns XPE_ERR_INVALID_INPUT
TEST(ExposureIndex, EmptyImage_WidthZero_ReturnsInvalidInput) {
    XpeImageBuffer img{};
    img.width = 0;
    img.height = 64;
    img.format = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = 32;
    img.bitsStored = 32;
    img.dataSize = 0;
    img.data = nullptr;

    auto meta = make_meta("CHEST");
    float outEI = -1.0f, outDI = -1.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_calc_exposure_index(&img, &meta, &outEI, &outDI));
}

// REQ-ENH-028: Height == 0 returns XPE_ERR_INVALID_INPUT
TEST(ExposureIndex, EmptyImage_HeightZero_ReturnsInvalidInput) {
    XpeImageBuffer img{};
    img.width = 64;
    img.height = 0;
    img.format = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = 32;
    img.bitsStored = 32;
    img.dataSize = 0;
    img.data = nullptr;

    auto meta = make_meta("CHEST");
    float outEI = -1.0f, outDI = -1.0f;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_calc_exposure_index(&img, &meta, &outEI, &outDI));
}

// REQ-ENH-030: Zero mean image returns XPE_ERR_PROCESSING_FAILED, outEI=0, outDI=0
TEST(ExposureIndex, ZeroMeanImage_ReturnsProcessingFailed) {
    auto img = make_f32(64, 64, 0.0f);
    auto meta = make_meta("CHEST");
    float outEI = -1.0f, outDI = -1.0f;
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, xpe_calc_exposure_index(&img, &meta, &outEI, &outDI));
    EXPECT_FLOAT_EQ(0.0f, outEI);
    EXPECT_FLOAT_EQ(0.0f, outDI);
    free_img(img);
}

// AC-06 / REQ-ENH-023: Known phantom EI matches reference
TEST(ExposureIndex, KnownPhantom_EI_MatchesReference) {
    const float mean = 500.0f;
    auto img = make_f32(64, 64, mean);
    auto meta = make_meta("CHEST");
    float outEI = 0.0f, outDI = 0.0f;

    ASSERT_EQ(XPE_OK, xpe_calc_exposure_index(&img, &meta, &outEI, &outDI));

    float expected_EI = EIT_CHEST * (mean / S0_REFERENCE);
    EXPECT_NEAR(expected_EI, outEI, expected_EI * 0.001f)
        << "expected EI=" << expected_EI << ", got " << outEI;
    free_img(img);
}

// AC-06 / REQ-ENH-024: Known phantom DI matches reference
TEST(ExposureIndex, KnownPhantom_DI_MatchesReference) {
    const float mean = 500.0f;
    auto img = make_f32(64, 64, mean);
    auto meta = make_meta("CHEST");
    float outEI = 0.0f, outDI = 0.0f;

    ASSERT_EQ(XPE_OK, xpe_calc_exposure_index(&img, &meta, &outEI, &outDI));

    float expected_EI = EIT_CHEST * (mean / S0_REFERENCE);
    float expected_DI = 10.0f * std::log10(expected_EI / EIT_CHEST);
    EXPECT_NEAR(expected_DI, outDI, 0.001f)
        << "expected DI=" << expected_DI << ", got " << outDI;
    free_img(img);
}

// AC-07 / REQ-ENH-026: DI outside [-3, +3] posts WARNING alert
TEST(ExposureIndex, DIOutOfRange_PostsAlert) {
    // mean much higher than S0 to push DI > 3.0
    // DI = 10*log10(EI/EIT) = 10*log10(mean/S0)
    // For DI > 3: mean/S0 > 10^0.3 = ~2.0 => mean > 2000
    const float mean = 3000.0f;
    auto img = make_f32(64, 64, mean);
    auto meta = make_meta("CHEST");
    float outEI = 0.0f, outDI = 0.0f;

    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_calc_exposure_index(&img, &meta, &outEI, &outDI));

    // Verify DI is indeed out of range
    EXPECT_GT(std::fabs(outDI), 3.0f) << "DI=" << outDI << " should be outside [-3, +3]";

    // Verify alert was posted
    EXPECT_GT(xpe_get_pending_alert_count(), 0) << "Expected WARNING alert for DI=" << outDI;

    // Verify alert severity is WARNING
    if (xpe_get_pending_alert_count() > 0) {
        char msg[256] = {};
        int32_t severity = -1;
        ASSERT_EQ(XPE_OK, xpe_get_pending_alert(0, msg, sizeof(msg), &severity));
        EXPECT_EQ(XPE_ALERT_WARNING, severity);
    }

    free_img(img);
}

// REQ-ENH-026: DI in range [-3, +3] does NOT post alert
TEST(ExposureIndex, DIInRange_NoAlert) {
    // mean = S0 => DI = 10*log10(mean/S0) = 10*log10(1) = 0
    const float mean = S0_REFERENCE;
    auto img = make_f32(64, 64, mean);
    auto meta = make_meta("CHEST");
    float outEI = 0.0f, outDI = 0.0f;

    xpe_clear_alerts();
    ASSERT_EQ(XPE_OK, xpe_calc_exposure_index(&img, &meta, &outEI, &outDI));

    EXPECT_LE(std::fabs(outDI), 3.0f) << "DI=" << outDI << " should be within [-3, +3]";
    EXPECT_EQ(0, xpe_get_pending_alert_count()) << "No alert expected for DI=" << outDI;

    free_img(img);
}

// REQ-ENH-025: Unknown body part uses default EIT
TEST(ExposureIndex, UnknownBodyPart_UsesDefault) {
    const float mean = 500.0f;
    auto img = make_f32(64, 64, mean);
    auto meta = make_meta("XRAY_UNKNOWN_REGION");
    float outEI = 0.0f, outDI = 0.0f;

    ASSERT_EQ(XPE_OK, xpe_calc_exposure_index(&img, &meta, &outEI, &outDI));

    float expected_EI = EIT_DEFAULT * (mean / S0_REFERENCE);
    EXPECT_NEAR(expected_EI, outEI, expected_EI * 0.01f)
        << "Unknown body part should use default EIT";
    free_img(img);
}

// REQ-ENH-029: ROI sub-buffer computes on smaller region only
TEST(ExposureIndex, ROISubBuffer_ComputesOnSubRegion) {
    // Create a 100x100 sub-buffer (representing a crop from 1024x1024)
    const float mean = 750.0f;
    auto img = make_f32(100, 100, mean);
    auto meta = make_meta("CHEST");
    float outEI = 0.0f, outDI = 0.0f;

    ASSERT_EQ(XPE_OK, xpe_calc_exposure_index(&img, &meta, &outEI, &outDI));

    float expected_EI = EIT_CHEST * (mean / S0_REFERENCE);
    EXPECT_NEAR(expected_EI, outEI, expected_EI * 0.001f)
        << "ROI sub-buffer EI should be computed on 100x100 pixels";
    free_img(img);
}

TEST(ExposureIndex, BenchmarkFreeze_BP08_EICalcTimeBaseline) {
    constexpr int kWidth = 512;
    constexpr int kHeight = 512;
    constexpr auto kMaxMs = 25;

    auto img = make_f32(kWidth, kHeight, S0_REFERENCE);
    auto meta = make_meta("CHEST");
    float outEI = 0.0f;
    float outDI = 0.0f;

    auto start = std::chrono::steady_clock::now();
    ASSERT_EQ(XPE_OK, xpe_calc_exposure_index(&img, &meta, &outEI, &outDI));
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    EXPECT_NEAR(EIT_CHEST, outEI, EIT_CHEST * 0.001f);
    EXPECT_NEAR(0.0f, outDI, 0.001f);
    EXPECT_LT(elapsed.count(), kMaxMs)
        << "BP-08 EI calculation baseline exceeded.";
    RecordProperty("BP", "BP-08");
    RecordProperty("baseline_ms_max", kMaxMs);
    RecordProperty("pixels", kWidth * kHeight);
    free_img(img);
}

TEST(ExposureIndex, BenchmarkFreeze_BP09_DICalcTimeBaseline) {
    constexpr int kWidth = 512;
    constexpr int kHeight = 512;
    constexpr auto kMaxMs = 25;
    constexpr float kMean = 3000.0f;

    auto img = make_f32(kWidth, kHeight, kMean);
    auto meta = make_meta("CHEST");
    float outEI = 0.0f;
    float outDI = 0.0f;

    xpe_clear_alerts();
    auto start = std::chrono::steady_clock::now();
    ASSERT_EQ(XPE_OK, xpe_calc_exposure_index(&img, &meta, &outEI, &outDI));
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);

    const float expectedEI = EIT_CHEST * (kMean / S0_REFERENCE);
    const float expectedDI = 10.0f * std::log10(expectedEI / EIT_CHEST);
    EXPECT_NEAR(expectedEI, outEI, expectedEI * 0.001f);
    EXPECT_NEAR(expectedDI, outDI, 0.001f);
    EXPECT_GT(std::fabs(outDI), 3.0f);
    EXPECT_LT(elapsed.count(), kMaxMs)
        << "BP-09 DI calculation baseline exceeded.";
    RecordProperty("BP", "BP-09");
    RecordProperty("baseline_ms_max", kMaxMs);
    RecordProperty("pixels", kWidth * kHeight);
    free_img(img);
}

} // namespace
