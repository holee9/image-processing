/**
 * @file test_ai_log_macros.cpp
 * @brief The AI_LOG_* macros of ai_log.h, both branches, compiled here (QA-B-177).
 *
 * The first half is the PRINTF branch (no XPE_AI_USE_SPDLOG): output goes to stdout, captured through a
 * file. The second half is the SPDLOG branch (this translation unit defines XPE_AI_USE_SPDLOG for its own
 * copy of the header): output goes to a spdlog logger with an ostream sink. They cannot share a TU, so the
 * spdlog half lives in test_ai_log_macros_spdlog.cpp.
 */
#ifdef XPE_AI_USE_SPDLOG
#undef XPE_AI_USE_SPDLOG   // this file tests the printf branch
#endif
#include "ai_log.h"

#include <gtest/gtest.h>

#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include <windows.h>

namespace {

/** Redirects the C stdout (file descriptor 1) to a temporary file for its lifetime. */
class StdoutCapture {
public:
    StdoutCapture() {
        char tmp[MAX_PATH] = {0};
        GetTempPathA(sizeof(tmp), tmp);
        path_ = std::string(tmp) + "xpe_ai_log_stdout_" + std::to_string(GetCurrentProcessId()) + ".txt";
        std::fflush(stdout);
        saved_ = _dup(1);
        int fd = -1;
        _sopen_s(&fd, path_.c_str(), _O_CREAT | _O_TRUNC | _O_WRONLY | _O_BINARY, _SH_DENYNO, _S_IREAD | _S_IWRITE);
        _dup2(fd, 1);
        _close(fd);
    }
    /** Stops capturing and returns what was written. */
    std::string Finish() {
        std::fflush(stdout);
        if (saved_ >= 0) {
            _dup2(saved_, 1);
            _close(saved_);
            saved_ = -1;
        }
        std::ifstream in(path_, std::ios::binary);
        std::stringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }
    ~StdoutCapture() {
        Finish();
        DeleteFileA(path_.c_str());
    }

private:
    std::string path_;
    int saved_ = -1;
};

}  // namespace

TEST(AiLogMacrosPrintf, FormatsItsArgumentsAndPrefixesTheLevel) {
    StdoutCapture cap;
    AI_LOG_INFO("a=%d s=%s z=%zu", 5, "xyz", static_cast<size_t>(7));
    AI_LOG_WARN("plain");
    AI_LOG_ERROR("code %d", -9);
    EXPECT_EQ("[AI INFO] a=5 s=xyz z=7\n[AI WARN] plain\n[AI ERROR] code -9\n", cap.Finish());
}

TEST(AiLogMacrosPrintf, TraceAndDebugEmitNothing) {
    StdoutCapture cap;
    AI_LOG_TRACE("t=%d", 1);
    AI_LOG_DEBUG("d=%d", 2);
    EXPECT_EQ("", cap.Finish());
}

// A call is ONE statement, so it is safe as the body of an unbraced if/else. With the old two-statement
// macro this did not compile at all (the else had no if).
TEST(AiLogMacrosPrintf, ACallIsOneStatementAfterAnUnbracedIf) {
    StdoutCapture cap;
    const bool yes = true;
    if (yes)
        AI_LOG_INFO("then");
    else
        AI_LOG_INFO("else");
    EXPECT_EQ("[AI INFO] then\n", cap.Finish());
}
