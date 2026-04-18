/**
 * @file xcal_writer.cpp
 * @brief XCal v1 file writer implementation (T-004)
 *
 * REQ-P1A-019, REQ-P1A-017, REQ-P1A-030
 */

#include "xcal_writer.hpp"
#include "xpe_sha256.hpp"
#include "xcal_validator.hpp"

#include <cstring>
#include <fstream>
#include <string>
#include <cstdio>  // std::rename

// @MX:NOTE: [AUTO] Atomic write pattern: write to <path>.tmp, then rename.
// This prevents corrupt partial files visible to concurrent readers.

XpeErrorCode write_xcal_file(
    const char* path,
    XCalFileHeader hdr_template,
    const uint8_t* config_json,
    uint64_t config_json_len,
    const uint8_t* payload,
    uint64_t payload_len)
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

        // Populate header fields controlled by writer
        hdr_template.config_json_len = config_json_len;
        hdr_template.payload_len     = payload_len;

        // Compute SHA-256(config_json || payload)
        auto sha = compute_sha256_two_parts(
            config_json, static_cast<size_t>(config_json_len),
            payload, static_cast<size_t>(payload_len));
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
            if (config_json_len > 0) {
                f.write(reinterpret_cast<const char*>(config_json),
                        static_cast<std::streamsize>(config_json_len));
                if (!f.good()) {
                    return XPE_ERR_IO_FAILED;
                }
            }

            // Write payload
            if (payload_len > 0) {
                f.write(reinterpret_cast<const char*>(payload),
                        static_cast<std::streamsize>(payload_len));
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
