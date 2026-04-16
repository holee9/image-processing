/**
 * @file DicomWriter.h
 * @brief Internal C++ class for DICOM Part 10 file writing (SWU-4.2).
 *
 * Wraps DCMTK DcmFileFormat + OpenJPEG for uncompressed and J2K lossless writing.
 * Not part of the public DLL ABI — internal use only.
 *
 * SPEC: SPEC-XPE-P1B-DICOM REQ-DICOM-013..022
 */
#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

// Forward declarations for DCMTK types
class DcmDataset;

namespace xpe {
namespace dicom {

// @MX:ANCHOR: [AUTO] Public API boundary — xpe_dicom_write, xpe_dicom_write_j2k
// @MX:REASON: Two exported API functions; shared DICOM dataset construction logic
// @MX:SPEC: SPEC-XPE-P1B-DICOM SWU-4.2
class DicomWriter {
public:
    /**
     * Write uncompressed DICOM file (Explicit VR Little Endian).
     * REQ-DICOM-013..018, REQ-DICOM-022
     */
    static XpeErrorCode write(const char* filePath,
                              const XpeImageBuffer* img,
                              const XpeImageMetadata* meta);

    /**
     * Write JPEG 2000 Lossless DICOM file.
     * REQ-DICOM-019..022
     */
    static XpeErrorCode writeJ2K(const char* filePath,
                                  const XpeImageBuffer* img,
                                  const XpeImageMetadata* meta);

private:
    /** Populate mandatory DX IOD DICOM tags from img and meta. */
    static XpeErrorCode populateDataset(void* dcmDataset,
                                         const XpeImageBuffer* img,
                                         const XpeImageMetadata* meta);

    /** Generate a DICOM-conformant UID string. */
    static std::string generateUID();

    /** Compress pixel buffer to J2K lossless using OpenJPEG. Returns empty on failure. */
    static std::vector<uint8_t> compressJ2K(const XpeImageBuffer* img);

    /** Populate a DcmPixelData element with J2K encoded data for DICOM saving. */
    static XpeErrorCode setJ2KPixelData(DcmDataset* ds, const std::vector<uint8_t>& j2kData);
};

} // namespace dicom
} // namespace xpe
