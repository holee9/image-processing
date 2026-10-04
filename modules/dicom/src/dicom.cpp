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


namespace {
// QA-B-209 C16 / 209b / 209c (REQ-DICOM-043: "log entry/exit at DEBUG level and error conditions at ERROR level"): every public
// function logs its EXIT at DEBUG, always, with the code it returns (`<fn> exit rc=<N>`), after the `<fn> entry` line it already
// wrote -- and when that code is not XPE_OK it ALSO logs one separate ERROR line `<fn> exit rc=<N> (<message>)`. The requirement
// has both halves; 209b replaced the DEBUG exit by the ERROR line for a failure (a misreading), which dropped half of it. The early
// INVALID_INPUT returns have no inner log site, so the ERROR line is the only ERROR they get; a failure that also has a detailed inner
// line produces that line too (the cause) next to this one (the outcome of the public call). The bodies are the `dicom_*_impl`
// functions below (unchanged), the exported function is a one-line wrapper over them, so every return path is covered without
// touching each `return`. Logging must never change what a public function returns or let an exception cross the ABI: every call is
// guarded.
XpeErrorCode log_exit(const char* fn, XpeErrorCode rc) {
    try {
        spdlog::debug("[xpe_dicom] {} exit rc={}", fn, static_cast<int>(rc));
        if (rc != XPE_OK) {
            spdlog::error("[xpe_dicom] {} exit rc={} ({})", fn, static_cast<int>(rc), xpe_error_string(rc));
        }
    } catch (...) {
        // logging must never change what a public function returns
    }
    return rc;
}
void log_entry_noexcept(const char* fn) {
    try {
        spdlog::debug("[xpe_dicom] {} entry", fn);
    } catch (...) {
    }
}
void log_exit_void(const char* fn) {
    try {
        spdlog::debug("[xpe_dicom] {} exit", fn);
    } catch (...) {
    }
}
}  // namespace

XPE_API const char* xpe_dicom_version(void) {
    log_entry_noexcept("xpe_dicom_version");
    const char* const version = "1.0.0";
    log_exit_void("xpe_dicom_version");
    return version;
}

/* -------------------------------------------------------------------------
 * SWU-4.1: DicomReader
 * -------------------------------------------------------------------------*/

static XpeErrorCode dicom_open_impl(const char* filePath, XpeDicomHandle** outHandle) {
    spdlog::debug("[xpe_dicom] xpe_dicom_open entry path={}", filePath ? filePath : "(null)");
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

static XpeErrorCode dicom_read_image_impl(XpeDicomHandle* handle, XpeImageBuffer* outImg) {
    spdlog::debug("[xpe_dicom] xpe_dicom_read_image entry");
    if (!handle || !outImg) return XPE_ERR_INVALID_INPUT;
    try {
        return handle->reader.readImage(outImg);
    } catch (...) {
        spdlog::error("[xpe_dicom] xpe_dicom_read_image: unexpected exception");
        return XPE_ERR_PROCESSING_FAILED;
    }
}

static XpeErrorCode dicom_get_metadata_impl(XpeDicomHandle* handle, XpeImageMetadata* outMeta) {
    spdlog::debug("[xpe_dicom] xpe_dicom_get_metadata entry");
    if (!handle || !outMeta) return XPE_ERR_INVALID_INPUT;
    try {
        return handle->reader.getMetadata(outMeta);
    } catch (...) {
        spdlog::error("[xpe_dicom] xpe_dicom_get_metadata: unexpected exception");
        return XPE_ERR_PROCESSING_FAILED;
    }
}

static void dicom_close_impl(XpeDicomHandle* handle) {
    spdlog::debug("[xpe_dicom] xpe_dicom_close entry");
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

static XpeErrorCode dicom_write_impl(const char* filePath,
                                      const XpeImageBuffer* img,
                                      const XpeImageMetadata* meta) {
    spdlog::debug("[xpe_dicom] xpe_dicom_write entry path={}", filePath ? filePath : "(null)");
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

static XpeErrorCode dicom_write_j2k_impl(const char* filePath,
                                           const XpeImageBuffer* img,
                                           const XpeImageMetadata* meta) {
    spdlog::debug("[xpe_dicom] xpe_dicom_write_j2k entry path={}", filePath ? filePath : "(null)");
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

static XpeErrorCode dicom_validate_impl(const char* filePath,
                                         char* outReportJson,
                                         uint32_t reportBufLen) {
    spdlog::debug("[xpe_dicom] xpe_dicom_validate entry path={}", filePath ? filePath : "(null)");
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

static XpeErrorCode dicom_cstore_impl(const char* host,
                                       uint16_t port,
                                       const char* aet,
                                       const char* filePath,
                                       uint32_t timeoutMs) {
    spdlog::debug("[xpe_dicom] xpe_dicom_cstore entry host={}:{} file={}", host ? host : "(null)", port, filePath ? filePath : "(null)");
    if (!host || !aet || !filePath) return XPE_ERR_INVALID_INPUT;
    try {
        return xpe::dicom::DicomNetworkSCU::cstore(host, port, aet, filePath, timeoutMs);
    } catch (...) {
        spdlog::error("[xpe_dicom] xpe_dicom_cstore: unexpected exception");
        return XPE_ERR_NETWORK_FAILED;
    }
}

static XpeErrorCode dicom_cfind_mwl_impl(const char* host,
                                           uint16_t port,
                                           const char* aet,
                                           const char* queryJson,
                                           char* outJson,
                                           uint32_t outBufLen,
                                           uint32_t timeoutMs) {
    spdlog::debug("[xpe_dicom] xpe_dicom_cfind_mwl entry host={}:{}", host ? host : "(null)", port);
    if (!host || !aet || !queryJson || !outJson) return XPE_ERR_INVALID_INPUT;
    try {
        return xpe::dicom::DicomNetworkSCU::cfindMwl(host, port, aet, queryJson,
                                                       outJson, outBufLen, timeoutMs);
    } catch (...) {
        spdlog::error("[xpe_dicom] xpe_dicom_cfind_mwl: unexpected exception");
        return XPE_ERR_NETWORK_FAILED;
    }
}

static void dicom_cancel_impl(void) {
    spdlog::debug("[xpe_dicom] xpe_dicom_cancel entry");
    try {
        xpe::dicom::DicomNetworkSCU::cancel();
    } catch (...) {
        // cancel must never throw
    }
}

XPE_API XpeErrorCode xpe_dicom_open(const char* filePath, XpeDicomHandle** outHandle) {
    return log_exit("xpe_dicom_open", dicom_open_impl(filePath, outHandle));
}
XPE_API XpeErrorCode xpe_dicom_read_image(XpeDicomHandle* handle, XpeImageBuffer* outImg) {
    return log_exit("xpe_dicom_read_image", dicom_read_image_impl(handle, outImg));
}
XPE_API XpeErrorCode xpe_dicom_get_metadata(XpeDicomHandle* handle, XpeImageMetadata* outMeta) {
    return log_exit("xpe_dicom_get_metadata", dicom_get_metadata_impl(handle, outMeta));
}
XPE_API void xpe_dicom_close(XpeDicomHandle* handle) {
    dicom_close_impl(handle);
    log_exit_void("xpe_dicom_close");
}
XPE_API XpeErrorCode xpe_dicom_write(const char* filePath, const XpeImageBuffer* img, const XpeImageMetadata* meta) {
    return log_exit("xpe_dicom_write", dicom_write_impl(filePath, img, meta));
}
XPE_API XpeErrorCode xpe_dicom_write_j2k(const char* filePath, const XpeImageBuffer* img, const XpeImageMetadata* meta) {
    return log_exit("xpe_dicom_write_j2k", dicom_write_j2k_impl(filePath, img, meta));
}
XPE_API XpeErrorCode xpe_dicom_validate(const char* filePath, char* outReportJson, uint32_t reportBufLen) {
    return log_exit("xpe_dicom_validate", dicom_validate_impl(filePath, outReportJson, reportBufLen));
}
XPE_API XpeErrorCode xpe_dicom_cstore(const char* host, uint16_t port, const char* aet, const char* filePath, uint32_t timeoutMs) {
    return log_exit("xpe_dicom_cstore", dicom_cstore_impl(host, port, aet, filePath, timeoutMs));
}
XPE_API XpeErrorCode xpe_dicom_cfind_mwl(const char* host, uint16_t port, const char* aet, const char* queryJson, char* outJson,
                                          uint32_t outBufLen, uint32_t timeoutMs) {
    return log_exit("xpe_dicom_cfind_mwl", dicom_cfind_mwl_impl(host, port, aet, queryJson, outJson, outBufLen, timeoutMs));
}
XPE_API void xpe_dicom_cancel(void) {
    dicom_cancel_impl();
    log_exit_void("xpe_dicom_cancel");
}
