# XPE Dependency Graph

**Last Updated**: 2026-04-20

## Module Dependencies

### Dependency Tree

```
xpe_common.dll (Layer 0)
├── All other XPE modules depend on this
└── Third-party: fmt.dll, spdlog.dll

xpe_preprocess.dll (Layer 1a)
├── Depends on: xpe_common
└── Third-party: fmt.dll, spdlog.dll

xpe_enhance_basic.dll (Layer 1b)
├── Depends on: xpe_common
└── Third-party: fmt.dll, spdlog.dll

xpe_enhance_advanced.dll (Layer 2)
├── Depends on: xpe_common
├── Third-party: fmt.dll, spdlog.dll, eigen3.dll
└── Eigen3 for matrix operations

xpe_display.dll (Layer 1b)
├── Depends on: xpe_common
└── Third-party: fmt.dll, spdlog.dll

xpe_dicom.dll (Layer 1b)
├── Depends on: xpe_common
└── Third-party: fmt.dll, spdlog.dll, dcmtk.dll (multiple)

xpe_ai.dll (Layer 3)
├── Depends on: xpe_common
├── No other XPE module dependencies
└── IPC launch of xpe_ai_worker.exe

gsvg.dll (Layer 2-G)
├── Independent: no XPE dependencies
└── Standalone build system
```

---

## Dependency Adjacency List

### Who Depends On Whom

| Module | Dependencies |
|--------|--------------|
| **xpe_common** | None (foundation) |
| **xpe_preprocess** | xpe_common |
| **xpe_enhance_basic** | xpe_common |
| **xpe_enhance_advanced** | xpe_common |
| **xpe_display** | xpe_common |
| **xpe_dicom** | xpe_common |
| **xpe_ai** | xpe_common |
| **gsvg** | None (independent) |

### Reverse Dependencies (Fan-In)

| Module | Dependents | Fan-In Count |
|--------|------------|--------------|
| **xpe_common** | All other modules | 7 |
| **xpe_preprocess** | None (leaf) | 0 |
| **xpe_enhance_basic** | None (leaf) | 0 |
| **xpe_enhance_advanced** | None (leaf) | 0 |
| **xpe_display** | None (leaf) | 0 |
| **xpe_dicom** | None (leaf) | 0 |
| **xpe_ai** | None (leaf) | 0 |
| **gsvg** | None (independent) | 0 |

---

## Third-Party Dependencies

### Core Dependencies (All Modules)

| Library | Version | Purpose | Used By |
|---------|---------|---------|---------|
| **spdlog** | 1.13.0+ | High-performance logging | All XPE modules |
| **fmt** | 10.0.0+ | Modern formatting | All XPE modules |
| **nlohmann-json** | 3.11.3+ | JSON parsing | All XPE modules |

### Algorithm-Specific Dependencies

| Library | Version | Purpose | Used By |
|---------|---------|---------|---------|
| **eigen3** | 3.4.0+ | Linear algebra | xpe_enhance_advanced |
| **opencv4** | 4.9.0+ | Computer vision | xpe_enhance_advanced |
| **dcmtk** | 3.6.8+ | DICOM toolkit | xpe_dicom |

### Build & Test Dependencies

| Library | Version | Purpose | Used By |
|---------|---------|---------|---------|
| **gtest** | 1.14.0+ | Testing framework | All test executables |
| **OpenMP** | - | Parallel execution | Performance-critical code |

---

## Dependency Rules

### Rule 1: No Lateral Dependencies
**Status**: ENFORCED

Each XPE module DLL depends ONLY on `xpe_common`. Lateral dependencies are forbidden.

**Verification**:
```bash
dumpbin /dependents <module>.dll
```

Expected: NO other `xpe_*.dll` in the output.

### Rule 2: Independent Deployment
**Status**: ENFORCED

A module DLL must function correctly when only `xpe_common.dll` is present.

**Test**: Load module with only xpe_common in the directory.

### Rule 3: Third-Party Isolation
**Status**: ENFORCED

Third-party libraries are module-specific and not shared across XPE modules (except through xpe_common).

**Exception**: fmt and spdlog are linked by all modules but loaded dynamically.

---

## Circular Dependency Check

### Status: ✅ NO CIRCULAR DEPENDENCIES

The dependency graph is acyclic by design:

```
Layer 0 (xpe_common)
    ↓
Layer 1 (preprocess, enhance_basic, display, dicom)
    ↓
Layer 2 (enhance_advanced, gsvg)
    ↓
Layer 3 (ai)
```

### Verification

**Method**: Topological sort of dependency graph

**Result**: Valid ordering with no back edges

**Confidence**: High (enforced by architecture rules)

---

## Dependency Visualization

### Mermaid Diagram

```mermaid
graph TD
    Common[xpe_common.dll<br/>Foundation]
    
    Pre[xpe_preprocess.dll<br/>Calibration]
    Basic[xpe_enhance_basic.dll<br/>Basic Enhancement]
    Disp[xpe_display.dll<br/>Presentation]
    Dicom[xpe_dicom.dll<br/>DICOM I/O]
    
    Adv[xpe_enhance_advanced.dll<br/>Advanced Algorithms]
    GSVG[gsvg.dll<br/>Grid Suppression<br/>(Independent)]
    
    AI[xpe_ai.dll<br/>AI Processing]
    
    Common --> Pre
    Common --> Basic
    Common --> Disp
    Common --> Dicom
    Common --> Adv
    Common --> AI
    
    Pre -.->|Foundation| Common
    Basic -.->|Foundation| Common
    Disp -.->|Foundation| Common
    Dicom -.->|Foundation| Common
    Adv -.->|Foundation| Common
    AI -.->|Foundation| Common
    
    style Common fill:#e1f5ff
    style Pre fill:#fff4e1
    style Basic fill:#fff4e1
    style Disp fill:#fff4e1
    style Dicom fill:#fff4e1
    style Adv fill:#ffe1f5
    style GSVG fill:#e1ffe1
    style AI fill:#f5e1ff
```

---

## Dependency Management

### vcpkg Integration

All third-party dependencies are managed through vcpkg:

```json
{
  "name": "xpe",
  "version": "3.0.0",
  "dependencies": [
    "spdlog",
    "fmt",
    "nlohmann-json",
    "eigen3",
    "opencv4",
    "dcmtk",
    "gtest"
  ]
}
```

### NativeDependencyLoader

The C# GUI uses `NativeDependencyLoader` to load third-party dependencies:

```csharp
// Load dependencies before attempting to load the module DLL
NativeDependencyLoader.LoadDependencies("xpe_enhance_advanced",
    new[] { "fmt.dll", "spdlog.dll", "eigen3.dll" }
);
```

---

## Dependency Changes (dev/postprocess)

### Recent Modifications

No dependency changes in the current branch.

### Focus Areas

- **Hough Transform**: Uses Eigen3 for matrix operations
- **Feature Detection**: Uses OpenCV4 for edge detection
- **Integration Testing**: Cross-module dependency validation

---

## Next Steps

1. Run `dumpbin /dependents` on all modules to verify independence
2. Test module loading with only xpe_common present
3. Verify third-party DLL loading order
4. Check for hidden dependencies through runtime analysis
