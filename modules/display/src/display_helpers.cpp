/**
 * @file display_helpers.cpp
 * @brief Shared utility functions for xpe_display module.
 * SPEC: SPEC-XPE-P1B-DISP
 */

#include "xpe/display/display_internal.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

#include <spdlog/spdlog.h>

extern "C" {

// REQ-DISP-031 (QA-B-200 M2a). The messages go to spdlog's default logger, which is the one xpe_common's
// xpe_log_set_level / xpe_log_set_file configure, so the module follows the project's logging switches. spdlog can
// throw (allocation); a log line must never change what the function returns, nor let an exception cross the ABI.
void xpe_display_log_enter(const char* fn) {
    try {
        spdlog::debug("[xpe_display] {}: enter", fn);
    } catch (...) {
    }
}

XpeErrorCode xpe_display_log_exit(const char* fn, XpeErrorCode rc) {
    try {
        if (rc == XPE_OK) {
            spdlog::debug("[xpe_display] {}: exit OK", fn);
        } else {
            spdlog::error("[xpe_display] {} failed: code {} ({})", fn, static_cast<int>(rc), xpe_error_string(rc));
        }
    } catch (...) {
    }
    return rc;
}

XpeErrorCode xpe_validate_float32(const XpeImageBuffer* img) {
    if (!img || !img->data) {
        return XPE_ERR_INVALID_INPUT;
    }
    // #142 (QA-B-41): the empty-image contract QA-B-40 set for enhance_basic.
    // width == 0, height == 0 or a NULL data pointer is INVALID_INPUT rather
    // than a no-op success. Judged before the format test because a dimension
    // is a property of the descriptor, not of the pixel type -- an empty UINT16
    // buffer is reported as empty, not as the wrong format. The data pointer
    // moved up with it: this validator used to let a NULL through to a
    // dereference in each entry point's pixel loop.
    if (img->width == 0 || img->height == 0) {
        return XPE_ERR_INVALID_INPUT;
    }
    if (img->format != XPE_PIXEL_FLOAT32) {
        return XPE_ERR_UNSUPPORTED_FORMAT;
    }
    // api-spec "XpeImageBuffer.dataSize on input" (#123). All three display
    // entry points route through here, so the module has one definition of the
    // check rather than three copies.
    if (!xpe_data_size_is_consistent(img)) {
        return XPE_ERR_INVALID_INPUT;
    }
    return XPE_OK;
}

} /* extern "C" */
