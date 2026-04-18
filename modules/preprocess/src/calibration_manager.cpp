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

/* =========================================================================
 * CRC-32/ISO-HDLC implementation (polynomial 0xEDB88320)
 * ========================================================================= */
static uint32_t crc32_table[256] = {0};
static bool     crc32_table_init = false;

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

    FILE* f = std::fopen(filePath, "rb");
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

    const size_t n = static_cast<size_t>(hdr.width) * hdr.height;
    const size_t pixelSize = (hdr.pixelFormat == XPE_PIXEL_FORMAT_FLOAT32) ? 4u : 2u;
    const size_t payloadBytes = n * pixelSize;

    // REQ-P1A-035: caller must have pre-allocated mapOut->pixels
    if (std::fread(mapOut->pixels, 1, payloadBytes, f) != payloadBytes) {
        std::fclose(f);
        return XPE_ERR_IO_FAILED;
    }
    std::fclose(f);

    // REQ-P1A-036: verify CRC-32 of pixel payload
    uint32_t crc = xpe_crc32(static_cast<const uint8_t*>(mapOut->pixels), payloadBytes);
    if (crc != hdr.payloadCrc32) return XPE_ERR_IO_FAILED;

    mapOut->width = hdr.width;
    mapOut->height = hdr.height;
    mapOut->pixelFormat = static_cast<XpePixelFormat>(hdr.pixelFormat);
    mapOut->stride = hdr.width * static_cast<uint32_t>(pixelSize);
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

    // REQ-P1A-038: compute per-pixel mean across frameCount uint16 dark frames
    const size_t n = static_cast<size_t>(frames[0].width) * frames[0].height;
    auto* out = static_cast<uint16_t*>(offsetMapOut->pixels);

    for (size_t i = 0; i < n; ++i) {
        uint64_t sum = 0;
        for (uint32_t f = 0; f < frameCount; ++f)
            sum += static_cast<const uint16_t*>(frames[f].pixels)[i];
        out[i] = static_cast<uint16_t>(sum / frameCount);
    }

    offsetMapOut->width = frames[0].width;
    offsetMapOut->height = frames[0].height;
    offsetMapOut->pixelFormat = XPE_PIXEL_FORMAT_UINT16;
    offsetMapOut->stride = frames[0].width * static_cast<uint32_t>(sizeof(uint16_t));
    return XPE_OK;
}

XpeErrorCode xpe_calib_check_expiry(const char* filePath,
                                     uint64_t* expiryEpochMsOut)
{
    if (!filePath || !expiryEpochMsOut) return XPE_ERR_INVALID_INPUT;

    FILE* f = std::fopen(filePath, "rb");
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

    FILE* f = std::fopen(filePath, "wb");
    if (!f) return XPE_ERR_IO_FAILED;

    const size_t n = static_cast<size_t>(calibMap->width) * calibMap->height;
    const size_t pixelSize = (calibMap->pixelFormat == XPE_PIXEL_FORMAT_FLOAT32) ? 4u : 2u;
    const size_t payloadBytes = n * pixelSize;

    CalibFileHeader hdr{};
    hdr.magic[0] = 'X'; hdr.magic[1] = 'P';
    hdr.magic[2] = 'E'; hdr.magic[3] = 'C';
    hdr.version = 1;
    hdr.width = calibMap->width;
    hdr.height = calibMap->height;
    hdr.pixelFormat = static_cast<uint32_t>(calibMap->pixelFormat);
    hdr.expiryEpochMs = expiryEpochMs;
    hdr.payloadCrc32 = xpe_crc32(
        static_cast<const uint8_t*>(calibMap->pixels), payloadBytes);

    if (std::fwrite(&hdr, sizeof(hdr), 1, f) != 1 ||
        std::fwrite(calibMap->pixels, 1, payloadBytes, f) != payloadBytes) {
        std::fclose(f);
        return XPE_ERR_IO_FAILED;
    }
    std::fclose(f);
    return XPE_OK;
}
