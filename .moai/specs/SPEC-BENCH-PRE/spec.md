# SPEC-BENCH-PRE: Preprocessing Benchmark Freeze Specification (BP-01~05)

**Document ID**: SPEC-BENCH-PRE
**Version**: 1.0.1
**Date**: 2026-10-03
**Status**: Active — Frozen

> **상태 메모 (2026-10-03, QA-A-231, #252)**: 이 동결은 판정선 회귀를 막지 못한다. REQ-BPRE-002~005 의 판정선(잔류 dark ADU, R², heel RMSE, NMSE)과 BP-SIMD 1830건을 단언하는 시험은 `modules/preprocess` 에 없다. CI 가 돌리는 것은 `ci.yml` preprocess-tests 의 `PreprocessDegraded.*` 6개 스모크(교정 미적재 시 `XPE_ERR_NOT_INITIALIZED` 반환 등)뿐이며, 그 시험 이름의 BP 번호(BP03=결함, BP04=고스트, BP05=온도·비선형)는 이 SPEC·manifest 의 번호(BP-03 heel, BP-04 결함, BP-05 lag)와 대응하지 않는다. 벤치마크 워크플로 `benchmark-regression.yml` 은 `ci-post` 프리셋(`BUILD_PREPROCESS=OFF`)으로 BP-06~09 만 실행하므로 preprocess 를 빌드하지 않는다. manifest 가 적은 데이터셋·`manifest.json` 해시·러너 `tools/benchmark/run_bp0N.py`·`test_results.json` 은 저장소에 없다. 아래 동결 내용(요구 문구·표)은 바꾸지 않았고, 각 항목의 실태는 해당 위치의 "실태 메모"에 적었다. 동결 범위를 스모크로 선언할지, 게이트를 실제로 만들지는 #252 에서 결정한다.

**Owner Lane**: Pre-A (`dev/preprocess`)
**Parent SPEC**: SPEC-XPE-P1A v1.3.0
**Companion Manifest**: `benchmark/BP-01-05-preprocess-manifest.md` v1.1.0
**IEC 62304 Class**: B
**Score Contribution**: Framework A +2 (검증완전성 + 벤치마크회귀방지)

---

## HISTORY

| Version | Date       | Author       | Changes |
|---------|------------|--------------|---------|
| 1.0.0   | 2026-04-22 | manager-spec | 초기 작성 — BP-01~05 DegradedMode 6/6 PASS 기반 freeze SPEC |
| 1.0.1   | 2026-10-03 | lead         | 상태 메모 추가 (QA-A-231, #252) — 판정선 시험 부재, 벤치마크 워크플로가 preprocess 를 빌드하지 않음. REQ-BPRE-001·007 아래와 §4 표 아래(002~006 해당)에 실태 메모. 동결 내용 자체는 바꾸지 않음 |

---

## 1. Purpose and Scope

This SPEC formalises the preprocessing benchmark freeze for BP-01 through BP-05.
It defines EARS-format requirements for benchmark regression prevention and references
the frozen manifest at `benchmark/BP-01-05-preprocess-manifest.md` v1.1.0.

### Scope

| Pack | Algorithm | REQ Coverage |
|------|-----------|-------------|
| BP-01 | Temperature sweep (offset, temp compensation) | REQ-P1A-010 |
| BP-02 | Multi-gain linearity (gain correction) | REQ-P1A-011 |
| BP-03 | Heel-effect SID variation (Duo-SID gain) | REQ-P1A-011 |
| BP-04 | Sparse + cluster defect correction | REQ-P1A-012, REQ-P1A-013 |
| BP-05 | Lag history sequence (reference only) | SPEC-XPE-P1B (Ghost) |
| BP-SIMD | Scalar vs AVX2 parity (addendum) | REQ-P1A-040 |

---

## 2. EARS Requirements

### REQ-BPRE-001: DegradedMode Smoke Test Coverage

**When** the preprocessing module is tested under DegradedMode (null/identity calibration),
**the system shall** pass all 6 benchmark smoke tests (BP-01 through BP-05 + BP-SIMD)
**without** crash, undefined behavior, or memory leak.

- **Status**: ✅ 6/6 PASS (frozen 2026-04-22)
- **Evidence**: `test_preprocess_degraded.cpp`, CI archive
- **실태 메모 (2026-10-03, QA-A-231, #252)**: 통과하는 시험은 `PreprocessDegraded` 6개(BP01, BP02, BP03, BP04, BP05 ×2)다. BP05 가 둘이고 BP-SIMD 시험은 포함되지 않는다. 근거는 `modules/preprocess/tests/test_preprocess_degraded.cpp` 와 `ci.yml` preprocess-tests 의 ctest 로그이며, "CI archive"(`test_results.json`)는 저장소에 없다.

### REQ-BPRE-002: Temperature Sweep Residual Dark

**When** offset correction processes temperature sweep data (BP-01, 6 temperature points x 4 PREP times),
**the system shall** produce residual dark mean < 2 ADU and sigma < 3 ADU across all cells.

- **Status**: DegradedMode PASS ✅; full dataset freeze pending (M2 gate)
- **Measurement**: ROI average over 9 regions per frame
- **Tolerance**: Absolute 2 ADU mean, 3 ADU sigma

### REQ-BPRE-003: Multi-Gain Linearity

**When** gain correction processes multi-exposure data (BP-02, 9 exposure levels),
**the system shall** achieve flat-field residual uniformity sigma/mean < 0.5% and linearity R^2 >= 0.9999.

- **Status**: DegradedMode PASS ✅; full dataset freeze pending (M2 gate)
- **NaN/Inf**: Zero NaN/Inf pixels in output (REQ-P1A-033)
- **SIMD Parity**: 1 ULP FLOAT32 (per simd-parity-harness.md)

### REQ-BPRE-004: Heel-Effect Compensation

**When** Duo-SID heel compensation processes SID variation data (BP-03, 5 SID points),
**the system shall** achieve heel-effect RMSE reduction >= 80% and uniformity sigma/mean < 1%.

- **Status**: DegradedMode PASS ✅; full dataset freeze pending (M2 gate)

### REQ-BPRE-005: Defect Map Effectiveness

**When** defect correction processes injected defect data (BP-04, isolated + lines + clusters),
**the system shall** achieve:
- Isolated-pixel NMSE improvement >= 10x over copy-neighbor
- Cluster (3x3) NMSE < 150 on UINT16 scale
- Runtime detection TPR >= 99.9%, FPR < 0.001%

- **Status**: DegradedMode PASS ✅; full dataset freeze pending (M2 gate)

### REQ-BPRE-006: SIMD Parity Regression Prevention

**When** any preprocessing SIMD code is modified,
**the system shall** maintain bit-identical parity (UINT16 ops) or 1 ULP parity (FLOAT32 ops)
against the scalar reference for all 1830 test cases in the parity harness.

- **Status**: Partially implemented — REQ-SIMD-001~006 tracked in SPEC-SIMD-001
- **Gate**: `ctest -R Parity --output-on-failure` exits 0

### REQ-BPRE-007: Benchmark Regression CI Gate

**When** a PR targets `dev/preprocess` or `main`,
**the CI pipeline shall** run all BP-01~05 benchmark tests and block merge on any regression.

- **Status**: CI workflow exists (`benchmark-regression.yml`)
- **Freeze Rule**: Content hashes locked; replacement requires version bump
- **실태 메모 (2026-10-03, QA-A-231, #252)**: BP-01~05 에 대해서는 충족되지 않는다. `benchmark-regression.yml` 은 `ci-post`(`BUILD_PREPROCESS=OFF`)로 BP-06~09 만 실행한다. BP-01~05 중 CI 가 실행하는 것은 `ci.yml` preprocess-tests 의 `PreprocessDegraded.*` 6개 스모크뿐이고, 이 시험들은 판정선을 단언하지 않는다.

---

## 3. Acceptance Criteria

| REQ | Acceptance Gate | Measurable Evidence |
|-----|----------------|---------------------|
| REQ-BPRE-001 | DegradedMode 6/6 PASS | CI log (frozen 2026-04-22) |
| REQ-BPRE-002 | Residual dark < 2 ADU mean, < 3 ADU sigma | Benchmark report |
| REQ-BPRE-003 | Uniformity < 0.5%, R^2 >= 0.9999 | Benchmark report |
| REQ-BPRE-004 | RMSE reduction >= 80%, sigma/mean < 1% | Benchmark report |
| REQ-BPRE-005 | NMSE < 150 (3x3), TPR >= 99.9%, FPR < 0.001% | Benchmark report |
| REQ-BPRE-006 | 1830/1830 parity PASS | CI log |
| REQ-BPRE-007 | CI blocks on regression | CI workflow green |

---

## 4. Freeze Status

| Pack | DegradedMode | Full Dataset | Freeze Date |
|------|:-----------:|:------------:|:-----------:|
| BP-01 | ✅ PASS | Pending (M2 gate) | 2026-04-22 (DM) |
| BP-02 | ✅ PASS | Pending (M2 gate) | 2026-04-22 (DM) |
| BP-03 | ✅ PASS | Pending (M2 gate) | 2026-04-22 (DM) |
| BP-04 | ✅ PASS | Pending (M2 gate) | 2026-04-22 (DM) |
| BP-05 | ✅ PASS (ref) | Out of Pre scope | — |
| BP-SIMD | ✅ PASS | Pending (SPEC-SIMD-001) | 2026-04-22 (DM) |

> **실태 메모 (2026-10-03, QA-A-231, #252)**: "DegradedMode ✅ PASS" 는 판정선과 무관한 스모크의 통과다 — DegradedMode 시험은 REQ-BPRE-002~005 의 판정선(ADU·R²·RMSE·NMSE)을 단언하지 않는다. 판정선 시험은 현재 없다. BP-01·BP-03 은 측정 대상 기능(온도 보간·PREP 시간 모델, kVp/SID 보간)이 구현에 없어 지금은 어떤 시험으로도 판정선을 만족시킬 수 없다(`preprocess_api.h` 가 NOT IMPLEMENTED 로 적음). BP-SIMD 의 `SimdParityTest`·`test_simd_parity.cpp` 는 없으며 `ctest -R Parity` 가 고르는 시험 40개에 1830 이라는 수는 나타나지 않는다. `PreprocessDegraded` 시험 이름에 붙은 BP 번호는 위 표의 BP 번호와 무관하다.

---

## 5. Traceability

| This SPEC | Parent SPEC | Manifest | SVVP | IEC 62304 |
|-----------|-------------|----------|------|-----------|
| REQ-BPRE-001~007 | REQ-P1A-010~013, REQ-P1A-040 | BP-01-05-manifest v1.1.0 | Section 5.1 | Class B §5.6.3 |

---

## 6. References

- Manifest: `benchmark/BP-01-05-preprocess-manifest.md` v1.1.0
- Parent SPEC: `.moai/specs/SPEC-XPE-P1A/spec.md` v1.2.0
- SIMD SPEC: `.moai/specs/SPEC-SIMD-001/spec.md` v1.0.0
- SVVP: `docs/project/XPE-SVVP-001_System_Verification_Validation_Plan.md` v1.4.0
- CI workflow: `.github/workflows/benchmark-regression.yml`

---

*Document End — SPEC-BENCH-PRE v1.0.1*
