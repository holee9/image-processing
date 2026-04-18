# xpe_common.dll Export Policy
# SPEC-XPE-P0 REQ-P0-008, REQ-P0-009

## Overview

xpe_common.dll exports exactly **20 symbols**:
- **18 public API functions** (documented, stable ABI)
- **2 test support functions** (white-box testing, internal use)

## XPE_API Macro Definition

```cpp
// xpe_types.h
#ifdef _WIN32
    #ifdef XPE_DLL_EXPORT
        #define XPE_API __declspec(dllexport)
    #else
        #define XPE_API __declspec(dllimport)
    #endif
#else
    #define XPE_API __attribute__((visibility("default")))
#endif
```

## Public API Functions (18)

### Lifecycle (3)
1. `xpe_init` - Initialize library
2. `xpe_shutdown` - Release resources
3. `xpe_version` - Get version string

### Configuration (2)
4. `xpe_configure` - Apply JSON configuration
5. `xpe_get_param_range` - Query parameter ranges

### Logging (3)
6. `xpe_log_set_level` - Set log level (0-5)
7. `xpe_log_set_file` - Redirect log to file
8. `xpe_log_flush` - Flush log buffer

