/**
 * @file xcal_validator.cpp
 * @brief XCal v1 header validation implementation (SPEC-XPE-P1A SUP-01)
 *
 * REQ-P1A-014, REQ-P1A-015, REQ-P1A-016, REQ-P1A-030
 *
 * @MX:ANCHOR: High fan_in — used by xpe_calib_load_offset, xpe_calib_load_gain,
 *            xpe_calib_load_defect_map, and all 6 SUP-01 functions. xpe_calib_check_expiry reads the
 *            header itself and calls only validate_xcal_session_field (check 11) from here (QA-A-229d).
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
        case static_cast<uint32_t>(XCAL_FMT_UINT16):    return sizeof(uint16_t);  // 2
        case static_cast<uint32_t>(XCAL_FMT_FLOAT32):   return sizeof(float);     // 4
        case static_cast<uint32_t>(XCAL_FMT_UINT8_MASK): return sizeof(uint8_t); // 1
        default:                 return 0;
    }
}

/**
 * The 64-byte session_id field (QA-A-229b, Codex #93 finding 3): UTF-8 text, NUL-terminated and zero-padded.
 * That is: there is a NUL somewhere in the field (so a text of up to 63 bytes, never 64 unterminated ones),
 * every byte after the first NUL is 0, and the bytes before it are well-formed UTF-8 (no overlong form, no
 * surrogate, nothing above U+10FFFF). An empty field is valid. Before this nothing checked the field, and the
 * loaders copied only 63 of its 64 bytes, so two ids that differ in the last byte compared equal.
 */
static bool session_field_valid(const char (&field)[64]) {
    size_t n = 0;
    while (n < sizeof(field) && field[n] != 0) ++n;
    if (n == sizeof(field)) return false;                       // no terminator
    for (size_t i = n; i < sizeof(field); ++i) {
        if (field[i] != 0) return false;                        // not zero-padded
    }
    const unsigned char* s = reinterpret_cast<const unsigned char*>(field);
    size_t i = 0;
    while (i < n) {
        const unsigned char c = s[i];
        if (c < 0x80) { ++i; continue; }
        size_t len = 0;
        uint32_t cp = 0;
        if (c >= 0xC2 && c <= 0xDF) { len = 2; cp = c & 0x1Fu; }
        else if (c >= 0xE0 && c <= 0xEF) { len = 3; cp = c & 0x0Fu; }
        else if (c >= 0xF0 && c <= 0xF4) { len = 4; cp = c & 0x07u; }
        else return false;                                      // stray continuation byte, C0/C1, F5..FF
        if (i + len > n) return false;                          // truncated sequence
        for (size_t k = 1; k < len; ++k) {
            if ((s[i + k] & 0xC0u) != 0x80u) return false;
            cp = (cp << 6) | (s[i + k] & 0x3Fu);
        }
        if (len == 3 && cp < 0x800u) return false;              // overlong
        if (len == 4 && (cp < 0x10000u || cp > 0x10FFFFu)) return false;
        if (cp >= 0xD800u && cp <= 0xDFFFu) return false;       // surrogate
        i += len;
    }
    return true;
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
 * 11. session_id field is well-formed (QA-A-229b): terminated, zero-padded, UTF-8
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
    // QA-A-37 (#140): XCAL_TYPE_GAIN_POLY (= 3) sits above XCAL_TYPE_DEFECT
    // (= 2), so the old ceiling rejected every polynomial gain file the public
    // generator writes -- the file was writable and unreadable.
    // QA-A-110 (#186): XCAL_TYPE_NONLIN_LUT (= 4) is the new ceiling.
    if (header.type > static_cast<uint32_t>(XCAL_TYPE_NONLIN_LUT)) {
        return XPE_ERR_CONFIG_INVALID;
    }

    // Check 4: pixel_format range
    if (header.pixel_format > static_cast<uint32_t>(XCAL_FMT_UINT8_MASK)) {
        return XPE_ERR_CONFIG_INVALID;
    }

    // Check 5: type <-> pixel_format semantic consistency
    //   OFFSET -> FLOAT32 (computed mean of UINT16 dark frames)
    //   GAIN   -> FLOAT32 (reciprocal gain map)
    //   DEFECT -> UINT8_MASK (boolean bad-pixel map)
    if (header.type == static_cast<uint32_t>(XCAL_TYPE_OFFSET) && header.pixel_format != static_cast<uint32_t>(XCAL_FMT_FLOAT32)) {
        return XPE_ERR_CONFIG_INVALID;
    }
    if (header.type == static_cast<uint32_t>(XCAL_TYPE_GAIN) && header.pixel_format != static_cast<uint32_t>(XCAL_FMT_FLOAT32)) {
        return XPE_ERR_CONFIG_INVALID;
    }
    if (header.type == static_cast<uint32_t>(XCAL_TYPE_DEFECT) && header.pixel_format != static_cast<uint32_t>(XCAL_FMT_UINT8_MASK)) {
        return XPE_ERR_CONFIG_INVALID;
    }
    if (header.type == static_cast<uint32_t>(XCAL_TYPE_GAIN_POLY) && header.pixel_format != static_cast<uint32_t>(XCAL_FMT_FLOAT32)) {
        return XPE_ERR_CONFIG_INVALID;
    }
    //   NONLIN_LUT -> UINT16 (SRS-CALIB-FUNC-006-EXT 6a: "LUT data type: uint16")
    if (header.type == static_cast<uint32_t>(XCAL_TYPE_NONLIN_LUT) && header.pixel_format != static_cast<uint32_t>(XCAL_FMT_UINT16)) {
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
    if (header.type == static_cast<uint32_t>(XCAL_TYPE_GAIN_POLY)) {
        // A polynomial gain file carries (degree + 1) coefficient planes, so
        // its payload is a whole positive multiple of one plane rather than
        // exactly one. The degree itself is derived from that multiple.
        if (expected_payload == 0 ||
            header.payload_len == 0 ||
            header.payload_len % expected_payload != 0) {
            return XPE_ERR_CONFIG_INVALID;
        }
    } else if (header.payload_len != expected_payload) {
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

    // Check 11: the session field's format (QA-A-229b)
    return validate_xcal_session_field(header);
}

XpeErrorCode validate_xcal_session_field(const XCalFileHeader& header) {
    return session_field_valid(header.session_id) ? XPE_OK : XPE_ERR_CONFIG_INVALID;
}
