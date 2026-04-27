# SPEC-BENCH-POST: Post-Processing Benchmark Freeze Specification (BP-06~09)

**Document ID**: SPEC-BENCH-POST
**Version**: 1.0.0
**Date**: 2026-04-22
**Status**: Active — Frozen
**Owner Lane**: Post-B (`dev/postprocess`)
**Parent SPEC**: SPEC-XPE-P1B-ENH, SPEC-XPE-P2-ADV
**Companion Baseline**: `benchmark/BP-06-09-post-benchmark-baseline.md`
**IEC 62304 Class**: B
**Score Contribution**: Framework A +1 (검증완전성)

---

## HISTORY

| Version | Date       | Author       | Changes |
|---------|------------|--------------|---------|
| 1.0.0   | 2026-04-22 | manager-spec | 초기 작성 — BP-06~09 BenchmarkFreeze 4/4 PASS 기반 freeze SPEC |

---

## 1. Purpose and Scope

This SPEC formalises the post-processing benchmark freeze for BP-06 through BP-09.
It defines EARS-format requirements for benchmark regression prevention covering
GSVG, Collimation, Exposure Index, and Deviation Index modules.

### Scope

| Pack | Module | Function | GTest Gate |
|------|--------|----------|------------|
| BP-06 | gsvg.dll | Version probe benchmark | BenchmarkFreeze.BP06_GsvgVersionProbeBaseline |
| BP-07 | xpe_enhance_advanced.dll | Collimation detection | BenchmarkFreeze_BP07_CollimationDetectionBaseline |
| BP-08 | xpe_enhance_basic.dll | EI calculation | BenchmarkFreeze_BP08_EICalcTimeBaseline |
| BP-09 | xpe_enhance_basic.dll | DI calculation | BenchmarkFreeze_BP09_DICalcTimeBaseline |

---

## 2. EARS Requirements

### REQ-BPOST-001: GSVG Version Probe Performance

**When** the GSVG module processes 1024 consecutive version probes,
**the system shall** complete all probes within 5000 microseconds.

- **Status**: ✅ PASS — 0 ms measured (2026-04-22)
- **Module**: gsvg.dll
- **GTest**: `BenchmarkFreeze.BP06_GsvgVersionProbeBaseline`

### REQ-BPOST-002: Collimation Detection Performance

**When** collimation detection processes a 512x512 synthetic input,
**the system shall** complete detection within 500 milliseconds.

- **Status**: ✅ PASS — 10 ms measured (2026-04-22)
- **Module**: xpe_enhance_advanced.dll
- **GTest**: `CollimationDetectTest.BenchmarkFreeze_BP07_CollimationDetectionBaseline`

### REQ-BPOST-003: Exposure Index Calculation Performance

**When** exposure index calculation processes a 512x512 input,
**the system shall** complete calculation within 25 milliseconds.

- **Status**: ✅ PASS — 0 ms measured (2026-04-22)
- **Module**: xpe_enhance_basic.dll
- **GTest**: `ExposureIndex.BenchmarkFreeze_BP08_EICalcTimeBaseline`

### REQ-BPOST-004: Deviation Index Calculation Performance

**When** deviation index calculation processes a 512x512 input,
**the system shall** complete calculation within 25 milliseconds.

- **Status**: ✅ PASS — 0 ms measured (2026-04-22)
- **Module**: xpe_enhance_basic.dll
- **GTest**: `ExposureIndex.BenchmarkFreeze_BP09_DICalcTimeBaseline`

### REQ-BPOST-005: DegradedMode Coverage

**When** any Post-B module DLL is absent from the runtime directory,
**the system shall** gracefully degrade without crash, and benchmark tests for absent modules shall be skipped.

- **Status**: ✅ DegradedMode.BP06_* through DegradedMode.BP10_* PASS
- **Test driver**: `test_xpe_common.exe`
- **CI**: Staged directory with target DLL removed

### REQ-BPOST-006: Benchmark Regression CI Gate

**When** a PR targets `dev/postprocess` or `main`,
**the CI pipeline shall** run all BP-06~09 benchmark tests and block merge on any regression.

- **Status**: CI workflow active (`benchmark-regression.yml`)
- **Threshold policy**: Host-tolerant GTest gates, not clinical performance claims

---

## 3. Acceptance Criteria

| REQ | Acceptance Gate | Measurable Evidence |
|-----|----------------|---------------------|
| REQ-BPOST-001 | 1024 probes < 5000 us | CI log (frozen 2026-04-22) |
| REQ-BPOST-002 | 512x512 collimation < 500 ms | CI log (frozen 2026-04-22) |
| REQ-BPOST-003 | 512x512 EI calc < 25 ms | CI log (frozen 2026-04-22) |
| REQ-BPOST-004 | 512x512 DI calc < 25 ms | CI log (frozen 2026-04-22) |
| REQ-BPOST-005 | DegradedMode PASS | CI log |
| REQ-BPOST-006 | CI blocks on regression | CI workflow green |

---

## 4. Freeze Status

| Pack | Result | Measured Time | Threshold | Freeze Date |
|------|--------|:------------:|:---------:|:-----------:|
| BP-06 | ✅ PASS | 0 ms | 5000 us | 2026-04-22 |
| BP-07 | ✅ PASS | 10 ms | 500 ms | 2026-04-22 |
| BP-08 | ✅ PASS | 0 ms | 25 ms | 2026-04-22 |
| BP-09 | ✅ PASS | 0 ms | 25 ms | 2026-04-22 |

---

## 5. Traceability

| This SPEC | Parent SPEC | Baseline | SVVP | IEC 62304 |
|-----------|-------------|----------|------|-----------|
| REQ-BPOST-001~006 | SPEC-XPE-P1B-ENH, SPEC-XPE-P2-ADV | BP-06-09-baseline | Section 5.1 | Class B §5.6.3 |

---

## 6. References

- Baseline: `benchmark/BP-06-09-post-benchmark-baseline.md`
- Parent SPECs: `.moai/specs/SPEC-XPE-P1B-ENH/spec.md`, `.moai/specs/SPEC-XPE-P2-ADV/spec.md`
- GSVG SRS: `docs/post-processing/gsvg/GSVG-SRS-001_Requirements.md`
- SVVP: `docs/project/XPE-SVVP-001_System_Verification_Validation_Plan.md` v1.4.0
- CI workflow: `.github/workflows/benchmark-regression.yml`
- DegradedMode driver: `tools/ci/Test-DegradedMode.ps1`

---

*Document End — SPEC-BENCH-POST v1.0.0*
