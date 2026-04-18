# Benchmark Pack Manifest: Pre Lane (BP-01 through BP-05)

**Document ID**: BP-01-05-PREPROCESS
**Version**: 1.0.0
**Date**: 2026-04-18
**Status**: Draft (freeze pending dataset capture)
**Scope**: Pre Lane (SPEC-XPE-P1A) — Offset, Gain, Defect, Lag-reference, SIMD parity
**Parent**: `docs/project/Algorithm-Benchmark-Pack-Spec.md` v1.3.0
**SVVP Linkage**: `docs/project/XPE-SVVP-001_System_Verification_Validation_Plan.md` v1.4.0 Section 5.1
**IEC 62304 Class**: B

---

## HISTORY

| Version | Date       | Author         | Changes |
|---------|------------|----------------|---------|
| 1.0.0   | 2026-04-18 | manager-spec   | Initial Pre Lane manifest (BP-01 to BP-05 detail + BP-SIMD addendum) |

---

## 1. Purpose

This manifest extracts the Pre Lane portion of the XPE benchmark pack and provides the concrete dataset, tolerance, pass-criteria, and reproducibility contract for SPEC-XPE-P1A acceptance. The parent document (`Algorithm-Benchmark-Pack-Spec.md` v1.3.0) defines the overall taxonomy; this document operationalises it for Pre Lane release gates.

Benchmark packs covered in this manifest:

| Pack | Parent Scope | Pre Lane REQ Coverage |
|------|--------------|----------------------|
| BP-01 | Temperature sweep (offset, temp compensation, drift) | REQ-P1A-010 |
| BP-02 | Multi-gain linearity (nonlinearity, gain) | REQ-P1A-011 |
| BP-03 | Heel-effect SID variation (gain, heel compensation) | REQ-P1A-011 |
| BP-04 | Sparse defect + cluster defect | REQ-P1A-012, REQ-P1A-013 |
| BP-05 | Lag history sequence | **Referenced only** (belongs to SPEC-XPE-P1B Ghost) |
| BP-SIMD (addendum) | Scalar vs AVX2 parity | REQ-P1A-040 |

BP-05 is listed here to document the Pre Lane integration boundary; actual lag correction acceptance is in SPEC-XPE-P1B.

---

## 2. BP-01: Temperature Sweep

### 2.1 Algorithm Coverage

- REQ-P1A-010 (Offset correction execution)
- PREP-time + temperature interpolation sub-routines (Pre Lane internal)

### 2.2 Required Strata (from parent spec)

- Low / nominal / high temperature
- Varying PREP times (0.5s, 2s, 5s, 10s)

### 2.3 Dataset

| Item | Value |
|------|-------|
| Detector model (reference) | Varex 4343R or equivalent 3072x3072 a-Si |
| Temperature points | 15 C, 20 C, 25 C, 30 C, 35 C, 40 C (6 points) |
| PREP time points | 0.5 s, 2 s, 5 s, 10 s (4 points) |
| Frames per (T, PREP) cell | 16 (for HF averaging) + 3 (for LF validation) |
| Image format | UINT16, 3072 x 3072 |
| Total frames | 24 cells x 19 frames = 456 frames |
| Metadata | Temperature sensor reading, PREP time, detector serial, timestamp |
| Content hash | SHA-256 per frame + SHA-256 of concatenated manifest |

### 2.4 Pass Criteria

| Metric | Target | Measurement |
|--------|--------|-------------|
| Residual dark mean (post-correction) | < 2 ADU across all cells | ROI average over 9 regions |
| Residual dark sigma | < 3 ADU across all cells | Same 9 regions |
| Temperature stability (20-35 C) | < 5 ADU drift | Cross-cell comparison |
| PREP-time model residual | < 2 ADU at all PREP times | Step-PREP regression |

### 2.5 Tolerance

- Absolute: 2 ADU mean, 3 ADU sigma
- Relative: sigma/mean < 0.003 at the nominal 25 C, 2 s cell

### 2.6 Reproducibility

- Dataset lives at `benchmark/datasets/BP-01-temp-sweep/` (to be populated)
- `manifest.json` lists per-frame SHA-256 + acquisition metadata
- Re-run: `tools/benchmark/run_bp01.py --seed 42 --output reports/BP-01-<date>.json`
- Freeze rule: content hashes locked on release gate; any replacement requires version bump per parent spec Section 7

---

## 3. BP-02: Multi-Gain Linearity

### 3.1 Algorithm Coverage

- REQ-P1A-011 (Gain correction execution)
- Reciprocal gain map optimisation (internal)
- NaN/Inf validation (REQ-P1A-033 cross-check)

### 3.2 Required Strata

- Low / medium / high exposure (5, 50, 500 mR at detector)
- Multiple detector modes (single-gain and multi-gain)

### 3.3 Dataset

| Item | Value |
|------|-------|
| Exposure levels | 1, 2, 5, 10, 20, 50, 100, 200, 500 mR (9 levels) |
| Flat-field frames per exposure | 16 |
| Dark-frame pairs per session | 16 |
| Image format | UINT16 input, FLOAT32 output |
| Total frames | 9 x 32 = 288 frames |
| Metadata | kVp, mAs, SID, temperature, calibration session ID |

### 3.4 Pass Criteria

| Metric | Target |
|--------|--------|
| Flat-field residual uniformity (post-gain) | sigma/mean < 0.5% over 90% FOV |
| Linearity R^2 (exposure vs mean signal) | >= 0.9999 |
| Monotonicity violations | Zero |
| NaN/Inf pixels in output | Zero |
| Scalar-vs-AVX2 parity | 1 ULP (per simd-parity-harness.md) |

### 3.5 Tolerance

- sigma/mean: 0.005 (absolute)
- R^2: 0.0001 deviation from 1.0 acceptable
- FLOAT32 1 ULP (computed as `nextafterf(x, INF) - x`)

### 3.6 Reproducibility

- Dataset at `benchmark/datasets/BP-02-multigain/`
- Run: `tools/benchmark/run_bp02.py`

---

## 4. BP-03: Heel-Effect SID Variation

### 4.1 Algorithm Coverage

- REQ-P1A-011 (Gain correction with Duo-SID heel compensation)

### 4.2 Required Strata

- At least two calibration SID anchors + extrapolated clinical SID

### 4.3 Dataset

| Item | Value |
|------|-------|
| Calibration SIDs | 1000 mm, 1800 mm (anchor pair) |
| Test SIDs | 1200 mm, 1500 mm, 2000 mm (extrapolation test) |
| Flat-field frames per SID | 16 |
| Anode orientation variants | Cathode-anode axis aligned with detector long axis |

### 4.4 Pass Criteria

| Metric | Target |
|--------|--------|
| Heel-effect RMSE reduction | >= 80% (Wang 2013 baseline) |
| Uniformity at 1500 mm SID | sigma/mean < 1% over 90% FOV |
| Extrapolation stability | RMSE at 2000 mm SID < 1.2x RMSE at 1800 mm |

### 4.5 Reproducibility

- Dataset at `benchmark/datasets/BP-03-heel/`
- Run: `tools/benchmark/run_bp03.py`

---

## 5. BP-04: Defect Map Effectiveness

### 5.1 Algorithm Coverage

- REQ-P1A-012 (Defect correction execution)
- REQ-P1A-013 (Runtime defect detection)

### 5.2 Required Strata

- Isolated bad pixels
- Short lines (3-10 pixels long)
- Clustered patches (2x2, 3x3, 5x5)

### 5.3 Dataset

| Item | Value |
|------|-------|
| Base frames | 10 clinical-grade dark-corrected 3072 x 3072 UINT16 |
| Injected defects per frame | 100 isolated + 20 short lines + 10 clusters (3x3) + 5 clusters (5x5) |
| Ground truth | Original pixel values before injection (for NMSE) |
| Static BPM | Injected defect locations (for REQ-P1A-012 baseline) |
| Runtime challenge | Additional 50 random transient defects NOT in BPM (for REQ-P1A-013) |

### 5.4 Pass Criteria

| Metric | Target | REQ |
|--------|--------|-----|
| Isolated-pixel NMSE improvement over copy-neighbor | >= 10x | REQ-P1A-012 |
| Cluster (3x3) NMSE | < 150 on UINT16 scale | REQ-P1A-012 |
| Cluster (5x5) NMSE | < 250 on UINT16 scale | REQ-P1A-012 |
| Artificial-edge count at defect boundaries | 0 (gradient delta < 10% local contrast) | REQ-P1A-012 |
| Runtime detection TPR on injected transients | >= 99.9% | REQ-P1A-013 |
| Runtime detection FPR on clean clinical frames | < 0.001% | REQ-P1A-013 |

### 5.5 Reproducibility

- Dataset at `benchmark/datasets/BP-04-defect/`
- Random defect injection seed: `CRC32("BP-04-v1")` = `0xBP04C0DE` (placeholder, compute at freeze)
- Run: `tools/benchmark/run_bp04.py --seed 0xBP04C0DE`

---

## 6. BP-05: Lag History Sequence (Reference Only)

### 6.1 Scope Note

BP-05 is **not** a Pre Lane acceptance gate. It belongs to SPEC-XPE-P1B (Ghost/Lag correction). It is listed here to document the integration boundary — the output of Pre Lane (REQ-P1A-010/011/012) is the input to BP-05 testing.

### 6.2 Pre Lane Integration Contract

Pre Lane must produce, for each frame in the BP-05 sequence:
- Offset-corrected UINT16 buffer (REQ-P1A-010)
- Gain-corrected FLOAT32 buffer (REQ-P1A-011)
- Defect-corrected buffer (REQ-P1A-012)
- No NaN/Inf (REQ-P1A-033)
- No wraparound (REQ-P1A-010 floor-at-zero)

BP-05 acceptance criteria are defined in SPEC-XPE-P1B documentation.

---

## 7. BP-SIMD Addendum: Scalar vs AVX2 Parity

### 7.1 Algorithm Coverage

- REQ-P1A-040 (SIMD Optimisation)

### 7.2 Dataset

See `.moai/specs/SPEC-XPE-P1A/simd-parity-harness.md` Section 4:
- 6 operations x 3 shapes x 100 random inputs = 1800 test cases
- Plus 30 hand-crafted edge cases
- Deterministic RNG seed: `CRC32("XPE-SIMD-PARITY-v1")`

### 7.3 Pass Criteria

| Operation | Rule | Allowed Failures |
|-----------|------|------------------|
| Offset subtract (UINT16) | Bit-identical | 0 / 300 |
| Defect bilinear (UINT16) | Bit-identical | 0 / 300 |
| Defect median (UINT16) | Bit-identical | 0 / 300 |
| Runtime detect (UINT16 MAD) | Bit-identical | 0 / 300 |
| Gain reciprocal (FLOAT32) | 1 ULP | 0 / 300 |
| Gain polynomial (FLOAT32) | 1 ULP | 0 / 300 |
| Edge cases (all ops) | Per rule | 0 / 30 |
| **Total** | | **0 / 1830** |

### 7.4 Reproducibility

- Harness at `modules/preprocess/tests/test_simd_parity.cpp`
- Run: `ctest -R SimdParityTest --output-on-failure`
- CI integration: required green before Pre Lane PR merges

---

## 8. Freeze and Promotion Rules

Per parent spec `Algorithm-Benchmark-Pack-Spec.md` Section 7:

Once this manifest is used for a Pre Lane release gate:

1. Manifest schema version is frozen (this file's `Version` field)
2. Dataset content hashes are frozen (SHA-256 per frame + manifest hash)
3. Any replacement requires a version bump and rationale
4. Historical Pre Lane results must remain reproducible against the old manifest version

Freeze checklist (to be completed by Pre Lane lead before M2 release gate):

- [ ] Dataset files captured and placed under `benchmark/datasets/BP-0N-<topic>/`
- [ ] SHA-256 computed for each file, listed in `<dataset>/manifest.json`
- [ ] SHA-256 of concatenated `manifest.json` listed in this document Section N.3
- [ ] Benchmark runner scripts (`tools/benchmark/run_bp0N.py`) produce deterministic JSON output
- [ ] CI pipeline gated on benchmark pass
- [ ] Version bumped to 1.1.0 (from 1.0.0 Draft to 1.1.0 Frozen)

Until freeze is complete, this manifest is `Draft` status and cannot be cited as release evidence.

---

## 9. Traceability to SVVP

Per `XPE-SVVP-001` v1.4.0 Section 5.1 (Mandatory benchmark packs):

| SVVP Req | This Manifest | Status |
|----------|---------------|--------|
| BP-01 temperature sweep | Section 2 | Specified (dataset freeze pending) |
| BP-02 multi-gain linearity | Section 3 | Specified (dataset freeze pending) |
| BP-03 heel-effect SID | Section 4 | Specified (dataset freeze pending) |
| BP-04 sparse + cluster defect | Section 5 | Specified (dataset freeze pending) |
| BP-05 lag history | Section 6 (reference) | Out of Pre Lane scope |

Pre Lane release gate (`Pre Lane M2 acceptance`) requires:

- BP-01, BP-02, BP-03, BP-04 datasets captured AND passing
- BP-SIMD addendum 1800/1800 parity tests passing
- Results archived per IEC 62304 Class B retention policy

---

## 10. References

- Parent manifest: `docs/project/Algorithm-Benchmark-Pack-Spec.md` v1.3.0
- SVVP: `docs/project/XPE-SVVP-001_System_Verification_Validation_Plan.md` v1.4.0
- SPEC parent: `.moai/specs/SPEC-XPE-P1A/spec.md` v1.2.0
- SIMD harness: `.moai/specs/SPEC-XPE-P1A/simd-parity-harness.md` v1.0.0
- Research: `.moai/specs/SPEC-XPE-P1A/research.md` v2.0.0 (Sections 8, 9)
- Deep research archive: `docs/project/XPE-PreProcess-DeepResearch.json` (ARCHIVAL)
- Algorithm spec: `docs/post-processing/xpe/XPE-ALG-001_Unified_Algorithm_Development_Specification.md`
- Wang 2013 (Duo-SID heel effect) — cited in BP-03
- Jeon et al. PMC7930811 (CNN defect correction) — cited in BP-04
- Schirrmacher et al. arXiv:2310.11637v2 (FixPix) — cited in BP-04
- Intel Intrinsics Guide 2023 — cited in BP-SIMD
- IEEE-754-2019 — cited for 1 ULP semantics

---

*Document End - BP-01-05 Pre Lane Manifest v1.0.0*
