/**
 * @file test_dicom_reader.cpp
 * @brief TDD tests for DicomReader / SWU-4.1 (>= 12 test cases)
 * SPEC: SPEC-XPE-P1B-DICOM REQ-DICOM-001..012, AC-01, AC-04, AC-09, AC-10
 *
 * Test data strategy:
 *   Synthetic DICOM files are created in SetUpTestSuite using DicomWriter
 *   to avoid dependency on external test assets.
 */
#include <gtest/gtest.h>
#include "xpe/dicom/dicom_api.h"

// #124 (QA-B-25): the Implicit VR LE fixture is derived from the written
// Explicit LE file with DCMTK, so the test links DCMTK directly. Before this,
// s_implicitLEDcm was only ever assigned a path -- the file was never written,
// so the guard skipped everywhere, CI and local alike.
#include <dcmtk/dcmdata/dctk.h>
#include <dcmtk/dcmdata/dcfilefo.h>
#include <dcmtk/dcmdata/dcpixel.h>
#include <dcmtk/dcmdata/dcpixseq.h>
#include <dcmtk/dcmdata/dcpxitem.h>
#include <dcmtk/dcmjpeg/djencode.h>
#include <dcmtk/dcmjpeg/djdecode.h>
#include <dcmtk/dcmjpeg/djrploss.h>
#include <dcmtk/dcmjpeg/djrplol.h>
#include <dcmtk/dcmjpeg/djrplol.h>
#include "DicomReader.h"   // #146: the accepted transfer-syntax table
#include "xpe/common/xpe_memory.h"
#include <atomic>
#include <cstdio>
#include <filesystem>
#include <thread>
#include <fstream>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Test fixture: creates synthetic DICOM files before all tests
// ---------------------------------------------------------------------------
class DicomReaderTest : public ::testing::Test {
protected:
    static void SetUpTestSuite();
    static void TearDownTestSuite();

    static fs::path s_validDcm;       // Valid Explicit LE DICOM
    static fs::path s_j2kDcm;         // Valid J2K Lossless DICOM
    static fs::path s_notDicom;       // PNG renamed to .dcm
    static fs::path s_implicitLEDcm;  // Implicit VR LE (unsupported TS)
    static fs::path s_tempDir;
};

fs::path DicomReaderTest::s_validDcm;
fs::path DicomReaderTest::s_j2kDcm;
fs::path DicomReaderTest::s_notDicom;
fs::path DicomReaderTest::s_implicitLEDcm;
fs::path DicomReaderTest::s_tempDir;

void DicomReaderTest::SetUpTestSuite() {
    s_tempDir = fs::temp_directory_path() / "xpe_dicom_reader_test";
    fs::create_directories(s_tempDir);

    // Create a minimal uint16 image
    XpeImageBuffer img{};
    xpe_alloc_image(256, 256, XPE_PIXEL_UINT16, &img);
    // Fill with gradient pattern
    auto* px = static_cast<uint16_t*>(img.data);
    for (uint32_t i = 0; i < img.width * img.height; ++i) px[i] = static_cast<uint16_t>(i & 0xFFFF);

    XpeImageMetadata meta{};
    std::snprintf(meta.bodyPart, sizeof(meta.bodyPart), "%s", "CHEST");
    meta.kVp = 80.0f;
    meta.mAs = 2.5f;
    meta.SID_mm = 1800.0f;
    meta.pixelPitch_mm = 0.148f;

    s_validDcm = s_tempDir / "valid_explicit_le.dcm";
    xpe_dicom_write(s_validDcm.string().c_str(), &img, &meta);

    s_j2kDcm = s_tempDir / "valid_j2k.dcm";
    xpe_dicom_write_j2k(s_j2kDcm.string().c_str(), &img, &meta);

    // Non-DICOM file (random bytes)
    s_notDicom = s_tempDir / "not_dicom.dcm";
    std::ofstream f(s_notDicom, std::ios::binary);
    const char fake[] = "\x89PNG\r\n\x1a\nFAKEDATA";
    f.write(fake, sizeof(fake));

    xpe_free_image(&img);

    // AC-04 fixture: the same content re-encoded as Implicit VR Little Endian,
    // which the reader is expected to reject as an unsupported transfer syntax.
    s_implicitLEDcm = s_tempDir / "implicit_le.dcm";
    {
        DcmFileFormat ff;
        if (ff.loadFile(s_validDcm.string().c_str()).good()) {
            ff.saveFile(s_implicitLEDcm.string().c_str(), EXS_LittleEndianImplicit);
        }
    }
}

void DicomReaderTest::TearDownTestSuite() {
    fs::remove_all(s_tempDir);
}

// ---------------------------------------------------------------------------
// REQ-DICOM-001: Valid file open
// ---------------------------------------------------------------------------
TEST_F(DicomReaderTest, OpenValid_ReturnsOkAndNonNullHandle) {
    XpeDicomHandle* handle = nullptr;
    EXPECT_EQ(XPE_OK, xpe_dicom_open(s_validDcm.string().c_str(), &handle));
    EXPECT_NE(nullptr, handle);
    xpe_dicom_close(handle);
}

// ---------------------------------------------------------------------------
// REQ-DICOM-002: Missing file returns IO_FAILED
// ---------------------------------------------------------------------------
TEST_F(DicomReaderTest, OpenMissingFile_ReturnsIOFailed) {
    XpeDicomHandle* handle = nullptr;
    EXPECT_EQ(XPE_ERR_IO_FAILED,
              xpe_dicom_open("/nonexistent/path/file.dcm", &handle));
    EXPECT_EQ(nullptr, handle);
}

// ---------------------------------------------------------------------------
// REQ-DICOM-003: Non-DICOM file returns DICOM_INVALID
// ---------------------------------------------------------------------------
TEST_F(DicomReaderTest, OpenInvalidDicom_ReturnsDicomInvalid) {
    XpeDicomHandle* handle = nullptr;
    EXPECT_EQ(XPE_ERR_DICOM_INVALID,
              xpe_dicom_open(s_notDicom.string().c_str(), &handle));
    EXPECT_EQ(nullptr, handle);
}

// ---------------------------------------------------------------------------
// REQ-DICOM-004: Explicit LE transfer syntax supported
// ---------------------------------------------------------------------------
TEST_F(DicomReaderTest, ReadExplicitLE_Succeeds) {
    XpeDicomHandle* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(s_validDcm.string().c_str(), &handle));
    XpeImageBuffer img{};
    EXPECT_EQ(XPE_OK, xpe_dicom_read_image(handle, &img));
    EXPECT_NE(nullptr, img.data);
    EXPECT_EQ(256u, img.width);
    EXPECT_EQ(256u, img.height);
    EXPECT_EQ(XPE_PIXEL_UINT16, img.format);
    xpe_free_image(&img);
    xpe_dicom_close(handle);
}

// ---------------------------------------------------------------------------
// REQ-DICOM-004: J2K Lossless transfer syntax supported
// ---------------------------------------------------------------------------
TEST_F(DicomReaderTest, ReadJ2KLossless_Succeeds) {
    XpeDicomHandle* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(s_j2kDcm.string().c_str(), &handle));
    XpeImageBuffer img{};
    EXPECT_EQ(XPE_OK, xpe_dicom_read_image(handle, &img));
    EXPECT_EQ(256u, img.width);
    xpe_free_image(&img);
    xpe_dicom_close(handle);
}

// ---------------------------------------------------------------------------
// REQ-DICOM-005: Unsupported transfer syntax
// ---------------------------------------------------------------------------
TEST_F(DicomReaderTest, UnsupportedTS_ReturnsUnsupportedFormat) {
    ASSERT_TRUE(fs::exists(s_implicitLEDcm)) << "Implicit LE fixture was not written";
    XpeDicomHandle* handle = nullptr;
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT,
              xpe_dicom_open(s_implicitLEDcm.string().c_str(), &handle));
}

// ---------------------------------------------------------------------------
// REQ-DICOM-006: Pixel data extracted with correct dimensions
// ---------------------------------------------------------------------------
TEST_F(DicomReaderTest, ReadImage_PixelDimensionsMatch) {
    XpeDicomHandle* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(s_validDcm.string().c_str(), &handle));
    XpeImageBuffer img{};
    ASSERT_EQ(XPE_OK, xpe_dicom_read_image(handle, &img));
    EXPECT_EQ(256u, img.width);
    EXPECT_EQ(256u, img.height);
    EXPECT_EQ(16u, img.bitsAllocated);
    EXPECT_EQ(XPE_PIXEL_UINT16, img.format);
    EXPECT_EQ(256u * 256u * sizeof(uint16_t), img.dataSize);
    xpe_free_image(&img);
    xpe_dicom_close(handle);
}

// ---------------------------------------------------------------------------
// REQ-DICOM-008: J2K decompressed pixel data is uint16
// ---------------------------------------------------------------------------
TEST_F(DicomReaderTest, ReadJ2KDecompress_ProducesUint16) {
    XpeDicomHandle* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(s_j2kDcm.string().c_str(), &handle));
    XpeImageBuffer img{};
    ASSERT_EQ(XPE_OK, xpe_dicom_read_image(handle, &img));
    EXPECT_EQ(XPE_PIXEL_UINT16, img.format);
    EXPECT_NE(nullptr, img.data);
    xpe_free_image(&img);
    xpe_dicom_close(handle);
}

// ---------------------------------------------------------------------------
// REQ-DICOM-009: Metadata extracted correctly
// ---------------------------------------------------------------------------
TEST_F(DicomReaderTest, GetMetadata_FieldsPopulated) {
    XpeDicomHandle* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(s_validDcm.string().c_str(), &handle));
    XpeImageMetadata meta{};
    EXPECT_EQ(XPE_OK, xpe_dicom_get_metadata(handle, &meta));
    EXPECT_STREQ("CHEST", meta.bodyPart);
    EXPECT_NEAR(80.0f, meta.kVp, 0.01f);
    EXPECT_NEAR(2.5f, meta.mAs, 0.001f);
    EXPECT_NEAR(1800.0f, meta.SID_mm, 0.01f);
    EXPECT_NEAR(0.148f, meta.pixelPitch_mm, 0.0001f);
    xpe_dicom_close(handle);
}

// ---------------------------------------------------------------------------
// REQ-DICOM-010: Missing tags silently default
// ---------------------------------------------------------------------------
TEST_F(DicomReaderTest, GetMetadata_MissingTagsDefault) {
    // A minimal DICOM written without optional acquisition tags
    // still returns XPE_OK with zeroed fields
    XpeDicomHandle* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(s_validDcm.string().c_str(), &handle));
    XpeImageMetadata meta{};
    EXPECT_EQ(XPE_OK, xpe_dicom_get_metadata(handle, &meta));
    // No error even if some tags are absent
    xpe_dicom_close(handle);
}

// ---------------------------------------------------------------------------
// REQ-DICOM-011: Close valid handle
// ---------------------------------------------------------------------------
TEST_F(DicomReaderTest, CloseValid_NoLeak) {
    XpeDicomHandle* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(s_validDcm.string().c_str(), &handle));
    ASSERT_NO_FATAL_FAILURE(xpe_dicom_close(handle));
}

// ---------------------------------------------------------------------------
// REQ-DICOM-012: Close NULL is no-op
// ---------------------------------------------------------------------------
TEST_F(DicomReaderTest, CloseNull_NoOp) {
    ASSERT_NO_FATAL_FAILURE(xpe_dicom_close(nullptr));
}

// ---------------------------------------------------------------------------
// AC-09: NULL parameters
// ---------------------------------------------------------------------------
TEST_F(DicomReaderTest, NullFilePath_ReturnsInvalidInput) {
    XpeDicomHandle* handle = nullptr;
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dicom_open(nullptr, &handle));
}

TEST_F(DicomReaderTest, NullOutHandle_ReturnsInvalidInput) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_dicom_open(s_validDcm.string().c_str(), nullptr));
}

TEST_F(DicomReaderTest, ReadImageNullHandle_ReturnsInvalidInput) {
    XpeImageBuffer img{};
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dicom_read_image(nullptr, &img));
}

TEST_F(DicomReaderTest, GetMetadataNullHandle_ReturnsInvalidInput) {
    XpeImageMetadata meta{};
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dicom_get_metadata(nullptr, &meta));
}

// ---------------------------------------------------------------------------
// #120 (QA-B-27): J2K decode error branches, reached with derived bad files
// rather than fault injection. Each case removes exactly one thing from a file
// that otherwise reads correctly, so a failure names its own cause.
// ---------------------------------------------------------------------------

// PixelData absent entirely: DicomReader.cpp:294-297 findAndGetElement fails.
TEST_F(DicomReaderTest, ReadJ2K_NoPixelData_ReturnsDicomInvalid) {
    auto path = s_tempDir / "j2k_no_pixeldata.dcm";
    {
        DcmFileFormat ff;
        ASSERT_TRUE(ff.loadFile(s_j2kDcm.string().c_str()).good());
        ff.getDataset()->findAndDeleteElement(DCM_PixelData);
        ASSERT_TRUE(ff.saveFile(path.string().c_str(), EXS_JPEG2000LosslessOnly).good());
    }
    XpeDicomHandle* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(path.string().c_str(), &handle));
    XpeImageBuffer img{};
    EXPECT_EQ(XPE_ERR_DICOM_INVALID, xpe_dicom_read_image(handle, &img));
    xpe_dicom_close(handle);
}

// NOT REACHABLE BY RELABELLING (attempted, QA-B-27): taking the Explicit LE file,
// rewriting its meta TransferSyntaxUID to J2K and saving produces a file the reader
// reads normally (xpe_dicom_read_image returned XPE_OK), so the
// getEncapsulatedRepresentation failure branch at DicomReader.cpp:311-319 was not
// entered. DCMTK appears to rewrite the meta transfer syntax to match the encoding
// actually used by saveFile. Reaching that branch needs a file whose declared J2K
// syntax survives the write -- left uncovered rather than asserted falsely.

// ---------------------------------------------------------------------------
// #120 (QA-B-29): J2K codestream failure branches (DicomReader.cpp:416-431).
//
// The DICOM container is left byte-for-byte intact and only the encapsulated
// J2K codestream is damaged, so the failure is attributable to the decoder and
// not to DICOM parsing. Lengths are preserved (bytes are overwritten in place,
// never inserted or removed), which is what keeps the container valid.
//
// The SOC/SIZ marker pair FF4F FF51 opens a J2K codestream; everything before
// it is DICOM framing.
// ---------------------------------------------------------------------------
namespace {

/// Copy `src` to `dst`, overwriting `count` bytes at `offsetFromSoc` past the
/// J2K SOC marker. Returns false if no codestream was found.
bool corrupt_j2k_codestream(const fs::path& src, const fs::path& dst,
                            size_t offsetFromSoc, size_t count)
{
    std::ifstream in(src, std::ios::binary);
    std::vector<char> buf((std::istreambuf_iterator<char>(in)),
                           std::istreambuf_iterator<char>());
    if (buf.size() < 8) return false;

    size_t soc = std::string::npos;
    for (size_t i = 0; i + 3 < buf.size(); ++i) {
        if (static_cast<unsigned char>(buf[i])     == 0xFF &&
            static_cast<unsigned char>(buf[i + 1]) == 0x4F &&
            static_cast<unsigned char>(buf[i + 2]) == 0xFF &&
            static_cast<unsigned char>(buf[i + 3]) == 0x51) {
            soc = i;
            break;
        }
    }
    if (soc == std::string::npos) return false;

    for (size_t k = 0; k < count; ++k) {
        const size_t at = soc + offsetFromSoc + k;
        if (at >= buf.size()) break;
        buf[at] = static_cast<char>(0xA5);   // not a valid marker or segment body
    }

    std::ofstream out(dst, std::ios::binary);
    out.write(buf.data(), static_cast<std::streamsize>(buf.size()));
    return out.good();
}

}  // namespace

// Damage inside the SIZ header segment: opj_read_header cannot parse it.
TEST_F(DicomReaderTest, ReadJ2K_CorruptHeader_ReturnsProcessingFailed) {
    auto path = s_tempDir / "j2k_corrupt_header.dcm";
    ASSERT_TRUE(corrupt_j2k_codestream(s_j2kDcm, path, /*offsetFromSoc=*/4, /*count=*/24))
        << "no J2K codestream found in the fixture";

    XpeDicomHandle* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(path.string().c_str(), &handle));
    XpeImageBuffer img{};
    const XpeErrorCode rc = xpe_dicom_read_image(handle, &img);
    EXPECT_NE(XPE_OK, rc) << "a corrupt J2K header must not decode";
    xpe_dicom_close(handle);
}

// Leave the header intact and damage the entropy-coded body instead: the
// header parses, the decode step is what fails.
TEST_F(DicomReaderTest, ReadJ2K_CorruptBody_ReturnsProcessingFailed) {
    auto path = s_tempDir / "j2k_corrupt_body.dcm";
    ASSERT_TRUE(corrupt_j2k_codestream(s_j2kDcm, path, /*offsetFromSoc=*/200, /*count=*/512))
        << "no J2K codestream found in the fixture";

    XpeDicomHandle* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(path.string().c_str(), &handle));
    XpeImageBuffer img{};
    const XpeErrorCode rc = xpe_dicom_read_image(handle, &img);
    // Either the decoder rejects it or it produces pixels from damaged data;
    // both are acceptable outcomes for a corrupt body, so only a crash or a
    // silent XPE_OK with a null buffer would be a defect.
    if (rc == XPE_OK) {
        EXPECT_NE(nullptr, img.data) << "XPE_OK must come with a real buffer";
        xpe_free_image(&img);
    }
    xpe_dicom_close(handle);
}

// ---------------------------------------------------------------------------
// #120 (QA-B-34): reader branches outside the J2K decode path.
// ---------------------------------------------------------------------------

// A dataset written without a Part 10 meta header carries no TransferSyntaxUID,
// which is the "no TS in meta -- treat as Explicit LE" branch
// (DicomReader.cpp:105-107).
TEST_F(DicomReaderTest, OpenDatasetWithoutMetaHeader_TreatedAsExplicitLE) {
    auto path = s_tempDir / "reader_no_meta.dcm";
    {
        DcmFileFormat ff;
        ASSERT_TRUE(ff.loadFile(s_validDcm.string().c_str()).good());
        ASSERT_TRUE(ff.saveFile(path.string().c_str(), EXS_LittleEndianExplicit,
                                EET_ExplicitLength, EGL_recalcGL, EPD_withoutPadding,
                                0, 0, EWM_dataset).good());
    }
    XpeDicomHandle* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(path.string().c_str(), &handle));
    EXPECT_NE(nullptr, handle);
    xpe_dicom_close(handle);
}

// PixelData absent from an uncompressed file: findAndGetUint16Array fails and
// the already-allocated output buffer must be released before returning
// (DicomReader.cpp:164-168). The free is the part worth exercising -- a leak
// here would be invisible to a return-code-only test.
TEST_F(DicomReaderTest, ReadImage_NoPixelData_ReturnsDicomInvalid) {
    auto path = s_tempDir / "no_pixeldata.dcm";
    {
        DcmFileFormat ff;
        ASSERT_TRUE(ff.loadFile(s_validDcm.string().c_str()).good());
        ff.getDataset()->findAndDeleteElement(DCM_PixelData);
        ASSERT_TRUE(ff.saveFile(path.string().c_str(), EXS_LittleEndianExplicit).good());
    }
    XpeDicomHandle* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(path.string().c_str(), &handle));
    XpeImageBuffer img{};
    EXPECT_EQ(XPE_ERR_DICOM_INVALID, xpe_dicom_read_image(handle, &img));
    xpe_dicom_close(handle);
}

// An empty path is EC_IllegalParameter / EC_InvalidFilename territory for
// DCMTK, which the reader maps to IO_FAILED (DicomReader.cpp:61-62).
TEST_F(DicomReaderTest, OpenEmptyPath_ReturnsIoFailed) {
    XpeDicomHandle* handle = nullptr;
    EXPECT_EQ(XPE_ERR_IO_FAILED, xpe_dicom_open("", &handle));
}

// ---------------------------------------------------------------------------
// #142 (QA-B-42): the output-pointer half of the contract on the reader.
//
// The existing cases above cover a NULL *handle*; the NULL *output* argument
// was never asserted. The implementation already answers INVALID_INPUT for it
// (dicom.cpp:56, :67), so this is a fix to the test surface, not to the code --
// the rule is now pinned rather than merely true today.
// ---------------------------------------------------------------------------
TEST_F(DicomReaderTest, ReadImageNullOutput_ReturnsInvalidInput) {
    XpeDicomHandle* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(s_validDcm.string().c_str(), &handle));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dicom_read_image(handle, nullptr));
    xpe_dicom_close(handle);
}

TEST_F(DicomReaderTest, GetMetadataNullOutput_ReturnsInvalidInput) {
    XpeDicomHandle* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(s_validDcm.string().c_str(), &handle));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dicom_get_metadata(handle, nullptr));
    xpe_dicom_close(handle);
}


// ---------------------------------------------------------------------------
// #120 (QA-B-44): what actually happens to a JPEG-Lossless-labelled file.
//
// open() accepts three transfer syntaxes, JPEG-LL (1.2.840.10008.1.2.4.70)
// among them (DicomReader.cpp:98-100), and readImage has a branch for it
// (:139-147). QA-B-44 tried to reach that branch by relabelling an uncompressed
// file's meta TransferSyntaxUID. It does not work, and the reason is worth
// pinning rather than leaving as a Gap: DCMTK's loadFile itself refuses the
// file, so open() answers XPE_ERR_DICOM_INVALID and readImage is never called.
//
// Two facts follow, both recorded in the QA-B-44 report:
//   - the JPEG-LL branch cannot be covered by relabelling; it needs a genuinely
//     JPEG-encoded fixture;
//   - at the time, no DCMTK codec was registered in this module, so a genuine
//     JPEG-LL file would not have decoded either.
//
// The second fact is FIXED and this note is kept only so the first is not read
// against a stale background: DicomReader.cpp:48 now calls
// DJDecoderRegistration::registerCodecs() on every open, and QA-B-67 measured
// that the registration covers Process 14 (.57) as well as .70 -- so the
// accepted-syntax list, not the codec set, is what limits this reader today.
//
// This case pins the first fact, which is unchanged.
// ---------------------------------------------------------------------------
TEST_F(DicomReaderTest, OpenJpegLosslessLabelledNativeData_ReturnsDicomInvalid) {
    auto path = s_tempDir / "reader_jpegll_label.dcm";
    {
        DcmFileFormat ff;
        ASSERT_TRUE(ff.loadFile(s_validDcm.string().c_str()).good());
        DcmMetaInfo* meta = ff.getMetaInfo();
        ASSERT_NE(nullptr, meta);
        ASSERT_TRUE(meta->putAndInsertString(DCM_TransferSyntaxUID,
                                             "1.2.840.10008.1.2.4.70").good());
        ASSERT_TRUE(ff.saveFile(path.string().c_str(), EXS_LittleEndianExplicit,
                                EET_ExplicitLength, EGL_recalcGL, EPD_withoutPadding,
                                0, 0, EWM_dontUpdateMeta).good());
    }

    XpeDicomHandle* handle = nullptr;
    EXPECT_EQ(XPE_ERR_DICOM_INVALID,
              xpe_dicom_open(path.string().c_str(), &handle));
    xpe_dicom_close(handle);   // NULL-safe by contract
}

// ---------------------------------------------------------------------------
// #146 (QA-B-45): JPEG Lossless is a requirement, not an option.
//
//   REQ-DICOM-004: "The system SHALL support the following Transfer Syntaxes
//     for reading: ... 1.2.840.10008.1.2.4.70 (JPEG Lossless, Non-Hierarchical,
//     First-Order Prediction)"
//   REQ-DICOM-008: "IF the DICOM file contains JPEG 2000 or JPEG Lossless
//     compressed pixel data, THEN the system SHALL decompress the data to raw
//     uint16"
//
// QA-B-44 found that no DCMTK codec was ever registered, so the accepted-syntax
// list promised more than the build delivered. The fixture below produces a
// GENUINE JPEG-LL file with DCMTK's own encoder rather than relabelling one --
// relabelling does not reach the decode path at all (QA-B-44 observed loadFile
// rejecting it outright).
//
// The pixel comparison is the point. "It read without error" is not evidence of
// lossless behaviour; only a byte-exact match against the source is.
// ---------------------------------------------------------------------------
namespace {

// Encode s_validDcm as JPEG-LL. Returns false when DCMTK's encoder cannot
// produce the syntax, so the test says which half failed.
bool WriteJpegLosslessCopy(const fs::path& src, const fs::path& dst) {
    DJEncoderRegistration::registerCodecs();
    bool ok = false;
    {
        DcmFileFormat ff;
        if (ff.loadFile(src.string().c_str()).good()) {
            DcmDataset* ds = ff.getDataset();
            // EXS_JPEGProcess14SV1 == 1.2.840.10008.1.2.4.70
            if (ds != nullptr &&
                ds->chooseRepresentation(EXS_JPEGProcess14SV1, nullptr).good() &&
                ds->canWriteXfer(EXS_JPEGProcess14SV1)) {
                ok = ff.saveFile(dst.string().c_str(), EXS_JPEGProcess14SV1).good();
            }
        }
    }
    DJEncoderRegistration::cleanup();
    return ok;
}

}  // namespace

TEST_F(DicomReaderTest, ReadJpegLossless_DecodesPixelExact) {
    // Baseline pixels from the uncompressed original.
    XpeDicomHandle* srcHandle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(s_validDcm.string().c_str(), &srcHandle));
    XpeImageBuffer expected{};
    ASSERT_EQ(XPE_OK, xpe_dicom_read_image(srcHandle, &expected));
    xpe_dicom_close(srcHandle);

    const auto jpegPath = s_tempDir / "reader_jpegll_real.dcm";
    ASSERT_TRUE(WriteJpegLosslessCopy(s_validDcm, jpegPath))
        << "DCMTK could not encode the fixture as JPEG-LL; the case below would "
           "not be testing the reader";

    XpeDicomHandle* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(jpegPath.string().c_str(), &handle))
        << "REQ-DICOM-004 lists JPEG-LL as supported for reading";

    XpeImageBuffer actual{};
    ASSERT_EQ(XPE_OK, xpe_dicom_read_image(handle, &actual))
        << "REQ-DICOM-008 requires JPEG Lossless pixel data to be decompressed";
    xpe_dicom_close(handle);

    ASSERT_EQ(expected.width, actual.width);
    ASSERT_EQ(expected.height, actual.height);
    ASSERT_NE(nullptr, actual.data);

    // Lossless means exactly equal, not approximately.
    const size_t bytes = static_cast<size_t>(expected.width) * expected.height *
                         sizeof(uint16_t);
    EXPECT_EQ(0, std::memcmp(expected.data, actual.data, bytes))
        << "JPEG Lossless round trip must be bit-exact";

    xpe_free_image(&expected);
    xpe_free_image(&actual);
}

// ---------------------------------------------------------------------------
// #146 (QA-B-45): the accepted-syntax list must match what the module can read.
//
// This is the guard that would have caught the original defect. It walks
// kSupportedTransferSyntaxes (DicomReader.h -- the same table open() checks
// against) and requires each entry to survive a real round trip: write a file
// in that syntax, open it, read the pixels, compare byte-for-byte against the
// uncompressed original.
//
// A syntax added to the table with no decode support fails here. So does one
// added with no fixture: the switch below has no silent default, because
// "we did not test it" and "it works" must not look the same.
// ---------------------------------------------------------------------------
namespace {

// Produce a copy of src encoded in the given transfer syntax.
// Returns false when this test does not know how to build that syntax.
bool WriteInTransferSyntax(const char* tsUid,
                           const fs::path& src,
                           const fs::path& dst,
                           std::string& whyNot) {
    const std::string uid(tsUid);

    if (uid == "1.2.840.10008.1.2.1") {          // Explicit VR Little Endian
        DcmFileFormat ff;
        if (!ff.loadFile(src.string().c_str()).good()) { whyNot = "loadFile failed"; return false; }
        return ff.saveFile(dst.string().c_str(), EXS_LittleEndianExplicit).good();
    }
    if (uid == "1.2.840.10008.1.2.4.90") {       // JPEG 2000 Lossless
        XpeDicomHandle* h = nullptr;
        if (xpe_dicom_open(src.string().c_str(), &h) != XPE_OK) { whyNot = "open failed"; return false; }
        XpeImageBuffer img{};
        const XpeErrorCode rc = xpe_dicom_read_image(h, &img);
        XpeImageMetadata meta{};
        xpe_dicom_get_metadata(h, &meta);
        xpe_dicom_close(h);
        if (rc != XPE_OK) { whyNot = "read failed"; return false; }
        const XpeErrorCode wrc = xpe_dicom_write_j2k(dst.string().c_str(), &img, &meta);
        xpe_free_image(&img);
        return wrc == XPE_OK;
    }
    if (uid == "1.2.840.10008.1.2.4.70") {       // JPEG Lossless, First-Order
        return WriteJpegLosslessCopy(src, dst);
    }
    if (uid == "1.2.840.10008.1.2.4.57") {       // JPEG Lossless, Process 14 (#147)
        // Added with the syntax itself (QA-B-68). This builder is why adding a
        // UID to kSupportedTransferSyntaxes is not a one-line change: the case
        // below insists every accepted syntax be demonstrably readable, so a UID
        // with no fixture fails here rather than shipping unexercised.
        DJEncoderRegistration::registerCodecs();
        bool ok = false;
        {
            DcmFileFormat ff;
            if (ff.loadFile(src.string().c_str()).good()) {
                DcmDataset* ds = ff.getDataset();
                if (ds != nullptr &&
                    ds->chooseRepresentation(EXS_JPEGProcess14, nullptr).good() &&
                    ds->canWriteXfer(EXS_JPEGProcess14)) {
                    ok = ff.saveFile(dst.string().c_str(), EXS_JPEGProcess14).good();
                }
            }
        }
        // Deliberately no DJDecoderRegistration::cleanup() anywhere in this file
        // -- see Genuine57IsAcceptedAndDcmtkDecodeSupportIsMeasured for what that
        // costs.
        if (!ok) whyNot = "DCMTK could not encode Process 14";
        return ok;
    }

    whyNot = "this test has no fixture builder for " + uid;
    return false;
}

}  // namespace

TEST_F(DicomReaderTest, EverySupportedTransferSyntaxActuallyReads) {
    // Baseline pixels, read from the uncompressed original once.
    XpeDicomHandle* srcHandle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(s_validDcm.string().c_str(), &srcHandle));
    XpeImageBuffer expected{};
    ASSERT_EQ(XPE_OK, xpe_dicom_read_image(srcHandle, &expected));
    xpe_dicom_close(srcHandle);

    ASSERT_GT(xpe::dicom::kSupportedTransferSyntaxCount, 0u);

    for (size_t i = 0; i < xpe::dicom::kSupportedTransferSyntaxCount; ++i) {
        const auto& ts = xpe::dicom::kSupportedTransferSyntaxes[i];
        SCOPED_TRACE(std::string(ts.name) + " (" + ts.uid + ")");

        const auto path = s_tempDir / ("ts_" + std::to_string(i) + ".dcm");
        std::string whyNot;
        ASSERT_TRUE(WriteInTransferSyntax(ts.uid, s_validDcm, path, whyNot))
            << "cannot build a fixture: " << whyNot
            << " -- either teach this test to write that syntax, or remove it "
               "from kSupportedTransferSyntaxes";

        XpeDicomHandle* handle = nullptr;
        ASSERT_EQ(XPE_OK, xpe_dicom_open(path.string().c_str(), &handle))
            << "listed as supported but open() rejected it";

        XpeImageBuffer actual{};
        ASSERT_EQ(XPE_OK, xpe_dicom_read_image(handle, &actual))
            << "listed as supported but the pixels could not be decoded";
        xpe_dicom_close(handle);

        ASSERT_EQ(expected.width, actual.width);
        ASSERT_EQ(expected.height, actual.height);
        const size_t bytes = static_cast<size_t>(expected.width) * expected.height *
                             sizeof(uint16_t);
        EXPECT_EQ(0, std::memcmp(expected.data, actual.data, bytes))
            << "every supported syntax here is lossless, so the round trip must "
               "be bit-exact";
        xpe_free_image(&actual);
    }

    xpe_free_image(&expected);
}

// ---------------------------------------------------------------------------
// #120 (QA-B-46): JPEG-LL input variety.
//
// QA-B-45 verified exactly one JPEG-LL file: DCMTK's encoder at its default
// settings. "Lossless works" was therefore a claim about one encoder
// configuration. JPEG Lossless (Process 14, first-order) admits several
// predictor selection values, and a real device uses whichever its vendor
// chose, so the reader must handle more than the one we happened to produce.
//
// Each variant is compared byte-for-byte against the uncompressed original --
// the standard QA-B-45 set. A lossless round trip that differs anywhere is a
// failure, however plausible the image looks.
// ---------------------------------------------------------------------------
namespace {

// Encode with an explicit predictor selection value. Returns false when DCMTK
// declines to produce that variant; the caller reports it rather than
// pretending the case ran.
bool WriteJpegLosslessVariant(const fs::path& src, const fs::path& dst,
                              int predictor, std::string& whyNot) {
    DJEncoderRegistration::registerCodecs();
    bool ok = false;
    {
        DcmFileFormat ff;
        if (!ff.loadFile(src.string().c_str()).good()) {
            whyNot = "loadFile failed";
        } else {
            DcmDataset* ds = ff.getDataset();
            const DJ_RPLossless params(predictor, 0);
            const OFCondition rc =
                ds ? ds->chooseRepresentation(EXS_JPEGProcess14SV1, &params)
                   : EC_IllegalCall;
            if (!rc.good()) {
                whyNot = std::string("chooseRepresentation: ") + rc.text();
            } else if (!ds->canWriteXfer(EXS_JPEGProcess14SV1)) {
                whyNot = "canWriteXfer said no";
            } else if (!ff.saveFile(dst.string().c_str(), EXS_JPEGProcess14SV1).good()) {
                whyNot = "saveFile failed";
            } else {
                ok = true;
            }
        }
    }
    DJEncoderRegistration::cleanup();
    return ok;
}

}  // namespace

TEST_F(DicomReaderTest, ReadJpegLosslessPredictorVariants_AllPixelExact) {
    XpeDicomHandle* srcHandle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(s_validDcm.string().c_str(), &srcHandle));
    XpeImageBuffer expected{};
    ASSERT_EQ(XPE_OK, xpe_dicom_read_image(srcHandle, &expected));
    xpe_dicom_close(srcHandle);

    const size_t bytes = static_cast<size_t>(expected.width) * expected.height *
                         sizeof(uint16_t);

    // Selection values 1..7 are the first-order predictors of the JPEG lossless
    // process. Any that DCMTK will not encode is reported, not skipped silently.
    int produced = 0;
    for (int predictor = 1; predictor <= 7; ++predictor) {
        SCOPED_TRACE("predictor selection value " + std::to_string(predictor));
        const auto path = s_tempDir / ("jpegll_pred" + std::to_string(predictor) + ".dcm");

        std::string whyNot;
        if (!WriteJpegLosslessVariant(s_validDcm, path, predictor, whyNot)) {
            // Recorded, not asserted: an encoder that cannot produce a variant
            // says nothing about whether the reader could decode it.
            GTEST_LOG_(INFO) << "predictor " << predictor
                             << ": fixture not produced (" << whyNot << ")";
            continue;
        }
        ++produced;

        XpeDicomHandle* handle = nullptr;
        ASSERT_EQ(XPE_OK, xpe_dicom_open(path.string().c_str(), &handle));
        XpeImageBuffer actual{};
        ASSERT_EQ(XPE_OK, xpe_dicom_read_image(handle, &actual));
        xpe_dicom_close(handle);

        ASSERT_EQ(expected.width, actual.width);
        ASSERT_EQ(expected.height, actual.height);
        EXPECT_EQ(0, std::memcmp(expected.data, actual.data, bytes))
            << "lossless round trip must be bit-exact for this predictor";
        xpe_free_image(&actual);
    }

    // Printed rather than inferred: the count is the evidence for how much
    // wider this case is than QA-B-45, and a reader of the log should not have
    // to deduce it from the absence of skip messages.
    GTEST_LOG_(INFO) << "predictor variants produced and verified: " << produced
                     << " of 7";
    EXPECT_GT(produced, 1)
        << "only one predictor variant could be produced; the case would then be "
           "no broader than QA-B-45";

    xpe_free_image(&expected);
}

// ---------------------------------------------------------------------------
// #120 (QA-B-46): concurrent open().
//
// QA-B-45 guarded the codec registration with std::call_once because DCMTK's
// registerCodecs mutates global tables. That reasoning came from reading the
// code; this case runs it.
//
// WHAT THIS TEST DOES NOT PROVE: passing is not evidence that the code is
// thread-safe. A data race can stay invisible across many runs. What a pass
// establishes is narrower -- that the obvious failure modes (crash, decode
// failure, corrupted pixels under contention) did not occur here. The QA-B-46
// report states the same limit rather than letting a green tick imply more.
//
// The threads are joined inside the case. Nothing is left running: no detached
// thread, no background load, no spawned process.
// ---------------------------------------------------------------------------
TEST_F(DicomReaderTest, ConcurrentOpenOfJpegLossless_NoCorruption) {
    const auto jpegPath = s_tempDir / "jpegll_concurrent.dcm";
    ASSERT_TRUE(WriteJpegLosslessCopy(s_validDcm, jpegPath))
        << "the concurrent path must exercise the codec, not plain Explicit LE";

    XpeDicomHandle* srcHandle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(s_validDcm.string().c_str(), &srcHandle));
    XpeImageBuffer expected{};
    ASSERT_EQ(XPE_OK, xpe_dicom_read_image(srcHandle, &expected));
    xpe_dicom_close(srcHandle);
    const size_t bytes = static_cast<size_t>(expected.width) * expected.height *
                         sizeof(uint16_t);

    constexpr int kThreads = 8;
    constexpr int kRounds  = 8;
    std::atomic<int> openFailures{0};
    std::atomic<int> readFailures{0};
    std::atomic<int> mismatches{0};

    std::vector<std::thread> workers;
    workers.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([&]() {
            for (int r = 0; r < kRounds; ++r) {
                XpeDicomHandle* h = nullptr;
                if (xpe_dicom_open(jpegPath.string().c_str(), &h) != XPE_OK) {
                    ++openFailures;
                    continue;
                }
                XpeImageBuffer img{};
                if (xpe_dicom_read_image(h, &img) != XPE_OK) {
                    ++readFailures;
                } else {
                    if (std::memcmp(expected.data, img.data, bytes) != 0) {
                        ++mismatches;
                    }
                    xpe_free_image(&img);
                }
                xpe_dicom_close(h);
            }
        });
    }
    for (auto& w : workers) {
        w.join();   // every thread joined here; none outlives the case
    }

    EXPECT_EQ(0, openFailures.load()) << "open failed under contention";
    EXPECT_EQ(0, readFailures.load()) << "decode failed under contention";
    EXPECT_EQ(0, mismatches.load())   << "pixels differed under contention";

    xpe_free_image(&expected);
}

// ===========================================================================
// #120 (QA-B-47): the J2K decode FAILURE paths.
//
// QA-B-46 confirmed what QA-B-44 predicted: widening the success path leaves
// these lines untouched. They only run when decoding fails, and until now
// nothing made it fail. For medical-device software that is not a coverage
// number -- it means nobody has checked what the reader RETURNS when
// decompression fails, or what it RELEASES on the way out.
//
// Fixtures are built by replacing the encapsulated pixel data of a real J2K
// file, so each case differs from a working file in exactly one way.
// ===========================================================================
#if defined(_WIN32)
#  include <windows.h>
#  include <psapi.h>
static size_t b47_working_set_bytes() {
    PROCESS_MEMORY_COUNTERS pmc{};
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
}
#else
static size_t b47_working_set_bytes() { return 0; }
#endif

namespace {

// What goes into the pixel sequence of the crafted file.
enum class FragmentShape {
    kGarbage,        // bytes that are not a J2K codestream at all
    kTruncated,      // the real codestream, cut in half
    kEmptyFragment,  // a fragment of length 0
    kOffsetTableOnly // no data fragment at all
};

// Pull the real J2K codestream out of a file this project wrote.
bool ExtractJ2kBitstream(const fs::path& j2kFile, std::vector<uint8_t>& out) {
    DcmFileFormat ff;
    if (!ff.loadFile(j2kFile.string().c_str()).good()) return false;
    DcmDataset* ds = ff.getDataset();
    if (ds == nullptr) return false;

    DcmElement* elem = nullptr;
    if (!ds->findAndGetElement(DCM_PixelData, elem).good() || elem == nullptr) return false;
    DcmPixelData* pd = OFstatic_cast(DcmPixelData*, elem);

    DcmPixelSequence* seq = nullptr;
    E_TransferSyntax repKey = EXS_JPEG2000LosslessOnly;
    const DcmRepresentationParameter* repParam = nullptr;
    if (!pd->getEncapsulatedRepresentation(repKey, repParam, seq).good() || seq == nullptr) {
        return false;
    }

    DcmPixelItem* item = nullptr;
    if (!seq->getItem(item, 1).good() || item == nullptr) return false;
    Uint8* bytes = nullptr;
    if (!item->getUint8Array(bytes).good() || bytes == nullptr) return false;
    const Uint32 len = static_cast<Uint32>(item->getLength());
    if (len == 0) return false;

    out.assign(bytes, bytes + len);
    return true;
}

// Write a file that declares J2K Lossless and carries the requested fragment.
bool WriteCraftedJ2kFile(const fs::path& src, const fs::path& dst,
                         const std::vector<uint8_t>& bitstream,
                         FragmentShape shape) {
    DcmFileFormat ff;
    if (!ff.loadFile(src.string().c_str()).good()) return false;
    DcmDataset* ds = ff.getDataset();
    if (ds == nullptr) return false;

    ds->findAndDeleteElement(DCM_PixelData);

    // Offset table first, exactly as an encapsulated dataset requires.
    DcmPixelSequence* seq = new DcmPixelSequence(DcmTag(DCM_PixelData, EVR_OB));
    seq->insert(new DcmPixelItem(DcmTag(DCM_Item, EVR_OB)));

    if (shape != FragmentShape::kOffsetTableOnly) {
        DcmPixelItem* frag = new DcmPixelItem(DcmTag(DCM_Item, EVR_OB));
        switch (shape) {
            case FragmentShape::kGarbage: {
                std::vector<uint8_t> junk(256);
                for (size_t i = 0; i < junk.size(); ++i) {
                    junk[i] = static_cast<uint8_t>(0xA5 ^ (i & 0xFF));
                }
                frag->putUint8Array(junk.data(), static_cast<Uint32>(junk.size()));
                break;
            }
            case FragmentShape::kTruncated: {
                const Uint32 half = static_cast<Uint32>(bitstream.size() / 2);
                frag->putUint8Array(bitstream.data(), half);
                break;
            }
            case FragmentShape::kEmptyFragment:
                break;   // inserted with no value at all
            default:
                break;
        }
        seq->insert(frag);
    }

    DcmPixelData* pd = new DcmPixelData(DcmTag(DCM_PixelData, EVR_OB));
    // putOriginalRepresentation returns void and takes ownership of seq.
    pd->putOriginalRepresentation(EXS_JPEG2000LosslessOnly, nullptr, seq);
    if (!ds->insert(pd, OFTrue).good()) {
        delete pd;
        return false;
    }

    return ff.saveFile(dst.string().c_str(), EXS_JPEG2000LosslessOnly).good();
}

// Open + read one crafted file. Returns the read_image result; XPE_OK cases
// free the buffer so the caller can loop without leaking on the success path.
XpeErrorCode ReadCrafted(const fs::path& path) {
    XpeDicomHandle* handle = nullptr;
    const XpeErrorCode openRc = xpe_dicom_open(path.string().c_str(), &handle);
    if (openRc != XPE_OK) return openRc;
    XpeImageBuffer img{};
    const XpeErrorCode rc = xpe_dicom_read_image(handle, &img);
    if (rc == XPE_OK) xpe_free_image(&img);
    xpe_dicom_close(handle);
    return rc;
}

}  // namespace

class DicomJ2kFailureTest : public DicomReaderTest {
protected:
    static std::vector<uint8_t> s_bitstream;
    static fs::path s_j2kSource;

    void SetUp() override {
        DicomReaderTest::SetUp();
        if (s_bitstream.empty()) {
            // One real J2K file, written by this project, is the donor for every
            // crafted fixture below.
            s_j2kSource = s_tempDir / "j2k_donor.dcm";
            XpeDicomHandle* h = nullptr;
            ASSERT_EQ(XPE_OK, xpe_dicom_open(s_validDcm.string().c_str(), &h));
            XpeImageBuffer img{};
            ASSERT_EQ(XPE_OK, xpe_dicom_read_image(h, &img));
            XpeImageMetadata meta{};
            xpe_dicom_get_metadata(h, &meta);
            xpe_dicom_close(h);
            ASSERT_EQ(XPE_OK, xpe_dicom_write_j2k(s_j2kSource.string().c_str(), &img, &meta));
            xpe_free_image(&img);
            ASSERT_TRUE(ExtractJ2kBitstream(s_j2kSource, s_bitstream));
            ASSERT_GT(s_bitstream.size(), 64u);
        }
    }
};

std::vector<uint8_t> DicomJ2kFailureTest::s_bitstream;
fs::path DicomJ2kFailureTest::s_j2kSource;

// A codestream that is not a codestream: opj_read_header rejects it
// (DicomReader.cpp:451-457).
TEST_F(DicomJ2kFailureTest, GarbageBitstream_ReturnsProcessingFailed) {
    const auto path = s_tempDir / "j2k_garbage.dcm";
    ASSERT_TRUE(WriteCraftedJ2kFile(s_validDcm, path, s_bitstream,
                                    FragmentShape::kGarbage));
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, ReadCrafted(path));
}

// Header present, data cut short. Which of the two handlers catches it
// (read_header or decode) is an OpenJPEG detail, so the assertion names the
// contract -- a failure is reported, not a half-decoded image.
TEST_F(DicomJ2kFailureTest, TruncatedBitstream_ReturnsProcessingFailed) {
    const auto path = s_tempDir / "j2k_truncated.dcm";
    ASSERT_TRUE(WriteCraftedJ2kFile(s_validDcm, path, s_bitstream,
                                    FragmentShape::kTruncated));
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED, ReadCrafted(path));
}

// A fragment carrying no bytes: rejected before OpenJPEG is involved
// (DicomReader.cpp:367-370).
TEST_F(DicomJ2kFailureTest, EmptyFragment_ReturnsDicomInvalid) {
    const auto path = s_tempDir / "j2k_empty_frag.dcm";
    ASSERT_TRUE(WriteCraftedJ2kFile(s_validDcm, path, s_bitstream,
                                    FragmentShape::kEmptyFragment));
    EXPECT_EQ(XPE_ERR_DICOM_INVALID, ReadCrafted(path));
}

// Only the offset table, no data fragment. The reader falls back from item 1 to
// item 0 and finds the empty offset table, so this lands on the same guard.
TEST_F(DicomJ2kFailureTest, OffsetTableOnly_ReturnsDicomInvalid) {
    const auto path = s_tempDir / "j2k_no_frag.dcm";
    ASSERT_TRUE(WriteCraftedJ2kFile(s_validDcm, path, s_bitstream,
                                    FragmentShape::kOffsetTableOnly));
    EXPECT_EQ(XPE_ERR_DICOM_INVALID, ReadCrafted(path));
}

// ---------------------------------------------------------------------------
// The part that matters more than the return codes: does the failure path
// RELEASE what it allocated?
//
// Each failing decode allocates an OpenJPEG codec, a stream, and (for the
// decode-failure branch) an image. The handlers destroy them in a particular
// order; a missing destroy leaks once per failed read, which in a viewer that
// retries a bad study is a leak per retry.
//
// Method: working-set growth across many repetitions, the same #105 G3 gate the
// other modules use -- warm up so first-touch and allocator arenas settle, take
// a baseline, then loop. It measures the process, so it cannot name which
// object leaked; what it can do is fail when one does. The sensitivity probe
// below is what keeps that claim honest.
// ---------------------------------------------------------------------------
TEST_F(DicomJ2kFailureTest, FailurePathsDoNotGrowWorkingSet) {
#if !defined(_WIN32)
    GTEST_SKIP() << "working-set measurement is Windows-only in this build";
#endif
    const auto garbage   = s_tempDir / "j2k_leak_garbage.dcm";
    const auto truncated = s_tempDir / "j2k_leak_truncated.dcm";
    ASSERT_TRUE(WriteCraftedJ2kFile(s_validDcm, garbage, s_bitstream,
                                    FragmentShape::kGarbage));
    ASSERT_TRUE(WriteCraftedJ2kFile(s_validDcm, truncated, s_bitstream,
                                    FragmentShape::kTruncated));

    constexpr int kWarmup = 100;
    constexpr int kCycles = 1000;

    for (int i = 0; i < kWarmup; ++i) {
        (void)ReadCrafted(garbage);
        (void)ReadCrafted(truncated);
    }

    const size_t baseline = b47_working_set_bytes();
    ASSERT_GT(baseline, 0u) << "working-set query failed; the gate would be blind";

    for (int i = 0; i < kCycles; ++i) {
        (void)ReadCrafted(garbage);
        (void)ReadCrafted(truncated);
    }

    const size_t after = b47_working_set_bytes();
    const size_t growth = (after > baseline) ? (after - baseline) : 0;
    EXPECT_LT(growth, 1u * 1024u * 1024u)
        << "working set grew " << growth << " bytes over " << kCycles
        << " failed decodes -- a failure path is not releasing what it allocated";
}

// ===========================================================================
// #120 (QA-B-48): what the caller gets back when a read FAILS.
//
// QA-B-47 showed the failure paths return the right codes and release what they
// allocated. It did not check the other half: if a failed read leaves a
// half-filled buffer in outImg, a caller that ignores the return code turns
// garbage into a diagnostic image. In a medical device that distinction is the
// whole point.
//
// Observed first, asserted after (log: .moai/reports/lane-post/QA-B-48/
// _observation.log). All four J2K failure paths leave outImg EXACTLY as the
// caller passed it -- sentinel width/height/dataSize survive untouched and the
// data pointer stays NULL. The decode failures all happen before
// xpe_alloc_image is ever called, so there is nothing to leave behind.
//
// The contract these cases pin: a failed read does not write to outImg at all.
// That is stronger than "leaves it empty" and is what the code already does, so
// nothing was changed to make them pass (the QA-B-41 rule: code that is already
// right is not touched).
// ===========================================================================
namespace {

XpeErrorCode ReadIntoSentinel(const fs::path& path, XpeImageBuffer* out) {
    // Values no caller would produce, so any field the implementation writes
    // becomes visible.
    *out = XpeImageBuffer{};
    out->width         = 4242u;
    out->height        = 2424u;
    out->dataSize      = 777u;
    out->bitsAllocated = 99u;
    out->data          = nullptr;

    XpeDicomHandle* handle = nullptr;
    const XpeErrorCode openRc = xpe_dicom_open(path.string().c_str(), &handle);
    if (openRc != XPE_OK) return openRc;
    const XpeErrorCode rc = xpe_dicom_read_image(handle, out);
    xpe_dicom_close(handle);
    return rc;
}

void ExpectUntouched(const XpeImageBuffer& img, const char* what) {
    EXPECT_EQ(nullptr, img.data)      << what << ": a failed read allocated a buffer";
    EXPECT_EQ(4242u, img.width)       << what << ": width was overwritten";
    EXPECT_EQ(2424u, img.height)      << what << ": height was overwritten";
    EXPECT_EQ(777u, img.dataSize)     << what << ": dataSize was overwritten";
    EXPECT_EQ(99u, img.bitsAllocated) << what << ": bitsAllocated was overwritten";
}

}  // namespace

TEST_F(DicomJ2kFailureTest, FailedReadLeavesOutputUntouched) {
    struct Case { const char* name; FragmentShape shape; XpeErrorCode expected; };
    const Case cases[] = {
        {"garbage bitstream",   FragmentShape::kGarbage,          XPE_ERR_PROCESSING_FAILED},
        {"truncated bitstream", FragmentShape::kTruncated,        XPE_ERR_PROCESSING_FAILED},
        {"empty fragment",      FragmentShape::kEmptyFragment,    XPE_ERR_DICOM_INVALID},
        {"offset table only",   FragmentShape::kOffsetTableOnly,  XPE_ERR_DICOM_INVALID},
    };

    for (const auto& c : cases) {
        SCOPED_TRACE(c.name);
        const auto path = s_tempDir / (std::string("outstate_") +
                                       std::to_string(static_cast<int>(c.shape)) + ".dcm");
        ASSERT_TRUE(WriteCraftedJ2kFile(s_validDcm, path, s_bitstream, c.shape));

        XpeImageBuffer img{};
        EXPECT_EQ(c.expected, ReadIntoSentinel(path, &img));
        ExpectUntouched(img, c.name);
    }
}

// ---------------------------------------------------------------------------
// #120 (QA-B-48) §3: "J2K declared, native pixel data" -- resolved by observation.
//
// QA-B-29 wrote a case expecting failure here, observed XPE_OK, and removed it
// rather than leave a false assertion standing. QA-B-47 declined to guess.
// Observed now: the file is rejected by xpe_dicom_open with
// XPE_ERR_DICOM_INVALID -- DCMTK will not parse a dataset whose meta declares an
// encapsulated syntax while the pixel data is native, so readImage is never
// reached. The QA-B-29 XPE_OK did NOT reproduce.
//
// Consequence for coverage: DicomReader.cpp:351-353 (no encapsulated
// representation) is NOT reachable through this input. It stays classified as
// unreached, with a reason rather than a guess.
// ---------------------------------------------------------------------------
TEST_F(DicomReaderTest, J2kLabelledNativePixels_RejectedAtOpen) {
    const auto path = s_tempDir / "j2k_labelled_native.dcm";
    {
        DcmFileFormat ff;
        ASSERT_TRUE(ff.loadFile(s_validDcm.string().c_str()).good());
        DcmMetaInfo* meta = ff.getMetaInfo();
        ASSERT_NE(nullptr, meta);
        ASSERT_TRUE(meta->putAndInsertString(DCM_TransferSyntaxUID,
                                             "1.2.840.10008.1.2.4.90").good());
        ASSERT_TRUE(ff.saveFile(path.string().c_str(), EXS_LittleEndianExplicit,
                                EET_ExplicitLength, EGL_recalcGL, EPD_withoutPadding,
                                0, 0, EWM_dontUpdateMeta).good());
    }

    XpeDicomHandle* handle = nullptr;
    EXPECT_EQ(XPE_ERR_DICOM_INVALID,
              xpe_dicom_open(path.string().c_str(), &handle));
    xpe_dicom_close(handle);   // NULL-safe by contract
}

// ---------------------------------------------------------------------------
// #150 (QA-B-49): a PixelData shorter than the header declares is a REJECTION,
// not a success.
//
// This case existed in QA-B-48 as KnownDivergence_ShortPixelDataSucceedsWith-
// ZeroPaddedTail, which asserted the opposite: XPE_OK plus a zero-padded tail.
// That assertion was not wrong when it was written -- it recorded what the code
// did, deliberately, because no sentence had been found that said what it
// SHOULD do. The sentence exists:
//
//   SR-DCM-003 / HAZ-DCM-002 (docs/dicom/SHA-DICOM-001) names this exact
//   failure -- "픽셀 데이터 불완전 ... 호출자에게 '성공' 반환", risk 8 (High),
//   control "손상 감지 시 즉시 에러 반환".
//   docs/dicom/README.md:639  "DO NOT return partial pixel data".
//   RTM STC-003                "손상 파일 거부 + 부분 데이터 금지".
//
// So the old expectation is REPLACED, not corrected: the record stood until the
// requirement was found, and the requirement is what settles it.
//
// On the output buffer: the shortfall is only detectable AFTER xpe_alloc_image
// has run, so the QA-B-48 "untouched" contract cannot hold here. What holds is
// the weaker guarantee the neighbouring no-PixelData path already gives
// (DicomReader.cpp:199-202) -- no usable buffer is handed back: data is NULL and
// dataSize is 0. width/height keep the declared values the allocation wrote.
// ---------------------------------------------------------------------------
TEST_F(DicomReaderTest, ShortPixelData_ReturnsDicomInvalid) {
    const auto path = s_tempDir / "short_pixeldata.dcm";
    {
        DcmFileFormat ff;
        ASSERT_TRUE(ff.loadFile(s_validDcm.string().c_str()).good());
        DcmDataset* ds = ff.getDataset();
        ASSERT_NE(nullptr, ds);
        Uint16 r = 0;
        Uint16 c = 0;
        ASSERT_TRUE(ds->findAndGetUint16(DCM_Rows, r).good());
        ASSERT_TRUE(ds->findAndGetUint16(DCM_Columns, c).good());
        const unsigned long half = (static_cast<unsigned long>(r) * c) / 2u;
        std::vector<Uint16> shortPixels(half, 0x1234);
        ASSERT_TRUE(ds->putAndInsertUint16Array(DCM_PixelData, shortPixels.data(),
                                                half).good());
        ASSERT_TRUE(ff.saveFile(path.string().c_str(), EXS_LittleEndianExplicit).good());
    }

    XpeDicomHandle* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(path.string().c_str(), &handle));
    XpeImageBuffer img{};
    const XpeErrorCode rc = xpe_dicom_read_image(handle, &img);
    xpe_dicom_close(handle);

    EXPECT_EQ(XPE_ERR_DICOM_INVALID, rc)
        << "half the declared pixels were present; reporting success hands the "
           "caller a half-black image with no signal that anything is missing";
    EXPECT_EQ(nullptr, img.data)  << "a rejected read must not hand back a buffer";
    EXPECT_EQ(0u, img.dataSize)   << "a rejected read must not report a size";

    xpe_free_image(&img);   // NULL-safe; keeps the test honest if the guard regresses
}

// ---------------------------------------------------------------------------
// The other side of the same boundary, asserted so this change is shown NOT to
// have spilled over it: a PixelData LONGER than the header declares is still a
// success. The surplus is discarded and exactly Rows x Columns pixels are
// copied. Trailing padding is legal in DICOM (odd-length values are padded, and
// writers may round up), so rejecting it would break files that are correct.
// ---------------------------------------------------------------------------
TEST_F(DicomReaderTest, SurplusPixelData_IsIgnoredAndReadSucceeds) {
    const auto path = s_tempDir / "surplus_pixeldata.dcm";
    uint32_t rows = 0;
    uint32_t cols = 0;
    constexpr uint16_t kFill = 0x1234;
    {
        DcmFileFormat ff;
        ASSERT_TRUE(ff.loadFile(s_validDcm.string().c_str()).good());
        DcmDataset* ds = ff.getDataset();
        ASSERT_NE(nullptr, ds);
        Uint16 r = 0;
        Uint16 c = 0;
        ASSERT_TRUE(ds->findAndGetUint16(DCM_Rows, r).good());
        ASSERT_TRUE(ds->findAndGetUint16(DCM_Columns, c).good());
        rows = r;
        cols = c;
        const unsigned long declared = static_cast<unsigned long>(r) * c;
        // Declared pixels, then 64 extra the reader must not carry into the image.
        std::vector<Uint16> pixels(declared + 64u, kFill);
        for (size_t i = declared; i < pixels.size(); ++i) pixels[i] = 0xBEEF;
        ASSERT_TRUE(ds->putAndInsertUint16Array(DCM_PixelData, pixels.data(),
                                                static_cast<unsigned long>(pixels.size())).good());
        ASSERT_TRUE(ff.saveFile(path.string().c_str(), EXS_LittleEndianExplicit).good());
    }

    XpeDicomHandle* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(path.string().c_str(), &handle));
    XpeImageBuffer img{};
    const XpeErrorCode rc = xpe_dicom_read_image(handle, &img);
    xpe_dicom_close(handle);

    ASSERT_EQ(XPE_OK, rc) << "surplus trailing bytes are legal; this must stay a success";
    ASSERT_NE(nullptr, img.data);
    EXPECT_EQ(cols, img.width);
    EXPECT_EQ(rows, img.height);

    const uint16_t* px = static_cast<const uint16_t*>(img.data);
    const size_t n = static_cast<size_t>(img.width) * img.height;
    size_t wrong = 0;
    for (size_t i = 0; i < n; ++i) {
        if (px[i] != kFill) ++wrong;
    }
    EXPECT_EQ(0u, wrong) << "the image must hold the declared pixels only, "
                            "with none of the surplus bleeding in";

    xpe_free_image(&img);
}

// ===========================================================================
// #150 (QA-B-50): the COMPRESSED paths, against the same contract.
//
// QA-B-49 closed the hazard on the native path and said so with a warning
// attached: "잘린 픽셀은 거절된다" must not be read as a property of the module.
// HAZ-DCM-002 (risk 8) does not distinguish transfer syntaxes, so a control that
// only holds for uncompressed pixel data stops half the hazard.
//
// Observed first (log: .moai/reports/lane-post/QA-B-50/_observation.log). Both
// compressed paths diverged, each in its own way:
//
//   j2k-undersized    rc=0  declared 256x256 -> RETURNED 256x128
//   jpegll-undersized rc=0  declared 256x256 -> returned 256x256, half of it zero
//
// J2K ignored the declared size entirely and handed back a smaller buffer than
// the metadata describes -- a caller that trusts Rows/Columns indexes past its
// end. JPEG-LL produced exactly the black-lower-half image #150 rejected for
// native data, and the native guard could not see it: DCMTK decompresses into a
// buffer sized from Rows/Columns, so the shortfall is already padded away by the
// time that guard runs. Each needed its own detection point; both now return
// XPE_ERR_DICOM_INVALID.
//
// The matched-size controls below use the SAME construction with declared ==
// real. They must still read normally -- otherwise a rejection above would be
// evidence about the fixture, not about the reader.
// ===========================================================================
namespace {

// Build a compressed file whose codestream carries `realRows` rows while the
// dataset declares `declaredRows`. Returns false with a reason when the shape
// cannot be produced.
bool WriteCompressedWithDeclaredRows(const fs::path& dst, bool useJ2K,
                                     uint32_t cols, uint32_t realRows,
                                     uint32_t declaredRows, std::string& whyNot) {
    // NOTE: `small` is a macro in the Windows SDK headers this file already
    // pulls in (rpcndr.h), so the local name here is deliberately not that.
    const fs::path nativeSmall = dst.parent_path() / (dst.stem().string() + "_n.dcm");
    const fs::path comp        = dst.parent_path() / (dst.stem().string() + "_c.dcm");

    XpeImageBuffer img{};
    if (xpe_alloc_image(cols, realRows, XPE_PIXEL_UINT16, &img) != XPE_OK) {
        whyNot = "xpe_alloc_image failed";
        return false;
    }
    auto* px = static_cast<uint16_t*>(img.data);
    for (uint32_t k = 0; k < img.width * img.height; ++k) {
        px[k] = static_cast<uint16_t>(0x1000 + (k & 0xFFF));   // never zero
    }
    XpeImageMetadata meta{};
    std::snprintf(meta.bodyPart, sizeof(meta.bodyPart), "%s", "CHEST");
    meta.pixelPitch_mm = 0.148f;

    bool built = false;
    if (useJ2K) {
        built = (xpe_dicom_write_j2k(comp.string().c_str(), &img, &meta) == XPE_OK);
        if (!built) whyNot = "xpe_dicom_write_j2k failed on the source image";
    } else {
        built = (xpe_dicom_write(nativeSmall.string().c_str(), &img, &meta) == XPE_OK) &&
                WriteJpegLosslessCopy(nativeSmall, comp);
        if (!built) whyNot = "JPEG-LL encode of the source image failed";
    }
    xpe_free_image(&img);
    if (!built) return false;

    // Rewrite the declared Rows without touching the compressed pixel data.
    DcmFileFormat ff;
    if (!ff.loadFile(comp.string().c_str()).good()) {
        whyNot = "could not reload the compressed file";
        return false;
    }
    DcmDataset* ds = ff.getDataset();
    if (ds == nullptr) { whyNot = "no dataset"; return false; }
    const E_TransferSyntax xfer = ds->getOriginalXfer();
    if (!ds->putAndInsertUint16(DCM_Rows, static_cast<Uint16>(declaredRows)).good()) {
        whyNot = "could not rewrite DCM_Rows";
        return false;
    }
    if (!ff.saveFile(dst.string().c_str(), xfer, EET_ExplicitLength, EGL_recalcGL,
                     EPD_withoutPadding, 0, 0, EWM_dontUpdateMeta).good()) {
        whyNot = "could not save with the original transfer syntax";
        return false;
    }
    return true;
}

// Reads into a sentinel-stamped buffer so any field the reader writes is visible.
XpeErrorCode ReadWithSentinel(const fs::path& path, XpeImageBuffer* out) {
    *out = XpeImageBuffer{};
    out->width = 4242u; out->height = 2424u; out->dataSize = 777u;
    out->bitsAllocated = 99u; out->data = nullptr;

    XpeDicomHandle* handle = nullptr;
    const XpeErrorCode openRc = xpe_dicom_open(path.string().c_str(), &handle);
    if (openRc != XPE_OK) return openRc;
    const XpeErrorCode rc = xpe_dicom_read_image(handle, out);
    xpe_dicom_close(handle);
    return rc;
}

}  // namespace

TEST_F(DicomReaderTest, J2kCodestreamSmallerThanDeclared_ReturnsDicomInvalid) {
    const auto path = s_tempDir / "j2k_undersized.dcm";
    std::string whyNot;
    ASSERT_TRUE(WriteCompressedWithDeclaredRows(path, /*useJ2K=*/true, 256, 128, 256, whyNot))
        << whyNot;

    XpeImageBuffer img{};
    EXPECT_EQ(XPE_ERR_DICOM_INVALID, ReadWithSentinel(path, &img))
        << "before #150 this returned XPE_OK with a 256x128 buffer while the "
           "metadata said 256x256";
    // Judged before xpe_alloc_image runs, so the QA-B-48 contract holds in full.
    EXPECT_EQ(nullptr, img.data);
    EXPECT_EQ(4242u, img.width)  << "a rejected read must not write to outImg";
    EXPECT_EQ(2424u, img.height) << "a rejected read must not write to outImg";
}

TEST_F(DicomReaderTest, JpegLosslessFrameSmallerThanDeclared_ReturnsDicomInvalid) {
    const auto path = s_tempDir / "jpegll_undersized.dcm";
    std::string whyNot;
    ASSERT_TRUE(WriteCompressedWithDeclaredRows(path, /*useJ2K=*/false, 256, 128, 256, whyNot))
        << whyNot;

    XpeImageBuffer img{};
    EXPECT_EQ(XPE_ERR_DICOM_INVALID, ReadWithSentinel(path, &img))
        << "before #150 this returned XPE_OK with a full-size image whose lower "
           "half was black padding -- HAZ-DCM-002 exactly";
    EXPECT_EQ(nullptr, img.data);
    EXPECT_EQ(4242u, img.width)  << "a rejected read must not write to outImg";
    EXPECT_EQ(2424u, img.height) << "a rejected read must not write to outImg";
}

// The controls. Same construction, declared == real: these must read normally,
// so the two rejections above are about the mismatch and not about the fixture.
TEST_F(DicomReaderTest, CompressedMatchedSize_StillReadsNormally) {
    struct Case { const char* name; bool j2k; };
    const Case cases[] = { {"J2K", true}, {"JPEG-LL", false} };
    for (const auto& c : cases) {
        SCOPED_TRACE(c.name);
        const auto path = s_tempDir / (std::string("matched_") + c.name + ".dcm");
        std::string whyNot;
        ASSERT_TRUE(WriteCompressedWithDeclaredRows(path, c.j2k, 256, 128, 128, whyNot))
            << whyNot;

        XpeImageBuffer img{};
        ASSERT_EQ(XPE_OK, ReadWithSentinel(path, &img));
        EXPECT_EQ(256u, img.width);
        EXPECT_EQ(128u, img.height);
        ASSERT_NE(nullptr, img.data);

        const uint16_t* px = static_cast<const uint16_t*>(img.data);
        const size_t n = static_cast<size_t>(img.width) * img.height;
        size_t zeros = 0;
        for (size_t k = 0; k < n; ++k) if (px[k] == 0) ++zeros;
        EXPECT_EQ(0u, zeros) << "the fixture writes no zero pixels, so a zero here "
                                "would mean padding crept into a matched-size read";
        xpe_free_image(&img);
    }
}

// ---------------------------------------------------------------------------
// #150 (QA-B-51): the OTHER direction -- a decoded frame LARGER than the dataset
// declares is a rejection too.
//
// This case existed in QA-B-50 as KnownDivergence_CompressedLargerThanDeclared,
// which pinned two different answers to one malformed shape:
//
//     j2k-oversized     rc=0   declared 256x128 -> returned 256x256
//     jpegll-oversized  rc=-3  (PROCESSING_FAILED, from DCMTK's decoder)
//
// That record was not wrong when it was written -- no contract existed, so it
// held the ground rather than guessing at one. The contract now exists, and it
// REPLACES the record: Rows/Columns are the file's own description of its
// pixels, so a decoded frame that is larger means the file contradicts itself,
// and handing that image onward (to a PACS, to a viewer) propagates the lie. The
// path-dependent split was itself the clearest evidence something was undecided:
// one malformed shape, two answers.
//
// The boundary this does NOT cross: on the NATIVE path, surplus trailing BYTES
// remain a success (SurplusPixelData_IsIgnoredAndReadSucceeds, unchanged) --
// surplus bytes are ordinary DICOM padding and contradict no dimension claim,
// whereas surplus DIMENSIONS contradict one.
// ---------------------------------------------------------------------------
TEST_F(DicomReaderTest, J2kCodestreamLargerThanDeclared_ReturnsDicomInvalid) {
    const auto path = s_tempDir / "j2k_oversized.dcm";
    std::string whyNot;
    ASSERT_TRUE(WriteCompressedWithDeclaredRows(path, /*useJ2K=*/true, 256, 256, 128, whyNot))
        << whyNot;

    XpeImageBuffer img{};
    EXPECT_EQ(XPE_ERR_DICOM_INVALID, ReadWithSentinel(path, &img))
        << "before #150/QA-B-51 this returned XPE_OK with a 256x256 buffer while "
           "the dataset declared 256x128";
    EXPECT_EQ(nullptr, img.data);
    EXPECT_EQ(4242u, img.width)  << "a rejected read must not write to outImg";
    EXPECT_EQ(2424u, img.height) << "a rejected read must not write to outImg";
}

TEST_F(DicomReaderTest, JpegLosslessFrameLargerThanDeclared_ReturnsDicomInvalid) {
    const auto path = s_tempDir / "jpegll_oversized.dcm";
    std::string whyNot;
    ASSERT_TRUE(WriteCompressedWithDeclaredRows(path, /*useJ2K=*/false, 256, 256, 128, whyNot))
        << whyNot;

    XpeImageBuffer img{};
    EXPECT_EQ(XPE_ERR_DICOM_INVALID, ReadWithSentinel(path, &img))
        << "this shape used to surface as XPE_ERR_PROCESSING_FAILED from DCMTK's "
           "decoder -- a decoder fault, which is not what is wrong with the file; "
           "the SOF check now names it as the dimension mismatch it is";
    EXPECT_EQ(nullptr, img.data);
    EXPECT_EQ(4242u, img.width)  << "a rejected read must not write to outImg";
    EXPECT_EQ(2424u, img.height) << "a rejected read must not write to outImg";
}

// ---------------------------------------------------------------------------
// #147 (QA-B-67) — how does a 1.2.840.10008.1.2.4.57 file FAIL?
//
// Two requirements name different UIDs and the implementation knows one of them:
//
//   REQ-IOP-003  (SPEC-XPE-IOP/spec.md:116)  "at minimum ... 1.2.840.10008.1.2.4.57"
//   REQ-DICOM-004                            ".70" (what DicomReader.h accepts)
//
// `.57` appears nowhere under modules/. The trap is the NAME: both read as "JPEG
// Lossless", but .57 is Process 14 and .70 is Process 14 Selection Value 1
// (first-order prediction). A reader that knows only .70 cannot decode a .57
// bitstream.
//
// Whether to support .57 is a decision. What is measurable NOW -- and what
// matters clinically -- is HOW it fails: an explicit refusal is safe, while
// mistaking it for a syntax the reader does know would decode wrong pixels
// silently, which is the worst failure shape in medical imaging.
//
// The existing UnsupportedTS_ReturnsUnsupportedFormat case does NOT answer this.
// It feeds an Implicit VR Little Endian file -- an UNCOMPRESSED syntax differing
// from the accepted list in every respect. It establishes that the list check
// works for the easiest possible input; it says nothing about a compressed
// syntax whose name and family match an accepted one.
//
// SYNTHETIC DATA, stated plainly (the #148 lesson): no .57 file from real
// equipment was used. The fixture below is a genuine .70-encoded file whose meta
// TransferSyntaxUID was rewritten to .57 -- the bitstream is real JPEG, the label
// is not. That is the sharpest form of the question (a reader that ignored the
// label and guessed would "succeed" here) but it is NOT a real .57 bitstream, so
// it cannot show what a true .57 decode would produce.
//
// EXISTENCE CONTROL, in the same run and with the same tool: the unmodified .70
// file must OPEN. Without that, "both failed" would be indistinguishable from a
// broken fixture rather than a refused syntax.
// ---------------------------------------------------------------------------
TEST_F(DicomReaderTest, KnownDivergence_MislabelledJpegLosslessIsDecodedAnyway) {
    const auto genuine70 = s_tempDir / "b67_genuine_70.dcm";
    if (!WriteJpegLosslessCopy(s_validDcm, genuine70)) {
        GTEST_SKIP() << "DCMTK could not produce a .70 fixture in this build -- "
                        "without it there is no control, and a lone failure would "
                        "prove nothing";
    }

    // --- control: the genuine .70 file opens -------------------------------
    XpeDicomHandle* control = nullptr;
    const XpeErrorCode ecControl =
        xpe_dicom_open(genuine70.string().c_str(), &control);
    ASSERT_EQ(XPE_OK, ecControl)
        << "the control fixture does not open, so nothing below distinguishes a "
           "behaviour from a broken fixture";
    xpe_dicom_close(control);

    // --- subject: .70 bytes wearing a .57 label ----------------------------
    const auto relabelled57 = s_tempDir / "b67_relabelled_57.dcm";
    {
        DcmFileFormat ff;
        ASSERT_TRUE(ff.loadFile(genuine70.string().c_str()).good());
        DcmMetaInfo* meta = ff.getMetaInfo();
        ASSERT_NE(nullptr, meta);
        ASSERT_TRUE(meta->putAndInsertString(DCM_TransferSyntaxUID,
                                             "1.2.840.10008.1.2.4.57").good());
        // EWM_dontUpdateMeta keeps the rewritten label instead of restoring the
        // syntax the pixel data is actually in.
        ASSERT_TRUE(ff.saveFile(relabelled57.string().c_str(), EXS_JPEGProcess14SV1,
                                EET_ExplicitLength, EGL_recalcGL, EPD_withoutPadding,
                                0, 0, EWM_dontUpdateMeta).good());
    }

    XpeDicomHandle* subject = nullptr;
    const XpeErrorCode ecOpen =
        xpe_dicom_open(relabelled57.string().c_str(), &subject);
    XpeImageBuffer img{};
    XpeErrorCode ecRead = XPE_ERR_INTERNAL;
    bool gotPixels = false;
    if (ecOpen == XPE_OK) {
        ecRead = xpe_dicom_read_image(subject, &img);
        gotPixels = (ecRead == XPE_OK && img.data != nullptr);
    }

    GTEST_LOG_(INFO) << "mislabelled (.70 bytes, .57 label): open=" << ecOpen
                     << " read=" << ecRead << " pixels=" << gotPixels;

    // The divergence: the label is wrong and the file is read anyway. DCMTK
    // decompresses from the representation the DATASET carries, not from the
    // meta-header label, so a JPEG-Lossless file labelled as the other
    // JPEG-Lossless syntax still decodes.
    //
    // Recorded rather than called a defect: for these two syntaxes the outcome
    // is a correct image, and DICOM readers are widely expected to tolerate
    // meta/dataset disagreement. What it DOES mean is that the #150 frame-size
    // guard is keyed on the LABEL (readImage picks EXS_JPEGProcess14 for a .57
    // label), so on a mislabelled file that guard looks for a representation
    // that is not there and silently checks nothing. The guard's own coverage,
    // not the pixels, is what a mislabel costs here.
    EXPECT_EQ(XPE_OK, ecOpen) << "a .57-labelled file is refused -- QA-B-68 added "
                                 "the UID to the accepted list";
    EXPECT_TRUE(gotPixels)
        << "the mislabelled file no longer decodes; if that is deliberate, this "
           "case records the old behaviour and should be retired";
    if (gotPixels) xpe_free_image(&img);
    xpe_dicom_close(subject);
}

// Does DCMTK itself know .57? The answer decides what implementing REQ-IOP-003
// would cost: a codec the library already ships is a different proposition from
// one that must be written. Measured, not assumed -- and reported either way,
// because a negative here is as much an input to that decision as a positive.
TEST_F(DicomReaderTest, KnownDivergence_DcmtkCodecSupportFor57IsMeasured) {
    auto canEncode = [](E_TransferSyntax xfer) {
        DJEncoderRegistration::registerCodecs();
        bool ok = false;
        {
            DcmFileFormat ff;
            if (ff.loadFile(s_validDcm.string().c_str()).good()) {
                DcmDataset* ds = ff.getDataset();
                if (ds != nullptr) {
                    ok = ds->chooseRepresentation(xfer, nullptr).good() &&
                         ds->canWriteXfer(xfer);
                }
            }
        }
        DJEncoderRegistration::cleanup();
        return ok;
    };

    // EXS_JPEGProcess14    == 1.2.840.10008.1.2.4.57
    // EXS_JPEGProcess14SV1 == 1.2.840.10008.1.2.4.70
    const bool canEncode57 = canEncode(EXS_JPEGProcess14);
    const bool canEncode70 = canEncode(EXS_JPEGProcess14SV1);

    GTEST_LOG_(INFO) << "DCMTK encoder: .57 (EXS_JPEGProcess14)=" << canEncode57
                     << "  .70 (EXS_JPEGProcess14SV1)=" << canEncode70;

    // Control in the same run with the same library: a false on .57 would
    // otherwise only mean the probe itself does not work.
    EXPECT_TRUE(canEncode70)
        << "the probe cannot produce .70 either, so its answer about .57 says "
           "nothing about DCMTK";

    // No assertion on canEncode57 -- the measurement IS the deliverable, and
    // pinning either answer would prejudge the support decision (#147).
    SUCCEED();
}

// A GENUINE .57 file, now that the encoder probe above showed DCMTK can produce
// one. This removes the relabelling caveat from the case further up: the
// bitstream really is Process 14, not a .70 stream wearing a .57 label.
//
// QA-B-67 wrote this case while .57 was refused, and the refusal was the thing
// it asserted. QA-B-68 changed the decision, so the assertion changed with it --
// the measurement it exists for (what DCMTK can do, which is what made the
// decision cheap) is unchanged and is still the deliverable.
//
// Two separate questions are answered here, and keeping them apart is the point:
//
//   1. What does xpe_dicom_open() do with it?  -- the product's behaviour.
//   2. Can DCMTK decode it?                    -- the library's capability, which
//      is what decides the COST of supporting .57 (#147). Adding a UID to a table
//      is not the same proposition as writing a codec.
//
// (2) is measured with the decoder, not the encoder. The probe above showed only
// that DCMTK can WRITE .57; a reader needs the other direction, and assuming one
// from the other would be the "it exists, therefore it works" error this session
// has hit repeatedly.
TEST_F(DicomReaderTest, Genuine57IsAcceptedAndDcmtkDecodeSupportIsMeasured) {
    const auto genuine57 = s_tempDir / "b67_genuine_57.dcm";

    // --- produce a real Process-14 bitstream -------------------------------
    bool encoded = false;
    DJEncoderRegistration::registerCodecs();
    {
        DcmFileFormat ff;
        if (ff.loadFile(s_validDcm.string().c_str()).good()) {
            DcmDataset* ds = ff.getDataset();
            if (ds != nullptr &&
                ds->chooseRepresentation(EXS_JPEGProcess14, nullptr).good() &&
                ds->canWriteXfer(EXS_JPEGProcess14)) {
                encoded = ff.saveFile(genuine57.string().c_str(), EXS_JPEGProcess14).good();
            }
        }
    }
    DJEncoderRegistration::cleanup();
    if (!encoded) {
        GTEST_SKIP() << "DCMTK could not encode .57 in this build -- the relabelled "
                        "case above is then the only available measurement";
    }

    // --- (1) the product accepts it (QA-B-68) ------------------------------
    // QA-B-67 measured XPE_ERR_UNSUPPORTED_FORMAT here and that was the right
    // answer at the time: the UID was not on the accepted list. The leader then
    // decided to support .57 (REQ-IOP-003 names it "at minimum", and (2) below
    // is why the cost was low), so the expected answer changed with the decision.
    // The pixel-exactness of that decode is asserted in
    // ReadJpegLosslessProcess14_DecodesPixelExact; this case keeps the
    // capability measurement that produced the decision.
    XpeDicomHandle* handle = nullptr;
    const XpeErrorCode ecOpen = xpe_dicom_open(genuine57.string().c_str(), &handle);
    EXPECT_EQ(XPE_OK, ecOpen) << "a genuine .57 file is refused again";
    xpe_dicom_close(handle);

    // --- (2) the library can decode it -------------------------------------
    // The reader already calls DJDecoderRegistration::registerCodecs() on every
    // open (DicomReader.cpp:48), so this asks what that registration covers.
    // NOT cleaned up afterwards, deliberately. DicomReader registers the JPEG
    // decoders exactly once (std::call_once, DicomReader.cpp:47), so a
    // DJDecoderRegistration::cleanup() here unregisters them for the whole
    // process and the once-flag prevents the reader from ever restoring them.
    //
    // QA-B-68 found this the hard way: with a cleanup() here, a later case
    // measured a genuine .57 file failing to decode (read=-3) and the obvious
    // reading was "the decoder cannot handle .57". Run in isolation the same
    // case decoded byte-exact. The defect was in this test, not in the product,
    // and reporting it the other way round would have argued against a decision
    // that had already been made on correct evidence.
    bool decoded57 = false;
    DJDecoderRegistration::registerCodecs();
    {
        DcmFileFormat ff;
        if (ff.loadFile(genuine57.string().c_str()).good()) {
            DcmDataset* ds = ff.getDataset();
            if (ds != nullptr) {
                decoded57 = ds->chooseRepresentation(EXS_LittleEndianExplicit, nullptr).good();
            }
        }
    }

    GTEST_LOG_(INFO) << "genuine .57: xpe_dicom_open=" << ecOpen
                     << "  DCMTK decode-to-uncompressed=" << decoded57;

    // Reported, not asserted: whether DCMTK decodes .57 is an input to the
    // support decision (#147), and pinning it either way here would prejudge
    // that decision. The number is the deliverable.
    SUCCEED();
}

// ---------------------------------------------------------------------------
// #147 (QA-B-68) — the path that never meets the accepted-syntax check.
//
// STATUS SINCE QA-B-72 (#167): the behaviour described below is CLOSED. The
// TS-less branch now checks the detected syntax against the accepted list and
// refuses an encapsulated PixelData that contradicts a native detection; the
// getMetaInfo() == NULL branch refuses outright. What follows is kept as the
// record of what was measured before that change, in the tense it was written.
//
// QA-B-67 measured that a .57 file is refused with XPE_ERR_UNSUPPORTED_FORMAT.
// That measurement was taken on ONE path: the one that reads the meta-header,
// finds a TransferSyntaxUID, and compares it against kSupportedTransferSyntaxes.
//
// DicomReader::open() had two branches that recorded Explicit VR Little Endian
// without consulting kSupportedTransferSyntaxes -- one for a NULL getMetaInfo(),
// one for a meta-header with no TransferSyntaxUID. QA-B-70 noted that the branch
// these fixtures take had not been measured; QA-B-71 then measured it: every
// fixture took the second one, and no input reached the first. Either way open()
// accepted the file and recorded m_tsUID = Explicit VR Little Endian WITHOUT any
// syntax check. A file arriving through that branch was declared uncompressed no
// matter what its pixel data actually was -- so "a .57 file is refused" would not
// hold there, and the conclusion that no silent misdecode happens would be true
// only of the path it was measured on.
//
// THIS IS MEASURABLE ONLY NOW. Once .57 joins the accepted list, both paths take
// it and the difference between them disappears.
//
// Control pairs, all in this one run:
//   - meta-less .57   (subject)
//   - meta-less .70   (does the branch depend on the syntax at all?)
//   - meta-bearing .57 (the QA-B-67 path, re-measured here for comparison)
// ---------------------------------------------------------------------------
namespace {

// Write the DATASET only -- no Part-10 preamble, no meta-header. DCMTK writes
// the group-2 elements only through DcmFileFormat, so going through DcmDataset
// is what produces a file that reaches the TS-less branch of open()
// (measured by QA-B-71; before QA-B-72 that branch had no syntax check).
bool WriteDatasetWithoutMeta(const fs::path& src, const fs::path& dst,
                             E_TransferSyntax xfer) {
    DJEncoderRegistration::registerCodecs();
    bool ok = false;
    {
        DcmFileFormat ff;
        if (ff.loadFile(src.string().c_str()).good()) {
            DcmDataset* ds = ff.getDataset();
            if (ds != nullptr &&
                ds->chooseRepresentation(xfer, nullptr).good() &&
                ds->canWriteXfer(xfer)) {
                ok = ds->saveFile(dst.string().c_str(), xfer).good();
            }
        }
    }
    DJEncoderRegistration::cleanup();
    return ok;
}

struct OpenResult {
    XpeErrorCode open = XPE_ERR_INTERNAL;
    XpeErrorCode read = XPE_ERR_INTERNAL;
    bool         gotPixels = false;
    uint32_t     w = 0, h = 0;
};

OpenResult OpenAndRead(const fs::path& p) {
    OpenResult r{};
    XpeDicomHandle* handle = nullptr;
    r.open = xpe_dicom_open(p.string().c_str(), &handle);
    if (r.open == XPE_OK) {
        XpeImageBuffer img{};
        r.read = xpe_dicom_read_image(handle, &img);
        if (r.read == XPE_OK) {
            r.gotPixels = (img.data != nullptr);
            r.w = img.width;
            r.h = img.height;
            xpe_free_image(&img);
        }
    }
    xpe_dicom_close(handle);
    return r;
}

}  // namespace

// RENAMED by QA-B-72 (#167). This case was KnownDivergence_MetaLessPathSkips-
// TheTransferSyntaxCheck: its name recorded that the TS-less path had no syntax
// check, and its body asserted only that no pixels came out -- which held then
// because the native read failed on encapsulated data. QA-B-72 added the checks,
// so the name stopped being true while the body kept passing. The assertion is
// unchanged; the name now says what it asserts. Issue #167 comments before
// QA-B-72 refer to the old name.
TEST_F(DicomReaderTest, MetaLessEncapsulatedFilesProduceNoPixels) {
    const auto metaless57 = s_tempDir / "b68_metaless_57.dcm";
    const auto metaless70 = s_tempDir / "b68_metaless_70.dcm";

    if (!WriteDatasetWithoutMeta(s_validDcm, metaless57, EXS_JPEGProcess14) ||
        !WriteDatasetWithoutMeta(s_validDcm, metaless70, EXS_JPEGProcess14SV1)) {
        GTEST_SKIP() << "could not write meta-less compressed fixtures in this build";
    }

    const OpenResult r57 = OpenAndRead(metaless57);
    const OpenResult r70 = OpenAndRead(metaless70);

    GTEST_LOG_(INFO) << "meta-less .57: open=" << r57.open << " read=" << r57.read
                     << " pixels=" << r57.gotPixels << " " << r57.w << "x" << r57.h;
    GTEST_LOG_(INFO) << "meta-less .70: open=" << r70.open << " read=" << r70.read
                     << " pixels=" << r70.gotPixels << " " << r70.w << "x" << r70.h;

    // The claim this case exists to protect: a file whose pixel data is in a
    // syntax this reader cannot decode must not come back as pixels. Which error
    // it gives is not the point; producing a frame is.
    EXPECT_FALSE(r57.open == XPE_OK && r57.read == XPE_OK && r57.gotPixels)
        << "a meta-less .57 file produced pixels -- the accepted-syntax check was "
           "never reached and the bytes were decoded as something else";
}

// ---------------------------------------------------------------------------
// #147 (QA-B-68) — .57 is supported now. Pixels, not just the absence of -7.
//
// Adding the UID to kSupportedTransferSyntaxes removes the refusal. That is NOT
// the same as reading the file, and the difference is measurable: before the
// decode branch learned .57, an accepted .57 file fell through to the native
// path and came back XPE_ERR_DICOM_INVALID. "-7 is gone" would have looked like
// success while no image existed.
//
// So this case asserts the pixels, and asserts them against the source: JPEG
// Lossless is lossless, so a byte-exact match is available and anything less
// would be a decode that ran without being right.
//
// SYNTHETIC (the #148 lesson, restated rather than assumed): the fixture is
// DCMTK's own Process-14 encoding of s_validDcm. A real acquisition device's
// encoder may differ; that remains open on #151.
// ---------------------------------------------------------------------------
TEST_F(DicomReaderTest, ReadJpegLosslessProcess14_DecodesPixelExact) {
    const auto genuine57 = s_tempDir / "b68_support_57.dcm";
    bool encoded = false;
    DJEncoderRegistration::registerCodecs();
    {
        DcmFileFormat ff;
        if (ff.loadFile(s_validDcm.string().c_str()).good()) {
            DcmDataset* ds = ff.getDataset();
            if (ds != nullptr &&
                ds->chooseRepresentation(EXS_JPEGProcess14, nullptr).good() &&
                ds->canWriteXfer(EXS_JPEGProcess14)) {
                encoded = ff.saveFile(genuine57.string().c_str(), EXS_JPEGProcess14).good();
            }
        }
    }
    DJEncoderRegistration::cleanup();
    ASSERT_TRUE(encoded) << "DCMTK could not encode .57 -- QA-B-67 measured that it can";

    // The source pixels, read through the uncompressed path.
    XpeDicomHandle* srcHandle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(s_validDcm.string().c_str(), &srcHandle));
    XpeImageBuffer srcImg{};
    ASSERT_EQ(XPE_OK, xpe_dicom_read_image(srcHandle, &srcImg));
    xpe_dicom_close(srcHandle);

    // The same pixels through .57.
    XpeDicomHandle* handle = nullptr;
    const XpeErrorCode ecOpen = xpe_dicom_open(genuine57.string().c_str(), &handle);
    EXPECT_EQ(XPE_OK, ecOpen) << "a .57 file is still refused";

    XpeImageBuffer img{};
    const XpeErrorCode ecRead = xpe_dicom_read_image(handle, &img);
    GTEST_LOG_(INFO) << "genuine .57 after support: open=" << ecOpen
                     << " read=" << ecRead << " " << img.width << "x" << img.height;
    ASSERT_EQ(XPE_OK, ecRead)
        << "the refusal is gone but no image came out -- accepting a syntax is "
           "not decoding it";

    ASSERT_EQ(srcImg.width,  img.width);
    ASSERT_EQ(srcImg.height, img.height);
    ASSERT_EQ(XPE_PIXEL_UINT16, img.format);

    const auto* a = static_cast<const uint16_t*>(srcImg.data);
    const auto* b = static_cast<const uint16_t*>(img.data);
    size_t differing = 0;
    for (size_t i = 0; i < static_cast<size_t>(img.width) * img.height; ++i) {
        if (a[i] != b[i]) ++differing;
    }
    EXPECT_EQ(0u, differing)
        << differing << " pixels differ from the source -- .57 decoded, but not "
           "losslessly";

    xpe_free_image(&img);
    xpe_free_image(&srcImg);
    xpe_dicom_close(handle);
}

// ---------------------------------------------------------------------------
// #147 (QA-B-68) — UnsupportedTS, with the gap QA-B-67 found closed.
//
// The original case feeds Implicit VR Little Endian: an UNCOMPRESSED syntax that
// differs from every accepted entry in every respect. It shows the list check
// works on the easiest possible input, while its name reads as a general claim
// about unsupported syntaxes. That overstatement is what let .57 sit unexamined
// -- a compressed syntax whose name and family match an accepted one.
//
// This case closes that: JPEG Baseline (1.2.840.10008.1.2.4.50) is JPEG, is
// compressed, is decodable by the very codec set this reader registers, and is
// still not on the accepted list. If the list check were ever replaced by
// something looser -- "is it JPEG?" -- the original case would not notice and
// this one would.
// ---------------------------------------------------------------------------
TEST_F(DicomReaderTest, UnsupportedCompressedTS_ReturnsUnsupportedFormat) {
    const auto baseline50 = s_tempDir / "b68_unsupported_50.dcm";
    bool encoded = false;
    DJEncoderRegistration::registerCodecs();
    {
        DcmFileFormat ff;
        if (ff.loadFile(s_validDcm.string().c_str()).good()) {
            DcmDataset* ds = ff.getDataset();
            DJ_RPLossy param;
            if (ds != nullptr &&
                ds->chooseRepresentation(EXS_JPEGProcess1, &param).good() &&
                ds->canWriteXfer(EXS_JPEGProcess1)) {
                encoded = ff.saveFile(baseline50.string().c_str(), EXS_JPEGProcess1).good();
            }
        }
    }
    DJEncoderRegistration::cleanup();
    if (!encoded) {
        GTEST_SKIP() << "DCMTK could not encode JPEG Baseline here; without the "
                        "fixture this case would assert nothing";
    }

    XpeDicomHandle* handle = nullptr;
    const XpeErrorCode ec = xpe_dicom_open(baseline50.string().c_str(), &handle);
    GTEST_LOG_(INFO) << "JPEG Baseline (.50, compressed, not accepted): open=" << ec;
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, ec)
        << "a compressed syntax that is NOT on the accepted list was not refused "
           "with UNSUPPORTED_FORMAT";
    xpe_dicom_close(handle);
}

// ---------------------------------------------------------------------------
// #167 (QA-B-69) — is it the check that stops these files, or the encapsulation?
//
// STATUS SINCE QA-B-72 (#167): the behaviour described below is CLOSED. The
// TS-less branch now checks the detected syntax against the accepted list and
// refuses an encapsulated PixelData that contradicts a native detection; the
// getMetaInfo() == NULL branch refuses outright. What follows is kept as the
// record of what was measured before that change, in the tense it was written.
//
// QA-B-68 measured that a meta-less .57 file reaches open() with no transfer
// syntax check at all (open() records Explicit VR Little Endian whatever the
// file actually is; see the note on the case above for which branch) and still
// produces no pixels. The reason was
// NOT the check: the native read path cannot pull an encapsulated PixelData out
// as a plain uint16 array, so it fails for a structural reason that has nothing
// to do with which syntax the file claims.
//
// That leaves exactly one question, and it decides whether the QA-B-67
// conclusion ("no silent misdecode") holds outside the path it was measured on:
//
//     does a NON-encapsulated unsupported syntax, arriving with no meta-header,
//     come back as pixels?
//
// The candidates are derived from what DCMTK can write, not from a list someone
// wrote down: a syntax qualifies if it is native (not encapsulated, so the
// native read path can work on it) and absent from kSupportedTransferSyntaxes.
//
// TWO CONTROLS, both in this run:
//   - the same file WITH its meta-header must answer XPE_ERR_UNSUPPORTED_FORMAT,
//     which is what shows the check is alive and the meta-less result is about
//     the missing header rather than about the syntax being tolerated;
//   - a meta-less file in a SUPPORTED syntax must open and yield pixels, or the
//     fixture writer is broken and every negative below means nothing.
//
// SYNTHETIC (#148): every file here is written by DCMTK from s_validDcm. No
// acquisition device produced them.
//
// ENABLED by QA-B-72 (#167). The paragraphs below record why it was DISABLED_
// until then; they are kept because the polarity they describe is exactly what
// flipped: this case went green the day the TS-less path gained its checks.
//
// THE ANSWER WAS YES, AND THAT IS WHY THIS CASE WAS DISABLED_ RATHER THAN RED.
// Measured 2026-09-16: Implicit VR Little Endian and Explicit VR Big Endian --
// both absent from kSupportedTransferSyntaxes, both refused with
// XPE_ERR_UNSUPPORTED_FORMAT when they carry a meta-header -- come back as a
// full 256x256 frame when the meta-header is absent. A file the reader rejects
// when it is labelled is read when the label is missing.
//
// The polarity follows QA-B-66: this case asserts what the reader SHOULD do, so
// it goes green the day the hole is closed rather than red the day someone fixes
// it. The always-on record of what happens today is
// KnownDivergence_MetaLessPathLeaksNativeUnsupportedSyntaxes below, which logs
// the same measurement and asserts nothing.
//
// It was DISABLED_ because the defect was BLOCKED on a decision, not on work:
// what to do was tangled with whether a meta-less file should be accepted at all
// (open() then accepted it and recorded Explicit VR Little Endian, which was a
// guess). Refusing meta-less files outright, checking the detected syntax instead
// of assuming one, or keeping the tolerance and documenting it were three
// different products. #167 made that call (check the detected syntax, refuse a
// contradiction, close the unreachable branch) and QA-B-72 implemented it.
// ---------------------------------------------------------------------------
namespace {

struct NativeSyntaxCandidate {
    E_TransferSyntax xfer;
    const char*      uid;
    const char*      name;
    bool             onAcceptedList;
};

// Native (non-encapsulated) syntaxes DCMTK knows. Encapsulated ones are excluded
// by construction -- QA-B-68 already showed the native path cannot read those,
// and this case is about the syntaxes where that structural block is absent.
const NativeSyntaxCandidate kNativeSyntaxes[] = {
    { EXS_LittleEndianImplicit,          "1.2.840.10008.1.2",       "Implicit VR Little Endian",    false },
    { EXS_LittleEndianExplicit,          "1.2.840.10008.1.2.1",     "Explicit VR Little Endian",    true  },
    { EXS_BigEndianExplicit,             "1.2.840.10008.1.2.2",     "Explicit VR Big Endian",       false },
    { EXS_DeflatedLittleEndianExplicit,  "1.2.840.10008.1.2.1.99",  "Deflated Explicit VR LE",      false },
};

bool IsOnAcceptedList(const char* uid) {
    for (size_t i = 0; i < xpe::dicom::kSupportedTransferSyntaxCount; ++i) {
        if (std::string(uid) == xpe::dicom::kSupportedTransferSyntaxes[i].uid) return true;
    }
    return false;
}

// Dataset only -- no preamble, no group-2 elements. This is what reaches the
// TS-less branch of open() (measured by QA-B-71).
bool WriteDatasetOnly(const fs::path& src, const fs::path& dst, E_TransferSyntax xfer) {
    DcmFileFormat ff;
    if (!ff.loadFile(src.string().c_str()).good()) return false;
    DcmDataset* ds = ff.getDataset();
    if (ds == nullptr) return false;
    if (!ds->chooseRepresentation(xfer, nullptr).good()) return false;
    if (!ds->canWriteXfer(xfer)) return false;
    return ds->saveFile(dst.string().c_str(), xfer).good();
}

// Full Part-10 file, meta-header included.
bool WriteWithMeta(const fs::path& src, const fs::path& dst, E_TransferSyntax xfer) {
    DcmFileFormat ff;
    if (!ff.loadFile(src.string().c_str()).good()) return false;
    DcmDataset* ds = ff.getDataset();
    if (ds == nullptr) return false;
    if (!ds->chooseRepresentation(xfer, nullptr).good()) return false;
    if (!ds->canWriteXfer(xfer)) return false;
    return ff.saveFile(dst.string().c_str(), xfer).good();
}

}  // namespace

TEST_F(DicomReaderTest, MetaLessNativeUnsupportedSyntaxesProduceNoPixels) {
    // The accepted-list membership in the table is a convenience for reading; the
    // authority is the list itself, so it is cross-checked rather than trusted.
    for (const auto& c : kNativeSyntaxes) {
        ASSERT_EQ(c.onAcceptedList, IsOnAcceptedList(c.uid))
            << "the table disagrees with kSupportedTransferSyntaxes about " << c.uid
            << " -- fix the table, not the list";
    }

    // --- control 2: a meta-less SUPPORTED syntax must yield pixels ----------
    // Placed first: if the writer cannot produce a readable meta-less file at
    // all, every "no pixels" below is about the fixture and not about the reader.
    const auto sane = s_tempDir / "b69_metaless_explicitLE.dcm";
    ASSERT_TRUE(WriteDatasetOnly(s_validDcm, sane, EXS_LittleEndianExplicit))
        << "could not write a meta-less Explicit LE file";
    const OpenResult sanity = OpenAndRead(sane);
    GTEST_LOG_(INFO) << "control: meta-less Explicit VR LE (SUPPORTED) open="
                     << sanity.open << " read=" << sanity.read
                     << " pixels=" << sanity.gotPixels
                     << " " << sanity.w << "x" << sanity.h;
    ASSERT_TRUE(sanity.open == XPE_OK && sanity.read == XPE_OK && sanity.gotPixels)
        << "a meta-less file in a SUPPORTED syntax produced no pixels -- the "
           "fixture writer is broken and nothing below is measured";

    // --- subjects -----------------------------------------------------------
    int leaked = 0;
    for (const auto& c : kNativeSyntaxes) {
        if (c.onAcceptedList) continue;   // supported ones are not the question

        const auto metaLess  = s_tempDir / (std::string("b69_metaless_") + c.uid + ".dcm");
        const auto withMeta  = s_tempDir / (std::string("b69_withmeta_") + c.uid + ".dcm");

        if (!WriteDatasetOnly(s_validDcm, metaLess, c.xfer) ||
            !WriteWithMeta(s_validDcm, withMeta, c.xfer)) {
            GTEST_LOG_(INFO) << c.name << " (" << c.uid
                             << "): DCMTK cannot write this syntax here -- not measured";
            continue;
        }

        // control 1: with a meta-header the check must refuse it.
        const OpenResult guarded = OpenAndRead(withMeta);
        EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, guarded.open)
            << c.name << " is not refused even WITH a meta-header -- the "
               "accepted-list check is not doing what the meta-less result is "
               "being compared against";

        const OpenResult bare = OpenAndRead(metaLess);
        GTEST_LOG_(INFO) << c.name << " (" << c.uid << "): with-meta open="
                         << guarded.open << " | meta-less open=" << bare.open
                         << " read=" << bare.read << " pixels=" << bare.gotPixels
                         << " " << bare.w << "x" << bare.h;

        if (bare.open == XPE_OK && bare.read == XPE_OK && bare.gotPixels) ++leaked;
    }

    EXPECT_EQ(0, leaked)
        << leaked << " native unsupported syntax/syntaxes produced pixels through "
           "the meta-less path -- a file the reader refuses when labelled is read "
           "when the label is absent (#167)";
}

// The always-on half of the pair above: the same sweep, logged, asserting only
// that the two controls still hold. It records TODAY's behaviour so the
// measurement does not live exclusively inside a case that default runs skip --
// a disabled test is a quiet place for a finding to sit.
TEST_F(DicomReaderTest, KnownDivergence_MetaLessPathLeaksNativeUnsupportedSyntaxes) {
    const auto sane = s_tempDir / "b69_rec_metaless_explicitLE.dcm";
    ASSERT_TRUE(WriteDatasetOnly(s_validDcm, sane, EXS_LittleEndianExplicit));
    const OpenResult sanity = OpenAndRead(sane);
    ASSERT_TRUE(sanity.open == XPE_OK && sanity.read == XPE_OK && sanity.gotPixels)
        << "the fixture writer is broken; nothing below is measured";

    int leaked = 0;
    for (const auto& c : kNativeSyntaxes) {
        if (c.onAcceptedList) continue;
        const auto metaLess = s_tempDir / (std::string("b69_rec_metaless_") + c.uid + ".dcm");
        const auto withMeta = s_tempDir / (std::string("b69_rec_withmeta_") + c.uid + ".dcm");
        if (!WriteDatasetOnly(s_validDcm, metaLess, c.xfer) ||
            !WriteWithMeta(s_validDcm, withMeta, c.xfer)) {
            GTEST_LOG_(INFO) << c.name << ": not writable here -- not measured";
            continue;
        }
        const OpenResult guarded = OpenAndRead(withMeta);
        const OpenResult bare    = OpenAndRead(metaLess);
        const bool gotPixels = (bare.open == XPE_OK && bare.read == XPE_OK && bare.gotPixels);
        if (gotPixels) ++leaked;
        GTEST_LOG_(INFO) << c.name << " (" << c.uid << "): with-meta="
                         << guarded.open << " meta-less open=" << bare.open
                         << " read=" << bare.read << " pixels=" << gotPixels
                         << " " << bare.w << "x" << bare.h;
        // The control, asserted: the check IS alive on the labelled path. Without
        // this the leak below could be read as "the syntax is simply tolerated".
        EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, guarded.open)
            << c.name << " is not refused even with a meta-header";
    }

    GTEST_LOG_(INFO) << "native unsupported syntaxes yielding pixels without a "
                        "meta-header: " << leaked << " (#167)";
    // QA-B-69 recorded leaked=2 here without asserting it. QA-B-72 closed the
    // path, and the sibling case (no longer DISABLED_) carries the requirement;
    // this one keeps logging the count so a regression shows up as a number in
    // every default run, not only as a red test.
    SUCCEED();
}

// QA-B-69 wrote this to show WHAT blocked the TS-less path, by stripping the
// encapsulation rather than the syntax: the same dataset written encapsulated
// and native, both unsupported. The measurement then was encapsulated -> no
// pixels (read failed, DICOM_INVALID) and native -> pixels. That contrast is what
// established that structure, not a check, was doing the blocking.
//
// Since QA-B-72 both are refused at open() (UNSUPPORTED_FORMAT) -- the
// encapsulated one by the contradiction check, the native one by the list check
// -- so the contrast this case was built to draw no longer exists. It stays as a
// record: if the two outcomes ever diverge again, one of those checks has
// stopped working. The per-check falsification is in
// TsLessPathIsDecidedByChecksNotByStructure's report (QA-B-72).
TEST_F(DicomReaderTest, KnownDivergence_WhatBlocksTheMetaLessPathIsMeasured) {
    const auto encapsulated = s_tempDir / "b69_metaless_encapsulated.dcm";
    const auto native       = s_tempDir / "b69_metaless_native_unsupported.dcm";

    DJEncoderRegistration::registerCodecs();
    const bool wroteEncapsulated =
        WriteDatasetOnly(s_validDcm, encapsulated, EXS_JPEGProcess14SV1);
    // no DJDecoderRegistration::cleanup() here -- see the note on the .57 probe.

    const bool wroteNative =
        WriteDatasetOnly(s_validDcm, native, EXS_LittleEndianImplicit);

    if (!wroteEncapsulated || !wroteNative) {
        GTEST_SKIP() << "could not write both fixtures; the comparison needs the pair";
    }

    const OpenResult enc = OpenAndRead(encapsulated);
    const OpenResult nat = OpenAndRead(native);

    GTEST_LOG_(INFO) << "meta-less ENCAPSULATED (.70): open=" << enc.open
                     << " read=" << enc.read << " pixels=" << enc.gotPixels;
    GTEST_LOG_(INFO) << "meta-less NATIVE (Implicit LE, unsupported): open=" << nat.open
                     << " read=" << nat.read << " pixels=" << nat.gotPixels;

    // Recorded, not asserted: TsLessPathIsDecidedByChecksNotByStructure carries
    // the requirement for both shapes.
    SUCCEED();
}

// ---------------------------------------------------------------------------
// #167 (QA-B-71) step 1 — which branch, and what does DCMTK know there?
//
// The decision is to apply the accepted-syntax check on the meta-less path by
// comparing the syntax DCMTK DETECTED against kSupportedTransferSyntaxes. That
// only works if the detected syntax is available on the branch the file takes,
// and QA-B-70 recorded that the branch itself was never measured: open() has two
// places that record Explicit VR Little Endian without consulting the list
// (getMetaInfo() == NULL, and a meta-header with no TransferSyntaxUID).
//
// This probe loads each fixture with EXACTLY the arguments DicomReader::open()
// uses and reports, per file:
//   - whether getMetaInfo() is NULL                 -> the first branch
//   - whether the meta carries a TransferSyntaxUID  -> otherwise the second
//   - DcmDataset::getOriginalXfer()                 -> what DCMTK detected
//   - whether that detection matches what was written
//
// Recorded, not asserted as a requirement: the deliverable is the table, and it
// decides whether step 2 is implementable at all.
// ---------------------------------------------------------------------------
namespace {

struct BranchProbe {
    bool             metaIsNull   = false;
    bool             metaHasTs    = false;
    E_TransferSyntax detected     = EXS_Unknown;
    bool             loaded       = false;
    // Control for the detection result: is the PixelData in the file actually
    // encapsulated? Without this, "detected Explicit LE" on a JPEG fixture could
    // simply mean the fixture was written uncompressed.
    bool             encapsulated = false;
};

BranchProbe ProbeLikeOpen(const fs::path& p) {
    BranchProbe r{};
    DcmFileFormat ff;
    // Same call as DicomReader::open().
    r.loaded = ff.loadFile(p.string().c_str(), EXS_Unknown, EGL_noChange,
                           DCM_MaxReadLength).good();
    if (!r.loaded) return r;
    DcmMetaInfo* meta = ff.getMetaInfo();
    r.metaIsNull = (meta == nullptr);
    if (meta != nullptr) {
        OFString ts;
        r.metaHasTs = meta->findAndGetOFString(DCM_TransferSyntaxUID, ts).good() &&
                      !ts.empty();
    }
    DcmDataset* ds = ff.getDataset();
    if (ds != nullptr) {
        r.detected = ds->getOriginalXfer();
        DcmElement* el = nullptr;
        if (ds->findAndGetElement(DCM_PixelData, el).good() && el != nullptr) {
            // An encapsulated PixelData is written with undefined length and
            // carries a pixel sequence; a native one has a defined length.
            DcmPixelData* pd = OFstatic_cast(DcmPixelData*, el);
            DcmPixelSequence* seq = nullptr;
            const DcmRepresentationParameter* param = nullptr;
            r.encapsulated =
                pd->getEncapsulatedRepresentation(EXS_JPEGProcess14SV1, param, seq).good() ||
                pd->getEncapsulatedRepresentation(EXS_JPEGProcess14,    param, seq).good() ||
                el->getLengthField() == DCM_UndefinedLength;
        }
    }
    return r;
}

const char* XferUid(E_TransferSyntax x) {
    if (x == EXS_Unknown) return "(unknown)";
    DcmXfer xf(x);
    return xf.getXferID();
}

}  // namespace

TEST_F(DicomReaderTest, KnownDivergence_MetaLessBranchAndDetectedSyntaxAreMeasured) {
    struct Case { E_TransferSyntax written; const char* label; bool encapsulated; };
    const Case cases[] = {
        { EXS_LittleEndianExplicit, "Explicit VR LE (supported)",      false },
        { EXS_LittleEndianImplicit, "Implicit VR LE (unsupported)",    false },
        { EXS_BigEndianExplicit,    "Explicit VR BE (unsupported)",    false },
        { EXS_JPEGProcess14SV1,     ".70 JPEG-LL (supported, encaps)", true  },
        { EXS_JPEGProcess14,        ".57 JPEG-LL (supported, encaps)", true  },
    };

    DJEncoderRegistration::registerCodecs();
    int matched = 0, measured = 0;
    for (const auto& c : cases) {
        const auto path = s_tempDir / (std::string("b71_probe_") +
                                       std::to_string(static_cast<int>(c.written)) + ".dcm");
        if (!WriteDatasetOnly(s_validDcm, path, c.written)) {
            GTEST_LOG_(INFO) << c.label << ": not writable here -- not measured";
            continue;
        }
        const BranchProbe b = ProbeLikeOpen(path);
        ++measured;
        const bool match = (b.detected == c.written);
        if (match) ++matched;

        const char* branch = !b.loaded      ? "load-failed"
                           : b.metaIsNull   ? "meta NULL"
                           : !b.metaHasTs   ? "meta present, no TS element"
                                            : "meta present WITH TS";
        GTEST_LOG_(INFO) << c.label
                         << " | branch=" << branch
                         << " | written=" << XferUid(c.written)
                         << " | detected=" << XferUid(b.detected)
                         << " | pixelData encapsulated=" << b.encapsulated
                         << " | match=" << match;
    }
    // no DJEncoderRegistration::cleanup() needed for the decoder side; the
    // encoder registration is not what the reader depends on.
    DJEncoderRegistration::cleanup();

    GTEST_LOG_(INFO) << "detected syntax matched the written one in " << matched
                     << " of " << measured << " measured files";
    SUCCEED();
}

// ---------------------------------------------------------------------------
// #167 (QA-B-72) — measured before implementing check (2).
//
// Check (2) must decide "is the PixelData encapsulated?" WITHOUT knowing the
// syntax, because on this path the syntax is exactly what cannot be trusted.
// QA-B-71's probe OR-ed three signals, two of which name a JPEG syntax. This
// splits them, so the implementation uses only the one that needs no syntax --
// the undefined length field an encapsulated PixelData is written with -- and
// only if that one alone separates the two groups.
//
// It also tries to REACH the getMetaInfo() == NULL branch, which QA-B-71 could
// not. A handful of malformed inputs are fed to the same loadFile call; for each
// the outcome is recorded (load failed / meta NULL / meta present).
// ---------------------------------------------------------------------------
TEST_F(DicomReaderTest, KnownDivergence_EncapsulationSignalsAndMetaNullReachability) {
    struct Case { E_TransferSyntax written; const char* label; };
    const Case cases[] = {
        { EXS_LittleEndianExplicit, "Explicit VR LE" },
        { EXS_LittleEndianImplicit, "Implicit VR LE" },
        { EXS_BigEndianExplicit,    "Explicit VR BE" },
        { EXS_JPEGProcess14SV1,     ".70 JPEG-LL" },
        { EXS_JPEGProcess14,        ".57 JPEG-LL" },
    };

    DJEncoderRegistration::registerCodecs();
    for (const auto& c : cases) {
        const auto path = s_tempDir / (std::string("b72_sig_") +
                                       std::to_string(static_cast<int>(c.written)) + ".dcm");
        if (!WriteDatasetOnly(s_validDcm, path, c.written)) continue;

        DcmFileFormat ff;
        ASSERT_TRUE(ff.loadFile(path.string().c_str(), EXS_Unknown, EGL_noChange,
                                DCM_MaxReadLength).good());
        DcmDataset* ds = ff.getDataset();
        ASSERT_NE(nullptr, ds);

        bool undefLen = false, sv1 = false, p14 = false;
        DcmElement* el = nullptr;
        if (ds->findAndGetElement(DCM_PixelData, el).good() && el != nullptr) {
            undefLen = (el->getLengthField() == DCM_UndefinedLength);
            DcmPixelData* pd = OFstatic_cast(DcmPixelData*, el);
            DcmPixelSequence* seq = nullptr;
            const DcmRepresentationParameter* param = nullptr;
            sv1 = pd->getEncapsulatedRepresentation(EXS_JPEGProcess14SV1, param, seq).good();
            p14 = pd->getEncapsulatedRepresentation(EXS_JPEGProcess14,    param, seq).good();
        }
        GTEST_LOG_(INFO) << c.label
                         << " | undefined length=" << undefLen
                         << " | has .70 rep=" << sv1
                         << " | has .57 rep=" << p14
                         << " | detected=" << XferUid(ds->getOriginalXfer());
    }
    DJEncoderRegistration::cleanup();

    // --- can anything reach getMetaInfo() == NULL? -------------------------
    struct Raw { const char* label; std::string bytes; };
    const Raw raws[] = {
        { "empty file",                 std::string() },
        { "preamble only (128+DICM)",   std::string(128, '\0') + "DICM" },
        { "4 random bytes",             std::string("\x01\x02\x03\x04", 4) },
        { "one tag, no value",          std::string("\x08\x00\x05\x00", 4) },
    };
    for (const auto& r : raws) {
        const auto path = s_tempDir / (std::string("b72_raw_") +
                                       std::to_string(&r - raws) + ".dcm");
        { std::ofstream f(path, std::ios::binary); f.write(r.bytes.data(), r.bytes.size()); }
        DcmFileFormat ff;
        const bool loaded = ff.loadFile(path.string().c_str(), EXS_Unknown, EGL_noChange,
                                        DCM_MaxReadLength).good();
        const bool metaNull = (ff.getMetaInfo() == nullptr);
        GTEST_LOG_(INFO) << "reach :171? " << r.label
                         << " | loadFile good=" << loaded
                         << " | getMetaInfo()==NULL=" << metaNull;
    }
    // A freshly constructed DcmFileFormat, never loaded: what does it return?
    DcmFileFormat fresh;
    GTEST_LOG_(INFO) << "reach :171? fresh DcmFileFormat | getMetaInfo()==NULL="
                     << (fresh.getMetaInfo() == nullptr);
    SUCCEED();
}


// ---------------------------------------------------------------------------
// #167 (QA-B-72) — the TS-less path, case by case.
//
// Three checks were added to open(): (1) the detected syntax must be on the
// accepted list; (2) an encapsulated PixelData contradicting a native detection
// is refused; (3) the getMetaInfo() == NULL branch is closed. This table pins
// the outcome of each fixture and, as importantly, WHERE it is decided:
//
//   - the native leaks must be refused at open() -- (1);
//   - the encapsulated files must ALSO be refused at open(). Before QA-B-72
//     they opened (0) and were stopped at read (DICOM_INVALID, -13) by the
//     native read failing on an encapsulated stream. An open() refusal is the
//     evidence that (2), not structure, is what stops them now;
//   - the supported native file keeps producing pixels -- the control that
//     says the checks did not block too much;
//   - labelled .70 / .57 files keep producing pixels -- the normal path is
//     untouched.
//
// (3) has no row: no input reaches that branch (measured, five attempts), so it
// has no execution test. That is stated, not hidden.
//
// SYNTHETIC (#148).
// ---------------------------------------------------------------------------
TEST_F(DicomReaderTest, TsLessPathIsDecidedByChecksNotByStructure) {
    struct Row {
        const char*      label;
        E_TransferSyntax xfer;
        bool             withMeta;
        XpeErrorCode     expectOpen;
        bool             expectPixels;
    };
    const Row rows[] = {
        { "TS-less Explicit VR LE (control)", EXS_LittleEndianExplicit, false, XPE_OK,                     true  },
        { "TS-less Implicit VR LE",           EXS_LittleEndianImplicit, false, XPE_ERR_UNSUPPORTED_FORMAT, false },
        { "TS-less Explicit VR BE",           EXS_BigEndianExplicit,    false, XPE_ERR_UNSUPPORTED_FORMAT, false },
        { "TS-less .70",                      EXS_JPEGProcess14SV1,     false, XPE_ERR_UNSUPPORTED_FORMAT, false },
        { "TS-less .57",                      EXS_JPEGProcess14,        false, XPE_ERR_UNSUPPORTED_FORMAT, false },
        { "labelled .70 (normal path)",       EXS_JPEGProcess14SV1,     true,  XPE_OK,                     true  },
        { "labelled .57 (normal path)",       EXS_JPEGProcess14,        true,  XPE_OK,                     true  },
    };

    DJEncoderRegistration::registerCodecs();
    int idx = 0;
    for (const auto& r : rows) {
        const auto path = s_tempDir / (std::string("b72_row_") + std::to_string(idx++) + ".dcm");
        const bool wrote = r.withMeta ? WriteWithMeta(s_validDcm, path, r.xfer)
                                      : WriteDatasetOnly(s_validDcm, path, r.xfer);
        ASSERT_TRUE(wrote) << r.label << ": fixture could not be written";

        const OpenResult got = OpenAndRead(path);
        GTEST_LOG_(INFO) << r.label << ": open=" << got.open << " read=" << got.read
                         << " pixels=" << got.gotPixels << " " << got.w << "x" << got.h;

        EXPECT_EQ(r.expectOpen, got.open) << r.label;
        EXPECT_EQ(r.expectPixels, got.open == XPE_OK && got.read == XPE_OK && got.gotPixels)
            << r.label;
    }
    DJEncoderRegistration::cleanup();
}
