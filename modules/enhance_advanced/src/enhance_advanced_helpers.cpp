/**
 * @file enhance_advanced_helpers.cpp
 * @brief Implementation of internal.h config parsers and shared utilities.
 *
 * Provides the JSON config parsing functions declared in internal.h namespace
 * xpe::enhance_advanced::config. Also provides the module init-state query
 * helper used by processing functions.
 *
 * SPEC: SPEC-XPE-P2-ADV
 * REQ-ADV-001: Config validation
 */

#include "xpe/enhance_advanced/internal.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace xpe {
namespace enhance_advanced {
namespace config {

/* ============================================================================
 * #145 (QA-B-61): unconsumed config keys, reported once per configuration
 * ============================================================================ */

/**
 * @brief Report top-level config keys this module does not consume -- once per
 *        distinct set of unknown keys, per thread.
 *
 * QA-B-60 wired this warning into the two init entry points and deliberately
 * left the per-call parsers alone: these run per frame, so one unknown key would
 * become one alert per frame and fill the queue with copies of a single fact.
 * That objection was about FREQUENCY, not content, so removing the frequency
 * makes the warning wirable.
 *
 * TWO DESIGN CHOICES, both of which change behaviour and so are stated here:
 *
 * 1. WHAT IS COMPARED: the set of unknown key NAMES, sorted and joined -- not
 *    the config string. A caller that varies a value every frame
 *    ({"step_size": 0.31} then 0.32 ...) would defeat whole-string comparison
 *    and get a warning per frame anyway, which is the case this exists to
 *    prevent. The names are already being collected to build the message, so
 *    the set costs a sort of a handful of short strings.
 *
 * 2. WHERE THE MEMORY LIVES: thread_local, owned by the caller and passed in.
 *    A module-global would let two threads erase each other's memory -- thread A
 *    warns, thread B's different config overwrites the record, and A's next
 *    frame warns again; the per-frame flood returns whenever two threads run.
 *    thread_local has no cross-thread visibility at all, so REQ-ADV-090's
 *    reentrancy ("reentrant with independent caller-supplied buffers") is
 *    unaffected: no result depends on it, and no thread can observe another's.
 *
 *    This raised a tension with the requirement's ORIGINAL wording ("No global
 *    mutable state shall be modified during processing calls"), which was
 *    reported rather than reinterpreted here. REQ-ADV-090 was amended
 *    2026-09-12 to name the property it protects instead: no cross-thread
 *    sharing, and no output dependence on state carried over from another call.
 *    The amendment permits thread_local narrowly -- invisible to other threads,
 *    not influencing any output, existing only to suppress duplicate
 *    diagnostics -- and explicitly does NOT extend to a mutex-protected
 *    module-global, since serialising access does not remove the cross-thread
 *    dependence. Both permitted properties are measured, not asserted, by
 *    tests/test_config_warning_once.cpp (identical outputs with and without the
 *    state; concurrent threads do not cross).
 *
 * @param json         Caller's config string; NULL means defaults, never warned.
 * @param knownKeys    Keys this parser consumes.
 * @param knownCount   Length of @p knownKeys.
 * @param nestedObject Optional object key whose contents are also consumed
 *                     (the MFP schema accepts a nested "mfp" object); NULL when
 *                     the schema is flat.
 * @param fnLabel      Entry-point name for the message.
 * @param lastWarned   Caller-owned thread_local memory of the last warned set.
 */
void warn_unconsumed_keys_once(const char*        json,
                               const char* const* knownKeys,
                               size_t             knownCount,
                               const char*        nestedObject,
                               const char*        fnLabel,
                               std::string&       lastWarned)
{
    if (json == nullptr) return;   // defaults are an ordinary call, not a mistake

    auto cfg = nlohmann::json::parse(json, nullptr, false);
    if (cfg.is_discarded() || !cfg.is_object()) return;   // malformed input is the parser's business

    auto isKnown = [&](const std::string& key) {
        for (size_t i = 0; i < knownCount; ++i) {
            if (key == knownKeys[i]) return true;
        }
        return false;
    };

    std::vector<std::string> unknown;
    for (auto it = cfg.begin(); it != cfg.end(); ++it) {
        if (nestedObject != nullptr && it.key() == nestedObject && it.value().is_object()) {
            // The nested object is consumed; its CONTENTS are what to check.
            for (auto inner = it.value().begin(); inner != it.value().end(); ++inner) {
                if (!isKnown(inner.key())) unknown.push_back(inner.key());
            }
            continue;
        }
        if (!isKnown(it.key())) unknown.push_back(it.key());
    }

    if (unknown.empty()) {
        // A correct config must also CLEAR the memory: otherwise a caller that
        // fixes its typo and then reintroduces it would stay silent the second
        // time, and the warning would be worth less than it looks.
        lastWarned.clear();
        return;
    }

    std::sort(unknown.begin(), unknown.end());
    std::string signature;
    for (const auto& k : unknown) {
        signature += k;
        signature += '\x1f';           // a separator no JSON key can contain
    }
    if (signature == lastWarned) return;   // same set as last time: already said
    lastWarned = signature;

    for (const auto& k : unknown) {
        char msg[192];
        std::snprintf(msg, sizeof(msg),
                      "%s config key '%s' is not read by this entry point and has "
                      "no effect", fnLabel, k.c_str());
        xpe_alert_push(msg, XPE_ALERT_WARNING);
    }
}

void warn_inert_keys_once(const char*     json,
                          const InertKey* inertKeys,
                          size_t          inertCount,
                          const char*     nestedObject,
                          const char*     fnLabel,
                          std::string&    lastWarned)
{
    if (json == nullptr) return;   // defaults are an ordinary call, not a mistake

    // #162 (QA-B-140): an EMPTY list is NOT "nothing to do". It is a call in
    // which nothing is inert -- exactly the config that must CLEAR the memory,
    // for the same reason the empty-`present` branch below clears it. Returning
    // early here let a caller that gates the list by value (multiscale gates on
    // num_levels) skip the reset entirely, so a caller that fixed its config and
    // then re-broke it was never told again.
    if (inertKeys == nullptr || inertCount == 0) {
        lastWarned.clear();
        return;
    }

    auto cfg = nlohmann::json::parse(json, nullptr, false);
    if (cfg.is_discarded() || !cfg.is_object()) return;

    // The parser accepts the same keys nested under one object, so look there
    // too -- otherwise the warning would be silent for exactly half the callers.
    auto written = [&](const char* key) {
        if (cfg.contains(key)) return true;
        if (nestedObject != nullptr && cfg.contains(nestedObject)
            && cfg[nestedObject].is_object()) {
            return cfg[nestedObject].contains(key);
        }
        return false;
    };

    // Only keys the caller actually WROTE are worth a warning: listing an inert
    // key the caller never set would make the warning fire on every config and
    // therefore distinguish nothing.
    std::vector<const InertKey*> present;
    for (size_t i = 0; i < inertCount; ++i) {
        if (written(inertKeys[i].key)) present.push_back(&inertKeys[i]);
    }

    if (present.empty()) {
        lastWarned.clear();   // same reason as above: let the next one be heard
        return;
    }

    std::vector<std::string> names;
    names.reserve(present.size());
    for (const auto* k : present) names.emplace_back(k->key);
    std::sort(names.begin(), names.end());

    std::string signature;
    for (const auto& n : names) {
        signature += n;
        signature += '\x1f';
    }
    if (signature == lastWarned) return;
    lastWarned = signature;

    for (const auto* k : present) {
        char msg[256];
        std::snprintf(msg, sizeof(msg),
                      "%s config key '%s' is recognised but has no effect: %s",
                      fnLabel, k->key, k->reason);
        xpe_alert_push(msg, XPE_ALERT_WARNING);
    }
}

/* ============================================================================
 * MFP Config Parser (SWU-2.5)
 * ============================================================================ */

bool parse_mfp_config(const char* json,
                      int&   outLevels,
                      float& outEdgeGain,
                      float& outTextureGain,
                      float& outFlatGain,
                      float& outNoiseThreshold) {
    // Apply defaults first
    outLevels        = XPE_MFP_DEFAULT_LEVELS;
    outEdgeGain      = XPE_MFP_DEFAULT_EDGE_GAIN;
    outTextureGain   = XPE_MFP_DEFAULT_TEXTURE_GAIN;
    outFlatGain      = XPE_MFP_DEFAULT_FLAT_GAIN;
    outNoiseThreshold = XPE_MFP_DEFAULT_NOISE_THRESH;

    if (json == nullptr) {
        return true;
    }

    try {
        auto cfg = nlohmann::json::parse(json, nullptr, false);
        if (cfg.is_discarded()) {
            return false;
        }

        // @MX:NOTE: Supports both flat schema and nested "mfp" key schema.
        // Nested takes precedence: if "mfp" object exists, read keys from it;
        // otherwise fall back to flat top-level keys for backward compatibility.
        nlohmann::json src = cfg;
        if (cfg.contains("mfp") && cfg["mfp"].is_object()) {
            src = cfg["mfp"];
        }

        // Parse keys from resolved source (nested or flat)
        // Backward compat: also accept "levels" (flat schema legacy key).
        // Read FIRST so that "num_levels", when also present, overwrites it:
        // the current name wins over the legacy one (#162, QA-B-79).
        if (src.contains("levels") && src["levels"].is_number_integer()) {
            int val = src["levels"].get<int>();
            outLevels = std::clamp(val, XPE_MFP_MIN_LEVELS, XPE_MFP_MAX_LEVELS);
        }
        if (src.contains("num_levels") && src["num_levels"].is_number_integer()) {
            int val = src["num_levels"].get<int>();
            outLevels = std::clamp(val, XPE_MFP_MIN_LEVELS, XPE_MFP_MAX_LEVELS);
        }

        if (src.contains("edge_gain") && src["edge_gain"].is_number()) {
            float val = src["edge_gain"].get<float>();
            outEdgeGain = std::clamp(val, 0.0f, 5.0f);
        }

        if (src.contains("texture_gain") && src["texture_gain"].is_number()) {
            float val = src["texture_gain"].get<float>();
            outTextureGain = std::clamp(val, 0.0f, 5.0f);
        }

        if (src.contains("flat_gain") && src["flat_gain"].is_number()) {
            float val = src["flat_gain"].get<float>();
            outFlatGain = std::clamp(val, 0.0f, 5.0f);
        }

        if (src.contains("noise_threshold") && src["noise_threshold"].is_number()) {
            float val = src["noise_threshold"].get<float>();
            outNoiseThreshold = std::clamp(val, 0.0f, 50.0f);
        }

        return true;
    } catch (const nlohmann::json::exception&) {
        return false;
    }
}

/* ============================================================================
 * Fractional Config Parser (SWU-2.6)
 * ============================================================================ */

bool parse_fractional_config(const char* json,
                             int&   outIterations,
                             bool&  outSafetyViolation) {
    // @MX:ANCHOR: [AUTO] SAF-100 forbidden key gate in fractional config parser
    // @MX:REASON: Safety-critical — overshoot limiting bypass must be blocked at config parse level (IEC 62304 Class B)
    // @MX:SPEC: REQ-ADV-051, SAF-100

    outSafetyViolation = false;

    // Apply defaults
    outIterations = XPE_FRAC_DEFAULT_ITER;

    if (json == nullptr) {
        return true;
    }

    try {
        auto cfg = nlohmann::json::parse(json, nullptr, false);
        if (cfg.is_discarded()) {
            return false;
        }

        // SAF-100 (REQ-ADV-051): Reject any attempt to configure overshoot limiting.
        // These keys are forbidden because overshoot limiting is mandatory and
        // non-configurable under IEC 62304 Class B safety requirements.
        const std::vector<const char*> forbiddenKeys = {
            "overshoot_limiting",
            "overshoot_limit",
            "overshoot_factor",
            "disable_overshoot_limit",
            "overshoot"  // Also catch bare "overshoot" used inside nested objects
        };

        for (const char* key : forbiddenKeys) {
            if (cfg.contains(key)) {
                outSafetyViolation = true;
                return false;  // SAF-100 violation: forbidden key at top level
            }
        }

        // Check nested "safety" object for forbidden keys
        if (cfg.contains("safety") && cfg["safety"].is_object()) {
            for (const char* key : forbiddenKeys) {
                if (cfg["safety"].contains(key)) {
                    outSafetyViolation = true;
                    return false;  // SAF-100 violation: forbidden key in safety object
                }
            }
        }

        if (cfg.contains("iterations") && cfg["iterations"].is_number_integer()) {
            int val = cfg["iterations"].get<int>();
            outIterations = std::clamp(val, 1, XPE_FRAC_MAX_ITER);
        }

        // #162 (QA-B-139): `step_size` was read here and clamped to
        // [0.01, 1.0]. Nothing consumed the result -- FractionalConfig carries
        // only `order` -- so the clamp produced a value for a debug log and
        // nothing else. Parsing it made the code claim an effect it does not
        // have. The key is still recognised by the caller's known-key list, and
        // the caller reports it through the inert-key warning.

        return true;
    } catch (const nlohmann::json::exception&) {
        return false;
    }
}

/* ============================================================================
 * Collimation Config Parser (SWU-2.8)
 * ============================================================================ */

bool parse_collimation_config(const char* json,
                              float& outConfidenceStrictness,
                              float& outMinAreaRatio,
                              int&   outBorderMargin) {
    // Apply defaults
    outConfidenceStrictness = XPE_COL_DEFAULT_CONF_STRICTNESS;
    outMinAreaRatio   = XPE_COL_DEFAULT_MIN_AREA_RATIO;
    outBorderMargin   = XPE_COL_DEFAULT_BORDER_MARGIN;

    if (json == nullptr) {
        return true;
    }

    try {
        auto cfg = nlohmann::json::parse(json, nullptr, false);
        if (cfg.is_discarded()) {
            return false;
        }

        // #164: named `sensitivity` until 2026-09-16. See internal.h for why the
        // name moved rather than the arithmetic.
        if (cfg.contains("confidence_strictness") &&
            cfg["confidence_strictness"].is_number()) {
            float val = cfg["confidence_strictness"].get<float>();
            outConfidenceStrictness = std::clamp(val, 0.0f, 1.0f);
        }

        if (cfg.contains("min_area_ratio") && cfg["min_area_ratio"].is_number()) {
            float val = cfg["min_area_ratio"].get<float>();
            outMinAreaRatio = std::clamp(val, 0.01f, 1.0f);
        }

        if (cfg.contains("border_margin") && cfg["border_margin"].is_number_integer()) {
            int val = cfg["border_margin"].get<int>();
            outBorderMargin = std::clamp(val, 0, 64);
        }

        return true;
    } catch (const nlohmann::json::exception&) {
        return false;
    }
}

} // namespace config
} // namespace enhance_advanced
} // namespace xpe
