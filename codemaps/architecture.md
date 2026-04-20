# XPE-Post Architecture Map

**Project**: XPE (X-ray Image Processing Engine)
**Lane**: Post-Processing (dev/postprocess branch)
**Last Updated**: 2026-04-19

## System Overview

XPE-Post는 모듈형 C++ DLL 아키텍처로 구현된 의료용 X-ray 이미지 처리 엔진입니다. IEC 62304 Class B 소프트웨어로 개발되며, 7개의 XPE 모듈과 1개의 GSVG 모듈로 구성됩니다.

### Architecture Layers

```
┌─────────────────────────────────────────────────────────────┐
│ Layer 2: Application Layer (C# WPF)                        │
│ ImageProcTest GUI (clients/ImageProcTest/)                 │
└───────────────────────┬─────────────────────────────────────┘
                        │ C ABI (DLL Import)
┌───────────────────────▼─────────────────────────────────────┐
│ Layer 1: Processing Modules (DLL)                          │
│ ┌─────────────────┬─────────────────┬─────────────────┐     │
│ │ preprocess      │ enhance_basic   │ enhance_advanced│     │
│ │ (Gain/Offset)   │ (CLAHE/Noise)   │ (MFP/Collimation)│    │
│ └─────────────────┴─────────────────┴─────────────────┘     │
│ ┌─────────────────┬─────────────────┬─────────────────┐     │
│ │ dicom           │ display         │ ai              │     │
│ │ (DICOM I/O)     │ (LUT/Window)    │ (Inference)     │     │
│ └─────────────────┴─────────────────┴─────────────────┘     │
│ ┌─────────────────┐                                         │
│ │ gsvg            │ (Independent, graphics)                │
│ └─────────────────┘                                         │
└───────────────────────┬─────────────────────────────────────┘
                        │
┌───────────────────────▼─────────────────────────────────────┐
│ Layer 0: Common Foundation                                 │
│ xpe_common.dll (Lifecycle, Config, Logging, Memory, AED)   │
└─────────────────────────────────────────────────────────────┘
```

## Module Dependency Graph

```
                    ┌──────────────┐
                    │  gsvg.dll    │ (Independent)
                    │  (Eigen3)    │
                    └──────────────┘

           ┌────────────────────────────────┐
           │        xpe_common.dll          │
           │  (spdlog, nlohmann_json, fmt)  │
           └────────┬───────────────────────┘
                    │ xpe_init(), xpe_types.h
      ┌─────────────┼─────────────┬─────────────┐
      │             │             │             │
┌─────▼─────┐ ┌────▼────┐ ┌─────▼─────┐ ┌──▼────────┐
│preprocess │ │enhance_ │ │enhance_   │ │display.dll│
│  .dll     │ │ basic   │ │advanced   │ │           │
│           │ │ .dll    │ │.dll       │ │           │
└───────────┘ └────┬────┘ └─────┬─────┘ └──┬────────┘
                   │             │          │
                   └──────┬──────┴──────────┘
                          │
                  ┌───────▼───────┐
                  │  dicom.dll    │
                  │               │
                  └───────────────┘

                    ┌─────────┐
                    │ ai.dll  │ (Future)
                    └─────────┘
```

## Module Specifications

### xpe_common.dll (Layer 0)
**Purpose**: Core foundation providing lifecycle, configuration, logging, memory management, and AED

**Dependencies**:
- spdlog (logging)
- nlohmann/json (config parsing)
- fmt (formatting)

**Exported Functions** (18 total):
- **Lifecycle** (3): `xpe_init`, `xpe_shutdown`, `xpe_version`
- **Config** (1): `xpe_configure`
- **ParamRange** (1): `xpe_get_param_range`
- **Error/Alert** (4): `xpe_error_string`, `xpe_get_pending_alert_count`, `xpe_get_pending_alert`, `xpe_clear_alerts`
- **Logging** (3): `xpe_log_set_level`, `xpe_log_set_file`, `xpe_log_flush`
- **AED** (3): `xpe_aed_configure`, `xpe_aed_poll_event`, `xpe_aed_get_status`
- **Image Memory** (3): `xpe_alloc_image`, `xpe_free_image`, `xpe_copy_image`

**Headers**:
- `xpe_common_api.h`: Main API
- `xpe_types.h`: Core data structures (`XpeImage`, `XpeErrorCode`)
- `xpe_error.h`: Error handling and alerts
- `xpe_memory.h`: Memory management

### preprocess.dll (Layer 1)
**Purpose**: Raw detector data correction and calibration

**Dependencies**: `xpe_common.dll`

**Key Algorithms**:
- Gain correction (offset/gain map application)
- Offset correction (dark current subtraction)
- Ghost correction (object scatter removal)
- Defect correction (bad pixel interpolation)
- Nonlinearity correction
- Binning correction
- Temperature compensation

**Calibration Format**: XCAL (proprietary XML-based format)

### enhance_basic.dll (Layer 1)
**Purpose**: Basic image enhancement (clinical utility)

**Dependencies**: `xpe_common.dll`

**Key Algorithms**:
- CLAHE (Contrast Limited Adaptive Histogram Equalization)
- Noise reduction (bilateral filter)
- Log transform
- Edge enhancement
- Exposure index calculation

### enhance_advanced.dll (Layer 1)
**Purpose**: Advanced image enhancement (multi-scale processing)

**Dependencies**: `xpe_common.dll`, Eigen3

**Key Algorithms**:
- Multi-scale Fractional Processing (MFP)
- Collimation detection (Hough transform)
- Edge enhancement (fractional derivative)
- Exposure index (advanced)

**Status**: SPEC-P2-ADV implementation in progress

### display.dll (Layer 1)
**Purpose**: Image display and window/level operations

**Dependencies**: `xpe_common.dll`

**Key Functions**:
- Modality LUT (rescale slope/intercept)
- Presentation LUT (gamma correction)
- VOI LUT (window/level)

### dicom.dll (Layer 1)
**Purpose**: DICOM format I/O and network communication

**Dependencies**: `xpe_common.dll`

**Key Classes**:
- `DicomReader`: DICOM file reading
- `DicomWriter`: DICOM file writing
- `DicomValidator`: DICOM conformance validation
- `DicomNetworkSCU`: DICOM network SCU

### ai.dll (Layer 1)
**Purpose**: AI-based image analysis

**Dependencies**: `xpe_common.dll`

**Status**: Placeholder for future AI inference module

### gsvg.dll (Layer 1-G)
**Purpose**: Graphics rendering (independent module)

**Dependencies**: Eigen3 (matrix operations)

**Status**: Independent of xpe_common, standalone graphics library

## Data Flow

```
Raw Detector Data
       │
       ▼
┌──────────────┐
│  preprocess  │ → Gain/Offset/Ghost/Defect correction
└──────┬───────┘
       │
       ▼
┌──────────────┐
│enhance_basic │ → CLAHE, Noise reduction
└──────┬───────┘
       │
       ▼
┌──────────────────┐
│enhance_advanced  │ → Multi-scale processing, Collimation detection
└──────┬───────────┘
       │
       ├──► dicom.dll ──► DICOM files
       │
       └──► display.dll ──► Displayable images
```

## C ABI Contract

All modules export C-compatible APIs using:
- `extern "C"` linkage
- `XPE_API` macro (DLL import/export)
- C-compatible types only (no C++ classes in API)
- Pack=8 struct alignment

**Call Order**:
1. `xpe_init()` (required first call)
2. Module initialization (per-module init functions)
3. Process calls (image processing operations)
4. Module cleanup (per-module shutdown functions)
5. `xpe_shutdown()` (required final call)

## Build Configuration

**CMake Options**:
- `BUILD_SHARED_LIBS=ON`: Build DLLs (default)
- `BUILD_TESTS=ON`: Build Google Test suites
- `BUILD_PREPROCESS=ON`: Build preprocess module
- `BUILD_ENHANCE_BASIC=ON`: Build enhance_basic module
- `BUILD_ENHANCE_ADVANCED=ON`: Build enhance_advanced module
- `BUILD_DISPLAY=ON`: Build display module
- `BUILD_DICOM=ON`: Build dicom module
- `BUILD_AI=ON`: Build ai module (placeholder)
- `BUILD_GSVG=ON`: Build gsvg module

**Dependencies** (FetchContent fallback):
- spdlog v1.14.1
- nlohmann/json v3.11.3
- fmt 11.0.2
- Eigen3 3.4.0
- Google Test (via vcpkg)

## Test Structure

```
tests/
├── common/              # Common functionality tests
├── common_smoke/        # Smoke tests
├── common_unit/         # Unit tests
├── enhance_advanced_tests/  # Advanced module tests (in progress)
└── CMakeLists.txt
```

**Test Categories**:
- API header tests: Verify C ABI compatibility
- Lifecycle tests: Init/shutdown cycles
- Pixel accuracy tests: Scalar vs SIMD equivalence
- Integration tests: End-to-end workflows

## Compliance

**IEC 62304 Class B**:
- SRS (Software Requirements Specification): `docs/project/srs_*.md`
- SDD (Software Design Description): `docs/project/sdd_*.md`
- VVP (Verification and Validation Plan): `docs/project/vvp_*.md`
- RTM (Requirements Traceability Matrix): `docs/project/rtm_*.md`

## Performance Considerations

**SIMD Optimization**:
- AVX2 intrinsics for pixel processing loops
- Scalar fallback for non-SIMD paths
- Deterministic output (same input → same output, regardless of code path)

**Memory Management**:
- RAII for C++ internal objects
- C ABI handles for external callers
- Custom allocators for image buffers (alignment requirements)

## Lane-Based Development

**Current Lane**: dev/postprocess (xpe-post worktree)

**Owned Paths**:
- `modules/enhance_*/`
- `modules/ai/`
- `modules/display/`
- `modules/dicom/`
- `modules/gsvg/`

**Shared Paths** (owned by main orchestrator):
- Root `CMakeLists.txt`
- `CLAUDE.md`
- `.moai/`
- `.claude/`
