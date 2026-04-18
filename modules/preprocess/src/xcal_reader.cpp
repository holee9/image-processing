/**
 * @file xcal_reader.cpp
 * @brief XCal v1 file reader implementation (T-005)
 *
 * REQ-P1A-014~016, REQ-P1A-018, REQ-P1A-030
 */

#include "xcal_reader.hpp"
#include "xcal_validator.hpp"
#include "xpe_sha256.hpp"

#include <fstream>
#include <cstring>
#include <chrono>
#include <cstdint>

// @MX:NOTE: [AUTO] Clock source for expiry: std::chrono::system_clock.
// Resolution is milliseconds (epoch_ms). Non-monotonic but
// consistent with XCal v1 created_epoch_ms semantics.

XpeErrorCode read_xcal_file(
    const char*            path,
    XCalFileHeader&        out_header,
    std::vector<uint8_t>&  out_config,
    std::vector<uint8_t>&  out_payload,
    bool                   check_expiry,
    int                    expected_type)
{
    try {
        if (path == nullptr) {
            return XPE_ERR_INVALID_INPUT;
        }

        // Open file
        std::ifstream f(path, std::ios::binary);
        if (!f.is_open()) {
            return XPE_ERR_IO_FAILED;
        }

        // Read header (152 bytes, pack=1)
        XCalFileHeader hdr;
        f.read(reinterpret_cast<char*>(&hdr), sizeof(hdr));
        if (!f.good() || f.gcount() != static_cast<std::streamsize>(sizeof(hdr))) {
            return XPE_ERR_IO_FAILED;
        }

        // Validate header
        XpeErrorCode vrc = validate_xcal_header(hdr, expected_type);
        if (vrc != XPE_OK) {
            return vrc;
        }

        // Read config_json
        std::vector<uint8_t> config;
        if (hdr.config_json_len > 0) {
            config.resize(static_cast<size_t>(hdr.config_json_len));
            f.read(reinterpret_cast<char*>(config.data()),
                   static_cast<std::streamsize>(hdr.config_json_len));
            if (!f.good() ||
                f.gcount() != static_cast<std::streamsize>(hdr.config_json_len)) {
                return XPE_ERR_IO_FAILED;
            }
        }

        // Read payload
        std::vector<uint8_t> payload;
        if (hdr.payload_len > 0) {
            payload.resize(static_cast<size_t>(hdr.payload_len));
            f.read(reinterpret_cast<char*>(payload.data()),
                   static_cast<std::streamsize>(hdr.payload_len));
            if (!f.good() ||
                f.gcount() != static_cast<std::streamsize>(hdr.payload_len)) {
                return XPE_ERR_IO_FAILED;
            }
        }

        // Verify SHA-256
        const uint8_t* cfg_ptr = config.empty()  ? nullptr : config.data();
        const uint8_t* pay_ptr = payload.empty() ? nullptr : payload.data();
        auto computed = compute_sha256_two_parts(
            cfg_ptr, config.size(),
            pay_ptr, payload.size());

        if (std::memcmp(computed.data(), hdr.sha256, 32) != 0) {
            return XPE_ERR_CONFIG_INVALID;
        }

        // Check expiry (if requested and expiry is set)
        if (check_expiry && hdr.expiry_epoch_ms != 0) {
            using namespace std::chrono;
            int64_t now_ms = duration_cast<milliseconds>(
                system_clock::now().time_since_epoch()).count();
            if (now_ms > hdr.expiry_epoch_ms) {
                return XPE_ERR_CALIBRATION_EXPIRED;
            }
        }

        // Success: commit outputs
        out_header  = hdr;
        out_config  = std::move(config);
        out_payload = std::move(payload);
        return XPE_OK;

    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}
