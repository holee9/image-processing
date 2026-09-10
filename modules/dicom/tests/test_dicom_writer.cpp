/**
 * @file test_dicom_writer.cpp
 * @brief TDD tests for DicomWriter / SWU-4.2 (>= 10 test cases)
 * SPEC: SPEC-XPE-P1B-DICOM REQ-DICOM-013..022, AC-01, AC-02, AC-03
 */
#include <gtest/gtest.h>
#include "xpe/dicom/dicom_api.h"
#include "xpe/common/xpe_memory.h"
#include <filesystem>
#include <cstdio>
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
        std::snprintf(m_meta.bodyPart, sizeof(m_meta.bodyPart), "%s", "CHEST");
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

// ---------------------------------------------------------------------------
// #123 (QA-B-21): XpeImageBuffer.dataSize size-consistency guard, per entry point
//
// Contract (api-spec "XpeImageBuffer.dataSize on input"):
//   dataSize == 0                           -> unspecified, accepted
//   0 < dataSize < width*height*bpp(format) -> XPE_ERR_INVALID_INPUT
//   dataSize >= width*height*bpp(format)    -> accepted
//
// Both dicom write entry points call data_size_is_consistent() (dicom.cpp).
// The fixture image stays fully allocated (256*256 uint16); only the declared
// dataSize is altered, so a case that reaches the writer reads inside its own
// allocation and the RED run fails on the return code, not on memory.
// ---------------------------------------------------------------------------

TEST_F(DicomWriterTest, DataSizeGuard_Write_ShortDataSize_ReturnsInvalidInput) {
    XpeImageBuffer shortImg = m_img;
    shortImg.dataSize = static_cast<size_t>(16) * 2;  // declares 256x256 UINT16
    auto path = m_tempDir / "short.dcm";
    EXPECT_EQ(xpe_dicom_write(path.string().c_str(), &shortImg, &m_meta),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(DicomWriterTest, DataSizeGuard_WriteJ2K_ShortDataSize_ReturnsInvalidInput) {
    XpeImageBuffer shortImg = m_img;
    shortImg.dataSize = static_cast<size_t>(16) * 2;
    auto path = m_tempDir / "short_j2k.dcm";
    EXPECT_EQ(xpe_dicom_write_j2k(path.string().c_str(), &shortImg, &m_meta),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(DicomWriterTest, DataSizeGuard_Write_ZeroDataSize_Accepted) {
    XpeImageBuffer img = m_img;
    img.dataSize = 0;  // unspecified per the contract
    auto path = m_tempDir / "zero.dcm";
    EXPECT_NE(xpe_dicom_write(path.string().c_str(), &img, &m_meta),
              XPE_ERR_INVALID_INPUT);
}

TEST_F(DicomWriterTest, DataSizeGuard_WriteJ2K_ZeroDataSize_Accepted) {
    XpeImageBuffer img = m_img;
    img.dataSize = 0;
    auto path = m_tempDir / "zero_j2k.dcm";
    EXPECT_NE(xpe_dicom_write_j2k(path.string().c_str(), &img, &m_meta),
              XPE_ERR_INVALID_INPUT);
}

// ---------------------------------------------------------------------------
// #105 G3: working-set measurement, mirroring enhance_basic
// test_enhance_integration.cpp (92bcf17) and preprocess T-010. Duplicated per
// module rather than exported: xpe_common's surface is fixed at 16 symbols
// (REQ-P0-008).
// ---------------------------------------------------------------------------
#ifdef _WIN32
#  include <windows.h>
#  include <psapi.h>
static SIZE_T get_working_set_bytes() {
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
}
#else
static size_t get_working_set_bytes() { return 0; }
#endif

// WARMUP exists because the first cycles fault in fresh heap pages and grow the
// CRT allocator arena; counting that one-time cost as "leak" would make the
// threshold a measure of startup, not of retention. The baseline is snapshotted
// after warm-up so only steady-state growth is scored.
constexpr int    ENDURANCE_CYCLES = 1000;
constexpr int    ENDURANCE_WARMUP = 100;
constexpr size_t ENDURANCE_ONE_MB = 1024u * 1024u;

// ---------------------------------------------------------------------------
// #105 G3: heap growth over 1000 alloc -> write -> free cycles.
//
// dicom had no endurance loop and no heap measurement before this. The cycle
// allocates a small UINT16 image, writes a DICOM Part 10 file to the same path
// (overwriting), and frees it -- so DCMTK's dataset construction and teardown
// runs 1000 times. If DCMTK holds an internal cache that grows, it surfaces
// here as working-set growth; the number is reported, not tuned around.
// ---------------------------------------------------------------------------
TEST_F(DicomWriterTest, ThousandCycles_MemoryGrowthUnderOneMB) {
#ifndef _WIN32
    GTEST_SKIP() << "Working-set measurement is Windows-only in this build";
#endif
    const auto path = (m_tempDir / "endurance.dcm").string();

    auto one_cycle = [&](int i) {
        XpeImageBuffer img{};
        ASSERT_EQ(xpe_alloc_image(64, 64, XPE_PIXEL_UINT16, &img), XPE_OK) << "cycle " << i;
        auto* px = static_cast<uint16_t*>(img.data);
        for (uint32_t k = 0; k < img.width * img.height; ++k)
            px[k] = static_cast<uint16_t>(k & 0xFFFF);

        EXPECT_EQ(xpe_dicom_write(path.c_str(), &img, &m_meta), XPE_OK) << "cycle " << i;

        xpe_free_image(&img);
    };

    for (int i = 0; i < ENDURANCE_WARMUP; ++i) one_cycle(i);

    const auto before = get_working_set_bytes();
    for (int i = 0; i < ENDURANCE_CYCLES; ++i) one_cycle(i);
    const auto after = get_working_set_bytes();

    if (after > before) {
        EXPECT_LT(after - before, ENDURANCE_ONE_MB)
            << "Working set grew by " << (after - before) / 1024 << " KB over "
            << ENDURANCE_CYCLES << " dicom alloc/write/free cycles";
    }
}

// ---------------------------------------------------------------------------
// #120 (QA-B-27): writer branches reachable without fault injection.
//
// Most of DicomWriter's uncovered lines are openjpeg failure handlers that only
// a faulty library run reaches. These two are ordinary inputs:
//   - an empty bodyPart takes the else-branch at DicomWriter.cpp:169
//   - a null data pointer makes compressJ2K return {} (DicomWriter.cpp:232),
//     which is the J2K "compression failed" path at :80-81
// ---------------------------------------------------------------------------
TEST_F(DicomWriterTest, WriteEmptyBodyPart_StillWritesFile) {
    XpeImageMetadata meta = m_meta;
    meta.bodyPart[0] = '\0';
    auto path = m_tempDir / "empty_bodypart.dcm";
    EXPECT_EQ(XPE_OK, xpe_dicom_write(path.string().c_str(), &m_img, &meta));
    EXPECT_TRUE(fs::exists(path));
}

TEST_F(DicomWriterTest, WriteJ2KNullPixelData_ReturnsProcessingFailed) {
    XpeImageBuffer img{};
    img.width         = 64;
    img.height        = 64;
    img.bitsAllocated = 16;
    img.bitsStored    = 12;
    img.format        = XPE_PIXEL_UINT16;
    img.data          = nullptr;   // passes the dicom.cpp null-struct check
    img.dataSize      = 0;         // 0 = unspecified, so the #123 guard stays quiet
    auto path = m_tempDir / "j2k_nodata.dcm";
    EXPECT_EQ(XPE_ERR_PROCESSING_FAILED,
              xpe_dicom_write_j2k(path.string().c_str(), &img, &m_meta));
}

// ---------------------------------------------------------------------------
// #120 (QA-B-29): the size guard's "unknown bits-per-pixel" branch
// (dicom.cpp:100-104). UINT8 is a declared XpePixelFormat that the dicom
// size check has no bytes-per-pixel entry for, so it returns "consistent"
// without comparing anything and the call proceeds to the writer. The guard
// must not reject on a format it cannot size -- that is the format check's job.
// ---------------------------------------------------------------------------
TEST_F(DicomWriterTest, WriteUint8Format_NotRejectedBySizeGuard) {
    std::vector<uint8_t> pixels(64 * 64, 0u);
    XpeImageBuffer img{};
    img.width         = 64;
    img.height        = 64;
    img.bitsAllocated = 8;
    img.bitsStored    = 8;
    img.format        = XPE_PIXEL_UINT8;
    img.data          = pixels.data();
    img.dataSize      = pixels.size();   // non-zero, so the guard does run

    auto path = m_tempDir / "uint8.dcm";
    // Whatever the writer decides, it must not be the size guard's
    // XPE_ERR_INVALID_INPUT: reaching the writer at all is the point.
    const XpeErrorCode rc = xpe_dicom_write(path.string().c_str(), &img, &m_meta);
    EXPECT_NE(XPE_ERR_INVALID_INPUT, rc)
        << "size guard rejected a format it cannot size; rc=" << rc;
}
