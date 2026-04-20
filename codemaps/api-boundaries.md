# XPE API Boundaries and Contracts

## C ABI Specification

All XPE modules expose C-compatible APIs for DLL interoperability.

### Export Macro

```cpp
// xpe_types.h
#ifdef XPE_DLL_EXPORT
  #define XPE_API __declspec(dllexport)
#elif XPE_DLL_IMPORT
  #define XPE_API __declspec(dllimport)
#else
  #define XPE_API
#endif
```

### Calling Convention

- **C linkage**: `extern "C"` for all exported functions
- **Struct packing**: `#pragma pack(push, 8)` for all public structures
- **Name mangling**: Disabled (C linkage)
- **Exception safety**: No C++ exceptions cross ABI boundaries

### Error Handling

All functions return `XpeErrorCode`:

```cpp
typedef enum {
    XPE_OK = 0,
    XPE_ERR_INVALID_INPUT = -1,
    XPE_ERR_OUT_OF_MEMORY = -2,
    XPE_ERR_CONFIG_INVALID = -3,
    XPE_ERR_IO_FAILED = -4,
    XPE_ERR_NOT_INITIALIZED = -5,
    XPE_STATUS_NO_EVENT = 1,  // Non-error status code
    // ... more codes
} XpeErrorCode;
```

**Pattern**:
- Non-negative = Success or status
- Negative = Error

### Memory Management

**Image Structure**:
```cpp
typedef struct {
    uint32_t width;       // Image width in pixels
    uint32_t height;      // Image height in pixels
    uint32_t stride;      // Bytes per row
    XpePixelFormat format; // Pixel format
    void* data;           // Pixel data (allocated by xpe_alloc_image)
} XpeImage;
```

**Memory Functions**:
- `xpe_alloc_image()`: Allocate image buffer
- `xpe_free_image()`: Free image buffer
- `xpe_copy_image()`: Deep copy image

**Ownership Rules**:
- Caller owns allocated images
- Caller must free with `xpe_free_image()`
- No ownership transfer across API boundaries

## Module API Boundaries

### xpe_common.dll API

#### Lifecycle

```cpp
XPE_API const char* xpe_version(void);
XPE_API XpeErrorCode xpe_init(const char* configJsonOrNull);
XPE_API void xpe_shutdown(void);
```

**Contract**:
- `xpe_version()`: Returns static string (do not free)
- `xpe_init()`: Must be called first, NULL for defaults
- `xpe_shutdown()`: Must be called last, safe to call without init

#### Configuration

```cpp
XPE_API XpeErrorCode xpe_configure(const char* jsonConfig);
XPE_API XpeErrorCode xpe_get_param_range(
    const char* bodyPart,
    const char* paramName,
    float* minVal,
    float* maxVal,
    float* defaultVal
);
```

**Contract**:
- JSON config uses UTF-8 encoding
- Unknown keys are silently ignored (forward-compatible)
- `xpe_get_param_range()` returns GUI slider bounds

#### Logging

```cpp
XPE_API XpeErrorCode xpe_log_set_level(int32_t level);
XPE_API XpeErrorCode xpe_log_set_file(const char* filePath);
XPE_API void xpe_log_flush(void);
```

**Level Mapping**:
- 0 = TRACE
- 1 = DEBUG
- 2 = INFO
- 3 = WARN
- 4 = ERROR
- 5 = OFF

#### AED (Automatic Exposure Detection)

```cpp
XPE_API XpeErrorCode xpe_aed_configure(const char* configJsonOrNull);
XPE_API XpeErrorCode xpe_aed_poll_event(
    int32_t* eventTypeOut,
    uint64_t* timestampOut,
    float* signalLevelOut
);
XPE_API XpeErrorCode xpe_aed_get_status(int32_t* stateOut);
```

**Event Types**:
- 0 = exposure_start
- 1 = exposure_end
- 2 = exposure_trigger

**AED States**:
- 0 = IDLE
- 1 = ARMED
- 2 = TRIGGERED

### Module-Specific APIs

Each processing module follows this pattern:

```cpp
// Version
XPE_API const char* xpe_<module>_version(void);

// Lifecycle (if applicable)
XPE_API XpeErrorCode xpe_<module>_init(const char* config);
XPE_API void xpe_<module>_shutdown(void);

// Processing
XPE_API XpeErrorCode xpe_<module>_process(
    const XpeImage* input,
    XpeImage* output,
    const char* paramsJson
);
```

## Data Flow Patterns

### Typical Processing Pipeline

```cpp
// 1. Initialize library
xpe_init(NULL);

// 2. Allocate images
XpeImage* input = NULL;
XpeImage* output = NULL;
xpe_alloc_image(2048, 2048, XPE_PIX_MONO16, &input);
xpe_alloc_image(2048, 2048, XPE_PIX_MONO16, &output);

// 3. Process image (module-specific)
xpe_enhance_basic_process(input, output, "{\"clahe_enable\": true}");

// 4. Use output image
// ... display or save ...

// 5. Cleanup
xpe_free_image(input);
xpe_free_image(output);
xpe_shutdown();
```

### Error Handling Pattern

```cpp
XpeErrorCode err = xpe_init(NULL);
if (err != XPE_OK) {
    const char* msg = xpe_error_string(err);
    fprintf(stderr, "Init failed: %s\n", msg);
    return;
}
```

## Thread Safety

**Thread-Safe Functions**:
- `xpe_version()`
- `xpe_log_flush()`
- `xpe_error_string()`

**Not Thread-Safe**:
- `xpe_init()` / `xpe_shutdown()` (global state)
- `xpe_configure()` (global config)
- Module process functions (module-specific state)

**Pattern**:
- Single-threaded init/shutdown
- Multi-threaded processing allowed after init
- Each thread should use separate image buffers

## Version Compatibility

**Semantic Versioning**: MAJOR.MINOR.PATCH

- **MAJOR**: Breaking API changes
- **MINOR**: New features, backward-compatible
- **PATCH**: Bug fixes, backward-compatible

**API Stability Guarantees**:
- C ABI is stable within major version
- New functions may be added
- Existing functions will not change signatures
- Deprecated functions marked for one major version cycle

## Validation Rules

### Precondition Validation

**Input Validation**:
- NULL pointers → `XPE_ERR_INVALID_INPUT`
- Invalid enum values → `XPE_ERR_INVALID_INPUT`
- Out-of-range integers → `XPE_ERR_INVALID_INPUT`

**State Validation**:
- Function called before `xpe_init()` → `XPE_ERR_NOT_INITIALIZED`
- Function called after `xpe_shutdown()` → `XPE_ERR_NOT_INITIALIZED`

### Postcondition Guarantees

**On Success**:
- Output parameters are written
- Resources are allocated/initialized
- Invariant is maintained

**On Failure**:
- Output parameters are unchanged (unless documented)
- Error code accurately describes failure
- No resource leaks
- System state is consistent

## Inter-Module Communication

Modules communicate through:
1. **Common types** (`XpeImage`, `XpeErrorCode`)
2. **JSON configuration** (string-based config passing)
3. **Image buffers** (pass-through processing)

**Forbidden**:
- Direct C++ object sharing across DLL boundaries
- Exception propagation across DLL boundaries
- RTTI/type_info across DLL boundaries

## Future API Extensions

**Planned Additions**:
- Async processing API (callback-based)
- Streaming API (chunked processing)
- ROI (Region of Interest) API
- Multi-frame processing API

**Compatibility Strategy**:
- New functions with new names
- Existing functions deprecated, not removed
- Version-based feature detection

---

**Last Updated**: 2026-04-19
**Specification Version**: 0.1.0
