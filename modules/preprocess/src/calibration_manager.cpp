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
#include <chrono>
#include <limits>

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

// @MX:NOTE: [AUTO] Per-pixel mean across frameCount dark-field frames
// @MX:SPEC: REQ-P1A-038
XpeErrorCode xpe_calib_generate_offset(const XpeImageBuffer* frames,
                                        uint32_t frameCount,
                                        XpeImageBuffer* offsetMapOut,
                                        const char* configJsonOrNull)
{
    if (!frames || frameCount == 0 || !offsetMapOut) return XPE_ERR_INVALID_INPUT;
    (void)configJsonOrNull;
    size_t n = 0;
    if (!xpe_buffer_has_format(&frames[0], XPE_PIXEL_UINT16, &n)) return XPE_ERR_INVALID_INPUT;
    if (!xpe_buffer_has_format(offsetMapOut, XPE_PIXEL_UINT16)) return XPE_ERR_INVALID_INPUT;

    // REQ-P1A-038: compute per-pixel mean across frameCount uint16 dark frames
    auto* out = static_cast<uint16_t*>(offsetMapOut->data);

    for (size_t i = 0; i < n; ++i) {
        uint64_t sum = 0;
        for (uint32_t f = 0; f < frameCount; ++f) {
            if (!xpe_dims_match(&frames[0], &frames[f]) ||
                !xpe_buffer_has_format(&frames[f], XPE_PIXEL_UINT16)) {
                return XPE_ERR_INVALID_INPUT;
            }
            sum += static_cast<const uint16_t*>(frames[f].data)[i];
        }
        out[i] = static_cast<uint16_t>(sum / frameCount);
    }

    offsetMapOut->width = frames[0].width;
    offsetMapOut->height = frames[0].height;
    offsetMapOut->format = XPE_PIXEL_UINT16;
    offsetMapOut->bitsAllocated = 16;
    offsetMapOut->bitsStored = 16;
    offsetMapOut->dataSize = n * sizeof(uint16_t);
    return XPE_OK;
}

XpeErrorCode xpe_calib_check_expiry(const char* filePath,
                                     uint64_t* expiryEpochMsOut)
{
    if (!filePath || !expiryEpochMsOut) return XPE_ERR_INVALID_INPUT;

    FILE* f = xpe_fopen(filePath, "rb");
    if (!f) return XPE_ERR_IO_FAILED;

    CalibFileHeader hdr{};
    if (std::fread(&hdr, sizeof(hdr), 1, f) != 1) {
        std::fclose(f);
        return XPE_ERR_IO_FAILED;
    }
    std::fclose(f);

    *expiryEpochMsOut = hdr.expiryEpochMs;

    auto now_ms = static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
    return (hdr.expiryEpochMs < now_ms) ? XPE_ERR_CALIBRATION_EXPIRED : XPE_OK;
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
