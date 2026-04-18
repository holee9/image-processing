/**
 * @file xcal_validator.cpp
 * @brief XCal v1 header validation implementation (SPEC-XPE-P1A SUP-01)
 *
 * REQ-P1A-014, REQ-P1A-015, REQ-P1A-016, REQ-P1A-030
 *
 * @MX:ANCHOR: High fan_in — used by xpe_calib_load_offset, xpe_calib_load_gain,
 *            xpe_calib_load_defect_map, xpe_calib_check_expiry, and all 6 SUP-01 functions.
 * @MX:REASON: Invariant contract — all calibration loaders call validate_xcal_header()
 *             before data access. Breaking this function breaks the entire XCal ecosystem.
 */

#include "xcal_validator.hpp"
#include "xpe/common/xpe_types.h"  // for XPE_API (XPE_DLL_EXPORT)
#include <cstring>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/**
 * @brief Map XCalPixelFormat to bytes per pixel.
 */
size_t xcal_bytes_per_pixel(uint32_t fmt) {
    switch (fmt) {
        case XCAL_FMT_UINT16:    return sizeof(uint16_t);  // 2
        case XCAL_FMT_FLOAT32:   return sizeof(float);     // 4
        case XCAL_FMT_UINT8_MASK: return sizeof(uint8_t);  // 1
        default:                 return 0;
    }
}

// ---------------------------------------------------------------------------
// validate_xcal_header
// ---------------------------------------------------------------------------

/**
 * @brief Validate XCal v1 header fields.
 *
 * Invariant checks enforced (all must pass for XPE_OK):
 *  1. magic == "XCAL"
 *  2. version == 1
 *  3. type in [0..2]
 *  4. pixel_format in [0..2]
 *  5. type-pixel_format semantic consistency
 *  6. width in [1..XCAL_MAX_DIM]
 *  7. height in [1..XCAL_MAX_DIM]
 *  8. payload_len == width * height * bpp (exact match)
 *  9. config_json_len <= XCAL_MAX_CONFIG_JSON_LEN
 * 10. expected_type match (when expected_type >= 0)
 */
XpeErrorCode validate_xcal_header(const XCalFileHeader& header,
                                   int expected_type) {
    // Check 1: magic
    if (std::memcmp(header.magic, XCAL_MAGIC, 4) != 0) {
        return XPE_ERR_CONFIG_INVALID;
    }

    // Check 2: version
    if (header.version != XCAL_VERSION) {
        return XPE_ERR_CONFIG_INVALID;
    }

    // Check 3: type range
    if (header.type > XCAL_TYPE_DEFECT) {
        return XPE_ERR_CONFIG_INVALID;
    }

    // Check 4: pixel_format range
    if (header.pixel_format > XCAL_FMT_UINT8_MASK) {
        return XPE_ERR_CONFIG_INVALID;
    }

    // Check 5: type <-> pixel_format semantic consistency
    //   OFFSET -> FLOAT32 (computed mean of UINT16 dark frames)
    //   GAIN   -> FLOAT32 (reciprocal gain map)
    //   DEFECT -> UINT8_MASK (boolean bad-pixel map)
    if (header.type == XCAL_TYPE_OFFSET && header.pixel_format != XCAL_FMT_FLOAT32) {
        return XPE_ERR_CONFIG_INVALID;
    }
    if (header.type == XCAL_TYPE_GAIN && header.pixel_format != XCAL_FMT_FLOAT32) {
        return XPE_ERR_CONFIG_INVALID;
    }
    if (header.type == XCAL_TYPE_DEFECT && header.pixel_format != XCAL_FMT_UINT8_MASK) {
        return XPE_ERR_CONFIG_INVALID;
    }

    // Check 6: width in valid range
    if (header.width == 0 || header.width > XCAL_MAX_DIM) {
        return XPE_ERR_CONFIG_INVALID;
    }

    // Check 7: height in valid range
    if (header.height == 0 || header.height > XCAL_MAX_DIM) {
        return XPE_ERR_CONFIG_INVALID;
    }

    // Check 8: payload_len must exactly equal width * height * bpp
    size_t bpp = xcal_bytes_per_pixel(header.pixel_format);
    if (bpp == 0) {
        return XPE_ERR_CONFIG_INVALID;  // Unknown format (already caught above)
    }
    uint64_t expected_payload = static_cast<uint64_t>(header.width) *
                                static_cast<uint64_t>(header.height) *
                                static_cast<uint64_t>(bpp);
    if (header.payload_len != expected_payload) {
        return XPE_ERR_CONFIG_INVALID;
    }

    // Check 9: config_json_len sanity cap
    if (header.config_json_len > XCAL_MAX_CONFIG_JSON_LEN) {
        return XPE_ERR_CONFIG_INVALID;
    }

    // Check 10: expected_type match (caller-specified)
    if (expected_type >= 0 && header.type != static_cast<uint32_t>(expected_type)) {
        return XPE_ERR_CONFIG_INVALID;
    }

    return XPE_OK;
}
