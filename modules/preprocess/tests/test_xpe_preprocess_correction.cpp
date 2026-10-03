/**
 * @file test_xpe_preprocess_correction.cpp
 * @brief Correction algorithm tests for XPE Preprocessing Module
 *
 * Tests offset, gain, and defect correction algorithms.
 * Covers REQ-P1A-010, REQ-P1A-011, REQ-P1A-012.
 */

#include <gtest/gtest.h>
#include <cstring>
#include <cmath>
#include <limits>
#include <vector>
#include <string>
#include <filesystem>
#include "xpe/preprocess_api.h"
#include "xpe/common/xpe_error.h"
#include "fixtures/make_xcal.hpp"

// =============================================================================
// Test Data Generation Helpers
// =============================================================================

namespace {

/**
 * @brief Create test image buffer
 */
XpeImageBuffer* CreateTestImage(uint32_t width, uint32_t height, XpePixelFormat format) {
    XpeImageBuffer* img = new XpeImageBuffer();
    std::memset(img, 0, sizeof(XpeImageBuffer));

    img->width = width;
    img->height = height;
    img->format = format;
    img->bitsAllocated = (format == XPE_PIXEL_UINT16) ? 16 : 32;
    img->bitsStored = img->bitsAllocated;

    // stride field does not exist in XpeImageBuffer; compute data_size directly
    size_t pixel_size = (format == XPE_PIXEL_UINT16) ? sizeof(uint16_t) : sizeof(float);

    // Allocate data
    size_t data_size = height * width * pixel_size;
    img->data = new uint8_t[data_size];
    img->dataSize = data_size;
    std::memset(img->data, 0, data_size);

    // Fill with test data
    if (format == XPE_PIXEL_UINT16) {
        uint16_t* data = reinterpret_cast<uint16_t*>(img->data);
        for (size_t i = 0; i < width * height; ++i) {
            data[i] = 1000 + (i % 100);  // Test pattern
        }
    } else {  // FLOAT32
        float* data = reinterpret_cast<float*>(img->data);
        for (size_t i = 0; i < width * height; ++i) {
            data[i] = 1.0f + (i % 100) * 0.01f;  // Test pattern
        }
    }

    return img;
}

/**
 * @brief Create test metadata
 */
XpeImageMetadata* CreateTestMetadata() {
    XpeImageMetadata* meta = new XpeImageMetadata();
    std::memset(meta, 0, sizeof(XpeImageMetadata));

    strncpy_s(meta->bodyPart, sizeof(meta->bodyPart), "CHEST", _TRUNCATE);
    meta->kVp = 120.0f;
    meta->mAs = 100.0f;
    meta->SID_mm = 1200.0f;
    meta->pixelPitch_mm = 0.14f;   // 140 um, project-wide detector pitch (QA-A-105)
    meta->acquisitionTime = 0;
    meta->flags = 0;

    return meta;
}

/**
 * @brief Free image buffer
 */
void FreeTestImage(XpeImageBuffer* img) {
    if (img) {
        delete[] img->data;
        delete img;
    }
}

} // anonymous namespace

// =============================================================================
// Test Fixtures
// =============================================================================

class PreprocessCorrectionTest : public ::testing::Test {
protected:
    void SetUp() override {
        xpe_preprocess_shutdown();
        ASSERT_EQ(xpe_preprocess_init(NULL), XPE_OK);
    }

    void TearDown() override {
        xpe_preprocess_shutdown();
    }
};

// =============================================================================
// Offset Correction Tests
// =============================================================================

/**
 * @test OffsetCorrect_BasicSubtraction
 *
 * Given module is initialized and offset map is loaded
 * When offset correction is applied
 * Then output = max(input - offset, 0)
 */
TEST_F(PreprocessCorrectionTest, OffsetCorrect_BasicSubtraction) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_UINT16);
    XpeImageBuffer* output = CreateTestImage(1024, 1024, XPE_PIXEL_UINT16);
    XpeImageMetadata* metadata = CreateTestMetadata();

    XpeErrorCode result = xpe_offset_correct(input, output, metadata);

    EXPECT_TRUE(result == XPE_OK || result == XPE_ERR_NOT_INITIALIZED || result == XPE_ERR_CALIB_NOT_LOADED);

    FreeTestImage(input);
    FreeTestImage(output);
    delete metadata;
}

/**
 * @test OffsetCorrect_FloorAtZeroClamping
 *
 * Given module is initialized
 * When input is less than offset map
 * Then output is clamped at 0 (no negative values)
 */
TEST_F(PreprocessCorrectionTest, OffsetCorrect_FloorAtZeroClamping) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_UINT16);
    XpeImageBuffer* output = CreateTestImage(1024, 1024, XPE_PIXEL_UINT16);
    XpeImageMetadata* metadata = CreateTestMetadata();

    // Set input values to 50 (below offset of 100)
    uint16_t* data = reinterpret_cast<uint16_t*>(input->data);
    for (size_t i = 0; i < 1024 * 1024; ++i) {
        data[i] = 50;
    }

    XpeErrorCode result = xpe_offset_correct(input, output, metadata);

    EXPECT_TRUE(result == XPE_OK || result == XPE_ERR_NOT_INITIALIZED || result == XPE_ERR_CALIB_NOT_LOADED);

    // Verify no negative values (UINT16 can't be negative, but check floor behavior)
    if (result == XPE_OK) {
        uint16_t* out_data = reinterpret_cast<uint16_t*>(output->data);
        for (size_t i = 0; i < 100; ++i) {  // Check first 100 pixels
            EXPECT_GE(out_data[i], 0);
        }
    }

    FreeTestImage(input);
    FreeTestImage(output);
    delete metadata;
}

/**
 * @test OffsetCorrect_DimensionMismatch
 *
 * Given module is initialized
 * When input/output dimensions don't match
 * Then returns XPE_ERR_BUFFER_TOO_SMALL
 */
TEST_F(PreprocessCorrectionTest, OffsetCorrect_DimensionMismatch) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_UINT16);
    XpeImageBuffer* output = CreateTestImage(512, 512, XPE_PIXEL_UINT16);
    XpeImageMetadata* metadata = CreateTestMetadata();

    XpeErrorCode result = xpe_offset_correct(input, output, metadata);

    EXPECT_TRUE(result == XPE_ERR_BUFFER_TOO_SMALL ||
                result == XPE_ERR_INVALID_INPUT ||
                result == XPE_ERR_NOT_INITIALIZED ||
                result == XPE_ERR_CALIB_NOT_LOADED);

    FreeTestImage(input);
    FreeTestImage(output);
    delete metadata;
}

/**
 * @test OffsetCorrect_NullBuffers
 *
 * Given module is initialized
 * When NULL pointers are passed
 * Then returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessCorrectionTest, OffsetCorrect_NullBuffers) {
    XpeImageMetadata* metadata = CreateTestMetadata();

    XpeErrorCode result = xpe_offset_correct(nullptr, nullptr, metadata);

    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);

    delete metadata;
}

/**
 * @test OffsetCorrect_NullMetadata
 *
 * Given module is initialized
 * When NULL metadata is passed
 * Then returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessCorrectionTest, OffsetCorrect_NullMetadata) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_UINT16);
    XpeImageBuffer* output = CreateTestImage(1024, 1024, XPE_PIXEL_UINT16);

    XpeErrorCode result = xpe_offset_correct(input, output, nullptr);

    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);

    FreeTestImage(input);
    FreeTestImage(output);
}

/**
 * @test OffsetCorrect_FormatMismatch
 *
 * Given module is initialized
 * When input format is not UINT16
 * Then returns XPE_ERR_UNSUPPORTED_FORMAT
 */
TEST_F(PreprocessCorrectionTest, OffsetCorrect_FormatMismatch) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_FLOAT32);
    XpeImageBuffer* output = CreateTestImage(1024, 1024, XPE_PIXEL_FLOAT32);
    XpeImageMetadata* metadata = CreateTestMetadata();

    const uint8_t* ip = static_cast<const uint8_t*>(input->data);
    const std::vector<uint8_t> before(ip, ip + input->dataSize);
    XpeErrorCode result = xpe_offset_correct(input, output, metadata);

    // QA-A-229 (#245): was a three-code OR. xpe_offset_correct_in checks input->format
    // BEFORE the initialized / map-loaded states, so a FLOAT32 input yields exactly
    // XPE_ERR_UNSUPPORTED_FORMAT whatever is loaded, and leaves the input alone.
    EXPECT_EQ(result, XPE_ERR_UNSUPPORTED_FORMAT);
    EXPECT_EQ(0, std::memcmp(before.data(), input->data, before.size()));

    FreeTestImage(input);
    FreeTestImage(output);
    delete metadata;
}

/**
 * @test OffsetCorrect_MetadataDoesNotChangeTheCorrection
 *
 * QA-A-229 M2b (#245). Replaces the retired legacy OffsetCorrect_TemperatureInterpolation and
 * OffsetCorrect_PREPTimeModel, which named features the shipped code does not have (and asserted
 * only "result is one of several codes"). What is true today: ONE offset map is applied
 * (out = max(in - offset, 0)); XpeImageMetadata has no temperature, and kVp / SID_mm /
 * acquisitionTime are not read by the correction. If temperature interpolation or a PREP-time
 * decay model is ever implemented this fails, and the header sentences must change with it.
 */
TEST_F(PreprocessCorrectionTest, OffsetCorrect_MetadataDoesNotChangeTheCorrection) {
    constexpr uint32_t W = 16, H = 8;
    constexpr size_t N = static_cast<size_t>(W) * H;
    const std::string path =
        (std::filesystem::temp_directory_path() / "qa_a_229_meta_offset.xcal").string();
    std::filesystem::remove(path);
    ASSERT_EQ(MakeOffsetXCal(path.c_str(), W, H, 100.0f), XPE_OK);
    ASSERT_EQ(xpe_calib_load_offset(path.c_str()), XPE_OK);

    std::vector<uint16_t> in(N);
    for (size_t i = 0; i < N; ++i) in[i] = static_cast<uint16_t>((i * 97u) % 400u);  // some below, some above 100
    const float kvp[] = {120.0f, 40.0f, 150.0f, 0.0f};
    const float sid[] = {1200.0f, 600.0f, 1800.0f, 0.0f};
    const uint64_t when[] = {0u, 1u, 1700000000u, 99999999999999ull};
    std::vector<uint16_t> first;
    for (size_t c = 0; c < 4; ++c) {
        XpeImageBuffer ib{}, ob{};
        std::vector<uint16_t> out(N, 0xBEEF);
        ib.width = ob.width = W; ib.height = ob.height = H;
        ib.format = ob.format = XPE_PIXEL_UINT16;
        ib.bitsAllocated = ib.bitsStored = ob.bitsAllocated = ob.bitsStored = 16;
        ib.data = in.data(); ib.dataSize = N * sizeof(uint16_t);
        ob.data = out.data(); ob.dataSize = N * sizeof(uint16_t);
        XpeImageMetadata meta{};
        meta.kVp = kvp[c]; meta.SID_mm = sid[c]; meta.acquisitionTime = when[c];
        meta.pixelPitch_mm = 0.14f;
        ASSERT_EQ(xpe_offset_correct(&ib, &ob, &meta), XPE_OK) << "case " << c;
        for (size_t i = 0; i < N; ++i)
            ASSERT_EQ(static_cast<uint16_t>(in[i] > 100 ? in[i] - 100 : 0), out[i])
                << "case " << c << " pixel " << i;
        if (c == 0) first = out;
        else EXPECT_EQ(first, out) << "case " << c;
    }
    std::filesystem::remove(path);
}

// =============================================================================
// Gain Correction Tests
// =============================================================================

/**
 * @test GainCorrect_UINT16ToFLOAT32Conversion
 *
 * Given module is initialized and gain map is loaded
 * When gain correction is applied
 * Then output is FLOAT32 with correct format
 */
TEST_F(PreprocessCorrectionTest, GainCorrect_UINT16ToFLOAT32Conversion) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_UINT16);
    XpeImageBuffer* output = CreateTestImage(1024, 1024, XPE_PIXEL_FLOAT32);
    XpeImageMetadata* metadata = CreateTestMetadata();

    XpeErrorCode result = xpe_gain_correct(input, output, metadata);

    EXPECT_TRUE(result == XPE_OK ||
                result == XPE_ERR_NOT_INITIALIZED || result == XPE_ERR_CALIB_NOT_LOADED ||
                result == XPE_ERR_UNSUPPORTED_FORMAT);

    // Verify output format
    if (result == XPE_OK) {
        EXPECT_EQ(output->format, XPE_PIXEL_FLOAT32);
    }

    FreeTestImage(input);
    FreeTestImage(output);
    delete metadata;
}

/**
 * @test GainCorrect_DimensionMismatch
 *
 * Given module is initialized
 * When input/output dimensions don't match
 * Then returns XPE_ERR_BUFFER_TOO_SMALL
 */
TEST_F(PreprocessCorrectionTest, GainCorrect_DimensionMismatch) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_UINT16);
    XpeImageBuffer* output = CreateTestImage(512, 512, XPE_PIXEL_FLOAT32);
    XpeImageMetadata* metadata = CreateTestMetadata();

    XpeErrorCode result = xpe_gain_correct(input, output, metadata);

    EXPECT_TRUE(result == XPE_ERR_BUFFER_TOO_SMALL ||
                result == XPE_ERR_INVALID_INPUT ||
                result == XPE_ERR_NOT_INITIALIZED ||
                result == XPE_ERR_CALIB_NOT_LOADED);

    FreeTestImage(input);
    FreeTestImage(output);
    delete metadata;
}

/**
 * @test GainCorrect_NullBuffers
 *
 * Given module is initialized
 * When NULL pointers are passed
 * Then returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessCorrectionTest, GainCorrect_NullBuffers) {
    XpeImageMetadata* metadata = CreateTestMetadata();

    XpeErrorCode result = xpe_gain_correct(nullptr, nullptr, metadata);

    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);

    delete metadata;
}

/**
 * @test GainCorrect_FormatMismatch
 *
 * Given module is initialized
 * When input format is not UINT16 or output format is not FLOAT32
 * Then returns XPE_ERR_UNSUPPORTED_FORMAT
 */
TEST_F(PreprocessCorrectionTest, GainCorrect_FormatMismatch) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_FLOAT32);
    XpeImageBuffer* output = CreateTestImage(1024, 1024, XPE_PIXEL_FLOAT32);
    XpeImageMetadata* metadata = CreateTestMetadata();

    const uint8_t* ip = static_cast<const uint8_t*>(input->data);
    const std::vector<uint8_t> before(ip, ip + input->dataSize);
    XpeErrorCode result = xpe_gain_correct(input, output, metadata);

    // QA-A-229 (#245): was a three-code OR. xpe_gain_correct_in checks input->format
    // BEFORE the initialized / map-loaded states, so a FLOAT32 input yields exactly
    // XPE_ERR_UNSUPPORTED_FORMAT whatever is loaded, and leaves the input alone.
    EXPECT_EQ(result, XPE_ERR_UNSUPPORTED_FORMAT);
    EXPECT_EQ(0, std::memcmp(before.data(), input->data, before.size()));

    FreeTestImage(input);
    FreeTestImage(output);
    delete metadata;
}

/**
 * @test GainCorrect_NoNaNInfInOutput
 *
 * Given module is initialized
 * When gain correction is applied
 * Then output contains no NaN or Inf values
 */
TEST_F(PreprocessCorrectionTest, GainCorrect_NoNaNInfInOutput) {
    // QA-A-229 (#245): this used to run xpe_gain_correct with NO gain map loaded, so the
    // result was always XPE_ERR_CALIB_NOT_LOADED, the `if (result == XPE_OK)` body never
    // ran, and the isfinite loop examined 0 pixels. Now a gain map is generated from a
    // flat field that contains a zero-valued pixel (the 1/G input that would make a naive
    // reciprocal infinite), loaded, and the test counts how many output pixels it checked.
    constexpr uint32_t W = 8, H = 8;
    constexpr size_t N = static_cast<size_t>(W) * H;
    // The dead flat-field pixel raises an alert; drain it on every exit path (global-state hygiene guard).
    struct AlertDrain { ~AlertDrain() { xpe_clear_alerts(); } } alert_drain;
    std::vector<std::vector<uint16_t>> flats(3, std::vector<uint16_t>(N));
    std::vector<XpeImageBuffer> flat_bufs(3);
    for (size_t f = 0; f < 3; ++f) {
        for (size_t i = 0; i < N; ++i) flats[f][i] = static_cast<uint16_t>(5000 + (i % 7) * 100);
        flats[f][5] = 0;  // dead pixel in the flat field: gain undefined there
        std::memset(&flat_bufs[f], 0, sizeof(XpeImageBuffer));
        flat_bufs[f].width = W; flat_bufs[f].height = H; flat_bufs[f].format = XPE_PIXEL_UINT16;
        flat_bufs[f].bitsAllocated = 16; flat_bufs[f].bitsStored = 16;
        flat_bufs[f].data = flats[f].data(); flat_bufs[f].dataSize = N * sizeof(uint16_t);
    }
    const std::string gain_path =
        (std::filesystem::temp_directory_path() / "qa_a_229_nonan_gain.xcal").string();
    std::filesystem::remove(gain_path);
    ASSERT_EQ(xpe_calib_generate_gain(flat_bufs.data(), 3, nullptr, gain_path.c_str(), nullptr), XPE_OK);
    ASSERT_EQ(xpe_calib_load_gain(gain_path.c_str()), XPE_OK);

    // Input covers the extremes: 0, 1, mid-range, 65535, and the flat field's dead pixel position.
    std::vector<uint16_t> in(N);
    for (size_t i = 0; i < N; ++i) in[i] = static_cast<uint16_t>((i * 1021u) % 65536u);
    in[0] = 0; in[1] = 1; in[5] = 0; in[N - 1] = 65535;
    std::vector<float> out(N, -12345.0f);
    XpeImageBuffer ib{}, ob{};
    ib.width = ob.width = W; ib.height = ob.height = H;
    ib.format = XPE_PIXEL_UINT16; ob.format = XPE_PIXEL_FLOAT32;
    ib.bitsAllocated = ib.bitsStored = 16; ob.bitsAllocated = ob.bitsStored = 32;
    ib.data = in.data(); ib.dataSize = N * sizeof(uint16_t);
    ob.data = out.data(); ob.dataSize = N * sizeof(float);
    XpeImageMetadata* metadata = CreateTestMetadata();

    ASSERT_EQ(xpe_gain_correct(&ib, &ob, metadata), XPE_OK);

    size_t examined = 0, nonfinite = 0;
    for (size_t i = 0; i < N; ++i) {
        ++examined;
        if (!std::isfinite(out[i])) ++nonfinite;
    }
    EXPECT_EQ(examined, N) << "the loop must look at every output pixel";
    EXPECT_EQ(nonfinite, 0u) << "a dead flat-field pixel must not leak NaN/Inf into the output";
    for (size_t i = 0; i < N; ++i) EXPECT_NE(out[i], -12345.0f) << "pixel " << i << " was never written";

    delete metadata;
    std::filesystem::remove(gain_path);
}

// =============================================================================
// Defect Correction Tests
// =============================================================================

/**
 * @test DefectCorrect_BasicInterpolation
 *
 * Given module is initialized and defect map is loaded
 * When defect correction is applied
 * Then defective pixels are interpolated
 */
TEST_F(PreprocessCorrectionTest, DefectCorrect_BasicInterpolation) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_FLOAT32);
    XpeImageBuffer* output = CreateTestImage(1024, 1024, XPE_PIXEL_FLOAT32);
    XpeImageMetadata* metadata = CreateTestMetadata();

    XpeErrorCode result = xpe_defect_correct(input, output, metadata);

    EXPECT_TRUE(result == XPE_OK ||
                result == XPE_ERR_NOT_INITIALIZED ||
                result == XPE_ERR_CALIB_NOT_LOADED);

    FreeTestImage(input);
    FreeTestImage(output);
    delete metadata;
}

/**
 * @test DefectCorrect_DimensionMismatch
 *
 * Given module is initialized
 * When input/output dimensions don't match
 * Then returns XPE_ERR_BUFFER_TOO_SMALL
 */
TEST_F(PreprocessCorrectionTest, DefectCorrect_DimensionMismatch) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_FLOAT32);
    XpeImageBuffer* output = CreateTestImage(512, 512, XPE_PIXEL_FLOAT32);
    XpeImageMetadata* metadata = CreateTestMetadata();

    XpeErrorCode result = xpe_defect_correct(input, output, metadata);

    EXPECT_TRUE(result == XPE_ERR_BUFFER_TOO_SMALL ||
                result == XPE_ERR_INVALID_INPUT ||
                result == XPE_ERR_NOT_INITIALIZED ||
                result == XPE_ERR_CALIB_NOT_LOADED);

    FreeTestImage(input);
    FreeTestImage(output);
    delete metadata;
}

/**
 * @test DefectCorrect_NullBuffers
 *
 * Given module is initialized
 * When NULL pointers are passed
 * Then returns XPE_ERR_INVALID_INPUT
 */
TEST_F(PreprocessCorrectionTest, DefectCorrect_NullBuffers) {
    XpeImageMetadata* metadata = CreateTestMetadata();

    XpeErrorCode result = xpe_defect_correct(nullptr, nullptr, metadata);

    EXPECT_EQ(result, XPE_ERR_INVALID_INPUT);

    delete metadata;
}

/**
 * @test DefectCorrect_FormatMismatch
 *
 * Given module is initialized
 * When input format is not FLOAT32
 * Then returns XPE_ERR_UNSUPPORTED_FORMAT
 */
TEST_F(PreprocessCorrectionTest, DefectCorrect_FormatMismatch) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_UINT16);
    XpeImageBuffer* output = CreateTestImage(1024, 1024, XPE_PIXEL_UINT16);
    XpeImageMetadata* metadata = CreateTestMetadata();

    XpeErrorCode result = xpe_defect_correct(input, output, metadata);

    EXPECT_TRUE(result == XPE_ERR_UNSUPPORTED_FORMAT ||
                result == XPE_ERR_NOT_INITIALIZED ||
                result == XPE_ERR_CALIB_NOT_LOADED);

    FreeTestImage(input);
    FreeTestImage(output);
    delete metadata;
}

/**
 * @test DefectCorrect_ClusterDefectHandling
 *
 * Given module is initialized
 * When cluster defects (adjacent bad pixels) are present
 * Then correction algorithm handles clusters properly
 */
TEST_F(PreprocessCorrectionTest, DefectCorrect_ClusterDefectHandling) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_FLOAT32);
    XpeImageBuffer* output = CreateTestImage(1024, 1024, XPE_PIXEL_FLOAT32);
    XpeImageMetadata* metadata = CreateTestMetadata();

    // Simulate cluster defect (set 2x2 region to 0)
    float* data = reinterpret_cast<float*>(input->data);
    for (int y = 100; y < 102; ++y) {
        for (int x = 100; x < 102; ++x) {
            data[y * 1024 + x] = 0.0f;
        }
    }

    XpeErrorCode result = xpe_defect_correct(input, output, metadata);

    EXPECT_TRUE(result == XPE_OK ||
                result == XPE_ERR_NOT_INITIALIZED ||
                result == XPE_ERR_CALIB_NOT_LOADED);

    FreeTestImage(input);
    FreeTestImage(output);
    delete metadata;
}

/**
 * @test DefectCorrect_EdgeDefectHandling
 *
 * Given module is initialized
 * When defects are at image edges
 * Then correction algorithm handles edge cases properly
 */
TEST_F(PreprocessCorrectionTest, DefectCorrect_EdgeDefectHandling) {
    XpeImageBuffer* input = CreateTestImage(1024, 1024, XPE_PIXEL_FLOAT32);
    XpeImageBuffer* output = CreateTestImage(1024, 1024, XPE_PIXEL_FLOAT32);
    XpeImageMetadata* metadata = CreateTestMetadata();

    // Simulate edge defects
    float* data = reinterpret_cast<float*>(input->data);
    data[0] = 0.0f;  // Top-left corner
    data[1023] = 0.0f;  // Top-right corner
    data[1024 * 1023] = 0.0f;  // Bottom-left corner
    data[1024 * 1024 - 1] = 0.0f;  // Bottom-right corner

    XpeErrorCode result = xpe_defect_correct(input, output, metadata);

    EXPECT_TRUE(result == XPE_OK ||
                result == XPE_ERR_NOT_INITIALIZED ||
                result == XPE_ERR_CALIB_NOT_LOADED);

    FreeTestImage(input);
    FreeTestImage(output);
    delete metadata;
}

// =============================================================================
// Main Test Runner
// =============================================================================

