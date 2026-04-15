# XPE Implementation Analysis Report

**Document ID**: XPE-IMPL-ANALYSIS-001  
**Version**: 1.3.0  
**Date**: 2026-04-15  
**Status**: Controlled Draft  
**Source**: `SPEC-XPE-MASTER v2.2.0`, `product.md v1.2.0`, `structure.md v1.2.0`, `pipeline-spec.md v1.5.0`, `api-spec.md v1.3.0`, `xpe-algorithm-spec-deepsync.md v3.2.0-ds4`, `XPE-Brainstorming-DeepSync-Execution.md v1.1.0`

---

## 1. Executive Snapshot

The repository is in an early implementation state.

### 1.1 What is already present

- CMake root and CI/CD workflows
- `modules/common/`
- `tests/common_smoke/`
- canonical project documents under `docs/project/`

### 1.2 What is not yet present as source modules

- `modules/preprocess/`
- `modules/enhance_basic/`
- `modules/enhance_advanced/`
- `modules/ai/`
- `modules/display/`
- `modules/dicom/`
- `gsvg/`
- `gui/`

### 1.3 Canonical target scope

| Metric | Canonical value |
|---|---:|
| XPE SWU | 38 |
| GSVG SI | 4 |
| Executable units | 42 |
| Native exported APIs | 82 |
| Delivery binaries | 10 |

---

### 1.4 Expected first-implementation score

For this document, **first implementation complete** means:

- `Phase 0`, `Phase 1a`, and `Phase 1b` are implemented,
- deterministic baseline image delivery works end-to-end,
- `Phase 2` and `Phase 3` remain unfinished or optional.

Under that assumption, the expected completion score is:

- **overall program score**: **66 / 100**
- **baseline product score**: **87 / 100**

#### 1.4.1 Score breakdown

| Category | Weight | Expected score | Rationale |
|---|---:|---:|---|
| functional scope | 35 | 26 | baseline modules implemented, but premium and AI still open |
| performance and memory | 15 | 12 | Phase 1 budgets are reachable, but not yet proven on full premium paths |
| algorithm quality and evidence | 20 | 11 | detector baseline can be strong, but benchmark freeze and task-based evidence remain incomplete |
| regulatory and document readiness | 15 | 9 | `docs/project` is strong, but regulated IEC package sync is still open |
| operational readiness | 15 | 8 | reject-analysis, DI drift, and field telemetry structures exist, but field evidence does not |
| **Total** | **100** | **66** | **expected first-implementation overall score** |

#### 1.4.2 What the 66 means

- It is a strong deterministic baseline, not a finished premium product.
- It implies good first-release feasibility for a controlled baseline deployment.
- It does not imply readiness for earth-class premium differentiation yet.

---

## 2. Verified Implemented Assets

| Area | Evidence in repo | Assessment |
|---|---|---|
| `xpe_common` headers and sources | `modules/common/include/...`, `modules/common/src/...` | partial foundation implementation |
| Native smoke test | `tests/common_smoke/test_common_smoke.cpp` | present |
| Local build helper and CI docs | `tools/ci/`, `docs/development/` | present |
| GitHub CI/CD | `.github/workflows/` | present |

The repository does not yet contain source implementations for the core detector and enhancement modules described by the architecture documents.

---

## 3. Current Architecture Gap

### 3.1 Module gap by workstream

| Workstream | Planned owner binaries | Code status |
|---|---|---|
| Common ABI and lifecycle | `xpe_common.dll` | partially present |
| Detector correction | `xpe_preprocess.dll` | missing |
| Deterministic enhancement | `xpe_enhance_basic.dll` | missing |
| Deterministic premium enhancement | `xpe_enhance_advanced.dll` | missing |
| Assistive AI | `xpe_ai.dll`, `xpe_ai_worker.exe` | missing |
| Display | `xpe_display.dll` | missing |
| DICOM | `xpe_dicom.dll` | missing |
| GSVG | `gsvg.dll` | missing |
| C# orchestration and QA | `ImageProcTest.exe` | missing |

### 3.2 Documentation gap by regulated package

The canonical `docs/project/` set is now aligned around the architectural model, but the regulated IEC package under `docs/post-processing/xpe/` still requires follow-on synchronization for:

- SRS
- SDD
- RTM
- VVP

This remains an open compliance task, not a code task.

---

## 4. Highest-Value Next Implementations

| Priority | Deliverable | Why it is first |
|---:|---|---|
| 1 | complete `xpe_common` ABI surface and tests | every native module depends on it |
| 2 | add `xpe_preprocess` with detector-domain benchmark harness | detector correction is the release baseline |
| 3 | add `xpe_enhance_basic`, `xpe_display`, and `xpe_dicom` | closes deterministic Phase 1 output |
| 4 | add benchmark pack manifests and automated evaluation harness | prevents unprovable quality claims |
| 5 | add Phase 2 deterministic premium features | only after Phase 1 metrics stabilize |
| 6 | add AI worker and assistive models | only after deterministic path is production-grade |

---

### 4.1 Score-lift path from 66 to 85

The most efficient path to **85 / 100** is not to start with AI. It is to close proof, determinism, and premium deterministic value first.

| Workstream | Expected point gain | Why it moves the score |
|---|---:|---|
| complete `xpe_common` ABI, alert, sidecar, and ownership rules | +2 | reduces integration and diagnostic debt |
| implement `xpe_preprocess` with scalar reference plus parity harness | +5 | closes the detector-domain core where most quality risk lives |
| implement `xpe_enhance_basic`, `xpe_display`, and `xpe_dicom` with end-to-end tests | +4 | turns architecture into a usable deterministic product |
| freeze benchmark manifests, dataset hashes, and evaluation automation | +3 | converts quality claims into reproducible evidence |
| synchronize `docs/post-processing/xpe/` SRS, SDD, RTM, and VVP | +3 | removes the biggest remaining compliance debt |
| implement selected Phase 2 deterministic premium features with graceful fallback | +4 | raises quality ceiling without breaking claim boundary |
| implement reject-analysis and DI drift telemetry schema end-to-end | +2 | improves operational readiness and field-quality score |
| extend CI to parity, performance-budget, and release-bundle evidence checks | +2 | turns one-time quality into enforced quality |
| **Total uplift** | **+25** | **66 -> 91 possible if fully executed** |

To reach **85**, the minimum practical subset is:

1. `xpe_preprocess` plus parity harness,
2. `xpe_enhance_basic` plus display and DICOM,
3. benchmark freeze and evaluation automation,
4. IEC package synchronization,
5. one bounded deterministic premium increment such as baseline collimation plus ROI EI refinement.

That subset is estimated to contribute **+19 points**, yielding **85 / 100**.

### 4.2 85-point milestone definition

The project may be treated as having reached **85 / 100** when all of the following are true:

- deterministic Phase 1 path is implemented and measured against its budgets,
- benchmark packs `BP-01` through `BP-10` are frozen at manifest level,
- scalar reference and parity harness exist for major detector stages,
- DICOM and GSDF conformance evidence exists,
- regulated package synchronization debt is closed for baseline scope,
- at least one Phase 2 deterministic premium path is implemented with verified degrade-open behavior.

---

## 5. Feasibility-Maximizing Technical Choices

| Choice | Why it matters |
|---|---|
| scalar reference for each major stage | makes correctness provable before optimization |
| SIMD parity harness | prevents silent drift between optimized and reference paths |
| sidecar contract for ROI / diagnostics / confidence | reduces ABI churn and module coupling |
| benchmark manifest freeze before premium tuning | prevents non-repeatable wins |
| small-model policy for learned features | keeps CPU inference and fallback realistic |
| quality-state vector | turns hidden heuristics into testable outputs |

### 5.1 No-regret scaffolding before full algorithms

The following scaffolding unlocks the most downstream work with the least rework:

1. benchmark manifest schema and hash-lock tooling,
2. scalar detector-domain kernels for preprocess stages,
3. timing and parity harness for scalar vs SIMD paths,
4. sidecar structures for ROI, quality state, and diagnostics,
5. calibration manifest/session validation logic.

This is the shortest route to a repository that can absorb advanced algorithms without collapsing into undocumented behavior.

---

## 6. Critical Risks

| Risk | Impact | Mitigation |
|---|---|---|
| docs overstate implementation progress | false readiness signals | keep UAT and analysis documents evidence-based only |
| benchmark data not frozen | impossible to compare algorithm revisions fairly | lock manifest schema and dataset hashes first |
| EI and detector metrics accidentally computed on post-presentation images | clinically misleading measurements | enforce domain separation in pipeline and tests |
| AI features invade release claim boundary too early | regulatory expansion and unstable scope | keep boundary matrix explicit and reviewed |
| optimized kernels diverge from reference behavior | invisible quality regressions | require scalar-reference plus parity harness |
| metadata field overloading spreads hidden coupling | unstable integration and ABI confusion | keep ROI and diagnostics in sidecar contracts |
