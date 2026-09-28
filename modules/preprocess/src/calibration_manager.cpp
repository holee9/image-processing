/**
 * @file calibration_manager.cpp
 * @brief SWU-1.5: Calibration Manager — file I/O, CRC-32, expiry (SUP-01)
 *        REQ-P1A-014 to REQ-P1A-019
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <cstdint>

/* =========================================================================
 * CRC-32/ISO-HDLC WAS HERE, AND IS WITHDRAWN -- QA-A-152 (#216).
 *
 * `xpe_crc32` was an exported function with NO CALLER: searched clients/,
 * gui/, modules/, tools/ and tests/ (build outputs excluded), the only two
 * hits were its own declaration and definition. Control: the same search
 * finds xpe_preprocess_init 19 times in clients/ + gui/, so it was not blind.
 *
 * And the capability it provided is superseded. SRS-CALIB-001:43-44 records
 * the integrity value moving from "CRC-32 4바이트 (0x04C11DB7)" to
 * "SHA-256 32바이트", verified streaming while reading (QA-A-105) via CNG on
 * Windows (QA-A-106). Writing a requirement for this function would have
 * attached a contract to a retired mechanism, which is why the answer was to
 * withdraw the export rather than to specify it.
 *
 * The table and its initializer went with it: they had exactly one consumer,
 * and leaving them would trip /WX as an unreferenced static function.
 * ========================================================================= */

