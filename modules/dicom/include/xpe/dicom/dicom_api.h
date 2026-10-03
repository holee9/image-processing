/**
 * @file dicom_api.h
 * @brief DICOM I/O module public C API for xpe_dicom.dll.
 *
 * Exports exactly 11 C-linkage functions: the version function and 10 organized into 4 Software Units:
 *   - SWU-4.1 DicomReader  : open / read_image / get_metadata / close
 *   - SWU-4.2 DicomWriter  : write / write_j2k
 *   - SWU-4.3 DicomValidator: validate
 *   - SWU-4.4 DicomNetworkSCU: cstore / cfind_mwl / cancel
 *   - version              : xpe_dicom_version (REQ-P0-033)
 *
 * @note ABI contract: all parameters are blittable C types compatible with
 *       .NET P/Invoke marshalling. No C++ types cross the DLL boundary.
 * @note C++ exceptions from DCMTK or OpenJPEG are never propagated to
 *       the caller; they are caught internally and mapped to XpeErrorCode.
 *       Concretely, every function except xpe_dicom_cancel() wraps its work in
 *       a catch-all that returns XPE_ERR_PROCESSING_FAILED -- or, for the two
 *       network functions, XPE_ERR_NETWORK_FAILED. xpe_dicom_close() has
 *       nothing to report and swallows the exception. That catch-all is not
 *       repeated in each @return list below.
 *
 * @ingroup xpe_dicom
 * SPEC: SPEC-XPE-P1B-DICOM v1.0.0
 */
#ifndef XPE_DICOM_API_H
#define XPE_DICOM_API_H

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

/**
 * @defgroup xpe_dicom XPE DICOM
 * @brief DICOM I/O module — read, write, validate, and network (SWU-4.1 to SWU-4.4).
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Returns the xpe_dicom module version string (e.g. "1.0.0").
 * @return Null-terminated version string. Lifetime: process. Never NULL.
 */
XPE_API const char* xpe_dicom_version(void);

/**
 * @brief Opaque handle to an open DICOM reader session.
 *
 * Allocated by xpe_dicom_open(), freed by xpe_dicom_close().
 * Never allocate or inspect this struct directly.
 *
 * THREAD SAFETY (QA-B-182, #235):
 *  - Different handles may be used from different threads at the same time: each owns its own parsed
 *    dataset, and the only module-wide state (registering the JPEG codecs with DCMTK) is done once under
 *    std::call_once. Measured: 8 threads, each opening and reading its own handle, 1440 reads, no mismatch.
 *  - ONE handle must not be used by two threads at the same time -- xpe_dicom_read_image,
 *    xpe_dicom_get_metadata and xpe_dicom_close on the same handle are serialised by the caller. Measured: 8
 *    threads calling xpe_dicom_read_image on one shared handle returned an intermittent
 *    XPE_ERR_DICOM_INVALID (2 of 7040 reads, 2 of 11 runs). That code comes from one of the two return
 *    paths in the reader that log nothing (the Rows/Columns lookup or the PixelData lookup; the logged
 *    "PixelData is short" path is ruled out). Which of the two was not determined. Both are DCMTK findAndGet*
 *    lookups on the handle's one dataset, and such lookups are not guaranteed read-only (DCMTK may load element
 *    values lazily), so a race inside the shared dataset is the likely cause. The module does not lock a handle.
 *  - xpe_dicom_cancel is the one call that may be made from any thread at any time (below).
 *
 * @ingroup xpe_dicom
 */
typedef struct XpeDicomHandle XpeDicomHandle;

/* -------------------------------------------------------------------------
 * SWU-4.1: DicomReader
 * -------------------------------------------------------------------------*/

/**
 * @brief Open and parse a DICOM Part 10 file.
 *
 * @param filePath  Null-terminated path to the DICOM file. Must not be NULL.
 * @param outHandle Output: receives the allocated handle on success, NULL on error.
 * @return XPE_OK on success.
 * @return XPE_ERR_INVALID_INPUT if filePath or outHandle is NULL.
 * @return XPE_ERR_IO_FAILED if the file does not exist or cannot be read.
 * @return XPE_ERR_DICOM_INVALID if the file is not a valid DICOM Part 10 file.
 * @return XPE_ERR_UNSUPPORTED_FORMAT if the Transfer Syntax is not supported.
 *
 * @note A file with no Part 10 meta header still opens: DCMTK accepts a bare
 *       dataset and the reader treats a missing Transfer Syntax UID as
 *       Explicit VR Little Endian. Use xpe_dicom_validate() to judge Part 10
 *       conformance -- xpe_dicom_open() judges only readability.
 * @note REQ-DICOM-001..005
 */
XPE_API XpeErrorCode xpe_dicom_open(const char* filePath, XpeDicomHandle** outHandle);

/**
 * @brief Extract pixel data from an open DICOM file into an XpeImageBuffer.
 *
 * Allocates the pixel buffer internally via xpe_alloc_image() and transfers
 * ownership to the caller (call xpe_free_image() to release).
 *
 * @param handle  Open DICOM handle from xpe_dicom_open(). Must not be NULL.
 * @param outImg  Output: receives populated XpeImageBuffer (XPE_PIXEL_UINT16).
 * @return XPE_OK on success.
 * @return XPE_ERR_INVALID_INPUT if handle or outImg is NULL.
 * @return XPE_ERR_OUT_OF_MEMORY if allocation fails.
 * @return XPE_ERR_DICOM_INVALID if the dataset carries no PixelData, if Rows /
 *         Columns are missing or zero, or if PixelData is SHORTER than
 *         Rows x Columns declares (#150: a partial image is never reported as a
 *         success -- SR-DCM-003 / HAZ-DCM-002). On this path @p outImg is left
 *         with no buffer to free. PixelData LONGER than declared is not an
 *         error: the surplus is ignored.
 * @return XPE_ERR_DICOM_INVALID if a COMPRESSED frame decodes to a size that
 *         DIFFERS from the declared Rows / Columns in either direction (#150) --
 *         J2K is judged against the codestream header, JPEG Lossless against the
 *         JPEG SOF marker, because DCMTK pads the decompressed frame to the
 *         declared size and a shortfall is no longer visible afterwards. On this
 *         path @p outImg is not written at all.
 *
 *         Note the asymmetry with the native path above, which is deliberate:
 *         surplus trailing BYTES are ordinary DICOM padding and contradict no
 *         dimension claim, so they stay a success; a surplus DIMENSION
 *         contradicts Rows / Columns, so it does not.
 * @return XPE_ERR_PROCESSING_FAILED if decompression fails.
 * @return XPE_ERR_UNSUPPORTED_FORMAT (QA-B-182/182b/182e/182f, #235) if the dataset is well formed but describes
 *         pixels this function cannot return faithfully as ONE plane of UNSIGNED 16-bit words: NumberOfFrames greater
 *         than 1, SamplesPerPixel other than 1 (RGB and the like), PixelRepresentation 1 (signed pixels),
 *         PhotometricInterpretation PALETTE COLOR (the pixel value is an index into the palette tables), the retired
 *         values and values whose meaning the standard does not define (anything but MONOCHROME1 and MONOCHROME2),
 *         and BitsAllocated values the standard allows but this function does not return: on the uncompressed path
 *         anything but 16 (1, 8, 24, 32 ...), on the JPEG Lossless path 8, and on the JPEG 2000 path 1, 24, 32 and
 *         40 (only 8 and 16 are returned there). A JPEG 2000 codestream must hold one unsigned component of at most
 *         16 bits of precision. All of this is judged before anything is decoded, allocated or written: @p outImg is
 *         untouched, the handle stays usable (the same call answers the same again and xpe_dicom_get_metadata still
 *         works). The handle's internal parse state is not promised unchanged -- DCMTK loads elements lazily --
 *         only what the API shows.
 * @return XPE_ERR_DICOM_INVALID (QA-B-182b/182c/182d/182e/182f) if the dataset breaks the standard or contradicts its
 *         own compressed stream:
 *         - SamplesPerPixel, PhotometricInterpretation, Rows, Columns, PixelRepresentation, BitsAllocated, BitsStored
 *           or HighBit is absent, empty or not a number / a string (they are Type 1 attributes of the Image Pixel
 *           Description Macro, PS3.3 Table C.7-11c, and have no default), or Rows / Columns is zero;
 *         - NumberOfFrames is present and not a number >= 1;
 *         - on EVERY path (PS3.5 8.1.1): BitsAllocated is neither 1 nor a multiple of 8, BitsStored is 0 or larger than
 *           BitsAllocated, or HighBit is not BitsStored - 1;
 *         - BitsAllocated is a value the transfer syntax's table does not list (JPEG Lossless: 8 and 16, PS3.5
 *           Table 8.2.1-2; JPEG 2000: 1, 8, 16, 24, 32 and 40, Table 8.2.4-1);
 *         - PhotometricInterpretation RGB, YBR_FULL, YBR_FULL_422, YBR_PARTIAL_420, YBR_ICT or YBR_RCT while
 *           SamplesPerPixel is 1 (PS3.3 C.7.6.3.1.2: those values are defined for three samples only);
 *         - a JPEG Lossless frame header declares a sample precision below BitsStored or above BitsAllocated, or a
 *           component count other than 1 (PS3.5 8.2: the attributes shall be consistent with the compressed data
 *           stream; a precision above BitsStored is accepted);
 *         - a JPEG 2000 codestream contradicts the dataset: a different number of components, signed samples, a
 *           precision that differs from BitsStored, or BitsAllocated below BitsStored (PS3.5 8.2.4);
 *         - RescaleSlope (0028,1053) or RescaleIntercept (0028,1052) is present and is not ONE finite number (empty,
 *           text, two values, inf or nan), or RescaleSlope is 0, or exactly ONE of the two is present (PS3.3 C.11.1
 *           requires the pair together; the alert names the missing one) (QA-B-187, QA-B-187c; judged before any
 *           decode, on every path). Both absent is the identity and is accepted.
 * @return XPE_ERR_UNSUPPORTED_FORMAT also for a JPEG 2000 codestream whose precision exceeds 16 bits.
 *
 * @note NumberOfFrames is the one attribute that may be absent (Multi-frame Module): absent means one frame.
 * @note Every refusal above also posts an XPE_ALERT_ERROR alert "dicom read refused: ..." that names the attribute
 *       or the two values that disagree (e.g. "JPEG 2000 codestream precision 16 does not match the dataset
 *       (BitsStored 12, BitsAllocated 16)"). The wording is a contract with the clients that display alerts.
 * @note Files written by the module's writer before QA-B-182b put a 16-bit-precision codestream under the image's
 *       BitsStored; those with BitsStored < 16 are refused by this function (XPE_ERR_DICOM_INVALID + that alert).
 * @note On success from a JPEG 2000 file the buffer is XPE_PIXEL_UINT16 with bitsAllocated 16 and bitsStored equal
 *       to the codestream's precision (an 8-bit file is described as 16 allocated, 8 stored: the buffer holds two
 *       bytes per sample).
 * @note PhotometricInterpretation MONOCHROME1 is returned INVERTED, in MONOCHROME2 sense (QA-B-185, #235,
 *       SRS-DICOM-001 FR-DCM-109): with B = outImg->bitsStored every returned word is
 *       (2^B - 1) - (stored & (2^B - 1)) -- the bits above BitsStored are not part of the sample and are masked off
 *       first. MONOCHROME2 is not inverted (its bits above BitsStored are masked as well, see the note below). The inversion is made on every path (uncompressed, JPEG Lossless,
 *       JPEG 2000), after the decode and on the caller's buffer, so a second read on the same handle returns the same
 *       words. One XPE_ALERT_INFO alert is posted per inverted read, with exactly this text (the wording is a
 *       contract with the clients that display alerts; {B} stands for the buffer's bitsStored, the braces only mark where a
 *       value is inserted):
 *       "MONOCHROME1 pixel values were inverted to MONOCHROME2 sense: value = (2^BitsStored - 1) - (stored &
 *       (2^BitsStored - 1)), BitsStored {B}". This alert is not posted for MONOCHROME2; an Info alert below is posted when its pixels changed.
 *       WHAT SURVIVES read -> write is the POLARITY, not the presentation: xpe_dicom_write always writes MONOCHROME2 +
 *       IDENTITY, so a MONOCHROME1 file read and written again is a MONOCHROME2 file with the inverted words, which
 *       has the polarity of the original. xpe_dicom_write does NOT preserve the source file's Window Center / Width
 *       (it writes none), Rescale Slope / Intercept (it writes 1 and 0) or Presentation LUT Shape (it writes
 *       IDENTITY): it copies nothing from the file the pixels came from. An image whose file carried a non-identity
 *       VOI or Rescale shows different brightness and contrast after read -> write (PS3.3 C.11.2). This records the
 *       current behaviour; it is not a requirement.
 *       The Window Center / Window Width and Rescale values stored IN THE FILE refer to the stored samples (the
 *       polarity is applied after the VOI transformation, PS3.3 C.11.2). This API returns neither, so nothing returned
 *       needs adjusting; a caller that reads them from the file itself and applies them to the returned (inverted)
 *       words must mirror them. With M = 2^B - 1, slope s, intercept b and K = s*M + 2*b the modality value of a
 *       returned word is K minus the modality value of the stored one, so the window width is unchanged and the centre
 *       c becomes K - c for VOI LUT Function LINEAR_EXACT and SIGMOID, and K - c + 1 for LINEAR (the default: its
 *       window is centred at c - 0.5, PS3.3 C.11.2.1.2). Slope 1 and intercept 0 give M - c and M - c + 1. An explicit
 *       VOI LUT table would have to be reversed instead (not covered here).
 *       Signed pixels (PixelRepresentation 1) are refused for every PhotometricInterpretation, MONOCHROME1 included.
 * @note Rescale (QA-B-187): the Modality LUT's RescaleSlope / RescaleIntercept are NOT applied and NOT returned (the
 *       metadata struct has no field for them): the returned pixels are the stored values. When the file's rescale is
 *       not the identity (slope 1 and intercept 0; an absent attribute counts as its identity value) ONE
 *       XPE_ALERT_WARNING is posted after a successful read, with exactly this text, the values spelled as the file
 *       spells them (a contract with the clients that display alerts; {s} and {b} mark where they are inserted):
 *       "RescaleSlope {s}, RescaleIntercept {b} (the identity is 1 and 0): returned pixels are stored values; rescale
 *       not applied". An explicit identity (1.0 and 0.0) posts nothing. A malformed or one-sided rescale is refused,
 *       see the XPE_ERR_DICOM_INVALID list above. Whether to apply or report the rescale is a separate decision.
 * @note Modality LUT Sequence (QA-B-187c): a file whose (0028,3000) sequence has at least one item is read as
 *       stored values too (the LUT is not applied), and ONE XPE_ALERT_WARNING is posted with exactly this text:
 *       "ModalityLUTSequence (0028,3000) is present: returned pixels are stored values; the Modality LUT was not
 *       applied".
 * @note Bits above BitsStored (QA-B-187, #235 item (h)): they are not part of the sample (PS3.5 8.1.1), so every
 *       returned word is (stored & (2^BitsStored - 1)) for MONOCHROME2 as well as MONOCHROME1. When at least one word
 *       changed, ONE XPE_ALERT_INFO is posted with exactly this text ({N} and {B} mark where the count of changed
 *       words and the BitsStored are inserted): "{N} pixel(s) had bits above BitsStored {B} set; those bits were
 *       masked off: value = stored & (2^BitsStored - 1)". Nothing is posted when no word changed;
 *       BitsStored 16 masks nothing.
 *
 * @note Transfer-Syntax support is decided in xpe_dicom_open(), not here: an
 *       unsupported syntax has already been rejected before a handle exists.
 *
 * @note REQ-DICOM-006..008
 */
XPE_API XpeErrorCode xpe_dicom_read_image(XpeDicomHandle* handle, XpeImageBuffer* outImg);

/**
 * @brief Extract acquisition metadata from an open DICOM file.
 *
 * Missing tags are silently defaulted (empty strings / 0.0f / 0).
 *
 * @param handle   Open DICOM handle. Must not be NULL.
 * @param outMeta  Output: receives populated XpeImageMetadata. Must not be NULL.
 * @return XPE_OK on success.
 * @return XPE_ERR_INVALID_INPUT if handle or outMeta is NULL.
 * @return XPE_ERR_DICOM_INVALID if the file carries no dataset.
 *
 * @note "Missing tags are silently defaulted" means absence is never an error
 *       here. A caller cannot distinguish "tag absent" from "tag present and
 *       empty" through this API.
 * @note REQ-DICOM-009..010
 */
XPE_API XpeErrorCode xpe_dicom_get_metadata(XpeDicomHandle* handle, XpeImageMetadata* outMeta);

/**
 * @brief Close a DICOM reader session and free all associated resources.
 *
 * Passing NULL is safe (no-op).
 *
 * @param handle DICOM session handle to close (may be NULL).
 * @note REQ-DICOM-011..012
 */
XPE_API void xpe_dicom_close(XpeDicomHandle* handle);

/* -------------------------------------------------------------------------
 * SWU-4.2: DicomWriter
 * -------------------------------------------------------------------------*/

/**
 * @brief Write a DICOM Part 10 file with Explicit VR Little Endian transfer syntax.
 *
 * SOP Class: Digital X-Ray Image Storage - For Presentation (1.2.840.10008.5.1.4.1.1.1.1).
 *
 * @param filePath  Destination file path. Must not be NULL.
 * @param img       Source pixel buffer (XPE_PIXEL_UINT16 only: any other
 *                  format, FLOAT32 and UINT8 included, is rejected, QA-B-201 M4).
 *                  Must not be NULL,
 *                  and must not be empty: a zero width or height, or a NULL
 *                  data pointer, is rejected (#142). A non-zero dataSize
 *                  smaller than width * height * bytes-per-pixel is rejected
 *                  (#123); dataSize == 0 means unspecified and is accepted.
 *                  Whatever dataSize says, the file's PixelData holds exactly
 *                  width * height * 2 bytes: dataSize == 0 writes the whole
 *                  image, and bytes beyond the image in a larger buffer are
 *                  not written (QA-B-206 C13).
 *                  The descriptor must agree with the 16-bit words written:
 *                  bitsAllocated 16 and bitsStored 1..16 (0 is not a default
 *                  and is rejected). The image must be describable by a file:
 *                  width and height at most 65535 (Rows and Columns are 16-bit
 *                  attributes) and width * height * 2 at most 0xFFFFFFFE bytes
 *                  (PixelData length). Anything else is rejected before a file
 *                  is created (QA-B-206 M1b).
 * @param meta      Acquisition metadata to embed. Must not be NULL.
 * @return XPE_OK on success.
 * @return XPE_ERR_INVALID_INPUT if any pointer is NULL, the image is empty,
 *         img->format is not XPE_PIXEL_UINT16, bitsAllocated is not 16 or
 *         bitsStored is not 1..16, the size is not representable (no file is
 *         created), or img->dataSize is inconsistent with its dimensions.
 * @return XPE_ERR_IO_FAILED if the file cannot be written.
 * @return XPE_ERR_PROCESSING_FAILED if the dataset cannot be assembled.
 *
 * @note The written file carries a generated SOP Instance UID and a meta group
 *       regenerated from the dataset, so its output satisfies
 *       xpe_dicom_validate().
 * @note The dataset is built from @p img and @p meta only: nothing is copied from any file the pixels were read from.
 *       Photometric Interpretation is always MONOCHROME2, Presentation LUT Shape IDENTITY, Rescale Slope / Intercept
 *       1 / 0, and no Window Center / Width is written. A caller that read a file with a different VOI, Rescale or
 *       polarity must not expect them in the copy (see xpe_dicom_read_image for the MONOCHROME1 case).
 * @note REQ-DICOM-013..018, REQ-DICOM-022
 */
XPE_API XpeErrorCode xpe_dicom_write(const char* filePath,
                                      const XpeImageBuffer* img,
                                      const XpeImageMetadata* meta);

/**
 * @brief Write a DICOM Part 10 file with JPEG 2000 Lossless transfer syntax.
 *
 * Transfer Syntax: JPEG 2000 Lossless Only (1.2.840.10008.1.2.4.90).
 * Bit-exact round-trip is guaranteed.
 *
 * @param filePath  Destination file path. Must not be NULL.
 * @param img       Source pixel buffer (XPE_PIXEL_UINT16 only, as for
 *                  xpe_dicom_write()). Must not be NULL.
 *                  The same empty-image (#142), pixel-format (QA-B-201 M4),
 *                  descriptor and representable-size (QA-B-206 M1b) and
 *                  dataSize consistency (#123) rules as xpe_dicom_write() apply. An empty image is reported
 *                  as INVALID_INPUT here rather than surfacing as a compressor
 *                  PROCESSING_FAILED, which is what it used to do.
 * @param meta      Acquisition metadata to embed. Must not be NULL.
 * @return XPE_OK on success.
 * @return XPE_ERR_INVALID_INPUT if any pointer is NULL, the image is empty,
 *         img->format is not XPE_PIXEL_UINT16, bitsAllocated is not 16 or
 *         bitsStored is not 1..16, the size is not representable (no file is
 *         created), or img->dataSize is inconsistent with its dimensions.
 * @return XPE_ERR_IO_FAILED if the file cannot be written.
 * @return XPE_ERR_PROCESSING_FAILED if J2K compression fails, or if the compressed
 *         bitstream is longer than one fragment can describe (0xFFFFFFFE bytes; it is
 *         refused, not split, and no file is created -- QA-B-206 M1c).
 *
 * @note REQ-DICOM-019..022
 */
XPE_API XpeErrorCode xpe_dicom_write_j2k(const char* filePath,
                                           const XpeImageBuffer* img,
                                           const XpeImageMetadata* meta);

/* -------------------------------------------------------------------------
 * SWU-4.3: DicomValidator
 * -------------------------------------------------------------------------*/

/**
 * @brief Validate a DICOM file for DX IOD conformance and produce a JSON report.
 *
 * Report JSON structure:
 * @code
 * {"valid":true,"errors":[],"warnings":[]}
 * {"valid":false,"errors":[{"tag":"0010,0020","message":"Missing required tag"}],"warnings":[]}
 * @endcode
 *
 * @param filePath      Path to the DICOM file. Must not be NULL.
 * @param outReportJson Buffer to receive the null-terminated JSON report. Must not be NULL.
 * @param reportBufLen  Size of outReportJson in bytes (recommended >= 4096).
 * Conformance covers the Part 10 file meta information group (0002) as well as
 * the dataset (#139): the file is reported invalid when the meta group is
 * absent, when TransferSyntaxUID (0002,0010) is missing or malformed, or when
 * MediaStorage SOP Class / Instance UID disagrees with the dataset's own
 * SOPClassUID / SOPInstanceUID. Each failing condition adds one entry to
 * "errors" tagged with the group-0002 element it concerns.
 *
 * Dataset attributes are judged by their Type in the DX IOD (QA-B-206). Type 1
 * (Study, Series and SOP Instance UID, Modality, Rows, Columns, Bits Allocated,
 * Bits Stored) must be present with a value; Patient's Name and Patient ID are
 * Type 2: present, and an empty value is conformant. A missing attribute and an
 * attribute with no value are separate entries. A UID that has a value must be
 * in dotted-numeric form; a UID with no value is reported once, as having no
 * value, and its format is not judged as well. Pixel Data is Type 1C, and the
 * Pixel Data Provider URL (0028,7FE0) belongs to the JPIP Referenced transfer
 * syntaxes (1.2.840.10008.1.2.4.94, .95 and the HTJ2K ones .204, .205, read from
 * (0002,0010)) alone. Basis: PS3.5 2026 current A.6, A.11 and A.12; PS3.3
 * C.7.6.3 still lists only .94 and .95, and this module follows PS3.5. Under
 * one of them, a file with no Pixel Data and a URL with a value is not reported
 * invalid, and "warnings" gets one entry saying that pixel data by reference is
 * not supported by this module. Under any other transfer syntax a URL replaces
 * nothing, and missing Pixel Data is an error. Pixel Data and the URL together
 * are an error under every syntax (mutually exclusive, PS3.5 8.2), and under a
 * JPIP Referenced syntax Pixel Data must not be in the file at all (PS3.5 A.6):
 * its presence is an error, with or without a URL. Only .94 and .204 are
 * exercised: the DCMTK this module builds against cannot read a file under .95
 * or .205 (the deflate variants), so conformance under those is NOT supported
 * and NOT verified -- such a file is reported as unparseable
 * (XPE_ERR_DICOM_INVALID).
 *
 * @return XPE_OK on success (check "valid" field in report). XPE_OK means the
 *         report was produced, NOT that the file is conformant.
 * @return XPE_ERR_INVALID_INPUT if filePath or outReportJson is NULL, or if
 *         reportBufLen is 0 -- a zero-length output buffer is a missing
 *         argument, not a small one (#142).
 * @return XPE_ERR_DICOM_INVALID if the file cannot be parsed at all. A report is
 *         still written to the buffer on this path.
 * @return XPE_ERR_BUFFER_TOO_SMALL if buffer is too small; required size written
 *         as uint32_t to the first 4 bytes of outReportJson. The buffer does NOT
 *         hold a valid JSON string in that case. A buffer shorter than 4 bytes
 *         still returns this code but receives no size report -- nothing is
 *         written past its end (#142).
 *
 * @note REQ-DICOM-023..028, #139
 */
XPE_API XpeErrorCode xpe_dicom_validate(const char* filePath,
                                         char* outReportJson,
                                         uint32_t reportBufLen);

/* -------------------------------------------------------------------------
 * SWU-4.4: DicomNetworkSCU
 * -------------------------------------------------------------------------*/

/**
 * @brief Send a DICOM file to a remote Storage SCP via C-STORE.
 *
 * Called AE title defaults to "ANY-SCP"; use "CALLED_AE\@hostname" in host
 * to specify it explicitly.
 *
 * @param host       Remote host address or "CALLED_AE\@hostname". Must not be NULL.
 * @param port       Remote DICOM port (e.g. 104, 11112).
 * @param aet        Calling AE title. Must not be NULL.
 * @param filePath   Path to the DICOM file to send. Must not be NULL.
 * @param timeoutMs  Connection/operation timeout in milliseconds. DCMTK takes whole seconds, so a value above 0 is
 *                  rounded UP to whole seconds, at least 1 s (QA-B-206 C10): 300 ms waits 1 s, 1400 ms waits 2 s.
 *                  0 keeps the DCMTK defaults: against a peer that accepts the connection and never answers the call
 *                  gives up after about 30 s with XPE_ERR_NETWORK_FAILED. 0 is NOT "no timeout" (QA-B-206).
 * @return XPE_OK on C-STORE success (RSP status 0x0000).
 * @return XPE_ERR_INVALID_INPUT if host, aet, or filePath is NULL.
 * @return XPE_ERR_NETWORK_FAILED on connection failure, timeout, rejection, or
 *         a non-success C-STORE response status.
 * @return XPE_ERR_IO_FAILED if filePath cannot be read, or carries no dataset.
 * @return XPE_ERR_PROCESSING_FAILED if a cancel is latched -- see
 *         xpe_dicom_cancel() for why this is racy in practice.
 *
 * @note If the file's meta group names no SOP Class UID, the dataset's
 *       SOPClassUID is used; if that is absent too, the DX For Presentation
 *       class is assumed. The call does not fail for a missing SOP Class.
 *
 * @note REQ-DICOM-029..033
 */
XPE_API XpeErrorCode xpe_dicom_cstore(const char* host,
                                       uint16_t port,
                                       const char* aet,
                                       const char* filePath,
                                       uint32_t timeoutMs);

/**
 * @brief Query a Modality Worklist SCP via C-FIND.
 *
 * queryJson keys honoured by the current implementation: "PatientID",
 * "PatientName", "Modality", "AccessionNumber". Any other key -- including
 * "ScheduledStationAETitle" and "ScheduledProcedureStepStartDate", which an
 * earlier version of this comment listed -- is accepted without error and has
 * no effect on the query. All four supported keys are also sent as universal
 * (empty) match keys when absent.
 *
 * @param host       Remote host address. Must not be NULL.
 * @param port       Remote DICOM port.
 * @param aet        Calling AE title. Must not be NULL.
 * @param queryJson  JSON object with DICOM tag key-value pairs. Must not be NULL.
 * @param outJson    Buffer to receive JSON array of results. Must not be NULL.
 * @param outBufLen  Size of outJson in bytes.
 * @param timeoutMs  Timeout in milliseconds; rounded up to whole seconds, at least 1 s (QA-B-206 C10). 0 keeps the DCMTK
 *                  defaults (about 30 s against a silent peer), it is NOT "no timeout" (QA-B-206).
 * @return XPE_OK on success (empty result writes "[]").
 * @return XPE_ERR_INVALID_INPUT if host, aet, queryJson, or outJson is NULL, or
 *         if outBufLen is 0 (#142). Judged before the association, so a broken
 *         output buffer costs no network round trip.
 * @return XPE_ERR_NETWORK_FAILED on connection failure, timeout, C-FIND failure,
 *         or when the peer accepts the association but not the Modality
 *         Worklist presentation context.
 * @return XPE_ERR_PROCESSING_FAILED if queryJson is not parseable JSON. Note
 *         that this is detected AFTER the association is negotiated, so a
 *         malformed query still costs a round trip to the peer.
 * @return XPE_ERR_BUFFER_TOO_SMALL if outBufLen is insufficient. Nothing is
 *         written to outJson in that case.
 *
 * @note REQ-DICOM-034..038
 */
XPE_API XpeErrorCode xpe_dicom_cfind_mwl(const char* host,
                                           uint16_t port,
                                           const char* aet,
                                           const char* queryJson,
                                           char* outJson,
                                           uint32_t outBufLen,
                                           uint32_t timeoutMs);

/**
 * @brief Signal cancellation to any in-progress C-STORE or C-FIND operation.
 *
 * Thread-safe. May be called from any thread. No-op if no operation is active.
 * A cancelled operation returns XPE_ERR_PROCESSING_FAILED.
 *
 * @note The cancel flag is cleared on entry to xpe_dicom_cstore() and
 *       xpe_dicom_cfind_mwl(), so calling this BEFORE starting an operation
 *       does not pre-cancel it -- the flag is erased. It only takes effect when
 *       set from another thread while an operation is already running, and the
 *       operation observes it at two points: before connecting and just after
 *       the association is established. A transfer that completes between those
 *       checks returns XPE_OK regardless.
 * @note REQ-DICOM-039..040
 */
XPE_API void xpe_dicom_cancel(void);

#ifdef __cplusplus
}
#endif

#endif /* XPE_DICOM_API_H */
