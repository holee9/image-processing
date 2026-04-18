# SPEC-XPE-IMPROVE-001: Phase 1A Detector Correction Algorithms Completion

---
id: SPEC-XPE-IMPROVE-001
version: 1.0.0
status: Draft
created: 2026-04-19
updated: 2026-04-19
author: MoAI (Team Plan: Phase 1A Correction)
priority: High
issue_number: 0
iec62304_class: B
development_mode: TDD
---

## HISTORY

| Version | Date       | Author  | Changes |
|---------|------------|---------|---------|
| 1.0.0   | 2026-04-19 | MoAI | Initial SPEC creation based on team research |

---

## 1. Scope

### 1.1 Overview

Phase 1A 검출기 보정 알고리즘을 완성하여 프로젝트 점수를 61점에서 71점으로 향상시킵니다. 현재 SUP-01(Calibration Management)만 완료되었고, PRE-02/03/06(Correction Algorithms)는 기본 스칼라 구현만 존재하며 SIMD 최적화와 벤치마크 커버리지가 누락되어 있습니다.

### 1.2 In Scope

- **REQ-IMPROVE-010**: Offset Correction SIMD Optimization + Benchmark
- **REQ-IMPROVE-011**: Gain Correction SIMD Optimization + Benchmark
- **REQ-IMPROVE-012**: Defect Correction Edge-Aware Interpolation + SIMD + Benchmark
- **REQ-IMPROVE-013**: Runtime Defect Detection Hampel 5-Sigma Algorithm + SIMD + Benchmark
- **REQ-IMPROVE-014**: SIMD Parity Harness (test_simd_parity.cpp)
- **REQ-IMPROVE-015**: SIMD Dispatch Infrastructure (CPUID detection + override)

### 1.3 Exclusions (What NOT to Build)

- Ghost/Lag Correction (PRE-04/05) - 별도 SPEC
- Temperature Compensation (PRE-07) - 별도 SPEC
- Nonlinearity/Binning Correction - 별도 SPEC
- GPU offload - Phase 3
- ML-based defect correction - Phase 2

---

## 2. Referenced Documents

| Document ID | Title | Version | Role |
|-------------|------|---------|------|
| SPEC-XPE-P1A | Pre-processing Module | 1.2.0 | Parent SPEC |
| SPEC-XPE-P1A-SIMD-PARITY | SIMD Parity Harness | 1.0.0 | Parity requirements |
| score-improvement-plan-85.md | Score Improvement Plan | 2.1.0 | Priority justification |

---

## 3. Requirements (EARS Format)

### 3.1 Ubiquitous Requirements

#### REQ-IMPROVE-001: Module ABI Compatibility

The preprocess module **shall** maintain existing P/Invoke ABI compatibility while adding SIMD dispatch paths.

- **Verification**: `static_assert` for struct sizes, integration test

#### REQ-IMPROVE-002: Thread Safety

All SIMD processing functions **shall** be reentrant with independent caller-supplied buffers.

#### REQ-IMPROVE-003: Error Code Consistency

All functions **shall** return `XpeErrorCode` values from the defined error code set.

### 3.2 Event-Driven Requirements

#### REQ-IMPROVE-010: Offset Correction SIMD Implementation

**When** `xpe_offset_correct()` is called with AVX2-capable hardware, the module **shall** use `_mm256_subs_epu16` for saturating subtraction with bit-identical parity to scalar baseline.

**Performance**: < 15ms for 3072x3072 (AVX2), < 55ms (scalar baseline)

**Parity**: Bit-identical (UINT16 saturating subtract is exact)

#### REQ-IMPROVE-011: Gain Correction SIMD Implementation

**When** `xpe_gain_correct()` is called with AVX2-capable hardware, the module **shall** use `_mm256_mul_ps` with pre-computed reciprocal gain map.

**Performance**: < 15ms for 3072x3072 (AVX2), < 55ms (scalar baseline)

**Parity**: 1 ULP tolerance (FLOAT32 FMA rounding)

#### REQ-IMPROVE-012: Defect Correction Edge-Aware Implementation

**When** `xpe_defect_correct()` is called, the module **shall** use edge-aware bilinear interpolation with gradient-weighted neighbors (not simple average).

**Algorithm**:
- Isolated defect: weighted average using inverse gradient magnitude
- Cluster (2+ defects): median-of-valid-neighbors (8-neighborhood)
- Edge/corner: in-bounds neighbors only

**Performance**: < 30ms for 3072x3072 (AVX2), < 95ms (scalar)

#### REQ-IMPROVE-013: Runtime Defect Detection Hampel Algorithm

**When** `xpe_defect_detect_runtime()` is called, the module **shall** use Hampel 5-sigma detector (median + MAD), replacing the current mean±3σ implementation.

**Algorithm**:
```
For each pixel p(x,y):
  1. m(x,y) = median of 3x3 neighborhood (8 values)
  2. MAD(x,y) = median(|neighbor - m|)
  3. z = 0.6745 * (p - m) / MAD
  4. defect = 1 if |z| > lambda (default 5.0)
```

**Performance**: TPR ≥ 99.9%, FPR < 0.001%, < 12ms (AVX2), < 35ms (scalar)

#### REQ-IMPROVE-014: SIMD Parity Harness

**When** test_simd_parity.cpp runs, it **shall** verify scalar vs AVX2 equivalence for 100 deterministic random inputs per operation.

**Parity Rules**:
- Offset: Bit-identical
- Gain: 1 ULP tolerance
- Defect: Bit-identical
- Runtime detection: Bit-identical

#### REQ-IMPROVE-015: SIMD Dispatch Infrastructure

**When** `xpe_preprocess_init()` is called, the module **shall** detect CPUID for AVX2 support and cache the dispatch path.

**Override Priority**:
1. `XPE_FORCE_SCALAR=1` environment variable
2. `"force_scalar": true` in config JSON
3. CPUID detection (default)

---

## 4. Implementation Plan

### 4.1 File Structure

**New Files** (6):
```
modules/preprocess/src/simd_dispatch.cpp          - CPUID detection + dispatch
modules/preprocess/src/simd_offset.cpp            - AVX2 offset correction
modules/preprocess/src/simd_gain.cpp              - AVX2 gain correction
modules/preprocess/src/simd_defect.cpp            - AVX2 defect correction
modules/preprocess/src/simd_detect_runtime.cpp    - AVX2 Hampel detector
modules/preprocess/tests/test_simd_parity.cpp      - Parity harness
modules/preprocess/include/xpe/preprocess/simd_dispatch.h
```

**Modified Files** (5):
```
modules/preprocess/src/xpe_offset.cpp              - Add dispatch call
modules/preprocess/src/xpe_gain.cpp                - Add dispatch call
modules/preprocess/src/xpe_defect.cpp              - Add dispatch call + Hampel
modules/preprocess/src/xpe_preprocess_internal.h   - Add dispatch state
modules/preprocess/CMakeLists.txt                  - Add new sources
```

**Reference Files** (Keep Unmodified):
```
modules/preprocess/src/offset_correct.cpp          - Scalar baseline
modules/preprocess/src/gain_correct.cpp            - Scalar baseline
modules/preprocess/src/defect_correct.cpp          - Scalar baseline
```

### 4.2 Implementation Order

| Phase | Task | Effort | Dependencies |
|-------|------|--------|--------------|
| 1 | simd_dispatch.cpp/h | 4h | None |
| 2 | test_simd_parity.cpp framework | 6h | Phase 1 |
| 3 | simd_offset.cpp | 3h | Phase 1 |
| 4 | Modify xpe_offset.cpp | 2h | Phase 3 |
| 5 | simd_gain.cpp | 5h | Phase 1 |
| 6 | Modify xpe_gain.cpp | 3h | Phase 5 |
| 7 | simd_defect.cpp | 12h | Phase 1 |
| 8 | Modify xpe_defect.cpp | 2h | Phase 7 |
| 9 | simd_detect_runtime.cpp (Hampel) | 16h | Phase 1 |
| 10 | Modify defect detection dispatch | 8h | Phase 9 |

**Total Estimated Effort**: 61 hours

### 4.3 Technical Approach

#### SIMD Dispatch Pattern
```cpp
XpeErrorCode xpe_offset_correct(XpeImageBuffer* img, const XpeImageBuffer* offsetMap) {
    // Validation (unchanged)
    if (!img || !offsetMap) return XPE_ERR_INVALID_INPUT;
    
    // SIMD dispatch (NEW)
    switch (xpe::simd::get_current_path()) {
        case xpe::simd::DispatchPath::AVX2:
            return xpe::simd::offset_correct_avx2(img, offsetMap);
        case xpe::simd::DispatchPath::SSE42:
            return xpe::simd::offset_correct_sse42(img, offsetMap);
        default:
            return xpe::simd::offset_correct_scalar(img, offsetMap);
    }
}
```

#### Tile-Based Memory Strategy
For 3072x3072 images (9.4MB float32), use 64x48 tiles to fit in L1 cache:
```cpp
constexpr size_t TILE_WIDTH = 64;
constexpr size_t TILE_HEIGHT = 48;  // 3072 pixels, 12KB (fits L1)
```

---

## 5. Acceptance Criteria

### AC-IMPROVE-001: SIMD Performance Targets

| Operation | Scalar | AVX2 Target | Speedup |
|-----------|--------|-------------|---------|
| Offset | < 55ms | < 15ms | ≥ 3.5x |
| Gain | < 55ms | < 15ms | ≥ 3.5x |
| Defect | < 95ms | < 30ms | ≥ 3x |
| Runtime Detect | < 35ms | < 12ms | ≥ 2.5x |

### AC-IMPROVE-002: Parity Harness

**Given** 100 deterministic random inputs for each operation
**When** test_simd_parity runs
**Then** scalar and AVX2 outputs match parity rules (bit-identical or 1 ULP)

### AC-IMPROVE-003: Hampel Detector Accuracy

**Given** 3072x3072 image with injected 5-sigma transients
**When** xpe_defect_detect_runtime runs
**Then** TPR ≥ 99.9%, FPR < 0.001%

### AC-IMPROVE-004: Edge-Aware Interpolation

**Given** defect map with isolated and clustered defects
**When** xpe_defect_correct runs
**Then** gradient delta at defect boundaries < 10% of local contrast

### AC-IMPROVE-005: IEC 62304 Compliance

**Given** unit test suite runs
**When** coverage analysis completes
**Then** ≥ 85% statement coverage, no memory leaks (1000-cycle endurance)

---

## 6. Risk Assessment

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| SIMD defect correction complexity | High | Medium | Hybrid: scalar detection + SIMD interpolation |
| Hampel sorting network performance | High | Medium | Fallback: scalar with multi-threading |
| Parity violation (float rounding) | Medium | Low | 1 ULP tolerance documented in tests |
| Tile boundary handling | Medium | Low | Unit tests for odd-sized images |

---

## 7. Success Criteria

1. **Performance**: All AVX2 targets met (offset/gain < 15ms, defect < 30ms, detect < 12ms)
2. **Parity**: test_simd_parity passes for all operations
3. **Accuracy**: Hampel detector TPR ≥ 99.9%, FPR < 0.001%
4. **Coverage**: ≥ 85% statement coverage
5. **Compatibility**: Existing C# P/Invoke tests pass unchanged
6. **Score**: Framework A +10 points (61 → 71), Framework B +10 points (50 → 60)

---

## 8. Next Steps

1. **Immediate**: Create SPEC branch and start Phase 1 (simd_dispatch)
2. **Parallel**: Set up benchmark pack BP-01~05 dataset
3. **After SPEC approval**: Execute /moai run SPEC-XPE-IMPROVE-001 --team
