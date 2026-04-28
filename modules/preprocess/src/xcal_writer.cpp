/**
 * @file xcal_writer.cpp
 * @brief XCal v1 file writer implementation (T-004)
 *
 * REQ-P1A-019, REQ-P1A-017, REQ-P1A-030
 *
 * Supports optional RLE compression for DEFECT (UINT8_MASK) payloads.
 * When compress_defect is true and type == XCAL_TYPE_DEFECT, the payload
 * is RLE-encoded and compression metadata is embedded in config_json.
 */

#include "xcal_writer.hpp"
#include "xpe_sha256.hpp"
#include "xcal_validator.hpp"
#include "rle_codec.hpp"

#include <cstring>
#include <fstream>
#include <string>
#include <cstdio>  // std::rename

// @MX:NOTE: [AUTO] Atomic write pattern: write to <path>.tmp, then rename.
// This prevents corrupt partial files visible to concurrent readers.

// Internal helper: build compression metadata JSON string.
// Returns empty string if no compression metadata is needed.
static std::string build_config_json(
    const uint8_t* caller_config,
    uint64_t caller_config_len,
    bool has_compression,
    uint32_t compression_method,
    uint64_t raw_payload_len)
{
    std::string result;

    if (has_compression) {
        // Build compression metadata JSON
        // Minimal: {"xcal_compression":1,"xcal_raw_payload_len":NNN}
        char meta[128];
        std::snprintf(meta, sizeof(meta),
            "{\"xcal_compression\":%u,\"xcal_raw_payload_len\":%llu}",
            static_cast<unsigned>(compression_method),
            static_cast<unsigned long long>(raw_payload_len));
        meta[sizeof(meta) - 1] = '\0';

        if (caller_config != nullptr && caller_config_len > 0) {
            // Merge: caller config is a JSON object; we inject our fields
            // Simple approach: strip trailing '}' from caller, prepend comma + meta
            std::string caller_str(reinterpret_cast<const char*>(caller_config),
                                   static_cast<size_t>(caller_config_len));
            // Find last '}'
            size_t last_brace = caller_str.rfind('}');
            if (last_brace != std::string::npos) {
                caller_str = caller_str.substr(0, last_brace);
            }
            // Our meta without leading '{'
            std::string meta_str(meta);
            size_t brace_pos = meta_str.find('{');
            if (brace_pos != std::string::npos) {
                meta_str = meta_str.substr(brace_pos + 1);
            }
            result = caller_str + "," + meta_str + "}";
        } else {
            result = meta;
        }
    } else {
        // No compression: pass through caller config as-is
        if (caller_config != nullptr && caller_config_len > 0) {
            result.assign(reinterpret_cast<const char*>(caller_config),
                          static_cast<size_t>(caller_config_len));
        }
    }

    return result;
}

XpeErrorCode write_xcal_file(
    const char* path,
    XCalFileHeader hdr_template,
    const uint8_t* config_json,
    uint64_t config_json_len,
    const uint8_t* payload,
    uint64_t payload_len)
{
    return write_xcal_file_ex(
        path, hdr_template,
        config_json, config_json_len,
        payload, payload_len,
        false);
}

XpeErrorCode write_xcal_file_ex(
    const char* path,
    XCalFileHeader hdr_template,
    const uint8_t* config_json,
    uint64_t config_json_len,
    const uint8_t* payload,
    uint64_t payload_len,
    bool compress_defect)
{
    try {
        // Input validation
        if (path == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }
        if (payload_len > 0 && payload == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }
        if (config_json_len > 0 && config_json == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }

        const uint8_t* final_payload    = payload;
        uint64_t       final_payload_len = payload_len;
        bool           has_compression   = false;
        uint64_t       raw_payload_len   = 0;

        // RLE compression for DEFECT maps
        std::vector<uint8_t> compressed_buf;
        if (compress_defect &&
            hdr_template.type == static_cast<uint32_t>(XCAL_TYPE_DEFECT) &&
            payload != nullptr &&
            payload_len > 0)
        {
            int rc = rle_encode(payload, static_cast<size_t>(payload_len),
                                compressed_buf);
            if (rc == XPE_OK && !compressed_buf.empty()) {
                // Only use compressed version if it's actually smaller
                if (compressed_buf.size() < payload_len) {
                    final_payload     = compressed_buf.data();
                    final_payload_len = static_cast<uint64_t>(compressed_buf.size());
                    has_compression   = true;
                    raw_payload_len   = payload_len;
                }
                // If compressed is larger, fall through to uncompressed
            }
            // If RLE encode fails, proceed with uncompressed data
        }

        // Build config JSON with compression metadata
        std::string effective_config = build_config_json(
            config_json, config_json_len,
            has_compression,
            XCAL_COMPRESSION_RLE,
            raw_payload_len);

        const uint8_t* final_config = effective_config.empty()
            ? nullptr
            : reinterpret_cast<const uint8_t*>(effective_config.data());
        uint64_t final_config_len = effective_config.size();

        // Populate header fields controlled by writer
        hdr_template.config_json_len = final_config_len;
        hdr_template.payload_len     = final_payload_len;

        // Compute SHA-256(config_json || payload)
        auto sha = compute_sha256_two_parts(
            final_config, static_cast<size_t>(final_config_len),
            final_payload, static_cast<size_t>(final_payload_len));
        std::memcpy(hdr_template.sha256, sha.data(), 32);

        // Build tmp path
        std::string tmp_path = std::string(path) + ".tmp";

        // Write to tmp file
        {
            std::ofstream f(tmp_path, std::ios::binary | std::ios::trunc);
            if (!f.is_open()) {
                return XPE_ERR_IO_FAILED;
            }

            // Write header (pack=1, 152 bytes)
            f.write(reinterpret_cast<const char*>(&hdr_template),
                    sizeof(XCalFileHeader));
            if (!f.good()) {
                return XPE_ERR_IO_FAILED;
            }

            // Write config_json (may be empty)
            if (final_config_len > 0) {
                f.write(reinterpret_cast<const char*>(final_config),
                        static_cast<std::streamsize>(final_config_len));
                if (!f.good()) {
                    return XPE_ERR_IO_FAILED;
                }
            }

            // Write payload
            if (final_payload_len > 0) {
                f.write(reinterpret_cast<const char*>(final_payload),
                        static_cast<std::streamsize>(final_payload_len));
                if (!f.good()) {
                    return XPE_ERR_IO_FAILED;
                }
            }

            f.flush();
            if (!f.good()) {
                return XPE_ERR_IO_FAILED;
            }
        }  // f is closed here

        // Atomic rename
        if (std::rename(tmp_path.c_str(), path) != 0) {
            std::remove(tmp_path.c_str());
            return XPE_ERR_IO_FAILED;
        }

        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
