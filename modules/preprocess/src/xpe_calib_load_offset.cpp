/**
 * @file xpe_calib_load_offset.cpp
 * @brief xpe_calib_load_offset implementation (T-006)
 *
 * REQ-P1A-014: Load XCal v1 OFFSET calibration map.
 * REQ-P1A-030: No C++ exceptions across C ABI boundary.
 * REQ-P1A-031: RAII for automatic cleanup on error.
 *
 * @MX:ANCHOR: [AUTO] Public API entry point for offset calibration load
 * @MX:REASON: Called by xpe_offset_correct (fan_in >= 3); g_calib write path
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include "xcal_reader.hpp"

#include <mutex>
#include <cstring>
#include <vector>
#include <chrono>

XpeErrorCode xpe_calib_stage_offset(const char* filepath, StagedOffset* out) noexcept {
    try {
        if (filepath == nullptr || out == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }

        // Read and validate XCal v1 file (SHA-256 + magic + type + expiry)
        XCalFileHeader hdr;
        std::vector<uint8_t> config_json;
        std::vector<uint8_t> payload;

        // QA-A-235b: the payload is read straight into the staged map (no zero-filled vector, no second copy).
        // The staged map is local until the whole file has been accepted, so a refused file leaves the module-global
        // store as it was, as before. new[] rather than make_unique: the bytes are overwritten by the read.
        StagedOffset staged;
        XCalPayloadSink sink;
        sink.ctx = &staged;
        sink.acquire = [](void* ctx, const XCalFileHeader& h, uint64_t len) -> uint8_t* {
            const size_t pixels = static_cast<size_t>(h.width) * h.height;
            if (len != static_cast<uint64_t>(pixels) * sizeof(float)) return nullptr;
            auto* st = static_cast<StagedOffset*>(ctx);
            st->map.reset(new float[pixels]);
            return reinterpret_cast<uint8_t*>(st->map.get());
        };

        XpeErrorCode rc = read_xcal_file(
            filepath, hdr, config_json, payload,
            /*check_expiry=*/true,
            /*expected_type=*/XCAL_TYPE_OFFSET,
            /*out_config_doc=*/nullptr, &sink);
        if (rc != XPE_OK) {
            return rc;
        }

        if (!sink.filled) {
            // A compressed file: the decompressed bytes are in `payload`, copied into the map as before.
            size_t expected = static_cast<size_t>(hdr.width) * hdr.height * sizeof(float);
            if (payload.size() != expected) {
                return XPE_ERR_CONFIG_INVALID;
            }
            staged.map.reset(new float[static_cast<size_t>(hdr.width) * hdr.height]);
            std::memcpy(staged.map.get(), payload.data(), payload.size());
        }
        staged.width     = hdr.width;
        staged.height    = hdr.height;
        staged.timestamp = hdr.created_epoch_ms;
        staged.expiryMs  = hdr.expiry_epoch_ms;

        // Session id (null-terminated, up to 63 chars)
        std::memcpy(staged.sessionId, hdr.session_id,
                    sizeof(hdr.session_id) < sizeof(staged.sessionId)
                        ? sizeof(hdr.session_id)
                        : sizeof(staged.sessionId) - 1);

        *out = std::move(staged);
        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}

void xpe_calib_commit_offset_locked(StagedOffset& staged) noexcept {
    g_calib.offset_map       = std::move(staged.map);
    g_calib.offset_width     = staged.width;
    g_calib.offset_height    = staged.height;
    g_calib.offset_timestamp = staged.timestamp;
    g_calib.offset_expiry_ms = staged.expiryMs;
    std::memcpy(g_calib.offset_session_id, staged.sessionId, sizeof(g_calib.offset_session_id));
}

extern "C" XPE_API XpeErrorCode xpe_calib_load_offset(const char* filepath) {
    try {
        StagedOffset staged;
        const XpeErrorCode rc = xpe_calib_stage_offset(filepath, &staged);
        if (rc != XPE_OK) {
            return rc;
        }

        // Commit under mutex (read-then-commit; no TOCTOU exposure)
        bool warnSession = false;
        {
            std::lock_guard<std::mutex> lock(g_calib_mutex);
            // QA-A-229 M4: refused BEFORE anything changes -- the loaded maps stay, this one is rejected.
            const XpeErrorCode src = xpe_calib_session_check_locked(CalibMapKind::Offset, staged.sessionId, &warnSession);
            if (src != XPE_OK) return src;
            xpe_calib_commit_offset_locked(staged);
        }
        xpe_calib_session_warn(warnSession);
        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
