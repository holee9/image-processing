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

### AED Subsystem (3)
9. `xpe_aed_configure` - Configure AED
10. `xpe_aed_poll_event` - Poll for events
11. `xpe_aed_get_status` - Get AED state

### Memory (3)
12. `xpe_alloc_image` - Allocate image buffer
13. `xpe_free_image` - Free image buffer
14. `xpe_copy_image` - Copy image buffer

### Error & Alerts (4)
15. `xpe_error_string` - Get error description
16. `xpe_get_pending_alert_count` - Get alert count
17. `xpe_get_pending_alert` - Retrieve alert
18. `xpe_clear_alerts` - Clear alert queue

## Test Support Functions (2)

**Decision:** Keep test functions exported (Option A - White-box testing)

### Rationale
1. **Testing Efficiency**: Direct injection of test events/states
2. **CI/CD Integration**: Enables automated testing without mock frameworks
3. **Debugging Support**: Allows direct state manipulation for diagnostics
4. **No Harm**: Clearly documented as test-only, not for production use

### Functions
19. `xpe_test_inject_alert` - Inject alert for testing
20. `xpe_test_inject_aed_event` - Inject AED event for testing

### Usage Policy
- **Documented**: All test functions have `@MX:TEST` annotation
- **Naming Convention**: Prefixed with `xpe_test_` to indicate test-only status
- **Production Use**: NOT recommended for production code
- **ABI Stability**: Test functions may change between versions without notice

## Export Verification

### Method 1: dumpbin (Windows)
```cmd
dumpbin /exports xpe_common.dll
```

Expected output: 20 exported symbols (18 public + 2 test)

### Method 2: objdump (Linux/GNU)
```bash
objdump -T xpe_common.so | grep xpe_
```

### Method 3: nm (Unix)
```bash
nm -D xpe_common.so | grep xpe_
```

## P/Invoke Compatibility (REQ-P0-009)

All functions use Pack=8 blittable types for C# interop:

- **Structs**: `XpeImageBuffer` (verified Pack=8)
- **Enums**: `XpePixelFormat`, `XpeErrorCode`, `XpeAlertSeverity`
- **Scalars**: `int32_t`, `uint64_t`, `float`, `const char*`

## Future Changes

### Adding New Functions
1. Declare with `XPE_API` macro
2. Add to appropriate header (xpe_common_api.h, xpe_memory.h, xpe_error.h)
3. Update this document
4. Bump semantic version (MINOR for new public API, PATCH for internal)

### Deprecating Functions
1. Mark with `[[deprecated]]` attribute
2. Document migration path
3. Maintain for at least 2 major versions
4. Remove in major version bump

## References
- SPEC-XPE-P0 REQ-P0-008: Exactly 18 exported functions
- SPEC-XPE-P0 REQ-P0-009: Pack=8 blittable types
- IEC 62304 Class B: Stable ABI requirement
