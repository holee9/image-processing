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
 *   2. Divide by num_frames to obtain mean.
 *   3. Replace any NaN or Inf result with 0.0f (REQ-P1A-033).
 *   4. Write result via xcal_writer (atomic, SHA-256).
 *
 * @MX:ANCHOR: [AUTO] xpe_calib_generate_offset – dark frame averaging
 * @MX:REASON: Critical offline calibration path; double accumulator + NaN guard mandatory
 * @MX:SPEC: REQ-P1A-017, REQ-P1A-033
 */

#include "xpe/preprocess_api.h"
#include "xpe_preprocess_internal.h"
#include "xcal_writer.hpp"

#include <cstring>
#include <cmath>    // std::isfinite
#include <memory>
#include <chrono>
#include <vector>
#include <cstdint>

extern "C" XPE_API XpeErrorCode xpe_calib_generate_offset(
    const XpeImageBuffer* dark_frames,
    int32_t               num_frames,
    float                 integration_time_ms,
    float                 temperature_c,
    const char*           output_path)
{
    (void)integration_time_ms;
    (void)temperature_c;

    try {
        // --- Input validation ---
        if (dark_frames == nullptr || output_path == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }
        if (num_frames <= 0) {
            return XPE_ERR_INVALID_INPUT;
        }

        // Dimensions from first frame
        uint32_t width  = dark_frames[0].width;
        uint32_t height = dark_frames[0].height;

        if (width == 0 || height == 0 ||
            width > XCAL_MAX_DIM || height > XCAL_MAX_DIM) {
            return XPE_ERR_INVALID_INPUT;
        }

        // --- Allocate double accumulator (REQ-P1A-033) ---
        size_t n_pixels = static_cast<size_t>(width) * height;
        auto accum = std::make_unique<double[]>(n_pixels);
        std::memset(accum.get(), 0, n_pixels * sizeof(double));

        // --- Accumulate ---
        for (int32_t i = 0; i < num_frames; ++i) {
            const XpeImageBuffer& frame = dark_frames[i];

            // Validate consistency
            if (frame.width != width || frame.height != height) {
                return XPE_ERR_INVALID_INPUT;
            }
            if (frame.data == nullptr) {
                return XPE_ERR_INVALID_INPUT;
            }
            if (frame.format != XPE_PIXEL_UINT16) {
                return XPE_ERR_UNSUPPORTED_FORMAT;
            }

            const uint16_t* src = static_cast<const uint16_t*>(frame.data);
            for (size_t j = 0; j < n_pixels; ++j) {
                accum[j] += static_cast<double>(src[j]);
            }
        }

        // --- Compute mean; apply NaN/Inf guard ---
        auto result = std::make_unique<float[]>(n_pixels);
        double inv_n = 1.0 / static_cast<double>(num_frames);
        for (size_t j = 0; j < n_pixels; ++j) {
            float v = static_cast<float>(accum[j] * inv_n);
            // REQ-P1A-033: replace non-finite values with 0.0f
            result[j] = std::isfinite(v) ? v : 0.0f;
        }

        // --- Build XCal v1 header ---
        using namespace std::chrono;
        int64_t now_ms = duration_cast<milliseconds>(
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
        hdr.expiry_epoch_ms  = 0;       // never expires
        hdr.config_json_len  = 0;
        hdr.payload_len      = static_cast<uint64_t>(n_pixels) * sizeof(float);

        // session_id: "generated" (null-padded)
        std::memcpy(hdr.session_id, "generated\0", 10);

        // --- Write via xcal_writer (atomic: write-to-tmp, rename, SHA-256) ---
        return write_xcal_file(
            output_path, hdr,
            nullptr, 0,
            reinterpret_cast<const uint8_t*>(result.get()),
            hdr.payload_len);

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
