# XPE Modules Catalog

**Last Updated**: 2026-04-20

## Module Inventory

### Layer 0: Common Infrastructure

#### xpe_common.dll
- **Purpose**: Foundation services and ABI contract for all modules
- **SWU Count**: 7 units
- **API Count**: 18 functions
- **Dependencies**: fmt.dll, spdlog.dll
- **Dependents**: All other XPE modules

**Core Responsibilities**:
- Memory management (`xpe_alloc_image`, `xpe_free_image`, `xpe_copy_image`)
- Lifecycle management (`xpe_init`, `xpe_shutdown`, `xpe_version`)
- Configuration system (`xpe_configure`, `xpe_get_param_range`)
- Error handling & logging (`xpe_error_string`, `xpe_log_set_level`)
- Type definitions (`XpeImageBuffer`, `XpeImageMetadata`, `XpeErrorCode`)

**Key Patterns**:
- C ABI for C# interop
- Thread-safe resource management
- JSON-based configuration
- `#pragma pack(8)` for deterministic struct layout

---

### Layer 1a: Preprocessing

#### xpe_preprocess.dll
- **Purpose**: Detector domain correction and calibration pipeline
- **SWU Count**: 9 units
- **API Count**: 18 functions
- **Dependencies**: xpe_common.dll, fmt.dll, spdlog.dll

**Core Responsibilities**:
- Offset/Gain correction (detector linearization)
- Defect detection and correction
- Nonlinearity Compensation (NLCSC)
- Temporal noise correction
- Binning and readout processing
- Ghost correction
- Calibration round-trip validation

**Key Features**:
- Golden Reference testing (26/26 tests passed)
- SIMD optimization (AVX2/FMA)
- Automatic calibration loading

---

### Layer 1b: Basic Enhancement

#### xpe_enhance_basic.dll
- **Purpose**: Fundamental image quality improvements
- **SWU Count**: 5 units
- **API Count**: 7 functions
- **Dependencies**: xpe_common.dll, fmt.dll, spdlog.dll

**Core Responsibilities**:
- Exposure Index calculation (IEC 62494-1)
- Basic noise reduction
- Simple sharpening
- Dynamic range compression

---

### Layer 2: Advanced Enhancement

#### xpe_enhance_advanced.dll
- **Purpose**: Clinical-grade deterministic processing
- **SWU Count**: 4 units
- **API Count**: 4 functions
- **Dependencies**: xpe_common.dll, fmt.dll, spdlog.dll, eigen3.dll

**Core Responsibilities**:
- **Multiscale Frequency Processing** (SWU-2.5): Laplacian pyramid decomposition
- **Fractional-Order Edge Enhancement** (SWU-2.6): Edge enhancement with fractional differentiation
- **Collimation ROI Detection** (SWU-2.8): Hough transform-based boundary detection
- **Exposure Index Calculation** (SWU-2.10): IEC 62494-1 compliant EI/DI calculation

**Key Algorithms**:
- Hough Transform for edge detection
- Fractional derivative for texture enhancement
- Eigen3 matrix operations
- Confidence-based ROI detection

**Current Status** (dev/postprocess):
- Hough Transform implementation complete
- Integration tests added
- Documentation updated (SRS, SDD, RTM)

---

### Layer 1b: Display Processing

#### xpe_display.dll
- **Purpose**: Presentation state management
- **SWU Count**: 4 units
- **API Count**: 11 functions
- **Dependencies**: xpe_common.dll, fmt.dll, spdlog.dll

**Core Responsibilities**:
- VOI LUT application
- Modality LUT calibration
- Presentation state processing
- GSDF calibration
- Custom LUT support

---

### Layer 1b: DICOM I/O

#### xpe_dicom.dll
- **Purpose**: DICOM standard compliance
- **SWU Count**: 4 units
- **API Count**: 10 functions
- **Dependencies**: xpe_common.dll, fmt.dll, spdlog.dll, dcmtk.dll (multiple)

**Core Responsibilities**:
- DICOM reading/writing (DCMTK-based)
- Network SCU operations
- Validation and compliance checking
- Metadata extraction

---

### Layer 3: AI Processing

#### xpe_ai.dll
- **Purpose**: AI-powered image analysis
- **SWU Count**: 4 units
- **API Count**: 7 functions
- **Dependencies**: xpe_common.dll

**Core Responsibilities**:
- Body part recognition
- Image stitching
- Bone suppression
- Denoising (DLDenoiser)

**Deployment Pattern**:
- Proxy module launches `xpe_ai_worker.exe` via IPC
- No direct DLL dependencies on other XPE modules

---

### Layer 2-G: Grid Suppression

#### gsvg.dll
- **Purpose**: Anti-aliasing grid suppression
- **SI Count**: 4 units
- **API Count**: 8 functions
- **Dependencies**: None (independent)

**Independence**:
- No `xpe_common` dependency
- No lateral dependencies on other XPE modules
- Separate build and deployment

---

## C# Orchestration Layer

### ImageProcTest.exe
- **Purpose**: Test GUI and production frontend
- **Language**: C#12 / .NET 8
- **Pattern**: Composite backend for graceful degradation

**Key Features**:
- Per-module P/Invoke wrappers with exception handling
- NativeDependencyLoader for dynamic loading
- Module readiness levels (R0-R3) for UI state
- Workflow pipeline with stage toggles

**Module Wrappers**:
- `XpeCommonWrapper.cs`
- `XpePreprocessWrapper.cs`
- `XpeEnhanceBasicWrapper.cs`
- `XpeEnhanceAdvancedWrapper.cs`
- `XpeDisplayWrapper.cs`
- `XpeDicomWrapper.cs`
- `XpeAiWrapper.cs`

---

## Module Independence Verification

### Rule 1: No Cross-Module DLL Dependencies
Each XPE module DLL links only to `xpe_common`. Lateral dependencies are **forbidden**.

### Rule 2: Independent Deployment
A module DLL must function correctly when only `xpe_common.dll` is present.

### Rule 3: Per-Module Testing
Every module has its own Google Test executable (`*_tests.exe`).

### Rule 4: GUI Graceful Degradation
The test GUI must tolerate any subset of DLLs being absent.

### Verification Command
```bash
dumpbin /dependents <module>.dll
```

Expected output: NO other `xpe_*.dll` in the dependency list.

---

## Module Statistics

| Module | Layer | SWU Units | API Functions | Dependencies |
|--------|-------|-----------|---------------|--------------|
| xpe_common | 0 | 7 | 18 | fmt, spdlog |
| xpe_preprocess | 1a | 9 | 18 | xpe_common |
| xpe_enhance_basic | 1b | 5 | 7 | xpe_common |
| xpe_enhance_advanced | 2 | 4 | 4 | xpe_common, eigen3 |
| xpe_display | 1b | 4 | 11 | xpe_common |
| xpe_dicom | 1b | 4 | 10 | xpe_common, dcmtk |
| xpe_ai | 3 | 4 | 7 | xpe_common |
| gsvg | 2-G | 4 SI | 8 | None |

**Total**: 8 modules, 45 SWU/SI units, 85 API functions
