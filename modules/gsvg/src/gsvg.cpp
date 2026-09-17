/**
 * @file gsvg.cpp
 * @brief GSVG (Grid Shadow and Vignette Gain) correction implementation.
 *
 * Implements a handle-based init/process/shutdown lifecycle independent of
 * the main XPE pipeline. Two selectable correction steps:
 *
 *  - Vignette gain correction: pixel-wise multiplication by a caller-supplied
 *    float32 gain map, followed by a 0..65535 clamp.
 *  - Grid line suppression (#180): recursive db4 2D DWT, grid detection on
 *    the input spectrum and in the detail sub-band (3 sigma), and a Gaussian
 *    band-stop on the detected sub-band only (Tang et al. 2015). An image in
 *    which no grid is detected is left byte-identical. See grid_dwt.h.
 *
 * Scalar reference only. Deterministic: identical input always produces
 * identical output on any platform.
 *
 * SPEC: Part of Lane B GSVG module. BP-06..09 benchmark coverage.
 */

#include "xpe/gsvg/gsvg_api.h"
#include "grid_dwt.h"
#include <cstdio>

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
// #145 (QA-B-60): report top-level config keys this parser does not consume.
//
// QA-B-59 measured the cost of staying quiet: a config naming only
// `gridFrequency_lp_per_mm` and `virtual_grid_enabled` -- both of which the
// documentation describes -- changes nothing and returns XPE_OK. The caller
// believes the setting was applied. This module reads two boolean leaves and
// nothing else, so anything a caller writes beyond those is a typo, a key meant
// for a different module, or a feature the documentation promises and the code
// does not have.
//
// The warning names the key. "Unknown key present" does not help someone find
// `grid_supression`; "grid_supression" does.
//
// RETURN CODES ARE UNCHANGED. Rejecting an unknown key is a behaviour change and
// a separate decision (#145); this only makes the silence audible.
//
// Scanning, not parsing: this module has no JSON library and gaining one for a
// warning would be a dependency bought with a diagnostic. The scanner walks the
// text tracking brace depth and quotes, and reports a name ONLY when it is at
// depth 1 and followed by ':'. Every ambiguity resolves toward silence -- a
// missed unknown key costs the warning that would have been nice to have, while
// a false warning on a correct config trains the reader to ignore warnings, and
// that costs the next real one.
void warn_unconsumed_top_level_keys(const char* json,
                                    const char* const* knownKeys,
                                    size_t knownCount)
{
    if (json == nullptr) return;   // NULL config means defaults, not a mistake.

    const std::string text(json);
    size_t depth = 0;
    bool   inString = false;
    bool   escaped = false;
    std::string current;
    size_t currentStart = std::string::npos;

    for (size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];

        if (inString) {
            if (escaped)           { escaped = false; current.push_back(c); continue; }
            if (c == '\\')         { escaped = true;  continue; }
            if (c == '"')          { inString = false; continue; }
            current.push_back(c);
            continue;
        }

        if (c == '"') {
            inString = true;
            current.clear();
            currentStart = i;
            continue;
        }
        if (c == '{' || c == '[') { ++depth; continue; }
        if (c == '}' || c == ']') { if (depth > 0) --depth; continue; }

        if (c == ':' && depth == 1 && currentStart != std::string::npos) {
            bool known = false;
            for (size_t k = 0; k < knownCount; ++k) {
                if (current == knownKeys[k]) { known = true; break; }
            }
            if (!known && !current.empty()) {
                char msg[192];
                std::snprintf(msg, sizeof(msg),
                              "gsvg config key '%s' is not read by this module and "
                              "has no effect", current.c_str());
                // The alert queue is the channel SRS-ALERT defines and the one
                // a host already polls; this module does not otherwise log, so
                // adding a logger for one diagnostic would buy a dependency with
                // a warning.
                xpe_alert_push(msg, XPE_ALERT_WARNING);
            }
            currentStart = std::string::npos;
            continue;
        }

        // Any other non-space token means the last string was a value, not a key.
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != ',') {
            currentStart = std::string::npos;
        }
    }
}

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

} // namespace

const char* xpe_gsvg_version(void)
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

    // #145: the two keys above are the whole config surface. Anything else the
    // caller wrote is reported by name rather than dropped in silence.
    {
        static const char* const kKnownKeys[] = { "vignette_correction", "grid_suppression" };
        warn_unconsumed_top_level_keys(configJsonOrNull, kKnownKeys,
                                       sizeof(kKnownKeys) / sizeof(kKnownKeys[0]));
    }

    *handleOut = h;
    return XPE_OK;
}

XpeErrorCode xpe_gsvg_process(void* handle,
                              const uint16_t* src,
                              size_t srcCount,
                              uint16_t* dst,
                              size_t dstCount,
                              int width,
                              int height,
                              const float* gainMap,
                              size_t gainCount)
{
    // A NULL handle is a NULL required pointer, so it is INVALID_INPUT — the
    // same code dicom returns for a NULL handle (dicom.cpp:56,67) and what the
    // api-spec precedence contract requires (#119). "Not initialised" would be
    // use-after-shutdown, which is a dangling pointer here, not a NULL one.
    if (!handle) return XPE_ERR_INVALID_INPUT;
    if (!src || !dst) return XPE_ERR_INVALID_INPUT;
    if (width <= 0 || height <= 0) return XPE_ERR_INVALID_INPUT;

    auto* h = static_cast<GsvgHandle*>(handle);
    const size_t count = static_cast<size_t>(width) * static_cast<size_t>(height);

    // #152: the buffers must actually hold the image the dimensions promise.
    // Judged after the NULL and zero checks above, per the api-spec ordering
    // rule -- a missing buffer is a different fault from a small one, and
    // saying so in the right order keeps the two distinguishable.
    //
    // Lengths are ELEMENT counts (see the header): each is compared against
    // count directly, in the same units width and height are stated in.
    if (srcCount < count || dstCount < count) {
        return XPE_ERR_INVALID_INPUT;
    }
    // A NULL gainMap means the vignette step is off, so its length is not
    // consulted at all. A gain map that IS supplied must be long enough --
    // QA-B-52 measured this one reading past its end with the step enabled.
    if (gainMap != nullptr && gainCount < count) {
        return XPE_ERR_INVALID_INPUT;
    }

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
        xpe_gsvg_detail::SuppressGrid(dst, width, height);
    }

    return XPE_OK;
}

XpeErrorCode xpe_gsvg_shutdown(void* handle)
{
    if (handle == nullptr) return XPE_OK;
    delete static_cast<GsvgHandle*>(handle);
    return XPE_OK;
}
