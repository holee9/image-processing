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
#include <mutex>
#include <string>
#include <vector>

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
static constexpr float  XPE_FRAC_DEFAULT_STEP = 0.25f;

/* ============================================================================
 * Collimation Detection Constants -- SWU-2.8
 * ============================================================================ */

static constexpr float  XPE_COL_DEFAULT_SENSITIVITY    = 0.5f;
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
 *  in that case. */
bool parse_fractional_config(const char* json,
                             int&   outIterations,
                             float& outStepSize,
                             bool&  outSafetyViolation);

/** Parse collimation config from JSON string. Returns true on success. */
bool parse_collimation_config(const char* json,
                              float& outSensitivity,
                              float& outMinAreaRatio,
                              int&   outBorderMargin);

} // namespace config
} // namespace enhance_advanced
} // namespace xpe

#endif /* XPE_ENHANCE_ADVANCED_INTERNAL_H */
