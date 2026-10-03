/**
 * @file pipeline.cpp
 * @brief Full pre-processing pipeline integration (stages 0.5-4)
 *        REQ-P1A-095 to REQ-P1A-101
 *        Extended: pre-loaded calibration state, batch processing
 * SPEC: SPEC-XPE-P1A v1.0.0
 *
 * REFACTORED: Uses new 3-arg API (input, output, metadata)
 * Calibration maps loaded via g_calib (xpe_calib_load_offset/gain/defect_map)
 */

#include "xpe_strict_parse.hpp"
#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <memory>
#include <new>

/* =========================================================================
 * Full Pre-Processing Pipeline (stages 0.5-4)
 * REQ-P1A-095 to REQ-P1A-101
 * ========================================================================= */

#ifdef XPE_CACHE_TEST_HOOKS
void (*xpe_calib_after_set_commit_hook)() = nullptr;
void (*xpe_calib_in_set_commit_hook)() = nullptr;
void (*xpe_pipeline_after_stage_hook)(int) = nullptr;
#endif

namespace {
    // Pipeline configuration from JSON
    struct PipelineConfig {
        bool bypassReadout{false};
        bool bypassTemp{false};
        bool bypassOffset{false};
        bool bypassNonlinearity{false};

        // QA-A-111 (#186): the nonlinearity stage reads `panel.*` from the caller's config, so the parsed
        // configuration travels with the flags (QA-A-209b: parsed once, here, for the whole call).
        XpeConfigDoc doc;
        bool bypassGain{false};
        bool bypassBinning{false};
        bool bypassDefect{false};
        bool bypassGhost{false};

        float detectorTempC{25.0f};
        int32_t binningMode{1};

        // QA-A-202 (#233): a number that is not a number is a refusal, not an exception. Every conversion
        // below is strict (the whole value, finite, in range -- xpe_strict_parse.hpp); a failure is
        // XPE_ERR_CONFIG_INVALID and `*out` is left as it was.
        static XpeErrorCode fromJson(const char* configJson, PipelineConfig* out) {
            PipelineConfig cfg;

            // QA-A-209 / QA-A-209b: the text is parsed ONCE into its top-level members. A text that is not one valid
            // JSON object, or a member name given twice (any name -- including the nonlinearity stage's keys, which
            // are read later, so a duplicate among them cannot fail the run half way), is XPE_ERR_CONFIG_INVALID; a
            // name only inside a nested object is not given; NULL is every default; an empty text is a refusal.
            XpeErrorCode rc = xpe_config_parse(configJson, &cfg.doc);
            if (rc != XPE_OK) return rc;

            std::string v;
            const struct { const char* key; bool* flag; } flags[] = {
                {"bypassReadout", &cfg.bypassReadout},         {"bypassTemp", &cfg.bypassTemp},
                {"bypassOffset", &cfg.bypassOffset},           {"bypassNonlinearity", &cfg.bypassNonlinearity},
                {"bypassGain", &cfg.bypassGain},               {"bypassBinning", &cfg.bypassBinning},
                {"bypassDefect", &cfg.bypassDefect},           {"bypassGhost", &cfg.bypassGhost},
            };
            for (const auto& f : flags) {
                if (cfg.doc.getString(f.key, &v) && v == "true") *f.flag = true;
            }

            // Parse temperature (a string or a bare number; an empty value is not given)
            if (cfg.doc.getString("detectorTempC", &v) && !v.empty() && !xpe_strict::parse_float(v, &cfg.detectorTempC))
                return XPE_ERR_CONFIG_INVALID;

            // Parse binning mode
            if (cfg.doc.getString("binningMode", &v) && !v.empty() && !xpe_strict::parse_int(v, &cfg.binningMode))
                return XPE_ERR_CONFIG_INVALID;

            *out = std::move(cfg);
            return XPE_OK;
        }
    };

    /**
     * width * height * elemSize as a size_t. False when a side is 0 or the product does not fit: two uint32
     * sides need 64 bits, and the byte count needs one or two more, so the unchecked product wrapped to a
     * small number that then passed the size checks and sized the copies (QA-A-205b, Codex #29 A3).
     */
    bool frame_bytes(uint32_t width, uint32_t height, size_t elemSize, size_t* bytes) {
        if (width == 0 || height == 0) return false;
        if (width > SIZE_MAX / height) return false;
        const size_t pixels = static_cast<size_t>(width) * height;
        if (pixels > SIZE_MAX / elemSize) return false;
        *bytes = pixels * elemSize;
        return true;
    }

    /**
     * Whether the frame the pipeline hands back is float32 -- some stage from the gain stage on runs -- or still
     * the uint16 frame it was given. Mirrors the stage conditions in pipeline_core.
     */
    bool final_result_is_float(const PipelineConfig& cfg, const void* ghostHandle) {
        return !cfg.bypassGain
            || (!cfg.bypassBinning && cfg.binningMode > 1)
            || !cfg.bypassDefect
            || (!cfg.bypassGhost && ghostHandle != nullptr);
    }

    /**
     * Whether the buffer a stage is about to read is what that stage takes: this format, at least this many bytes.
     * The stages after the gain stage take float32; a buffer that is not one -- the uint16 frame of a bypassed
     * gain stage, before QA-A-208 -- was read as floats anyway. On the normal path this never fails; it is the
     * check that turns a future slip in the stage chain into an error instead of a read past a buffer
     * (QA-A-208, Codex #34).
     */
    bool stage_input_is(const XpeImageBuffer& b, decltype(XpeImageBuffer::format) format, size_t bytes) {
        return b.data != nullptr && b.format == format && b.dataSize >= bytes;
    }

    /**
     * @brief Internal pipeline core using new 3-arg API.
     *
     * Uses g_calib for calibration maps (loaded via xpe_calib_load_* functions).
     * Manages buffer transitions: uint16 → uint16 (offset) → float32 (gain) → ...
     *
     * @param img         [in]     Input image (uint16)
     * @param meta        [in/out] Metadata
     * @param ghostHandle [in]     Ghost corrector handle
     * @param cfg         [in]     Pipeline configuration
     * @param calib       [in]     The calibration set this frame is processed with: every stage that reads a
     *                             map reads it from here, never from g_calib, so a set loaded while the frame
     *                             runs does not reach it (QA-A-202d, Codex #32 A2)
     * @return XPE_OK or error code
     */
    XpeErrorCode pipeline_core(
        const XpeImageBuffer* img,
        XpeImageMetadata* meta,
        void* ghostHandle,
        const PipelineConfig& cfg,
        const CalibSnapshot& calib)
    {
        if (!img || !img->data) return XPE_ERR_INVALID_INPUT;

        // QA-A-205b (#234, Codex #29 A1-A3): before any stage runs, the frame the caller describes is checked
        // against the room the caller gives -- for the frame that is read AND for the frame that will be written
        // back, which is float32 (4 bytes a pixel) when any stage from the gain stage on runs.
        //  * A side that is 0, or a byte count that does not fit in a size_t: XPE_ERR_INVALID_INPUT.
        //  * 0 < dataSize < the uint16 frame the dimensions describe: XPE_ERR_INVALID_INPUT -- the buffer does
        //    not hold the input (QA-A-205, the #123 input rule: a non-zero size that is too small).
        //  * dataSize smaller than the final frame (0 included): XPE_ERR_BUFFER_TOO_SMALL -- the pipeline writes
        //    its result into the buffer it read from, so this is an output buffer, and an output dataSize of
        //    0 is not "unspecified" (api-spec.md, output buffers; a float result written into a buffer of 0 or
        //    width*height*2 bytes used to come back as OK and a truncated frame).
        // Nothing is read, written or flagged before these refusals.
        size_t inputBytes = 0;
        size_t floatBytes = 0;
        if (!frame_bytes(img->width, img->height, sizeof(uint16_t), &inputBytes) ||
            !frame_bytes(img->width, img->height, sizeof(float), &floatBytes))
            return XPE_ERR_INVALID_INPUT;
        const size_t outputBytes = final_result_is_float(cfg, ghostHandle) ? floatBytes : inputBytes;
        if (img->dataSize != 0 && img->dataSize < inputBytes) return XPE_ERR_INVALID_INPUT;
        if (img->dataSize < outputBytes) return XPE_ERR_BUFFER_TOO_SMALL;

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
            // The work buffer holds exactly inputBytes. This copied img->dataSize bytes, and a dataSize larger
            // than the frame -- the size a caller must give for the float result of the gain stage to be
            // written back -- overran it (#234).
            std::memcpy(stage1Data.data(), img->data, inputBytes);

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
            result = xpe_offset_correct_in(calib, &stage1, &stage2, meta);
            if (result != XPE_OK) return result;

            if (meta) meta->flags |= XPE_FLAG_OFFSET_CORRECTED;
        }
#ifdef XPE_CACHE_TEST_HOOKS
        if (xpe_pipeline_after_stage_hook) xpe_pipeline_after_stage_hook(2);
#endif

        // Stage 3: Nonlinearity Correction (PRE-08) - uint16 in/out
        XpeImageBuffer stage3 = stage2;
        std::vector<uint16_t> stage3Data;

        if (!cfg.bypassNonlinearity) {
            stage3Data.resize(pixelCount);
            stage3.data = stage3Data.data();
            stage3.dataSize = stage3Data.size() * sizeof(uint16_t);

            // The stage works in place on its own copy of the stage-2 frame.
            // QA-A-104 (#184): the copy used to run only when stage 2 had no
            // buffer of its own, so with offset enabled (the default) the gain
            // stage received an all-zero frame.
            std::memcpy(stage3Data.data(), stage2.data, pixelCount * sizeof(uint16_t));

            bool applied = false;
            // QA-A-111 (#186): the stage now reads the config -- `panel.linear`
            // decides whether the correction applies at all, and it used to be
            // dropped here by passing nullptr.
            result = xpe_nonlinearity_apply_doc(&stage3, cfg.doc, &applied);
            if (result != XPE_OK) return result;

            // Set only when pixels were corrected (#184).
            if (meta && applied) meta->flags |= XPE_FLAG_NONLINEARITY_CORRECTED;
        }

        // Stage 4: Gain Correction (PRE-03) - uint16 in, float32 out (DOMAIN TRANSITION)
        XpeImageBuffer stage4;
        std::vector<float> stage4Data;
        // QA-A-211 (#233): the pixels a polynomial gain classified defective in THIS frame, handed to the defect stage
        // (a scalar map's list travels in the snapshot).
        std::vector<uint32_t> frameGainDefects;

        if (!cfg.bypassGain) {
            stage4Data.resize(pixelCount);
            stage4.width = img->width;
            stage4.height = img->height;
            stage4.bitsAllocated = 32u;
            stage4.bitsStored = 32u;
            stage4.format = XPE_PIXEL_FLOAT32;
            stage4.data = stage4Data.data();
            stage4.dataSize = stage4Data.size() * sizeof(float);

            // Use new 3-arg API: xpe_gain_correct(input, output, metadata)
            // This performs UINT16 → FLOAT32 domain transition
            result = xpe_gain_correct_in(calib, &stage3, &stage4, meta, &frameGainDefects);
            if (result != XPE_OK) return result;

            if (meta) meta->flags |= XPE_FLAG_GAIN_CORRECTED;

            // QA-A-211 (#233): pixels the gain calibration classified defective carry the uncorrected value (gain 1.0)
            // until the defect stage fills them from their neighbours.
            const uint64_t classified = static_cast<uint64_t>(calib.gain_defect_count) + frameGainDefects.size();
            if (classified > 0) {
                if (!cfg.bypassBinning && cfg.binningMode > 1) {
                    // Binning mixes a pixel with its neighbours BEFORE the defect stage: the uncorrected value would be
                    // spread into good pixels and no later stage could take it out again. Refused, not warned: the
                    // result would be a wrong image with nothing to show for it.
                    xpe_alert_push(
                        "XPE_WARN_GAIN_PIXELS_WITH_BINNING: pixels classified defective by the gain calibration cannot be "
                        "combined with binning (binningMode > 1): binning would mix their uncorrected value into their "
                        "neighbours before the defect correction can replace it; the frame was not processed",
                        XPE_ALERT_ERROR);
                    return XPE_ERR_CONFIG_INVALID;
                }
                if (cfg.bypassDefect) {
                    char msg[320];
                    std::snprintf(msg, sizeof(msg),
                        "XPE_WARN_GAIN_PIXELS_UNCORRECTED: %llu pixel(s) were classified defective by the gain calibration but no "
                        "defect correction follows (defect stage bypassed); they carry the uncorrected value with gain 1.0",
                        static_cast<unsigned long long>(classified));
                    msg[sizeof(msg) - 1] = '\0';
                    xpe_alert_push(msg, XPE_ALERT_WARNING);
                }
            }
        } else if (final_result_is_float(cfg, ghostHandle) && stage3.format == XPE_PIXEL_UINT16) {
            // QA-A-208 (Codex #34 A1): a bypassed gain stage means "gain = 1", and a float stage follows
            // (binning, defect or ghost), so the frame is converted to float32 here, explicitly -- every float
            // stage then reads a float32 buffer of its own size. It used to pass the uint16 buffer on (N*2 bytes)
            // and binning and ghost copied N*4 bytes out of it. A gain map of ones gives this frame bit for bit:
            // x * (1/1) is exact, as is the conversion of a uint16.
            stage4Data.resize(pixelCount);
            stage4.width = img->width;
            stage4.height = img->height;
            stage4.bitsAllocated = 32u;
            stage4.bitsStored = 32u;
            stage4.format = XPE_PIXEL_FLOAT32;
            stage4.data = stage4Data.data();
            stage4.dataSize = stage4Data.size() * sizeof(float);

            const uint16_t* in16 = static_cast<const uint16_t*>(stage3.data);
            for (size_t i = 0; i < pixelCount; ++i) stage4Data[i] = static_cast<float>(in16[i]);
        } else {
            // No gain correction and no float stage after it: stage4 = stage3 (uint16)
            stage4 = stage3;
        }

        // QA-A-211b (#233, Codex #71): the same refusal when the GAIN stage is bypassed. The defect stage reads the stored
        // classification (the scalar map's list, kept in the snapshot) whether or not the gain stage ran, so binning
        // would mix those pixels' original values into their neighbours and the defect stage would then fill the
        // listed positions of an already-mixed frame. The list is only delivered when the defect stage runs.
        if (cfg.bypassGain && !cfg.bypassDefect && calib.gain_defect_count > 0 &&
            !cfg.bypassBinning && cfg.binningMode > 1) {
            xpe_alert_push(
                "XPE_WARN_GAIN_PIXELS_WITH_BINNING: pixels classified defective by the gain calibration cannot be "
                "combined with binning (binningMode > 1): binning would mix their uncorrected value into their "
                "neighbours before the defect correction can replace it; the frame was not processed",
                XPE_ALERT_ERROR);
            return XPE_ERR_CONFIG_INVALID;
        }

        // Stage 5: Binning Correction (PRE-09) - float32 in/out
        XpeImageBuffer stage5 = stage4;
        std::vector<float> stage5Data;

        if (!cfg.bypassBinning && cfg.binningMode > 1) {
            stage5Data.resize(pixelCount);
            stage5.width = img->width;
            stage5.height = img->height;
            stage5.bitsAllocated = 32u;
            stage5.bitsStored = 32u;
            stage5.format = XPE_PIXEL_FLOAT32;
            stage5.data = stage5Data.data();
            stage5.dataSize = stage5Data.size() * sizeof(float);

            // In place on its own copy of the stage-4 frame. QA-A-104: it used
            // to bin stage4 while the (empty) stage5 buffer went on to the
            // defect stage, so a binned frame came out as zeros.
            if (!stage_input_is(stage4, XPE_PIXEL_FLOAT32, floatBytes)) return XPE_ERR_PROCESSING_FAILED;
            std::memcpy(stage5Data.data(), stage4.data, pixelCount * sizeof(float));
            result = xpe_binning_correct(&stage5, cfg.binningMode, nullptr);
            if (result != XPE_OK) return result;

            if (meta) meta->flags |= XPE_FLAG_BINNING_CORRECTED;
        }

        // Stage 6: Defect Correction (PRE-06) - float32 in/out
        XpeImageBuffer stage6 = stage5;
        std::vector<float> stage6Data;

        // #117 decision B: the defect map lives in the global calibration, not in
        // a caller-supplied struct. The old gate tested XpeCalibrationState::
        // defectMap, which xpe_calib_state_load never fills, so this stage never
        // ran from either entry point. xpe_defect_correct() itself reads g_calib.
        //
        // QA-A-34 (#120, api-spec 6 rule 3): map presence is NOT a gate. An
        // enabled stage whose map is missing stops the pipeline, exactly as
        // offset and gain do -- otherwise the caller reads XPE_OK for a frame
        // that was never defect-corrected (SRS-ALERT-001). Skipping is only ever
        // the result of the explicit bypass flag.
        if (!cfg.bypassDefect) {
            if (!calib.defect_map) return XPE_ERR_CALIB_NOT_LOADED;
        }

        if (!cfg.bypassDefect) {
            stage6Data.resize(pixelCount);
            stage6.width = img->width;
            stage6.height = img->height;
            stage6.bitsAllocated = 32u;
            stage6.bitsStored = 32u;
            stage6.format = XPE_PIXEL_FLOAT32;
            stage6.data = stage6Data.data();
            stage6.dataSize = stage6Data.size() * sizeof(float);

            // Defect correction: stage5(input) → stage6(output), defectMap for BPM lookup
            // meta, not nullptr: xpe_defect_correct rejects a null metadata
            // pointer. This stage never ran before the gate was fixed above, so
            // the malformed call had never been reached.
            if (!stage_input_is(stage5, XPE_PIXEL_FLOAT32, floatBytes)) return XPE_ERR_PROCESSING_FAILED;
            result = xpe_defect_correct_in(calib, &stage5, &stage6, meta, &frameGainDefects);
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
            stage7.bitsAllocated = 32u;
            stage7.bitsStored = 32u;
            stage7.format = XPE_PIXEL_FLOAT32;
            stage7.data = stage7Data.data();
            stage7.dataSize = stage7Data.size() * sizeof(float);

            // Ghost correction works in place; give it its own copy of the
            // stage-6 frame. QA-A-104: it used to correct stage6 while the
            // (empty) stage7 buffer was copied back, so the output was zeros.
            if (!stage_input_is(stage6, XPE_PIXEL_FLOAT32, floatBytes)) return XPE_ERR_PROCESSING_FAILED;
            std::memcpy(stage7Data.data(), stage6.data, pixelCount * sizeof(float));
            result = xpe_ghost_correct(ghostHandle, &stage7, meta);
            if (result != XPE_OK) return result;

            // QA-A-226 (#241): a handle without calibrated lag parameters passes the frame through unchanged, so the
            // frame is not flagged as ghost-corrected. The output stays float32 either way (final_result_is_float
            // depends on the handle being present, not on whether it corrected), so the format does not change
            // with the configuration.
            if (meta && xpe_ghost_is_calibrated(ghostHandle)) meta->flags |= XPE_FLAG_GHOST_CORRECTED;
        }

        // Copy the final frame back to the original img buffer: all of it -- outputBytes, which the room check
        // above guaranteed fits (this was min(dataSize, final size), a truncated frame reported as OK, #234).
        // Preserves the output format (float32 after gain correction, uint16 otherwise). When no stage made a
        // buffer of its own the final stage IS the caller's buffer and there is nothing to copy (a memcpy of a
        // range onto itself is undefined). A stage buffer is never smaller than the frame it holds; if one were,
        // the call refuses here, before it writes, rather than reading past the end of the stage buffer.
        const XpeImageBuffer* finalStage = &stage7;
        if (finalStage->dataSize < outputBytes ||
            finalStage->format != (final_result_is_float(cfg, ghostHandle) ? XPE_PIXEL_FLOAT32 : XPE_PIXEL_UINT16))
            return XPE_ERR_PROCESSING_FAILED;
        if (finalStage->data != img->data)
            std::memcpy(const_cast<void*>(img->data), finalStage->data, outputBytes);

        // Update img metadata to reflect actual output format
        const_cast<XpeImageBuffer*>(img)->format = finalStage->format;
        const_cast<XpeImageBuffer*>(img)->bitsAllocated = finalStage->bitsAllocated;
        const_cast<XpeImageBuffer*>(img)->bitsStored = finalStage->bitsStored;

        return XPE_OK;
    }

} // anonymous namespace

/**
 * Loads offset.xcal, gain.xcal and defect.xcal from `calibPath` as a SET: all three are read, validated and
 * allocated first (nothing global is touched), and only when every one succeeded are they committed to the
 * store, together, under one lock (QA-A-202c, #233 / Codex #27 A1). Any failure -- a missing or corrupt file,
 * an expired one, a wrong type, a malformed quality field, an allocation failure -- leaves the calibration
 * store, the quality metadata and the alerts exactly as the call found them. The three individual loaders
 * (xpe_calib_load_offset / _gain / _defect_map) are unchanged: each is its own stage + commit.
 *
 * On success `*snapshot` is the snapshot of the set just committed, taken in the same critical section
 * (QA-A-202d, Codex #32 A2): the frame, or the batch, is processed with exactly the set this call loaded, even
 * if another thread loads a different one a moment later.
 */
static XpeErrorCode load_calibration_set(const char* calibPath, CalibSnapshot* snapshot)
{
    char offsetPath[512] = {0};
    char gainPath[512] = {0};
    char defectPath[512] = {0};
    std::snprintf(offsetPath, sizeof(offsetPath), "%s/offset.xcal", calibPath);
    std::snprintf(gainPath, sizeof(gainPath), "%s/gain.xcal", calibPath);
    std::snprintf(defectPath, sizeof(defectPath), "%s/defect.xcal", calibPath);

    StagedOffset offset;
    StagedGain gain;
    StagedDefect defect;
    XpeErrorCode rc = xpe_calib_stage_offset(offsetPath, &offset);
    if (rc != XPE_OK) return rc;
    rc = xpe_calib_stage_gain(gainPath, &gain);
    if (rc != XPE_OK) return rc;
    rc = xpe_calib_stage_defect(defectPath, &defect);
    if (rc != XPE_OK) return rc;

    // QA-A-229 M4: the set replaces all three maps, so only the three files are compared with each other.
    bool sessionMixed = false;
    rc = xpe_calib_session_check_set(offset.sessionId, gain.sessionId, defect.sessionId, &sessionMixed);
    if (rc != XPE_OK) return rc;

    bool warnSession = false;
    {
        std::lock_guard<std::mutex> lock(g_calib_mutex);
        warnSession = xpe_calib_session_transition_locked(sessionMixed);   // decided in the commit's own critical section
        xpe_calib_commit_offset_locked(offset);
        xpe_calib_commit_gain_locked(gain);
        xpe_calib_commit_defect_locked(defect);
        *snapshot = xpe_calib_snapshot_locked();
#ifdef XPE_CACHE_TEST_HOOKS
        if (xpe_calib_in_set_commit_hook) xpe_calib_in_set_commit_hook();
#endif
    }
#ifdef XPE_CACHE_TEST_HOOKS
    if (xpe_calib_after_set_commit_hook) xpe_calib_after_set_commit_hook();
#endif
    xpe_calib_session_warn(warnSession);
    xpe_calib_after_gain_commit(gain);
    return XPE_OK;
}

// @MX:WARN: [AUTO] Re-reads the three calibration files on every call (about
// 475 ms at 3072x3072). The per-frame path is xpe_calib_state_load() once plus
// xpe_preprocess_pipeline_ex() per frame -- see the header (QA-A-105, #179).
// @MX:REASON: SRS-CALIB-PERF-003 budgets 200 ms for loading all three files at
// startup, not per frame; per-frame loading alone exceeds the 500 ms frame
// budget of SRS-CALIB-PERF-001.
// @MX:ANCHOR: [AUTO] xpe_preprocess_pipeline — full pipeline integration
// @MX:REASON: Main pipeline entry point; all correction stages fan in here
// @MX:SPEC: REQ-P1A-095 to REQ-P1A-101
static XpeErrorCode pipeline_impl(XpeImageBuffer* img,
                                  XpeImageMetadata* meta,
                                  const char* calibPath,
                                  void* ghostHandle,
                                  const char* configJsonOrNull)
{
    if (!img || !meta) return XPE_ERR_INVALID_INPUT;

    // The configuration is read first: a refused configuration must not have loaded anything.
    PipelineConfig cfg;
    const XpeErrorCode cfgRc = PipelineConfig::fromJson(configJsonOrNull, &cfg);
    if (cfgRc != XPE_OK) return cfgRc;

    // Load the calibration set (all three files, or none -- see load_calibration_set); the frame is processed
    // with the set just loaded. With no path, with the set that is current as the frame starts.
    CalibSnapshot calib;
    if (calibPath) {
        const XpeErrorCode loadRc = load_calibration_set(calibPath, &calib);
        if (loadRc != XPE_OK) return loadRc;
    } else {
        calib = xpe_calib_snapshot();
    }

    return pipeline_core(img, meta, ghostHandle, cfg, calib);
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
static XpeErrorCode pipeline_ex_impl(XpeImageBuffer* img,
                                          XpeImageMetadata* meta,
                                          const void* calibState,
                                          void* ghostHandle,
                                          const char* configJsonOrNull)
{
    if (!img || !meta) return XPE_ERR_INVALID_INPUT;

    // Calibration should already be loaded in g_calib via xpe_calib_state_load
    PipelineConfig cfg;
    const XpeErrorCode cfgRc = PipelineConfig::fromJson(configJsonOrNull, &cfg);
    if (cfgRc != XPE_OK) return cfgRc;

    // calibState is accepted for source compatibility but carries no maps:
    // xpe_calib_state_load() loads into g_calib and leaves the struct empty by
    // contract (#117 decision B). Every stage reads g_calib.
    (void)calibState;

    // The frame is processed with the set that is current as it starts (one snapshot, every stage).
    return pipeline_core(img, meta, ghostHandle, cfg, xpe_calib_snapshot());
}

/* =========================================================================
 * Batch Processing
 * ========================================================================= */

// @MX:ANCHOR: [AUTO] xpe_preprocess_pipeline_batch — multi-frame batch processing
// @MX:REASON: Batch API for multi-frame acquisition; SIMD parallelism for offset
// @MX:WARN: Ghost correction is stateful per-handle; batch must use sequential ghost
static XpeErrorCode pipeline_batch_impl(
    XpeImageBuffer* images,
    uint32_t imageCount,
    XpeImageMetadata* metas,
    const char* calibPath,
    void* ghostHandle,
    const char* configJsonOrNull)
{
    if (!images || !metas || imageCount == 0) return XPE_ERR_INVALID_INPUT;

    // The configuration is read first: a refused configuration must not have loaded anything.
    PipelineConfig cfg;
    const XpeErrorCode cfgRc = PipelineConfig::fromJson(configJsonOrNull, &cfg);
    if (cfgRc != XPE_OK) return cfgRc;

    // Load the calibration set once (all three files, or none -- see load_calibration_set). Every frame of the
    // batch is processed with that one set ("all frames share the same calibration maps"), even if another
    // load lands while the batch runs. With no path, the set current as the batch starts.
    CalibSnapshot calib;
    if (calibPath) {
        const XpeErrorCode loadRc = load_calibration_set(calibPath, &calib);
        if (loadRc != XPE_OK) return loadRc;
    } else {
        calib = xpe_calib_snapshot();
    }

    // Process each image with graceful degradation:
    // Continue processing remaining frames even if one frame fails.
    // Return the first error encountered (or XPE_OK if all succeed).
    XpeErrorCode firstError = XPE_OK;

    for (uint32_t i = 0; i < imageCount; ++i) {
        // QA-A-202b: a frame that runs out of memory is that frame's failure, like any other error --
        // the batch carries on and reports the first one -- and it leaves the frame's metadata as found.
        const XpeImageMetadata saved = metas[i];
        XpeErrorCode result = XPE_OK;
        try {
            result = pipeline_core(&images[i], &metas[i], ghostHandle, cfg, calib);
        } catch (const std::bad_alloc&) {
            result = XPE_ERR_OUT_OF_MEMORY;
        } catch (...) {
            result = XPE_ERR_PROCESSING_FAILED;
        }
        if (result == XPE_ERR_OUT_OF_MEMORY) metas[i] = saved;
        if (result != XPE_OK) {
            if (firstError == XPE_OK) {
                firstError = result;
            }
            // Continue processing remaining frames
        }
    }

    return firstError;
}

/* =========================================================================
 * Exported entry points (QA-A-202b, Codex #23)
 *
 * The bodies above read the configuration (strings), stage the frame in up to seven buffers and call
 * stages that allocate; any of those can throw std::bad_alloc, and an exception must not leave a C ABI
 * function. Each entry point runs its body inside one try block: bad_alloc -> OUT_OF_MEMORY, anything else ->
 * PROCESSING_FAILED. A call that fails for lack of memory leaves the metadata as it found it (the stages
 * set their flags as they go, and the frame itself is written only at the very end, so on failure the
 * image is untouched and a flag would claim work that never reached it). The other error codes keep
 * their long-standing behaviour: the flags of the stages that completed stay set.
 * ========================================================================= */

#define XPE_PIPELINE_GUARD(metaPtr, call)                                                 \
    XpeImageMetadata saved{};                                                              \
    if (metaPtr) saved = *(metaPtr);                                                       \
    try {                                                                                  \
        const XpeErrorCode rc = (call);                                                    \
        if (rc == XPE_ERR_OUT_OF_MEMORY && (metaPtr)) *(metaPtr) = saved;                  \
        return rc;                                                                         \
    } catch (const std::bad_alloc&) {                                                      \
        if (metaPtr) *(metaPtr) = saved;                                                   \
        return XPE_ERR_OUT_OF_MEMORY;                                                      \
    } catch (...) {                                                                        \
        if (metaPtr) *(metaPtr) = saved;                                                   \
        return XPE_ERR_PROCESSING_FAILED;                                                  \
    }

XpeErrorCode xpe_preprocess_pipeline(XpeImageBuffer* img,
                                     XpeImageMetadata* meta,
                                     const char* calibPath,
                                     void* ghostHandle,
                                     const char* configJsonOrNull)
{
    XPE_PIPELINE_GUARD(meta, pipeline_impl(img, meta, calibPath, ghostHandle, configJsonOrNull))
}

XpeErrorCode xpe_preprocess_pipeline_ex(XpeImageBuffer* img,
                                        XpeImageMetadata* meta,
                                        const void* calibState,
                                        void* ghostHandle,
                                        const char* configJsonOrNull)
{
    XPE_PIPELINE_GUARD(meta, pipeline_ex_impl(img, meta, calibState, ghostHandle, configJsonOrNull))
}

XpeErrorCode xpe_preprocess_pipeline_batch(XpeImageBuffer* images,
                                           uint32_t imageCount,
                                           XpeImageMetadata* metas,
                                           const char* calibPath,
                                           void* ghostHandle,
                                           const char* configJsonOrNull)
{
    // The metadata of each frame is restored by the frame's own guard inside the loop; what can still
    // throw out here is reading the configuration and loading the calibration files, before any frame.
    try {
        return pipeline_batch_impl(images, imageCount, metas, calibPath, ghostHandle, configJsonOrNull);
    } catch (const std::bad_alloc&) {
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}

#undef XPE_PIPELINE_GUARD
