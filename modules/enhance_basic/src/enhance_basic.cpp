// enhance_basic.cpp -- Module entry point: version string
// SPEC-XPE-P1B-ENH  IEC 62304 Class B

#ifndef XPE_DLL_EXPORT
#define XPE_DLL_EXPORT
#endif

#include "xpe/enhance_basic/enhance_basic_api.h"

extern "C" {

XPE_API const char* xpe_enhance_basic_version(void)
{
    return "1.0.0";
}

} // extern "C"

