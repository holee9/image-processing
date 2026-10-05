/**
 * @file xcal_reader.cpp
 * @brief XCal v1 file reader implementation (T-005)
 *
 * REQ-P1A-014~016, REQ-P1A-018, REQ-P1A-030
 *
 * Supports automatic RLE decompression for DEFECT payloads when
 * compression metadata is present in config_json.
 *
 * @MX:ANCHOR: read_xcal_file() is the core reader used by xpe_calib_load_offset,
 *            xpe_calib_load_gain, and xpe_calib_load_defect_map (3+ callers).
 * @MX:REASON: Invariant contract -- all calibration loading functions depend on correct
 *             file parsing and validation. File format evolution must maintain backward compat.
 */

#include "xcal_reader.hpp"
#include "xcal_validator.hpp"
#include "xpe_sha256.hpp"
#include "rle_codec.hpp"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <algorithm>
#include <fstream>
#include <cstring>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <string>

// @MX:NOTE: [AUTO] Clock source for expiry: std::chrono::system_clock.
// Resolution is milliseconds (epoch_ms). Non-monotonic but
// consistent with XCal v1 created_epoch_ms semantics.

// Files the writer of 90c1b6b1 and before made (QA-A-209c, Codex #53; narrowed by QA-A-209d, Codex #56). For a
// compressed DEFECT file (RLE, method 1) with a caller config it merged the compression metadata by cutting the
// caller's last '}' and appending ",<meta>}" where <meta> still carried its own '}' -- so the stored block ends "}}".
// Given NO caller config it wrote the pair as an ordinary single-brace object, and it compressed nothing but DEFECT
// maps. The two shapes it could make with a caller config:
//   A: "{" <the caller's members> "," <pair> "}" "}"        the caller had members: a real member, then the comma
//   B: "{" white-space* "," <pair> "}" "}"                  the caller object was empty ("{}", "{ }", "{\n}")
// where <pair> is `"xcal_compression":<digits>,"xcal_raw_payload_len":<digits>` and is the END of the block. This
// repairs EXACTLY those, and only for the file the old writer compressed (the caller of the repair checks that the
// header type is DEFECT and that the method is RLE): drop the extra '}' (A), or the comma and the extra '}' (B). The
// repaired text is then parsed strictly, so the duplicate / nested / pair rules apply to it unchanged. Anything else --
// a third brace, text after, the pair with no member before it, a member between "{," and the pair, the pair not last,
// a nested pair -- is not something the old writer could make and is not repaired. Only a parse that FAILED is
// repaired, and the stored bytes (which the SHA-256 covers) are never altered: the repair exists in memory only.
static bool is_json_ws(char ch) { return ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n'; }

// True when `text` from `pos` is `"xcal_compression":<digits>,"xcal_raw_payload_len":<digits>}` and nothing more.
static bool is_legacy_pair_tail(const std::string& text, size_t pos)
{
    static const char kMethod[] = "\"xcal_compression\":";
    static const char kRaw[] = ",\"xcal_raw_payload_len\":";
    size_t i = pos;
    if (text.compare(i, sizeof(kMethod) - 1, kMethod) != 0) return false;
    i += sizeof(kMethod) - 1;
    size_t digits = 0;
    while (i < text.size() && text[i] >= '0' && text[i] <= '9') { ++i; ++digits; }
    if (digits == 0) return false;
    if (text.compare(i, sizeof(kRaw) - 1, kRaw) != 0) return false;
    i += sizeof(kRaw) - 1;
    digits = 0;
    while (i < text.size() && text[i] >= '0' && text[i] <= '9') { ++i; ++digits; }
    if (digits == 0) return false;
    return i + 1 == text.size() && text[i] == '}';
}

static bool repair_legacy_writer_config(const std::vector<uint8_t>& config, std::string* repaired)
{
    const std::string s(config.begin(), config.end());
    if (s.size() < 2 || s[s.size() - 1] != '}' || s[s.size() - 2] != '}') return false;
    const std::string cand = s.substr(0, s.size() - 1);                   // the one extra '}' dropped
    const size_t p = cand.rfind("\"xcal_compression\":");
    if (p == std::string::npos || !is_legacy_pair_tail(cand, p)) return false;
    const std::string prefix = cand.substr(0, p);
    // both shapes are "{" <something> "," immediately before the pair
    if (prefix.size() < 2 || prefix[0] != '{' || prefix.back() != ',') return false;
    size_t i = 1;
    while (i + 1 < prefix.size() && is_json_ws(prefix[i])) ++i;
    if (i + 1 == prefix.size()) {
        // shape B: only white space between '{' and the comma -- the caller's object had no members; drop the comma
        *repaired = prefix.substr(0, prefix.size() - 1) + cand.substr(p);
        return true;
    }
    // shape A: something other than white space stands between '{' and the comma -- a caller member; whether it is
    // a valid member list, and so whether the pair is a TOP-LEVEL pair at the end rather than a nested one, is the
    // strict parse's verdict, which the caller takes.
    *repaired = cand;
    return true;
}

// Parses an XCal config block into `doc` by the strict rule, with the one narrow exception above. `*repaired` tells
// the caller that the exception was taken (the alert is raised only once the whole file has been accepted).
static XpeErrorCode parse_config_block_with_legacy(const std::vector<uint8_t>& config, uint32_t header_type,
                                                    XpeConfigDoc* doc, bool* repaired)
{
    *repaired = false;
    const char* text = config.empty() ? nullptr : reinterpret_cast<const char*>(config.data());
    XpeErrorCode rc = xpe_config_parse_block(text, config.size(), doc);
    if (rc != XPE_ERR_CONFIG_INVALID) return rc;
    // the old writer compressed DEFECT maps and nothing else
    if (header_type != static_cast<uint32_t>(XCAL_TYPE_DEFECT)) return rc;
    std::string fixed;
    if (!repair_legacy_writer_config(config, &fixed)) return rc;
    XpeConfigDoc fixedDoc;
    if (xpe_config_parse_block(fixed.data(), fixed.size(), &fixedDoc) != XPE_OK) return rc;
    // ... and only ever with method 1 (RLE)
    const XpeConfigEntry* method = fixedDoc.find("xcal_compression");
    if (method == nullptr || !method->scalar || method->quoted || method->text != "1") return rc;
    *doc = std::move(fixedDoc);
    *repaired = true;
    return XPE_OK;
}

// Reads the compression metadata of an XCal config block from its parsed document (QA-A-209b, Codex #49).
//
// The metadata is the pair of TOP-LEVEL keys "xcal_compression" and "xcal_raw_payload_len", each a bare non-negative
// integer. A key inside a nested object is not metadata, a key given twice refuses the block (xpe_config_parse_block),
// and half a pair is a refusal: the writer always makes both, so one alone is a damaged or hand-made block, not
// "uncompressed". `is_compressed` is false when neither key is there.
static XpeErrorCode read_compression_meta(
    const XpeConfigDoc& doc,
    bool& is_compressed,
    uint32_t& out_method,
    uint64_t& out_raw_payload_len)
{
    is_compressed = false;
    const XpeConfigEntry* method = doc.find("xcal_compression");
    const XpeConfigEntry* raw = doc.find("xcal_raw_payload_len");
    if (method == nullptr && raw == nullptr) return XPE_OK;
    if (method == nullptr || raw == nullptr) return XPE_ERR_CONFIG_INVALID;

    // a bare run of decimal digits that fits: not a string, not a sign, not a fraction or exponent
    auto unsignedInteger = [](const XpeConfigEntry* e, unsigned long long limit, unsigned long long* v) {
        if (!e->scalar || e->quoted || e->text.empty() || e->text.size() > 20) return false;
        unsigned long long n = 0;
        for (const char ch : e->text) {
            if (ch < '0' || ch > '9') return false;
            const unsigned d = static_cast<unsigned>(ch - '0');
            if (n > (limit - d) / 10) return false;
            n = n * 10 + d;
        }
        *v = n;
        return true;
    };
    unsigned long long m = 0, r = 0;
    if (!unsignedInteger(method, 0xFFFFFFFFull, &m)) return XPE_ERR_CONFIG_INVALID;
    if (!unsignedInteger(raw, 0xFFFFFFFFFFFFFFFFull, &r)) return XPE_ERR_CONFIG_INVALID;
    out_method = static_cast<uint32_t>(m);
    out_raw_payload_len = static_cast<uint64_t>(r);
    is_compressed = true;
    return XPE_OK;
}

// Payload read granularity. 1 MiB keeps the file buffer and the hash input in
// cache while still amortising the read calls (QA-A-105).
static const size_t kReadChunkBytes = 1u << 20;

XpeErrorCode read_xcal_file(
    const char*            path,
    XCalFileHeader&        out_header,
    std::vector<uint8_t>&  out_config,
    std::vector<uint8_t>&  out_payload,
    bool                   check_expiry,
    int                    expected_type,
    XpeConfigDoc*          out_config_doc,
    XCalPayloadSink*       sink)
{
    try {
        if (sink != nullptr) sink->filled = false;
        if (path == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }

        // Open file
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) {
            return XPE_ERR_IO_FAILED;
        }

        // Read header (152 bytes, pack=1)
        XCalFileHeader hdr;
        f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
        if (!f.good() || f.gcount() != static_cast<std::streamsize>(sizeof(hdr))) {
            return XPE_ERR_IO_FAILED;
        }

        // Check for compressed payload before strict validation
        // Compressed payload_len will NOT match width*height*bpp
        bool is_compressed = false;
        uint32_t compression_method = XCAL_COMPRESSION_NONE;
        uint64_t raw_payload_len = 0;

        // Read config_json first (needed for compression metadata)
        std::vector<uint8_t> config;
        if (hdr.config_json_len > 0) {
            config.resize(static_cast<size_t>(hdr.config_json_len));
            f.read(reinterpret_cast<char*>(config.data()),
                   static_cast<std::streamsize>(hdr.config_json_len));
            if (!f.good() ||
                f.gcount() != static_cast<std::streamsize>(hdr.config_json_len)) {
                return XPE_ERR_IO_FAILED;
            }
        }

        // Check compression metadata
        // The block is parsed ONCE here (QA-A-209c); the document goes to the caller when it asked for it.
        XpeConfigDoc config_doc;
        bool legacy_repaired = false;
        {
            const XpeErrorCode prc = parse_config_block_with_legacy(config, hdr.type, &config_doc, &legacy_repaired);
            if (prc != XPE_OK) return prc;
            const XpeErrorCode mrc = read_compression_meta(
                config_doc,
                is_compressed,
                compression_method,
                raw_payload_len);
            if (mrc != XPE_OK) return mrc;
        }

        // Validate header (skip payload_len check for compressed data)
        // For compressed files, we need relaxed validation
        if (is_compressed) {
            // Manual validation without payload_len check
            if (std::memcmp(hdr.magic, XCAL_MAGIC, 4) != 0) {
                return XPE_ERR_CONFIG_INVALID;
            }
            if (hdr.version != XCAL_VERSION) {
                return XPE_ERR_CONFIG_INVALID;
            }
            // QA-A-37 (#140): same ceiling correction as validate_xcal_header
            // Check 3 -- the compressed path had its own copy of it.
            if (hdr.type > static_cast<uint32_t>(XCAL_TYPE_NONLIN_LUT)) {
                return XPE_ERR_CONFIG_INVALID;
            }
            if (hdr.pixel_format > static_cast<uint32_t>(XCAL_FMT_UINT8_MASK)) {
                return XPE_ERR_CONFIG_INVALID;
            }
            if (hdr.width == 0 || hdr.width > XCAL_MAX_DIM) {
                return XPE_ERR_CONFIG_INVALID;
            }
            if (hdr.height == 0 || hdr.height > XCAL_MAX_DIM) {
                return XPE_ERR_CONFIG_INVALID;
            }
            if (hdr.config_json_len > XCAL_MAX_CONFIG_JSON_LEN) {
                return XPE_ERR_CONFIG_INVALID;
            }
            if (expected_type >= 0 && hdr.type != static_cast<uint32_t>(expected_type)) {
                return XPE_ERR_CONFIG_INVALID;
            }
            // Verify raw_payload_len matches expected uncompressed size
            size_t bpp = xcal_bytes_per_pixel(hdr.pixel_format);
            if (bpp > 0) {
                uint64_t expected_raw = static_cast<uint64_t>(hdr.width) *
                                        static_cast<uint64_t>(hdr.height) *
                                        static_cast<uint64_t>(bpp);
                if (raw_payload_len != expected_raw) {
                    return XPE_ERR_CONFIG_INVALID;
                }
            }
        } else {
            // Standard validation (includes payload_len check)
            XpeErrorCode vrc = validate_xcal_header(hdr, expected_type);
            if (vrc != XPE_OK) {
                return vrc;
            }
        }

        // Read the payload and hash it as it arrives (QA-A-105, #179):
        // SRS-CALIB-PERF-003 asks for the integrity check to be "calculated
        // during read, not post-hoc". The digest is unchanged -- SHA-256 of
        // (config_json || payload) -- because the chunks are fed in file order.
        std::vector<uint8_t> payload;
        Sha256Stream hasher;
        if (!config.empty()) hasher.update(config.data(), config.size());
        // QA-A-235b: with a sink the payload is read into the caller's own (unpublished) buffer instead of a
        // zero-filled vector that the caller then copies. Only for an uncompressed, non-empty payload.
        uint8_t* direct = nullptr;
        if (sink != nullptr && sink->acquire != nullptr && !is_compressed && hdr.payload_len > 0) {
            direct = sink->acquire(sink->ctx, hdr, hdr.payload_len);   // nullptr = declined: the default path below
        }
        const size_t payload_bytes = static_cast<size_t>(hdr.payload_len);
        if (hdr.payload_len > 0) {
            uint8_t* dest = direct;
            if (dest == nullptr) {
                payload.resize(payload_bytes);
                dest = payload.data();
            }
            size_t done = 0;
            while (done < payload_bytes) {
                const size_t chunk = std::min(kReadChunkBytes, payload_bytes - done);
                f.read(reinterpret_cast<char*>(dest + done),
                       static_cast<std::streamsize>(chunk));
                if (!f.good() || f.gcount() != static_cast<std::streamsize>(chunk)) {
                    return XPE_ERR_IO_FAILED;
                }
                hasher.update(dest + done, chunk);
                done += chunk;
            }
        }

        if (std::memcmp(hasher.digest().data(), hdr.sha256, 32) != 0) {
            return XPE_ERR_CONFIG_INVALID;
        }

        // Decompress if needed
        if (is_compressed && compression_method == XCAL_COMPRESSION_RLE) {
            std::vector<uint8_t> decompressed;
            int rc = rle_decode(payload.data(), payload.size(),
                                static_cast<size_t>(raw_payload_len),
                                decompressed);
            if (rc != XPE_OK) {
                // an allocation failure inside the decoder is OUT_OF_MEMORY, not a bad file (QA-A-209d)
                return rc == XPE_ERR_OUT_OF_MEMORY ? XPE_ERR_OUT_OF_MEMORY : XPE_ERR_CONFIG_INVALID;
            }
            payload = std::move(decompressed);
        }

        // Check expiry (if requested and expiry is set)
        if (check_expiry && hdr.expiry_epoch_ms != 0) {
            const int64_t now_ms = xpe_calib_now_ms();   // the system clock; a test clock in the clock-test build (QA-A-244)
            if (now_ms > hdr.expiry_epoch_ms) {
                return XPE_ERR_CALIBRATION_EXPIRED;
            }
        }

        // The legacy warning's text is a std::string built from the path: it can throw bad_alloc. It is built HERE, before
        // any output argument is touched, so a failed allocation is an OUT_OF_MEMORY that leaves every output as the
        // caller gave it (QA-A-209d, Codex #56). The alert itself is pushed AFTER the outputs are committed -- the push
        // cannot throw -- and only now that the whole file, hash included, has been accepted: a refused file is not
        // reported as repaired. The text is a cross-lane contract (QA-A-209c): the GUI and tools may match on the prefix.
        std::string legacy_alert;
        if (legacy_repaired) {
            legacy_alert = std::string("XPE_WARN_XCAL_LEGACY_CONFIG: the config block of ") + path +
                           " has the doubled closing brace of an older XCal writer and was read after a "
                           "deterministic repair; regenerate the file with the current writer";
        }

        // Success: commit outputs (assignments and moves of trivially-copyable and vector members: none can throw)
        // For compressed files, update payload_len to reflect decompressed size
        out_header  = hdr;
        out_header.payload_len = (direct != nullptr) ? hdr.payload_len : payload.size();
        out_config  = std::move(config);
        out_payload = std::move(payload);   // empty when the payload went to the sink
        if (sink != nullptr) sink->filled = (direct != nullptr);
        if (out_config_doc != nullptr) *out_config_doc = std::move(config_doc);

        if (legacy_repaired) xpe_alert_push(legacy_alert.c_str(), XPE_ALERT_WARNING);
        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
