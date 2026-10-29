/**
 * @file saturation.hpp
 * @brief Detector saturation level: pile-up detection and the output rule (QA-A-296, Refs #278).
 *
 * The raw pixel value at which a detector saturates differs per detector (Samsung 2G: 12800, cyan_test: 65535),
 * so the module has no built-in value. The level comes from the caller (pipeline key `saturationLevel`) or from the
 * gain calibration file (keys `saturation_*` in its config block, written by xpe_calib_generate_gain), and a frame
 * processed with neither gets an alert and no saturation handling.
 *
 * The pile-up rule below was tuned on 2G data only (three detectors seen); every threshold is a parameter.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

constexpr uint32_t XPE_SAT_BINS = 65536;

/** Pile-up thresholds. Tuned on Samsung 2G frames; other detectors have not been checked. */
struct XpeSatParams {
    double minCountFrac{5e-4};     ///< the pile must hold at least this fraction of the counted pixels (0.05 %)
    double minRatio{50.0};         ///< ... and at least this many times the median count of the 64 bins just below it
    double maxAboveFrac{1e-3};     ///< ... and at most this fraction of the counted pixels may lie above it (0.1 %)
};

/**
 * Histogram of a uint16 frame. Only every `rowStep`-th row is counted (1 = all). A pixel whose `exclude` byte is
 * non-zero is not counted (known defect pixels); `exclude` may be null. `hist` must hold XPE_SAT_BINS entries and
 * is added to (zero it first). `*counted` is increased by the number of pixels counted.
 */
void xpe_sat_histogram(const uint16_t* raw, uint32_t width, uint32_t height, uint32_t rowStep,
                       const uint8_t* exclude, uint32_t* hist, uint64_t* counted) noexcept;

/**
 * The largest value that sits on a pile-up at the top of the histogram: scanning down from the highest occupied
 * value, a value with more than maxAboveFrac * total pixels above it ends the search (no pile-up at the top); the
 * first value whose count is >= minCountFrac * total and >= minRatio * max(1, median of the 64 bins below it)
 * is the answer. False when there is no such value ("undetermined" -- the caller must not invent one).
 */
bool xpe_sat_find_pile(const uint32_t* hist, uint64_t total, const XpeSatParams& p,
                       uint32_t* level, uint64_t* count) noexcept;

/**
 * The saturated output value S_out = level / median(gain): the value a pixel at the saturation level would have
 * after the gain stage on a pixel of median gain. 0 when the inputs are not usable.
 */
float xpe_sat_output_value(uint32_t level, float medianGain) noexcept;

/**
 * The median (element m/2 of the sorted values) of the gain plane's values except those at the ascending indices
 * `excluded` (the pixels the gain calibration classified defective; their plane value was replaced by 1.0 and is not a gain).
 * 0 when no value is left. Copies the population.
 */
float xpe_sat_gain_median(const float* gain, size_t n, const uint32_t* excluded, size_t excludedCount);

/**
 * The saturation record of a gain file's config block.
 *   saturation_status   "determined" | "undetermined"
 *   saturation_level    integer 1..65535 (present when determined)
 *   saturation_method   text, e.g. "histogram_top_pile/1"
 *   saturation_min_count_frac, saturation_min_ratio, saturation_max_above_frac   the thresholds used
 *   saturation_frames_used, saturation_frames_total, saturation_decided_at (ISO 8601 UTC)
 * A config block with no saturation_* key at all reads as "none" (a file written before QA-A-296). Any saturation_* key makes the
 * whole record mandatory: all common fields typed and in range, frames_used <= frames_total, `determined` needs a level and
 * frames_used >= 1, `undetermined` must not carry a level (frames_used may be > 0). A violation is a load error.
 */
struct XpeSatRecord {
    bool known{false};        ///< the config block carries a valid status
    bool determined{false};
    uint32_t level{0};
};

struct CalibSnapshot;
/**
 * The median of the scalar gain values of the pixels that count: inside the geometric valid area (the imaging-area layer of a version-2
 * defect file when it has one, else the whole frame), not in the defect map, not classified defective by the gain calibration
 * (QA-A-296 D3). Element m/2 of the sorted values. 0 when the snapshot has no scalar gain or no pixel is left. Cached for the
 * (gain map, defect map) pair of the snapshot and computed again when either map is replaced.
 */
float xpe_sat_median_for(const CalibSnapshot& calib);

struct XpeConfigDoc;
/** Reads the record from a parsed config block. False when a present key is malformed (the file is refused). */
bool xpe_sat_record_from_config(const XpeConfigDoc& doc, XpeSatRecord* out) noexcept;

/** Appends the record's keys (with a leading comma) to a JSON object text that is not closed yet. */
void xpe_sat_record_append_json(std::string* json, bool determined, uint32_t level, const XpeSatParams& p,
                                uint32_t framesUsed, uint32_t framesTotal);

/**
 * Applies the output rule in place to a float32 frame the gain stage has just written:
 * a pixel whose raw value is >= level becomes sOut; every other pixel is limited to sOut.
 * `raw` is the pipeline input (the caller's uint16 frame, before offset and gain).
 */
void xpe_sat_apply(float* out, const uint16_t* raw, size_t n, uint32_t level, float sOut) noexcept;
