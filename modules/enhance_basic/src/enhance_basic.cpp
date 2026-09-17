// enhance_basic.cpp -- Module entry point: version string
// SPEC-XPE-P1B-ENH  IEC 62304 Class B

#ifndef XPE_DLL_EXPORT
#define XPE_DLL_EXPORT
#endif

#include "xpe/enhance_basic/enhance_basic_api.h"

#include <atomic>

// #179 (QA-B-103): thread-count request for the per-pixel passes. Process-wide
// and add-only; the output does not depend on it (see the header).
namespace {
std::atomic<int> g_maxThreads{0};
}

int XpeThreadRequest() { return g_maxThreads.load(std::memory_order_relaxed); }

extern "C" {

XPE_API const char* xpe_enhance_basic_version(void)
{
    return "1.0.0";
}

XPE_API XpeErrorCode xpe_enhance_basic_set_max_threads(int32_t threads)
{
    g_maxThreads.store(threads > 0 ? threads : 0, std::memory_order_relaxed);
    return XPE_OK;
}

XPE_API int32_t xpe_enhance_basic_get_max_threads(void)
{
    return g_maxThreads.load(std::memory_order_relaxed);
}

} // extern "C"

