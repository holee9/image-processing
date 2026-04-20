/**
 * @file test_exposure_index.cpp
 * @brief Google Test suite for SWU-2.10: Exposure Index Calculation
 *
 * Tests the xpe_calc_exposure_index() C ABI function covering:
 *   - IEC 62494-1 EI/DI formula correctness (REQ-ADV-013)
 *   - Body-part EI target lookup
 *   - NULL pointer guards (REQ-ADV-022)
 *   - Dimension validation (REQ-ADV-070)
 *   - Format validation (REQ-ADV-071)
 *   - NaN/Inf output guard (REQ-ADV-032)
 *   - Edge cases: zero image, zero kVp/mAs
 *   - Different body parts produce different DI values
 *
 * SPEC Reference: SPEC-XPE-P2-ADV v1.0.0
 * SWU: 2.10
 */

#include <gtest/gtest.h>

#include <cmath>
#include <cstring>
#include <limits>

#include "xpe/enhance_advanced/xpe_enhance_advanced_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

// ============================================================================
// Test Helpers
// ============================================================================

namespace {

XpeImageBuffer MakeConstantImage(uint32_t w, uint32_t h, float value) {
    XpeImageBuffer buf{};
    buf.width = w;
    buf.height = h;
    buf.bitsAllocated = 32;
    buf.bitsStored = 32;
    buf.format = XPE_PIXEL_FLOAT32;
    size_t n = static_cast<size_t>(w) * h;
    buf.dataSize = n * sizeof(float);
    buf.data = new float[n];
    float* p = static_cast<float*>(buf.data);
    for (size_t i = 0; i < n; ++i) p[i] = value;
    return buf;
}

XpeImageMetadata MakeMeta(const char* bodyPart, float kVp = 80.0f, float mAs = 10.0f) {
    XpeImageMetadata meta{};
    std::memset(&meta, 0, sizeof(meta));
    strncpy_s(meta.bodyPart, sizeof(meta.bodyPart), bodyPart, _TRUNCATE);
    meta.kVp = kVp;
    meta.mAs = mAs;
    meta.SID_mm = 1800.0f;
    meta.pixelPitch_mm = 0.139f;
    return meta;
}

void FreeImageBuffer(XpeImageBuffer& buf) {
    delete[] static_cast<float*>(buf.data);
    buf.data = nullptr;
    buf.dataSize = 0;
}

} // anonymous namespace

// ============================================================================
// Test Fixture
// ============================================================================

class ExposureIndexTest : public ::testing::Test {
protected:
    void SetUp() override {
        XpeErrorCode err = xpe_enhance_advanced_init(nullptr);
        ASSERT_EQ(err, XPE_OK);
    }

    void TearDown() override {
        xpe_enhance_advanced_shutdown();
    }
};

// ============================================================================
// REQ-ADV-022: NULL Pointer Input Guard
// ============================================================================

TEST_F(ExposureIndexTest, NullImageReturnsInvalidInput) {
    XpeImageMetadata meta = MakeMeta("CHEST");
    float ei = 0.0f, di = 0.0f;
    EXPECT_EQ(xpe_calc_exposure_index(nullptr, &meta, &ei, &di), XPE_ERR_INVALID_INPUT);
}

TEST_F(ExposureIndexTest, NullMetaReturnsInvalidInput) {
    XpeImageBuffer img = MakeConstantImage(32, 32, 500.0f);
    float ei = 0.0f, di = 0.0f;
    EXPECT_EQ(xpe_calc_exposure_index(&img, nullptr, &ei, &di), XPE_ERR_INVALID_INPUT);
    FreeImageBuffer(img);
}

TEST_F(ExposureIndexTest, NullEiOutReturnsInvalidInput) {
    XpeImageBuffer img = MakeConstantImage(32, 32, 500.0f);
    XpeImageMetadata meta = MakeMeta("CHEST");
    float di = 0.0f;
    EXPECT_EQ(xpe_calc_exposure_index(&img, &meta, nullptr, &di), XPE_ERR_INVALID_INPUT);
    FreeImageBuffer(img);
}

TEST_F(ExposureIndexTest, NullDiOutReturnsInvalidInput) {
    XpeImageBuffer img = MakeConstantImage(32, 32, 500.0f);
    XpeImageMetadata meta = MakeMeta("CHEST");
    float ei = 0.0f;
    EXPECT_EQ(xpe_calc_exposure_index(&img, &meta, &ei, nullptr), XPE_ERR_INVALID_INPUT);
    FreeImageBuffer(img);
}

// ============================================================================
// REQ-ADV-013: Valid EI/DI Calculation
// ============================================================================

TEST_F(ExposureIndexTest, ValidInputReturnsPositiveEI) {
    XpeImageBuffer img = MakeConstantImage(64, 64, 1000.0f);
    XpeImageMetadata meta = MakeMeta("CHEST", 80.0f, 10.0f);
    float ei = 0.0f, di = 0.0f;

    XpeErrorCode err = xpe_calc_exposure_index(&img, &meta, &ei, &di);
    ASSERT_EQ(err, XPE_OK);

    // EI must be finite and positive
    EXPECT_TRUE(std::isfinite(ei)) << "EI is not finite: " << ei;
    EXPECT_GT(ei, 0.0f) << "EI must be positive";

    // DI must be finite
    EXPECT_TRUE(std::isfinite(di)) << "DI is not finite: " << di;

    FreeImageBuffer(img);
}

// ============================================================================
// Body-Part EI Target Lookup
// ============================================================================

TEST_F(ExposureIndexTest, DifferentBodyPartsProduceFiniteResults) {
    const char* bodyParts[] = {"CHEST", "ABDOMEN", "EXTREMITY", "SKULL", "SPINE", "PELVIS"};

    for (const char* bp : bodyParts) {
        XpeImageBuffer img = MakeConstantImage(32, 32, 500.0f);
        XpeImageMetadata meta = MakeMeta(bp, 80.0f, 10.0f);
        float ei = 0.0f, di = 0.0f;

        XpeErrorCode err = xpe_calc_exposure_index(&img, &meta, &ei, &di);
        ASSERT_EQ(err, XPE_OK) << "Failed for body part: " << bp;
        EXPECT_TRUE(std::isfinite(ei)) << "EI not finite for " << bp;
        EXPECT_TRUE(std::isfinite(di)) << "DI not finite for " << bp;

        FreeImageBuffer(img);
    }
}

TEST_F(ExposureIndexTest, ChestLatVsChestPA) {
    // Chest LAT should have different EI target than Chest PA
    XpeImageBuffer img = MakeConstantImage(32, 32, 500.0f);

    XpeImageMetadata metaPA = MakeMeta("CHEST", 80.0f, 10.0f);
    XpeImageMetadata metaLAT = MakeMeta("CHEST LAT", 80.0f, 10.0f);

    float eiPA = 0.0f, diPA = 0.0f;
    float eiLAT = 0.0f, diLAT = 0.0f;

    ASSERT_EQ(xpe_calc_exposure_index(&img, &metaPA, &eiPA, &diPA), XPE_OK);
    ASSERT_EQ(xpe_calc_exposure_index(&img, &metaLAT, &eiLAT, &diLAT), XPE_OK);

    // EI should be the same (same image, same gain)
    EXPECT_FLOAT_EQ(eiPA, eiLAT);

    // DI should differ (different EI_target)
    // CHEST PA target = 250, CHEST LAT target = 200
    // Same EI but different target -> different DI
    EXPECT_NE(diPA, diLAT);

    FreeImageBuffer(img);
}

// ============================================================================
// REQ-ADV-071: Format Validation
// ============================================================================

TEST_F(ExposureIndexTest, Uint16FormatReturnsUnsupportedFormat) {
    XpeImageBuffer img{};
    img.width = 32;
    img.height = 32;
    img.format = XPE_PIXEL_UINT16;
    img.dataSize = 32 * 32 * sizeof(uint16_t);
    img.data = new uint16_t[32 * 32]();

    XpeImageMetadata meta = MakeMeta("CHEST");
    float ei = 0.0f, di = 0.0f;

    EXPECT_EQ(xpe_calc_exposure_index(&img, &meta, &ei, &di), XPE_ERR_UNSUPPORTED_FORMAT);
    delete[] static_cast<uint16_t*>(img.data);
}

// ============================================================================
// REQ-ADV-070: Dimension Validation
// ============================================================================

TEST_F(ExposureIndexTest, ZeroDimensionReturnsInvalidInput) {
    XpeImageBuffer img{};
    img.width = 0;
    img.height = 0;
    img.format = XPE_PIXEL_FLOAT32;

    XpeImageMetadata meta = MakeMeta("CHEST");
    float ei = 0.0f, di = 0.0f;

    EXPECT_EQ(xpe_calc_exposure_index(&img, &meta, &ei, &di), XPE_ERR_INVALID_INPUT);
}

// ============================================================================
// REQ-ADV-032: NaN/Inf Output Guard
// ============================================================================

TEST_F(ExposureIndexTest, ZeroImageProducesFiniteOutput) {
    XpeImageBuffer img = MakeConstantImage(64, 64, 0.0f);
    XpeImageMetadata meta = MakeMeta("CHEST", 80.0f, 10.0f);
    float ei = 0.0f, di = 0.0f;

    XpeErrorCode err = xpe_calc_exposure_index(&img, &meta, &ei, &di);
    if (err == XPE_OK) {
        EXPECT_TRUE(std::isfinite(ei)) << "EI not finite from zero image";
        EXPECT_TRUE(std::isfinite(di)) << "DI not finite from zero image";
    }
    FreeImageBuffer(img);
}

TEST_F(ExposureIndexTest, ZeroKvpMasProducesFiniteOutput) {
    XpeImageBuffer img = MakeConstantImage(64, 64, 500.0f);
    XpeImageMetadata meta = MakeMeta("CHEST", 0.0f, 0.0f);
    float ei = 0.0f, di = 0.0f;

    XpeErrorCode err = xpe_calc_exposure_index(&img, &meta, &ei, &di);
    if (err == XPE_OK) {
        EXPECT_TRUE(std::isfinite(ei)) << "EI not finite for zero exposure";
        EXPECT_TRUE(std::isfinite(di)) << "DI not finite for zero exposure";
    }
    FreeImageBuffer(img);
}

TEST_F(ExposureIndexTest, ImageWithNaNPixelsProducesFiniteOutput) {
    XpeImageBuffer img = MakeConstantImage(32, 32, 1.0f);
    float* data = static_cast<float*>(img.data);
    data[32 * 32 / 2] = std::numeric_limits<float>::quiet_NaN();

    XpeImageMetadata meta = MakeMeta("CHEST", 80.0f, 10.0f);
    float ei = 0.0f, di = 0.0f;

    XpeErrorCode err = xpe_calc_exposure_index(&img, &meta, &ei, &di);
    if (err == XPE_OK) {
        EXPECT_TRUE(std::isfinite(ei)) << "NaN propagated to EI";
        EXPECT_TRUE(std::isfinite(di)) << "NaN propagated to DI";
    }
    FreeImageBuffer(img);
}

// ============================================================================
// EI/DI Scaling Verification
// ============================================================================

TEST_F(ExposureIndexTest, HigherMeanProducesHigherEI) {
    XpeImageBuffer imgLow = MakeConstantImage(32, 32, 100.0f);
    XpeImageBuffer imgHigh = MakeConstantImage(32, 32, 1000.0f);
    XpeImageMetadata meta = MakeMeta("CHEST", 80.0f, 10.0f);

    float eiLow = 0.0f, diLow = 0.0f;
    float eiHigh = 0.0f, diHigh = 0.0f;

    ASSERT_EQ(xpe_calc_exposure_index(&imgLow, &meta, &eiLow, &diLow), XPE_OK);
    ASSERT_EQ(xpe_calc_exposure_index(&imgHigh, &meta, &eiHigh, &diHigh), XPE_OK);

    EXPECT_GT(eiHigh, eiLow) << "Higher mean should produce higher EI";

    FreeImageBuffer(imgLow);
    FreeImageBuffer(imgHigh);
}

TEST_F(ExposureIndexTest, HigherKvpProducesHigherEI) {
    XpeImageBuffer img = MakeConstantImage(32, 32, 500.0f);
    XpeImageMetadata metaLow = MakeMeta("CHEST", 60.0f, 10.0f);
    XpeImageMetadata metaHigh = MakeMeta("CHEST", 120.0f, 10.0f);

    float eiLow = 0.0f, diLow = 0.0f;
    float eiHigh = 0.0f, diHigh = 0.0f;

    ASSERT_EQ(xpe_calc_exposure_index(&img, &metaLow, &eiLow, &diLow), XPE_OK);
    ASSERT_EQ(xpe_calc_exposure_index(&img, &metaHigh, &eiHigh, &diHigh), XPE_OK);

    // Higher kVp -> higher gain -> higher EI (for same image)
    EXPECT_GT(eiHigh, eiLow) << "Higher kVp should produce higher EI";

    FreeImageBuffer(img);
}

// ============================================================================
// Unknown Body Part Handling
// ============================================================================

TEST_F(ExposureIndexTest, UnknownBodyPartUsesDefault) {
    XpeImageBuffer img = MakeConstantImage(32, 32, 500.0f);
    XpeImageMetadata meta = MakeMeta("UNKNOWN_BODY_PART", 80.0f, 10.0f);
    float ei = 0.0f, di = 0.0f;

    XpeErrorCode err = xpe_calc_exposure_index(&img, &meta, &ei, &di);
    ASSERT_EQ(err, XPE_OK);
    EXPECT_TRUE(std::isfinite(ei));
    EXPECT_TRUE(std::isfinite(di));

    FreeImageBuffer(img);
}
