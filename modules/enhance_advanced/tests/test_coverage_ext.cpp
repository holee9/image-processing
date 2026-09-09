/**
 * @file test_coverage_ext.cpp
 * @brief Functional cases for config-option and degenerate-input paths (#120).
 *
 * The CI coverage-post run reported five files under the 0.85 per-file target.
 * Classifying the uncovered lines showed most of the reachable gap was config
 * parsing (enhance_advanced_helpers.cpp) plus a few degenerate-input paths that
 * no existing case exercised. These are functional cases: every assertion below
 * states the documented behaviour, not whatever the code happens to return.
 *
 * SPEC: SPEC-XPE-P2-ADV
 */

#include <gtest/gtest.h>

#include "xpe/enhance_advanced/xpe_enhance_advanced_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <cmath>
#include <cstring>
#include <limits>
#include <vector>

namespace {

XpeImageBuffer MakeImage(uint32_t w, uint32_t h, std::vector<float>& storage,
                         float fill = 500.0f) {
    storage.assign(static_cast<size_t>(w) * h, fill);
    XpeImageBuffer img{};
    img.width  = w;
    img.height = h;
    img.format = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = 32;
    img.bitsStored    = 32;
    img.data     = storage.data();
    img.dataSize = storage.size() * sizeof(float);
    return img;
}

XpeImageMetadata MakeMeta(const char* bodyPart = "CHEST",
                          float kVp = 80.0f, float mAs = 10.0f) {
    XpeImageMetadata meta{};
    std::snprintf(meta.bodyPart, sizeof(meta.bodyPart), "%s", bodyPart);
    meta.kVp = kVp;
    meta.mAs = mAs;
    return meta;
}

class EnhanceAdvancedConfigTest : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_EQ(xpe_enhance_advanced_init(nullptr), XPE_OK);
    }
    void TearDown() override {
        xpe_enhance_advanced_shutdown();
    }
};

} // namespace

/* ============================================================================
 * Config option branches — accepted keys must not change the return contract
 * ========================================================================= */

// Legacy flat "levels" key (the parser also accepts "num_levels").
TEST_F(EnhanceAdvancedConfigTest, MultiscaleAcceptsLegacyLevelsKey) {
    std::vector<float> storage;
    XpeImageBuffer img = MakeImage(32, 32, storage);
    XpeImageMetadata meta = MakeMeta();
    EXPECT_EQ(xpe_multiscale_process(&img, &meta, "{\"levels\": 4}"), XPE_OK);
}

// Out-of-range values are clamped, not rejected.
// "levels" is deliberately absent here — see DISABLED_ case at the end of this
// file: clamping it to XPE_MFP_MAX_LEVELS crashes on a 32x32 image (#120).
TEST_F(EnhanceAdvancedConfigTest, MultiscaleClampsOutOfRangeGains) {
    std::vector<float> storage;
    XpeImageBuffer img = MakeImage(32, 32, storage);
    XpeImageMetadata meta = MakeMeta();
    EXPECT_EQ(xpe_multiscale_process(
                  &img, &meta,
                  "{\"edge_gain\": 42.0, \"texture_gain\": -3.0,"
                  " \"flat_gain\": 7.5, \"noise_threshold\": 999.0}"),
              XPE_OK);
}

TEST_F(EnhanceAdvancedConfigTest, FractionalAcceptsStepSizeKey) {
    std::vector<float> storage;
    XpeImageBuffer img = MakeImage(32, 32, storage);
    EXPECT_EQ(xpe_fractional_process(&img, 1.0f, "{\"step_size\": 0.5}"), XPE_OK);
}

TEST_F(EnhanceAdvancedConfigTest, FractionalClampsOutOfRangeStepSize) {
    std::vector<float> storage;
    XpeImageBuffer img = MakeImage(32, 32, storage);
    EXPECT_EQ(xpe_fractional_process(&img, 1.0f, "{\"step_size\": 99.0}"), XPE_OK);
}

TEST_F(EnhanceAdvancedConfigTest, CollimationAcceptsAllConfigKeys) {
    std::vector<float> storage;
    XpeImageBuffer img = MakeImage(64, 64, storage);
    int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    EXPECT_EQ(xpe_detect_collimation(
                  &img, &x0, &y0, &x1, &y1,
                  "{\"sensitivity\": 0.7, \"min_area_ratio\": 0.2,"
                  " \"border_margin\": 8}"),
              XPE_OK);
}

TEST_F(EnhanceAdvancedConfigTest, CollimationClampsOutOfRangeConfigKeys) {
    std::vector<float> storage;
    XpeImageBuffer img = MakeImage(64, 64, storage);
    int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    EXPECT_EQ(xpe_detect_collimation(
                  &img, &x0, &y0, &x1, &y1,
                  "{\"sensitivity\": 9.0, \"min_area_ratio\": -1.0,"
                  " \"border_margin\": 4096}"),
              XPE_OK);
}

// A config string that is not valid JSON is rejected by the parser, and the
// entry point reports it as a configuration error.
TEST_F(EnhanceAdvancedConfigTest, CollimationRejectsMalformedConfig) {
    std::vector<float> storage;
    XpeImageBuffer img = MakeImage(64, 64, storage);
    int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    EXPECT_EQ(xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, "{not json"),
              XPE_ERR_CONFIG_INVALID);
}

/* ============================================================================
 * Degenerate inputs — documented guards that no case exercised
 * ========================================================================= */

// A buffer with valid dimensions but no pixel storage is invalid input.
TEST_F(EnhanceAdvancedConfigTest, MultiscaleRejectsNullPixelData) {
    XpeImageBuffer img{};
    img.width  = 32;
    img.height = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.data   = nullptr;
    XpeImageMetadata meta = MakeMeta();
    EXPECT_EQ(xpe_multiscale_process(&img, &meta, nullptr), XPE_ERR_INVALID_INPUT);
}

// A buffer with valid dimensions but no pixel storage is invalid input — the
// same contract the other three entry points already applied (#122). Measured
// before the fix: multiscale/fractional/collimation returned INVALID_INPUT and
// only this one returned XPE_OK with a plausible-looking EI/DI pair, which is
// worse than an error because a caller would display it as a measurement.
TEST_F(EnhanceAdvancedConfigTest, ExposureIndexRejectsNullPixelData) {
    XpeImageBuffer img{};
    img.width  = 32;
    img.height = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.data   = nullptr;
    img.dataSize = 0;
    XpeImageMetadata meta = MakeMeta();
    float ei = 0.0f, di = 0.0f;
    EXPECT_EQ(xpe_calc_exposure_index(&img, &meta, &ei, &di), XPE_ERR_INVALID_INPUT);
}

// REQ-ADV-032: outputs stay finite even when the exposure inputs are not.
TEST_F(EnhanceAdvancedConfigTest, ExposureIndexNonFiniteTechniqueFactorsStayFinite) {
    std::vector<float> storage;
    XpeImageBuffer img = MakeImage(32, 32, storage);
    XpeImageMetadata meta =
        MakeMeta("CHEST", std::numeric_limits<float>::quiet_NaN(),
                 std::numeric_limits<float>::quiet_NaN());
    float ei = 0.0f, di = 0.0f;
    ASSERT_EQ(xpe_calc_exposure_index(&img, &meta, &ei, &di), XPE_OK);
    EXPECT_TRUE(std::isfinite(ei));
    EXPECT_TRUE(std::isfinite(di));
}

TEST_F(EnhanceAdvancedConfigTest, ExposureIndexAllNonFinitePixelsStayFinite) {
    std::vector<float> storage;
    XpeImageBuffer img =
        MakeImage(32, 32, storage, std::numeric_limits<float>::quiet_NaN());
    XpeImageMetadata meta = MakeMeta();
    float ei = 0.0f, di = 0.0f;
    ASSERT_EQ(xpe_calc_exposure_index(&img, &meta, &ei, &di), XPE_OK);
    EXPECT_TRUE(std::isfinite(ei));
    EXPECT_TRUE(std::isfinite(di));
}

TEST_F(EnhanceAdvancedConfigTest, ExposureIndexZeroSignalStaysFinite) {
    std::vector<float> storage;
    XpeImageBuffer img = MakeImage(32, 32, storage, 0.0f);
    XpeImageMetadata meta = MakeMeta();
    float ei = 0.0f, di = 0.0f;
    ASSERT_EQ(xpe_calc_exposure_index(&img, &meta, &ei, &di), XPE_OK);
    EXPECT_TRUE(std::isfinite(ei));
    EXPECT_TRUE(std::isfinite(di));
}

/* ============================================================================
 * Defect found while writing the cases above (#120) — NOT a test problem
 *
 * "levels" is clamped to XPE_MFP_MAX_LEVELS (8, internal.h:42) with no regard
 * for the image size, and the pyramid then reads past the end of the buffer.
 * Config {"levels": 99} on a square FLOAT32 image, observed:
 *
 *     32x32  -> access violation (0xc0000005) inside the call
 *     64x64  -> returns XPE_OK, then the process faults at exit
 *
 * The 64x64 case is the dangerous one: the call reports success and the damage
 * only surfaces later, so a "return code is XPE_OK" check does not detect it.
 * Larger sizes were not observed to fault, but a clean exit is weak evidence
 * for the same reason — no size is asserted safe here.
 *
 * Fixed in #121 (QA-B-17): LaplacianPyramid now bounds the level count by
 * floor(log2(min(w,h))) + 1. The cases above are the regression guard; ASan is
 * the observation tool, because the 64x64 arm returned XPE_OK while corrupting
 * the heap — a return code cannot detect this class of defect.
 * ========================================================================= */
TEST_F(EnhanceAdvancedConfigTest, MultiscaleMaxLevelsOnSmallImage) {
    std::vector<float> storage;
    XpeImageBuffer img = MakeImage(32, 32, storage);
    XpeImageMetadata meta = MakeMeta();
    EXPECT_EQ(xpe_multiscale_process(&img, &meta, "{\"levels\": 99}"), XPE_OK);
}

TEST_F(EnhanceAdvancedConfigTest, MultiscaleMaxLevelsOn64x64) {
    std::vector<float> storage;
    XpeImageBuffer img = MakeImage(64, 64, storage);
    XpeImageMetadata meta = MakeMeta();
    EXPECT_EQ(xpe_multiscale_process(&img, &meta, "{\"levels\": 99}"), XPE_OK);
}

// Boundary sweep: the level count is bounded by floor(log2(min(w,h))) + 1, so
// the configured maximum (8) is affordable from 128x128 up and clamped below it.
// Every size must complete without corrupting memory — verified under ASan.
TEST_F(EnhanceAdvancedConfigTest, MultiscaleMaxLevelsAcrossSizes) {
    for (uint32_t n : {32u, 64u, 128u, 256u}) {
        std::vector<float> storage;
        XpeImageBuffer img = MakeImage(n, n, storage);
        XpeImageMetadata meta = MakeMeta();
        EXPECT_EQ(xpe_multiscale_process(&img, &meta, "{\"levels\": 99}"), XPE_OK)
            << "size " << n << "x" << n;
    }
}

// The derived bound uses min(w,h); these confirm that is the right dimension by
// exercising non-square and extreme aspect ratios under ASan (#121 follow-up).
TEST_F(EnhanceAdvancedConfigTest, MultiscaleMaxLevelsOnNonSquareImages) {
    const uint32_t sizes[][2] = { {256u, 32u}, {32u, 256u}, {1024u, 8u} };
    for (const auto& wh : sizes) {
        std::vector<float> storage;
        XpeImageBuffer img = MakeImage(wh[0], wh[1], storage);
        XpeImageMetadata meta = MakeMeta();
        EXPECT_EQ(xpe_multiscale_process(&img, &meta, "{\"levels\": 99}"), XPE_OK)
            << "size " << wh[0] << "x" << wh[1];
    }
}

// The smallest images the API accepts must also survive the maximum level count.
TEST_F(EnhanceAdvancedConfigTest, MultiscaleMaxLevelsOnTinyImages) {
    for (uint32_t n : {1u, 2u, 3u, 5u}) {
        std::vector<float> storage;
        XpeImageBuffer img = MakeImage(n, n, storage);
        XpeImageMetadata meta = MakeMeta();
        EXPECT_EQ(xpe_multiscale_process(&img, &meta, "{\"levels\": 99}"), XPE_OK)
            << "size " << n << "x" << n;
    }
}

/* ============================================================================
 * dataSize size-consistency contract (#123)
 *
 * api-spec "XpeImageBuffer.dataSize on input":
 *   dataSize == 0                        -> unspecified, not checked
 *   0 < dataSize < w*h*bpp(format)       -> XPE_ERR_INVALID_INPUT
 *   dataSize >= w*h*bpp(format)          -> accepted
 *
 * The short-buffer arms allocate only 16 floats while declaring 32x32, so
 * without the guard the module reads past the allocation — ASan is the
 * observation tool here, not the return code.
 * ========================================================================= */

namespace {
// Declares 32x32 FLOAT32 but allocates only 16 floats.
XpeImageBuffer MakeShortBuffer(std::vector<float>& storage) {
    storage.assign(16, 500.0f);
    XpeImageBuffer img{};
    img.width  = 32;
    img.height = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.bitsAllocated = 32;
    img.bitsStored    = 32;
    img.data     = storage.data();
    img.dataSize = 16 * sizeof(float);   // < 32*32*4
    return img;
}
} // namespace

TEST_F(EnhanceAdvancedConfigTest, MultiscaleRejectsShortBuffer) {
    std::vector<float> storage;
    XpeImageBuffer img = MakeShortBuffer(storage);
    XpeImageMetadata meta = MakeMeta();
    EXPECT_EQ(xpe_multiscale_process(&img, &meta, nullptr), XPE_ERR_INVALID_INPUT);
}

TEST_F(EnhanceAdvancedConfigTest, FractionalRejectsShortBuffer) {
    std::vector<float> storage;
    XpeImageBuffer img = MakeShortBuffer(storage);
    EXPECT_EQ(xpe_fractional_process(&img, 1.0f, nullptr), XPE_ERR_INVALID_INPUT);
}

TEST_F(EnhanceAdvancedConfigTest, CollimationRejectsShortBuffer) {
    std::vector<float> storage;
    XpeImageBuffer img = MakeShortBuffer(storage);
    int32_t x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    EXPECT_EQ(xpe_detect_collimation(&img, &x0, &y0, &x1, &y1, nullptr),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(EnhanceAdvancedConfigTest, ExposureIndexRejectsShortBuffer) {
    std::vector<float> storage;
    XpeImageBuffer img = MakeShortBuffer(storage);
    XpeImageMetadata meta = MakeMeta();
    float ei = 0.0f, di = 0.0f;
    EXPECT_EQ(xpe_calc_exposure_index(&img, &meta, &ei, &di), XPE_ERR_INVALID_INPUT);
}

// dataSize == 0 stays accepted (legacy callers do not populate the field).
TEST_F(EnhanceAdvancedConfigTest, MultiscaleAcceptsUnspecifiedDataSize) {
    std::vector<float> storage;
    XpeImageBuffer img = MakeImage(32, 32, storage);
    img.dataSize = 0;
    XpeImageMetadata meta = MakeMeta();
    EXPECT_EQ(xpe_multiscale_process(&img, &meta, nullptr), XPE_OK);
}

// A larger dataSize than the declared dimensions is accepted.
TEST_F(EnhanceAdvancedConfigTest, MultiscaleAcceptsOversizedDataSize) {
    std::vector<float> storage;
    XpeImageBuffer img = MakeImage(32, 32, storage);
    img.dataSize *= 4;
    XpeImageMetadata meta = MakeMeta();
    EXPECT_EQ(xpe_multiscale_process(&img, &meta, nullptr), XPE_OK);
}
