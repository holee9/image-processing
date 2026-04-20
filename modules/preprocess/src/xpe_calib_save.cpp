/**
 * @file xpe_calib_save.cpp
 * @brief xpe_calib_save implementation (T-007)
 *
 * REQ-P1A-019: Save in-memory calibration state to XCal v1 file.
 * REQ-P1A-030: No C++ exceptions across C ABI boundary.
 *
 * Supports calib_type: "offset" | "gain" | "defect"
 * Uses xcal_writer for atomic write (write-to-tmp then rename).
 *
 * @MX:ANCHOR: [AUTO] xpe_calib_save – serialises g_calib to XCal v1 file
 * @MX:REASON: Reads all three g_calib sub-trees under mutex; atomic write via xcal_writer
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include "xcal_writer.hpp"

#include <mutex>
#include <cstring>
#include <memory>
#include <vector>
#include <chrono>

extern "C" XPE_API XpeErrorCode xpe_calib_save(const char* filepath,
                                               const char* calib_type) {
    try {
        if (filepath == nullptr || calib_type == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }

        // Snapshot calibration data under mutex to minimise lock duration
        std::unique_ptr<uint8_t[]> payload_copy;
        uint64_t payload_bytes = 0;
        XCalFileHeader hdr;
        std::memset(&hdr, 0, sizeof(hdr));

        {
            std::lock_guard<std::mutex> lock(g_calib_mutex);

            if (std::strcmp(calib_type, "offset") == 0) {
                if (!g_calib.offset_map) {
                    return XPE_ERR_INVALID_INPUT;
                }
                payload_bytes = static_cast<uint64_t>(g_calib.offset_width)
                                * g_calib.offset_height * sizeof(float);
                payload_copy = std::make_unique<uint8_t[]>(payload_bytes);
                std::memcpy(payload_copy.get(), g_calib.offset_map.get(), payload_bytes);

                std::memcpy(hdr.magic, XCAL_MAGIC, 4);
                hdr.version         = XCAL_VERSION;
                hdr.type            = static_cast<uint32_t>(XCAL_TYPE_OFFSET);
                hdr.pixel_format    = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
                hdr.width           = g_calib.offset_width;
                hdr.height          = g_calib.offset_height;
                hdr.created_epoch_ms = g_calib.offset_timestamp;
                hdr.expiry_epoch_ms  = 0;
                std::memcpy(hdr.session_id, g_calib.offset_session_id,
                            sizeof(g_calib.offset_session_id));

            } else if (std::strcmp(calib_type, "gain") == 0) {
                if (!g_calib.gain_map) {
                    return XPE_ERR_INVALID_INPUT;
                }
                payload_bytes = static_cast<uint64_t>(g_calib.gain_width)
                                * g_calib.gain_height * sizeof(float);
                payload_copy = std::make_unique<uint8_t[]>(payload_bytes);
                std::memcpy(payload_copy.get(), g_calib.gain_map.get(), payload_bytes);

                std::memcpy(hdr.magic, XCAL_MAGIC, 4);
                hdr.version         = XCAL_VERSION;
                hdr.type            = static_cast<uint32_t>(XCAL_TYPE_GAIN);
                hdr.pixel_format    = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
                hdr.width           = g_calib.gain_width;
                hdr.height          = g_calib.gain_height;
                hdr.created_epoch_ms = g_calib.gain_timestamp;
                hdr.expiry_epoch_ms  = 0;
                std::memcpy(hdr.session_id, g_calib.gain_session_id,
                            sizeof(g_calib.gain_session_id));

            } else if (std::strcmp(calib_type, "defect") == 0) {
                if (!g_calib.defect_map) {
                    return XPE_ERR_INVALID_INPUT;
                }
                payload_bytes = static_cast<uint64_t>(g_calib.defect_width)
                                * g_calib.defect_height;
                payload_copy = std::make_unique<uint8_t[]>(payload_bytes);
                std::memcpy(payload_copy.get(), g_calib.defect_map.get(), payload_bytes);

                std::memcpy(hdr.magic, XCAL_MAGIC, 4);
                hdr.version         = XCAL_VERSION;
                hdr.type            = static_cast<uint32_t>(XCAL_TYPE_DEFECT);
                hdr.pixel_format    = static_cast<uint32_t>(XCAL_FMT_UINT8_MASK);
                hdr.width           = g_calib.defect_width;
                hdr.height          = g_calib.defect_height;
                hdr.created_epoch_ms = 0;
                hdr.expiry_epoch_ms  = 0;
                std::memcpy(hdr.session_id, "defect_map\0", 11);

            } else {
                return XPE_ERR_INVALID_INPUT;
            }
        }  // lock released

        // Write via xcal_writer (atomic: write-to-tmp then rename, SHA-256 computed)
        return write_xcal_file(
            filepath, hdr,
            nullptr, 0,
            payload_copy.get(), payload_bytes);

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
