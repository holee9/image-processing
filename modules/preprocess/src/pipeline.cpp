/**
 * @file pipeline.cpp
 * @brief Full pre-processing pipeline integration (stages 0.5-4)
 *        REQ-P1A-041 to REQ-P1A-047
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <cstring>
#include <cmath>
#include <algorithm>

/* =========================================================================
 * SWU-1.6: Temperature Compensation (PRE-07) - Stub implementation
 * REQ-P1A-005 to REQ-P1A-008
 * ========================================================================= */

XpeErrorCode xpe_temp_compensate(XpeImageBuffer* img,
                                 float detectorTempC,
                                 const char* configJsonOrNull)
{
    if (!img) return XPE_ERR_INVALID_INPUT;

    // REQ-P1A-005: validate temperature range [-20, +60] Celsius
    if (std::isnan(detectorTempC)) {
        detectorTempC = 25.0f; // fallback to nominal temperature
    }
    if (detectorTempC < -20.0f || detectorTempC > 60.0f) {
        return XPE_ERR_INVALID_INPUT;
    }

    size_t n = 0;
    if (!xpe_buffer_has_format(img, XPE_PIXEL_UINT16, &n)) {
        return XPE_ERR_INVALID_INPUT;
    }

    // REQ-P1A-006: apply exponential dark current model
    // I_dark(T) = I_0 * exp(-E_g / (2 * k_B * T))
    // For now: simple linear model (2% change per degree from 25C)
    const float tempDelta = detectorTempC - 25.0f;
    const float correctionFactor = 1.0f + 0.02f * tempDelta;

    auto* px = static_cast<uint16_t*>(img->data);
    for (size_t i = 0; i < n; ++i) {
        const float corrected = static_cast<float>(px[i]) / correctionFactor;
        px[i] = static_cast<uint16_t>(std::round(std::min(65535.0f, std::max(0.0f, corrected))));
    }

    (void)configJsonOrNull; // TODO: parse calibration coefficients
    return XPE_OK;
}

/* =========================================================================
 * SWU-1.7: Nonlinearity Correction (PRE-08) - Stub implementation
 * REQ-P1A-012 to REQ-P1A-015
 * ========================================================================= */

XpeErrorCode xpe_nonlinearity_correct(XpeImageBuffer* img,
                                     const char* configJsonOrNull)
{
    if (!img) return XPE_ERR_INVALID_INPUT;

    size_t n = 0;
    if (!xpe_buffer_has_format(img, XPE_PIXEL_UINT16, &n)) {
        return XPE_ERR_INVALID_INPUT;
    }

    // REQ-P1A-012: apply piecewise linear or polynomial correction
    // For now: no-op (linear detector assumed)
    // TODO: Load detector mode and coefficients from config

    (void)configJsonOrNull;
    return XPE_OK;
}

/* =========================================================================
 * SWU-1.8: Binning Correction (PRE-09) - Stub implementation
 * REQ-P1A-020 to REQ-P1A-023
 * ========================================================================= */

XpeErrorCode xpe_binning_correct(XpeImageBuffer* img,
                                int32_t binningMode,
                                const char* configJsonOrNull)
{
    if (!img) return XPE_ERR_INVALID_INPUT;

    // REQ-P1A-020: validate binning mode
    if (binningMode != 1 && binningMode != 2 && binningMode != 4) {
        return XPE_ERR_CONFIG_INVALID;
    }

    // REQ-P1A-021: no-op when binningMode == 1 (1x1, no binning)
    if (binningMode == 1) {
        return XPE_OK;
    }

    size_t n = 0;
    if (!xpe_buffer_has_format(img, XPE_PIXEL_FLOAT32, &n)) {
        return XPE_ERR_INVALID_INPUT;
    }

    // REQ-P1A-022: apply per-mode correction for gain/uniformity differences
    // For now: simple scaling based on binning factor
    const float scaleFactor = static_cast<float>(binningMode);
    auto* px = static_cast<float*>(img->data);
    for (size_t i = 0; i < n; ++i) {
        px[i] *= scaleFactor;
    }

    (void)configJsonOrNull; // TODO: load correction profile from config
    return XPE_OK;
}

/* =========================================================================
 * SWU-1.9: Readout Artifact Validation (PRE-01) - Stub implementation
 * REQ-P1A-001 to REQ-P1A-004
 * ========================================================================= */

XpeErrorCode xpe_validate_readout_artifact(const XpeImageBuffer* rawImg,
                                           int32_t* artifactScoreOut,
                                           char* msgOut,
                                           size_t msgLen)
{
    if (!rawImg || !artifactScoreOut) return XPE_ERR_INVALID_INPUT;

    size_t n = 0;
    if (!xpe_buffer_has_format(rawImg, XPE_PIXEL_UINT16, &n)) {
        return XPE_ERR_INVALID_INPUT;
    }

    // REQ-P1A-001: detect dropped columns (>10 consecutive zero columns)
    // REQ-P1A-002: detect ADC saturation (>1% pixels at max value)
    // REQ-P1A-003: detect line noise (high-frequency horizontal patterns)

    const uint32_t W = rawImg->width;
    const uint32_t H = rawImg->height;
    const auto* px = static_cast<const uint16_t*>(rawImg->data);

    // Simple detection: count zero columns
    int32_t zeroColumns = 0;
    for (uint32_t x = 0; x < W; ++x) {
        bool columnIsZero = true;
        for (uint32_t y = 0; y < H && columnIsZero; ++y) {
            if (px[y * W + x] != 0) {
                columnIsZero = false;
            }
        }
        if (columnIsZero) ++zeroColumns;
    }

    // Calculate artifact score (0 = clean, 100 = severely corrupted)
    int32_t score = (zeroColumns * 100) / static_cast<int32_t>(W);
    if (score > 100) score = 100;

    *artifactScoreOut = score;

    // REQ-P1A-004: operator-readable message
    if (msgOut && msgLen > 0) {
        std::snprintf(msgOut, msgLen, "Artifact score: %d (zero columns: %d)",
                     score, zeroColumns);
    }

    return XPE_OK;
}

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

        // REQ-P1A-043: set XPE_FLAG_TEMP_CORRECTED
        if (meta) meta->flags |= XPE_FLAG_TEMP_CORRECTED;
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
