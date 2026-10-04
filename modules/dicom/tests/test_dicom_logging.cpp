// QA-B-209 C16 / 209b (#251, REQ-DICOM-043): "Each function SHALL log entry/exit at DEBUG level and error conditions at ERROR level
// via the logging subsystem in xpe_common.dll."
//
// The module's lines go through xpe_common's logger (measured in QA-B-209: the [xpe_file] logger obeys xpe_log_set_level and
// xpe_log_set_file), so these tests read the log FILE that xpe_common writes. The file name carries the process id and the test
// name: ctest registers every TEST_F as its own process, and a fixed name made `ctest -j` runs delete each other's log.
//
// Policy pinned here (QA-B-209b, Codex #149):
//   * every public function logs its entry and its exit at DEBUG -- ELEVEN functions, xpe_dicom_version included;
//   * the exit line of a call that returns anything but XPE_OK is written at ERROR (so an early INVALID_INPUT, which has no
//     inner log site, is visible at ERROR as well); on success it stays DEBUG;
//   * a failure that also has a detailed inner line (the reader refusing a file) therefore produces TWO ERROR lines: the cause
//     from the inside and the outcome of the public call. They say different things and the exit line is the one place that
//     covers every error return.
#include <gtest/gtest.h>

#include "xpe/common/xpe_common_api.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"
#include "xpe/dicom/dicom_api.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
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
        std::istringstream in(Log());
        for (std::string line; std::getline(in, line);) {
            if (line.find(needle) != std::string::npos) out.push_back(line);
        }
        return out;
    }
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

// Entry and exit of every one of the ELEVEN public functions, on the early-return (invalid input) path. The exit line carries the
// code the function returned; a non-OK code is an ERROR-level line.
TEST_F(DicomLogging, EveryPublicFunctionLogsItsEntryAndExit) {
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

    const std::string log = Log();
    for (const Case& c : cases) {
        EXPECT_NE(std::string::npos, log.find(std::string("[debug] [xpe_dicom] ") + c.fn)) << c.fn << ": no DEBUG entry line";
        const std::string exit = std::string("[error] [xpe_dicom] ") + c.fn + " exit rc=" + std::to_string(c.rc);
        EXPECT_NE(std::string::npos, log.find(exit)) << c.fn << ": no ERROR exit line with rc=" << c.rc << "\n" << log;
        EXPECT_EQ(std::string::npos, log.find(std::string("[debug] [xpe_dicom] ") + c.fn + " exit"))
            << c.fn << ": a failed call must not leave a DEBUG exit line";
    }
    // the three functions without a status code: DEBUG entry and DEBUG exit
    for (const char* fn : {"xpe_dicom_close", "xpe_dicom_cancel", "xpe_dicom_version"}) {
        EXPECT_NE(std::string::npos, log.find(std::string("[debug] [xpe_dicom] ") + fn)) << fn << ": no DEBUG entry line";
        EXPECT_NE(std::string::npos, log.find(std::string("[debug] [xpe_dicom] ") + fn + " exit")) << fn << ": no DEBUG exit line";
    }
    // the function still returns what it returned
    EXPECT_STREQ("1.0.0", version);
}

// The three representative outcomes and the LEVEL each one is written at.
TEST_F(DicomLogging, AnEarlyRefusalIsLoggedAtErrorLevelByTheExitLine) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dicom_open(nullptr, nullptr));
    const auto errors = Lines("[error] [xpe_dicom] xpe_dicom_open exit rc=" + std::to_string(XPE_ERR_INVALID_INPUT));
    EXPECT_EQ(1u, errors.size()) << Log();
    EXPECT_TRUE(Lines("[debug] [xpe_dicom] xpe_dicom_open exit").empty());
    EXPECT_TRUE(Lines("[error] [DicomReader]").empty()) << "an early refusal has no inner log site: the exit line is the only ERROR";
}

TEST_F(DicomLogging, AnInternalFailureIsLoggedAtErrorLevelTwice_TheCauseAndTheOutcome) {
    const fs::path bad = scratch_;
    {
        std::ofstream f(bad, std::ios::binary);
        f << std::string(500, 'x');
    }
    XpeDicomHandle* h = nullptr;
    const int rc = xpe_dicom_open(bad.string().c_str(), &h);
    EXPECT_EQ(XPE_ERR_DICOM_INVALID, rc);
    EXPECT_EQ(1u, Lines("[error] [DicomReader] not a DICOM Part 10 file").size()) << "the cause, from the inside\n" << Log();
    EXPECT_EQ(1u, Lines("[error] [xpe_dicom] xpe_dicom_open exit rc=" + std::to_string(rc)).size()) << "the outcome of the public call";
    EXPECT_TRUE(Lines("[warning] [DicomReader] not a DICOM Part 10 file").empty())
        << "the same condition must not be logged as a warning";
}

TEST_F(DicomLogging, ASuccessIsLoggedAtDebugLevelOnly) {
    SmallImage s;
    ASSERT_EQ(XPE_OK, xpe_dicom_write(scratch_.string().c_str(), &s.img, &s.meta));
    XpeDicomHandle* h = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(scratch_.string().c_str(), &h));
    ASSERT_NE(nullptr, h);
    xpe_dicom_close(h);
    EXPECT_EQ(1u, Lines("[debug] [xpe_dicom] xpe_dicom_write exit rc=0").size()) << Log();
    EXPECT_EQ(1u, Lines("[debug] [xpe_dicom] xpe_dicom_open exit rc=0").size()) << Log();
    EXPECT_EQ(1u, Lines("[debug] [xpe_dicom] xpe_dicom_close exit").size());
    EXPECT_TRUE(Lines("[error]").empty()) << "a run of successful calls writes no ERROR line\n" << Log();
    EXPECT_TRUE(Lines("[warning]").empty());
}

// The exit line is written once per call and after the entry line (not before it).
TEST_F(DicomLogging, TheExitLineFollowsTheEntryLineAndIsWrittenOncePerCall) {
    const int rc = xpe_dicom_open("this_file_does_not_exist.dcm", nullptr);
    const std::string log = Log();
    const size_t entry = log.find("xpe_dicom_open(");
    const std::string exit = "xpe_dicom_open exit rc=" + std::to_string(rc);
    const size_t first = log.find(exit);
    ASSERT_NE(std::string::npos, entry);
    ASSERT_NE(std::string::npos, first);
    EXPECT_LT(entry, first);
    EXPECT_EQ(std::string::npos, log.find(exit, first + 1)) << "one exit line per call";
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
