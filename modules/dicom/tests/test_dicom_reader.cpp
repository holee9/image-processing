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
#include "xpe/common/xpe_error.h"
#include <openjpeg.h>   // QA-B-182b: encode variant codestreams inside the test
#include <atomic>
#include <functional>
#include <map>
#include <set>
#include <cstdio>
#include <filesystem>
#include <thread>
#include <fstream>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// Test fixture: creates synthetic DICOM files before all tests
// ---------------------------------------------------------------------------
class DicomReaderTest : public ::testing::Test {
public:
    // Read access for the free helper functions of the Tc235 matrix (QA-B-187).
    static const fs::path& ValidDcm() { return s_validDcm; }
    static const fs::path& J2kDcm() { return s_j2kDcm; }
    static const fs::path& TempDir() { return s_tempDir; }

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

// A dataset written without a Part 10 meta header has no preamble and no "DICM", which REQ-DICOM-003 makes
// XPE_ERR_DICOM_INVALID (QA-B-207 C3, user decision on #251). It used to open and be read as Explicit VR Little Endian,
// and this test pinned that. The Part 10 file with the same dataset is the control: it opens.
TEST_F(DicomReaderTest, OpenDatasetWithoutMetaHeader_IsDicomInvalid) {
    auto path = s_tempDir / "reader_no_meta.dcm";
    {
        DcmFileFormat ff;
        ASSERT_TRUE(ff.loadFile(s_validDcm.string().c_str()).good());
        ASSERT_TRUE(ff.saveFile(path.string().c_str(), EXS_LittleEndianExplicit,
                                EET_ExplicitLength, EGL_recalcGL, EPD_withoutPadding,
                                0, 0, EWM_dataset).good());
    }
    XpeDicomHandle* handle = reinterpret_cast<XpeDicomHandle*>(0x1);   // must be overwritten with NULL
    EXPECT_EQ(XPE_ERR_DICOM_INVALID, xpe_dicom_open(path.string().c_str(), &handle));
    EXPECT_EQ(nullptr, handle);

    XpeDicomHandle* control = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(s_validDcm.string().c_str(), &control)) << "control: the Part 10 original opens";
    xpe_dicom_close(control);
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

// #174 (QA-B-76): the predictor every .57 fixture in this file is written with.
// The encoder default is 1, which makes a .57 stream byte-identical to a .70 one
// (QA-B-73) -- QA-B-75 found four cases whose ".57" rows were therefore the
// ".70" input a second time. Any predictor other than 1 keeps them apart.
const int kP14FixturePredictor = 2;

// Chooses the .57 representation with kP14FixturePredictor and FAILS the
// running test if the encoded stream still carries predictor 1. Defined below,
// next to the stream parser it uses.
bool ChooseDistinctP14Representation(DcmDataset* ds);

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
                    ChooseDistinctP14Representation(ds) &&
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

// JPEG-LL stream inspection helpers. Written for QA-B-73 (#168); moved up by
// QA-B-74 (#174) so the QA-B-46 predictor case below can check what it
// actually received.
namespace {

struct SosInfo {
    bool     found     = false;
    int      sofMarker = -1;   // 0xC0..0xCF (C3 = lossless, sequential, Huffman)
    int      ss        = -1;   // predictor selection value for lossless
    int      se        = -1;
    int      al        = -1;   // point transform
};

// Walk markers from SOI to the first SOS. Stops at SOS: the entropy-coded data
// after it is not marker-structured and is not needed for this question.
SosInfo ParseSofSos(const std::vector<Uint8>& b) {
    SosInfo r{};
    size_t i = 0;
    if (b.size() < 4 || b[0] != 0xFF || b[1] != 0xD8) return r;   // SOI
    i = 2;
    while (i + 4 <= b.size()) {
        if (b[i] != 0xFF) return r;
        const int m = b[i + 1];
        if (m == 0xD8 || (m >= 0xD0 && m <= 0xD7) || m == 0x01) { i += 2; continue; }
        const size_t len = (static_cast<size_t>(b[i + 2]) << 8) | b[i + 3];
        if (m >= 0xC0 && m <= 0xCF && m != 0xC4 && m != 0xC8 && m != 0xCC) {
            r.sofMarker = m;
        }
        if (m == 0xDA) {
            const size_t p = i + 4;                 // Ns
            if (p >= b.size()) return r;
            const size_t ns = b[p];
            const size_t q = p + 1 + 2 * ns;        // Ss
            if (q + 2 >= b.size()) return r;
            r.ss = b[q];
            r.se = b[q + 1];
            r.al = b[q + 2] & 0x0F;
            r.found = true;
            return r;
        }
        i += 2 + len;
    }
    return r;
}

struct FragmentProbe {
    bool                 ok = false;
    std::string          labelUid;
    std::vector<Uint8>   firstFragment;
    bool                 paramPresent = false;
    int                  paramPrediction = -1;
};

// Load a Part-10 file and return its first compressed fragment, looked up under
// the representation key of the syntax the meta-header names -- which is the
// key DCMTK files the pixel data under (QA-B-72 measured that it uses the label).
FragmentProbe FirstFragment(const fs::path& p) {
    FragmentProbe r{};
    DcmFileFormat ff;
    if (!ff.loadFile(p.string().c_str()).good()) return r;
    OFString ts;
    if (ff.getMetaInfo()->findAndGetOFString(DCM_TransferSyntaxUID, ts).bad()) return r;
    r.labelUid = ts.c_str();
    E_TransferSyntax key = DcmXfer(ts.c_str()).getXfer();

    DcmElement* el = nullptr;
    if (ff.getDataset()->findAndGetElement(DCM_PixelData, el).bad() || el == nullptr) return r;
    DcmPixelData* pd = OFstatic_cast(DcmPixelData*, el);
    DcmPixelSequence* seq = nullptr;
    const DcmRepresentationParameter* param = nullptr;
    if (pd->getEncapsulatedRepresentation(key, param, seq).bad() || seq == nullptr) return r;

    r.paramPresent = (param != nullptr);
    if (const auto* ll = dynamic_cast<const DJ_RPLossless*>(param)) {
        r.paramPrediction = ll->getPrediction();
    }

    DcmPixelItem* frag = nullptr;
    // Item 0 is the Basic Offset Table; item 1 is the first frame's fragment.
    if (seq->getItem(frag, 1).bad() || frag == nullptr) return r;
    Uint8* data = nullptr;
    if (frag->getUint8Array(data).bad() || data == nullptr) return r;
    r.firstFragment.assign(data, data + frag->getLength());
    r.ok = true;
    return r;
}

bool EncodeAs(const fs::path& src, const fs::path& dst, E_TransferSyntax xfer,
              int predictor /* 0 = encoder default */) {
    DJEncoderRegistration::registerCodecs();
    bool ok = false;
    {
        DcmFileFormat ff;
        if (ff.loadFile(src.string().c_str()).good()) {
            DcmDataset* ds = ff.getDataset();
            const DJ_RPLossless params(predictor == 0 ? 1 : predictor, 0);
            OFCondition rc = (predictor == 0)
                ? ds->chooseRepresentation(xfer, nullptr)
                : ds->chooseRepresentation(xfer, &params);
            ok = rc.good() && ds->canWriteXfer(xfer) &&
                 ff.saveFile(dst.string().c_str(), xfer).good();
        }
    }
    DJEncoderRegistration::cleanup();
    return ok;
}

// The guard QA-B-76 asked for: it lives in the helper, so reverting the
// predictor to 1 turns every caller red here instead of letting four cases
// quietly test the .70 input twice.
bool ChooseDistinctP14Representation(DcmDataset* ds) {
    const DJ_RPLossless params(kP14FixturePredictor, 0);
    if (ds->chooseRepresentation(EXS_JPEGProcess14, &params).bad()) return false;

    DcmElement* el = nullptr;
    if (ds->findAndGetElement(DCM_PixelData, el).bad() || el == nullptr) {
        ADD_FAILURE() << ".57 fixture: no PixelData after encoding";
        return false;
    }
    DcmPixelData* pd = OFstatic_cast(DcmPixelData*, el);
    DcmPixelSequence* seq = nullptr;
    DcmPixelItem* frag = nullptr;
    Uint8* data = nullptr;
    // The parameter is the lookup key: the representation was stored under it.
    if (pd->getEncapsulatedRepresentation(EXS_JPEGProcess14, &params, seq).bad() ||
        seq == nullptr || seq->getItem(frag, 1).bad() || frag == nullptr ||
        frag->getUint8Array(data).bad() || data == nullptr) {
        ADD_FAILURE() << ".57 fixture: no encoded fragment to inspect";
        return false;
    }
    const SosInfo sos = ParseSofSos(std::vector<Uint8>(data, data + frag->getLength()));
    if (!sos.found || sos.ss == 1) {
        ADD_FAILURE() << ".57 fixture carries predictor " << sos.ss
                      << " -- predictor 1 makes it the .70 stream again (QA-B-75)";
        return false;
    }
    return true;
}

}  // namespace

// ---------------------------------------------------------------------------
// #120 (QA-B-46) -> #174 (QA-B-74): JPEG-LL predictors, and what was really tested.
//
// QA-B-46 set out to widen QA-B-45's single JPEG-LL file to predictor
// selection values 1..7, encoding each through EXS_JPEGProcess14SV1 (.70) and
// guarding with EXPECT_GT(produced, 1). QA-B-73 then measured that DCMTK's .70
// encoder IGNORES the predictor argument and always writes predictor 1 -- all
// seven fixtures were one stream -- and the guard counted files produced, so it
// passed. The case logged "predictor variants produced and verified: 7 of 7"
// while testing one predictor. QA-B-74 replaced the guard with a distinct-stream
// count and it failed at 1, as predicted.
//
// That was not a gap in the encoder: .70 is "Selection Value 1" -- predictor 1
// is the only legal value for that syntax, so expecting variants from it was the
// error. The case therefore now says what it can: .70 carries predictor 1, and
// the reader decodes it exactly. The multi-predictor claim moved to
// ReadJpegLosslessProcess14AllPredictors_PixelExactAndDistinct, which uses .57
// (where predictors 2..7 are legal and the encoder honours them) and asserts
// that each fixture is a distinct stream carrying the predictor it names.
//
// Each decode is compared byte-for-byte against the uncompressed original.
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

TEST_F(DicomReaderTest, ReadJpegLosslessSV1_Predictor1PixelExact) {
    XpeDicomHandle* srcHandle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(s_validDcm.string().c_str(), &srcHandle));
    XpeImageBuffer expected{};
    ASSERT_EQ(XPE_OK, xpe_dicom_read_image(srcHandle, &expected));
    xpe_dicom_close(srcHandle);
    const size_t bytes = static_cast<size_t>(expected.width) * expected.height *
                         sizeof(uint16_t);

    const auto path = s_tempDir / "jpegll_sv1_pred1.dcm";
    std::string whyNot;
    ASSERT_TRUE(WriteJpegLosslessVariant(s_validDcm, path, 1, whyNot)) << whyNot;

    // What the case received, not what it asked for.
    const FragmentProbe fp = FirstFragment(path);
    ASSERT_TRUE(fp.ok);
    EXPECT_EQ("1.2.840.10008.1.2.4.70", fp.labelUid);
    const SosInfo sos = ParseSofSos(fp.firstFragment);
    ASSERT_TRUE(sos.found);
    EXPECT_EQ(0xC3, sos.sofMarker);
    EXPECT_EQ(1, sos.ss) << ".70 must carry predictor 1";

    XpeDicomHandle* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(path.string().c_str(), &handle));
    XpeImageBuffer actual{};
    ASSERT_EQ(XPE_OK, xpe_dicom_read_image(handle, &actual));
    xpe_dicom_close(handle);
    ASSERT_EQ(expected.width, actual.width);
    ASSERT_EQ(expected.height, actual.height);
    EXPECT_EQ(0, std::memcmp(expected.data, actual.data, bytes))
        << "lossless round trip must be bit-exact";

    xpe_free_image(&actual);
    xpe_free_image(&expected);
}

// Records the encoder behaviour QA-B-73 measured, so a DCMTK upgrade that starts
// honouring the argument -- and would thereby write predictor != 1 under a .70
// label, which the syntax forbids -- surfaces here instead of silently widening
// or corrupting other cases.
TEST_F(DicomReaderTest, KnownDivergence_Sv1EncoderIgnoresPredictorArgument) {
    std::set<std::vector<Uint8>> streams;
    int produced = 0;
    for (int predictor = 1; predictor <= 7; ++predictor) {
        const auto path = s_tempDir / ("jpegll_sv1_req" + std::to_string(predictor) + ".dcm");
        std::string whyNot;
        if (!WriteJpegLosslessVariant(s_validDcm, path, predictor, whyNot)) continue;
        ++produced;
        const FragmentProbe fp = FirstFragment(path);
        ASSERT_TRUE(fp.ok);
        const SosInfo sos = ParseSofSos(fp.firstFragment);
        EXPECT_EQ(1, sos.ss) << "requested predictor " << predictor
                             << " -- the .70 encoder wrote it into the stream";
        streams.insert(fp.firstFragment);
    }
    GTEST_LOG_(INFO) << ".70 encoder: requests=" << produced
                     << " distinct streams=" << streams.size();
    EXPECT_EQ(1u, streams.size());
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
// #181 (QA-B-93): retention is measured on the CRT heap (heap_growth.h).
#include "heap_growth.h"

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
// Method: CRT heap growth across many repetitions (#181, heap_growth.h) --
// warm up so first-touch and allocator arenas settle, take a baseline, then
// loop. It counts blocks still allocated, so it cannot name which object
// leaked; what it can do is fail when one does. The control case after this
// one leaks on purpose inside the same loop and must be caught.
// (Until QA-B-93 this comment promised a "sensitivity probe below" that did
// not exist in this file; the control case is that probe.)
// ---------------------------------------------------------------------------
TEST_F(DicomJ2kFailureTest, FailurePathsDoNotGrowCrtHeap) {
#if !defined(_WIN32)
    GTEST_SKIP() << "CRT heap walk is Windows-only in this build";
#endif
    const auto garbage   = s_tempDir / "j2k_leak_garbage.dcm";
    const auto truncated = s_tempDir / "j2k_leak_truncated.dcm";
    ASSERT_TRUE(WriteCraftedJ2kFile(s_validDcm, garbage, s_bitstream,
                                    FragmentShape::kGarbage));
    ASSERT_TRUE(WriteCraftedJ2kFile(s_validDcm, truncated, s_bitstream,
                                    FragmentShape::kTruncated));

    auto one_cycle = [&](int) {
        (void)ReadCrafted(garbage);
        (void)ReadCrafted(truncated);
    };
    const heap_growth::Growth g = heap_growth::Measure(one_cycle);
    GTEST_LOG_(INFO) << heap_growth::Describe(g);
    EXPECT_LT(g.heap.blocks, heap_growth::MaxBlocks(g.cycles))
        << "failed decodes left blocks allocated -- a failure path is not releasing what it allocated";
    EXPECT_LT(g.heap.bytes, heap_growth::kMaxBytes)
        << "failed decodes left bytes allocated -- a failure path is not releasing what it allocated";
}

// #181 (QA-B-93) control for the case above.
TEST_F(DicomJ2kFailureTest, FailurePaths_ControlLeakIsCaught) {
#if !defined(_WIN32)
    GTEST_SKIP() << "CRT heap walk is Windows-only in this build";
#endif
    const auto garbage = s_tempDir / "j2k_leak_garbage_control.dcm";
    ASSERT_TRUE(WriteCraftedJ2kFile(s_validDcm, garbage, s_bitstream,
                                    FragmentShape::kGarbage));
    auto one_cycle = [&](int) { (void)ReadCrafted(garbage); };
    const heap_growth::Growth g = heap_growth::Measure(one_cycle, 64);
    GTEST_LOG_(INFO) << "control 64 B/cycle: " << heap_growth::Describe(g);
    EXPECT_GE(g.heap.blocks, g.cycles * 9 / 10);
    EXPECT_GE(g.heap.bytes, 64LL * g.cycles * 9 / 10);
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
// #147 (QA-B-68) — .57 is accepted. Pixels, not just the absence of -7.
//
// Adding the UID to kSupportedTransferSyntaxes removes the refusal. That is NOT
// the same as reading the file, so this case asserts the pixels, and asserts
// them against the source: JPEG Lossless is lossless, so a byte-exact match is
// available and anything less would be a decode that ran without being right.
//
// Two corrections, recorded here because this header once said otherwise:
//   - QA-B-70: the list entry and the decode branch were changed in ONE edit, so
//     what an accepted .57 file does WITHOUT the branch was never run. An
//     earlier version of this paragraph described that as measured; it was not.
//   - QA-B-73: this fixture uses the encoder default, predictor 1, and a
//     predictor-1 .57 stream is byte-identical to a .70 stream. This case
//     therefore proves nothing that the .70 case does not. What .57 adds --
//     predictors 2..7 -- is tested by
//     ReadJpegLosslessProcess14AllPredictors_PixelExactAndDistinct (QA-B-74).
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

// Dataset only -- no preamble, no group-2 elements. Before QA-B-207 C3 this is what reached the TS-less branch of open()
// (measured by QA-B-71); a bare dataset is now refused at the door (XPE_ERR_DICOM_INVALID, REQ-DICOM-003), whatever its
// syntax. The probes that load it with DCMTK directly are unaffected.
bool WriteDatasetOnly(const fs::path& src, const fs::path& dst, E_TransferSyntax xfer) {
    DcmFileFormat ff;
    if (!ff.loadFile(src.string().c_str()).good()) return false;
    DcmDataset* ds = ff.getDataset();
    if (ds == nullptr) return false;
    if (xfer == EXS_JPEGProcess14) {
        if (!ChooseDistinctP14Representation(ds)) return false;
    } else if (!ds->chooseRepresentation(xfer, nullptr).good()) {
        return false;
    }
    if (!ds->canWriteXfer(xfer)) return false;
    return ds->saveFile(dst.string().c_str(), xfer).good();
}


// A Part 10 file (preamble, "DICM", meta header) whose meta header carries NO TransferSyntaxUID (0002,0010): the TS-less
// branch of open() is reached only by a file like this one since QA-B-207 C3. DCMTK can parse it only when the dataset is
// Explicit VR Little Endian (the meta header is read as that and the dataset follows in the same encoding), which covers
// the native Explicit LE file and the encapsulated JPEG ones; Implicit LE and Big Endian datasets behind such a meta
// header do not parse.
bool WriteTsLessPart10(const fs::path& src, const fs::path& dst, E_TransferSyntax xfer) {
    DcmFileFormat ff;
    if (!ff.loadFile(src.string().c_str()).good()) return false;
    DcmDataset* ds = ff.getDataset();
    if (ds == nullptr) return false;
    if (xfer == EXS_JPEGProcess14) {
        if (!ChooseDistinctP14Representation(ds)) return false;
    } else if (!ds->chooseRepresentation(xfer, nullptr).good()) {
        return false;
    }
    if (!ds->canWriteXfer(xfer)) return false;
    DcmMetaInfo* meta = ff.getMetaInfo();
    if (meta == nullptr) return false;
    meta->findAndDeleteElement(DCM_TransferSyntaxUID);
    return ff.saveFile(dst.string().c_str(), xfer, EET_ExplicitLength, EGL_recalcGL, EPD_withoutPadding, 0, 0,
                       EWM_dontUpdateMeta).good();
}

// Full Part-10 file, meta-header included.
bool WriteWithMeta(const fs::path& src, const fs::path& dst, E_TransferSyntax xfer) {
    DcmFileFormat ff;
    if (!ff.loadFile(src.string().c_str()).good()) return false;
    DcmDataset* ds = ff.getDataset();
    if (ds == nullptr) return false;
    if (xfer == EXS_JPEGProcess14) {
        if (!ChooseDistinctP14Representation(ds)) return false;
    } else if (!ds->chooseRepresentation(xfer, nullptr).good()) {
        return false;
    }
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
    GTEST_LOG_(INFO) << "control: bare dataset, Explicit VR LE (SUPPORTED) open="
                     << sanity.open << " read=" << sanity.read
                     << " pixels=" << sanity.gotPixels
                     << " " << sanity.w << "x" << sanity.h;
    // QA-B-207 C3: this control used to be "a meta-less file in a SUPPORTED syntax yields pixels". A bare dataset is now
    // refused at the door whatever its syntax (REQ-DICOM-003), so the control is the REFUSAL: the supported syntax is not
    // an exception. The same fixture with a Part 10 header is the proof that the writer is not what is being refused.
    ASSERT_EQ(XPE_ERR_DICOM_INVALID, sanity.open)
        << "a bare dataset must be refused at the door even in a SUPPORTED syntax";
    const auto labelledControl = s_tempDir / "b69_labelled_explicitLE.dcm";
    ASSERT_TRUE(WriteWithMeta(s_validDcm, labelledControl, EXS_LittleEndianExplicit));
    const OpenResult labelledSanity = OpenAndRead(labelledControl);
    ASSERT_TRUE(labelledSanity.open == XPE_OK && labelledSanity.read == XPE_OK && labelledSanity.gotPixels)
        << "the Part 10 version of the same fixture does not produce pixels -- the fixture writer is broken";

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
        // QA-B-207 C3: the bare dataset is refused by the door (DICOM_INVALID), not by the accepted-list check
        EXPECT_EQ(XPE_ERR_DICOM_INVALID, bare.open) << c.name << ": a bare dataset must be refused at the door";

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
    // QA-B-207 C3: the control is the refusal of a bare dataset in a SUPPORTED syntax (see the sibling case)
    ASSERT_EQ(XPE_ERR_DICOM_INVALID, sanity.open)
        << "a bare dataset must be refused at the door even in a SUPPORTED syntax";

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
        // QA-B-207 C3: what this record logged as a count is now asserted: every bare dataset is refused at the door
        EXPECT_EQ(XPE_ERR_DICOM_INVALID, bare.open) << c.name << ": a bare dataset must be refused at the door";
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
    EXPECT_EQ(0, leaked) << "QA-B-207 C3: no bare dataset yields pixels any more";
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
    // QA-B-207 C3: a bare dataset (no preamble, no "DICM") is refused at the door before any syntax is considered, so the
    // checks of this table are reached by a Part 10 file whose meta header lacks the TransferSyntaxUID (kTsLess). DCMTK
    // parses such a file only for an Explicit-VR-LE-encoded dataset (native Explicit LE, and the encapsulated JPEG ones);
    // Implicit LE and Big Endian datasets behind that meta header do not parse and are DICOM_INVALID.
    enum Kind { kBare, kTsLess, kLabelled };
    struct Row {
        const char*      label;
        E_TransferSyntax xfer;
        Kind             kind;
        XpeErrorCode     expectOpen;
        bool             expectPixels;
    };
    const Row rows[] = {
        { "bare dataset, Explicit VR LE",     EXS_LittleEndianExplicit, kBare,     XPE_ERR_DICOM_INVALID,     false },
        { "bare dataset, Implicit VR LE",     EXS_LittleEndianImplicit, kBare,     XPE_ERR_DICOM_INVALID,     false },
        { "bare dataset, Explicit VR BE",     EXS_BigEndianExplicit,    kBare,     XPE_ERR_DICOM_INVALID,     false },
        { "bare dataset, .70",                EXS_JPEGProcess14SV1,     kBare,     XPE_ERR_DICOM_INVALID,     false },
        { "bare dataset, .57",                EXS_JPEGProcess14,        kBare,     XPE_ERR_DICOM_INVALID,     false },
        { "TS-less Part 10, Explicit VR LE (control)", EXS_LittleEndianExplicit, kTsLess, XPE_OK,              true  },
        { "TS-less Part 10, Implicit VR LE",  EXS_LittleEndianImplicit, kTsLess,   XPE_ERR_DICOM_INVALID,     false },
        { "TS-less Part 10, Explicit VR BE",  EXS_BigEndianExplicit,    kTsLess,   XPE_ERR_DICOM_INVALID,     false },
        { "TS-less Part 10, .70",             EXS_JPEGProcess14SV1,     kTsLess,   XPE_ERR_UNSUPPORTED_FORMAT, false },
        { "TS-less Part 10, .57",             EXS_JPEGProcess14,        kTsLess,   XPE_ERR_UNSUPPORTED_FORMAT, false },
        { "labelled .70 (normal path)",       EXS_JPEGProcess14SV1,     kLabelled, XPE_OK,                     true  },
        { "labelled .57 (normal path)",       EXS_JPEGProcess14,        kLabelled, XPE_OK,                     true  },
    };

    DJEncoderRegistration::registerCodecs();
    int idx = 0;
    for (const auto& r : rows) {
        const auto path = s_tempDir / (std::string("b72_row_") + std::to_string(idx++) + ".dcm");
        const bool wrote = r.kind == kLabelled ? WriteWithMeta(s_validDcm, path, r.xfer)
                         : r.kind == kTsLess   ? WriteTsLessPart10(s_validDcm, path, r.xfer)
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

// ---------------------------------------------------------------------------
// #168 (QA-B-73) — can the bitstream tell .57 from .70?  (measurement only)
//
// .70 is JPEG Lossless Process 14 with Selection Value 1: the predictor is
// fixed to 1. .57 is Process 14 with ANY first-order predictor 1..7. In a
// lossless JPEG stream the predictor is the Ss byte of the SOS segment
// (FF DA, Ls, Ns, Ns x {Cs, Td/Ta}, Ss, Se, Ah/Al).
//
// THE TRAP, checked first: a .57 stream encoded with predictor 1 may be
// byte-for-byte a .70 stream. If so, ".70 bytes under a .57 label" is not a
// contradiction at all -- it is a legal .57 file -- and #168's premise needs
// restating. This case therefore compares whole first fragments, not just Ss.
//
// Also asked: does DCMTK hand back the predictor it decoded (a representation
// parameter on the loaded dataset), or must the stream be read?
//
// SYNTHETIC (#148): every stream here is DCMTK's own encoder output.
// ---------------------------------------------------------------------------

TEST_F(DicomReaderTest, KnownDivergence_JpegLosslessPredictorInBitstreamIsMeasured) {
    struct Row { const char* label; E_TransferSyntax xfer; int predictor; };
    std::vector<Row> rows = {
        { ".70 default",  EXS_JPEGProcess14SV1, 0 },
        { ".57 default",  EXS_JPEGProcess14,    0 },
    };
    for (int p = 1; p <= 7; ++p) rows.push_back({ nullptr, EXS_JPEGProcess14SV1, p });
    for (int p = 1; p <= 7; ++p) rows.push_back({ nullptr, EXS_JPEGProcess14,    p });

    std::map<std::string, std::vector<Uint8>> fragments;
    int idx = 0;
    for (const auto& r : rows) {
        const std::string name = r.label ? std::string(r.label)
            : std::string(r.xfer == EXS_JPEGProcess14SV1 ? ".70" : ".57") +
              " pred=" + std::to_string(r.predictor);
        const auto path = s_tempDir / ("b73_" + std::to_string(idx++) + ".dcm");
        if (!EncodeAs(s_validDcm, path, r.xfer, r.predictor)) {
            GTEST_LOG_(INFO) << name << ": encoder refused -- not measured";
            continue;
        }
        const FragmentProbe fp = FirstFragment(path);
        if (!fp.ok) {
            GTEST_LOG_(INFO) << name << ": could not read first fragment";
            continue;
        }
        const SosInfo s = ParseSofSos(fp.firstFragment);
        fragments[name] = fp.firstFragment;
        char sof[8];
        std::snprintf(sof, sizeof(sof), "0x%02X", s.sofMarker);
        GTEST_LOG_(INFO) << name
                         << " | label=" << fp.labelUid
                         << " | SOF=" << sof
                         << " | SOS Ss(predictor)=" << s.ss
                         << " Se=" << s.se << " Al=" << s.al
                         << " | fragment bytes=" << fp.firstFragment.size()
                         << " | DCMTK param present=" << fp.paramPresent
                         << " prediction=" << fp.paramPrediction;
    }

    // THE TRAP: are a .70 stream and a predictor-1 .57 stream the same bytes?
    auto same = [&](const std::string& a, const std::string& b) {
        if (!fragments.count(a) || !fragments.count(b)) return std::string("n/a");
        return std::string(fragments[a] == fragments[b] ? "IDENTICAL" : "differ");
    };
    GTEST_LOG_(INFO) << "first fragment  .70 default  vs .57 pred=1  : " << same(".70 default", ".57 pred=1");
    GTEST_LOG_(INFO) << "first fragment  .70 default  vs .57 default : " << same(".70 default", ".57 default");
    GTEST_LOG_(INFO) << "first fragment  .70 pred=1   vs .57 pred=1  : " << same(".70 pred=1", ".57 pred=1");
    GTEST_LOG_(INFO) << "first fragment  .70 default  vs .70 pred=1  : " << same(".70 default", ".70 pred=1");
    GTEST_LOG_(INFO) << "first fragment  .70 pred=1   vs .70 pred=2  : " << same(".70 pred=1", ".70 pred=2");

    // The #168 fixture itself: .70 bytes relabelled .57.
    const auto genuine70 = s_tempDir / "b73_genuine70.dcm";
    const auto relabel   = s_tempDir / "b73_relabel57.dcm";
    if (WriteJpegLosslessCopy(s_validDcm, genuine70)) {
        DcmFileFormat ff;
        ASSERT_TRUE(ff.loadFile(genuine70.string().c_str()).good());
        ASSERT_TRUE(ff.getMetaInfo()->putAndInsertString(DCM_TransferSyntaxUID,
                                                         "1.2.840.10008.1.2.4.57").good());
        ASSERT_TRUE(ff.saveFile(relabel.string().c_str(), EXS_JPEGProcess14SV1,
                                EET_ExplicitLength, EGL_recalcGL, EPD_withoutPadding,
                                0, 0, EWM_dontUpdateMeta).good());
        const FragmentProbe fp = FirstFragment(relabel);
        const SosInfo s = fp.ok ? ParseSofSos(fp.firstFragment) : SosInfo{};
        GTEST_LOG_(INFO) << "#168 fixture (.70 bytes, .57 label) | read ok=" << fp.ok
                         << " | label=" << fp.labelUid
                         << " | SOS Ss=" << s.ss
                         << " | fragment bytes=" << fp.firstFragment.size();
    }
    SUCCEED();
}

// ---------------------------------------------------------------------------
// #174 (QA-B-74) — the first test of what .57 can carry that .70 cannot.
//
// .57 (JPEG Lossless, Process 14) allows first-order predictors 1..7; .70 fixes
// the predictor at 1. QA-B-73 measured that a predictor-1 .57 stream is
// byte-identical to a .70 stream, so every .57 test before this one exercised
// nothing that .70 does not. This case exercises predictors 2..7.
//
// TWO things are asserted per fixture, and the second is the point of #174:
//   1. the decoded frame is pixel-exact against the uncompressed source;
//   2. the fixture IS what the case claims to test -- its SOS Ss byte equals the
//      requested predictor, and its first fragment differs from every other
//      fixture's. QA-B-73 found a predictor-variant test that handed seven
//      requests to an encoder which ignored six of them, and whose guard counted
//      files produced rather than streams received. Size is not a proxy: in
//      QA-B-73 predictor 5 and predictor 1 produced fragments of the same length.
//
// The distinctness check is written to be falsifiable: feeding the same
// predictor more than once must turn it red (QA-B-74 §3).
//
// SYNTHETIC (#148): DCMTK's own encoder output.
// ---------------------------------------------------------------------------
namespace {

// The predictor list the case runs over. Kept as data so the falsification --
// predictor 1 six times -- is a one-line change here and nothing else.
const std::vector<int> kP14Predictors = {1, 2, 3, 4, 5, 6, 7};

}  // namespace

TEST_F(DicomReaderTest, ReadJpegLosslessProcess14AllPredictors_PixelExactAndDistinct) {
    XpeDicomHandle* srcHandle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(s_validDcm.string().c_str(), &srcHandle));
    XpeImageBuffer expected{};
    ASSERT_EQ(XPE_OK, xpe_dicom_read_image(srcHandle, &expected));
    xpe_dicom_close(srcHandle);
    const size_t bytes = static_cast<size_t>(expected.width) * expected.height *
                         sizeof(uint16_t);

    std::set<std::vector<Uint8>> distinctFragments;
    int produced = 0;
    int pixelExact = 0;

    for (int predictor : kP14Predictors) {
        SCOPED_TRACE(".57 predictor " + std::to_string(predictor));
        const auto path = s_tempDir / ("b74_p14_pred" + std::to_string(predictor) + "_" +
                                       std::to_string(produced) + ".dcm");
        ASSERT_TRUE(EncodeAs(s_validDcm, path, EXS_JPEGProcess14, predictor))
            << "DCMTK did not encode .57 with this predictor -- QA-B-73 measured that it can";
        ++produced;

        // --- (2) is this fixture what it claims to be? ------------------------
        const FragmentProbe fp = FirstFragment(path);
        ASSERT_TRUE(fp.ok) << "first fragment not readable";
        EXPECT_EQ("1.2.840.10008.1.2.4.57", fp.labelUid);
        const SosInfo sos = ParseSofSos(fp.firstFragment);
        ASSERT_TRUE(sos.found) << "no SOS in the first fragment";
        EXPECT_EQ(0xC3, sos.sofMarker) << "not a lossless (SOF3) stream";
        EXPECT_EQ(predictor, sos.ss)
            << "the stream carries a different predictor than was requested";
        distinctFragments.insert(fp.firstFragment);

        // --- (1) does the reader decode it exactly? ---------------------------
        XpeDicomHandle* handle = nullptr;
        const XpeErrorCode ecOpen = xpe_dicom_open(path.string().c_str(), &handle);
        XpeImageBuffer actual{};
        XpeErrorCode ecRead = XPE_ERR_INTERNAL;
        size_t differing = static_cast<size_t>(-1);
        if (ecOpen == XPE_OK) {
            ecRead = xpe_dicom_read_image(handle, &actual);
            if (ecRead == XPE_OK && actual.width == expected.width &&
                actual.height == expected.height) {
                const auto* a = static_cast<const uint16_t*>(expected.data);
                const auto* b = static_cast<const uint16_t*>(actual.data);
                differing = 0;
                for (size_t i = 0; i < bytes / sizeof(uint16_t); ++i) {
                    if (a[i] != b[i]) ++differing;
                }
            }
        }
        GTEST_LOG_(INFO) << ".57 predictor " << predictor
                         << " | Ss=" << sos.ss
                         << " | fragment bytes=" << fp.firstFragment.size()
                         << " | open=" << ecOpen << " read=" << ecRead
                         << " | " << actual.width << "x" << actual.height
                         << " | differing pixels=" << static_cast<long long>(differing);

        EXPECT_EQ(XPE_OK, ecOpen);
        EXPECT_EQ(XPE_OK, ecRead);
        EXPECT_EQ(0u, differing) << "not pixel-exact";
        if (differing == 0) ++pixelExact;

        if (ecRead == XPE_OK) xpe_free_image(&actual);
        xpe_dicom_close(handle);
    }

    GTEST_LOG_(INFO) << ".57 predictor fixtures: produced=" << produced
                     << " distinct streams=" << distinctFragments.size()
                     << " pixel-exact=" << pixelExact;

    // The guard #174 is about: count what was RECEIVED, not what was requested.
    EXPECT_EQ(static_cast<size_t>(produced), distinctFragments.size())
        << "two or more fixtures are the same stream -- the case is testing fewer "
           "predictors than it names";
    EXPECT_GE(distinctFragments.size(), 2u)
        << "only one distinct stream -- nothing beyond what .70 carries was tested";

    xpe_free_image(&expected);
}

// ===========================================================================
// QA-B-182 (#235, QA-B-180): what readImage refuses, and what it deliberately still returns as stored
//
// QA-B-180 measured that the reader copied the 16-bit words of any dataset as they were and answered OK, so a
// signed, multi-frame or RGB file came back as wrong pixels with no signal (and an 8-bit file was refused as a
// corrupt "short" file). The datasets below are derived from the module's own valid file with DCMTK inside the
// test; no .dcm is committed.
// ===========================================================================
namespace {

template <class F>
fs::path MakeScopeVariant(const fs::path& src, const char* name, F mutate) {
    DcmFileFormat ff;
    EXPECT_TRUE(ff.loadFile(src.string().c_str()).good());
    mutate(ff.getDataset());
    const fs::path out = src.parent_path() / (std::string("scope_") + name + ".dcm");
    EXPECT_TRUE(ff.saveFile(out.string().c_str(), EXS_LittleEndianExplicit).good());
    return out;
}

struct ScopeRead {
    XpeErrorCode open = XPE_OK;
    XpeErrorCode read = XPE_OK;
    XpeErrorCode metaAfter = XPE_OK;
    std::vector<uint16_t> words;
    bool outUntouchedOnFailure = true;
    XpePixelFormat format = XPE_PIXEL_FLOAT32;   // descriptor of a successful read
    uint32_t bitsAllocated = 0;
    uint32_t bitsStored = 0;
};

/** Opens, reads (into a sentinel-filled buffer), then asks for the metadata on the SAME handle. */
ScopeRead ReadScope(const fs::path& p) {
    ScopeRead r;
    XpeDicomHandle* h = nullptr;
    r.open = xpe_dicom_open(p.string().c_str(), &h);
    if (r.open != XPE_OK) return r;
    XpeImageBuffer img{};
    img.width = 7;
    img.height = 9;
    img.bitsAllocated = 99;
    img.bitsStored = 98;
    img.format = XPE_PIXEL_FLOAT32;
    img.data = nullptr;
    img.dataSize = 5;
    r.read = xpe_dicom_read_image(h, &img);
    if (r.read == XPE_OK) {
        const size_t n = static_cast<size_t>(img.width) * img.height;
        const uint16_t* d = static_cast<const uint16_t*>(img.data);
        r.words.assign(d, d + n);
        r.format = img.format;
        r.bitsAllocated = img.bitsAllocated;
        r.bitsStored = img.bitsStored;
        xpe_free_image(&img);
    } else {
        r.outUntouchedOnFailure = img.width == 7 && img.height == 9 && img.bitsAllocated == 99 &&
                                  img.bitsStored == 98 && img.format == XPE_PIXEL_FLOAT32 &&
                                  img.data == nullptr && img.dataSize == 5;
    }
    XpeImageMetadata m{};
    r.metaAfter = xpe_dicom_get_metadata(h, &m);
    xpe_dicom_close(h);
    return r;
}

std::vector<uint16_t> Words(const fs::path& p) {
    return ReadScope(p).words;
}

void PutWords(DcmDataset* ds, const std::vector<uint16_t>& w) {
    EXPECT_TRUE(ds->putAndInsertUint16Array(DCM_PixelData, w.data(), static_cast<unsigned long>(w.size())).good());
}

}  // namespace

// The control: the module's own file reads as before, and spelling out the defaults changes nothing.
TEST_F(DicomReaderTest, Scope_OrdinaryUnsignedSingleFrame16BitStillReads) {
    const ScopeRead r = ReadScope(s_validDcm);
    EXPECT_EQ(XPE_OK, r.read);
    EXPECT_EQ(256u * 256u, r.words.size());
    const fs::path explicitDefaults = MakeScopeVariant(s_validDcm, "explicit_defaults", [](DcmDataset* ds) {
        ds->putAndInsertString(DCM_NumberOfFrames, "1");
        ds->putAndInsertUint16(DCM_SamplesPerPixel, 1);
        ds->putAndInsertUint16(DCM_PixelRepresentation, 0);
        ds->putAndInsertUint16(DCM_BitsAllocated, 16);
    });
    const ScopeRead e = ReadScope(explicitDefaults);
    EXPECT_EQ(XPE_OK, e.read);
    EXPECT_EQ(r.words, e.words) << "stating the defaults must not change the pixels";
}

// PS3.3 C.7.6.3.1.1: Samples per Pixel, Pixel Representation and Bits Allocated are Type 1 (required, with a value).
// There is no default for them. Absent, empty: the file is malformed (QA-B-182b; QA-B-182 had them default).
TEST_F(DicomReaderTest, Scope_AbsentOrEmptyRequiredAttributesAreMalformed) {
    struct Required { const char* name; DcmTagKey key; };
    const Required required[] = {
        {"samples_per_pixel", DCM_SamplesPerPixel},
        {"pixel_representation", DCM_PixelRepresentation},
        {"bits_allocated", DCM_BitsAllocated},
    };
    for (const Required& r : required) {
        const fs::path absent = MakeScopeVariant(s_validDcm, (std::string("absent_") + r.name).c_str(),
                                                 [&](DcmDataset* ds) { delete ds->remove(r.key); });
        const ScopeRead a = ReadScope(absent);
        EXPECT_EQ(XPE_ERR_DICOM_INVALID, a.read) << r.name << " absent";
        EXPECT_TRUE(a.outUntouchedOnFailure) << r.name << " absent";
        EXPECT_EQ(XPE_OK, a.metaAfter) << r.name << " absent: the handle still serves its metadata";

        const fs::path empty = MakeScopeVariant(s_validDcm, (std::string("empty_") + r.name).c_str(),
                                                [&](DcmDataset* ds) { ds->putAndInsertString(r.key, ""); });
        const ScopeRead e = ReadScope(empty);
        EXPECT_EQ(XPE_ERR_DICOM_INVALID, e.read) << r.name << " empty";
        EXPECT_TRUE(e.outUntouchedOnFailure) << r.name << " empty";
    }
}

// Number of Frames is the one attribute that may be absent (Multi-frame Module): absent is a single frame.
TEST_F(DicomReaderTest, Scope_AbsentNumberOfFramesIsASingleFrame) {
    const std::vector<uint16_t> baseline = Words(s_validDcm);
    const fs::path stripped = MakeScopeVariant(s_validDcm, "absent_frames", [](DcmDataset* ds) {
        delete ds->remove(DCM_NumberOfFrames);
    });
    const ScopeRead r = ReadScope(stripped);
    EXPECT_EQ(XPE_OK, r.read);
    EXPECT_EQ(baseline, r.words);
}

// Uncompressed pixel data is copied as 16-bit words, so only a 16-bit declaration can be honest.
// Bits Allocated {1, 8, 12, 16, 32}: 16 is the control. 1, 8 and 32 are well-formed files this reader cannot return
// (unsupported); 12 is not a value PS3.5 8.1.1 allows at all ("shall either be 1, or a multiple of 8"), so it is a
// malformed dataset (QA-B-182e: it was filed as unsupported before the standard was read). Never a read of 16-bit
// words out of data that is not.
TEST_F(DicomReaderTest, Scope_OnlySixteenBitAllocationIsReadFromNativePixelData) {
    const std::vector<uint16_t> baseline = Words(s_validDcm);
    for (int bits : {1, 8, 12, 16, 32}) {
        const fs::path p = MakeScopeVariant(s_validDcm, (std::string("alloc_") + std::to_string(bits)).c_str(),
                                            [&](DcmDataset* ds) {
            ds->putAndInsertUint16(DCM_BitsAllocated, static_cast<Uint16>(bits));
            ds->putAndInsertUint16(DCM_BitsStored, static_cast<Uint16>(bits < 16 ? bits : 16));
            ds->putAndInsertUint16(DCM_HighBit, static_cast<Uint16>((bits < 16 ? bits : 16) - 1));
        });
        const ScopeRead r = ReadScope(p);
        if (bits == 16) {
            EXPECT_EQ(XPE_OK, r.read) << "control: 16 bits allocated reads";
            EXPECT_EQ(baseline, r.words);
        } else {
            EXPECT_EQ(bits == 12 ? XPE_ERR_DICOM_INVALID : XPE_ERR_UNSUPPORTED_FORMAT, r.read) << "BitsAllocated=" << bits;
            EXPECT_TRUE(r.outUntouchedOnFailure) << "BitsAllocated=" << bits;
            EXPECT_EQ(XPE_OK, r.metaAfter) << "BitsAllocated=" << bits;
        }
    }
    // 32 bits with pixel data of the right size for 32 bits: no longer read as 16-bit words.
    const fs::path p32 = MakeScopeVariant(s_validDcm, "alloc_32_real", [](DcmDataset* ds) {
        ds->putAndInsertUint16(DCM_BitsAllocated, 32);
        ds->putAndInsertUint16(DCM_BitsStored, 32);
        ds->putAndInsertUint16(DCM_HighBit, 31);
        std::vector<uint16_t> words(256u * 256u * 2u, 0x1234);
        ds->putAndInsertUint16Array(DCM_PixelData, words.data(), static_cast<unsigned long>(words.size()));
    });
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, ReadScope(p32).read);
}

// With 16 bits allocated, Bits Stored must fit and High Bit must be Bits Stored - 1: a High Bit above that means
// the significant bits are not the low ones, and the words would come back unshifted. PS3.5 8.1.1 makes both a
// "shall" (QA-B-182e), so a breach is a malformed dataset, not an unsupported feature.
TEST_F(DicomReaderTest, Scope_BitsStoredAndHighBitMustDescribeLowBitsOfA16BitWord) {
    struct Case { int stored, high; bool ok; };
    const Case cases[] = {
        {16, 15, true}, {12, 11, true}, {1, 0, true},            // the low bits: fine
        {12, 15, false},                                          // data held in the high 12 bits
        {12, 12, false}, {16, 14, false},                         // High Bit is not Bits Stored - 1
        {17, 16, false}, {0, 0xFFFF, false},                      // wider than the word / nothing stored
    };
    for (const Case& c : cases) {
        const fs::path p = MakeScopeVariant(s_validDcm,
            (std::string("stored_") + std::to_string(c.stored) + "_" + std::to_string(c.high)).c_str(),
            [&](DcmDataset* ds) {
                ds->putAndInsertUint16(DCM_BitsStored, static_cast<Uint16>(c.stored));
                ds->putAndInsertUint16(DCM_HighBit, static_cast<Uint16>(c.high));
            });
        const ScopeRead r = ReadScope(p);
        EXPECT_EQ(c.ok ? XPE_OK : XPE_ERR_DICOM_INVALID, r.read)
            << "BitsStored=" << c.stored << " HighBit=" << c.high;
    }
}

// ---- QA-B-182b: JPEG 2000 is judged against its codestream -------------------------------------------------
namespace {

struct J2kSpec {
    uint32_t width = 256, height = 256;
    uint32_t components = 1;
    uint32_t precision = 16;
    bool isSigned = false;
};

/** A real codestream of the given characteristics, lossless, encoded with OpenJPEG inside the test. */
std::vector<uint8_t> EncodeJ2k(const J2kSpec& s, const std::vector<int32_t>& planeInterleavedByComponent) {
    opj_cparameters_t params;
    opj_set_default_encoder_parameters(&params);
    params.irreversible = 0;
    params.numresolution = 1;
    params.tcp_numlayers = 1;
    params.cp_disto_alloc = 1;
    params.tcp_rates[0] = 0;
    std::vector<opj_image_cmptparm_t> cp(s.components);
    for (auto& c : cp) {
        c = opj_image_cmptparm_t{};
        c.dx = 1;
        c.dy = 1;
        c.w = s.width;
        c.h = s.height;
        c.prec = s.precision;
        c.bpp = s.precision;
        c.sgnd = s.isSigned ? 1 : 0;
    }
    opj_image_t* img = opj_image_create(s.components, cp.data(),
                                        s.components == 1 ? OPJ_CLRSPC_GRAY : OPJ_CLRSPC_UNSPECIFIED);
    if (!img) return {};
    img->x0 = 0;
    img->y0 = 0;
    img->x1 = s.width;
    img->y1 = s.height;
    const size_t n = static_cast<size_t>(s.width) * s.height;
    for (uint32_t c = 0; c < s.components; ++c)
        for (size_t i = 0; i < n; ++i) img->comps[c].data[i] = planeInterleavedByComponent[c * n + i];

    std::vector<uint8_t> out;
    opj_codec_t* codec = opj_create_compress(OPJ_CODEC_J2K);
    opj_set_error_handler(codec, nullptr, nullptr);
    opj_set_warning_handler(codec, nullptr, nullptr);
    opj_set_info_handler(codec, nullptr, nullptr);
    if (codec && opj_setup_encoder(codec, &params, img)) {
        struct Sink { std::vector<uint8_t>* v; size_t pos; } sink{&out, 0};
        opj_stream_t* stream = opj_stream_default_create(OPJ_FALSE);
        opj_stream_set_user_data(stream, &sink, nullptr);
        opj_stream_set_write_function(stream, [](void* buf, OPJ_SIZE_T nb, void* ud) -> OPJ_SIZE_T {
            Sink* k = static_cast<Sink*>(ud);
            if (k->v->size() < k->pos + nb) k->v->resize(k->pos + nb);
            std::memcpy(k->v->data() + k->pos, buf, nb);
            k->pos += nb;
            return nb;
        });
        opj_stream_set_seek_function(stream, [](OPJ_OFF_T off, void* ud) -> OPJ_BOOL {
            Sink* k = static_cast<Sink*>(ud);
            if (off < 0) return OPJ_FALSE;
            if (k->v->size() < static_cast<size_t>(off)) k->v->resize(static_cast<size_t>(off));
            k->pos = static_cast<size_t>(off);
            return OPJ_TRUE;
        });
        opj_stream_set_skip_function(stream, [](OPJ_OFF_T nb, void* ud) -> OPJ_OFF_T {
            Sink* k = static_cast<Sink*>(ud);
            k->pos += static_cast<size_t>(nb);
            if (k->v->size() < k->pos) k->v->resize(k->pos);
            return nb;
        });
        const bool ok = opj_start_compress(codec, img, stream) && opj_encode(codec, stream) &&
                        opj_end_compress(codec, stream);
        opj_stream_destroy(stream);
        if (!ok) out.clear();
    }
    if (codec) opj_destroy_codec(codec);
    opj_image_destroy(img);
    return out;
}

/** The donor J2K file with its codestream replaced and its pixel attributes set as given. */
template <class F>
fs::path MakeJ2kVariant(const fs::path& donor, const char* name, const std::vector<uint8_t>& codestream, F mutate) {
    DcmFileFormat ff;
    EXPECT_TRUE(ff.loadFile(donor.string().c_str()).good());
    DcmDataset* ds = ff.getDataset();
    ds->findAndDeleteElement(DCM_PixelData);
    DcmPixelSequence* seq = new DcmPixelSequence(DcmTag(DCM_PixelData, EVR_OB));
    seq->insert(new DcmPixelItem(DcmTag(DCM_Item, EVR_OB)));
    DcmPixelItem* frag = new DcmPixelItem(DcmTag(DCM_Item, EVR_OB));
    frag->putUint8Array(codestream.data(), static_cast<Uint32>(codestream.size()));
    seq->insert(frag);
    DcmPixelData* pd = new DcmPixelData(DcmTag(DCM_PixelData, EVR_OB));
    pd->putOriginalRepresentation(EXS_JPEG2000LosslessOnly, nullptr, seq);
    EXPECT_TRUE(ds->insert(pd, OFTrue).good());
    mutate(ds);
    const fs::path out = donor.parent_path() / (std::string("j2k_") + name + ".dcm");
    EXPECT_TRUE(ff.saveFile(out.string().c_str(), EXS_JPEG2000LosslessOnly).good());
    return out;
}

void SetPixelAttrs(DcmDataset* ds, int samples, int pixelRep, int alloc, int stored, int high) {
    ds->putAndInsertUint16(DCM_SamplesPerPixel, static_cast<Uint16>(samples));
    ds->putAndInsertUint16(DCM_PixelRepresentation, static_cast<Uint16>(pixelRep));
    ds->putAndInsertUint16(DCM_BitsAllocated, static_cast<Uint16>(alloc));
    ds->putAndInsertUint16(DCM_BitsStored, static_cast<Uint16>(stored));
    ds->putAndInsertUint16(DCM_HighBit, static_cast<Uint16>(high));
}

std::vector<int32_t> Ramp(size_t n, int mod) {
    std::vector<int32_t> v(n);
    for (size_t i = 0; i < n; ++i) v[i] = static_cast<int32_t>((i * 7 + i / 256) % static_cast<size_t>(mod));
    return v;
}

}  // namespace

// The control: this project's own 16-bit J2K file. The result describes what it holds: UINT16, 16 allocated.
TEST_F(DicomReaderTest, J2kScope_OwnSixteenBitFileReadsAndIsDescribedAsSixteenBit) {
    const ScopeRead r = ReadScope(s_j2kDcm);
    ASSERT_EQ(XPE_OK, r.read);
    EXPECT_EQ(XPE_PIXEL_UINT16, r.format);
    EXPECT_EQ(16u, r.bitsAllocated);
    EXPECT_EQ(16u, r.bitsStored);
    EXPECT_EQ(Words(s_validDcm), r.words) << "lossless: the same pixels as the uncompressed file";
}

// The 8-bit exception, measured. An unsigned 8-bit, one-component codestream with tags that say so is read: the
// pixels are the original 0..255 and the result is UINT16 with bitsAllocated 16 (it used to claim 8 for a buffer
// of 2-byte samples) and bitsStored = the codestream's precision.
TEST_F(DicomReaderTest, J2kScope_EightBitUnsignedSingleComponentIsReadAndDescribedAsSixteenBit) {
    J2kSpec spec;
    spec.precision = 8;
    const std::vector<int32_t> px = Ramp(256u * 256u, 256);
    const auto cs = EncodeJ2k(spec, px);
    ASSERT_FALSE(cs.empty());
    const fs::path p = MakeJ2kVariant(s_j2kDcm, "gray8", cs, [](DcmDataset* ds) { SetPixelAttrs(ds, 1, 0, 8, 8, 7); });
    const ScopeRead r = ReadScope(p);
    ASSERT_EQ(XPE_OK, r.read);
    EXPECT_EQ(XPE_PIXEL_UINT16, r.format);
    EXPECT_EQ(16u, r.bitsAllocated) << "a UINT16 buffer holds 16 allocated bits whatever the file allocated";
    EXPECT_EQ(8u, r.bitsStored);
    ASSERT_EQ(px.size(), r.words.size());
    size_t differing = 0;
    for (size_t i = 0; i < px.size(); ++i) differing += (r.words[i] != static_cast<uint16_t>(px[i]));
    EXPECT_EQ(0u, differing) << "every pixel must equal the original 0..255 value";
}

TEST_F(DicomReaderTest, J2kScope_TwelveBitInASixteenBitWordKeepsItsPrecision) {
    J2kSpec spec;
    spec.precision = 12;
    const std::vector<int32_t> px = Ramp(256u * 256u, 4096);
    const fs::path p = MakeJ2kVariant(s_j2kDcm, "gray12", EncodeJ2k(spec, px),
                                      [](DcmDataset* ds) { SetPixelAttrs(ds, 1, 0, 16, 12, 11); });
    const ScopeRead r = ReadScope(p);
    ASSERT_EQ(XPE_OK, r.read);
    EXPECT_EQ(16u, r.bitsAllocated);
    EXPECT_EQ(12u, r.bitsStored);
    ASSERT_EQ(px.size(), r.words.size());
    for (size_t i = 0; i < px.size(); ++i) ASSERT_EQ(static_cast<uint16_t>(px[i]), r.words[i]) << i;
}

// A codestream that disagrees with the dataset about what it holds is refused BEFORE any output exists. PS3.5 8.2.4:
// the attributes shall be consistent with the compressed data stream, and the stream's own characteristics are the
// ones used for decoding. A file that contradicts itself is malformed: DICOM_INVALID (as for a codestream of the
// wrong size). Each variant changes ONE thing; the control above shows the rest of the file is accepted.
TEST_F(DicomReaderTest, J2kScope_ACodestreamThatContradictsTheTagsIsRefused) {
    struct Case { const char* name; J2kSpec spec; int mod; int samples, rep, alloc, stored, high; XpeErrorCode want; };
    J2kSpec twoComp;   twoComp.components = 2;
    J2kSpec signedCs;  signedCs.isSigned = true;
    J2kSpec prec12;    prec12.precision = 12;
    J2kSpec prec8;     prec8.precision = 8;
    J2kSpec prec20;    prec20.precision = 20;
    const Case cases[] = {
        {"two_components_tags_say_one", twoComp, 65536, 1, 0, 16, 16, 15, XPE_ERR_DICOM_INVALID},
        {"signed_codestream_tags_say_unsigned", signedCs, 30000, 1, 0, 16, 16, 15, XPE_ERR_DICOM_INVALID},
        {"precision_12_tags_say_16", prec12, 4096, 1, 0, 16, 16, 15, XPE_ERR_DICOM_INVALID},
        {"precision_8_tags_say_16", prec8, 256, 1, 0, 16, 16, 15, XPE_ERR_DICOM_INVALID},
        {"precision_16_tags_say_12", J2kSpec{}, 4096, 1, 0, 16, 12, 11, XPE_ERR_DICOM_INVALID},
        {"precision_20_beyond_the_16_bit_output", prec20, 65536, 1, 0, 16, 16, 15, XPE_ERR_UNSUPPORTED_FORMAT},
    };
    for (const Case& c : cases) {
        const size_t n = 256u * 256u * c.spec.components;
        const auto cs = EncodeJ2k(c.spec, Ramp(n, c.mod));
        ASSERT_FALSE(cs.empty()) << c.name << ": the variant codestream could not be encoded";
        const fs::path p = MakeJ2kVariant(s_j2kDcm, c.name, cs, [&](DcmDataset* ds) {
            SetPixelAttrs(ds, c.samples, c.rep, c.alloc, c.stored, c.high);
        });
        const ScopeRead r = ReadScope(p);
        EXPECT_EQ(c.want, r.read) << c.name;
        EXPECT_TRUE(r.outUntouchedOnFailure) << c.name;
        EXPECT_EQ(XPE_OK, r.metaAfter) << c.name << ": the handle still serves its metadata";
    }
}

// What the dataset alone can already say about a J2K file: allocation of 8 or 16 only, and never less than the
// bits stored.
TEST_F(DicomReaderTest, J2kScope_TheTagsAloneRejectWhatNoCodestreamCouldFix) {
    const auto cs = EncodeJ2k(J2kSpec{}, Ramp(256u * 256u, 65536));
    ASSERT_FALSE(cs.empty());
    auto read = [&](const char* name, int samples, int rep, int alloc, int stored, int high) {
        return ReadScope(MakeJ2kVariant(s_j2kDcm, name, cs, [&](DcmDataset* ds) {
            SetPixelAttrs(ds, samples, rep, alloc, stored, high);
        })).read;
    };
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, read("tags_alloc_32", 1, 0, 32, 16, 15));
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, read("tags_alloc_1", 1, 0, 1, 1, 0));
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, read("tags_rgb", 3, 0, 16, 16, 15));
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, read("tags_signed", 1, 1, 16, 16, 15));
    EXPECT_EQ(XPE_ERR_DICOM_INVALID, read("tags_stored_above_alloc", 1, 0, 8, 16, 15));
}

// QA-B-182d (Codex #51): the HighBit of a JPEG 2000 file is judged like the other paths' -- it must be BitsStored - 1.
// The codestream carries no HighBit, so nothing downstream could catch 16/12/15 (data in the high 12 bits of a
// word) on this path; the same description is refused for uncompressed and JPEG Lossless data.
TEST_F(DicomReaderTest, J2kScope_HighBitMustBeBitsStoredMinusOne) {
    struct Case { const char* name; int precision; int alloc, stored, high; XpeErrorCode want; };
    const Case cases[] = {
        {"hb_ok_8_8_7", 8, 8, 8, 7, XPE_OK},
        {"hb_ok_16_12_11", 12, 16, 12, 11, XPE_OK},
        {"hb_ok_16_16_15", 16, 16, 16, 15, XPE_OK},
        {"hb_16_12_15", 12, 16, 12, 15, XPE_ERR_DICOM_INVALID},    // the Codex #51 case
        {"hb_16_12_12", 12, 16, 12, 12, XPE_ERR_DICOM_INVALID},
        {"hb_16_16_14", 16, 16, 16, 14, XPE_ERR_DICOM_INVALID},
        {"hb_8_8_6", 8, 8, 8, 6, XPE_ERR_DICOM_INVALID},
        {"hb_8_8_15", 8, 8, 8, 15, XPE_ERR_DICOM_INVALID},
        {"hb_16_12_0xFFFF", 12, 16, 12, 0xFFFF, XPE_ERR_DICOM_INVALID},
    };
    for (const Case& c : cases) {
        J2kSpec spec;
        spec.precision = c.precision;
        const auto cs = EncodeJ2k(spec, Ramp(256u * 256u, 1 << c.precision));
        ASSERT_FALSE(cs.empty()) << c.name;
        const fs::path p = MakeJ2kVariant(s_j2kDcm, c.name, cs, [&](DcmDataset* ds) {
            SetPixelAttrs(ds, 1, 0, c.alloc, c.stored, c.high);
        });
        xpe_clear_alerts();
        const ScopeRead r = ReadScope(p);
        EXPECT_EQ(c.want, r.read) << c.name;
        if (c.want != XPE_OK) {
            EXPECT_TRUE(r.outUntouchedOnFailure) << c.name;
            EXPECT_EQ(XPE_OK, r.metaAfter) << c.name << ": the handle still serves its metadata";
            bool named = false;
            for (int32_t i = 0; i < xpe_get_pending_alert_count(); ++i) {
                char buf[512] = {0};
                int32_t sev = -1;
                if (xpe_get_pending_alert(i, buf, sizeof(buf), &sev) == XPE_OK && std::string(buf).find("HighBit") != std::string::npos) named = true;
            }
            EXPECT_TRUE(named) << c.name << ": the refusal names HighBit";
        }
    }
}

// The module's own writer must produce files its reader accepts: a 12-bit image written as J2K has a codestream of
// 12-bit precision, the same as the Bits Stored it declares (it used to encode 16-bit precision under a 12 bit tag).
TEST_F(DicomReaderTest, J2kScope_AnImageWrittenWithTwelveBitsStoredIsReadBack) {
    XpeImageBuffer img{};
    ASSERT_EQ(XPE_OK, xpe_alloc_image(64, 48, XPE_PIXEL_UINT16, &img));
    img.bitsStored = 12;
    auto* px = static_cast<uint16_t*>(img.data);
    for (uint32_t i = 0; i < img.width * img.height; ++i) px[i] = static_cast<uint16_t>((i * 5) & 0x0FFF);
    std::vector<uint16_t> original(px, px + static_cast<size_t>(img.width) * img.height);
    XpeImageMetadata meta{};
    const fs::path p = s_tempDir / "written_12bit.dcm";
    ASSERT_EQ(XPE_OK, xpe_dicom_write_j2k(p.string().c_str(), &img, &meta));
    xpe_free_image(&img);
    const ScopeRead r = ReadScope(p);
    ASSERT_EQ(XPE_OK, r.read);
    EXPECT_EQ(12u, r.bitsStored);
    EXPECT_EQ(original, r.words);
}

// ---- QA-B-182c: BitsStored and HighBit are Type 1 too, and a refusal says why ---------------------------------
namespace {

/** Same transfer syntax as the source (a JPEG Lossless file stays JPEG Lossless), pixel attributes edited. */
template <class F>
fs::path MakeSameSyntaxVariant(const fs::path& src, const char* name, F mutate) {
    DcmFileFormat ff;
    EXPECT_TRUE(ff.loadFile(src.string().c_str()).good());
    DcmDataset* ds = ff.getDataset();
    const E_TransferSyntax xfer = ds->getOriginalXfer();
    mutate(ds);
    const fs::path out = src.parent_path() / (std::string("same_") + name + ".dcm");
    EXPECT_TRUE(ff.saveFile(out.string().c_str(), xfer).good());
    return out;
}

/** Every pending alert, oldest first. */
std::vector<std::string> PendingAlerts() {
    std::vector<std::string> all;
    const int32_t n = xpe_get_pending_alert_count();
    for (int32_t i = 0; i < n; ++i) {
        char buf[512] = {0};
        int32_t sev = -1;
        if (xpe_get_pending_alert(i, buf, sizeof(buf), &sev) == XPE_OK) all.emplace_back(buf);
    }
    return all;
}

bool AnyAlertContains(const std::vector<std::string>& alerts, const char* a, const char* b = nullptr) {
    for (const auto& m : alerts) {
        if (m.find(a) != std::string::npos && (b == nullptr || m.find(b) != std::string::npos)) return true;
    }
    return false;
}

}  // namespace

// PS3.3 C.7.6.3.1.1: Bits Stored and High Bit are Type 1 like the other three. 182b let them default (Bits Stored =
// Bits Allocated, High Bit = Bits Stored - 1), which makes the 16-bit rule and the J2K precision check test a value
// the reader invented. Absent and empty are malformed, on every pixel-data path.
TEST_F(DicomReaderTest, Scope_AbsentOrEmptyBitsStoredAndHighBitAreMalformedOnEveryPath) {
    const fs::path jpegLl = s_tempDir / "jpegll_for_182c.dcm";
    ASSERT_TRUE(WriteJpegLosslessCopy(s_validDcm, jpegLl));
    std::vector<uint8_t> codestream;
    ASSERT_TRUE(ExtractJ2kBitstream(s_j2kDcm, codestream));

    struct Path { const char* name; std::function<fs::path(const char*, std::function<void(DcmDataset*)>)> make; };
    const Path paths[] = {
        {"native", [&](const char* n, std::function<void(DcmDataset*)> m) { return MakeSameSyntaxVariant(s_validDcm, n, m); }},
        {"jpeg_lossless", [&](const char* n, std::function<void(DcmDataset*)> m) { return MakeSameSyntaxVariant(jpegLl, n, m); }},
        {"j2k", [&](const char* n, std::function<void(DcmDataset*)> m) { return MakeJ2kVariant(s_j2kDcm, n, codestream, m); }},
    };
    struct Tag { const char* name; DcmTagKey key; };
    const Tag tags[] = {{"bits_stored", DCM_BitsStored}, {"high_bit", DCM_HighBit}};

    for (const Path& p : paths) {
        // control: the unmodified variant of this path reads
        EXPECT_EQ(XPE_OK, ReadScope(p.make((std::string("ctl_") + p.name).c_str(), [](DcmDataset*) {})).read)
            << p.name << ": control";
        for (const Tag& t : tags) {
            const std::string base = std::string(p.name) + "_" + t.name;
            const ScopeRead a = ReadScope(p.make((base + "_absent").c_str(), [&](DcmDataset* ds) { delete ds->remove(t.key); }));
            EXPECT_EQ(XPE_ERR_DICOM_INVALID, a.read) << base << " absent";
            EXPECT_TRUE(a.outUntouchedOnFailure) << base << " absent";
            EXPECT_EQ(XPE_OK, a.metaAfter) << base << " absent";
            const ScopeRead e = ReadScope(p.make((base + "_empty").c_str(), [&](DcmDataset* ds) { ds->putAndInsertString(t.key, ""); }));
            EXPECT_EQ(XPE_ERR_DICOM_INVALID, e.read) << base << " empty";
        }
    }
}

// ---- QA-B-182e: the bits attributes, judged against the standard -----------------------------------------------
// PS3.5 8.1.1 (every Pixel Data): Bits Allocated "shall either be 1, or a multiple of 8"; "Bits Stored shall never be
// larger than Bits Allocated"; "High Bit shall be one less than Bits Stored". PS3.5 Table 8.2.1-2 (JPEG Lossless)
// lists Bits Allocated 8 or 16; Table 8.2.4-1 (JPEG 2000, monochrome) lists 1, 8, 16, 24, 32 or 40. A value the
// standard forbids is a malformed dataset (DICOM_INVALID); a value it allows that this reader cannot return is
// unsupported. The same attribute therefore gets the same answer on every path.
TEST_F(DicomReaderTest, Scope_BitsAttributesAreViolationOrUnsupportedAsThePixelDataEncodingRulesSay) {
    const fs::path jpegLl = s_tempDir / "jpegll_for_182e.dcm";
    ASSERT_TRUE(WriteJpegLosslessCopy(s_validDcm, jpegLl));
    std::vector<uint8_t> codestream;
    ASSERT_TRUE(ExtractJ2kBitstream(s_j2kDcm, codestream));

    enum Want { OK, INV, UNS, SKIP };
    struct Case { int alloc, stored, high; Want native, ll, j2k; };
    // SKIP: the case is not decidable from the tags on that path (the J2K donor codestream is 16 bit, so its own
    // precision check answers for any stored value other than 16).
    const Case cases[] = {
        {16, 16, 15, OK,  OK,  OK},     // control
        {16, 12, 11, OK,  OK,  SKIP},   // 12 of 16 bits, well formed
        {16, 12, 15, INV, INV, INV},    // High Bit is not Bits Stored - 1 (the Codex #51 case)
        {16, 12, 12, INV, INV, INV},
        {16, 16, 14, INV, INV, INV},
        {16, 17, 16, INV, INV, INV},    // Bits Stored larger than Bits Allocated
        {16, 0, 0xFFFF, INV, INV, INV},
        {12, 12, 11, INV, INV, INV},    // 12 is neither 1 nor a multiple of 8
        {8, 8, 7, UNS,  UNS, SKIP},     // allowed everywhere, returned nowhere but J2K 8 bit
        {1, 1, 0, UNS,  INV, UNS},      // JPEG Lossless lists 8 and 16 only
        {24, 24, 23, UNS, INV, UNS},
        {32, 32, 31, UNS, INV, UNS},
        {48, 48, 47, UNS, INV, INV},    // beyond Table 8.2.4-1 as well
        // QA-B-181g (Codex #57): Table 8.2.4-1 lists BitsAllocated 40 AND BitsStored 1-38 / HighBit 0-37 for JPEG 2000.
        // An allocation the table allows can still carry a BitsStored it does not.
        {40, 40, 39, UNS, INV, INV},    // BitsStored 40 is beyond 38
        {40, 39, 38, UNS, INV, INV},    // BitsStored 39 is beyond 38 (the Codex case)
        {40, 38, 37, UNS, INV, UNS},    // the table allows it, this reader does not return it
    };
    const XpeErrorCode answer[] = {XPE_OK, XPE_ERR_DICOM_INVALID, XPE_ERR_UNSUPPORTED_FORMAT, XPE_OK};
    int n = 0;
    for (const Case& c : cases) {
        const std::string tag = std::to_string(c.alloc) + "_" + std::to_string(c.stored) + "_" + std::to_string(c.high);
        auto mutate = [&](DcmDataset* ds) { SetPixelAttrs(ds, 1, 0, c.alloc, c.stored, c.high); };
        struct Run { const char* name; Want want; fs::path file; };
        const Run runs[] = {
            {"native", c.native, c.native == SKIP ? fs::path() : MakeSameSyntaxVariant(s_validDcm, ("e_nat_" + tag).c_str(), mutate)},
            {"jpeg_lossless", c.ll, c.ll == SKIP ? fs::path() : MakeSameSyntaxVariant(jpegLl, ("e_ll_" + tag).c_str(), mutate)},
            {"j2k", c.j2k, c.j2k == SKIP ? fs::path() : MakeJ2kVariant(s_j2kDcm, ("e_j2k_" + tag).c_str(), codestream, mutate)},
        };
        for (const Run& r : runs) {
            if (r.want == SKIP) continue;
            const ScopeRead got = ReadScope(r.file);
            EXPECT_EQ(answer[r.want], got.read) << r.name << " alloc/stored/high = " << tag;
            if (r.want != OK) {
                EXPECT_TRUE(got.outUntouchedOnFailure) << r.name << " " << tag;
                EXPECT_EQ(XPE_OK, got.metaAfter) << r.name << " " << tag;
            }
            ++n;
        }
    }
    EXPECT_GE(n, 30) << "the table must actually have been run";
}

// PS3.5 8.2 / 8.2.4: for an encapsulated JPEG stream the pixel attributes "shall contain Values that are consistent
// with the characteristics of the compressed data stream", and Table 8.2.1-2 says "The Pixel Data characteristics
// included in the JPEG Interchange Format shall be used to decode the compressed data stream". The only bit depth the
// JPEG stream carries is the SOF sample precision P. P below Bits Stored cannot hold the declared values; P above
// Bits Allocated does not fit the declared container. P above Bits Stored is NOT refused: the standard does not say
// what "consistent" means there, encoders are known to write a wider P, and refusing would turn away real files.
namespace {
/** The first SOF3 (JPEG Lossless) frame header in the file, patched to carry sample precision `p`. */
bool PatchJpegLosslessPrecision(const fs::path& src, const fs::path& dst, int p, int* original) {
    std::ifstream in(src, std::ios::binary);
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    for (size_t k = 0; k + 4 < b.size(); ++k) {
        if (b[k] == 0xFF && b[k + 1] == 0xC3 && b[k + 2] == 0x00 && b[k + 3] == 0x0B) {   // SOF3, one component
            if (original) *original = b[k + 4];
            b[k + 4] = static_cast<uint8_t>(p);
            std::ofstream out(dst, std::ios::binary);
            out.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
            return out.good();
        }
    }
    return false;
}
}  // namespace

TEST_F(DicomReaderTest, Scope_JpegLosslessPrecisionIsComparedWithBitsStoredBeforeDecoding) {
    const fs::path ll = s_tempDir / "jpegll_for_182e_p.dcm";
    ASSERT_TRUE(WriteJpegLosslessCopy(s_validDcm, ll));
    int original = 0;
    const fs::path same = s_tempDir / "jpegll_p_same.dcm";
    ASSERT_TRUE(PatchJpegLosslessPrecision(ll, same, 16, &original));
    ASSERT_EQ(16, original) << "the donor stream is 16 bit; the patch below is a change";
    EXPECT_EQ(XPE_OK, ReadScope(same).read) << "control: precision 16, Bits Stored 16";

    for (int p : {12, 8, 20}) {   // 12 and 8 cannot hold Bits Stored 16; 20 does not fit 16 bits allocated
        const fs::path narrow = s_tempDir / ("jpegll_p_" + std::to_string(p) + ".dcm");
        ASSERT_TRUE(PatchJpegLosslessPrecision(ll, narrow, p, nullptr));
        xpe_clear_alerts();
        const ScopeRead r = ReadScope(narrow);
        EXPECT_EQ(XPE_ERR_DICOM_INVALID, r.read) << "precision " << p << " does not fit Bits Stored 16 / Bits Allocated 16";
        EXPECT_TRUE(r.outUntouchedOnFailure) << "precision " << p;
        EXPECT_EQ(XPE_OK, r.metaAfter) << "precision " << p;
        bool named = false;
        for (int32_t i = 0; i < xpe_get_pending_alert_count(); ++i) {
            char buf[512] = {0};
            int32_t sev = -1;
            if (xpe_get_pending_alert(i, buf, sizeof(buf), &sev) == XPE_OK && std::string(buf).find("precision") != std::string::npos) named = true;
        }
        EXPECT_TRUE(named) << "the refusal names the stream precision, precision " << p;
    }

    // A wider stream than Bits Stored is allowed: 16 bit precision, 12 bits stored.
    const fs::path wider = MakeSameSyntaxVariant(ll, "e_ll_p16_stored12", [](DcmDataset* ds) { SetPixelAttrs(ds, 1, 0, 16, 12, 11); });
    EXPECT_EQ(XPE_OK, ReadScope(wider).read) << "precision 16 above Bits Stored 12 is not refused";
}

// QA-B-182e (card 182f item 5): a JPEG Lossless stream whose frame header declares three components while the dataset
// says SamplesPerPixel = 1. A JPEG Lossless frame header carries no sign flag (PixelRepresentation lives in the dataset
// only), so the component count is the only independent attribute besides precision and size.
namespace {
/** The first SOF3 frame header in the file, rewritten to declare `nf` components (a consistent header: length and
 *  component specs follow). The scan header is left alone, so the file is inconsistent on purpose. */
bool PatchJpegLosslessComponents(const fs::path& src, const fs::path& dst, int nf) {
    std::ifstream in(src, std::ios::binary);
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    for (size_t k = 0; k + 12 < b.size(); ++k) {
        if (b[k] == 0xFF && b[k + 1] == 0xC3 && b[k + 2] == 0x00 && b[k + 3] == 0x0B) {   // SOF3, one component
            const size_t len = 8 + 3 * static_cast<size_t>(nf);
            b[k + 2] = static_cast<uint8_t>(len >> 8);
            b[k + 3] = static_cast<uint8_t>(len & 0xFF);
            b[k + 9] = static_cast<uint8_t>(nf);
            std::vector<uint8_t> extra;
            for (int c = 1; c < nf; ++c) {
                extra.push_back(static_cast<uint8_t>(c + 1));   // component id
                extra.push_back(0x11);                          // sampling factors
                extra.push_back(0x00);                          // quantization table (unused in lossless)
            }
            // The bytes added to the frame header must also be added to the length of the pixel-data fragment (item)
            // that holds it, or the file no longer parses and the reader never reaches the frame header. The item
            // header is the 8 bytes before the JPEG SOI: tag FE FF 00 E0, then a 4 byte little endian length.
            size_t soi = k;
            while (soi >= 2 && !(b[soi] == 0xD8 && b[soi - 1] == 0xFF)) --soi;
            soi -= 1;                                   // index of the 0xFF of FF D8
            if (soi < 8 || b[soi - 8] != 0xFE || b[soi - 7] != 0xFF || b[soi - 6] != 0x00 || b[soi - 5] != 0xE0) return false;
            uint32_t itemLen = static_cast<uint32_t>(b[soi - 4]) | (static_cast<uint32_t>(b[soi - 3]) << 8) |
                               (static_cast<uint32_t>(b[soi - 2]) << 16) | (static_cast<uint32_t>(b[soi - 1]) << 24);
            itemLen += static_cast<uint32_t>(extra.size());
            b[soi - 4] = static_cast<uint8_t>(itemLen & 0xFF);
            b[soi - 3] = static_cast<uint8_t>((itemLen >> 8) & 0xFF);
            b[soi - 2] = static_cast<uint8_t>((itemLen >> 16) & 0xFF);
            b[soi - 1] = static_cast<uint8_t>((itemLen >> 24) & 0xFF);
            b.insert(b.begin() + static_cast<std::ptrdiff_t>(k + 13), extra.begin(), extra.end());
            std::ofstream out(dst, std::ios::binary);
            out.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
            return out.good();
        }
    }
    return false;
}
}  // namespace

TEST_F(DicomReaderTest, Scope_JpegLosslessComponentCountIsComparedWithSamplesPerPixelBeforeDecoding) {
    const fs::path ll = s_tempDir / "jpegll_for_182e_nf.dcm";
    ASSERT_TRUE(WriteJpegLosslessCopy(s_validDcm, ll));
    const fs::path one = s_tempDir / "jpegll_nf_1.dcm";
    ASSERT_TRUE(PatchJpegLosslessComponents(ll, one, 1));
    EXPECT_EQ(XPE_OK, ReadScope(one).read) << "control: one component, SamplesPerPixel 1";

    const fs::path three = s_tempDir / "jpegll_nf_3.dcm";
    ASSERT_TRUE(PatchJpegLosslessComponents(ll, three, 3));
    xpe_clear_alerts();
    const ScopeRead r = ReadScope(three);
    GTEST_LOG_(INFO) << "QA-B-182e probe: frame header with 3 components, dataset SamplesPerPixel 1 -> rc=" << r.read;
    EXPECT_EQ(XPE_ERR_DICOM_INVALID, r.read) << "the stream says 3 components, the dataset says 1";
    EXPECT_TRUE(r.outUntouchedOnFailure);
    EXPECT_EQ(XPE_OK, r.metaAfter);
    bool named = false;
    for (int32_t i = 0; i < xpe_get_pending_alert_count(); ++i) {
        char buf[512] = {0};
        int32_t sev = -1;
        if (xpe_get_pending_alert(i, buf, sizeof(buf), &sev) == XPE_OK && std::string(buf).find("component") != std::string::npos) named = true;
    }
    EXPECT_TRUE(named) << "the refusal names the component count";
}

// ---- QA-B-182f: PhotometricInterpretation (Type 1) and the rest of the Image Pixel Description Macro -------------
// PS3.3 Table C.7-11c (Image Pixel Description Macro): Samples per Pixel, Photometric Interpretation, Rows, Columns,
// Bits Allocated, Bits Stored, High Bit and Pixel Representation are Type 1. C.7.6.3.1.2 defines the values:
// MONOCHROME1 / MONOCHROME2 "may be used only when Samples per Pixel has a Value of 1" (a single plane, minimum
// displayed as white / black); PALETTE COLOR is a single plane whose value is an index into the palette tables (so a
// gray uint16 buffer would misrepresent it); RGB, YBR_FULL, YBR_FULL_422, YBR_PARTIAL_420, YBR_ICT, YBR_RCT "may be
// used only when Samples per Pixel has a Value of 3" (so with one sample they are a malformed dataset); other values
// are "permitted if supported by the Transfer Syntax but the meaning is not defined by this Standard".
namespace {
bool AlertNames(const char* needle) {
    for (int32_t i = 0; i < xpe_get_pending_alert_count(); ++i) {
        char buf[512] = {0};
        int32_t sev = -1;
        if (xpe_get_pending_alert(i, buf, sizeof(buf), &sev) == XPE_OK && std::string(buf).find(needle) != std::string::npos) return true;
    }
    return false;
}
}  // namespace

TEST_F(DicomReaderTest, Scope_PhotometricInterpretationIsRequiredAndOnlyAMonochromePlaneIsReturned) {
    const fs::path jpegLl = s_tempDir / "jpegll_for_182f.dcm";
    ASSERT_TRUE(WriteJpegLosslessCopy(s_validDcm, jpegLl));
    std::vector<uint8_t> codestream;
    ASSERT_TRUE(ExtractJ2kBitstream(s_j2kDcm, codestream));

    enum Want { OK, INV, UNS };
    struct Row { const char* label; const char* pi; int samples; Want want; };   // pi == nullptr: absent
    const Row rows[] = {
        {"control_monochrome2", "MONOCHROME2", 1, OK},
        {"monochrome2_padded", "MONOCHROME2 ", 1, OK},          // a CS value of odd length is padded with a space
        {"monochrome2_leading_space", " MONOCHROME2", 1, OK},   // leading and trailing spaces of a CS are not significant (PS3.5 6.2)
        {"monochrome1", "MONOCHROME1", 1, OK},                  // read, inverted to MONOCHROME2 sense (QA-B-185, FR-DCM-109)
        {"absent", nullptr, 1, INV},
        {"empty", "", 1, INV},
        {"palette_color", "PALETTE COLOR", 1, UNS},             // valid, but the value is an index, not a gray level
        {"rgb_one_sample", "RGB", 1, INV},                      // RGB is defined only for 3 samples
        {"ybr_full_one_sample", "YBR_FULL", 1, INV},
        {"ybr_full_422_one_sample", "YBR_FULL_422", 1, INV},
        {"ybr_rct_one_sample", "YBR_RCT", 1, INV},
        {"xyb", "XYB", 1, UNS},
        {"undefined_value", "FOO", 1, UNS},                     // "permitted ... but the meaning is not defined"
        {"rgb_three_samples", "RGB", 3, UNS},                   // well formed colour, which this reader cannot return
    };
    const XpeErrorCode answer[] = {XPE_OK, XPE_ERR_DICOM_INVALID, XPE_ERR_UNSUPPORTED_FORMAT};
    int n = 0;
    for (const Row& r : rows) {
        auto mutate = [&](DcmDataset* ds) {
            if (r.pi == nullptr) delete ds->remove(DCM_PhotometricInterpretation);
            else ds->putAndInsertString(DCM_PhotometricInterpretation, r.pi);
            if (r.samples != 1) ds->putAndInsertUint16(DCM_SamplesPerPixel, static_cast<Uint16>(r.samples));
        };
        struct Run { const char* name; fs::path file; };
        const Run runs[] = {
            {"native", MakeSameSyntaxVariant(s_validDcm, (std::string("f_nat_") + r.label).c_str(), mutate)},
            {"jpeg_lossless", MakeSameSyntaxVariant(jpegLl, (std::string("f_ll_") + r.label).c_str(), mutate)},
            {"j2k", MakeJ2kVariant(s_j2kDcm, (std::string("f_j2k_") + r.label).c_str(), codestream, mutate)},
        };
        for (const Run& run : runs) {
            xpe_clear_alerts();
            const ScopeRead got = ReadScope(run.file);
            EXPECT_EQ(answer[r.want], got.read) << run.name << " PhotometricInterpretation=" << (r.pi ? r.pi : "<absent>") << " samples=" << r.samples;
            if (r.want != OK) {
                EXPECT_TRUE(got.outUntouchedOnFailure) << run.name << " " << r.label;
                EXPECT_EQ(XPE_OK, got.metaAfter) << run.name << " " << r.label;
                EXPECT_TRUE(AlertNames("PhotometricInterpretation") || AlertNames("SamplesPerPixel")) << run.name << " " << r.label << ": the refusal names the attribute";
            }
            ++n;
        }
    }
    EXPECT_EQ(14 * 3, n);
}

TEST_F(DicomReaderTest, Scope_PhotometricInterpretationRefusalsNameTheValueAndMonochrome1IsInverted) {
    xpe_clear_alerts();
    const fs::path absent = MakeScopeVariant(s_validDcm, "f_alert_absent", [](DcmDataset* ds) { delete ds->remove(DCM_PhotometricInterpretation); });
    EXPECT_EQ(XPE_ERR_DICOM_INVALID, ReadScope(absent).read);
    EXPECT_TRUE(AlertNames("PhotometricInterpretation (0028,0004) is absent")) << "absent attribute not named";

    xpe_clear_alerts();
    const fs::path palette = MakeScopeVariant(s_validDcm, "f_alert_palette", [](DcmDataset* ds) { ds->putAndInsertString(DCM_PhotometricInterpretation, "PALETTE COLOR"); });
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, ReadScope(palette).read);
    EXPECT_TRUE(AlertNames("PALETTE COLOR")) << "the unsupported value is named";

    xpe_clear_alerts();
    const fs::path rgb1 = MakeScopeVariant(s_validDcm, "f_alert_rgb1", [](DcmDataset* ds) { ds->putAndInsertString(DCM_PhotometricInterpretation, "RGB"); });
    EXPECT_EQ(XPE_ERR_DICOM_INVALID, ReadScope(rgb1).read);
    EXPECT_TRUE(AlertNames("RGB") && AlertNames("SamplesPerPixel")) << "the value and the sample count that contradict each other are named";

    // QA-B-185 (#235): MONOCHROME1 is read, and returned INVERTED to MONOCHROME2 sense (FR-DCM-109); one Info alert
    // tells the caller. (QA-B-182f pinned the opposite: "the words come back as stored, and nothing tells the caller".)
    const std::vector<uint16_t> baseline = Words(s_validDcm);
    xpe_clear_alerts();
    const fs::path mono1 = MakeScopeVariant(s_validDcm, "f_mono1", [](DcmDataset* ds) { ds->putAndInsertString(DCM_PhotometricInterpretation, "MONOCHROME1"); });
    const ScopeRead r = ReadScope(mono1);
    EXPECT_EQ(XPE_OK, r.read);
    ASSERT_EQ(baseline.size(), r.words.size());
    size_t wrong = 0;
    for (size_t i = 0; i < baseline.size(); ++i) {
        if (r.words[i] != static_cast<uint16_t>(0xFFFFu - baseline[i])) ++wrong;
    }
    EXPECT_EQ(0u, wrong) << "inverted: 65535 - stored (BitsStored 16)";
    EXPECT_EQ(1, xpe_get_pending_alert_count()) << "one alert says the values were inverted";
}

// Rows and Columns are Type 1 too. They were read, and an absent or zero value was refused with the right code, but
// silently: the other Type 1 attributes post an alert that names the cause (QA-B-182c).
TEST_F(DicomReaderTest, Scope_AbsentOrZeroRowsAndColumnsAreRefusedWithAnAlertOnEveryPath) {
    const fs::path jpegLl = s_tempDir / "jpegll_for_182f_rc.dcm";
    ASSERT_TRUE(WriteJpegLosslessCopy(s_validDcm, jpegLl));
    std::vector<uint8_t> codestream;
    ASSERT_TRUE(ExtractJ2kBitstream(s_j2kDcm, codestream));
    struct Tag { const char* name; DcmTagKey key; };
    const Tag tags[] = {{"Rows", DCM_Rows}, {"Columns", DCM_Columns}};
    for (const Tag& t : tags) {
        for (int mode = 0; mode < 2; ++mode) {   // 0 absent, 1 zero
            auto mutate = [&](DcmDataset* ds) {
                if (mode == 0) delete ds->remove(t.key);
                else ds->putAndInsertUint16(t.key, 0);
            };
            const std::string base = std::string("f_rc_") + t.name + (mode == 0 ? "_absent" : "_zero");
            const struct { const char* name; fs::path file; } runs[] = {
                {"native", MakeSameSyntaxVariant(s_validDcm, (base + "_nat").c_str(), mutate)},
                {"jpeg_lossless", MakeSameSyntaxVariant(jpegLl, (base + "_ll").c_str(), mutate)},
                {"j2k", MakeJ2kVariant(s_j2kDcm, (base + "_j2k").c_str(), codestream, mutate)},
            };
            for (const auto& run : runs) {
                xpe_clear_alerts();
                const ScopeRead got = ReadScope(run.file);
                EXPECT_EQ(XPE_ERR_DICOM_INVALID, got.read) << run.name << " " << base;
                EXPECT_TRUE(got.outUntouchedOnFailure) << run.name << " " << base;
                EXPECT_TRUE(AlertNames(t.name)) << run.name << " " << base << ": the refusal names the attribute";
            }
        }
    }
}

// QA-B-181h (Codex #60): the component-count comparison must treat Nf = 0 and an absent Nf as a mismatch. It used `0` both
// as "not read" and as a real Nf, so a frame header declaring zero components, or one cut off before the Nf byte, passed.
namespace {
/** The first SOF3 frame header, with its Nf byte and/or its segment length overwritten (-1 leaves a field alone). */
bool PatchJpegLosslessSof(const fs::path& src, const fs::path& dst, int nf, int segmentLength) {
    std::ifstream in(src, std::ios::binary);
    std::vector<uint8_t> b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    for (size_t k = 0; k + 12 < b.size(); ++k) {
        if (b[k] == 0xFF && b[k + 1] == 0xC3 && b[k + 2] == 0x00 && b[k + 3] == 0x0B) {   // SOF3, one component
            if (nf >= 0) b[k + 9] = static_cast<uint8_t>(nf);
            if (segmentLength >= 0) {
                b[k + 2] = static_cast<uint8_t>(segmentLength >> 8);
                b[k + 3] = static_cast<uint8_t>(segmentLength & 0xFF);
            }
            std::ofstream out(dst, std::ios::binary);
            out.write(reinterpret_cast<const char*>(b.data()), static_cast<std::streamsize>(b.size()));
            return out.good();
        }
    }
    return false;
}
}  // namespace

TEST_F(DicomReaderTest, Scope_JpegLosslessComponentCountMustBeExactlyOneAndPresent) {
    const fs::path ll = s_tempDir / "jpegll_for_181h.dcm";
    ASSERT_TRUE(WriteJpegLosslessCopy(s_validDcm, ll));
    struct Case { const char* name; int nf; int segLen; XpeErrorCode want; };
    const Case cases[] = {
        {"nf_1_control", 1, -1, XPE_OK},
        {"nf_0", 0, -1, XPE_ERR_DICOM_INVALID},                 // zero components
        {"nf_2", 2, -1, XPE_ERR_DICOM_INVALID},
        {"nf_3", 3, -1, XPE_ERR_DICOM_INVALID},
        {"nf_byte_beyond_the_segment", -1, 7, XPE_ERR_DICOM_INVALID},   // the segment ends before the Nf byte
        {"segment_length_zero", -1, 0, XPE_ERR_DICOM_INVALID},
    };
    for (const Case& c : cases) {
        const fs::path p = s_tempDir / (std::string("jpegll_181h_") + c.name + ".dcm");
        ASSERT_TRUE(PatchJpegLosslessSof(ll, p, c.nf, c.segLen)) << c.name;
        xpe_clear_alerts();
        const ScopeRead r = ReadScope(p);
        GTEST_LOG_(INFO) << "QA-B-181h probe: " << c.name << " -> rc=" << r.read;
        EXPECT_EQ(c.want, r.read) << c.name;
        if (c.want != XPE_OK) {
            EXPECT_TRUE(r.outUntouchedOnFailure) << c.name;
            EXPECT_EQ(XPE_OK, r.metaAfter) << c.name;
            bool named = false;
            for (int32_t i = 0; i < xpe_get_pending_alert_count(); ++i) {
                char buf[512] = {0};
                int32_t sev = -1;
                if (xpe_get_pending_alert(i, buf, sizeof(buf), &sev) == XPE_OK && std::string(buf).find("component") != std::string::npos) named = true;
            }
            EXPECT_TRUE(named) << c.name << ": the refusal names the component count";
        }
    }
}

// The refusal reaches the operator: an alert names the attribute or the two values that disagree. (The module's
// only other channel is its log.) The alert wording is a contract with the clients that display alerts.
TEST_F(DicomReaderTest, Scope_ARefusalPostsAnAlertThatNamesTheCause) {
    xpe_clear_alerts();
    const fs::path noBits = MakeScopeVariant(s_validDcm, "alert_no_bitsstored", [](DcmDataset* ds) { delete ds->remove(DCM_BitsStored); });
    EXPECT_EQ(XPE_ERR_DICOM_INVALID, ReadScope(noBits).read);
    auto alerts = PendingAlerts();
    EXPECT_TRUE(AnyAlertContains(alerts, "BitsStored", "absent")) << "absent attribute not named";

    xpe_clear_alerts();
    const fs::path rgb = MakeScopeVariant(s_validDcm, "alert_rgb", [](DcmDataset* ds) { ds->putAndInsertUint16(DCM_SamplesPerPixel, 3); });
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, ReadScope(rgb).read);
    alerts = PendingAlerts();
    EXPECT_TRUE(AnyAlertContains(alerts, "SamplesPerPixel", "3")) << "unsupported value not named";

    xpe_clear_alerts();
    const fs::path ok = s_validDcm;
    EXPECT_EQ(XPE_OK, ReadScope(ok).read);
    EXPECT_EQ(0, xpe_get_pending_alert_count()) << "a file that reads must not post a refusal alert";
    xpe_clear_alerts();
}

// What the pre-182b writer produced for a 12-bit image: a 16-bit-precision codestream under Bits Stored 12 / High Bit
// 11. The writer is fixed, but such files may exist; the reader refuses them, and says which two values disagree.
TEST_F(DicomReaderTest, J2kScope_AFileFromTheOldWriterIsRefusedAndTheAlertSaysWhy) {
    // reproduce the old writer: 16-bit precision codestream, dataset declaring 12 bits
    const std::vector<int32_t> px = Ramp(256u * 256u, 4096);
    J2kSpec sixteen;   // precision 16, as the old writer always encoded
    const auto cs = EncodeJ2k(sixteen, px);
    ASSERT_FALSE(cs.empty());
    const fs::path p = MakeJ2kVariant(s_j2kDcm, "old_writer_12bit", cs, [](DcmDataset* ds) { SetPixelAttrs(ds, 1, 0, 16, 12, 11); });

    xpe_clear_alerts();
    const ScopeRead r = ReadScope(p);
    EXPECT_EQ(XPE_ERR_DICOM_INVALID, r.read);
    EXPECT_TRUE(r.outUntouchedOnFailure);
    const auto alerts = PendingAlerts();
    EXPECT_TRUE(AnyAlertContains(alerts, "precision 16", "BitsStored 12"))
        << "the alert must name the codestream precision and the declared Bits Stored";
    xpe_clear_alerts();
}

TEST_F(DicomReaderTest, Scope_MultiFrameIsUnsupportedNotTheFirstFrame) {
    const std::vector<uint16_t> one = Words(s_validDcm);
    std::vector<uint16_t> three;
    for (int k = 0; k < 3; ++k) three.insert(three.end(), one.begin(), one.end());
    const fs::path p = MakeScopeVariant(s_validDcm, "three_frames", [&](DcmDataset* ds) {
        ds->putAndInsertString(DCM_NumberOfFrames, "3");
        PutWords(ds, three);
    });
    const ScopeRead r = ReadScope(p);
    EXPECT_EQ(XPE_OK, r.open);
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, r.read);
    EXPECT_TRUE(r.outUntouchedOnFailure) << "the output buffer must not be touched";
    EXPECT_EQ(XPE_OK, r.metaAfter) << "the same handle still serves its metadata";
}

TEST_F(DicomReaderTest, Scope_RgbIsUnsupportedNotByteSoup) {
    // 16-bit samples so only SamplesPerPixel can be the reason
    const std::vector<uint16_t> rgb(256u * 256u * 3u, 1000);
    const fs::path p16 = MakeScopeVariant(s_validDcm, "rgb16", [&](DcmDataset* ds) {
        ds->putAndInsertUint16(DCM_SamplesPerPixel, 3);
        ds->putAndInsertString(DCM_PhotometricInterpretation, "RGB");
        ds->putAndInsertUint16(DCM_PlanarConfiguration, 0);
        PutWords(ds, rgb);
    });
    const ScopeRead r16 = ReadScope(p16);
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, r16.read);
    EXPECT_TRUE(r16.outUntouchedOnFailure);
    EXPECT_EQ(XPE_OK, r16.metaAfter);
    // and the ordinary 8-bit RGB the measurement used
    const fs::path p8 = MakeScopeVariant(s_validDcm, "rgb8", [&](DcmDataset* ds) {
        ds->putAndInsertUint16(DCM_SamplesPerPixel, 3);
        ds->putAndInsertString(DCM_PhotometricInterpretation, "RGB");
        ds->putAndInsertUint16(DCM_PlanarConfiguration, 0);
        ds->putAndInsertUint16(DCM_BitsAllocated, 8);
        ds->putAndInsertUint16(DCM_BitsStored, 8);
        ds->putAndInsertUint16(DCM_HighBit, 7);
        std::vector<uint8_t> bytes(256u * 256u * 3u, 7);
        ds->putAndInsertUint8Array(DCM_PixelData, bytes.data(), static_cast<unsigned long>(bytes.size()));
    });
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, ReadScope(p8).read);
}

TEST_F(DicomReaderTest, Scope_SignedPixelsAreUnsupportedNotReinterpreted) {
    std::vector<uint16_t> w = Words(s_validDcm);
    w[1] = static_cast<uint16_t>(static_cast<int16_t>(-1000));   // a negative stored value
    const fs::path p = MakeScopeVariant(s_validDcm, "signed", [&](DcmDataset* ds) {
        ds->putAndInsertUint16(DCM_PixelRepresentation, 1);
        PutWords(ds, w);
    });
    const ScopeRead r = ReadScope(p);
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, r.read);
    EXPECT_TRUE(r.outUntouchedOnFailure);
    EXPECT_EQ(XPE_OK, r.metaAfter);
}

TEST_F(DicomReaderTest, Scope_EightBitIsUnsupportedNotACorruptFile) {
    const fs::path p = MakeScopeVariant(s_validDcm, "gray8", [&](DcmDataset* ds) {
        ds->putAndInsertUint16(DCM_BitsAllocated, 8);
        ds->putAndInsertUint16(DCM_BitsStored, 8);
        ds->putAndInsertUint16(DCM_HighBit, 7);
        std::vector<uint8_t> bytes(256u * 256u, 9);
        ds->putAndInsertUint8Array(DCM_PixelData, bytes.data(), static_cast<unsigned long>(bytes.size()));
    });
    const ScopeRead r = ReadScope(p);
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, r.read) << "it used to be XPE_ERR_DICOM_INVALID ('PixelData is short')";
    EXPECT_NE(XPE_ERR_DICOM_INVALID, r.read);
    EXPECT_TRUE(r.outUntouchedOnFailure);
    EXPECT_EQ(XPE_OK, r.metaAfter);
}

// A present attribute that cannot be read as the number it must be is a malformed file, not a default.
TEST_F(DicomReaderTest, Scope_PresentButUnreadableAttributesAreMalformedNotDefaulted) {
    EXPECT_EQ(XPE_ERR_DICOM_INVALID, ReadScope(MakeScopeVariant(s_validDcm, "frames_not_numeric", [](DcmDataset* ds) {
        ds->putAndInsertString(DCM_NumberOfFrames, "abc");
    })).read);
    EXPECT_EQ(XPE_ERR_DICOM_INVALID, ReadScope(MakeScopeVariant(s_validDcm, "frames_zero", [](DcmDataset* ds) {
        ds->putAndInsertString(DCM_NumberOfFrames, "0");
    })).read);
    EXPECT_EQ(XPE_ERR_DICOM_INVALID, ReadScope(MakeScopeVariant(s_validDcm, "frames_empty", [](DcmDataset* ds) {
        ds->putAndInsertString(DCM_NumberOfFrames, "");
    })).read);
}

// The refusal does not change the handle: the same handle refuses the same way again.
TEST_F(DicomReaderTest, Scope_ARefusedHandleRefusesTheSameWayAgain) {
    const fs::path p = MakeScopeVariant(s_validDcm, "refuse_twice", [](DcmDataset* ds) {
        ds->putAndInsertUint16(DCM_PixelRepresentation, 1);
    });
    XpeDicomHandle* h = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(p.string().c_str(), &h));
    XpeImageBuffer a{}, b{};
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, xpe_dicom_read_image(h, &a));
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, xpe_dicom_read_image(h, &b));
    xpe_dicom_close(h);
}

// ---- PINNED, NOT ENDORSED: #235 awaits a design decision on these three ------------------------------------
// They are returned as stored today. When #235 decides (normalise MONOCHROME1 to MONOCHROME2, report or apply the
// rescale, mask above BitsStored), THESE TESTS MUST CHANGE with it: each one fails the day the behaviour does.

// QA-B-185: DECIDED (leader, after the QA-B-184 report). This was "Pinned_Issue235AwaitsDesignDecision_Monochrome1Is
// ReturnedAsStored", asserting "no inversion, and no signal that the data is inverted" -- the state QA-B-182f recorded
// so that the day the behaviour changed this test would fail. It changed: FR-DCM-109 (SRS-DICOM-001, SAD step 10,
// README, PRD REQ-4.1.9) requires the inversion, so the assertion is now its opposite. The full matrix is in the
// Tc109_ tests at the end of this file.
TEST_F(DicomReaderTest, Issue235Decided_Monochrome1IsInvertedToMonochrome2Sense) {
    const std::vector<uint16_t> baseline = Words(s_validDcm);
    const fs::path p = MakeScopeVariant(s_validDcm, "mono1", [](DcmDataset* ds) {
        ds->putAndInsertString(DCM_PhotometricInterpretation, "MONOCHROME1");
    });
    const ScopeRead r = ReadScope(p);
    EXPECT_EQ(XPE_OK, r.read);
    ASSERT_EQ(baseline.size(), r.words.size());
    for (size_t i = 0; i < baseline.size(); ++i) {
        ASSERT_EQ(static_cast<uint16_t>(0xFFFFu - baseline[i]), r.words[i]) << "pixel " << i;
    }
}

// QA-B-187: DECIDED (leader, after the QA-B-186 matrix). This was "Pinned_Issue235AwaitsDesignDecision_RescaleIsNot
// AppliedNorReported", asserting "stored values, neither rescaled nor flagged". The stored values are still what is
// returned (applying or reporting the rescale is a separate decision), but it is no longer silent: a non-identity
// Rescale posts one Warning. The full matrix is the Tc235_ tests at the end of this file.
TEST_F(DicomReaderTest, Issue235Decided_RescaleIsNotAppliedButWarned) {
    const std::vector<uint16_t> baseline = Words(s_validDcm);
    const fs::path p = MakeScopeVariant(s_validDcm, "rescale", [](DcmDataset* ds) {
        ds->putAndInsertString(DCM_RescaleSlope, "2");
        ds->putAndInsertString(DCM_RescaleIntercept, "-1024");
    });
    xpe_clear_alerts();
    const ScopeRead r = ReadScope(p);
    EXPECT_EQ(XPE_OK, r.read);
    EXPECT_EQ(baseline, r.words) << "stored values, not rescaled";
    EXPECT_TRUE(AlertNames("rescale not applied")) << "and the caller is told";
}

// QA-B-187: DECIDED (leader; issue #235 body item (h)). This was "Pinned_Issue235AwaitsDesignDecision_BitsAbove
// BitsStoredAreNotMasked": "the 4 bits above BitsStored are returned, not masked" (QA-B-182f recorded it; QA-B-185 kept it
// true for MONOCHROME2 while MONOCHROME1 already masked). The bits above BitsStored are not part of the sample (PS3.5
// 8.1.1), so MONOCHROME2 is masked too -- one rule for both polarities.
TEST_F(DicomReaderTest, Issue235Decided_BitsAboveBitsStoredAreMaskedForMonochrome2Too) {
    std::vector<uint16_t> w = Words(s_validDcm);
    for (size_t i = 0; i < w.size(); ++i) w[i] = static_cast<uint16_t>((w[i] & 0x0FFF) | 0xF000);
    const fs::path p = MakeScopeVariant(s_validDcm, "high_bits", [&](DcmDataset* ds) {
        ds->putAndInsertUint16(DCM_BitsStored, 12);
        ds->putAndInsertUint16(DCM_HighBit, 11);
        PutWords(ds, w);
    });
    const ScopeRead r = ReadScope(p);
    EXPECT_EQ(XPE_OK, r.read);
    ASSERT_EQ(w.size(), r.words.size());
    for (size_t i = 0; i < w.size(); ++i) ASSERT_EQ(static_cast<uint16_t>(w[i] & 0x0FFF), r.words[i]) << "pixel " << i;
}

// ===========================================================================
// QA-B-185 (#235, leader decision after QA-B-184): MONOCHROME1 is inverted on read.
//
// TC-109 / FR-DCM-109 (SRS-DICOM-001): "MONOCHROME1 (small value = bright): automatic inversion (MAX - pixel);
// MONOCHROME2: used as is." SAD-DICOM-001 step 10, README, PRD REQ-4.1.9 and RTM-DICOM row FR-DCM-109 say the same;
// until now the code returned the stored words and the tests pinned that (QA-B-182f "Pinned_..._Monochrome1IsReturned
// AsStored"). The tests below are the ones the RTM's TC-109 row points at.
//
// WHAT "MAX" IS: the mask of the buffer's BitsStored, 2^B - 1, applied to the stored word first (the bits above
// BitsStored are not part of the sample): v' = (2^B - 1) - (v & (2^B - 1)). The result is the value MONOCHROME2
// reading of the same image would have: PS3.3 C.7.6.3.1.2 says the MINIMUM sample is displayed white for
// MONOCHROME1 and black for MONOCHROME2, so the inversion maps the one onto the other.
// ===========================================================================
namespace {

/** @p src with the unique occurrence of @p from replaced by @p to (same length); false unless it occurs exactly once. */
bool PatchOnce(const fs::path& src, const fs::path& dst, const std::string& from, const std::string& to) {
    std::ifstream in(src, std::ios::binary);
    std::string b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (from.size() != to.size()) return false;
    const size_t at = b.find(from);
    if (at == std::string::npos || b.find(from, at + 1) != std::string::npos) return false;
    b.replace(at, from.size(), to);
    std::ofstream out(dst, std::ios::binary);
    out.write(b.data(), static_cast<std::streamsize>(b.size()));
    return out.good();
}

/** A conformant DX MONOCHROME1 copy of @p src: PI MONOCHROME1 and, where the file has one, Presentation LUT Shape INVERSE. */
bool MakeMonochrome1(const fs::path& src, const fs::path& dst) {
    if (!PatchOnce(src, dst, "MONOCHROME2", "MONOCHROME1")) return false;
    std::ifstream in(dst, std::ios::binary);
    const std::string b((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    if (b.find("IDENTITY") == std::string::npos) return true;   // compressed copies written by DCMTK carry no shape
    return PatchOnce(dst, dst, "IDENTITY", "INVERSE ");         // CS values are padded to even length with a space
}

uint32_t BitsMask(uint32_t bitsStored) { return bitsStored >= 16 ? 0xFFFFu : ((1u << bitsStored) - 1u); }

/** Pending alerts with @p needle in the text, and the severity of the last one. */
int AlertsWith(const char* needle, int32_t* severity = nullptr) {
    int n = 0;
    for (int32_t i = 0; i < xpe_get_pending_alert_count(); ++i) {
        char buf[512] = {0};
        int32_t sev = -1;
        if (xpe_get_pending_alert(i, buf, sizeof(buf), &sev) == XPE_OK && std::string(buf).find(needle) != std::string::npos) {
            ++n;
            if (severity) *severity = sev;
        }
    }
    return n;
}

constexpr const char* kMono1Alert = "MONOCHROME1";

// The WHOLE text is the cross-lane contract (QA-B-185b). It states the exact formula the code evaluates: the stored
// word is masked to BitsStored first, so a word with bits above BitsStored gives (2^B - 1) - (stored & (2^B - 1)).
std::string Mono1AlertText(unsigned bitsStored) {
    return "MONOCHROME1 pixel values were inverted to MONOCHROME2 sense: "
           "value = (2^BitsStored - 1) - (stored & (2^BitsStored - 1)), BitsStored " + std::to_string(bitsStored);
}

/** The text of the first pending alert that contains @p needle ("" when none). */
std::string AlertTextWith(const char* needle) {
    for (int32_t i = 0; i < xpe_get_pending_alert_count(); ++i) {
        char buf[512] = {0};
        int32_t sev = -1;
        if (xpe_get_pending_alert(i, buf, sizeof(buf), &sev) == XPE_OK && std::string(buf).find(needle) != std::string::npos) return buf;
    }
    return std::string();
}

}  // namespace

TEST_F(DicomReaderTest, Tc109_FrDcm109_Monochrome1IsInvertedAndMonochrome2IsKept_OnEveryPath) {
    struct Source { const char* name; fs::path file; };
    std::vector<Source> sources;
    sources.push_back({"native 16-bit", s_validDcm});
    // The 12-bit native file: the words really are 12-bit.
    {
        std::vector<uint16_t> w = Words(s_validDcm);
        for (uint16_t& v : w) v &= 0x0FFF;
        sources.push_back({"native 12-bit", MakeScopeVariant(s_validDcm, "m1_native12", [&](DcmDataset* ds) {
            ds->putAndInsertUint16(DCM_BitsStored, 12);
            ds->putAndInsertUint16(DCM_HighBit, 11);
            PutWords(ds, w);
        })});
    }
    const fs::path ll = s_tempDir / "jpegll_for_185.dcm";
    ASSERT_TRUE(WriteJpegLosslessCopy(s_validDcm, ll));
    sources.push_back({"JPEG Lossless", ll});
    sources.push_back({"JPEG 2000", s_j2kDcm});

    for (const Source& s : sources) {
        const fs::path m1 = s_tempDir / (std::string("m1_") + s.file.filename().string());
        ASSERT_TRUE(MakeMonochrome1(s.file, m1)) << s.name << ": the MONOCHROME1 copy could not be made";
        xpe_clear_alerts();
        const ScopeRead a = ReadScope(s.file);   // MONOCHROME2: kept
        ASSERT_EQ(XPE_OK, a.read) << s.name;
        EXPECT_EQ(0, AlertsWith(kMono1Alert)) << s.name << ": MONOCHROME2 must not mention an inversion";
        xpe_clear_alerts();
        const ScopeRead b = ReadScope(m1);       // MONOCHROME1: inverted
        ASSERT_EQ(XPE_OK, b.read) << s.name;
        ASSERT_EQ(a.words.size(), b.words.size()) << s.name;
        EXPECT_EQ(a.bitsStored, b.bitsStored) << s.name;
        EXPECT_EQ(a.bitsAllocated, b.bitsAllocated) << s.name;
        EXPECT_EQ(XPE_PIXEL_UINT16, b.format) << s.name;
        const uint32_t M = BitsMask(b.bitsStored);
        size_t wrong = 0;
        for (size_t i = 0; i < a.words.size(); ++i) {
            if (b.words[i] != static_cast<uint16_t>(M - (a.words[i] & M))) ++wrong;
        }
        EXPECT_EQ(0u, wrong) << s.name << ": " << wrong << " words differ from (2^BitsStored - 1) - value (BitsStored " << b.bitsStored << ")";
        int32_t sev = -1;
        EXPECT_EQ(1, AlertsWith(kMono1Alert, &sev)) << s.name << ": exactly one alert says the values were inverted";
        EXPECT_EQ(Mono1AlertText(b.bitsStored), AlertTextWith(kMono1Alert)) << s.name << ": the alert text is the contract text, whole";
        EXPECT_EQ(XPE_ALERT_INFO, sev) << s.name << ": it is an Info alert";
    }
}

// The bits above BitsStored are not part of the sample: they are masked off BEFORE the inversion, so a MONOCHROME1
// word never comes out larger than 2^BitsStored - 1. (QA-B-187: MONOCHROME2 is masked too.)
TEST_F(DicomReaderTest, Tc109_Monochrome1_MasksTheBitsAboveBitsStoredBeforeInverting) {
    std::vector<uint16_t> w = Words(s_validDcm);
    for (size_t i = 0; i < w.size(); ++i) w[i] = static_cast<uint16_t>((w[i] & 0x0FFF) | 0xF000);
    const fs::path m2 = MakeScopeVariant(s_validDcm, "m1_high_bits", [&](DcmDataset* ds) {
        ds->putAndInsertUint16(DCM_BitsStored, 12);
        ds->putAndInsertUint16(DCM_HighBit, 11);
        PutWords(ds, w);
    });
    const fs::path m1 = s_tempDir / "m1_high_bits_mono1.dcm";
    ASSERT_TRUE(MakeMonochrome1(m2, m1));
    const ScopeRead a = ReadScope(m2);
    const ScopeRead b = ReadScope(m1);
    ASSERT_EQ(XPE_OK, a.read);
    ASSERT_EQ(XPE_OK, b.read);
    // QA-B-187: MONOCHROME2 is masked too now (it returned the words unmasked until this card).
    for (size_t i = 0; i < w.size(); ++i) ASSERT_EQ(static_cast<uint16_t>(w[i] & 0x0FFF), a.words[i]) << "MONOCHROME2 pixel " << i;
    size_t wrong = 0, tooBig = 0;
    for (size_t i = 0; i < w.size(); ++i) {
        if (b.words[i] != static_cast<uint16_t>(0x0FFFu - (w[i] & 0x0FFFu))) ++wrong;
        if (b.words[i] > 0x0FFF) ++tooBig;
    }
    EXPECT_EQ(0u, wrong) << "MONOCHROME1: 4095 - (value & 4095)";
    EXPECT_EQ(0u, tooBig) << "a MONOCHROME1 word never exceeds 2^BitsStored - 1";
}

// A second read on the same handle must not invert again: the inversion is made on the caller's buffer, never on the
// dataset the handle keeps.
TEST_F(DicomReaderTest, Tc109_Monochrome1_ReadingTwiceOnOneHandleGivesTheSameWords) {
    const fs::path m1 = s_tempDir / "m1_twice.dcm";
    ASSERT_TRUE(MakeMonochrome1(s_validDcm, m1));
    const fs::path ll = s_tempDir / "jpegll_for_185_twice.dcm";
    ASSERT_TRUE(WriteJpegLosslessCopy(s_validDcm, ll));
    const fs::path m1ll = s_tempDir / "m1_twice_ll.dcm";
    ASSERT_TRUE(MakeMonochrome1(ll, m1ll));
    const fs::path m1j2k = s_tempDir / "m1_twice_j2k.dcm";
    ASSERT_TRUE(MakeMonochrome1(s_j2kDcm, m1j2k));
    for (const fs::path& p : {m1, m1ll, m1j2k}) {
        XpeDicomHandle* h = nullptr;
        ASSERT_EQ(XPE_OK, xpe_dicom_open(p.string().c_str(), &h)) << p;
        std::vector<uint16_t> first, second;
        for (std::vector<uint16_t>* out : {&first, &second}) {
            XpeImageBuffer img{};
            ASSERT_EQ(XPE_OK, xpe_dicom_read_image(h, &img)) << p;
            const uint16_t* d = static_cast<const uint16_t*>(img.data);
            out->assign(d, d + static_cast<size_t>(img.width) * img.height);
            xpe_free_image(&img);
        }
        xpe_dicom_close(h);
        EXPECT_EQ(first, second) << p << ": the second read on the same handle differs (inverted twice?)";
    }
}

// Signed pixels with MONOCHROME1: PS3.3 does not tie Photometric Interpretation to Pixel Representation, so the
// combination is legal, and for it the inversion would be -1 - v (a different formula). Signed pixels are refused for
// every Photometric Interpretation (the buffer is unsigned words), so the combination stays refused, untouched.
TEST_F(DicomReaderTest, Tc109_Monochrome1WithSignedPixelsIsRefusedLikeEverySignedImage) {
    std::vector<uint16_t> w = Words(s_validDcm);
    w[1] = static_cast<uint16_t>(static_cast<int16_t>(-1000));
    const fs::path p = MakeScopeVariant(s_validDcm, "m1_signed", [&](DcmDataset* ds) {
        ds->putAndInsertString(DCM_PhotometricInterpretation, "MONOCHROME1");
        ds->putAndInsertUint16(DCM_PixelRepresentation, 1);
        PutWords(ds, w);
    });
    xpe_clear_alerts();
    const ScopeRead r = ReadScope(p);
    EXPECT_EQ(XPE_ERR_UNSUPPORTED_FORMAT, r.read);
    EXPECT_TRUE(r.outUntouchedOnFailure);
    EXPECT_EQ(XPE_OK, r.metaAfter);
    EXPECT_EQ(0, AlertsWith(kMono1Alert)) << "a refusal is not an inversion: no 'inverted' alert";
}

// The reason for the decision (QA-B-184 section 4): read a MONOCHROME1 + INVERSE file, write what was read, read it
// again. Before this card the words came back as stored and the writer labelled them MONOCHROME2 + IDENTITY, so the
// copy showed the image inverted relative to the original. Now the POLARITY survives: for a file whose VOI is the
// identity and whose Rescale is 1/0 (this fixture), display(original, INVERSE) = (2^B - 1) - stored and
// display(copy, IDENTITY) = its words = the same numbers. This is a statement about polarity under an identity VOI,
// NOT about the whole presentation: xpe_dicom_write copies no Window, Rescale or Presentation LUT from the source
// (Tc109_CurrentBehaviour_WriteDoesNotCarry... below records exactly that).
TEST_F(DicomReaderTest, Tc109_Monochrome1PlusInverse_RoundTripKeepsThePolarity) {
    const fs::path m1 = s_tempDir / "m1_roundtrip.dcm";
    ASSERT_TRUE(MakeMonochrome1(s_validDcm, m1));
    const std::vector<uint16_t> stored = Words(s_validDcm);   // the words stored in the MONOCHROME1 file (same bytes)
    const ScopeRead first = ReadScope(m1);
    ASSERT_EQ(XPE_OK, first.read);

    XpeImageBuffer img{};
    ASSERT_EQ(XPE_OK, xpe_alloc_image(256, 256, XPE_PIXEL_UINT16, &img));
    std::memcpy(img.data, first.words.data(), first.words.size() * sizeof(uint16_t));
    img.bitsStored = first.bitsStored;
    img.bitsAllocated = first.bitsAllocated;
    const fs::path copy = s_tempDir / "m1_roundtrip_copy.dcm";
    XpeImageMetadata meta{};
    ASSERT_EQ(XPE_OK, xpe_dicom_write(copy.string().c_str(), &img, &meta));
    xpe_free_image(&img);

    const ScopeRead second = ReadScope(copy);
    ASSERT_EQ(XPE_OK, second.read);
    EXPECT_EQ(first.words, second.words) << "the copy's words are what the first read returned";
    const uint32_t M = BitsMask(first.bitsStored);
    size_t wrong = 0;
    for (size_t i = 0; i < stored.size(); ++i) {
        const uint32_t displayedOriginal = M - (stored[i] & M);   // INVERSE: minimum stored value is displayed white
        if (second.words[i] != displayedOriginal) ++wrong;        // IDENTITY + MONOCHROME2: the word is the brightness
    }
    EXPECT_EQ(0u, wrong) << "the copy has the opposite polarity to the original at " << wrong << " pixels (identity VOI, Rescale 1/0)";
    // And the copy carries no inversion label of its own.
    std::ifstream in(copy, std::ios::binary);
    const std::string bytes((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_NE(std::string::npos, bytes.find("MONOCHROME2"));
    EXPECT_EQ(std::string::npos, bytes.find("MONOCHROME1"));
}

// ---- RECORDS CURRENT BEHAVIOUR, IS NOT A REQUIREMENT (QA-B-185b, Codex #70) -----------------------------------------
// The inversion keeps the POLARITY across read -> write. It does not keep how the image displays in general: the writer
// builds its own dataset from the pixel buffer and the metadata struct and copies nothing from the file the pixels came
// from, so a source file's Window Center/Width, Rescale Slope/Intercept and Presentation LUT Shape are gone, and the
// copy carries Rescale 1/0, no Window and IDENTITY (REQ-DICOM-022 defaults). An image whose file had a non-identity VOI
// or Rescale therefore displays with different brightness and contrast after read -> write (PS3.3 C.11.2).
// If a later decision makes the writer preserve any of this, THIS TEST MUST CHANGE with it.
TEST_F(DicomReaderTest, Tc109_CurrentBehaviour_WriteDoesNotCarryWindowRescaleOrPresentationShapeFromTheSourceFile) {
    const fs::path m1 = MakeScopeVariant(s_validDcm, "m1_vois", [](DcmDataset* ds) {
        ds->putAndInsertString(DCM_PhotometricInterpretation, "MONOCHROME1");
        ds->putAndInsertString(DCM_PresentationLUTShape, "INVERSE");
        ds->putAndInsertString(DCM_WindowCenter, "1500");
        ds->putAndInsertString(DCM_WindowWidth, "800");
        ds->putAndInsertString(DCM_RescaleSlope, "2");
        ds->putAndInsertString(DCM_RescaleIntercept, "-1024");
    });
    // The control: the source file really carries what the test says it carries.
    {
        DcmFileFormat ff;
        ASSERT_TRUE(ff.loadFile(m1.string().c_str()).good());
        OFString v;
        ASSERT_TRUE(ff.getDataset()->findAndGetOFString(DCM_WindowCenter, v).good());
        EXPECT_EQ("1500", std::string(v.c_str()));
        ASSERT_TRUE(ff.getDataset()->findAndGetOFString(DCM_RescaleSlope, v).good());
        EXPECT_EQ("2", std::string(v.c_str()));
        ASSERT_TRUE(ff.getDataset()->findAndGetOFString(DCM_PresentationLUTShape, v).good());
        EXPECT_EQ("INVERSE", std::string(v.c_str()));
    }
    const ScopeRead first = ReadScope(m1);
    ASSERT_EQ(XPE_OK, first.read);

    XpeImageBuffer img{};
    ASSERT_EQ(XPE_OK, xpe_alloc_image(256, 256, XPE_PIXEL_UINT16, &img));
    std::memcpy(img.data, first.words.data(), first.words.size() * sizeof(uint16_t));
    img.bitsStored = first.bitsStored;
    img.bitsAllocated = first.bitsAllocated;
    const fs::path copy = s_tempDir / "m1_vois_copy.dcm";
    XpeImageMetadata meta{};
    ASSERT_EQ(XPE_OK, xpe_dicom_write(copy.string().c_str(), &img, &meta));
    xpe_free_image(&img);

    DcmFileFormat out;
    ASSERT_TRUE(out.loadFile(copy.string().c_str()).good());
    DcmDataset* ds = out.getDataset();
    OFString v;
    EXPECT_FALSE(ds->tagExists(DCM_WindowCenter)) << "the source's Window Center is not carried";
    EXPECT_FALSE(ds->tagExists(DCM_WindowWidth)) << "the source's Window Width is not carried";
    ASSERT_TRUE(ds->findAndGetOFString(DCM_RescaleSlope, v).good());
    EXPECT_EQ("1", std::string(v.c_str())) << "Rescale Slope is the writer's default, not the source's 2";
    ASSERT_TRUE(ds->findAndGetOFString(DCM_RescaleIntercept, v).good());
    EXPECT_EQ("0", std::string(v.c_str())) << "Rescale Intercept is the writer's default, not the source's -1024";
    ASSERT_TRUE(ds->findAndGetOFString(DCM_PresentationLUTShape, v).good());
    EXPECT_EQ("IDENTITY", std::string(v.c_str())) << "the shape is the writer's fixed IDENTITY, not the source's INVERSE";
    ASSERT_TRUE(ds->findAndGetOFString(DCM_PhotometricInterpretation, v).good());
    EXPECT_EQ("MONOCHROME2", std::string(v.c_str()));
}

// ===========================================================================
// QA-B-187 (#235 closing): the five items of the issue title x the three compression paths, as NAMED tests.
//
// QA-B-186 read these cells with a temporary probe and found three with no named test and one that was silent
// (Rescale). Every cell below is `Tc235_<item>_<path>_<expectation>`, so `grep Tc235_` lists the whole matrix and a
// regression in any one cell turns exactly that test red. Paths: Native (uncompressed Explicit VR LE), JpegLl
// (JPEG Lossless), J2k (JPEG 2000 Lossless). The files are built with the helpers the earlier scope tests use.
// ===========================================================================
namespace {

enum class PathId { Native, JpegLl, J2k };

struct Tc235Env {
    bool ok = false;
    fs::path ll;
    std::vector<uint8_t> codestream;
    std::vector<uint16_t> base;   // the unmodified donor's words (all three donors read back the same words)
};

Tc235Env& Env235() {
    static Tc235Env env;
    static fs::path builtFor;
    if (builtFor != DicomReaderTest::TempDir()) {   // the suite's temp directory is created once; rebuild if it moved
        builtFor = DicomReaderTest::TempDir();
        env = Tc235Env{};
        env.ll = DicomReaderTest::TempDir() / "jpegll_for_235.dcm";
        env.ok = WriteJpegLosslessCopy(DicomReaderTest::ValidDcm(), env.ll) && ExtractJ2kBitstream(DicomReaderTest::J2kDcm(), env.codestream);
        if (env.ok) {
            env.base = Words(DicomReaderTest::ValidDcm());
            env.ok = env.base.size() == 65536u && Words(env.ll) == env.base && Words(DicomReaderTest::J2kDcm()) == env.base;
        }
    }
    return env;
}

using Mutator = void (*)(DcmDataset*, bool native);

/** The file for @p path with @p mutate applied; @p native tells a mutator that it may rewrite the pixel words. */
fs::path File235(PathId path, const char* tag, Mutator mutate) {
    Tc235Env& e = Env235();
    const char* suffix = path == PathId::Native ? "_nat" : path == PathId::JpegLl ? "_ll" : "_j2k";
    const std::string name = std::string("tc235_") + tag + suffix;
    const bool native = path == PathId::Native;
    auto apply = [&](DcmDataset* ds) { mutate(ds, native); };
    if (path == PathId::Native) return MakeSameSyntaxVariant(DicomReaderTest::ValidDcm(), name.c_str(), apply);
    if (path == PathId::JpegLl) return MakeSameSyntaxVariant(e.ll, name.c_str(), apply);
    return MakeJ2kVariant(DicomReaderTest::J2kDcm(), name.c_str(), e.codestream, apply);
}

struct Alert235 { std::string text; int32_t severity; };

struct Obs235 {
    ScopeRead read;
    std::vector<Alert235> alerts;
};

Obs235 Observe235(const fs::path& file) {
    Obs235 o;
    xpe_clear_alerts();
    o.read = ReadScope(file);
    for (int32_t i = 0; i < xpe_get_pending_alert_count(); ++i) {
        char buf[512] = {0};
        int32_t sev = -1;
        if (xpe_get_pending_alert(i, buf, sizeof(buf), &sev) == XPE_OK) o.alerts.push_back({buf, sev});
    }
    return o;
}

/** The refusal cell: the code, nothing written, the handle still serves its metadata, ONE Error alert that names @p needle. */
void ExpectRefused235(PathId path, const char* tag, Mutator mutate, XpeErrorCode code, const char* needle) {
    ASSERT_TRUE(Env235().ok) << "the fixtures of the matrix could not be built";
    const Obs235 o = Observe235(File235(path, tag, mutate));
    EXPECT_EQ(code, o.read.read);
    EXPECT_TRUE(o.read.outUntouchedOnFailure) << "the caller's buffer must not be touched";
    EXPECT_EQ(XPE_OK, o.read.metaAfter) << "the same handle still serves its metadata";
    ASSERT_EQ(1u, o.alerts.size()) << "a refusal posts exactly one alert";
    EXPECT_EQ(XPE_ALERT_ERROR, o.alerts[0].severity);
    EXPECT_NE(std::string::npos, o.alerts[0].text.find(needle)) << "the alert names the cause: " << o.alerts[0].text;
}

// ---- the contract texts (cross-lane: clients display them), whole -------------------------------------------------
std::string RescaleWarning235(const char* slope, const char* intercept) {
    return std::string("RescaleSlope ") + slope + ", RescaleIntercept " + intercept +
           " (the identity is 1 and 0): returned pixels are stored values; rescale not applied";
}

// ---- mutators ------------------------------------------------------------------------------------------------------
void MutSigned(DcmDataset* ds, bool) { ds->putAndInsertUint16(DCM_PixelRepresentation, 1); }
void MutMonochrome1(DcmDataset* ds, bool) { ds->putAndInsertString(DCM_PhotometricInterpretation, "MONOCHROME1"); }
void MutRescaleNonIdentity(DcmDataset* ds, bool) {
    ds->putAndInsertString(DCM_RescaleSlope, "2");
    ds->putAndInsertString(DCM_RescaleIntercept, "-1024");
}
void MutRescaleIdentityExplicit(DcmDataset* ds, bool) {
    ds->putAndInsertString(DCM_RescaleSlope, "1.0");
    ds->putAndInsertString(DCM_RescaleIntercept, "0.0");
}
void MutRescaleSlopeZero(DcmDataset* ds, bool) {
    ds->putAndInsertString(DCM_RescaleSlope, "0");
    ds->putAndInsertString(DCM_RescaleIntercept, "0");
}
void MutRescaleSlopeNotANumber(DcmDataset* ds, bool) { ds->putAndInsertString(DCM_RescaleSlope, "abc"); }
void MutRescaleInterceptNotANumber(DcmDataset* ds, bool) { ds->putAndInsertString(DCM_RescaleIntercept, "xyz"); }
void MutRescaleSlopeEmpty(DcmDataset* ds, bool) { ds->putAndInsertString(DCM_RescaleSlope, ""); }
void MutRescaleSlopeNotFinite(DcmDataset* ds, bool) { ds->putAndInsertString(DCM_RescaleSlope, "inf"); }
void MutRescaleTwoValues(DcmDataset* ds, bool) { ds->putAndInsertString(DCM_RescaleSlope, "1\\2"); }
// QA-B-187c: PS3.3 C.11.1 -- the Slope / Intercept pair is required together. The writer's donor carries both, so the
// one-sided files are made by deleting the other element.
void MutRescaleSlopeOnly(DcmDataset* ds, bool) {
    ds->putAndInsertString(DCM_RescaleSlope, "2");
    ds->findAndDeleteElement(DCM_RescaleIntercept);
}
void MutRescaleInterceptOnly(DcmDataset* ds, bool) {
    ds->putAndInsertString(DCM_RescaleIntercept, "-1024");
    ds->findAndDeleteElement(DCM_RescaleSlope);
}
void MutRescaleBothAbsent(DcmDataset* ds, bool) {
    ds->findAndDeleteElement(DCM_RescaleSlope);
    ds->findAndDeleteElement(DCM_RescaleIntercept);
}
// A Modality LUT Sequence instead of the pair (the other legal form of the module).
void MutModalityLutSequence(DcmDataset* ds, bool) {
    ds->findAndDeleteElement(DCM_RescaleSlope);
    ds->findAndDeleteElement(DCM_RescaleIntercept);
    DcmItem* item = nullptr;
    ds->findOrCreateSequenceItem(DCM_ModalityLUTSequence, item, -2);
    if (item != nullptr) item->putAndInsertString(DCM_LUTExplanation, "tc235 modality lut");
}
// Legal DS notations (PS3.5 6.2): exponent, explicit plus, padding spaces, a decimal point.
void MutRescaleDsExponent(DcmDataset* ds, bool) {
    ds->putAndInsertString(DCM_RescaleSlope, "1E0");
    ds->putAndInsertString(DCM_RescaleIntercept, "0e0");
}
void MutRescaleDsPlusSign(DcmDataset* ds, bool) {
    ds->putAndInsertString(DCM_RescaleSlope, "+1");
    ds->putAndInsertString(DCM_RescaleIntercept, "+0");
}
void MutRescaleDsPadded(DcmDataset* ds, bool) {
    ds->putAndInsertString(DCM_RescaleSlope, " 1 ");
    ds->putAndInsertString(DCM_RescaleIntercept, " 0 ");
}
void MutRescaleDsDecimal(DcmDataset* ds, bool) {
    ds->putAndInsertString(DCM_RescaleSlope, "1");
    ds->putAndInsertString(DCM_RescaleIntercept, "-1024.0");
}
void MutRescaleInterceptTwoValues(DcmDataset* ds, bool) { ds->putAndInsertString(DCM_RescaleIntercept, "0\\1"); }
void MutMultiFrame(DcmDataset* ds, bool native) {
    ds->putAndInsertString(DCM_NumberOfFrames, "3");
    if (native) {
        const std::vector<uint16_t> one = Words(DicomReaderTest::ValidDcm());
        std::vector<uint16_t> three;
        for (int k = 0; k < 3; ++k) three.insert(three.end(), one.begin(), one.end());
        PutWords(ds, three);
    }
}
void MutRgb(DcmDataset* ds, bool) {
    ds->putAndInsertUint16(DCM_SamplesPerPixel, 3);
    ds->putAndInsertString(DCM_PhotometricInterpretation, "RGB");
    ds->putAndInsertUint16(DCM_PlanarConfiguration, 0);
}
void MutRgbLabelOnOnePlane(DcmDataset* ds, bool) { ds->putAndInsertString(DCM_PhotometricInterpretation, "RGB"); }

void ExpectMonochrome1Inverted235(PathId path) {
    ASSERT_TRUE(Env235().ok);
    const Obs235 o = Observe235(File235(path, "mono1", MutMonochrome1));
    ASSERT_EQ(XPE_OK, o.read.read);
    ASSERT_EQ(Env235().base.size(), o.read.words.size());
    const uint32_t M = BitsMask(o.read.bitsStored);
    size_t wrong = 0;
    for (size_t i = 0; i < o.read.words.size(); ++i) {
        if (o.read.words[i] != static_cast<uint16_t>(M - (Env235().base[i] & M))) ++wrong;
    }
    EXPECT_EQ(0u, wrong) << "every word is (2^B - 1) - (stored & (2^B - 1))";
    ASSERT_EQ(1u, o.alerts.size());
    EXPECT_EQ(XPE_ALERT_INFO, o.alerts[0].severity);
    EXPECT_EQ(Mono1AlertText(o.read.bitsStored), o.alerts[0].text);
}

void ExpectRescaleStoredWithWarning235(PathId path) {
    ASSERT_TRUE(Env235().ok);
    const Obs235 o = Observe235(File235(path, "rescale", MutRescaleNonIdentity));
    ASSERT_EQ(XPE_OK, o.read.read);
    EXPECT_EQ(Env235().base, o.read.words) << "the returned pixels are the stored values, not rescaled";
    ASSERT_EQ(1u, o.alerts.size()) << "one Warning says so";
    EXPECT_EQ(XPE_ALERT_WARNING, o.alerts[0].severity);
    EXPECT_EQ(RescaleWarning235("2", "-1024"), o.alerts[0].text);
}

void ExpectRescaleIdentityQuiet235(PathId path) {
    ASSERT_TRUE(Env235().ok);
    const Obs235 o = Observe235(File235(path, "rescale_id", MutRescaleIdentityExplicit));
    ASSERT_EQ(XPE_OK, o.read.read);
    EXPECT_EQ(Env235().base, o.read.words);
    EXPECT_TRUE(o.alerts.empty()) << "an explicit identity (1.0 and 0.0) is not announced";
}

// A readable DS spelling: the pixels are stored values; warnSlope == nullptr means the rescale it spells is the identity.
void ExpectDsAccepted235(PathId path, const char* tag, Mutator mut, const char* warnSlope, const char* warnIntercept) {
    ASSERT_TRUE(Env235().ok);
    const Obs235 o = Observe235(File235(path, tag, mut));
    ASSERT_EQ(XPE_OK, o.read.read);
    EXPECT_EQ(Env235().base, o.read.words);
    if (warnSlope == nullptr) {
        EXPECT_TRUE(o.alerts.empty()) << "an identity spelled in a legal notation is not announced";
    } else {
        ASSERT_EQ(1u, o.alerts.size());
        EXPECT_EQ(XPE_ALERT_WARNING, o.alerts[0].severity);
        EXPECT_EQ(RescaleWarning235(warnSlope, warnIntercept), o.alerts[0].text);
    }
}

void ExpectBothAbsentIdentity235(PathId path) {
    ASSERT_TRUE(Env235().ok);
    const Obs235 o = Observe235(File235(path, "rescale_none", MutRescaleBothAbsent));
    ASSERT_EQ(XPE_OK, o.read.read);
    EXPECT_EQ(Env235().base, o.read.words);
    EXPECT_TRUE(o.alerts.empty()) << "both absent is the identity: nothing is announced";
}

void ExpectModalityLutWarned235(PathId path) {
    ASSERT_TRUE(Env235().ok);
    const Obs235 o = Observe235(File235(path, "mod_lut", MutModalityLutSequence));
    ASSERT_EQ(XPE_OK, o.read.read);
    EXPECT_EQ(Env235().base, o.read.words) << "the stored values are returned, the LUT is not applied";
    ASSERT_EQ(1u, o.alerts.size()) << "one Warning says so";
    EXPECT_EQ(XPE_ALERT_WARNING, o.alerts[0].severity);
    EXPECT_EQ("ModalityLUTSequence (0028,3000) is present: returned pixels are stored values; the Modality LUT was not applied",
              o.alerts[0].text);
}

}  // namespace

#define TC235_REFUSED(Item, Path, Expect, Code, Mut, Needle) \
    TEST_F(DicomReaderTest, Tc235_##Item##_##Path##_##Expect) { ExpectRefused235(PathId::Path, #Item, Mut, Code, Needle); }

// ---- 1. signed pixels ----------------------------------------------------------------------------------------------
TC235_REFUSED(Signed, Native, RefusedUnsupported, XPE_ERR_UNSUPPORTED_FORMAT, MutSigned, "PixelRepresentation 1")
TC235_REFUSED(Signed, JpegLl, RefusedUnsupported, XPE_ERR_UNSUPPORTED_FORMAT, MutSigned, "PixelRepresentation 1")
TC235_REFUSED(Signed, J2k, RefusedUnsupported, XPE_ERR_UNSUPPORTED_FORMAT, MutSigned, "PixelRepresentation 1")

// ---- 2. MONOCHROME1 ------------------------------------------------------------------------------------------------
TEST_F(DicomReaderTest, Tc235_Monochrome1_Native_InvertedWithInfoAlert) { ExpectMonochrome1Inverted235(PathId::Native); }
TEST_F(DicomReaderTest, Tc235_Monochrome1_JpegLl_InvertedWithInfoAlert) { ExpectMonochrome1Inverted235(PathId::JpegLl); }
TEST_F(DicomReaderTest, Tc235_Monochrome1_J2k_InvertedWithInfoAlert) { ExpectMonochrome1Inverted235(PathId::J2k); }

// ---- 3. rescale: stored values and a Warning when it is not the identity; refusal when it cannot be read -------------
TEST_F(DicomReaderTest, Tc235_RescaleNonIdentity_Native_StoredValuesWithWarning) { ExpectRescaleStoredWithWarning235(PathId::Native); }
TEST_F(DicomReaderTest, Tc235_RescaleNonIdentity_JpegLl_StoredValuesWithWarning) { ExpectRescaleStoredWithWarning235(PathId::JpegLl); }
TEST_F(DicomReaderTest, Tc235_RescaleNonIdentity_J2k_StoredValuesWithWarning) { ExpectRescaleStoredWithWarning235(PathId::J2k); }
TEST_F(DicomReaderTest, Tc235_RescaleIdentityExplicit_Native_StoredValuesNoAlert) { ExpectRescaleIdentityQuiet235(PathId::Native); }
TEST_F(DicomReaderTest, Tc235_RescaleIdentityExplicit_JpegLl_StoredValuesNoAlert) { ExpectRescaleIdentityQuiet235(PathId::JpegLl); }
TEST_F(DicomReaderTest, Tc235_RescaleIdentityExplicit_J2k_StoredValuesNoAlert) { ExpectRescaleIdentityQuiet235(PathId::J2k); }
TC235_REFUSED(RescaleSlopeZero, Native, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRescaleSlopeZero, "RescaleSlope")
TC235_REFUSED(RescaleSlopeZero, JpegLl, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRescaleSlopeZero, "RescaleSlope")
TC235_REFUSED(RescaleSlopeZero, J2k, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRescaleSlopeZero, "RescaleSlope")
TC235_REFUSED(RescaleSlopeNotANumber, Native, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRescaleSlopeNotANumber, "RescaleSlope")
TC235_REFUSED(RescaleSlopeNotANumber, JpegLl, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRescaleSlopeNotANumber, "RescaleSlope")
TC235_REFUSED(RescaleSlopeNotANumber, J2k, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRescaleSlopeNotANumber, "RescaleSlope")
TC235_REFUSED(RescaleInterceptNotANumber, Native, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRescaleInterceptNotANumber, "RescaleIntercept")
TC235_REFUSED(RescaleInterceptNotANumber, JpegLl, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRescaleInterceptNotANumber, "RescaleIntercept")
TC235_REFUSED(RescaleInterceptNotANumber, J2k, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRescaleInterceptNotANumber, "RescaleIntercept")
TC235_REFUSED(RescaleSlopeEmpty, Native, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRescaleSlopeEmpty, "RescaleSlope")
TC235_REFUSED(RescaleSlopeEmpty, JpegLl, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRescaleSlopeEmpty, "RescaleSlope")
TC235_REFUSED(RescaleSlopeEmpty, J2k, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRescaleSlopeEmpty, "RescaleSlope")
TC235_REFUSED(RescaleSlopeNotFinite, Native, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRescaleSlopeNotFinite, "RescaleSlope")
TC235_REFUSED(RescaleSlopeNotFinite, JpegLl, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRescaleSlopeNotFinite, "RescaleSlope")
TC235_REFUSED(RescaleSlopeNotFinite, J2k, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRescaleSlopeNotFinite, "RescaleSlope")
TC235_REFUSED(RescaleSlopeTwoValues, Native, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRescaleTwoValues, "RescaleSlope")
TC235_REFUSED(RescaleSlopeTwoValues, JpegLl, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRescaleTwoValues, "RescaleSlope")
TC235_REFUSED(RescaleSlopeTwoValues, J2k, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRescaleTwoValues, "RescaleSlope")

// ---- 3b. QA-B-187c: one side of the pair, the Modality LUT Sequence, legal DS notations ------------------------------
TC235_REFUSED(RescaleSlopeOnly, Native, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRescaleSlopeOnly, "RescaleIntercept (0028,1052) is absent")
TC235_REFUSED(RescaleSlopeOnly, JpegLl, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRescaleSlopeOnly, "RescaleIntercept (0028,1052) is absent")
TC235_REFUSED(RescaleSlopeOnly, J2k, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRescaleSlopeOnly, "RescaleIntercept (0028,1052) is absent")
TC235_REFUSED(RescaleInterceptOnly, Native, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRescaleInterceptOnly, "RescaleSlope (0028,1053) is absent")
TC235_REFUSED(RescaleInterceptOnly, JpegLl, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRescaleInterceptOnly, "RescaleSlope (0028,1053) is absent")
TC235_REFUSED(RescaleInterceptOnly, J2k, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRescaleInterceptOnly, "RescaleSlope (0028,1053) is absent")
TC235_REFUSED(RescaleInterceptTwoValues, Native, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRescaleInterceptTwoValues, "RescaleIntercept")
TEST_F(DicomReaderTest, Tc235_RescaleBothAbsent_Native_IdentityNoAlert) { ExpectBothAbsentIdentity235(PathId::Native); }
TEST_F(DicomReaderTest, Tc235_RescaleBothAbsent_JpegLl_IdentityNoAlert) { ExpectBothAbsentIdentity235(PathId::JpegLl); }
TEST_F(DicomReaderTest, Tc235_RescaleBothAbsent_J2k_IdentityNoAlert) { ExpectBothAbsentIdentity235(PathId::J2k); }
TEST_F(DicomReaderTest, Tc235_ModalityLutSequence_Native_StoredValuesWithWarning) { ExpectModalityLutWarned235(PathId::Native); }
TEST_F(DicomReaderTest, Tc235_ModalityLutSequence_JpegLl_StoredValuesWithWarning) { ExpectModalityLutWarned235(PathId::JpegLl); }
TEST_F(DicomReaderTest, Tc235_ModalityLutSequence_J2k_StoredValuesWithWarning) { ExpectModalityLutWarned235(PathId::J2k); }
TEST_F(DicomReaderTest, Tc235_RescaleDsExponent_Native_Accepted) { ExpectDsAccepted235(PathId::Native, "ds_exp", MutRescaleDsExponent, nullptr, nullptr); }
TEST_F(DicomReaderTest, Tc235_RescaleDsPlusSign_Native_Accepted) { ExpectDsAccepted235(PathId::Native, "ds_plus", MutRescaleDsPlusSign, nullptr, nullptr); }
TEST_F(DicomReaderTest, Tc235_RescaleDsPadded_Native_Accepted) { ExpectDsAccepted235(PathId::Native, "ds_pad", MutRescaleDsPadded, nullptr, nullptr); }
TEST_F(DicomReaderTest, Tc235_RescaleDsDecimal_Native_AcceptedWithWarning) { ExpectDsAccepted235(PathId::Native, "ds_dec", MutRescaleDsDecimal, "1", "-1024.0"); }

// ---- 4. multi-frame --------------------------------------------------------------------------------------------------
TC235_REFUSED(MultiFrame, Native, RefusedUnsupported, XPE_ERR_UNSUPPORTED_FORMAT, MutMultiFrame, "NumberOfFrames 3")
TC235_REFUSED(MultiFrame, JpegLl, RefusedUnsupported, XPE_ERR_UNSUPPORTED_FORMAT, MutMultiFrame, "NumberOfFrames 3")
TC235_REFUSED(MultiFrame, J2k, RefusedUnsupported, XPE_ERR_UNSUPPORTED_FORMAT, MutMultiFrame, "NumberOfFrames 3")

// ---- 5. RGB ----------------------------------------------------------------------------------------------------------
TC235_REFUSED(Rgb, Native, RefusedUnsupported, XPE_ERR_UNSUPPORTED_FORMAT, MutRgb, "SamplesPerPixel 3")
TC235_REFUSED(Rgb, JpegLl, RefusedUnsupported, XPE_ERR_UNSUPPORTED_FORMAT, MutRgb, "SamplesPerPixel 3")
TC235_REFUSED(Rgb, J2k, RefusedUnsupported, XPE_ERR_UNSUPPORTED_FORMAT, MutRgb, "SamplesPerPixel 3")
TC235_REFUSED(RgbLabelOnOnePlane, Native, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRgbLabelOnOnePlane, "RGB with SamplesPerPixel 1")
TC235_REFUSED(RgbLabelOnOnePlane, JpegLl, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRgbLabelOnOnePlane, "RGB with SamplesPerPixel 1")
TC235_REFUSED(RgbLabelOnOnePlane, J2k, RefusedInvalid, XPE_ERR_DICOM_INVALID, MutRgbLabelOnOnePlane, "RGB with SamplesPerPixel 1")

// ---- (h) MONOCHROME2: the bits above BitsStored are not part of the sample ---------------------------------------------
// Words with the four bits above BitsStored 12 set: they are masked off (value & 4095) and ONE Info alert counts the
// words that changed. A file whose words are all within range changes nothing and posts nothing (the control).
namespace {
std::string BitsAboveAlert235(size_t changed, unsigned bitsStored) {
    return std::to_string(changed) + " pixel(s) had bits above BitsStored " + std::to_string(bitsStored) +
           " set; those bits were masked off: value = stored & (2^BitsStored - 1)";
}
}  // namespace

TEST_F(DicomReaderTest, Tc235_BitsAboveBitsStored_Native_MaskedWithInfoAlert) {
    ASSERT_TRUE(Env235().ok);
    std::vector<uint16_t> w = Env235().base;
    size_t changed = 0;
    for (size_t i = 0; i < w.size(); ++i) {
        const uint16_t clean = static_cast<uint16_t>(w[i] & 0x0FFF);
        if (i % 2 == 0) { w[i] = static_cast<uint16_t>(clean | 0xF000); ++changed; }   // half the words carry junk above BitsStored
        else w[i] = clean;
    }
    const fs::path p = MakeSameSyntaxVariant(DicomReaderTest::ValidDcm(), "tc235_bits_above", [&](DcmDataset* ds) {
        ds->putAndInsertUint16(DCM_BitsStored, 12);
        ds->putAndInsertUint16(DCM_HighBit, 11);
        PutWords(ds, w);
    });
    const Obs235 o = Observe235(p);
    ASSERT_EQ(XPE_OK, o.read.read);
    ASSERT_EQ(w.size(), o.read.words.size());
    size_t wrong = 0;
    for (size_t i = 0; i < w.size(); ++i) {
        if (o.read.words[i] != (w[i] & 0x0FFF)) ++wrong;
    }
    EXPECT_EQ(0u, wrong) << "every word is value & (2^BitsStored - 1)";
    ASSERT_EQ(1u, o.alerts.size());
    EXPECT_EQ(XPE_ALERT_INFO, o.alerts[0].severity);
    EXPECT_EQ(BitsAboveAlert235(changed, 12), o.alerts[0].text);
}

TEST_F(DicomReaderTest, Tc235_BitsAboveBitsStored_Native_NothingToMaskPostsNothing) {
    ASSERT_TRUE(Env235().ok);
    std::vector<uint16_t> w = Env235().base;
    for (uint16_t& v : w) v &= 0x0FFF;
    const fs::path p = MakeSameSyntaxVariant(DicomReaderTest::ValidDcm(), "tc235_bits_clean", [&](DcmDataset* ds) {
        ds->putAndInsertUint16(DCM_BitsStored, 12);
        ds->putAndInsertUint16(DCM_HighBit, 11);
        PutWords(ds, w);
    });
    const Obs235 o = Observe235(p);
    ASSERT_EQ(XPE_OK, o.read.read);
    EXPECT_EQ(w, o.read.words);
    EXPECT_TRUE(o.alerts.empty());
}

// JPEG Lossless allows a frame precision above BitsStored (PS3.5 8.2: P < BitsStored is the violation), so a lossless
// file that declares BitsStored 12 can carry 16-bit samples: the bits above BitsStored are masked there too. (A JPEG
// 2000 codestream's precision must EQUAL BitsStored, so no word of it can carry such bits; there is no J2k cell.)
TEST_F(DicomReaderTest, Tc235_BitsAboveBitsStored_JpegLl_MaskedWithInfoAlert) {
    ASSERT_TRUE(Env235().ok);
    const std::vector<uint16_t>& base = Env235().base;
    size_t changed = 0;
    for (uint16_t v : base) if ((v & 0xF000) != 0) ++changed;
    ASSERT_GT(changed, 0u) << "the donor's gradient must have words above 4095";
    const fs::path p = MakeSameSyntaxVariant(Env235().ll, "tc235_ll_bits_above", [](DcmDataset* ds) {
        ds->putAndInsertUint16(DCM_BitsStored, 12);
        ds->putAndInsertUint16(DCM_HighBit, 11);
    });
    const Obs235 o = Observe235(p);
    ASSERT_EQ(XPE_OK, o.read.read);
    ASSERT_EQ(base.size(), o.read.words.size());
    size_t wrong = 0;
    for (size_t i = 0; i < base.size(); ++i) {
        if (o.read.words[i] != (base[i] & 0x0FFF)) ++wrong;
    }
    EXPECT_EQ(0u, wrong);
    ASSERT_EQ(1u, o.alerts.size());
    EXPECT_EQ(XPE_ALERT_INFO, o.alerts[0].severity);
    EXPECT_EQ(BitsAboveAlert235(changed, 12), o.alerts[0].text);
}

// ---------------------------------------------------------------------------
// QA-B-207 C3: what makes a file DICOM Part 10 at the door of xpe_dicom_open -- a 128-byte preamble then "DICM".
// ---------------------------------------------------------------------------

namespace {

std::string ReadAllBytes(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

/**
 * A bare Explicit-VR-LE dataset that DCMTK parses by itself (one PatientName element with a long value), whose bytes 128..131
 * are @p four. With "DICM" there it would be taken for a Part 10 magic; with anything else it is the case that only the door
 * of xpe_dicom_open can refuse, because DCMTK reads the file happily from byte 0 (a file made of zeros plus a wrong magic is
 * refused by DCMTK's own parse, which hides a door that is too lenient about the magic).
 */
std::string BareDatasetWithBytesAt128(const char* four) {
    std::string value(300, 'A');
    value.replace(120, 4, four, 4);                      // the value starts at offset 8
    std::string bytes;
    bytes.push_back(static_cast<char>(0x10)); bytes.push_back(0x00);          // tag (0010,0010)
    bytes.push_back(static_cast<char>(0x10)); bytes.push_back(0x00);
    bytes += "PN";
    bytes.push_back(static_cast<char>(300 & 0xFF)); bytes.push_back(static_cast<char>(300 >> 8));   // length 300
    bytes += value;
    return bytes;
}

XpeErrorCode OpenBytes(const fs::path& path, const std::string& bytes, XpeDicomHandle** h) {
    {
        std::ofstream f(path, std::ios::binary);
        f.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    }
    return xpe_dicom_open(path.string().c_str(), h);
}

}  // namespace

TEST_F(DicomReaderTest, AFileThatIsNotPartTenIsDicomInvalidWhateverItContains) {
    const std::string good = ReadAllBytes(s_validDcm);
    ASSERT_GT(good.size(), 200u);
    ASSERT_EQ("DICM", good.substr(128, 4)) << "precondition: the fixture is Part 10";
    const std::string dataset = good.substr(132);   // meta group and all, but no preamble and no magic

    struct Case {
        const char* what;
        std::string bytes;
    } cases[] = {
        {"the dataset bytes alone, no preamble and no magic", dataset},
        {"128 zero bytes and a wrong magic (XXXX) in front of the same data", std::string(128, '\0') + "XXXX" + good.substr(132)},
        {"the magic differs in its last byte (DICX)", std::string(128, '\0') + "DICX" + good.substr(132)},
        {"the magic in lower case (dicm)", std::string(128, '\0') + "dicm" + good.substr(132)},
        {"a bare dataset DCMTK reads by itself, bytes 128..131 = DICX", BareDatasetWithBytesAt128("DICX")},
        {"a bare dataset DCMTK reads by itself, bytes 128..131 = dicm", BareDatasetWithBytesAt128("dicm")},
        {"a bare dataset DCMTK reads by itself, bytes 128..131 = DICN", BareDatasetWithBytesAt128("DICN")},
        {"the magic at offset 0 instead of 128", "DICM" + good.substr(132)},
        {"the magic one byte late (offset 129)", std::string(129, '\0') + "DICM" + good.substr(133)},
        {"131 bytes: one short of the magic's end", good.substr(0, 131)},
        {"empty file", std::string()},
        {"plain text", std::string("this is not a dicom file ") + std::string(300, 'x')},
    };
    int n = 0;
    for (const Case& c : cases) {
        XpeDicomHandle* h = reinterpret_cast<XpeDicomHandle*>(0x1);
        EXPECT_EQ(XPE_ERR_DICOM_INVALID, OpenBytes(s_tempDir / ("c3_" + std::to_string(n++) + ".dcm"), c.bytes, &h)) << c.what;
        EXPECT_EQ(nullptr, h) << c.what;
    }
}

TEST_F(DicomReaderTest, ThePreambleBytesThemselvesAreNotJudgedAndAMissingFileIsStillAnIoError) {
    const std::string good = ReadAllBytes(s_validDcm);
    ASSERT_EQ("DICM", good.substr(128, 4));

    // any 128 bytes are a valid preamble (PS3.10 7.5.1): text, a TIFF header, noise
    std::string withNoise = good;
    for (size_t i = 0; i < 128; ++i) withNoise[i] = static_cast<char>(0x41 + (i % 26));
    XpeDicomHandle* h = nullptr;
    ASSERT_EQ(XPE_OK, OpenBytes(s_tempDir / "c3_preamble_text.dcm", withNoise, &h)) << "a non-zero preamble is still Part 10";
    ASSERT_NE(nullptr, h);
    xpe_dicom_close(h);

    // the mapping of a file that cannot be opened is not the door's business
    XpeDicomHandle* none = nullptr;
    EXPECT_EQ(XPE_ERR_IO_FAILED, xpe_dicom_open((s_tempDir / "c3_does_not_exist.dcm").string().c_str(), &none));
    EXPECT_EQ(nullptr, none);
}
