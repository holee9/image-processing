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

struct XpeConfigDoc;   // xpe_preprocess_internal.h

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
 * @param out_config_doc When given, receives the config block PARSED (QA-A-209c): the reader parses the block once, to
 *                     read the compression metadata, and hands that document to the caller, so a loader that reads
 *                     keys from the block (quality fields, dose range, extension start) does not parse it again. It
 *                     is the document of the REPAIRED text when the block was one of the old writer's two shapes
 *                     (xcal_format.h). An empty document for a block of length 0.
 * @return XPE_OK on success.
 *         XPE_ERR_INVALID_INPUT if path is nullptr.
 *         XPE_ERR_IO_FAILED on file not found or read error.
 *         XPE_ERR_CONFIG_INVALID on header validation, SHA-256 mismatch, or a config block that is not one valid
 *                                JSON object (xpe_config_parse_block; the old writer's two shapes excepted).
 *         XPE_ERR_CALIBRATION_EXPIRED if file has expired.
 *         XPE_ERR_OUT_OF_MEMORY on allocation failure.
 */
/**
 * Where an UNCOMPRESSED payload is read to, when the caller wants it in its own buffer (QA-A-235b, #245).
 *
 * The default path reads the payload into `out_payload` (a zero-filled vector) and the loader then copies it into
 * the map it keeps: two full-size passes that the SHA-256 and the file read do not need. With a sink the reader
 * asks `acquire` for the destination once, after the header has been validated and before any payload byte is
 * read, reads each chunk straight into it and hashes the same chunk as before (the digest and the order of the
 * checks are unchanged).
 *
 *  - `acquire(ctx, header, len)` returns `len` writable bytes, or nullptr to DECLINE (a size or type the caller
 *    does not take directly): the reader then runs the default path, so which error a doubly bad file gets does not
 *    change. It may throw std::bad_alloc (the reader reports XPE_ERR_OUT_OF_MEMORY).
 *  - The destination must be a buffer the caller has NOT published: if the read, the hash check or anything after
 *    it fails, the reader returns the error and the caller discards the buffer. Nothing it holds is reachable
 *    from the module-global store, so a failed load leaves the previous map exactly as it was.
 *  - `filled` is set to true only on XPE_OK with the payload in the sink's buffer (then `out_payload` is empty).
 *    A compressed file, or a payload of length 0, does not use the sink: `filled` stays false and `out_payload`
 *    holds the (decompressed) bytes as before.
 */
struct XCalPayloadSink {
    uint8_t* (*acquire)(void* ctx, const XCalFileHeader& header, uint64_t len) = nullptr;
    void*    ctx = nullptr;
    bool     filled = false;
};

XPE_API XpeErrorCode read_xcal_file(
    const char*            path,
    XCalFileHeader&        out_header,
    std::vector<uint8_t>&  out_config,
    std::vector<uint8_t>&  out_payload,
    bool                   check_expiry = true,
    int                    expected_type = -1,
    XpeConfigDoc*          out_config_doc = nullptr,
    XCalPayloadSink*       sink = nullptr);

#endif /* XPE_XCAL_READER_HPP */
