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

#include <atomic>
#include <cstdlib>
#include <memory>
#include <new>
#include <sstream>
#include <string>

namespace {

// Allocation-failure injection (QA-B-177 follow-up). Replacing the global operator new affects every test in
// this executable, but the replacement is a plain malloc unless a test ARMS it, and a test arms it only
// around the one call under test -- gtest's own allocations are never failed.
std::atomic<long> g_allocsLeftBeforeFailure{-1};   // -1: disarmed; n >= 0: allocation number n+1 throws

}  // namespace

void* operator new(std::size_t n) {
    long left = g_allocsLeftBeforeFailure.load(std::memory_order_relaxed);
    while (left >= 0) {
        if (left == 0) {
            g_allocsLeftBeforeFailure.store(-1, std::memory_order_relaxed);   // one failure per arming
            throw std::bad_alloc();
        }
        if (g_allocsLeftBeforeFailure.compare_exchange_weak(left, left - 1, std::memory_order_relaxed)) break;
    }
    if (void* p = std::malloc(n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete(void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }

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

// The log call is inside extern "C" exports of the DLL and must never throw (QA-B-177 follow-up, #233's
// hazard: bad_alloc leaving the C ABI skips the unwinding of the exporting function's locals under /EHsc).
// Fail the 1st, 2nd, ... allocation made INSIDE the call, one at a time, until a call allocates fewer times
// than that: every allocation point in formatting and in spdlog is covered, whatever their number.
TEST(AiLogMacrosSpdlog, NoAllocationFailureEscapesALogCall) {
    SpdlogCapture cap;
    const std::string longArg(2000, 'y');   // longer than any small-string buffer: formatting must allocate
    bool callReachedTheEnd = false;
    int failuresInjected = 0;
    for (long failAt = 0; failAt < 64; ++failAt) {
        bool threw = false;
        g_allocsLeftBeforeFailure.store(failAt);
        try {
            AI_LOG_ERROR("model unreadable: %s", longArg.c_str());
        } catch (...) {
            threw = true;
        }
        const bool fired = g_allocsLeftBeforeFailure.load() == -1;   // the failure was actually injected
        g_allocsLeftBeforeFailure.store(-1);
        EXPECT_FALSE(threw) << "an allocation failure at allocation #" << failAt + 1 << " escaped the log call";
        if (!fired) {
            callReachedTheEnd = true;   // the call made fewer than failAt+1 allocations: all points swept
            break;
        }
        ++failuresInjected;
    }
    // Controls: the sweep injected real failures (not vacuous) and ended by running out of allocations.
    EXPECT_GE(failuresInjected, 1) << "control: the log call made no allocation, so nothing was injected";
    EXPECT_TRUE(callReachedTheEnd) << "control: the sweep never reached a call that allocated less";
    // And a failure-free call after the sweep still logs normally (the swallow did not break the logger).
    // A fresh capture: an injected failure inside THIS capture's ostream sink may have left the stream failed.
    SpdlogCapture fresh;
    AI_LOG_INFO("after %d", 7);
    EXPECT_EQ("info|after 7\n", fresh.Text());
}
