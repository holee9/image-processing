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
#include <spdlog/sinks/stdout_color_sinks.h>

#include "xpe/ai/ai_onnx_session.h"

#include <windows.h>

#include <atomic>
#include <cstdlib>
#include <memory>
#include <new>
#include <sstream>
#include <string>
#include <vector>

namespace {

// Allocation-failure injection (QA-B-177 follow-up). Replacing the global operator new affects every test in
// this executable, but the replacement is a plain malloc unless a test ARMS it, and a test arms it only
// around the one call under test -- gtest's own allocations are never failed.
std::atomic<long> g_allocsLeftBeforeFailure{-1};   // -1: disarmed; n >= 0: allocation number n+1 throws
// When true only allocations made while a log call is in progress (ai_log.h's test seam) are counted and
// failed; when false every allocation is (the first sweep test below).
std::atomic<bool> g_failOnlyInsideLogCalls{false};
thread_local int g_logCallDepth = 0;

}  // namespace

namespace xpe::ai::detail {
void LogTestScope(int delta) noexcept { g_logCallDepth += delta; }
}  // namespace xpe::ai::detail

void* operator new(std::size_t n) {
    const bool counted = !g_failOnlyInsideLogCalls.load(std::memory_order_relaxed) || g_logCallDepth > 0;
    long left = g_allocsLeftBeforeFailure.load(std::memory_order_relaxed);
    while (counted && left >= 0) {
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
// than that. Scope: the allocation points observed through THIS executable's operator new in the synchronous
// call with its in-process sink -- not every allocation of every sink (a file sink, the DLL's own allocator).
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

// ===== QA-B-177b (Codex #24 A1 + A2) =====================================================================

// A2: the default logger is null after spdlog::shutdown() / drop_all() or set_default_logger(nullptr). A null
// dereference is not an exception, so the catch in the log functions cannot stop it: the pointer must be
// checked. Every entry point is called with no default logger, then the logger is restored and must work.
TEST(AiLogMacrosSpdlog, AllEntryPointsTolerateANullDefaultLogger) {
    SpdlogCapture cap;
    auto restore = spdlog::default_logger();   // the capture's logger
    spdlog::set_default_logger(nullptr);
    ASSERT_EQ(nullptr, spdlog::default_logger_raw()) << "control: the default logger really is null now";
    AI_LOG_TRACE("t %d", 1);
    AI_LOG_DEBUG("d %d", 2);
    AI_LOG_INFO("i %s", "x");
    AI_LOG_WARN("w");
    AI_LOG_ERROR("x");
    AI_LOG_TEXT(spdlog::level::err, std::string("text ") + "100%s");
    ::xpe::ai::detail::LogText(spdlog::level::info, "direct");
    spdlog::set_default_logger(restore);   // the SpdlogCapture destructor restores the original afterwards
    ASSERT_EQ(restore.get(), spdlog::default_logger_raw());
    EXPECT_EQ("", cap.Text()) << "nothing may have been written while there was no logger";
    AI_LOG_INFO("back %d", 1);
    AI_LOG_TEXT(spdlog::level::warn, std::string("back ") + "again");
    EXPECT_EQ("info|back 1\nwarning|back again\n", cap.Text());
}

// A1: a finished-string message is never reinterpreted as a printf format.
TEST(AiLogMacrosSpdlog, LogTextKeepsPercentAndBracesAsData) {
    SpdlogCapture cap;
    AI_LOG_TEXT(spdlog::level::err, std::string("Model file not found: ") + "C:/m%s/%d/{x}/{}.onnx");
    EXPECT_EQ("error|Model file not found: C:/m%s/%d/{x}/{}.onnx\n", cap.Text());
}

// A1: the same one-at-a-time allocation-failure sweep, for the finished-string entry point, including the
// construction of its argument (the macro opens its guarded scope before the argument is evaluated).
TEST(AiLogMacrosSpdlog, NoAllocationFailureEscapesALogTextCall) {
    SpdlogCapture cap;
    const std::string longPart(2000, 'z');
    int injected = 0;
    bool swept = false;
    g_failOnlyInsideLogCalls.store(true);
    for (long failAt = 0; failAt < 64; ++failAt) {
        bool threw = false;
        g_allocsLeftBeforeFailure.store(failAt);
        try {
            AI_LOG_TEXT(spdlog::level::err, std::string("session failed: ") + longPart);
        } catch (...) {
            threw = true;
        }
        const bool fired = g_allocsLeftBeforeFailure.load() == -1;
        g_allocsLeftBeforeFailure.store(-1);
        EXPECT_FALSE(threw) << "an allocation failure at in-log allocation #" << failAt + 1 << " escaped";
        if (!fired) {
            swept = true;
            break;
        }
        ++injected;
    }
    g_failOnlyInsideLogCalls.store(false);
    EXPECT_GE(injected, 1) << "control: nothing was injected";
    EXPECT_TRUE(swept) << "control: the sweep never reached a call that allocated less";
}

// A1, end to end through the real code path: OnnxSession::Create with a model path that does not exist logs
// its report (LOG_ERROR, ai_onnx_session.cpp) -- the path xpe_bone_suppress takes in-process. Fail the
// allocations made INSIDE that log call, one at a time. Asserted per injection: no exception escapes, the
// result is the ORIGINAL error (model not found) with its original message, and the next call still works.
// Scope: allocations observed through this executable's operator new, in the synchronous log call, with this
// executable's compilation of ai_onnx_session.cpp -- the DLL has its own allocator and is not injectable.
TEST(AiLogMacrosSpdlog, ModelNotFoundReportSurvivesAnyAllocationFailureInItsLogCall) {
    SpdlogCapture cap;
    xpe::ai::OnnxSessionConfig cfg;
    cfg.model_path = "C:/xpe_log_probe/does_not_exist/" + std::string(1500, 'p') + ".onnx";
    const std::string expectedMessage = "Model file not found: " + cfg.model_path;
    int injected = 0;
    bool swept = false;
    g_failOnlyInsideLogCalls.store(true);
    for (long failAt = 0; failAt < 64; ++failAt) {
        bool threw = false;
        xpe::ai::OnnxErrorCode code = xpe::ai::OnnxErrorCode::kOk;
        std::string message;
        g_allocsLeftBeforeFailure.store(failAt);
        try {
            auto r = xpe::ai::OnnxSession::Create(cfg);
            code = r.code;
            message = r.message;
        } catch (...) {
            threw = true;
        }
        const bool fired = g_allocsLeftBeforeFailure.load() == -1;
        g_allocsLeftBeforeFailure.store(-1);
        EXPECT_FALSE(threw) << "in-log allocation #" << failAt + 1 << " failure escaped OnnxSession::Create";
        EXPECT_EQ(xpe::ai::OnnxErrorCode::kInvalidModelPath, code) << "failAt=" << failAt;
        EXPECT_EQ(expectedMessage, message) << "failAt=" << failAt;
        if (!fired) {
            swept = true;
            break;
        }
        ++injected;
    }
    g_failOnlyInsideLogCalls.store(false);
    EXPECT_GE(injected, 1) << "control: the log call made no allocation, nothing was injected";
    EXPECT_TRUE(swept) << "control: the sweep never reached a call that allocated less";
    // The next call still works and still logs (nothing was left locked or half-written).
    SpdlogCapture fresh;
    auto again = xpe::ai::OnnxSession::Create(cfg);
    EXPECT_EQ(xpe::ai::OnnxErrorCode::kInvalidModelPath, again.code);
    EXPECT_EQ("error|" + expectedMessage + "\n", fresh.Text());
}

// QA-B-183. The printf-branch tests redirect the C stdout to a file and put it back (_dup2 twice). _dup2 closes
// the OS handle that fd 1 held, and spdlog's default console sink had cached exactly that handle value when it
// was created. Afterwards the sink wrote to a closed handle value, and the next CreateFile -- in the worker
// tests, the client end of a named pipe -- was handed the same value, so product log lines arrived in the pipe
// where the worker expected a message header (worker log: ReadFile ERROR_MORE_DATA, next bytes "[2026-..").
// Single-process runs hit it intermittently, depending on which handle value was free next. The test repeats
// the cycle, opens handles until one has the value the sink captured, logs, and requires that nothing arrived.
/** Gives the default logger a console sink that holds the CURRENT stdout handle. */
void XpeTestRebindDefaultLoggerStdout() {
    spdlog::set_default_logger(std::make_shared<spdlog::logger>(
        "", std::make_shared<spdlog::sinks::stdout_color_sink_mt>()));
}

void XpeTestStdoutRedirectCycle();   // test_ai_log_macros.cpp

TEST(AiLogStdoutRedirect, ARedirectCycleDoesNotLeaveTheDefaultLoggerWritingIntoAnUnrelatedHandle) {
    // A console sink created now caches the current stdout handle; any default logger created in an
    // earlier test did the same with whatever the handle was then. Start from a known sink.
    spdlog::set_default_logger(std::make_shared<spdlog::logger>(
        "xpe_stale_probe", std::make_shared<spdlog::sinks::stdout_color_sink_mt>()));
    const HANDLE sinkHandle = GetStdHandle(STD_OUTPUT_HANDLE);
    XpeTestStdoutRedirectCycle();

    char tmp[MAX_PATH] = {0};
    GetTempPathA(sizeof(tmp), tmp);
    const std::string path =
        std::string(tmp) + "xpe_ai_stale_handle_probe_" + std::to_string(GetCurrentProcessId()) + ".txt";

    std::vector<HANDLE> held;
    HANDLE match = nullptr;
    for (int i = 0; i < 20000 && !match; ++i) {
        HANDLE h = CreateFileA(path.c_str(), GENERIC_READ | GENERIC_WRITE,
                               FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                               i == 0 ? CREATE_ALWAYS : OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (h == INVALID_HANDLE_VALUE) break;
        held.push_back(h);
        if (h == sinkHandle) match = h;
    }

    if (match) {
        // The value the sink captured now belongs to this file. Any log line the sink writes lands in it.
        spdlog::default_logger()->info("stale-handle-probe");
        spdlog::default_logger()->flush();
        EXPECT_EQ(0u, GetFileSize(match, nullptr))
            << "the default logger wrote into a handle it does not own";
    } else {
        DWORD flags = 0;
        EXPECT_TRUE(GetHandleInformation(sinkHandle, &flags) != 0)
            << "the captured stdout handle is neither reused nor still valid; nothing was proven";
    }

    for (HANDLE h : held) CloseHandle(h);
    DeleteFileA(path.c_str());
}

// A1 + A2 together, through the real code path: with no default logger (after spdlog::shutdown/drop_all)
// OnnxSession::Create still reports the original error. Calling spdlog directly from LOG_ERROR dereferences
// the null logger here, which is how the pre-QA-B-177b code behaved.
TEST(AiLogMacrosSpdlog, ModelNotFoundWithNoDefaultLoggerStillReturnsTheOriginalError) {
    SpdlogCapture cap;
    auto restore = spdlog::default_logger();
    spdlog::set_default_logger(nullptr);
    ASSERT_EQ(nullptr, spdlog::default_logger_raw()) << "control: the default logger really is null now";
    xpe::ai::OnnxSessionConfig cfg;
    cfg.model_path = "C:/xpe_log_probe/does_not_exist/m.onnx";
    auto r = xpe::ai::OnnxSession::Create(cfg);
    spdlog::set_default_logger(restore);
    EXPECT_EQ(xpe::ai::OnnxErrorCode::kInvalidModelPath, r.code);
    EXPECT_EQ("Model file not found: " + cfg.model_path, r.message);
    EXPECT_EQ(nullptr, r.value.get());
    EXPECT_EQ("", cap.Text()) << "nothing may have been written while there was no logger";
}
