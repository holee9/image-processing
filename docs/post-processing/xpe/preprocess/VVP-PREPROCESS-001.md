# VVP Addendum: Pre Lane (Preprocessing) Verification & Validation Plan

**Document ID**: VVP-PREPROCESS-001
**Version**: 1.1.0
**Date**: 2026-04-22
**Parent**: XPE-VVP-001 v1.1 (docs/post-processing/xpe/)
**Grandparent**: XPE-SVVP-001 v1.4.0 (docs/project/)
**Scope**: Pre Lane only (SPEC-XPE-P1A, modules/preprocess/, modules/common/)
**IEC 62304 Clause**: 5.5.1–5.5.5 (L1 Unit), 5.6.1–5.6.7 (L2 Integration), 5.7.1–5.7.5 (L3 System)
**Safety Classification**: Class B
**Author**: manager-spec (Pre Lane upgrade)

---

## HISTORY

| Version | Date       | Author         | Changes |
|---------|------------|----------------|---------|
| 1.0.0   | 2026-04-18 | manager-spec   | Initial Pre Lane-specific VVP addendum covering L1-L4 test strategy for REQ-P1A-010~013, SIMD parity, P/Invoke. |
| 1.1.0   | 2026-04-22 | manager-spec   | SPEC-SIMD-001 반영: 4개 AVX2 parity test 파일 등록, REQ-SIMD-001~004 추가, BP-01~05 DegradedMode freeze(6/6 PASS) 반영, test count 202/202로 갱신. |

---

## 1. Purpose

This addendum operationalises `XPE-VVP-001` for the Pre Lane scope. It maps each Pre Lane requirement (REQ-P1A-010 through REQ-P1A-042) to concrete verification activities at Levels L1 through L4 as defined in `XPE-SVVP-001` Section 2.

Out of scope (covered elsewhere):
- L5 Usability/Clinical — `XPE-SVVP-001` only
- L6 Field Performance — `XPE-SVVP-001` only
- GUI-specific tests — Lane C (`xpe-gui` worktree)
- Post-processing modules — Lane B (`xpe-post` worktree)

---

## 2. Pre Lane Requirement → VV Level Mapping

| REQ ID | Description | L1 Unit | L2 Integration | L3 System | L4 Feature |
|--------|-------------|:-------:|:--------------:|:---------:|:----------:|
| REQ-P1A-001 | Module init | ✓ | | ✓ | |
| REQ-P1A-002 | P/Invoke ABI | ✓ | ✓ | | |
| REQ-P1A-003 | Thread safety | ✓ | | ✓ | |
| REQ-P1A-004 | Error code consistency | ✓ | | | |
| REQ-P1A-005 | Input validation | ✓ | | | |
| **REQ-P1A-010** | **Offset correction** | **✓** | **✓** | **✓** | **✓ (BP-01)** |
| **REQ-P1A-011** | **Gain correction** | **✓** | **✓** | **✓** | **✓ (BP-02, BP-03)** |
| **REQ-P1A-012** | **Defect correction** | **✓** | **✓** | **✓** | **✓ (BP-04)** |
| **REQ-P1A-013** | **Runtime detection** | **✓** | | **✓** | **✓ (BP-04 runtime)** |
| REQ-P1A-014~019 | Calibration I/O | ✓ | ✓ | ✓ | |
| REQ-P1A-020~022 | State guards | ✓ | | | |
| REQ-P1A-030~033 | Unwanted behaviour | ✓ | | | |
| **REQ-P1A-040** | **SIMD parity** | **✓** | | | **✓ (BP-SIMD)** |
| REQ-P1A-041 | Readout validation | ✓ | | | |
| REQ-P1A-042 | Parameter range | ✓ | | | |

---

## 3. Level 1 (Unit Verification) — Pre Lane Specifics

Follows `XPE-VVP-001` Section 2. Pre Lane-specific rules and targets:

### 3.1 Test Suite Mapping

| REQ ID | Test File (modules/preprocess/tests/) | Target Count |
|--------|---------------------------------------|--------------|
| REQ-P1A-010 | test_offset_correct.cpp | 15+ |
| REQ-P1A-011 | test_gain_correct.cpp | 15+ |
| REQ-P1A-012 | test_defect_correct.cpp | 20+ |
| REQ-P1A-013 | test_defect_correct.cpp (runtime section) + new test_runtime_detect.cpp | 10+ |
| REQ-P1A-014~019 | test_xpe_calib_*.cpp (6 files) | 56 currently passing |
| REQ-P1A-020~022 | test_boundary.cpp, test_xpe_preprocess_init.cpp | 10+ |
| REQ-P1A-030~033 | test_integration.cpp + test_xpe_preprocess.cpp | 15+ |
| REQ-P1A-040 / REQ-SIMD-001 | test_offset_correct_avx2_parity.cpp | 6 cases (3-arg API) |
| REQ-SIMD-002 | test_gain_correct_avx2_parity.cpp | 8 cases (1 ULP FLOAT32) |
| REQ-SIMD-003 | test_defect_correct_avx2_parity.cpp | 6 cases (bit-identical) |
| REQ-SIMD-004 | test_runtime_detection_avx2_parity.cpp | 4 cases (bit-identical) |
| BP-01~05 DegradedMode | test_preprocess_degraded.cpp | 6/6 PASS (Frozen 2026-04-22) |
| REQ-P1A-041~042 | test_readout_validate.cpp | 8+ |

### 3.2 Acceptance Criteria (from parent XPE-VVP-001 §2.2)

- Statement coverage ≥ 80% per unit (Pre Lane target: ≥ 85%)
- Branch coverage ≥ 70% per unit
- Zero test failures
- Zero memory leaks (ASan clean)
- Zero critical static analysis findings

### 3.3 Pre Lane-Specific L1 Pass/Fail Criteria

| Criterion | Target | Verification |
|-----------|--------|--------------|
| REQ-P1A-010 floor-at-zero | No negative / no wraparound | Edge-case test with offset > image |
| REQ-P1A-011 no NaN/Inf | Zero violations | FLOAT32 isfinite() check on output |
| REQ-P1A-012 no artificial edges | Gradient delta < 10% local contrast | Gradient analysis at defect boundaries |
| REQ-P1A-013 TPR / FPR | TPR ≥ 99.9%, FPR < 0.001% | 1000-frame synthetic injection test |
| REQ-SIMD-001~004 AVX2 parity | 4 files, 24 total cases | offset/gain/defect/runtime_detect parity tests |
| BP-01~05 DegradedMode | 6/6 PASS (Frozen) | `test_preprocess_degraded.cpp` |

---

## 4. Level 2 (Integration Verification) — Pre Lane Specifics

### 4.1 P/Invoke Integration Tests

Location: `clients/ImageProcTest/` (C# test project).

| Test | REQ | Pass Criteria |
|------|-----|---------------|
| XpeImageBuffer marshaling | REQ-P1A-002 | sizeof matches C++ (36 bytes), Pack=8 alignment verified |
| xpe_offset_correct via P/Invoke | REQ-P1A-010 | Same output as direct C++ call on shared buffer |
| xpe_gain_correct via P/Invoke | REQ-P1A-011 | Same output as direct C++ call |
| xpe_defect_correct via P/Invoke | REQ-P1A-012 | Same output as direct C++ call |
| Calibration load round-trip (C#→C++→C#) | REQ-P1A-014~019 | Byte-identical buffer contents |

### 4.2 xpe_common ↔ xpe_preprocess Linkage

| Test | REQ | Pass Criteria |
|------|-----|---------------|
| Alert queue propagation | REQ-P0-019 ↔ REQ-P1A-xxx | Alerts raised in preprocess surface through xpe_common queue |
| Log routing | REQ-P0-023 ↔ REQ-P1A-xxx | Preprocess logs appear in xpe_common log file with correct severity |
| Memory allocation through xpe_common | REQ-P0-015 ↔ REQ-P1A-002 | All preprocess allocations use xpe_common primitives |

### 4.3 Pass Criteria (L2)

All integration tests pass on the CI pipeline with both Debug and Release configurations. Any divergence between direct-C++ and P/Invoke outputs is a BLOCKER.

---

## 5. Level 3 (System Verification) — Pre Lane Specifics

### 5.1 Full Preprocess Pipeline Test

| Step | Operation | REQ |
|------|-----------|-----|
| 1 | Load 3072x3072 UINT16 raw frame | (test harness) |
| 2 | Load offset/gain/defect maps from XCal files | REQ-P1A-014, 015, 016 |
| 3 | xpe_offset_correct | REQ-P1A-010 |
| 4 | xpe_gain_correct | REQ-P1A-011 |
| 5 | xpe_defect_correct | REQ-P1A-012 |
| 6 | Verify output: no NaN/Inf, no wraparound, within expected range | REQ-P1A-033 |
| 7 | Measure end-to-end latency | Performance target |

Pass criteria:
- End-to-end latency < 500 ms (scalar) or < 100 ms (AVX2) on reference hardware
- Output SHA-256 matches reference golden output (bit-identical for scalar path)
- No error codes returned

### 5.2 Concurrent Access Test

Four threads independently run the full pipeline with independent buffers:
- No data races (ThreadSanitizer clean)
- All 4 threads complete with XPE_OK
- Aggregate throughput at least 3x single-thread rate (parallel efficiency >= 75%)

### 5.3 1000-Cycle Endurance Test

1000 consecutive iterations of (init → process → shutdown):
- Heap RSS returns to baseline (within 1%)
- No file handle leaks
- No GDI/COM handle leaks (Windows-specific check)

Currently SKIPPED in test suite (1 of 90 tests); must be enabled before Pre Lane release gate.

---

## 6. Level 4 (Feature Benchmark Verification) — Pre Lane Specifics

Per `XPE-SVVP-001` Section 5.1 and this module's `benchmark/BP-01-05-preprocess-manifest.md`:

| Benchmark Pack | REQ Coverage | Pass Criterion |
|----------------|--------------|----------------|
| BP-01 (Temperature sweep) | REQ-P1A-010 | See BP-01-05-preprocess-manifest Section 2.4 |
| BP-02 (Multi-gain linearity) | REQ-P1A-011 | See manifest Section 3.4 |
| BP-03 (Heel-effect SID) | REQ-P1A-011 | See manifest Section 4.4 |
| BP-04 (Defect density) | REQ-P1A-012, 013 | See manifest Section 5.4 |
| BP-01 (Temperature sweep) | REQ-P1A-010 | DegradedMode 6/6 PASS (Frozen 2026-04-22) |
| BP-02 (Multi-gain linearity) | REQ-P1A-011 | See manifest Section 3.4 |
| BP-03 (Heel-effect SID) | REQ-P1A-011 | See manifest Section 4.4 |
| BP-04 (Defect density) | REQ-P1A-012, 013 | See manifest Section 5.4 |
| BP-05 (DegradedMode stress) | REQ-P1A-013 | DegradedMode 6/6 PASS (Frozen 2026-04-22) |
| SIMD Parity (SPEC-SIMD-001) | REQ-SIMD-001~004 | 24 parity test cases across 4 files (ctest -R Parity) |

Pass criteria:
- All Pre Lane benchmark packs frozen (SHA-256 hashes locked)
- All packs pass per their manifest criteria
- Results archived per IEC 62304 retention policy

---

## 7. Explicit Pre Lane Pass/Fail Summary

Pre Lane M2 release gate is PASSED when all of the following are true:

- [ ] L1: All 202 currently-passing tests remain passing (Pre Lane 202/202 GREEN as of 2026-04-22)
- [ ] L1: `ctest -R Parity` passes — 24 AVX2 parity cases across offset/gain/defect/runtime_detect
- [ ] L1: BP-01~05 DegradedMode 6/6 PASS (Frozen 2026-04-22)
- [ ] L1: Statement coverage >= 85% on modules/preprocess/
- [ ] L2: P/Invoke marshaling test matrix fully green
- [ ] L2: xpe_common ↔ xpe_preprocess linkage tests fully green
- [ ] L3: Full pipeline test passes at 3072x3072 within budget
- [ ] L3: Concurrent access test passes on 4 threads
- [ ] L3: 1000-cycle endurance test enabled AND passing (no leaks)
- [ ] L4: BP-01 through BP-05 Frozen AND passing (DegradedMode 6/6 PASS confirmed 2026-04-22)
- [ ] L4: SIMD parity suite (SPEC-SIMD-001) — ctest -R Parity GREEN in CI
- [ ] Code review complete per TRUST 5 (see `.claude/rules/moai/core/moai-constitution.md`)
- [ ] No MX:WARN without MX:REASON in modules/preprocess/

Pre Lane M2 release gate is FAILED when any of the following is true:
- Any L1/L2/L3/L4 test fails
- Parity rule violated (any operation)
- Performance budget exceeded without documented justification
- P/Invoke output differs from direct-C++ output
- Memory leak detected in endurance test

---

## 8. Responsibilities

| Role | Responsibility |
|------|----------------|
| Pre Lane lead (manager-spec + expert-backend) | Maintain this plan, approve freeze |
| Implementation agent (manager-tdd via xpe-pre worktree) | Implement REQ-P1A-010~013 with tests |
| CI system (Gitea Actions) | Automate L1/L2 on every PR |
| Release engineer | Run L3 + L4 benchmark packs at release candidate gate |
| QA (expert-testing) | Validate test coverage, sign off on release gate |

---

## 9. References

- Parent: `docs/post-processing/xpe/XPE-VVP-001_Verification_Validation_Plan.md` v1.1
- Grandparent: `docs/project/XPE-SVVP-001_System_Verification_Validation_Plan.md` v1.4.0
- SPEC: `.moai/specs/SPEC-XPE-P1A/spec.md` v1.2.0
- Acceptance: `.moai/specs/SPEC-XPE-P1A/acceptance.md`
- Research: `.moai/specs/SPEC-XPE-P1A/research.md` v2.0.0
- SIMD harness: `.moai/specs/SPEC-XPE-P1A/simd-parity-harness.md` v2.0.0
- SIMD parity SPEC: `.moai/specs/SPEC-SIMD-001/spec.md` v1.0.0
- Benchmark: `benchmark/BP-01-05-preprocess-manifest.md` v1.1.0 (Frozen)
- IEC 62304 (2015+Amd1:2015) Clauses 5.5, 5.6, 5.7
- TRUST 5 framework: `.claude/rules/moai/core/moai-constitution.md`

---

*Document End - VVP-PREPROCESS-001 v1.1.0*
