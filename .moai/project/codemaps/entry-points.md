# XPE Entry Points Catalog

**Last Updated**: 2026-04-20

## Common Module Entry Points

### Lifecycle Management

```cpp
XPE_API XpeErrorCode xpe_init(const char* configJsonOrNull);
XPE_API void xpe_shutdown(void);
XPE_API const char* xpe_version(void);
```

### Memory Management

```cpp
XPE_API XpeErrorCode xpe_alloc_image(uint32_t width, uint32_t height, XpeImageBuffer** bufOut);
XPE_API XpeErrorCode xpe_free_image(XpeImageBuffer* buf);
XPE_API XpeErrorCode xpe_copy_image(const XpeImageBuffer* src, XpeImageBuffer* dst);
```

### Configuration & Logging

```cpp
XPE_API XpeErrorCode xpe_configure(const char* jsonConfig);
XPE_API XpeErrorCode xpe_get_param_range(const char* paramName, float* minOut, float* maxOut, float* defaultOut);
XPE_API XpeErrorCode xpe_log_set_level(int32_t level);
XPE_API XpeErrorCode xpe_log_set_file(const char* filePath);
XPE_API const char* xpe_error_string(XpeErrorCode code);
```

---

## Preprocessing Module Entry Points

### Calibration

```cpp
XPE_API XpeErrorCode xpe_offset_gain_correct(XpeImageBuffer* img, const XpeImageMetadata* meta, const char* configJsonOrNull);
XPE_API XpeErrorCode xpe_correct_defects(XpeImageBuffer* img, const XpeImageMetadata* meta, const char* configJsonOrNull);
```

### Ghost Correction

```cpp
XPE_API XpeErrorCode xpe_correct_ghost(XpeImageBuffer* img, const XpeImageMetadata* meta, const char* configJsonOrNull);
```

---

## Basic Enhancement Module Entry Points

```cpp
XPE_API XpeErrorCode xpe_calc_exposure_index_basic(const XpeImageBuffer* img, const XpeImageMetadata* meta, float* eiOut, float* deviationIndexOut);
```

---

## Advanced Enhancement Module Entry Points

### Multiscale Frequency Processing

```cpp
XPE_API XpeErrorCode xpe_multiscale_process(XpeImageBuffer* img, const XpeImageMetadata* meta, const char* configJsonOrNull);
```

### Fractional-Order Edge Enhancement

```cpp
XPE_API XpeErrorCode xpe_fractional_process(XpeImageBuffer* img, float order, const char* configJsonOrNull);
```

### Collimation ROI Detection

```cpp
XPE_API XpeErrorCode xpe_detect_collimation(const XpeImageBuffer* img, int32_t* x0Out, int32_t* y0Out, int32_t* x1Out, int32_t* y1Out, const char* configJsonOrNull);
```

### Advanced Exposure Index

```cpp
XPE_API XpeErrorCode xpe_calc_exposure_index(const XpeImageBuffer* img, const XpeImageMetadata* meta, float* eiOut, float* deviationIndexOut);
```

---

## Display Module Entry Points

```cpp
XPE_API XpeErrorCode xpe_apply_voi_lut(XpeImageBuffer* img, const XpeImageMetadata* meta, const char* configJsonOrNull);
```

---

## DICOM Module Entry Points

```cpp
XPE_API XpeErrorCode xpe_read_dicom(const char* filePath, XpeImageBuffer** imgOut, XpeImageMetadata** metaOut);
XPE_API XpeErrorCode xpe_write_dicom(const char* filePath, const XpeImageBuffer* img, const XpeImageMetadata* meta);
```

---

## AI Module Entry Points

```cpp
XPE_API XpeErrorCode xpe_recognize_body_part(const XpeImageBuffer* img, char* bodyPartOut, int32_t bufferSize);
XPE_API XpeErrorCode xpe_stitch_images(const XpeImageBuffer* img1, const XpeImageBuffer* img2, XpeImageBuffer** resultOut);
XPE_API XpeErrorCode xpe_suppress_bone(XpeImageBuffer* img, const char* configJsonOrNull);
XPE_API XpeErrorCode xpe_denoise(XpeImageBuffer* img, const char* configJsonOrNull);
```

---

## Entry Points by Module

| Module | Entry Points | Purpose |
|--------|--------------|---------|
| xpe_common | 18 | Foundation services |
| xpe_preprocess | 18 | Calibration & correction |
| xpe_enhance_basic | 7 | Basic enhancement |
| xpe_enhance_advanced | 4 | Advanced algorithms |
| xpe_display | 11 | Presentation |
| xpe_dicom | 10 | DICOM I/O |
| xpe_ai | 7 | AI processing |

Total: 75 entry points across 7 modules
