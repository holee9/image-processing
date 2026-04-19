# Architect Design: xpe_enhance_advanced.dll (SPEC-XPE-P2-ADV)

**Date**: 2026-04-18
**Architect**: xpe-architect
**Status**: Complete -- ready for xpe-implementer review

---

## 1. Module Overview

xpe_enhance_advanced.dll provides advanced image processing algorithms for
X-ray FPD images in the post-processing pipeline. It is a Layer 1 module
depending ONLY on xpe_common.dll.

### Exported API (6 functions)

| Function | SWU | SRS | Category |
|----------|-----|-----|----------|
| `xpe_enhance_advanced_init` | -- | REQ-ADV-001 | Lifecycle |
| `xpe_enhance_advanced_shutdown` | -- | REQ-ADV-020 | Lifecycle |
| `xpe_enhance_advanced_version` | -- | -- | Lifecycle |
| `xpe_multiscale_process` | SWU-2.5 | SRS-ADV-001,002 | MFP |
| `xpe_fractional_process` | SWU-2.6 | SRS-ADV-010 | Edge enhancement |
| `xpe_detect_collimation` | SWU-2.8 | SRS-ADV-020 | ROI detection |
| `xpe_calc_exposure_index` | SWU-2.10 | SRS-ADV-013 | EI/DI (IEC 62494-1) |

**Note**: `xpe_calc_exposure_index` also exists in xpe_enhance_basic.dll per
SPEC-XPE-MASTER v2.1.0. The copy in this module enables ROI-aware EI refinement
after collimation detection without a cross-module call. Both implementations
MUST produce identical results for the same inputs.

## 2. ABI Contract

All declarations follow the XPE C ABI convention:

- `extern "C"` linkage with `__cdecl` calling convention
- `XPE_API` macro (`__declspec(dllexport)` when `XPE_DLL_EXPORT` is defined)
- `#pragma pack(push, 8)` for all structs (inherited from xpe_types.h)
- Caller owns all memory via `xpe_alloc_image()` / `xpe_free_image()`
- `const char* configJsonOrNull` pattern: NULL means use defaults
- All processing functions are reentrant on independent buffers

## 3. File Layout

```
modules/enhance_advanced/
├── CMakeLists.txt
├── include/xpe/enhance_advanced/
│   ├── enhance_advanced_api.h     # Public C ABI (Doxygen-documented)
│   ├── xpe_enhance_advanced_api.h # Legacy header (same content, kept for compat)
│   └── internal.h                 # Private: constants, state, config parsers
├── src/
│   ├── xpe_enhance_advanced.cpp   # Lifecycle + API dispatch
│   ├── mfp_scalar.cpp             # SWU-2.5: Laplacian pyramid MFP
│   ├── mfp_scalar.h               # LaplacianPyramid class
│   ├── fractional_derivative.cpp  # SWU-2.6: Fractional-order differentiation
│   ├── xpe_collimation_detect.cpp # SWU-2.8: Hough-based collimation detection
│   ├── exposure_index.cpp         # SWU-2.10: IEC 62494-1 EI/DI
│   └── detail/
│       ├── edge_detection.cpp     # Internal: Sobel/Canny edge helpers
│       └── hough_transform.cpp    # Internal: Hough line detection
└── tests/
    ├── test_api_header.cpp
    ├── test_lifecycle.cpp
    ├── test_mfp_scalar.cpp
    ├── test_collimation_detect.cpp
    ├── test_edge_enhancement.cpp
    ├── test_exposure_index.cpp
    └── test_integration.cpp
```

## 4. Dependency Graph

```
xpe_enhance_advanced.dll
├── xpe_common.dll         (Layer 0 -- shared types, errors, memory)
└── Eigen 3.4.x            (header-only -- matrix/vector ops for MFP)

NO lateral dependency on:
  xpe_preprocess.dll
  xpe_enhance_basic.dll
  xpe_ai.dll
  xpe_display.dll
  xpe_dicom.dll
  gsvg.dll
```

## 5. CMake Configuration

- **Target**: `xpe_enhance_advanced` (SHARED library)
- **C++ Standard**: C++17
- **Eigen**: `find_package(Eigen3 3.4 QUIET CONFIG)` with FetchContent fallback
- **Compile flags (MSVC)**: `/W4 /arch:AVX2 /fp:fast` (SVML auto-vectorization)
- **Compile flags (GCC/Clang)**: `-Wall -Wextra -Wpedantic -mavx2`
- **Definitions**: `XPE_DLL_EXPORT`, `_USE_MATH_DEFINES`
- **Tests**: Google Test with `gtest_discover_tests()` (vcpkg or FetchContent)

## 6. Design Decisions

### 6.1 Dual API Headers

Two header files exist for backward compatibility:
- `xpe_enhance_advanced_api.h`: Original header with all declarations
- `enhance_advanced_api.h`: Upgraded with full Doxygen documentation

Both include the same set of declarations. The enhanced version is the
canonical header going forward.

### 6.2 Exposure Index Duplication

`xpe_calc_exposure_index` appears in both `xpe_enhance_basic.dll` and this
module. This is intentional: the P2 pipeline uses collimation detection to
crop the image to the ROI, then calls EI on the cropped region. Having EI
locally avoids a lateral dependency on xpe_enhance_basic.dll. Both copies
MUST produce bit-identical results.

### 6.3 internal.h Design

The internal.h header provides:
- Compile-time constants (defaults, min/max ranges, safety limits)
- Module state externs (g_initialized, g_initMutex)
- Config parsing function declarations (namespace-qualified C++)

This header is PRIVATE. It is excluded from the PUBLIC include path and
must only be included by src/ and tests/ files.

### 6.4 Eigen Integration

Eigen 3.4.x is a header-only library used for:
- Laplacian pyramid matrix operations (MFP)
- Fractional-order coefficient computation
- Edge detection kernel convolution

It is linked via `Eigen3::Eigen` CMake target. The FetchContent fallback
ensures standalone builds work without vcpkg.

### 6.5 AVX2 Requirement

All source files are compiled with `/arch:AVX2 /fp:fast` (MSVC) or
`-mavx2` (GCC/Clang). This is a deployment prerequisite: the target
platform must support Haswell+ (2013) instruction set. The /fp:fast flag
enables MSVC SVML auto-vectorization for transcendental functions (exp,
log) giving ~8x throughput in MFP and bilateral filter loops.

## 7. Implementation Notes for xpe-implementer

1. **All 3 core API functions already have implementations** in
   src/xpe_enhance_advanced.cpp. The task is to verify they match the
   documented API contract in enhance_advanced_api.h.

2. **internal.h is NEW** and must be integrated into existing source files
   (replace ad-hoc constants with the centralized definitions).

3. **xpe_enhance_advanced_api.h** should be updated to include
   enhance_advanced_api.h or the declarations should be synchronized.

4. **No exceptions across C ABI boundary** (REQ-ADV-030). All functions
   must catch exceptions and return XPE_ERR_INTERNAL or
   XPE_ERR_SAFETY_VIOLATION.

5. **Thread safety**: g_initMutex protects initialization state. Processing
   functions must be reentrant on independent buffers (no shared mutable
   state during processing).

## 8. Configuration JSON Schemas

### xpe_multiscale_process
```json
{
  "levels": 4,
  "edge_gain": 1.5,
  "texture_gain": 1.0,
  "flat_gain": 0.8,
  "noise_threshold": 5.0
}
```

### xpe_fractional_process
```json
{
  "iterations": 1,
  "step_size": 0.25
}
```

### xpe_detect_collimation
```json
{
  "sensitivity": 0.5,
  "min_area_ratio": 0.05,
  "border_margin": 8
}
```

---

End of design document.
