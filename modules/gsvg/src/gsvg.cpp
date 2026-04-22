/**
 * @file gsvg.cpp
 * @brief GSVG (Grid Shadow and Vignette Gain) correction implementation.
 *
 * Implements a handle-based init/process/shutdown lifecycle independent of
 * the main XPE pipeline. Two selectable correction steps:
 *
 *  - Vignette gain correction: pixel-wise multiplication by a caller-supplied
 *    float32 gain map, followed by a 0..65535 clamp.
 *  - Grid shadow suppression: per-row mean-deviation subtraction. For each
 *    row, the deviation of the row mean from the global mean is computed;
 *    if the absolute deviation exceeds a threshold the row is adjusted toward
 *    the global mean. This is a low-complexity baseline sufficient for
 *    DegradedMode and BP-06..09 benchmarks. A proper FFT-based notch filter
 *    is out of scope here (no FFT dependency available) and is documented
 *    as future work.
 *
 * Scalar reference only. Deterministic: identical input always produces
 * identical output on any platform.
 *
 * SPEC: Part of Lane B GSVG module. BP-06..09 benchmark coverage.
 */

#include "xpe/gsvg/gsvg_api.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <new>
#include <string>

namespace {

/**
 * @brief Internal handle state.
 */
struct GsvgHandle {
    bool vignette_enabled = false;
    bool grid_enabled     = false;
};

/**
 * @brief Minimal JSON boolean field extractor.
 *
 * Looks for a substring of the form `"key"` followed (possibly with whitespace
 * and a colon) by either `true` or `false`. Returns @p defaultValue if the
 * key is not found or the value token is unrecognized. This intentionally
 * avoids a full JSON parser — only boolean leaves are needed for GSVG config.
 *
 * @param json         Null-terminated JSON string. May be NULL.
 * @param key          Field name (without surrounding quotes).
 * @param defaultValue Value to return when key is absent or malformed.
 * @return Parsed boolean, or @p defaultValue.
 */
bool json_get_bool(const char* json, const char* key, bool defaultValue)
{
    if (!json || !key) return defaultValue;

    const std::string haystack(json);
    // Search for "key" (with surrounding quotes) as the field marker.
    const std::string needle = std::string("\"") + key + "\"";
    const auto keyPos = haystack.find(needle);
    if (keyPos == std::string::npos) return defaultValue;

    // Advance past the key token and find the colon that introduces the value.
    size_t cursor = keyPos + needle.size();
    while (cursor < haystack.size() && (haystack[cursor] == ' '
                                        || haystack[cursor] == '\t'
                                        || haystack[cursor] == '\r'
                                        || haystack[cursor] == '\n')) {
        ++cursor;
    }
    if (cursor >= haystack.size() || haystack[cursor] != ':') return defaultValue;
    ++cursor;
    while (cursor < haystack.size() && (haystack[cursor] == ' '
                                        || haystack[cursor] == '\t'
                                        || haystack[cursor] == '\r'
                                        || haystack[cursor] == '\n')) {
        ++cursor;
    }

    // Compare against "true" / "false" tokens.
    if (haystack.compare(cursor, 4, "true") == 0)  return true;
    if (haystack.compare(cursor, 5, "false") == 0) return false;
    return defaultValue;
}

/**
 * @brief Apply vignette gain: dst[i] = clamp(src[i] * gainMap[i], 0, 65535).
 *
 * Scalar reference implementation. Deterministic.
 *
 * @param src     Source uint16 pixels (read).
 * @param dst     Destination uint16 pixels (write). May alias src.
 * @param gain    Float32 gain map, same element count as src/dst.
 * @param count   Total pixel count (width * height).
 */
void apply_vignette_scalar(const uint16_t* src, uint16_t* dst,
                           const float* gain, size_t count)
{
    for (size_t i = 0; i < count; ++i) {
        const float v = static_cast<float>(src[i]) * gain[i];
        float clamped = v;
        if (clamped < 0.0f)      clamped = 0.0f;
        if (clamped > 65535.0f)  clamped = 65535.0f;
        dst[i] = static_cast<uint16_t>(clamped + 0.5f);
    }
}

/**
 * @brief Row-mean-deviation grid shadow suppression (in-place on dst).
 *
 * Baseline algorithm:
 *  1. Compute the global mean of the image.
 *  2. For each row, compute the row mean.
 *  3. If |row_mean - global_mean| > threshold, subtract (row_mean - global_mean)
 *     from every pixel in that row and clamp to 0..65535.
 *
 * Rationale: grid shadows manifest as rows whose mean deviates periodically
 * from the global mean. Subtracting the row-mean deviation removes the DC
 * component of the shadow while leaving spatial anatomical variation intact.
 * This is a deliberately simple baseline — a production system would use an
 * FFT-based notch filter or directional morphological filter. That upgrade
 * is future work; this baseline is sufficient for BP-06..09 determinism
 * tests and DegradedMode coverage.
 *
 * @param pixels Image pixels, modified in place.
 * @param width  Row width in pixels.
 * @param height Image height in rows.
 */
void suppress_grid_row_mean(uint16_t* pixels, int width, int height)
{
    if (width <= 0 || height <= 0 || pixels == nullptr) return;

    const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height);

    // 1. Global mean in double precision for determinism across platforms.
    double globalAcc = 0.0;
    for (size_t i = 0; i < count; ++i) {
        globalAcc += static_cast<double>(pixels[i]);
    }
    const double globalMean = globalAcc / static_cast<double>(count);

    // Threshold: mean deviation below this magnitude is treated as signal,
    // above it as grid shadow. 1.0 code is a conservative baseline that
    // still leaves the no-artifact case untouched (since synthetic test
    // images with uniform rows produce per-row means equal to the global
    // mean, deviation is exactly 0 and the branch is skipped).
    constexpr double kDeviationThreshold = 1.0;

    // 2/3. Per-row correction.
    for (int y = 0; y < height; ++y) {
        uint16_t* row = pixels + static_cast<size_t>(y) * static_cast<size_t>(width);
        double rowAcc = 0.0;
        for (int x = 0; x < width; ++x) {
            rowAcc += static_cast<double>(row[x]);
        }
        const double rowMean  = rowAcc / static_cast<double>(width);
        const double deviation = rowMean - globalMean;

        if (std::fabs(deviation) <= kDeviationThreshold) continue;

        for (int x = 0; x < width; ++x) {
            const double corrected = static_cast<double>(row[x]) - deviation;
            double clamped = corrected;
            if (clamped < 0.0)     clamped = 0.0;
            if (clamped > 65535.0) clamped = 65535.0;
            row[x] = static_cast<uint16_t>(clamped + 0.5);
        }
    }
}

} // namespace

const char* gsvg_version(void)
{
    return "0.2.0";
}

XpeErrorCode xpe_gsvg_init(void** handleOut, const char* configJsonOrNull)
{
    if (!handleOut) return XPE_ERR_INVALID_INPUT;

    auto* h = new (std::nothrow) GsvgHandle();
    if (!h) return XPE_ERR_OUT_OF_MEMORY;

    // Defaults: both correction steps OFF → pass-through behaviour when no
    // config is supplied. This aligns with DegradedMode expectations.
    h->vignette_enabled = json_get_bool(configJsonOrNull, "vignette_correction", false);
    h->grid_enabled     = json_get_bool(configJsonOrNull, "grid_suppression",    false);

    *handleOut = h;
    return XPE_OK;
}

XpeErrorCode xpe_gsvg_process(void* handle,
                              const uint16_t* src,
                              uint16_t* dst,
                              int width,
                              int height,
                              const float* gainMap)
{
    if (!handle) return XPE_ERR_NOT_INITIALIZED;
    if (!src || !dst) return XPE_ERR_INVALID_INPUT;
    if (width <= 0 || height <= 0) return XPE_ERR_INVALID_INPUT;

    auto* h = static_cast<GsvgHandle*>(handle);
    const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height);

    // Step 1: vignette gain or passthrough copy.
    // The vignette step is active only when BOTH the config flag is set AND
    // a gain map is provided. Either absent yields an identity copy.
    if (h->vignette_enabled && gainMap != nullptr) {
        apply_vignette_scalar(src, dst, gainMap, count);
    } else if (src != dst) {
        std::memcpy(dst, src, count * sizeof(uint16_t));
    }
    // If src == dst and no vignette, the image is already in place — no copy.

    // Step 2: grid shadow suppression applied in-place on dst.
    if (h->grid_enabled) {
        suppress_grid_row_mean(dst, width, height);
    }

    return XPE_OK;
}

XpeErrorCode xpe_gsvg_shutdown(void* handle)
{
    if (handle == nullptr) return XPE_OK;
    delete static_cast<GsvgHandle*>(handle);
    return XPE_OK;
}
