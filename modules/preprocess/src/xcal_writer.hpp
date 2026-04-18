/**
 * @file xcal_writer.hpp
 * @brief XCal v1 file writer (T-004)
 *
 * Writes a valid XCal v1 binary file with correct SHA-256 hash.
 * Uses atomic write pattern: write to .tmp, flush+close, rename to final.
 *
 * REQ-P1A-019: xpe_calib_save uses this writer.
 * REQ-P1A-017: xpe_calib_generate_offset uses this writer.
 */

#ifndef XPE_XCAL_WRITER_HPP
#define XPE_XCAL_WRITER_HPP

#include "xpe/preprocess/xcal_format.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include <cstdint>
#include <cstddef>

/**
 * @brief Write an XCal v1 file atomically.
 *
 * Computes SHA-256 of (config_json || payload), populates header.sha256,
 * then writes header + config_json + payload to path+".tmp" and renames
 * to the final path.
 *
 * @param path            Destination file path (UTF-8).
 * @param hdr_template    Caller-supplied header template.
 *                        Fields filled by writer: sha256, config_json_len, payload_len.
 *                        All other fields must be set by caller.
 * @param config_json     Optional config JSON bytes (nullptr if none).
 * @param config_json_len Length of config_json in bytes (0 if none).
 * @param payload         Pixel payload bytes.
 * @param payload_len     Length of payload in bytes.
 * @return XPE_OK on success.
 *         XPE_ERR_INVALID_INPUT if path or payload is nullptr when payload_len > 0.
 *         XPE_ERR_IO_FAILED on file write error.
 *         XPE_ERR_OUT_OF_MEMORY on allocation failure.
 */
XPE_API XpeErrorCode write_xcal_file(
    const char* path,
    XCalFileHeader hdr_template,
    const uint8_t* config_json,
    uint64_t config_json_len,
    const uint8_t* payload,
    uint64_t payload_len);

#endif /* XPE_XCAL_WRITER_HPP */
