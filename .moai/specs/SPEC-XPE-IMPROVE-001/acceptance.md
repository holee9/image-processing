# Acceptance Criteria: SPEC-XPE-IMPROVE-001

**Document ID**: AC-XPE-IMPROVE-001
**Version**: 1.0.0
**Date**: 2026-04-19
**Status**: Draft

---

## 1. Functional Acceptance Criteria

### AC-IMPROVE-001: Offset Correction SIMD

**Given** a 3072x3072 UINT16 image and offset map
**When** xpe_offset_correct() is called with AVX2 dispatch
**Then**:
- Processing completes in < 15ms (vs < 55ms scalar)
- Output is bit-identical to scalar baseline
- No wrap-around or negative values (floor-at-zero)

**Test Cases**:
```cpp
// Basic subtraction
Input: [1000, 2000, 3000] - [500, 500, 500]
Expected: [500, 1500, 2500]

// Floor-at-zero
Input: [100, 200, 50] - [500, 100, 500]
Expected: [0, 100, 0]

// Dimension mismatch
Input: 512x512 vs 1024x1024
Expected: XPE_ERR_INVALID_INPUT
```

### AC-IMPROVE-002: Gain Correction SIMD

**Given** a 3072x3072 UINT16 image and gain map
**When** xpe_gain_correct() is called with AVX2 dispatch
**Then**:
- Processing completes in < 15ms (vs < 55ms scalar)
- Output matches scalar within 1 ULP (FLOAT32 rounding)
- No NaN/Inf in output
- No UINT16 overflow at pixel = 65535

**Test Cases**:
```cpp
// Basic multiplication
Input: [100, 200, 300] * [1.5, 2.0, 0.5]
Expected: [150, 400, 150] (±1 ULP)

// Zero-division protection
Input: Gain map with I_flat - I_dark ≤ 0
Expected: XPE_ERR_INVALID_INPUT (pre-calibration validation)
```

### AC-IMPROVE-003: Defect Correction Edge-Aware

**Given** a defect map with isolated and clustered defects
**When** xpe_defect_correct() is called
**Then**:
- Processing completes in < 30ms (vs < 95ms scalar)
- Gradient delta at defect boundaries < 10% local contrast
- Correction recall on BPM defects ≥ 99%
- No new edges introduced

**Test Cases**:
```cpp
// Single defect (isolated)
Defect at (100, 100), all neighbors valid
Expected: Bilinear interpolation of 4-neighbors

// Edge defect (corner)
Defect at (0, 0)
Expected: Use available in-bounds neighbors

// Cluster (>50% defective)
Neighborhood 8 pixels, 5 defective
Expected: Median of 3 valid neighbors (not hallucination)
```

### AC-IMPROVE-004: Runtime Defect Detection Hampel

**Given** a 3072x3072 image with injected 5-sigma transients
**When** xpe_defect_detect_runtime() is called
**Then**:
- Processing completes in < 12ms (vs < 35ms scalar)
- True Positive Rate ≥ 99.9% on injected transients
- False Positive Rate < 0.001% on clean frames (< 9 pixels)
- Edge pixels handled with available subset

**Test Cases**:
```cpp
// 5-sigma transient
Pixel value: 1000, local median: 100, local MAD: 20
z-score: 0.6745 * (1000 - 100) / 20 = 30.37
Expected: defect = 1 (|30.37| > 5.0)

// Threshold configuration
Lambda = 3.5: More aggressive (higher TPR, higher FPR)
Lambda = 5.0: Default (balanced)
Lambda = 10.0: Conservative (lower TPR, lower FPR)

// Uniform region (MAD = 0)
All neighbors = 1000
Expected: Skip pixel (defect = 0), no division by zero
```

---

## 2. Performance Acceptance Criteria

### AC-IMPROVE-005: SIMD Speedup Targets

| Operation | Image Size | Scalar Baseline | AVX2 Target | Min Speedup |
|-----------|------------|-----------------|-------------|-------------|
| Offset | 3072x3072 | < 55ms | < 15ms | ≥ 3.5x |
| Gain | 3072x3072 | < 55ms | < 15ms | ≥ 3.5x |
| Defect | 3072x3072 | < 95ms | < 30ms | ≥ 3.0x |
| Runtime Detect | 3072x3072 | < 35ms | < 12ms | ≥ 2.5x |

**Measurement**: `std::chrono::high_resolution_clock` before/after, average of 100 runs

### AC-IMPROVE-006: Memory Efficiency

**Given** 1000-cycle endurance test
**When** Processing loop runs continuously
**Then**:
- No memory leaks detected (AddressSanitizer clean)
- No heap growth (stable memory usage)
- No buffer overflows (BoundsSanitizer clean)

---

## 3. Quality Acceptance Criteria

### AC-IMPROVE-007: SIMD Parity Harness

**Given** 100 deterministic random inputs per operation
**When** test_simd_parity executes
**Then**:
- Offset: Bit-identical (memcmp == 0)
- Gain: 1 ULP tolerance (abs(scalar - avx2) ≤ 1 ULP)
- Defect: Bit-identical
- Runtime Detect: Bit-identical

### AC-IMPROVE-008: Test Coverage

**Given** Unit test suite executes
**When** Coverage analysis runs
**Then**:
- Statement coverage ≥ 85%
- Branch coverage ≥ 80%
- All SIMD paths tested (scalar, AVX2, SSE42 fallback)

### AC-IMPROVE-009: IEC 62304 Class B Compliance

**Given** Implementation complete
**When** Safety review conducted
**Then**:
- No C++ exceptions across C ABI boundary
- All inputs validated (NULL, dimension, format)
- Output buffers safe (zero-fill on error)
- Thread safety verified (reentrant functions)

---

## 4. Integration Acceptance Criteria

### AC-IMPROVE-010: C# P/Invoke Compatibility

**Given** ImageProcTest C# application
**When** P/Invoke calls to xpe_preprocess.dll
**Then**:
- All existing tests pass unchanged
- No ABI breakage (struct sizes unchanged)
- Pack=8 alignment preserved

### AC-IMPROVE-011: Dispatch Override

**Given** XPE_FORCE_SCALAR=1 environment variable set
**When** xpe_preprocess_init() executes
**Then**:
- Log entry: "[xpe_preprocess] SIMD dispatch: scalar (override=env.XPE_FORCE_SCALAR)"
- All operations use scalar path (verified via log)

### AC-IMPROVE-012: Benchmark Evidence

**Given** BP-01~05 benchmark pack
**When** Benchmark suite executes
**Then**:
- All operations meet performance targets
- Results reproducible across 3 runs (±5% variance)
- Benchmark manifest frozen (hash-locked)

---

## 5. Score Improvement Criteria

### AC-IMPROVE-013: Framework A Points

**Given** SPEC-XPE-IMPROVE-001 fully implemented
**When** Cross-validation assessment runs
**Then**:
- Implementation progress: +5 points (xpe_preprocess SIMD complete)
- Quality assurance: +3 points (parity harness + benchmark)
- Requirements completeness: +2 points (REQ-P1A-010~013 satisfied)
**Total**: +10 points (61 → 71)

### AC-IMPROVE-014: Framework B Points

**Given** Phase 1A correction algorithms complete
**When** Product assessment runs
**Then**:
- Functional scope: +5 points (PRE-02/03/06 complete)
- Algorithm quality: +3 points (SIMD + parity verified)
- Performance evidence: +2 points (benchmark targets met)
**Total**: +10 points (50 → 60)

---

## 6. Edge Case Scenarios

### EC-IMPROVE-001: Empty Defect Map

**Given** All-zero defect map
**When** xpe_defect_correct executes
**Then**: Image unchanged (no interpolation)

### EC-IMPROVE-002: Full Defect Map

**Given** All-ones defect map
**When** xpe_defect_correct executes
**Then**: Return XPE_ERR_INVALID_INPUT (cannot interpolate)

### EC-IMPROVE-003: Border Pixel Handling

**Given** Defect at (0,0) corner
**When** xpe_defect_correct executes
**Then**: Use only available in-bounds neighbors (no out-of-bounds access)

### EC-IMPROVE-004: Temperature Drift

**Given** Offset map with thermal drift
**When** xpe_offset_correct executes
**Then**: Residual dark mean < 2 ADU (within spec)

### EC-IMPROVE-005: Cluster Defect Artifact

**Given** Defect cluster (>50% neighborhood defective)
**When** xpe_defect_correct executes
**Then**: Median-of-valid-neighbors (not hallucination)

---

## 7. Negative Scenarios

### NS-IMPROVE-001: NULL Input

**Given** NULL image buffer
**When** Any correction function called
**Then**: Return XPE_ERR_INVALID_INPUT, no crash

### NS-IMPROVE-002: Dimension Mismatch

**Given** 512x512 image with 1024x1024 offset map
**When** xpe_offset_correct called
**Then**: Return XPE_ERR_INVALID_INPUT

### NS-IMPROVE-003: Format Mismatch

**Given** UINT16 image with FLOAT32 gain map
**When** xpe_gain_correct called
**Then**: Return XPE_ERR_UNSUPPORTED_FORMAT

### NS-IMPROVE-004: Gain Map Zero Division

**Given** Gain map with I_flat - I_dark ≤ 0
**When** xpe_gain_correct called
**Then**: Return XPE_ERR_INVALID_INPUT (pre-calibration validation)

### NS-IMPROVE-005: MAD = 0 Division

**Given** Uniform region (all pixels = 1000)
**When** xpe_defect_detect_runtime called
**Then**: Skip pixel (defect = 0), no division by zero

---

## 8. Regression Prevention

### RP-IMPROVE-001: Parity Regression

**Given** Existing scalar tests pass
**When** SIMD implementation added
**Then**: All existing tests still pass (no regression)

### RP-IMPROVE-002: Performance Regression

**Given** AVX2 implementation meets targets
**When** Code refactored
**Then**: Performance still within 10% of target

### RP-IMPROVE-003: Memory Regression

**Given** 1000-cycle test passes
**When** New SIMD code added
**Then**: No new leaks detected (ASan clean)
