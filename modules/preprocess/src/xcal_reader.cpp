/**
 * @file xcal_reader.cpp
 * @brief XCal v1 file reader implementation (T-005)
 *
 * REQ-P1A-014~016, REQ-P1A-018, REQ-P1A-030
 *
 * Supports automatic RLE decompression for DEFECT payloads when
 * compression metadata is present in config_json.
 *
 * @MX:ANCHOR: read_xcal_file() is the core reader used by xpe_calib_load_offset,
 *            xpe_calib_load_gain, and xpe_calib_load_defect_map (3+ callers).
 * @MX:REASON: Invariant contract -- all calibration loading functions depend on correct
 *             file parsing and validation. File format evolution must maintain backward compat.
 */

#include "xcal_reader.hpp"
#include "xcal_validator.hpp"
#include "xpe_sha256.hpp"
#include "rle_codec.hpp"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <algorithm>
#include <fstream>
#include <cstring>
#include <chrono>
#include <cstdint>
#include <cstdlib>

// @MX:NOTE: [AUTO] Clock source for expiry: std::chrono::system_clock.
// Resolution is milliseconds (epoch_ms). Non-monotonic but
// consistent with XCal v1 created_epoch_ms semantics.

// Internal helper: read the compression metadata of an XCal config block (QA-A-209b, Codex #49).
//
// The block is ONE valid JSON object (xpe_config_parse_block); the metadata is the pair of TOP-LEVEL keys
// "xcal_compression" and "xcal_raw_payload_len", each a bare non-negative integer. A key inside a nested object is
// not metadata, a key given twice (any key) refuses the block, and half a pair is a refusal: the writer always makes
// both, so one alone is a damaged or hand-made block, not "uncompressed". A block of length 0 is a file without
// metadata. `*is_compressed` is false when neither key is there.
static XpeErrorCode parse_compression_meta(
    const uint8_t* config_json,
    size_t config_len,
    bool& is_compressed,
    uint32_t& out_method,
    uint64_t& out_raw_payload_len)
{
    is_compressed = false;
    XpeConfigDoc doc;
    const XpeErrorCode rc = xpe_config_parse_block(reinterpret_cast<const char*>(config_json), config_len, &doc);
    if (rc != XPE_OK) return rc;

    const XpeConfigEntry* method = doc.find("xcal_compression");
    const XpeConfigEntry* raw = doc.find("xcal_raw_payload_len");
    if (method == nullptr && raw == nullptr) return XPE_OK;
    if (method == nullptr || raw == nullptr) return XPE_ERR_CONFIG_INVALID;

    // a bare run of decimal digits that fits: not a string, not a sign, not a fraction or exponent
    auto unsignedInteger = [](const XpeConfigEntry* e, unsigned long long limit, unsigned long long* v) {
        if (!e->scalar || e->quoted || e->text.empty() || e->text.size() > 20) return false;
        unsigned long long n = 0;
        for (const char ch : e->text) {
            if (ch < '0' || ch > '9') return false;
            const unsigned d = static_cast<unsigned>(ch - '0');
            if (n > (limit - d) / 10) return false;
            n = n * 10 + d;
        }
        *v = n;
        return true;
    };
    unsigned long long m = 0, r = 0;
    if (!unsignedInteger(method, 0xFFFFFFFFull, &m)) return XPE_ERR_CONFIG_INVALID;
    if (!unsignedInteger(raw, 0xFFFFFFFFFFFFFFFFull, &r)) return XPE_ERR_CONFIG_INVALID;
    out_method = static_cast<uint32_t>(m);
    out_raw_payload_len = static_cast<uint64_t>(r);
    is_compressed = true;
    return XPE_OK;
}

// Payload read granularity. 1 MiB keeps the file buffer and the hash input in
// cache while still amortising the read calls (QA-A-105).
static const size_t kReadChunkBytes = 1u << 20;

XpeErrorCode read_xcal_file(
    const char*            path,
    XCalFileHeader&        out_header,
    std::vector<uint8_t>&  out_config,
    std::vector<uint8_t>&  out_payload,
    bool                   check_expiry,
    int                    expected_type)
{
    try {
        if (path == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }

        // Open file
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) {
            return XPE_ERR_IO_FAILED;
        }

        // Read header (152 bytes, pack=1)
        XCalFileHeader hdr;
        f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
        if (!f.good() || f.gcount() != static_cast<std::streamsize>(sizeof(hdr))) {
            return XPE_ERR_IO_FAILED;
        }

        // Check for compressed payload before strict validation
        // Compressed payload_len will NOT match width*height*bpp
        bool is_compressed = false;
        uint32_t compression_method = XCAL_COMPRESSION_NONE;
        uint64_t raw_payload_len = 0;

        // Read config_json first (needed for compression metadata)
        std::vector<uint8_t> config;
        if (hdr.config_json_len > 0) {
            config.resize(static_cast<size_t>(hdr.config_json_len));
            f.read(reinterpret_cast<char*>(config.data()),
                   static_cast<std::streamsize>(hdr.config_json_len));
            if (!f.good() ||
                f.gcount() != static_cast<std::streamsize>(hdr.config_json_len)) {
                return XPE_ERR_IO_FAILED;
            }
        }

        // Check compression metadata
        {
            const XpeErrorCode mrc = parse_compression_meta(
                config.empty() ? nullptr : config.data(),
                config.size(),
                is_compressed,
                compression_method,
                raw_payload_len);
            if (mrc != XPE_OK) return mrc;
        }

        // Validate header (skip payload_len check for compressed data)
        // For compressed files, we need relaxed validation
        if (is_compressed) {
            // Manual validation without payload_len check
            if (std::memcmp(hdr.magic, XCAL_MAGIC, 4) != 0) {
                return XPE_ERR_CONFIG_INVALID;
            }
            if (hdr.version != XCAL_VERSION) {
                return XPE_ERR_CONFIG_INVALID;
            }
            // QA-A-37 (#140): same ceiling correction as validate_xcal_header
            // Check 3 -- the compressed path had its own copy of it.
            if (hdr.type > static_cast<uint32_t>(XCAL_TYPE_NONLIN_LUT)) {
                return XPE_ERR_CONFIG_INVALID;
            }
            if (hdr.pixel_format > static_cast<uint32_t>(XCAL_FMT_UINT8_MASK)) {
                return XPE_ERR_CONFIG_INVALID;
            }
            if (hdr.width == 0 || hdr.width > XCAL_MAX_DIM) {
                return XPE_ERR_CONFIG_INVALID;
            }
            if (hdr.height == 0 || hdr.height > XCAL_MAX_DIM) {
                return XPE_ERR_CONFIG_INVALID;
            }
            if (hdr.config_json_len > XCAL_MAX_CONFIG_JSON_LEN) {
                return XPE_ERR_CONFIG_INVALID;
            }
            if (expected_type >= 0 && hdr.type != static_cast<uint32_t>(expected_type)) {
                return XPE_ERR_CONFIG_INVALID;
            }
            // Verify raw_payload_len matches expected uncompressed size
            size_t bpp = xcal_bytes_per_pixel(hdr.pixel_format);
            if (bpp > 0) {
                uint64_t expected_raw = static_cast<uint64_t>(hdr.width) *
                                        static_cast<uint64_t>(hdr.height) *
                                        static_cast<uint64_t>(bpp);
                if (raw_payload_len != expected_raw) {
                    return XPE_ERR_CONFIG_INVALID;
                }
            }
        } else {
            // Standard validation (includes payload_len check)
            XpeErrorCode vrc = validate_xcal_header(hdr, expected_type);
            if (vrc != XPE_OK) {
                return vrc;
            }
        }

        // Read the payload and hash it as it arrives (QA-A-105, #179):
        // SRS-CALIB-PERF-003 asks for the integrity check to be "calculated
        // during read, not post-hoc". The digest is unchanged -- SHA-256 of
        // (config_json || payload) -- because the chunks are fed in file order.
        std::vector<uint8_t> payload;
        Sha256Stream hasher;
        if (!config.empty()) hasher.update(config.data(), config.size());
        if (hdr.payload_len > 0) {
            payload.resize(static_cast<size_t>(hdr.payload_len));
            size_t done = 0;
            while (done < payload.size()) {
                const size_t chunk = std::min(kReadChunkBytes, payload.size() - done);
                f.read(reinterpret_cast<char*>(payload.data() + done),
                       static_cast<std::streamsize>(chunk));
                if (!f.good() || f.gcount() != static_cast<std::streamsize>(chunk)) {
                    return XPE_ERR_IO_FAILED;
                }
                hasher.update(payload.data() + done, chunk);
                done += chunk;
            }
        }

        if (std::memcmp(hasher.digest().data(), hdr.sha256, 32) != 0) {
            return XPE_ERR_CONFIG_INVALID;
        }

        // Decompress if needed
        if (is_compressed && compression_method == XCAL_COMPRESSION_RLE) {
            std::vector<uint8_t> decompressed;
            int rc = rle_decode(payload.data(), payload.size(),
                                static_cast<size_t>(raw_payload_len),
                                decompressed);
            if (rc != XPE_OK) {
                return XPE_ERR_CONFIG_INVALID;
            }
            payload = std::move(decompressed);
        }

        // Check expiry (if requested and expiry is set)
        if (check_expiry && hdr.expiry_epoch_ms != 0) {
            using namespace std::chrono;
            int64_t now_ms = duration_cast<milliseconds>(
                system_clock::now().time_since_epoch()).count();
            if (now_ms > hdr.expiry_epoch_ms) {
                return XPE_ERR_CALIBRATION_EXPIRED;
            }
        }

        // Success: commit outputs
        // For compressed files, update payload_len to reflect decompressed size
        out_header  = hdr;
        out_header.payload_len = payload.size();
        out_config  = std::move(config);
        out_payload = std::move(payload);
        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
