/**
 * @file test_datasize_guard.cpp
 * @brief Per-entry-point regression cases for the XpeImageBuffer.dataSize
 *        size-consistency guard (#123, QA-B-21).
 *
 * Contract (api-spec "XpeImageBuffer.dataSize on input"):
 *   - dataSize == 0                              -> unspecified, accepted
 *   - 0 < dataSize < width*height*bpp(format)    -> XPE_ERR_INVALID_INPUT
 *   - dataSize >= width*height*bpp(format)       -> accepted
 *
 * enhance_basic routes all seven image entry points through
 * validate_float32_image() (enhance_basic_internal.h), which calls
 * xpe_data_size_is_consistent(). Every entry point gets one short-dataSize
 * case and one dataSize==0 case, so the guard is proven to fire on each path
 * rather than merely to compile.
 *
 * The backing allocation is always FULL (32*32 floats). Only the declared
 * dataSize is short, so a case that reaches the implementation reads inside
 * its allocation -- the RED run (guard removed) fails on the return code, not
 * by corrupting memory.
 */

#include <gtest/gtest.h>
#include <vector>
#include <cstdint>

#include "xpe/enhance_basic/enhance_basic_api.h"
#include "xpe/common/xpe_error.h"

namespace {

constexpr uint32_t kW = 32;
constexpr uint32_t kH = 32;
constexpr size_t   kFullBytes = static_cast<size_t>(kW) * kH * 4;
/* Declares 32x32 FLOAT32 but claims room for only 16 pixels. */
constexpr size_t   kShortBytes = static_cast<size_t>(16) * 4;

class DataSizeGuardTest : public ::testing::Test {
protected:
    void SetUp() override {
        storage_.assign(static_cast<size_t>(kW) * kH, 0.5f);
    }

    /* Fully allocated 32x32 FLOAT32 image with a caller-chosen dataSize. */
    XpeImageBuffer Make(size_t declaredDataSize) {
        XpeImageBuffer img{};
        img.width         = kW;
        img.height        = kH;
        img.bitsAllocated = 32;
        img.bitsStored    = 32;
        img.format        = XPE_PIXEL_FLOAT32;
        img.data          = storage_.data();
        img.dataSize      = declaredDataSize;
        return img;
    }

    XpeImageBuffer Short() { return Make(kShortBytes); }
    XpeImageBuffer Unspecified() { return Make(0); }

    static XpeNoiseReduceParams NrParams() {
        XpeNoiseReduceParams p{};
        p.mode          = XPE_NOISE_BILATERAL;
        p.sigma_space   = 3.0f;
        p.sigma_range   = 50.0f;
        p.search_window = 21;
        p.patch_size    = 7;
        p.h_param       = 10.0f;
        return p;
    }

    static XpeImageMetadata Meta() {
        XpeImageMetadata m{};
        m.kVp           = 80.0f;
        m.mAs           = 2.5f;
        m.SID_mm        = 1800.0f;
        m.pixelPitch_mm = 0.148f;
        return m;
    }

    std::vector<float> storage_;
};

/* ---------------------------------------------------------------------------
 * Short dataSize -> INVALID_INPUT, one case per entry point
 * ------------------------------------------------------------------------- */

TEST_F(DataSizeGuardTest, LogTransform_ShortDataSize_ReturnsInvalidInput) {
    XpeImageBuffer img = Short();
    EXPECT_EQ(xpe_log_transform(&img, 1.0f), XPE_ERR_INVALID_INPUT);
}

TEST_F(DataSizeGuardTest, LogInverse_ShortDataSize_ReturnsInvalidInput) {
    XpeImageBuffer img = Short();
    EXPECT_EQ(xpe_log_inverse(&img, 1.0f), XPE_ERR_INVALID_INPUT);
}

TEST_F(DataSizeGuardTest, NoiseReduce_ShortDataSize_ReturnsInvalidInput) {
    XpeImageBuffer img = Short();
    XpeNoiseReduceParams p = NrParams();
    EXPECT_EQ(xpe_noise_reduce(&img, &p), XPE_ERR_INVALID_INPUT);
}

TEST_F(DataSizeGuardTest, NoiseEstimateSigma_ShortDataSize_ReturnsInvalidInput) {
    XpeImageBuffer img = Short();
    float sigma = 0.0f;
    EXPECT_EQ(xpe_noise_estimate_sigma(&img, &sigma), XPE_ERR_INVALID_INPUT);
}

TEST_F(DataSizeGuardTest, ContrastEnhance_ShortDataSize_ReturnsInvalidInput) {
    XpeImageBuffer img = Short();
    EXPECT_EQ(xpe_contrast_enhance(&img, nullptr), XPE_ERR_INVALID_INPUT);
}

TEST_F(DataSizeGuardTest, EdgeEnhance_ShortDataSize_ReturnsInvalidInput) {
    XpeImageBuffer img = Short();
    EXPECT_EQ(xpe_edge_enhance(&img, nullptr), XPE_ERR_INVALID_INPUT);
}

TEST_F(DataSizeGuardTest, CalcExposureIndex_ShortDataSize_ReturnsInvalidInput) {
    XpeImageBuffer img = Short();
    XpeImageMetadata meta = Meta();
    float ei = 0.0f, di = 0.0f;
    EXPECT_EQ(xpe_calc_exposure_index(&img, &meta, &ei, &di), XPE_ERR_INVALID_INPUT);
}

/* ---------------------------------------------------------------------------
 * dataSize == 0 -> unspecified, must NOT be rejected
 * ------------------------------------------------------------------------- */

TEST_F(DataSizeGuardTest, LogTransform_ZeroDataSize_Accepted) {
    XpeImageBuffer img = Unspecified();
    EXPECT_EQ(xpe_log_transform(&img, 1.0f), XPE_OK);
}

TEST_F(DataSizeGuardTest, LogInverse_ZeroDataSize_Accepted) {
    XpeImageBuffer img = Unspecified();
    EXPECT_EQ(xpe_log_inverse(&img, 1.0f), XPE_OK);
}

TEST_F(DataSizeGuardTest, NoiseReduce_ZeroDataSize_Accepted) {
    XpeImageBuffer img = Unspecified();
    XpeNoiseReduceParams p = NrParams();
    EXPECT_EQ(xpe_noise_reduce(&img, &p), XPE_OK);
}

TEST_F(DataSizeGuardTest, NoiseEstimateSigma_ZeroDataSize_Accepted) {
    XpeImageBuffer img = Unspecified();
    float sigma = -1.0f;
    EXPECT_EQ(xpe_noise_estimate_sigma(&img, &sigma), XPE_OK);
}

TEST_F(DataSizeGuardTest, ContrastEnhance_ZeroDataSize_Accepted) {
    XpeImageBuffer img = Unspecified();
    EXPECT_EQ(xpe_contrast_enhance(&img, nullptr), XPE_OK);
}

TEST_F(DataSizeGuardTest, EdgeEnhance_ZeroDataSize_Accepted) {
    XpeImageBuffer img = Unspecified();
    EXPECT_EQ(xpe_edge_enhance(&img, nullptr), XPE_OK);
}

TEST_F(DataSizeGuardTest, CalcExposureIndex_ZeroDataSize_Accepted) {
    XpeImageBuffer img = Unspecified();
    XpeImageMetadata meta = Meta();
    float ei = 0.0f, di = 0.0f;
    EXPECT_NE(xpe_calc_exposure_index(&img, &meta, &ei, &di), XPE_ERR_INVALID_INPUT);
}

/* ---------------------------------------------------------------------------
 * Oversized dataSize is explicitly allowed by the contract.
 * ------------------------------------------------------------------------- */

TEST_F(DataSizeGuardTest, LogTransform_OversizedDataSize_Accepted) {
    XpeImageBuffer img = Make(kFullBytes * 2);
    EXPECT_EQ(xpe_log_transform(&img, 1.0f), XPE_OK);
}

/* ---------------------------------------------------------------------------
 * ASan probe (#123 QA-B-21 step 4): a GENUINELY short allocation.
 *
 * Unlike the cases above, the backing buffer really holds only 16 floats while
 * the image declares 32x32. With the guard in place these return
 * XPE_ERR_INVALID_INPUT and nothing is read. With the guard removed the
 * implementation reads 32*32 floats out of a 16-float allocation, which is the
 * heap-buffer-overflow READ the ASan run is meant to observe.
 * ------------------------------------------------------------------------- */

TEST(DataSizeGuardAsanProbe, LogTransform_TrulyShortBuffer_RejectedBeforeRead) {
    std::vector<float> tiny(16, 0.5f);
    XpeImageBuffer img{};
    img.width         = kW;
    img.height        = kH;
    img.bitsAllocated = 32;
    img.bitsStored    = 32;
    img.format        = XPE_PIXEL_FLOAT32;
    img.data          = tiny.data();
    img.dataSize      = tiny.size() * sizeof(float);
    EXPECT_EQ(xpe_log_transform(&img, 1.0f), XPE_ERR_INVALID_INPUT);
}

TEST(DataSizeGuardAsanProbe, NoiseEstimateSigma_TrulyShortBuffer_RejectedBeforeRead) {
    std::vector<float> tiny(16, 0.5f);
    XpeImageBuffer img{};
    img.width         = kW;
    img.height        = kH;
    img.bitsAllocated = 32;
    img.bitsStored    = 32;
    img.format        = XPE_PIXEL_FLOAT32;
    img.data          = tiny.data();
    img.dataSize      = tiny.size() * sizeof(float);
    float sigma = 0.0f;
    EXPECT_EQ(xpe_noise_estimate_sigma(&img, &sigma), XPE_ERR_INVALID_INPUT);
}

}  // namespace
