# SPEC-XPE-P2-ADV: Advanced Post-Processing Module

---
id: SPEC-XPE-P2-ADV
version: 1.0.0
status: Planned
created: 2026-04-17
updated: 2026-04-17
author: manager-spec (MoAI)
priority: Must
issue_number: null
iec62304_class: B
development_mode: TDD
sprint: S2-A
dependency: S1-B1 (xpe_enhance_basic)
---

## HISTORY

| Version | Date       | Author       | Changes                        |
|---------|------------|--------------|--------------------------------|
| 1.0.0   | 2026-04-17 | manager-spec | Initial EARS SPEC creation for Sprint S2-A |

---

## 1. Scope

### 1.1 Overview

xpe_enhance_advanced.dll(Layer 1, Phase 2)의 핵심 고급 후처리 알고리즘을 구현한다. 본 모듈은 xpe_enhance_basic.dll(S1-B1)에서 log transform 및 기본 CLAHE가 완료된 enhancement-domain(float32) 이미지를 입력으로 받아 다음 3개 SWU를 처리한다:

- **SWU-2.5**: Multiscale Frequency Processing (MFP) -- Laplacian pyramid 분해, 주파수 대역별 계수 조정, 재구성
- **SWU-2.6**: Edge Enhancement -- Unsharp masking 기반 엣지 강조, mandatory overshoot limiting, fractional-order differentiation
- **SWU-2.8**: ROI-Aware Collimation Detection -- Hough 변환 기반 collimation 영역 검출, confidence scoring

추가로 **SWU-2.10**(Exposure Index Calculation)의 ROI 기반 EI/DI 계산을 지원한다.

본 모듈은 xpe_common.dll(Layer 0)에만 의존하며, Eigen 3.4.x를 내부 행렬 연산에 사용한다. 다른 Layer 1 DLL과는 상호 의존하지 않는다(Anti-Spaghetti 원칙).

### 1.2 In Scope

- **POST-05**: Multiscale Frequency Processing (SWU-2.5) -- Laplacian pyramid 분해/재구성, 주파수 대역별 enhancement 계수, body-part adaptive parameter
- **POST-06**: Edge Enhancement (SWU-2.6) -- Unsharp masking, mandatory overshoot limiting (+-3sigma), multi-band edge enhancement, fractional-order differentiation
- **POST-07 baseline**: Collimation ROI Detection (SWU-2.8) -- Hough 변환, axis-aligned line detection, confidence scoring, JSON sidecar
- **SUP-03**: Exposure Index Calculation (SWU-2.10) -- IEC 62494-1 EI/DI, ROI-aware masking, QC alerting
- api-spec.md v1.2.0 Section 8의 4개 함수 (P2-ADV 범위)

### 1.3 Exclusions (What NOT to Build)

- **POST-05 DL variant**: AI 기반 multiscale processing -- xpe_ai.dll(Phase 3) 범위
- **POST-07 AI variant**: AI 기반 collimation detection -- xpe_ai.dll(Phase 3) 범위
- **POST-09**: Bone Suppression (U-Net) -- xpe_ai.dll(Phase 3) 범위
- **POST-11**: Virtual Grid (GSVG) -- gsvg.dll(Sprint S2-B) 범위, 별도 IEC 62304 패키지
- **GPU offload**: CUDA/OpenCL 가속 -- Phase 3 최적화 단계
- **NLM Tier 3 denoising**: 본 모듈은 MFP 기반 noise reduction 사용. NLM은 SRS-ENHANCE-ADV-001에 정의되었으나 Sprint Roadmap v2.0에서 MFP 우선 범위로 축소
- **Wavelet Tier 4 denoising**: Phase 3 연구 모드
- **Wiener Filter (Tier 0)**: Research-only, 본 sprint 범위 외
- **Log Transform, 기본 CLAHE**: xpe_enhance_basic.dll(S1-B1) 범위
- **Display pipeline (LUT)**: xpe_display.dll(S1-B2) 범위

---

## 2. Referenced Documents

| Document ID            | Title                                           | Version | Role                     |
|------------------------|-------------------------------------------------|---------|--------------------------|
| XPE-API-SPEC-001       | XPE API Specification                           | 1.2.0   | API contract definition  |
| SRS-ENHANCE-ADV-001    | Software Requirements Specification (Adv Enhance)| 1.0    | Functional requirements  |
| xpe-enhance-advanced-prd | Algorithm PRD (Adv Enhancement)              | 1.0.0   | Algorithm reference      |
| SAD-ENHANCE-ADV-001    | Software Architecture Description               | Draft   | Architecture reference   |
| XPE-ALG-001            | Unified Algorithm Development Specification     | Draft   | Mathematical formulas    |
| XPE-TERM-001           | Terminology and Acronym Control                 | 1.0.0   | Terminology standard     |
| SPRINT-ROADMAP-001     | Sprint Execution Roadmap v2.0                   | 2.0.0   | Sprint planning          |
| SPEC-XPE-P1A           | Pre-Processing Module SPEC                      | 1.0.0   | Pattern reference        |
| IEC-62494-1            | Exposure Index Standard                         | 2008    | EI/DI normative ref      |

---

## 3. Definitions and Acronyms

| Term     | Definition                                                        |
|----------|-------------------------------------------------------------------|
| MFP      | Multiscale Frequency Processing (다중 스케일 주파수 처리)           |
| MTF      | Modulation Transfer Function (변조 전달 함수)                       |
| EI       | Exposure Index (노출 지수, IEC 62494-1)                              |
| DI       | Deviation Index (편차 지수, IEC 62494-1)                             |
| ROI      | Region of Interest (관심 영역)                                      |
| SWU      | Software Unit (IEC 62304 소프트웨어 유닛)                           |
| SOUP     | Software of Unknown Provenance (IEC 62304)                        |
| FPD      | Flat Panel Detector (평판 디텍터)                                    |
| Laplacian Pyramid | Multi-scale image decomposition via successive Gaussian blur and subtraction |
| Overshoot Limiting | Mandatory safety mechanism to clip edge enhancement artifacts  |
| Fractional Derivative | Generalized derivative of non-integer order for texture enhancement |
| Sidecar  | Auxiliary JSON file accompanying an image file with metadata       |
| Confidence Score | [0.0, 1.0] metric quantifying ROI detection reliability           |

---

## 4. Requirements (EARS Format)

### 4.1 Ubiquitous Requirements (항상 활성)

#### REQ-ADV-001: Module Initialization

The enhance_advanced module **shall** initialize its internal state when `xpe_enhance_advanced_init()` is called with valid configuration, and report `XPE_OK` on success.

- **SRS**: SRS-ADV-INIT-001
- **Traceability**: SWU-2.5, SWU-2.6, SWU-2.8, SWU-2.10
- **Note**: Lifecycle management follows xpe_common pattern (g_initialized flag, std::mutex)

#### REQ-ADV-002: P/Invoke ABI Compliance

The enhance_advanced module **shall** export all functions with `extern "C"` linkage, `__cdecl` calling convention, and `#pragma pack(push, 8)` struct alignment compatible with C# `[StructLayout(Pack = 8)]`.

- **SRS**: SRS-ABI-001
- **Traceability**: All SWUs
- **Verification**: `static_assert` for struct sizes, P/Invoke integration test

---

### 4.2 Event-Driven Requirements (이벤트 구동)

#### REQ-ADV-010: Multiscale Frequency Processing Execution

**When** `xpe_multiscale_process(img, meta, configJsonOrNull)` is called with valid inputs, the module **shall** decompose `img` into a Laplacian pyramid (3-4 levels by default), apply per-band enhancement coefficients derived from `meta->bodyPart` and configuration, and reconstruct the image in-place.

- **SRS**: SRS-ADV-001, SRS-ADV-002
- **Traceability**: POST-05, SWU-2.5
- **Algorithm**: Laplacian pyramid decomposition: `L_k = G_k - Expand(G_{k+1})`, where G_k is Gaussian pyramid level k. Enhancement: `L'_k = alpha_k * L_k` with body-part-specific alpha_k. Reconstruction: `G'_k = L'_k + Expand(G'_{k+1})`.
- **Performance**: < 800ms for 3072x3072 FLOAT32 frame

#### REQ-ADV-011: Fractional-Order Process Execution

**When** `xpe_fractional_process(img, order, configJsonOrNull)` is called with `order` in range [0.0, 2.0], the module **shall** apply a fractional-order differentiation operator in-place, where values near 1.0 preserve edges and values near 2.0 emphasize fine texture.

- **SRS**: SRS-ADV-010
- **Traceability**: POST-06, SWU-2.6
- **Performance**: < 400ms for 3072x3072 FLOAT32 frame

#### REQ-ADV-012: Collimation Detection Execution

**When** `xpe_detect_collimation(img, x0Out, y0Out, x1Out, y1Out, configJsonOrNull)` is called with valid inputs, the module **shall** detect collimation boundaries using edge detection followed by Hough line transform, filter for axis-aligned lines (theta within +-5 degrees of 0 or 90), and output the bounding rectangle as pixel coordinates.

- **SRS**: SRS-ADV-020, SRS-SAFE-015
- **Traceability**: POST-07, SWU-2.8
- **Performance**: < 500ms for 3072x3072 FLOAT32 frame

#### REQ-ADV-013: Exposure Index Calculation Execution

**When** `xpe_calc_exposure_index(img, meta, eiOut, deviationIndexOut)` is called with valid detector-domain image data, the module **shall** compute the IEC 62494-1 Exposure Index (EI) and Deviation Index (DI), writing results to `*eiOut` and `*deviationIndexOut`.

- **SRS**: SRS-ADV-030, SRS-SAFE-016
- **Traceability**: SUP-03, SWU-2.10
- **Algorithm**: `EI = c1 * g * mean(pixel_values_roi) + c2` (IEC 62494-1). `DI = 10 * log10(EI / EI_target)`. EI_target from body-part lookup table.

---

### 4.3 State-Driven Requirements (상태 구동)

#### REQ-ADV-020: Not-Initialized Guard

**While** the module is not initialized, all processing functions **shall** return `XPE_ERR_NOT_INITIALIZED` without modifying any output parameters.

- **SRS**: SRS-INIT-003
- **Traceability**: All SWUs

#### REQ-ADV-021: Invalid Order Parameter Guard

**While** `order` parameter is outside range [0.0, 2.0] in `xpe_fractional_process`, the module **shall** return `XPE_ERR_INVALID_INPUT` without modifying the image buffer.

- **SRS**: SRS-ADV-010
- **Traceability**: SWU-2.6

#### REQ-ADV-022: NULL Pointer Input Guard

**While** any required pointer parameter (`img`, `meta`, `x0Out`, etc.) is NULL, the called function **shall** return `XPE_ERR_INVALID_INPUT` without modifying any output parameters.

- **SRS**: SRS-SAFE-001
- **Traceability**: All SWUs

---

### 4.4 Unwanted Behavior Requirements (금지 동작)

#### REQ-ADV-030: No Exceptions Across C ABI

The module **shall not** allow any C++ exception to propagate across the C ABI boundary. All internal exceptions shall be caught and converted to appropriate `XpeErrorCode` values.

- **IEC 62304**: Class B requirement
- **Traceability**: All SWUs

#### REQ-ADV-031: No Memory Leak

The module **shall not** leak any heap-allocated memory. All temporary allocations (pyramid buffers, coefficient arrays, FFT work buffers) during processing shall be freed before function return, including error paths.

- **IEC 62304**: Class B requirement
- **Traceability**: All SWUs
- **Verification**: 1000-cycle allocation/free test + ASan

#### REQ-ADV-032: No NaN/Inf in Output

The module **shall not** produce NaN or Inf values in output image buffers. All floating-point operations in multiscale decomposition, fractional differentiation, and reconstruction shall include validation to clamp or replace invalid values.

- **SRS**: SRS-SAFE-020
- **Traceability**: SWU-2.5, SWU-2.6

---

### 4.5 Optional Requirements (선택 사항)

#### REQ-ADV-040: AVX2 SIMD Optimization

**Where** AVX2 is available at runtime, the module **shall** use AVX2 intrinsics for performance-critical operations (Laplacian pyramid computation, convolution kernels, edge detection gradients) while maintaining numerical parity with the scalar reference implementation (error < 1e-6 for FLOAT32).

- **SRS**: SRS-PERF-001
- **Traceability**: SWU-2.5, SWU-2.6, SWU-2.8
- **Cross-validation**: R8-04 NLM performance risk mitigation via AVX2 SIMD

#### REQ-ADV-041: Confidence-Based ROI Fallback

**Where** collimation detection confidence is below 0.7, the module **shall** return the full image extent as the ROI and log a warning: "ROI confidence (%.2f) below threshold 0.7. Using full-image extent." The function shall not return an error in this case.

- **SRS**: SRS-ADV-025
- **Traceability**: SWU-2.8

---

### 4.6 Algorithm Correctness Requirements (알고리즘 정확성)

#### REQ-ADV-050: Laplacian Pyramid Reconstruction Fidelity

**When** a Laplacian pyramid is decomposed and reconstructed with identity enhancement coefficients (all alpha_k = 1.0), the module **shall** produce output identical to the input within FLOAT32 precision (max absolute error < 1e-5).

- **SRS**: SRS-ADV-002
- **Traceability**: SWU-2.5 (MFP)

#### REQ-ADV-051: Overshoot Limiting Enforcement

**When** edge enhancement is applied, the module **shall** clip enhancement boost at +-3*sigma_local for every pixel, where sigma_local is the standard deviation of a 3x3 neighborhood. This is mandatory and non-configurable. Attempting to disable shall return `XPE_ERR_SAFETY_VIOLATION`.

- **SRS**: SRS-SAFE-010, FR-900.1
- **Traceability**: SWU-2.6 (Edge Enhancement)
- **Safety**: SAF-100 (mandatory safeguard)

#### REQ-ADV-052: Collimation Detection Accuracy

**When** a synthetic image with known rectangular collimation borders is processed, the detected ROI coordinates **shall** match the ground truth within +-3 pixels on each edge.

- **SRS**: SRS-ADV-020
- **Traceability**: SWU-2.8 (ROI Detection)

---

### 4.7 Performance Budget Requirements (성능 예산)

#### REQ-ADV-060: MFP Performance Budget

**When** `xpe_multiscale_process` is called on a 3072x3072 FLOAT32 image, execution **shall** complete within 800ms (scalar) or 250ms (AVX2) on reference hardware (Intel Core i7 2.6GHz).

- **SRS**: PERF-100
- **Traceability**: SWU-2.5
- **Budget contribution**: MFP is the primary time consumer in the < 2500ms total advanced pipeline budget

#### REQ-ADV-061: Edge Enhancement Performance Budget

**When** `xpe_fractional_process` is called on a 3072x3072 FLOAT32 image, execution **shall** complete within 400ms (scalar) or 120ms (AVX2).

- **SRS**: PERF-100
- **Traceability**: SWU-2.6

#### REQ-ADV-062: Total Pipeline Performance Budget

**When** the full advanced enhancement pipeline (MFP + Edge Enhancement + Collimation Detection + EI Calculation) is executed on a 3072x3072 FLOAT32 image, total processing time **shall** be less than 2500ms.

- **SRS**: PERF-101, Sprint Roadmap v2.0 Quality Gates
- **Traceability**: All SWUs

---

### 4.8 Error Handling Requirements (오류 처리)

#### REQ-ADV-070: Dimension Validation

**When** any processing function receives an image with zero width or height, the function **shall** return `XPE_ERR_INVALID_INPUT` without processing.

- **SRS**: SRS-SAFE-003
- **Traceability**: All SWUs

#### REQ-ADV-071: Format Validation

**When** any processing function receives an image with pixel format other than `XPE_PIXEL_FLOAT32`, the function **shall** return `XPE_ERR_UNSUPPORTED_FORMAT`. The advanced module operates exclusively in the enhancement domain (float32).

- **SRS**: SRS-SAFE-004
- **Traceability**: All SWUs

---

### 4.9 Memory Management Requirements (메모리 관리)

#### REQ-ADV-080: Peak Memory Budget

**While** processing a 3072x3072 FLOAT32 image, the module **shall** not allocate more than 200MB of temporary memory across all processing stages. Laplacian pyramid buffers shall be reused across levels where possible.

- **SRS**: PERF-102
- **Traceability**: SWU-2.5

#### REQ-ADV-081: Memory Release After Processing

**When** a processing function completes (success or error), the module **shall** release all temporary heap allocations. Memory usage shall return to baseline after each frame.

- **SRS**: PERF-103
- **Traceability**: All SWUs
- **Verification**: 100-frame batch test with memory monitoring

---

### 4.10 Cross-Cutting Requirements (횡단 요구사항)

#### REQ-ADV-090: Thread Safety

All processing functions **shall** be reentrant with independent caller-supplied buffers. No global mutable state shall be modified during processing calls. The g_initialized flag is the only shared state and shall be protected by std::mutex.

- **SRS**: SRS-THREAD-001
- **Traceability**: All SWUs

#### REQ-ADV-091: Diagnostic Logging

**When** any processing function completes, the module **shall** log per-stage execution time, selected parameters, and decision points to the xpe_common logging subsystem (spdlog). Log format: `{"stage":"MFP","levels":4,"time_ms":450,"body_part":"CHEST"}`.

- **SRS**: FR-1800.1
- **Traceability**: All SWUs

---

### 4.11 Boundary Condition Requirements (경계 조건)

#### REQ-ADV-100: Single-Pixel Image Rejection

**When** a 1x1 image is passed to any processing function, the function **shall** return `XPE_ERR_INVALID_INPUT` because multiscale decomposition and neighborhood operations require minimum dimensions.

- **SRS**: SRS-SAFE-005
- **Traceability**: SWU-2.5, SWU-2.6, SWU-2.8

#### REQ-ADV-101: Maximum Image Size Support

**When** a 4096x4096 FLOAT32 image is processed, all functions **shall** complete successfully within 1.5x the 3072x3072 performance budget without memory allocation failure.

- **SRS**: SRS-PERF-010
- **Traceability**: All SWUs

---

## 5. API Function Summary

본 SPEC에서 구현하는 api-spec.md Section 8의 4개 함수:

### 5.1 Multiscale Processing (1 function)

| # | Function                        | SRS          | Priority |
|---|---------------------------------|--------------|----------|
| 1 | `xpe_multiscale_process()`      | SRS-ADV-001  | High     |

### 5.2 Edge Enhancement (1 function)

| # | Function                        | SRS          | Priority |
|---|---------------------------------|--------------|----------|
| 2 | `xpe_fractional_process()`      | SRS-ADV-010  | High     |

### 5.3 Collimation Detection (1 function)

| # | Function                        | SRS          | Priority |
|---|---------------------------------|--------------|----------|
| 3 | `xpe_detect_collimation()`      | SRS-ADV-020  | High     |

### 5.4 Exposure Index (1 function)

| # | Function                        | SRS          | Priority |
|---|---------------------------------|--------------|----------|
| 4 | `xpe_calc_exposure_index()`     | SRS-ADV-030  | High     |

### 5.5 API Signatures

```c
// SWU-2.5: Multiscale Frequency Processing (REQ-ADV-010)
XPE_API XpeErrorCode xpe_multiscale_process(
    XpeImageBuffer* img,
    const XpeImageMetadata* meta,
    const char* configJsonOrNull);

// SWU-2.6: Fractional-Order Edge Enhancement (REQ-ADV-011)
XPE_API XpeErrorCode xpe_fractional_process(
    XpeImageBuffer* img,
    float order,
    const char* configJsonOrNull);

// SWU-2.8: Collimation ROI Detection (REQ-ADV-012)
XPE_API XpeErrorCode xpe_detect_collimation(
    const XpeImageBuffer* img,
    int32_t* x0Out, int32_t* y0Out,
    int32_t* x1Out, int32_t* y1Out,
    const char* configJsonOrNull);

// SWU-2.10: Exposure Index Calculation (REQ-ADV-013)
XPE_API XpeErrorCode xpe_calc_exposure_index(
    const XpeImageBuffer* img,
    const XpeImageMetadata* meta,
    float* eiOut,
    float* deviationIndexOut);
```

---

## 6. Performance Targets

출처: Sprint Roadmap v2.0 Quality Gates, 3072x3072 FLOAT32 기준

| Algorithm                      | Target (Scalar) | Target (AVX2) | Sprint Budget |
|--------------------------------|:---------------:|:--------------:|:-------------:|
| MFP (Laplacian Pyramid)        | < 800ms        | < 250ms       | < 2500ms total |
| Edge Enhancement (Fractional)  | < 400ms        | < 120ms       |               |
| Collimation Detection          | < 500ms        | < 200ms       |               |
| EI Calculation                 | < 50ms         | < 20ms        |               |
| **Total Pipeline**             | **< 2500ms**   | **< 600ms**   |               |

---

## 7. Quality Requirements

| Attribute       | Target                                    |
|-----------------|-------------------------------------------|
| Test Coverage   | >= 85% statement coverage                 |
| Branch Coverage | >= 70%                                    |
| Testing Method  | TDD (RED-GREEN-REFACTOR)                  |
| SIMD Parity     | Scalar vs AVX2 numerical equivalence (error < 1e-6) |
| IEC 62304 Class | B (medical device software)               |
| Thread Safety   | All processing functions reentrant        |
| Memory Safety   | Zero leaks in 1000-cycle endurance test   |
| Static Analysis | 0 warnings                                |

---

## 8. Dependency Constraints

### 8.1 Allowed Dependencies

- xpe_common.dll (Layer 0)
- Eigen 3.4.x (matrix operations, FFT for pyramid computation)
- spdlog 1.13.x (logging via xpe_common)
- nlohmann/json 3.11.x (configuration parsing)
- fmt 10.x (string formatting)
- Google Test 1.14.x (testing)

### 8.2 Forbidden Dependencies

- OpenCV (enhance_basic에서만 사용)
- ONNX Runtime (ai 모듈 전용)
- DCMTK (dicom 모듈 전용)
- FFTW3 (gsvg 모듈 전용, GPL)

---

*Document End - SPEC-XPE-P2-ADV v1.0.0*
