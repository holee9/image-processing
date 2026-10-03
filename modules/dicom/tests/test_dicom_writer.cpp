/**
 * @file test_dicom_writer.cpp
 * @brief TDD tests for DicomWriter / SWU-4.2 (>= 10 test cases)
 * SPEC: SPEC-XPE-P1B-DICOM REQ-DICOM-013..022, AC-01, AC-02, AC-03
 */
#include <gtest/gtest.h>
#include "xpe/dicom/dicom_api.h"
#include "xpe/common/xpe_memory.h"
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>
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
// #105 G3 / #181: heap-growth measurement (heap_growth.h)
// test_enhance_integration.cpp (92bcf17) and preprocess T-010. Duplicated per
// module rather than exported: xpe_common's surface is fixed at 16 symbols
// (REQ-P0-008).
// ---------------------------------------------------------------------------
#include "heap_growth.h"

// #181 (QA-B-93): retention is measured on the CRT heap (heap_growth.h), not
// on the working set, which QA-B-92 showed does not follow a leak. Warm-up
// (heap_growth::kWarmup) lets first-touch pages and allocator arenas settle
// before the baseline.

// ---------------------------------------------------------------------------
// #105 G3: heap growth over 1000 alloc -> write -> free cycles.
//
// dicom had no endurance loop and no heap measurement before this. The cycle
// allocates a small UINT16 image, writes a DICOM Part 10 file to the same path
// (overwriting), and frees it -- so DCMTK's dataset construction and teardown
// runs 1000 times. If DCMTK holds an internal cache that grows, it surfaces
// here as CRT heap growth; the number is reported, not tuned around.
// ---------------------------------------------------------------------------
TEST_F(DicomWriterTest, ThousandCycles_CrtHeapDoesNotGrow) {
#ifndef _WIN32
    GTEST_SKIP() << "CRT heap walk is Windows-only in this build";
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

    const heap_growth::Growth g = heap_growth::Measure(one_cycle);
    GTEST_LOG_(INFO) << heap_growth::Describe(g);
    EXPECT_LT(g.heap.blocks, heap_growth::MaxBlocks(g.cycles))
        << g.cycles << " dicom alloc/write/free cycles left blocks allocated";
    EXPECT_LT(g.heap.bytes, heap_growth::kMaxBytes)
        << g.cycles << " dicom alloc/write/free cycles left bytes allocated";
}

// #181 (QA-B-93) control: the same cycle plus one unfreed 64-byte block per
// cycle. The heap measurement above must see it; otherwise its zero means nothing.
TEST_F(DicomWriterTest, ThousandCycles_ControlLeakIsCaught) {
#ifndef _WIN32
    GTEST_SKIP() << "CRT heap walk is Windows-only in this build";
#endif
    const auto path = (m_tempDir / "endurance_control.dcm").string();
    auto one_cycle = [&](int i) {
        XpeImageBuffer img{};
        ASSERT_EQ(xpe_alloc_image(64, 64, XPE_PIXEL_UINT16, &img), XPE_OK) << "cycle " << i;
        EXPECT_EQ(xpe_dicom_write(path.c_str(), &img, &m_meta), XPE_OK) << "cycle " << i;
        xpe_free_image(&img);
    };
    const heap_growth::Growth g = heap_growth::Measure(one_cycle, 64);
    GTEST_LOG_(INFO) << "control 64 B/cycle: " << heap_growth::Describe(g);
    EXPECT_GE(g.heap.blocks, g.cycles * 9 / 10);
    EXPECT_GE(g.heap.bytes, 64LL * g.cycles * 9 / 10);
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

// Renamed and re-asserted by QA-B-41. It used to expect
// XPE_ERR_PROCESSING_FAILED, which was the old behaviour: the NULL data pointer
// passed every guard and surfaced from inside the J2K compressor as a generic
// processing failure. #142 decided that a NULL data pointer is INVALID_INPUT
// across every post module, so the answer now names the actual problem and is
// the same one xpe_dicom_write gives for the same input.
//
// The old expectation is superseded, not wrong-then: it recorded what the code
// did before the contract existed.
TEST_F(DicomWriterTest, WriteJ2KNullPixelData_ReturnsInvalidInput) {
    XpeImageBuffer img{};
    img.width         = 64;
    img.height        = 64;
    img.bitsAllocated = 16;
    img.bitsStored    = 12;
    img.format        = XPE_PIXEL_UINT16;
    img.data          = nullptr;   // passes the dicom.cpp null-struct check
    img.dataSize      = 0;         // 0 = unspecified, so the #123 guard stays quiet
    auto path = m_tempDir / "j2k_nodata.dcm";
    EXPECT_EQ(XPE_ERR_INVALID_INPUT,
              xpe_dicom_write_j2k(path.string().c_str(), &img, &m_meta));
}

// ---------------------------------------------------------------------------
// #120 (QA-B-29) asked what the size guard does with UINT8, a declared XpePixelFormat it has no bytes-per-pixel entry for,
// and answered "it must not reject on a format it cannot size -- that is the format check's job". There was no format
// check then: the call went on to the writer, and a UINT8 buffer was written with its bytes read as 16-bit words.
// QA-B-201 M4 added the format check, so a UINT8 image is now refused at the door as INVALID_INPUT, before any file is
// made; the size guard sees only UINT16.
// ---------------------------------------------------------------------------
TEST_F(DicomWriterTest, WriteUint8Format_IsRefusedAtTheDoorByTheFormatCheck) {
    // two bytes per pixel in the buffer, so that the size guard (width * height * 2) is satisfied and ONLY the format
    // check can refuse this image
    std::vector<uint8_t> pixels(64 * 64 * 2, 0u);
    XpeImageBuffer img{};
    img.width         = 64;
    img.height        = 64;
    img.bitsAllocated = 8;
    img.bitsStored    = 8;
    img.format        = XPE_PIXEL_UINT8;
    img.data          = pixels.data();
    img.dataSize      = pixels.size();

    auto path = m_tempDir / "uint8.dcm";
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dicom_write(path.string().c_str(), &img, &m_meta));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dicom_write_j2k(path.string().c_str(), &img, &m_meta));
    EXPECT_FALSE(std::filesystem::exists(path)) << "refused before any file is made";
}

// ---------------------------------------------------------------------------
// QA-B-201 M4: the writers take UINT16 only. A FLOAT32 image used to be written as a 32-bit file (BitsAllocated 32) that
// this module's own reader refuses, with an XPE_OK.
// ---------------------------------------------------------------------------
namespace {

std::vector<float> RampWithNegatives(uint32_t w, uint32_t h) {
    std::vector<float> px(static_cast<size_t>(w) * h);
    for (size_t i = 0; i < px.size(); ++i) px[i] = -500.0f + 4500.0f * static_cast<float>(i) / static_cast<float>(px.size() - 1);
    return px;
}

std::string FileBytes(const std::filesystem::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

}  // namespace

TEST_F(DicomWriterTest, AFloat32ImageIsRefusedByBothWritersAndNoFileIsMade) {
    std::vector<float> px = RampWithNegatives(64, 64);
    XpeImageBuffer img{};
    img.width = 64;
    img.height = 64;
    img.bitsAllocated = 32;
    img.bitsStored = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.data = px.data();
    img.dataSize = px.size() * sizeof(float);

    const auto plain = m_tempDir / "float.dcm";
    const auto j2k = m_tempDir / "float_j2k.dcm";
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dicom_write(plain.string().c_str(), &img, &m_meta)) << "was XPE_OK";
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dicom_write_j2k(j2k.string().c_str(), &img, &m_meta));
    EXPECT_FALSE(std::filesystem::exists(plain)) << "refused before any file is made";
    EXPECT_FALSE(std::filesystem::exists(j2k));
}

TEST_F(DicomWriterTest, ARefusedFormatLeavesAnExistingDestinationFileUntouched) {
    const auto path = m_tempDir / "keep.dcm";
    {
        std::ofstream f(path, std::ios::binary);
        f << "KEEP-THESE-BYTES";
    }
    std::vector<float> px = RampWithNegatives(8, 8);
    XpeImageBuffer img{};
    img.width = 8;
    img.height = 8;
    img.bitsAllocated = 32;
    img.bitsStored = 32;
    img.format = XPE_PIXEL_FLOAT32;
    img.data = px.data();
    img.dataSize = px.size() * sizeof(float);
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dicom_write(path.string().c_str(), &img, &m_meta));
    EXPECT_EQ("KEEP-THESE-BYTES", FileBytes(path)) << "the refusal comes before the destination is opened";
}

TEST_F(DicomWriterTest, EveryImageTheWriterAcceptsIsAnImageTheReaderReads) {
    // The coupling the defect broke: the writer said OK for a file the reader refused. For each pixel format, a write that
    // succeeds must produce a file that opens and reads back. The UINT16 row is the control: it succeeds and reads.
    struct Row {
        XpePixelFormat format;
        uint32_t bits;
        size_t bytesPerPixel;   // bytes per pixel in the BUFFER (not necessarily the format's own size)
        const char* name;
    } rows[] = {{XPE_PIXEL_UINT16, 16, 2, "UINT16"}, {XPE_PIXEL_FLOAT32, 32, 4, "FLOAT32"}, {XPE_PIXEL_UINT8, 8, 2, "UINT8"}};   // UINT8 with two bytes per pixel in the buffer: the size guard passes
    for (const Row& r : rows) {
        std::vector<uint8_t> bytes(static_cast<size_t>(32) * 32 * r.bytesPerPixel, 0x11u);
        XpeImageBuffer img{};
        img.width = 32;
        img.height = 32;
        img.bitsAllocated = r.bits;
        img.bitsStored = r.bits;
        img.format = r.format;
        img.data = bytes.data();
        img.dataSize = bytes.size();
        const auto path = m_tempDir / (std::string("fmt_") + r.name + ".dcm");
        const XpeErrorCode wrote = xpe_dicom_write(path.string().c_str(), &img, &m_meta);
        if (r.format == XPE_PIXEL_UINT16) ASSERT_EQ(XPE_OK, wrote) << "control: the supported format is written";
        if (wrote != XPE_OK) continue;
        XpeDicomHandle* h = nullptr;
        ASSERT_EQ(XPE_OK, xpe_dicom_open(path.string().c_str(), &h)) << r.name;
        XpeImageBuffer back{};
        const XpeErrorCode read = xpe_dicom_read_image(h, &back);
        EXPECT_EQ(XPE_OK, read) << r.name << ": the writer said OK, so the reader must read the file";
        if (read == XPE_OK) xpe_free_image(&back);
        xpe_dicom_close(h);
    }
}

// ---------------------------------------------------------------------------
// #142 (QA-B-41): the empty-image contract QA-B-40 set for enhance_basic and
// QA-B-41 carried to display now applies to the writer entry points too.
// width == 0, height == 0, or a NULL data pointer is XPE_ERR_INVALID_INPUT.
//
// Before this, xpe_dicom_write accepted a zero-sized image and produced a file
// with no pixels, while xpe_dicom_write_j2k reported XPE_ERR_PROCESSING_FAILED
// from deep inside the compressor -- two different answers to one bad input,
// neither of them naming the actual problem.
// ---------------------------------------------------------------------------
namespace {

struct NamedWriter {
    const char* name;
    XpeErrorCode (*fn)(const char*, const XpeImageBuffer*, const XpeImageMetadata*);
};

const NamedWriter kWriters[] = {
    {"xpe_dicom_write",     xpe_dicom_write},
    {"xpe_dicom_write_j2k", xpe_dicom_write_j2k},
};

}  // namespace

TEST_F(DicomWriterTest, EmptyImage_ZeroWidth_ReturnsInvalidInput) {
    for (const auto& w : kWriters) {
        XpeImageBuffer img = m_img;
        img.width    = 0;
        img.dataSize = 0;   // 0 means unspecified (#123), so it is not what fails
        const auto path = m_tempDir / "empty_w.dcm";
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, w.fn(path.string().c_str(), &img, &m_meta))
            << w.name << " accepted width == 0";
    }
}

TEST_F(DicomWriterTest, EmptyImage_ZeroHeight_ReturnsInvalidInput) {
    for (const auto& w : kWriters) {
        XpeImageBuffer img = m_img;
        img.height   = 0;
        img.dataSize = 0;
        const auto path = m_tempDir / "empty_h.dcm";
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, w.fn(path.string().c_str(), &img, &m_meta))
            << w.name << " accepted height == 0";
    }
}

TEST_F(DicomWriterTest, EmptyImage_NullData_ReturnsInvalidInput) {
    for (const auto& w : kWriters) {
        XpeImageBuffer img = m_img;
        img.data = nullptr;   // the fixture still owns m_img.data
        const auto path = m_tempDir / "empty_null.dcm";
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, w.fn(path.string().c_str(), &img, &m_meta))
            << w.name << " accepted a NULL data pointer";
    }
}

// The complement: without it a guard that rejected everything would satisfy
// the three cases above.
TEST_F(DicomWriterTest, EmptyImageContract_ValidImageStillAccepted) {
    for (const auto& w : kWriters) {
        const auto path = m_tempDir / "contract_valid.dcm";
        EXPECT_EQ(XPE_OK, w.fn(path.string().c_str(), &m_img, &m_meta))
            << w.name << " rejected a well-formed image";
    }
}

// ---------------------------------------------------------------------------
// QA-B-206 C13: the PixelData length is what the dimensions say (api-spec "XpeImageBuffer.dataSize on input", #123).
//
// dataSize == 0 is "unspecified": the writer trusts width * height * bytes-per-pixel. It used to take the PixelData length
// from dataSize itself, so dataSize == 0 produced a file with a zero-length PixelData and the call said XPE_OK (QA-B-204:
// xpe_dicom_read_image then refused it with XPE_ERR_DICOM_INVALID), and a larger dataSize wrote the caller's surplus bytes
// into PixelData. A dataSize SMALLER than the image is still refused (the DataSizeGuard tests above).
// ---------------------------------------------------------------------------

namespace {

/** The value length of the (7FE0,0010) element of an Explicit VR Little Endian file; -1 when it cannot be found. */
int64_t PixelDataLength(const std::filesystem::path& p) {
    const std::string bytes = FileBytes(p);
    const char tag[4] = {static_cast<char>(0xE0), static_cast<char>(0x7F), static_cast<char>(0x10), static_cast<char>(0x00)};
    const size_t at = bytes.find(std::string(tag, 4));
    if (at == std::string::npos || at + 12 > bytes.size()) return -1;
    if (bytes[at + 4] != 'O' || (bytes[at + 5] != 'W' && bytes[at + 5] != 'B')) return -1;
    uint32_t len = 0;
    std::memcpy(&len, bytes.data() + at + 8, sizeof(len));
    return static_cast<int64_t>(len);
}

}  // namespace

TEST_F(DicomWriterTest, ZeroDataSizeWritesTheWholeImageAndTheFileReadsBackExactly) {
    XpeImageBuffer img = m_img;
    img.dataSize = 0;
    const auto path = m_tempDir / "zero_full.dcm";
    ASSERT_EQ(XPE_OK, xpe_dicom_write(path.string().c_str(), &img, &m_meta));
    const int64_t expected = static_cast<int64_t>(m_img.width) * m_img.height * 2;
    EXPECT_EQ(expected, PixelDataLength(path)) << "was 0: a file with no pixels, reported as success";

    XpeDicomHandle* h = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(path.string().c_str(), &h));
    XpeImageBuffer back{};
    ASSERT_EQ(XPE_OK, xpe_dicom_read_image(h, &back)) << "was XPE_ERR_DICOM_INVALID";
    EXPECT_EQ(static_cast<size_t>(expected), back.dataSize);
    EXPECT_EQ(0, std::memcmp(back.data, m_img.data, static_cast<size_t>(expected)));
    xpe_free_image(&back);
    xpe_dicom_close(h);
}

TEST_F(DicomWriterTest, ZeroDataSizeWritesTheWholeImageForTheJ2kWriterToo) {
    XpeImageBuffer img = m_img;
    img.dataSize = 0;
    const auto path = m_tempDir / "zero_full_j2k.dcm";
    ASSERT_EQ(XPE_OK, xpe_dicom_write_j2k(path.string().c_str(), &img, &m_meta));
    XpeDicomHandle* h = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(path.string().c_str(), &h));
    XpeImageBuffer back{};
    ASSERT_EQ(XPE_OK, xpe_dicom_read_image(h, &back));
    EXPECT_EQ(0, std::memcmp(back.data, m_img.data, static_cast<size_t>(m_img.width) * m_img.height * 2));
    xpe_free_image(&back);
    xpe_dicom_close(h);
}

TEST_F(DicomWriterTest, ASurplusBeyondTheImageInALargerBufferIsNotWrittenIntoPixelData) {
    const size_t imageBytes = static_cast<size_t>(m_img.width) * m_img.height * 2;
    std::vector<uint8_t> big(imageBytes + 64, 0xEEu);   // 64 bytes of surplus the file must not contain
    std::memcpy(big.data(), m_img.data, imageBytes);
    XpeImageBuffer img = m_img;
    img.data = big.data();
    img.dataSize = big.size();
    const auto path = m_tempDir / "surplus.dcm";
    ASSERT_EQ(XPE_OK, xpe_dicom_write(path.string().c_str(), &img, &m_meta));
    EXPECT_EQ(static_cast<int64_t>(imageBytes), PixelDataLength(path)) << "was imageBytes + 64: the surplus was written";

    XpeDicomHandle* h = nullptr;
    ASSERT_EQ(XPE_OK, xpe_dicom_open(path.string().c_str(), &h));
    XpeImageBuffer back{};
    ASSERT_EQ(XPE_OK, xpe_dicom_read_image(h, &back));
    EXPECT_EQ(imageBytes, back.dataSize);
    EXPECT_EQ(0, std::memcmp(back.data, m_img.data, imageBytes));
    xpe_free_image(&back);
    xpe_dicom_close(h);
}

TEST_F(DicomWriterTest, ADataSizeOfOneByteLessThanTheImageIsStillRefusedAndNoFileIsMade) {
    const size_t imageBytes = static_cast<size_t>(m_img.width) * m_img.height * 2;
    for (const size_t declared : {static_cast<size_t>(1), imageBytes - 1}) {
        XpeImageBuffer img = m_img;
        img.dataSize = declared;
        const auto path = m_tempDir / ("short_" + std::to_string(declared) + ".dcm");
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dicom_write(path.string().c_str(), &img, &m_meta)) << declared;
        EXPECT_FALSE(std::filesystem::exists(path)) << declared << ": the refusal comes before any file exists";
    }
}

// ---------------------------------------------------------------------------
// QA-B-206 M1b (Codex #108): what a file can describe is decided at the door of both public writers.
//
//  1. The PixelData length is width * height * 2 computed in 64 bits. The first version of the C13 fix multiplied
//     `unsigned long`s (32 bits on Windows): 65535 x 32769 x 2 = 4,295,032,830 wrapped to 65,534. An image the file cannot
//     describe -- Rows/Columns above 65535 (16-bit attributes) or PixelData above 0xFFFFFFFE bytes -- is refused with
//     XPE_ERR_INVALID_INPUT before any file exists. The rejection cases pass a TINY buffer with dataSize 0 (unspecified):
//     if the refusal were not first, the writer would read far past it.
//  2. The descriptor has to agree with the 16-bit words the writer emits: BitsAllocated 16, BitsStored 1..16. 0 is refused
//     (nothing promises it means "default"); bitsAllocated 8 over 16-bit pixels used to be written as a file that
//     contradicts its own PixelData and that this module's reader refuses.
// ---------------------------------------------------------------------------

#include "DicomImageLimits.h"

namespace {

XpeImageBuffer TinyBufferClaiming(uint32_t width, uint32_t height, std::vector<uint16_t>* storage) {
    storage->assign(16, 0x0123u);
    XpeImageBuffer img{};
    img.width = width;
    img.height = height;
    img.format = XPE_PIXEL_UINT16;
    img.bitsAllocated = 16;
    img.bitsStored = 16;
    img.data = storage->data();
    img.dataSize = 0;   // unspecified: the entry point trusts the dimensions -- which is why the size must be checked first
    return img;
}

}  // namespace

TEST(DicomImageLimits, TheLengthIsComputedIn64BitsAndTheBoundaryIsWhereTheElementEnds) {
    using xpe::dicom::image_size_is_representable;
    using xpe::dicom::pixel_data_bytes;
    EXPECT_EQ(4295032830ull, pixel_data_bytes(65535u, 32769u)) << "the case of Codex #108: 32-bit arithmetic gave 65,534";
    EXPECT_EQ(65534u, static_cast<uint32_t>(pixel_data_bytes(65535u, 32769u))) << "control: this is what the wrap looks like";

    EXPECT_TRUE(image_size_is_representable(1u, 1u));
    EXPECT_TRUE(image_size_is_representable(46340u, 46340u)) << "2,144,... pixels x 2 = 4,294,791,200 bytes: just below the limit";
    EXPECT_FALSE(image_size_is_representable(46341u, 46341u)) << "x 2 = 4,294,... > 0xFFFFFFFE: just above";
    EXPECT_TRUE(image_size_is_representable(65535u, 32768u)) << "4,294,901,760 bytes";
    EXPECT_FALSE(image_size_is_representable(65535u, 32769u)) << "4,295,032,830 bytes";
    EXPECT_FALSE(image_size_is_representable(65536u, 1u)) << "Columns is a 16-bit attribute";
    EXPECT_FALSE(image_size_is_representable(1u, 65536u)) << "Rows is a 16-bit attribute";
    EXPECT_FALSE(image_size_is_representable(0xFFFFFFFFu, 0xFFFFFFFFu)) << "the product of two full 32-bit values";
    EXPECT_FALSE(image_size_is_representable(0u, 5u));
    EXPECT_FALSE(image_size_is_representable(5u, 0u));
    EXPECT_TRUE(xpe::dicom::kMaxPixelDataBytes % 2u == 0u) << "an OB/OW length is even";
}

TEST_F(DicomWriterTest, AnImageTheFileCannotDescribeIsRefusedBeforeAnyFileExistsByBothWriters) {
    struct Size {
        uint32_t w, h;
        const char* why;
    } sizes[] = {{65535u, 32769u, "Codex #108: width*height*2 = 4,295,032,830 wrapped to 65,534"},
                 {46341u, 46341u, "just above the PixelData limit"},
                 {65536u, 1u, "Columns above 65535"},
                 {1u, 65536u, "Rows above 65535"},
                 {0x7FFFFFFFu, 2u, "a dimension the module's other entry points accept as int"}};
    int n = 0;
    for (const Size& s : sizes) {
        std::vector<uint16_t> storage;
        const XpeImageBuffer img = TinyBufferClaiming(s.w, s.h, &storage);
        const auto a = m_tempDir / ("huge_a" + std::to_string(n) + ".dcm");
        const auto b = m_tempDir / ("huge_b" + std::to_string(n) + ".dcm");
        ++n;
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dicom_write(a.string().c_str(), &img, &m_meta)) << s.why;
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dicom_write_j2k(b.string().c_str(), &img, &m_meta)) << s.why;
        EXPECT_FALSE(std::filesystem::exists(a)) << s.why;
        EXPECT_FALSE(std::filesystem::exists(b)) << s.why;
    }
}

TEST_F(DicomWriterTest, ADescriptorThatContradictsTheSixteenBitWordsIsRefusedByBothWritersAndNoFileIsMade) {
    struct Bits {
        uint32_t allocated, stored;
        const char* why;
    } bad[] = {{8u, 8u, "Codex #108: BitsAllocated 8 over 16-bit words"},
               {8u, 16u, "BitsAllocated 8, BitsStored 16"},
               {32u, 16u, "BitsAllocated 32 over 16-bit words"},
               {0u, 16u, "BitsAllocated 0: nothing promises it means default"},
               {16u, 0u, "BitsStored 0"},
               {16u, 17u, "BitsStored above BitsAllocated"},
               {16u, 32u, "BitsStored 32"}};
    int n = 0;
    for (const Bits& c : bad) {
        XpeImageBuffer img = m_img;
        img.bitsAllocated = c.allocated;
        img.bitsStored = c.stored;
        const auto a = m_tempDir / ("bits_a" + std::to_string(n) + ".dcm");
        const auto b = m_tempDir / ("bits_b" + std::to_string(n) + ".dcm");
        ++n;
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dicom_write(a.string().c_str(), &img, &m_meta)) << c.why;
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, xpe_dicom_write_j2k(b.string().c_str(), &img, &m_meta)) << c.why;
        EXPECT_FALSE(std::filesystem::exists(a)) << c.why;
        EXPECT_FALSE(std::filesystem::exists(b)) << c.why;
    }
}

TEST_F(DicomWriterTest, EveryBitsStoredFromOneToSixteenIsWrittenAndReadsBack) {
    // The control for the refusals above: the whole accepted range (BitsAllocated 16, BitsStored 1..16) still writes and the
    // module's own reader reads the file. Pixel values stay below 2^BitsStored so the J2K precision and the check agree.
    for (uint32_t stored = 1; stored <= 16; ++stored) {
        XpeImageBuffer img = m_img;
        img.bitsAllocated = 16;
        img.bitsStored = stored;
        const uint16_t mask = static_cast<uint16_t>(stored == 16 ? 0xFFFFu : ((1u << stored) - 1u));
        std::vector<uint16_t> px(static_cast<size_t>(img.width) * img.height);
        for (size_t i = 0; i < px.size(); ++i) px[i] = static_cast<uint16_t>(i & mask);
        img.data = px.data();
        const auto path = m_tempDir / ("bits_ok_" + std::to_string(stored) + ".dcm");
        ASSERT_EQ(XPE_OK, xpe_dicom_write(path.string().c_str(), &img, &m_meta)) << "BitsStored " << stored;
        XpeDicomHandle* h = nullptr;
        ASSERT_EQ(XPE_OK, xpe_dicom_open(path.string().c_str(), &h)) << stored;
        XpeImageBuffer back{};
        ASSERT_EQ(XPE_OK, xpe_dicom_read_image(h, &back)) << stored;
        EXPECT_EQ(0, std::memcmp(back.data, px.data(), px.size() * 2)) << "BitsStored " << stored;
        xpe_free_image(&back);
        xpe_dicom_close(h);
    }
}

// ---------------------------------------------------------------------------
// QA-B-206 C14: xpe_dicom_open frees what it allocated whether or not it succeeds.
//
// The handle used to be a raw `new` outside the reach of the catch, so an exception from reader.open() leaked it. No input
// makes open() throw in production (DCMTK reports failures as an OFCondition), so this test measures the CRT heap over open /
// close cycles and the leak itself was proven with a throw injected into DicomReader::open (see the QA-B-206 report): with the
// raw pointer the same loop grows by one handle per cycle, with the unique_ptr it does not.
// ---------------------------------------------------------------------------
TEST_F(DicomWriterTest, ThousandOpenCycles_CrtHeapDoesNotGrowWhetherOrNotOpenSucceeds) {
#ifndef _WIN32
    GTEST_SKIP() << "CRT heap walk is Windows-only in this build";
#endif
    const auto good = m_tempDir / "open_cycles.dcm";
    ASSERT_EQ(XPE_OK, xpe_dicom_write(good.string().c_str(), &m_img, &m_meta));
    const auto missing = (m_tempDir / "does_not_exist.dcm").string();

    auto one_cycle = [&](int i) {
        XpeDicomHandle* h = nullptr;
        if (xpe_dicom_open(good.string().c_str(), &h) == XPE_OK) xpe_dicom_close(h);   // success path
        XpeDicomHandle* none = nullptr;
        EXPECT_NE(XPE_OK, xpe_dicom_open(missing.c_str(), &none)) << "cycle " << i;     // failure path
        EXPECT_EQ(nullptr, none);
    };
    const heap_growth::Growth g = heap_growth::Measure(one_cycle);
    GTEST_LOG_(INFO) << heap_growth::Describe(g);
    EXPECT_LT(g.heap.blocks, heap_growth::MaxBlocks(g.cycles)) << g.cycles << " open cycles left blocks allocated";
    EXPECT_LT(g.heap.bytes, heap_growth::kMaxBytes) << g.cycles << " open cycles left bytes allocated";
}
