/**
 * @file test_xcal_validator.cpp
 * @brief Unit tests for XCal v1 header validator (T-002)
 *
 * SPEC-XPE-P1A SUP-01 -- REQ-P1A-014, REQ-P1A-015, REQ-P1A-016
 *
 * 11 invariant tests:
 *  1.  Valid OFFSET header -> XPE_OK
 *  2.  Valid GAIN header -> XPE_OK
 *  3.  Valid DEFECT header -> XPE_OK
 *  4.  Bad magic -> XPE_ERR_CONFIG_INVALID
 *  5.  Bad version (version=2) -> XPE_ERR_CONFIG_INVALID
 *  6.  type out of range (4) -> XPE_ERR_CONFIG_INVALID
 *  6b. GAIN_POLY payload = whole number of coefficient planes (QA-A-37)
 *  7.  pixel_format out of range (5) -> XPE_ERR_CONFIG_INVALID
 *  8.  width = 0 -> XPE_ERR_CONFIG_INVALID
 *  9.  height > XCAL_MAX_DIM -> XPE_ERR_CONFIG_INVALID
 * 10.  payload_len mismatch -> XPE_ERR_CONFIG_INVALID
 * 11.  config_json_len > cap -> XPE_ERR_CONFIG_INVALID
 * 12.  expected_type mismatch -> XPE_ERR_CONFIG_INVALID
 * 13.  type-format semantic mismatch (OFFSET + UINT8_MASK) -> CONFIG_INVALID
 */

#include <gtest/gtest.h>
#include <cstring>
#include "xpe/preprocess/xcal_format.h"
#include "xpe/common/xpe_error.h"
#include "xcal_validator.hpp"

namespace {

// Helper: build a valid OFFSET header (512x512, FLOAT32)
XCalFileHeader MakeValidOffsetHeader(uint32_t w = 512, uint32_t h = 512) {
    XCalFileHeader hdr;
    std::memset(&hdr, 0, sizeof(hdr));
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version         = XCAL_VERSION;
    hdr.type            = XCAL_TYPE_OFFSET;
    hdr.pixel_format    = XCAL_FMT_FLOAT32;
    hdr.width           = w;
    hdr.height          = h;
    hdr.created_epoch_ms = 1000LL;
    hdr.expiry_epoch_ms  = 0LL;   // never expires
    hdr.config_json_len  = 0;
    hdr.payload_len = static_cast<uint64_t>(w) * h * sizeof(float);
    return hdr;
}

// Helper: build a valid GAIN header (512x512, FLOAT32)
XCalFileHeader MakeValidGainHeader(uint32_t w = 512, uint32_t h = 512) {
    XCalFileHeader hdr = MakeValidOffsetHeader(w, h);
    hdr.type = XCAL_TYPE_GAIN;
    return hdr;
}

// Helper: build a valid DEFECT header (512x512, UINT8_MASK)
XCalFileHeader MakeValidDefectHeader(uint32_t w = 512, uint32_t h = 512) {
    XCalFileHeader hdr;
    std::memset(&hdr, 0, sizeof(hdr));
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version         = XCAL_VERSION;
    hdr.type            = XCAL_TYPE_DEFECT;
    hdr.pixel_format    = XCAL_FMT_UINT8_MASK;
    hdr.width           = w;
    hdr.height          = h;
    hdr.created_epoch_ms = 1000LL;
    hdr.expiry_epoch_ms  = 0LL;
    hdr.config_json_len  = 0;
    hdr.payload_len = static_cast<uint64_t>(w) * h * sizeof(uint8_t);
    return hdr;
}

} // anonymous namespace

// =============================================================================
// Test 1: Valid OFFSET header
// =============================================================================
TEST(XCalValidatorTest, ValidOffsetHeader_ReturnsOk) {
    XCalFileHeader hdr = MakeValidOffsetHeader(512, 512);
    EXPECT_EQ(validate_xcal_header(hdr, XCAL_TYPE_OFFSET), XPE_OK);
}

// =============================================================================
// Test 2: Valid GAIN header
// =============================================================================
TEST(XCalValidatorTest, ValidGainHeader_ReturnsOk) {
    XCalFileHeader hdr = MakeValidGainHeader(1024, 1024);
    EXPECT_EQ(validate_xcal_header(hdr, XCAL_TYPE_GAIN), XPE_OK);
}

// =============================================================================
// Test 3: Valid DEFECT header
// =============================================================================
TEST(XCalValidatorTest, ValidDefectHeader_ReturnsOk) {
    XCalFileHeader hdr = MakeValidDefectHeader(256, 256);
    EXPECT_EQ(validate_xcal_header(hdr, XCAL_TYPE_DEFECT), XPE_OK);
}

// =============================================================================
// Test 4: Bad magic -> CONFIG_INVALID
// =============================================================================
TEST(XCalValidatorTest, BadMagic_ReturnsConfigInvalid) {
    XCalFileHeader hdr = MakeValidOffsetHeader();
    hdr.magic[0] = 'B';  // corrupt first byte
    EXPECT_EQ(validate_xcal_header(hdr), XPE_ERR_CONFIG_INVALID);
}

// =============================================================================
// Test 5: Version != 1 -> CONFIG_INVALID
// =============================================================================
TEST(XCalValidatorTest, BadVersion_ReturnsConfigInvalid) {
    XCalFileHeader hdr = MakeValidOffsetHeader();
    hdr.version = 2;
    EXPECT_EQ(validate_xcal_header(hdr), XPE_ERR_CONFIG_INVALID);
}

// =============================================================================
// Test 6: type out of range -> CONFIG_INVALID
// =============================================================================
TEST(XCalValidatorTest, TypeOutOfRange_ReturnsConfigInvalid) {
    XCalFileHeader hdr = MakeValidOffsetHeader();
    // QA-A-37 (#140): 3 is XCAL_TYPE_GAIN_POLY, declared in xcal_format.h since
    // the format was written -- the "only 0,1,2 are valid" premise this case
    // used to encode never matched the header. 4 is the first value with no
    // type behind it.
    hdr.type = 4;
    EXPECT_EQ(validate_xcal_header(hdr), XPE_ERR_CONFIG_INVALID);
}

// =============================================================================
// Test 6b: XCAL_TYPE_GAIN_POLY is a real type, and its payload is a whole
// number of coefficient planes rather than exactly one (QA-A-37, #140).
// =============================================================================
TEST(XCalValidatorTest, GainPolyAcceptsWholeNumberOfPlanes) {
    XCalFileHeader hdr = MakeValidOffsetHeader();
    hdr.type = static_cast<uint32_t>(XCAL_TYPE_GAIN_POLY);

    const uint64_t plane = hdr.payload_len;  // W * H * sizeof(float)
    EXPECT_EQ(validate_xcal_header(hdr), XPE_OK) << "one plane = degree 0";

    hdr.payload_len = plane * 3;
    EXPECT_EQ(validate_xcal_header(hdr), XPE_OK) << "three planes = degree 2";

    hdr.payload_len = plane + 4;
    EXPECT_EQ(validate_xcal_header(hdr), XPE_ERR_CONFIG_INVALID) << "ragged";

    hdr.payload_len = 0;
    EXPECT_EQ(validate_xcal_header(hdr), XPE_ERR_CONFIG_INVALID) << "no planes";
}

// =============================================================================
// Test 7: pixel_format out of range -> CONFIG_INVALID
// =============================================================================
TEST(XCalValidatorTest, PixelFormatOutOfRange_ReturnsConfigInvalid) {
    XCalFileHeader hdr = MakeValidOffsetHeader();
    hdr.pixel_format = 5;
    EXPECT_EQ(validate_xcal_header(hdr), XPE_ERR_CONFIG_INVALID);
}

// =============================================================================
// Test 8: width == 0 -> CONFIG_INVALID
// =============================================================================
TEST(XCalValidatorTest, WidthZero_ReturnsConfigInvalid) {
    XCalFileHeader hdr = MakeValidOffsetHeader();
    hdr.width       = 0;
    hdr.payload_len = 0;  // would need adjustment; keep it zero to avoid overflow
    EXPECT_EQ(validate_xcal_header(hdr), XPE_ERR_CONFIG_INVALID);
}

// =============================================================================
// Test 9: height > XCAL_MAX_DIM -> CONFIG_INVALID
// =============================================================================
TEST(XCalValidatorTest, HeightExceedsMax_ReturnsConfigInvalid) {
    XCalFileHeader hdr = MakeValidOffsetHeader(512, XCAL_MAX_DIM + 1);
    EXPECT_EQ(validate_xcal_header(hdr), XPE_ERR_CONFIG_INVALID);
}

// =============================================================================
// Test 10: payload_len mismatch -> CONFIG_INVALID
// =============================================================================
TEST(XCalValidatorTest, PayloadLenMismatch_ReturnsConfigInvalid) {
    XCalFileHeader hdr = MakeValidOffsetHeader(512, 512);
    hdr.payload_len += 1;  // off by one byte
    EXPECT_EQ(validate_xcal_header(hdr), XPE_ERR_CONFIG_INVALID);
}

// =============================================================================
// Test 11: config_json_len > cap -> CONFIG_INVALID
// =============================================================================
TEST(XCalValidatorTest, ConfigJsonLenOverCap_ReturnsConfigInvalid) {
    XCalFileHeader hdr = MakeValidOffsetHeader();
    hdr.config_json_len = XCAL_MAX_CONFIG_JSON_LEN + 1;
    EXPECT_EQ(validate_xcal_header(hdr), XPE_ERR_CONFIG_INVALID);
}

// =============================================================================
// Test 12: expected_type mismatch -> CONFIG_INVALID
// =============================================================================
TEST(XCalValidatorTest, ExpectedTypeMismatch_ReturnsConfigInvalid) {
    XCalFileHeader hdr = MakeValidOffsetHeader();
    // Validate expecting GAIN (1) but header has OFFSET (0)
    EXPECT_EQ(validate_xcal_header(hdr, XCAL_TYPE_GAIN), XPE_ERR_CONFIG_INVALID);
}

// =============================================================================
// Test 13: type-format semantic mismatch (OFFSET with UINT8_MASK) -> INVALID
// =============================================================================
TEST(XCalValidatorTest, TypeFormatSemanticMismatch_ReturnsConfigInvalid) {
    XCalFileHeader hdr = MakeValidOffsetHeader(512, 512);
    hdr.pixel_format = XCAL_FMT_UINT8_MASK;
    // Payload len must also be adjusted to avoid a payload mismatch error first;
    // set it to the UINT8_MASK-correct size so we test the semantic check.
    hdr.payload_len = static_cast<uint64_t>(hdr.width) * hdr.height * 1;
    EXPECT_EQ(validate_xcal_header(hdr, XCAL_TYPE_OFFSET), XPE_ERR_CONFIG_INVALID);
}

// =============================================================================
// Test 14: xcal_bytes_per_pixel helper correctness
// =============================================================================
TEST(XCalValidatorTest, BytesPerPixel_CorrectValues) {
    EXPECT_EQ(xcal_bytes_per_pixel(XCAL_FMT_UINT16),    2u);
    EXPECT_EQ(xcal_bytes_per_pixel(XCAL_FMT_FLOAT32),   4u);
    EXPECT_EQ(xcal_bytes_per_pixel(XCAL_FMT_UINT8_MASK), 1u);
    EXPECT_EQ(xcal_bytes_per_pixel(99),                  0u);  // unknown
}
