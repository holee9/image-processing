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
#include <limits>
#include <vector>
#include <string>
#include <memory>
#include <mutex>

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

    // Ghost correction tier (1=LTI, 2=exposure-weighted LTI, 3=NLCSC)
    int tier{1};

    // Dual-exponential IRF coefficients (PMC3465354)
    double alpha1{0.9};    // fast component amplitude
    double tau1{1.0};      // fast component time constant (frames)
    double alpha2{0.05};   // slow component amplitude
    double tau2{20.0};     // slow component time constant (frames)

    // Tier 2: exposure-weighted LTI thresholds
    double tier2Threshold{0.005};  // auto-escalate to Tier 2 when residual > 0.5%
    double exposureWeight{1.0};    // dynamic exposure scaling factor

    // Tier 3: NLCSC signal-dependent coefficients
    double nlcscBeta{0.1};  // signal dependency parameter

    // Frame history ring buffer (float32 per pixel per history slot)
    std::vector<float> hist1; // fast IRF accumulator
    std::vector<float> hist2; // slow IRF accumulator

    double lastAcqTimeSec{0.0};
    double lastFrameMean{0.0}; // mean signal level for exposure weighting

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

double xpe_json_get_double(const char* configJson, const char* key, double defaultVal);

/* =========================================================================
 * Dimension / null guard inline helpers
 * ========================================================================= */

inline bool xpe_dims_match(const XpeImageBuffer* a, const XpeImageBuffer* b) noexcept {
    return a && b && a->width == b->width && a->height == b->height;
}

inline bool xpe_pixel_size(XpePixelFormat format, size_t* bytesOut) noexcept {
    if (!bytesOut) return false;

    switch (format) {
    case XPE_PIXEL_UINT8:
        *bytesOut = 1;
        return true;
    case XPE_PIXEL_UINT16:
        *bytesOut = 2;
        return true;
    case XPE_PIXEL_FLOAT32:
        *bytesOut = 4;
        return true;
    default:
        return false;
    }
}

inline bool xpe_pixel_count(const XpeImageBuffer* buf, size_t* countOut) noexcept {
    if (!buf || !countOut || buf->width == 0 || buf->height == 0) return false;

    const size_t width = static_cast<size_t>(buf->width);
    const size_t height = static_cast<size_t>(buf->height);
    if (width > std::numeric_limits<size_t>::max() / height) return false;

    *countOut = width * height;
    return true;
}

inline bool xpe_required_bytes(const XpeImageBuffer* buf, size_t elementSize,
                               size_t* bytesOut) noexcept {
    if (!bytesOut) return false;

    size_t count = 0;
    if (!xpe_pixel_count(buf, &count)) return false;
    if (count > std::numeric_limits<size_t>::max() / elementSize) return false;

    *bytesOut = count * elementSize;
    return true;
}

inline bool xpe_buffer_has_format(const XpeImageBuffer* buf,
                                  XpePixelFormat expectedFormat,
                                  size_t* countOut = nullptr) noexcept {
    if (!buf || !buf->data || buf->format != expectedFormat) return false;

    size_t elementSize = 0;
    if (!xpe_pixel_size(expectedFormat, &elementSize)) return false;

    size_t requiredBytes = 0;
    if (!xpe_required_bytes(buf, elementSize, &requiredBytes)) return false;
    if (buf->dataSize < requiredBytes) return false;

    if (countOut) {
        *countOut = requiredBytes / elementSize;
    }
    return true;
}

/* =========================================================================
 * Pre-loaded Calibration State (Pipeline Optimization)
 *
 * Holds calibration maps loaded once and reused across multiple pipeline
 * invocations. Eliminates per-frame file I/O for offset/gain/defect maps.
 * ========================================================================= */

/**
 * @brief Pre-loaded calibration maps for pipeline optimization.
 *
 * Populated once via xpe_calib_state_load(), then passed to
 * xpe_preprocess_pipeline_ex() to skip file I/O on each frame.
 *
 * Lifecycle:
 * 1. Zero-initialize: XpeCalibrationState state = {};
 * 2. Load maps:       xpe_calib_state_load(&state, calibPath);
 * 3. Process frames:  xpe_preprocess_pipeline_ex(img, meta, &state, ...);
 * 4. Release:         xpe_calib_state_release(&state);
 *
 * Memory ownership: offset/gain/defect data pointers are owned by this struct.
 * Callers must call xpe_calib_state_release() to free.
 */
struct XpeCalibrationState {
    XpeImageBuffer offsetMap;   ///< Pre-loaded offset correction map (uint16)
    XpeImageBuffer gainMap;     ///< Pre-loaded gain correction map (float32)
    XpeImageBuffer defectMap;   ///< Pre-loaded defect pixel map (uint8)

    bool offsetLoaded;          ///< true if offsetMap is valid
    bool gainLoaded;            ///< true if gainMap is valid
    bool defectLoaded;          ///< true if defectMap is valid
};

/* =========================================================================
 * Global Calibration Data (singleton, new XCal v1 API)
 * Populated by xpe_calib_load_offset / xpe_calib_load_gain / xpe_calib_load_defect_map.
 * ========================================================================= */

struct CalibrationData {
    std::unique_ptr<float[]>   offset_map;
    uint32_t offset_width{0};
    uint32_t offset_height{0};
    int64_t  offset_timestamp{0};
    char     offset_session_id[64]{};

    std::unique_ptr<float[]>   gain_map;
    uint32_t gain_width{0};
    uint32_t gain_height{0};
    int64_t  gain_timestamp{0};
    char     gain_session_id[64]{};

    std::unique_ptr<uint8_t[]> defect_map;
    uint32_t defect_width{0};
    uint32_t defect_height{0};
};

extern CalibrationData g_calib;
extern std::mutex      g_calib_mutex;

#endif /* XPE_PREPROCESS_INTERNAL_H_ */
