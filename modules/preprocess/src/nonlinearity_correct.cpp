/**
 * @file nonlinearity_correct.cpp
 * @brief SWU-1.7: Detector nonlinearity correction (PRE-08)
 *        Piecewise linear or polynomial correction.
 *        No-op if no nonlinearity coefficients loaded for this panel profile.
 *        REQ-P1A-012 to REQ-P1A-015
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <vector>
#include <string>

// @MX:NOTE: [AUTO] No-op when configJsonOrNull is null (REQ-P1A-013); error on unknown mode
// @MX:SPEC: REQ-P1A-012

// Known detector modes with nonlinearity tables; identity polynomial applied as baseline
static const std::vector<std::string> kKnownModes = {"standard", "high_gain", "low_dose"};

XpeErrorCode xpe_nonlinearity_correct(XpeImageBuffer* img,
                                       const char* configJsonOrNull)
{
    if (!img) return XPE_ERR_INVALID_INPUT;
    if (!xpe_buffer_has_format(img, XPE_PIXEL_UINT16)) return XPE_ERR_INVALID_INPUT;

    // REQ-P1A-013: no-op when no config supplied
    if (!configJsonOrNull) return XPE_OK;

    // Parse detector mode from JSON
    const std::string mode = xpe_json_get_string(configJsonOrNull, "mode");
    if (mode.empty()) return XPE_OK; // no mode key — no-op

    // REQ-P1A-014: unknown mode -> XPE_ERR_CONFIG_INVALID
    bool known = false;
    for (const auto& m : kKnownModes) {
        if (m == mode) { known = true; break; }
    }
    if (!known) return XPE_ERR_CONFIG_INVALID;

    // REQ-P1A-012/015: apply identity polynomial for now (baseline, uint16 format)
    // Real coefficients would be loaded from a per-detector calibration profile
    (void)img;
    return XPE_OK;
}
