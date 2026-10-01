/**
 * @file helpers.cpp
 * @brief Shared internal helpers: interpolation, JSON field extractor
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <cstring>
#include <cstdio>
#include <algorithm>
#include <cstdlib>

/* =========================================================================
 * Edge-aware bilinear interpolation
 * Skips neighbours that are also marked as defective in defectMask.
 * ========================================================================= */
float xpe_interpolate_pixel(const float* pixels, const uint8_t* defectMask,
                             uint32_t x, uint32_t y,
                             uint32_t width, uint32_t height) noexcept
{
    // Collect non-defective 4-connected neighbors (N/S/E/W)
    float sum = 0.0f;
    int   count = 0;

    auto try_add = [&](int nx, int ny) {
        if (nx < 0 || ny < 0 || static_cast<uint32_t>(nx) >= width ||
                                  static_cast<uint32_t>(ny) >= height) return;
        const size_t idx = static_cast<size_t>(ny) * width + nx;
        if (defectMask[idx] == 0) { sum += pixels[idx]; ++count; }
    };

    try_add(static_cast<int>(x) - 1, static_cast<int>(y));
    try_add(static_cast<int>(x) + 1, static_cast<int>(y));
    try_add(static_cast<int>(x),     static_cast<int>(y) - 1);
    try_add(static_cast<int>(x),     static_cast<int>(y) + 1);

    if (count == 0) {
        // Cluster fallback: search the nearest complete ring of valid pixels.
        for (int radius = 1; radius <= 3 && count == 0; ++radius) {
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    if (std::max(std::abs(dx), std::abs(dy)) != radius) continue;
                    try_add(static_cast<int>(x) + dx, static_cast<int>(y) + dy);
                }
            }
        }
    }

    return (count > 0)
        ? sum / static_cast<float>(count)
        : pixels[static_cast<size_t>(y) * width + x];
}

/* =========================================================================
 * Minimal JSON string field extractor — no external dependency
 * Finds: "key": "value" pattern, returns value string.
 * ========================================================================= */
XpeJsonKey xpe_json_find_scalar(const char* configJson, const char* key, std::string* value) {
    if (!configJson || !key) return XpeJsonKey::Absent;

    // Search for: "key"
    char needle[128];
    std::snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char* pos = std::strstr(configJson, needle);
    if (!pos) return XpeJsonKey::Absent;

    // Skip past "key":
    pos += std::strlen(needle);
    while (*pos && (*pos == ' ' || *pos == '\t' || *pos == '\n' ||
                    *pos == '\r' || *pos == ':')) ++pos;

    if (*pos == '"') {
        ++pos; // skip opening quote
        const char* end = std::strchr(pos, '"');
        if (!end) return XpeJsonKey::NotScalar;
        *value = std::string(pos, end);
        return XpeJsonKey::Scalar;
    }

    // #126: unquoted scalar (true / false / number). The pipeline writes its
    // bypass flags as JSON booleans, and requiring quotes made those configs
    // silently do nothing -- the stage ran and failed later on missing
    // calibration instead. A nested object or array is not a scalar; this
    // extractor does not descend into one.
    if (*pos == '{' || *pos == '[' || *pos == '\0') return XpeJsonKey::NotScalar;

    const char* end = pos;
    while (*end && *end != ',' && *end != '}' && *end != ']' &&
           *end != ' ' && *end != '\t' && *end != '\n' && *end != '\r') ++end;

    *value = std::string(pos, end);
    return XpeJsonKey::Scalar;
}

std::string xpe_json_get_string(const char* configJson, const char* key) {
    std::string value;
    // An absent key, and a value that is not a scalar, are both "nothing" to this reader (the pipeline
    // configuration's rule: an empty value is an absent one). A caller that must tell them apart uses
    // xpe_json_find_scalar.
    return xpe_json_find_scalar(configJson, key, &value) == XpeJsonKey::Scalar ? value : std::string();
}

namespace {

void json_skip_ws(const char*& p) {
    while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') ++p;
}

/** `p` is at an opening quote. Returns the position after the closing quote, or nullptr if there is none. */
const char* json_skip_string(const char* p) {
    ++p;
    while (*p) {
        if (*p == '\\') {
            if (!p[1]) return nullptr;
            p += 2;
            continue;
        }
        if (*p == '"') return p + 1;
        ++p;
    }
    return nullptr;
}

/** `p` is at '{' or '['. Returns the position after the matching closer, or nullptr if it never closes. */
const char* json_skip_nested(const char* p) {
    int depth = 0;
    while (*p) {
        if (*p == '"') {
            p = json_skip_string(p);
            if (!p) return nullptr;
            continue;
        }
        if (*p == '{' || *p == '[') {
            ++depth;
        } else if (*p == '}' || *p == ']') {
            if (--depth == 0) return p + 1;
        }
        ++p;
    }
    return nullptr;
}

}  // namespace

XpeJsonTop xpe_json_top_level_scalar(const char* json, const char* key, std::string* value) {
    if (!json || !key) return XpeJsonTop::Absent;
    const char* p = json;
    json_skip_ws(p);
    if (!*p) return XpeJsonTop::Absent;            // an empty config has no keys
    if (*p != '{') return XpeJsonTop::Malformed;
    ++p;
    json_skip_ws(p);
    if (*p == '}') return XpeJsonTop::Absent;      // {}

    const size_t keyLen = std::strlen(key);
    int found = 0;
    XpeJsonTop firstKind = XpeJsonTop::Absent;
    std::string firstValue;
    for (;;) {
        json_skip_ws(p);
        if (*p != '"') return XpeJsonTop::Malformed;
        const char* nameBegin = p + 1;
        const char* afterName = json_skip_string(p);
        if (!afterName) return XpeJsonTop::Malformed;
        const bool match = static_cast<size_t>(afterName - 1 - nameBegin) == keyLen &&
                           std::strncmp(nameBegin, key, keyLen) == 0;
        p = afterName;
        json_skip_ws(p);
        if (*p != ':') return XpeJsonTop::Malformed;
        ++p;
        json_skip_ws(p);

        XpeJsonTop kind = XpeJsonTop::Scalar;
        std::string text;
        if (*p == '"') {
            const char* end = json_skip_string(p);
            if (!end) return XpeJsonTop::Malformed;
            text.assign(p + 1, end - 1);
            p = end;
        } else if (*p == '{' || *p == '[') {
            const char* end = json_skip_nested(p);
            if (!end) return XpeJsonTop::Malformed;
            kind = XpeJsonTop::NotScalar;
            p = end;
        } else {
            // A bare token (number, true, false, null) -- or nothing at all, which is an empty scalar.
            const char* end = p;
            while (*end && *end != ',' && *end != '}' && *end != ' ' && *end != '\t' && *end != '\n' && *end != '\r') ++end;
            if (!*end) return XpeJsonTop::Malformed;
            text.assign(p, end);
            p = end;
        }
        if (match) {
            if (++found == 1) {
                firstKind = kind;
                firstValue = std::move(text);
            }
        }
        json_skip_ws(p);
        if (*p == ',') { ++p; continue; }
        if (*p == '}') break;
        return XpeJsonTop::Malformed;
    }
    if (found == 0) return XpeJsonTop::Absent;
    if (found > 1) return XpeJsonTop::Duplicate;
    if (firstKind == XpeJsonTop::Scalar && value) *value = std::move(firstValue);
    return firstKind;
}

/**
 * Minimal JSON numeric field extractor — parses "key": number (int or float).
 * Returns defaultVal when key is absent or configJson is null.
 */
double xpe_json_get_double(const char* configJson, const char* key, double defaultVal) {
    if (!configJson || !key) return defaultVal;

    // Search for: "key"
    char needle[128];
    std::snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char* pos = std::strstr(configJson, needle);
    if (!pos) return defaultVal;

    // Skip past "key":
    pos += std::strlen(needle);
    while (*pos && (*pos == ' ' || *pos == '\t' || *pos == ':')) ++pos;

    // Parse numeric value (int or float, possibly negative)
    char* end = nullptr;
    double val = std::strtod(pos, &end);
    if (end == pos) return defaultVal; // no conversion performed

    return val;
}
