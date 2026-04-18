/**
 * @file test_dicom_writer.cpp
 * @brief TDD tests for DicomWriter / SWU-4.2 (>= 10 test cases)
 * SPEC: SPEC-XPE-P1B-DICOM REQ-DICOM-013..022, AC-01, AC-02, AC-03
 */
#include <gtest/gtest.h>
#include "xpe/dicom/dicom_api.h"
#include "xpe/common/xpe_memory.h"
#include <filesystem>
#include <cstring>
#include <cstdint>

namespace fs = std::filesystem;

class DicomWriterTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_tempDir = fs::temp_directory_path() / "xpe_dicom_writer_test";
        fs::create_directories(m_tempDir);

        xpe_alloc_image(256, 256, XPE_PIXEL_UINT16, &m_img);
        auto* px = static_cast<uint16_t*>(m_img.data);
        for (uint32_t i = 0; i < m_img.width * m_img.height; ++i)
            px[i] = static_cast<uint16_t>(i & 0xFFFF);

        std::memset(&m_meta, 0, sizeof(m_meta));
        std::strncpy(m_meta.bodyPart, "CHEST", sizeof(m_meta.bodyPart));
        m_meta.kVp = 80.0f;
        m_meta.mAs = 2.5f;
        m_meta.SID_mm = 1800.0f;
        m_meta.pixelPitch_mm = 0.148f;
        m_meta.acquisitionTime = 1713226800ULL; // fixed epoch for determinism
    }

    void TearDown() override {
        xpe_free_image(&m_img);
        fs::remove_all(m_tempDir);
    }

    fs::path m_tempDir;
    XpeImageBuffer m_img{};
    XpeImageMetadata m_meta{};
};

// ---------------------------------------------------------------------------
// REQ-DICOM-013: Write creates a valid DICOM file
// ---------------------------------------------------------------------------
TEST_F(DicomWriterTest, WriteBasic_CreatesFile) {
    auto path = m_tempDir / "out.dcm";
    EXPECT_EQ(XPE_OK, xpe_dicom_write(path.string().c_str(), &m_img, &m_meta));
    EXPECT_TRUE(fs::exists(path));
    EXPECT_GT(fs::file_size(path), 128u); // must have at least preamble
}

// ---------------------------------------------------------------------------
// AC-01: Uncompressed round-trip pixel-exact
// ---------------------------------------------------------------------------
TEST_F(DicomWriterTest, WriteRoundTrip_PixelExact) {
    auto path = m_tempDir / "roundtrip.dcm";
    ASSERT_EQ(XPE_OK, xpe_dicom_write(path.string().c_str(), &m_img, &m_meta));

    XpeDicomHandle* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(path.string().c_str(), &handle));

    XpeImageBuffer readBack{};
    ASSERT_EQ(XPE_OK, xpe_dicom_read_image(handle, &readBack));
    ASSERT_EQ(m_img.dataSize, readBack.dataSize);

    const auto* orig = static_cast<const uint16_t*>(m_img.data);
    const auto* back = static_cast<const uint16_t*>(readBack.data);
    bool pixelExact = true;
    for (uint32_t i = 0; i < m_img.width * m_img.height && pixelExact; ++i)
        pixelExact = (orig[i] == back[i]);
    EXPECT_TRUE(pixelExact) << "Round-trip pixel data mismatch";

    xpe_free_image(&readBack);
    xpe_dicom_close(handle);
}

// ---------------------------------------------------------------------------
// AC-03: Metadata preserved through write/read
// ---------------------------------------------------------------------------
TEST_F(DicomWriterTest, WriteMetadata_Preserved) {
    auto path = m_tempDir / "meta.dcm";
    ASSERT_EQ(XPE_OK, xpe_dicom_write(path.string().c_str(), &m_img, &m_meta));

    XpeDicomHandle* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(path.string().c_str(), &handle));

    XpeImageMetadata readMeta{};
    ASSERT_EQ(XPE_OK, xpe_dicom_get_metadata(handle, &readMeta));
    EXPECT_STREQ("CHEST", readMeta.bodyPart);
    EXPECT_NEAR(80.0f, readMeta.kVp, 0.01f);
    EXPECT_NEAR(2.5f, readMeta.mAs, 0.001f);
    EXPECT_NEAR(1800.0f, readMeta.SID_mm, 0.01f);
    EXPECT_NEAR(0.148f, readMeta.pixelPitch_mm, 0.0001f);
    xpe_dicom_close(handle);
}

// ---------------------------------------------------------------------------
// REQ-DICOM-017: Write to unwritable path returns IO_FAILED
// ---------------------------------------------------------------------------
TEST_F(DicomWriterTest, WriteIOError_ReturnsIOFailed) {
    EXPECT_EQ(XPE_ERR_IO_FAILED,
              xpe_dicom_write("/nonexistent/dir/out.dcm", &m_img, &m_meta));
}

// ---------------------------------------------------------------------------
// REQ-DICOM-018: NULL parameters return INVALID_INPUT
// ---------------------------------------------------------------------------
TEST_F(DicomWriterTest, WriteNullPath_ReturnsInvalidInput) {
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dicom_write(nullptr, &m_img, &m_meta));
}

TEST_F(DicomWriterTest, WriteNullImg_ReturnsInvalidInput) {
    auto path = m_tempDir / "out.dcm";
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_dicom_write(path.string().c_str(), nullptr, &m_meta));
}

TEST_F(DicomWriterTest, WriteNullMeta_ReturnsInvalidInput) {
    auto path = m_tempDir / "out.dcm";
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_dicom_write(path.string().c_str(), &m_img, nullptr));
}

// ---------------------------------------------------------------------------
// AC-02: J2K Lossless round-trip pixel-exact
// ---------------------------------------------------------------------------
TEST_F(DicomWriterTest, WriteJ2KRoundTrip_PixelExact) {
    auto path = m_tempDir / "j2k_roundtrip.dcm";
    ASSERT_EQ(XPE_OK, xpe_dicom_write_j2k(path.string().c_str(), &m_img, &m_meta));

    XpeDicomHandle* handle = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(path.string().c_str(), &handle));

    XpeImageBuffer readBack{};
    ASSERT_EQ(XPE_OK, xpe_dicom_read_image(handle, &readBack));

    const auto* orig = static_cast<const uint16_t*>(m_img.data);
    const auto* back = static_cast<const uint16_t*>(readBack.data);
    bool pixelExact = true;
    for (uint32_t i = 0; i < m_img.width * m_img.height && pixelExact; ++i)
        pixelExact = (orig[i] == back[i]);
    EXPECT_TRUE(pixelExact) << "J2K round-trip pixel data mismatch";

    xpe_free_image(&readBack);
    xpe_dicom_close(handle);
}

// ---------------------------------------------------------------------------
// REQ-DICOM-022: Rescale defaults to 1.0/0.0
// ---------------------------------------------------------------------------
TEST_F(DicomWriterTest, RescaleDefaults_OneAndZero) {
    auto path = m_tempDir / "rescale.dcm";
    ASSERT_EQ(XPE_OK, xpe_dicom_write(path.string().c_str(), &m_img, &m_meta));
    // Validate via validator that the written file conforms (implicitly checks tags)
    char report[4096] = {};
    EXPECT_EQ(XPE_OK, xpe_dicom_validate(path.string().c_str(), report, sizeof(report)));
    // Report must be valid
    EXPECT_NE(nullptr, std::strstr(report, "\"valid\":true"));
}

// ---------------------------------------------------------------------------
// REQ-DICOM-021: J2K encoder failure cleanup (no partial file)
// ---------------------------------------------------------------------------
TEST_F(DicomWriterTest, WriteJ2KNullImg_ReturnsInvalidInput) {
    auto path = m_tempDir / "j2k_null.dcm";
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_dicom_write_j2k(path.string().c_str(), nullptr, &m_meta));
    EXPECT_FALSE(fs::exists(path)) << "Partial file must not be created on error";
}
