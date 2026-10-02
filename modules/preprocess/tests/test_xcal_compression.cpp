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
#include <string>
#include <vector>
#include <random>
#include <cstdio>
#include <cstdint>

#include "xpe/preprocess/xcal_format.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess_api.h"
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

/* =============================================================================
 * QA-A-209b (Codex #49): the compression metadata is read from the TOP LEVEL of one valid JSON object.
 *
 * read_xcal_file found "xcal_compression": and "xcal_raw_payload_len": as the first occurrence of those strings
 * anywhere in the config block, before the hash was checked and before anything was decompressed -- in a block that
 * every loader (offset, gain, defect, nonlinearity table) reads. A key inside a nested object decided whether the
 * payload was decompressed; a key given twice was whichever came first.
 * ============================================================================= */

class XCalCompressionMetaTest : public ::testing::Test {
protected:
    const char* path = "xcal_comp_meta_test.xcal";
    void TearDown() override {
        xpe_clear_alerts();   // a read of an old-writer file raises a warning; drain what the test raised (QA-A-113 hygiene)
        std::remove(path);
        std::remove((std::string(path) + ".tmp").c_str());
    }

    /** The RLE payload the writer makes for an all-zero map of the given size. */
    std::vector<uint8_t> rlePayloadOfZeros(uint32_t w, uint32_t h) {
        auto defect = MakeDefectMap(w, h, 0.0f);
        XCalFileHeader hdr = MakeDefectHeader(w, h);
        EXPECT_EQ(XPE_OK, write_xcal_file_ex(path, hdr, nullptr, 0, defect.data(), defect.size(), true));
        XCalFileHeader rh{};
        std::vector<uint8_t> cfg, payload;
        EXPECT_EQ(XPE_OK, read_xcal_file(path, rh, cfg, payload, false, XCAL_TYPE_DEFECT));
        std::ifstream f(path, std::ios::binary);
        f.seekg(static_cast<std::streamoff>(sizeof(XCalFileHeader)) + static_cast<std::streamoff>(cfg.size()));
        std::vector<uint8_t> rle(static_cast<size_t>(FileSize(path)) - sizeof(XCalFileHeader) - cfg.size());
        f.read(reinterpret_cast<char*>(rle.data()), static_cast<std::streamsize>(rle.size()));
        return rle;
    }

    /** A DEFECT file of w x h whose config block is exactly `config` and whose payload is `rle` (the writer hashes both). */
    XpeErrorCode writeCompressedWith(const std::string& config, const std::vector<uint8_t>& rle, uint32_t w, uint32_t h) {
        XCalFileHeader hdr = MakeDefectHeader(w, h);
        return write_xcal_file_ex(path, hdr, reinterpret_cast<const uint8_t*>(config.data()), config.size(),
                                  rle.data(), rle.size(), /*compress_defect=*/false);
    }

    XpeErrorCode readDefect(std::vector<uint8_t>* payload = nullptr) {
        XCalFileHeader rh{};
        std::vector<uint8_t> cfg, pl;
        const XpeErrorCode rc = read_xcal_file(path, rh, cfg, pl, false, XCAL_TYPE_DEFECT);
        if (payload) *payload = pl;
        return rc;
    }
};

TEST_F(XCalCompressionMetaTest, AnUncompressedFileWithNestedCompressionKeysIsNotCompressed) {
    const std::string decoy = "{\"nested\":{\"xcal_compression\":1,\"xcal_raw_payload_len\":999}}";
    {   // DEFECT
        auto defect = MakeDefectMap(64, 64, 0.01f);
        XCalFileHeader hdr = MakeDefectHeader(64, 64);
        ASSERT_EQ(XPE_OK, write_xcal_file(path, hdr, reinterpret_cast<const uint8_t*>(decoy.data()), decoy.size(),
                                          defect.data(), defect.size()));
        std::vector<uint8_t> payload;
        ASSERT_EQ(XPE_OK, readDefect(&payload)) << "a key inside a nested object is not compression metadata";
        EXPECT_EQ(payload, defect);
    }
    {   // OFFSET (float32)
        std::vector<float> off(64 * 64, 12.5f);
        XCalFileHeader hdr = MakeOffsetHeader(64, 64);
        ASSERT_EQ(XPE_OK, write_xcal_file(path, hdr, reinterpret_cast<const uint8_t*>(decoy.data()), decoy.size(),
                                          reinterpret_cast<const uint8_t*>(off.data()), off.size() * sizeof(float)));
        XCalFileHeader rh{};
        std::vector<uint8_t> cfg, pl;
        ASSERT_EQ(XPE_OK, read_xcal_file(path, rh, cfg, pl, false, XCAL_TYPE_OFFSET));
        EXPECT_EQ(off.size() * sizeof(float), pl.size());
    }
}

TEST_F(XCalCompressionMetaTest, TheLoadersAreNotFooledEither) {
    // the public loaders of an offset map and a defect map go through the same reader
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    const std::string decoy = "{\"nested\":{\"xcal_compression\":1,\"xcal_raw_payload_len\":999}}";
    {
        std::vector<float> off(64 * 64, 12.5f);
        XCalFileHeader hdr = MakeOffsetHeader(64, 64);
        ASSERT_EQ(XPE_OK, write_xcal_file(path, hdr, reinterpret_cast<const uint8_t*>(decoy.data()), decoy.size(),
                                          reinterpret_cast<const uint8_t*>(off.data()), off.size() * sizeof(float)));
        EXPECT_EQ(XPE_OK, xpe_calib_load_offset(path));
    }
    {
        auto defect = MakeDefectMap(64, 64, 0.01f);
        XCalFileHeader hdr = MakeDefectHeader(64, 64);
        ASSERT_EQ(XPE_OK, write_xcal_file(path, hdr, reinterpret_cast<const uint8_t*>(decoy.data()), decoy.size(),
                                          defect.data(), defect.size()));
        EXPECT_EQ(XPE_OK, xpe_calib_load_defect_map(path));
    }
    xpe_preprocess_shutdown();
}

TEST_F(XCalCompressionMetaTest, ACompressedFileIsReadFromItsTopLevelKeysNotFromANestedDecoyBeforeThem) {
    const auto rle = rlePayloadOfZeros(64, 64);
    const std::string cfg =
        "{\"nested\":{\"xcal_compression\":1,\"xcal_raw_payload_len\":999},\"xcal_compression\":1,\"xcal_raw_payload_len\":4096}";
    ASSERT_EQ(XPE_OK, writeCompressedWith(cfg, rle, 64, 64));
    std::vector<uint8_t> payload;
    ASSERT_EQ(XPE_OK, readDefect(&payload)) << "the decoy's 999 was read instead of the top-level 4096";
    EXPECT_EQ(std::vector<uint8_t>(64 * 64, 0), payload);
}

TEST_F(XCalCompressionMetaTest, ACompressionKeyGivenTwiceAtTheTopLevelIsRefused) {
    const auto rle = rlePayloadOfZeros(64, 64);
    for (const char* cfg : {
             "{\"xcal_compression\":1,\"xcal_compression\":1,\"xcal_raw_payload_len\":4096}",
             "{\"xcal_compression\":1,\"xcal_raw_payload_len\":4096,\"xcal_raw_payload_len\":4096}",
             "{\"xcal_compression\":1,\"xcal_raw_payload_len\":4096,\"future\":1,\"future\":2}"}) {
        SCOPED_TRACE(cfg);
        ASSERT_EQ(XPE_OK, writeCompressedWith(cfg, rle, 64, 64));
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, readDefect());
    }
}

TEST_F(XCalCompressionMetaTest, BrokenOrIncompleteCompressionMetadataIsRefused) {
    const auto rle = rlePayloadOfZeros(64, 64);
    for (const char* cfg : {
             "{\"xcal_compression\":1,\"xcal_raw_payload_len\":4096",         // not closed
             "[{\"xcal_compression\":1,\"xcal_raw_payload_len\":4096}]",      // not an object
             "{\"xcal_compression\":1,\"xcal_raw_payload_len\":4096} x",      // text after the object
             "{\"xcal_compression\":1}",                                      // one key of the pair
             "{\"xcal_raw_payload_len\":4096}",                               // the other one
             "{\"xcal_compression\":\"rle\",\"xcal_raw_payload_len\":4096}",  // not an integer
             "{\"xcal_compression\":1,\"xcal_raw_payload_len\":-4096}",       // not unsigned
             "{\"xcal_compression\":1.5,\"xcal_raw_payload_len\":4096}",      // not an integer
             "   "}) {                                                         // a block that is only white space
        SCOPED_TRACE(cfg);
        ASSERT_EQ(XPE_OK, writeCompressedWith(cfg, rle, 64, 64));
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, readDefect());
    }
}

// Half of the pair beside a payload of the RAW size: if the lone key were read as "uncompressed" the file would load
// (the sizes agree), so only the pair rule refuses it -- the RLE-payload case above is also caught by the size check.
TEST_F(XCalCompressionMetaTest, HalfAPairIsRefusedEvenWhenTheRestOfTheFileIsValid) {
    auto defect = MakeDefectMap(64, 64, 0.01f);
    XCalFileHeader hdr = MakeDefectHeader(64, 64);
    for (const char* cfg : {"{\"xcal_compression\":1}", "{\"xcal_raw_payload_len\":4096}", "{\"xcal_compression\":1,\"a\":{\"xcal_raw_payload_len\":4096}}"}) {
        SCOPED_TRACE(cfg);
        ASSERT_EQ(XPE_OK, write_xcal_file(path, hdr, reinterpret_cast<const uint8_t*>(cfg), std::strlen(cfg),
                                          defect.data(), defect.size()));
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, readDefect());
    }
}

// The writer joined the caller's object and its own two keys by cutting the caller's last '}' and appending ",<keys>}":
// for a caller object with no members that is "{,...}", which is not JSON. The reader that checks the block as JSON would
// refuse a file the writer just made.
TEST_F(XCalCompressionMetaTest, TheWriterMakesValidJsonForAnEmptyCallerObjectToo) {
    auto defect = MakeDefectMap(64, 64, 0.01f);
    XCalFileHeader hdr = MakeDefectHeader(64, 64);
    for (const char* caller : {"{}", "{ }", "{\n}", "{\"mode\":\"production\"}", "{\"a\":{\"b\":1}}\n"}) {
        SCOPED_TRACE(caller);
        ASSERT_EQ(XPE_OK, write_xcal_file_ex(path, hdr, reinterpret_cast<const uint8_t*>(caller), std::strlen(caller),
                                             defect.data(), defect.size(), /*compress_defect=*/true));
        std::vector<uint8_t> payload;
        ASSERT_EQ(XPE_OK, readDefect(&payload));
        EXPECT_EQ(defect, payload);
    }
}

// ---------------------------------------------------------------------------------------------------------------
// QA-A-209c (Codex #53): files the writer of 90c1b6b1 and before made. For a compressed DEFECT file with a caller
// config (the only case it merged: compress_defect, the type DEFECT, RLE smaller than the raw payload) it cut the
// caller's last '}' and appended ",<meta>}" where <meta> still carried its own '}' -- so the block ended "}}" -- and
// for a caller object with no members it made "{,<meta>}}". The string-searching reader of that time accepted both.
// The strict reader refused them before the hash was checked, which locked a site out of a calibration it had
// stored. The reader now repairs exactly those two shapes -- strict parsing failed, the block is one of the two, and
// the repaired text passes the same strict parse -- and says so in an alert. The hash covers the STORED bytes
// (config || payload), so the repair loosens no integrity check.
// ---------------------------------------------------------------------------------------------------------------

namespace legacy209c {

// The old writer's merge, reproduced from 90c1b6b1's xcal_writer.cpp build_config_json.
std::string oldWriterConfig(const std::string& caller, unsigned method, unsigned long long rawLen) {
    char meta[128];
    std::snprintf(meta, sizeof(meta), "{\"xcal_compression\":%u,\"xcal_raw_payload_len\":%llu}", method, rawLen);
    std::string callerStr = caller;
    const size_t lastBrace = callerStr.rfind('}');
    if (lastBrace != std::string::npos) callerStr = callerStr.substr(0, lastBrace);
    std::string metaStr(meta);
    const size_t bracePos = metaStr.find('{');
    if (bracePos != std::string::npos) metaStr = metaStr.substr(bracePos + 1);
    return callerStr + "," + metaStr + "}";
}

bool alertContains(const char* needle) {
    char msg[1024];
    int32_t sev = -1;
    const int32_t count = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < count; ++i) {
        if (xpe_get_pending_alert(i, msg, sizeof(msg), &sev) != XPE_OK) continue;
        if (std::string(msg).find(needle) != std::string::npos) return true;
    }
    return false;
}

}  // namespace legacy209c

TEST_F(XCalCompressionMetaTest, TheOldWritersTwoShapesAreReproducedExactlyByTheTestHelper) {
    // control for the helper: it makes the shapes the card describes
    EXPECT_EQ("{\"mode\":\"production\",\"xcal_compression\":1,\"xcal_raw_payload_len\":4096}}",
              legacy209c::oldWriterConfig("{\"mode\":\"production\"}", 1, 4096));
    EXPECT_EQ("{,\"xcal_compression\":1,\"xcal_raw_payload_len\":4096}}", legacy209c::oldWriterConfig("{}", 1, 4096));
    EXPECT_EQ("{ ,\"xcal_compression\":1,\"xcal_raw_payload_len\":4096}}", legacy209c::oldWriterConfig("{ }", 1, 4096));
}

TEST_F(XCalCompressionMetaTest, AFileInTheOldWritersShapeLoadsWithTheSameMapAndAWarning) {
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    const auto rle = rlePayloadOfZeros(64, 64);
    for (const char* caller : {"{\"mode\":\"production\"}", "{}", "{ }", "{\"a\":{\"b\":1},\"c\":[1,2]}", "{\n}"}) {
        SCOPED_TRACE(caller);
        const std::string legacy = legacy209c::oldWriterConfig(caller, 1, 4096);
        ASSERT_EQ(XPE_OK, writeCompressedWith(legacy, rle, 64, 64));
        xpe_clear_alerts();
        std::vector<uint8_t> payload;
        ASSERT_EQ(XPE_OK, readDefect(&payload)) << "a file the old writer made must still load";
        EXPECT_EQ(std::vector<uint8_t>(64 * 64, 0), payload);
        EXPECT_TRUE(legacy209c::alertContains("XPE_WARN_XCAL_LEGACY_CONFIG")) << "the repair must be reported";
        xpe_clear_alerts();
        EXPECT_EQ(XPE_OK, xpe_calib_load_defect_map(path)) << "and so must the public loader";
        EXPECT_TRUE(legacy209c::alertContains("regenerate"));
    }
    // a file the CURRENT writer made: the same map, no warning
    auto defect = MakeDefectMap(64, 64, 0.0f);
    XCalFileHeader hdr = MakeDefectHeader(64, 64);
    const char* caller = "{\"mode\":\"production\"}";
    ASSERT_EQ(XPE_OK, write_xcal_file_ex(path, hdr, reinterpret_cast<const uint8_t*>(caller), std::strlen(caller),
                                         defect.data(), defect.size(), true));
    xpe_clear_alerts();
    std::vector<uint8_t> payload;
    ASSERT_EQ(XPE_OK, readDefect(&payload));
    EXPECT_EQ(defect, payload);
    EXPECT_FALSE(legacy209c::alertContains("XPE_WARN_XCAL_LEGACY_CONFIG")) << "a current file is not reported";
    xpe_preprocess_shutdown();
}

TEST_F(XCalCompressionMetaTest, ABlockThatOnlyLooksLikeTheOldWritersShapeIsStillRefused) {
    const auto rle = rlePayloadOfZeros(64, 64);
    const std::string tail = "\"xcal_compression\":1,\"xcal_raw_payload_len\":4096";
    const std::vector<std::string> blocks = {
        "{\"mode\":\"p\"," + tail + "}}}",                         // three closing braces
        "{\"mode\":\"p\"," + tail + "}} ",                         // text after the braces
        "{,\"mode\":\"p\"," + tail + "}}",                         // a member between "{," and the pair
        "{,," + tail + "}}",                                       // two commas
        "{\"mode\":\"p\"," + tail + ",\"mode\":\"q\"}}",           // a repeated key; the pair is not last
        "{\"mode\":\"p\"," + tail + ",\"x\":1}}",                  // the pair is not the last two members
        "{\"n\":{" + tail + "}}",                                  // repaired, the pair would be NESTED
        "{,\"n\":{" + tail + "}}",                                 // the same after the comma repair
        "{\"mode\":\"p\",\"xcal_compression\":1,\"xcal_compression\":1,\"xcal_raw_payload_len\":4096}}",  // pair repeated
        "{\"mode\":\"p\",\"xcal_compression\":1,\"xcal_raw_payload_len\":-4096}}",                        // not unsigned
        "{\"mode\":\"p\",\"xcal_compression\":\"1\",\"xcal_raw_payload_len\":4096}}",                     // a string
        "{\"mode\":\"p\",\"xcal_raw_payload_len\":4096,\"xcal_compression\":1}}",                         // order swapped
        "{\"mode\":\"p\"}}",                                       // no pair at all
        "{\"mode\":\"p\"," + tail + "}}}}",                        // four braces
        "{\"mode\":\"p\" " + tail + "}}",                          // a missing comma
        "{ \t,\n" + tail + "}}"};                                  // white space AFTER the comma: the old writer cannot make it
    for (const std::string& cfg : blocks) {
        SCOPED_TRACE(cfg);
        ASSERT_EQ(XPE_OK, writeCompressedWith(cfg, rle, 64, 64));
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, readDefect());
    }
}

TEST_F(XCalCompressionMetaTest, TheRepairLoosensNoIntegrityCheck) {
    ASSERT_EQ(XPE_OK, xpe_preprocess_init(nullptr));
    const auto rle = rlePayloadOfZeros(64, 64);
    const std::string legacy = legacy209c::oldWriterConfig("{\"mode\":\"production\"}", 1, 4096);
    ASSERT_EQ(XPE_OK, writeCompressedWith(legacy, rle, 64, 64));
    ASSERT_EQ(XPE_OK, readDefect());
    // flip one byte of the stored config that keeps its shape: the hash over the stored bytes no longer matches
    {
        std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
        const std::streamoff at = static_cast<std::streamoff>(sizeof(XCalFileHeader)) + 10;   // inside "production"
        f.seekg(at);
        char ch = 0;
        f.read(&ch, 1);
        ch = (ch == 'x') ? 'y' : 'x';
        f.seekp(at);
        f.write(&ch, 1);
    }
    xpe_clear_alerts();
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, readDefect());
    EXPECT_FALSE(legacy209c::alertContains("XPE_WARN_XCAL_LEGACY_CONFIG")) << "a refused file is not reported as repaired";
    // and a flip of the payload is caught as before
    ASSERT_EQ(XPE_OK, writeCompressedWith(legacy, rle, 64, 64));
    {
        std::fstream f(path, std::ios::in | std::ios::out | std::ios::binary);
        f.seekp(-1, std::ios::end);
        const char ch = 0x5A;
        f.write(&ch, 1);
    }
    EXPECT_EQ(XPE_ERR_CONFIG_INVALID, readDefect());
    xpe_preprocess_shutdown();
}

// ---------------------------------------------------------------------------------------------------------------
// QA-A-209d (Codex #56 item 2): the repair is for what the old writer could make, and no wider. It made the doubled
// brace only for a compressed (RLE, method 1) DEFECT file with a caller config that had members (shape A: members,
// then the pair) or was an empty object (shape B). A block that is only the pair plus a second brace, the same tail on
// a file of another type, or a method the old writer never wrote is none of those.
// ---------------------------------------------------------------------------------------------------------------

TEST_F(XCalCompressionMetaTest, ThePairAloneWithADoubledBraceIsNotWhatTheOldWriterMade) {
    // the old writer, given NO caller config, wrote the pair as a normal single-brace object; it never wrote this
    const auto rle = rlePayloadOfZeros(64, 64);
    for (const char* cfg : {"{\"xcal_compression\":1,\"xcal_raw_payload_len\":4096}}",
                            "{ \"xcal_compression\":1,\"xcal_raw_payload_len\":4096}}",
                            "{\n\"xcal_compression\":1,\"xcal_raw_payload_len\":4096}}"}) {
        SCOPED_TRACE(cfg);
        ASSERT_EQ(XPE_OK, writeCompressedWith(cfg, rle, 64, 64));
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, readDefect());
    }
}

TEST_F(XCalCompressionMetaTest, TheOldWritersShapesAreOnlyRepairedForAnRleCompressedDefectFile) {
    const auto rle = rlePayloadOfZeros(64, 64);            // 4096 raw bytes, RLE
    // a file of ANOTHER type with the old shapes and a payload that WOULD decode if it were repaired and read as compressed:
    // a 32x32 float32 OFFSET map is 4096 raw bytes
    XCalFileHeader off = MakeOffsetHeader(32, 32);
    for (const char* caller : {"{\"mode\":\"production\"}", "{}"}) {
        SCOPED_TRACE(caller);
        const std::string legacy = legacy209c::oldWriterConfig(caller, 1, 4096);
        ASSERT_EQ(XPE_OK, write_xcal_file_ex(path, off, reinterpret_cast<const uint8_t*>(legacy.data()), legacy.size(),
                                             rle.data(), rle.size(), /*compress_defect=*/false));
        XCalFileHeader rh{};
        std::vector<uint8_t> cfg, pl;
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, read_xcal_file(path, rh, cfg, pl, false, XCAL_TYPE_OFFSET))
            << "the old writer compressed only DEFECT maps; the same tail on an OFFSET file is not its work";
    }
    // a method the old writer never wrote (it wrote 1, RLE)
    for (const char* caller : {"{\"mode\":\"production\"}", "{}"}) {
        SCOPED_TRACE(caller);
        const std::string legacy = legacy209c::oldWriterConfig(caller, 2, 4096);
        ASSERT_EQ(XPE_OK, writeCompressedWith(legacy, rle, 64, 64));
        EXPECT_EQ(XPE_ERR_CONFIG_INVALID, readDefect());
    }
    // control: method 1 on the DEFECT file is the accepted case
    const std::string good = legacy209c::oldWriterConfig("{\"mode\":\"production\"}", 1, 4096);
    ASSERT_EQ(XPE_OK, writeCompressedWith(good, rle, 64, 64));
    EXPECT_EQ(XPE_OK, readDefect());
}
