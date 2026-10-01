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
    bool firstWasString() const { return firstQuoted_; }

    bool null() override { return scalar("null", false); }
    bool boolean(bool v) override { return scalar(v ? "true" : "false", false); }
    bool number_integer(Json::number_integer_t v) override { return scalar(std::to_string(v), false); }
    bool number_unsigned(Json::number_unsigned_t v) override { return scalar(std::to_string(v), false); }
    bool number_float(Json::number_float_t, const Json::string_t& text) override { return scalar(text, false); }
    bool string(Json::string_t& v) override { return scalar(v, true); }
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
    bool scalar(std::string text, bool isString) {
        if (depth_ == 0) return false;
        if (depth_ == 1 && pending_) {
            pending_ = false;
            if (found_ == 1) {
                firstKind_ = XpeJsonTop::Scalar;
                firstValue_ = std::move(text);
                firstQuoted_ = isString;
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
    bool firstQuoted_ = false;
};

}  // namespace

XpeJsonTop xpe_json_top_level_scalar(const char* json, size_t len, const char* key, std::string* value, bool* quoted) {
    if (!json || !key) return XpeJsonTop::Absent;

    // An empty (or all-white-space) config has no keys; it is not malformed. Everything else must parse.
    const char* const end = json + len;
    const char* first = json;
    while (first != end && (*first == ' ' || *first == '\t' || *first == '\n' || *first == '\r')) ++first;
    if (first == end) return XpeJsonTop::Absent;

    // A NUL byte is refused here, not left to the parser: nlohmann-json 3.11.3 lexes a NUL outside a string as
    // the END of the input (token end_of_input), so even `strict` accepts "{}<NUL>{bad}" and never reads past the
    // NUL (QA-A-208c: observed by the NUL rows of test_config_strict_parse.cpp; inside a string it is already a
    // control-character error). No JSON text holds one, so this refuses nothing valid.
    if (std::memchr(json, 0, len) != nullptr) return XpeJsonTop::Malformed;

    // QA-A-208b (Codex #40): the text is parsed by nlohmann-json -- the parser the repository already carries
    // (third_party/common/vcpkg.json) -- through its SAX interface, with `strict` (nothing may follow the value)
    // and no comments, over the `len` bytes given (QA-A-208c, Codex #43; a NUL is refused above). The parse reports
    // errors by returning false: no exception for a malformed text. (An
    // allocation failure is a std::bad_alloc, which the callers' guards turn into XPE_ERR_OUT_OF_MEMORY.)
    TopLevelProbe probe(key);
    const bool ok = nlohmann::json::sax_parse(json, end, &probe,
                                              nlohmann::json::input_format_t::json,
                                              /*strict=*/true, /*ignore_comments=*/false);
    if (!ok || !probe.topIsObject()) return XpeJsonTop::Malformed;

    if (probe.found() == 0) return XpeJsonTop::Absent;
    if (probe.found() > 1) return XpeJsonTop::Duplicate;
    if (probe.firstKind() == XpeJsonTop::Scalar) {
        if (quoted) *quoted = probe.firstWasString();
        if (value) *value = probe.takeFirstValue();
    }
    return probe.firstKind();
}

/* =========================================================================
 * Configuration JSON readers (QA-A-209)
 *
 * One rule for every configuration text -- the pipeline's, the ghost corrector's, the nonlinearity stage's, the
 * offset generation's, and the config block of a calibration file: a key is a TOP-LEVEL key of one valid JSON
 * object (xpe_json_top_level_scalar, nlohmann-json). The first occurrence of a quoted name anywhere in the text
 * used to win, so {"nested":{"bypassGain":true}} switched the gain stage off.
 *   - a key absent from the top level (also: present only inside a nested object or array)   -> not given
 *   - a top-level value that is a string, empty or not                                       -> the caller decides;
 *     the readers' callers treat an EMPTY string as not given (the pipeline configuration's rule, QA-A-202b)
 *   - a value that is an object or an array (not a scalar)                                   -> not given (as before)
 *   - the key given twice at the top level, or a text that is not one valid JSON object      -> XPE_ERR_CONFIG_INVALID
 *   - a null text pointer                                                                    -> not given
 * ========================================================================= */
XpeErrorCode xpe_config_get_string(const char* configJson, const char* key, std::string* value, bool* quoted) {
    value->clear();
    if (quoted) *quoted = false;
    if (!configJson || !key) return XPE_OK;

    std::string v;
    bool q = false;
    switch (xpe_json_top_level_scalar(configJson, std::strlen(configJson), key, &v, &q)) {
        case XpeJsonTop::Absent:
        case XpeJsonTop::NotScalar:
            return XPE_OK;
        case XpeJsonTop::Scalar:
            *value = std::move(v);
            if (quoted) *quoted = q;
            return XPE_OK;
        default:   // Duplicate, Malformed
            return XPE_ERR_CONFIG_INVALID;
    }
}

XpeErrorCode xpe_config_get_double(const char* json, size_t len, const char* key, bool* present, double* value) {
    *present = false;
    if (!json || !key) return XPE_OK;

    std::string v;
    bool q = false;
    switch (xpe_json_top_level_scalar(json, len, key, &v, &q)) {
        case XpeJsonTop::Absent:
        case XpeJsonTop::NotScalar:
            return XPE_OK;
        case XpeJsonTop::Scalar: {
            // A number is a bare JSON number; a string, true, false and null are not one (the reader has always
            // taken the first for "not given": the number is read where the value starts, not out of a string).
            if (q) return XPE_OK;
            char* end = nullptr;
            const double d = std::strtod(v.c_str(), &end);
            if (end == v.c_str()) return XPE_OK;
            *value = d;
            *present = true;
            return XPE_OK;
        }
        default:   // Duplicate, Malformed
            return XPE_ERR_CONFIG_INVALID;
    }
}
