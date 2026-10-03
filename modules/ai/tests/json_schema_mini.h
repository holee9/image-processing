/**
 * @file json_schema_mini.h
 * @brief A small JSON Schema checker for the tests of the model card and the model sidecar (QA-B-197).
 *
 * The schemas under the repository-root schemas/ directory are JSON Schema documents; this checks an instance against them without a
 * schema library (a dependency for a dozen fields was judged not worth its supply-chain cost). It supports exactly the
 * keywords those schemas use, and it REFUSES a schema that uses any other keyword, so a keyword it does not know can
 * never be silently ignored (which would make a schema look stricter than what is checked):
 *   type (a name or an array of names), enum, const, required, properties, additionalProperties (boolean),
 *   pattern, minLength, minProperties, oneOf, and the annotations $schema, $id, title, description.
 * Test code only.
 */
#ifndef XPE_AI_TEST_JSON_SCHEMA_MINI_H
#define XPE_AI_TEST_JSON_SCHEMA_MINI_H

#include <nlohmann/json.hpp>

#include <regex>
#include <set>
#include <string>
#include <vector>

namespace xpe_test {

namespace schema_detail {

inline const std::set<std::string>& Known() {
    static const std::set<std::string> k = {"$schema", "$id", "title", "description", "type", "enum", "const",
                                            "required", "properties", "additionalProperties", "pattern",
                                            "minLength", "minProperties", "oneOf"};
    return k;
}

inline bool TypeMatches(const std::string& t, const nlohmann::json& v) {
    if (t == "string") return v.is_string();
    if (t == "object") return v.is_object();
    if (t == "array") return v.is_array();
    if (t == "null") return v.is_null();
    if (t == "boolean") return v.is_boolean();
    if (t == "number") return v.is_number();
    if (t == "integer") return v.is_number_integer();
    return false;
}

/** Appends a message to @p errors for every way @p v fails @p schema; @p path names where @p v is. */
inline void Check(const nlohmann::json& schema, const nlohmann::json& v, const std::string& path,
                  std::vector<std::string>* errors) {
    for (auto it = schema.begin(); it != schema.end(); ++it) {
        if (Known().count(it.key()) == 0) errors->push_back(path + ": the schema uses a keyword this checker does not support: " + it.key());
    }
    if (schema.contains("type")) {
        const auto& t = schema["type"];
        bool ok = false;
        if (t.is_string()) {
            ok = TypeMatches(t.get<std::string>(), v);
        } else {
            for (const auto& e : t) ok = ok || TypeMatches(e.get<std::string>(), v);
        }
        if (!ok) errors->push_back(path + ": wrong type");
    }
    if (schema.contains("enum")) {
        bool ok = false;
        for (const auto& e : schema["enum"]) ok = ok || e == v;
        if (!ok) errors->push_back(path + ": not one of the enum values");
    }
    if (schema.contains("const") && !(schema["const"] == v)) errors->push_back(path + ": not the constant value");
    if (v.is_string()) {
        const std::string s = v.get<std::string>();
        if (schema.contains("minLength") && s.size() < schema["minLength"].get<size_t>()) errors->push_back(path + ": too short");
        if (schema.contains("pattern") && !std::regex_search(s, std::regex(schema["pattern"].get<std::string>()))) {
            errors->push_back(path + ": does not match the pattern");
        }
    }
    if (v.is_object()) {
        if (schema.contains("minProperties") && v.size() < schema["minProperties"].get<size_t>()) errors->push_back(path + ": too few properties");
        if (schema.contains("required")) {
            for (const auto& r : schema["required"]) {
                if (!v.contains(r.get<std::string>())) errors->push_back(path + ": missing required " + r.get<std::string>());
            }
        }
        const nlohmann::json props = schema.contains("properties") ? schema["properties"] : nlohmann::json::object();
        for (auto it = v.begin(); it != v.end(); ++it) {
            if (props.contains(it.key())) {
                Check(props[it.key()], it.value(), path + "/" + it.key(), errors);
            } else if (schema.contains("additionalProperties") && schema["additionalProperties"] == false) {
                errors->push_back(path + ": property not allowed: " + it.key());
            }
        }
    }
    if (schema.contains("oneOf")) {
        int matches = 0;
        for (const auto& alt : schema["oneOf"]) {
            std::vector<std::string> sub;
            Check(alt, v, path, &sub);
            if (sub.empty()) ++matches;
        }
        if (matches != 1) errors->push_back(path + ": matches " + std::to_string(matches) + " of the oneOf alternatives, not exactly one");
    }
}

}  // namespace schema_detail

/** The ways @p instanceText fails @p schemaText; empty when it conforms. Both are JSON texts. */
inline std::vector<std::string> SchemaErrors(const std::string& schemaText, const std::string& instanceText) {
    std::vector<std::string> errors;
    const nlohmann::json schema = nlohmann::json::parse(schemaText, nullptr, false);
    if (schema.is_discarded()) return {"the schema is not valid JSON"};
    const nlohmann::json instance = nlohmann::json::parse(instanceText, nullptr, false);
    if (instance.is_discarded()) return {"the instance is not valid JSON"};
    schema_detail::Check(schema, instance, "", &errors);
    return errors;
}

}  // namespace xpe_test

#endif  // XPE_AI_TEST_JSON_SCHEMA_MINI_H
