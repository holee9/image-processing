/**
 * @file calibration_manager.cpp
 * @brief SWU-1.5: Calibration Manager — file I/O, CRC-32, expiry (SUP-01)
 *        REQ-P1A-035 to REQ-P1A-040
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <cstdint>

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

