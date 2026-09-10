/**
 * @file DicomReader.h
 * @brief Internal C++ class for DICOM Part 10 file reading (SWU-4.1).
 *
 * Wraps DCMTK DcmFileFormat to implement the handle-based reader API.
 * Not part of the public DLL ABI — internal use only.
 *
 * SPEC: SPEC-XPE-P1B-DICOM REQ-DICOM-001..012
 */
#pragma once

#include <string>
#include <memory>
#include <cstdint>
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

// Forward-declare DCMTK types to avoid pulling heavy headers into all TUs
class DcmFileFormat;

namespace xpe {
namespace dicom {

/**
 * @brief Transfer syntaxes this module accepts for reading (REQ-DICOM-004).
 *
 * #146 (QA-B-45): this list used to live as three file-static constants inside
 * DicomReader.cpp, where nothing connected it to the module's actual decode
 * capability. JPEG Lossless sat in the list for months while no DCMTK codec was
 * ever registered, so open() accepted files readImage could not decode.
 *
 * It is a single table now, and test_dicom_reader.cpp iterates it: every entry
 * must have a fixture that round-trips. Adding a syntax here without teaching
 * the reader to decode it — or the test to build a file in it — fails the
 * build's test run rather than shipping another promise the code cannot keep.
 */
struct SupportedTransferSyntax {
    const char* uid;
    const char* name;
};

inline constexpr SupportedTransferSyntax kSupportedTransferSyntaxes[] = {
    { "1.2.840.10008.1.2.1",     "Explicit VR Little Endian" },
    { "1.2.840.10008.1.2.4.90",  "JPEG 2000 Lossless Only" },
    { "1.2.840.10008.1.2.4.70",  "JPEG Lossless, Non-Hierarchical, First-Order" },
};

inline constexpr size_t kSupportedTransferSyntaxCount =
    sizeof(kSupportedTransferSyntaxes) / sizeof(kSupportedTransferSyntaxes[0]);

// @MX:ANCHOR: [AUTO] Public API boundary — maps to XpeDicomHandle opaque pointer
// @MX:REASON: fan_in >= 3: xpe_dicom_open, xpe_dicom_read_image, xpe_dicom_get_metadata, xpe_dicom_close
// @MX:SPEC: SPEC-XPE-P1B-DICOM SWU-4.1
class DicomReader {
public:
    explicit DicomReader(const std::string& filePath);
    ~DicomReader();

    // Non-copyable, non-movable (owns DCMTK state)
    DicomReader(const DicomReader&) = delete;
    DicomReader& operator=(const DicomReader&) = delete;

    /** Parse the file. Returns XPE_OK or error code. */
    XpeErrorCode open();

    /** Extract pixel data. Caller owns the allocated buffer. */
    XpeErrorCode readImage(XpeImageBuffer* outImg);

    /** Extract acquisition metadata. Missing tags silently defaulted. */
    XpeErrorCode getMetadata(XpeImageMetadata* outMeta);

private:
    std::string m_filePath;
    std::unique_ptr<DcmFileFormat> m_dcmFile;
    bool m_opened{false};
    std::string m_tsUID;  // Transfer Syntax UID read in open()

    /** Decompress J2K pixel data to raw uint16 via OpenJPEG. */
    XpeErrorCode decompressPixelData(DcmFileFormat* dcm, XpeImageBuffer* outImg);

    /** Decode a raw J2K bitstream buffer to outImg using OpenJPEG. */
    static XpeErrorCode decodeJ2KBitstream(const uint8_t* j2kData, size_t j2kLen,
                                            uint32_t rows, uint32_t cols,
                                            uint16_t bitsAlloc, uint16_t bitsStored,
                                            XpeImageBuffer* outImg);
};

} // namespace dicom
} // namespace xpe
