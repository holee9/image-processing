/**
 * @file test_dicom_writer_inner.cpp
 * @brief The inner writers (DicomWriter::write / writeJ2K) called directly, without the public door (QA-B-206 M1c, Codex #110).
 *
 * The public functions xpe_dicom_write / xpe_dicom_write_j2k refuse a descriptor that contradicts the 16-bit words they
 * write (QA-B-206 M1b). The inner writers carry the same predicates, so that a future caller that reaches them directly
 * cannot write BitsAllocated 8 over 16-bit pixel data, a FLOAT32 buffer as words, or an image no file can describe.
 */
#include <gtest/gtest.h>
#include "test_pid.h"

#include "DicomImageLimits.h"
#include "DicomWriter.h"
#include "xpe/common/xpe_error.h"
#include "xpe/common/xpe_types.h"

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using xpe::dicom::DicomWriter;

namespace {

struct Fixture {
    fs::path dir = fs::temp_directory_path() / ("xpe_dicom_writer_inner_test" + xpe_test::pid_suffix());
    std::vector<uint16_t> px = std::vector<uint16_t>(64, 0x0102u);
    XpeImageBuffer img{};
    XpeImageMetadata meta{};
    Fixture() {
        fs::create_directories(dir);
        img.width = 8;
        img.height = 8;
        img.format = XPE_PIXEL_UINT16;
        img.bitsAllocated = 16;
        img.bitsStored = 16;
        img.data = px.data();
        img.dataSize = px.size() * 2;
        std::snprintf(meta.bodyPart, sizeof(meta.bodyPart), "%s", "CHEST");
    }
    ~Fixture() {
        std::error_code ec;
        fs::remove_all(dir, ec);
    }
};

}  // namespace

TEST(DicomWriterInner, TheControlImageIsWrittenByBothInnerWriters) {
    Fixture f;
    EXPECT_EQ(XPE_OK, DicomWriter::write((f.dir / "ok.dcm").string().c_str(), &f.img, &f.meta));
    EXPECT_EQ(XPE_OK, DicomWriter::writeJ2K((f.dir / "ok_j2k.dcm").string().c_str(), &f.img, &f.meta));
    EXPECT_TRUE(fs::exists(f.dir / "ok.dcm"));
    EXPECT_TRUE(fs::exists(f.dir / "ok_j2k.dcm"));
}

TEST(DicomWriterInner, ADescriptorThatContradictsTheSixteenBitWordsIsRefusedWithoutTheDoor) {
    struct Case {
        uint32_t allocated, stored;
        const char* why;
    } bad[] = {{8u, 8u, "Codex #108: BitsAllocated 8 over 16-bit words"}, {0u, 16u, "BitsAllocated 0"},
               {32u, 16u, "BitsAllocated 32"}, {16u, 0u, "BitsStored 0"}, {16u, 17u, "BitsStored 17"}};
    int n = 0;
    for (const Case& c : bad) {
        Fixture f;
        f.img.bitsAllocated = c.allocated;
        f.img.bitsStored = c.stored;
        const fs::path a = f.dir / ("a" + std::to_string(n) + ".dcm");
        const fs::path b = f.dir / ("b" + std::to_string(n) + ".dcm");
        ++n;
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, DicomWriter::write(a.string().c_str(), &f.img, &f.meta)) << c.why;
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, DicomWriter::writeJ2K(b.string().c_str(), &f.img, &f.meta)) << c.why;
        EXPECT_FALSE(fs::exists(a)) << c.why;
        EXPECT_FALSE(fs::exists(b)) << c.why;
    }
}

TEST(DicomWriterInner, APixelFormatOtherThanUint16IsRefusedWithoutTheDoor) {
    for (const XpePixelFormat format : {XPE_PIXEL_UINT8, XPE_PIXEL_FLOAT32}) {
        Fixture f;
        f.img.format = format;
        const fs::path a = f.dir / "fmt_a.dcm";
        const fs::path b = f.dir / "fmt_b.dcm";
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, DicomWriter::write(a.string().c_str(), &f.img, &f.meta)) << format;
        EXPECT_EQ(XPE_ERR_INVALID_INPUT, DicomWriter::writeJ2K(b.string().c_str(), &f.img, &f.meta)) << format;
        EXPECT_FALSE(fs::exists(a));
        EXPECT_FALSE(fs::exists(b));
    }
}

TEST(DicomWriterInner, AnImageNoFileCanDescribeIsRefusedWithoutTheDoorAndTheTinyBufferIsNotRead) {
    Fixture f;
    f.img.width = 65535u;
    f.img.height = 32769u;   // 4,295,032,830 bytes; the buffer holds 128
    f.img.dataSize = 0;
    const fs::path a = f.dir / "big_a.dcm";
    const fs::path b = f.dir / "big_b.dcm";
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, DicomWriter::write(a.string().c_str(), &f.img, &f.meta));
    EXPECT_EQ(XPE_ERR_INVALID_INPUT, DicomWriter::writeJ2K(b.string().c_str(), &f.img, &f.meta));
    EXPECT_FALSE(fs::exists(a));
    EXPECT_FALSE(fs::exists(b));
}
