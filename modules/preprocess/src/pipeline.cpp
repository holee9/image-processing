/**
 * @file pipeline.cpp
 * @brief Full pre-processing pipeline integration (stages 0.5-4)
 *        REQ-P1A-041 to REQ-P1A-047
 *        Extended: pre-loaded calibration state, batch processing
 * SPEC: SPEC-XPE-P1A v1.0.0
 *
 * REFACTORED: Uses new 3-arg API (input, output, metadata)
 * Calibration maps loaded via g_calib (xpe_calib_load_offset/gain/defect_map)
 */

#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <memory>

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

    /**
     * @brief Internal pipeline core using new 3-arg API.
     *
     * Uses g_calib for calibration maps (loaded via xpe_calib_load_* functions).
     * Manages buffer transitions: uint16 → uint16 (offset) → float32 (gain) → ...
     *
     * @param img         [in]     Input image (uint16)
     * @param meta        [in/out] Metadata
     * @param defectMap   [in]     Pre-loaded defect map (may be nullptr)
     * @param ghostHandle [in]     Ghost corrector handle
     * @param cfg         [in]     Pipeline configuration
     * @return XPE_OK or error code
     */
    XpeErrorCode pipeline_core(
        const XpeImageBuffer* img,
        XpeImageMetadata* meta,
        const XpeImageBuffer* defectMap,
        void* ghostHandle,
        const PipelineConfig& cfg)
    {
        if (!img || !img->data) return XPE_ERR_INVALID_INPUT;

        XpeErrorCode result = XPE_OK;
        const size_t pixelCount = static_cast<size_t>(img->width) * img->height;

        // Stage 0.5: Readout Artifact Validation (PRE-01)
        if (!cfg.bypassReadout) {
            bool hasDropped = false, hasNonuniform = false;
            XpeImageMetadata tmpMeta{};
            const XpeImageMetadata* metaPtr = meta ? meta : &tmpMeta;
            result = xpe_validate_readout_artifact(img, metaPtr, &hasDropped, &hasNonuniform);
            if (result != XPE_OK) return result;

            if (meta) meta->flags |= XPE_FLAG_READOUT_VALIDATED;
        }

        // Stage 1: Temperature Compensation (PRE-07)
        XpeImageBuffer stage1 = *img; // Start with input
        std::vector<uint16_t> stage1Data;

        if (!cfg.bypassTemp) {
            stage1Data.resize(pixelCount);
            std::memcpy(stage1Data.data(), img->data, img->dataSize);

            stage1.data = stage1Data.data();
            stage1.dataSize = stage1Data.size() * sizeof(uint16_t);

            result = xpe_temp_compensate(&stage1, cfg.detectorTempC, nullptr);
            if (result != XPE_OK) return result;

            if (meta) meta->flags |= XPE_FLAG_TEMP_COMPENSATED;
        }

        // Stage 2: Offset Correction (PRE-02) - uint16 in/out
        XpeImageBuffer stage2 = stage1;
        std::vector<uint16_t> stage2Data;

        if (!cfg.bypassOffset) {
            stage2Data.resize(pixelCount);
            stage2.data = stage2Data.data();
            stage2.dataSize = stage2Data.size() * sizeof(uint16_t);

            // Use new 3-arg API: xpe_offset_correct(input, output, metadata)
            result = xpe_offset_correct(&stage1, &stage2, meta);
            if (result != XPE_OK) return result;

            if (meta) meta->flags |= XPE_FLAG_OFFSET_CORRECTED;
        }

        // Stage 3: Nonlinearity Correction (PRE-08) - uint16 in/out
        XpeImageBuffer stage3 = stage2;
        std::vector<uint16_t> stage3Data;

        if (!cfg.bypassNonlinearity) {
            stage3Data.resize(pixelCount);
            stage3.data = stage3Data.data();
            stage3.dataSize = stage3Data.size() * sizeof(uint16_t);

            // Copy input if we didn't have a dedicated buffer
            if (stage2Data.empty()) {
                stage3Data.assign(static_cast<const uint16_t*>(stage2.data),
                                 static_cast<const uint16_t*>(stage2.data) + pixelCount);
            }

            result = xpe_nonlinearity_correct(&stage3, nullptr);
            if (result != XPE_OK) return result;

            if (meta) meta->flags |= XPE_FLAG_NONLINEARITY_CORRECTED;
        }

        // Stage 4: Gain Correction (PRE-03) - uint16 in, float32 out (DOMAIN TRANSITION)
        XpeImageBuffer stage4;
        std::vector<float> stage4Data;

        if (!cfg.bypassGain) {
            stage4Data.resize(pixelCount);
            stage4.width = img->width;
            stage4.height = img->height;
            stage4.bitsAllocated = 32;
            stage4.bitsStored = 32;
            stage4.format = XPE_PIXEL_FLOAT32;
            stage4.data = stage4Data.data();
            stage4.dataSize = stage4Data.size() * sizeof(float);

            // Use new 3-arg API: xpe_gain_correct(input, output, metadata)
            // This performs UINT16 → FLOAT32 domain transition
            result = xpe_gain_correct(&stage3, &stage4, meta);
            if (result != XPE_OK) return result;

            if (meta) meta->flags |= XPE_FLAG_GAIN_CORRECTED;
        } else {
            // No gain correction: stage4 = stage3 (uint16)
            stage4 = stage3;
        }

        // Stage 5: Binning Correction (PRE-09) - float32 in/out
        XpeImageBuffer stage5 = stage4;
        std::vector<float> stage5Data;

        if (!cfg.bypassBinning && cfg.binningMode > 1) {
            stage5Data.resize(pixelCount);
            stage5.width = img->width;
            stage5.height = img->height;
            stage5.bitsAllocated = 32;
            stage5.bitsStored = 32;
            stage5.format = XPE_PIXEL_FLOAT32;
            stage5.data = stage5Data.data();
            stage5.dataSize = stage5Data.size() * sizeof(float);

            result = xpe_binning_correct(&stage4, cfg.binningMode, nullptr);
            if (result != XPE_OK) return result;

            if (meta) meta->flags |= XPE_FLAG_BINNING_CORRECTED;
        }

        // Stage 6: Defect Correction (PRE-06) - float32 in/out
        XpeImageBuffer stage6 = stage5;
        std::vector<float> stage6Data;

        if (!cfg.bypassDefect && defectMap && defectMap->data) {
            stage6Data.resize(pixelCount);
            stage6.width = img->width;
            stage6.height = img->height;
            stage6.bitsAllocated = 32;
            stage6.bitsStored = 32;
            stage6.format = XPE_PIXEL_FLOAT32;
            stage6.data = stage6Data.data();
            stage6.dataSize = stage6Data.size() * sizeof(float);

            // Defect correction still uses defectMap parameter
            result = xpe_defect_correct(&stage5, defectMap, nullptr);
            if (result != XPE_OK) return result;

            if (meta) meta->flags |= XPE_FLAG_DEFECT_CORRECTED;
        }

        // Stage 7: Ghost Correction (PRE-04) - float32 in/out
        XpeImageBuffer stage7 = stage6;
        std::vector<float> stage7Data;

        if (!cfg.bypassGhost && ghostHandle) {
            stage7Data.resize(pixelCount);
            stage7.width = img->width;
            stage7.height = img->height;
            stage7.bitsAllocated = 32;
            stage7.bitsStored = 32;
            stage7.format = XPE_PIXEL_FLOAT32;
            stage7.data = stage7Data.data();
            stage7.dataSize = stage7Data.size() * sizeof(float);

            result = xpe_ghost_correct(ghostHandle, &stage6, meta);
            if (result != XPE_OK) return result;

            if (meta) meta->flags |= XPE_FLAG_GHOST_CORRECTED;
        }

        // Copy final result back to original img buffer.
        // Preserves the output format (float32 after gain correction, uint16 otherwise).
        // Caller must ensure img->data is large enough for the final stage data.
        const XpeImageBuffer* finalStage = &stage7;
        const size_t copySize = std::min(img->dataSize, finalStage->dataSize);
        std::memcpy(const_cast<void*>(img->data), finalStage->data, copySize);

        // Update img metadata to reflect actual output format
        const_cast<XpeImageBuffer*>(img)->format = finalStage->format;
        const_cast<XpeImageBuffer*>(img)->bitsAllocated = finalStage->bitsAllocated;
        const_cast<XpeImageBuffer*>(img)->bitsStored = finalStage->bitsStored;

        return XPE_OK;
    }

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

    // Load calibration maps to g_calib (global calibration state)
    if (calibPath) {
        // Load offset calibration (1-arg: populates g_calib internally)
        char offsetPath[512] = {0};
        std::snprintf(offsetPath, sizeof(offsetPath), "%s/offset.xcal", calibPath);
        XpeErrorCode rc = xpe_calib_load_offset(offsetPath);
        if (rc != XPE_OK) return rc;

        // Load gain calibration (1-arg: populates g_calib internally)
        char gainPath[512] = {0};
        std::snprintf(gainPath, sizeof(gainPath), "%s/gain.xcal", calibPath);
        rc = xpe_calib_load_gain(gainPath);
        if (rc != XPE_OK) return rc;

        // Load defect calibration (1-arg: populates g_calib internally)
        char defectPath[512] = {0};
        std::snprintf(defectPath, sizeof(defectPath), "%s/defect.xcal", calibPath);
        rc = xpe_calib_load_defect_map(defectPath);
        if (rc != XPE_OK) return rc;
    }

    // Execute pipeline core (g_calib is now populated)
    const PipelineConfig cfg = PipelineConfig::fromJson(configJsonOrNull);
    return pipeline_core(img, meta, nullptr, ghostHandle, cfg);
}

/* =========================================================================
 * Pre-loaded Calibration State (LEGACY - Kept for compatibility)
 * ========================================================================= */

// @MX:NOTE: [AUTO] xpe_calib_state_load — loads calibration to g_calib
// @MX:REASON: Compatibility wrapper; maps to global g_calib state
XpeErrorCode xpe_calib_state_load(void* state, const char* calibPath)
{
    if (!state || !calibPath) return XPE_ERR_INVALID_INPUT;

    auto* cs = static_cast<XpeCalibrationState*>(state);
    // Assume state is already zero-initialized

    // Load offset calibration to g_calib (1-arg: populates g_calib internally)
    char offsetPath[512] = {0};
    std::snprintf(offsetPath, sizeof(offsetPath), "%s/offset.xcal", calibPath);
    XpeErrorCode rc = xpe_calib_load_offset(offsetPath);
    cs->offsetLoaded = (rc == XPE_OK);

    // Load gain calibration to g_calib (1-arg: populates g_calib internally)
    char gainPath[512] = {0};
    std::snprintf(gainPath, sizeof(gainPath), "%s/gain.xcal", calibPath);
    rc = xpe_calib_load_gain(gainPath);
    cs->gainLoaded = (rc == XPE_OK);

    // Load defect calibration to g_calib (1-arg: populates g_calib internally)
    char defectPath[512] = {0};
    std::snprintf(defectPath, sizeof(defectPath), "%s/defect.xcal", calibPath);
    rc = xpe_calib_load_defect_map(defectPath);
    cs->defectLoaded = (rc == XPE_OK);

    return XPE_OK;
}

// @MX:NOTE: [AUTO] Safe release — no-op if state is already zero-initialized
void xpe_calib_state_release(void* state)
{
    if (!state) return;

    auto* cs = static_cast<XpeCalibrationState*>(state);

    if (cs->offsetMap.data) {
        std::free(const_cast<void*>(cs->offsetMap.data));
        cs->offsetMap.data = nullptr;
    }
    if (cs->gainMap.data) {
        std::free(const_cast<void*>(cs->gainMap.data));
        cs->gainMap.data = nullptr;
    }
    if (cs->defectMap.data) {
        std::free(const_cast<void*>(cs->defectMap.data));
        cs->defectMap.data = nullptr;
    }

    cs->offsetLoaded = false;
    cs->gainLoaded = false;
    cs->defectLoaded = false;
}

/* =========================================================================
 * Optimized Pipeline with Pre-loaded Calibration State
 * ========================================================================= */

// @MX:ANCHOR: [AUTO] xpe_preprocess_pipeline_ex — optimized pipeline with pre-loaded state
// @MX:REASON: Eliminates per-frame file I/O; used in batch and streaming scenarios
XpeErrorCode xpe_preprocess_pipeline_ex(XpeImageBuffer* img,
                                          XpeImageMetadata* meta,
                                          const void* calibState,
                                          void* ghostHandle,
                                          const char* configJsonOrNull)
{
    if (!img || !meta) return XPE_ERR_INVALID_INPUT;

    // Calibration should already be loaded in g_calib via xpe_calib_state_load
    const PipelineConfig cfg = PipelineConfig::fromJson(configJsonOrNull);

    // Get defect map from calibState (other maps use g_calib)
    const XpeImageBuffer* defectMap = nullptr;
    if (calibState) {
        const auto* cs = static_cast<const XpeCalibrationState*>(calibState);
        if (cs->defectLoaded) defectMap = &cs->defectMap;
    }

    return pipeline_core(img, meta, defectMap, ghostHandle, cfg);
}

/* =========================================================================
 * Batch Processing
 * ========================================================================= */

// @MX:ANCHOR: [AUTO] xpe_preprocess_pipeline_batch — multi-frame batch processing
// @MX:REASON: Batch API for multi-frame acquisition; SIMD parallelism for offset
// @MX:WARN: Ghost correction is stateful per-handle; batch must use sequential ghost
XpeErrorCode xpe_preprocess_pipeline_batch(
    XpeImageBuffer* images,
    uint32_t imageCount,
    XpeImageMetadata* metas,
    const char* calibPath,
    void* ghostHandle,
    const char* configJsonOrNull)
{
    if (!images || !metas || imageCount == 0) return XPE_ERR_INVALID_INPUT;

    // Load calibration once (to g_calib, 1-arg: populates g_calib internally)
    if (calibPath) {
        char offsetPath[512] = {0};
        std::snprintf(offsetPath, sizeof(offsetPath), "%s/offset.xcal", calibPath);
        XpeErrorCode rc = xpe_calib_load_offset(offsetPath);
        if (rc != XPE_OK) return rc;

        char gainPath[512] = {0};
        std::snprintf(gainPath, sizeof(gainPath), "%s/gain.xcal", calibPath);
        rc = xpe_calib_load_gain(gainPath);
        if (rc != XPE_OK) return rc;

        char defectPath[512] = {0};
        std::snprintf(defectPath, sizeof(defectPath), "%s/defect.xcal", calibPath);
        rc = xpe_calib_load_defect_map(defectPath);
        if (rc != XPE_OK) return rc;
    }

    const PipelineConfig cfg = PipelineConfig::fromJson(configJsonOrNull);

    // Process each image with graceful degradation:
    // Continue processing remaining frames even if one frame fails.
    // Return the first error encountered (or XPE_OK if all succeed).
    XpeErrorCode firstError = XPE_OK;

    for (uint32_t i = 0; i < imageCount; ++i) {
        XpeErrorCode result = pipeline_core(&images[i], &metas[i], nullptr, ghostHandle, cfg);
        if (result != XPE_OK) {
            if (firstError == XPE_OK) {
                firstError = result;
            }
            // Continue processing remaining frames
        }
    }

    return firstError;
}
