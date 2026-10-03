/**
 * @file test_dicom_validator.cpp
 * @brief TDD tests for DicomValidator / SWU-4.3 (>= 6 test cases)
 * SPEC: SPEC-XPE-P1B-DICOM REQ-DICOM-023..028, AC-05, AC-10, AC-11
 */
#include <gtest/gtest.h>
#include "xpe/dicom/dicom_api.h"

// #124 (QA-B-25): the negative-path fixtures below are derived from the
// conformant file with DCMTK, so the test links DCMTK directly. Before this,
// s_missingTagDcm was only ever assigned a path -- the file was never written,
// so the guard skipped everywhere, CI and local alike.
#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include "xpe/common/xpe_memory.h"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <cstdio>
#include <cstring>
#include <functional>

namespace fs = std::filesystem;
using json = nlohmann::json;

class DicomValidatorTest : public ::testing::Test {
protected:
    static void SetUpTestSuite();
    static void TearDownTestSuite();

    static fs::path s_conformantDcm;    // Written by xpe_dicom_write (known conformant)
    static fs::path s_missingTagDcm;    // Patient ID removed
    static fs::path s_badUidDcm;        // malformed SOPInstanceUID
    static fs::path s_notDicom;
    static fs::path s_tempDir;
};

fs::path DicomValidatorTest::s_conformantDcm;
fs::path DicomValidatorTest::s_missingTagDcm;
fs::path DicomValidatorTest::s_badUidDcm;
fs::path DicomValidatorTest::s_notDicom;
fs::path DicomValidatorTest::s_tempDir;

void DicomValidatorTest::SetUpTestSuite() {
    s_tempDir = fs::temp_directory_path() / "xpe_dicom_validator_test";
    fs::create_directories(s_tempDir);

    XpeImageBuffer img{};
    xpe_alloc_image(128, 128, XPE_PIXEL_UINT16, &img);
    XpeImageMetadata meta{};
    std::snprintf(meta.bodyPart, sizeof(meta.bodyPart), "%s", "HAND");

    s_conformantDcm = s_tempDir / "conformant.dcm";
    xpe_dicom_write(s_conformantDcm.string().c_str(), &img, &meta);

    // AC-05 negative fixture: the conformant file with PatientID (0010,0020)
    // removed. Derived rather than hand-authored so it differs from the
    // conformant file in exactly the one tag under test.
    s_missingTagDcm = s_tempDir / "missing_patient_id.dcm";
    {
        DcmFileFormat ff;
        if (ff.loadFile(s_conformantDcm.string().c_str()).good()) {
            ff.getDataset()->findAndDeleteElement(DCM_PatientID);
            ff.saveFile(s_missingTagDcm.string().c_str(), EXS_LittleEndianExplicit);
        }
    }

    // REQ-DICOM-024 fixture: SOPInstanceUID set to a value that is not a
    // dot-separated numeric string, which is what isValidUID() rejects.
    s_badUidDcm = s_tempDir / "bad_uid.dcm";
    {
        DcmFileFormat ff;
        if (ff.loadFile(s_conformantDcm.string().c_str()).good()) {
            ff.getDataset()->putAndInsertString(DCM_SOPInstanceUID, "not.a.valid.uid");
            ff.saveFile(s_badUidDcm.string().c_str(), EXS_LittleEndianExplicit);
        }
    }

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
    ASSERT_TRUE(fs::exists(s_missingTagDcm)) << "missing-PatientID fixture was not written";
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
// QA-B-206 C6 renamed this from ValidateBadUID_ReportsWarning: it asserted that a bad UID is a WARNING, but the same
// problem was also pushed to errors, so one defect read as both a failure and a non-critical issue (REQ-DICOM-025).
TEST_F(DicomValidatorTest, ValidateBadUID_IsAnErrorAndNotAlsoAWarning) {
    ASSERT_TRUE(fs::exists(s_badUidDcm)) << "bad-UID fixture was not written";
    char report[8192] = {};
    ASSERT_EQ(XPE_OK, xpe_dicom_validate(
        s_badUidDcm.string().c_str(), report, sizeof(report)));
    auto j = json::parse(report);
    EXPECT_FALSE(j["valid"].get<bool>()) << report;
    ASSERT_EQ(1u, j["errors"].size()) << "expected exactly the Invalid UID format error, report: " << report;
    EXPECT_EQ("0008,0018", j["errors"][0]["tag"].get<std::string>()) << report;
    EXPECT_NE(std::string::npos, j["errors"][0]["message"].get<std::string>().find("Invalid UID format")) << report;
    EXPECT_TRUE(j["warnings"].empty()) << "was pushed to warnings too, report: " << report;
}

// ---------------------------------------------------------------------------
// #120 (QA-B-27): report-buffer sizing on the DICOM_INVALID early-return path.
//
// DicomValidator::validate builds the report BEFORE returning DICOM_INVALID and
// does its own buffer-size check there (DicomValidator.cpp:66-74). That branch
// was never executed: the existing invalid-file test always passes an 8 KB
// buffer, so only the success-path sizing check ran.
// ---------------------------------------------------------------------------
TEST_F(DicomValidatorTest, ValidateNotDicom_SmallBuffer_ReturnsBufferTooSmall) {
    char tiny[8] = {};
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL, xpe_dicom_validate(
        s_notDicom.string().c_str(), tiny, sizeof(tiny)));
}

TEST_F(DicomValidatorTest, ValidateConformant_SmallBuffer_ReturnsBufferTooSmall) {
    char tiny[8] = {};
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL, xpe_dicom_validate(
        s_conformantDcm.string().c_str(), tiny, sizeof(tiny)));
}

// A file that parses but carries none of the four required Type 1 tags
// exercises the missing-tag loop for every tag at once, plus the
// warnings-empty branch of buildReport (DicomValidator.cpp:232-238).
TEST_F(DicomValidatorTest, ValidateStrippedTags_ReportsAllMissing) {
    auto stripped = s_tempDir / "stripped.dcm";
    {
        DcmFileFormat ff;
        ASSERT_TRUE(ff.loadFile(s_conformantDcm.string().c_str()).good());
        DcmDataset* ds = ff.getDataset();
        ds->findAndDeleteElement(DCM_PatientID);
        ds->findAndDeleteElement(DCM_StudyInstanceUID);
        ds->findAndDeleteElement(DCM_SeriesInstanceUID);
        ds->findAndDeleteElement(DCM_SOPInstanceUID);
        ASSERT_TRUE(ff.saveFile(stripped.string().c_str(), EXS_LittleEndianExplicit).good());
    }
    char report[8192] = {};
    ASSERT_EQ(XPE_OK, xpe_dicom_validate(stripped.string().c_str(), report, sizeof(report)));
    auto j = json::parse(report);
    EXPECT_FALSE(j["valid"].get<bool>());
    EXPECT_GE(j["errors"].size(), 4u) << report;
}

// ---------------------------------------------------------------------------
// #139 (QA-B-36): the Part 10 meta group (0002) is now part of the contract.
//
// QA-B-34 observed that a dataset-only file validates as valid=true, and
// deliberately did NOT assert otherwise -- no api-spec text made a missing
// meta header non-conformant, so asserting it would have turned one reader's
// opinion into a norm. #139 closed that gap: leader decided (a) Part 10 meta
// is required and its content must agree with the dataset. The case withdrawn
// in B-34 is restored here as a real assertion, now that a decision backs it.
// ---------------------------------------------------------------------------

TEST_F(DicomValidatorTest, ValidateDatasetWithoutMetaHeader_ReportsMissingMeta) {
    auto path = s_tempDir / "no_meta_header.dcm";
    {
        DcmFileFormat ff;
        ASSERT_TRUE(ff.loadFile(s_conformantDcm.string().c_str()).good());
        // EWM_dataset writes the dataset alone -- no preamble, no meta group.
        ASSERT_TRUE(ff.saveFile(path.string().c_str(), EXS_LittleEndianExplicit,
                                EET_ExplicitLength, EGL_recalcGL, EPD_withoutPadding,
                                0, 0, EWM_dataset).good());
    }

    char report[8192] = {};
    ASSERT_EQ(XPE_OK, xpe_dicom_validate(path.string().c_str(), report, sizeof(report)));
    auto j = json::parse(report);
    EXPECT_FALSE(j["valid"].get<bool>()) << report;

    // The reason must name the meta header, not merely fail: a caller that has
    // to guess why is no better off than one told nothing.
    bool mentionsMeta = false;
    for (const auto& e : j["errors"]) {
        const std::string msg = e.value("message", std::string());
        if (msg.find("meta") != std::string::npos ||
            msg.find("Meta") != std::string::npos) {
            mentionsMeta = true;
        }
    }
    EXPECT_TRUE(mentionsMeta) << "no error names the meta header: " << report;
}

// The meta group exists but its MediaStorageSOPClassUID disagrees with the
// dataset's SOPClassUID. PS3.10 requires the two to match; a file where they
// differ describes itself as something it is not.
TEST_F(DicomValidatorTest, ValidateMetaSopClassMismatch_ReportsInconsistency) {
    auto path = s_tempDir / "meta_sopclass_mismatch.dcm";
    {
        DcmFileFormat ff;
        ASSERT_TRUE(ff.loadFile(s_conformantDcm.string().c_str()).good());
        DcmMetaInfo* meta = ff.getMetaInfo();
        ASSERT_NE(nullptr, meta);
        // Relabel the meta as Secondary Capture while the dataset stays DX.
        ASSERT_TRUE(meta->putAndInsertString(
            DCM_MediaStorageSOPClassUID, UID_SecondaryCaptureImageStorage).good());
        ASSERT_TRUE(ff.saveFile(path.string().c_str(), EXS_LittleEndianExplicit,
                                EET_ExplicitLength, EGL_recalcGL, EPD_withoutPadding,
                                0, 0, EWM_dontUpdateMeta).good());
    }

    char report[8192] = {};
    ASSERT_EQ(XPE_OK, xpe_dicom_validate(path.string().c_str(), report, sizeof(report)));
    auto j = json::parse(report);
    EXPECT_FALSE(j["valid"].get<bool>()) << report;
}

// Round trip: what this project's own writer produces must satisfy the new
// check. A conformance rule that rejects our own output is a defect in the
// rule, and this case is what would catch it.
TEST_F(DicomValidatorTest, ValidateWriterOutput_IsConformant) {
    auto path = s_tempDir / "writer_roundtrip.dcm";
    XpeImageBuffer img{};
    ASSERT_EQ(XPE_OK, xpe_alloc_image(32, 32, XPE_PIXEL_UINT16, &img));
    XpeImageMetadata meta{};
    std::snprintf(meta.bodyPart, sizeof(meta.bodyPart), "%s", "CHEST");
    ASSERT_EQ(XPE_OK, xpe_dicom_write(path.string().c_str(), &img, &meta));
    xpe_free_image(&img);

    char report[8192] = {};
    ASSERT_EQ(XPE_OK, xpe_dicom_validate(path.string().c_str(), report, sizeof(report)));
    auto j = json::parse(report);
    EXPECT_TRUE(j["valid"].get<bool>()) << report;
}

// ---------------------------------------------------------------------------
// #139 (QA-B-37): the three branches QA-B-36 implemented but did not test.
// B-36 listed them as Gaps rather than claiming them covered; these cases turn
// that judgement into observation.
// ---------------------------------------------------------------------------

// (0002,0003) is the second half of the meta/dataset identity pair. B-36 tested
// only the SOP *Class* axis and recorded "same code path" as the reason the
// Instance axis was believed to work -- a reason, not evidence.
TEST_F(DicomValidatorTest, ValidateMetaSopInstanceMismatch_ReportsInconsistency) {
    auto path = s_tempDir / "meta_sopinstance_mismatch.dcm";
    {
        DcmFileFormat ff;
        ASSERT_TRUE(ff.loadFile(s_conformantDcm.string().c_str()).good());
        DcmMetaInfo* meta = ff.getMetaInfo();
        ASSERT_NE(nullptr, meta);
        // A well-formed UID that is simply not the dataset's SOP Instance UID.
        ASSERT_TRUE(meta->putAndInsertString(
            DCM_MediaStorageSOPInstanceUID, "1.2.826.0.1.3680043.9.7133.1.99").good());
        ASSERT_TRUE(ff.saveFile(path.string().c_str(), EXS_LittleEndianExplicit,
                                EET_ExplicitLength, EGL_recalcGL, EPD_withoutPadding,
                                0, 0, EWM_dontUpdateMeta).good());
    }

    char report[8192] = {};
    ASSERT_EQ(XPE_OK, xpe_dicom_validate(path.string().c_str(), report, sizeof(report)));
    auto j = json::parse(report);
    EXPECT_FALSE(j["valid"].get<bool>()) << report;

    // The tag must be named: an error that says "something disagrees" sends the
    // caller back to the file to find out which half.
    bool taggedInstance = false;
    for (const auto& e : j["errors"]) {
        if (e.value("tag", std::string()) == "0002,0003") taggedInstance = true;
    }
    EXPECT_TRUE(taggedInstance) << "no error tagged 0002,0003: " << report;
}

// A TransferSyntaxUID that is present but not a UID. "1.2.abc" is well-formed
// as a string and malformed as a UID, which is exactly the case a presence-only
// check would wave through.
TEST_F(DicomValidatorTest, ValidateMetaMalformedTransferSyntax_ReportsInvalidUid) {
    auto path = s_tempDir / "meta_bad_transfer_syntax.dcm";
    {
        DcmFileFormat ff;
        ASSERT_TRUE(ff.loadFile(s_conformantDcm.string().c_str()).good());
        DcmMetaInfo* meta = ff.getMetaInfo();
        ASSERT_NE(nullptr, meta);
        ASSERT_TRUE(meta->putAndInsertString(DCM_TransferSyntaxUID, "1.2.abc").good());
        ASSERT_TRUE(ff.saveFile(path.string().c_str(), EXS_LittleEndianExplicit,
                                EET_ExplicitLength, EGL_recalcGL, EPD_withoutPadding,
                                0, 0, EWM_dontUpdateMeta).good());
    }

    char report[8192] = {};
    ASSERT_EQ(XPE_OK, xpe_dicom_validate(path.string().c_str(), report, sizeof(report)));
    auto j = json::parse(report);
    EXPECT_FALSE(j["valid"].get<bool>()) << report;

    bool taggedTs = false;
    for (const auto& e : j["errors"]) {
        if (e.value("tag", std::string()) == "0002,0010") taggedTs = true;
    }
    EXPECT_TRUE(taggedTs) << "no error tagged 0002,0010: " << report;
}

// The meta rule is a consistency rule, not a DX rule. A file relabelled as CT
// on BOTH sides must still pass -- if it did not, the check would be quietly
// enforcing "DX only", which no requirement states.
TEST_F(DicomValidatorTest, ValidateNonDxSopClass_RoundTripsAsConformant) {
    auto path = s_tempDir / "ct_roundtrip.dcm";
    static const char* const kCtStorage = "1.2.840.10008.5.1.4.1.1.2";
    {
        DcmFileFormat ff;
        ASSERT_TRUE(ff.loadFile(s_conformantDcm.string().c_str()).good());
        DcmDataset* ds = ff.getDataset();
        ASSERT_NE(nullptr, ds);
        ASSERT_TRUE(ds->putAndInsertString(DCM_SOPClassUID, kCtStorage).good());
        // Default write mode regenerates the meta group from the dataset, so
        // both sides move together -- that is the point of the case.
        ASSERT_TRUE(ff.saveFile(path.string().c_str(), EXS_LittleEndianExplicit).good());
    }

    char report[8192] = {};
    ASSERT_EQ(XPE_OK, xpe_dicom_validate(path.string().c_str(), report, sizeof(report)));
    auto j = json::parse(report);
    EXPECT_TRUE(j["valid"].get<bool>()) << report;
}

// ---------------------------------------------------------------------------
// #139 (QA-B-38): the "absent" half of the meta checks. B-36/B-37 exercised
// only the "present but different" branches; the implementation has a separate
// path for a meta element that carries no value, and nothing had entered it.
// ---------------------------------------------------------------------------

namespace {
// Rewrite one meta element of the conformant fixture and save without letting
// DCMTK regenerate the meta group. Returns the path written.
fs::path writeWithMetaEdit(const fs::path& src,
                           const fs::path& dst,
                           const std::function<void(DcmMetaInfo&)>& edit) {
    DcmFileFormat ff;
    EXPECT_TRUE(ff.loadFile(src.string().c_str()).good());
    DcmMetaInfo* meta = ff.getMetaInfo();
    EXPECT_NE(nullptr, meta);
    if (meta != nullptr) edit(*meta);
    EXPECT_TRUE(ff.saveFile(dst.string().c_str(), EXS_LittleEndianExplicit,
                            EET_ExplicitLength, EGL_recalcGL, EPD_withoutPadding,
                            0, 0, EWM_dontUpdateMeta).good());
    return dst;
}

// Validate and return the parsed report; asserts the call itself succeeded.
json validateReport(const fs::path& path) {
    char report[8192] = {};
    EXPECT_EQ(XPE_OK, xpe_dicom_validate(path.string().c_str(), report, sizeof(report)));
    return json::parse(report);
}

bool hasErrorTagged(const json& report, const char* tag) {
    for (const auto& e : report["errors"]) {
        if (e.value("tag", std::string()) == tag) return true;
    }
    return false;
}
}  // namespace

TEST_F(DicomValidatorTest, ValidateMetaSopClassEmpty_ReportsMissingMetaSopClass) {
    const auto path = writeWithMetaEdit(
        s_conformantDcm, s_tempDir / "meta_sopclass_empty.dcm",
        [](DcmMetaInfo& m) { m.putAndInsertString(DCM_MediaStorageSOPClassUID, ""); });

    const auto j = validateReport(path);
    EXPECT_FALSE(j["valid"].get<bool>()) << j.dump();
    EXPECT_TRUE(hasErrorTagged(j, "0002,0002")) << j.dump();
}

TEST_F(DicomValidatorTest, ValidateMetaSopInstanceEmpty_ReportsMissingMetaSopInstance) {
    const auto path = writeWithMetaEdit(
        s_conformantDcm, s_tempDir / "meta_sopinstance_empty.dcm",
        [](DcmMetaInfo& m) { m.putAndInsertString(DCM_MediaStorageSOPInstanceUID, ""); });

    const auto j = validateReport(path);
    EXPECT_FALSE(j["valid"].get<bool>()) << j.dump();
    EXPECT_TRUE(hasErrorTagged(j, "0002,0003")) << j.dump();
}

// The element is removed outright, not emptied: this is the one case where the
// meta group exists (so card() != 0) yet says nothing about the encoding.
TEST_F(DicomValidatorTest, ValidateMetaTransferSyntaxAbsent_ReportsMissingTransferSyntax) {
    const auto path = writeWithMetaEdit(
        s_conformantDcm, s_tempDir / "meta_no_transfer_syntax.dcm",
        [](DcmMetaInfo& m) { m.findAndDeleteElement(DCM_TransferSyntaxUID); });

    const auto j = validateReport(path);
    EXPECT_FALSE(j["valid"].get<bool>()) << j.dump();
    EXPECT_TRUE(hasErrorTagged(j, "0002,0010")) << j.dump();
}

// ---------------------------------------------------------------------------
// #142 (QA-B-42): the output-buffer contract.
//
// leader's decision: an output pointer that is NULL, or a declared size of 0,
// is XPE_ERR_INVALID_INPUT -- the argument does not exist, which is a different
// fault from "it exists but is too small" (XPE_ERR_BUFFER_TOO_SMALL). NULL/0 is
// judged first so the two can never overlap.
//
// The size-report protocol makes this more than a naming question. On
// BUFFER_TOO_SMALL the validator writes the required size as a uint32_t into
// the first 4 bytes of the caller's buffer -- unconditionally, without checking
// that 4 bytes exist. A caller passing a 2-byte buffer therefore had 2 bytes
// written past its end. The cases below use an over-allocated block with a
// known fill so that overflow lands in the test's own memory and is detectable
// instead of corrupting the heap.
// ---------------------------------------------------------------------------

TEST_F(DicomValidatorTest, OutputBufferZeroLength_ReturnsInvalidInput) {
    char guarded[64];
    std::memset(guarded, static_cast<int>(0xAB), sizeof(guarded));

    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_dicom_validate(s_conformantDcm.string().c_str(), guarded, 0));

    for (size_t i = 0; i < sizeof(guarded); ++i) {
        ASSERT_EQ(static_cast<unsigned char>(0xAB),
                  static_cast<unsigned char>(guarded[i]))
            << "byte " << i << " written despite a declared length of 0";
    }
}

TEST_F(DicomValidatorTest, OutputBufferTooSmallForSizeReport_DoesNotOverflow) {
    char guarded[64];
    std::memset(guarded, static_cast<int>(0xAB), sizeof(guarded));

    // 2 bytes is a real buffer, so this is BUFFER_TOO_SMALL -- but it cannot
    // hold the 4-byte required-size report either.
    const XpeErrorCode rc =
        xpe_dicom_validate(s_conformantDcm.string().c_str(), guarded, 2);
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL, rc);

    for (size_t i = 2; i < sizeof(guarded); ++i) {
        ASSERT_EQ(static_cast<unsigned char>(0xAB),
                  static_cast<unsigned char>(guarded[i]))
            << "byte " << i << " written past a 2-byte buffer";
    }
}

// The complement: a buffer that IS big enough for the size report must still
// receive it, so the fix above does not silently drop the protocol.
TEST_F(DicomValidatorTest, OutputBufferTooSmall_StillReportsRequiredSize) {
    char buf[8] = {};
    EXPECT_EQ(XPE_ERR_BUFFER_TOO_SMALL,
              xpe_dicom_validate(s_conformantDcm.string().c_str(), buf, sizeof(buf)));

    uint32_t required = 0;
    std::memcpy(&required, buf, sizeof(required));
    EXPECT_GT(required, sizeof(buf)) << "required size must exceed the buffer given";
}

// ---------------------------------------------------------------------------
// QA-B-206 C5: REQ-DICOM-024 "Required Type 1 tags ... present AND non-empty". Each case derives from the conformant file,
// changing exactly one attribute, so the one tag under test is the only difference.
// ---------------------------------------------------------------------------

namespace {

/** Validates the conformant file with @p change applied to its dataset; returns the parsed report. */
json ValidateChanged(const fs::path& conformant, const fs::path& out, const std::function<void(DcmDataset*)>& change,
                     XpeErrorCode* rc) {
    DcmFileFormat ff;
    EXPECT_TRUE(ff.loadFile(conformant.string().c_str()).good());
    change(ff.getDataset());
    EXPECT_TRUE(ff.saveFile(out.string().c_str(), EXS_LittleEndianExplicit).good());
    char report[8192] = {};
    *rc = xpe_dicom_validate(out.string().c_str(), report, sizeof(report));
    return json::parse(report);
}

bool HasErrorFor(const json& j, const char* tag) {
    for (const auto& e : j["errors"]) {
        if (e["tag"].get<std::string>() == tag) return true;
    }
    return false;
}

}  // namespace

TEST_F(DicomValidatorTest, ARequiredTypeOneTagWithNoValueIsAnErrorWhateverTheKindOfTag) {
    struct Case {
        const char* what;
        const char* tag;
        std::function<void(DcmDataset*)> change;
    } cases[] = {
        // Patient's Name and Patient ID are NOT in this list: they are Type 2 (QA-B-206 M2b), see below
        {"Modality empty", "0008,0060", [](DcmDataset* d) { d->putAndInsertString(DCM_Modality, ""); }},
        {"StudyInstanceUID empty", "0020,000D", [](DcmDataset* d) { d->putAndInsertString(DCM_StudyInstanceUID, ""); }},
        {"Rows empty (US without a value)", "0028,0010", [](DcmDataset* d) { d->insertEmptyElement(DCM_Rows); }},
        {"BitsStored empty (US without a value)", "0028,0101", [](DcmDataset* d) { d->insertEmptyElement(DCM_BitsStored); }},
        {"PixelData zero length", "7FE0,0010", [](DcmDataset* d) {
             // insertEmptyElement alone leaves DcmPixelData's existing representation (the full 32768 bytes) in place, so the
             // element is removed first and a zero-length one is put in its place
             d->findAndDeleteElement(DCM_PixelData);
             d->putAndInsertUint8Array(DCM_PixelData, nullptr, 0);
         }},
    };
    int n = 0;
    for (const Case& c : cases) {
        XpeErrorCode rc = XPE_ERR_NOT_INITIALIZED;
        const json j = ValidateChanged(s_conformantDcm, s_tempDir / ("c5_" + std::to_string(n++) + ".dcm"), c.change, &rc);
        EXPECT_EQ(XPE_OK, rc) << c.what;
        EXPECT_FALSE(j["valid"].get<bool>()) << c.what << ": " << j.dump();
        EXPECT_TRUE(HasErrorFor(j, c.tag)) << c.what << ": no error for " << c.tag << ": " << j.dump();
    }
}

TEST_F(DicomValidatorTest, AValueThatIsPresentIsStillValidAndOnlyPaddingCountsAsEmpty) {
    // Controls: a short value, a value with surrounding spaces, and a value of a single character are all non-empty.
    struct Case {
        const char* what;
        std::function<void(DcmDataset*)> change;
    } cases[] = {
        {"PatientID one character", [](DcmDataset* d) { d->putAndInsertString(DCM_PatientID, "X"); }},
        {"PatientID with surrounding spaces", [](DcmDataset* d) { d->putAndInsertString(DCM_PatientID, "  A1  "); }},
        {"PatientName with component delimiters only is a value", [](DcmDataset* d) { d->putAndInsertString(DCM_PatientName, "^"); }},
    };
    int n = 0;
    for (const Case& c : cases) {
        XpeErrorCode rc = XPE_ERR_NOT_INITIALIZED;
        const json j = ValidateChanged(s_conformantDcm, s_tempDir / ("c5ok_" + std::to_string(n++) + ".dcm"), c.change, &rc);
        EXPECT_EQ(XPE_OK, rc) << c.what;
        EXPECT_TRUE(j["valid"].get<bool>()) << c.what << ": " << j.dump();
        EXPECT_TRUE(j["errors"].empty()) << c.what << ": " << j.dump();
    }
}

TEST_F(DicomValidatorTest, AMissingTagAndAnEmptyTagAreReportedWithDifferentMessages) {
    XpeErrorCode rc = XPE_ERR_NOT_INITIALIZED;
    const json missing = ValidateChanged(s_conformantDcm, s_tempDir / "c5_missing.dcm",
                                         [](DcmDataset* d) { d->findAndDeleteElement(DCM_StudyInstanceUID); }, &rc);
    const json empty = ValidateChanged(s_conformantDcm, s_tempDir / "c5_empty.dcm",
                                       [](DcmDataset* d) { d->putAndInsertString(DCM_StudyInstanceUID, ""); }, &rc);
    ASSERT_EQ(1u, missing["errors"].size()) << missing.dump();
    EXPECT_NE(std::string::npos, missing["errors"][0]["message"].get<std::string>().find("Missing"));
    // QA-B-206 M2c (Codex #113): an empty UID is ONE defect, reported once as "no value"; its format is not judged as well.
    // M2b had loosened this assertion to allow the second, "Invalid UID format", report.
    ASSERT_EQ(1u, empty["errors"].size()) << empty.dump();
    EXPECT_EQ("0020,000D", empty["errors"][0]["tag"].get<std::string>());
    EXPECT_NE(std::string::npos, empty["errors"][0]["message"].get<std::string>().find("no value")) << empty.dump();
    EXPECT_EQ(std::string::npos, empty["errors"][0]["message"].get<std::string>().find("Missing")) << empty.dump();
}

// ---------------------------------------------------------------------------
// QA-B-206 M2b (Codex #111): Patient's Name (0010,0010) and Patient ID (0010,0020) are Type 2 in the DX IOD (PS3.3 Table C.7-1,
// included by A.26.3): the attribute must be present, an empty value is allowed. QA-B-206 M2 judged them as Type 1 (REQ-DICOM-024
// listed them there) and so refused an anonymized file that keeps both elements with no value. Presence is still required.
// ---------------------------------------------------------------------------

TEST_F(DicomValidatorTest, PatientNameAndPatientIdWithNoValueAreConformantTypeTwoAttributes) {
    struct Case {
        const char* what;
        std::function<void(DcmDataset*)> change;
    } cases[] = {
        {"PatientID empty", [](DcmDataset* d) { d->putAndInsertString(DCM_PatientID, ""); }},
        {"PatientName empty", [](DcmDataset* d) { d->putAndInsertString(DCM_PatientName, ""); }},
        {"PatientID only spaces", [](DcmDataset* d) { d->putAndInsertString(DCM_PatientID, "    "); }},
        {"PatientName only spaces", [](DcmDataset* d) { d->putAndInsertString(DCM_PatientName, "  "); }},
        {"both empty (an anonymized file)", [](DcmDataset* d) {
             d->putAndInsertString(DCM_PatientID, "");
             d->putAndInsertString(DCM_PatientName, "");
         }},
    };
    int n = 0;
    for (const Case& c : cases) {
        XpeErrorCode rc = XPE_ERR_NOT_INITIALIZED;
        const json j = ValidateChanged(s_conformantDcm, s_tempDir / ("m2b_empty_" + std::to_string(n++) + ".dcm"), c.change, &rc);
        EXPECT_EQ(XPE_OK, rc) << c.what;
        EXPECT_TRUE(j["valid"].get<bool>()) << c.what << ": " << j.dump();
        EXPECT_TRUE(j["errors"].empty()) << c.what << ": " << j.dump();
    }
}

TEST_F(DicomValidatorTest, PatientNameAndPatientIdThatAreAbsentAreStillAnErrorBecauseTypeTwoNeedsPresence) {
    struct Case {
        const char* what;
        const char* tag;
        std::function<void(DcmDataset*)> change;
    } cases[] = {
        {"PatientID absent", "0010,0020", [](DcmDataset* d) { d->findAndDeleteElement(DCM_PatientID); }},
        {"PatientName absent", "0010,0010", [](DcmDataset* d) { d->findAndDeleteElement(DCM_PatientName); }},
    };
    int n = 0;
    for (const Case& c : cases) {
        XpeErrorCode rc = XPE_ERR_NOT_INITIALIZED;
        const json j = ValidateChanged(s_conformantDcm, s_tempDir / ("m2b_absent_" + std::to_string(n++) + ".dcm"), c.change, &rc);
        EXPECT_EQ(XPE_OK, rc) << c.what;
        EXPECT_FALSE(j["valid"].get<bool>()) << c.what << ": " << j.dump();
        ASSERT_TRUE(HasErrorFor(j, c.tag)) << c.what << ": " << j.dump();
    }
    // both at once: two errors, one per attribute, and the message names the Type
    XpeErrorCode rc = XPE_ERR_NOT_INITIALIZED;
    const json both = ValidateChanged(s_conformantDcm, s_tempDir / "m2b_absent_both.dcm", [](DcmDataset* d) {
        d->findAndDeleteElement(DCM_PatientID);
        d->findAndDeleteElement(DCM_PatientName);
    }, &rc);
    ASSERT_EQ(2u, both["errors"].size()) << both.dump();
    for (const auto& e : both["errors"]) {
        EXPECT_NE(std::string::npos, e["message"].get<std::string>().find("Type 2")) << both.dump();
    }
}

// The whole required list, by Type, in one table: what must be present with a value (Type 1), what only present (Type 2).
TEST_F(DicomValidatorTest, EveryRequiredAttributeIsJudgedByTheTypeTheStandardGivesIt) {
    struct Row {
        const char* name;
        const char* tag;
        DcmTagKey key;
        bool type1;   // false: Type 2
    } rows[] = {
        {"Patient's Name", "0010,0010", DCM_PatientName, false},
        {"Patient ID", "0010,0020", DCM_PatientID, false},
        {"Study Instance UID", "0020,000D", DCM_StudyInstanceUID, true},
        {"Series Instance UID", "0020,000E", DCM_SeriesInstanceUID, true},
        {"SOP Instance UID", "0008,0018", DCM_SOPInstanceUID, true},
        {"Modality", "0008,0060", DCM_Modality, true},
    };
    int n = 0;
    for (const Row& r : rows) {
        XpeErrorCode rc = XPE_ERR_NOT_INITIALIZED;
        const json absent = ValidateChanged(s_conformantDcm, s_tempDir / ("m2b_t_abs_" + std::to_string(n) + ".dcm"),
                                            [&](DcmDataset* d) { d->findAndDeleteElement(r.key); }, &rc);
        EXPECT_TRUE(HasErrorFor(absent, r.tag)) << r.name << ": an absent attribute is an error for both Types";
        const json empty = ValidateChanged(s_conformantDcm, s_tempDir / ("m2b_t_emp_" + std::to_string(n) + ".dcm"),
                                           [&](DcmDataset* d) { d->putAndInsertString(r.key, ""); }, &rc);
        // judged by the "no value" message and not just by the tag, and by the NUMBER of reports for the tag: before M2c an empty
        // UID was ALSO reported as an invalid UID format, so an error for the same tag would have hidden a Type 1 attribute
        // downgraded to Type 2. Now an empty Type 1 value is exactly one report, "no value"; an empty Type 2 value is none.
        bool noValue = false;
        int reports = 0;
        for (const auto& e : empty["errors"]) {
            if (e["tag"].get<std::string>() != r.tag) continue;
            ++reports;
            if (e["message"].get<std::string>().find("no value") != std::string::npos) noValue = true;
        }
        EXPECT_EQ(r.type1, noValue) << r.name << ": an empty value is an error only for Type 1: " << empty.dump();
        EXPECT_EQ(r.type1 ? 1 : 0, reports) << r.name << ": " << empty.dump();
        if (!r.type1) EXPECT_TRUE(empty["errors"].empty()) << r.name << ": " << empty.dump();
        ++n;
    }
}

// ---------------------------------------------------------------------------
// QA-B-206 M2c (Codex #113): Pixel Data (7FE0,0010) is Type 1C in the Image Pixel module -- "required if Pixel Data Provider URL
// (0028,7FE0) is not present". A file that gives its pixels by reference has no Pixel Data and is not wrong for that; this
// module cannot read pixels by reference, so it says so once, as a warning that leaves `valid` alone. The URL has to carry a
// value to count as a provider. The cases are the table in the M2d test below.
// ---------------------------------------------------------------------------

namespace {

int CountMessages(const json& list, const char* tag, const char* text) {
    int n = 0;
    for (const auto& e : list) {
        if (e["tag"].get<std::string>() == tag && e["message"].get<std::string>().find(text) != std::string::npos) ++n;
    }
    return n;
}

}  // namespace

// QA-B-206 M2d (Codex #114): the Provider URL is part of the JPIP Referenced transfer syntaxes alone (1.2.840.10008.1.2.4.94 and
// .95), and Pixel Data and the URL are mutually exclusive (PS3.5 8.2). What a URL means therefore depends on the transfer syntax
// the file declares in its meta group, so every case is written UNDER the syntax it names (DCMTK sets the meta group's
// TransferSyntaxUID from the syntax it is asked to write). The first M2c version of this test treated "both present" as valid
// and ignored the syntax; this is the table the lead set in QA-B-206.md (M2d).
namespace {

/** Like ValidateChanged, but the file is written under transfer syntax @p xfer. */
json ValidateChangedUnder(E_TransferSyntax xfer, const fs::path& conformant, const fs::path& out,
                          const std::function<void(DcmDataset*)>& change, XpeErrorCode* rc, bool* saved) {
    DcmFileFormat ff;
    EXPECT_TRUE(ff.loadFile(conformant.string().c_str()).good());
    change(ff.getDataset());
    const OFCondition st = ff.saveFile(out.string().c_str(), xfer);
    *saved = st.good();
    if (!st.good()) {
        ADD_FAILURE() << "DCMTK cannot write this file under that transfer syntax: " << st.text();
        return json::object();
    }
    char report[8192] = {};
    *rc = xpe_dicom_validate(out.string().c_str(), report, sizeof(report));
    return json::parse(report);
}

}  // namespace

TEST_F(DicomValidatorTest, PixelDataProviderUrlIsJudgedByTheTransferSyntaxAndExcludesPixelData) {
    const char* kUrl = "http://example.invalid/jpip";
    struct Case {
        const char* what;
        E_TransferSyntax xfer;
        bool pixelData;   // false: the element is deleted
        const char* url;  // nullptr: no URL element; "": the element with no value
        bool valid;
        int missing;      // "Missing required Type 1 tag" reports for Pixel Data
        int exclusive;    // "mutually exclusive" reports
        int jpipPixel;    // "shall not be present under a JPIP Referenced transfer syntax" reports (PS3.5 A.6)
        int reference;    // "by reference" warnings
    };
    const Case cases[] = {
        {"ordinary file: Pixel Data, no URL", EXS_LittleEndianExplicit, true, nullptr, true, 0, 0, 0, 0},
        {"JPIP Referenced (.94): URL only", EXS_JPIPReferenced, false, kUrl, true, 0, 0, 0, 1},
        {"JPIP HTJ2K Referenced (.204): URL only", EXS_JPIPHTJ2KReferenced, false, kUrl, true, 0, 0, 0, 1},
        {"JPIP HTJ2K Referenced (.204): Pixel Data, no URL (PS3.5 A.11)", EXS_JPIPHTJ2KReferenced, true, nullptr, false, 0, 0, 1, 0},
        {"JPIP HTJ2K Referenced (.204): neither", EXS_JPIPHTJ2KReferenced, false, nullptr, false, 1, 0, 0, 0},
        {"JPIP Referenced: Pixel Data and URL breaks both rules", EXS_JPIPReferenced, true, kUrl, false, 0, 1, 1, 0},
        {"JPIP Referenced: Pixel Data, no URL (PS3.5 A.6)", EXS_JPIPReferenced, true, nullptr, false, 0, 0, 1, 0},
        {"other syntax: URL only, it replaces nothing", EXS_LittleEndianExplicit, false, kUrl, false, 1, 0, 0, 0},
        {"other syntax: Pixel Data and URL", EXS_LittleEndianExplicit, true, kUrl, false, 0, 1, 0, 0},
        {"other syntax: neither", EXS_LittleEndianExplicit, false, nullptr, false, 1, 0, 0, 0},
        {"JPIP Referenced: neither", EXS_JPIPReferenced, false, nullptr, false, 1, 0, 0, 0},
        {"JPIP Referenced: an EMPTY URL is not a provider", EXS_JPIPReferenced, false, "", false, 1, 0, 0, 0},
        {"other syntax: Pixel Data and a URL element with no value (present counts)", EXS_LittleEndianExplicit, true, "", false, 0, 1, 0, 0},
    };
    int n = 0;
    for (const Case& c : cases) {
        XpeErrorCode rc = XPE_ERR_NOT_INITIALIZED;
        bool saved = false;
        const json j = ValidateChangedUnder(
            c.xfer, s_conformantDcm, s_tempDir / ("m2d_px_" + std::to_string(n++) + ".dcm"),
            [&](DcmDataset* d) {
                if (!c.pixelData) d->findAndDeleteElement(DCM_PixelData);
                if (c.url) d->putAndInsertString(DCM_PixelDataProviderURL, c.url);
            },
            &rc, &saved);
        if (!saved) continue;
        // a file that parses but does not conform is a report with valid:false and rc OK; DICOM_INVALID is for a file that
        // cannot be parsed at all
        EXPECT_EQ(XPE_OK, rc) << c.what << ": " << j.dump();
        EXPECT_EQ(c.valid, j["valid"].get<bool>()) << c.what << ": " << j.dump();
        EXPECT_EQ(c.missing, CountMessages(j["errors"], "7FE0,0010", "Missing required Type 1 tag")) << c.what << ": " << j.dump();
        EXPECT_EQ(c.exclusive, CountMessages(j["errors"], "0028,7FE0", "mutually exclusive")) << c.what << ": " << j.dump();
        EXPECT_EQ(c.jpipPixel, CountMessages(j["errors"], "7FE0,0010", "shall not be present under a JPIP")) << c.what << ": " << j.dump();
        EXPECT_EQ(static_cast<size_t>(c.missing + c.exclusive + c.jpipPixel), j["errors"].size()) << c.what << ": no other error: " << j.dump();
        EXPECT_EQ(c.reference, CountMessages(j["warnings"], "7FE0,0010", "by reference")) << c.what << ": " << j.dump();
        EXPECT_EQ(static_cast<size_t>(c.reference), j["warnings"].size()) << c.what << ": no other warning: " << j.dump();
    }
}

// .95 (JPIP Referenced Deflate) and .205 (JPIP HTJ2K Referenced Deflate) belong in the table above exactly like .94 and .204, but
// this DCMTK build cannot read a file under either: the validator gets "Unsupported compression or encryption" from loadFile and
// reports the file as unparseable, so nothing about Pixel Data or the URL is ever judged. Recorded as what is observed, not as
// what is wanted: when the dependency gains deflate support this goes red, and the cases are added to the table (the code
// already treats .95 and .205 like .94 and .204).
TEST_F(DicomValidatorTest, KnownDivergence_JpipReferencedDeflateSyntaxesCannotBeParsedByThisDcmtkBuild) {
    const E_TransferSyntax xfers[] = {EXS_JPIPReferencedDeflate, EXS_JPIPHTJ2KReferencedDeflate};
    int n = 0;
    for (E_TransferSyntax x : xfers) {
        XpeErrorCode rc = XPE_ERR_NOT_INITIALIZED;
        bool saved = false;
        const json j = ValidateChangedUnder(
            x, s_conformantDcm, s_tempDir / ("m2d_px_deflate_" + std::to_string(n++) + ".dcm"),
            [](DcmDataset* d) {
                d->findAndDeleteElement(DCM_PixelData);
                d->putAndInsertString(DCM_PixelDataProviderURL, "http://example.invalid/jpip");
            },
            &rc, &saved);
        if (!saved) continue;
        EXPECT_EQ(XPE_ERR_DICOM_INVALID, rc) << j.dump();
        EXPECT_FALSE(j["valid"].get<bool>()) << j.dump();
        EXPECT_EQ(1, CountMessages(j["errors"], "0008,0000", "cannot be parsed")) << j.dump();
    }
}

TEST_F(DicomValidatorTest, ABlankUidIsReportedOnceAsNoValueAndItsFormatIsNotJudgedAsWell) {
    const struct { const char* what; const char* tag; DcmTagKey key; } rows[] = {
        {"Study", "0020,000D", DCM_StudyInstanceUID},
        {"Series", "0020,000E", DCM_SeriesInstanceUID},
        {"SOP Instance", "0008,0018", DCM_SOPInstanceUID},
    };
    int n = 0;
    for (const auto& r : rows) {
        XpeErrorCode rc = XPE_ERR_NOT_INITIALIZED;
        const json empty = ValidateChanged(s_conformantDcm, s_tempDir / ("m2c_uid_e" + std::to_string(n) + ".dcm"),
                                           [&](DcmDataset* d) { d->putAndInsertString(r.key, ""); }, &rc);
        EXPECT_EQ(1, CountMessages(empty["errors"], r.tag, "no value")) << r.what << ": " << empty.dump();
        EXPECT_EQ(0, CountMessages(empty["errors"], r.tag, "Invalid UID format")) << r.what << ": " << empty.dump();
        // a UID that HAS a value is still judged for its format: the skip is for the blank one only
        const json bad = ValidateChanged(s_conformantDcm, s_tempDir / ("m2c_uid_b" + std::to_string(n) + ".dcm"),
                                         [&](DcmDataset* d) { d->putAndInsertString(r.key, "not.a.uid"); }, &rc);
        EXPECT_EQ(1, CountMessages(bad["errors"], r.tag, "Invalid UID format")) << r.what << ": " << bad.dump();
        EXPECT_EQ(0, CountMessages(bad["errors"], r.tag, "no value")) << r.what << ": " << bad.dump();
        ++n;
    }
}
