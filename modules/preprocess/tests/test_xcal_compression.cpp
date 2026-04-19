/**
 * @file test_xcal_compression.cpp
 * @brief Unit tests for XCal v1 RLE compression and decompression
 *
 * SPEC-XPE-P1A SUP-01 -- DEFECT map compression
 *
 * Test cases:
 *  1.  RLE encode/decode round-trip: all-zero defect map
 *  2.  RLE encode/decode round-trip: sparse defect map (random defects)
 *  3.  RLE compression ratio: all-zero 3072x3072 -> < 1 KB
 *  4.  RLE decode with wrong expected_len -> CONFIG_INVALID
 *  5.  RLE decoded_size matches actual decoded size
 *  6.  RLE encode with nullptr data -> INVALID_INPUT
 *  7.  RLE decode with truncated data (len % 5 != 0) -> CONFIG_INVALID
 *  8.  Write compressed defect file, read back, data bit-identical
 *  9.  Compressed file is smaller than uncompressed
 *  10. Write compressed with caller config_json, metadata merged correctly
 *  11. Read compressed defect with expected_type mismatch -> CONFIG_INVALID
 *  12. Backward compat: uncompressed defect file still reads correctly
 *  13. Compress OFFSET type: payload NOT compressed (compress only for DEFECT)
 *  14. Worst-case input (alternating values) -> compressed >= uncompressed, fallback
 */

#include <gtest/gtest.h>
#include <cstring>
#include <fstream>
#include <vector>
#include <random>
#include <cstdio>
#include <cstdint>

#include "xpe/preprocess/xcal_format.h"
#include "xpe/common/xpe_error.h"
#include "rle_codec.hpp"
#include "xcal_writer.hpp"
#include "xcal_reader.hpp"

namespace {

constexpr uint32_t W = 512;
constexpr uint32_t H = 512;

uint64_t FileSize(const char* path) {
    std::ifstream f(path, std::ios::binary | std::ios::ate);
    if (!f) return 0;
    return static_cast<uint64_t>(f.tellg());
}

// Create a defect map with given defect ratio (0.0 = all good, 1.0 = all bad)
std::vector<uint8_t> MakeDefectMap(uint32_t w, uint32_t h, float defect_ratio = 0.0f) {
    std::vector<uint8_t> map(static_cast<size_t>(w) * h, 0);
    if (defect_ratio <= 0.0f) return map;

    std::mt19937 rng(42);  // Fixed seed for reproducibility
    std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    for (size_t i = 0; i < map.size(); ++i) {
        if (dist(rng) < defect_ratio) {
            map[i] = 1;  // Defect pixel
        }
    }
    return map;
}

XCalFileHeader MakeDefectHeader(uint32_t w, uint32_t h) {
    XCalFileHeader hdr;
    std::memset(&hdr, 0, sizeof(hdr));
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version          = XCAL_VERSION;
    hdr.type             = XCAL_TYPE_DEFECT;
    hdr.pixel_format     = XCAL_FMT_UINT8_MASK;
    hdr.width            = w;
    hdr.height           = h;
    hdr.created_epoch_ms = 0LL;
    hdr.expiry_epoch_ms  = 0LL;
    hdr.config_json_len  = 0;
    hdr.payload_len      = static_cast<uint64_t>(w) * h * sizeof(uint8_t);
    return hdr;
}

XCalFileHeader MakeOffsetHeader(uint32_t w, uint32_t h) {
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

} // anonymous namespace

// Helper for nullptr tests (defined before use)
static void TestRleNullptr() {
    std::vector<uint8_t> out;
    EXPECT_EQ(rle_encode(nullptr, 100, out), XPE_ERR_INVALID_INPUT);
    EXPECT_EQ(rle_decode(nullptr, 100, 0, out), XPE_ERR_INVALID_INPUT);
    size_t sz = 0;
    EXPECT_EQ(rle_decoded_size(nullptr, 100, sz), XPE_ERR_INVALID_INPUT);
}

// =============================================================================
// RLE Codec Tests
// =============================================================================

class RleCodecTest : public ::testing::Test {
protected:
    // nothing special
};

// Test 1: Round-trip all-zero defect map
TEST_F(RleCodecTest, RoundTrip_AllZero) {
    auto data = MakeDefectMap(W, H, 0.0f);

    std::vector<uint8_t> encoded;
    ASSERT_EQ(rle_encode(data.data(), data.size(), encoded), XPE_OK);

    // All-zero 512x512 = 262144 bytes -> should compress to exactly 5 bytes (one run)
    ASSERT_EQ(encoded.size(), 5u);
    EXPECT_EQ(encoded[0], 0);  // value
    // count should be 262144 = 0x40000
    uint32_t count = static_cast<uint32_t>(encoded[1]) |
                     (static_cast<uint32_t>(encoded[2]) << 8) |
                     (static_cast<uint32_t>(encoded[3]) << 16) |
                     (static_cast<uint32_t>(encoded[4]) << 24);
    EXPECT_EQ(count, data.size());

    std::vector<uint8_t> decoded;
    ASSERT_EQ(rle_decode(encoded.data(), encoded.size(), data.size(), decoded), XPE_OK);
    ASSERT_EQ(decoded.size(), data.size());
    EXPECT_EQ(std::memcmp(decoded.data(), data.data(), data.size()), 0);
}

// Test 2: Round-trip sparse defect map
TEST_F(RleCodecTest, RoundTrip_SparseDefect) {
    auto data = MakeDefectMap(W, H, 0.001f);  // 0.1% defect rate

    std::vector<uint8_t> encoded;
    ASSERT_EQ(rle_encode(data.data(), data.size(), encoded), XPE_OK);

    // Should be significantly compressed
    EXPECT_LT(encoded.size(), data.size() / 2);

    std::vector<uint8_t> decoded;
    ASSERT_EQ(rle_decode(encoded.data(), encoded.size(), data.size(), decoded), XPE_OK);
    ASSERT_EQ(decoded.size(), data.size());
    EXPECT_EQ(std::memcmp(decoded.data(), data.data(), data.size()), 0);
}

// Test 3: Large all-zero map compression ratio
TEST_F(RleCodecTest, CompressionRatio_LargeAllZero) {
    constexpr uint32_t LW = 3072, LH = 3072;
    auto data = MakeDefectMap(LW, LH, 0.0f);

    std::vector<uint8_t> encoded;
    ASSERT_EQ(rle_encode(data.data(), data.size(), encoded), XPE_OK);

    // 3072*3072 = 9,437,184 bytes -> should be 5 bytes (one run)
    // Actually UINT32_MAX = 4,294,967,295, and 9,437,184 < UINT32_MAX,
    // so one run of 5 bytes.
    EXPECT_EQ(encoded.size(), 5u);

    // Verify decoded_size helper
    size_t dec_size = 0;
    ASSERT_EQ(rle_decoded_size(encoded.data(), encoded.size(), dec_size), XPE_OK);
    EXPECT_EQ(dec_size, data.size());
}

// Test 4: Wrong expected_len -> CONFIG_INVALID
TEST_F(RleCodecTest, Decode_WrongExpectedLen_ConfigInvalid) {
    auto data = MakeDefectMap(64, 64, 0.0f);

    std::vector<uint8_t> encoded;
    ASSERT_EQ(rle_encode(data.data(), data.size(), encoded), XPE_OK);

    // Decode with wrong expected_len
    std::vector<uint8_t> decoded;
    EXPECT_EQ(rle_decode(encoded.data(), encoded.size(),
                         data.size() + 100, decoded),
              XPE_ERR_CONFIG_INVALID);
}

// Test 5: decoded_size matches actual decoded size
TEST_F(RleCodecTest, DecodedSize_MatchesActual) {
    auto data = MakeDefectMap(128, 128, 0.01f);

    std::vector<uint8_t> encoded;
    ASSERT_EQ(rle_encode(data.data(), data.size(), encoded), XPE_OK);

    size_t reported_size = 0;
    ASSERT_EQ(rle_decoded_size(encoded.data(), encoded.size(), reported_size), XPE_OK);

    std::vector<uint8_t> decoded;
    ASSERT_EQ(rle_decode(encoded.data(), encoded.size(), 0, decoded), XPE_OK);

    EXPECT_EQ(reported_size, decoded.size());
    EXPECT_EQ(reported_size, data.size());
}

// Test 6: nullptr data with nonzero len -> INVALID_INPUT
TEST_F(RleCodecTest, NullData_InvalidInput) {
    TestRleNullptr();
}

// Test 7: Truncated encoded data (len % 5 != 0) -> CONFIG_INVALID
TEST_F(RleCodecTest, TruncatedEncoded_ConfigInvalid) {
    uint8_t bad_data[] = {0x00, 0x01, 0x02};  // 3 bytes, not multiple of 5
    std::vector<uint8_t> decoded;
    EXPECT_EQ(rle_decode(bad_data, 3, 0, decoded), XPE_ERR_CONFIG_INVALID);

    size_t sz = 0;
    EXPECT_EQ(rle_decoded_size(bad_data, 3, sz), XPE_ERR_CONFIG_INVALID);
}

// =============================================================================
// XCal Write/Read Compression Integration Tests
// =============================================================================

class XCalCompressionTest : public ::testing::Test {
protected:
    const char* compressed_path = "xcal_comp_test_defect.xcal";
    const char* uncompressed_path = "xcal_comp_test_uncomp.xcal";
    const char* offset_path = "xcal_comp_test_offset.xcal";

    void TearDown() override {
        std::remove(compressed_path);
        std::remove(uncompressed_path);
        std::remove(offset_path);
        std::remove((std::string(compressed_path) + ".tmp").c_str());
        std::remove((std::string(uncompressed_path) + ".tmp").c_str());
        std::remove((std::string(offset_path) + ".tmp").c_str());
    }
};

// Test 8: Write compressed defect, read back, data bit-identical
TEST_F(XCalCompressionTest, WriteCompressed_ReadBack_BitIdentical) {
    auto defect = MakeDefectMap(W, H, 0.005f);
    XCalFileHeader hdr = MakeDefectHeader(W, H);

    // Write with compression
    XpeErrorCode rc = write_xcal_file_ex(
        compressed_path, hdr,
        nullptr, 0,
        defect.data(), defect.size(),
        /*compress_defect=*/true);
    ASSERT_EQ(rc, XPE_OK);

    // Read back
    XCalFileHeader read_hdr;
    std::vector<uint8_t> config, payload;
    rc = read_xcal_file(compressed_path, read_hdr, config, payload,
                        /*check_expiry=*/false, XCAL_TYPE_DEFECT);
    ASSERT_EQ(rc, XPE_OK);

    // Verify dimensions
    EXPECT_EQ(read_hdr.width, W);
    EXPECT_EQ(read_hdr.height, H);
    EXPECT_EQ(read_hdr.type, static_cast<uint32_t>(XCAL_TYPE_DEFECT));

    // Verify payload is decompressed to original size
    ASSERT_EQ(payload.size(), defect.size());
    EXPECT_EQ(std::memcmp(payload.data(), defect.data(), defect.size()), 0);
}

// Test 9: Compressed file is smaller than uncompressed
TEST_F(XCalCompressionTest, CompressedFile_SmallerThanUncompressed) {
    auto defect = MakeDefectMap(W, H, 0.0f);  // All zeros
    XCalFileHeader hdr = MakeDefectHeader(W, H);

    // Write uncompressed
    ASSERT_EQ(write_xcal_file_ex(
        uncompressed_path, hdr,
        nullptr, 0,
        defect.data(), defect.size(),
        /*compress_defect=*/false), XPE_OK);

    // Write compressed
    ASSERT_EQ(write_xcal_file_ex(
        compressed_path, hdr,
        nullptr, 0,
        defect.data(), defect.size(),
        /*compress_defect=*/true), XPE_OK);

    uint64_t uncomp_size = FileSize(uncompressed_path);
    uint64_t comp_size   = FileSize(compressed_path);

    // Compressed should be significantly smaller
    // All-zero 512x512 = 262144 bytes payload -> 5 bytes RLE
    // Compressed file: 152 header + config_json(~50 bytes) + 5 bytes payload
    EXPECT_LT(comp_size, uncomp_size);
    // The payload alone should be < 1% of original
    // Compressed file has config_json overhead, so total file may be ~200 bytes vs ~262KB
    EXPECT_LT(comp_size, uncomp_size / 10);
}

// Test 10: Write compressed with caller config_json, metadata merged
TEST_F(XCalCompressionTest, CompressedWithCallerConfig_MetadataMerged) {
    auto defect = MakeDefectMap(64, 64, 0.0f);
    XCalFileHeader hdr = MakeDefectHeader(64, 64);

    const char* caller_json = "{\"mode\":\"production\"}";

    XpeErrorCode rc = write_xcal_file_ex(
        compressed_path, hdr,
        reinterpret_cast<const uint8_t*>(caller_json), std::strlen(caller_json),
        defect.data(), defect.size(),
        /*compress_defect=*/true);
    ASSERT_EQ(rc, XPE_OK);

    // Read back and check config contains both caller data and compression metadata
    XCalFileHeader read_hdr;
    std::vector<uint8_t> config, payload;
    rc = read_xcal_file(compressed_path, read_hdr, config, payload,
                        /*check_expiry=*/false);
    ASSERT_EQ(rc, XPE_OK);

    // Config should contain both "mode" and "xcal_compression"
    std::string cfg_str(reinterpret_cast<const char*>(config.data()), config.size());
    EXPECT_NE(cfg_str.find("\"mode\":\"production\""), std::string::npos);
    EXPECT_NE(cfg_str.find("\"xcal_compression\":1"), std::string::npos);
    EXPECT_NE(cfg_str.find("\"xcal_raw_payload_len\":"), std::string::npos);

    // Data should be correct
    ASSERT_EQ(payload.size(), defect.size());
    EXPECT_EQ(std::memcmp(payload.data(), defect.data(), defect.size()), 0);
}

// Test 11: Read compressed defect with expected_type mismatch -> CONFIG_INVALID
TEST_F(XCalCompressionTest, Compressed_TypeMismatch_ConfigInvalid) {
    auto defect = MakeDefectMap(64, 64, 0.0f);
    XCalFileHeader hdr = MakeDefectHeader(64, 64);

    ASSERT_EQ(write_xcal_file_ex(
        compressed_path, hdr,
        nullptr, 0,
        defect.data(), defect.size(),
        /*compress_defect=*/true), XPE_OK);

    XCalFileHeader read_hdr;
    std::vector<uint8_t> config, payload;
    XpeErrorCode rc = read_xcal_file(compressed_path, read_hdr, config, payload,
                                     /*check_expiry=*/false,
                                     /*expected_type=*/XCAL_TYPE_GAIN);  // Wrong type
    EXPECT_EQ(rc, XPE_ERR_CONFIG_INVALID);
}

// Test 12: Backward compat: uncompressed defect file still reads correctly
TEST_F(XCalCompressionTest, UncompressedDefect_StillReadsCorrectly) {
    auto defect = MakeDefectMap(128, 128, 0.01f);
    XCalFileHeader hdr = MakeDefectHeader(128, 128);

    // Write WITHOUT compression
    ASSERT_EQ(write_xcal_file(
        uncompressed_path, hdr,
        nullptr, 0,
        defect.data(), defect.size()), XPE_OK);

    // Read back (reader should handle non-compressed correctly)
    XCalFileHeader read_hdr;
    std::vector<uint8_t> config, payload;
    XpeErrorCode rc = read_xcal_file(uncompressed_path, read_hdr, config, payload,
                                     /*check_expiry=*/false, XCAL_TYPE_DEFECT);
    ASSERT_EQ(rc, XPE_OK);
    ASSERT_EQ(payload.size(), defect.size());
    EXPECT_EQ(std::memcmp(payload.data(), defect.data(), defect.size()), 0);
}

// Test 13: Compress OFFSET type -> payload NOT compressed
TEST_F(XCalCompressionTest, CompressOffsetType_PayloadNotCompressed) {
    std::vector<float> offset_data(static_cast<size_t>(W) * H, 1.0f);
    XCalFileHeader hdr = MakeOffsetHeader(W, H);

    // Try to write with compression enabled
    XpeErrorCode rc = write_xcal_file_ex(
        offset_path, hdr,
        nullptr, 0,
        reinterpret_cast<const uint8_t*>(offset_data.data()),
        offset_data.size() * sizeof(float),
        /*compress_defect=*/true);  // Should be ignored for OFFSET type
    ASSERT_EQ(rc, XPE_OK);

    // File size should be standard (no compression applied)
    uint64_t expected_size = sizeof(XCalFileHeader) +
                             static_cast<uint64_t>(W) * H * sizeof(float);
    EXPECT_EQ(FileSize(offset_path), expected_size);

    // Read back should work normally
    XCalFileHeader read_hdr;
    std::vector<uint8_t> config, payload;
    rc = read_xcal_file(offset_path, read_hdr, config, payload,
                        /*check_expiry=*/false, XCAL_TYPE_OFFSET);
    ASSERT_EQ(rc, XPE_OK);

    // Config should be empty (no compression metadata injected)
    EXPECT_TRUE(config.empty());
}

// Test 14: Worst-case input (alternating values) -> fallback to uncompressed
TEST_F(XCalCompressionTest, WorstCaseInput_FallbackToUncompressed) {
    // Create alternating 0,1,0,1,... pattern -- worst case for RLE
    // 5 bytes per run, 2 runs per 2 input bytes = 5 bytes per byte (5x expansion)
    const uint32_t SW = 256, SH = 256;
    std::vector<uint8_t> alternating(static_cast<size_t>(SW) * SH);
    for (size_t i = 0; i < alternating.size(); ++i) {
        alternating[i] = static_cast<uint8_t>(i & 1);
    }

    XCalFileHeader hdr = MakeDefectHeader(SW, SH);

    XpeErrorCode rc = write_xcal_file_ex(
        compressed_path, hdr,
        nullptr, 0,
        alternating.data(), alternating.size(),
        /*compress_defect=*/true);
    ASSERT_EQ(rc, XPE_OK);

    // Since RLE expands this data, writer should fall back to uncompressed
    // File size should be standard (header + raw payload, no config_json)
    uint64_t expected_size = sizeof(XCalFileHeader) +
                             static_cast<uint64_t>(SW) * SH;
    EXPECT_EQ(FileSize(compressed_path), expected_size);

    // Read back should work
    XCalFileHeader read_hdr;
    std::vector<uint8_t> config, payload;
    rc = read_xcal_file(compressed_path, read_hdr, config, payload,
                        /*check_expiry=*/false);
    ASSERT_EQ(rc, XPE_OK);
    ASSERT_EQ(payload.size(), alternating.size());
    EXPECT_EQ(std::memcmp(payload.data(), alternating.data(), alternating.size()), 0);
}
