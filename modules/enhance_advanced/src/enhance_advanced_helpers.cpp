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

        // Parse direct keys (flat schema per design doc Section 8)
        if (cfg.contains("levels") && cfg["levels"].is_number_integer()) {
            int val = cfg["levels"].get<int>();
            outLevels = std::clamp(val, XPE_MFP_MIN_LEVELS, XPE_MFP_MAX_LEVELS);
        }

        if (cfg.contains("edge_gain") && cfg["edge_gain"].is_number()) {
            float val = cfg["edge_gain"].get<float>();
            outEdgeGain = std::clamp(val, 0.0f, 5.0f);
        }

        if (cfg.contains("texture_gain") && cfg["texture_gain"].is_number()) {
            float val = cfg["texture_gain"].get<float>();
            outTextureGain = std::clamp(val, 0.0f, 5.0f);
        }

        if (cfg.contains("flat_gain") && cfg["flat_gain"].is_number()) {
            float val = cfg["flat_gain"].get<float>();
            outFlatGain = std::clamp(val, 0.0f, 5.0f);
        }

        if (cfg.contains("noise_threshold") && cfg["noise_threshold"].is_number()) {
            float val = cfg["noise_threshold"].get<float>();
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
                             float& outStepSize) {
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
