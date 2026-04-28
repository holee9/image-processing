/**
 * @file rle_codec.cpp
 * @brief Run-Length Encoding codec implementation for XCal defect map compression
 *
 * @MX:NOTE: [AUTO] RLE format: [value:u8][count:u32le] per run.
 * @MX:SPEC: SPEC-XPE-P1A
 */

#include "rle_codec.hpp"
#include "xpe/common/xpe_error.h"

#include <cstring>
#include <cstdint>

// Little-endian write of uint32_t
static inline void write_u32_le(uint8_t* dst, uint32_t val) {
    dst[0] = static_cast<uint8_t>(val & 0xFFu);
    dst[1] = static_cast<uint8_t>((val >> 8) & 0xFFu);
    dst[2] = static_cast<uint8_t>((val >> 16) & 0xFFu);
    dst[3] = static_cast<uint8_t>((val >> 24) & 0xFFu);
}

// Little-endian read of uint32_t
static inline uint32_t read_u32_le(const uint8_t* src) {
    return static_cast<uint32_t>(src[0]) |
           (static_cast<uint32_t>(src[1]) << 8) |
           (static_cast<uint32_t>(src[2]) << 16) |
           (static_cast<uint32_t>(src[3]) << 24);
}

// @MX:WARN: [AUTO] Maximum run length is UINT32_MAX. Longer runs are split.
// @MX:REASON: count field is 4 bytes LE; runs exceeding UINT32_MAX emit multiple entries.

int rle_encode(const uint8_t* data, size_t len, std::vector<uint8_t>& out) {
    out.clear();

    if (len == 0) {
        return XPE_OK;
    }
    if (data == nullptr) {
        return XPE_ERR_INVALID_INPUT;
    }

    try {
        // Worst case: 5 bytes per input byte. Reserve conservatively.
        // For typical defect maps (mostly zeros), compression ratio is >99%.
        const size_t reserve_hint = (len < size_t{1024}) ? (len * size_t{5}) : size_t{4096};
        out.reserve(reserve_hint);

        uint8_t  current_val = data[0];
        uint32_t run_count   = 1;
        constexpr uint32_t max_run = 0xFFFFFFFFu;

        for (size_t i = 1; i < len; ++i) {
            if (data[i] == current_val && run_count < max_run) {
                ++run_count;
            } else {
                // Emit run: [value][count_le32]
                size_t pos = out.size();
                out.resize(pos + size_t{5});
                out[pos] = current_val;
                write_u32_le(&out[pos + size_t{1}], run_count);

                current_val = data[i];
                run_count   = 1;
            }
        }

        // Emit final run
        {
            size_t pos = out.size();
            out.resize(pos + size_t{5});
            out[pos] = current_val;
            write_u32_le(&out[pos + size_t{1}], run_count);
        }

        return XPE_OK;

    } catch (const std::bad_alloc&) {
        out.clear();
        return XPE_ERR_OUT_OF_MEMORY;
    }
}

int rle_decode(const uint8_t* encoded, size_t enc_len,
               size_t expected_len,
               std::vector<uint8_t>& out) {
    out.clear();

    if (enc_len == 0) {
        return XPE_OK;
    }
    if (encoded == nullptr) {
        return XPE_ERR_INVALID_INPUT;
    }

    try {
        // Validate: encoded length must be a multiple of 5
        if (enc_len % size_t{5} != 0u) {
            return XPE_ERR_CONFIG_INVALID;
        }

        // Pre-allocate output if expected_len is known
        if (expected_len > 0) {
            out.reserve(expected_len);
        }

        size_t decoded_total = 0;
        for (size_t i = 0; i < enc_len; i += size_t{5}) {
            uint8_t  value = encoded[i];
            uint32_t count = read_u32_le(&encoded[i + size_t{1}]);

            // Overflow check
            if (decoded_total + count < decoded_total) {
                out.clear();
                return XPE_ERR_CONFIG_INVALID;
            }
            decoded_total += count;

            size_t pos = out.size();
            out.resize(pos + count);
            std::memset(&out[pos], value, count);
        }

        // Validate against expected size if provided
        if (expected_len > 0 && decoded_total != expected_len) {
            out.clear();
            return XPE_ERR_CONFIG_INVALID;
        }

        return XPE_OK;

    } catch (const std::bad_alloc&) {
        out.clear();
        return XPE_ERR_OUT_OF_MEMORY;
    }
}

int rle_decoded_size(const uint8_t* encoded, size_t enc_len, size_t& out_size) {
    out_size = 0;

    if (enc_len == 0) {
        return XPE_OK;
    }
    if (encoded == nullptr) {
        return XPE_ERR_INVALID_INPUT;
    }

    // Validate: encoded length must be a multiple of 5
    if (enc_len % size_t{5} != 0u) {
        return XPE_ERR_CONFIG_INVALID;
    }

    for (size_t i = 0; i < enc_len; i += size_t{5}) {
        uint32_t count = read_u32_le(&encoded[i + size_t{1}]);

        // Overflow check
        if (out_size + count < out_size) {
            return XPE_ERR_CONFIG_INVALID;
        }
        out_size += count;
    }

    return XPE_OK;
}
