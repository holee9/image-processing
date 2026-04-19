/**
 * @file calibration_manager.cpp
 * @brief SWU-1.5: Calibration Manager — file I/O, CRC-32, expiry (SUP-01)
 *        REQ-P1A-035 to REQ-P1A-040
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <cstdio>
#include <ctime>
#include <cstring>
#include <cmath>
#include <chrono>
#include <limits>
#include <vector>

/* =========================================================================
 * CRC-32/ISO-HDLC implementation (polynomial 0xEDB88320)
 * ========================================================================= */
static uint32_t crc32_table[256] = {0};
static bool     crc32_table_init = false;

static FILE* xpe_fopen(const char* filePath, const char* mode) noexcept {
#if defined(_MSC_VER)
    FILE* file = nullptr;
    return (fopen_s(&file, filePath, mode) == 0) ? file : nullptr;
#else
    return std::fopen(filePath, mode);
#endif
}

static void init_crc32_table() {
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int j = 0; j < 8; ++j)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crc32_table[i] = c;
    }
    crc32_table_init = true;
}

uint32_t xpe_crc32(const uint8_t* data, size_t len) noexcept {
    if (!crc32_table_init) init_crc32_table();
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i)
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

/* =========================================================================
 * Calibration file load helper (shared by offset, gain, defect loaders)
 * ========================================================================= */
static XpeErrorCode calib_load_file(const char* filePath, XpeImageBuffer* mapOut) {
    if (!filePath || !mapOut) return XPE_ERR_INVALID_INPUT;

    FILE* f = xpe_fopen(filePath, "rb");
    if (!f) return XPE_ERR_IO_FAILED;

    CalibFileHeader hdr{};
    if (std::fread(&hdr, sizeof(hdr), 1, f) != 1) {
        std::fclose(f);
        return XPE_ERR_IO_FAILED;
    }
    if (hdr.magic[0] != 'X' || hdr.magic[1] != 'P' ||
        hdr.magic[2] != 'E' || hdr.magic[3] != 'C') {
        std::fclose(f);
        return XPE_ERR_IO_FAILED;
    }

    // REQ-P1A-037: check calibration expiry
    auto now_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    if (hdr.expiryEpochMs < now_ms) {
        std::fclose(f);
        return XPE_ERR_CALIBRATION_EXPIRED;
    }

    XpePixelFormat format = static_cast<XpePixelFormat>(hdr.pixelFormat);
    size_t pixelSize = 0;
    if (!xpe_pixel_size(format, &pixelSize)) {
        std::fclose(f);
        return XPE_ERR_UNSUPPORTED_FORMAT;
    }
    if (hdr.width == 0 || hdr.height == 0) {
        std::fclose(f);
        return XPE_ERR_IO_FAILED;
    }
    const size_t n = static_cast<size_t>(hdr.width) * hdr.height;
    if (n > std::numeric_limits<size_t>::max() / pixelSize) {
        std::fclose(f);
        return XPE_ERR_IO_FAILED;
    }
    const size_t payloadBytes = n * pixelSize;
    if (!mapOut->data || mapOut->dataSize < payloadBytes) {
        std::fclose(f);
        return XPE_ERR_BUFFER_TOO_SMALL;
    }

    // REQ-P1A-035: caller must have pre-allocated mapOut->data
    if (std::fread(mapOut->data, 1, payloadBytes, f) != payloadBytes) {
        std::fclose(f);
        return XPE_ERR_IO_FAILED;
    }
    std::fclose(f);

    // REQ-P1A-036: verify CRC-32 of pixel payload
    uint32_t crc = xpe_crc32(static_cast<const uint8_t*>(mapOut->data), payloadBytes);
    if (crc != hdr.payloadCrc32) return XPE_ERR_IO_FAILED;

    mapOut->width = hdr.width;
    mapOut->height = hdr.height;
    mapOut->format = format;
    mapOut->bitsAllocated = static_cast<uint32_t>(pixelSize * 8u);
    mapOut->bitsStored = mapOut->bitsAllocated;
    mapOut->dataSize = payloadBytes;
    return XPE_OK;
}

// @MX:ANCHOR: [AUTO] xpe_calib_load_offset — calibration pipeline entry
// @MX:REASON: Called by pipeline init and test harness; fan_in >= 3
// @MX:SPEC: REQ-P1A-035
XpeErrorCode xpe_calib_load_offset(const char* filePath,
                                    XpeImageBuffer* offsetMapOut)
{
    return calib_load_file(filePath, offsetMapOut);
}

XpeErrorCode xpe_calib_load_gain(const char* filePath,
                                  XpeImageBuffer* gainMapOut)
{
    return calib_load_file(filePath, gainMapOut);
}

XpeErrorCode xpe_calib_load_defect_map(const char* filePath,
                                        XpeImageBuffer* defectMapOut)
{
    return calib_load_file(filePath, defectMapOut);
}

/* =========================================================================
 * Offset Generation Method enum
 * ========================================================================= */
enum class OffsetMethod : int {
    Mean       = 0,  // Simple arithmetic mean (default, backward compatible)
    Median     = 1,  // Per-pixel median across frames
    SigmaClip  = 2,  // Iterative sigma clipping (remove outliers beyond N*sigma)
    Winsor     = 3   // Winsorization (clip extremes to percentiles, then mean)
};

/* =========================================================================
 * Internal helpers for multi-method offset generation
 * ========================================================================= */

/**
 * @brief Validate all frames have consistent dimensions and UINT16 format.
 * @return true if all frames are valid
 */
static bool validate_all_frames(const XpeImageBuffer* frames, uint32_t frameCount) noexcept {
    for (uint32_t f = 0; f < frameCount; ++f) {
        if (!xpe_dims_match(&frames[0], &frames[f])) return false;
        if (!xpe_buffer_has_format(&frames[f], XPE_PIXEL_UINT16)) return false;
    }
    return true;
}

/**
 * @brief Compute per-pixel arithmetic mean (method: "mean").
 *        Uses double accumulator for precision (IEC 62304 Class B safety).
 */
static void compute_mean_offset(const XpeImageBuffer* frames,
                                 uint32_t frameCount, size_t n,
                                 uint16_t* out) noexcept
{
    for (size_t i = 0; i < n; ++i) {
        double sum = 0.0;
        for (uint32_t f = 0; f < frameCount; ++f) {
            sum += static_cast<double>(
                static_cast<const uint16_t*>(frames[f].data)[i]);
        }
        double mean = sum / static_cast<double>(frameCount);
        // Clamp to uint16 range with NaN/Inf guard
        if (!std::isfinite(mean)) { out[i] = 0; continue; }
        if (mean < 0.0) mean = 0.0;
        if (mean > 65535.0) mean = 65535.0;
        out[i] = static_cast<uint16_t>(mean + 0.5);  // round to nearest
    }
}

/**
 * @brief Compute per-pixel median using insertion sort (method: "median").
 *        Robust to outliers -- O(frameCount^2) per pixel but frameCount is
 *        typically small (3-64 dark frames).
 */
static void compute_median_offset(const XpeImageBuffer* frames,
                                   uint32_t frameCount, size_t n,
                                   uint16_t* out) noexcept
{
    // Pre-allocate sorting buffer (avoids per-pixel allocation)
    std::vector<uint16_t> buf(static_cast<size_t>(frameCount));

    for (size_t i = 0; i < n; ++i) {
        // Collect pixel values across frames
        for (uint32_t f = 0; f < frameCount; ++f) {
            buf[f] = static_cast<const uint16_t*>(frames[f].data)[i];
        }

        // Insertion sort (efficient for small N)
        for (uint32_t j = 1; j < frameCount; ++j) {
            uint16_t key = buf[j];
            int32_t k = static_cast<int32_t>(j) - 1;
            while (k >= 0 && buf[k] > key) {
                buf[k + 1] = buf[k];
                --k;
            }
            buf[k + 1] = key;
        }

        // Select median
        if (frameCount % 2 == 1) {
            out[i] = buf[frameCount / 2];
        } else {
            // Average of two middle values
            uint32_t a = buf[frameCount / 2 - 1];
            uint32_t b = buf[frameCount / 2];
            out[i] = static_cast<uint16_t>((a + b + 1) / 2);  // round up
        }
    }
}

/**
 * @brief Compute per-pixel sigma-clipped mean (method: "sigma_clip").
 *        Iteratively removes pixels beyond sigmaThreshold * stddev from mean.
 * @param sigmaThreshold  Number of standard deviations for clipping (default: 3.0)
 * @param maxIter         Maximum iterations (default: 5)
 */
static void compute_sigma_clip_offset(const XpeImageBuffer* frames,
                                       uint32_t frameCount, size_t n,
                                       double sigmaThreshold, uint32_t maxIter,
                                       uint16_t* out) noexcept
{
    // Pre-allocate buffers
    std::vector<double> values(static_cast<size_t>(frameCount));
    std::vector<double> filtered(static_cast<size_t>(frameCount));

    for (size_t i = 0; i < n; ++i) {
        // Collect pixel values as doubles
        size_t count = frameCount;
        for (uint32_t f = 0; f < frameCount; ++f) {
            values[f] = static_cast<double>(
                static_cast<const uint16_t*>(frames[f].data)[i]);
        }

        for (uint32_t iter = 0; iter < maxIter; ++iter) {
            // Compute mean
            double sum = 0.0;
            for (size_t j = 0; j < count; ++j) sum += values[j];
            double mean = sum / static_cast<double>(count);

            // Compute standard deviation
            double sqSum = 0.0;
            for (size_t j = 0; j < count; ++j) {
                double diff = values[j] - mean;
                sqSum += diff * diff;
            }
            double stddev = std::sqrt(sqSum / static_cast<double>(count));

            // If stddev is zero (all values identical), no clipping needed
            if (stddev < 1e-10) break;

            // Filter: keep values within sigmaThreshold * stddev
            double lo = mean - sigmaThreshold * stddev;
            double hi = mean + sigmaThreshold * stddev;

            size_t newCount = 0;
            for (size_t j = 0; j < count; ++j) {
                if (values[j] >= lo && values[j] <= hi) {
                    filtered[newCount++] = values[j];
                }
            }

            // If no values were removed, converged
            if (newCount == count) break;

            // If all values removed, keep last mean (safety: never empty)
            if (newCount == 0) break;

            // Swap buffers for next iteration
            for (size_t j = 0; j < newCount; ++j) values[j] = filtered[j];
            count = newCount;
        }

        // Final mean from remaining values
        double sum = 0.0;
        for (size_t j = 0; j < count; ++j) sum += values[j];
        double finalMean = sum / static_cast<double>(count);

        if (!std::isfinite(finalMean)) { out[i] = 0; continue; }
        if (finalMean < 0.0) finalMean = 0.0;
        if (finalMean > 65535.0) finalMean = 65535.0;
        out[i] = static_cast<uint16_t>(finalMean + 0.5);
    }
}

/**
 * @brief Compute per-pixel Winsorized mean (method: "winsor").
 *        Clips extreme values to percentile boundaries, then takes mean.
 * @param lowerPercentile  Lower percentile (0-100, default: 5)
 * @param upperPercentile  Upper percentile (0-100, default: 95)
 */
static void compute_winsor_offset(const XpeImageBuffer* frames,
                                   uint32_t frameCount, size_t n,
                                   double lowerPercentile, double upperPercentile,
                                   uint16_t* out) noexcept
{
    // Pre-allocate sorting buffer
    std::vector<uint16_t> buf(static_cast<size_t>(frameCount));

    for (size_t i = 0; i < n; ++i) {
        // Collect and sort pixel values
        for (uint32_t f = 0; f < frameCount; ++f) {
            buf[f] = static_cast<const uint16_t*>(frames[f].data)[i];
        }

        // Insertion sort
        for (uint32_t j = 1; j < frameCount; ++j) {
            uint16_t key = buf[j];
            int32_t k = static_cast<int32_t>(j) - 1;
            while (k >= 0 && buf[k] > key) {
                buf[k + 1] = buf[k];
                --k;
            }
            buf[k + 1] = key;
        }

        // Compute percentile indices (nearest-rank method)
        size_t loIdx = static_cast<size_t>(
            (lowerPercentile / 100.0) * static_cast<double>(frameCount - 1) + 0.5);
        size_t hiIdx = static_cast<size_t>(
            (upperPercentile / 100.0) * static_cast<double>(frameCount - 1) + 0.5);

        // Clamp indices
        if (loIdx >= frameCount) loIdx = 0;
        if (hiIdx >= frameCount) hiIdx = frameCount - 1;
        if (loIdx > hiIdx) loIdx = hiIdx;

        uint16_t loVal = buf[loIdx];
        uint16_t hiVal = buf[hiIdx];

        // Winsorize: clip values below loVal to loVal, above hiVal to hiVal
        double sum = 0.0;
        for (uint32_t f = 0; f < frameCount; ++f) {
            uint16_t v = static_cast<const uint16_t*>(frames[f].data)[i];
            if (v < loVal) v = loVal;
            if (v > hiVal) v = hiVal;
            sum += static_cast<double>(v);
        }

        double mean = sum / static_cast<double>(frameCount);
        if (!std::isfinite(mean)) { out[i] = 0; continue; }
        if (mean < 0.0) mean = 0.0;
        if (mean > 65535.0) mean = 65535.0;
        out[i] = static_cast<uint16_t>(mean + 0.5);
    }
}

// @MX:NOTE: [AUTO] Multi-method offset generation: mean/median/sigma_clip/winsor
// @MX:SPEC: REQ-P1A-038
// @MX:ANCHOR: [AUTO] xpe_calib_generate_offset -- dark frame offset generation
// @MX:REASON: Critical calibration path; method selection via configJsonOrNull
XpeErrorCode xpe_calib_generate_offset(const XpeImageBuffer* frames,
                                        uint32_t frameCount,
                                        XpeImageBuffer* offsetMapOut,
                                        const char* configJsonOrNull)
{
    // --- Input validation ---
    if (!frames || frameCount == 0 || !offsetMapOut) return XPE_ERR_INVALID_INPUT;

    size_t n = 0;
    if (!xpe_buffer_has_format(&frames[0], XPE_PIXEL_UINT16, &n)) return XPE_ERR_INVALID_INPUT;
    if (!xpe_buffer_has_format(offsetMapOut, XPE_PIXEL_UINT16)) return XPE_ERR_INVALID_INPUT;

    // Validate all frames up front (fail fast)
    if (!validate_all_frames(frames, frameCount)) return XPE_ERR_INVALID_INPUT;

    // --- Parse method from config JSON ---
    OffsetMethod method = OffsetMethod::Mean;  // default: backward compatible

    if (configJsonOrNull) {
        std::string methodStr = xpe_json_get_string(configJsonOrNull, "method");

        if (methodStr == "median") {
            method = OffsetMethod::Median;
        } else if (methodStr == "sigma_clip") {
            method = OffsetMethod::SigmaClip;
        } else if (methodStr == "winsor") {
            method = OffsetMethod::Winsor;
        } else if (!methodStr.empty() && methodStr != "mean") {
            // Unknown method string
            return XPE_ERR_CONFIG_INVALID;
        }
        // "mean" or empty string -> Mean (default)
    }

    // --- Dispatch to selected algorithm ---
    auto* out = static_cast<uint16_t*>(offsetMapOut->data);

    switch (method) {
    case OffsetMethod::Mean:
        compute_mean_offset(frames, frameCount, n, out);
        break;

    case OffsetMethod::Median:
        compute_median_offset(frames, frameCount, n, out);
        break;

    case OffsetMethod::SigmaClip: {
        double sigma = xpe_json_get_double(configJsonOrNull, "sigma", 3.0);
        double maxIterD = xpe_json_get_double(configJsonOrNull, "max_iter", 5.0);
        // Clamp to safe range
        if (sigma < 1.0) sigma = 1.0;
        if (sigma > 10.0) sigma = 10.0;
        uint32_t maxIter = static_cast<uint32_t>(maxIterD);
        if (maxIter < 1) maxIter = 1;
        if (maxIter > 20) maxIter = 20;
        compute_sigma_clip_offset(frames, frameCount, n, sigma, maxIter, out);
        break;
    }

    case OffsetMethod::Winsor: {
        double lowerPct = xpe_json_get_double(configJsonOrNull, "lower_percentile", 5.0);
        double upperPct = xpe_json_get_double(configJsonOrNull, "upper_percentile", 95.0);
        // Clamp to safe range
        if (lowerPct < 0.0) lowerPct = 0.0;
        if (lowerPct > 49.0) lowerPct = 49.0;
        if (upperPct < 51.0) upperPct = 51.0;
        if (upperPct > 100.0) upperPct = 100.0;
        compute_winsor_offset(frames, frameCount, n, lowerPct, upperPct, out);
        break;
    }
    }

    // --- Populate output metadata ---
    offsetMapOut->width = frames[0].width;
    offsetMapOut->height = frames[0].height;
    offsetMapOut->format = XPE_PIXEL_UINT16;
    offsetMapOut->bitsAllocated = 16;
    offsetMapOut->bitsStored = 16;
    offsetMapOut->dataSize = n * sizeof(uint16_t);
    return XPE_OK;
}

XpeErrorCode xpe_calib_save(const XpeImageBuffer* calibMap,
                             const char* filePath,
                             uint64_t expiryEpochMs,
                             const char* configJsonOrNull)
{
    if (!calibMap || !filePath) return XPE_ERR_INVALID_INPUT;
    (void)configJsonOrNull;

    FILE* f = xpe_fopen(filePath, "wb");
    if (!f) return XPE_ERR_IO_FAILED;

    size_t pixelSize = 0;
    if (!xpe_pixel_size(calibMap->format, &pixelSize)) {
        std::fclose(f);
        return XPE_ERR_UNSUPPORTED_FORMAT;
    }
    size_t n = 0;
    if (!xpe_pixel_count(calibMap, &n)) {
        std::fclose(f);
        return XPE_ERR_INVALID_INPUT;
    }
    if (n > std::numeric_limits<size_t>::max() / pixelSize) {
        std::fclose(f);
        return XPE_ERR_INVALID_INPUT;
    }
    const size_t payloadBytes = n * pixelSize;
    if (!calibMap->data || calibMap->dataSize < payloadBytes) {
        std::fclose(f);
        return XPE_ERR_INVALID_INPUT;
    }

    CalibFileHeader hdr{};
    hdr.magic[0] = 'X'; hdr.magic[1] = 'P';
    hdr.magic[2] = 'E'; hdr.magic[3] = 'C';
    hdr.version = 1;
    hdr.width = calibMap->width;
    hdr.height = calibMap->height;
    hdr.pixelFormat = static_cast<uint32_t>(calibMap->format);
    hdr.expiryEpochMs = expiryEpochMs;
    hdr.payloadCrc32 = xpe_crc32(
        static_cast<const uint8_t*>(calibMap->data), payloadBytes);

    if (std::fwrite(&hdr, sizeof(hdr), 1, f) != 1 ||
        std::fwrite(calibMap->data, 1, payloadBytes, f) != payloadBytes) {
        std::fclose(f);
        return XPE_ERR_IO_FAILED;
    }
    std::fclose(f);
    return XPE_OK;
}
