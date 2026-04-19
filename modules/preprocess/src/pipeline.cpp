/**
 * @file pipeline.cpp
 * @brief Full pre-processing pipeline integration (stages 0.5-4)
 *        REQ-P1A-041 to REQ-P1A-047
 *        Extended: pre-loaded calibration state, batch processing
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include "xpe/preprocess/xpe_preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

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
     * @brief Internal pipeline core using pre-loaded calibration maps.
     *
     * Shared by xpe_preprocess_pipeline (loads from files),
     * xpe_preprocess_pipeline_ex (uses pre-loaded state), and
     * xpe_preprocess_pipeline_batch (per-frame dispatch).
     *
     * @param img        [in/out] Image to process
     * @param meta       [in/out] Metadata
     * @param offsetMap  [in]     Pre-loaded offset map (may be nullptr)
     * @param gainMap    [in]     Pre-loaded gain map (may be nullptr)
     * @param defectMap  [in]     Pre-loaded defect map (may be nullptr)
     * @param ghostHandle [in]    Ghost corrector handle
     * @param cfg        [in]     Pipeline configuration
     * @return XPE_OK or error code
     */
    XpeErrorCode pipeline_core(
        XpeImageBuffer* img,
        XpeImageMetadata* meta,
        const XpeImageBuffer* offsetMap,
        const XpeImageBuffer* gainMap,
        const XpeImageBuffer* defectMap,
        void* ghostHandle,
        const PipelineConfig& cfg)
    {
        XpeErrorCode result = XPE_OK;

        // Stage 0.5: Readout Artifact Validation (PRE-01)
        if (!cfg.bypassReadout) {
            int32_t artifactScore = 0;
            char msg[256] = {0};
            result = xpe_validate_readout_artifact(img, &artifactScore, msg, sizeof(msg));
            if (result != XPE_OK) return result;

            if (artifactScore > 80) {
                // TODO: post alert via alert queue
            }

            if (meta) meta->flags |= XPE_FLAG_READOUT_VALIDATED;
        }

        // Stage 1: Temperature Compensation (PRE-07)
        if (!cfg.bypassTemp) {
            result = xpe_temp_compensate(img, cfg.detectorTempC, nullptr);
            if (result != XPE_OK) return result;

            if (meta) meta->flags |= XPE_FLAG_TEMP_COMPENSATED;
        }

        // Stage 2: Offset Correction (PRE-02)
        if (!cfg.bypassOffset && offsetMap && offsetMap->data) {
            result = xpe_offset_correct(img, offsetMap);
            if (result != XPE_OK) return result;

            if (meta) meta->flags |= XPE_FLAG_OFFSET_CORRECTED;
        }

        // Stage 3: Nonlinearity Correction (PRE-08)
        if (!cfg.bypassNonlinearity) {
            result = xpe_nonlinearity_correct(img, nullptr);
            if (result != XPE_OK) return result;

            if (meta) meta->flags |= XPE_FLAG_NONLINEARITY_CORRECTED;
        }

        // Stage 4: Gain Correction (PRE-03)
        if (!cfg.bypassGain && gainMap && gainMap->data) {
            result = xpe_gain_correct(img, gainMap);
            if (result != XPE_OK) return result;

            if (meta) meta->flags |= XPE_FLAG_GAIN_CORRECTED;
        }

        // Stage 5: Binning Correction (PRE-09)
        if (!cfg.bypassBinning && cfg.binningMode > 1) {
            result = xpe_binning_correct(img, cfg.binningMode, nullptr);
            if (result != XPE_OK) return result;

            if (meta) meta->flags |= XPE_FLAG_BINNING_CORRECTED;
        }

        // Stage 6: Defect Correction (PRE-06)
        if (!cfg.bypassDefect && defectMap && defectMap->data) {
            result = xpe_defect_correct(img, defectMap, nullptr);
            if (result != XPE_OK) return result;

            if (meta) meta->flags |= XPE_FLAG_DEFECT_CORRECTED;
        }

        // Stage 7: Ghost Correction (PRE-04)
        if (!cfg.bypassGhost && ghostHandle) {
            result = xpe_ghost_correct(ghostHandle, img, meta);
            if (result != XPE_OK) return result;

            if (meta) meta->flags |= XPE_FLAG_GHOST_CORRECTED;
        }

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

    const PipelineConfig cfg = PipelineConfig::fromJson(configJsonOrNull);

    // Load calibration maps from files when calibPath is provided
    XpeImageBuffer offsetMap = {};
    XpeImageBuffer gainMap = {};
    XpeImageBuffer defectMap = {};

    if (calibPath) {
        // Determine required buffer size from the input image dimensions
        // Offset map: uint16 (same as input)
        // Gain map: float32
        // Defect map: uint8
        const size_t n = static_cast<size_t>(img->width) * img->height;

        // Offset map (uint16)
        if (!cfg.bypassOffset) {
            char offsetPath[512] = {0};
            std::snprintf(offsetPath, sizeof(offsetPath), "%s/offset.xcal", calibPath);

            std::vector<uint16_t> offsetData(n, 0);
            offsetMap.data = offsetData.data();
            offsetMap.width = img->width;
            offsetMap.height = img->height;
            offsetMap.bitsAllocated = 16;
            offsetMap.bitsStored = 16;
            offsetMap.format = XPE_PIXEL_UINT16;
            offsetMap.dataSize = n * sizeof(uint16_t);

            // Use raw data pointer that persists beyond the local vector
            offsetMap.data = std::malloc(n * sizeof(uint16_t));
            if (offsetMap.data) {
                offsetMap.dataSize = n * sizeof(uint16_t);
                XpeErrorCode rc = xpe_calib_load_offset(offsetPath, &offsetMap);
                if (rc != XPE_OK) {
                    std::free(offsetMap.data);
                    offsetMap.data = nullptr;
                }
            }
        }

        // Gain map (float32)
        if (!cfg.bypassGain) {
            char gainPath[512] = {0};
            std::snprintf(gainPath, sizeof(gainPath), "%s/gain.xcal", calibPath);

            gainMap.data = std::malloc(n * sizeof(float));
            if (gainMap.data) {
                gainMap.width = img->width;
                gainMap.height = img->height;
                gainMap.bitsAllocated = 32;
                gainMap.bitsStored = 32;
                gainMap.format = XPE_PIXEL_FLOAT32;
                gainMap.dataSize = n * sizeof(float);

                XpeErrorCode rc = xpe_calib_load_gain(gainPath, &gainMap);
                if (rc != XPE_OK) {
                    std::free(gainMap.data);
                    gainMap.data = nullptr;
                }
            }
        }

        // Defect map (uint8)
        if (!cfg.bypassDefect) {
            char defectPath[512] = {0};
            std::snprintf(defectPath, sizeof(defectPath), "%s/defect.xcal", calibPath);

            defectMap.data = std::malloc(n * sizeof(uint8_t));
            if (defectMap.data) {
                defectMap.width = img->width;
                defectMap.height = img->height;
                defectMap.bitsAllocated = 8;
                defectMap.bitsStored = 8;
                defectMap.format = XPE_PIXEL_UINT8;
                defectMap.dataSize = n * sizeof(uint8_t);

                XpeErrorCode rc = xpe_calib_load_defect_map(defectPath, &defectMap);
                if (rc != XPE_OK) {
                    std::free(defectMap.data);
                    defectMap.data = nullptr;
                }
            }
        }
    }

    // Execute pipeline core with loaded maps
    XpeErrorCode result = pipeline_core(img, meta,
                                         offsetMap.data ? &offsetMap : nullptr,
                                         gainMap.data ? &gainMap : nullptr,
                                         defectMap.data ? &defectMap : nullptr,
                                         ghostHandle, cfg);

    // Clean up loaded calibration maps
    std::free(offsetMap.data);
    std::free(gainMap.data);
    std::free(defectMap.data);

    return result;
}

/* =========================================================================
 * Pre-loaded Calibration State
 * ========================================================================= */

// @MX:ANCHOR: [AUTO] xpe_calib_state_load — pre-load calibration maps
// @MX:REASON: Eliminates per-frame file I/O for batch processing; fan_in >= 2
XpeErrorCode xpe_calib_state_load(void* state, const char* calibPath)
{
    if (!state || !calibPath) return XPE_ERR_INVALID_INPUT;

    auto* cs = static_cast<XpeCalibrationState*>(state);
    // Assume state is already zero-initialized

    // We don't know dimensions yet — load by calling the file loader
    // which will populate width/height from file headers.
    // We need to allocate sufficiently large buffers.
    // Use a large default allocation for typical FPD sizes (up to 4096x4096).
    static constexpr size_t kMaxPixels = 4096u * 4096u;

    // Offset map (uint16)
    {
        char offsetPath[512] = {0};
        std::snprintf(offsetPath, sizeof(offsetPath), "%s/offset.xcal", calibPath);

        cs->offsetMap.data = std::malloc(kMaxPixels * sizeof(uint16_t));
        if (cs->offsetMap.data) {
            cs->offsetMap.dataSize = kMaxPixels * sizeof(uint16_t);
            XpeErrorCode rc = xpe_calib_load_offset(offsetPath, &cs->offsetMap);
            cs->offsetLoaded = (rc == XPE_OK);
            if (rc != XPE_OK) {
                std::free(cs->offsetMap.data);
                cs->offsetMap.data = nullptr;
                cs->offsetMap.dataSize = 0;
            }
        }
    }

    // Gain map (float32)
    {
        char gainPath[512] = {0};
        std::snprintf(gainPath, sizeof(gainPath), "%s/gain.xcal", calibPath);

        cs->gainMap.data = std::malloc(kMaxPixels * sizeof(float));
        if (cs->gainMap.data) {
            cs->gainMap.dataSize = kMaxPixels * sizeof(float);
            XpeErrorCode rc = xpe_calib_load_gain(gainPath, &cs->gainMap);
            cs->gainLoaded = (rc == XPE_OK);
            if (rc != XPE_OK) {
                std::free(cs->gainMap.data);
                cs->gainMap.data = nullptr;
                cs->gainMap.dataSize = 0;
            }
        }
    }

    // Defect map (uint8)
    {
        char defectPath[512] = {0};
        std::snprintf(defectPath, sizeof(defectPath), "%s/defect.xcal", calibPath);

        cs->defectMap.data = std::malloc(kMaxPixels * sizeof(uint8_t));
        if (cs->defectMap.data) {
            cs->defectMap.dataSize = kMaxPixels * sizeof(uint8_t);
            XpeErrorCode rc = xpe_calib_load_defect_map(defectPath, &cs->defectMap);
            cs->defectLoaded = (rc == XPE_OK);
            if (rc != XPE_OK) {
                std::free(cs->defectMap.data);
                cs->defectMap.data = nullptr;
                cs->defectMap.dataSize = 0;
            }
        }
    }

    return XPE_OK;
}

// @MX:NOTE: [AUTO] Safe release — no-op if state is already zero-initialized
void xpe_calib_state_release(void* state)
{
    if (!state) return;

    auto* cs = static_cast<XpeCalibrationState*>(state);

    if (cs->offsetMap.data) {
        std::free(cs->offsetMap.data);
        cs->offsetMap.data = nullptr;
    }
    if (cs->gainMap.data) {
        std::free(cs->gainMap.data);
        cs->gainMap.data = nullptr;
    }
    if (cs->defectMap.data) {
        std::free(cs->defectMap.data);
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

    const PipelineConfig cfg = PipelineConfig::fromJson(configJsonOrNull);

    const XpeImageBuffer* offsetMap = nullptr;
    const XpeImageBuffer* gainMap = nullptr;
    const XpeImageBuffer* defectMap = nullptr;

    if (calibState) {
        const auto* cs = static_cast<const XpeCalibrationState*>(calibState);
        if (cs->offsetLoaded) offsetMap = &cs->offsetMap;
        if (cs->gainLoaded)   gainMap = &cs->gainMap;
        if (cs->defectLoaded) defectMap = &cs->defectMap;
    }

    return pipeline_core(img, meta, offsetMap, gainMap, defectMap,
                         ghostHandle, cfg);
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
    if (!images || imageCount == 0) return XPE_ERR_INVALID_INPUT;
    if (imageCount > 0 && !metas) return XPE_ERR_INVALID_INPUT;

    const PipelineConfig cfg = PipelineConfig::fromJson(configJsonOrNull);

    // Load calibration state once for the entire batch
    XpeCalibrationState calibState = {};
    if (calibPath) {
        XpeErrorCode loadRc = xpe_calib_state_load(&calibState, calibPath);
        if (loadRc != XPE_OK) {
            // Not fatal — proceed without calibration maps
        }
    }

    const XpeImageBuffer* offsetMap = calibState.offsetLoaded ? &calibState.offsetMap : nullptr;
    const XpeImageBuffer* gainMap = calibState.gainLoaded ? &calibState.gainMap : nullptr;
    const XpeImageBuffer* defectMap = calibState.defectLoaded ? &calibState.defectMap : nullptr;

    // Process each frame
    XpeErrorCode firstError = XPE_OK;

    for (uint32_t i = 0; i < imageCount; ++i) {
        XpeErrorCode rc = pipeline_core(&images[i], &metas[i],
                                         offsetMap, gainMap, defectMap,
                                         ghostHandle, cfg);
        if (rc != XPE_OK && firstError == XPE_OK) {
            firstError = rc;
            // Continue processing remaining frames even on error
            // (IEC 62304 Class B: graceful degradation)
        }
    }

    // Release calibration state
    xpe_calib_state_release(&calibState);

    return firstError;
}
