/**
 * @file test_ai_log_macros_spdlog.cpp
 * @brief The SPDLOG branch of the AI_LOG_* macros (QA-B-177). Its own translation unit: it defines
 *        XPE_AI_USE_SPDLOG for ai_log.h, which the printf-branch tests (test_ai_log_macros.cpp) must not.
 *
 * The log call sites of ai.cpp are printf-style ("model_dir=%s, ep=%d"); spdlog formats with {}. These
 * tests pin that the macros turn a printf call into the text it says, and that data is never read as a
 * format.
 */
#define XPE_AI_USE_SPDLOG 1
#include "ai_log.h"

#include <gtest/gtest.h>

#include <spdlog/sinks/ostream_sink.h>

#include <memory>
#include <sstream>
#include <string>

namespace {

/** Replaces the default logger with one writing "level|text" lines to a string; restores it afterwards. */
class SpdlogCapture {
public:
    SpdlogCapture() : previous_(spdlog::default_logger()) {
        auto sink = std::make_shared<spdlog::sinks::ostream_sink_mt>(out_);
        logger_ = std::make_shared<spdlog::logger>("ai_log_test", sink);
        logger_->set_pattern("%l|%v");
        logger_->set_level(spdlog::level::trace);
        spdlog::set_default_logger(logger_);
    }
    ~SpdlogCapture() { spdlog::set_default_logger(previous_); }
    /** The captured text with line ends normalised: spdlog ends a line with CR LF on Windows. */
    std::string Text() {
        logger_->flush();
        std::string text = out_.str();
        for (size_t pos; (pos = text.find("\r\n")) != std::string::npos;) text.erase(pos, 1);
        return text;
    }

private:
    std::ostringstream out_;
    std::shared_ptr<spdlog::logger> previous_;
    std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace

TEST(AiLogMacrosSpdlog, FormatsPrintfArgumentsInsteadOfPrintingPlaceholders) {
    SpdlogCapture cap;
    AI_LOG_INFO("a=%d s=%s z=%zu u=%u", 5, "xyz", static_cast<size_t>(7), 3u);
    EXPECT_EQ("info|a=5 s=xyz z=7 u=3\n", cap.Text());
}

TEST(AiLogMacrosSpdlog, EachLevelKeepsItsSpdlogLevel) {
    SpdlogCapture cap;
    AI_LOG_TRACE("t=%d", 1);
    AI_LOG_DEBUG("d=%d", 2);
    AI_LOG_INFO("i=%d", 3);
    AI_LOG_WARN("w=%d", 4);
    AI_LOG_ERROR("e=%d", 5);
    EXPECT_EQ("trace|t=1\ndebug|d=2\ninfo|i=3\nwarning|w=4\nerror|e=5\n", cap.Text());
}

// Data is data: a path or an error message may contain braces, and must not be read as a spdlog format.
TEST(AiLogMacrosSpdlog, BracesInTheDataAreNotAFormat) {
    SpdlogCapture cap;
    AI_LOG_INFO("no model at %s", "C:/models/{id}/bone_suppress.onnx");
    AI_LOG_ERROR("session failed: %s", "bad {0} and {} here");
    EXPECT_EQ("info|no model at C:/models/{id}/bone_suppress.onnx\n"
              "error|session failed: bad {0} and {} here\n", cap.Text());
}

// A REGRESSION GUARD, not a past defect: spdlog logs a lone argument as-is (this passes against the old
// macro too), so a literal with braces and no arguments was never lost. The printf front end must keep it
// that way and not start reading braces as a format.
TEST(AiLogMacrosSpdlog, ALiteralWithBracesAndNoArgumentsIsLoggedAsWritten) {
    SpdlogCapture cap;
    AI_LOG_WARN("config looks like {json} but is not");
    EXPECT_EQ("warning|config looks like {json} but is not\n", cap.Text());
}

TEST(AiLogMacrosSpdlog, APercentSignIsWrittenWithPercentPercent) {
    SpdlogCapture cap;
    AI_LOG_INFO("100%% sure, %d done", 3);
    EXPECT_EQ("info|100% sure, 3 done\n", cap.Text());
}

// The text is not cut at a fixed buffer size: model paths and ONNX error messages can be long.
TEST(AiLogMacrosSpdlog, ALongMessageIsNotTruncated) {
    SpdlogCapture cap;
    const std::string longArg(5000, 'x');
    AI_LOG_ERROR("model unreadable: %s", longArg.c_str());
    EXPECT_EQ("error|model unreadable: " + longArg + "\n", cap.Text());
}

TEST(AiLogMacrosSpdlog, ACallIsOneStatementAfterAnUnbracedIf) {
    SpdlogCapture cap;
    const bool yes = false;
    if (yes)
        AI_LOG_INFO("then");
    else
        AI_LOG_WARN("else %d", 1);
    EXPECT_EQ("warning|else 1\n", cap.Text());
}
