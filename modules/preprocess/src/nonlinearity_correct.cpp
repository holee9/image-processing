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

#include <mutex>
#include <string>
#include <vector>

// @MX:NOTE: [AUTO] No-op when configJsonOrNull is null (REQ-P1A-013); error on unknown mode
// @MX:SPEC: REQ-P1A-012

// Known detector modes with nonlinearity tables; identity polynomial applied as baseline
static const std::vector<std::string> kKnownModes = {"standard", "high_gain", "low_dose"};

XpeErrorCode xpe_nonlinearity_apply(XpeImageBuffer* img,
                                     const char* configJsonOrNull,
                                     bool* applied)
{
    bool ignored = false;
    bool& changed = applied ? *applied : ignored;
    changed = false;
    if (!img) return XPE_ERR_INVALID_INPUT;
    if (!xpe_buffer_has_format(img, XPE_PIXEL_UINT16)) return XPE_ERR_INVALID_INPUT;

    // ---------------------------------------------------------------------
    // SRS-CALIB-FUNC-006-EXT 6a (#186, QA-A-111): apply the loaded LUT.
    //
    // "Lookup: I_lin = LUT[I_raw] (direct index, O(1))".
    //
    // `panel.linear` governs enable/disable per SRS-CALIB-FUNC-006: "Detector
    // profile governs enable/disable via field panel.linear = true/false". A
    // linear panel needs no correction, so the stage is skipped AND the frame
    // is not marked corrected -- the flag has to mean pixels changed (#184).
    const std::string panel_linear =
        configJsonOrNull ? xpe_json_get_string(configJsonOrNull, "panel.linear")
                         : std::string();
    if (panel_linear == "true") return XPE_OK;

    {
        std::lock_guard<std::mutex> lock(g_calib_mutex);
        if (g_calib.nonlin_lut && g_calib.nonlin_entries > 0) {
            uint16_t* px = static_cast<uint16_t*>(img->data);
            size_t n = 0;
            if (!xpe_pixel_count(img, &n)) return XPE_ERR_INVALID_INPUT;
            const uint32_t last = g_calib.nonlin_entries - 1u;
            const uint16_t* lut = g_calib.nonlin_lut.get();
            for (size_t i = 0; i < n; ++i) {
                // A 16-bit frame can address past a 4096-entry table. Clamping
                // to the last entry continues the table's own extension rather
                // than reading out of bounds; the table's extension region is
                // already marked as claiming no accuracy.
                const uint32_t raw = px[i];
                px[i] = lut[raw > last ? last : raw];
            }
            changed = true;
            return XPE_OK;
        }
    }

    // No LUT is loaded. A panel explicitly declared non-linear must not pass
    // through silently: the frame would reach gain correction uncorrected and
    // nothing downstream can tell. Saying so at the stage is the only place the
    // operator can still act on it.
    if (panel_linear == "false") {
        xpe_alert_push("panel.linear is false but no nonlinearity LUT is loaded; "
                       "call xpe_calib_load_nonlin_lut() before processing "
                       "(SRS-CALIB-FUNC-006, issue #186)", XPE_ALERT_ERROR);
        return XPE_ERR_CALIB_NOT_LOADED;
    }

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
    // (SRS-CALIB-FUNC-006, not implemented). No pixel changes, so `changed`
    // stays false and the pipeline does not mark the frame corrected (#184).
    (void)img;
    return XPE_OK;
}

XpeErrorCode xpe_nonlinearity_correct(XpeImageBuffer* img,
                                       const char* configJsonOrNull)
{
    return xpe_nonlinearity_apply(img, configJsonOrNull, nullptr);
}
