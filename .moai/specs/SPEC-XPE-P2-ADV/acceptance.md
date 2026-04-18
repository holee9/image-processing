# Acceptance Criteria: SPEC-XPE-P2-ADV

---
spec_id: SPEC-XPE-P2-ADV
version: 1.0.0
status: Planned
created: 2026-04-17
updated: 2026-04-17
author: manager-spec (MoAI)
---

## 1. Definition of Done

본 SPEC은 다음 모든 항목이 충족될 때 "Done"으로 간주한다:

- [ ] SPEC-XPE-P2-ADV plan.md의 모든 Milestone M1~M7 task 완료
- [ ] 4개 API 함수 모두 구현 및 export 확인
- [ ] Test coverage >= 85% (statement coverage)
- [ ] Branch coverage >= 70%
- [ ] 모든 Given-When-Then 시나리오 통과
- [ ] SIMD parity 테스트 100% 통과 (scalar vs AVX2, error < 1e-6)
- [ ] Performance target 달성 (3072x3072 기준 < 2500ms total)
- [ ] Overshoot limiting safety 테스트 통과 (SAF-100)
- [ ] IEC 62304 Class B 요구사항 준수 확인
- [ ] P/Invoke 통합 테스트 통과
- [ ] Static analysis 0 warnings
- [ ] Memory leak test 통과 (1000 frames)

---

## 2. Acceptance Scenarios (Given-When-Then)

### 2.1 Module Lifecycle

#### AC-LC-001: Initialization with Default Config

```gherkin
Given xpe_enhance_advanced.dll is loaded
When xpe_enhance_advanced_init(NULL) is called
Then the function returns XPE_OK
And the module enters initialized state
And subsequent processing functions can be called
```

**REQ Mapping**: REQ-ADV-001
**Test Type**: Unit

#### AC-LC-002: Processing Before Init

```gherkin
Given the module is NOT initialized
When xpe_multiscale_process(img, meta, NULL) is called
Then the function returns XPE_ERR_NOT_INITIALIZED
And img buffer is not modified
```

**REQ Mapping**: REQ-ADV-020
**Test Type**: Unit

#### AC-LC-003: Shutdown After Init

```gherkin
Given the module is initialized
When xpe_enhance_advanced_shutdown() is called
Then all internal resources are released
And subsequent processing calls return XPE_ERR_NOT_INITIALIZED
```

**REQ Mapping**: REQ-ADV-020
**Test Type**: Unit

---

### 2.2 Multiscale Frequency Processing (MFP)

#### AC-MFP-001: Identity Reconstruction

```gherkin
Given a 512x512 FLOAT32 image with random pixel values
When xpe_multiscale_process is called with identity coefficients (all alpha_k = 1.0)
Then the output image is identical to input within FLOAT32 precision (max error < 1e-5)
And the function returns XPE_OK
```

**REQ Mapping**: REQ-ADV-050
**Test Type**: Unit (Golden reference)

#### AC-MFP-002: Enhancement Effect Visible

```gherkin
Given a 512x512 FLOAT32 image with a sharp edge at column 256
When xpe_multiscale_process is called with enhancement coefficients alpha_low=0.5, alpha_mid=1.5, alpha_high=1.0
Then mid-frequency details near the edge are amplified relative to the input
And the function returns XPE_OK
```

**REQ Mapping**: REQ-ADV-010
**Test Type**: Unit

#### AC-MFP-003: NULL Image Rejection

```gherkin
Given any module state
When xpe_multiscale_process(NULL, meta, NULL) is called
Then the function returns XPE_ERR_INVALID_INPUT
```

**REQ Mapping**: REQ-ADV-022
**Test Type**: Unit (Negative)

#### AC-MFP-004: Invalid Pixel Format Rejection

```gherkin
Given a 512x512 UINT16 image (format = XPE_PIXEL_UINT16)
When xpe_multiscale_process(img, meta, NULL) is called
Then the function returns XPE_ERR_UNSUPPORTED_FORMAT
And img is not modified
```

**REQ Mapping**: REQ-ADV-071
**Test Type**: Unit (Negative)

#### AC-MFP-005: NaN/Inf Free Output

```gherkin
Given a 512x512 FLOAT32 image with extreme values (1e30, -1e30, 0.0)
When xpe_multiscale_process(img, meta, NULL) is called
Then the output contains no NaN or Inf values
And the function returns XPE_OK
```

**REQ Mapping**: REQ-ADV-032
**Test Type**: Unit (Edge case)

#### AC-MFP-006: Large Image Performance

```gherkin
Given a 3072x3072 FLOAT32 image
When xpe_multiscale_process is called
Then execution completes within 800ms (scalar) or 250ms (AVX2)
And the function returns XPE_OK
```

**REQ Mapping**: REQ-ADV-060
**Test Type**: Performance

---

### 2.3 Edge Enhancement (Fractional Process)

#### AC-EDGE-001: Fractional Derivative Order 1.0

```gherkin
Given a 512x512 FLOAT32 image with a step edge
When xpe_fractional_process(img, 1.0, NULL) is called
Then the output shows edge enhancement at the step boundary
And pixel values at the edge are modified
And the function returns XPE_OK
```

**REQ Mapping**: REQ-ADV-011
**Test Type**: Unit

#### AC-EDGE-002: Order Out of Range Rejection

```gherkin
Given a valid 512x512 FLOAT32 image
When xpe_fractional_process(img, 3.0, NULL) is called with order > 2.0
Then the function returns XPE_ERR_INVALID_INPUT
And img is not modified
```

**REQ Mapping**: REQ-ADV-021
**Test Type**: Unit (Negative)

#### AC-EDGE-003: Negative Order Rejection

```gherkin
Given a valid 512x512 FLOAT32 image
When xpe_fractional_process(img, -0.5, NULL) is called with order < 0.0
Then the function returns XPE_ERR_INVALID_INPUT
And img is not modified
```

**REQ Mapping**: REQ-ADV-021
**Test Type**: Unit (Negative)

#### AC-EDGE-004: Overshoot Limiting Active

```gherkin
Given a 512x512 FLOAT32 image with a very high-contrast edge (gradient > 1000)
When xpe_fractional_process(img, 1.5, NULL) is called
Then the enhancement boost at every pixel is clipped to +-3*sigma_local
And no pixel exhibits halo artifacts exceeding the local 3-sigma bound
And the function returns XPE_OK
```

**REQ Mapping**: REQ-ADV-051
**Test Type**: Unit (Safety)

#### AC-EDGE-005: Overshoot Disable Attempt Blocked

```gherkin
Given a 512x512 FLOAT32 image
And a config JSON with "overshoot_limit_enabled": false
When xpe_fractional_process(img, 1.0, config) is called
Then the function returns XPE_ERR_SAFETY_VIOLATION
And an error is logged: "Overshoot limiting is mandatory and cannot be disabled"
```

**REQ Mapping**: REQ-ADV-051 (SAF-100)
**Test Type**: Unit (Safety, Negative)

---

### 2.4 Collimation Detection

#### AC-COL-001: Synthetic Collimation Detection

```gherkin
Given a 1024x1024 FLOAT32 image with rectangular collimation borders at known positions
  - Top border: y=100
  - Bottom border: y=900
  - Left border: x=150
  - Right border: x=850
When xpe_detect_collimation(img, &x0, &y0, &x1, &y1, NULL) is called
Then x0 is within [147, 153]
And y0 is within [97, 103]
And x1 is within [847, 853]
And y1 is within [897, 903]
And the function returns XPE_OK
```

**REQ Mapping**: REQ-ADV-052
**Test Type**: Unit (Golden reference)

#### AC-COL-002: No Collimation Present

```gherkin
Given a 1024x1024 FLOAT32 uniform image (no collimation borders)
When xpe_detect_collimation(img, &x0, &y0, &x1, &y1, NULL) is called
Then the function returns XPE_OK with full image extent
And confidence is logged as below 0.7
And a warning is logged: "ROI confidence below threshold 0.7. Using full-image extent."
```

**REQ Mapping**: REQ-ADV-041
**Test Type**: Unit (Edge case)

#### AC-COL-003: NULL Output Pointer Rejection

```gherkin
Given a valid 1024x1024 FLOAT32 image
When xpe_detect_collimation(img, NULL, &y0, &x1, &y1, NULL) is called
Then the function returns XPE_ERR_INVALID_INPUT
```

**REQ Mapping**: REQ-ADV-022
**Test Type**: Unit (Negative)

#### AC-COL-004: Boundary Coordinates Clipped

```gherkin
Given a 512x512 FLOAT32 image where detected borders extend beyond image edges
When xpe_detect_collimation(img, &x0, &y0, &x1, &y1, NULL) is called
Then all output coordinates are within [0, 511]
And no coordinate is negative or >= image dimension
```

**REQ Mapping**: REQ-ADV-052
**Test Type**: Unit (Boundary)

---

### 2.5 Exposure Index Calculation

#### AC-EI-001: Basic EI Calculation

```gherkin
Given a 512x512 FLOAT32 detector-domain image with uniform pixel value 1000.0
And metadata with bodyPart = "CHEST", kVp = 80, mAs = 2.0
When xpe_calc_exposure_index(img, &meta, &ei, &di) is called
Then ei is a positive finite float value
And di is computed as 10 * log10(ei / ei_target_chest)
And the function returns XPE_OK
```

**REQ Mapping**: REQ-ADV-013
**Test Type**: Unit

#### AC-EI-002: QC Alert for High DI

```gherkin
Given a 512x512 FLOAT32 image with extremely high pixel values (saturated, DI > 3)
And metadata with bodyPart = "CHEST"
When xpe_calc_exposure_index(img, &meta, &ei, &di) is called
Then |di| > 3.0
And a QC alert is logged: "ALERT: Exposure out of range (|DI| = X.X > 3)"
And the function returns XPE_OK (alert does not block)
```

**REQ Mapping**: REQ-ADV-013, FR-1500.2
**Test Type**: Unit

#### AC-EI-003: NULL EI Output Rejection

```gherkin
Given a valid image and metadata
When xpe_calc_exposure_index(img, &meta, NULL, &di) is called
Then the function returns XPE_ERR_INVALID_INPUT
```

**REQ Mapping**: REQ-ADV-022
**Test Type**: Unit (Negative)

#### AC-EI-004: Zero Image Values

```gherkin
Given a 512x512 FLOAT32 image with all pixel values = 0.0
And valid metadata
When xpe_calc_exposure_index(img, &meta, &ei, &di) is called
Then the function handles the zero-mean case gracefully
And does not produce NaN or Inf in ei or di
```

**REQ Mapping**: REQ-ADV-013, REQ-ADV-032
**Test Type**: Unit (Edge case)

---

### 2.6 Pipeline Integration

#### AC-PIPE-001: Full Advanced Pipeline

```gherkin
Given a pre-processed 3072x3072 FLOAT32 image from xpe_enhance_basic (log-transformed)
And valid XpeImageMetadata
When the advanced pipeline executes in order:
  1. xpe_multiscale_process(img, meta, NULL)
  2. xpe_fractional_process(img, 1.0, NULL)
  3. xpe_detect_collimation(img, &x0, &y0, &x1, &y1, NULL)
  4. xpe_calc_exposure_index(img, &meta, &ei, &di)
Then all functions return XPE_OK
And total processing time < 2500ms
And no NaN/Inf in output pixels
And ROI coordinates are valid (within image bounds)
And EI/DI are finite float values
```

**REQ Mapping**: REQ-ADV-010, REQ-ADV-011, REQ-ADV-012, REQ-ADV-013, REQ-ADV-062
**Test Type**: Integration + Performance

#### AC-PIPE-002: Independent Function Calling

```gherkin
Given an initialized module
When each of the 4 functions is called independently (not in pipeline order)
Then each function completes correctly based on its own inputs
And no function depends on state from a previous function call
```

**REQ Mapping**: REQ-ADV-090
**Test Type**: Integration

---

### 2.7 SIMD Parity

#### AC-SIMD-001: MFP Scalar vs AVX2 Parity

```gherkin
Given 50 random 512x512 FLOAT32 images
When scalar and AVX2 multiscale_process are both applied with same parameters
Then all output pixels satisfy max|scalar - avx2| < 1e-6
```

**REQ Mapping**: REQ-ADV-040
**Test Type**: Parity

#### AC-SIMD-002: Edge Enhancement Scalar vs AVX2 Parity

```gherkin
Given 50 random 512x512 FLOAT32 images
When scalar and AVX2 fractional_process are both applied with order=1.0
Then all output pixels satisfy max|scalar - avx2| < 1e-6
```

**REQ Mapping**: REQ-ADV-040
**Test Type**: Parity

#### AC-SIMD-003: Collimation Detection Determinism

```gherkin
Given 50 random 1024x1024 FLOAT32 images with synthetic collimation
When scalar and AVX2 detect_collimation are both applied
Then detected ROI coordinates are identical (x0, y0, x1, y1 match exactly)
```

**REQ Mapping**: REQ-ADV-040
**Test Type**: Parity

---

### 2.8 IEC 62304 Compliance

#### AC-IEC-001: No Exception Propagation

```gherkin
Given any internal state that would throw a C++ exception
When any exported function is called
Then the exception is caught internally
And an appropriate XPE_ERR_* code is returned
And no exception propagates across the C ABI boundary
```

**REQ Mapping**: REQ-ADV-030
**Test Type**: Unit (via internal error injection)

#### AC-IEC-002: Memory Safety Endurance

```gherkin
Given an initialized module
When 1000 consecutive cycles of alloc-process-free are executed
Then no memory leaks are detected
And heap usage returns to baseline after each cycle
```

**REQ Mapping**: REQ-ADV-031
**Test Type**: Endurance

#### AC-IEC-003: Concurrent Access Safety

```gherkin
Given an initialized module
When 4 threads concurrently call xpe_multiscale_process with independent buffers
Then all calls complete successfully with XPE_OK
And no data races or corruption occur
```

**REQ Mapping**: REQ-ADV-090
**Test Type**: Concurrency

---

### 2.9 P/Invoke Compatibility

#### AC-PIN-001: C# Struct Marshaling

```gherkin
Given xpe_enhance_advanced.dll loaded via P/Invoke in C#
When XpeImageBuffer and XpeImageMetadata are marshaled with [StructLayout(Pack = 8)]
Then struct sizes match between C++ and C#
And all fields are correctly accessible from C#
```

**REQ Mapping**: REQ-ADV-002
**Test Type**: Integration (C#)

#### AC-PIN-002: Full Pipeline from C#

```gherkin
Given ImageProcTest C# application
When the full advanced enhancement pipeline is invoked via P/Invoke
Then the DLL functions execute correctly
And the processed image is displayed in the GUI
```

**REQ Mapping**: REQ-ADV-002
**Test Type**: Integration (C#)

---

## 3. Edge Cases and Boundary Conditions

| Category                | Test Case                                    | Expected Result                  |
|-------------------------|----------------------------------------------|----------------------------------|
| Zero-sized image        | 0x0 dimensions                               | XPE_ERR_INVALID_INPUT            |
| Single pixel            | 1x1 image                                    | XPE_ERR_INVALID_INPUT            |
| Maximum image           | 4096x4096 FLOAT32                            | Correct processing within 1.5x budget |
| All-zero pixels         | Entire image is 0.0                          | No NaN/Inf, graceful handling    |
| All-same pixels         | Entire image is constant (e.g., 1000.0)      | MFP produces valid output        |
| Extreme values          | Pixels = 1e30 or -1e30                       | No overflow, output clamped      |
| UINT16 format image     | format = XPE_PIXEL_UINT16                    | XPE_ERR_UNSUPPORTED_FORMAT       |
| NULL metadata           | meta = NULL in xpe_multiscale_process        | XPE_ERR_INVALID_INPUT            |
| NULL config JSON        | configJsonOrNull = NULL                      | Use defaults (XPE_OK)            |
| Empty config JSON       | configJsonOrNull = ""                        | XPE_ERR_CONFIG_INVALID           |
| Order boundary 0.0      | order = 0.0                                  | XPE_OK (identity-like operation) |
| Order boundary 2.0      | order = 2.0                                  | XPE_OK (strong texture emphasis) |
| No collimation borders  | Uniform or gradient image                    | Full image ROI, low confidence    |
| Partial collimation     | Only 2 borders visible                       | Full image fallback              |
| Negative EI target      | EI_target < 0 (corrupt lookup)               | Graceful error handling          |

---

## 4. Performance Benchmarks

| Benchmark ID | Operation                        | Image Size    | Target (Scalar) | Target (AVX2) |
|--------------|----------------------------------|---------------|:---------------:|:--------------:|
| PERF-ADV-001 | MFP (Laplacian pyramid 4 levels) | 3072x3072 F32 | < 800ms        | < 250ms       |
| PERF-ADV-002 | Fractional process (order 1.0)   | 3072x3072 F32 | < 400ms        | < 120ms       |
| PERF-ADV-003 | Collimation detection            | 3072x3072 F32 | < 500ms        | < 200ms       |
| PERF-ADV-004 | EI/DI calculation                | 3072x3072 F32 | < 50ms         | < 20ms        |
| PERF-ADV-005 | Full pipeline (MFP+Edge+ROI+EI)  | 3072x3072 F32 | < 2500ms       | < 600ms       |
| PERF-ADV-006 | MFP identity reconstruction      | 3072x3072 F32 | < 800ms        | < 250ms       |

---

## 5. Quality Gate Criteria

### 5.1 Code Quality (TRUST 5)

| Pillar      | Criterion                                      | Verification           |
|-------------|------------------------------------------------|------------------------|
| Tested      | Statement coverage >= 85%                      | gcov/llvm-cov          |
| Readable    | No compiler warnings with -Wall -Wextra        | CI build               |
| Unified     | Consistent naming (xpe_ prefix, snake_case)    | Code review            |
| Secured     | Input validation on all exported functions      | Test matrix            |
| Trackable   | REQ-ADV-xxx to test case mapping documented    | Acceptance matrix      |

### 5.2 IEC 62304 Artifacts

| Artifact                    | Status      | Location                                   |
|-----------------------------|-------------|--------------------------------------------|
| Software Requirements       | Referenced  | SRS-ENHANCE-ADV-001                        |
| Software Design             | This SPEC   | .moai/specs/SPEC-XPE-P2-ADV/               |
| Unit Test Results           | Generated   | modules/enhance_advanced/tests/            |
| Integration Test Results    | Generated   | modules/enhance_advanced/tests/            |
| Code Review                 | Pending     | PR review                                  |
| Static Analysis             | Pending     | CI pipeline                                |

---

## 6. Acceptance Matrix (REQ to Test Traceability)

| REQ ID        | Test Scenarios                                  | Priority |
|---------------|------------------------------------------------|----------|
| REQ-ADV-001   | AC-LC-001                                      | High     |
| REQ-ADV-002   | AC-PIN-001, AC-PIN-002                          | High     |
| REQ-ADV-010   | AC-MFP-002, AC-MFP-006                          | High     |
| REQ-ADV-011   | AC-EDGE-001                                     | High     |
| REQ-ADV-012   | AC-COL-001, AC-COL-002                          | High     |
| REQ-ADV-013   | AC-EI-001, AC-EI-002, AC-EI-004                 | High     |
| REQ-ADV-020   | AC-LC-002, AC-LC-003                            | High     |
| REQ-ADV-021   | AC-EDGE-002, AC-EDGE-003                        | High     |
| REQ-ADV-022   | AC-MFP-003, AC-COL-003, AC-EI-003               | High     |
| REQ-ADV-030   | AC-IEC-001                                      | High     |
| REQ-ADV-031   | AC-IEC-002                                      | High     |
| REQ-ADV-032   | AC-MFP-005, AC-EI-004                           | High     |
| REQ-ADV-040   | AC-SIMD-001, AC-SIMD-002, AC-SIMD-003          | Medium   |
| REQ-ADV-041   | AC-COL-002                                      | Medium   |
| REQ-ADV-050   | AC-MFP-001                                      | High     |
| REQ-ADV-051   | AC-EDGE-004, AC-EDGE-005                        | High     |
| REQ-ADV-052   | AC-COL-001, AC-COL-004                          | High     |
| REQ-ADV-060   | AC-MFP-006                                      | High     |
| REQ-ADV-061   | PERF-ADV-002                                    | High     |
| REQ-ADV-062   | AC-PIPE-001, PERF-ADV-005                       | High     |
| REQ-ADV-070   | Zero-sized image edge case                      | High     |
| REQ-ADV-071   | AC-MFP-004                                      | High     |
| REQ-ADV-080   | Memory profiling test                           | High     |
| REQ-ADV-081   | AC-IEC-002                                      | High     |
| REQ-ADV-090   | AC-IEC-003, AC-PIPE-002                         | High     |
| REQ-ADV-091   | Diagnostic log verification                     | Medium   |
| REQ-ADV-100   | 1x1 image rejection test                        | High     |
| REQ-ADV-101   | 4096x4096 processing test                       | Medium   |

---

*Document End - SPEC-XPE-P2-ADV Acceptance v1.0.0*
