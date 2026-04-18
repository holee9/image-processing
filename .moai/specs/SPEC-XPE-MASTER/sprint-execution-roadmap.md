# Sprint Execution Roadmap

**Document ID**: SPRINT-ROADMAP-001
**Version**: 1.0.0
**Date**: 2026-04-14
**Status**: Active
**Source**: SPEC-XPE-MASTER v2.0.0, ALG-SPEC-001 v3.0.0-ds2, Cross-Validation v3.0.0
**Classification**: IEC 62304 Class B

---

## 1. Sprint Methodology

### 1.1 Sprint Cycle Definition

Each sprint follows the iterative cycle:

```
PLAN → SPEC → IMPLEMENT → REVIEW → EVALUATE → FIX → (repeat until acceptance)
```

| Phase | Duration Target | Entry Criteria | Exit Criteria |
|-------|:---------------:|----------------|---------------|
| PLAN | 1 session | Sprint backlog available | Sprint scope + task breakdown approved |
| SPEC | 1 session | Plan approved | Sub-SPEC with EARS requirements + acceptance criteria |
| IMPLEMENT | 1-3 sessions | SPEC approved | All deliverables coded, unit tests written |
| REVIEW | 1 session | Implementation complete | Code review passed, no critical issues |
| EVALUATE | 1 session | Review passed | All acceptance criteria met, quality gates green |
| FIX | As needed | Evaluation failed | All identified defects resolved |

### 1.2 Quality Gates per Sprint

| Gate | Threshold | Tool |
|------|-----------|------|
| Unit Test Coverage | >= 85% (90% for core pre-processing) | CTest + gcov |
| Branch Coverage | >= 70% (60% for AI) | gcov |
| Static Analysis | 0 warnings | cppcheck + clang-tidy |
| Memory Leak | 0 leaks in 1000-frame test | ASan/Valgrind |
| Performance | Within phase budget | Benchmark harness |
| ABI Compatibility | P/Invoke test passed | C# interop test |

### 1.3 Iteration Rules

- Maximum 5 iterations per sprint (GAN Loop inspired)
- Escalation to user after 3 iterations without progress
- Improvement threshold: >= 5% per iteration
- Stagnation: 2 consecutive iterations with < 5% improvement triggers user escalation

---

## 2. Sprint Structure

### 2.1 Sprint Overview

| Sprint | SPEC | Phase | DLL | SWU | API | Priority | Dependency |
|--------|------|-------|-----|:---:|:---:|:--------:|:----------:|
| S0-A | SPEC-XPE-P0-INFRA | 0 | Build/CMake | -- | -- | Must | None |
| S0-B | SPEC-XPE-P0-COMMON | 0 | xpe_common | 7 | 18 | Must | S0-A |
| S0-C | SPEC-XPE-P0-GUI | 0 | ImageProcTest | 1 | N/A | Must | S0-A |
| S1-A | SPEC-XPE-P1A | 1a | xpe_preprocess | 9 | 18 | Must | S0-B |
| S1-B1 | SPEC-XPE-P1B-ENH | 1b | xpe_enhance_basic | 5 | 7 | Must | S1-A |
| S1-B2 | SPEC-XPE-P1B-DISP | 1b | xpe_display | 4 | 11 | Must | S1-A |
| S1-B3 | SPEC-XPE-P1B-DICOM | 1b | xpe_dicom | 4 | 10 | Must | S1-A |
| S1-B4 | SPEC-XPE-P1B-GUI | 1b | ImageProcTest | 1+1 | N/A | Should | S1-B1 |
| S2-A | SPEC-XPE-P2-ADV | 2 | xpe_enhance_advanced | 3 | 3 | Must | S1-B1 |
| S2-B | SPEC-XPE-P2-GSVG | 2 | gsvg | 4 | 8 | Must | S1-A |
| S3 | SPEC-XPE-P3-AI | 3 | xpe_ai + worker | 4 | 7 | Should | S2-A, S1-B4 |

**Total**: 11 sprints across 6 phases

### 2.2 Parallel Execution Opportunities

```
Sprint Timeline:

S0-A ──────────────┐
                    ├── S0-C ────────────┐
S0-B ──────────────┘                     │
                                         ├── S1-B1 ──────┐
S1-A ─────────────────────────────────────┤               ├── S2-A ──── S3
                                         ├── S1-B2 ──────┤
                                         ├── S1-B3 ──────┤
                                         └── S1-B4 ──────┘
S2-B ──────────────────────────────────── (parallel with S2-A)
```

Phase 1b sprints (S1-B1/B2/B3) can execute in parallel (all depend only on S1-A).
S2-A and S2-B can execute in parallel.

---

## 3. Sprint S0: Foundation (Phase 0)

### S0-A: Build Infrastructure

**Goal**: CMake build system, test framework, module scaffolding

| Task ID | Deliverable | Priority | Status |
|---------|------------|:--------:|:------:|
| S0A-01 | CMake root + modules/ CMakeLists.txt | Must | DONE |
| S0A-02 | CMakePresets.json (Debug/Release/CI/ci-common) | Must | DONE |
| S0A-03 | vcpkg.json SOUP manifest | Must | DONE |
| S0A-04 | cmake/ helpers (CompilerWarnings, Platform, DependencyRules) | Must | DONE |
| S0A-05 | Google Test + CTest integration | Must | TODO |
| S0A-06 | Module directory scaffolding (8 modules) | Must | TODO |
| S0A-07 | CI pipeline (CMake + CTest + coverage) | Should | TODO |
| S0A-08 | Coverage reporting (gcov/lcov) | Should | TODO |

**Acceptance Criteria**:
- [ ] `cmake --preset release && cmake --build --preset release` succeeds
- [ ] All 8 module directories exist with CMakeLists.txt
- [ ] CTest discovers and runs Google Test suite
- [ ] CI pipeline runs on push to main

### S0-B: xpe_common.dll Implementation

| Task ID | Deliverable | SWU | Priority | Status |
|---------|------------|-----|:--------:|:------:|
| S0B-01 | Complete xpe_common_api.h (15 declarations) | -- | Must | PARTIAL |
| S0B-02 | Logging subsystem implementation | SWU-5.4 | Must | TODO |
| S0B-03 | Memory pool hardening | SWU-5.1 | Must | PARTIAL |
| S0B-04 | Error handler validation | SWU-5.3 | Must | PARTIAL |
| S0B-05 | Config manager (xpe_init/shutdown/configure) | SWU-5.6 | Must | PARTIAL |
| S0B-06 | Unit tests for all 15 APIs (>= 85% coverage) | -- | Must | TODO |
| S0B-07 | P/Invoke compatibility test (C# <-> C ABI) | -- | Must | TODO |

**Acceptance Criteria**:
- [ ] dumpbin /exports shows exactly 15 functions
- [ ] All 15 functions have unit tests with >= 85% coverage
- [ ] P/Invoke test passes from C#
- [ ] Logging: file output + level filtering verified
- [ ] No memory leaks in 1000-cycle init/shutdown test

### S0-C: C# GUI Scaffolding

| Task ID | Deliverable | SWU | Priority | Status |
|---------|------------|-----|:--------:|:------:|
| S0C-01 | ImageProcTest WPF project (.csproj) | SWU-5.7 | Must | TODO |
| S0C-02 | P/Invoke wrapper class for xpe_common.dll | SWU-5.7 | Must | TODO |
| S0C-03 | Basic main window with DLL load test | SWU-5.7 | Must | TODO |
| S0C-04 | PipelineOrchestrator skeleton | SWU-5.7 | Should | TODO |

**Acceptance Criteria**:
- [ ] ImageProcTest.exe builds and runs
- [ ] xpe_common.dll loads via P/Invoke
- [ ] xpe_version() returns expected string in GUI

---

## 4. Sprint S1: Pre-Processing (Phase 1a)

### S1-A: xpe_preprocess.dll

**Goal**: Raw -> Clean Image pipeline (9 SWU, 18 API)

| Task ID | Deliverable | SWU | Research ID | Priority |
|---------|------------|-----|-------------|:--------:|
| S1A-01 | Offset Correction (dynamic dark) | SWU-1.1 | PRE-02 | Must |
| S1A-02 | Gain Correction (+ uint16->float32) | SWU-1.2 | PRE-03 | Must |
| S1A-03 | Defect Pixel Correction (edge-aware bilinear) | SWU-1.3 | PRE-06 | Must |
| S1A-04 | Ghost/Lag Correction Tier 1 (LTI) | SWU-1.4 | PRE-04 | Must |
| S1A-05 | Calibration Manager (load/save/validate/expiry) | SWU-1.5 | SUP-01 | Must |
| S1A-06 | Temperature Compensator | SWU-1.6 | PRE-07 | Must |
| S1A-07 | Nonlinearity Corrector | SWU-1.7 | PRE-08 | Must |
| S1A-08 | Binning Corrector (conditional) | SWU-1.8 | PRE-09 | Should |
| S1A-09 | Readout Artifact Validator | SWU-1.9 | PRE-01 | Must |
| S1A-10 | Ghost/Lag Correction Tier 2 (exposure-weighted) | SWU-1.4 | PRE-04 | Should |
| S1A-11 | Ghost/Lag Correction Tier 3 (NLCSC) | SWU-1.4 | PRE-04 | Could |

**Acceptance Criteria**:
- [ ] 18 API functions exported from xpe_preprocess.dll
- [ ] Unit test coverage >= 90% (core pre-processing), branch >= 80%
- [ ] Ghost Tier 1 <= 150ms, Tier 2 <= 190ms (3072x3072)
- [ ] Pre-processing total <= 500ms
- [ ] Pipeline order validated: Readout -> Temp -> Offset -> Nonlinearity -> Gain -> Binning -> Defect -> Ghost
- [ ] Calibration CRC verification working
- [ ] P/Invoke integration test passed

**Algorithm Reference**: ALG-SPEC-001 v3.0.0-ds2 Sections 5.1-5.8

---

## 5. Sprint S1-B: Basic Enhancement + Display + DICOM + EI (Phase 1b)

### S1-B1: xpe_enhance_basic.dll (5 SWU, 7 API)

| Task ID | Deliverable | SWU | Priority |
|---------|------------|-----|:--------:|
| S1B1-01 | Log Transform / Inverse | SWU-2.1 | Must |
| S1B1-02 | Noise Reduction (Bilateral + NLM) | SWU-2.2 | Must |
| S1B1-03 | Contrast Enhancement (CLAHE) | SWU-2.3 | Must |
| S1B1-04 | Edge Enhancement (USM) | SWU-2.4 | Must |
| S1B1-05 | Exposure Index Baseline (whole-image EI/DI) | SWU-2.10 | Must |

### S1-B2: xpe_display.dll (4 SWU, 11 API)

| Task ID | Deliverable | SWU | Priority |
|---------|------------|-----|:--------:|
| S1B2-01 | Modality LUT | SWU-3.1 | Must |
| S1B2-02 | VOI LUT (Linear/Sigmoid + presets) | SWU-3.2 | Must |
| S1B2-03 | Presentation LUT / GSDF | SWU-3.3 | Must |
| S1B2-04 | LUT Manager (preset CRUD + auto-select) | SWU-3.4 | Must |

### S1-B3: xpe_dicom.dll (4 SWU, 10 API)

| Task ID | Deliverable | SWU | Priority |
|---------|------------|-----|:--------:|
| S1B3-01 | DICOM Reader | SWU-4.1 | Must |
| S1B3-02 | DICOM Writer (+ J2K) | SWU-4.2 | Must |
| S1B3-03 | GSPS (Presentation State) | SWU-4.3 | Should |
| S1B3-04 | DICOM Network SCU (C-STORE/C-FIND) | SWU-4.4 | Should |

### S1-B4: C# GUI Enhancement

| Task ID | Deliverable | SWU | Priority |
|---------|------------|-----|:--------:|
| S1B4-01 | PipelineOrchestrator full implementation | SWU-5.7 | Must |
| S1B4-02 | QA Constancy Test | SWU-6.1 | Should |

**Phase 1b Acceptance Criteria**:
- [ ] Phase 1 total pipeline < 3000ms
- [ ] VOI LUT interactive latency <= 16ms
- [ ] DICOM DX IOD read/write verified
- [ ] GSDF compliance (xpe_presentation_lut_check_display)
- [ ] Phase 1 peak memory <= 190MB
- [ ] EI/DI: IEC 62494-1 compliant (detector-domain, single-irradiation)
- [ ] Integration test: Raw -> Preproc -> Enhance -> EI -> Display -> DICOM Write

---

## 6. Sprint S2: Advanced Enhancement + GSVG (Phase 2)

### S2-A: xpe_enhance_advanced.dll (3 SWU, 3 API)

| Task ID | Deliverable | SWU | Priority |
|---------|------------|-----|:--------:|
| S2A-01 | Multiscale Frequency Processing | SWU-2.5 | Should |
| S2A-02 | Fractional Processing | SWU-2.6 | Should |
| S2A-03 | Collimation Detection (Gradient+Hough) | SWU-2.8 | Must |
| S2A-04 | EI ROI Refinement (re-call xpe_calc_exposure_index) | SWU-2.10 | Must |

### S2-B: gsvg.dll (4 SI, 8 API)

| Task ID | Deliverable | SI | Priority |
|---------|------------|----|:--------:|
| S2B-01 | Grid Detection (DWT-based) | SI-001 | Must |
| S2B-02 | Grid Suppression | SI-002 | Must |
| S2B-03 | Virtual Grid (scatter correction) | SI-003 | Should |
| S2B-04 | Scatter LUT Manager | SI-004 | Should |

**Phase 2 Acceptance Criteria**:
- [ ] GSVG: Grid artifact visually imperceptible, MTF degradation <= 5%
- [ ] Virtual Grid: CNR >= 90% (vs 6:1 physical grid)
- [ ] EI ROI refinement: Collimation ROI-based DI recalculation works
- [ ] Phase 2 total <= 2500ms
- [ ] Phase 2 peak memory <= 440MB

---

## 7. Sprint S3: AI / Intelligence (Phase 3)

| Task ID | Deliverable | SWU | Priority |
|---------|------------|-----|:--------:|
| S3-01 | AI Worker Process (xpe_ai_worker.exe) | -- | Must |
| S3-02 | Body Part Recognition (MobileNet-v3) | SWU-2.7 | Should |
| S3-03 | AI Collimation Refinement | SWU-2.8 ext | Could |
| S3-04 | Image Stitching (phase correlation + AI) | SWU-2.9 | Should |
| S3-05 | Bone Suppression (Residual U-Net) | SWU-2.11 | Could |
| S3-06 | DL Denoiser (DnCNN) | SWU-2.12 | Could |
| S3-07 | Defect Correction ML (ANN/ViT AE) | SWU-1.3 ext | Could |

**Phase 3 Acceptance Criteria**:
- [ ] AI worker process isolated (crash isolation)
- [ ] Body Part >= 15 categories, >= 95% accuracy
- [ ] Bone Suppression PSNR >= 33dB, SSIM >= 0.97
- [ ] AI unavailable: deterministic fallback path works
- [ ] Phase 3 total <= 3000ms
- [ ] Phase 3 peak memory <= 740MB

---

## 8. Cross-Validation Resolutions Required

### 8.1 Before Sprint S0-B (Blockers)

### 8.2 Before Sprint S1-A

| ID | Issue | Resolution | Owner |
|----|-------|-----------|-------|
| XV3-01 | ALG-SPEC uses "SWU-2.0" instead of "SWU-2.10" | Fix 3 locations in ALG-SPEC-001 | Dev |
| C2 | XPE-SDD-001 6 SWU missing | Publish SDD v1.1 | QA-RA |
| N2 | RTM revision | Publish RTM v1.1 (with SDD) | QA |

### 8.3 Before Sprint S1-B

| ID | Issue | Resolution | Owner |
|----|-------|-----------|-------|
| XV3-10 | Ghost Tier 2/3 phase assignment | Decide: Phase 1a (Should) or Phase 2 (per Algorithm SPEC) | Tech Lead |
| XV3-11 | Multi-gain model phase integration | Clarify: internal to Phase 1a gain stage, enhanced in Phase 1b+ | Tech Lead |
| XV3-15 | Core preprocess coverage 85% vs 90% | Harmonize to 90% for SWU-1.1~1.9 | QA |

### 8.4 Before Sprint S2

| ID | Issue | Resolution | Owner |
|----|-------|-----------|-------|
| R1 | Multi-gain calibration API | Add exposure_level parameter or xpe_gain_correct_multi() | Tech Lead |
| R3 | Drift detection API | Add xpe_calib_assess_drift() | Tech Lead |
| R5 | Portable detector pipeline | Add conditional sub-flow to pipeline-spec | Tech Lead |

### 8.5 Document Updates (Any Time)

| ID | Issue | Target |
|----|-------|--------|
| XV3-SUB-01 | SPEC-XPE-P2-ADV API Count 4→3 | SPEC-XPE-MASTER v2.1 |
| XV3-SUB-02 | API Total 83→82 | SPEC-XPE-MASTER v2.1 |
| XV3-04 | ALG-SPEC sync action says 82, correct | ALG-SPEC-001 v3.1 |
| XV3-07 | Phase 1 vs Phase 1a naming | ALG-SPEC-001 v3.1 |
| XV3-12 | Defect ML DLL ownership | SPEC-XPE-MASTER v2.1 |
| R2 | Beam hardening in PRD | PRD-FPD-CAL-001 v1.1 |
| N10 | product.md SWU count | product.md v1.1 |
| N14 | SPEC risk summary enhancement | SPEC-XPE-MASTER v2.1 |

---

## 9. API Total Resolution (Corrected)

Cross-validation determined the correct API total:

| DLL | API Count |
|-----|:---------:|
| xpe_common | 18 |
| xpe_preprocess | 18 |
| xpe_enhance_basic | 7 |
| xpe_display | 11 |
| xpe_dicom | 10 |
| xpe_enhance_advanced | **3** (not 4 - EI moved to enhance_basic) |
| gsvg | 8 |
| xpe_ai | 7 |
| **Total** | **82** |

SPEC-XPE-MASTER Section 6 claims 83 but should be **82**.

---

## Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0.0 | 2026-04-14 | MoAI | Initial sprint roadmap based on 3-round cross-validation |

---

*Document End -- Sprint Execution Roadmap v1.0.0*
