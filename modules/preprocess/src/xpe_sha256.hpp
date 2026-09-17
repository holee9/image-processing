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
 * @brief Incremental SHA-256 over chunks fed in order (QA-A-105, #179).
 *
 * Same digest as compute_sha256_two_parts() when the same bytes are fed in the
 * same order; it exists so a file can be hashed while it is being read instead
 * of after the whole payload is in memory.
 *
 * QA-A-106 (#179): the implementation lives in xpe_sha256_backend.cpp and uses
 * Windows CNG when the provider opens, PicoSHA2 otherwise. Both produce the
 * same SHA-256 digest; xpe_sha256_backend_name() says which one this build got.
 */
class Sha256Stream {
public:
    Sha256Stream();
    ~Sha256Stream();
    Sha256Stream(const Sha256Stream&) = delete;
    Sha256Stream& operator=(const Sha256Stream&) = delete;

    void update(const uint8_t* data, size_t len);

    /** Finishes (once) and returns the digest. */
    const std::array<uint8_t, 32>& digest();

private:
    struct Impl;
    Impl*                   impl_;
    std::array<uint8_t, 32> digest_ = {};
    bool                    done_ = false;
};

/** @return "cng" or "picosha2" -- which backend Sha256Stream uses here. */
const char* xpe_sha256_backend_name();

/**
 * @brief Compute SHA-256 over a contiguous byte buffer.
 *
 * @param data  Pointer to data bytes (may be nullptr when len == 0).
 * @param len   Number of bytes.
 * @return std::array<uint8_t, 32> containing the 256-bit digest.
 */
inline std::array<uint8_t, 32> compute_sha256(const uint8_t* data, size_t len) {
    Sha256Stream hasher;                      // CNG when available (QA-A-106)
    hasher.update(data, len);
    return hasher.digest();
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
    Sha256Stream hasher;                      // CNG when available (QA-A-106)
    hasher.update(buf1, len1);
    hasher.update(buf2, len2);
    return hasher.digest();
}



#endif /* XPE_SHA256_HPP */
