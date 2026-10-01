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
 * The events of one SAX pass of nlohmann-json over a configuration text, reduced to the members of the top-level
 * object: one entry per member, in order, with the scalar text when the value is a string, a number, true, false or
 * null. nlohmann hands over keys with their escapes already interpreted and rejects everything that is not JSON; a
 * callback that returns false stops the parse. A member given twice at the top level is noted, whatever its name.
 */
class DocBuilder final : public nlohmann::json::json_sax_t {
public:
    using Json = nlohmann::json;

    explicit DocBuilder(XpeConfigDoc* doc) : doc_(doc) {}

    bool topIsObject() const { return topIsObject_; }
    bool hasDuplicate() const { return duplicate_; }

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
        if (depth_ == 1) {
            if (doc_->find(k) != nullptr) duplicate_ = true;
            doc_->entries.push_back(XpeConfigEntry{k, std::string(), false, false});
            slot_ = doc_->entries.size() - 1;
            pending_ = true;
        }
        return true;
    }
    bool parse_error(std::size_t, const std::string&, const nlohmann::detail::exception&) override { return false; }

private:
    /** A scalar event: at the top level that is not an object; at depth 1 it is the value of the member just named. */
    bool scalar(std::string text, bool isString) {
        if (depth_ == 0) return false;
        if (depth_ == 1 && pending_) {
            XpeConfigEntry& e = doc_->entries[slot_];
            e.text = std::move(text);
            e.scalar = true;
            e.quoted = isString;
            pending_ = false;
        }
        return true;
    }
    /** An object or array that opens as the value of the member just named: the member is there, and is not a scalar. */
    void nested() {
        if (depth_ == 1) pending_ = false;
    }

    XpeConfigDoc* doc_;
    int depth_ = 0;
    bool topIsObject_ = false;
    bool duplicate_ = false;
    bool pending_ = false;     // the member just named has no value yet
    size_t slot_ = 0;          // its entry
};

/** Parses `len` bytes at `text` into `doc`. Blank, a NUL byte, anything but one valid JSON object, and a member name
 *  given twice at the top level are XPE_ERR_CONFIG_INVALID (and leave `doc` empty). */
XpeErrorCode parse_text(const char* text, size_t len, XpeConfigDoc* doc) {
    const char* const end = text + len;
    const char* first = text;
    while (first != end && (*first == ' ' || *first == '\t' || *first == '\n' || *first == '\r')) ++first;
    if (first == end) return XPE_ERR_CONFIG_INVALID;      // nothing there: not a configuration

    // A NUL byte is refused here, not left to the parser: nlohmann-json 3.11.3 lexes a NUL outside a string as the
    // END of the input (token end_of_input), so even `strict` accepts "{}<NUL>{bad}" and never reads past the NUL
    // (QA-A-208c; inside a string it is already a control-character error). No JSON text holds one.
    if (std::memchr(text, 0, len) != nullptr) return XPE_ERR_CONFIG_INVALID;

    // nlohmann-json -- the parser the repository already carries (third_party/common/vcpkg.json) -- through its SAX
    // interface, with `strict` (nothing may follow the value) and no comments. It reports errors by returning false:
    // no exception for a malformed text. (An allocation failure is a std::bad_alloc, which the callers' guards map.)
    DocBuilder builder(doc);
    const bool ok = nlohmann::json::sax_parse(text, end, &builder, nlohmann::json::input_format_t::json,
                                              /*strict=*/true, /*ignore_comments=*/false);
    if (!ok || !builder.topIsObject() || builder.hasDuplicate()) {
        doc->entries.clear();
        return XPE_ERR_CONFIG_INVALID;
    }
    return XPE_OK;
}

}  // namespace

const XpeConfigEntry* XpeConfigDoc::find(const std::string& key) const {
    for (const XpeConfigEntry& e : entries) {
        if (e.key == key) return &e;
    }
    return nullptr;
}

bool XpeConfigDoc::getString(const char* key, std::string* value, bool* quoted) const {
    value->clear();
    if (quoted) *quoted = false;
    const XpeConfigEntry* e = find(std::string(key));
    if (!e || !e->scalar) return false;
    *value = e->text;
    if (quoted) *quoted = e->quoted;
    return true;
}

bool XpeConfigDoc::getNumber(const char* key, double* value) const {
    const XpeConfigEntry* e = find(std::string(key));
    if (!e || !e->scalar || e->quoted) return false;   // a string, true, false, null and an object are not a number
    char* end = nullptr;
    const double d = std::strtod(e->text.c_str(), &end);
    if (end == e->text.c_str()) return false;
    *value = d;
    return true;
}

/* =========================================================================
 * Configuration JSON (QA-A-209, QA-A-209b)
 *
 * One rule for every configuration text -- the pipeline's, the ghost corrector's, the nonlinearity stage's, the
 * offset generation's, and the config block of a calibration file -- and ONE parse of each: the text is parsed once
 * into a document of its top-level members (XpeConfigDoc), and the keys are read from that.
 *   - the text is ONE valid JSON object; a key is a TOP-LEVEL member (a name only inside a nested object or array is
 *     not given)
 *   - a member name given twice at the top level, whatever the name and whatever the values, is refused: the object
 *     is ambiguous, and which key the caller reads must not decide whether it is noticed
 *   - a text the caller supplies that is empty or only white space is refused: NULL is how a caller says "no
 *     configuration". The config block STORED in an XCal file is another case: length 0 is a file without one
 * All refusals are XPE_ERR_CONFIG_INVALID.
 * ========================================================================= */
XpeErrorCode xpe_config_parse(const char* text, XpeConfigDoc* doc) {
    doc->entries.clear();
    if (text == nullptr) return XPE_OK;
    return parse_text(text, std::strlen(text), doc);
}

XpeErrorCode xpe_config_parse_block(const char* text, size_t len, XpeConfigDoc* doc) {
    doc->entries.clear();
    if (len == 0) return XPE_OK;
    if (text == nullptr) return XPE_ERR_CONFIG_INVALID;
    return parse_text(text, len, doc);
}
