# SPEC-SIMD-001: SIMD Scalar Reference + Full-Operation Parity

**Document ID**: SPEC-SIMD-001
**Version**: 1.0.0
**Date**: 2026-04-22
**Status**: Active
**Owner Lane**: Pre-A (`dev/preprocess`)
**Parent SPEC**: SPEC-XPE-P1A v1.3.0
**Companion**: `.moai/specs/SPEC-XPE-P1A/simd-parity-harness.md` v2.0.0
**IEC 62304 Class**: B
**Score Contribution**: Framework A +5 (구현진행도 + 검증완전성)

---

## HISTORY

| Version | Date       | Author       | Changes |
|---------|------------|--------------|---------|
| 1.0.0   | 2026-04-22 | manager-spec | 초기 작성 — SIMD 전 연산 parity 확장 정의 |

---

## 1. Purpose and Scope

SPEC-XPE-P1A (REQ-P1A-040) defines SIMD optimization for offset, gain, and defect corrections.
`simd-parity-harness.md` v2.0.0 specifies the parity verification protocol.

**Current Gap**: Parity tests exist only for offset correction (AVX2/AVX-512/NEON).
Gain correction (FLOAT32, 1 ULP) and defect correction (UINT16, bit-identical) have no parity tests.

This SPEC defines requirements to complete full-operation parity coverage and integrate all parity tests
into the CMake build.

### Covered Operations

| Operation | File | Scalar Reference | SIMD Target | Parity Rule |
|-----------|------|-----------------|-------------|-------------|
| Offset subtract | offset_correct.cpp | ✅ Complete | AVX2 ✅ | Bit-identical |
| Gain multiply (reciprocal+FMA) | gain_correct.cpp | ✅ Complete | AVX2 ✅ | 1 ULP FLOAT32 |
| Defect bilinear interp | defect_correct.cpp | ✅ Complete | AVX2 🔴 | Bit-identical |
| Defect median filter | defect_correct.cpp | ✅ Complete | AVX2 🔴 | Bit-identical |
| Runtime detection (MAD) | runtime_detection.cpp | ✅ Complete | AVX2 🔴 | Bit-identical |

Legend: ✅ = implemented, 🔴 = parity test missing

---

## 2. EARS Requirements

### REQ-SIMD-001: Offset Parity Coverage (AVX2)

**When** the pre-processing pipeline runs offset correction on AVX2-capable hardware,
**the system shall** produce bit-identical output compared to the scalar reference
**for all 6 test cases** in `test_offset_correct_avx2_parity.cpp`.

- **Status**: 🔴 Test file exists but uses OLD 2-arg API `xpe_offset_correct(input, offsetMap)`.
  Current API is 3-arg: `xpe_offset_correct(input, output, metadata)`.
  Tests compile but fail at runtime with error code -6 (XPE_ERROR_INVALID_ARGS).
- **Action**: Rewrite `test_offset_correct_avx2_parity.cpp` SetUp() to use new 3-arg API
  (load calibration via `xpe_calib_load()`, call new signature). Then add to CMakeLists.
- **Priority**: P0 (gate blocker)

### REQ-SIMD-002: Gain Parity Coverage (AVX2)

**When** the pre-processing pipeline runs gain correction using the reciprocal+FMA path on AVX2-capable hardware,
**the system shall** produce output within 1 ULP of the scalar FLOAT32 reference
**for all inputs in** the deterministic test harness (seed = `CRC32("XPE-SIMD-PARITY-v1")`).

- **Status**: Test file does NOT exist (`test_gain_correct_avx2_parity.cpp`).
- **Action**: Write `test_gain_correct_avx2_parity.cpp` per harness protocol (100 random + 30 edge cases).
- **Priority**: P0

### REQ-SIMD-003: Defect Bilinear Parity Coverage (AVX2)

**When** the defect correction runs bilinear interpolation in AVX2 dispatch mode,
**the system shall** produce bit-identical output compared to the scalar UINT16 reference
**for all defect map configurations** in the parity harness.

- **Status**: Test file does NOT exist (`test_defect_correct_avx2_parity.cpp`).
- **Action**: Write `test_defect_correct_avx2_parity.cpp`.
- **Priority**: P1

### REQ-SIMD-004: Runtime Detection MAD Parity Coverage (AVX2)

**When** runtime defect detection runs on AVX2-capable hardware,
**the system shall** produce bit-identical TPR/FPR results compared to the scalar MAD reference
**for all test frames** in the parity harness.

- **Status**: Test file does NOT exist.
- **Action**: Write `test_runtime_detection_avx2_parity.cpp`.
- **Priority**: P1

### REQ-SIMD-005: CMakeLists Integration

**When** `BUILD_TESTS=ON` or `BUILD_TESTING=ON` is set,
**the system shall** compile and register ALL parity test files in `XPE_TEST_SOURCES`
**so that** `ctest -R Parity` runs the complete parity suite.

- **Status**: 🔴 No parity tests currently compiled.
- **Action**: Update `modules/preprocess/CMakeLists.txt` to include all parity test files.
- **Priority**: P0 (gate blocker — parity tests are worthless if not run in CI)

### REQ-SIMD-006: Scalar Reference Performance Baseline

**When** the pre-processing pipeline runs on hardware without AVX2,
**the system shall** complete offset, gain, and defect correction for a 3072×3072 UINT16 frame
**within** the scalar performance budget:
- Offset: < 55ms
- Gain: < 55ms
- Defect (bilinear): < 95ms

- **Status**: Performance targets defined in SPEC-XPE-P1A. Not measured under DegradedMode.
- **Action**: Add timing assertions in DegradedMode tests for scalar-only path.
- **Priority**: P1

---

## 3. Acceptance Criteria

| REQ | Acceptance Gate | Measurable Evidence |
|-----|----------------|---------------------|
| REQ-SIMD-001 | `ctest -R OffsetAVX2Parity` → 6/6 PASS | CI log |
| REQ-SIMD-002 | `ctest -R GainAVX2Parity` → 130/130 PASS | CI log (100 random + 30 edge) |
| REQ-SIMD-003 | `ctest -R DefectAVX2Parity` → all PASS | CI log |
| REQ-SIMD-004 | `ctest -R RuntimeDetectAVX2Parity` → all PASS | CI log |
| REQ-SIMD-005 | `ctest -R Parity --output-on-failure` exits 0 | CI green |
| REQ-SIMD-006 | DegradedMode timing assertions < budget | test output |

---

## 4. Implementation Priority for Pre-A Lane

### P0 (Unblocks CI Gate)

1. Add `tests/test_offset_correct_avx2_parity.cpp` to `XPE_TEST_SOURCES` in CMakeLists (REQ-SIMD-001, REQ-SIMD-005)
2. Write `test_gain_correct_avx2_parity.cpp` (REQ-SIMD-002) — most impactful, 1 ULP FLOAT32 gate
3. Rebuild and verify `ctest -R Parity` is GREEN in CI

### P1 (Score Enhancement)

4. Write `test_defect_correct_avx2_parity.cpp` (REQ-SIMD-003)
5. Write `test_runtime_detection_avx2_parity.cpp` (REQ-SIMD-004)
6. Add scalar timing assertions in DegradedMode tests (REQ-SIMD-006)

---

## 5. What Already Exists (Do Not Duplicate)

| Artifact | Location | Notes |
|----------|----------|-------|
| SIMD parity protocol | `simd-parity-harness.md` v2.0.0 | Normative, use as test design reference |
| Offset AVX2 parity tests | `tests/test_offset_correct_avx2_parity.cpp` | Exists, needs CMakeLists entry |
| Offset AVX-512 parity tests | `tests/test_offset_correct_avx512_parity.cpp` | Exists, needs CMakeLists entry |
| Offset NEON parity tests | `tests/test_offset_correct_neon_parity.cpp` | Exists, NEON=ARM; skip-on-x86 guard required |
| SIMD implementation | `offset_correct.cpp`, `gain_correct.cpp` | AVX2 complete (M2) |
| DegradedMode GTests | `test_preprocess_degraded.cpp` | 6/6 PASS (frozen 2026-04-22) |

---

## 6. Traceability

| This SPEC | Parent SPEC | SVVP | IEC 62304 |
|-----------|-------------|------|-----------|
| REQ-SIMD-001~006 | REQ-P1A-040 | Section 5.1 (BP-SIMD addendum) | Class B §5.6.3 |

---

*Document End — SPEC-SIMD-001 v1.0.0*
