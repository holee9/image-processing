# XPE Image Processing Engine - Architecture Overview

**Last Updated**: 2026-04-20
**Branch**: dev/postprocess
**Architecture Type**: Modular DLL with C# Orchestration

## System Architecture

The XPE (X-ray Processing Engine) is a modular medical image processing system designed for clinical deployment with IEC 62304 Class B compliance. The architecture follows a layered design with clear separation of concerns.

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    C# Orchestration Layer                   │
│                    (ImageProcTest.exe)                      │
│  - Composite backend pattern                                │
│  - Graceful degradation                                     │
│  - Module readiness levels (R0-R3)                          │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                      C++ Processing Layer                    │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ Layer 3: AI Processing (xpe_ai.dll)                   │  │
│  │   - Body part recognition, stitching, denoising       │  │
│  └───────────────────────────────────────────────────────┘  │
│                              │                              │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ Layer 2: Advanced Processing                          │  │
│  │   - xpe_enhance_advanced.dll: MFP, Edge, Collimation  │  │
│  │   - gsvg.dll: Grid suppression (independent)          │  │
│  └───────────────────────────────────────────────────────┘  │
│                              │                              │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ Layer 1: Basic Processing                             │  │
│  │   - xpe_preprocess.dll: Calibration, defect correction │  │
│  │   - xpe_enhance_basic.dll: EI, noise reduction        │  │
│  │   - xpe_display.dll: VOI LUT, presentation state      │  │
│  │   - xpe_dicom.dll: DICOM I/O                          │  │
│  └───────────────────────────────────────────────────────┘  │
│                              │                              │
│  ┌───────────────────────────────────────────────────────┐  │
│  │ Layer 0: Common Infrastructure (xpe_common.dll)       │  │
│  │   - Memory management, logging, configuration         │  │
│  │   - C ABI for interop                                 │  │
│  └───────────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│                   Third-Party Libraries                      │
│  spdlog, fmt, nlohmann-json, eigen3, opencv4, dcmtk, gtest  │
└─────────────────────────────────────────────────────────────┘
```

## Architectural Principles

### 1. Module Independence
- Each XPE module DLL depends **only** on `xpe_common`
- No lateral dependencies between processing modules
- Enables independent deployment and testing

### 2. Graceful Degradation
- GUI continues functioning with subset of modules available
- Per-module P/Invoke wrappers with exception handling
- Module readiness levels (R0-R3) drive UI state

### 3. Layered Architecture
- **Layer 0**: Common infrastructure (memory, logging, config)
- **Layer 1a**: Preprocessing (calibration, defect correction)
- **Layer 1b**: Basic enhancement, display, DICOM I/O
- **Layer 2**: Advanced algorithms (MFP, Edge, Collimation)
- **Layer 3**: AI-powered analysis

### 4. Zero-Crossing ABI
- C linkage for C# interop
- No C++ exceptions cross module boundaries
- Deterministic struct layout with `#pragma pack(8)`

### 5. Configuration-Driven
- JSON-based runtime configuration
- Parameter ranges for validation
- Logging level control

## Technology Stack

### Native Layer (C++)
- **Language**: C++17 with modern features
- **Compiler**: MSVC 2022 (Windows)
- **Build**: CMake 3.20+ with vcpkg integration
- **Testing**: Google Test framework
- **Optimization**: SIMD (AVX2/FMA) for critical algorithms

### Managed Layer (C#)
- **Language**: C#12 / .NET 8
- **Architecture**: Clean Architecture with separation of concerns
- **Testing**: xUnit framework
- **Deployment**: Single executable with native DLL loading

### Third-Party Dependencies
- **spdlog** (1.13.0+): High-performance logging
- **fmt** (10.0.0+): Modern formatting
- **nlohmann-json** (3.11.3+): JSON parsing
- **eigen3** (3.4.0+): Linear algebra
- **opencv4** (4.9.0+): Computer vision
- **dcmtk** (3.6.8+): DICOM toolkit
- **gtest** (1.14.0+): Testing framework

## Quality Assurance

### IEC 62304 Class B Compliance
- Full regulatory documentation (SRS, SDD, VVP, RTM)
- Traceability from requirements to tests
- Verification and validation procedures

### Testing Strategy
- **Golden Reference Testing**: Bit-identical validation
- **Memory Safety**: 1000 frame leak testing
- **Benchmark Freeze**: Reproducible performance evaluation
- **Cross-Verification**: Independent claim verification

### Quality Gates
- Multi-stage verification with automated testing
- Code coverage targets (85%+)
- Static analysis integration

## Current Status (dev/postprocess)

### Recently Modified
- `modules/enhance_advanced/src/detail/hough_transform.cpp`
- `modules/enhance_advanced/tests/test_integration.cpp`
- Documentation updates (SRS, SDD, RTM)

### Focus Areas
- **Hough Transform**: Collimation ROI detection
- **Feature Detection**: Edge and boundary detection
- **Integration Testing**: Cross-module validation

### Known Issues
- See `memory/quality-verification-spec-xpe-p2-adv.md` for detailed quality assessment

## Next Steps

1. Review detailed module documentation in `modules.md`
2. Examine dependency relationships in `dependencies.md`
3. Reference entry point catalog in `entry-points.md`
4. Trace data flows in `data-flow.md`
