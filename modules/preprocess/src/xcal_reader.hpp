/**
 * @file xcal_reader.hpp
 * @brief XCal v1 file reader (T-005)
 *
 * Reads, validates, and returns header + optional config_json + payload.
 * Verifies SHA-256 integrity and optionally checks expiry.
 *
 * REQ-P1A-014, REQ-P1A-015, REQ-P1A-016, REQ-P1A-018
 */

#ifndef XPE_XCAL_READER_HPP
#define XPE_XCAL_READER_HPP

#include "xpe/preprocess/xcal_format.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include <vector>
#include <cstdint>

/**
 * @brief Read and validate an XCal v1 file.
 *
 * Steps:
 *  1. Open file and read 152-byte header.
 *  2. Validate header fields via validate_xcal_header().
 *  3. Read config_json_len bytes (if non-zero).
 *  4. Read payload_len bytes.
 *  5. Recompute SHA-256(config_json || payload) and compare with header.sha256.
 *  6. If check_expiry and expiry_epoch_ms != 0 and now > expiry_epoch_ms:
 *     return XPE_ERR_CALIBRATION_EXPIRED.
 *
 * @param path         File path (UTF-8).
 * @param out_header   Receives the parsed header (pack=1 layout).
 * @param out_config   Receives config_json bytes (empty if none).
 * @param out_payload  Receives pixel payload bytes.
 * @param check_expiry When true, validate expiry timestamp.
 * @param expected_type Expected XCalType (XCAL_TYPE_*). Pass -1 to skip.
 * @return XPE_OK on success.
 *         XPE_ERR_INVALID_INPUT if path is nullptr.
 *         XPE_ERR_IO_FAILED on file not found or read error.
 *         XPE_ERR_CONFIG_INVALID on header validation or SHA-256 mismatch.
 *         XPE_ERR_CALIBRATION_EXPIRED if file has expired.
 *         XPE_ERR_OUT_OF_MEMORY on allocation failure.
 */
XPE_API XpeErrorCode read_xcal_file(
    const char*            path,
    XCalFileHeader&        out_header,
    std::vector<uint8_t>&  out_config,
    std::vector<uint8_t>&  out_payload,
    bool                   check_expiry = true,
    int                    expected_type = -1);

#endif /* XPE_XCAL_READER_HPP */
