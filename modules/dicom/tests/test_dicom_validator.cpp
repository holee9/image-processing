/**
 * @file test_dicom_validator.cpp
 * @brief TDD tests for DicomValidator / SWU-4.3 (>= 6 test cases)
 * SPEC: SPEC-XPE-P1B-DICOM REQ-DICOM-023..028, AC-05, AC-10, AC-11
 */
#include <gtest/gtest.h>
#include "xpe/dicom/dicom_api.h"
#include "xpe/common/xpe_memory.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <cstring>

namespace fs = std::filesystem;
using json = nlohmann::json;

class DicomValidatorTest : public ::testing::Test {
protected:
    static void SetUpTestSuite();
    static void TearDownTestSuite();

    static fs::path s_conformantDcm;    // Written by xpe_dicom_write (known conformant)
    static fs::path s_missingTagDcm;    // Patient ID removed
    static fs::path s_notDicom;
    static fs::path s_tempDir;
};

fs::path DicomValidatorTest::s_conformantDcm;
fs::path DicomValidatorTest::s_missingTagDcm;
fs::path DicomValidatorTest::s_notDicom;
fs::path DicomValidatorTest::s_tempDir;

void DicomValidatorTest::SetUpTestSuite() {
    s_tempDir = fs::temp_directory_path() / "xpe_dicom_validator_test";
    fs::create_directories(s_tempDir);

    XpeImageBuffer img{};
    xpe_alloc_image(128, 128, XPE_PIXEL_UINT16, &img);
    XpeImageMetadata meta{};
    std::strncpy(meta.bodyPart, "HAND", sizeof(meta.bodyPart));

    s_conformantDcm = s_tempDir / "conformant.dcm";
    xpe_dicom_write(s_conformantDcm.string().c_str(), &img, &meta);

    // TODO: create a DICOM with Patient ID removed for AC-05 negative test
    s_missingTagDcm = s_tempDir / "missing_patient_id.dcm";

    s_notDicom = s_tempDir / "not_dicom.dcm";
    std::ofstream f(s_notDicom, std::ios::binary);
    f.write("NOT A DICOM FILE CONTENT", 24);

    xpe_free_image(&img);
}

void DicomValidatorTest::TearDownTestSuite() {
    fs::remove_all(s_tempDir);
}

// ---------------------------------------------------------------------------
// AC-05: Conformant file validates as valid
// ---------------------------------------------------------------------------
TEST_F(DicomValidatorTest, ValidateConformant_ReturnsValid) {
    char report[8192] = {};
    EXPECT_EQ(XPE_OK, xpe_dicom_validate(
        s_conformantDcm.string().c_str(), report, sizeof(report)));
    auto j = json::parse(report);
    EXPECT_TRUE(j["valid"].get<bool>());
    EXPECT_TRUE(j["errors"].empty());
}

// ---------------------------------------------------------------------------
// AC-05: Missing required tag produces error in report
// ---------------------------------------------------------------------------
TEST_F(DicomValidatorTest, ValidateMissingPatientID_ReportsError) {
    if (!fs::exists(s_missingTagDcm)) GTEST_SKIP() << "Missing tag test file not created yet";
    char report[8192] = {};
    EXPECT_EQ(XPE_OK, xpe_dicom_validate(
        s_missingTagDcm.string().c_str(), report, sizeof(report)));
    auto j = json::parse(report);
    EXPECT_FALSE(j["valid"].get<bool>());
    bool foundTag = false;
    for (const auto& e : j["errors"])
        if (e["tag"] == "0010,0020") { foundTag = true; break; }
    EXPECT_TRUE(foundTag) << "Expected error for tag 0010,0020 (Patient ID)";
}

// ---------------------------------------------------------------------------
// AC-10 / REQ-DICOM-026: Non-DICOM file returns DICOM_INVALID
// ---------------------------------------------------------------------------
TEST_F(DicomValidatorTest, ValidateNotDicom_ReturnsDicomInvalid) {
    char report[8192] = {};
    EXPECT_EQ(XPE_ERR_DICOM_INVALID, xpe_dicom_validate(
        s_notDicom.string().c_str(), report, sizeof(report)));
    // Must write error report even on INVALID
    auto j = json::parse(report);
    EXPECT_FALSE(j["valid"].get<bool>());
    EXPECT_FALSE(j["errors"].empty());
}

// ---------------------------------------------------------------------------
// AC-11: Buffer too small returns BUFFER_TOO_SMALL
// ---------------------------------------------------------------------------
TEST_F(DicomValidatorTest, BufferTooSmall_ReturnsBufferTooSmall) {
    char tiny[10] = {};
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL, xpe_dicom_validate(
        s_conformantDcm.string().c_str(), tiny, sizeof(tiny)));
    // Required size written to first 4 bytes
    uint32_t required = 0;
    std::memcpy(&required, tiny, sizeof(uint32_t));
    EXPECT_GT(required, 10u);
}

// ---------------------------------------------------------------------------
// AC-09: NULL parameters
// ---------------------------------------------------------------------------
TEST_F(DicomValidatorTest, NullFilePath_ReturnsInvalidInput) {
    char buf[256] = {};
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dicom_validate(nullptr, buf, sizeof(buf)));
}

TEST_F(DicomValidatorTest, NullOutBuf_ReturnsInvalidInput) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dicom_validate(
        s_conformantDcm.string().c_str(), nullptr, 0));
}

// ---------------------------------------------------------------------------
// REQ-DICOM-024: UID format validation
// ---------------------------------------------------------------------------
TEST_F(DicomValidatorTest, ValidateBadUID_ReportsWarning) {
    // TODO: create a DICOM with a malformed UID (too long or non-numeric)
    GTEST_SKIP() << "Bad UID test file not yet created";
}
