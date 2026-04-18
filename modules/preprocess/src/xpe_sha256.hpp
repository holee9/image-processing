/**
 * @file xpe_sha256.hpp
 * @brief SHA-256 computation wrapper for XCal integrity verification (T-003)
 *
 * Wraps PicoSHA2 (header-only, MIT license) from third_party/picosha2/.
 * Suppresses MSVC /W4 warnings in vendor header via pragma warning.
 *
 * REQ-P1A-014~016 (AC-CAL-001): SHA-256 integrity validation of XCal payload.
 */

#ifndef XPE_SHA256_HPP
#define XPE_SHA256_HPP

#include <array>
#include <cstdint>
#include <cstddef>
#include <vector>

/* Suppress warnings from the vendor header (not our code) */
#ifdef _MSC_VER
#  pragma warning(push, 3)
#endif
#include "picosha2/picosha2.h"
#ifdef _MSC_VER
#  pragma warning(pop)
#endif

/**
 * @brief Compute SHA-256 over a contiguous byte buffer.
 *
 * @param data  Pointer to data bytes (may be nullptr when len == 0).
 * @param len   Number of bytes.
 * @return std::array<uint8_t, 32> containing the 256-bit digest.
 */
inline std::array<uint8_t, 32> compute_sha256(const uint8_t* data, size_t len) {
    std::array<uint8_t, 32> digest = {};
    if (len == 0) {
        // SHA-256 of empty string: well-known constant
        picosha2::hash256(static_cast<const uint8_t*>(nullptr),
                          static_cast<const uint8_t*>(nullptr),
                          digest.begin(), digest.end());
    } else {
        picosha2::hash256(data, data + len, digest.begin(), digest.end());
    }
    return digest;
}

/**
 * @brief Compute SHA-256 over two contiguous buffers concatenated logically.
 *
 * Equivalent to SHA-256(buf1 || buf2) without allocating a combined buffer.
 * This matches the XCal v1 hash coverage: SHA-256(config_json || payload).
 *
 * @param buf1     First buffer (config_json bytes; nullptr allowed when len1==0).
 * @param len1     Length of buf1.
 * @param buf2     Second buffer (payload bytes; nullptr allowed when len2==0).
 * @param len2     Length of buf2.
 * @return std::array<uint8_t, 32> containing the 256-bit digest.
 */
inline std::array<uint8_t, 32> compute_sha256_two_parts(
        const uint8_t* buf1, size_t len1,
        const uint8_t* buf2, size_t len2)
{
    picosha2::hash256_one_by_one hasher;
    hasher.init();
    if (buf1 && len1 > 0) {
        hasher.process(buf1, buf1 + len1);
    }
    if (buf2 && len2 > 0) {
        hasher.process(buf2, buf2 + len2);
    }
    hasher.finish();

    std::array<uint8_t, 32> digest = {};
    hasher.get_hash_bytes(digest.begin(), digest.end());
    return digest;
}

#endif /* XPE_SHA256_HPP */
