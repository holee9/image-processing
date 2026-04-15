/**
 * @file helpers.cpp
 * @brief Shared internal helpers: interpolation, JSON field extractor
 * SPEC: SPEC-XPE-P1A v1.0.0  IEC 62304 Class B
 */

#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <cstring>
#include <cstdio>
#include <algorithm>

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
        // All 4-connected neighbors are defective; fall back to diagonals
        try_add(static_cast<int>(x) - 1, static_cast<int>(y) - 1);
        try_add(static_cast<int>(x) + 1, static_cast<int>(y) - 1);
        try_add(static_cast<int>(x) - 1, static_cast<int>(y) + 1);
        try_add(static_cast<int>(x) + 1, static_cast<int>(y) + 1);
    }

    return (count > 0) ? sum / static_cast<float>(count) : 0.0f;
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
