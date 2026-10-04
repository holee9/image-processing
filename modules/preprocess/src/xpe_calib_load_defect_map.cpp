/**
 * @file xpe_calib_load_defect_map.cpp
 * @brief xpe_calib_load_defect_map implementation (T-006)
 *
 * REQ-P1A-016: Load XCal v1 DEFECT calibration map (BPM format).
 * REQ-P1A-030: No C++ exceptions across C ABI boundary.
 * REQ-P1A-031: RAII for automatic cleanup on error.
 *
 * @MX:ANCHOR: [AUTO] Public API entry point for defect map load
 * @MX:REASON: Called by xpe_defect_correct (fan_in >= 3); g_calib write path
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include "xcal_reader.hpp"

#include <mutex>
#include <cstring>
#include <vector>

XpeErrorCode xpe_calib_stage_defect(const char* filepath, StagedDefect* out) noexcept {
    try {
        if (filepath == nullptr || out == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }

        // Read and validate XCal v1 file (SHA-256 + magic + type + expiry).
        // Defect BPM is a safety-critical calibration artifact, so it follows
        // the same expiry policy as offset/gain maps.
        XCalFileHeader hdr;
        std::vector<uint8_t> config_json;
        std::vector<uint8_t> payload;

        // QA-A-235b: the payload is read straight into the staged map (see xpe_calib_load_offset.cpp). The staged map
        // is local until the whole file has been accepted.
        StagedDefect staged;
        XCalPayloadSink sink;
        sink.ctx = &staged;
        sink.acquire = [](void* ctx, const XCalFileHeader& h, uint64_t len) -> uint8_t* {
            const size_t pixels = static_cast<size_t>(h.width) * h.height;
            if (len != static_cast<uint64_t>(pixels)) return nullptr;
            auto* st = static_cast<StagedDefect*>(ctx);
            st->map.reset(new uint8_t[pixels]);
            return st->map.get();
        };

        XpeErrorCode rc = read_xcal_file(
            filepath, hdr, config_json, payload,
            /*check_expiry=*/true,
            /*expected_type=*/XCAL_TYPE_DEFECT,
            /*out_config_doc=*/nullptr, &sink);
        if (rc != XPE_OK) {
            return rc;
        }

        if (!sink.filled) {
            // A compressed file: the decompressed bytes are in `payload`, copied into the map as before.
            size_t expected = static_cast<size_t>(hdr.width) * hdr.height;
            if (payload.size() != expected) {
                return XPE_ERR_CONFIG_INVALID;
            }
            staged.map.reset(new uint8_t[expected]);
            std::memcpy(staged.map.get(), payload.data(), payload.size());
        }
        staged.width    = hdr.width;
        staged.height   = hdr.height;
        staged.expiryMs = hdr.expiry_epoch_ms;
        std::memcpy(staged.sessionId, hdr.session_id,
                    sizeof(hdr.session_id) < sizeof(staged.sessionId) ? sizeof(hdr.session_id)
                                                                      : sizeof(staged.sessionId) - 1);

        *out = std::move(staged);
        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}

void xpe_calib_commit_defect_locked(StagedDefect& staged) noexcept {
    g_calib.defect_map       = std::move(staged.map);
    g_calib.defect_width     = staged.width;
    g_calib.defect_height    = staged.height;
    g_calib.defect_expiry_ms = staged.expiryMs;
    std::memcpy(g_calib.defect_session_id, staged.sessionId, sizeof(g_calib.defect_session_id));
}

extern "C" XPE_API XpeErrorCode xpe_calib_load_defect_map(const char* filepath) {
    try {
        StagedDefect staged;
        const XpeErrorCode rc = xpe_calib_stage_defect(filepath, &staged);
        if (rc != XPE_OK) {
            return rc;
        }

        // Commit under mutex
        bool warnSession = false;
        {
            std::lock_guard<std::mutex> lock(g_calib_mutex);
            // QA-A-229 M4: refused BEFORE anything changes -- the loaded maps stay, this one is rejected.
            const XpeErrorCode src = xpe_calib_session_check_locked(CalibMapKind::Defect, staged.sessionId, &warnSession);
            if (src != XPE_OK) return src;
            xpe_calib_commit_defect_locked(staged);
        }
        xpe_calib_session_warn(warnSession);
        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
