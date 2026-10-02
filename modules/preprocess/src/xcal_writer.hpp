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
#ifdef XPE_CACHE_TEST_HOOKS
#include <ios>
#endif

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

/**
 * @brief Write an XCal v1 file atomically with optional RLE compression.
 *
 * Extended version of write_xcal_file that supports RLE compression for
 * DEFECT (UINT8_MASK) payloads. When compress_defect is true and the
 * header type is XCAL_TYPE_DEFECT, the payload is RLE-encoded.
 *
 * Compression metadata is stored in the config_json field as:
 *   {"xcal_compression":1,"xcal_raw_payload_len":NNN}
 *
 * If the compressed payload is larger than the original (e.g. random data),
 * the uncompressed version is written instead.
 *
 * @param path             Destination file path (UTF-8).
 * @param hdr_template     Caller-supplied header template.
 * @param config_json      Optional config JSON bytes (nullptr if none).
 * @param config_json_len  Length of config_json in bytes (0 if none).
 * @param payload          Pixel payload bytes.
 * @param payload_len      Length of payload in bytes.
 * @param compress_defect  Enable RLE compression for DEFECT type payloads.
 * @return XPE_OK on success.
 *         XPE_ERR_INVALID_INPUT if path or payload is nullptr when payload_len > 0.
 *         XPE_ERR_IO_FAILED on file write error.
 *         XPE_ERR_OUT_OF_MEMORY on allocation failure.
 */
XPE_API XpeErrorCode write_xcal_file_ex(
    const char* path,
    XCalFileHeader hdr_template,
    const uint8_t* config_json,
    uint64_t config_json_len,
    const uint8_t* payload,
    uint64_t payload_len,
    bool compress_defect);

#ifdef XPE_CACHE_TEST_HOOKS
/**
 * Test-only (QA-A-221c, Codex #81): called right after each stream operation of the temporary-file write and
 * before its outcome is judged, with the step number -- 1 header, 2 config (only when there is one), 3 payload
 * (only when there is one), 4 flush, 5 close -- and the stream itself, so a test can leave a stale error behind
 * after a step that succeeded and fail the next step in a way the operating system does not report (the
 * stream's own failbit). Only the allocation-failure executable defines XPE_CACHE_TEST_HOOKS; the shipped
 * library has neither the declaration nor the call.
 */
extern void (*xpe_xcal_write_step_hook)(int step, std::ios& stream);
#endif

#endif /* XPE_XCAL_WRITER_HPP */
