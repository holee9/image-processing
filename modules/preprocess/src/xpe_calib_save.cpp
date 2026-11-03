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
#include "xcal_reader.hpp"
#include "rle_codec.hpp"
#include "defect_ext.hpp"

#include <algorithm>
#include <mutex>
#include <cstring>
#include <memory>
#include <vector>
#include <chrono>
#include <string>
#include <limits>

extern "C" XPE_API XpeErrorCode xpe_calib_save(const char* filepath,
                                               const char* calib_type,
                                               uint64_t    expiry_epoch_ms) {
    try {
        if (filepath == nullptr || calib_type == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }

        // Snapshot calibration data under mutex to minimise lock duration
        std::unique_ptr<uint8_t[]> payload_copy;
        std::shared_ptr<const std::vector<uint8_t>> config;
        std::shared_ptr<const DefectLayers> defectLayers;
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
                config = g_calib.offset_config;

                std::memcpy(hdr.magic, XCAL_MAGIC, 4);
                hdr.version         = XCAL_VERSION;
                hdr.type            = static_cast<uint32_t>(XCAL_TYPE_OFFSET);
                hdr.pixel_format    = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
                hdr.width           = g_calib.offset_width;
                hdr.height          = g_calib.offset_height;
                hdr.created_epoch_ms = g_calib.offset_timestamp;
                hdr.expiry_epoch_ms  = static_cast<int64_t>(expiry_epoch_ms);
                std::memcpy(hdr.session_id, g_calib.offset_session_id,
                            sizeof(g_calib.offset_session_id));

            } else if (std::strcmp(calib_type, "gain") == 0) {
                const bool polynomial = !g_calib.gain_map && g_calib.gain_poly_coeffs && g_calib.gain_poly_num_coeffs != 0;
                const float* gain = polynomial ? g_calib.gain_poly_coeffs.get() : g_calib.gain_map.get();
                if (gain == nullptr) {
                    return XPE_ERR_INVALID_INPUT;
                }
                const uint64_t pixels = static_cast<uint64_t>(g_calib.gain_width) * g_calib.gain_height;
                const uint64_t coefficients = polynomial ? g_calib.gain_poly_num_coeffs : 1u;
                if (pixels == 0) return XPE_ERR_INVALID_CALIB_DATA;
                if (pixels > static_cast<uint64_t>(std::numeric_limits<size_t>::max()) / sizeof(float) / coefficients)
                    return XPE_ERR_OUT_OF_MEMORY;
                payload_bytes = pixels * coefficients * sizeof(float);
                payload_copy = std::make_unique<uint8_t[]>(static_cast<size_t>(payload_bytes));
                std::memcpy(payload_copy.get(), gain, static_cast<size_t>(payload_bytes));
                config = g_calib.gain_config;
                // The applied map uses 1.0 at classified defects. Serialize those indices with the existing invalid
                // scalar value 0 so the existing loader restores BOTH the neutral map and its defect classification.
                if (!polynomial) {
                    if (g_calib.gain_defect_count != 0 && !g_calib.gain_defect_idx) return XPE_ERR_PROCESSING_FAILED;
                    for (uint32_t i = 0; i < g_calib.gain_defect_count; ++i) {
                        const uint32_t index = g_calib.gain_defect_idx[i];
                        if (index >= payload_bytes / sizeof(float)) return XPE_ERR_PROCESSING_FAILED;
                        const float marker = 0.0f;
                        std::memcpy(payload_copy.get() + static_cast<size_t>(index) * sizeof(float), &marker, sizeof(marker));
                    }
                }

                std::memcpy(hdr.magic, XCAL_MAGIC, 4);
                hdr.version         = XCAL_VERSION;
                hdr.type            = static_cast<uint32_t>(polynomial ? XCAL_TYPE_GAIN_POLY : XCAL_TYPE_GAIN);
                hdr.pixel_format    = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
                hdr.width           = g_calib.gain_width;
                hdr.height          = g_calib.gain_height;
                hdr.created_epoch_ms = g_calib.gain_timestamp;
                hdr.expiry_epoch_ms  = static_cast<int64_t>(expiry_epoch_ms);
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
                defectLayers = g_calib.defect_layers;

                std::memcpy(hdr.magic, XCAL_MAGIC, 4);
                hdr.version         = XCAL_VERSION;
                hdr.type            = static_cast<uint32_t>(XCAL_TYPE_DEFECT);
                hdr.pixel_format    = static_cast<uint32_t>(XCAL_FMT_UINT8_MASK);
                hdr.width           = g_calib.defect_width;
                hdr.height          = g_calib.defect_height;
                hdr.created_epoch_ms = 0;
                hdr.expiry_epoch_ms  = static_cast<int64_t>(expiry_epoch_ms);
                std::memcpy(hdr.session_id, g_calib.defect_session_id,
                            sizeof(hdr.session_id));

            } else {
                return XPE_ERR_INVALID_INPUT;
            }
        }  // lock released

        if (defectLayers) {
            // Preserve the CURRENT runtime layers using the existing v2 writer. Diagnostic candidates and original
            // file config/provenance were not retained by the runtime store and are not reconstructed here.
            DefectExt extension;
            extension.lines = defectLayers->lines;
            extension.blobs = defectLayers->blobs;
            DefectXcalWrite job;
            job.width = hdr.width;
            job.height = hdr.height;
            job.map = payload_copy.get();
            job.area = defectLayers->area.empty() ? nullptr : defectLayers->area.data();
            job.ext = &extension;
            job.created_epoch_ms = hdr.created_epoch_ms;
            job.expiry_epoch_ms = hdr.expiry_epoch_ms;
            job.session_id.assign(hdr.session_id,
                std::find(hdr.session_id, hdr.session_id + sizeof(hdr.session_id), '\0'));
            return write_defect_xcal(filepath, job);
        }

        // Config bytes may include a payload codec. Preserve both the opaque metadata AND its storage meaning:
        // writing decoded floats under an unchanged RLE marker would create a file the reader rejects.
        std::vector<uint8_t> encoded;
        const uint8_t* storedPayload = payload_copy.get();
        uint64_t storedBytes = payload_bytes;
        if (config && !config->empty()) {
            XpeConfigDoc doc;
            const XpeErrorCode parseRc = xpe_config_parse_block(
                reinterpret_cast<const char*>(config->data()), config->size(), &doc);
            if (parseRc != XPE_OK) return parseRc;
            XCalCompressionMeta meta;
            const XpeErrorCode metaRc = xcal_read_compression_meta(doc, meta);
            if (metaRc != XPE_OK) return metaRc;
            if (meta.is_compressed && meta.method == XCAL_COMPRESSION_RLE) {
                if (meta.raw_payload_len != payload_bytes) return XPE_ERR_CONFIG_INVALID;
                const int codecRc = rle_encode(payload_copy.get(), static_cast<size_t>(payload_bytes), encoded);
                if (codecRc != XPE_OK) return static_cast<XpeErrorCode>(codecRc);
                storedPayload = encoded.data();
                storedBytes = encoded.size();
            }
        }

        // Write via xcal_writer (atomic: write-to-tmp then rename, SHA-256 computed)
        return write_xcal_file(
            filepath, hdr,
            config && !config->empty() ? config->data() : nullptr, config ? config->size() : 0,
            storedPayload, storedBytes);

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
