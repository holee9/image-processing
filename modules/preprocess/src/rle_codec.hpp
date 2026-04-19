/**
 * @file rle_codec.hpp
 * @brief Run-Length Encoding codec for XCal defect map compression (SPEC-XPE-P1A)
 *
 * Provides RLE encode/decode for UINT8_MASK defect maps. Typical defect maps
 * have long runs of 0x00 (good pixels), making RLE very effective:
 * 3072x3072 all-zero defect map: 9.4 MB -> ~100 bytes.
 *
 * RLE binary format (little-endian):
 *   For each run: [value:1 byte][count:4 bytes LE]
 *   Maximum run length: 2^32 - 1 (4,294,967,295).
 *
 * @MX:NOTE: [AUTO] RLE format is self-contained; no external header needed.
 * @MX:SPEC: SPEC-XPE-P1A
 */

#ifndef XPE_RLE_CODEC_HPP
#define XPE_RLE_CODEC_HPP

#include <cstdint>
#include <cstddef>
#include <vector>

/**
 * @brief Encode a UINT8_MASK buffer using Run-Length Encoding.
 *
 * Each run produces 5 bytes: 1 byte value + 4 bytes count (LE).
 * Worst case expansion: input of alternating distinct values produces
 * 5 bytes per 1 byte input (5x). For defect maps this is highly unlikely.
 *
 * @param data  Pointer to input byte buffer (may be nullptr when len == 0).
 * @param len   Number of bytes in input buffer.
 * @param out   Receives the RLE-encoded bytes. Cleared on error.
 * @return XPE_OK on success.
 *         XPE_ERR_OUT_OF_MEMORY on allocation failure.
 *         XPE_ERR_INVALID_INPUT if data is nullptr and len > 0.
 */
int rle_encode(const uint8_t* data, size_t len, std::vector<uint8_t>& out);

/**
 * @brief Decode an RLE-encoded buffer back to raw bytes.
 *
 * @param encoded  Pointer to RLE-encoded byte buffer.
 * @param enc_len  Number of bytes in encoded buffer.
 * @param expected_len  Expected output size in bytes (for pre-allocation and validation).
 *                      If 0, the function will decode without size validation.
 * @param out     Receives the decoded raw bytes. Cleared on error.
 * @return XPE_OK on success.
 *         XPE_ERR_CONFIG_INVALID if encoded data is malformed (truncated, invalid).
 *         XPE_ERR_OUT_OF_MEMORY on allocation failure.
 *         XPE_ERR_INVALID_INPUT if encoded is nullptr and enc_len > 0.
 */
int rle_decode(const uint8_t* encoded, size_t enc_len,
               size_t expected_len,
               std::vector<uint8_t>& out);

/**
 * @brief Compute the exact decoded size from an RLE-encoded buffer.
 *
 * Walks the encoded buffer summing run counts without decoding.
 *
 * @param encoded  Pointer to RLE-encoded byte buffer.
 * @param enc_len  Number of bytes in encoded buffer.
 * @param out_size Receives the total decoded byte count.
 * @return XPE_OK on success.
 *         XPE_ERR_CONFIG_INVALID if encoded data is malformed.
 *         XPE_ERR_INVALID_INPUT if encoded is nullptr and enc_len > 0.
 */
int rle_decoded_size(const uint8_t* encoded, size_t enc_len, size_t& out_size);

#endif /* XPE_RLE_CODEC_HPP */
