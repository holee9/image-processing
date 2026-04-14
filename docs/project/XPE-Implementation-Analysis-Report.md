# XPE Implementation Analysis Report

**Document ID**: XPE-IMPL-ANALYSIS-001  
**Version**: 1.1.0  
**Date**: 2026-04-14  
**Status**: Controlled Draft  
**Source**: `SPEC-XPE-MASTER v2.0.0`, `product.md v1.2.0`, `structure.md v1.2.0`, `pipeline-spec.md v1.5.0`, `api-spec.md v1.3.0`, `xpe-algorithm-spec-deepsync.md v3.1.0-ds3`

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

## 5. Critical Risks

| Risk | Impact | Mitigation |
|---|---|---|
| docs overstate implementation progress | false readiness signals | keep UAT and analysis documents evidence-based only |
| benchmark data not frozen | impossible to compare algorithm revisions fairly | lock manifest schema and dataset hashes first |
| EI and detector metrics accidentally computed on post-presentation images | clinically misleading measurements | enforce domain separation in pipeline and tests |
| AI features invade release claim boundary too early | regulatory expansion and unstable scope | keep boundary matrix explicit and reviewed |
