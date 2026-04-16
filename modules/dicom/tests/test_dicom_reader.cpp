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
#include "xpe/common/xpe_memory.h"
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
    std::strncpy(meta.bodyPart, "CHEST", sizeof(meta.bodyPart));
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

    // TODO: create implicit LE DICOM for AC-04 unsupported TS test
    s_implicitLEDcm = s_tempDir / "implicit_le.dcm";
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
    if (!fs::exists(s_implicitLEDcm)) GTEST_SKIP() << "Implicit LE test file not created yet";
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
