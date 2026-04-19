/**
 * @file pipeline.cpp
 * @brief Full pre-processing pipeline integration (stages 0.5-4)
 *        REQ-P1A-041 to REQ-P1A-047
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <cstdio>
#include <cstdlib>
#include <string>

/* =========================================================================
 * Full Pre-Processing Pipeline (stages 0.5-4)
 * REQ-P1A-041 to REQ-P1A-047
 * ========================================================================= */

namespace {
    // Pipeline configuration from JSON
    struct PipelineConfig {
        bool bypassReadout{false};
        bool bypassTemp{false};
        bool bypassOffset{false};
        bool bypassNonlinearity{false};
        bool bypassGain{false};
        bool bypassBinning{false};
        bool bypassDefect{false};
        bool bypassGhost{false};

        float detectorTempC{25.0f};
        int32_t binningMode{1};

        static PipelineConfig fromJson(const char* configJson) {
            PipelineConfig cfg;
            if (!configJson) return cfg;

            // Parse bypass flags
            std::string bypassStr = xpe_json_get_string(configJson, "bypassReadout");
            if (!bypassStr.empty() && bypassStr == "true") cfg.bypassReadout = true;

            bypassStr = xpe_json_get_string(configJson, "bypassTemp");
            if (!bypassStr.empty() && bypassStr == "true") cfg.bypassTemp = true;

            bypassStr = xpe_json_get_string(configJson, "bypassOffset");
            if (!bypassStr.empty() && bypassStr == "true") cfg.bypassOffset = true;

            bypassStr = xpe_json_get_string(configJson, "bypassNonlinearity");
            if (!bypassStr.empty() && bypassStr == "true") cfg.bypassNonlinearity = true;

            bypassStr = xpe_json_get_string(configJson, "bypassGain");
            if (!bypassStr.empty() && bypassStr == "true") cfg.bypassGain = true;

            bypassStr = xpe_json_get_string(configJson, "bypassBinning");
            if (!bypassStr.empty() && bypassStr == "true") cfg.bypassBinning = true;

            bypassStr = xpe_json_get_string(configJson, "bypassDefect");
            if (!bypassStr.empty() && bypassStr == "true") cfg.bypassDefect = true;

            bypassStr = xpe_json_get_string(configJson, "bypassGhost");
            if (!bypassStr.empty() && bypassStr == "true") cfg.bypassGhost = true;

            // Parse temperature
            std::string tempStr = xpe_json_get_string(configJson, "detectorTempC");
            if (!tempStr.empty()) cfg.detectorTempC = std::stof(tempStr);

            // Parse binning mode
            std::string binningStr = xpe_json_get_string(configJson, "binningMode");
            if (!binningStr.empty()) cfg.binningMode = std::stoi(binningStr);

            return cfg;
        }
    };
} // anonymous namespace

// @MX:ANCHOR: [AUTO] xpe_preprocess_pipeline — full pipeline integration
// @MX:REASON: Main pipeline entry point; all correction stages fan in here
// @MX:SPEC: REQ-P1A-041 to REQ-P1A-047
XpeErrorCode xpe_preprocess_pipeline(XpeImageBuffer* img,
                                  XpeImageMetadata* meta,
                                  const char* calibPath,
                                  void* ghostHandle,
                                  const char* configJsonOrNull)
{
    if (!img || !meta) return XPE_ERR_INVALID_INPUT;

    // Parse pipeline configuration
    const PipelineConfig cfg = PipelineConfig::fromJson(configJsonOrNull);

    XpeErrorCode result = XPE_OK;

    // Stage 0.5: Readout Artifact Validation (PRE-01)
    if (!cfg.bypassReadout) {
        int32_t artifactScore = 0;
        char msg[256] = {0};
        result = xpe_validate_readout_artifact(img, &artifactScore, msg, sizeof(msg));
        if (result != XPE_OK) return result;

        // REQ-P1A-041: post warning alert for severely corrupted images
        if (artifactScore > 80) {
            // TODO: post alert via alert queue
        }

        // REQ-P1A-042: set XPE_FLAG_READOUT_VALIDATED
        if (meta) meta->flags |= XPE_FLAG_READOUT_VALIDATED;
    }

    // Stage 1: Temperature Compensation (PRE-07)
    if (!cfg.bypassTemp) {
        result = xpe_temp_compensate(img, cfg.detectorTempC, configJsonOrNull);
        if (result != XPE_OK) return result;

        // REQ-P1A-043: set XPE_FLAG_TEMP_COMPENSATED
        if (meta) meta->flags |= XPE_FLAG_TEMP_COMPENSATED;
    }

    // Stage 2: Offset Correction (PRE-02)
    if (!cfg.bypassOffset && calibPath) {
        // Load offset map from calibration path
        XpeImageBuffer offsetMap = {0};
        char offsetPath[512] = {0};
        std::snprintf(offsetPath, sizeof(offsetPath), "%s/offset.xcal", calibPath);

        result = xpe_calib_load_offset(offsetPath, &offsetMap);
        if (result == XPE_OK) {
            result = xpe_offset_correct(img, &offsetMap);
            std::free(offsetMap.data);
            if (result != XPE_OK) return result;

            // REQ-P1A-044: set XPE_FLAG_OFFSET_CORRECTED
            if (meta) meta->flags |= XPE_FLAG_OFFSET_CORRECTED;
        }
    }

    // Stage 3: Nonlinearity Correction (PRE-08)
    if (!cfg.bypassNonlinearity) {
        result = xpe_nonlinearity_correct(img, configJsonOrNull);
        if (result != XPE_OK) return result;

        // REQ-P1A-045: set XPE_FLAG_NONLINEARITY_CORRECTED
        if (meta) meta->flags |= XPE_FLAG_NONLINEARITY_CORRECTED;
    }

    // Stage 4: Gain Correction (PRE-03) + uint16->float32 transition
    if (!cfg.bypassGain && calibPath) {
        // Load gain map from calibration path
        XpeImageBuffer gainMap = {0};
        char gainPath[512] = {0};
        std::snprintf(gainPath, sizeof(gainPath), "%s/gain.xcal", calibPath);

        result = xpe_calib_load_gain(gainPath, &gainMap);
        if (result == XPE_OK) {
            result = xpe_gain_correct(img, &gainMap);
            std::free(gainMap.data);
            if (result != XPE_OK) return result;

            // REQ-P1A-046: set XPE_FLAG_GAIN_CORRECTED
            if (meta) meta->flags |= XPE_FLAG_GAIN_CORRECTED;
        }
    }

    // Stage 5: Binning Correction (PRE-09)
    if (!cfg.bypassBinning && cfg.binningMode > 1) {
        result = xpe_binning_correct(img, cfg.binningMode, configJsonOrNull);
        if (result != XPE_OK) return result;

        // REQ-P1A-047: set XPE_FLAG_BINNING_CORRECTED
        if (meta) meta->flags |= XPE_FLAG_BINNING_CORRECTED;
    }

    // Stage 6: Defect Correction (PRE-06)
    if (!cfg.bypassDefect && calibPath) {
        // Load defect map from calibration path
        XpeImageBuffer defectMap = {0};
        char defectPath[512] = {0};
        std::snprintf(defectPath, sizeof(defectPath), "%s/defect.xcal", calibPath);

        result = xpe_calib_load_defect_map(defectPath, &defectMap);
        if (result == XPE_OK) {
            result = xpe_defect_correct(img, &defectMap, configJsonOrNull);
            std::free(defectMap.data);
            if (result != XPE_OK) return result;

            // REQ-P1A-048: set XPE_FLAG_DEFECT_CORRECTED
            if (meta) meta->flags |= XPE_FLAG_DEFECT_CORRECTED;
        }
    }

    // Stage 7: Ghost Correction (PRE-04)
    if (!cfg.bypassGhost && ghostHandle) {
        result = xpe_ghost_correct(ghostHandle, img, meta);
        if (result != XPE_OK) return result;

        // REQ-P1A-049: set XPE_FLAG_GHOST_CORRECTED
        if (meta) meta->flags |= XPE_FLAG_GHOST_CORRECTED;
    }

    return XPE_OK;
}
