/**
 * @file xpe_calib_generate_offset.cpp
 * @brief xpe_calib_generate_offset implementation (T-008)
 *
 * REQ-P1A-017: Generate offset map from dark frame array.
 * REQ-P1A-033: NaN/Inf guard -- finite values only in output map.
 * REQ-P1A-030: No C++ exceptions across C ABI boundary.
 *
 * Algorithm:
 *   1. Pixel-wise accumulation using double accumulators (REQ-P1A-033).
 *   2. Apply the selected generation method. The stable C ABI defaults to mean.
 *   3. Replace any NaN or Inf result with 0.0f (REQ-P1A-033).
 *   4. Write result via xcal_writer (atomic, SHA-256).
 *
 * @MX:ANCHOR: [AUTO] xpe_calib_generate_offset -- dark frame generation
 * @MX:REASON: Critical offline calibration path; double accumulator + NaN guard mandatory
 * @MX:SPEC: REQ-P1A-017, REQ-P1A-033
 */

#include "xpe/preprocess_api.h"
#include "xpe_calib_generate_offset_methods.hpp"
#include "xcal_writer.hpp"

#include <chrono>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>

extern "C" XPE_API XpeErrorCode xpe_calib_generate_offset(
    const XpeImageBuffer* dark_frames,
    int32_t               num_frames,
    float                 integration_time_ms,
    float                 temperature_c,
    const char*           output_path)
{
    try {
        if (output_path == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }

        xpe::preprocess::OffsetGenerationConfig config;
        XpeErrorCode rc = xpe::preprocess::parse_offset_generation_config(nullptr, &config);
        if (rc != XPE_OK) return rc;

        std::vector<float> result;
        uint32_t width = 0;
        uint32_t height = 0;
        rc = xpe::preprocess::generate_offset_values(dark_frames, num_frames, config,
                                                     &result, &width, &height);
        if (rc != XPE_OK) return rc;

        const size_t n_pixels = result.size();

        using namespace std::chrono;
        const int64_t now_ms = duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()).count();

        XCalFileHeader hdr;
        std::memset(&hdr, 0, sizeof(hdr));
        std::memcpy(hdr.magic, XCAL_MAGIC, 4);
        hdr.version          = XCAL_VERSION;
        hdr.type             = static_cast<uint32_t>(XCAL_TYPE_OFFSET);
        hdr.pixel_format     = static_cast<uint32_t>(XCAL_FMT_FLOAT32);
        hdr.width            = width;
        hdr.height           = height;
        hdr.created_epoch_ms = now_ms;
        hdr.expiry_epoch_ms  = 0;
        hdr.config_json_len  = 0;
        hdr.payload_len      = static_cast<uint64_t>(n_pixels) * sizeof(float);

        std::memcpy(hdr.session_id, "generated\0", 10);

        // 192 bytes: longest method name (sigma_clip=9) + two floats ≈ 90 chars.
        // If more parameters are serialised here in the future, increase this buffer
        // and add a static_assert or truncation guard.
        char config_json[192];
        std::snprintf(config_json, sizeof(config_json),
                      "{\"method\":\"%s\",\"integration_time_ms\":%.6g,\"temperature_c\":%.6g}",
                      xpe::preprocess::offset_generation_method_name(config.method),
                      static_cast<double>(integration_time_ms),
                      static_cast<double>(temperature_c));
        config_json[sizeof(config_json) - 1] = '\0';
        hdr.config_json_len = static_cast<uint64_t>(std::strlen(config_json));

        return write_xcal_file(
            output_path, hdr,
            reinterpret_cast<const uint8_t*>(config_json), hdr.config_json_len,
            reinterpret_cast<const uint8_t*>(result.data()),
            hdr.payload_len);

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
