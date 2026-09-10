/**
 * @file test_gain_correct.cpp
 * @brief Tests for SWU-1.2: xpe_gain_correct (REQ-P1A-016 to REQ-P1A-019)
 *        Validates uint16->float32 domain transition.
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 *
 * #117 decision B (QA-A-20): the gain map is not a parameter. It is loaded into
 * the global calibration by xpe_calib_load_gain() and the entry point is
 * xpe_gain_correct(input, output, metadata). This suite was originally written
 * against the map-argument signature and was unregistered from XPE_TEST_SOURCES
 * for that reason; it is rewritten here against the shipped contract.
 */

#include <gtest/gtest.h>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_writer.hpp"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

namespace {

class GainCorrectTest : public ::testing::Test {
protected:
    static constexpr uint32_t W = 4;
    static constexpr uint32_t H = 4;

    std::vector<uint16_t> rawPixels;
    std::vector<float>    outPixels;
    XpeImageBuffer   input{};
    XpeImageBuffer   output{};
    XpeImageMetadata metadata{};
    const char* gainPath = "test_gain_correct_gain.xcal";

    void SetUp() override {
        xpe_preprocess_init(nullptr);

        rawPixels.assign(W * H, 2000);
        outPixels.assign(W * H, 0.0f);

        input.data          = rawPixels.data();
        input.width         = W;
        input.height        = H;
        input.bitsAllocated = 16;
        input.bitsStored    = 16;
        input.format        = XPE_PIXEL_UINT16;
        input.dataSize      = rawPixels.size() * sizeof(uint16_t);

        output.data          = outPixels.data();
        output.width         = W;
        output.height        = H;
        output.bitsAllocated = 32;
        output.bitsStored    = 32;
        output.format        = XPE_PIXEL_FLOAT32;
        output.dataSize      = outPixels.size() * sizeof(float);

        loadGainMap(1.5f);
    }

    void TearDown() override {
        std::remove(gainPath);
        std::remove("test_gain_correct_gain.xcal.tmp");
        xpe_preprocess_shutdown();
    }

    // Writes a uniform gain map and loads it into the global calibration.
    void loadGainMap(float value) {
        std::remove(gainPath);
        std::remove("test_gain_correct_gain.xcal.tmp");

        const std::vector<float> values(W * H, value);

        XCalFileHeader hdr{};
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version      = XCAL_VERSION;
        hdr.type         = static_cast<uint32_t>(XCAL_TYPE_GAIN);
        hdr.pixel_format = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width        = W;
        hdr.height       = H;
        hdr.payload_len  = static_cast<uint64_t>(values.size() * sizeof(float));

        ASSERT_EQ(XPE_OK,
                  write_xcal_file(gainPath, hdr, nullptr, 0,
                                  reinterpret_cast<const uint8_t*>(values.data()),
                                  hdr.payload_len));
        ASSERT_EQ(XPE_OK, xpe_calib_load_gain(gainPath));
    }
};

// REQ-P1A-016: corrected[i] = img[i] / gain[i] (flat-field normalization).
TEST_F(GainCorrectTest, AppliesGainCorrection) {
    ASSERT_EQ(XPE_OK, xpe_gain_correct(&input, &output, &metadata));
    const auto* out = static_cast<const float*>(output.data);
    EXPECT_NEAR(2000.0f / 1.5f, out[0], 1e-3f);
}

// REQ-P1A-017: the output buffer carries float32 after conversion.
TEST_F(GainCorrectTest, OutputFormatIsFloat32) {
    ASSERT_EQ(XPE_OK, xpe_gain_correct(&input, &output, &metadata));
    EXPECT_EQ(XPE_PIXEL_FLOAT32, output.format);
}

TEST_F(GainCorrectTest, NullInputReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_gain_correct(nullptr, &output, &metadata));
}

TEST_F(GainCorrectTest, NullOutputReturnsError) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_gain_correct(&input, nullptr, &metadata));
}

// Output dimensions must match the input; a mismatch is a buffer problem.
TEST_F(GainCorrectTest, DimensionMismatchReturnsError) {
    output.height = H + 1;
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL, xpe_gain_correct(&input, &output, &metadata));
}

// Unity gain: values pass through unchanged, in float.
TEST_F(GainCorrectTest, UnityGainPreservesValues) {
    loadGainMap(1.0f);

    ASSERT_EQ(XPE_OK, xpe_gain_correct(&input, &output, &metadata));
    const auto* out = static_cast<const float*>(output.data);
    EXPECT_NEAR(2000.0f, out[0], 1e-3f);
}

// Zero dimensions are rejected before any buffer access.
TEST_F(GainCorrectTest, ZeroWidthReturnsError) {
    input.width = 0;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_gain_correct(&input, &output, &metadata));
}

// #123: a non-zero dataSize smaller than the dimensions require is refused
// before the kernel reads past the allocation.
TEST_F(GainCorrectTest, TruncatedInputDataSizeReturnsError) {
    input.dataSize = rawPixels.size() * sizeof(uint16_t) - sizeof(uint16_t);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_gain_correct(&input, &output, &metadata));
}

// #117: the map is global state, so "no gain map" is its own condition --
// distinct from "the module was never initialized". Replaces the old
// truncated-gain-argument case, which no longer has an argument to truncate.
TEST_F(GainCorrectTest, GainMapNotLoadedReturnsCalibNotLoaded) {
    xpe_preprocess_shutdown();
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));

    EXPECT_EQ(XPE_ERR_CALIB_NOT_LOADED, xpe_gain_correct(&input, &output, &metadata));
}

// The output byte size reflects the float32 conversion.
TEST_F(GainCorrectTest, OutputDataSizeEqualsPixelCountTimesFloat) {
    ASSERT_EQ(XPE_OK, xpe_gain_correct(&input, &output, &metadata));
    EXPECT_EQ(W * H * sizeof(float), output.dataSize);
}

} // namespace
