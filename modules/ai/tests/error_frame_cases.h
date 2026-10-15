/**
 * @file error_frame_cases.h
 * @brief The ERROR frames a worker may and may not send, as data shared by the tests of every request type
 *        (QA-B-191 M4f, QA-B-193).
 *
 * The bridge has ONE parser for a worker's ERROR frame (ai_ipc_bridge.cpp ParseWorkerErrorFrame). These lists are
 * what its tests feed it, so body-part recognition and bone suppression are held to the same cases.
 */
#ifndef XPE_AI_TEST_ERROR_FRAME_CASES_H
#define XPE_AI_TEST_ERROR_FRAME_CASES_H

#include <string>
#include <utility>
#include <vector>

namespace error_frame_cases {

/** ERROR frames no request type accepts: a protocol fault whatever the request. */
inline std::vector<std::pair<const char*, std::string>> ForbiddenEverywhere() {
    return {
        // a flag that contradicts its code: "the model cannot be used" goes with IO_FAILED or CONFIG_INVALID only
        {"flag true with PROCESSING_FAILED", R"({"error_code":-3,"model_unavailable":true,"error_message":"x"})"},
        {"flag true with INVALID_INPUT", R"({"error_code":-1,"model_unavailable":true})"},
        {"flag true with BUFFER_TOO_SMALL", R"({"error_code":-8,"model_unavailable":true})"},
        {"flag true with an unknown code", R"({"error_code":-50,"model_unavailable":true})"},
        // the flag itself
        {"flag a string", R"({"error_code":-4,"model_unavailable":"true"})"},
        {"flag a number", R"({"error_code":-4,"model_unavailable":1})"},
        {"flag null", R"({"error_code":-4,"model_unavailable":null})"},
        {"flag twice", R"({"error_code":-4,"model_unavailable":true,"model_unavailable":true})"},
        // the code
        {"no code", R"({"error_message":"x"})"},
        {"code twice", R"({"error_code":-4,"error_code":-3})"},
        {"code zero", R"({"error_code":0})"},
        {"code minus zero", R"({"error_code":-0})"},
        {"code positive", R"({"error_code":3})"},
        {"code positive with two digits", R"({"error_code":13})"},    // only the sign check refuses these two:
        {"code positive, the largest", R"({"error_code":99})"},      // the length and digit checks all pass
        {"code a lone minus", R"({"error_code":-})"},
        {"code below the range", R"({"error_code":-100})"},
        {"code with a leading zero", R"({"error_code":-03})"},
        {"code a decimal", R"({"error_code":-3.0})"},
        {"code with an exponent", R"({"error_code":-3e0})"},
        {"code a string", R"({"error_code":"-3"})"},
        {"code null", R"({"error_code":null})"},
        // the message
        {"message a number", R"({"error_code":-3,"error_message":5})"},
        {"message null", R"({"error_code":-3,"error_message":null})"},
        {"message with an unknown escape", R"({"error_code":-3,"error_message":"a\nb"})"},
        // built by concatenation: written inside a raw string, the compiler turns the escape into a letter
        {"message with a unicode escape", std::string(R"({"error_code":-3,"error_message":"a)") + "\\u0041" + R"(b"})"},
        {"message with a raw control character", std::string(R"({"error_code":-3,"error_message":"a)") + "\x07" + R"(b"})"},
        {"message never closed", R"({"error_code":-3,"error_message":"abc})"},
        // the object
        {"trailing junk", R"({"error_code":-3}x)"},
        {"nested object", R"({"error_code":-3,"detail":{"a":1}})"},
        {"an array", R"([{"error_code":-3}])"},
        {"empty object", "{}"},
        {"empty body", ""},
        {"not JSON", "error -3"},
    };
}

}  // namespace error_frame_cases

#endif  // XPE_AI_TEST_ERROR_FRAME_CASES_H
