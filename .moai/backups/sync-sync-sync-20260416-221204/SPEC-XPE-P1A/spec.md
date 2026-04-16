# SPEC-XPE-P1A: Pre-processing Module (Gain/Offset/Defect Correction)

---
id: SPEC-XPE-P1A
version: 1.0.0
status: Planned
created: 2026-04-16
updated: 2026-04-16
author: manager-spec (MoAI)
priority: High
issue_number: 16
iec62304_class: B
development_mode: TDD
---

## HISTORY

| Version | Date       | Author  | Changes                  |
|---------|------------|---------|--------------------------|
| 1.0.0   | 2026-04-16 | manager-spec | Initial SPEC creation |

---

## 1. Scope

### 1.1 Overview

xpe_preprocess.dll의 핵심 전처리 알고리즘 3종(SWU-1.1 Offset Correction, SWU-1.2 Gain Correction, SWU-1.3 Defect Correction)과 이를 지원하는 Calibration Data Management(SUP-01)를 구현한다.

이 모듈은 xpe_common.dll(Layer 0)에만 의존하는 Layer 1 알고리즘 DLL로서, C# GUI(ImageProcTest)에서 P/Invoke로 호출된다.

### 1.2 In Scope

- **PRE-02**: Offset/Dark Correction (SWU-1.1) -- 온도 보간, PREP-time 모델, saturating subtraction
- **PRE-03**: Gain/Flat-Field Correction (SWU-1.2) -- UINT16/FLOAT32 포맷 변환, reciprocal gain map, NaN/Inf validation
- **PRE-06**: Defective Pixel Correction (SWU-1.3 baseline) -- Edge-aware bilinear interpolation, static BPM + runtime detection
- **SUP-01**: Calibration Parameter Management -- XCal 포맷 로딩, SHA-256 무결성 검증, session matching, 만료 확인
- API 명세서(api-spec.md v1.3.0) Section 6의 18개 함수 중 본 SPEC 범위 14개 함수 (Ghost/Temp/Nonlinearity/Binning 제외)

### 1.3 Exclusions (What NOT to Build)

- **PRE-04/PRE-05**: Ghost/Lag Correction (SWU-1.4) -- 별도 SPEC으로 분리 (상태유지 handle 기반 아키텍처)
- **PRE-07**: Temperature Compensation (SWU-1.6) -- 별도 SPEC, MCU 이관 가능 구조 필요
- **PRE-08**: Nonlinearity Correction -- 별도 SPEC
- **PRE-09**: Pixel Binning Correction -- 별도 SPEC, 형광투시/CBCT 전용
- **PRE-06 ML/ViT AE**: 고급 defect correction -- Phase 2 Differentiator
- GPU offload (CUDA/OpenCL) -- Phase 3 최적화 단계
- ONNX Runtime 추론 -- AI 모듈(xpe_ai.dll) 범위
- OpenCV 의존성 -- preprocess 모듈은 xpe_common만 의존

---

## 2. Referenced Documents

| Document ID            | Title                                      | Version | Role                  |
|------------------------|--------------------------------------------|---------|-----------------------|
| XPE-API-SPEC-001       | XPE API Specification                      | 1.1.0   | API 계약 정의           |
| XPE-SRS-001            | Software Requirements Specification        | Draft   | 기능 요구사항 원천       |
| XPE-SAD-001            | Software Architecture Description          | Draft   | 아키텍처 참조           |
| XPE-ALG-001            | Algorithm Technical Reference              | Draft   | 수학적 공식 및 성능 기준  |
| XPE-TERM-001           | Terminology and Acronym Control            | 1.0.0   | 용어 표준               |
| SPEC-XPE-P1A-RESEARCH  | Research Report                            | 1.0.0   | 코드베이스 분석 결과     |

---

## 3. Definitions and Acronyms

| Term        | Definition                                                   |
|-------------|--------------------------------------------------------------|
| FPD         | Flat Panel Detector (평판 디텍터)                              |
| BPM         | Bad Pixel Map (불량 픽셀 맵)                                   |
| SWU         | Software Unit (IEC 62304 소프트웨어 유닛)                      |
| SOUP        | Software of Unknown Provenance (IEC 62304)                   |
| Gain Map    | Flat-field correction map, per-pixel multiplication factor     |
| Offset Map  | Dark-field correction map, per-pixel dark signal reference     |
| Defect Map  | Boolean map indicating defective pixel locations               |
| XCal        | XPE Calibration file format (magic: "XCal")                   |
| PREP-time   | Time since last detector reset/erase                          |

---

## 4. Requirements (EARS Format)

### 4.1 Ubiquitous Requirements (항상 활성)

#### REQ-P1A-001: Module Initialization

The preprocess module **shall** initialize its internal state when `xpe_preprocess_init()` is called with valid configuration, and report `XPE_OK` on success.

- **SRS**: SRS-INIT-001, SRS-INIT-002
- **Traceability**: SWU-1.1, SWU-1.2, SWU-1.3

#### REQ-P1A-002: P/Invoke ABI Compliance

The preprocess module **shall** export all functions with `extern "C"` linkage, `__cdecl` calling convention, and `#pragma pack(push, 8)` struct alignment compatible with C# `[StructLayout(Pack = 8)]`.

- **SRS**: SRS-ABI-001
- **Traceability**: All SWUs
- **Verification**: `static_assert` for struct sizes, P/Invoke integration test

#### REQ-P1A-003: Thread Safety

All processing functions **shall** be reentrant with independent caller-supplied buffers. No global mutable state shall be modified during processing calls.

- **SRS**: SRS-THREAD-001
- **Traceability**: All SWUs

#### REQ-P1A-004: Error Code Consistency

All exported functions **shall** return `XpeErrorCode` (`int32_t`) values from the defined error code set (XPE_OK through XPE_ERR_NETWORK_FAILED). No function shall return undocumented error codes.

- **SRS**: SRS-ERR-001
- **Traceability**: All SWUs

#### REQ-P1A-005: Input Validation

Every exported function **shall** validate all pointer parameters for non-NULL and all dimension parameters for non-zero before accessing any data. Invalid inputs shall return `XPE_ERR_INVALID_INPUT` without modifying output parameters.

- **SRS**: SRS-SAFE-001
- **Traceability**: All SWUs

### 4.2 Event-Driven Requirements (이벤트 구동)

#### REQ-P1A-010: Offset Correction Execution

**When** `xpe_offset_correct(img, offsetMap)` is called with valid inputs of matching dimensions and format, the module **shall** subtract the per-pixel dark offset from `img` in-place, clamping the result to zero (floor-at-zero behavior).

- **SRS**: SRS-CALIB-001
- **Traceability**: PRE-02, SWU-1.1
- **Algorithm**: `I_offset(x,y) = max(I_raw(x,y) - I_dark(x,y), 0)`
- **Performance**: < 55ms for 3072x3072 UINT16 frame

#### REQ-P1A-011: Gain Correction Execution

**When** `xpe_gain_correct(img, gainMap)` is called with valid inputs of matching dimensions and format, the module **shall** multiply each pixel by the corresponding gain factor in-place.

- **SRS**: SRS-CALIB-002
- **Traceability**: PRE-03, SWU-1.2
- **Algorithm**: `I_corrected(x,y) = I_offset(x,y) * G(x,y)` where `G(x,y) = mean(I_flat) / (I_flat(x,y) - I_dark(x,y))`
- **Performance**: < 55ms for 3072x3072 UINT16 frame

#### REQ-P1A-012: Defect Correction Execution

**When** `xpe_defect_correct(img, defectMap, configJsonOrNull)` is called with valid inputs, the module **shall** replace all defective pixel values (where defectMap is non-zero) with interpolated values from valid neighboring pixels using the configured interpolation method (nearest/bilinear/median, default: bilinear).

- **SRS**: SRS-CALIB-003, SRS-CALIB-004
- **Traceability**: PRE-06, SWU-1.3
- **Performance**: < 95ms for 3072x3072 UINT16 frame

#### REQ-P1A-013: Runtime Defect Detection

**When** `xpe_defect_detect_runtime(img, defectMapOut, configJsonOrNull)` is called, the module **shall** analyze the input image to identify transient defect pixels and write a boolean defect map to `defectMapOut`.

- **SRS**: SRS-CALIB-005
- **Traceability**: PRE-06, SWU-1.3

#### REQ-P1A-014: Calibration File Loading (Offset)

**When** `xpe_calib_load_offset(filePath, offsetMapOut)` is called with a valid XCal file path, the module **shall** parse the XCal header, validate SHA-256 integrity, check expiry, and load the offset map data into the pre-allocated `offsetMapOut`.

- **SRS**: SRS-CALIB-010
- **Traceability**: SUP-01

#### REQ-P1A-015: Calibration File Loading (Gain)

**When** `xpe_calib_load_gain(filePath, gainMapOut)` is called with a valid XCal file path, the module **shall** parse, validate, and load the gain map data into `gainMapOut`.

- **SRS**: SRS-CALIB-011
- **Traceability**: SUP-01

#### REQ-P1A-016: Calibration File Loading (Defect Map)

**When** `xpe_calib_load_defect_map(filePath, defectMapOut)` is called with a valid XCal file path, the module **shall** load the boolean defect map into `defectMapOut`.

- **SRS**: SRS-CALIB-012
- **Traceability**: SUP-01

#### REQ-P1A-017: Calibration Offset Generation

**When** `xpe_calib_generate_offset(frames, frameCount, offsetMapOut, configJsonOrNull)` is called, the module **shall** compute the pixel-wise mean of `frameCount` dark-field frames and write the result to `offsetMapOut`.

- **SRS**: SRS-CALIB-020
- **Traceability**: SUP-01

#### REQ-P1A-018: Calibration Expiry Check

**When** `xpe_calib_check_expiry(filePath, expiryEpochMsOut)` is called, the module **shall** read the embedded expiry timestamp and return `XPE_ERR_CALIBRATION_EXPIRED` if the timestamp is in the past.

- **SRS**: SRS-CALIB-030, SRS-SAFE-010
- **Traceability**: SUP-01

#### REQ-P1A-019: Calibration Save

**When** `xpe_calib_save(calibMap, filePath, expiryEpochMs, configJsonOrNull)` is called, the module **shall** write the calibration map to `filePath` in XCal format with the specified expiry timestamp.

- **SRS**: SRS-CALIB-021
- **Traceability**: SUP-01

### 4.3 State-Driven Requirements (상태 구동)

#### REQ-P1A-020: Not-Initialized Guard

**While** the module is not initialized (`xpe_preprocess_init()` not called or after `xpe_preprocess_shutdown()`), all processing functions **shall** return `XPE_ERR_NOT_INITIALIZED` without modifying any output parameters.

- **SRS**: SRS-INIT-003
- **Traceability**: All SWUs

#### REQ-P1A-021: Dimension Mismatch Guard

**While** input and map buffer dimensions differ, all correction functions **shall** return `XPE_ERR_INVALID_INPUT` without modifying the output buffer.

- **SRS**: SRS-SAFE-003
- **Traceability**: SWU-1.1, SWU-1.2, SWU-1.3

#### REQ-P1A-022: Format Mismatch Guard

**While** input and map buffer pixel formats differ, all correction functions **shall** return `XPE_ERR_UNSUPPORTED_FORMAT`.

- **SRS**: SRS-SAFE-004
- **Traceability**: SWU-1.1, SWU-1.2

### 4.4 Unwanted Behavior Requirements (금지 동작)

#### REQ-P1A-030: No Exceptions Across C ABI

The module **shall not** allow any C++ exception to propagate across the C ABI boundary. All internal exceptions shall be caught and converted to appropriate `XpeErrorCode` values.

- **IEC 62304**: Class B requirement
- **Traceability**: All SWUs

#### REQ-P1A-031: No Memory Leak

The module **shall not** leak any heap-allocated memory. All temporary allocations during processing shall be freed before function return, including error paths.

- **IEC 62304**: Class B requirement
- **Traceability**: All SWUs
- **Verification**: 1000-cycle allocation/free test (reference: `modules/common/tests/test_xpe_common.cpp`)

#### REQ-P1A-032: No Uninitialized Output

The module **shall not** leave output buffers in a partially initialized state on error. On failure, the module shall either leave the output unmodified or zero-fill it entirely.

- **IEC 62304**: Class B requirement
- **Traceability**: All SWUs

#### REQ-P1A-033: No NaN/Inf in Output

The module **shall not** produce NaN or Inf values in output image buffers. All floating-point operations shall include validation to clamp or replace invalid values.

- **SRS**: SRS-SAFE-020
- **Traceability**: SWU-1.2 (gain correction)

### 4.5 Optional Requirements (선택 사항)

#### REQ-P1A-040: SIMD Optimization

**Where** AVX2 is available at runtime, the module **shall** use AVX2 intrinsics for performance-critical operations (offset subtraction, gain multiplication, defect interpolation) while maintaining bit-exact parity with the scalar reference implementation.

- **SRS**: SRS-PERF-001
- **Traceability**: SWU-1.1, SWU-1.2, SWU-1.3

#### REQ-P1A-041: Readout Artifact Validation

**Where** the caller provides a raw frame, the module **shall** validate it for line noise, dropped columns, and ADC saturation patterns via `xpe_validate_readout_artifact()`.

- **SRS**: SRS-PERF-001
- **Traceability**: PRE-01

#### REQ-P1A-042: Parameter Range Query

**Where** the caller requests valid parameter ranges, the module **shall** return body-part-specific parameter limits via `xpe_preprocess_get_param_range()`.

- **SRS**: SRS-SAFE-002, SRS-SAFE-005
- **Traceability**: SUP-01

---

## 5. API Function Summary

본 SPEC에서 구현하는 14개 함수 (api-spec.md Section 6의 18개 중 P1A 범위):

### 5.1 Lifecycle (2 functions)

| #  | Function                      | SRS          | Priority |
|----|-------------------------------|--------------|----------|
| 1  | `xpe_preprocess_init()`       | SRS-INIT-001 | High     |
| 2  | `xpe_preprocess_shutdown()`   | SRS-INIT-003 | High     |

### 5.2 Calibration Loading (3 functions)

| #  | Function                      | SRS           | Priority |
|----|-------------------------------|---------------|----------|
| 3  | `xpe_calib_load_offset()`     | SRS-CALIB-010 | High     |
| 4  | `xpe_calib_load_gain()`       | SRS-CALIB-011 | High     |
| 5  | `xpe_calib_load_defect_map()` | SRS-CALIB-012 | High     |

### 5.3 Correction Processing (4 functions)

| #  | Function                      | SRS                     | Priority |
|----|-------------------------------|-------------------------|----------|
| 6  | `xpe_offset_correct()`        | SRS-CALIB-001           | High     |
| 7  | `xpe_gain_correct()`          | SRS-CALIB-002           | High     |
| 8  | `xpe_defect_correct()`        | SRS-CALIB-003, SRS-CALIB-004 | High |
| 9  | `xpe_defect_detect_runtime()` | SRS-CALIB-005           | Medium   |

### 5.4 Calibration Management (4 functions)

| #  | Function                      | SRS           | Priority |
|----|-------------------------------|---------------|----------|
| 10 | `xpe_calib_generate_offset()` | SRS-CALIB-020 | Medium   |
| 11 | `xpe_calib_check_expiry()`    | SRS-CALIB-030 | High     |
| 12 | `xpe_calib_save()`            | SRS-CALIB-021 | Medium   |
| 13 | `xpe_validate_readout_artifact()` | SRS-PERF-001 | Low    |

### 5.5 Utility (1 function)

| #  | Function                          | SRS          | Priority |
|----|-----------------------------------|--------------|----------|
| 14 | `xpe_preprocess_get_param_range()` | SRS-SAFE-002 | Medium   |

### 5.6 Excluded Functions (Phase 2+)

| Function                    | Reason                          | Target SPEC  |
|-----------------------------|---------------------------------|--------------|
| `xpe_ghost_create()`        | Stateful handle architecture    | SPEC-XPE-P1B |
| `xpe_ghost_correct()`       | Stateful handle architecture    | SPEC-XPE-P1B |
| `xpe_ghost_reset()`         | Stateful handle architecture    | SPEC-XPE-P1B |
| `xpe_ghost_destroy()`       | Stateful handle architecture    | SPEC-XPE-P1B |
| `xpe_temp_compensate()`     | MCU migration design needed     | SPEC-XPE-P1C |
| `xpe_nonlinearity_correct()`| Separate SWU                   | SPEC-XPE-P1D |
| `xpe_binning_correct()`     | Fluoro/CBCT only               | SPEC-XPE-P1E |

---

## 6. Performance Targets

출처: XPE-ALG-001, 3072x3072 UINT16 기준

| Algorithm          | Target     | SIMD Target (AVX2) |
|--------------------|------------|--------------------|
| Offset Correction  | < 55ms     | < 15ms             |
| Gain Correction    | < 55ms     | < 15ms             |
| Defect Correction  | < 95ms     | < 30ms             |
| Full Pipeline      | < 500ms    | < 100ms            |

---

## 7. Quality Requirements

| Attribute       | Target                                    |
|-----------------|-------------------------------------------|
| Test Coverage   | >= 85% statement coverage                 |
| Testing Method  | TDD (RED-GREEN-REFACTOR)                  |
| SIMD Parity     | Scalar vs AVX2 bit-exact equivalence      |
| IEC 62304 Class | B (medical device software)               |
| Thread Safety   | All processing functions reentrant        |
| Memory Safety   | Zero leaks in 1000-cycle endurance test   |

---

*Document End - SPEC-XPE-P1A v1.0.0*
