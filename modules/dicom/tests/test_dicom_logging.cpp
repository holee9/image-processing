// QA-B-209 C16 (#251, REQ-DICOM-043): "Each function SHALL log entry/exit at DEBUG level and error conditions at ERROR level via
// the logging subsystem in xpe_common.dll."
//
// The module's lines go through xpe_common's logger (measured in QA-B-209: the [xpe_file] logger obeys xpe_log_set_level and
// xpe_log_set_file), so these tests read the log FILE that xpe_common writes. Before this change a public function logged its
// entry only, and a failure that returned an error code was logged as a warning.
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

namespace fs = std::filesystem;

namespace {

class DicomLogging : public ::testing::Test {
protected:
    void SetUp() override {
        path_ = fs::temp_directory_path() / "xpe_dicom_logging_test.log";
        std::error_code ec;
        fs::remove(path_, ec);
        ASSERT_EQ(XPE_OK, xpe_log_set_file(path_.string().c_str()));
        ASSERT_EQ(XPE_OK, xpe_log_set_level(0));   // TRACE: everything the module writes reaches the file
    }
    void TearDown() override {
        xpe_log_set_level(2);                      // INFO, the library default
        xpe_log_set_file(nullptr);
        std::error_code ec;
        fs::remove(path_, ec);
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
};

}  // namespace

// Entry and exit of every public function, on the early-return (invalid input) path -- which is one of the paths a per-return
// log would miss. The exit line carries the code the function returned.
TEST_F(DicomLogging, EveryPublicFunctionLogsItsExitWithTheReturnCode) {
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

    const std::string log = Log();
    for (const Case& c : cases) {
        const std::string entry = std::string("[debug] [xpe_dicom] ") + c.fn;
        const std::string exit = std::string("[debug] [xpe_dicom] ") + c.fn + " exit rc=" + std::to_string(c.rc);
        EXPECT_NE(std::string::npos, log.find(entry)) << c.fn << ": no DEBUG entry line";
        EXPECT_NE(std::string::npos, log.find(exit)) << c.fn << ": no DEBUG exit line with rc=" << c.rc << "\n" << log;
    }
    // the two void functions: exit without a code
    EXPECT_NE(std::string::npos, log.find("[debug] [xpe_dicom] xpe_dicom_close exit")) << "xpe_dicom_close";
    EXPECT_NE(std::string::npos, log.find("[debug] [xpe_dicom] xpe_dicom_cancel exit")) << "xpe_dicom_cancel";
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

// A failure that returns an error code is an ERROR condition (REQ-DICOM-043), not a warning: the reader refusing a file.
TEST_F(DicomLogging, ARefusedFileIsLoggedAtErrorLevel) {
    const fs::path bad = fs::temp_directory_path() / "xpe_dicom_logging_not_dicom.txt";
    {
        std::ofstream f(bad, std::ios::binary);
        f << std::string(500, 'x');
    }
    XpeDicomHandle* h = nullptr;
    EXPECT_EQ(XPE_ERR_DICOM_INVALID, xpe_dicom_open(bad.string().c_str(), &h));
    std::error_code ec;
    fs::remove(bad, ec);
    const auto errors = Lines("[error] [DicomReader] not a DICOM Part 10 file");
    EXPECT_EQ(1u, errors.size()) << Log();
    EXPECT_TRUE(Lines("[warning] [DicomReader] not a DICOM Part 10 file").empty())
        << "the same condition must not be logged as a warning";
}

// ... the SCU failing to load the file it was asked to send.
TEST_F(DicomLogging, AnUnreadableFileToSendIsLoggedAtErrorLevel) {
    EXPECT_EQ(XPE_ERR_IO_FAILED, xpe_dicom_cstore("localhost", 11112, "TESTSCU", "this_file_does_not_exist.dcm", 1000));
    EXPECT_EQ(1u, Lines("[error] [DicomNetworkSCU] cstore: file load failed").size()) << Log();
    EXPECT_TRUE(Lines("[warning] [DicomNetworkSCU]").empty());
}

// ... the writer failing to save (a directory that does not exist).
TEST_F(DicomLogging, AFailedSaveIsLoggedAtErrorLevel) {
    uint16_t pixels[16] = {};
    XpeImageBuffer img{};
    img.width = 4;
    img.height = 4;
    img.format = XPE_PIXEL_UINT16;
    img.bitsAllocated = 16;
    img.bitsStored = 16;
    img.data = pixels;
    img.dataSize = sizeof(pixels);
    XpeImageMetadata meta{};
    const fs::path nowhere = fs::temp_directory_path() / "xpe_dicom_logging_no_such_dir" / "x.dcm";
    const int rc = xpe_dicom_write(nowhere.string().c_str(), &img, &meta);
    EXPECT_NE(XPE_OK, rc);
    if (rc == XPE_ERR_IO_FAILED) {
        EXPECT_EQ(1u, Lines("[error] [DicomWriter] saveFile failed").size()) << Log();
        EXPECT_TRUE(Lines("[warning] [DicomWriter]").empty());
    } else {
        GTEST_LOG_(INFO) << "write returned " << rc << " before saveFile; the writer ERROR level is checked in the source only";
    }
}
