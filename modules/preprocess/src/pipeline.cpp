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
#include "saturation.hpp"
#include "parallel.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstdint>
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
void (*xpe_pipeline_gain_frame_hook)(void*, size_t, bool) = nullptr;
#endif
// QA-A-237b (#245): the observation point after every stage, so a test can read the process's memory at each stage
// boundary (0 readout, 1 temp, 2 offset, 3 nonlinearity, 4 gain, 5 binning, 6 defect, 7 ghost, 9 the final copy).
// Test builds only: in the shipped library the macro expands to nothing.
#ifdef XPE_CACHE_TEST_HOOKS
#define XPE_STAGE_HOOK(n) do { if (xpe_pipeline_after_stage_hook) xpe_pipeline_after_stage_hook(n); } while (0)
#else
#define XPE_STAGE_HOOK(n) ((void)0)
#endif

namespace {
    // QA-A-315 (Refs #278): a whole-frame copy between two disjoint buffers, in ranges on several threads (the bytes are the memcpy's).
    // The frame copies into the working buffer and out to the caller took 2.3 ms and 1.6-1.8 ms on one thread at 3072x2560 (evidence 06).
    void parallel_copy(void* dst, const void* src, size_t bytes)
    {
        xpe_parallel_chunks(bytes, size_t{2} << 20, [&](size_t b, size_t e) {
            std::memcpy(static_cast<unsigned char*>(dst) + b, static_cast<const unsigned char*>(src) + b, e - b);
        });
    }

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

        // QA-A-296 (Refs #278): the detector's saturation level, as a raw pixel value 1..65535, or "off". Neither given
        // = the gain file's record decides; no record either = no saturation handling and one alert per call.
        bool     saturationOff{false};
        uint32_t saturationLevel{0};   // 0 = not given

        // QA-A-296: how long runs of masked pixels in a version-1 defect map are corrected: "off" (every masked pixel the
        // way it was before), "rows" (default: a horizontal run is interpolated across the line) or "rows_cols".
        XpeDefectLineOptions defectLines;

        // QA-A-296: the most threads one call may use; 0 = as many as the machine has (up to 16). 1 = single threaded. The output is the
        // same whatever the value (the split never changes a pixel).
        uint32_t maxThreads{0};

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

            // QA-A-296 (D4): the three keys below check the JSON TYPE as well. A key that is present must be a bare integer in range
            // (saturationLevel, maxThreads) or one of the fixed strings; an empty string, an object, an array, a number written as a string,
            // true/false/null, a fraction are all XPE_ERR_CONFIG_INVALID. (getString is false for an object or an array.)
            bool quoted = false;
            if (cfg.doc.find("saturationLevel") != nullptr) {
                if (!cfg.doc.getString("saturationLevel", &v, &quoted)) return XPE_ERR_CONFIG_INVALID;
                if (quoted) {
                    if (v != "off") return XPE_ERR_CONFIG_INVALID;   // "", "OFF", "12800" ...
                    cfg.saturationOff = true;
                } else {
                    double lvNum = 0.0;
                    int32_t lv = 0;
                    if (!cfg.doc.getNumber("saturationLevel", &lvNum) || !xpe_strict::parse_int(v, &lv) || lv < 1 || lv > 65535) return XPE_ERR_CONFIG_INVALID;
                    cfg.saturationLevel = static_cast<uint32_t>(lv);
                }
            }
            if (cfg.doc.find("maxThreads") != nullptr) {
                double mtNum = 0.0;
                int32_t mt = 0;
                if (!cfg.doc.getString("maxThreads", &v, &quoted) || quoted || !cfg.doc.getNumber("maxThreads", &mtNum) ||
                    !xpe_strict::parse_int(v, &mt) || mt < 1 || mt > static_cast<int32_t>(XPE_PARALLEL_MAX_THREADS)) return XPE_ERR_CONFIG_INVALID;
                cfg.maxThreads = static_cast<uint32_t>(mt);
            }
            if (cfg.doc.find("defectLineInterpolation") != nullptr) {
                if (!cfg.doc.getString("defectLineInterpolation", &v, &quoted) || !quoted) return XPE_ERR_CONFIG_INVALID;
                if (v == "off") { cfg.defectLines.rows = false; cfg.defectLines.cols = false; }
                else if (v == "rows") { cfg.defectLines.rows = true; cfg.defectLines.cols = false; }
                else if (v == "rows_cols") { cfg.defectLines.rows = true; cfg.defectLines.cols = true; }
                else return XPE_ERR_CONFIG_INVALID;
            }

            *out = std::move(cfg);
            return XPE_OK;
        }
    };

    /**
     * QA-A-296 (D4): the saturation notes of one call. Saturation alerts are at most ONE per call, INFO included: every condition adds a
     * note, and the call pushes a single alert at the end of the saturation step. The alert carries the code of the most severe note (the
     * first among equals) and the texts of all notes, so nothing is lost when conditions overlap.
     */
    struct SatNotes {
        std::vector<std::pair<int, std::string>> items;   // severity, text (the text starts with its code)
        void add(int severity, const char* text) { items.emplace_back(severity, text); }
        void push() const
        {
            if (items.empty()) return;
            size_t lead = 0;
            for (size_t i = 1; i < items.size(); ++i) if (items[i].first > items[lead].first) lead = i;
            std::string text = items[lead].second;
            for (size_t i = 0; i < items.size(); ++i) if (i != lead) text += " | " + items[i].second;
            xpe_alert_push(text.c_str(), static_cast<XpeAlertSeverity>(items[lead].first));
        }
    };

    /**
     * QA-A-296 (Refs #278): saturation handling right after a scalar-map gain stage wrote `stage4`. The level comes from
     * the caller's key, else the gain file's record, else there is none: then no handling, one alert (XPE_WARN_SATURATION_UNSET).
     * With a level: a pixel whose RAW value (the caller's frame) is >= level becomes S_out = level / median(gain), every other
     * pixel is limited to S_out. A frame whose pile-up sits at another value than the level raises an alert and changes nothing.
     * QA-A-325 (Refs #278): split in two around the gain stage. This half runs BEFORE it: it settles the level and S_out and adds the
     * same notes in the same order, and when the rule applies it fills `*fuse` and returns true -- the gain stage then applies the rule
     * to each block it writes (XpeGainSatFuse), so the frame is not read and written a second time. monitor_saturation() is the
     * other half, run after a successful gain stage. Neither half writes a pixel; a gain stage that fails pushes none of the notes,
     * as before (the notes are pushed once, at the end of the step).
     */
    bool prepare_saturation(const PipelineConfig& cfg, const CalibSnapshot& calib, const XpeImageBuffer* img,
                            const XpeImageBuffer* stage4, SatNotes* notes, XpeGainSatFuse* fuse)
    {
        if (cfg.saturationOff) return false;
        uint32_t level = cfg.saturationLevel;
        const bool fromConfig = level != 0;
        if (level == 0 && calib.gain_sat.known && calib.gain_sat.determined) level = calib.gain_sat.level;
        if (level == 0) {
            notes->add(XPE_ALERT_WARNING,
                calib.gain_sat.known
                    ? "XPE_WARN_SATURATION_UNSET: the gain calibration records the saturation level as undetermined and the "
                      "pipeline configuration gives no saturationLevel; saturated pixels are not handled (set saturationLevel, or "
                      "regenerate the gain calibration with a frame exposed to saturation; \"off\" silences this alert)"
                    : "XPE_WARN_SATURATION_UNSET: neither the pipeline configuration (saturationLevel) nor the gain calibration "
                      "gives the detector's saturation level; saturated pixels are not handled (set saturationLevel, or regenerate "
                      "the gain calibration with a frame exposed to saturation; \"off\" silences this alert)");
            return false;
        }
        const float sOut = xpe_sat_output_value(level, xpe_sat_median_for(calib));
        if (stage4->format != XPE_PIXEL_FLOAT32 || img->format != XPE_PIXEL_UINT16) return false;
        if (!(sOut > 0.0f) || !std::isfinite(sOut)) {
            // the gain plane gives no usable median: saturated pixels cannot be given their value, so nothing is changed and the call says so
            notes->add(XPE_ALERT_WARNING,
                "XPE_WARN_SATURATION_NOT_APPLIED: the saturated output value cannot be formed (the gain plane has no usable median); "
                "saturated pixels are not handled");
            return false;
        }
        if (fromConfig && calib.gain_sat.known && calib.gain_sat.determined && calib.gain_sat.level != level) {
            char msg[256];
            std::snprintf(msg, sizeof(msg),
                "XPE_INFO_SATURATION_OVERRIDE: saturationLevel %u from the pipeline configuration is used; the gain calibration records %u",
                static_cast<unsigned>(level), static_cast<unsigned>(calib.gain_sat.level));
            msg[sizeof(msg) - 1] = '\0';
            notes->add(XPE_ALERT_INFO, msg);
        }
        fuse->raw = static_cast<const uint16_t*>(img->data);
        fuse->level = level;
        fuse->sOut = sOut;
        return true;
    }

    /** QA-A-325: the second half of the saturation step (see prepare_saturation), after the gain stage applied the rule. */
    void monitor_saturation(const CalibSnapshot& calib, const XpeImageBuffer* img, uint32_t level, SatNotes* notes)
    {
        const uint16_t* raw = static_cast<const uint16_t*>(img->data);

        // Monitor (alert only, the level is never changed): every 16th row, known defect pixels left out.
        // A looser share above the pile than the calibration rule: a detector can carry a few rows of fixed 65535.
        std::vector<uint32_t> hist(XPE_SAT_BINS, 0u);
        uint64_t counted = 0;
        const uint8_t* excl = (calib.defect_map && calib.defect_width == img->width && calib.defect_height == img->height)
                                  ? calib.defect_map.get() : nullptr;
        xpe_sat_histogram(raw, img->width, img->height, 16u, excl, hist.data(), &counted);
        XpeSatParams mp;
        mp.maxAboveFrac = 5e-3;
        uint32_t seen = 0;
        uint64_t seenCount = 0;
        if (xpe_sat_find_pile(hist.data(), counted, mp, &seen, &seenCount) && seen != level) {
            char msg[320];
            std::snprintf(msg, sizeof(msg),
                "XPE_WARN_SATURATION_MISMATCH: the frame piles up at the raw value %u but the saturation level in use is %u; "
                "the level was not changed (check saturationLevel / the gain calibration for this detector)",
                static_cast<unsigned>(seen), static_cast<unsigned>(level));
            msg[sizeof(msg) - 1] = '\0';
            notes->add(XPE_ALERT_WARNING, msg);
        }
    }

    /**
     * width * height * elemSize as a size_t. False when a side is 0 or the product does not fit: two uint32
     * sides need 64 bits, and the byte count needs one or two more, so the unchecked product wrapped to a
     * small number that then passed the size checks and sized the copies (QA-A-205b, Codex #29 A3).
     */
    /**
     * QA-A-246: the number of NaN / +-infinity values in a float32 frame (exponent field all ones); 0 for any other format (an
     * integer frame has none). Branch-free bit test per pixel, one pass.
     */
    uint64_t count_non_finite(const XpeImageBuffer& b, size_t pixelCount) noexcept {
        if (b.format != XPE_PIXEL_FLOAT32 || !b.data) return 0;
        const float* f = static_cast<const float*>(b.data);
        uint64_t n = 0;
        for (size_t i = 0; i < pixelCount; ++i) {
            uint32_t bits;
            std::memcpy(&bits, &f[i], sizeof(bits));
            n += ((bits & 0x7F800000u) == 0x7F800000u) ? 1u : 0u;
        }
        return n;
    }

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
    /**
     * QA-A-237b (#245, SRS-CALIB-PERF-002): hands a stage buffer back as soon as nothing reads it any more. The
     * per-frame buffers used to live until pipeline_core returned, which put six of them (162.0 MiB at 3072x3072) on
     * top of the calibration maps and the ghost history at the last stage.
     */
    template <class V> void release_buffer(V& v) noexcept { V().swap(v); }

    bool stage_input_is(const XpeImageBuffer& b, decltype(XpeImageBuffer::format) format, size_t bytes) {
        return b.data != nullptr && b.format == format && b.dataSize >= bytes;
    }

    /**
     * Whether [a, a + aBytes) and [b, b + bBytes) share a byte -- computed on addresses as integers, and safe when a range would
     * run past the end of the address space (such a range is treated as extending to the end, so an absurd size can only make the
     * answer "overlap", never wrap to "no overlap"). A zero-length range overlaps nothing.
     */
    bool ranges_overlap(const void* a, size_t aBytes, const void* b, size_t bBytes) {
        if (aBytes == 0 || bBytes == 0) return false;
        const uintptr_t ua = reinterpret_cast<uintptr_t>(a), ub = reinterpret_cast<uintptr_t>(b);
        const uintptr_t aEnd = aBytes > UINTPTR_MAX - ua ? UINTPTR_MAX : ua + aBytes;
        const uintptr_t bEnd = bBytes > UINTPTR_MAX - ub ? UINTPTR_MAX : ub + bBytes;
        return ua < bEnd && ub < aEnd;
    }

    /**
     * SRS-CALIB-SAFE-001 (QA-A-241): offset and gain correction are mandatory by default; an explicit bypass is allowed for
     * research and diagnostics, and then the frame says so: XPE_FLAG_CORRECTION_BYPASSED on the metadata and one
     * XPE_WARN_CORRECTION_BYPASSED alert per frame, so an output that is not offset- or gain-corrected is identifiable.
     */
    void note_bypass(const PipelineConfig& cfg, XpeImageMetadata* meta) {
        if (!cfg.bypassOffset && !cfg.bypassGain) return;
        if (meta) meta->flags |= XPE_FLAG_CORRECTION_BYPASSED;
        try {
            xpe_alert_push(cfg.bypassOffset && cfg.bypassGain
                ? "XPE_WARN_CORRECTION_BYPASSED: offset and gain correction are bypassed; the output is not offset- or gain-corrected"
                : (cfg.bypassOffset
                    ? "XPE_WARN_CORRECTION_BYPASSED: offset correction is bypassed; the output is not offset-corrected"
                    : "XPE_WARN_CORRECTION_BYPASSED: gain correction is bypassed; the output is not gain-corrected"),
                XPE_ALERT_WARNING);
        } catch (...) {
            // [no-throw-boundary] advisory
        }
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
     * @param out         [out]    NULL: the result is written back into `img` (the long-standing in-place entry points).
     *                             Not NULL: the result is written into `out` and `img` is never written (SRS-CALIB-SAFE-004);
     *                             `img` then needs room for the input only
     * @return XPE_OK or error code
     */
    XpeErrorCode pipeline_core(
        const XpeImageBuffer* img,
        XpeImageMetadata* meta,
        void* ghostHandle,
        const PipelineConfig& cfg,
        const CalibSnapshot& calib,
        XpeImageBuffer* out = nullptr,
        XpeStageDiagnostics* diag = nullptr)
    {
        if (!img || !img->data) return XPE_ERR_INVALID_INPUT;
        const XpeParallelLimitGuard threadLimit(cfg.maxThreads);   // QA-A-296: for this call only
        // QA-A-246: the per-stage counts are collected here and copied to the caller's struct only when the call succeeds, so a failed
        // call leaves validMask 0. With diag == nullptr nothing below counts anything.
        XpeStageDiagnostics counts{};

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
        if (out) {
            // Separate output (SAFE-004): the input only has to hold the input; the room for the result is the output buffer's.
            if (img->dataSize == 0) return XPE_ERR_INVALID_INPUT;
            if (!out->data) return XPE_ERR_INVALID_INPUT;
            // Codex #157: not only the same pointer -- any shared byte between the input buffer (as the caller declares it) and the
            // bytes the result will occupy. The result written over part of the input would destroy it (and memcpy of overlapping
            // ranges is undefined). Nothing has been read or written yet.
            // QA-A-246b: the size is judged BEFORE the overlap. The overlap below is taken over the bytes the result WOULD occupy, so for
            // an output buffer that is too small it depends on where the allocator put it relative to the input: a small buffer placed
            // just before the input made the required float range overlap the input and the call said INVALID_INPUT instead of
            // BUFFER_TOO_SMALL (5 of 60 runs of the same test). A buffer that cannot hold the result is refused for that reason first.
            if (out->dataSize < outputBytes) return XPE_ERR_BUFFER_TOO_SMALL;
            if (ranges_overlap(img->data, img->dataSize, out->data, outputBytes)) return XPE_ERR_INVALID_INPUT;
        } else if (img->dataSize < outputBytes) {
            return XPE_ERR_BUFFER_TOO_SMALL;
        }
        // SRS-CALIB-SAFE-002: a map that expired after it was loaded stops the frame, before anything is read or written.
        const XpeErrorCode expiryRc = xpe_calib_snapshot_expiry_check(calib, XPE_EXPIRY_ALL);
        if (expiryRc != XPE_OK) return expiryRc;
        note_bypass(cfg, meta);

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
        XPE_STAGE_HOOK(0);

        // QA-A-237c (#245, SRS-CALIB-PERF-002): ONE working buffer of the float frame's size for the whole chain from the
        // temperature stage to the gain stage. The uint16 frame the point-wise stages (temperature, offset, nonlinearity) work on
        // sits in its first half and each of them works on it in place (offset and nonlinearity read and write a pixel at the
        // same index); the gain stage then writes the float frame OVER it, from the last pixel to the first, so that no float
        // pixel is written before the uint16 pixels it overlaps have been read (xpe_gain_correct_in, output aliasing input).
        // Before, each of those stages had a frame of its own: at the gain stage a uint16 frame and the float frame were alive
        // together (18.0 + 36.0 MiB at 3072x3072), now one buffer of 36.0 MiB is. Used only where the gain stage runs on a scalar
        // map and the frame is uint16 (a polynomial gain reads the whole frame before it writes and keeps the old buffers).
        // The buffer is the caller's frame copied first, so a stage that fails leaves the caller's buffer as it was.
        const bool widen = !cfg.bypassGain && calib.gain_map && img->format == XPE_PIXEL_UINT16;
        std::unique_ptr<unsigned char[]> work;
        if (widen) {
            work.reset(new unsigned char[floatBytes]);   // every byte is written before it is read: the frame first, the float frame later
            parallel_copy(work.get(), img->data, inputBytes);
        }
        uint16_t* const work16 = reinterpret_cast<uint16_t*>(work.get());
        float* const workF = reinterpret_cast<float*>(work.get());

        // Stage 1: Temperature Compensation (PRE-07)
        XpeImageBuffer stage1 = *img; // Start with input
        std::vector<uint16_t> stage1Data;
        if (widen) {
            stage1.data = work16;
            stage1.dataSize = inputBytes;
        }

        if (!cfg.bypassTemp) {
            if (!widen) {
                stage1Data.resize(pixelCount);
                // The work buffer holds exactly inputBytes. This copied img->dataSize bytes, and a dataSize larger
                // than the frame -- the size a caller must give for the float result of the gain stage to be
                // written back -- overran it (#234).
                std::memcpy(stage1Data.data(), img->data, inputBytes);

                stage1.data = stage1Data.data();
                stage1.dataSize = stage1Data.size() * sizeof(uint16_t);
            }

            result = xpe_temp_compensate(&stage1, cfg.detectorTempC, nullptr);
            if (result != XPE_OK) return result;

            if (meta) meta->flags |= XPE_FLAG_TEMP_COMPENSATED;
        }
        XPE_STAGE_HOOK(1);

        // Stage 2: Offset Correction (PRE-02) - uint16 in/out
        XpeImageBuffer stage2 = stage1;
        std::vector<uint16_t> stage2Data;

        if (!cfg.bypassOffset) {
            if (!widen) {   // (widening: stage 2 is stage 1's buffer, the offset kernel works in place)
                stage2Data.resize(pixelCount);
                stage2.data = stage2Data.data();
                stage2.dataSize = stage2Data.size() * sizeof(uint16_t);
            }

            // Use new 3-arg API: xpe_offset_correct(input, output, metadata)
            result = xpe_offset_correct_in(calib, &stage1, &stage2, meta);
            if (result != XPE_OK) return result;

            if (meta) meta->flags |= XPE_FLAG_OFFSET_CORRECTED;
            release_buffer(stage1Data);   // stage 2 holds its own frame; nothing reads stage 1 again
        }
#ifdef XPE_CACHE_TEST_HOOKS
        if (xpe_pipeline_after_stage_hook) xpe_pipeline_after_stage_hook(2);
#endif

        // Stage 3: Nonlinearity Correction (PRE-08) - uint16 in/out
        XpeImageBuffer stage3 = stage2;
        std::vector<uint16_t> stage3Data;

        if (!cfg.bypassNonlinearity) {
            // The stage works in place on its own copy of the stage-2 frame (widening: the working buffer IS that copy).
            // QA-A-104 (#184): the copy used to run only when stage 2 had no
            // buffer of its own, so with offset enabled (the default) the gain
            // stage received an all-zero frame.
            if (!widen) {
                stage3Data.resize(pixelCount);
                stage3.data = stage3Data.data();
                stage3.dataSize = stage3Data.size() * sizeof(uint16_t);
                std::memcpy(stage3Data.data(), stage2.data, pixelCount * sizeof(uint16_t));
                release_buffer(stage1Data);   // stage 3 holds its own copy: the frames it was made from are not read again
                release_buffer(stage2Data);
            }

            bool applied = false;
            // QA-A-111 (#186): the stage now reads the config -- `panel.linear`
            // decides whether the correction applies at all, and it used to be
            // dropped here by passing nullptr.
            result = xpe_nonlinearity_apply_doc(&stage3, cfg.doc, &applied);
            if (result != XPE_OK) return result;

            // Set only when pixels were corrected (#184).
            if (meta && applied) meta->flags |= XPE_FLAG_NONLINEARITY_CORRECTED;
        }
        XPE_STAGE_HOOK(3);

        // Stage 4: Gain Correction (PRE-03) - uint16 in, float32 out (DOMAIN TRANSITION)
        SatNotes satNotes;   // QA-A-296: the saturation notes of this call, pushed once after the gain stage
        XpeImageBuffer stage4;
        std::vector<float> stage4Data;
        // QA-A-211 (#233): the pixels a polynomial gain classified defective in THIS frame, handed to the defect stage
        // (a scalar map's list travels in the snapshot).
        std::vector<uint32_t> frameGainDefects;

        if (!cfg.bypassGain) {
            if (!widen) stage4Data.resize(pixelCount);
            stage4.width = img->width;
            stage4.height = img->height;
            stage4.bitsAllocated = 32u;
            stage4.bitsStored = 32u;
            stage4.format = XPE_PIXEL_FLOAT32;
            stage4.data = widen ? static_cast<void*>(workF) : static_cast<void*>(stage4Data.data());   // widening: over the uint16 frame
            stage4.dataSize = pixelCount * sizeof(float);

            // Use new 3-arg API: xpe_gain_correct(input, output, metadata)
            // This performs UINT16 → FLOAT32 domain transition
            // QA-A-325 (Refs #278): the saturation rule rides in the gain stage (prepare_saturation); its monitor follows the stage.
            XpeGainSatFuse satFuse;
            const bool satFused = calib.gain_map && img->format == XPE_PIXEL_UINT16 &&
                                  prepare_saturation(cfg, calib, img, &stage4, &satNotes, &satFuse);
            result = xpe_gain_correct_in(calib, &stage3, &stage4, meta, &frameGainDefects, satFused ? &satFuse : nullptr);
            if (result != XPE_OK) return result;

            if (meta) meta->flags |= XPE_FLAG_GAIN_CORRECTED;
            if (satFused) monitor_saturation(calib, img, satFuse.level, &satNotes);
            release_buffer(stage1Data);   // stage 4 holds its own float frame; the uint16 frames are not read again
            release_buffer(stage2Data);
            release_buffer(stage3Data);

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
            release_buffer(stage1Data);   // the converted frame is its own; the uint16 frames are not read again
            release_buffer(stage2Data);
            release_buffer(stage3Data);
        } else {
            // No gain correction and no float stage after it: stage4 = stage3 (uint16)
            stage4 = stage3;
        }
        // QA-A-296: saturation handling exists only for a scalar gain map applied to a uint16 frame; anything else says so once.
        if (!cfg.saturationOff && !(!cfg.bypassGain && calib.gain_map && img->format == XPE_PIXEL_UINT16)) {
            satNotes.add(XPE_ALERT_WARNING,
                "XPE_WARN_SATURATION_NOT_APPLIED: saturation handling applies only to a scalar gain map on a uint16 frame; this call uses "
                "no gain, a polynomial gain or another input format, so saturated pixels are not handled (\"saturationLevel\":\"off\" silences this alert)");
        }
        satNotes.push();   // at most one saturation alert for the call
        XPE_STAGE_HOOK(4);
#ifdef XPE_CACHE_TEST_HOOKS
        if (xpe_pipeline_gain_frame_hook) xpe_pipeline_gain_frame_hook(stage4.data, pixelCount, stage4.format == XPE_PIXEL_FLOAT32);
#endif
        if (diag) {   // QA-A-246: the gain stage's own output, before binning / defect / ghost can overwrite the bad pixels
            counts.validMask |= XPE_DIAG_GAIN_NONFINITE;
            counts.gainStageNonFinite = count_non_finite(stage4, pixelCount);
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
            release_buffer(stage4Data);   // stage 5 holds its own copy
            work.reset();
            result = xpe_binning_correct(&stage5, cfg.binningMode, nullptr);
            if (result != XPE_OK) return result;

            if (meta) meta->flags |= XPE_FLAG_BINNING_CORRECTED;
        }
        XPE_STAGE_HOOK(5);

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

        // QA-A-237b (#245): whether `p` is the frame of one of the float buffers this call made itself (the caller's
        // buffer and the buffers of earlier stages that were handed back are not). A stage may work in place on such a
        // buffer; on the caller's buffer it may not, because a stage that fails half way would leave it changed.
        const auto owns_float = [&](const void* p) noexcept {
            return p != nullptr && ((workF != nullptr && p == workF) ||
                                    (!stage4Data.empty() && p == stage4Data.data()) ||
                                    (!stage5Data.empty() && p == stage5Data.data()) ||
                                    (!stage6Data.empty() && p == stage6Data.data()));
        };

        if (!cfg.bypassDefect) {
            // Defect correction: stage5(input) → stage6(output), defectMap for BPM lookup
            // meta, not nullptr: xpe_defect_correct rejects a null metadata
            // pointer. This stage never ran before the gate was fixed above, so
            // the malformed call had never been reached.
            if (!stage_input_is(stage5, XPE_PIXEL_FLOAT32, floatBytes)) return XPE_ERR_PROCESSING_FAILED;
            if (owns_float(stage5.data)) {
                // In place (QA-A-146: the stage reads only pixels it does not write, so one buffer serves as input and
                // output; the call shape is documented and pinned by DefectCorrectTest.InPlaceMatchesOutOfPlace). No
                // third float frame, and the copy of input to output is skipped inside the stage.
                result = xpe_defect_correct_in(calib, &stage5, &stage5, meta, &frameGainDefects, nullptr, nullptr, &cfg.defectLines);
                if (result != XPE_OK) return result;
                stage6 = stage5;
            } else {
                stage6Data.resize(pixelCount);
                stage6.width = img->width;
                stage6.height = img->height;
                stage6.bitsAllocated = 32u;
                stage6.bitsStored = 32u;
                stage6.format = XPE_PIXEL_FLOAT32;
                stage6.data = stage6Data.data();
                stage6.dataSize = stage6Data.size() * sizeof(float);
                result = xpe_defect_correct_in(calib, &stage5, &stage6, meta, &frameGainDefects, nullptr, nullptr, &cfg.defectLines);
                if (result != XPE_OK) return result;
                release_buffer(stage4Data);   // stage 6 holds its own frame
                release_buffer(stage5Data);
                work.reset();
            }

            if (meta) meta->flags |= XPE_FLAG_DEFECT_CORRECTED;
        }
        XPE_STAGE_HOOK(6);

        // Stage 7: Ghost Correction (PRE-04) - float32 in/out
        XpeImageBuffer stage7 = stage6;
        std::vector<float> stage7Data;

        if (!cfg.bypassGhost && ghostHandle) {
            if (!stage_input_is(stage6, XPE_PIXEL_FLOAT32, floatBytes)) return XPE_ERR_PROCESSING_FAILED;
            // Ghost correction works in place. On a frame this call made itself that is the frame it is given
            // (QA-A-237b: no fourth float copy; a frame that fails is put back by the stage itself and the call returns
            // the error without writing the caller's buffer). On the caller's own buffer it gets a copy of the stage-6
            // frame, as before. QA-A-104: it used to correct stage6 while the (empty) stage7 buffer was copied back,
            // so the output was zeros.
            if (!owns_float(stage6.data)) {
                stage7Data.resize(pixelCount);
                stage7.width = img->width;
                stage7.height = img->height;
                stage7.bitsAllocated = 32u;
                stage7.bitsStored = 32u;
                stage7.format = XPE_PIXEL_FLOAT32;
                stage7.data = stage7Data.data();
                stage7.dataSize = stage7Data.size() * sizeof(float);
                std::memcpy(stage7Data.data(), stage6.data, pixelCount * sizeof(float));
            }
            bool ghostBypassed = true;
            result = xpe_ghost_correct_with_status(ghostHandle, &stage7, meta, &ghostBypassed);
            if (result != XPE_OK) return result;

            // QA-A-226 (#241): a handle without calibrated lag parameters passes the frame through unchanged, so the
            // frame is not flagged as ghost-corrected. The output stays float32 either way (final_result_is_float
            // depends on the handle being present, not on whether it corrected), so the format does not change
            // with the configuration.
            if (meta) {
                if (!ghostBypassed) meta->flags |= XPE_FLAG_GHOST_CORRECTED;
                else meta->flags &= ~static_cast<uint32_t>(XPE_FLAG_GHOST_CORRECTED);
            }
        }
        XPE_STAGE_HOOK(7);

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
        if (diag) {   // QA-A-246: the final frame, as it is about to be handed back
            counts.validMask |= XPE_DIAG_FINAL_NONFINITE;
            counts.finalNonFinite = count_non_finite(*finalStage, pixelCount);
            counts.pixelCount = pixelCount;
            counts.structSize = diag->structSize;
        }
        if (out) {
            // SAFE-004: the result goes to the caller's output buffer; the input buffer is never written.
            parallel_copy(out->data, finalStage->data, outputBytes);
            out->width = img->width;
            out->height = img->height;
            out->format = finalStage->format;
            out->bitsAllocated = finalStage->bitsAllocated;
            out->bitsStored = finalStage->bitsStored;
            XPE_STAGE_HOOK(9);
            if (diag) *diag = counts;
            return XPE_OK;
        }
        if (finalStage->data != img->data)
            parallel_copy(const_cast<void*>(img->data), finalStage->data, outputBytes);
        XPE_STAGE_HOOK(9);

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
    xpe_calib_after_defect_commit(defect);   // only now: the set is installed (a refused set raises nothing)
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

// SRS-CALIB-SAFE-004 (QA-A-241): the pipeline with a separate output buffer. Same stages, same calibration snapshot rule as
// xpe_preprocess_pipeline_ex; the input frame is never written.
static XpeErrorCode pipeline_out_impl(const XpeImageBuffer* in,
                                      XpeImageBuffer* out,
                                      XpeImageMetadata* meta,
                                      const void* calibState,   // NOLINT(bugprone-easily-swappable-parameters): same shape as xpe_preprocess_pipeline_ex, which this entry mirrors
                                      void* ghostHandle,
                                      const char* configJsonOrNull,
                                      XpeStageDiagnostics* diag)
{
    if (!in || !out || !meta) return XPE_ERR_INVALID_INPUT;
    if (diag) {   // QA-A-246: a too-small struct is refused before anything runs; a failed call leaves the counts zero
        if (diag->structSize < sizeof(XpeStageDiagnostics)) return XPE_ERR_INVALID_INPUT;
        diag->validMask = 0;
        diag->pixelCount = 0;
        diag->gainStageNonFinite = 0;
        diag->finalNonFinite = 0;
    }
    PipelineConfig cfg;
    const XpeErrorCode cfgRc = PipelineConfig::fromJson(configJsonOrNull, &cfg);
    if (cfgRc != XPE_OK) return cfgRc;
    (void)calibState;   // as for pipeline_ex: every stage reads the store
    return pipeline_core(in, meta, ghostHandle, cfg, xpe_calib_snapshot(), out, diag);
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

XpeErrorCode xpe_preprocess_pipeline_out(const XpeImageBuffer* in,
                                         XpeImageBuffer* out,
                                         XpeImageMetadata* meta,
                                         const void* calibState,
                                         void* ghostHandle,
                                         const char* configJsonOrNull)
{
    XPE_PIPELINE_GUARD(meta, pipeline_out_impl(in, out, meta, calibState, ghostHandle, configJsonOrNull, nullptr))
}

XpeErrorCode xpe_preprocess_pipeline_out_ex(const XpeImageBuffer* in,
                                            XpeImageBuffer* out,
                                            XpeImageMetadata* meta,
                                            const void* calibState,
                                            void* ghostHandle,
                                            const char* configJsonOrNull,
                                            XpeStageDiagnostics* diag)
{
    XPE_PIPELINE_GUARD(meta, pipeline_out_impl(in, out, meta, calibState, ghostHandle, configJsonOrNull, diag))
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
