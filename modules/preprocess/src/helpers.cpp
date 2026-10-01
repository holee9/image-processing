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
#include <string>

#include <nlohmann/json.hpp>

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

/**
 * The events of one pass of nlohmann::json's SAX parser over a config text, reduced to what
 * xpe_json_top_level_scalar needs: how many times the wanted key appears among the members of the top-level
 * object, and what the first such member's value is. nlohmann hands over keys with their escapes already
 * interpreted (so "fit\u005fr_squared" arrives as fit_r_squared) and rejects everything that is not JSON; a
 * callback that returns false stops the parse.
 */
class TopLevelProbe final : public nlohmann::json::json_sax_t {
public:
    using Json = nlohmann::json;

    explicit TopLevelProbe(const char* wanted) : wanted_(wanted) {}

    bool topIsObject() const { return topIsObject_; }
    int found() const { return found_; }
    XpeJsonTop firstKind() const { return firstKind_; }
    std::string takeFirstValue() { return std::move(firstValue_); }

    bool null() override { return scalar("null"); }
    bool boolean(bool v) override { return scalar(v ? "true" : "false"); }
    bool number_integer(Json::number_integer_t v) override { return scalar(std::to_string(v)); }
    bool number_unsigned(Json::number_unsigned_t v) override { return scalar(std::to_string(v)); }
    bool number_float(Json::number_float_t, const Json::string_t& text) override { return scalar(text); }
    bool string(Json::string_t& v) override { return scalar(v); }
    bool binary(Json::binary_t&) override { return false; }   // binary is not JSON text

    bool start_object(std::size_t) override {
        if (depth_ == 0) topIsObject_ = true;
        else nested();
        ++depth_;
        return true;
    }
    bool end_object() override { --depth_; return true; }
    bool start_array(std::size_t) override {
        if (depth_ == 0) return false;        // the top level must be an object
        nested();
        ++depth_;
        return true;
    }
    bool end_array() override { --depth_; return true; }
    bool key(Json::string_t& k) override {
        if (depth_ == 1 && k == wanted_) {
            ++found_;
            pending_ = true;
        }
        return true;
    }
    bool parse_error(std::size_t, const std::string&, const nlohmann::detail::exception&) override { return false; }

private:
    /** A scalar event: at the top level that is not an object; at depth 1 it is the value of a top-level member. */
    bool scalar(std::string text) {
        if (depth_ == 0) return false;
        if (depth_ == 1 && pending_) {
            pending_ = false;
            if (found_ == 1) {
                firstKind_ = XpeJsonTop::Scalar;
                firstValue_ = std::move(text);
            }
        }
        return true;
    }
    /** An object or array that opens as the value of a top-level member. */
    void nested() {
        if (depth_ == 1 && pending_) {
            pending_ = false;
            if (found_ == 1) firstKind_ = XpeJsonTop::NotScalar;
        }
    }

    std::string wanted_;
    int depth_ = 0;
    bool topIsObject_ = false;
    bool pending_ = false;     // the key just seen is the wanted one and its value is next
    int found_ = 0;
    XpeJsonTop firstKind_ = XpeJsonTop::Absent;
    std::string firstValue_;
};

}  // namespace

XpeJsonTop xpe_json_top_level_scalar(const char* json, const char* key, std::string* value) {
    if (!json || !key) return XpeJsonTop::Absent;

    // An empty (or all-white-space) config has no keys; it is not malformed. Everything else must parse.
    const char* first = json;
    while (*first == ' ' || *first == '\t' || *first == '\n' || *first == '\r') ++first;
    if (!*first) return XpeJsonTop::Absent;

    // QA-A-208b (Codex #40): the text is parsed by nlohmann-json -- the parser the repository already carries
    // (third_party/common/vcpkg.json) -- through its SAX interface, with `strict` (nothing may follow the value)
    // and no comments. The parse reports errors by returning false: no exception for a malformed text. (An
    // allocation failure is a std::bad_alloc, which the callers' guards turn into XPE_ERR_OUT_OF_MEMORY.)
    TopLevelProbe probe(key);
    const bool ok = nlohmann::json::sax_parse(json, json + std::strlen(json), &probe,
                                              nlohmann::json::input_format_t::json,
                                              /*strict=*/true, /*ignore_comments=*/false);
    if (!ok || !probe.topIsObject()) return XpeJsonTop::Malformed;

    if (probe.found() == 0) return XpeJsonTop::Absent;
    if (probe.found() > 1) return XpeJsonTop::Duplicate;
    if (probe.firstKind() == XpeJsonTop::Scalar && value) *value = probe.takeFirstValue();
    return probe.firstKind();
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
