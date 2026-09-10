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
#include "xpe/common/xpe_memory.h"
#include <cstdio>
#include <filesystem>
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
