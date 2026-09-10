# XPE API Specification: Complete Exported C ABI Reference

**Document ID**: XPE-API-SPEC-001
**Version**: 1.4.0
**Date**: 2026-04-22
**Source Documents**: XPE-SRS-001, XPE-SAD-001, GSVG-SDD-001, GSVG-SRS-001, xpe_types.h, xpe_error.h, xpe_memory.h, xpe_common_api.h, SPEC-XPE-MASTER v3.0.0
**Changelog**: v1.1.0 -> v1.2.0 moved `xpe_calc_exposure_index` from `xpe_enhance_advanced.dll` to `xpe_enhance_basic.dll`. v1.2.0 -> v1.3.0 added the explicit-path management appendix and clarified that calibration paths remain caller-owned. v1.3.0 -> v1.4.0 removed AED (Auto Exposure Detection) functions and terminology; AED is a detector-hardware function outside XPE scope. Exported function count corrected to 79. Updated SPEC-XPE-MASTER reference to v3.0.0. Added GSVG-SRS-001 to source documents. Section 6 revised 2026-09-10 per #117: documents converged on the implemented global-calibration design introduced by SPEC-XPE-P1A M2, commit e9b8ed4 — the calibration loaders take a single path and populate a module-global store, the correction functions take `(input, output, metadata)` and read that store, and `XPE_ERR_CALIB_NOT_LOADED` was added for "initialised but map not loaded".
**Reference**: For JSON configuration schemas, calibration file formats, and body-part lookup tables, see xpe-implementation-reference.md. For production software integration patterns, see production-integration-guide.md.

---

## 1. ABI Conventions

| Rule | Value |
|------|-------|
| Calling convention | `__cdecl` (Windows default for C) |
| Struct packing | `#pragma pack(push, 8)` for 8-byte alignment |
| Type universe | Pure C types only (`stdint.h`, `stddef.h`); no STL and no RTTI |
| Linkage | `extern "C"` on all exported symbols |
| Export macro | `__declspec(dllexport)` (XPE_DLL_EXPORT defined) / `__declspec(dllimport)` (consumer) |
| Return convention | `XpeErrorCode` (`int32_t`) for all fallible functions; `void` or `const char*` for infallible |
| Error detail | Out-parameter `char* errorMsg` buffers where extended diagnostics are needed |
| Memory ownership | Caller allocates via `xpe_alloc_image()`, caller frees via `xpe_free_image()` |
| Thread safety | All processing functions are reentrant with independent caller-supplied buffers |
| Config format | `const char*` UTF-8 JSON string; `NULL` accepted as "use defaults" |

### P/Invoke Alignment Notes

- `XpeImageBuffer`: size = 40 bytes on x64 with pack=8 (20 bytes scalar fields + 4 bytes padding + 8 + 8). Map `void* data` as `IntPtr` in C#.
- `XpeImageMetadata`: size = 96 bytes (64 + 4+4+4+4+8+4 + 4 padding). All fields blittable.
- `XpePixelFormat`: marshal as `int` (`[MarshalAs(UnmanagedType.I4)]`).
- `XpeAlertSeverity`: marshal as `int`.
- Function pointers / callbacks: not used in this ABI; async results use alert polling.
- Strings returned as `const char*` are owned by the DLL (static storage); do NOT free them.
- Output `char*` buffers (e.g., `msg` in `xpe_get_pending_alert`) must be caller-allocated.

---

## 2. Common Types

Defined in `modules/common/include/xpe/common/xpe_types.h`:

```c
#pragma pack(push, 8)

typedef enum XpePixelFormat {
    XPE_PIXEL_UINT16  = 0,   /* 16-bit unsigned integer pixels */
    XPE_PIXEL_FLOAT32 = 1    /* 32-bit IEEE 754 floating-point pixels */
} XpePixelFormat;

typedef struct XpeImageBuffer {
    uint32_t       width;         /* Image width in pixels */
    uint32_t       height;        /* Image height in pixels */
    uint32_t       bitsAllocated; /* Storage bit depth (e.g., 16) */
    uint32_t       bitsStored;    /* Valid bit depth (e.g., 14) */
    XpePixelFormat format;        /* Pixel data type */
    void*          data;          /* Pixel data allocated via xpe_alloc_image */
    size_t         dataSize;      /* Byte size of data buffer; max 64 MB (4096x4096x4).
                                     Input contract: see "XpeImageBuffer.dataSize on input" */
} XpeImageBuffer;

**`XpeImageBuffer.dataSize` on input (normative, 2026-09-10, #123).** Buffers returned by `xpe_alloc_image` always carry the exact byte size. A caller that builds the struct by hand MUST either zero-initialise it (`XpeImageBuffer img{};`) or fill every field; reading an entry point with an indeterminate `dataSize` is undefined behaviour. On input:

- `dataSize == 0` means *unspecified*: the entry point trusts `width × height × bytesPerPixel(format)` and does not check the size (legacy behaviour).
- `dataSize != 0` and `dataSize < width × height × bytesPerPixel(format)` yields `XPE_ERR_INVALID_INPUT` (content validation, precedence class 2). A larger `dataSize` is accepted.
- `data == NULL` yields `XPE_ERR_INVALID_INPUT` regardless of `dataSize` (precedence class 1).

**Output buffers are not covered by this rule.** A caller-provided output `XpeImageBuffer` is written to, so its `dataSize` MUST be populated; an output `dataSize` smaller than the required byte count yields `XPE_ERR_BUFFER_TOO_SMALL`, and `0` is not treated as unspecified (leader decision 2026-09-10, QA-A-17).

`bytesPerPixel` is 2 for `XPE_PIXEL_UINT16` and 4 for `XPE_PIXEL_FLOAT32`. Rationale: a non-NULL buffer smaller than its declared dimensions reads past its allocation (QA-B-18); `0` stays accepted because existing callers do not populate the field.

typedef struct XpeImageMetadata {
    char     bodyPart[64];     /* Null-terminated body part label (e.g., "CHEST") */
    float    kVp;              /* Tube voltage in kilo-volts peak */
    float    mAs;              /* Tube current-time product in milliampere-seconds */
    float    SID_mm;           /* Source-Image Distance in millimetres */
    float    pixelPitch_mm;    /* Pixel pitch in millimetres */
    uint64_t acquisitionTime;  /* UNIX epoch milliseconds */
    uint32_t flags;            /* Bitfield: see XPE_FLAG_* constants */
} XpeImageMetadata;

#pragma pack(pop)

/* flags bitfield values */
#define XPE_FLAG_GHOST_CORRECTED         0x00000001u
#define XPE_FLAG_AI_PROCESSED            0x00000002u
#define XPE_FLAG_DEFECT_CORRECTED        0x00000004u
#define XPE_FLAG_GAIN_CORRECTED          0x00000008u
#define XPE_FLAG_READOUT_VALIDATED       0x00000010u
#define XPE_FLAG_TEMP_COMPENSATED        0x00000020u
#define XPE_FLAG_NONLINEARITY_CORRECTED  0x00000040u
#define XPE_FLAG_BINNING_CORRECTED       0x00000080u
#define XPE_FLAG_COLLIMATION_DETECTED    0x00000200u
#define XPE_FLAG_STITCHED                0x00000400u
#define XPE_FLAG_BONE_SUPPRESSED         0x00000800u
#define XPE_FLAG_GSVG_SKIPPED            0x00001000u
```

### XPE Error Codes

Defined in `modules/common/include/xpe/common/xpe_error.h`:

```c
typedef int32_t XpeErrorCode;

#define XPE_OK                       0   /* Success */
#define XPE_ERR_INVALID_INPUT       -1   /* NULL pointer, out-of-range value, wrong dimensions */
#define XPE_ERR_OUT_OF_MEMORY       -2   /* Heap allocation failed */
#define XPE_ERR_PROCESSING_FAILED   -3   /* Algorithm-internal failure */
#define XPE_ERR_CONFIG_INVALID      -4   /* Malformed or unsupported JSON config */
#define XPE_ERR_CALIBRATION_EXPIRED -5   /* Calibration data past expiry date */
#define XPE_ERR_NOT_INITIALIZED     -6   /* xpe_init() not called or failed */
#define XPE_ERR_UNSUPPORTED_FORMAT  -7   /* XpePixelFormat not supported by this function */
#define XPE_ERR_BUFFER_TOO_SMALL    -8   /* Caller output buffer insufficient */
#define XPE_ERR_IO_FAILED           -9   /* File read/write error */
#define XPE_ERR_NETWORK_FAILED      -10  /* DICOM network (C-STORE / C-FIND) failure */
```

Additional codes defined in the same header and referenced by this document:

| Code | Value | Meaning |
|------|-------|---------|
| `XPE_ERR_CALIB_NOT_LOADED` | Not yet present in `xpe_error.h` as of 2026-09-10; assigned in QA-A-20 as the next code after the current last entry (`XPE_ERR_NOT_IMPLEMENTED = -15`) | The module is initialised, but the calibration map required by the requested correction has not been loaded into the module-global calibration store. Returned by `xpe_offset_correct`, `xpe_gain_correct`, `xpe_defect_correct` and the pipeline entry points. Distinct from `XPE_ERR_NOT_INITIALIZED`, which means `xpe_preprocess_init()` was never called (or the module was shut down). |

---

## 3. GSVG Types

Defined independently. GSVG does not depend on `xpe_common` types:

```c
#pragma pack(push, 8)

typedef struct GsvgConfig {
    int32_t  gridFrequency_lp_per_mm; /* Anti-scatter grid line frequency */
    float    gridAngle_deg;           /* Grid orientation angle in degrees */
    int32_t  algorithmMode;           /* 0=Auto, 1=Fourier, 2=Wavelet */
    float    suppressionStrength;     /* 0.0-1.0; suppression aggressiveness */
    int32_t  enableVirtualGrid;       /* 1 = synthesise virtual grid post-suppression */
    char     reserved[64];           /* Zero-padded, for future extension */
} GsvgConfig;

typedef struct GsvgImageMetadata {
    uint32_t width;            /* Image width in pixels */
    uint32_t height;           /* Image height in pixels */
    float    pixelPitch_mm;    /* Detector pixel pitch in millimetres */
    float    kVp;              /* Acquisition tube voltage */
    float    mAs;              /* Acquisition tube current-time product */
    char     bodyPart[64];     /* Body part label */
    uint32_t flags;            /* GSVG_FLAG_* bitfield */
} GsvgImageMetadata;

#pragma pack(pop)

/* GsvgErrorCode */
typedef int32_t GsvgErrorCode;
#define GSVG_OK                      0
#define GSVG_ERR_INVALID_INPUT      -1
#define GSVG_ERR_OUT_OF_MEMORY      -2
#define GSVG_ERR_PROCESSING_FAILED  -3
#define GSVG_ERR_CONFIG_INVALID     -4
#define GSVG_ERR_GRID_NOT_DETECTED  -5
#define GSVG_ERR_LUT_LOAD_FAILED    -6
#define GSVG_ERR_UNSUPPORTED_FORMAT -7
```

---

## 4. Function Count Summary

| DLL | Exported Functions | Notes |
|-----|--------------------|----|
| xpe_common.dll | 16 | removed 3 AED functions; AED is detector hardware only. 15 public API + `xpe_alert_push` (§5.16; renamed from `xpe_test_inject_alert` in #111, alias removed QA-A-19) |
| xpe_preprocess.dll | 18 | no change |
| xpe_enhance_basic.dll | 8 | includes `xpe_calc_exposure_index` moved from enhance_advanced |
| xpe_enhance_advanced.dll | 3 | `xpe_calc_exposure_index` moved to enhance_basic |
| xpe_ai.dll | 7 | no change |
| xpe_display.dll | 11 | no change |
| xpe_dicom.dll | 10 | no change |
| gsvg.dll | 8 | no change |
| **Total** | **79** | **-3 from v1.3.0** |

---

## 5. xpe_common.dll

Provides library lifecycle, memory management, configuration, parameter ranges, alert polling, and logging.

Dependencies: none (base layer).

### 5.1 xpe_init

```c
XPE_API XpeErrorCode xpe_init(const char* configJsonOrNull);
```

**Description**: Initialises all XPE subsystems. Must be called once before any other XPE function. Pass `NULL` to accept default configuration.  
**SRS**: SRS-INIT-001, SRS-INIT-002  
**Thread safety**: Not thread-safe ;call from a single thread at startup.  
**Error codes**: `XPE_OK`, `XPE_ERR_CONFIG_INVALID`, `XPE_ERR_OUT_OF_MEMORY`

---

### 5.2 xpe_shutdown

```c
XPE_API void xpe_shutdown(void);
```

**Description**: Releases all XPE subsystem resources. Must be the last XPE call; no XPE function may be called after this returns.  
**SRS**: SRS-INIT-003  
**Thread safety**: Not thread-safe ;call from a single thread at shutdown.  
**Error codes**: none; this function returns `void`

---

### 5.3 xpe_version

```c
XPE_API const char* xpe_version(void);
```

**Description**: Returns a pointer to a static, null-terminated version string (e.g., `"1.0.0-rc1"`). The returned buffer is DLL-owned; do NOT free it.  
**SRS**: SRS-VER-001  
**Thread safety**: Thread-safe (read-only static storage).  
**Error codes**: (never NULL)

---

### 5.4 xpe_configure

```c
XPE_API XpeErrorCode xpe_configure(const char* jsonConfig);
```

**Description**: Applies a runtime configuration update from a UTF-8 JSON string. Keys not present in the JSON are left unchanged. Forward-compatible: unknown keys are silently ignored.  
**SRS**: SRS-CFG-001, SRS-CFG-002  
**Thread safety**: Not thread-safe; serialize configuration changes with respect to processing calls.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT` (NULL), `XPE_ERR_CONFIG_INVALID`

---

### 5.5 xpe_alloc_image

```c
XPE_API XpeErrorCode xpe_alloc_image(uint32_t width, uint32_t height,
                                      XpePixelFormat format, XpeImageBuffer* out);
```

**Description**: Allocates pixel data for `out->data` and fills all fields of `*out`. Caller must eventually call `xpe_free_image` on the same buffer.  
**SRS**: SRS-MEM-001, SRS-MEM-002  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_OUT_OF_MEMORY`, `XPE_ERR_UNSUPPORTED_FORMAT`

---

### 5.6 xpe_free_image

```c
XPE_API XpeErrorCode xpe_free_image(XpeImageBuffer* buf);
```

**Description**: Releases the `data` buffer allocated by `xpe_alloc_image` and zeroes `buf->data` and `buf->dataSize`. Passing a zero-initialised buffer is a no-op.  
**SRS**: SRS-MEM-003  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT` (NULL buf)

---

### 5.7 xpe_copy_image

```c
XPE_API XpeErrorCode xpe_copy_image(const XpeImageBuffer* src, XpeImageBuffer* dst);
```

**Description**: Copies pixel data from `src` into the pre-allocated `dst`. `dst` must already be allocated with matching dimensions and format.  
**SRS**: SRS-MEM-004  
**Thread safety**: Reentrant (provided src and dst are independent buffers).  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_BUFFER_TOO_SMALL`

---

### 5.8 xpe_error_string

```c
XPE_API const char* xpe_error_string(XpeErrorCode code);
```

**Description**: Returns a static, human-readable English description for `code`. Unknown codes return `"Unknown error"`. Returned pointer is DLL-owned.  
**SRS**: SRS-ERR-001  
**Thread safety**: Thread-safe (read-only static storage).  
**Error codes**: (never NULL)

---

### 5.9 xpe_get_pending_alert_count

```c
XPE_API int32_t xpe_get_pending_alert_count(void);
```

**Description**: Returns the number of unread alerts queued in the internal alert ring buffer. Returns 0 when no alerts are pending.  
**SRS**: SRS-ALERT-001, SRS-ALERT-002  
**Thread safety**: Thread-safe (atomic read).  
**Error codes**: (returns count; negative values indicate internal error)

---

### 5.10 xpe_get_pending_alert

```c
XPE_API XpeErrorCode xpe_get_pending_alert(int32_t index, char* msg, size_t msgLen,
                                            int32_t* severity);
```

**Description**: Copies the alert message at `index` into caller-supplied `msg` buffer of `msgLen` bytes, and sets `*severity` to an `XpeAlertSeverity` value. `index` is zero-based; does not consume the alert.  
**SRS**: SRS-ALERT-003, SRS-ALERT-004  
**Thread safety**: Thread-safe.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_BUFFER_TOO_SMALL`

---

### 5.11 xpe_clear_alerts

```c
XPE_API void xpe_clear_alerts(void);
```

**Description**: Discards all queued alerts from the alert ring buffer.  
**SRS**: SRS-ALERT-005, SRS-ALERT-006  
**Thread safety**: Thread-safe.  
**Error codes**: (void)

---

### 5.12 xpe_get_param_range

```c
XPE_API XpeErrorCode xpe_get_param_range(const char* bodyPart, const char* paramName,
                                          float* minVal, float* maxVal, float* defaultVal);
```

**Description**: Retrieves the valid range and default value for a named processing parameter scoped to a body part. Used by GUI sliders to clamp user input without hard-coding limits.  
**SRS**: SRS-SAFE-002, SRS-SAFE-005  
**Thread safety**: Thread-safe (read-only lookup).  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_NOT_INITIALIZED`

---

### 5.13 xpe_log_set_level

```c
XPE_API XpeErrorCode xpe_log_set_level(int32_t level);
```

**Description**: Sets the minimum log severity level (0=TRACE, 1=DEBUG, 2=INFO, 3=WARN, 4=ERROR, 5=OFF). Messages below this level are discarded.  
**SRS**: SRS-LOG-001  
**Thread safety**: Not thread-safe; set at startup before processing begins.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT` (level out of range 0-5)

---

### 5.14 xpe_log_set_file

```c
XPE_API XpeErrorCode xpe_log_set_file(const char* filePath);
```

**Description**: Redirects log output to the file at `filePath` (UTF-8 path). Pass `NULL` to revert to stderr. File is opened in append mode.  
**SRS**: SRS-LOG-002  
**Thread safety**: Not thread-safe; set at startup.  
**Error codes**: `XPE_OK`, `XPE_ERR_IO_FAILED`, `XPE_ERR_INVALID_INPUT`

---

### 5.15 xpe_log_flush

```c
XPE_API void xpe_log_flush(void);
```

**Description**: Flushes any buffered log entries to disk immediately. Useful before process termination or crash reporting.  
**SRS**: SRS-LOG-003  
**Thread safety**: Thread-safe.  
**Error codes**: (void)

---


### 5.16 xpe_alert_push (alert queue producer)

```c
XPE_API void xpe_alert_push(const char* msg, int32_t severity);
```

Pushes an alert onto the alert queue read by `xpe_get_pending_alert` (overflow behaviour: §5.17 / SRS-ALERT-007). Called from
`modules/enhance_basic/src/exposure_index.cpp` (a cross-DLL producer, not a test hook) and by the
integration test suites. Counted in the export total (16) because it is a real DLL export
(REQ-P0-008 as revised 2026-09-09, #111).

**Renamed (#111, completed 2026-09-10).** This function was exported as `xpe_test_inject_alert`; the `test_` prefix misdescribed a production ABI. The rename landed in three stages (QA-A-18 added `xpe_alert_push` with a temporary alias, QA-B-24 moved the enhance_basic caller, QA-A-19 removed the alias). `xpe_test_inject_alert` is no longer exported; the DLL exports exactly 16 symbols (REQ-P0-008). Hosts binding to the old name fail to load.

### 5.17 Alert queue overflow policy (normative — SRS-ALERT-007, HAZ-006, decision #110)

The alert queue in `xpe_common` has a fixed capacity (`kAlertQueueMax`, currently 64 entries;
an implementation constant, not an exported symbol). When `xpe_alert_push` is called on a full
queue the following rules apply, in order:

1. **Priority-protected FIFO eviction.** Evict the oldest queued alert whose severity is
   `XPE_ALERT_INFO`. Only when no Info alert remains, evict the oldest `XPE_ALERT_WARNING`;
   only when neither remains, evict the oldest `XPE_ALERT_ERROR`. Within one severity class the
   order is strictly FIFO. Pushing a Warning/Error therefore never silently drops another
   Warning/Error while an Info entry is still queued.
2. **Guaranteed loss alert.** The queue keeps a cumulative count of evicted alerts since the
   last `xpe_clear_alerts`. While that count is greater than zero, exactly one synthetic alert
   with severity `XPE_ALERT_ERROR` and message
   `"alert queue overflow: <N> alert(s) dropped"` (N = cumulative count, ASCII decimal) is
   retrievable through `xpe_get_pending_alert` / counted by `xpe_get_pending_alert_count`.
   It is updated in place (never duplicated), is never selected for eviction, and occupies one
   of the 64 slots. `xpe_clear_alerts` removes it and resets the count to zero.
   **Counting note (observed by QA-A-28):** N counts entries actually evicted, not overflowing pushes. The first overflow on a
   full queue evicts two entries (one to admit the incoming alert, one to seat the loss alert), so the first loss alert
   reports N = 2, and the effective capacity for module alerts is 63 for as long as the loss alert is present.
   Identification is by an internal flag, not by the message prefix, so a module alert that happens to start with the
   same text is an ordinary (evictable) entry.
3. **No new export.** The policy is observable only through the existing §5.9–§5.11 and §5.16
   functions; `xpe_common.dll` stays at 16 exports (REQ-P0-008). The producer-side signature of
   `xpe_alert_push` is unchanged.

Consumers (Lane C display) MUST render the loss alert like any other Error alert; its message
prefix `alert queue overflow:` is stable and may be used to distinguish it from module alerts.

---

## 6. xpe_preprocess.dll

Provides offline calibration (offset / gain / defect map), runtime correction, and ghost artifact correction.

Dependencies: xpe_common.dll.

### Calibration state model (normative)

Calibration is a **two-step, load-then-correct** model; the correction functions do not take a map argument.

1. **Load.** `xpe_calib_load_offset` / `xpe_calib_load_gain` / `xpe_calib_load_defect_map` each take a single file path and read the map into a **module-global calibration store** (SAD-CALIB-001 SWU-1.5, "LoadedCalibration global cache"). There is one store per loaded `xpe_preprocess.dll` instance, not one per caller.
2. **Correct.** `xpe_offset_correct` / `xpe_gain_correct` / `xpe_defect_correct` take `(input, output, metadata)` and read the corresponding map from that global store. If the module is initialised but the required map has not been loaded, they return `XPE_ERR_CALIB_NOT_LOADED`.

3. **Pipeline stage propagation (decision #120 / QA-A-34, 2026-09-10).** `xpe_preprocess_pipeline` treats the three correction stages alike: when a stage is not bypassed (`bypassOffset` / `bypassGain` / `bypassDefect` false) and its map is not loaded, the pipeline stops with `XPE_ERR_CALIB_NOT_LOADED`. A silently skipped defect stage (observed by QA-A-32: `XPE_OK` with no correction) is a defect, not a contract — a caller would otherwise read "success" for a frame that was never defect-corrected (SRS-ALERT-001). Skipping is only ever the result of an explicit bypass flag.

The store is guarded by a module-internal mutex (`g_calib_mutex`). Loaders take the lock to write; correction calls take it only to read the map they need, then release it before running the pixel kernel. Concurrent correction calls are therefore safe against each other; a load concurrent with a correction is serialised but the ordering between them is the caller's responsibility.

**`xpe_calib_state_load` contract.** `xpe_calib_state_load(state, calibPath)` is a compatibility wrapper: it composes `offset.xcal`, `gain.xcal` and `defect.xcal` under `calibPath` and calls the three single-path loaders, so **the maps land in the global store, not in the caller's struct**. It sets only the three `*Loaded` boolean flags on `XpeCalibrationState`; the `offsetMap` / `gainMap` / `defectMap` buffer fields are **not required to be filled** and callers must not read them as if they were. Missing files are skipped (that map's flag stays `false`) and the call still returns `XPE_OK`. `xpe_preprocess_pipeline_ex(img, meta, calibState, ...)` correspondingly takes offset and gain from the global store; it consults `calibState` only for a defect map, and passing `NULL` for `calibState` is valid.

**Cached loaders — ownership (normative, 2026-09-10, #127).** `xpe_calib_load_offset_cached` / `xpe_calib_load_gain_cached` / `xpe_calib_load_defect_cached` return a *cache-owned view*: the returned `XpeImageBuffer.data` belongs to the calibration cache on both the hit and the miss path. Callers MUST NOT free it, and it stays valid only until `xpe_calib_cache_clear()`, eviction by `xpe_calib_cache_set_max_size()`, or module shutdown. A caller that needs a longer-lived map takes a copy with `xpe_copy_image`. (The miss path did not follow this rule before QA-A-22.)

### 6.1 xpe_offset_correct

```c
XPE_API XpeErrorCode xpe_offset_correct(const XpeImageBuffer* input,
                                        XpeImageBuffer* output,
                                        const XpeImageMetadata* metadata);
```

**Description**: Applies `I_offset = max(I_raw - I_dark, 0)` using the offset map held in the module-global calibration store (loaded beforehand by `xpe_calib_load_offset`). `input` is `XPE_PIXEL_UINT16`; `output` is written as `XPE_PIXEL_UINT16` with `bitsAllocated`/`bitsStored` set to 16 and `dataSize` set to `width × height × 2`. `output` must be a caller-allocated buffer of the same dimensions as `input`; the store's map must match those dimensions too. In-place operation (`input == output`) is not part of the contract — pass distinct buffers. `metadata` is required (not optional) and supplies temperature and acquisition time for temperature interpolation and PREP-time decay.  
**SRS**: SRS-CALIB-001  
**Thread safety**: Reentrant; reads the global calibration store under the module mutex.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_UNSUPPORTED_FORMAT`, `XPE_ERR_BUFFER_TOO_SMALL`, `XPE_ERR_NOT_INITIALIZED`, `XPE_ERR_CALIB_NOT_LOADED`

---

### 6.2 xpe_gain_correct

```c
XPE_API XpeErrorCode xpe_gain_correct(const XpeImageBuffer* input,
                                      XpeImageBuffer* output,
                                      const XpeImageMetadata* metadata);
```

**Description**: Applies per-pixel flat-field gain correction using the gain map held in the module-global calibration store (loaded beforehand by `xpe_calib_load_gain`), converting `XPE_PIXEL_UINT16` input to `XPE_PIXEL_FLOAT32` output. `output` must be a caller-allocated buffer of the same dimensions as `input`; pass distinct buffers. `metadata` is required and supplies kVp and SID for multi-SID gain interpolation.    
**Gain map convention (normative, 2026-09-10, #120/QA-A-21).** The stored gain map is the *normalized sensitivity* `S(x,y) = (I_flat − I_dark)(x,y) / mean(I_flat − I_dark)` (mean ≈ 1; `xpe_calib_generate_gain` writes exactly this). Correction divides by it: `out = in / S` — the implementation precomputes `1/S` and multiplies. SPEC-XPE-P1A REQ-P1A-011 names the same quantity the other way round (`G = mean/(I_flat − I_dark) = 1/S`, "multiply by G"); the two statements are equivalent, and a test that multiplies the input by the *stored* map is wrong. A stored value outside `(0, MIN..MAX]` (zero, negative, non-finite) is rejected with `XPE_ERR_CONFIG_INVALID` before any pixel is touched.
**SRS**: SRS-CALIB-002  
**Thread safety**: Reentrant; reads the global calibration store under the module mutex.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_UNSUPPORTED_FORMAT`, `XPE_ERR_BUFFER_TOO_SMALL`, `XPE_ERR_CONFIG_INVALID`, `XPE_ERR_NOT_INITIALIZED`, `XPE_ERR_CALIB_NOT_LOADED`

---

### 6.3 xpe_defect_correct

```c
XPE_API XpeErrorCode xpe_defect_correct(const XpeImageBuffer* input,
                                        XpeImageBuffer* output,
                                        const XpeImageMetadata* metadata);
```

**Description**: Replaces defective pixels using the defect map (BPM) held in the module-global calibration store. Isolated defects (no 4-connected defective neighbour) take the unweighted mean of their valid N/S/E/W neighbours, falling back to the nearest valid pixels in Chebyshev rings of radius 1–3 when all four are defective; clustered defects (2+ 4-connected) take the median of the valid pixels in their 3×3 neighbourhood, centre and other defects excluded. Edge and corner pixels use in-bounds neighbours only. (Wording corrected 2026-09-10 per #125 from a reading of `defect_correct.cpp:164-186` / `helpers.cpp:18-53`; the previous "5×5 edge-aware bilinear" text matched no kernel.)
**SRS**: SRS-CALIB-003, SRS-CALIB-004  
**Thread safety**: Reentrant; reads the global calibration store under the module mutex.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_BUFFER_TOO_SMALL`, `XPE_ERR_NOT_INITIALIZED`, `XPE_ERR_CALIB_NOT_LOADED`

---

### 6.4 xpe_defect_detect_runtime

```c
XPE_API XpeErrorCode xpe_defect_detect_runtime(const XpeImageBuffer* img,
                                                XpeImageBuffer* defectMapOut,
                                                const char* configJsonOrNull);
```

**Description**: Detects transient defect pixels in `img` at acquisition time and writes a boolean defect map to `defectMapOut` (pre-allocated, same dimensions). Complements the static calibration defect map.  
**SRS**: SRS-CALIB-005  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_PROCESSING_FAILED`

---

### 6.5 xpe_ghost_create

```c
XPE_API XpeErrorCode xpe_ghost_create(uint32_t width, uint32_t height,
                                       const char* configJsonOrNull,
                                       void** handleOut);
```

**Description**: Creates an opaque ghost corrector handle for the given image dimensions. The corrector accumulates history across calls to `xpe_ghost_correct`. Store and reuse the handle across frames.  
**SRS**: SRS-GHOST-001, SRS-GHOST-002  
**Thread safety**: Reentrant (each handle is independent).  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_OUT_OF_MEMORY`, `XPE_ERR_CONFIG_INVALID`

---

### 6.6 xpe_ghost_correct

```c
XPE_API XpeErrorCode xpe_ghost_correct(void* handle, XpeImageBuffer* img,
                                        const XpeImageMetadata* meta);
```

**Description**: Applies ghost (lag) correction to `img` in-place using the history accumulated in `handle`. Updates internal state for subsequent frames.  
**SRS**: SRS-GHOST-003, SRS-GHOST-004  
**Thread safety**: Reentrant per handle (do not share a single handle across threads).  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_PROCESSING_FAILED`

---

### 6.7 xpe_ghost_reset

```c
XPE_API XpeErrorCode xpe_ghost_reset(void* handle);
```

**Description**: Clears the lag history in `handle` without destroying it. Call between patient acquisitions or after a detector power cycle.  
**SRS**: SRS-GHOST-005  
**Thread safety**: Reentrant per handle.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`

---

### 6.8 xpe_ghost_destroy

```c
XPE_API void xpe_ghost_destroy(void* handle);
```

**Description**: Frees all resources associated with a ghost corrector handle created by `xpe_ghost_create`. After this call `handle` is invalid.  
**SRS**: SRS-GHOST-006  
**Thread safety**: Not thread-safe; ensure no concurrent use of `handle` at destroy time.  
**Error codes**: (void)

---

### 6.9 xpe_calib_load_offset

```c
XPE_API XpeErrorCode xpe_calib_load_offset(const char* filepath);
```

**Description**: Loads an offset (dark) calibration map in XCal format from `filepath` into the module-global calibration store (see "Calibration state model"). The map is validated on load: SHA-256 integrity, session matching, and expiry. There is no output-buffer parameter — the loaded map is subsequently used by `xpe_offset_correct` and by the pipeline entry points. A separate LRU-cached variant, `xpe_calib_load_offset_cached(filePath, offsetMapOut)`, does return the map to the caller.  
**SRS**: SRS-CALIB-010  
**Thread safety**: Reentrant; writes the global calibration store under the module mutex.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_NOT_INITIALIZED`, `XPE_ERR_IO_FAILED`, `XPE_ERR_CALIBRATION_EXPIRED`, `XPE_ERR_CONFIG_INVALID` (session mismatch)

---

### 6.10 xpe_calib_load_gain

```c
XPE_API XpeErrorCode xpe_calib_load_gain(const char* filepath);
```

**Description**: Loads a flat-field gain calibration map in XCal format from `filepath` into the module-global calibration store, together with its interpolation table for kVp-specific gain. There is no output-buffer parameter; the LRU-cached variant `xpe_calib_load_gain_cached(filePath, gainMapOut)` returns the map to the caller.  
**SRS**: SRS-CALIB-011  
**Thread safety**: Reentrant; writes the global calibration store under the module mutex.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_NOT_INITIALIZED`, `XPE_ERR_IO_FAILED`, `XPE_ERR_CALIBRATION_EXPIRED`

---

### 6.11 xpe_calib_load_defect_map

```c
XPE_API XpeErrorCode xpe_calib_load_defect_map(const char* filepath);
```

**Description**: Loads a static defect pixel map (BPM) in XCal format from `filepath` into the module-global calibration store; defect locations and file integrity are validated on load. Map pixels are non-zero where defects exist. There is no output-buffer parameter; the LRU-cached variant `xpe_calib_load_defect_cached(filePath, defectMapOut)` returns the map to the caller.  
**SRS**: SRS-CALIB-012  
**Thread safety**: Reentrant; writes the global calibration store under the module mutex.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_NOT_INITIALIZED`, `XPE_ERR_IO_FAILED`

---

### 6.12 xpe_calib_generate_offset

```c
XPE_API XpeErrorCode xpe_calib_generate_offset(const XpeImageBuffer* frames,
                                                uint32_t frameCount,
                                                XpeImageBuffer* offsetMapOut,
                                                const char* configJsonOrNull);
```

**Description**: Averages `frameCount` dark-field `frames` to generate an offset calibration map in `offsetMapOut`. `frames` is a contiguous array of `XpeImageBuffer` structs.  
**SRS**: SRS-CALIB-020  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_OUT_OF_MEMORY`, `XPE_ERR_PROCESSING_FAILED`

---

### 6.13 xpe_calib_check_expiry

```c
XPE_API XpeErrorCode xpe_calib_check_expiry(const char* filePath,
                                             uint64_t* expiryEpochMsOut);
```

**Description**: Reads the expiry timestamp embedded in the calibration file at `filePath` and writes it to `*expiryEpochMsOut` (UNIX epoch milliseconds). Returns `XPE_ERR_CALIBRATION_EXPIRED` if the timestamp is in the past.  
**SRS**: SRS-CALIB-030, SRS-SAFE-010  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_CALIBRATION_EXPIRED`, `XPE_ERR_IO_FAILED`, `XPE_ERR_INVALID_INPUT`

---

### 6.14 xpe_calib_save

```c
XPE_API XpeErrorCode xpe_calib_save(const char* filePath,
                                     const char* calibType,
                                     uint64_t expiryEpochMs);
```

**Description**: Writes the calibration map of `calibType` (`"offset"`, `"gain"`, `"defect"`) held in the global calibration store (§6 state model, #117) to `filePath` as XCal with SHA-256 integrity and the embedded expiry `expiryEpochMs` (Unix milliseconds; `0` = never expires, which `xpe_calib_check_expiry` reports as not expired). Returns `XPE_ERR_CALIB_NOT_LOADED` when that map is not loaded. **Decision #132 (2026-09-10):** the previous two-argument form `(filePath, calibType)` always wrote `expiry_epoch_ms = 0`, so no API path could produce an expiring file and SRS-ALERT-005 / REQ-P1A-018 were unreachable through the public API; the third parameter restores the requirement. Implemented by QA-A-29.  
**SRS**: SRS-CALIB-021, SRS-ALERT-005  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_CALIB_NOT_LOADED`, `XPE_ERR_IO_FAILED`

---

### 6.15 xpe_validate_readout_artifact

```c
XPE_API XpeErrorCode xpe_validate_readout_artifact(const XpeImageBuffer* image,
                                                    const XpeImageMetadata* metadata,
                                                    bool* has_dropped_columns,
                                                    bool* has_nonuniform_gain);
```

**Description**: Validates the raw uint16 readout frame before any correction stage. Sets `*has_dropped_columns` when any column is all-zero and `*has_nonuniform_gain` when any row mean exceeds 0.9 x UINT16_MAX (ADC saturation pattern). Both outputs are plain flags; there is no artifact score or message. All four pointers are required. **Corrected 2026-09-10 (QA-A-25):** this section previously documented a `(rawImg, int32_t* artifactScoreOut, char* msgOut, size_t msgLen)` form that never existed in the header (`preprocess_api.h`), the implementation, the pipeline caller, or the GUI export list; the flag form is canonical. Line-noise detection named in REQ-P1A-041 is not implemented by this function (open gap, tracked separately).
**Traceability**: PRE-01, SRS-PERF-001  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT` (NULL pointer or non-uint16 buffer)

---

### 6.16 xpe_temp_compensate

```c
XPE_API XpeErrorCode xpe_temp_compensate(XpeImageBuffer* img,
                                          float detectorTempC,
                                          const char* configJsonOrNull);
```

**Description**: Applies detector temperature compensation to `img` in-place using LUT or polynomial coefficients selected by `configJsonOrNull`. The caller passes the current detector temperature in Celsius.  
**Traceability**: PRE-07, SRS-PERF-001  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_CONFIG_INVALID`, `XPE_ERR_PROCESSING_FAILED`

---

### 6.17 xpe_nonlinearity_correct

```c
XPE_API XpeErrorCode xpe_nonlinearity_correct(XpeImageBuffer* img,
                                               const char* configJsonOrNull);
```

**Description**: Corrects detector response nonlinearity in `img` in-place using pre-characterized calibration coefficients. `configJsonOrNull` selects detector mode and coefficient set.  
**Traceability**: PRE-08, SRS-PERF-001  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_CONFIG_INVALID`, `XPE_ERR_PROCESSING_FAILED`

---

### 6.18 xpe_binning_correct

```c
XPE_API XpeErrorCode xpe_binning_correct(XpeImageBuffer* img,
                                          int32_t binningMode,
                                          const char* configJsonOrNull);
```

**Description**: Applies per-mode correction for binned acquisition data in `img` in-place. `binningMode` is detector-defined (for example 1=`1x1`, 2=`2x2`) and must match the loaded calibration profile.  
**Traceability**: PRE-09, SRS-PERF-001  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_CONFIG_INVALID`, `XPE_ERR_PROCESSING_FAILED`

---

## 7. xpe_enhance_basic.dll

Provides fundamental image enhancement operations: logarithmic transforms, noise reduction, contrast, and edge enhancement.

Dependencies: xpe_common.dll.

### 7.1 xpe_log_transform

```c
XPE_API XpeErrorCode xpe_log_transform(XpeImageBuffer* img,
                                        const char* configJsonOrNull);
```

**Description**: Applies a logarithmic intensity transform to `img` in-place, compressing the dynamic range to approximate film-screen response. `configJsonOrNull` may specify base and offset.  
**SRS**: SRS-ENH-001  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_UNSUPPORTED_FORMAT`

---

### 7.2 xpe_log_inverse

```c
XPE_API XpeErrorCode xpe_log_inverse(XpeImageBuffer* img,
                                      const char* configJsonOrNull);
```

**Description**: Applies the inverse (exponential) of the log transform to `img` in-place, restoring linear intensity values. Parameters must match the forward transform.  
**SRS**: SRS-ENH-002  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_UNSUPPORTED_FORMAT`

---

### 7.3 xpe_noise_reduce

```c
XPE_API XpeErrorCode xpe_noise_reduce(XpeImageBuffer* img,
                                       const char* configJsonOrNull);
```

**Description**: Reduces quantum noise in `img` in-place using adaptive filtering (bilateral or non-local means, selectable via config). Strength and kernel size are configurable.  
**SRS**: SRS-ENH-010, SRS-ENH-011  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_CONFIG_INVALID`, `XPE_ERR_PROCESSING_FAILED`

---

### 7.4 xpe_noise_estimate_sigma

```c
XPE_API XpeErrorCode xpe_noise_estimate_sigma(const XpeImageBuffer* img,
                                               float* sigmaOut);
```

**Description**: Estimates the standard deviation of additive noise in `img` using a wavelet-based estimator and writes the result to `*sigmaOut`. Non-destructive.  
**SRS**: SRS-ENH-012  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_PROCESSING_FAILED`

---

### 7.5 xpe_contrast_enhance

```c
XPE_API XpeErrorCode xpe_contrast_enhance(XpeImageBuffer* img,
                                           const char* configJsonOrNull);
```

**Description**: Applies Contrast Limited Adaptive Histogram Equalization (CLAHE) or similar technique to `img` in-place. Clip limit, tile size, and method are configurable.  
**SRS**: SRS-ENH-020  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_CONFIG_INVALID`, `XPE_ERR_PROCESSING_FAILED`

---

### 7.6 xpe_edge_enhance

```c
XPE_API XpeErrorCode xpe_edge_enhance(XpeImageBuffer* img,
                                       const char* configJsonOrNull);
```

**Description**: Sharpens edges in `img` in-place via unsharp masking or Laplacian enhancement. Strength and radius are configurable.  
**SRS**: SRS-ENH-021  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_CONFIG_INVALID`

---

### 7.7 xpe_calc_exposure_index

```c
XPE_API XpeErrorCode xpe_calc_exposure_index(const XpeImageBuffer* img,
                                              const XpeImageMetadata* meta,
                                              float* eiOut,
                                              float* deviationIndexOut);
```

**Description**: Calculates the IEC 62494 Exposure Index (EI) and Deviation Index (DI) for a detector-domain, pre-presentation image, writing results to `*eiOut` and `*deviationIndexOut`. Whole-image EI is always supported; when a valid collimation ROI sidecar is available, the relevant image region may be restricted to that ROI by the caller. Exam/view metadata selects the primary `EIT`; `meta->bodyPart` may refine defaults when available. Stitched or multi-irradiation images are non-normative inputs and should be rejected or explicitly flagged by the caller.

**Phase assignment**: This function is implemented in xpe_enhance_basic.dll (Phase 1b). In Phase 2, the orchestrator re-invokes this function with a collimation ROI-cropped image for ROI-aware EI refinement. No separate API is needed for ROI refinement.

**SRS**: SRS-ADV-030, SRS-SAFE-016, SRS-EI-001  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_PROCESSING_FAILED`


### 7.8 xpe_enhance_basic_version

```c
XPE_API const char* xpe_enhance_basic_version(void);
```

**Description**: Returns a pointer to a static, null-terminated version string for `xpe_enhance_basic.dll`. The returned buffer is DLL-owned; do NOT free it.  
**SRS**: SRS-VER-001  
**Thread safety**: Thread-safe (read-only static storage).  
**Error codes**: N/A (returns a pointer, never NULL)

---

## 8. xpe_enhance_advanced.dll

Provides multi-scale frequency processing, fractional calculus enhancement, and collimation detection.

Dependencies: xpe_common.dll.

Execution order (`xpe_log_transform` before advanced enhancement) is enforced by the caller/orchestrator, not by a DLL-to-DLL dependency.

**Note**: `xpe_calc_exposure_index` belongs to `xpe_enhance_basic.dll` per SPEC-XPE-MASTER v2.1.0. Phase 2 ROI-aware EI refinement is performed by the orchestrator re-invoking that function with a collimation ROI-cropped image.  

### 8.1 xpe_multiscale_process

```c
XPE_API XpeErrorCode xpe_multiscale_process(XpeImageBuffer* img,
                                             const XpeImageMetadata* meta,
                                             const char* configJsonOrNull);
```

**Description**: Decomposes `img` into frequency sub-bands using a multi-scale framework (e.g., Laplacian pyramid), applies per-band enhancement coefficients derived from `meta`, and reconstructs in-place.  
**SRS**: SRS-ADV-001, SRS-ADV-002  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_CONFIG_INVALID`, `XPE_ERR_PROCESSING_FAILED`

---

### 8.2 xpe_fractional_process

```c
XPE_API XpeErrorCode xpe_fractional_process(XpeImageBuffer* img,
                                             float order,
                                             const char* configJsonOrNull);
```

**Description**: Applies a fractional-order differentiation operator of degree `order` (0.0-2.0) to `img` in-place. Values near 1.0 preserve edges; values near 2.0 emphasise fine texture.  
**SRS**: SRS-ADV-010  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT` (order out of range), `XPE_ERR_PROCESSING_FAILED`

---

### 8.3 xpe_detect_collimation

```c
XPE_API XpeErrorCode xpe_detect_collimation(const XpeImageBuffer* img,
                                             int32_t* x0Out, int32_t* y0Out,
                                             int32_t* x1Out, int32_t* y1Out,
                                             const char* configJsonOrNull);
```

**Description**: Detects the collimation boundary (primary beam edge) in `img` and writes the bounding rectangle as `(x0, y0) - (x1, y1)` in pixel coordinates. Non-destructive.  
**SRS**: SRS-ADV-020, SRS-SAFE-015  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_PROCESSING_FAILED`

---

## 9. xpe_ai.dll

Provides deep-learning inference: body part recognition, stitching, bone suppression, and DL-based denoising.

Dependencies: xpe_common.dll. GPU via ONNX Runtime / TensorRT (optional).

Execution model: `xpe_ai.dll` is an in-process C ABI proxy. Actual inference executes in a sandboxed companion worker process (`xpe_ai_worker.exe`) over IPC. Worker launch, heartbeat, and crash isolation are handled inside `xpe_ai_init` / `xpe_ai_shutdown`.

### 9.1 xpe_ai_init

```c
XPE_API XpeErrorCode xpe_ai_init(const char* modelDirPath,
                                  const char* configJsonOrNull);
```

**Description**: Launches or attaches to the sandboxed AI worker, loads model files from `modelDirPath`, and initialises the worker-side inference runtime. Must be called before any other xpe_ai function. `configJsonOrNull` selects device (CPU/CUDA), IPC timeout, and batch settings.  
**SRS**: SRS-AI-001, SRS-AI-002  
**Thread safety**: Not thread-safe ;call from a single thread at startup.  
**Error codes**: `XPE_OK`, `XPE_ERR_IO_FAILED`, `XPE_ERR_CONFIG_INVALID`, `XPE_ERR_OUT_OF_MEMORY`

---

### 9.2 xpe_ai_shutdown

```c
XPE_API void xpe_ai_shutdown(void);
```

**Description**: Stops the sandboxed AI worker session, unloads models, and releases IPC resources. No xpe_ai function may be called after this.  
**SRS**: SRS-AI-003  
**Thread safety**: Not thread-safe ;call from a single thread at shutdown.  
**Error codes**: (void)

---

### 9.3 xpe_bodypart_recognize

```c
XPE_API XpeErrorCode xpe_bodypart_recognize(const XpeImageBuffer* img,
                                             char* bodyPartOut, size_t bufLen,
                                             float* confidenceOut);
```

**Description**: Classifies the anatomical body part in `img` using a CNN classifier. Writes the label (e.g., `"CHEST"`) to `bodyPartOut` and the confidence score [0,1] to `*confidenceOut`.  
**SRS**: SRS-AI-010  
**Thread safety**: Reentrant (thread-safe inference session per call).  
**Error codes**: `XPE_OK`, `XPE_ERR_NOT_INITIALIZED`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_BUFFER_TOO_SMALL`, `XPE_ERR_PROCESSING_FAILED`

---

### 9.4 xpe_stitch_images

```c
XPE_API XpeErrorCode xpe_stitch_images(const XpeImageBuffer* parts,
                                        uint32_t partCount,
                                        XpeImageBuffer* stitchedOut,
                                        const char* configJsonOrNull);
```

**Description**: Stitches `partCount` overlapping partial images from the `parts` array into a single wide-field image in `stitchedOut` (pre-allocated via `xpe_stitch_estimate_size`). Alignment uses AI-based feature matching.  
**SRS**: SRS-AI-020, SRS-AI-021  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_PROCESSING_FAILED`, `XPE_ERR_BUFFER_TOO_SMALL`

---

### 9.5 xpe_stitch_estimate_size

```c
XPE_API XpeErrorCode xpe_stitch_estimate_size(const XpeImageBuffer* parts,
                                               uint32_t partCount,
                                               uint32_t* widthOut,
                                               uint32_t* heightOut);
```

**Description**: Estimates the output dimensions of a stitch operation without performing stitching. Use the returned dimensions to pre-allocate the buffer for `xpe_stitch_images`.  
**SRS**: SRS-AI-020  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_PROCESSING_FAILED`

---

### 9.6 xpe_bone_suppress

```c
XPE_API XpeErrorCode xpe_bone_suppress(const XpeImageBuffer* img,
                                        XpeImageBuffer* softTissueOut,
                                        const char* configJsonOrNull);
```

**Description**: Produces a soft-tissue-only image in `softTissueOut` by suppressing bony structures using a U-Net style model. `softTissueOut` must be pre-allocated with the same dimensions as `img`.  
**SRS**: SRS-AI-030  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_NOT_INITIALIZED`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_PROCESSING_FAILED`

---

### 9.7 xpe_dl_denoise

```c
XPE_API XpeErrorCode xpe_dl_denoise(XpeImageBuffer* img,
                                     const XpeImageMetadata* meta,
                                     const char* configJsonOrNull);
```

**Description**: Applies a deep-learning denoising network to `img` in-place. Selects model variant based on `meta->bodyPart` and `meta->mAs`. Complements (and may replace) the classical `xpe_noise_reduce`.  
**SRS**: SRS-AI-040  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_NOT_INITIALIZED`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_PROCESSING_FAILED`

---

## 10. xpe_display.dll

Provides DICOM-standard LUT pipeline: Modality LUT, VOI LUT, Presentation LUT, preset management, and auto-selection.

Dependencies: xpe_common.dll.

### 10.1 xpe_modality_lut_apply

```c
XPE_API XpeErrorCode xpe_modality_lut_apply(XpeImageBuffer* img,
                                             float rescaleSlope,
                                             float rescaleIntercept);
```

**Description**: Applies the DICOM Modality LUT linear transformation `output = input * rescaleSlope + rescaleIntercept` to all pixels of `img` in-place.  
**SRS**: SRS-DISP-001  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`

---

### 10.2 xpe_voi_lut_apply

```c
XPE_API XpeErrorCode xpe_voi_lut_apply(XpeImageBuffer* img,
                                        float windowCenter,
                                        float windowWidth,
                                        int32_t function);
```

**Description**: Applies a VOI LUT windowing operation to `img` in-place. `function` selects: 0=LINEAR, 1=LINEAR_EXACT, 2=SIGMOID per DICOM PS 3.3 C.7.6.3.1.5.  
**SRS**: SRS-DISP-010, SRS-DISP-011  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`

---

### 10.3 xpe_voi_lut_apply_fast

```c
XPE_API XpeErrorCode xpe_voi_lut_apply_fast(XpeImageBuffer* img,
                                              float windowCenter,
                                              float windowWidth,
                                              uint8_t* lut8bit,
                                              size_t lutLen);
```

**Description**: Applies a pre-computed 8-bit output LUT to `img` for real-time display (e.g., panning / scrolling). `lut8bit` is a 65536-entry lookup table from 16-bit input to 8-bit output.  
**SRS**: SRS-DISP-012, SRS-PERF-005  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_BUFFER_TOO_SMALL`

---

### 10.4 xpe_voi_lut_apply_sequence

```c
XPE_API XpeErrorCode xpe_voi_lut_apply_sequence(XpeImageBuffer* imgs,
                                                  uint32_t imgCount,
                                                  float windowCenter,
                                                  float windowWidth,
                                                  int32_t function);
```

**Description**: Batch-applies the same VOI LUT windowing to an array of `imgCount` images for consistent series display. Equivalent to calling `xpe_voi_lut_apply` on each image but more efficient.  
**SRS**: SRS-DISP-013  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`

---

### 10.5 xpe_presentation_lut_apply

```c
XPE_API XpeErrorCode xpe_presentation_lut_apply(XpeImageBuffer* img,
                                                  const char* presetNameOrNull,
                                                  const char* configJsonOrNull);
```

**Description**: Applies a Presentation LUT (gamma / perceptual linearisation) to `img` in-place. `presetNameOrNull` selects a named preset; pass `NULL` for sRGB default.  
**SRS**: SRS-DISP-020  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_CONFIG_INVALID`

---

### 10.6 xpe_presentation_lut_check_display

```c
XPE_API XpeErrorCode xpe_presentation_lut_check_display(float* gsdfComplianceOut);
```

**Description**: Measures the attached display's luminance response against the DICOM GSDF (Grayscale Standard Display Function) and writes a compliance score [0,1] to `*gsdfComplianceOut`.  
**SRS**: SRS-DISP-021, SRS-SAFE-020  
**Thread safety**: Thread-safe.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_PROCESSING_FAILED`

---

### 10.7 xpe_lut_get_preset_count

```c
XPE_API int32_t xpe_lut_get_preset_count(void);
```

**Description**: Returns the total number of available LUT presets (built-in plus custom). Returns a negative value on internal error.  
**SRS**: SRS-DISP-030  
**Thread safety**: Thread-safe.  
**Error codes**: (returns count; negative = error)

---

### 10.8 xpe_lut_get_preset

```c
XPE_API XpeErrorCode xpe_lut_get_preset(int32_t index,
                                         char* nameOut, size_t nameBufLen,
                                         char* descriptionOut, size_t descBufLen);
```

**Description**: Copies the name and description of the LUT preset at `index` (zero-based) into the caller-supplied buffers.  
**SRS**: SRS-DISP-031  
**Thread safety**: Thread-safe.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_BUFFER_TOO_SMALL`

---

### 10.9 xpe_lut_add_custom_preset

```c
XPE_API XpeErrorCode xpe_lut_add_custom_preset(const char* name,
                                                 const char* description,
                                                 const char* lutDefinitionJson);
```

**Description**: Registers a new custom LUT preset from a JSON definition string. The preset is persisted to the user preset store and immediately available for selection.  
**SRS**: SRS-DISP-032  
**Thread safety**: Not thread-safe; serialize preset modifications.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_CONFIG_INVALID`, `XPE_ERR_IO_FAILED`

---

### 10.10 xpe_lut_remove_custom_preset

```c
XPE_API XpeErrorCode xpe_lut_remove_custom_preset(const char* name);
```

**Description**: Removes a custom LUT preset by name. Built-in presets cannot be removed (returns `XPE_ERR_INVALID_INPUT`).  
**SRS**: SRS-DISP-033  
**Thread safety**: Not thread-safe; serialize preset modifications.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_IO_FAILED`

---

### 10.11 xpe_lut_auto_select

```c
XPE_API XpeErrorCode xpe_lut_auto_select(const XpeImageMetadata* meta,
                                           char* presetNameOut, size_t bufLen);
```

**Description**: Selects the recommended LUT preset for the given image metadata (body part, modality, acquisition parameters) and writes the preset name to `presetNameOut`.  
**SRS**: SRS-DISP-040  
**Thread safety**: Thread-safe.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_BUFFER_TOO_SMALL`

---

## 11. xpe_dicom.dll

Provides DICOM file I/O, tag manipulation, GSPS annotation, and network services (C-STORE / C-FIND MWL).

Dependencies: xpe_common.dll.

### 11.1 xpe_dicom_read

```c
XPE_API XpeErrorCode xpe_dicom_read(const char* filePath,
                                     XpeImageBuffer* imgOut,
                                     XpeImageMetadata* metaOut);
```

**Description**: Reads a DICOM file from `filePath`, decodes the pixel data into `imgOut` (caller pre-allocates or passes zeroed struct for auto-allocation), and fills `metaOut` with key attributes.  
**SRS**: SRS-DICOM-001, SRS-DICOM-002  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_IO_FAILED`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_UNSUPPORTED_FORMAT`, `XPE_ERR_OUT_OF_MEMORY`

---

### 11.2 xpe_dicom_query_dimensions

```c
XPE_API XpeErrorCode xpe_dicom_query_dimensions(const char* filePath,
                                                 uint32_t* widthOut,
                                                 uint32_t* heightOut,
                                                 XpePixelFormat* formatOut);
```

**Description**: Reads only the image dimension and format tags from `filePath` without decoding pixel data. Use to pre-allocate the buffer before calling `xpe_dicom_read`.  
**SRS**: SRS-DICOM-003  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_IO_FAILED`, `XPE_ERR_INVALID_INPUT`

---

### 11.3 xpe_dicom_read_tag_string

```c
XPE_API XpeErrorCode xpe_dicom_read_tag_string(const char* filePath,
                                                uint16_t group, uint16_t element,
                                                char* valueOut, size_t bufLen);
```

**Description**: Reads a single DICOM tag identified by `(group, element)` from `filePath` and writes its string representation to `valueOut`. Supports VRs: LO, LT, SH, ST, UI, UN, UT.  
**SRS**: SRS-DICOM-004  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_IO_FAILED`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_BUFFER_TOO_SMALL`

---

### 11.4 xpe_dicom_write

```c
XPE_API XpeErrorCode xpe_dicom_write(const char* filePath,
                                      const XpeImageBuffer* img,
                                      const XpeImageMetadata* meta,
                                      const char* configJsonOrNull);
```

**Description**: Encodes `img` and `meta` into a DICOM Part 10 file at `filePath`. `configJsonOrNull` may specify transfer syntax (Explicit Little Endian, JPEG 2000 Lossless, etc.).  
**SRS**: SRS-DICOM-010, SRS-DICOM-011  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_IO_FAILED`, `XPE_ERR_CONFIG_INVALID`

---

### 11.5 xpe_dicom_write_j2k

```c
XPE_API XpeErrorCode xpe_dicom_write_j2k(const char* filePath,
                                           const XpeImageBuffer* img,
                                           const XpeImageMetadata* meta,
                                           float compressionRatio);
```

**Description**: Writes a DICOM file with JPEG 2000 compressed pixel data at the specified `compressionRatio` (1.0 = lossless). Convenience wrapper for `xpe_dicom_write` with J2K transfer syntax.  
**SRS**: SRS-DICOM-012  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_IO_FAILED`, `XPE_ERR_PROCESSING_FAILED`

---

### 11.6 xpe_dicom_set_tag_string

```c
XPE_API XpeErrorCode xpe_dicom_set_tag_string(const char* filePath,
                                               uint16_t group, uint16_t element,
                                               const char* value);
```

**Description**: Updates or inserts a string-valued DICOM tag in an existing file at `filePath`. The file is modified in-place; a backup is not created.  
**SRS**: SRS-DICOM-005  
**Thread safety**: Not thread-safe per file path; serialize modifications to the same file.  
**Error codes**: `XPE_OK`, `XPE_ERR_IO_FAILED`, `XPE_ERR_INVALID_INPUT`

---

### 11.7 xpe_gsps_create

```c
XPE_API XpeErrorCode xpe_gsps_create(const char* referencedFilePath,
                                      const char* annotationJson,
                                      const char* gspsFilePathOut,
                                      size_t gspsPathBufLen);
```

**Description**: Creates a DICOM Grayscale Softcopy Presentation State (GSPS) object referencing `referencedFilePath` and embedding annotations from `annotationJson` (ROI, overlay, measurement). Writes the GSPS file path to `gspsFilePathOut`.  
**SRS**: SRS-DICOM-020  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_IO_FAILED`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_CONFIG_INVALID`, `XPE_ERR_BUFFER_TOO_SMALL`

---

### 11.8 xpe_gsps_apply

```c
XPE_API XpeErrorCode xpe_gsps_apply(const char* gspsFilePath,
                                     XpeImageBuffer* img,
                                     const char* configJsonOrNull);
```

**Description**: Renders the annotations from a GSPS file onto `img` in-place (burns-in overlays). Useful for secondary capture or print output.  
**SRS**: SRS-DICOM-021  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_IO_FAILED`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_PROCESSING_FAILED`

---

### 11.9 xpe_dicom_cstore

```c
XPE_API XpeErrorCode xpe_dicom_cstore(const char* filePath,
                                       const char* remoteAeTitle,
                                       const char* remoteHost,
                                       uint16_t    remotePort,
                                       const char* localAeTitle);
```

**Description**: Sends a DICOM file to a remote SCP via C-STORE. Blocks until the SCP returns a status response or a timeout occurs (configurable via `xpe_configure`).  
**SRS**: SRS-DICOM-030  
**Thread safety**: Reentrant (each call uses an independent DICOM association).  
**Error codes**: `XPE_OK`, `XPE_ERR_NETWORK_FAILED`, `XPE_ERR_IO_FAILED`, `XPE_ERR_INVALID_INPUT`

---

### 11.10 xpe_dicom_cfind_mwl

```c
XPE_API XpeErrorCode xpe_dicom_cfind_mwl(const char* queryJson,
                                           const char* remoteAeTitle,
                                           const char* remoteHost,
                                           uint16_t    remotePort,
                                           const char* localAeTitle,
                                           char*       resultsJsonOut,
                                           size_t      resultsBufLen);
```

**Description**: Queries a Modality Worklist SCP using C-FIND. `queryJson` encodes the query keys (Patient ID, Accession Number, etc.). Results are returned as a JSON array of matching worklist items in `resultsJsonOut`.  
**SRS**: SRS-DICOM-031  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_NETWORK_FAILED`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_BUFFER_TOO_SMALL`

---

### 11.11 xpe_dicom_validate

```c
XPE_API XpeErrorCode xpe_dicom_validate(const char* filePath, char* outReportJson, uint32_t reportBufLen);
```

**Description**: Validate a DICOM file for DX IOD conformance and produce a JSON report. **File meta information (PS3.10 group 0002) is required (decision #139, 2026-09-11 / QA-B-36):** the file must carry a Part 10 file meta information group whose content agrees with the dataset. The result is `valid: false` when any of the following holds:

- the file has no meta information group (a bare dataset written without a Part 10 header — DCMTK synthesises an empty `DcmMetaInfo` for such files, so the check is on the group being empty, not on a null pointer);
- `TransferSyntaxUID (0002,0010)` is absent or is not a well-formed UID (presence and format only — the parser trusts the meta transfer syntax to read the dataset, so an encoding comparison cannot detect anything);
- `MediaStorageSOPClassUID (0002,0002)` is absent or differs from the dataset's `SOPClassUID (0008,0016)`;
- `MediaStorageSOPInstanceUID (0002,0003)` is absent or differs from the dataset's `SOPInstanceUID (0008,0018)`.

Each failing condition contributes one entry to the error report, tagged with the group-0002 element it concerns. When a dataset-side tag is itself missing, the missing-Type-1 report covers it and no separate meta mismatch is reported. Files written by `xpe_dicom_write` / `xpe_dicom_write_j2k` satisfy these rules (round-trip verified).  
**SRS**: SRS-IF-002  
**Thread safety**: Reentrant.  
**Error codes**: `XPE_OK`, `XPE_ERR_INVALID_INPUT`, `XPE_ERR_IO_FAILED`

---

## 12. gsvg.dll

Provides anti-scatter grid detection and virtual grid suppression. This independent module does not depend on `xpe_common` types.

### 12.1 gsvg_process

```c
GSVG_API GsvgErrorCode gsvg_process(uint16_t* pixels,
                                     uint32_t  width,
                                     uint32_t  height,
                                     const GsvgConfig* config);
```

**Description**: Suppresses anti-scatter grid artifacts from the raw 16-bit pixel buffer `pixels` (width x height, row-major) in-place using parameters in `config`. Auto-detects grid frequency if `config->gridFrequency_lp_per_mm == 0`.  
**SRS**: SRS-GSVG-001, SRS-GSVG-002, GSVG-SDD-001  
**Thread safety**: Reentrant.  
**Error codes**: `GSVG_OK`, `GSVG_ERR_INVALID_INPUT`, `GSVG_ERR_GRID_NOT_DETECTED`, `GSVG_ERR_PROCESSING_FAILED`

---

### 12.2 gsvg_process_ex

```c
GSVG_API GsvgErrorCode gsvg_process_ex(uint16_t* pixels,
                                        uint32_t  width,
                                        uint32_t  height,
                                        const GsvgConfig* config,
                                        const GsvgImageMetadata* meta,
                                        char* diagnosticJsonOut,
                                        size_t diagnosticBufLen);
```

**Description**: Extended variant of `gsvg_process` that additionally accepts image metadata for body-part-aware tuning and writes a JSON diagnostic report (detected grid parameters, suppression quality metrics) to `diagnosticJsonOut`.  
**SRS**: SRS-GSVG-003, GSVG-SDD-001 Section 4.2  
**Thread safety**: Reentrant.  
**Error codes**: `GSVG_OK`, `GSVG_ERR_INVALID_INPUT`, `GSVG_ERR_GRID_NOT_DETECTED`, `GSVG_ERR_PROCESSING_FAILED`, `GSVG_ERR_BUFFER_TOO_SMALL`

---

### 12.3 gsvg_version

```c
GSVG_API const char* gsvg_version(void);
```

**Description**: Returns a pointer to a static, null-terminated GSVG module version string (e.g., `"2.1.0"`). DLL-owned; do NOT free.  
**SRS**: SRS-GSVG-VER-001  
**Thread safety**: Thread-safe.  
**Error codes**: (never NULL)

---

### 12.4 gsvg_error_string

```c
GSVG_API const char* gsvg_error_string(GsvgErrorCode code);
```

**Description**: Returns a static, human-readable English description for a `GsvgErrorCode`. Unknown codes return `"Unknown GSVG error"`. DLL-owned.  
**SRS**: SRS-GSVG-ERR-001  
**Thread safety**: Thread-safe.  
**Error codes**: (never NULL)

---

### 12.5 gsvg_detect_grid

```c
GSVG_API GsvgErrorCode gsvg_detect_grid(const uint16_t* pixels,
                                          uint32_t width,
                                          uint32_t height,
                                          float pixelPitch_mm,
                                          int32_t* freqOut_lp_per_mm,
                                          float*   angleOut_deg);
```

**Description**: Detects the anti-scatter grid line frequency and orientation angle from the image, writing results to `*freqOut_lp_per_mm` and `*angleOut_deg`. Non-destructive; use results to populate `GsvgConfig` for `gsvg_process`.
**SRS**: SRS-GSVG-010, GSVG-SDD-001 Section 3.1  
**Thread safety**: Reentrant.  
**Error codes**: `GSVG_OK`, `GSVG_ERR_INVALID_INPUT`, `GSVG_ERR_GRID_NOT_DETECTED`

---

### 12.6 gsvg_suppress_grid

```c
GSVG_API GsvgErrorCode gsvg_suppress_grid(uint16_t* pixels,
                                           uint32_t  width,
                                           uint32_t  height,
                                           int32_t   freq_lp_per_mm,
                                           float     angle_deg,
    float    suppressionStrength;     /* 0.0-1.0; suppression aggressiveness */
```

**Description**: Suppresses a known grid at the specified frequency and angle. Separated from `gsvg_process` for scenarios where grid parameters are already known (e.g., from detector metadata).  
**SRS**: SRS-GSVG-011  
**Thread safety**: Reentrant.  
**Error codes**: `GSVG_OK`, `GSVG_ERR_INVALID_INPUT`, `GSVG_ERR_PROCESSING_FAILED`

---

### 12.7 gsvg_virtual_grid

```c
GSVG_API GsvgErrorCode gsvg_virtual_grid(uint16_t* pixels,
                                          uint32_t  width,
                                          uint32_t  height,
                                          const GsvgImageMetadata* meta,
                                          const char* configJsonOrNull);
```

**Description**: Synthesises a virtual grid effect in `pixels` in-place after scatter suppression, improving perceived contrast for images acquired without a physical anti-scatter grid.  
**SRS**: SRS-GSVG-020, GSVG-SDD-001 Section 5  
**Thread safety**: Reentrant.  
**Error codes**: `GSVG_OK`, `GSVG_ERR_INVALID_INPUT`, `GSVG_ERR_CONFIG_INVALID`, `GSVG_ERR_PROCESSING_FAILED`

---

### 12.8 gsvg_load_scatter_lut

```c
GSVG_API GsvgErrorCode gsvg_load_scatter_lut(const char* filePath);
```

**Description**: Loads a scatter correction look-up table from `filePath` into module-internal storage. The LUT improves suppression quality for known detector-grid combinations. Replaces any previously loaded LUT.  
**SRS**: SRS-GSVG-030  
**Thread safety**: Not thread-safe; call before processing begins.  
**Error codes**: `GSVG_OK`, `GSVG_ERR_LUT_LOAD_FAILED`, `GSVG_ERR_INVALID_INPUT`

---

## 13. Appendix A: Error Code Cross-Reference

| Code | Symbol | Applicable DLLs |
|------|--------|-----------------|
| 0 | XPE_OK | All |
| -1 | XPE_ERR_INVALID_INPUT | All |
| -2 | XPE_ERR_OUT_OF_MEMORY | common, preprocess, ai, dicom |
| -3 | XPE_ERR_PROCESSING_FAILED | preprocess, enhance_basic, enhance_advanced, ai, display, dicom, gsvg |
| -4 | XPE_ERR_CONFIG_INVALID | common, preprocess, enhance_basic, enhance_advanced, display, dicom |
| -5 | XPE_ERR_CALIBRATION_EXPIRED | preprocess |
| -6 | XPE_ERR_NOT_INITIALIZED | common, ai |
| -7 | XPE_ERR_UNSUPPORTED_FORMAT | common, enhance_basic, dicom |
| -8 | XPE_ERR_BUFFER_TOO_SMALL | common, display, dicom |
| -9 | XPE_ERR_IO_FAILED | preprocess, dicom |
| -10 | XPE_ERR_NETWORK_FAILED | dicom |

### Error code precedence (normative, 2026-09-09; narrowed same day per #119)

When several error conditions hold at once, every XPE entry point reports errors in this order. Callers and tests must not assume anything stricter (#119).

1. **Required-pointer nullness first.** A NULL required pointer argument yields `XPE_ERR_INVALID_INPUT` before any other check, so an uninitialized module never dereferences caller memory. Optional pointers (`configJsonOrNull`, optional metadata) are not required and do not trigger this rule.
2. **After the null checks the order is implementation-defined** between `XPE_ERR_NOT_INITIALIZED` and content validation (`XPE_ERR_INVALID_INPUT` for zero sizes / out-of-range scalars, `XPE_ERR_UNSUPPORTED_FORMAT`, `XPE_ERR_CONFIG_INVALID`, `XPE_ERR_BUFFER_TOO_SMALL`). Reference implementations differ here (`preprocess` validates format and dimensions before the initialization check; `common` and `enhance_advanced` check initialization first) and both are conforming. A test that wants to observe `XPE_ERR_NOT_INITIALIZED` must pass otherwise-valid, non-NULL arguments; a test that wants a content error must run on an initialized module. `XPE_ERR_CALIB_NOT_LOADED` belongs to this class: on an initialized `preprocess` module whose required calibration map has not been loaded, it may be reported before or after the other class-2 content errors, and a test that wants to observe it must pass otherwise-valid, non-NULL arguments with matching dimensions.
3. Processing errors — `XPE_ERR_PROCESSING_FAILED`, `XPE_ERR_IO_FAILED`, `XPE_ERR_OUT_OF_MEMORY` — are reported only after 1 and 2 pass.

Handle-based modules (`xpe_gsvg`, `xpe_dicom`) carry their state in the handle rather than in a module-global flag: a NULL handle is a NULL required pointer (`XPE_ERR_INVALID_INPUT`, rule 1) and these modules never return `XPE_ERR_NOT_INITIALIZED`. Using a handle after its `*_shutdown()` / `*_close()` is undefined behaviour and is not detected (#119, QA-B-14).

`configJsonOrNull` parameters: `NULL` selects defaults; an empty string `""` is not valid JSON and yields `XPE_ERR_CONFIG_INVALID` (class 3).


GSVG error codes are separate and defined in Section 3.

---

## 14. Appendix B: DLL Dependency Graph

```
gsvg.dll          (independent)
xpe_common.dll    (base; no XPE dependencies)
xpe_preprocess.dll  -> xpe_common.dll
xpe_enhance_basic.dll  -> xpe_common.dll
xpe_enhance_advanced.dll -> xpe_common.dll
xpe_ai.dll        -> xpe_common.dll (+ xpe_ai_worker.exe via IPC)
xpe_display.dll   -> xpe_common.dll
xpe_dicom.dll     -> xpe_common.dll
```

Load order: `xpe_common.dll` must be loaded before any other XPE DLL. `xpe_ai_worker.exe` is launched on demand by `xpe_ai_init`. `gsvg.dll` may be loaded at any time independently.

---

## 15. Appendix C: Path Management Design

### Overview: Explicit-Path API Pattern

XPE uses an **explicit-path API pattern**: all file and directory operations require the caller to provide complete, absolute file paths. The library does NOT maintain a global "data directory" or "calibration directory".

**Design rationale**:
- **Flexibility**: Different deployments (hospital, clinic, research lab) may organize calibration data differently
- **No hidden paths**: Caller has complete control and visibility into file access
- **Stateless**: No configuration side effects; each call is independent

### Path Parameters Across API

| Function | Path Parameter | Type | Example |
|----------|----------------|------|---------|
| `xpe_log_set_file` | `filePath` | output log | `"C:\\logs\\xpe_runtime.log"` |
| `xpe_calib_load_offset` | `filePath` | input calibration | `"C:\\data\\calib\\offset_2024.xpe_calib"` |
| `xpe_calib_load_gain` | `filePath` | input calibration | `"C:\\data\\calib\\gain_2024.xpe_calib"` |
| `xpe_calib_load_defect_map` | `filePath` | input calibration | `"C:\\data\\calib\\defect_map.xpe_calib"` |
| `xpe_calib_save` | `filePath` | output calibration | `"C:\\data\\calib\\offset_generated.xpe_calib"` |
| `xpe_calib_check_expiry` | `filePath` | input calibration | `"C:\\data\\calib\\offset_2024.xpe_calib"` |
| `xpe_dicom_read` | `filePath` | input DICOM | `"C:\\clinical\\patient_001.dcm"` |
| `xpe_dicom_write` | `filePath` | output DICOM | `"C:\\processed\\patient_001_xpe.dcm"` |
| `xpe_dicom_read_tag_string` | `filePath` | input DICOM | `"C:\\clinical\\patient_001.dcm"` |
| `xpe_dicom_set_tag_string` | `filePath` | input/output DICOM | `"C:\\processed\\patient_001.dcm"` |
| `xpe_dicom_cstore` | `filePath` | input DICOM | `"C:\\processed\\patient_001.dcm"` |
| `xpe_dicom_write_j2k` | `filePath` | output DICOM | `"C:\\processed\\lossless.dcm"` |
| `xpe_ai_init` | `modelDirPath` | input directory | `"C:\\data\\models\\"` |
| `gsvg_load_scatter_lut` | `filePath` | input LUT | `"C:\\data\\calib\\gsvg\\grid_lut.dat"` |

**Note**: `xpe_init` config JSON does **not** include calibration or data directory paths. These are always caller-specified at function-call time.

### Recommended Directory Structure for Production Software

See **production-integration-guide.md** for:
- Standard deployment layout for medical software integrating XPE
- C# `XpePathManager` class example
- Path persistence patterns (appsettings.json)
- Site-specific customization

### Future Enhancement (v2.0+)

A future version of `xpe_init` config JSON may include:

```json
{
  "dataDirectories": {
    "calibrationDir": "C:\\data\\calibration",
    "modelDir": "C:\\data\\models"
  }
}
```

This would allow callers to provide default paths once at initialization, reducing boilerplate in application code. **Current version (1.x) requires explicit paths per call.**
