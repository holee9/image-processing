/**
 * @file xpe_preprocess_internal.h
 * @brief XPE Pre-Processing internal C++ helpers (NOT exported, TU-private)
 *
 * Only included by the .cpp translation units inside modules/preprocess/src/.
 * SPEC: SPEC-XPE-P1A v1.0.0
 * IEC 62304 Class B
 */

#ifndef XPE_PREPROCESS_INTERNAL_H_
#define XPE_PREPROCESS_INTERNAL_H_

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cmath>
#include <vector>
#include <string>
#include <memory>

/* =========================================================================
 * SWU-1.4: GhostCorrectorHandle — opaque handle backing xpe_ghost_create
 * REQ-P1A-029 to REQ-P1A-034
 * ========================================================================= */

// @MX:ANCHOR: [AUTO] GhostCorrectorHandle — opaque handle for xpe_ghost_* API
// @MX:REASON: Public API boundary; handle pointer cast checked in every ghost function
struct GhostCorrectorHandle {
    static constexpr uint32_t kMagic = 0xA7057AC0u; // sentinel for handle validation

    uint32_t magic{kMagic};
    uint32_t width{0};
    uint32_t height{0};

    // Dual-exponential IRF coefficients (PMC3465354)
    double alpha1{0.9};    // fast component amplitude
    double tau1{1.0};      // fast component time constant (frames)
    double alpha2{0.05};   // slow component amplitude
    double tau2{20.0};     // slow component time constant (frames)

    // Frame history ring buffer (float32 per pixel per history slot)
    std::vector<float> hist1; // fast IRF accumulator
    std::vector<float> hist2; // slow IRF accumulator

    double lastAcqTimeSec{0.0};

    // Validate that a void* is a live handle
    static bool isValid(const void* h) noexcept {
        if (!h) return false;
        const auto* gh = static_cast<const GhostCorrectorHandle*>(h);
        return gh->magic == kMagic;
    }
};

/* =========================================================================
 * CRC-32 helpers (SWU-1.5 calibration manager)
 * REQ-P1A-036
 * ========================================================================= */

// @MX:NOTE: [AUTO] CRC-32/ISO-HDLC (polynomial 0xEDB88320) — matches Python bindings
uint32_t xpe_crc32(const uint8_t* data, size_t len) noexcept;

/* =========================================================================
 * Calibration file I/O helpers
 * ========================================================================= */

struct CalibFileHeader {
    uint8_t  magic[4];       // "XPEC"
    uint32_t version;        // format version (currently 1)
    uint32_t width;
    uint32_t height;
    uint32_t pixelFormat;    // XpePixelFormat
    uint64_t expiryEpochMs;  // expiry timestamp
    uint32_t payloadCrc32;   // CRC-32 of pixel data
    uint32_t reserved[7];    // pad to 64 bytes
};
static_assert(sizeof(CalibFileHeader) == 64, "CalibFileHeader must be 64 bytes");

/* =========================================================================
 * Bilinear interpolation helper (SWU-1.3 defect pixel correction)
 * ========================================================================= */

// @MX:NOTE: [AUTO] Edge-aware bilinear: skips neighbours that are also defective
float xpe_interpolate_pixel(const float* pixels, const uint8_t* defectMask,
                             uint32_t x, uint32_t y,
                             uint32_t width, uint32_t height) noexcept;

/* =========================================================================
 * Lightweight JSON field extractor (no external dependency)
 * Returns empty string when key is absent or configJson is null.
 * ========================================================================= */

std::string xpe_json_get_string(const char* configJson, const char* key);

/* =========================================================================
 * Dimension / null guard inline helpers
 * ========================================================================= */

inline bool xpe_dims_match(const XpeImageBuffer* a, const XpeImageBuffer* b) noexcept {
    return a && b && a->width == b->width && a->height == b->height;
}

// Returns true when buffer has contiguous layout: stride == width * elementSize.
// All XPE processing functions require contiguous buffers (no row padding).
inline bool xpe_is_contiguous(const XpeImageBuffer* buf, size_t elementSize) noexcept {
    return buf && buf->stride == static_cast<uint32_t>(buf->width * elementSize);
}

#endif /* XPE_PREPROCESS_INTERNAL_H_ */
