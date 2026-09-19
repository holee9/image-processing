/**
 * @file nonlinearity_correct.cpp
 * @brief SWU-1.7: Detector nonlinearity correction (PRE-08)
 *        Applies SRS-CALIB-FUNC-006-EXT 6a (nonlinearity LUT); no-op with an
 *        alert when no LUT is loaded for this panel profile.
 *
 *        The REQ-P1A-012..015 citation that used to sit here named four
 *        requirements about defect correction and calibration loading, and
 *        SPEC-XPE-P1A puts nonlinearity out of its own scope (QA-A-126, #186).
 *        The governing requirement is SRS-CALIB-FUNC-006 / -EXT 6a.
 * IEC 62304 Class B
 */

#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <mutex>
#include <string>
#include <vector>

// @MX:NOTE: [AUTO] Applies the loaded nonlinearity LUT; no-op with an alert
//           when none is loaded (QA-A-127, #196)
// @MX:SPEC: SRS-CALIB-FUNC-006-EXT 6a


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

    // QA-A-125 (#186): A LOADED LUT TAKES PRECEDENCE OVER THE DETECTOR MODE.
    // This block sits ahead of the "mode" parse below, so when a LUT is loaded
    // it is applied whatever `mode` says. That ordering was deliberate and
    // nowhere written down, so it is written here. (The mode list it used to
    // outrank is gone -- QA-A-127, #196; the precedence note stays because a
    // second model will reintroduce the question.)
    //
    // It is the same question #187 answered for the gain models with
    // "the model loaded last is the one applied". Here there is only one
    // model, so precedence is unambiguous; when EXT 6b (the global polynomial)
    // arrives there will be two, and LUT-vs-polynomial has to be decided then
    // -- not inferred from whichever branch happens to come first.
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

    if (!configJsonOrNull) return XPE_OK;

    // No LUT, and the panel was not declared non-linear: nothing is corrected.
    //
    // QA-A-127 (#196): this used to reject any "mode" outside a hard-coded
    // list {"standard","high_gain","low_dose"} with XPE_ERR_CONFIG_INVALID,
    // failing the whole pipeline. Three measurements retired that list:
    //
    //  - the rejection was REACHED. A pipeline config carrying
    //    "mode":"clinical" returned -4 while the same config without the key
    //    returned 0 (QA-A-126 probe).
    //  - the three names have NO source. Searched docs/ and .moai/specs/:
    //    "high_gain" appears once as a defect-detection pixel mask
    //    (gain_map > gain_mean * 2.0), "low_dose" as a display LUT name
    //    (pediatric_low_dose), "standard" only as the English word. None is
    //    defined as a detector mode anywhere.
    //  - the requirement it cited says something else. REQ-P1A-014 is
    //    "Calibration File Loading (Offset)"; nonlinearity is explicitly out
    //    of SPEC-XPE-P1A's scope, which points at a SPEC-XPE-P1D that does
    //    not exist.
    //
    // The two "mode" vocabularies are the root: this list held DETECTOR modes
    // while the rest of the repository puts OPERATING modes ("clinical",
    // "research", "production", "test") under the same JSON key.
    //
    // So the stage no longer decides anything from "mode". It reports that it
    // did nothing, because silence cannot be told apart from a correction
    // that ran: the frame leaves this stage byte-identical either way, and
    // XPE_FLAG_NONLINEARITY_CORRECTED is absent in both cases too. Turning the
    // error into a silent pass would drop the one signal the caller had.
    xpe_alert_push("nonlinearity correction did nothing: no LUT is loaded and "
                   "panel.linear is not \"false\", so the frame passed through "
                   "unchanged; load a LUT with xpe_calib_load_nonlin_lut() if "
                   "this panel needs correcting (issue #196)",
                   XPE_ALERT_WARNING);
    (void)img;
    return XPE_OK;
}

XpeErrorCode xpe_nonlinearity_correct(XpeImageBuffer* img,
                                       const char* configJsonOrNull)
{
    return xpe_nonlinearity_apply(img, configJsonOrNull, nullptr);
}
