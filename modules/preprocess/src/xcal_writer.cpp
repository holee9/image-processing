/**
 * @file xcal_writer.cpp
 * @brief XCal v1 file writer implementation (T-004)
 *
 * REQ-P1A-019, REQ-P1A-017, REQ-P1A-030
 *
 * Supports optional RLE compression for DEFECT (UINT8_MASK) payloads.
 * When compress_defect is true and type == XCAL_TYPE_DEFECT, the payload
 * is RLE-encoded and compression metadata is embedded in config_json.
 */

#include "xcal_writer.hpp"
#include "xpe_sha256.hpp"
#include "xcal_validator.hpp"
#include "xpe/common/xpe_error.h"
#include "rle_codec.hpp"

#include <cstring>
#include <fstream>
#include <string>
#include <cstdio>  // std::rename, std::remove

#ifdef _WIN32
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#endif

// @MX:NOTE: [AUTO] Atomic write pattern: write to <path>.tmp, then rename.
// This prevents corrupt partial files visible to concurrent readers.

// Move tmp onto path, replacing an existing file (QA-A-102, #169).
// SRS-CALIB-001 FUNC-033 (3) and the field recalibration flow overwrite an
// existing calibration file. std::rename replaces on POSIX but refuses an
// existing destination on Windows, which made every second write to the same
// path fail with XPE_ERR_IO_FAILED and keep the old file.
//
// QA-A-212b (#233): ON WINDOWS THE MOVE IS RETRIED, BRIEFLY, WHEN THE DESTINATION IS BUSY.
// QA-A-212 found why a calibration write failed now and then with XPE_ERR_IO_FAILED (-9) while nothing in this
// module was wrong: MoveFileEx answered ERROR_ACCESS_DENIED (5) because ANOTHER PROCESS -- a virus scanner, a
// search indexer, a file watcher -- had the destination open without FILE_SHARE_DELETE at that instant. The
// destination was not locked: opening it for DELETE or READ right after the failure worked, and the same call
// succeeded 1 ms later (13 of 13 events). A program with no calibration code in it failed the same way in the same
// folder at the same time; a program that opens the destination in a loop made 1565 of 3000 replaces fail with 5
// (opening the temp file instead gives 32, ERROR_SHARING_VIOLATION). Both codes mean "somebody else has it open
// right now", so both are retried; every other error is final at once.
//
// The budget is ONE constant. 100 ms is two orders above the 1 ms in which every observed event cleared, and
// below what a person waiting on a save notices. Sleeps double from 1 ms up to a cap of 25 ms, so the total slept
// stays within the budget. ERROR_ACCESS_DENIED is also what a READ-ONLY destination or a missing permission
// answers, and those never clear: they cost the full budget and then fail exactly as before -- an
// XPE_ERR_IO_FAILED, the temp file removed, the old file untouched.
//
// A failure after the retries is not silent any more (QA-A-212's second defect: the caller could not tell why):
// an XPE_ALERT_ERROR "XPE_WARN_XCAL_REPLACE_FAILED: ..." carries the path, the last Windows error and the number
// of retries. A write that succeeds after retrying raises nothing. POSIX is unchanged: std::rename replaces
// atomically and does not meet this.
constexpr unsigned kReplaceRetryBudgetMs = 100;   // QA-A-212: observed clearing time 1 ms; see above
constexpr unsigned kReplaceRetryMaxSleepMs = 25;

struct ReplaceOutcome {
    bool ok = false;
    unsigned long lastError = 0;   // Windows only: the last GetLastError() of a failed move
    unsigned retries = 0;          // Windows only: how many times the move was repeated
};

static ReplaceOutcome replace_file(const std::string& tmp, const char* path)
{
    ReplaceOutcome out;
#ifdef _WIN32
    unsigned slept = 0;
    unsigned delay = 1;
    for (;;) {
        if (MoveFileExA(tmp.c_str(), path, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0) {
            out.ok = true;
            return out;
        }
        out.lastError = GetLastError();
        const bool busy = (out.lastError == ERROR_ACCESS_DENIED || out.lastError == ERROR_SHARING_VIOLATION);
        if (!busy || slept >= kReplaceRetryBudgetMs) return out;
        if (delay > kReplaceRetryBudgetMs - slept) delay = kReplaceRetryBudgetMs - slept;
        Sleep(delay);
        slept += delay;
        ++out.retries;
        if (delay < kReplaceRetryMaxSleepMs) delay = (delay * 2 > kReplaceRetryMaxSleepMs) ? kReplaceRetryMaxSleepMs : delay * 2;
    }
#else
    out.ok = (std::rename(tmp.c_str(), path) == 0);
    return out;
#endif
}

// The alert for a move that failed for good (see above). Advisory: it must never turn the failure into another
// one, so an allocation failure while building it is swallowed.
static void report_replace_failure(const char* path, const ReplaceOutcome& o)
{
#ifdef _WIN32
    try {
        const bool busy = (o.lastError == ERROR_ACCESS_DENIED || o.lastError == ERROR_SHARING_VIOLATION);
        std::string msg = "XPE_WARN_XCAL_REPLACE_FAILED: could not replace '";
        msg += path;
        msg += "' with the newly written calibration file: Windows error ";
        msg += std::to_string(o.lastError);
        msg += " after ";
        msg += std::to_string(o.retries);
        msg += " retries (";
        msg += busy ? "the destination stayed open in another process, is read-only, or may not be changed"
                    : "not a transient condition, so it was not retried";
        msg += "). The previous file, if any, is unchanged and the temporary file was removed";
        xpe_alert_push(msg.c_str(), XPE_ALERT_ERROR);
    } catch (...) {
    }
#else
    (void)path; (void)o;
#endif
}

// Internal helper: build compression metadata JSON string.
// Returns empty string if no compression metadata is needed.
static std::string build_config_json(
    const uint8_t* caller_config,
    uint64_t caller_config_len,
    bool has_compression,
    uint32_t compression_method,
    uint64_t raw_payload_len)
{
    std::string result;

    if (has_compression) {
        // Build compression metadata JSON
        // Minimal: {"xcal_compression":1,"xcal_raw_payload_len":NNN}
        char meta[128];
        std::snprintf(meta, sizeof(meta),
            "{\"xcal_compression\":%u,\"xcal_raw_payload_len\":%llu}",
            static_cast<unsigned>(compression_method),
            static_cast<unsigned long long>(raw_payload_len));
        meta[sizeof(meta) - 1] = '\0';

        if (caller_config != nullptr && caller_config_len > 0) {
            // Merge: caller config is a JSON object; we inject our fields
            // Simple approach: strip trailing '}' from caller, prepend comma + meta
            std::string caller_str(reinterpret_cast<const char*>(caller_config),
                                   static_cast<size_t>(caller_config_len));
            // Find last '}'
            size_t last_brace = caller_str.rfind('}');
            if (last_brace != std::string::npos) {
                caller_str = caller_str.substr(0, last_brace);
            }
            // Our meta without leading '{'
            std::string meta_str(meta);
            size_t brace_pos = meta_str.find('{');
            if (brace_pos != std::string::npos) {
                meta_str = meta_str.substr(brace_pos + 1);
            }
            // The merged text ends with its own closing brace below, so the one meta carries is dropped: keeping
            // both made "...}}", which is not JSON (QA-A-209b; the old substring reader never noticed).
            if (!meta_str.empty() && meta_str.back() == '}') meta_str.pop_back();
            // "{}" (no members) must not become "{,...}": a comma only after an existing member
            size_t tail = caller_str.find_last_not_of(" \t\r\n");
            const bool has_members = tail != std::string::npos && caller_str[tail] != '{';
            result = caller_str + (has_members ? "," : "") + meta_str + "}";
        } else {
            result = meta;
        }
    } else {
        // No compression: pass through caller config as-is
        if (caller_config != nullptr && caller_config_len > 0) {
            result.assign(reinterpret_cast<const char*>(caller_config),
                          static_cast<size_t>(caller_config_len));
        }
    }

    return result;
}

XpeErrorCode write_xcal_file(
    const char* path,
    XCalFileHeader hdr_template,
    const uint8_t* config_json,
    uint64_t config_json_len,
    const uint8_t* payload,
    uint64_t payload_len)
{
    return write_xcal_file_ex(
        path, hdr_template,
        config_json, config_json_len,
        payload, payload_len,
        false);
}

XpeErrorCode write_xcal_file_ex(
    const char* path,
    XCalFileHeader hdr_template,
    const uint8_t* config_json,
    uint64_t config_json_len,
    const uint8_t* payload,
    uint64_t payload_len,
    bool compress_defect)
{
    try {
        // Input validation
        if (path == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }
        if (payload_len > 0 && payload == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }
        if (config_json_len > 0 && config_json == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }

        const uint8_t* final_payload    = payload;
        uint64_t       final_payload_len = payload_len;
        bool           has_compression   = false;
        uint64_t       raw_payload_len   = 0;

        // RLE compression for DEFECT maps
        std::vector<uint8_t> compressed_buf;
        if (compress_defect &&
            hdr_template.type == static_cast<uint32_t>(XCAL_TYPE_DEFECT) &&
            payload != nullptr &&
            payload_len > 0)
        {
            int rc = rle_encode(payload, static_cast<size_t>(payload_len),
                                compressed_buf);
            if (rc == XPE_OK && !compressed_buf.empty()) {
                // Only use compressed version if it's actually smaller
                if (compressed_buf.size() < payload_len) {
                    final_payload     = compressed_buf.data();
                    final_payload_len = static_cast<uint64_t>(compressed_buf.size());
                    has_compression   = true;
                    raw_payload_len   = payload_len;
                }
                // If compressed is larger, fall through to uncompressed
            }
            // If RLE encode fails, proceed with uncompressed data
        }

        // Build config JSON with compression metadata
        std::string effective_config = build_config_json(
            config_json, config_json_len,
            has_compression,
            XCAL_COMPRESSION_RLE,
            raw_payload_len);

        const uint8_t* final_config = effective_config.empty()
            ? nullptr
            : reinterpret_cast<const uint8_t*>(effective_config.data());
        uint64_t final_config_len = effective_config.size();

        // Populate header fields controlled by writer
        hdr_template.config_json_len = final_config_len;
        hdr_template.payload_len     = final_payload_len;

        // Compute SHA-256(config_json || payload)
        auto sha = compute_sha256_two_parts(
            final_config, static_cast<size_t>(final_config_len),
            final_payload, static_cast<size_t>(final_payload_len));
        std::memcpy(hdr_template.sha256, sha.data(), 32);

        // Build tmp path
        std::string tmp_path = std::string(path) + ".tmp";

        // Write to tmp file
        {
            std::ofstream f(tmp_path, std::ios::binary | std::ios::trunc);
            if (!f.is_open()) {
                return XPE_ERR_IO_FAILED;
            }

            // Write header (pack=1, 152 bytes)
            f.write(reinterpret_cast<const char*>(&hdr_template),
                    sizeof(XCalFileHeader));
            if (!f.good()) {
                return XPE_ERR_IO_FAILED;
            }

            // Write config_json (may be empty)
            if (final_config_len > 0) {
                f.write(reinterpret_cast<const char*>(final_config),
                        static_cast<std::streamsize>(final_config_len));
                if (!f.good()) {
                    return XPE_ERR_IO_FAILED;
                }
            }

            // Write payload
            if (final_payload_len > 0) {
                f.write(reinterpret_cast<const char*>(final_payload),
                        static_cast<std::streamsize>(final_payload_len));
                if (!f.good()) {
                    return XPE_ERR_IO_FAILED;
                }
            }

            f.flush();
            if (!f.good()) {
                return XPE_ERR_IO_FAILED;
            }
        }  // f is closed here

        // Atomic rename
        const ReplaceOutcome moved = replace_file(tmp_path, path);
        if (!moved.ok) {
            std::remove(tmp_path.c_str());
            report_replace_failure(path, moved);
            return XPE_ERR_IO_FAILED;
        }

        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
