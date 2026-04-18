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
