#include "xpe/common/xpe_common_api.h"
#include <cstdlib>
#include <cstring>

static const char* XPE_VERSION_STRING = "0.1.0";

XPE_API XpeErrorCode xpe_init(const char* configJsonOrNull) {
    (void)configJsonOrNull;
    /* TODO: Initialize logger, config manager, memory pool */
    return XPE_OK;
}

XPE_API void xpe_shutdown(void) {
    /* TODO: Cleanup resources */
}

XPE_API const char* xpe_version(void) {
    return XPE_VERSION_STRING;
}

XPE_API XpeErrorCode xpe_configure(const char* jsonConfig) {
    if (!jsonConfig) return XPE_ERR_INVALID_INPUT;
    /* TODO: Parse JSON config via nlohmann/json */
    return XPE_OK;
}

XPE_API XpeErrorCode xpe_get_param_range(const char* bodyPart, const char* paramName,
                                         float* minVal, float* maxVal, float* defaultVal) {
    if (!bodyPart || !paramName || !minVal || !maxVal || !defaultVal)
        return XPE_ERR_INVALID_INPUT;
    /* TODO: Lookup parameter ranges from ParameterValidator */
    *minVal = 0.0f;
    *maxVal = 1.0f;
    *defaultVal = 0.5f;
    return XPE_OK;
}

XPE_API const char* xpe_error_string(XpeErrorCode code) {
    switch (code) {
        case XPE_OK:                       return "Success";
        case XPE_ERR_INVALID_INPUT:        return "Invalid input parameter";
        case XPE_ERR_OUT_OF_MEMORY:        return "Out of memory";
        case XPE_ERR_PROCESSING_FAILED:    return "Processing failed";
        case XPE_ERR_CONFIG_INVALID:       return "Invalid configuration";
        case XPE_ERR_CALIBRATION_EXPIRED:  return "Calibration data expired";
        case XPE_ERR_NOT_INITIALIZED:      return "Module not initialized";
        case XPE_ERR_UNSUPPORTED_FORMAT:   return "Unsupported pixel format";
        case XPE_ERR_BUFFER_TOO_SMALL:     return "Buffer too small";
        case XPE_ERR_IO_FAILED:            return "I/O operation failed";
        case XPE_ERR_NETWORK_FAILED:       return "Network operation failed";
        default:                           return "Unknown error";
    }
}

XPE_API int32_t xpe_get_pending_alert_count(void) {
    /* TODO: Return count from alert queue */
    return 0;
}

XPE_API XpeErrorCode xpe_get_pending_alert(int32_t index, char* msg, size_t msgLen, int32_t* severity) {
    (void)index; (void)msg; (void)msgLen; (void)severity;
    return XPE_ERR_INVALID_INPUT; /* No alerts yet */
}

XPE_API void xpe_clear_alerts(void) {
    /* TODO: Clear alert queue */
}
