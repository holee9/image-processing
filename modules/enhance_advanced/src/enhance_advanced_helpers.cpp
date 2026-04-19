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

namespace xpe {
namespace enhance_advanced {
namespace config {

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
        if (src.contains("num_levels") && src["num_levels"].is_number_integer()) {
            int val = src["num_levels"].get<int>();
            outLevels = std::clamp(val, XPE_MFP_MIN_LEVELS, XPE_MFP_MAX_LEVELS);
        }
        // Backward compat: also accept "levels" (flat schema legacy key)
        if (src.contains("levels") && src["levels"].is_number_integer()) {
            int val = src["levels"].get<int>();
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
                             float& outStepSize,
                             bool&  outSafetyViolation) {
    // @MX:ANCHOR: [AUTO] SAF-100 forbidden key gate in fractional config parser
    // @MX:REASON: Safety-critical — overshoot limiting bypass must be blocked at config parse level (IEC 62304 Class B)
    // @MX:SPEC: REQ-ADV-051, SAF-100

    outSafetyViolation = false;

    // Apply defaults
    outIterations = XPE_FRAC_DEFAULT_ITER;
    outStepSize   = XPE_FRAC_DEFAULT_STEP;

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

        if (cfg.contains("step_size") && cfg["step_size"].is_number()) {
            float val = cfg["step_size"].get<float>();
            outStepSize = std::clamp(val, 0.01f, 1.0f);
        }

        return true;
    } catch (const nlohmann::json::exception&) {
        return false;
    }
}

/* ============================================================================
 * Collimation Config Parser (SWU-2.8)
 * ============================================================================ */

bool parse_collimation_config(const char* json,
                              float& outSensitivity,
                              float& outMinAreaRatio,
                              int&   outBorderMargin) {
    // Apply defaults
    outSensitivity    = XPE_COL_DEFAULT_SENSITIVITY;
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

        if (cfg.contains("sensitivity") && cfg["sensitivity"].is_number()) {
            float val = cfg["sensitivity"].get<float>();
            outSensitivity = std::clamp(val, 0.0f, 1.0f);
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
