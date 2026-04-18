# Acceptance Criteria: SPEC-XPE-P1A

---
spec_id: SPEC-XPE-P1A
version: 1.2.0
status: In Progress (SUP-01 done, M2 pending)
created: 2026-04-16
updated: 2026-04-18
author: manager-spec (MoAI)
---

## HISTORY

| Version | Date       | Author       | Changes |
|---------|------------|--------------|---------|
| 1.2.0   | 2026-04-18 | manager-spec (Pre Lane upgrade) | Align with spec.md v1.2.0. Strengthen AC-DET-001 with Hampel recipe. Expand AC-SIMD-001~003 to reference simd-parity-harness.md. Add AC-PERF-001~005 pixel-accuracy benchmark mapping. |
| 1.0.0   | 2026-04-16 | manager-spec | Initial acceptance criteria |

## 1. Definition of Done

본 SPEC은 다음 모든 항목이 충족될 때 "Done"으로 간주한다:

- [ ] SPEC-XPE-P1A plan.md의 모든 Milestone M1~M6 task 완료
- [ ] 14개 API 함수 모두 구현 및 export 확인
- [ ] Test coverage >= 85% (statement coverage)
- [ ] 모든 Given-When-Then 시나리오 통과
- [ ] SIMD parity 테스트 100% 통과 (scalar vs AVX2)
- [ ] Performance target 달성 (3072x3072 기준)
- [ ] IEC 62304 Class B 요구사항 준수 확인
- [ ] P/Invoke 통합 테스트 통과
- [ ] Code review 완료 (TRUST 5 기준)

---

## 2. Acceptance Scenarios (Given-When-Then)

### 2.1 Module Lifecycle

#### AC-LC-001: Initialization with Default Config

```gherkin
Given xpe_preprocess.dll is loaded
When xpe_preprocess_init(NULL) is called
Then the function returns XPE_OK
And the module enters initialized state
And subsequent processing functions can be called
```

**REQ Mapping**: REQ-P1A-001
**Test Type**: Unit

#### AC-LC-002: Initialization with Valid JSON Config

```gherkin
Given xpe_preprocess.dll is loaded
When xpe_preprocess_init("{\"mode\":\"clinical\"}") is called
Then the function returns XPE_OK
And the module enters initialized state
```

**REQ Mapping**: REQ-P1A-001
**Test Type**: Unit

#### AC-LC-003: Shutdown After Init

```gherkin
Given the module is initialized
When xpe_preprocess_shutdown() is called
Then all internal resources are released
And subsequent processing calls return XPE_ERR_NOT_INITIALIZED
```

**REQ Mapping**: REQ-P1A-020
**Test Type**: Unit

#### AC-LC-004: Processing Before Init

```gherkin
Given the module is NOT initialized
When xpe_offset_correct(img, offsetMap) is called
Then the function returns XPE_ERR_NOT_INITIALIZED
And img buffer is not modified
```

**REQ Mapping**: REQ-P1A-020
**Test Type**: Unit

---

### 2.2 Offset Correction

#### AC-OFF-001: Basic Offset Subtraction

```gherkin
Given a 512x512 UINT16 image with pixel values [1000, 2000, 3000]
And a 512x512 UINT16 offset map with pixel values [500, 500, 500]
When xpe_offset_correct(img, offsetMap) is called
Then img pixel values become [500, 1500, 2500]
And the function returns XPE_OK
```

**REQ Mapping**: REQ-P1A-010
**Test Type**: Unit (Golden reference)

#### AC-OFF-002: Floor-at-Zero Behavior

```gherkin
Given a 512x512 UINT16 image with pixel values [100, 200, 50]
And a 512x512 UINT16 offset map with pixel values [500, 100, 500]
When xpe_offset_correct(img, offsetMap) is called
Then img pixel values become [0, 100, 0]
And no pixel value is negative or wraps around
```

**REQ Mapping**: REQ-P1A-010
**Test Type**: Unit (Edge case)

#### AC-OFF-003: Dimension Mismatch Rejection

```gherkin
Given a 512x512 UINT16 image
And a 1024x1024 UINT16 offset map
When xpe_offset_correct(img, offsetMap) is called
Then the function returns XPE_ERR_INVALID_INPUT
And img buffer is not modified
```

**REQ Mapping**: REQ-P1A-021
**Test Type**: Unit (Negative)

#### AC-OFF-004: Format Mismatch Rejection

```gherkin
Given a 512x512 UINT16 image
And a 512x512 FLOAT32 offset map
When xpe_offset_correct(img, offsetMap) is called
Then the function returns XPE_ERR_UNSUPPORTED_FORMAT
And img buffer is not modified
```

**REQ Mapping**: REQ-P1A-022
**Test Type**: Unit (Negative)

#### AC-OFF-005: NULL Input Rejection

```gherkin
Given any module state
When xpe_offset_correct(NULL, offsetMap) is called
Then the function returns XPE_ERR_INVALID_INPUT
```

**REQ Mapping**: REQ-P1A-005
**Test Type**: Unit (Negative)

---

### 2.3 Gain Correction

#### AC-GAIN-001: Basic Gain Multiplication

```gherkin
Given a 512x512 FLOAT32 image with pixel values [100.0, 200.0, 300.0]
And a 512x512 FLOAT32 gain map with pixel values [1.5, 2.0, 0.5]
When xpe_gain_correct(img, gainMap) is called
Then img pixel values become [150.0, 400.0, 150.0]
And the function returns XPE_OK
```

**REQ Mapping**: REQ-P1A-011
**Test Type**: Unit (Golden reference)

#### AC-GAIN-002: NaN/Inf Clamping

```gherkin
Given a FLOAT32 image with pixel value 0.0 at position (x,y)
And a FLOAT32 gain map with very large gain value at (x,y)
When xpe_gain_correct(img, gainMap) is called
Then the output pixel at (x,y) is NOT NaN and NOT Inf
And the output is clamped to a finite value
```

**REQ Mapping**: REQ-P1A-033
**Test Type**: Unit (Edge case)

#### AC-GAIN-003: UINT16 Gain Correction

```gherkin
Given a 512x512 UINT16 image with uniform pixel values 10000
And a 512x512 UINT16 gain map representing flat-field correction factors
When xpe_gain_correct(img, gainMap) is called
Then all output pixels are corrected by per-pixel gain factors
And no output pixel exceeds UINT16 max (65535) or wraps around
```

**REQ Mapping**: REQ-P1A-011
**Test Type**: Unit

---

### 2.4 Defect Correction

#### AC-DEF-001: Single Defect Pixel Bilinear Interpolation

```gherkin
Given a 512x512 UINT16 image with a defect pixel at (100,100) having value 65535
And a 512x512 defect map with non-zero at (100,100) only
When xpe_defect_correct(img, defectMap, NULL) is called
Then pixel (100,100) is replaced with bilinear interpolation of its valid neighbors
And the function returns XPE_OK
```

**REQ Mapping**: REQ-P1A-012
**Test Type**: Unit

#### AC-DEF-002: Edge Pixel Defect (Edge-Aware)

```gherkin
Given a 512x512 UINT16 image with a defect pixel at corner (0,0)
And a defect map marking (0,0) as defective
When xpe_defect_correct(img, defectMap, NULL) is called
Then pixel (0,0) is replaced using only available valid neighbors
And no out-of-bounds memory access occurs
```

**REQ Mapping**: REQ-P1A-012
**Test Type**: Unit (Edge case)

#### AC-DEF-003: Median Interpolation Mode

```gherkin
Given a defect pixel surrounded by [100, 200, 150, 180, 120, 160, 140, 170]
And configJsonOrNull specifies "interpolation_mode": "median"
When xpe_defect_correct(img, defectMap, config) is called
Then the defect pixel is replaced with the median of its valid neighbors
```

**REQ Mapping**: REQ-P1A-012
**Test Type**: Unit

#### AC-DEF-004: No Defect Pixels

```gherkin
Given a 512x512 image with no defects
And an all-zero defect map
When xpe_defect_correct(img, defectMap, NULL) is called
Then the function returns XPE_OK
And img is identical to the input (no modifications)
```

**REQ Mapping**: REQ-P1A-012
**Test Type**: Unit (Boundary)

---

### 2.5 Runtime Defect Detection

#### AC-DET-001: Transient Defect Detection (Hampel 5-sigma)

```gherkin
Given a 512x512 UINT16 image with a pixel whose value differs from its 8-neighborhood median by more than 5 * MAD / 0.6745
And the config JSON omits "hampel_threshold" (use default 5.0)
When xpe_defect_detect_runtime(img, defectMapOut, config) is called
Then defectMapOut[x,y] == 1 for that deviating pixel
And all other pixels have defectMapOut == 0 (assuming they are within threshold)
And the function returns XPE_OK
```

**REQ Mapping**: REQ-P1A-013
**Test Type**: Unit
**Algorithm Reference**: spec.md v1.2.0 Section 4.2 REQ-P1A-013 (Hampel recipe), research.md v2.0.0 Section 8.3

#### AC-DET-002: Runtime Detection Accuracy

```gherkin
Given 1000 synthetic clinical frames with 50 injected 5-sigma transient defects per frame (ground truth known)
When xpe_defect_detect_runtime is applied with default threshold
Then aggregate TPR (true-positive rate) over all 50,000 injections >= 99.9%
And aggregate FPR (false-positive rate) on un-injected pixels < 0.001%
And no function call returns an error
```

**REQ Mapping**: REQ-P1A-013
**Test Type**: Statistical / Benchmark (BP-04 runtime portion)

#### AC-DET-003: Edge-of-Image Handling

```gherkin
Given a 512x512 UINT16 image where the top row has >5-sigma values
When xpe_defect_detect_runtime is called
Then pixels in the top row (incomplete 3x3 neighborhood) are handled with available subset
And no out-of-bounds memory access occurs
And at least 5 valid neighbors are required; otherwise defectMapOut[x,y] = 0
```

**REQ Mapping**: REQ-P1A-013, REQ-P1A-005
**Test Type**: Unit (Edge case)

#### AC-DET-004: Configurable Threshold

```gherkin
Given a 512x512 UINT16 image with a 4-sigma transient
When xpe_defect_detect_runtime is called with config '{"hampel_threshold": 3.5}'
Then the 4-sigma transient is flagged (defectMapOut == 1)
And calling again with config '{"hampel_threshold": 5.0}' does NOT flag the same transient
```

**REQ Mapping**: REQ-P1A-013
**Test Type**: Unit (Parameterized)

---

### 2.6 Calibration Management

#### AC-CAL-001: Load Valid Offset Calibration

```gherkin
Given a valid XCal offset file at "test_data/offset_map_512x512.xcal"
And a pre-allocated XpeImageBuffer offsetMapOut with dimensions 512x512
When xpe_calib_load_offset(filePath, &offsetMapOut) is called
Then the offset map data is loaded into offsetMapOut.data
And the function returns XPE_OK
```

**REQ Mapping**: REQ-P1A-014
**Test Type**: Integration

#### AC-CAL-002: Calibration Expiry Detection

```gherkin
Given an XCal file with expiry timestamp in the past
When xpe_calib_check_expiry(filePath, &expiryOut) is called
Then the function returns XPE_ERR_CALIBRATION_EXPIRED
And expiryOut contains the past timestamp
```

**REQ Mapping**: REQ-P1A-018
**Test Type**: Unit

#### AC-CAL-003: Calibration Save and Load Round-Trip

```gherkin
Given a valid calibration map in memory
When xpe_calib_save(calibMap, "output.xcal", expiryMs, NULL) is called
And then xpe_calib_load_offset("output.xcal", &loaded) is called
Then loaded data is bit-identical to the original calibMap data
And SHA-256 checksums match
```

**REQ Mapping**: REQ-P1A-019, REQ-P1A-014
**Test Type**: Integration

#### AC-CAL-004: Offset Generation from Multiple Frames

```gherkin
Given 10 dark-field frames of 512x512 UINT16
When xpe_calib_generate_offset(frames, 10, &offsetOut, NULL) is called
Then offsetOut contains the pixel-wise mean of all 10 frames
And the function returns XPE_OK
```

**REQ Mapping**: REQ-P1A-017
**Test Type**: Unit

#### AC-CAL-005: Corrupted File Detection

```gherkin
Given a file with corrupted XCal header or payload
When xpe_calib_load_offset(corruptedPath, &offsetMapOut) is called
Then the function returns XPE_ERR_IO_FAILED or XPE_ERR_PROCESSING_FAILED
And offsetMapOut is not modified
```

**REQ Mapping**: REQ-P1A-014
**Test Type**: Unit (Negative)

---

### 2.7 Pipeline Integration

#### AC-PIPE-001: Full Pre-processing Pipeline

```gherkin
Given a raw 3072x3072 UINT16 image from FPD
And a valid offset map, gain map, and defect map loaded via calibration functions
When the pipeline executes in order:
  1. xpe_offset_correct(rawImg, offsetMap)
  2. xpe_gain_correct(rawImg, gainMap)
  3. xpe_defect_correct(rawImg, defectMap, NULL)
Then the output image is correctly pre-processed
And total processing time < 500ms (scalar) or < 100ms (AVX2)
And no NaN/Inf/overflow in output pixels
```

**REQ Mapping**: REQ-P1A-010, REQ-P1A-011, REQ-P1A-012
**Test Type**: Integration + Performance

#### AC-PIPE-002: Idempotent Correction (No Defects Case)

```gherkin
Given a corrected image with no defect map entries
When xpe_defect_correct is applied
Then the image is unchanged
And applying offset + gain correction again with same maps produces predictable results
```

**REQ Mapping**: REQ-P1A-012
**Test Type**: Integration

---

### 2.8 SIMD Parity

Full parity protocol specification: `.moai/specs/SPEC-XPE-P1A/simd-parity-harness.md` v1.0.0. The scenarios below are the high-level acceptance gates; the harness document contains the detailed seed policy, input distributions, ULP computation, and dispatch override mechanism.

#### AC-SIMD-001: Offset Correction Parity (UINT16 Bit-Identical)

```gherkin
Given 100 pseudo-random UINT16 frames per shape (512x512, 1024x1024, 3072x3072) generated with seed = CRC32("offset_subtract" derived from master "XPE-SIMD-PARITY-v1")
And corresponding random UINT16 offset maps (range 0..32767)
When scalar xpe_offset_correct and AVX2 xpe_offset_correct are both applied
Then memcmp(scalar_output, avx2_output, byte_count) == 0 for all 100 inputs per shape
```

**REQ Mapping**: REQ-P1A-040
**Test Type**: Parity (simd-parity-harness.md Section 3, Rule: Bit-identical)
**Test Count**: 300 (100 per 3 shapes)

#### AC-SIMD-002: Gain Correction Parity (FLOAT32 1-ULP)

```gherkin
Given 100 pseudo-random FLOAT32 frames per shape (seeded per harness Section 4.1)
And random FLOAT32 gain maps in range [0.5, 2.0]
When scalar gain correction (a * (1.0f/b)) and AVX2 gain correction (_mm256_mul_ps with reciprocal) are both applied
Then for every output pixel: fabsf(scalar - avx2) <= 1 * ULP(max(|scalar|, |avx2|))
And neither path produces NaN or Inf when the other does not
```

**REQ Mapping**: REQ-P1A-040, REQ-P1A-033
**Test Type**: Parity (harness Section 3, Rule: OneULP)
**Test Count**: 300 (100 per 3 shapes)

#### AC-SIMD-003: Defect Correction Parity (UINT16 Bit-Identical)

```gherkin
Given 100 pseudo-random UINT16 frames + random defect maps (Bernoulli p=0.001)
When scalar bilinear defect correction and AVX2 defect correction are both applied
Then scalar_output == avx2_output (byte-level)
And the same parity holds for median mode (cluster defects)
```

**REQ Mapping**: REQ-P1A-040
**Test Type**: Parity (harness, Rule: Bit-identical)
**Test Count**: 600 (100 per 3 shapes x 2 modes: bilinear + median)

#### AC-SIMD-004: Runtime Detection Parity (UINT16 Bit-Identical)

```gherkin
Given 100 pseudo-random UINT16 frames with injected 5-sigma transients
When scalar runtime detection (median-of-9 + MAD) and AVX2 runtime detection (sorting network + MAD) are both applied
Then the output defectMap is byte-identical between paths
```

**REQ Mapping**: REQ-P1A-013, REQ-P1A-040
**Test Type**: Parity (harness, Rule: Bit-identical)
**Test Count**: 300

#### AC-SIMD-005: Dispatch Override Honors Force-Scalar

```gherkin
Given the module initialized with config '{"force_scalar": true}'
When any correction function is called on AVX2-capable hardware
Then the scalar code path executes (verified via profiler or debug flag)
And subsequent calls after xpe_preprocess_shutdown + re-init WITHOUT override default to AVX2
```

**REQ Mapping**: REQ-P1A-040
**Test Type**: Dispatch control
**Test Count**: 2 (one with override, one without)

---

### 2.9 IEC 62304 Compliance

#### AC-IEC-001: No Exception Propagation

```gherkin
Given any internal state that would throw a C++ exception
When any exported function is called
Then the exception is caught internally
And an appropriate XPE_ERR_* code is returned
And no exception propagates across the C ABI boundary
```

**REQ Mapping**: REQ-P1A-030
**Test Type**: Unit (via internal error injection)

#### AC-IEC-002: Memory Safety Endurance

```gherkin
Given an initialized module
When 1000 consecutive cycles of alloc-process-free are executed
Then no memory leaks are detected
And heap usage returns to baseline after each cycle
```

**REQ Mapping**: REQ-P1A-031
**Test Type**: Endurance

#### AC-IEC-003: Concurrent Access Safety

```gherkin
Given an initialized module
When 4 threads concurrently call xpe_offset_correct with independent buffers
Then all calls complete successfully with XPE_OK
And no data races or corruption occur
```

**REQ Mapping**: REQ-P1A-003
**Test Type**: Concurrency

---

### 2.10 P/Invoke Compatibility

#### AC-PIN-001: C# Struct Marshaling

```gherkin
Given xpe_preprocess.dll loaded via P/Invoke in C#
When XpeImageBuffer is marshaled with [StructLayout(Pack = 8)]
Then sizeof(XpeImageBuffer) matches between C++ (36 bytes) and C#
And all fields are correctly accessible
```

**REQ Mapping**: REQ-P1A-002
**Test Type**: Integration (C#)

#### AC-PIN-002: Full Pipeline from C#

```gherkin
Given ImageProcTest C# application
When the full pre-processing pipeline is invoked via P/Invoke
Then the DLL functions execute correctly
And the processed image is displayed in the GUI
```

**REQ Mapping**: REQ-P1A-002
**Test Type**: Integration (C#)

---

## 3. Edge Cases and Boundary Conditions

| Category                | Test Case                                    | Expected Result                  |
|-------------------------|----------------------------------------------|----------------------------------|
| Zero-sized image        | 0x0 dimensions                               | XPE_ERR_INVALID_INPUT            |
| Maximum image           | 4096x4096 UINT16                             | Correct processing within budget |
| Single pixel            | 1x1 image                                    | XPE_ERR_INVALID_INPUT (defect)   |
| All-zero offset map     | Entire map is 0                              | img unchanged                    |
| All-zero gain map       | Entire map is 0                              | XPE_ERR_PROCESSING_FAILED or 0   |
| Max UINT16 values       | All pixels = 65535                           | No overflow/wraparound           |
| Empty defect map        | All zeros (no defects)                       | img unchanged                    |
| Full defect map         | All pixels defective                         | Cannot interpolate (error)       |
| Negative temperatures   | detectorTemp < 0                             | Valid range (negative Celsius OK)|
| Expired calibration     | Expiry in past                               | XPE_ERR_CALIBRATION_EXPIRED      |
| Corrupted XCal magic    | Magic != "XCal"                              | XPE_ERR_IO_FAILED                |
| SHA-256 mismatch        | Corrupted payload                            | XPE_ERR_PROCESSING_FAILED        |
| NULL config JSON        | configJsonOrNull = NULL                      | Use defaults (XPE_OK)            |
| Empty config JSON       | configJsonOrNull = ""                        | XPE_ERR_INVALID_INPUT            |

---

## 4. Performance Benchmarks

| Benchmark ID | Operation                        | Image Size    | Target (Scalar) | Target (AVX2) |
|--------------|----------------------------------|---------------|-----------------|---------------|
| PERF-001     | Offset correction                | 3072x3072 U16 | < 55ms          | < 15ms        |
| PERF-002     | Gain correction                  | 3072x3072 U16 | < 55ms          | < 15ms        |
| PERF-003     | Defect correction (bilinear)     | 3072x3072 U16 | < 95ms          | < 30ms        |
| PERF-004     | Full pipeline (offset+gain+defect)| 3072x3072 U16 | < 500ms         | < 100ms       |
| PERF-005     | XCal file load (offset)          | 3072x3072     | < 50ms          | N/A           |
| PERF-006     | Calibration generate (10 frames) | 3072x3072     | < 200ms         | < 80ms        |
| PERF-007     | Runtime detection (Hampel)       | 3072x3072 U16 | < 35ms          | < 12ms        |

---

## 4A. Pixel-Accuracy Benchmarks (BP-01~05 linkage)

These acceptance criteria are verified by the Pre Lane benchmark pack (`benchmark/BP-01-05-preprocess-manifest.md`). Each row below maps a REQ to its benchmark pack and pass criterion.

| REQ | Benchmark Pack | Metric | Target |
|-----|---------------|--------|--------|
| REQ-P1A-010 | BP-01 Temperature sweep | Residual dark mean (post-correction) | < 2 ADU |
| REQ-P1A-010 | BP-01 | Residual dark sigma | < 3 ADU |
| REQ-P1A-010 | BP-01 | Temperature stability 20-35 C | < 5 ADU drift |
| REQ-P1A-011 | BP-02 Multi-gain linearity | Flat-field residual sigma/mean | < 0.5% over 90% FOV |
| REQ-P1A-011 | BP-02 | Linearity R^2 | >= 0.9999 |
| REQ-P1A-011 | BP-02 | NaN/Inf pixel count | 0 |
| REQ-P1A-011 | BP-03 Heel-effect SID | Heel RMSE reduction | >= 80% (Wang 2013 baseline) |
| REQ-P1A-012 | BP-04 Defect density | Isolated-pixel NMSE improvement over copy-neighbor | >= 10x |
| REQ-P1A-012 | BP-04 | Cluster (3x3) NMSE on UINT16 | < 150 |
| REQ-P1A-012 | BP-04 | Cluster (5x5) NMSE on UINT16 | < 250 |
| REQ-P1A-012 | BP-04 | Artificial-edge count at defect boundaries | 0 |
| REQ-P1A-013 | BP-04 runtime | TPR on 5-sigma injections | >= 99.9% |
| REQ-P1A-013 | BP-04 runtime | FPR on clean frames | < 0.001% |
| REQ-P1A-040 | BP-SIMD (parity harness) | Total parity cases pass | 1830/1830 |

Research basis for targets: `.moai/specs/SPEC-XPE-P1A/research.md` v2.0.0 Section 8.

---

## 5. Quality Gate Criteria

### 5.1 Code Quality (TRUST 5)

| Pillar      | Criterion                                      | Verification           |
|-------------|------------------------------------------------|------------------------|
| Tested      | Statement coverage >= 85%                      | gcov/llvm-cov          |
| Readable    | No compiler warnings with -Wall -Wextra        | CI build               |
| Unified     | Consistent naming (xpe_ prefix, snake_case)    | Code review            |
| Secured     | Input validation on all exported functions      | Test matrix            |
| Trackable   | REQ-P1A-xxx to test case mapping documented    | Acceptance matrix      |

### 5.2 IEC 62304 Artifacts

| Artifact                    | Status      | Location                              |
|-----------------------------|-------------|---------------------------------------|
| Software Requirements       | Referenced  | XPE-SRS-001                           |
| Software Design             | This SPEC   | .moai/specs/SPEC-XPE-P1A/             |
| Unit Test Results           | Generated   | modules/preprocess/tests/             |
| Integration Test Results    | Generated   | modules/preprocess/tests/             |
| Code Review                 | Pending     | PR review                             |
| Static Analysis             | Pending     | CI pipeline                           |

---

## 6. Acceptance Matrix (REQ to Test Traceability)

| REQ ID        | Test Scenarios                                  | Priority |
|---------------|------------------------------------------------|----------|
| REQ-P1A-001   | AC-LC-001, AC-LC-002                            | High     |
| REQ-P1A-002   | AC-PIN-001, AC-PIN-002                          | High     |
| REQ-P1A-003   | AC-IEC-003                                      | High     |
| REQ-P1A-004   | All scenarios (return code check)               | High     |
| REQ-P1A-005   | AC-OFF-005, all NULL input tests                | High     |
| REQ-P1A-010   | AC-OFF-001, AC-OFF-002                          | High     |
| REQ-P1A-011   | AC-GAIN-001, AC-GAIN-002, AC-GAIN-003          | High     |
| REQ-P1A-012   | AC-DEF-001~AC-DEF-004                           | High     |
| REQ-P1A-013   | AC-DET-001                                      | Medium   |
| REQ-P1A-014   | AC-CAL-001                                      | High     |
| REQ-P1A-015   | AC-CAL-001 (gain variant)                       | High     |
| REQ-P1A-016   | AC-CAL-001 (defect variant)                     | High     |
| REQ-P1A-017   | AC-CAL-004                                      | Medium   |
| REQ-P1A-018   | AC-CAL-002                                      | High     |
| REQ-P1A-019   | AC-CAL-003                                      | Medium   |
| REQ-P1A-020   | AC-LC-003, AC-LC-004                            | High     |
| REQ-P1A-021   | AC-OFF-003                                      | High     |
| REQ-P1A-022   | AC-OFF-004                                      | High     |
| REQ-P1A-030   | AC-IEC-001                                      | High     |
| REQ-P1A-031   | AC-IEC-002                                      | High     |
| REQ-P1A-032   | All error path tests                            | High     |
| REQ-P1A-033   | AC-GAIN-002                                     | High     |
| REQ-P1A-040   | AC-SIMD-001, AC-SIMD-002, AC-SIMD-003          | Medium   |
| REQ-P1A-041   | AC-PIPE-001                                     | Low      |
| REQ-P1A-042   | Parameter range test                            | Medium   |

---

*Document End - SPEC-XPE-P1A Acceptance v1.2.0*
