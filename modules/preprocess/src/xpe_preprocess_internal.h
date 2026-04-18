/**
 * @file xpe_preprocess_internal.h
 * @brief Internal header for XPE Preprocessing Module
 *
 * This header contains shared structures and declarations used internally
 * by the preprocessing module implementation files.
 */

#ifndef XPE_PREPROCESS_INTERNAL_H
#define XPE_PREPROCESS_INTERNAL_H

#include <memory>
#include <mutex>
#include <cstdint>
#include <ctime>  // For std::time

/* XCal v1 canonical header is defined in xcal_format.h
 * Use XCalFileHeader (pack=1, 152 bytes) for file I/O.
 *
 * REQ-P1A-014: XCal format structure for calibration data
 */
#include "xpe/preprocess/xcal_format.h"

/* Legacy alias so existing code that references XCalHeader still compiles.
 * New code should use XCalFileHeader directly.
 */
typedef XCalFileHeader XCalHeader;

/**
 * @brief Global calibration data storage
 *
 * REQ-P1A-014~016: Thread-safe calibration storage
 * REQ-P1A-031: RAII for automatic cleanup
 *
 * @MX:ANCHOR: [AUTO] Global calibration data structure
 * Shared across all preprocessing algorithms for thread-safe access
 */
struct CalibrationData {
    // Offset calibration map
    std::unique_ptr<float[]> offset_map;
    uint32_t offset_width = 0;
    uint32_t offset_height = 0;

    // Gain calibration map
    std::unique_ptr<float[]> gain_map;
    uint32_t gain_width = 0;
    uint32_t gain_height = 0;

    // Defect map (BPM format)
    std::unique_ptr<uint8_t[]> defect_map;
    uint32_t defect_width = 0;
    uint32_t defect_height = 0;

    // Metadata
    char offset_session_id[64] = {0};
    char gain_session_id[64] = {0};
    int64_t offset_timestamp = 0;
    int64_t gain_timestamp = 0;
};

// Global instances (defined in xpe_calibration.cpp)
extern CalibrationData g_calib;
extern std::mutex g_calib_mutex;

/**
 * @brief Validate XCal file header
 *
 * @param header XCal header to validate
 * @param expected_type Expected calibration type (0=offset, 1=gain, 2=defect)
 * @return true if header is valid, false otherwise
 */
static bool validate_xcal_header(const XCalHeader& header, int32_t expected_type);

#endif /* XPE_PREPROCESS_INTERNAL_H */
