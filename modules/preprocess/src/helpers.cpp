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
std::string xpe_json_get_string(const char* configJson, const char* key) {
    if (!configJson || !key) return {};

    // Search for: "key"
    char needle[128];
    std::snprintf(needle, sizeof(needle), "\"%s\"", key);
    const char* pos = std::strstr(configJson, needle);
    if (!pos) return {};

    // Skip past "key":
    pos += std::strlen(needle);
    while (*pos && (*pos == ' ' || *pos == '\t' || *pos == ':')) ++pos;
    if (*pos != '"') return {};

    ++pos; // skip opening quote
    const char* end = std::strchr(pos, '"');
    if (!end) return {};

    return std::string(pos, end);
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
