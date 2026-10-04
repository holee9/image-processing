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
bool xpe_find_nonfinite(const float* values, size_t n, size_t* count, size_t* first) noexcept
{
    constexpr uint32_t kExpMask = 0x7F800000u;   // every exponent bit set: NaN or +-infinity
    uint32_t bad = 0;
    for (size_t i = 0; i < n; ++i) {
        uint32_t b;
        std::memcpy(&b, values + i, sizeof(b));
        bad |= static_cast<uint32_t>((b & kExpMask) == kExpMask);
    }
    if (bad == 0) return false;
    size_t c = 0, f = 0;
    for (size_t i = 0; i < n; ++i) {
        uint32_t b;
        std::memcpy(&b, values + i, sizeof(b));
        if ((b & kExpMask) == kExpMask) { if (c == 0) f = i; ++c; }
    }
    if (count) *count = c;
    if (first) *first = f;
    return true;
}

void xpe_alert_nonfinite(const char* prefix, size_t count, size_t first, uint32_t width, const char* tail) noexcept
{
    try {
        char msg[400];
        std::snprintf(msg, sizeof(msg), "%s %zu pixel(s) of the input frame are NaN or infinite (first: index %zu, x=%zu, y=%zu); %s",
                      prefix, count, first, width ? first % width : size_t{0}, width ? first / width : size_t{0}, tail);
        msg[sizeof(msg) - 1] = '\0';
        xpe_alert_push(msg, XPE_ALERT_ERROR);
    } catch (...) {
        // [no-throw-boundary] advisory
    }
}

// The signature is the one declared in xpe_preprocess_internal.h and called by the tests with these six arguments; the four
// uint32_t are a position and a size, in that order, as everywhere in this module.
float xpe_interpolate_pixel(const float* pixels, const uint8_t* defectMask,
                             // Public ABI signature (declared in the internal header, called by the tests): the order cannot change.
                             // NOLINTNEXTLINE(bugprone-easily-swappable-parameters)
                             uint32_t x, uint32_t y,
                             uint32_t width, uint32_t height) noexcept
{
    return xpe_interpolate_pixel_masked(pixels, [defectMask](size_t idx) { return defectMask[idx] != 0; }, x, y, width, height);
}

#ifdef XPE_CACHE_TEST_HOOKS
unsigned long xpe_config_parse_calls = 0;   // test-only counter (QA-A-209c); not compiled into the library
#endif

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
#ifdef XPE_CACHE_TEST_HOOKS
    ++xpe_config_parse_calls;
#endif
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
