/**
 * @file test_common_logging_contract.cpp
 * @brief The logging API does what its header and the requirements say (QA-A-232 M1, #253)
 *
 * QA-A-231 found three places where the code read differently from the header or the requirement:
 *   (a) xpe_log_set_file(NULL)  -- header and SRS-FUNC-041: "revert to stderr"; the code installed a null sink.
 *   (b) xpe_log_set_level(5)    -- header and SRS-FUNC-040: 5 = OFF; the code mapped 5 to critical.
 *   (c) the default level       -- REQ-P0-011: "default logging to stderr at INFO"; the code started at TRACE
 *                                  and installed no stderr logger at xpe_init.
 * Every case here reads what actually reached the process's stderr (file descriptor 2 is redirected to a
 * temporary file around the calls), so a logger that is set up but writes nowhere cannot pass. The library
 * sources are compiled into this executable (as in xpe_common_oom_tests) because the spdlog default logger
 * the test writes through must be the same one the library configures; a copy in another module is not.
 */

#include <gtest/gtest.h>

#include "xpe/common/xpe_common_api.h"

#include <spdlog/spdlog.h>

#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>

#ifdef _WIN32
#include <io.h>
#define XPE_TEST_DUP _dup
#define XPE_TEST_DUP2 _dup2
#define XPE_TEST_CLOSE _close
#define XPE_TEST_FILENO _fileno
#else
#include <unistd.h>
#define XPE_TEST_DUP dup
#define XPE_TEST_DUP2 dup2
#define XPE_TEST_CLOSE close
#define XPE_TEST_FILENO fileno
#endif

namespace {

constexpr const char* kCapture = "logging_contract_stderr_capture.txt";
constexpr const char* kLogFile = "logging_contract_file.txt";

std::string readAll(const char* path) {
    std::ifstream in(path, std::ios::binary);
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

bool has(const std::string& haystack, const char* needle) {
    return haystack.find(needle) != std::string::npos;
}

std::FILE* openForWrite(const char* path) {
#ifdef _WIN32
    std::FILE* f = nullptr;
    return fopen_s(&f, path, "wb") == 0 ? f : nullptr;
#else
    return std::fopen(path, "wb");
#endif
}

// Sends everything written to file descriptor 2 to kCapture until stop().
class StderrCapture {
public:
    StderrCapture() {
        std::fflush(stderr);
        saved_ = XPE_TEST_DUP(XPE_TEST_FILENO(stderr));
        std::FILE* f = openForWrite(kCapture);
        if (f != nullptr) {
            XPE_TEST_DUP2(XPE_TEST_FILENO(f), XPE_TEST_FILENO(stderr));
            std::fclose(f);
        }
    }
    ~StderrCapture() { restore(); }
    std::string stop() {
        xpe_log_flush();
        restore();
        const std::string got = readAll(kCapture);
        std::remove(kCapture);
        return got;
    }

private:
    void restore() {
        if (saved_ < 0) return;
        std::fflush(stderr);
        XPE_TEST_DUP2(saved_, XPE_TEST_FILENO(stderr));
        XPE_TEST_CLOSE(saved_);
        saved_ = -1;
    }
    int saved_ = -1;
};

// One line per severity, each with its own marker, written through the spdlog default logger -- the logger
// every module in the product writes through.
void emitAll() {
    spdlog::trace("MARK_TRACE");
    spdlog::debug("MARK_DEBUG");
    spdlog::info("MARK_INFO");
    spdlog::warn("MARK_WARN");
    spdlog::error("MARK_ERROR");
    spdlog::critical("MARK_CRITICAL");
}

class LoggingContract : public ::testing::Test {
protected:
    void SetUp() override {
        std::remove(kCapture);
        std::remove(kLogFile);
        ASSERT_EQ(XPE_OK, xpe_init(nullptr));
    }
    void TearDown() override {
        xpe_shutdown();
        std::remove(kCapture);
        std::remove(kLogFile);
    }
};

}  // namespace

// Control: the capture sees a line that is written to stderr. Without it, every "absent" assertion below
// would pass on a capture that reads nothing.
TEST_F(LoggingContract, CaptureSeesALineWrittenToStderr) {
    StderrCapture cap;
    std::fputs("MARK_CONTROL\n", stderr);
    EXPECT_TRUE(has(cap.stop(), "MARK_CONTROL"));
}

// (c) REQ-P0-011 -- after xpe_init, logging goes to stderr at INFO.
TEST_F(LoggingContract, AfterInitInfoReachesStderrButTraceAndDebugDoNot) {
    StderrCapture cap;
    emitAll();
    const std::string got = cap.stop();
    EXPECT_TRUE(has(got, "MARK_INFO")) << got;
    EXPECT_TRUE(has(got, "MARK_WARN")) << got;
    EXPECT_TRUE(has(got, "MARK_ERROR")) << got;
    EXPECT_TRUE(has(got, "MARK_CRITICAL")) << got;
    EXPECT_FALSE(has(got, "MARK_TRACE")) << got;
    EXPECT_FALSE(has(got, "MARK_DEBUG")) << got;
}

// (c) The default also holds for a file the caller names without choosing a level.
TEST_F(LoggingContract, ANamedFileWithoutALevelRecordsFromInfoUp) {
    ASSERT_EQ(XPE_OK, xpe_log_set_file(kLogFile));
    emitAll();
    xpe_log_flush();
    const std::string got = readAll(kLogFile);
    EXPECT_TRUE(has(got, "MARK_INFO")) << got;
    EXPECT_FALSE(has(got, "MARK_TRACE")) << got;
    EXPECT_FALSE(has(got, "MARK_DEBUG")) << got;
}

// (c) A level chosen by the caller still moves the line either way.
TEST_F(LoggingContract, ALevelChosenByTheCallerOpensTraceAgain) {
    ASSERT_EQ(XPE_OK, xpe_log_set_level(0));
    StderrCapture cap;
    emitAll();
    const std::string got = cap.stop();
    EXPECT_TRUE(has(got, "MARK_TRACE")) << got;
    EXPECT_TRUE(has(got, "MARK_DEBUG")) << got;
}

// (a) SRS-FUNC-041 / header -- NULL reverts the output to stderr.
TEST_F(LoggingContract, NullRevertsFromAFileToStderr) {
    ASSERT_EQ(XPE_OK, xpe_log_set_file(kLogFile));
    ASSERT_EQ(XPE_OK, xpe_log_set_file(nullptr));
    StderrCapture cap;
    spdlog::info("MARK_AFTER_NULL");
    const std::string onStderr = cap.stop();
    EXPECT_TRUE(has(onStderr, "MARK_AFTER_NULL")) << onStderr;
    EXPECT_FALSE(has(readAll(kLogFile), "MARK_AFTER_NULL"));
}

// (a) ... and keeps the level the caller chose.
TEST_F(LoggingContract, NullKeepsTheChosenLevel) {
    ASSERT_EQ(XPE_OK, xpe_log_set_level(3));
    ASSERT_EQ(XPE_OK, xpe_log_set_file(nullptr));
    StderrCapture cap;
    emitAll();
    const std::string got = cap.stop();
    EXPECT_FALSE(has(got, "MARK_INFO")) << got;
    EXPECT_TRUE(has(got, "MARK_WARN")) << got;
}

// (b) SRS-FUNC-040 / header -- level 5 is OFF: nothing is recorded, not even a critical line.
TEST_F(LoggingContract, LevelFiveIsOffOnStderr) {
    ASSERT_EQ(XPE_OK, xpe_log_set_level(5));
    StderrCapture cap;
    emitAll();
    const std::string got = cap.stop();
    EXPECT_FALSE(has(got, "MARK_CRITICAL")) << got;
    EXPECT_FALSE(has(got, "MARK_ERROR")) << got;
}

TEST_F(LoggingContract, LevelFiveIsOffInAFile) {
    ASSERT_EQ(XPE_OK, xpe_log_set_level(5));
    ASSERT_EQ(XPE_OK, xpe_log_set_file(kLogFile));
    emitAll();
    xpe_log_flush();
    EXPECT_FALSE(has(readAll(kLogFile), "MARK_CRITICAL"));
}

// (b) Control for the two cases above: the level just below OFF still records critical.
TEST_F(LoggingContract, LevelFourStillRecordsErrorAndCritical) {
    ASSERT_EQ(XPE_OK, xpe_log_set_level(4));
    StderrCapture cap;
    emitAll();
    const std::string got = cap.stop();
    EXPECT_TRUE(has(got, "MARK_ERROR")) << got;
    EXPECT_TRUE(has(got, "MARK_CRITICAL")) << got;
    EXPECT_FALSE(has(got, "MARK_WARN")) << got;
}

// (c) The level a caller gets without choosing one is INFO even before xpe_init has run: a file named first
// records from INFO up. Not a fixture case on purpose -- no init has happened. (ctest runs each case in its own
// process, so this is the process's first logging call; after a shutdown the level is back at INFO too.)
TEST(LoggingContractFresh, ALevelNeverChosenIsInfoEvenBeforeInit) {
    std::remove(kLogFile);
    ASSERT_EQ(XPE_OK, xpe_log_set_file(kLogFile));
    emitAll();
    xpe_log_flush();
    const std::string got = readAll(kLogFile);
    xpe_shutdown();
    std::remove(kLogFile);
    EXPECT_TRUE(has(got, "MARK_INFO")) << got;
    EXPECT_FALSE(has(got, "MARK_TRACE")) << got;
    EXPECT_FALSE(has(got, "MARK_DEBUG")) << got;
}

// QA-A-232 M3: the library's own lines (xpe_init writes one) follow the same level and destination as every
// other line. The old internal_log kept a level and a file of its own, so with the level set to OFF the line
// still reached stderr, and a log file chosen with xpe_log_set_file did not get it.
TEST(LoggingContractInit, OffMeansNoBytesEvenFromTheLibrarysOwnInitLine) {
    xpe_shutdown();
    ASSERT_EQ(XPE_OK, xpe_log_set_level(5));
    StderrCapture cap;
    ASSERT_EQ(XPE_OK, xpe_init(nullptr));
    const std::string got = cap.stop();
    xpe_shutdown();
    EXPECT_TRUE(got.empty()) << got;
}

TEST(LoggingContractInit, TheInitLineIsWrittenAtInfoToStderr) {
    xpe_shutdown();
    StderrCapture cap;
    ASSERT_EQ(XPE_OK, xpe_init(nullptr));
    const std::string got = cap.stop();
    xpe_shutdown();
    EXPECT_TRUE(has(got, "library initialised")) << got;
}

TEST(LoggingContractInit, TheInitLineGoesToTheLogFileTheCallerChose) {
    std::remove(kLogFile);
    xpe_shutdown();
    ASSERT_EQ(XPE_OK, xpe_log_set_file(kLogFile));
    StderrCapture cap;
    ASSERT_EQ(XPE_OK, xpe_init(nullptr));
    const std::string onStderr = cap.stop();
    xpe_log_flush();
    const std::string inFile = readAll(kLogFile);
    xpe_shutdown();
    std::remove(kLogFile);
    EXPECT_TRUE(has(inFile, "library initialised")) << inFile;
    EXPECT_FALSE(has(onStderr, "library initialised")) << onStderr;
}

TEST(LoggingContractInit, ALevelAboveInfoSilencesTheInitLine) {
    xpe_shutdown();
    ASSERT_EQ(XPE_OK, xpe_log_set_level(3));
    StderrCapture cap;
    ASSERT_EQ(XPE_OK, xpe_init(nullptr));
    const std::string got = cap.stop();
    xpe_shutdown();
    EXPECT_FALSE(has(got, "library initialised")) << got;
}

// A new init after a shutdown brings the default back (shutdown parks logging on a null sink).
TEST_F(LoggingContract, ASecondInitRestoresTheStderrDefault) {
    xpe_shutdown();
    ASSERT_EQ(XPE_OK, xpe_init(nullptr));
    StderrCapture cap;
    emitAll();
    const std::string got = cap.stop();
    EXPECT_TRUE(has(got, "MARK_INFO")) << got;
    EXPECT_FALSE(has(got, "MARK_DEBUG")) << got;
}
