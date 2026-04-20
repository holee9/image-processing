# XPE Module Catalog

Complete reference for all XPE modules in the post-processing lane.

## Module Index

| Module | DLL Name | Purpose | Layer | Status |
|--------|----------|---------|-------|--------|
| Common | xpe_common.dll | Foundation services | 0 | Stable |
| Preprocess | xpe_preprocess.dll | Raw data correction | 1 | Stable |
| Enhance Basic | xpe_enhance_basic.dll | Basic enhancement | 1 | Stable |
| Enhance Advanced | xpe_enhance_advanced.dll | Advanced processing | 1 | WIP (SPEC-P2-ADV) |
| Display | xpe_display.dll | Display operations | 1 | Stable |
| DICOM | xpe_dicom.dll | DICOM I/O | 1 | Stable |
| AI | xpe_ai.dll | AI inference | 1 | Placeholder |
| GSVG | xpe_gsvg.dll | Graphics rendering | 1-G | Stable |

## Detailed Module Specifications

### 1. xpe_common.dll

**Version**: 0.1.0
**Layer**: 0 (Foundation)
**Dependencies**: spdlog, nlohmann/json, fmt

**Responsibilities**:
- Library lifecycle (init/shutdown)
- Configuration management (JSON-based)
- Logging subsystem
- Error handling and alerts
- Memory management
- AED (Automatic Exposure Detection)

**Public API** (18 functions):
```
Lifecycle:
  xpe_init()
  xpe_shutdown()
  xpe_version()

Configuration:
  xpe_configure()
  xpe_get_param_range()

Error/Alert:
  xpe_error_string()
  xpe_get_pending_alert_count()
  xpe_get_pending_alert()
  xpe_clear_alerts()

Logging:
  xpe_log_set_level()
  xpe_log_set_file()
  xpe_log_flush()

AED:
  xpe_aed_configure()
  xpe_aed_poll_event()
  xpe_aed_get_status()

Memory:
  xpe_alloc_image()
  xpe_free_image()
  xpe_copy_image()
```

**Key Data Structures**:
- `XpeImage`: Image container (width, height, data pointer)
- `XpeErrorCode`: Error codes (XPE_OK, XPE_ERR_*)
- `XpeAlert`: Alert structure (severity, message)

**Source Files**:
- `modules/common/src/xpe_common.cpp`
- `modules/common/src/xpe_logging.cpp`
- `modules/common/src/xpe_memory.cpp`
- `modules/common/src/xpe_aed.cpp`

---

### 2. xpe_preprocess.dll

**Version**: 0.1.0
**Layer**: 1 (Processing)
**Dependencies**: xpe_common.dll

**Responsibilities**:
- Gain correction
- Offset correction
- Ghost reduction
- Defect pixel correction
- Nonlinearity correction
- Binning correction
- Temperature compensation
- Calibration data management (XCAL format)

**Calibration Tools** (CLI utilities):
- `xpe_calib_check_expiry`: Check calibration expiry
- `xpe_calib_generate_offset`: Generate offset map
- `xpe_calib_load_defect_map`: Load defect map
- `xpe_calib_load_gain`: Load gain map
- `xpe_calib_load_offset`: Load offset map
- `xpe_calib_save`: Save calibration data

**Key Classes**:
- `XpePreprocess`: Main preprocessing class
- `XpeGain`: Gain correction
- `XpeOffset`: Offset correction
- `XpeDefect`: Defect correction
- `XpeCalibration`: Calibration manager
- `XcalReader`: XCAL format reader
- `XcalValidator`: XCAL validation

**Source Files**:
- `modules/preprocess/src/xpe_preprocess.cpp`
- `modules/preprocess/src/gain_correct.cpp`
- `modules/preprocess/src/offset_correct.cpp`
- `modules/preprocess/src/ghost_correct.cpp`
- `modules/preprocess/src/defect_correct.cpp`
- `modules/preprocess/src/nonlinearity_correct.cpp`
- `modules/preprocess/src/binning_correct.cpp`
- `modules/preprocess/src/temp_compensate.cpp`
- `modules/preprocess/src/calibration_manager.cpp`
- `modules/preprocess/src/xcal_reader.cpp`
- `modules/preprocess/src/xcal_validator.cpp`

---

### 3. xpe_enhance_basic.dll

**Version**: 0.1.0
**Layer**: 1 (Processing)
**Dependencies**: xpe_common.dll

**Responsibilities**:
- CLAHE (Contrast Limited Adaptive Histogram Equalization)
- Noise reduction (bilateral filter)
- Log transform
- Edge enhancement
- Exposure index calculation

**Key Algorithms**:
- CLAHE: Adaptive histogram equalization with contrast limiting
- Bilateral filter: Edge-preserving smoothing
- Log transform: Logarithmic intensity mapping
- Edge enhancement: Sharpening filters

**Source Files**:
- `modules/enhance_basic/src/enhance_basic.cpp`
- `modules/enhance_basic/src/contrast_enhance.cpp`
- `modules/enhance_basic/src/noise_reduce.cpp`
- `modules/enhance_basic/src/log_transform.cpp`
- `modules/enhance_basic/src/edge_enhance.cpp`
- `modules/enhance_basic/src/exposure_index.cpp`

---

### 4. xpe_enhance_advanced.dll

**Version**: 0.1.0 (WIP)
**Layer**: 1 (Processing)
**Dependencies**: xpe_common.dll, Eigen3
**SPEC**: SPEC-P2-ADV

**Responsibilities**:
- Multi-scale Fractional Processing (MFP)
- Collimation detection (Hough transform)
- Advanced edge enhancement (fractional derivative)
- Advanced exposure index

**Key Algorithms**:
- Multi-scale processing: Pyramid-based multi-resolution analysis
- Fractional processing: Fractional derivative and integration
- Collimation detection: Hough line detection for collimator blades

**Test Status**:
- `test_api_header.cpp`: ✅ C ABI verification
- `test_lifecycle.cpp`: ✅ Init/shutdown cycles
- `test_mfp_scalar.cpp`: ✅ MFP scalar implementation
- `test_collimation_detect.cpp`: ✅ Collimation detection
- `test_edge_enhancement.cpp`: ✅ Edge enhancement
- `test_exposure_index.cpp`: ✅ Exposure index
- `test_integration.cpp`: 🚧 End-to-end integration

**Source Files**:
- `modules/enhance_advanced/src/xpe_enhance_advanced.cpp`
- `modules/enhance_advanced/src/enhance_advanced.cpp`
- `modules/enhance_advanced/src/multiscale_process.cpp`
- `modules/enhance_advanced/src/fractional_process.cpp`
- `modules/enhance_advanced/src/fractional_derivative.cpp`
- `modules/enhance_advanced/src/collimation_detect.cpp`
- `modules/enhance_advanced/src/detail/edge_detection.cpp`
- `modules/enhance_advanced/src/detail/hough_transform.cpp`
- `modules/enhance_advanced/src/exposure_index.cpp`
- `modules/enhance_advanced/src/enhance_advanced_helpers.cpp`

---

### 5. xpe_display.dll

**Version**: 0.1.0
**Layer**: 1 (Processing)
**Dependencies**: xpe_common.dll

**Responsibilities**:
- Modality LUT (rescale slope/intercept)
- Presentation LUT (gamma correction)
- VOI LUT (window/level)

**Key Classes**:
- `ModalityLUT`: Modality transformation
- `PresentationLUT`: Presentation transformation
- `VOILUT`: Window/level operations

**Source Files**:
- `modules/display/src/display.cpp`
- `modules/display/src/modality_lut.cpp`
- `modules/display/src/presentation_lut.cpp`
- `modules/display/src/voi_lut.cpp`
- `modules/display/src/display_helpers.cpp`

---

### 6. xpe_dicom.dll

**Version**: 0.1.0
**Layer**: 1 (Processing)
**Dependencies**: xpe_common.dll

**Responsibilities**:
- DICOM file reading
- DICOM file writing
- DICOM conformance validation
- DICOM network SCU

**Key Classes**:
- `DicomReader`: Read DICOM files
- `DicomWriter`: Write DICOM files
- `DicomValidator`: Validate DICOM conformance
- `DicomNetworkSCU`: DICOM network operations

**Source Files**:
- `modules/dicom/src/dicom.cpp`
- `modules/dicom/src/DicomReader.cpp`
- `modules/dicom/src/DicomWriter.cpp`
- `modules/dicom/src/DicomValidator.cpp`
- `modules/dicom/src/DicomNetworkSCU.cpp`

---

### 7. xpe_ai.dll

**Version**: 0.1.0 (Placeholder)
**Layer**: 1 (Processing)
**Dependencies**: xpe_common.dll

**Responsibilities**:
- AI-based image analysis (future)
- Inference engine (future)

**Status**: Placeholder for future AI inference module

**Source Files**:
- `modules/ai/src/ai.cpp` (placeholder)

---

### 8. xpe_gsvg.dll

**Version**: 0.1.0
**Layer**: 1-G (Independent Graphics)
**Dependencies**: Eigen3 (independent of xpe_common)

**Responsibilities**:
- Graphics rendering (SVG-like)
- Independent of XPE common infrastructure

**Status**: Standalone graphics library

**Source Files**:
- `modules/gsvg/src/gsvg.cpp`

---

## Module Dependencies Summary

```
┌─────────────────────────────────────────────────────────┐
│ External Dependencies                                   │
│ spdlog, nlohmann/json, fmt, Eigen3, GoogleTest         │
└────────┬────────────────────────────────────────────────┘
         │
┌────────▼───────────────────────────────────────────────┐
│ xpe_common (Layer 0)                                   │
│ Depends: spdlog, nlohmann/json, fmt                    │
└────┬───┬───────────┬───────────┬───────────┬──────────┘
     │   │           │           │           │
┌────▼───▼┐ ┌────▼───┐ ┌────▼───┐ ┌──▼────────┐ ┌─────┐
│preprocess│ │enhance_│ │enhance_│ │display    │ │dicom│
│          │ │basic   │ │advanced│ │           │ │     │
└──────────┘ └────┬───┘ └────┬───┘ └────────────┘ └──┬──┘
                  │           │                    │
                  └─────┬─────┴────────────────────┘
                        │
                  ┌─────▼─────┐
                  │   ai.dll  │ (Placeholder)
                  └───────────┘

┌─────────────────────────────────────────────────────────┐
│ gsvg.dll (Layer 1-G, Independent)                       │
│ Depends: Eigen3 only (no xpe_common dependency)         │
└─────────────────────────────────────────────────────────┘
```

## Header File Organization

### Common Headers (modules/common/include/xpe/common/)
- `xpe_common_api.h`: Main API exports
- `xpe_types.h`: Core types and structures
- `xpe_error.h`: Error codes and alerts
- `xpe_memory.h`: Memory management

### Module Headers (modules/*/include/xpe/<module>/)
- `<module>_api.h`: Public C API
- `<module>_internal.h`: Internal C++ interfaces

---

**Last Updated**: 2026-04-19
**Project**: XPE Post-Processing Lane (dev/postprocess)
