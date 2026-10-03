/**
 * @file xcal_validator.hpp
 * @brief XCal v1 header validation (SPEC-XPE-P1A SUP-01)
 *
 * REQ-P1A-014, REQ-P1A-015, REQ-P1A-016
 */

#ifndef XPE_XCAL_VALIDATOR_HPP
#define XPE_XCAL_VALIDATOR_HPP

#include "xpe/preprocess/xcal_format.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"  // for XPE_API

/**
 * @brief Validate an XCal v1 file header.
 *
 * Checks:
 *  - magic == "XCAL"
 *  - version == 1
 *  - type in [0..2]
 *  - pixel_format in [0..2]
 *  - pixel_format matches the expected format for the given type
 *  - width and height in [1..XCAL_MAX_DIM]
 *  - payload_len == width * height * bytes_per_pixel(pixel_format)
 *  - config_json_len <= XCAL_MAX_CONFIG_JSON_LEN
 *
 * @param header    Header read from file (pack=1, 152 bytes).
 * @param expected_type  Expected XCalType. Pass -1 to skip type check.
 * @return XPE_OK                if header is fully valid.
 *         XPE_ERR_CONFIG_INVALID if any field is malformed.
 */
XPE_API XpeErrorCode validate_xcal_header(const XCalFileHeader& header,
                                           int expected_type = -1);

/**
 * @brief Validate the 64-byte session_id field of an XCal header (check 11 of validate_xcal_header).
 *
 * The field is UTF-8 text, NUL-terminated and zero-padded. Kept as its own function because
 * xpe_calib_check_expiry reads the header itself and does not run the full validation, but must judge
 * this field the way the loaders do (QA-A-229d, Codex #97).
 *
 * @return XPE_OK if the field is well-formed (an empty field is), XPE_ERR_CONFIG_INVALID otherwise.
 */
XPE_API XpeErrorCode validate_xcal_session_field(const XCalFileHeader& header);

/**
 * @brief Return the expected bytes-per-pixel for a given XCalPixelFormat.
 *
 * @param fmt  XCalPixelFormat value.
 * @return bytes per pixel, or 0 for unknown format.
 */
XPE_API size_t xcal_bytes_per_pixel(uint32_t fmt);

#endif /* XPE_XCAL_VALIDATOR_HPP */
