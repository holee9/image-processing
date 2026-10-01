/**
 * @file ai_log.h
 * @brief The logging macros of xpe_ai (internal; not installed).
 *
 * The call sites in ai.cpp are PRINTF-style: AI_LOG_INFO("model_dir=%s, ep=%d", dir, ep). spdlog formats
 * with {} and not %, so forwarding such a call to spdlog::info unchanged logged the placeholders
 * literally ("model_dir=%s, ep=%d") and dropped the values -- including the worker failure code and the
 * consecutive-failure count (QA-B-177). Here the printf call is formatted first and its RESULT is handed
 * to spdlog as data, so call-site strings stay as they were written.
 *
 * Properties the tests pin (test_ai_log_macros*.cpp, test_ai_log_format.cpp):
 *   - values are printed, % conversions behave as printf's, "%%" is a percent sign;
 *   - braces in the arguments (a path, an ONNX error message) are data, never a spdlog format;
 *   - a literal with braces and no arguments is logged as written (a regression guard: spdlog itself logs a
 *     lone argument as-is, and the printf front end must not start treating it as a format);
 *   - no fixed buffer: a long message is not truncated;
 *   - each AI_LOG_* keeps its own spdlog level.
 *
 * With MSVC the format parameter carries _Printf_format_string_ for Code Analysis (/analyze). The ordinary
 * compile does NOT check it: a call whose conversion does not match its argument ("%s" for an unsigned)
 * built without a warning (measured, QA-B-177). Mismatches are therefore caught by the tests that read the
 * log text, not by the compiler.
 */
#pragma once

// @MX:NOTE: [AUTO] spdlog is a soft dependency; logging falls back to
//           no-op if not linked.
#ifdef XPE_AI_USE_SPDLOG
#include <spdlog/spdlog.h>

#include <cstdarg>
#include <cstdio>
#include <string>

#ifdef _MSC_VER
#include <sal.h>
#define XPE_AI_PRINTF_FORMAT _In_z_ _Printf_format_string_
#else
#define XPE_AI_PRINTF_FORMAT
#endif

namespace xpe::ai::detail {

/** vsnprintf into a std::string of exactly the needed size (no fixed buffer). */
inline std::string VFormat(const char* fmt, std::va_list ap) {
    std::va_list measure;
    va_copy(measure, ap);
    const int n = std::vsnprintf(nullptr, 0, fmt, measure);
    va_end(measure);
    if (n < 0) {
        // An encoding error in the arguments: say so, and keep the format so the call is still findable.
        return std::string("<log formatting failed> ") + fmt;
    }
    std::string text(static_cast<size_t>(n), '\0');
    std::vsnprintf(&text[0], text.size() + 1, fmt, ap);
    return text;
}

/**
 * printf-style front end to spdlog: the formatted text goes in as an ARGUMENT of "{}", never as a format.
 * (With the spdlog in use, a lone std::string argument is logged as-is anyway; "{}" states the intent without
 * relying on overload resolution.)
 */
inline void LogPrintf(spdlog::level::level_enum level, XPE_AI_PRINTF_FORMAT const char* fmt, ...) {
    if (!spdlog::default_logger_raw()->should_log(level)) {
        return;   // a disabled level costs no formatting
    }
    std::va_list ap;
    va_start(ap, fmt);
    const std::string text = VFormat(fmt, ap);
    va_end(ap);
    spdlog::log(level, "{}", text);
}

}  // namespace xpe::ai::detail

#define AI_LOG_TRACE(...) ::xpe::ai::detail::LogPrintf(spdlog::level::trace, __VA_ARGS__)
#define AI_LOG_DEBUG(...) ::xpe::ai::detail::LogPrintf(spdlog::level::debug, __VA_ARGS__)
#define AI_LOG_INFO(...)  ::xpe::ai::detail::LogPrintf(spdlog::level::info, __VA_ARGS__)
#define AI_LOG_WARN(...)  ::xpe::ai::detail::LogPrintf(spdlog::level::warn, __VA_ARGS__)
#define AI_LOG_ERROR(...) ::xpe::ai::detail::LogPrintf(spdlog::level::err, __VA_ARGS__)
#else
#include <cstdio>
// One statement each (do/while), so a call is safe after an unbraced if.
#define AI_LOG_TRACE(...) do {} while (0)
#define AI_LOG_DEBUG(...) do {} while (0)
#define AI_LOG_INFO(...)  do { std::printf("[AI INFO] " __VA_ARGS__); std::printf("\n"); } while (0)
#define AI_LOG_WARN(...)  do { std::printf("[AI WARN] " __VA_ARGS__); std::printf("\n"); } while (0)
#define AI_LOG_ERROR(...) do { std::printf("[AI ERROR] " __VA_ARGS__); std::printf("\n"); } while (0)
#endif
