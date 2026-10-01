/**
 * @file xpe_strict_parse.hpp
 * @brief Strict number parsing for values taken from a caller's configuration JSON (QA-A-202, #233).
 *
 * std::stof / std::stoi / std::stod throw on a malformed value and accept trailing garbage ("12abc" is 12), so a
 * single bad character in a configuration made an extern "C" function throw. These helpers never throw: the
 * whole string must be one number, in range, and finite. They use std::from_chars, which does not depend on
 * the C locale (a decimal comma cannot change what a configuration means), accepts no leading whitespace and no
 * leading '+', and does not parse hexadecimal.
 *
 * A failed parse leaves `*out` untouched.
 */
#ifndef XPE_STRICT_PARSE_HPP
#define XPE_STRICT_PARSE_HPP

#include <charconv>
#include <cmath>
#include <cstdint>
#include <string>
#include <system_error>

namespace xpe_strict {

/** The whole of `v` is one finite floating-point number representable as T (float or double). */
template <typename T>
inline bool parse_real(const std::string& v, T* out) noexcept
{
    if (v.empty()) return false;
    T value{};
    const char* first = v.data();
    const char* last  = v.data() + v.size();
    const std::from_chars_result r = std::from_chars(first, last, value);
    if (r.ec != std::errc() || r.ptr != last) return false;   // malformed, out of range, or trailing characters
    if (!std::isfinite(value)) return false;                  // "inf", "nan" are accepted by from_chars
    *out = value;
    return true;
}

inline bool parse_float(const std::string& v, float* out) noexcept { return parse_real<float>(v, out); }
inline bool parse_double(const std::string& v, double* out) noexcept { return parse_real<double>(v, out); }

/** The whole of `v` is one base-10 integer that fits in int32_t. */
inline bool parse_int(const std::string& v, int32_t* out) noexcept
{
    if (v.empty()) return false;
    int32_t value = 0;
    const char* first = v.data();
    const char* last  = v.data() + v.size();
    const std::from_chars_result r = std::from_chars(first, last, value, 10);
    if (r.ec != std::errc() || r.ptr != last) return false;
    *out = value;
    return true;
}

}  // namespace xpe_strict

#endif  // XPE_STRICT_PARSE_HPP
