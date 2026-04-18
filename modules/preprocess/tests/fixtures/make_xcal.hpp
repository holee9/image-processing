/**
 * @file make_xcal.hpp
 * @brief Shared XCal v1 test fixture helper (T-005+)
 *
 * Provides MakeXCalFile() which uses xcal_writer to produce
 * correct XCal v1 files for use in reader and load tests.
 */

#ifndef FIXTURES_MAKE_XCAL_HPP
#define FIXTURES_MAKE_XCAL_HPP

#include <cstring>
#include <vector>
#include <chrono>
#include "xpe/preprocess/xcal_format.h"
#include "xpe/common/xpe_error.h"
#include "xcal_writer.hpp"

/**
 * @brief Write a valid XCal v1 OFFSET file.
 *
 * @param path       Output file path.
 * @param w          Width in pixels.
 * @param h          Height in pixels.
 * @param value      Pixel value for all elements (default 1.0f).
 * @param expiry_ms  Expiry epoch ms (0 = never, negative = already expired).
 * @return XPE_OK on success.
 */
inline XpeErrorCode MakeOffsetXCal(const char* path,
                                   uint32_t w, uint32_t h,
                                   float value = 1.0f,
                                   int64_t expiry_ms = 0)
{
    using namespace std::chrono;
    int64_t now_ms = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();

    std::vector<float> payload(static_cast<size_t>(w) * h, value);

    XCalFileHeader hdr;
    std::memset(&hdr, 0, sizeof(hdr));
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version          = XCAL_VERSION;
    hdr.type             = XCAL_TYPE_OFFSET;
    hdr.pixel_format     = XCAL_FMT_FLOAT32;
    hdr.width            = w;
    hdr.height           = h;
    hdr.created_epoch_ms = now_ms;
    hdr.expiry_epoch_ms  = expiry_ms;
    hdr.config_json_len  = 0;
    hdr.payload_len      = static_cast<uint64_t>(w) * h * sizeof(float);

    return write_xcal_file(path, hdr,
                           nullptr, 0,
                           reinterpret_cast<const uint8_t*>(payload.data()),
                           payload.size() * sizeof(float));
}

/**
 * @brief Write a valid XCal v1 GAIN file.
 */
inline XpeErrorCode MakeGainXCal(const char* path,
                                 uint32_t w, uint32_t h,
                                 float value = 1.0f,
                                 int64_t expiry_ms = 0)
{
    using namespace std::chrono;
    int64_t now_ms = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();

    std::vector<float> payload(static_cast<size_t>(w) * h, value);

    XCalFileHeader hdr;
    std::memset(&hdr, 0, sizeof(hdr));
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version          = XCAL_VERSION;
    hdr.type             = XCAL_TYPE_GAIN;
    hdr.pixel_format     = XCAL_FMT_FLOAT32;
    hdr.width            = w;
    hdr.height           = h;
    hdr.created_epoch_ms = now_ms;
    hdr.expiry_epoch_ms  = expiry_ms;
    hdr.config_json_len  = 0;
    hdr.payload_len      = static_cast<uint64_t>(w) * h * sizeof(float);

    return write_xcal_file(path, hdr,
                           nullptr, 0,
                           reinterpret_cast<const uint8_t*>(payload.data()),
                           payload.size() * sizeof(float));
}

/**
 * @brief Write a valid XCal v1 DEFECT file.
 */
inline XpeErrorCode MakeDefectXCal(const char* path,
                                   uint32_t w, uint32_t h,
                                   uint8_t value = 0)
{
    using namespace std::chrono;
    int64_t now_ms = duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();

    std::vector<uint8_t> payload(static_cast<size_t>(w) * h, value);

    XCalFileHeader hdr;
    std::memset(&hdr, 0, sizeof(hdr));
    std::memcpy(hdr.magic, XCAL_MAGIC, 4);
    hdr.version          = XCAL_VERSION;
    hdr.type             = XCAL_TYPE_DEFECT;
    hdr.pixel_format     = XCAL_FMT_UINT8_MASK;
    hdr.width            = w;
    hdr.height           = h;
    hdr.created_epoch_ms = now_ms;
    hdr.expiry_epoch_ms  = 0;
    hdr.config_json_len  = 0;
    hdr.payload_len      = static_cast<uint64_t>(w) * h * sizeof(uint8_t);

    return write_xcal_file(path, hdr,
                           nullptr, 0,
                           payload.data(),
                           payload.size());
}

/**
 * @brief Write a minimal bad-magic file (4 bytes "BAD!").
 */
inline bool MakeBadMagicFile(const char* path) {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;
    f.write("BAD!", 4);
    return f.good();
}

#endif /* FIXTURES_MAKE_XCAL_HPP */
