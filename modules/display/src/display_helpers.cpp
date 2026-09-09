/**
 * @file display_helpers.cpp
 * @brief Shared utility functions for xpe_display module.
 * SPEC: SPEC-XPE-P1B-DISP
 */

#include "xpe/display/display_internal.h"
#include "xpe/common/xpe_types.h"
#include "xpe/common/xpe_error.h"

extern "C" {

XpeErrorCode xpe_validate_float32(const XpeImageBuffer* img) {
    if (!img) {
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
