/**
 * @file xpe_calib_load_nonlin_lut.cpp
 * @brief xpe_calib_load_nonlin_lut -- SRS-CALIB-FUNC-006-EXT 6a, QA-A-111 (#186)
 *
 * Loads an XCAL_TYPE_NONLIN_LUT file into the global calibration store, the same
 * way xpe_calib_load_gain / _offset / _defect_map do: read + verify the file,
 * check what the requirement asks of the CONTENT, then commit under the mutex.
 *
 * What is checked here, and why at load rather than at correction time:
 *
 *   - entry count is 4096 or 65536 ("LUT size: 4096 entries ... or 65536")
 *   - the table is non-decreasing ("Monotonicity check: LUT[i] <= LUT[i+1] for
 *     all i (enforced; non-monotone LUT = XPE_ERR_INVALID_CALIB_DATA)")
 *   - the extension boundary the file records is inside the table
 *
 * A correction runs per frame; a load runs once. Checking 65536 entries on every
 * frame would be paid 30 times a second to learn something that cannot change
 * between frames.
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"
#include "xpe/preprocess/xcal_format.h"
#include "xcal_reader.hpp"

#include <cstring>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

extern "C" XPE_API XpeErrorCode xpe_calib_load_nonlin_lut(const char* filepath) {
    try {
        if (filepath == nullptr) return XPE_ERR_INVALID_INPUT;

        XCalFileHeader hdr;
        std::vector<uint8_t> config_json;
        std::vector<uint8_t> payload;
        XpeErrorCode rc = read_xcal_file(filepath, hdr, config_json, payload,
                                         /*check_expiry=*/true,
                                         /*expected_type=*/XCAL_TYPE_NONLIN_LUT);
        if (rc != XPE_OK) return rc;

        if (payload.size() % sizeof(uint16_t) != 0) return XPE_ERR_CONFIG_INVALID;
        const size_t entries = payload.size() / sizeof(uint16_t);
        if (entries != 4096u && entries != 65536u) {
            return XPE_ERR_INVALID_CALIB_DATA;
        }

        std::unique_ptr<uint16_t[]> lut(new uint16_t[entries]);
        std::memcpy(lut.get(), payload.data(), payload.size());

        for (size_t i = 0; i + 1 < entries; ++i) {
            if (lut[i] > lut[i + 1]) return XPE_ERR_INVALID_CALIB_DATA;
        }

        // Where the generator stopped interpolating and started extrapolating.
        // Absent (an older file, or one from another tool) reads as 0 from the
        // helper's default; treat that as "the whole table is measured" rather
        // than rejecting, but a value past the end is a corrupt record.
        const std::string json(config_json.begin(), config_json.end());
        const double ext = xpe_json_get_double(json.c_str(),
                                               "xcal_nonlin_extension_start", 0.0);
        if (ext < 0.0 || ext > static_cast<double>(entries)) {
            return XPE_ERR_INVALID_CALIB_DATA;
        }

        {
            std::lock_guard<std::mutex> lock(g_calib_mutex);
            g_calib.nonlin_lut = std::move(lut);
            g_calib.nonlin_entries = static_cast<uint32_t>(entries);
            g_calib.nonlin_extension_start = static_cast<uint32_t>(ext);
            g_calib.nonlin_timestamp = hdr.created_epoch_ms;
        }
        return XPE_OK;
    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}

/**
 * Drops the loaded LUT, leaving the store with no nonlinearity calibration.
 *
 * Symmetric with the load, and needed for a real reason rather than a test one:
 * a host that switches detector profiles must be able to say "this panel has no
 * LUT" rather than leave the previous panel's table applying to a new detector's
 * frames. Without it the only way back to the uncalibrated state is to restart
 * the process.
 */
extern "C" XPE_API void xpe_calib_unload_nonlin_lut(void) {
    std::lock_guard<std::mutex> lock(g_calib_mutex);
    g_calib.nonlin_lut.reset();
    g_calib.nonlin_entries = 0;
    g_calib.nonlin_extension_start = 0;
    g_calib.nonlin_timestamp = 0;
}
