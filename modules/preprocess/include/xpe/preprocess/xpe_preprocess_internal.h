/**
 * @file xpe_preprocess_internal.h
 * @brief XPE Pre-Processing internal C++ helpers (NOT exported, TU-private)
 *
 * Only included by the .cpp translation units inside modules/preprocess/src/.
 * SPEC: SPEC-XPE-P1A v1.0.0
 * IEC 62304 Class B
 */

#ifndef XPE_PREPROCESS_INTERNAL_H_
#define XPE_PREPROCESS_INTERNAL_H_

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"
#include "xpe/preprocess_api.h"   // XpeCalibQualityMeta (FUNC-033, QA-A-35)

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cmath>
#include <limits>
#include <mutex>
#include <vector>
#include <string>
#include <memory>
#include <mutex>

/* =========================================================================
 * SWU-1.4: GhostCorrectorHandle — opaque handle backing xpe_ghost_create
 * REQ-P1A-085 to REQ-P1A-087
 * ========================================================================= */

// @MX:ANCHOR: [AUTO] GhostCorrectorHandle — opaque handle for xpe_ghost_* API
// @MX:REASON: Public API boundary; handle pointer cast checked in every ghost function
struct GhostCorrectorHandle {
    static constexpr uint32_t kMagic = 0xA7057AC0u; // sentinel for handle validation

    uint32_t magic{kMagic};
    uint32_t width{0};
    uint32_t height{0};

    // Ghost correction tier (1=LTI, 2=exposure-weighted LTI, 3=NLCSC)
    int tier{1};

    // Dual-exponential IRF coefficients (PMC3465354)
    double alpha1{0.9};    // fast component amplitude
    double tau1{1.0};      // fast component time constant (frames)
    double alpha2{0.05};   // slow component amplitude
    double tau2{20.0};     // slow component time constant (frames)

    // Tier 2: exposure-weighted LTI thresholds
    double tier2Threshold{0.005};  // auto-escalate to Tier 2 when residual > 0.5%
    double exposureWeight{1.0};    // dynamic exposure scaling factor

    // Tier 3: NLCSC signal-dependent coefficients
    double nlcscBeta{0.1};  // signal dependency parameter

    // Frame history ring buffer (float32 per pixel per history slot)
    std::vector<float> hist1; // fast IRF accumulator
    std::vector<float> hist2; // slow IRF accumulator

    // QA-A-202c (#233): where a frame's NEW history is written. A frame reads hist1/hist2 and writes next1/next2;
    // only when the whole frame succeeded are the two pairs swapped, so a frame that fails half way leaves the
    // history the next frame sees exactly as it was. Allocated once with the handle (not per frame); it
    // doubles the handle's memory (4 float planes instead of 2).
    std::vector<float> next1;
    std::vector<float> next2;

    double lastAcqTimeSec{0.0};
    double lastFrameMean{0.0}; // mean signal level for exposure weighting

    // SRS-CALIB-NFR-003: guards hist1/hist2 and the three fields above. Held for the whole of
    // xpe_ghost_correct() and xpe_ghost_reset(), so threads sharing one handle are serialised
    // call by call and no history update is lost. width/height/tier/IRF are set once in
    // xpe_ghost_create() and never change, so they need no lock. xpe_ghost_destroy() must
    // still not run concurrently with a call on the same handle.
    std::mutex mtx;

    // Validate that a void* is a live handle
    static bool isValid(const void* h) noexcept {
        if (!h) return false;
        const auto* gh = static_cast<const GhostCorrectorHandle*>(h);
        return gh->magic == kMagic;
    }
};

/* =========================================================================
 * Calibration file I/O helpers
 * ========================================================================= */

struct CalibFileHeader {
    uint8_t  magic[4];       // "XPEC"
    uint32_t version;        // format version (currently 1)
    uint32_t width;
    uint32_t height;
    uint32_t pixelFormat;    // XpePixelFormat
    uint64_t expiryEpochMs;  // expiry timestamp
    uint32_t payloadCrc32;   // CRC-32 of pixel data
    uint32_t reserved[7];    // pad to 64 bytes
};
static_assert(sizeof(CalibFileHeader) == 64u, "CalibFileHeader must be 64 bytes");

/* =========================================================================
 * Bilinear interpolation helper (SWU-1.3 defect pixel correction)
 * ========================================================================= */

// @MX:NOTE: [AUTO] Edge-aware bilinear: skips neighbours that are also defective
float xpe_interpolate_pixel(const float* pixels, const uint8_t* defectMask,
                             uint32_t x, uint32_t y,
                             uint32_t width, uint32_t height) noexcept;

/* =========================================================================
 * The gain applier's arithmetic, shared with the gain polynomial generator (QA-A-210e, Codex #61)
 *
 * A gain polynomial is generated in one place and applied in another, and a file the generator calls a success must be
 * one the applier can use, with the quality the generator reports. So the two use ONE definition of what "applying"
 * computes: the float32 Horner evaluation of the stored float32 coefficients at the pixel value as a float, and the
 * guard that refuses a gain outside the applier's range. gain_correct.cpp applies them to a frame; xpe_calib_generate_gain
 * applies them to the measured doses of each pixel to score and to accept the fit.
 * ========================================================================= */

/** The applier refuses a gain outside [XPE_GAIN_APPLIED_MIN, XPE_GAIN_APPLIED_MAX] (gain_correct.cpp: a second line of defence behind the load-time range). */
constexpr float XPE_GAIN_APPLIED_MIN = 0.001f;
constexpr float XPE_GAIN_APPLIED_MAX = 1000.0f;

/** Whether the applier would accept this gain value (finite, positive, within its range). */
inline bool xpe_gain_value_valid(float gain) noexcept {
    return std::isfinite(gain) && gain > 0.0f && gain >= XPE_GAIN_APPLIED_MIN && gain <= XPE_GAIN_APPLIED_MAX;
}

/** Float32 Horner from the highest coefficient down, `ncoeffs` coefficients c[0..ncoeffs-1] -- what the applier evaluates per pixel. */
inline float xpe_gain_poly_eval_f32(const float* c, uint32_t ncoeffs, float x) noexcept {
    float acc = c[ncoeffs - 1];
    for (uint32_t j = ncoeffs - 1; j > 0; --j) {
        acc = acc * x + c[j - 1];
    }
    return acc;
}

/** One top-level member of a configuration object (QA-A-209b). */
struct XpeConfigEntry {
    std::string key;     ///< the member name, escapes interpreted
    std::string text;    ///< a scalar's text: the string with its escapes interpreted, or the number/true/false/null token
    bool scalar{false};  ///< the value is a string, a number, true, false or null (not an object or an array)
    bool quoted{false};  ///< ... and it was a JSON string
};

/**
 * A configuration text parsed ONCE into its top-level members (helpers.cpp, "Configuration JSON"). The rules, for
 * every configuration this module reads -- the pipeline's, the ghost corrector's, the nonlinearity stage's, the offset
 * generation's, and the config block of a calibration file:
 *   - the text is ONE valid JSON object (nlohmann-json, strict, no comments, no NUL byte); a key is a TOP-LEVEL member,
 *     and a name that appears only inside a nested object or array is not given
 *   - a member name given twice at the top level -- ANY name, whatever the values -- is XPE_ERR_CONFIG_INVALID
 *   - xpe_config_parse: a text the caller supplies. NULL is "no configuration" (an empty document); a text that is
 *     empty or only white space is XPE_ERR_CONFIG_INVALID
 *   - xpe_config_parse_block: a block stored with its length (an XCal file's config). Length 0 is "no block" (an empty
 *     document); a block that is there is parsed like a text, so white space only is XPE_ERR_CONFIG_INVALID
 * Parsing allocates and may throw std::bad_alloc (the callers' guards map it to XPE_ERR_OUT_OF_MEMORY); no exception for
 * a malformed text.
 *
 * Reading: getString gives the text of a scalar member (`*quoted` tells a JSON string from a bare token; an empty
 * string is a value -- the callers that follow the pipeline's rule read it as "not given"); it is false for an absent
 * member and for one whose value is an object or an array. getNumber is true only for a BARE JSON number, which is
 * converted into `*value`; a string (however numeric), true, false, null, an object, an array or an absent member is
 * not given.
 */
struct XpeConfigDoc {
    std::vector<XpeConfigEntry> entries;

    const XpeConfigEntry* find(const std::string& key) const;
    bool getString(const char* key, std::string* value, bool* quoted = nullptr) const;
    bool getNumber(const char* key, double* value) const;
};

XpeErrorCode xpe_config_parse(const char* text, XpeConfigDoc* doc);
XpeErrorCode xpe_config_parse_block(const char* text, size_t len, XpeConfigDoc* doc);

#ifdef XPE_CACHE_TEST_HOOKS
/** Test-only (QA-A-209c): how many configuration texts have been parsed. Compiled into the allocation-failure executable only. */
extern unsigned long xpe_config_parse_calls;
#endif

/**
 * @brief xpe_nonlinearity_correct with a report of whether pixels were corrected.
 *
 * Same validation and return codes as the public function. *applied is set to
 * true only when the call changed the image.
 *
 * It is TRUE when a nonlinearity LUT is loaded: SRS-CALIB-FUNC-006-EXT 6a is
 * implemented end to end -- generated by xpe_calib_generate_nonlin_lut(),
 * loaded by xpe_calib_load_nonlin_lut(), applied here, and wired into the
 * pipeline ahead of gain correction (pipeline.cpp, stage 3).
 *
 * It is FALSE for a NULL config, a config without "mode", and the known modes
 * when no LUT is loaded -- that path is the older REQ-P1A-012 baseline and
 * changes no pixels.
 *
 * EXT 6b (the global 4th-degree polynomial, for embedded/FPGA use) is NOT
 * implemented; there is no generate, load, or apply for it and no XCal type.
 * "FUNC-006 is not implemented" -- which this comment used to say -- and
 * "FUNC-006 is implemented" are both false; 6a is, 6b is not (QA-A-125, #186).
 *
 * The pipeline sets XPE_FLAG_NONLINEARITY_CORRECTED from it (#184).
 */
XpeErrorCode xpe_nonlinearity_apply(XpeImageBuffer* img, const char* configJsonOrNull,
                                    bool* applied);

/** xpe_nonlinearity_apply for a configuration that is already parsed (the pipeline parses its configuration once). */
XpeErrorCode xpe_nonlinearity_apply_doc(XpeImageBuffer* img, const XpeConfigDoc& config, bool* applied);


/* =========================================================================
 * Module lifecycle predicate (defined in xpe_preprocess.cpp)
 *
 * SPEC-XPE-P1A REQ-P1A-020 requires every processing function to return
 * XPE_ERR_NOT_INITIALIZED while the module is not initialized. #117 decision B
 * split that from "calibration map absent" (XPE_ERR_CALIB_NOT_LOADED), so the
 * correction entry points need the predicate rather than inferring the state
 * from a missing map.
 * ========================================================================= */

extern "C" {

/**
 * @brief Report whether the module is currently initialized
 *
 * QA-A-113 (#176): exported so a test-suite hygiene guard can ask whether a
 * test left the module up WITHOUT mutating anything. The probe that existed
 * before -- call xpe_preprocess_init() and read its error code -- changes the
 * state it is trying to observe, and its undo also clears the calibration
 * store, which is not the guard's to clear.
 *
 * @return true while the module is up.
 */
XPE_API bool xpe_preprocess_is_initialized(void);

}  // extern "C"

/**
 * @brief Reset the calibration mode and quality metadata to start-up values
 *
 * QA-A-120 (#176): called by xpe_preprocess_shutdown() so that "shutdown"
 * clears every module global, not only the calibration maps. Defined in
 * xpe_calib_mode.cpp, which owns both.
 */
void xpe_calib_mode_reset_globals() noexcept;

/* =========================================================================
 * Dimension / null guard inline helpers
 * ========================================================================= */

inline bool xpe_dims_match(const XpeImageBuffer* a, const XpeImageBuffer* b) noexcept {
    return a && b && a->width == b->width && a->height == b->height;
}

inline bool xpe_pixel_size(XpePixelFormat format, size_t* bytesOut) noexcept {
    if (!bytesOut) return false;

    switch (format) {
    case XPE_PIXEL_UINT8:
        *bytesOut = 1;
        return true;
    case XPE_PIXEL_UINT16:
        *bytesOut = 2;
        return true;
    case XPE_PIXEL_FLOAT32:
        *bytesOut = 4;
        return true;
    default:
        return false;
    }
}

inline bool xpe_pixel_count(const XpeImageBuffer* buf, size_t* countOut) noexcept {
    if (!buf || !countOut || buf->width == 0 || buf->height == 0) return false;

    const size_t width = static_cast<size_t>(buf->width);
    const size_t height = static_cast<size_t>(buf->height);
    if (width > std::numeric_limits<size_t>::max() / height) return false;

    *countOut = width * height;
    return true;
}

inline bool xpe_required_bytes(const XpeImageBuffer* buf, size_t elementSize,
                               size_t* bytesOut) noexcept {
    if (!bytesOut) return false;

    size_t count = 0;
    if (!xpe_pixel_count(buf, &count)) return false;
    if (count > std::numeric_limits<size_t>::max() / elementSize) return false;

    *bytesOut = count * elementSize;
    return true;
}

inline bool xpe_buffer_has_format(const XpeImageBuffer* buf,
                                  XpePixelFormat expectedFormat,
                                  size_t* countOut = nullptr) noexcept {
    if (!buf || !buf->data || buf->format != expectedFormat) return false;

    size_t elementSize = 0;
    if (!xpe_pixel_size(expectedFormat, &elementSize)) return false;

    size_t requiredBytes = 0;
    if (!xpe_required_bytes(buf, elementSize, &requiredBytes)) return false;

    // #123 dataSize input contract (docs/project/api-spec.md, 2026-09-10):
    // 0 means *unspecified* -- trust the dimensions and skip the size check,
    // because existing callers do not populate the field. A non-zero value
    // smaller than the dimensions require is rejected: reading width*height
    // pixels out of it would run past the allocation. Larger is accepted.
    if (buf->dataSize != 0 && buf->dataSize < requiredBytes) return false;

    if (countOut) {
        *countOut = requiredBytes / elementSize;
    }
    return true;
}

/* =========================================================================
 * Pre-loaded Calibration State (Pipeline Optimization)
 *
 * Holds calibration maps loaded once and reused across multiple pipeline
 * invocations. Eliminates per-frame file I/O for offset/gain/defect maps.
 * ========================================================================= */

/**
 * @brief Pre-loaded calibration maps for pipeline optimization.
 *
 * Populated once via xpe_calib_state_load(), then passed to
 * xpe_preprocess_pipeline_ex() to skip file I/O on each frame.
 *
 * Lifecycle:
 * 1. Zero-initialize: XpeCalibrationState state = {};
 * 2. Load maps:       xpe_calib_state_load(&state, calibPath);
 * 3. Process frames:  xpe_preprocess_pipeline_ex(img, meta, &state, ...);
 * 4. Release:         xpe_calib_state_release(&state);
 *
 * Memory ownership: offset/gain/defect data pointers are owned by this struct.
 * Callers must call xpe_calib_state_release() to free.
 */
struct XpeCalibrationState {
    XpeImageBuffer offsetMap;   ///< Pre-loaded offset correction map (uint16)
    XpeImageBuffer gainMap;     ///< Pre-loaded gain correction map (float32)
    XpeImageBuffer defectMap;   ///< Pre-loaded defect pixel map (uint8)

    bool offsetLoaded;          ///< true if offsetMap is valid
    bool gainLoaded;            ///< true if gainMap is valid
    bool defectLoaded;          ///< true if defectMap is valid
};

/* =========================================================================
 * Global Calibration Data (singleton, new XCal v1 API)
 * Populated by xpe_calib_load_offset / xpe_calib_load_gain / xpe_calib_load_defect_map.
 * ========================================================================= */

struct CalibrationData {
    // offset_map and defect_map are SHARED so that xpe_offset_correct / xpe_defect_correct can take a
    // reference under the lock and read the map outside it (QA-A-202, #233): a reload in the middle of
    // a frame replaces the pointer in this store and leaves the frame the map it started with. A map is
    // therefore never modified in place once it is installed -- an update builds a new array and swaps.
    std::shared_ptr<float[]>   offset_map;
    uint32_t offset_width{0};
    uint32_t offset_height{0};
    int64_t  offset_timestamp{0};
    char     offset_session_id[64]{};
    // What the cached loaders need from the file the plain loader just read (QA-A-196, #216): the
    // file's expiry (0 = never) and, for gain, its config JSON. The plain loaders write them so the
    // cache can keep them in its entry and judge a later hit as the file read would have.
    int64_t  offset_expiry_ms{0};

    // gain_map and gain_poly_coeffs are SHARED for the same reason as offset_map (QA-A-202d, Codex #32): a
    // frame takes a CalibSnapshot of the three maps when it starts, and a load that replaces the store while
    // the frame runs must leave the frame the maps it began with. A map is never modified in place.
    std::shared_ptr<float[]>   gain_map;
    uint32_t gain_width{0};
    uint32_t gain_height{0};
    int64_t  gain_timestamp{0};
    char     gain_session_id[64]{};
    int64_t  gain_expiry_ms{0};
    // The gain file's FUNC-033 quality metadata, parsed before the commit and kept so that a cache
    // hit applies the very same values as the load did (QA-A-200).
    XpeCalibQualityMeta gain_quality{};
    bool     gain_has_quality{false};

    // QA-A-37 (#140): XCAL_TYPE_GAIN_POLY coefficients, pixel-major --
    // coefficient j of pixel p lives at [p * gain_poly_num_coeffs + j], which
    // is the layout xpe_calib_generate_gain_polynomial() writes.
    //
    // A polynomial calibration and a scalar map are alternatives, never both:
    // loading either clears the other, so the store always describes exactly
    // one gain model and xpe_gain_correct() cannot silently apply a stale map.
    std::shared_ptr<float[]>   gain_poly_coeffs;
    uint32_t gain_poly_num_coeffs{0};

    // QA-A-123 (#194): the dose range the polynomial was fitted and
    // monotonicity-checked over, read from the file's config_json. The fit
    // guarantees nothing outside it, so xpe_gain_correct() clamps the pixel
    // value into this interval before evaluating.
    //
    // `gain_poly_has_range` is false for a file written before QA-A-123.
    // Such a file is NOT rejected -- that would retire every existing
    // calibration -- and it is NOT silently accepted either: the loader
    // raises one alert saying the clamp does not apply to it.
    bool   gain_poly_has_range{false};
    double gain_poly_dose_min{0.0};
    double gain_poly_dose_max{0.0};

    std::shared_ptr<uint8_t[]> defect_map;   // shared for the same reason as offset_map
    uint32_t defect_width{0};
    uint32_t defect_height{0};
    int64_t  defect_expiry_ms{0};

    // QA-A-111 (#186): SRS-CALIB-FUNC-006-EXT 6a nonlinearity LUT, a flat table
    // indexed by raw ADU. `nonlin_extension_start` is the first index the
    // generator extrapolated -- the file records it because the corrected SPEC
    // judges the 0.3% accuracy clause inside the measured range only, and a
    // reader that cannot tell the regions apart would over-claim.
    std::unique_ptr<uint16_t[]> nonlin_lut;
    uint32_t nonlin_entries{0};
    uint32_t nonlin_extension_start{0};
    int64_t  nonlin_timestamp{0};

    // The module's FUNC-033 quality record: what xpe_calib_get_quality_meta() serves (QA-A-202d, Codex #32 A1).
    // It lives in the store, under g_calib_mutex, so that a gain load moves the maps and the quality in one
    // critical section and a reader never sees the new maps beside the old quality (it used to be a separate
    // global, updated after the lock was released and read without one). It stays a second field next to
    // `gain_quality` -- the file's own parsed values, which the calibration cache keeps -- because the record is
    // more than the loaded file's: a generator also writes it (xpe_calib_record_quality_meta), a cached load
    // makes it current without replacing any map, and it carries previous_r_squared, the history of the
    // record it replaced.
    XpeCalibQualityMeta quality_meta = [] {
        XpeCalibQualityMeta m{};
        m.previous_r_squared = -1.0;   // "no previous fit", as at start-up
        return m;
    }();
};

extern CalibrationData g_calib;
extern std::mutex      g_calib_mutex;

/* =========================================================================
 * The calibration set a frame is processed with (QA-A-202d, Codex #32 A2)
 *
 * The pipeline calls the offset, gain and defect corrections one after the other, and each used to take
 * g_calib_mutex afresh to fetch its map -- so a load that landed between two stages gave one frame an offset
 * from set A and a gain from set B. A CalibSnapshot is the three maps (shared ownership), their dimensions, the
 * gain polynomial's range and the module's initialized state, copied under ONE lock; the pipeline takes one
 * when a frame (or, for the path-taking entry points, the set it has just loaded) starts and runs every stage
 * on it. The exported single-stage functions (xpe_offset_correct and the others) take a snapshot of their
 * own per call: they use the maps current when they are called -- the set consistency is the pipeline's.
 * ========================================================================= */
struct CalibSnapshot {
    bool initialized{false};

    std::shared_ptr<float[]>   offset_map;
    uint32_t offset_width{0};
    uint32_t offset_height{0};

    std::shared_ptr<float[]>   gain_map;           ///< the scalar plane; null while a polynomial is loaded
    std::shared_ptr<float[]>   gain_poly_coeffs;   ///< pixel-major coefficient planes; null for a scalar map
    uint32_t gain_poly_num_coeffs{0};
    bool     gain_poly_has_range{false};
    double   gain_poly_dose_min{0.0};
    double   gain_poly_dose_max{0.0};
    uint32_t gain_width{0};
    uint32_t gain_height{0};

    std::shared_ptr<uint8_t[]> defect_map;
    uint32_t defect_width{0};
    uint32_t defect_height{0};
};

/** The snapshot of the store as it is now; the caller holds g_calib_mutex. Copies pointers and numbers only. */
CalibSnapshot xpe_calib_snapshot_locked() noexcept;
/** The same, taking g_calib_mutex itself. */
CalibSnapshot xpe_calib_snapshot() noexcept;

/**
 * The three corrections on a given snapshot. xpe_offset_correct / xpe_gain_correct / xpe_defect_correct are
 * these with a snapshot taken at the call; the pipeline passes the frame's own.
 */
XpeErrorCode xpe_offset_correct_in(const CalibSnapshot& calib, const XpeImageBuffer* input,
                                   XpeImageBuffer* output, const XpeImageMetadata* metadata);
XpeErrorCode xpe_gain_correct_in(const CalibSnapshot& calib, const XpeImageBuffer* input,
                                 XpeImageBuffer* output, const XpeImageMetadata* metadata);
XpeErrorCode xpe_defect_correct_in(const CalibSnapshot& calib, const XpeImageBuffer* input,
                                   XpeImageBuffer* output, const XpeImageMetadata* metadata);

/* =========================================================================
 * Staged calibration loads (QA-A-202c, #233 / Codex #27 A1)
 *
 * A plain loader reads and validates a file and then commits it to g_calib. The pipeline loads three files and
 * must replace the calibration set as a whole or not at all, so each loader is split in two: a STAGE
 * function that does everything that can fail (read, checksum, validate, allocate, parse) into a staged
 * object and touches no global, and a COMMIT function that only moves the staged object into g_calib and
 * cannot fail. xpe_calib_load_offset/gain/defect_map are stage + commit, unchanged for their callers; the
 * pipeline stages all three first and commits them together under one lock.
 * ========================================================================= */
// The offset and defect maps are shared_ptr in the store, so they are shared_ptr here: converting a unique_ptr
// to a shared_ptr allocates the control block, and a commit must not allocate.
struct StagedOffset {
    std::shared_ptr<float[]> map;
    uint32_t width{0};
    uint32_t height{0};
    int64_t  timestamp{0};
    int64_t  expiryMs{0};
    char     sessionId[64]{};
};

struct StagedGain {
    std::shared_ptr<float[]> map;      ///< the scalar plane, or the coefficient planes of a polynomial
                                       ///< (shared: the store holds it so, and converting at commit would allocate)
    bool     isPoly{false};
    uint32_t numCoeffs{0};
    uint32_t width{0};
    uint32_t height{0};
    int64_t  timestamp{0};
    int64_t  expiryMs{0};
    char     sessionId[64]{};
    XpeCalibQualityMeta quality{};
    bool     qualityFound{false};
    // The polynomial's fitted dose range, as read from the file's config block (QA-A-123).
    double   doseLo{-1.0};
    double   doseHi{-1.0};
    bool     rangePresent{false};
    bool     rangeUsable{false};
};

struct StagedDefect {
    std::shared_ptr<uint8_t[]> map;
    uint32_t width{0};
    uint32_t height{0};
    int64_t  expiryMs{0};
};

/** Read, validate and allocate; changes no global. Never throws. */
XpeErrorCode xpe_calib_stage_offset(const char* filepath, StagedOffset* out) noexcept;
XpeErrorCode xpe_calib_stage_gain(const char* filepath, StagedGain* out) noexcept;
XpeErrorCode xpe_calib_stage_defect(const char* filepath, StagedDefect* out) noexcept;

/** Move a staged object into g_calib. The caller holds g_calib_mutex. Cannot fail. */
void xpe_calib_commit_offset_locked(StagedOffset& staged) noexcept;
void xpe_calib_commit_gain_locked(StagedGain& staged) noexcept;
void xpe_calib_commit_defect_locked(StagedDefect& staged) noexcept;

/**
 * What follows a committed gain load, after g_calib_mutex was released: the advisory alerts about a polynomial's
 * dose range are raised. It cannot fail the load. (The FUNC-033 quality metadata is NOT here any more: it is
 * committed with the maps, in xpe_calib_commit_gain_locked -- QA-A-202d.)
 */
void xpe_calib_after_gain_commit(const StagedGain& staged) noexcept;

/**
 * Whether the calibration cache's list and index describe the same entries (every list node has its
 * index slot and the counts agree). Not exported: a seam for the allocation-failure sweep, which
 * compiles the product sources into its own executable (QA-A-203, Codex #21).
 */
bool xpe_calib_cache_is_consistent();

#ifdef XPE_CACHE_TEST_HOOKS
/**
 * Test-only: called by a cached loader's hit lookup right after it has opened the file and before the
 * expiry is judged, so a test can make that step take as long as a slow (network) path would. Only the
 * allocation-failure executable defines XPE_CACHE_TEST_HOOKS; the shipped library has neither the
 * declaration nor the call (QA-A-203b, Codex #22).
 */
extern void (*xpe_cache_after_open_check_hook)();

/**
 * Test-only (QA-A-202d, Codex #32): the pipeline's observation points. `xpe_calib_after_set_commit_hook` runs
 * after load_calibration_set committed the three maps and released g_calib_mutex; `xpe_pipeline_after_stage_hook`
 * runs inside pipeline_core after a stage (2 = the offset stage), so a test can load another calibration set
 * at exactly the point where a concurrent load would hurt. Same rule as the hook above: only the
 * allocation-failure executable defines XPE_CACHE_TEST_HOOKS.
 */
extern void (*xpe_calib_after_set_commit_hook)();
extern void (*xpe_pipeline_after_stage_hook)(int stage);
/** Runs INSIDE the critical section that commits the set, with g_calib_mutex held: a test starts a reader here
 *  and checks that it cannot finish until the section ends. */
extern void (*xpe_calib_in_set_commit_hook)();
/** Runs at the top of xpe_calib_get_quality_meta, just before it takes g_calib_mutex (QA-A-202e): a test uses it to
 *  know that a reader thread has reached the getter's lock attempt, instead of guessing from a thread start. */
extern void (*xpe_calib_quality_before_lock_hook)();
#endif

/* =========================================================================
 * FUNC-033 quality metadata (QA-A-35, #140)
 *
 * SRS-CALIB-001 SRS-CALIB-FUNC-033 requires every generated XCal gain file to
 * carry fit-quality metadata, and the R2 gate to fire below 0.999. The store
 * lives in xpe_calib_mode.cpp behind xpe_calib_get_quality_meta(); generation
 * code writes it through the two functions below.
 *
 * Internal, NOT exported: xpe_preprocess.dll stays at 50 exports.
 * ========================================================================= */

/**
 * @brief Record the quality metadata of a freshly generated calibration.
 *
 * Moves the current r_squared into previous_r_squared, then overwrites the
 * store with @p meta (calibration_mode included: pass the resolved mode) and stamps calibration_timestamp. Applies the
 * FUNC-033 (2) gate: calibration_pass = (r_squared >= 0.999).
 *
 * @return true when the gate passed, false when it did not (the caller logs).
 */
bool xpe_calib_record_quality_meta(const XpeCalibQualityMeta& meta) noexcept;

/**
 * @brief FUNC-031 (3)(4)(5)(8): the mode a generator runs under.
 *
 * Explicit mode: XPE_OK and that mode when num_levels and degree are within
 * its max_points / poly_degree, XPE_ERR_INVALID_INPUT otherwise.
 * AUTO: the smallest explicit mode with max_points >= num_levels and
 * poly_degree >= degree; XPE_ERR_INVALID_INPUT when none (more than 10 levels
 * or degree above 4). num_levels < 1 or degree < 0 is invalid input.
 */
XpeErrorCode xpe_calib_resolve_mode(int32_t num_levels, int32_t degree,
                                    XpeCalibrationMode* resolved) noexcept;

/**
 * @brief SRS-CALIB-FUNC-002 gain value range, quoted from the SRS (#188).
 *
 * "Values shall be in range [0.1, 10.0]; out-of-range values shall trigger
 * XPE_ERR_INVALID_CALIB_DATA error." The bounds are inclusive. Gain maps are
 * normalised to unit mean (xpe_calib_generate_gain.cpp), so a value ten times
 * the mean is a defect in the calibration data, not a usable gain.
 */
constexpr float XPE_CALIB_GAIN_MIN = 0.1f;
constexpr float XPE_CALIB_GAIN_MAX = 10.0f;

/** @brief The FUNC-033 (2) R-squared gate threshold, quoted from the SRS. */
constexpr double XPE_CALIB_R_SQUARED_GATE = 0.999;

/** The fill value of r_squared / previous_r_squared when there is none -- NOT the indicator: that is has_r_squared (QA-A-208d). */
constexpr double XPE_R_SQUARED_NOT_GIVEN = -1.0;

/**
 * @brief Restore FUNC-033 metadata from an XCal file's config JSON.
 *
 * SRS-CALIB-FUNC-033 (5): "All metadata shall be stored in XCal file header
 * section (JSON-encoded in reserved header bytes)." This reads the fields back
 * into the store that xpe_calib_get_quality_meta() serves.
 *
 * Backward compatible by construction: a file written before QA-A-35 carries
 * none of these keys, and each missing field takes its "no data" value
 * (r_squared / previous_r_squared -1.0, the rest 0) rather than failing the
 * load.
 *
 * Nothing is changed until xpe_calib_commit_quality_meta_locked().
 *
 * A field that is present and is not a number in the range it has by definition (fit_r_squared at most 1.0;
 * polynomial_degree 0..4; actual_dose_levels 1..10; calibration_mode 0..4) is XPE_ERR_CONFIG_INVALID (QA-A-204,
 * QA-A-208c) -- it used to be read as 0 or truncated, silently.
 *
 * @param config The file's config block, parsed once (xpe_config_parse_block): one valid JSON object, top-level keys.
 * @param out    Receives the parsed metadata when *found is true.
 * @param found  Set to true when at least one FUNC-033 field was present (and all were valid).
 * @return XPE_OK, XPE_ERR_CONFIG_INVALID for a malformed field, XPE_ERR_INVALID_INPUT for a null argument.
 */
XpeErrorCode xpe_calib_parse_quality_meta(const XpeConfigDoc& config, XpeCalibQualityMeta* out, bool* found);

/**
 * @brief Make parsed FUNC-033 metadata the one xpe_calib_get_quality_meta() serves; the caller holds g_calib_mutex.
 *
 * Never throws: it copies a plain struct. Split from the parse so a loader can do everything that
 * allocates before it commits anything (QA-A-200); previous_r_squared is chained from the metadata
 * being replaced, as a load always did. The gain loaders call this inside the critical section that moves the
 * maps, so the maps and the quality become current together (QA-A-202d).
 */
void xpe_calib_commit_quality_meta_locked(const XpeCalibQualityMeta& parsed) noexcept;

/**
 * A gain whose file carries NO quality metadata becomes current: the record becomes "none" (valid = 0, every field
 * zero) instead of keeping the previous file's values (QA-A-202e). previous_r_squared keeps the history -- the R2 of
 * the last record that had one -- apart from the current record. The caller holds g_calib_mutex; never throws.
 */
void xpe_calib_commit_no_quality_locked() noexcept;

/**
 * @brief Scalar reference implementation of the gain-correction inner loop.
 *
 * SPEC-XPE-P1A section 4.6 names the scalar form as the REFERENCE the vector
 * form is checked against. QA-A-72 (#160) made that checkable: the function used
 * to be a file-local fallback selected by a runtime AVX2 probe, so nothing could
 * call it to compare, and when the probe was removed it would have become an
 * unreferenced static -- deleted by the compiler, and a warning under /W4 /WX.
 *
 * NOT A FALLBACK. THE COMPARAND. Exposing it is the alternative to deleting it,
 * and its one consumer is test_gain_correct_avx2_parity.cpp, which requires the
 * shipped path to agree with it inside the AC-GAIN-004 tolerance. DELETING THAT
 * TEST DELETES THE ONLY INDEPENDENT CHECK the AVX2 kernel has -- the other tests
 * in that file compare repeated calls with each other, which a consistently
 * wrong kernel passes. It is also the only caller: with the test gone this is an
 * unused inline, which no compiler warns about, so nothing would say the check
 * had been lost.
 *
 * It lives here as an INLINE definition rather than an exported symbol on
 * purpose: the library and the test then compile the same source, and the
 * module's export surface does not grow to make a test possible. QA-A-61 set
 * that boundary -- an export is hard to withdraw -- and it holds for a reference
 * implementation as much as for a feature.
 *
 * @param input Source pixels, width*height uint16 ELEMENTS.
 * @param reciprocal_gain Per-pixel 1/gain, width*height float ELEMENTS.
 * @param output Destination, width*height float ELEMENTS.
 * @param width Image width in pixels.
 * @param height Image height in pixels.
 */
/**
 * @brief Scalar reference implementation of the offset-correction inner loop.
 *
 * NOT A FALLBACK. THE COMPARAND. Nothing in the library selects between this and
 * the vector form at run time any more -- QA-A-73 (#160) removed the probe that
 * used to, because it could not protect anything (see the note in
 * offset_correct.cpp). On x86 the vector form is the only path the library takes.
 *
 * This function survives for one reason: test_offset_correct_avx2_parity.cpp
 * compares the shipped path against it. DELETING THAT TEST DELETES THE ONLY
 * INDEPENDENT CHECK the AVX2 kernel has -- the remaining tests there compare
 * repeated calls with each other, which a consistently wrong kernel passes. It
 * is also the only caller: with the test gone this function is an unused inline,
 * which no compiler warns about, so nothing would say the check had been lost.
 *
 * REQ-P1A-010 states the vector kernel is "bit-identical to scalar version --
 * verified by test suite". QA-A-73 made the second half true.
 *
 * @param src Source pixels, n uint16 ELEMENTS.
 * @param off Per-pixel offset, n float ELEMENTS.
 * @param dst Destination, n uint16 ELEMENTS.
 * @param n Element count.
 */
inline void xpe_offset_apply_scalar_reference(const uint16_t* src,
                                              const float* off,
                                              uint16_t* dst,
                                              size_t n) noexcept {
    for (size_t i = 0; i < n; ++i) {
        float v = static_cast<float>(src[i]) - off[i];
        if (v < 0.0f) v = 0.0f;
        if (v > 65535.0f) v = 65535.0f;
        dst[i] = static_cast<uint16_t>(v + 0.5f);
    }
}

inline float xpe_gain_apply_scalar_pixel(uint16_t input, float reciprocal_gain) noexcept {
    // AC-GAIN-002: the a * (1.0f / b) pattern, and the whole of the rule.
    return static_cast<float>(input) * reciprocal_gain;
}

inline void xpe_gain_apply_scalar_reference(const uint16_t* input,
                                            const float* reciprocal_gain,
                                            float* output,
                                            uint32_t width,
                                            uint32_t height) noexcept {
    const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height);
    for (size_t i = 0; i < count; ++i) {
        output[i] = xpe_gain_apply_scalar_pixel(input[i], reciprocal_gain[i]);
    }
}

#endif /* XPE_PREPROCESS_INTERNAL_H_ */
