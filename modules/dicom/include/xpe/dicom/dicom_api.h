/**
 * @file dicom_api.h
 * @brief DICOM I/O module public C API for xpe_dicom.dll.
 *
 * Exports exactly 10 C-linkage functions organized into 4 Software Units:
 *   - SWU-4.1 DicomReader  : open / read_image / get_metadata / close
 *   - SWU-4.2 DicomWriter  : write / write_j2k
 *   - SWU-4.3 DicomValidator: validate
 *   - SWU-4.4 DicomNetworkSCU: cstore / cfind_mwl / cancel
 *
 * @note ABI contract: all parameters are blittable C types compatible with
 *       .NET P/Invoke marshalling. No C++ types cross the DLL boundary.
 * @note C++ exceptions from DCMTK or OpenJPEG are never propagated to
 *       the caller; they are caught internally and mapped to XpeErrorCode.
 *
 * @ingroup xpe_dicom
 * SPEC: SPEC-XPE-P1B-DICOM v1.0.0
 */
#ifndef XPE_DICOM_API_H
#define XPE_DICOM_API_H

#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Opaque handle to an open DICOM reader session.
 *
 * Allocated by xpe_dicom_open(), freed by xpe_dicom_close().
 * Never allocate or inspect this struct directly.
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
 * @return XPE_ERR_PROCESSING_FAILED if decompression fails.
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
 *
 * @note REQ-DICOM-009..010
 */
XPE_API XpeErrorCode xpe_dicom_get_metadata(XpeDicomHandle* handle, XpeImageMetadata* outMeta);

/**
 * @brief Close a DICOM reader session and free all associated resources.
 *
 * Passing NULL is safe (no-op).
 *
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
 * @param img       Source pixel buffer (XPE_PIXEL_UINT16). Must not be NULL.
 * @param meta      Acquisition metadata to embed. Must not be NULL.
 * @return XPE_OK on success.
 * @return XPE_ERR_INVALID_INPUT if any pointer is NULL.
 * @return XPE_ERR_IO_FAILED if the file cannot be written.
 *
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
 * @param img       Source pixel buffer (XPE_PIXEL_UINT16). Must not be NULL.
 * @param meta      Acquisition metadata to embed. Must not be NULL.
 * @return XPE_OK on success.
 * @return XPE_ERR_INVALID_INPUT if any pointer is NULL.
 * @return XPE_ERR_IO_FAILED if the file cannot be written.
 * @return XPE_ERR_PROCESSING_FAILED if J2K compression fails.
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
 * @return XPE_OK on success (check "valid" field in report).
 * @return XPE_ERR_INVALID_INPUT if filePath or outReportJson is NULL.
 * @return XPE_ERR_DICOM_INVALID if the file cannot be parsed at all.
 * @return XPE_ERR_BUFFER_TOO_SMALL if buffer is too small; required size written
 *         as uint32_t to the first 4 bytes of outReportJson.
 *
 * @note REQ-DICOM-023..028
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
 * @param timeoutMs  Connection/operation timeout in milliseconds (0 = no timeout).
 * @return XPE_OK on C-STORE success (RSP status 0x0000).
 * @return XPE_ERR_INVALID_INPUT if any pointer is NULL.
 * @return XPE_ERR_NETWORK_FAILED on connection failure, timeout, or rejection.
 * @return XPE_ERR_IO_FAILED if filePath cannot be read.
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
 * queryJson keys: "PatientID", "PatientName", "ScheduledStationAETitle",
 *                 "Modality", "ScheduledProcedureStepStartDate"
 *
 * @param host       Remote host address. Must not be NULL.
 * @param port       Remote DICOM port.
 * @param aet        Calling AE title. Must not be NULL.
 * @param queryJson  JSON object with DICOM tag key-value pairs. Must not be NULL.
 * @param outJson    Buffer to receive JSON array of results. Must not be NULL.
 * @param outBufLen  Size of outJson in bytes.
 * @param timeoutMs  Timeout in milliseconds (0 = no timeout).
 * @return XPE_OK on success (empty result writes "[]").
 * @return XPE_ERR_INVALID_INPUT if any required pointer is NULL.
 * @return XPE_ERR_NETWORK_FAILED on connection failure, timeout, or C-FIND failure.
 * @return XPE_ERR_BUFFER_TOO_SMALL if outBufLen is insufficient.
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
 * The cancelled operation returns XPE_ERR_PROCESSING_FAILED.
 *
 * @note REQ-DICOM-039..040
 */
XPE_API void xpe_dicom_cancel(void);

#ifdef __cplusplus
}
#endif

#endif /* XPE_DICOM_API_H */
