#include "xpe/preprocess/xpe_preprocess_api.h"

#include <atomic>
#include <cstring>
#include <mutex>

namespace {
std::atomic_bool g_initialized{false};
std::mutex g_lifecycleMutex;

struct ParamRange {
    const char* name;
    float minValue;
    float maxValue;
};

constexpr ParamRange kParamRanges[] = {
    {"integration_time_ms", 1.0f, 10000.0f},
    {"temperature_c", -20.0f, 60.0f},
    {"kVp", 40.0f, 150.0f},
    {"mAs", 0.1f, 1000.0f},
    {"SID_mm", 1000.0f, 2000.0f},
    {"pixelPitch_mm", 0.1f, 0.5f},
};
} // namespace

extern "C" XPE_API const char* xpe_preprocess_version(void)
{
    return "0.1.0";
}

extern "C" XPE_API XpeErrorCode xpe_preprocess_init(const char* configJsonOrNull)
{
    try {
        std::lock_guard<std::mutex> lock(g_lifecycleMutex);
        if (configJsonOrNull && configJsonOrNull[0] == '\0') {
            return XPE_ERR_CONFIG_INVALID;
        }

        g_initialized.store(true, std::memory_order_release);
        return XPE_OK;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}

extern "C" XPE_API void xpe_preprocess_shutdown(void)
{
    try {
        std::lock_guard<std::mutex> lock(g_lifecycleMutex);
        g_initialized.store(false, std::memory_order_release);
    } catch (...) {
    }
}

extern "C" XPE_API XpeErrorCode xpe_preprocess_get_param_range(
    const char* paramName,
    float* minValue,
    float* maxValue)
{
    if (!paramName || !minValue || !maxValue) {
        return XPE_ERR_INVALID_INPUT;
    }

    for (const auto& range : kParamRanges) {
        if (std::strcmp(paramName, range.name) == 0) {
            *minValue = range.minValue;
            *maxValue = range.maxValue;
            return XPE_OK;
        }
    }

    return XPE_ERR_INVALID_INPUT;
}
