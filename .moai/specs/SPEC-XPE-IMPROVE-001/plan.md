# Implementation Plan: SPEC-XPE-IMPROVE-001

**Document ID**: PLAN-XPE-IMPROVE-001
**Version**: 1.0.0
**Date**: 2026-04-19
**Status**: Draft

---

## 1. Task Decomposition

### Phase 1: Infrastructure (Priority: HIGH, Effort: 10h)

| Task ID | Description | File | Effort | Dependencies |
|---------|-------------|------|--------|--------------|
| T-001 | CPUID detection + dispatch state | simd_dispatch.cpp/h | 4h | None |
| T-002 | Parity harness framework | test_simd_parity.cpp | 6h | T-001 |

### Phase 2: Offset Correction (Priority: HIGH, Effort: 5h)

| Task ID | Description | File | Effort | Dependencies |
|---------|-------------|------|--------|--------------|
| T-003 | AVX2 offset correction | simd_offset.cpp | 3h | T-001 |
| T-004 | Integrate offset dispatch | xpe_offset.cpp | 2h | T-003 |

### Phase 3: Gain Correction (Priority: HIGH, Effort: 8h)

| Task ID | Description | File | Effort | Dependencies |
|---------|-------------|------|--------|--------------|
| T-005 | AVX2 gain correction | simd_gain.cpp | 5h | T-001 |
| T-006 | Integrate gain dispatch | xpe_gain.cpp | 3h | T-005 |

### Phase 4: Defect Correction (Priority: MEDIUM, Effort: 14h)

| Task ID | Description | File | Effort | Dependencies |
|---------|-------------|------|--------|--------------|
| T-007 | AVX2 edge-aware defect correction | simd_defect.cpp | 12h | T-001 |
| T-008 | Integrate defect dispatch | xpe_defect.cpp | 2h | T-007 |

### Phase 5: Runtime Detection (Priority: MEDIUM, Effort: 24h)

| Task ID | Description | File | Effort | Dependencies |
|---------|-------------|------|--------|--------------|
| T-009 | AVX2 Hampel 5-sigma detector | simd_detect_runtime.cpp | 16h | T-001 |
| T-010 | Integrate detection dispatch | xpe_defect.cpp | 8h | T-009 |

**Total Effort**: 61 hours

---

## 2. Technology Stack

### SIMD Instruction Sets
- **Primary**: AVX2 (Intel Haswell 2013+, AMD Excavator 2015+)
- **Fallback**: SSE4.2 (Intel Core 2006+, AMD K10 2007+)
- **Baseline**: Scalar C++ (universal)

### Key Intrinsics
- `_mm256_subs_epu16` - Saturating subtract (offset)
- `_mm256_mul_ps` - Float multiply (gain)
- `_mm256_cvtepu16_ps` - UINT16→float conversion
- `_mm256_store_ps` / `_mm256_load_ps` - Memory operations

### Dependencies
- xpe_common.dll (Layer 0) - Already exists
- SUP-01 calibration infrastructure - Already complete
- Google Test - Already configured
- CMake 3.25+ - Already configured

---

## 3. Risk Analysis

### High-Risk Items

1. **SIMD Defect Correction (T-007)**
   - Risk: Complex branching doesn't vectorize well
   - Mitigation: Hybrid approach (scalar detection + SIMD interpolation)
   - Fallback: Keep scalar if < 2x speedup

2. **Hampel Detector (T-009)**
   - Risk: Median calculation requires sorting
   - Mitigation: SIMD sorting networks for 3x3 neighborhoods
   - Fallback: Scalar with multi-threading

### Medium-Risk Items

3. **Gain Correction Buffer Ownership (T-006)**
   - Risk: Ownership transfer of pixel buffer
   - Mitigation: Document clearly in Doxygen
   - Validation: Integration test for memory leaks

4. **Tile Boundary Handling (All SIMD tasks)**
   - Risk: Edge tiles require special handling
   - Mitigation: Clamped tile sizes + unit tests
   - Validation: Test with 1x1, 63x47, 3072x3072

---

## 4. Development Methodology

**Mode**: TDD (RED-GREEN-REFACTOR)
**Test Framework**: Google Test
**Coverage Target**: ≥ 85% statement coverage

### TDD Cycle per Task

1. **RED**: Write failing test for SIMD function
2. **GREEN**: Implement SIMD version (or stub)
3. **REFACTOR**: Optimize while keeping tests green

### Parity Test Strategy

For each SIMD operation:
1. Generate 100 deterministic random inputs
2. Run scalar baseline
3. Run AVX2 implementation
4. Verify parity (bit-identical or 1 ULP)

---

## 5. Quality Gates

### Pre-Commit
- LSP zero errors, zero warnings
- All tests passing
- Code formatted (clang-format)

### Pre-Merge
- Parity harness passes (100 inputs × 4 operations)
- Performance targets met (benchmark on 3072x3072)
- Coverage ≥ 85%
- IEC 62304 Class B compliance verified

### Release
- Integration tests pass (C# P/Invoke)
- Memory safety verified (1000-cycle endurance)
- Documentation complete (Doxygen comments)

---

## 6. Rollout Plan

### Stage 1: Infrastructure (T-001, T-002)
- Deliverable: CPUID detection + parity harness framework
- Validation: Unit tests pass
- Risk: Low

### Stage 2: Offset/Gain (T-003~T-006)
- Deliverable: AVX2 offset + gain correction
- Validation: Parity passes, performance targets met
- Risk: Low-Medium

### Stage 3: Defect (T-007, T-008)
- Deliverable: AVX2 edge-aware defect correction
- Validation: Parity passes, artifact suppression verified
- Risk: Medium (hybrid approach if needed)

### Stage 4: Runtime Detection (T-009, T-010)
- Deliverable: Hampel 5-sigma detector
- Validation: TPR/FPR targets met, performance verified
- Risk: High (may need fallback)

---

## 7. Success Metrics

| Metric | Target | Measurement |
|--------|--------|-------------|
| Performance | Offset < 15ms, Gain < 15ms, Defect < 30ms, Detect < 12ms | Benchmark on 3072x3072 |
| Parity | 100% pass rate | test_simd_parity |
| Accuracy | TPR ≥ 99.9%, FPR < 0.001% | Hampel detector test |
| Coverage | ≥ 85% | gcov/lcov |
| Memory | No leaks (1000 cycles) | AddressSanitizer |
| Score Improvement | +10 points | 61 → 71 |
