/**
 * @file dicom.cpp
 * @brief xpe_dicom.dll exported C API entry points.
 *
 * Thin wrappers that delegate to C++ implementation classes,
 * catching all exceptions to prevent crossing the ABI boundary.
 *
 * SPEC: SPEC-XPE-P1B-DICOM REQ-DICOM-041..046
 */
#ifndef XPE_DLL_EXPORT
#define XPE_DLL_EXPORT
#endif
#include "xpe/dicom/dicom_api.h"

#include "DicomReader.h"
#include "DicomWriter.h"
#include "DicomValidator.h"
#include "DicomNetworkSCU.h"
#include "DicomImageLimits.h"

#include <spdlog/spdlog.h>
#include <memory>

// @MX:ANCHOR: [AUTO] DLL ABI entry point — all exported functions guarded by catch(...)
// @MX:REASON: REQ-DICOM-042: C++ exceptions must never cross the DLL ABI boundary
// @MX:SPEC: SPEC-XPE-P1B-DICOM REQ-DICOM-041..042

/**
 * Internal struct backing XpeDicomHandle (opaque to callers).
 */
struct XpeDicomHandle {
    xpe::dicom::DicomReader reader;
    explicit XpeDicomHandle(const char* path) : reader(path) {}
};

/* -------------------------------------------------------------------------
 * Module version (REQ-P0-033: every module exports a version function)
 * -------------------------------------------------------------------------*/

XPE_API const char* xpe_dicom_version(void) {
    return "1.0.0";
}

/* -------------------------------------------------------------------------
 * SWU-4.1: DicomReader
 * -------------------------------------------------------------------------*/

XPE_API XpeErrorCode xpe_dicom_open(const char* filePath, XpeDicomHandle** outHandle) {
    spdlog::debug("[xpe_dicom] xpe_dicom_open({})", filePath ? filePath : "(null)");
    if (!filePath || !outHandle) return XPE_ERR_INVALID_INPUT;
    *outHandle = nullptr;
    try {
        // QA-B-206 C14: the handle is owned by a unique_ptr until it is handed out. `h` used to be a raw pointer declared
        // inside the try, out of reach of the catch below: if reader.open() threw (std::bad_alloc from DCMTK, say), the
        // XpeDicomHandle -- the reader and its DcmFileFormat -- was never freed. No input is known that makes open() throw
        // (DCMTK reports its failures as an OFCondition), so this is closed by construction and proven with an injected throw.
        auto h = std::make_unique<XpeDicomHandle>(filePath);
        XpeErrorCode rc = h->reader.open();
        if (rc != XPE_OK) return rc;
        *outHandle = h.release();
        return XPE_OK;
    } catch (...) {
        spdlog::error("[xpe_dicom] xpe_dicom_open: unexpected exception");
        return XPE_ERR_PROCESSING_FAILED;
    }
}

XPE_API XpeErrorCode xpe_dicom_read_image(XpeDicomHandle* handle, XpeImageBuffer* outImg) {
    spdlog::debug("[xpe_dicom] xpe_dicom_read_image");
    if (!handle || !outImg) return XPE_ERR_INVALID_INPUT;
    try {
        return handle->reader.readImage(outImg);
    } catch (...) {
        spdlog::error("[xpe_dicom] xpe_dicom_read_image: unexpected exception");
        return XPE_ERR_PROCESSING_FAILED;
    }
}

XPE_API XpeErrorCode xpe_dicom_get_metadata(XpeDicomHandle* handle, XpeImageMetadata* outMeta) {
    spdlog::debug("[xpe_dicom] xpe_dicom_get_metadata");
    if (!handle || !outMeta) return XPE_ERR_INVALID_INPUT;
    try {
        return handle->reader.getMetadata(outMeta);
    } catch (...) {
        spdlog::error("[xpe_dicom] xpe_dicom_get_metadata: unexpected exception");
        return XPE_ERR_PROCESSING_FAILED;
    }
}

XPE_API void xpe_dicom_close(XpeDicomHandle* handle) {
    spdlog::debug("[xpe_dicom] xpe_dicom_close");
    try {
        delete handle;
    } catch (...) {
        spdlog::error("[xpe_dicom] xpe_dicom_close: unexpected exception (ignored)");
    }
}

/* -------------------------------------------------------------------------
 * SWU-4.2: DicomWriter
 * -------------------------------------------------------------------------*/


namespace {
/**
 * @brief api-spec "XpeImageBuffer.dataSize on input" size-consistency check (#123).
 *
 * `dataSize == 0` is unspecified and accepted; a smaller-than-declared value
 * means the buffer cannot hold the image and is read past its allocation.
 * File-local: dicom has no internal header, and xpe_common exports are fixed
 * at 16 symbols (REQ-P0-008).
 */
// #142 (QA-B-41): the empty-image contract. width == 0, height == 0 or a NULL
// data pointer is INVALID_INPUT rather than a file with no pixels (write) or a
// compressor failure reported as PROCESSING_FAILED (write_j2k). Both writer
// entry points route through here so the module keeps one definition.
bool image_is_non_empty(const XpeImageBuffer* img) {
    return img != nullptr && img->data != nullptr &&
           img->width != 0u && img->height != 0u;
}

// QA-B-201 M4: the writers take XPE_PIXEL_UINT16 and nothing else. Until now a FLOAT32 image was written as a 32-bit
// file (BitsAllocated 32) that this module's own reader refuses (REQ-DICOM-006: the reader returns UINT16), and a UINT8
// image was written with its bytes read as 16-bit words. A format the writer does not support is refused at the door,
// before any dataset is built and before any file exists. XPE_ERR_INVALID_INPUT is the code xpe_error.h defines for
// "wrong pixel format"; the caller that holds a float image owns the choice of how to turn it into 16-bit counts
// (round, clamp, window), which a writer cannot know.
bool pixel_format_is_writable(const XpeImageBuffer* img) {
    return img != nullptr && xpe::dicom::image_format_is_writable(img->format);
}

// QA-B-206 M1b (Codex #108): the descriptor must agree with the 16-bit words the writer emits. `format == UINT16` with
// bitsAllocated 8 used to be written as a file whose (0028,0100) says 8 over 16-bit pixel data, which this module's own
// reader refuses. BitsAllocated is 16 and BitsStored 1..16; 0 is not a "default" anyone promised (header, api-spec).
bool bits_are_writable(const XpeImageBuffer* img) {
    return img != nullptr && xpe::dicom::image_bits_are_writable(img->bitsAllocated, img->bitsStored);
}

// QA-B-206 M1b (Codex #108): Rows/Columns are 16-bit attributes and PixelData an element of at most 0xFFFFFFFE bytes. A
// size beyond either cannot be described by a file; it used to be truncated by a 32-bit product / a 16-bit cast.
bool size_is_representable(const XpeImageBuffer* img) {
    return img != nullptr && xpe::dicom::image_size_is_representable(img->width, img->height);
}

// Called after pixel_format_is_writable, so the image is UINT16: two bytes per pixel. (It used to size FLOAT32 too, and
// returned "consistent" for a format it could not size; no format other than UINT16 reaches it any more, QA-B-201 M4.)
bool data_size_is_consistent(const XpeImageBuffer* img) {
    if (img == nullptr || img->dataSize == 0) return true;
    const uint64_t required = static_cast<uint64_t>(img->width) *
                              static_cast<uint64_t>(img->height) *
                              static_cast<uint64_t>(sizeof(uint16_t));
    return static_cast<uint64_t>(img->dataSize) >= required;
}
} // namespace

XPE_API XpeErrorCode xpe_dicom_write(const char* filePath,
                                      const XpeImageBuffer* img,
                                      const XpeImageMetadata* meta) {
    spdlog::debug("[xpe_dicom] xpe_dicom_write({})", filePath ? filePath : "(null)");
    if (!filePath || !img || !meta) return XPE_ERR_INVALID_INPUT;
    if (!image_is_non_empty(img)) return XPE_ERR_INVALID_INPUT;
    if (!pixel_format_is_writable(img)) return XPE_ERR_INVALID_INPUT;
    if (!bits_are_writable(img)) return XPE_ERR_INVALID_INPUT;
    if (!size_is_representable(img)) return XPE_ERR_INVALID_INPUT;
    if (!data_size_is_consistent(img)) return XPE_ERR_INVALID_INPUT;
    try {
        return xpe::dicom::DicomWriter::write(filePath, img, meta);
    } catch (...) {
        spdlog::error("[xpe_dicom] xpe_dicom_write: unexpected exception");
        return XPE_ERR_PROCESSING_FAILED;
    }
}

XPE_API XpeErrorCode xpe_dicom_write_j2k(const char* filePath,
                                           const XpeImageBuffer* img,
                                           const XpeImageMetadata* meta) {
    spdlog::debug("[xpe_dicom] xpe_dicom_write_j2k({})", filePath ? filePath : "(null)");
    if (!filePath || !img || !meta) return XPE_ERR_INVALID_INPUT;
    if (!image_is_non_empty(img)) return XPE_ERR_INVALID_INPUT;
    if (!pixel_format_is_writable(img)) return XPE_ERR_INVALID_INPUT;
    if (!bits_are_writable(img)) return XPE_ERR_INVALID_INPUT;
    if (!size_is_representable(img)) return XPE_ERR_INVALID_INPUT;
    if (!data_size_is_consistent(img)) return XPE_ERR_INVALID_INPUT;
    try {
        return xpe::dicom::DicomWriter::writeJ2K(filePath, img, meta);
    } catch (...) {
        spdlog::error("[xpe_dicom] xpe_dicom_write_j2k: unexpected exception");
        return XPE_ERR_PROCESSING_FAILED;
    }
}

/* -------------------------------------------------------------------------
 * SWU-4.3: DicomValidator
 * -------------------------------------------------------------------------*/

XPE_API XpeErrorCode xpe_dicom_validate(const char* filePath,
                                         char* outReportJson,
                                         uint32_t reportBufLen) {
    spdlog::debug("[xpe_dicom] xpe_dicom_validate({})", filePath ? filePath : "(null)");
    if (!filePath || !outReportJson) return XPE_ERR_INVALID_INPUT;
    try {
        return xpe::dicom::DicomValidator::validate(filePath, outReportJson, reportBufLen);
    } catch (...) {
        spdlog::error("[xpe_dicom] xpe_dicom_validate: unexpected exception");
        return XPE_ERR_PROCESSING_FAILED;
    }
}

/* -------------------------------------------------------------------------
 * SWU-4.4: DicomNetworkSCU
 * -------------------------------------------------------------------------*/

XPE_API XpeErrorCode xpe_dicom_cstore(const char* host,
                                       uint16_t port,
                                       const char* aet,
                                       const char* filePath,
                                       uint32_t timeoutMs) {
    spdlog::debug("[xpe_dicom] xpe_dicom_cstore({}:{} file={})", host ? host : "(null)", port, filePath ? filePath : "(null)");
    if (!host || !aet || !filePath) return XPE_ERR_INVALID_INPUT;
    try {
        return xpe::dicom::DicomNetworkSCU::cstore(host, port, aet, filePath, timeoutMs);
    } catch (...) {
        spdlog::error("[xpe_dicom] xpe_dicom_cstore: unexpected exception");
        return XPE_ERR_NETWORK_FAILED;
    }
}

XPE_API XpeErrorCode xpe_dicom_cfind_mwl(const char* host,
                                           uint16_t port,
                                           const char* aet,
                                           const char* queryJson,
                                           char* outJson,
                                           uint32_t outBufLen,
                                           uint32_t timeoutMs) {
    spdlog::debug("[xpe_dicom] xpe_dicom_cfind_mwl({}:{})", host ? host : "(null)", port);
    if (!host || !aet || !queryJson || !outJson) return XPE_ERR_INVALID_INPUT;
    try {
        return xpe::dicom::DicomNetworkSCU::cfindMwl(host, port, aet, queryJson,
                                                       outJson, outBufLen, timeoutMs);
    } catch (...) {
        spdlog::error("[xpe_dicom] xpe_dicom_cfind_mwl: unexpected exception");
        return XPE_ERR_NETWORK_FAILED;
    }
}

XPE_API void xpe_dicom_cancel(void) {
    spdlog::debug("[xpe_dicom] xpe_dicom_cancel");
    try {
        xpe::dicom::DicomNetworkSCU::cancel();
    } catch (...) {
        // cancel must never throw
    }
}
