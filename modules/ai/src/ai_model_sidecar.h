/**
 * @file ai_model_sidecar.h
 * @brief The model sidecar `<name>.json`: what it must say, and the model card made from it (QA-B-197, #130).
 *
 * T-005a (REQ-AI-008): a model's metadata travels in the sidecar beside the model file. The sidecar is part of what
 * the signature covers (QA-B-195), and it is read ONLY after the signature verified, from the verified bytes. This
 * file holds the one definition of what a usable sidecar is; OnnxSession::Create applies it, so every path that loads
 * a model (in-process bone suppression and body-part recognition, the worker) refuses a model whose sidecar does not
 * say what REQ-AI-008 requires.
 *
 * T-005b (REQ-AI-010 / 011): the model card is made ONLY from a sidecar that passed. Nothing in a card is a constant
 * of this module: a field the sidecar does not carry is `null`, never a made-up value.
 *
 * REQ-AI-008, verbatim: "Model versioning shall follow semver; model metadata shall include: model_id, version,
 * pccp_scope, training_data_hash, validation_metrics." The REQUIRED fields below are exactly those five.
 *
 *   field                 required  rule
 *   model_id              yes       string, 1 to 64 characters of letters, digits, '.', '_' and '-'
 *   version               yes       string, semantic version 2.0.0 (MAJOR.MINOR.PATCH[-pre][+build])
 *   pccp_scope            yes       non-empty string
 *   training_data_hash    yes       non-empty string
 *   validation_metrics    yes       non-empty JSON object
 *   intended_use          optional  string                      -- REQ-AI-010
 *   training_data_summary optional  string                      -- REQ-AI-010
 *   demographic_performance optional JSON object                -- REQ-AI-010
 *   limitations           optional  string                      -- REQ-AI-010
 *   published_date        optional  string, a calendar date YYYY-MM-DD -- REQ-AI-010
 * Keys this file does not name (`labels` of a body-part model, `note`) are not judged here: the sidecar's other
 * readers judge their own keys. A key that IS named and has the wrong type is a refusal, not a skip.
 *
 * A top-level key that appears twice is a refusal (the signed text would say two things).
 *
 * The sidecar is read with nlohmann's SAX interface, not into a JSON document: freeing a non-empty document allocates
 * inside a noexcept destructor and terminates the process when memory runs out at that instant (QA-B-194 M5), and this
 * is read on the cold path of every model load.
 */
#ifndef XPE_AI_MODEL_SIDECAR_H
#define XPE_AI_MODEL_SIDECAR_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace xpe::ai {

/** A sidecar larger than this is refused unparsed. An implementation safety cap, not a requirement. */
constexpr size_t kMaxSidecarBytes = 1u << 20;

/** What a verified, valid sidecar says. Plain strings: no JSON document is kept alive after parsing. */
struct ModelSidecar {
    std::string model_id;
    std::string version;
    std::string pccp_scope;
    std::string training_data_hash;
    std::string validation_metrics_json;   ///< compact JSON object text

    bool has_intended_use = false;
    std::string intended_use;
    bool has_training_data_summary = false;
    std::string training_data_summary;
    bool has_demographic_performance = false;
    std::string demographic_performance_json;   ///< compact JSON object text
    bool has_limitations = false;
    std::string limitations;
    bool has_published_date = false;
    std::string published_date;
};

/** 1 to 64 characters of letters, digits, '.', '_' and '-'. */
inline bool IsModelIdText(const std::string& id) {
    if (id.empty() || id.size() > 64) return false;
    for (const char ch : id) {
        const bool ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
                        ch == '.' || ch == '_' || ch == '-';
        if (!ok) return false;
    }
    return true;
}

namespace detail {
inline bool AllOf(const std::string& s, size_t from, size_t to, bool (*pred)(char)) {
    if (from >= to) return false;
    for (size_t i = from; i < to; ++i) {
        if (!pred(s[i])) return false;
    }
    return true;
}
inline bool IsDigit(char c) { return c >= '0' && c <= '9'; }
inline bool IsIdentChar(char c) { return IsDigit(c) || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '-'; }
/** A dot-separated list of identifiers in s[from, to); numeric identifiers have no leading zero when @p noLeadingZero. */
inline bool IsIdentList(const std::string& s, size_t from, size_t to, bool numericNoLeadingZero) {
    if (from >= to) return false;
    size_t start = from;
    for (size_t i = from; i <= to; ++i) {
        if (i == to || s[i] == '.') {
            if (!AllOf(s, start, i, IsIdentChar)) return false;
            if (numericNoLeadingZero && AllOf(s, start, i, IsDigit) && i - start > 1 && s[start] == '0') return false;
            start = i + 1;
        }
    }
    return true;
}
}  // namespace detail

/** Semantic Versioning 2.0.0: MAJOR.MINOR.PATCH, no leading zeros, optional -prerelease and +build. */
inline bool IsSemverText(const std::string& v) {
    size_t i = 0;
    for (int part = 0; part < 3; ++part) {
        const size_t start = i;
        while (i < v.size() && detail::IsDigit(v[i])) ++i;
        if (i == start || i - start > 18) return false;
        if (i - start > 1 && v[start] == '0') return false;
        if (part < 2) {
            if (i >= v.size() || v[i] != '.') return false;
            ++i;
        }
    }
    if (i == v.size()) return true;
    size_t plus = v.find('+', i);
    if (v[i] == '-') {
        const size_t preEnd = plus == std::string::npos ? v.size() : plus;
        if (!detail::IsIdentList(v, i + 1, preEnd, /*numericNoLeadingZero=*/true)) return false;
        i = preEnd;
    }
    if (i == v.size()) return true;
    if (v[i] != '+') return false;
    return detail::IsIdentList(v, i + 1, v.size(), /*numericNoLeadingZero=*/false);
}

/** A calendar date written YYYY-MM-DD. */
inline bool IsIsoDateText(const std::string& d) {
    if (d.size() != 10 || d[4] != '-' || d[7] != '-') return false;
    for (const size_t i : {0u, 1u, 2u, 3u, 5u, 6u, 8u, 9u}) {
        if (!detail::IsDigit(d[i])) return false;
    }
    const int y = (d[0] - '0') * 1000 + (d[1] - '0') * 100 + (d[2] - '0') * 10 + (d[3] - '0');
    const int m = (d[5] - '0') * 10 + (d[6] - '0');
    const int day = (d[8] - '0') * 10 + (d[9] - '0');
    if (m < 1 || m > 12 || day < 1) return false;
    static const int kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leap = (y % 4 == 0 && y % 100 != 0) || y % 400 == 0;
    return day <= kDays[m - 1] + ((m == 2 && leap) ? 1 : 0);
}

/** @p s as a JSON string literal (quotes included); control characters and the two characters JSON must escape. */
inline std::string JsonQuote(const std::string& s) {
    std::string r = "\"";
    for (const char ch : s) {
        const unsigned char u = static_cast<unsigned char>(ch);
        switch (ch) {
            case '"': r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n"; break;
            case '\r': r += "\\r"; break;
            case '\t': r += "\\t"; break;
            default:
                if (u < 0x20) {
                    static const char kHex[] = "0123456789abcdef";
                    r += "\\u00";
                    r += kHex[u >> 4];
                    r += kHex[u & 0xF];
                } else {
                    r += ch;
                }
        }
    }
    r += '"';
    return r;
}

namespace detail {

/**
 * Reads the sidecar WITHOUT building a JSON document (nlohmann's SAX interface). Only the top-level members this file
 * names are kept, as plain strings; an object value is kept as its compact text. Why not a document: nlohmann 3.11.3
 * frees a non-empty parsed document with an allocation inside a noexcept destructor, so when memory runs out at that
 * instant the process terminates (the QA-B-194 M5 finding). A sidecar is read on the cold path of every model load, and
 * that path is covered by the allocation-failure sweeps: with no document there is nothing to terminate.
 */
class SidecarSax final : public nlohmann::json::json_sax_t {
public:
    enum class Kind { kNone, kNull, kBool, kNumber, kString, kObject, kArray };
    struct Entry {
        bool seen = false;
        Kind kind = Kind::kNone;
        std::string str;     ///< the value of a string
        std::string dump;    ///< the compact text of an object value
        size_t members = 0;  ///< the number of members of an object value
    };
    Entry model_id, version, pccp_scope, training_data_hash, validation_metrics, intended_use, training_data_summary,
        demographic_performance, limitations, published_date;
    bool rootChecked = false;
    bool rootIsObject = false;
    std::string duplicate;   ///< the first top-level key that appears twice

    using number_integer_t = nlohmann::json::number_integer_t;
    using number_unsigned_t = nlohmann::json::number_unsigned_t;
    using number_float_t = nlohmann::json::number_float_t;
    using string_t = nlohmann::json::string_t;
    using binary_t = nlohmann::json::binary_t;

    bool null() override { return scalar(Kind::kNull, std::string(), "null"); }
    bool boolean(bool v) override { return scalar(Kind::kBool, std::string(), v ? "true" : "false"); }
    bool number_integer(number_integer_t v) override { return scalar(Kind::kNumber, std::string(), std::to_string(v)); }
    bool number_unsigned(number_unsigned_t v) override { return scalar(Kind::kNumber, std::string(), std::to_string(v)); }
    bool number_float(number_float_t, const string_t& literal) override { return scalar(Kind::kNumber, std::string(), literal); }
    bool string(string_t& v) override { return scalar(Kind::kString, v, JsonQuote(v)); }
    bool binary(binary_t&) override { return false; }
    bool start_object(std::size_t) override { return open(Kind::kObject, '{'); }
    bool start_array(std::size_t) override { return open(Kind::kArray, '['); }
    bool end_object() override { return close('}'); }
    bool end_array() override { return close(']'); }
    bool key(string_t& k) override {
        if (depth_ == 1) {
            for (const std::string& seen : keys_) {
                if (seen == k && duplicate.empty()) duplicate = k;
            }
            keys_.push_back(k);
            cur_ = lookup(k);
            if (cur_ != nullptr) {
                *cur_ = Entry();
                cur_->seen = true;
            }
        } else if (cur_ != nullptr) {
            if (!comma_.empty()) {
                if (comma_.back()) cur_->dump += ',';
                comma_.back() = true;
            }
            if (depth_ == 2) ++cur_->members;
            cur_->dump += JsonQuote(k);
            cur_->dump += ':';
            afterKey_ = true;
        }
        return true;
    }
    bool parse_error(std::size_t, const std::string&, const nlohmann::detail::exception&) override { return false; }

private:
    size_t depth_ = 0;                 ///< 0 outside the root object, 1 inside it, deeper inside a member's container
    Entry* cur_ = nullptr;             ///< the entry the current top-level member belongs to (null: a key not named here)
    std::vector<bool> comma_;          ///< per open container of the current member: has an element been written?
    bool afterKey_ = false;
    std::vector<std::string> keys_;    ///< the top-level keys seen, to find a duplicate

    Entry* lookup(const std::string& k) {
        if (k == "model_id") return &model_id;
        if (k == "version") return &version;
        if (k == "pccp_scope") return &pccp_scope;
        if (k == "training_data_hash") return &training_data_hash;
        if (k == "validation_metrics") return &validation_metrics;
        if (k == "intended_use") return &intended_use;
        if (k == "training_data_summary") return &training_data_summary;
        if (k == "demographic_performance") return &demographic_performance;
        if (k == "limitations") return &limitations;
        if (k == "published_date") return &published_date;
        return nullptr;
    }
    /** An element of an open container of the current member: the comma that separates it from the one before. */
    void beginElement() {
        if (cur_ == nullptr) return;
        if (afterKey_) {
            afterKey_ = false;
        } else if (!comma_.empty()) {
            if (comma_.back()) cur_->dump += ',';
            comma_.back() = true;
        }
    }
    bool scalar(Kind k, const std::string& str, const std::string& token) {
        if (!rootChecked) {   // the root is not an object
            rootChecked = true;
            return false;
        }
        if (depth_ == 1) {
            if (cur_ != nullptr) {
                cur_->kind = k;
                if (k == Kind::kString) cur_->str = str;
            }
            return true;
        }
        if (cur_ != nullptr) {
            beginElement();
            cur_->dump += token;
        }
        return true;
    }
    bool open(Kind k, char bracket) {
        if (!rootChecked) {
            rootChecked = true;
            if (k != Kind::kObject) return false;   // the root is not an object
            rootIsObject = true;
            depth_ = 1;
            return true;
        }
        if (depth_ == 1) {   // a top-level member that is itself a container
            if (cur_ != nullptr) {
                cur_->kind = k;
                cur_->dump = std::string(1, bracket);
                comma_.assign(1, false);
            }
        } else if (cur_ != nullptr) {
            beginElement();
            cur_->dump += bracket;
            comma_.push_back(false);
        }
        ++depth_;
        return true;
    }
    bool close(char bracket) {
        if (depth_ >= 2) {
            if (cur_ != nullptr) {
                cur_->dump += bracket;
                if (!comma_.empty()) comma_.pop_back();
            }
            --depth_;
        } else if (depth_ == 1) {
            depth_ = 0;
        }
        return true;
    }
};

}  // namespace detail

/**
 * Judge the sidecar text. @p text is null when there is no sidecar file. On success fills @p out and returns true; on
 * failure returns false with the reason in @p reason (a short English sentence naming the field, used in the alert).
 */
inline bool ParseModelSidecar(const std::string* text, ModelSidecar* out, std::string* reason) {
    using Kind = detail::SidecarSax::Kind;
    if (text == nullptr) {
        *reason = "there is no sidecar file (REQ-AI-008 requires the model metadata)";
        return false;
    }
    if (text->size() > kMaxSidecarBytes) {
        *reason = "the sidecar is too large";
        return false;
    }
    detail::SidecarSax h;
    const bool parsed = nlohmann::json::sax_parse(*text, &h);
    if (!parsed) {
        *reason = (h.rootChecked && !h.rootIsObject) ? "the sidecar is not a JSON object" : "the sidecar is not valid JSON";
        return false;
    }
    if (!h.rootIsObject) {
        *reason = "the sidecar is not a JSON object";
        return false;
    }
    if (!h.duplicate.empty()) {
        *reason = "the key " + h.duplicate + " appears more than once";
        return false;
    }
    ModelSidecar sc;

    auto requiredString = [&](const char* key, const detail::SidecarSax::Entry& e, std::string* dst) -> bool {
        if (!e.seen) {
            *reason = std::string("the required field ") + key + " is missing";
            return false;
        }
        if (e.kind != Kind::kString) {
            *reason = std::string("the field ") + key + " is not a string";
            return false;
        }
        *dst = e.str;
        return true;
    };
    auto optionalString = [&](const char* key, const detail::SidecarSax::Entry& e, bool* has, std::string* dst) -> bool {
        if (!e.seen) return true;
        if (e.kind != Kind::kString) {
            *reason = std::string("the field ") + key + " is not a string";
            return false;
        }
        *has = true;
        *dst = e.str;
        return true;
    };

    if (!requiredString("model_id", h.model_id, &sc.model_id)) return false;
    if (!IsModelIdText(sc.model_id)) {
        *reason = "the field model_id is not 1 to 64 letters, digits, '.', '_' or '-'";
        return false;
    }
    if (!requiredString("version", h.version, &sc.version)) return false;
    if (!IsSemverText(sc.version)) {
        *reason = "the field version is not a semantic version (MAJOR.MINOR.PATCH)";
        return false;
    }
    if (!requiredString("pccp_scope", h.pccp_scope, &sc.pccp_scope)) return false;
    if (sc.pccp_scope.empty()) {
        *reason = "the field pccp_scope is empty";
        return false;
    }
    if (!requiredString("training_data_hash", h.training_data_hash, &sc.training_data_hash)) return false;
    if (sc.training_data_hash.empty()) {
        *reason = "the field training_data_hash is empty";
        return false;
    }
    {
        const auto& e = h.validation_metrics;
        if (!e.seen) {
            *reason = "the required field validation_metrics is missing";
            return false;
        }
        if (e.kind != Kind::kObject) {
            *reason = "the field validation_metrics is not a JSON object";
            return false;
        }
        if (e.members == 0) {
            *reason = "the field validation_metrics is empty";
            return false;
        }
        sc.validation_metrics_json = e.dump;
    }
    if (!optionalString("intended_use", h.intended_use, &sc.has_intended_use, &sc.intended_use)) return false;
    if (!optionalString("training_data_summary", h.training_data_summary, &sc.has_training_data_summary,
                        &sc.training_data_summary)) {
        return false;
    }
    if (!optionalString("limitations", h.limitations, &sc.has_limitations, &sc.limitations)) return false;
    if (!optionalString("published_date", h.published_date, &sc.has_published_date, &sc.published_date)) return false;
    if (sc.has_published_date && !IsIsoDateText(sc.published_date)) {
        *reason = "the field published_date is not a calendar date YYYY-MM-DD";
        return false;
    }
    if (h.demographic_performance.seen) {
        if (h.demographic_performance.kind != Kind::kObject) {
            *reason = "the field demographic_performance is not a JSON object";
            return false;
        }
        sc.has_demographic_performance = true;
        sc.demographic_performance_json = h.demographic_performance.dump;
    }
    *out = std::move(sc);
    return true;
}

/**
 * The model card (REQ-AI-010) of a model whose sidecar passed. Built by string concatenation, not through a JSON
 * document, so that producing a card allocates only strings (the allocation-failure sweeps cover it).
 *
 * `pccp_status` is `"not_evaluated"`: the module has no authorized PCCP to compare `pccp_scope` with (T-011, REQ-AI-110),
 * and "within_boundary" or "not_applicable" would be a claim it cannot make. The card says what the sidecar says
 * (`pccp_scope`) and that the module did not judge it.
 */
inline std::string BuildModelCardJson(const ModelSidecar& s) {
    auto optStr = [](bool has, const std::string& v) { return has ? JsonQuote(v) : std::string("null"); };
    std::string r = "{";
    r += "\"model_id\":" + JsonQuote(s.model_id);
    r += ",\"model_version\":" + JsonQuote(s.version);
    r += ",\"intended_use\":" + optStr(s.has_intended_use, s.intended_use);
    r += ",\"training_data_summary\":" + optStr(s.has_training_data_summary, s.training_data_summary);
    r += ",\"demographic_performance\":" +
         (s.has_demographic_performance ? s.demographic_performance_json : std::string("null"));
    r += ",\"limitations\":" + optStr(s.has_limitations, s.limitations);
    r += ",\"pccp_status\":\"not_evaluated\"";
    r += ",\"published_date\":" + optStr(s.has_published_date, s.published_date);
    r += ",\"pccp_scope\":" + JsonQuote(s.pccp_scope);
    r += ",\"training_data_hash\":" + JsonQuote(s.training_data_hash);
    r += ",\"validation_metrics\":" + s.validation_metrics_json;
    r += "}";
    return r;
}

}  // namespace xpe::ai

#endif  // XPE_AI_MODEL_SIDECAR_H
