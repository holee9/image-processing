// QA-B-209 C16 / 209b / 209c (#251, REQ-DICOM-043): "Each function SHALL log entry/exit at DEBUG level and error conditions at ERROR
// level via the logging subsystem in xpe_common.dll."
//
// The module's lines go through xpe_common's logger (measured in QA-B-209: the [xpe_file] logger obeys xpe_log_set_level and
// xpe_log_set_file), so these tests read the log FILE that xpe_common writes. The file name carries the process id and the test
// name: ctest registers every TEST_F as its own process, and a fixed name made `ctest -j` runs delete each other's log.
//
// The contract pinned here (QA-B-209c, Codex #152 -- the requirement says BOTH, entry/exit at DEBUG AND error conditions at ERROR):
//   * every public function logs   [debug] [xpe_dicom] <fn> entry [details]
//                                  [debug] [xpe_dicom] <fn> exit rc=<N>        (always, whatever the code; void functions: `exit`)
//   * a call that returns anything but XPE_OK ALSO logs, as a separate line,
//                                  [error] [xpe_dicom] <fn> exit rc=<N> (<xpe_error_string>)
//   * a success writes the DEBUG lines only. A failure with a detailed inner line (the reader refusing a file) has the inner ERROR
//     line (the cause) too.
// Every function line is matched as a WHOLE line with the delimiter after the function name (`<fn> entry`, `<fn> exit rc=<N>`), so
// `xpe_dicom_write` can never be satisfied by a line of `xpe_dicom_write_j2k`.
#include <gtest/gtest.h>

#include "xpe/common/xpe_common_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xpe/dicom/dicom_api.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <regex>
#include <sstream>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#define XPE_TEST_GETPID _getpid
#else
#include <unistd.h>
#define XPE_TEST_GETPID getpid
#endif

namespace fs = std::filesystem;

namespace {

class DicomLogging : public ::testing::Test {
protected:
    void SetUp() override {
        const std::string tag = std::to_string(XPE_TEST_GETPID()) + "_" + ::testing::UnitTest::GetInstance()->current_test_info()->name();
        path_ = fs::temp_directory_path() / ("xpe_dicom_logging_" + tag + ".log");
        scratch_ = fs::temp_directory_path() / ("xpe_dicom_logging_" + tag + ".dcm");
        std::error_code ec;
        fs::remove(path_, ec);
        fs::remove(scratch_, ec);
        ASSERT_EQ(XPE_OK, xpe_log_set_file(path_.string().c_str()));
        ASSERT_EQ(XPE_OK, xpe_log_set_level(0));   // TRACE: everything the module writes reaches the file
    }
    void TearDown() override {
        xpe_log_set_level(2);                      // INFO, the library default
        xpe_log_set_file(nullptr);
        std::error_code ec;
        fs::remove(path_, ec);
        fs::remove(scratch_, ec);
    }
    std::string Log() const {
        xpe_log_flush();
        std::ifstream f(path_, std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    }
    // The lines of the log that contain `needle`.
    std::vector<std::string> Lines(const std::string& needle) const {
        std::vector<std::string> out;
        for (const std::string& line : AllLines()) {
            if (line.find(needle) != std::string::npos) out.push_back(line);
        }
        return out;
    }
    std::vector<std::string> AllLines() const {
        std::vector<std::string> out;
        std::istringstream in(Log());
        for (std::string line; std::getline(in, line);) {
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) line.pop_back();
            out.push_back(line);
        }
        return out;
    }
    // Lines matching the regular expression (the WHOLE line, from the level tag to its end).
    size_t Count(const std::string& pattern) const {
        const std::regex re(pattern);
        size_t n = 0;
        for (const std::string& line : AllLines()) {
            if (std::regex_search(line, re)) ++n;
        }
        return n;
    }
    // The exact-format line matchers of the contract above.
    size_t Entries(const std::string& fn) const { return Count("\\[debug\\] \\[xpe_dicom\\] " + fn + " entry( .*)?$"); }
    size_t DebugExits(const std::string& fn, int rc) const {
        return Count("\\[debug\\] \\[xpe_dicom\\] " + fn + " exit rc=" + std::to_string(rc) + "$");
    }
    size_t DebugExitsVoid(const std::string& fn) const { return Count("\\[debug\\] \\[xpe_dicom\\] " + fn + " exit$"); }
    size_t ErrorResults(const std::string& fn, int rc) const {
        return Count("\\[error\\] \\[xpe_dicom\\] " + fn + " exit rc=" + std::to_string(rc) + " \\(.+\\)$");
    }
    size_t ErrorResultsAny(const std::string& fn) const { return Count("\\[error\\] \\[xpe_dicom\\] " + fn + " exit rc="); }
    fs::path path_;
    fs::path scratch_;
};

// A valid 4x4 UINT16 image and zeroed metadata: xpe_dicom_write accepts them (QA-B-209 measured), which gives a SUCCESS path.
struct SmallImage {
    uint16_t pixels[16] = {};
    XpeImageBuffer img{};
    XpeImageMetadata meta{};
    SmallImage() {
        img.width = 4;
        img.height = 4;
        img.format = XPE_PIXEL_UINT16;
        img.bitsAllocated = 16;
        img.bitsStored = 16;
        img.data = pixels;
        img.dataSize = sizeof(pixels);
    }
};

}  // namespace

// All ELEVEN public functions, each in its own exact-format lines, on the early-return (invalid input) path: ONE entry line, ONE
// DEBUG exit line carrying the code, and -- the call failed -- ONE ERROR result line. write and write_j2k are in the same run, so a
// prefix match would let either stand in for the other: the counts below are exactly 1 for each name separately.
TEST_F(DicomLogging, EveryPublicFunctionLogsEntryDebugExitAndAnErrorLineWhenItFails) {
    struct Case {
        const char* fn;
        int rc;
    };
    XpeDicomHandle* h = nullptr;
    char buf[8] = {};
    std::vector<Case> cases;
    cases.push_back({"xpe_dicom_open", xpe_dicom_open(nullptr, nullptr)});
    cases.push_back({"xpe_dicom_read_image", xpe_dicom_read_image(nullptr, nullptr)});
    cases.push_back({"xpe_dicom_get_metadata", xpe_dicom_get_metadata(nullptr, nullptr)});
    cases.push_back({"xpe_dicom_write", xpe_dicom_write(nullptr, nullptr, nullptr)});
    cases.push_back({"xpe_dicom_write_j2k", xpe_dicom_write_j2k(nullptr, nullptr, nullptr)});
    cases.push_back({"xpe_dicom_validate", xpe_dicom_validate(nullptr, buf, sizeof(buf))});
    cases.push_back({"xpe_dicom_cstore", xpe_dicom_cstore(nullptr, 0, nullptr, nullptr, 0)});
    cases.push_back({"xpe_dicom_cfind_mwl", xpe_dicom_cfind_mwl(nullptr, 0, nullptr, nullptr, nullptr, 0, 0)});
    xpe_dicom_close(h);
    xpe_dicom_cancel();
    const char* version = xpe_dicom_version();

    for (const Case& c : cases) {
        EXPECT_EQ(1u, Entries(c.fn)) << c.fn << ": exactly one DEBUG entry line\n" << Log();
        EXPECT_EQ(1u, DebugExits(c.fn, c.rc)) << c.fn << ": exactly one DEBUG exit line with rc=" << c.rc << " (also for a failure)\n" << Log();
        EXPECT_EQ(1u, ErrorResults(c.fn, c.rc)) << c.fn << ": exactly one ERROR result line with rc=" << c.rc << "\n" << Log();
        EXPECT_EQ(1u, ErrorResultsAny(c.fn)) << c.fn;
    }
    // the three functions without a status code: DEBUG entry and DEBUG exit, nothing at ERROR
    for (const char* fn : {"xpe_dicom_close", "xpe_dicom_cancel", "xpe_dicom_version"}) {
        EXPECT_EQ(1u, Entries(fn)) << fn << ": exactly one DEBUG entry line";
        EXPECT_EQ(1u, DebugExitsVoid(fn)) << fn << ": exactly one DEBUG exit line";
        EXPECT_EQ(0u, ErrorResultsAny(fn)) << fn;
    }
    // the function still returns what it returned
    EXPECT_STREQ("1.0.0", version);
}

// The contract on a refusal found before any work (no inner log site): entry, DEBUG exit and ERROR result -- three lines, each once.
TEST_F(DicomLogging, AnEarlyRefusalLogsDebugEntryDebugExitAndOneErrorResult) {
    const int rc = xpe_dicom_open(nullptr, nullptr);
    ASSERT_EQ(XPE_ERR_INVALID_INPUT, rc);
    EXPECT_EQ(1u, Entries("xpe_dicom_open"));
    EXPECT_EQ(1u, DebugExits("xpe_dicom_open", rc));
    EXPECT_EQ(1u, ErrorResults("xpe_dicom_open", rc));
    EXPECT_EQ(0u, Lines("[error] [DicomReader]").size()) << "an early refusal has no inner log site: the result line is the only ERROR";
}

// A failure found inside: the inner ERROR line is the cause, the result line is the outcome of the public call, and the DEBUG exit
// is still there.
TEST_F(DicomLogging, AnInternalFailureLogsTheCauseTheDebugExitAndTheErrorResult) {
    {
        std::ofstream f(scratch_, std::ios::binary);
        f << std::string(500, 'x');
    }
    XpeDicomHandle* h = nullptr;
    const int rc = xpe_dicom_open(scratch_.string().c_str(), &h);
    EXPECT_EQ(XPE_ERR_DICOM_INVALID, rc);
    EXPECT_EQ(1u, Lines("[error] [DicomReader] not a DICOM Part 10 file").size()) << "the cause, from the inside\n" << Log();
    EXPECT_EQ(1u, Entries("xpe_dicom_open"));
    EXPECT_EQ(1u, DebugExits("xpe_dicom_open", rc)) << Log();
    EXPECT_EQ(1u, ErrorResults("xpe_dicom_open", rc)) << "the outcome of the public call";
    EXPECT_TRUE(Lines("[warning] [DicomReader] not a DICOM Part 10 file").empty())
        << "the same condition must not be logged as a warning";
}

// A success: the DEBUG lines of the calls and not one ERROR or warning line.
TEST_F(DicomLogging, ASuccessLogsDebugLinesOnly) {
    SmallImage s;
    ASSERT_EQ(XPE_OK, xpe_dicom_write(scratch_.string().c_str(), &s.img, &s.meta));
    XpeDicomHandle* h = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(scratch_.string().c_str(), &h));
    ASSERT_NE(nullptr, h);
    xpe_dicom_close(h);
    EXPECT_EQ(1u, Entries("xpe_dicom_write"));
    EXPECT_EQ(1u, DebugExits("xpe_dicom_write", 0)) << Log();
    EXPECT_EQ(1u, Entries("xpe_dicom_open"));
    EXPECT_EQ(1u, DebugExits("xpe_dicom_open", 0)) << Log();
    EXPECT_EQ(1u, Entries("xpe_dicom_close"));
    EXPECT_EQ(1u, DebugExitsVoid("xpe_dicom_close"));
    EXPECT_TRUE(Lines("[error]").empty()) << "a run of successful calls writes no ERROR line\n" << Log();
    EXPECT_TRUE(Lines("[warning]").empty());
}

// Exit follows entry, once per call.
TEST_F(DicomLogging, TheDebugExitFollowsTheEntryAndIsWrittenOncePerCall) {
    const int rc = xpe_dicom_open("this_file_does_not_exist.dcm", nullptr);
    const std::vector<std::string> lines = AllLines();
    size_t entryAt = lines.size(), exitAt = lines.size();
    for (size_t i = 0; i < lines.size(); ++i) {
        if (lines[i].find("[debug] [xpe_dicom] xpe_dicom_open entry") != std::string::npos) entryAt = i;
        if (lines[i].find("[debug] [xpe_dicom] xpe_dicom_open exit rc=" + std::to_string(rc)) != std::string::npos) exitAt = i;
    }
    ASSERT_LT(entryAt, lines.size());
    ASSERT_LT(exitAt, lines.size());
    EXPECT_LT(entryAt, exitAt);
    EXPECT_EQ(1u, DebugExits("xpe_dicom_open", rc)) << "one DEBUG exit line per call";
}

// A failure that returns an error code is an ERROR condition (REQ-DICOM-043), not a warning: the SCU failing to load the file it
// was asked to send ...
TEST_F(DicomLogging, AnUnreadableFileToSendIsLoggedAtErrorLevel) {
    EXPECT_EQ(XPE_ERR_IO_FAILED, xpe_dicom_cstore("localhost", 11112, "TESTSCU", "this_file_does_not_exist.dcm", 1000));
    EXPECT_EQ(1u, Lines("[error] [DicomNetworkSCU] cstore: file load failed").size()) << Log();
    EXPECT_TRUE(Lines("[warning] [DicomNetworkSCU]").empty());
}

// ... the writer failing to save (a directory that does not exist).
TEST_F(DicomLogging, AFailedSaveIsLoggedAtErrorLevel) {
    SmallImage s;
    const fs::path nowhere = fs::temp_directory_path() / "xpe_dicom_logging_no_such_dir" / "x.dcm";
    const int rc = xpe_dicom_write(nowhere.string().c_str(), &s.img, &s.meta);
    EXPECT_EQ(XPE_ERR_IO_FAILED, rc);
    EXPECT_EQ(1u, Lines("[error] [DicomWriter] saveFile failed").size()) << Log();
    EXPECT_TRUE(Lines("[warning] [DicomWriter]").empty());
}
