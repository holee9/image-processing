/**
 * @file DicomImageLimits.h
 * @brief What an uncompressed UINT16 DICOM file can describe, as header-inline predicates (QA-B-206 M1b, Codex #108).
 *
 * One definition shared by the two public writers (dicom.cpp), DicomWriter.cpp and the tests, so that the answer to "can this
 * image be written?" cannot differ between the door and the code behind it.
 */
#ifndef XPE_DICOM_IMAGE_LIMITS_H
#define XPE_DICOM_IMAGE_LIMITS_H

#include <cstdint>

#include "xpe/common/xpe_types.h"

namespace xpe {
namespace dicom {

/** Rows (0028,0010) and Columns (0028,0011) are US (16-bit unsigned) attributes. */
constexpr uint32_t kMaxRowsOrColumns = 0xFFFFu;

/**
 * The longest value an OB/OW element can hold: its length field is 32 bits and 0xFFFFFFFF means "undefined length",
 * so 0xFFFFFFFE, the largest even length. DCMTK's `putAndInsertUint8Array` takes the length as `unsigned long`
 * (32 bits on Windows), so a longer image is not representable either.
 */
constexpr uint64_t kMaxPixelDataBytes = 0xFFFFFFFEull;

/** Bytes per pixel of the only format the writers take (XPE_PIXEL_UINT16). */
constexpr uint64_t kBytesPerPixel = 2u;

/**
 * @brief width * height * 2, computed in 64 bits (a 32-bit product wraps at 2^32: 65535 x 32769 -> 65,534 bytes, Codex #108).
 * Precondition: width and height are at most kMaxRowsOrColumns (the product of two full 32-bit values times 2 would not fit
 * 64 bits); image_size_is_representable checks that before it calls this.
 */
inline uint64_t pixel_data_bytes(uint32_t width, uint32_t height) {
    return static_cast<uint64_t>(width) * static_cast<uint64_t>(height) * kBytesPerPixel;
}

/**
 * @brief Can a file hold an image of this size: Rows and Columns fit their 16-bit attributes and PixelData fits an element?
 * Evaluated on the 64-bit product, before anything is narrowed.
 */
inline bool image_size_is_representable(uint32_t width, uint32_t height) {
    if (width == 0u || height == 0u) return false;
    if (width > kMaxRowsOrColumns || height > kMaxRowsOrColumns) return false;
    return pixel_data_bytes(width, height) <= kMaxPixelDataBytes;
}

/** The only pixel format the writers take: XPE_PIXEL_UINT16 (QA-B-201 M4). */
inline bool image_format_is_writable(XpePixelFormat format) {
    return format == XPE_PIXEL_UINT16;
}

/**
 * @brief The length of one encapsulated fragment as the 32-bit value a DICOM item carries (QA-B-206 M1c, Codex #110).
 *
 * The size of a JPEG 2000 bitstream is only known after compression, and nothing says it is smaller than the raw image, so the
 * limit on the raw size (image_size_is_representable) does not bound it. A fragment is written whole, never split; one longer
 * than the largest even length (0xFFFFFFFE; 0xFFFFFFFF is "undefined length") cannot be described and is refused. @p out is
 * written only when the length fits.
 */
inline bool narrow_fragment_length(uint64_t bytes, uint32_t* out) {
    if (bytes > kMaxPixelDataBytes) return false;
    if (out != nullptr) *out = static_cast<uint32_t>(bytes);
    return true;
}

/**
 * @brief Do BitsAllocated / BitsStored describe the 16-bit words the writers emit?
 * BitsAllocated is 16 and BitsStored is 1..16. 0 is not accepted for either: neither the header nor api-spec promises
 * that 0 means "default" (QA-B-206 M1b); a descriptor that says 8 over 16-bit words would produce a file that contradicts
 * its own pixel data.
 */
inline bool image_bits_are_writable(uint32_t bitsAllocated, uint32_t bitsStored) {
    return bitsAllocated == 16u && bitsStored >= 1u && bitsStored <= 16u;
}

}  // namespace dicom
}  // namespace xpe

#endif /* XPE_DICOM_IMAGE_LIMITS_H */
