# Technical Stack

**Document ID**: XPE-TECH-001  
**Version**: 1.3.0  
**Date**: 2026-04-16  
**Status**: Controlled Draft  
**Canonical Scope**: `docs/project/`

---

## 1. Purpose

This document defines the implementation stack and engineering rules for XPE.

It answers:

- which languages and runtimes are used,
- which third-party components are allowed,
- which platform assumptions are fixed,
- which engineering rules exist to keep image quality, determinism, and integration quality high.

---

## 2. Language and Runtime Split

| Technology | Role | Notes |
|---|---|---|
| C++17 | native algorithm implementation | primary language for detector correction, enhancement, display, DICOM, and AI proxy layers |
| C11 ABI surface | exported DLL contract | stable boundary for host integration and P/Invoke |
| C# / .NET 8 / WPF | orchestration and QA host | `ImageProcTest.exe`, integration harness, QA workflows |

The project rule remains:

- compute-heavy and latency-sensitive logic in native code,
- orchestration, tooling, and QA workflows in C#,
- no algorithm contract that exists only in the host layer.

---

## 3. Build and Packaging Stack

| Component | Role |
|---|---|
| CMake | native project generation |
| Ninja | preferred native build generator |
| vcpkg manifest mode | third-party dependency pinning |
| Visual Studio 2022 toolchain | primary Windows native compiler environment |
| MSBuild / dotnet | managed host build |
| GitHub Actions | CI validation and delivery bundles |

---

## 4. Approved Third-Party Components

### 4.1 XPE

| Component | Role |
|---|---|
| OpenCV | image operations, filters, utility transforms |
| DCMTK | DICOM file and network handling |
| Eigen | matrix and numeric support |
| ONNX Runtime | assistive AI worker inference runtime |
| spdlog | logging |
| nlohmann/json | JSON configuration and diagnostics |
| fmt | formatting support |
| Google Test | native unit and smoke tests |

### 4.2 GSVG

| Component | Role |
|---|---|
| FFTW3 | transform support where licensed and packaged appropriately |
| OpenCV | image operations |
| Eigen | matrix support |
| DCMTK | DICOM metadata handling |
| nlohmann/json | configuration |

### 4.3 Documentation and Help Toolchain

| Component | Version | Role |
|---|---|---|
| C# XML documentation comments | — | Structured managed API comments and IntelliSense source |
| Doxygen | 1.12+ | Native C/C++ API reference generation from exported headers; `WARN_AS_ERROR = FAIL_ON_WARNINGS` enforces full coverage |
| doxygen-awesome-css | latest | Modern responsive theme for Doxygen HTML output — dark mode, collapsible sidebar, interactive TOC, full-text search |
| DocFX | v2.74+ | Managed API reference + conceptual documentation portal (C#); integrates with Doxygen XML output for cross-reference |
| WebView2 | current Evergreen runtime where available | In-app offline help viewer inside the WPF host using local virtual-host mapping |

Documentation generation is automated via `.github/workflows/docs-generate.yml`:

- Doxygen job runs on every push to `main` / `dev/integration` that touches a public header.
- DocFX job generates the managed API + conceptual site in parallel.
- On release: both artifacts are bundled into a versioned `help-bundle-{version}.zip` and attached to the GitHub Release.

### 4.4 GUI Menu and Commanding Stack

| Component | Role |
|---|---|
| WPF `Menu` | stable top-level command taxonomy for File, Backend, View, Pipeline, Tools, and Help |
| WPF `ToolBarTray` | high-frequency shortcut surface for sprint demo and common operations |
| WPF command pattern | shared command semantics across menu items, toolbar buttons, keyboard shortcuts, automation, and later context surfaces |

The GUI rule is:

- top-level menu groups organize capabilities,
- toolbar buttons duplicate only frequent commands,
- disabled or absent menu items must not imply unsupported native, DICOM, premium, or AI behavior exists,
- every menu command must have an owner sprint, automation ID, help target, and verification path.

---

## 5. Platform Assumptions

| Platform | Status | Notes |
|---|---|---|
| Windows x86-64 | primary | AVX2-capable target assumed |
| Linux x86-64 | secondary / optional | not the primary deployment assumption |
| ARM64 | future / optional | requires explicit SIMD and packaging review |

---

## 6. Engineering Rules

### 6.1 Determinism before optimization

Every major detector or enhancement stage shall have:

- a scalar or reference implementation,
- a parity check against optimized implementations,
- explicit numerical tolerances,
- performance evidence tied to the reference behavior.

### 6.2 ABI stability

The exported DLL boundary shall remain:

- C-compatible,
- pack-stable,
- explicit about ownership and lifetime,
- independent of C++ exceptions or STL types.

### 6.3 Sidecar over metadata overloading

ROI masks, AI confidence, reject-analysis records, and premium-stage diagnostics shall be carried in explicit sidecar or diagnostic structures, not squeezed into generic image metadata fields.

### 6.4 Worker isolation for AI

Inference shall not run as opaque logic inside the deterministic baseline DLL chain. The worker model remains:

- `xpe_ai.dll` as proxy,
- `xpe_ai_worker.exe` as isolated runtime,
- explicit restart and timeout handling.

### 6.5 Integrity and reproducibility

Configuration, calibration manifests, benchmark manifests, and AI model artifacts shall be integrity-checked before use. Release evidence shall remain reproducible against frozen manifest versions and hashes.

### 6.6 Documentation as code

Public and operator-facing documentation shall be treated as build artifacts, not detached side files:

- public C# contracts use `///` XML documentation comments and XML output generation,
- exported native ABI headers use Doxygen-style comments in headers,
- conceptual documentation remains in version-controlled Markdown under `docs/project/`,
- generated Help output is version-matched to the build and packaged for offline use.

Comment rules are intentionally narrow:

- comment public contracts, invariants, units, threading, safety assumptions, and degraded-mode behavior,
- do not narrate obvious line-by-line implementation steps,
- keep algorithm rationale in comments only where the code would otherwise be ambiguous or safety-relevant.

---

## 7. Hardware and Software Partitioning

The current implementation posture remains software-first:

- detector correction and enhancement are implemented on the host,
- future hardware offload may be added only if the software contract remains stable,
- FPGA or MCU offload shall not change the normative pipeline order or quality gates.

This means any future hardware acceleration must preserve:

- canonical stage ordering,
- benchmark comparability,
- the same degraded-mode behavior,
- the same exported ABI and diagnostics contract.

---

## 8. Quality-Driven Technical Priorities

The current technical priorities are:

1. detector-domain correctness,
2. benchmark reproducibility,
3. SIMD acceleration with parity proof,
4. safe optional premium processing,
5. transparent and degradable assistive AI,
6. documentation and Help that stay synchronized with the shipped build.

This order is intentional. It maximizes implementation feasibility while preserving the highest-value image-quality gains.
