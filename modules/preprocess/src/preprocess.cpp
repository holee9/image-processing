#include "xpe/preprocess_api.h"
#include "xpe/preprocess/xpe_preprocess_internal.h"

#include <atomic>
#include <cstring>
#include <mutex>
#include <new>
#include <nlohmann/json.hpp>

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

/* Module lifecycle predicate declared in xpe_preprocess_internal.h.
 * g_initialized lives in this translation unit's anonymous namespace, so the
 * predicate has to be defined here. A same-named definition exists in the dead
 * xpe_preprocess.cpp, which is deliberately absent from XPE_TEST_SOURCES and
 * from the library sources (#112) -- adding it back collides at link time. */
extern "C" XPE_API bool xpe_preprocess_is_initialized(void)
{
    return g_initialized.load(std::memory_order_acquire);
}

extern "C" XPE_API const char* xpe_preprocess_version(void)
{
    return "0.1.0";
}

extern "C" XPE_API XpeErrorCode xpe_preprocess_init(const char* configJsonOrNull)
{
    try {
        std::lock_guard<std::mutex> lock(g_lifecycleMutex);

        if (g_initialized.load(std::memory_order_acquire)) {
            return XPE_ERR_INVALID_INPUT;
        }

        if (configJsonOrNull != nullptr) {
            // The same rule as every configuration this module reads (QA-A-209b): one valid JSON object, no member
            // name given twice at the top level; an empty or white-space-only text is not one.
            XpeConfigDoc doc;
            const XpeErrorCode rc = xpe_config_parse(configJsonOrNull, &doc);
            if (rc != XPE_OK) return rc;
        }

        g_initialized.store(true, std::memory_order_release);
        return XPE_OK;
    } catch (const std::bad_alloc&) {
        // QA-A-221b (#233): the header documents XPE_ERR_OUT_OF_MEMORY for an allocation failure and every other export
        // returns it; this one answered PROCESSING_FAILED (19 of 19 injected failures in the QA-A-221 sweep). The
        // initialized flag is set last, so the module is as it was.
        return XPE_ERR_OUT_OF_MEMORY;
    } catch (...) {
        return XPE_ERR_PROCESSING_FAILED;
    }
}

extern "C" XPE_API void xpe_preprocess_shutdown(void)
{
    try {
        std::lock_guard<std::mutex> lock(g_lifecycleMutex);
        g_initialized.store(false, std::memory_order_release);
        // QA-A-193 (#216): api-spec.md section 6 -- a cache-owned view is valid until module
        // shutdown. Done before g_calib_mutex is taken: the cache has its own mutex and the two are
        // never held together anywhere.
        xpe_calib_cache_clear();
        std::lock_guard<std::mutex> calib_lock(g_calib_mutex);
        g_calib = CalibrationData{};
        // QA-A-120 (#176): every module global, not only the maps. Clearing one
        // of three made the function's name describe less than it did.
        xpe_calib_mode_reset_globals();
    } catch (...) {
        // [no-throw-boundary] shutdown cannot report; resetting the remaining globals must not throw out of a void function
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
