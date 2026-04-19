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

#include <fstream>
#include <cstring>
#include <chrono>
#include <cstdint>
#include <cstdlib>

// @MX:NOTE: [AUTO] Clock source for expiry: std::chrono::system_clock.
// Resolution is milliseconds (epoch_ms). Non-monotonic but
// consistent with XCal v1 created_epoch_ms semantics.

// Internal helper: parse compression metadata from config_json.
// Returns true if compression metadata found and valid.
// Sets out_method and out_raw_payload_len.
static bool parse_compression_meta(
    const uint8_t* config_json,
    size_t config_len,
    uint32_t& out_method,
    uint64_t& out_raw_payload_len)
{
    if (config_json == nullptr || config_len == 0) {
        return false;
    }

    // Simple substring search for "xcal_compression" field
    // We avoid full JSON parsing to minimize dependencies.
    std::string cfg(reinterpret_cast<const char*>(config_json), config_len);

    // Find "xcal_compression":
    const char key_compression[] = "\"xcal_compression\":";
    size_t pos = cfg.find(key_compression);
    if (pos == std::string::npos) {
        return false;
    }

    // Parse the integer value after the key
    pos += sizeof(key_compression) - 1;
    // Skip whitespace
    while (pos < cfg.size() && (cfg[pos] == ' ' || cfg[pos] == '\t')) {
        ++pos;
    }
    if (pos >= cfg.size() || cfg[pos] < '0' || cfg[pos] > '9') {
        return false;
    }
    char* endp = nullptr;
    unsigned long method_val = std::strtoul(cfg.c_str() + pos, &endp, 10);
    if (endp == cfg.c_str() + pos) {
        return false;
    }
    out_method = static_cast<uint32_t>(method_val);

    // Find "xcal_raw_payload_len":
    const char key_raw_len[] = "\"xcal_raw_payload_len\":";
    pos = cfg.find(key_raw_len);
    if (pos == std::string::npos) {
        return false;
    }
    pos += sizeof(key_raw_len) - 1;
    while (pos < cfg.size() && (cfg[pos] == ' ' || cfg[pos] == '\t')) {
        ++pos;
    }
    if (pos >= cfg.size() || cfg[pos] < '0' || cfg[pos] > '9') {
        return false;
    }
    char* endp2 = nullptr;
    unsigned long long raw_len_val = std::strtoull(cfg.c_str() + pos, &endp2, 10);
    if (endp2 == cfg.c_str() + pos) {
        return false;
    }
    out_raw_payload_len = static_cast<uint64_t>(raw_len_val);

    return true;
}

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
        is_compressed = parse_compression_meta(
            config.empty() ? nullptr : config.data(),
            config.size(),
            compression_method,
            raw_payload_len);

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
            if (hdr.type > XCAL_TYPE_DEFECT) {
                return XPE_ERR_CONFIG_INVALID;
            }
            if (hdr.pixel_format > XCAL_FMT_UINT8_MASK) {
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

        // Read payload
        std::vector<uint8_t> payload;
        if (hdr.payload_len > 0) {
            payload.resize(static_cast<size_t>(hdr.payload_len));
            f.read(reinterpret_cast<char*>(payload.data()),
                   static_cast<std::streamsize>(hdr.payload_len));
            if (!f.good() ||
                f.gcount() != static_cast<std::streamsize>(hdr.payload_len)) {
                return XPE_ERR_IO_FAILED;
            }
        }

        // Verify SHA-256 (over stored config + stored payload)
        const uint8_t* cfg_ptr = config.empty()  ? nullptr : config.data();
        const uint8_t* pay_ptr = payload.empty() ? nullptr : payload.data();
        auto computed = compute_sha256_two_parts(
            cfg_ptr, config.size(),
            pay_ptr, payload.size());

        if (std::memcmp(computed.data(), hdr.sha256, 32) != 0) {
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
