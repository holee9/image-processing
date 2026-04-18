/**
 * @file xpe_calib_check_expiry.cpp
 * @brief xpe_calib_check_expiry implementation (T-009)
 *
 * REQ-P1A-018: Check calibration expiry from XCal v1 header only (header-read-only).
 * REQ-P1A-030: No C++ exceptions across C ABI boundary.
 *
 * Reads only the 152-byte XCal v1 header (no payload I/O).
 * Validates magic signature before inspecting expiry_epoch_ms.
 * expiry_epoch_ms == 0 means never expires.
 *
 * @MX:ANCHOR: [AUTO] xpe_calib_check_expiry – XCal v1 header-only expiry check
 * @MX:REASON: Header-only I/O is intentional (no payload read for performance);
 *             SHA-256 is NOT checked here -- use xpe_calib_load_* for full validation.
 * @MX:SPEC: REQ-P1A-018
 */

#include "xpe/preprocess_api.h"
#include "xpe_preprocess_internal.h"

#include <fstream>
#include <cstring>
#include <chrono>
#include <climits>

extern "C" XPE_API XpeErrorCode xpe_calib_check_expiry(const char* filepath,
                                                       bool*       is_expired,
                                                       int32_t*    remaining_days) {
    try {
        if (filepath == nullptr || is_expired == nullptr || remaining_days == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }

        // Open and read only the 152-byte header
        std::ifstream f(filepath, std::ios::binary);
        if (!f.is_open()) {
            return XPE_ERR_IO_FAILED;
        }

        XCalFileHeader hdr;
        f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
        if (!f.good() || f.gcount() != static_cast<std::streamsize>(sizeof(hdr))) {
            return XPE_ERR_IO_FAILED;
        }

        // Validate magic (quick sanity; full SHA-256 left to load functions)
        if (std::memcmp(hdr.magic, XCAL_MAGIC, 4) != 0) {
            return XPE_ERR_CONFIG_INVALID;
        }

        // Validate version
        if (hdr.version != XCAL_VERSION) {
            return XPE_ERR_CONFIG_INVALID;
        }

        // Determine expiry
        if (hdr.expiry_epoch_ms == 0) {
            // Never expires
            *is_expired    = false;
            *remaining_days = INT32_MAX;
            return XPE_OK;
        }

        using namespace std::chrono;
        int64_t now_ms = duration_cast<milliseconds>(
            system_clock::now().time_since_epoch()).count();

        int64_t remaining_ms = hdr.expiry_epoch_ms - now_ms;
        *is_expired     = (remaining_ms <= 0);
        // Truncate to days; clamp to INT32 range to avoid overflow
        int64_t rem_days_64 = remaining_ms / (24LL * 3600LL * 1000LL);
        if (rem_days_64 > static_cast<int64_t>(INT32_MAX)) {
            *remaining_days = INT32_MAX;
        } else if (rem_days_64 < static_cast<int64_t>(INT32_MIN)) {
            *remaining_days = INT32_MIN;
        } else {
            *remaining_days = static_cast<int32_t>(rem_days_64);
        }

        return XPE_OK;

    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
