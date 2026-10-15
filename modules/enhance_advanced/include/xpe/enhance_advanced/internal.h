/**
 * @file internal.h
 * @brief Internal shared declarations for xpe_enhance_advanced module.
 *
 * This header is PRIVATE to the module. It must NOT be included by external
 * consumers or by other XPE modules. Only files under src/ and tests/ may
 * include it.
 *
 * Provides:
 *   - Module-level constants and version
 *   - Internal configuration parsing helpers
 *   - Shared forward declarations for detail/ sub-components
 *
 * @ingroup xpe_enhance_advanced_internal
 */

#ifndef XPE_ENHANCE_ADVANCED_INTERNAL_H
#define XPE_ENHANCE_ADVANCED_INTERNAL_H

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <new>
#include <string>
#include <vector>

/* ============================================================================
 * Exception guard (QA-B-181, QA-B-179, #233)
 * ============================================================================ */

/**
 * @brief The outermost guard of an exported function that has no guard of its own.
 *
 * No exception may leave an `extern "C"` function. std::bad_alloc becomes XPE_ERR_OUT_OF_MEMORY, anything else
 * XPE_ERR_PROCESSING_FAILED; the handlers allocate nothing. The body must be an `extern "C++"` function: one
 * declared inside an extern "C" block gets C linkage and the "never throws" treatment, the compiler may drop the
 * catch, and a lock_guard in the body stays locked (measured, modules/ai/src/ai.cpp). Header-inline so the test
 * executable can run it directly.
 */
template <class F>
inline XpeErrorCode XpeAdvGuardedCall(F&& body) noexcept {
    try {
        return body();
    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}

/* ============================================================================
 * Module Version
 * ============================================================================ */

#define XPE_ENHANCE_ADVANCED_VERSION "1.0.0"

/* ============================================================================
 * MFP (Multi-scale Frequency Processing) Constants -- SWU-2.5
 * ============================================================================ */

/** Default number of Laplacian pyramid levels. */
static constexpr int    XPE_MFP_DEFAULT_LEVELS         = 4;
static constexpr int    XPE_MFP_MIN_LEVELS             = 2;
static constexpr int    XPE_MFP_MAX_LEVELS             = 8;

static constexpr float  XPE_MFP_DEFAULT_EDGE_GAIN      = 1.5f;
static constexpr float  XPE_MFP_DEFAULT_TEXTURE_GAIN   = 1.0f;
static constexpr float  XPE_MFP_DEFAULT_FLAT_GAIN      = 0.8f;
static constexpr float  XPE_MFP_DEFAULT_NOISE_THRESH   = 5.0f;

/* ============================================================================
 * Fractional-Order Edge Enhancement Constants -- SWU-2.6
 * ============================================================================ */

static constexpr float  XPE_FRAC_MIN_ORDER    = 0.0f;
static constexpr float  XPE_FRAC_MAX_ORDER    = 2.0f;
static constexpr int    XPE_FRAC_DEFAULT_ITER = 1;
static constexpr int    XPE_FRAC_MAX_ITER     = 5;
// #162 (QA-B-139): XPE_FRAC_DEFAULT_STEP = 0.25f lived here and had exactly one
// reader -- the parser default for a value nothing applied. Removed with it.
// The SDD still lists step_size with this default; that row needs the same
// amendment (lead owns docs/, so the block is in the QA-B-139 report).

/* ============================================================================
 * Collimation Detection Constants -- SWU-2.8
 * ============================================================================ */

static constexpr float  XPE_COL_DEFAULT_CONF_STRICTNESS = 0.5f;
static constexpr float  XPE_COL_DEFAULT_MIN_AREA_RATIO = 0.05f;
static constexpr int    XPE_COL_DEFAULT_BORDER_MARGIN  = 8;

/* ============================================================================
 * Safety Limits
 * ============================================================================ */

/** Maximum pixel overshoot ratio allowed after fractional enhancement.
 *  Violation triggers XPE_ERR_SAFETY_VIOLATION (SAF-100). */
static constexpr float  XPE_SAFETY_MAX_OVERSHOOT_RATIO = 0.05f;

/* ============================================================================
 * Module State (extern, defined in xpe_enhance_advanced.cpp)
 * ============================================================================ */

extern bool          g_initialized;
extern std::mutex    g_initMutex;

/** Check if module is initialized (thread-safe). */
bool isModuleInitialized();

/* ============================================================================
 * JSON Config Parsing Helpers
 * ============================================================================ */

namespace xpe {
namespace enhance_advanced {
namespace config {

/** Parse MFP config from JSON string. Returns true on success. */
bool parse_mfp_config(const char* json,
                      int&   outLevels,
                      float& outEdgeGain,
                      float& outTextureGain,
                      float& outFlatGain,
                      float& outNoiseThreshold);

/** Parse fractional-order config from JSON string. Returns true on success.
 *  If a SAF-100 forbidden key is detected, returns false and sets
 *  outSafetyViolation to true. Caller should return XPE_ERR_SAFETY_VIOLATION
 *  in that case.
 *
 *  #162 (QA-B-139): `step_size` is NOT an output here. It used to be read and
 *  clamped to [0.01, 1.0] and then travel to a debug log and stop, which made
 *  the code read as if the value were applied. What the value would mean is
 *  undefined: the Gruenwald-Letnikov form in detail/fractional_derivative.h
 *  carries a step `h`, but computeFractionalMask builds coefficients only and
 *  the convolution samples at INTEGER pixel offsets -- there is no h and no
 *  sub-pixel sampling to carry one. The key stays KNOWN (so the unknown-key
 *  warning does not fire) and xpe_fractional_process reports it through the
 *  inert-key warning instead, which says why. */
bool parse_fractional_config(const char* json,
                             int&   outIterations,
                             bool&  outSafetyViolation);

/**
 * @brief Parse collimation config from JSON string. Returns true on success.
 *
 * @param outConfidenceStrictness  The `confidence_strictness` key, [0, 1].
 *        RENAMED 2026-09-16 (#164, user decision). The key was `sensitivity`,
 *        which said the opposite of what the value does: it is interpolated
 *        into the confidence a detection must reach before it is accepted
 *        (0.7 + 0.3 * value, collimation_detect.cpp), so RAISING it rejects
 *        MORE. QA-B-63 measured that -- at the detection margin, 0.0 detects a
 *        rectangle that 1.0 discards. The arithmetic was left untouched and the
 *        name was moved to match it; reversing the arithmetic was considered and
 *        not taken, because nothing in the requirements says which direction is
 *        correct. The old name is not accepted: it now falls to the QA-B-61
 *        unknown-key warning, which names it.
 */
bool parse_collimation_config(const char* json,
                              float& outConfidenceStrictness,
                              float& outMinAreaRatio,
                              int&   outBorderMargin);

/**
 * @brief #145 (QA-B-61): report top-level config keys this entry point does not
 *        consume -- once per distinct set of unknown keys, per thread.
 *
 * @p lastWarned is caller-owned thread_local memory; see the implementation for
 * why the memory is not module-global and what that costs.
 */
void warn_unconsumed_keys_once(const char*        json,
                               const char* const* knownKeys,
                               size_t             knownCount,
                               const char*        nestedObject,
                               const char*        fnLabel,
                               std::string&       lastWarned);

/**
 * @brief #162 (QA-B-122): a key whose NAME the parser knows but whose value
 *        cannot reach the output -- the case warn_unconsumed_keys_once is blind
 *        to, because that one only reports names absent from the known list.
 */
struct InertKey {
    const char* key;      ///< the config key, as written in the JSON
    const char* reason;   ///< why it has no effect, in one clause
};

/**
 * @brief Report the inert keys that are actually PRESENT in @p json -- once per
 *        distinct set, per thread, through the same alert channel.
 *
 * The caller assembles the list, because inertness is often value-dependent
 * (`texture_gain` is inert only when the level count leaves no middle band).
 * @p lastWarned is caller-owned thread_local memory, as above.
 */
void warn_inert_keys_once(const char*     json,
                          const InertKey* inertKeys,
                          size_t          inertCount,
                          const char*     nestedObject,
                          const char*     fnLabel,
                          std::string&    lastWarned);

} // namespace config

/**
 * @brief Bytes per pixel for a supported pixel format, or 0 if unknown.
 */
inline uint32_t bytes_per_pixel(XpePixelFormat format) {
    switch (format) {
        case XPE_PIXEL_UINT16:  return 2u;
        case XPE_PIXEL_FLOAT32: return 4u;
        default:                return 0u;
    }
}

/**
 * @brief api-spec "XpeImageBuffer.dataSize on input" size-consistency check.
 *
 * `dataSize == 0` means unspecified and is accepted (legacy callers do not
 * populate the field). A non-zero `dataSize` smaller than
 * width * height * bytesPerPixel(format) means the buffer cannot hold the
 * image it declares, and reading it overruns the allocation (#123, observed
 * as an ASan heap-buffer-overflow READ). A larger value is accepted.
 *
 * Header-inline on purpose: one definition for the module without adding an
 * export to xpe_common (REQ-P0-008 fixes that surface at 16 symbols).
 *
 * @return true when the declared size is consistent (or unspecified).
 */
inline bool data_size_is_consistent(const XpeImageBuffer* img) {
    if (img == nullptr || img->dataSize == 0) {
        return true;
    }
    const uint32_t bpp = bytes_per_pixel(img->format);
    if (bpp == 0u) {
        return true;   // unknown format is the format check's business, not this one
    }
    const uint64_t required = static_cast<uint64_t>(img->width) *
                              static_cast<uint64_t>(img->height) *
                              static_cast<uint64_t>(bpp);
    return static_cast<uint64_t>(img->dataSize) >= required;
}

/**
 * @brief True when every one of the `n` floats is finite (QA-B-181f, #233).
 *
 * A bit test on the exponent field, not std::isfinite and not a range comparison: whether `x <= 0` refuses NaN
 * depends on /fp:fast against /fp:precise (QA-B-181e), and a bit test does not. One pass, no early exit, so it
 * vectorizes; about 4x cheaper than a std::isfinite loop (3072x3072: 1.6 ms against 6.5 ms). Same helper as
 * enhance_basic_internal.h; each module keeps its own copy, as it does with data_size_is_consistent.
 */
inline bool all_finite(const float* p, uint64_t n) {
    uint32_t bad = 0;
    for (uint64_t i = 0; i < n; ++i) {
        uint32_t u;
        std::memcpy(&u, p + i, sizeof u);
        bad |= static_cast<uint32_t>((u & 0x7F800000u) == 0x7F800000u);
    }
    return bad == 0;
}

} // namespace enhance_advanced
} // namespace xpe

#endif /* XPE_ENHANCE_ADVANCED_INTERNAL_H */
