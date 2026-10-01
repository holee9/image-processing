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

#include <cmath>
#include <mutex>
#include <string>
#include <vector>

// @MX:NOTE: [AUTO] Applies the loaded nonlinearity LUT; no-op with an alert
//           when none is loaded (QA-A-127, #196)
// @MX:SPEC: SRS-CALIB-FUNC-006-EXT 6a


namespace {

/**
 * @brief SRS-CALIB-FUNC-006-EXT 6b -- the global polynomial method.
 *
 *   I_lin = c0 + I_raw*(c1 + I_raw*(c2 + I_raw*(c3 + I_raw*c4)))   (Horner)
 *
 * Coefficients live in the detector profile, which is where FUNC-006 says
 * `f_nonlin` is stored ("detector-specific and stored in calibration profile").
 * They are read as `panel.nonlin_poly_c0` .. `_c4` from the same JSON the
 * profile's other `panel.*` fields already come from -- no new file format is
 * invented for five numbers, and 6b's target is MCU/FPGA, where a 128 KB table
 * is the thing being avoided.
 *
 * @return XPE_OK                     applied (or nothing to do)
 *         XPE_ERR_INVALID_CALIB_DATA rejected -- caller falls back to 6a
 */
XpeErrorCode xpe_nonlinearity_apply_polynomial(XpeImageBuffer* img,
                                               const char* configJsonOrNull,
                                               bool* changed)
{
    if (!configJsonOrNull) return XPE_ERR_INVALID_CALIB_DATA;

    // Degree 4 per 6b; FUNC-006 caps degree at 5. Every key defaults to 0, so
    // "all five absent" is indistinguishable from "all five zero" -- and both
    // are treated as NO COEFFICIENTS below rather than as the zero polynomial.
    // A profile naming POLY with no coefficients is a configuration error; the
    // zero polynomial would silently flatten every frame to 0.
    double c[5];
    c[0] = xpe_json_get_double(configJsonOrNull, "panel.nonlin_poly_c0", 0.0);
    c[1] = xpe_json_get_double(configJsonOrNull, "panel.nonlin_poly_c1", 0.0);
    c[2] = xpe_json_get_double(configJsonOrNull, "panel.nonlin_poly_c2", 0.0);
    c[3] = xpe_json_get_double(configJsonOrNull, "panel.nonlin_poly_c3", 0.0);
    c[4] = xpe_json_get_double(configJsonOrNull, "panel.nonlin_poly_c4", 0.0);

    const bool have_coeffs =
        (c[0] != 0.0 || c[1] != 0.0 || c[2] != 0.0 || c[3] != 0.0 || c[4] != 0.0);
    if (!have_coeffs) {
        xpe_alert_push("panel.nonlinearity_mode selects the polynomial method but no "
                       "panel.nonlin_poly_c0..c4 coefficients are present; falling back "
                       "to the LUT method (SRS-CALIB-FUNC-006-EXT 6b, issue #186)",
                       XPE_ALERT_WARNING);
        return XPE_ERR_INVALID_CALIB_DATA;
    }

    for (double v : c) {
        if (!std::isfinite(v)) {
            xpe_alert_push("nonlinearity polynomial has a non-finite coefficient; "
                           "falling back to the LUT method (FUNC-006-EXT 6b, #186)",
                           XPE_ALERT_ERROR);
            return XPE_ERR_INVALID_CALIB_DATA;
        }
    }

    // The operational range. 6c's table is written for a 12-bit ADC, but this
    // buffer is uint16 and a 16-bit panel addresses the whole range, so the
    // default is the widest the pixel type can carry and the profile narrows it.
    const double adc_max_cfg = xpe_json_get_double(configJsonOrNull, "panel.adc_max", 65535.0);
    const uint32_t adc_max =
        (adc_max_cfg >= 1.0 && adc_max_cfg <= 65535.0) ? static_cast<uint32_t>(adc_max_cfg) : 65535u;

    auto horner = [&c](double x) {
        return c[0] + x * (c[1] + x * (c[2] + x * (c[3] + x * c[4])));
    };

    // 6b step 4: "Enforce monotonicity in [0, ADC_max]".
    //
    // The requirement says to check derivative root locations. This checks
    // every INTEGER input in the range instead, which is not an approximation
    // of that test but a stricter reading of the same property: the inputs are
    // uint16 pixels, so the integers ARE the domain. A polynomial whose
    // derivative dips negative between two integers without reordering them
    // cannot reorder any value this function will ever be handed, and one that
    // does reorder two integers is caught here exactly. The cost is a setup
    // loop of at most 65536 Horner evaluations, paid once per call against
    // millions of pixels.
    double previous = horner(0.0);
    for (uint32_t x = 1; x <= adc_max; ++x) {
        const double current = horner(static_cast<double>(x));
        if (current < previous) {
            xpe_alert_push("nonlinearity polynomial is non-monotone in [0, ADC_max]; "
                           "rejected and falling back to the LUT method "
                           "(SRS-CALIB-FUNC-006-EXT 6b step 5, issue #186)",
                           XPE_ALERT_WARNING);
            return XPE_ERR_INVALID_CALIB_DATA;
        }
        previous = current;
    }

    size_t n = 0;
    if (!xpe_pixel_count(img, &n)) return XPE_ERR_INVALID_INPUT;
    uint16_t* px = static_cast<uint16_t*>(img->data);

    for (size_t i = 0; i < n; ++i) {
        const double linearized = horner(static_cast<double>(px[i]));
        // Clamp to the pixel type. The polynomial is monotone over the declared
        // range, but a frame may carry values above adc_max (the same situation
        // the 6a path clamps for), and c0 can put the low end below zero.
        const double bounded = linearized < 0.0     ? 0.0
                             : linearized > 65535.0 ? 65535.0
                                                    : linearized;
        px[i] = static_cast<uint16_t>(bounded + 0.5);
    }

    if (changed) *changed = true;
    return XPE_OK;
}

} // anonymous namespace

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

    // -----------------------------------------------------------------------
    // SRS-CALIB-FUNC-006-EXT 6c: WHICH METHOD RUNS IS THE PROFILE'S DECISION.
    //
    //   "If panel.nonlinearity_mode == "LUT" use 6a; if "POLY" use 6b; if
    //    "AUTO" select LUT for CPU targets, polynomial for MCU/FPGA targets
    //    (detected via panel.target_platform field)."
    //
    // QA-A-160 (#186) implemented 6b and this selector. Until then the file
    // carried only 6a, and the precedence note below said the LUT-vs-polynomial
    // question "has to be decided then -- not inferred from whichever branch
    // happens to come first". This is that decision, and it is not ours: the
    // requirement already made it, so the code reads the profile instead of
    // ranking the two methods itself.
    //
    // This module is a CPU build, so AUTO resolves to LUT unless the profile
    // names an MCU/FPGA target. An unknown mode string falls through to the
    // 6a path rather than failing the frame -- the same shape QA-A-127 (#196)
    // retired the hard-coded "mode" list for.
    const std::string nonlin_mode =
        configJsonOrNull ? xpe_json_get_string(configJsonOrNull, "panel.nonlinearity_mode")
                         : std::string();
    const std::string target_platform =
        configJsonOrNull ? xpe_json_get_string(configJsonOrNull, "panel.target_platform")
                         : std::string();
    const bool embedded_target = (target_platform == "MCU" || target_platform == "FPGA");
    const bool want_polynomial =
        (nonlin_mode == "POLY") || (nonlin_mode == "AUTO" && embedded_target);

    if (want_polynomial) {
        const XpeErrorCode poly_rc = xpe_nonlinearity_apply_polynomial(img, configJsonOrNull, &changed);
        // 6b step 5: "If polynomial is non-monotone in operational range,
        // reject and fallback to LUT method." XPE_ERR_INVALID_CALIB_DATA is
        // that rejection; anything else (applied, or no coefficients present)
        // is final. Falling through here IS the fallback.
        if (poly_rc != XPE_ERR_INVALID_CALIB_DATA) {
            return poly_rc;
        }
    }

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

    // QA-A-140 (#196): THIS IS WHERE `if (!configJsonOrNull) return XPE_OK;`
    // USED TO BE, and removing it is the whole change.
    //
    // It arrived with the original implementation (ee2c607) carrying the
    // comment `// REQ-P1A-013: no-op when no config supplied`, and it was
    // right for what followed it THEN: the next statement parsed "mode" out of
    // the config, so with no config there was nothing to parse. QA-A-127
    // replaced that parse with the no-op REPORT below and left the guard
    // standing, which is how a sound guard came to block something it was
    // never written for.
    //
    // Its stated reason is void twice over:
    //
    //  - the requirement is gone. Old REQ-P1A-013 (ee2c607 spec.md:88) read
    //    "WHERE the detector panel profile indicates linear response ... the
    //    system SHALL bypass the correction and return XPE_OK without
    //    modifying the image". The bc22093 renumbering reassigned that number:
    //    REQ-P1A-013 in the CURRENT set is runtime defect detection (the Hampel
    //    recipe, acceptance.md:273, BP-04 TPR/FPR). Same shape as 014 and 015
    //    above -- one renumbering, several orphaned citations.
    //  - even as written it never mandated SILENCE. It mandated bypass without
    //    modification, returning XPE_OK -- both of which still hold below: the
    //    alert changes no pixel and the return stays XPE_OK. And it keyed on
    //    the PANEL PROFILE, which is handled at `panel_linear` above, not on
    //    whether a config string was supplied at all.
    //
    // The comment on the report below is what settles it: silence cannot be
    // told apart from a correction that ran. That is true whether or not a
    // config was passed -- nullptr does not make a no-op less silent.

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
    //  - the requirement it implemented is GONE FROM THE CURRENT SPEC SET,
    //    not absent. Old REQ-P1A-015 (ee2c607) said exactly this: "IF
    //    configJsonOrNull specifies an unknown detector mode, THEN the system
    //    SHALL return XPE_ERR_CONFIG_INVALID". The bc22093 renumbering moved
    //    nonlinearity out of SPEC-XPE-P1A (spec.md:49) and gave 012-015 to
    //    other requirements, and the SPEC-XPE-P1D it points at does not
    //    exist. So this guard was not baseless -- it implemented a real
    //    requirement whose home was removed. Note that the old requirement
    //    said "unknown detector mode" and enumerated NO names, so the three
    //    names above are unsourced either way: the requirement existed, the
    //    list did not follow from it.
    //
    // The two "mode" vocabularies are the root: this list held DETECTOR modes
    // while the rest of the repository puts OPERATING modes ("clinical",
    // "research", "production", "test") under the same JSON key.
    //
    // WHAT TO DO ABOUT IT, for whoever writes SPEC-XPE-P1D. The real defect
    // was that key collision, not the guard. The right fix was to give the
    // detector mode a key of its own -- removing the guard was the way around
    // the collision, taken because the requirement had no home to be checked
    // against. So: do NOT reuse "mode" for the detector mode in P1D. Give it
    // its own key, and the unknown-value rejection can come back with it.
    //
    // So the stage no longer decides anything from "mode". It reports that it
    // did nothing, because silence cannot be told apart from a correction
    // that ran: the frame leaves this stage byte-identical either way, and
    // XPE_FLAG_NONLINEARITY_CORRECTED is absent in both cases too. Turning the
    // error into a silent pass would drop the one signal the caller had.
    //
    // ONE REPORT PER CALL -- QA-A-142 (#199) REMOVED THE SUPPRESSION LATCH
    // QA-A-140 PUT HERE, AND THE REASON IS THAT ITS JUSTIFICATION DID NOT HOLD.
    //
    // QA-A-140 wrote: "the alert queue holds 64 entries with FIFO eviction, so
    // a stream of frames would push every other alert out, the #194 clamp
    // count included." That is the half that was wrong. It counted production
    // and never looked at consumption: the host drains the queue in a `finally`
    // after every native call (NativeAlertDrain.cs:109), so the queue is
    // emptied per frame and nothing is ever evicted. The symptom was never
    // information LOST; it was the same line repeating.
    //
    // WHAT THE LATCH ACTUALLY DELIVERED, MEASURED (QA-A-142, 20 runs):
    //
    //   host keeps the module up          20 calls -> 1 alert    (it worked)
    //   host is init/../shutdown per run  20 calls -> 20 alerts  (it did not)
    //
    // The second row is the only host there is. GuiPreprocessRunner.cs wraps
    // every run in init -> load -> stages -> finally { shutdown }, and
    // xpe_preprocess_shutdown() assigns g_calib = CalibrationData{} (#176,
    // deliberately clearing every module global), which put the latch back.
    // So in the host that reported #199 the suppression rate was zero.
    //
    // AND IT COULD NOT BE FIXED IN THIS MODULE. Suppressing across that host's
    // pattern means state that outlives xpe_preprocess_shutdown() -- exactly
    // what #176 forbids and what test_global_state_hygiene.cpp fails a test
    // for. Moving the flag out of g_calib does not change the measurement
    // either, because shutdown still has to clear it: measured, still 20/20.
    // De-duplicating an identical line belongs to whoever renders the log.
    //
    // WHAT IS LOST BY REMOVING IT: a host that DOES keep the module up now
    // gets one line per frame instead of one per session. No such host exists
    // today. If one appears, the decision is re-opened with its numbers --
    // this comment is the record of why there is nothing to re-open yet.
    //
    // Contrast gain_correct.cpp:374, which alerts per frame BECAUSE its
    // message carries that frame's clamped-pixel count. Frequency follows what
    // the message says, not a house style -- and this message says the same
    // thing every time, which is an argument for the CONSUMER collapsing it,
    // not for the producer withholding it.
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
