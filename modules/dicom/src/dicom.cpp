/**
 * @file dicom.cpp
 * @brief xpe_dicom.dll exported C API entry points.
 *
 * Thin wrappers that delegate to C++ implementation classes,
 * catching all exceptions to prevent crossing the ABI boundary.
 *
 * SPEC: SPEC-XPE-P1B-DICOM REQ-DICOM-041..046
 */
#define XPE_DLL_EXPORT
#include "xpe/dicom/dicom_api.h"

#include "DicomReader.h"
#include "DicomWriter.h"
#include "DicomValidator.h"
#include "DicomNetworkSCU.h"

#include <spdlog/spdlog.h>

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
 * SWU-4.1: DicomReader
 * -------------------------------------------------------------------------*/

XPE_API XpeErrorCode xpe_dicom_open(const char* filePath, XpeDicomHandle** outHandle) {
    spdlog::debug("[xpe_dicom] xpe_dicom_open({})", filePath ? filePath : "(null)");
    if (!filePath || !outHandle) return XPE_ERR_INVALID_INPUT;
    *outHandle = nullptr;
    try {
        auto* h = new XpeDicomHandle(filePath);
        XpeErrorCode rc = h->reader.open();
        if (rc != XPE_OK) { delete h; return rc; }
        *outHandle = h;
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

XPE_API XpeErrorCode xpe_dicom_write(const char* filePath,
                                      const XpeImageBuffer* img,
                                      const XpeImageMetadata* meta) {
    spdlog::debug("[xpe_dicom] xpe_dicom_write({})", filePath ? filePath : "(null)");
    if (!filePath || !img || !meta) return XPE_ERR_INVALID_INPUT;
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
