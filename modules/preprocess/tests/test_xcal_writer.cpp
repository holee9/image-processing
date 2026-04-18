/**
 * @file test_xcal_writer.cpp
 * @brief Unit tests for XCal v1 writer (T-004)
 *
 * SPEC-XPE-P1A SUP-01 -- REQ-P1A-019, REQ-P1A-017
 *
 * Test cases:
 *  1.  512x512 OFFSET FLOAT32 round-trip: file size is exact
 *  2.  SHA-256 in header matches recomputed digest
 *  3.  Null path -> INVALID_INPUT
 *  4.  Payload nullptr with nonzero len -> INVALID_INPUT
 *  5.  config_json nullptr with nonzero len -> INVALID_INPUT
 *  6.  Write with config_json: file size = header + json + payload
 *  7.  Atomic write: .tmp file absent after success
 *  8.  DEFECT UINT8_MASK: correct file size
 */

#include <gtest/gtest.h>
#include <fstream>
#include <cstring>
#include <string>
#include <vector>
#include <cstdio>
#include "xpe/preprocess/xcal_format.h"
#include "xpe/common/xpe_error.h"
#include "xcal_writer.hpp"
#include "xpe_sha256.hpp"

namespace {

constexpr uint32_t W = 512;
constexpr uint32_t H = 512;

XCalFileHeader MakeOffsetHeader(uint32_t w = W, uint32_t h = H) {
    XCalFileHeader hdr;
    std::memset(&hdr, 0, sizeof(hdr));
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version          = XCAL_VERSION;
    hdr.type             = XCAL_TYPE_OFFSET;
    hdr.pixel_format     = XCAL_FMT_FLOAT32;
    hdr.width            = w;
    hdr.height           = h;
    hdr.created_epoch_ms = 0LL;
    hdr.expiry_epoch_ms  = 0LL;
    hdr.config_json_len  = 0;
    hdr.payload_len      = static_cast<uint64_t>(w) * h * sizeof(float);
    return hdr;
}

std::vector<float> MakeFloatPayload(uint32_t w, uint32_t h, float value = 1.0f) {
    return std::vector<float>(static_cast<size_t>(w) * h, value);
}

// Read exactly n bytes at offset from file
bool ReadFileAt(const char* path, uint64_t offset, uint8_t* buf, size_t n) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    f.seekg(static_cast<std::streamoff>(offset));
    f.read(reinterpret_cast<char*>(buf), static_cast<std::streamsize>(n));
    return f.good();
}

uint64_t FileSize(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return 0;
    return static_cast<uint64_t>(f.tellg());
}

} // anonymous namespace

class XCalWriterTest : public ::testing::Test {
protected:
    const char* test_path  = "t004_test.xcal";
    const char* tmp_path   = "t004_test.xcal.tmp";

    void TearDown() override {
        std::remove(test_path);
        std::remove(tmp_path);
    }
};

// =============================================================================
// Test 1: File size is exact (no config_json)
// =============================================================================
TEST_F(XCalWriterTest, OffsetFloat32_ExactFileSize) {
    auto payload = MakeFloatPayload(W, H);
    XCalFileHeader hdr = MakeOffsetHeader(W, H);

    XpeErrorCode rc = write_xcal_file(
        test_path, hdr,
        nullptr, 0,
        reinterpret_cast<const uint8_t*>(payload.data()),
        payload.size() * sizeof(float));

    ASSERT_EQ(rc, XPE_OK);

    uint64_t expected_size = sizeof(XCalFileHeader) +
                             static_cast<uint64_t>(W) * H * sizeof(float);
    EXPECT_EQ(FileSize(test_path), expected_size);
}

// =============================================================================
// Test 2: SHA-256 in header matches recomputed digest
// =============================================================================
TEST_F(XCalWriterTest, OffsetFloat32_ShaMatchesRecompute) {
    auto payload = MakeFloatPayload(W, H, 2.5f);
    XCalFileHeader hdr = MakeOffsetHeader(W, H);

    XpeErrorCode rc = write_xcal_file(
        test_path, hdr,
        nullptr, 0,
        reinterpret_cast<const uint8_t*>(payload.data()),
        payload.size() * sizeof(float));
    ASSERT_EQ(rc, XPE_OK);

    // Read sha256 from file header (at offset 120 in the 152-byte header)
    uint8_t file_sha[32];
    ASSERT_TRUE(ReadFileAt(test_path, 120, file_sha, 32));

    // Recompute SHA-256 of payload only (no config_json)
    auto expected_sha = compute_sha256(
        reinterpret_cast<const uint8_t*>(payload.data()),
        payload.size() * sizeof(float));

    EXPECT_EQ(std::memcmp(file_sha, expected_sha.data(), 32), 0);
}

// =============================================================================
// Test 3: Null path -> XPE_ERR_INVALID_INPUT
// =============================================================================
TEST_F(XCalWriterTest, NullPath_ReturnsInvalidInput) {
    auto payload = MakeFloatPayload(W, H);
    XCalFileHeader hdr = MakeOffsetHeader();

    XpeErrorCode rc = write_xcal_file(
        nullptr, hdr,
        nullptr, 0,
        reinterpret_cast<const uint8_t*>(payload.data()),
        payload.size() * sizeof(float));

    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
}

// =============================================================================
// Test 4: payload == nullptr with nonzero payload_len -> INVALID_INPUT
// =============================================================================
TEST_F(XCalWriterTest, NullPayloadNonzeroLen_ReturnsInvalidInput) {
    XCalFileHeader hdr = MakeOffsetHeader();

    XpeErrorCode rc = write_xcal_file(
        test_path, hdr,
        nullptr, 0,
        nullptr,        // null payload
        1024);          // nonzero len

    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
}

// =============================================================================
// Test 5: config_json == nullptr with nonzero len -> INVALID_INPUT
// =============================================================================
TEST_F(XCalWriterTest, NullConfigJsonNonzeroLen_ReturnsInvalidInput) {
    auto payload = MakeFloatPayload(W, H);
    XCalFileHeader hdr = MakeOffsetHeader();

    XpeErrorCode rc = write_xcal_file(
        test_path, hdr,
        nullptr, 99,    // null config_json, nonzero len
        reinterpret_cast<const uint8_t*>(payload.data()),
        payload.size() * sizeof(float));

    EXPECT_EQ(rc, XPE_ERR_INVALID_INPUT);
}

// =============================================================================
// Test 6: With config_json: file size = header + json_len + payload_len
// =============================================================================
TEST_F(XCalWriterTest, WithConfigJson_ExactFileSize) {
    auto payload = MakeFloatPayload(W, H);
    XCalFileHeader hdr = MakeOffsetHeader();

    const char* json = "{\"mode\":\"test\"}";
    uint64_t json_len = static_cast<uint64_t>(std::strlen(json));

    XpeErrorCode rc = write_xcal_file(
        test_path, hdr,
        reinterpret_cast<const uint8_t*>(json), json_len,
        reinterpret_cast<const uint8_t*>(payload.data()),
        payload.size() * sizeof(float));

    ASSERT_EQ(rc, XPE_OK);

    uint64_t expected_size = sizeof(XCalFileHeader) + json_len +
                             payload.size() * sizeof(float);
    EXPECT_EQ(FileSize(test_path), expected_size);
}

// =============================================================================
// Test 7: Atomic write -- .tmp file absent after success
// =============================================================================
TEST_F(XCalWriterTest, AtomicWrite_TmpFileAbsentAfterSuccess) {
    auto payload = MakeFloatPayload(W, H);
    XCalFileHeader hdr = MakeOffsetHeader();

    XpeErrorCode rc = write_xcal_file(
        test_path, hdr,
        nullptr, 0,
        reinterpret_cast<const uint8_t*>(payload.data()),
        payload.size() * sizeof(float));

    ASSERT_EQ(rc, XPE_OK);

    // .tmp file must not exist after successful write
    std::ifstream tmp_check(tmp_path);
    EXPECT_FALSE(tmp_check.good()) << ".tmp file should be absent after success";
}

// =============================================================================
// Test 8: DEFECT UINT8_MASK -- correct file size
// =============================================================================
TEST_F(XCalWriterTest, DefectUint8Mask_ExactFileSize) {
    const uint32_t DW = 256, DH = 256;
    std::vector<uint8_t> defect(static_cast<size_t>(DW) * DH, 0);

    XCalFileHeader hdr;
    std::memset(&hdr, 0, sizeof(hdr));
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version          = XCAL_VERSION;
    hdr.type             = XCAL_TYPE_DEFECT;
    hdr.pixel_format     = XCAL_FMT_UINT8_MASK;
    hdr.width            = DW;
    hdr.height           = DH;
    hdr.payload_len      = static_cast<uint64_t>(DW) * DH * sizeof(uint8_t);

    XpeErrorCode rc = write_xcal_file(
        test_path, hdr,
        nullptr, 0,
        defect.data(),
        defect.size() * sizeof(uint8_t));

    ASSERT_EQ(rc, XPE_OK);

    uint64_t expected_size = sizeof(XCalFileHeader) +
                             static_cast<uint64_t>(DW) * DH;
    EXPECT_EQ(FileSize(test_path), expected_size);
}
