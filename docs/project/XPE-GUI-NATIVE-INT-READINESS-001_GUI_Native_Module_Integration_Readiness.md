# GUI Native Module Integration Readiness

**Document ID**: XPE-GUI-NATIVE-INT-READINESS-001  
**Version**: 1.1.0
**Date**: 2026-04-17
**Status**: Controlled Draft  
**Scope**: Test GUI native DLL integration readiness, with preprocessing-first priority  
**Owner**: XPE Integration / GUI Test Console

---

## 1. Purpose

This document defines when a native XPE module is ready to be connected to the Test GUI application.

The Test GUI is not a single-feature calibration viewer. It is an integration test console for the full XPE image-processing pipeline. Its native-module connection policy must therefore prevent premature DLL binding, ambiguous fallback behavior, and visually misleading validation.

The current strategic priority is:

1. keep `xpe_common.dll` as the native baseline dependency;
2. prepare `xpe_preprocess.dll` as the first image-transforming native integration target;
3. defer `xpe_display.dll`, `xpe_enhance_basic.dll`, DICOM, GSVG, and AI until their exported APIs and smoke tests are ready.

### 1.1 Test GUI evolution policy

The Test GUI is allowed to start as a diagnostic-first console during early integration, but that is not the release-level user experience.

The GUI shall evolve through these UI modes:

| Mode | Project stage | Primary screen | Diagnostics placement |
|---|---|---|---|
| `D0` Diagnostic console | early native ABI integration | health, readiness, DLL path, ABI checks | top-level and prominent |
| `D1` Hybrid validation console | module-by-module bring-up | raw fixture browser, before/after comparison, stage modes, E2E reports | visible panel below workflow |
| `D2` Workflow-first test app | Phase 1 baseline integration | module function testing and automated E2E workflow | secondary Diagnostics tab/panel |
| `D3` Release validation console | release-candidate validation | workflow, metrics, reports, help/manual | diagnostics retained but not workflow-dominant |

By release-candidate level, the default operator flow shall not be a health dashboard. The default flow shall be image and module validation: load fixture, select calibration, configure Off/On/Auto stages, execute available modules, compare before/after, inspect metrics, and export an E2E report.

Health and readiness shall not disappear. They shall move to a dedicated `Diagnostics` area because DLL discovery, P/Invoke ABI mismatch, version skew, missing exports, architecture mismatch, and calibration context errors remain valid failure modes after implementation is complete.

---

## 2. Current Integration Assessment Snapshot

Snapshot date: 2026-04-16.

| Module | Observed status | GUI integration decision |
|---|---|---|
| `xpe_common.dll` | Runtime DLL exists; exported lifecycle, memory, alert, log, and Auto Exposure Detection APIs were observed. | Ready for baseline native health/status integration. |
| `xpe_preprocess.dll` | Source or runtime DLL not present in the current inspected workspace snapshot. | Not ready. Prepare gates and adapter contract only. |
| `xpe_display.dll` | Runtime DLL exists but only `xpe_display_version` was observed. Display pipeline entry points were not available. | Not ready for real display pipeline. Keep mock/fallback path. |
| `xpe_enhance_basic.dll` | No ready source/runtime export evidence in the inspected snapshot. | Not ready. |
| `xpe_dicom.dll` | No ready source/runtime export evidence in the inspected snapshot. | Not ready. |
| `gsvg.dll` | No ready source/runtime export evidence in the inspected snapshot. | Not ready. |
| `xpe_ai.dll` / worker | No ready source/runtime export evidence in the inspected snapshot. | Not ready. |

This snapshot is not a permanent project judgment. It is a gate result for whether the Test GUI should bind to a module now.

---

## 3. Readiness Levels

| Level | Name | Meaning | GUI behavior |
|---|---|---|---|
| `R0` | Absent | DLL/source is not present or cannot be discovered. | Show `Unavailable`; use mock only if enabled. |
| `R1` | Binary discoverable | DLL exists and version function can be called. | Show version and module health only. No image processing. |
| `R2` | ABI discoverable | Required exported functions match `api-spec.md` and struct packing checks pass. | Enable native dry-run command. |
| `R3` | Synthetic smoke pass | Small synthetic oracle test passes without raw mutation, NaN/Inf, or crash. | Enable native module execution on synthetic/demo data. |
| `R4` | Fixture E2E pass | Local fixture E2E report passes module-specific gates. | Enable native module execution on local real fixture data. |
| `R5` | GUI validation pass | Automated GUI E2E and user-facing controls pass with native backend. | Enable operator-visible validation workflow. |

Only `R4` or higher can be considered ready for meaningful user review on real fixture data.

---

## 4. Global Native Module Entry Criteria

A native module shall not be connected to Test GUI execution controls unless all global criteria are met.

| Gate ID | Criterion | Required evidence |
|---|---|---|
| `GUI-NATIVE-GATE-001` | DLL or source exists in the current branch/worktree. | file path and Git status recorded |
| `GUI-NATIVE-GATE-002` | version function exists and returns non-empty string. | `dumpbin /exports` plus P/Invoke smoke result |
| `GUI-NATIVE-GATE-003` | required C ABI exports match `docs/project/api-spec.md`. | export checklist |
| `GUI-NATIVE-GATE-004` | C# struct layout matches C/C++ ABI. | `Marshal.SizeOf` and field-offset check |
| `GUI-NATIVE-GATE-005` | native call failure is non-fatal. | missing DLL, missing entry point, bad input, and processing failure tests |
| `GUI-NATIVE-GATE-006` | fallback mode is explicit. | GUI state shows `Native`, `Mock`, `Fallback`, or `Unavailable` |
| `GUI-NATIVE-GATE-007` | no silent image mutation. | raw SHA-256 before/after if module receives raw or detector-domain input |
| `GUI-NATIVE-GATE-008` | metric/report artifact is emitted. | JSON report path and schema version |
| `GUI-NATIVE-GATE-009` | module issue has `codex:` progress comments. | GitHub issue link |

---

## 5. Preprocessing-First Readiness Gates

`xpe_preprocess.dll` is the preferred first image-transforming module for Test GUI integration because it directly connects local raw/calibration fixtures, automatic E2E metrics, and user-visible before/after comparison.

The module is not ready until these gates pass.

| Gate ID | Criterion | Required evidence |
|---|---|---|
| `GUI-PRE-GATE-001` | Runtime artifact exists: `xpe_preprocess.dll`. | file path, build SHA, timestamp |
| `GUI-PRE-GATE-002` | Minimum ABI exists. | `dumpbin /exports` confirms required functions |
| `GUI-PRE-GATE-003` | Required function set is stable or adapter-mapped. | exported names mapped to API spec |
| `GUI-PRE-GATE-004` | calibration paths are explicit. | offset/gain/defect directories passed by GUI settings, no arbitrary scanning |
| `GUI-PRE-GATE-005` | synthetic oracle passes. | `PRE-E2E-1` report |
| `GUI-PRE-GATE-006` | local raw/calibration fixture passes. | `PRE-E2E-2` report under `tests/test_data/calibration_cases` |
| `GUI-PRE-GATE-007` | mismatch negative test passes. | `PRE-E2E-5` report |
| `GUI-PRE-GATE-008` | GUI comparison can show original vs processed. | swipe/zoom/pan view displays both buffers without data loss |
| `GUI-PRE-GATE-009` | processing mode controls are honored. | Off/On/Auto state recorded per stage |
| `GUI-PRE-GATE-010` | failure handling is visible. | status, log, and alert panels show actionable failure reason |

### 5.1 Minimum preprocessing export set

The preferred API is a single pipeline function plus calibration load functions:

```c
const char* xpe_preprocess_version(void);
XpeErrorCode xpe_preprocess_init(const char* jsonConfigOrNull);
void xpe_preprocess_shutdown(void);
XpeErrorCode xpe_calib_load_offset(const char* path);
XpeErrorCode xpe_calib_load_gain(const char* path);
XpeErrorCode xpe_calib_load_defect_map(const char* path);
XpeErrorCode xpe_preprocess_apply_pipeline(
    const XpeImageBuffer* input,
    XpeImageBuffer* output,
    const XpeImageMetadata* metadata,
    const char* jsonOptionsOrNull,
    char* reportJson,
    size_t reportJsonLen);
```

If the implementation exposes separate stage functions instead of a single pipeline entry point, the GUI adapter may map them, but the adapter must still emit the same report schema defined by `XPE-PRE-E2E-001`.

### 5.2 Minimum preprocessing report fields

The native preprocessing GUI adapter shall emit or synthesize these fields:

- `schema = xpe-pre-e2e-report-v1`
- `backend = native-preprocess`
- `preprocess_version`
- `raw_sha256_before`
- `raw_sha256_after`
- `input_preserved`
- `enabled_stages`
- `applied_stages`
- `calibration_paths`
- `gain_semantics`
- `metrics`
- `gates`
- `degraded_evidence`
- `error`

---

## 6. Display Module Readiness Gates

`xpe_display.dll` shall remain in mock/fallback mode until it has real display pipeline exports.

Minimum required exports:

```c
const char* xpe_display_version(void);
XpeErrorCode xpe_apply_modality_lut(...);
XpeErrorCode xpe_apply_voi_lut(...);
XpeErrorCode xpe_apply_presentation_lut(...);
XpeErrorCode xpe_display_apply_pipeline(...);
```

If only `xpe_display_version` is available, the GUI may show display version and health, but must not label any output as native display processing.

---

## 7. GUI Adapter Policy

The Test GUI shall use a layered backend model:

| Layer | Responsibility |
|---|---|
| `MockXpeBackend` | deterministic UI development and fallback behavior |
| `RealXpeCommonBackend` | lifecycle, alert, log, memory, and version health checks |
| `RealXpePreprocessBackend` | raw/calibration preprocessing integration when gates pass |
| `RealXpeDisplayBackend` | display pipeline integration when gates pass |
| `CompositeXpeBackend` | selects native/mock per module and reports mixed-mode status |

The GUI must not treat the backend as all-or-nothing. Mixed mode is expected during development.

Examples:

- common native + preprocess mock + display mock;
- common native + preprocess native + display mock;
- common native + preprocess native + display native.

Every visible output shall show which modules were native and which were mock/fallback.

---

## 8. Release UI Information Architecture

The release-level Test GUI shall use this information architecture:

| Area | Purpose | Required content |
|---|---|---|
| `Workflow` | normal module validation flow | raw/calibration selection, stage Off/On/Auto controls, pipeline/run controls, before/after swipe, zoom, pan, pixel inspection |
| `Metrics` | quality and performance evidence | CES, dark/gain/defect/lag metrics, PSNR/SSIM where applicable, latency, memory, pass/fail gates |
| `Reports` | evidence generation and review | JSON/Markdown E2E report generation, previous report list, fixture SHA-256, module mode summary |
| `Diagnostics` | failure triage | module readiness matrix, DLL paths, versions, export status, ABI sizes, last native error, fallback reason |
| `Help` | embedded user and developer guidance | workflow steps, stage definitions, Off/On/Auto meaning, fixture rules, report interpretation |

The `Diagnostics` area shall remain available in all builds, including release validation builds, but it shall not dominate the first screen once `Workflow` reaches `D2`.

### 8.1 Progressive demotion rule for health panels

The following demotion rule is mandatory:

| Readiness state | GUI behavior |
|---|---|
| No native modules ready | health/readiness may be top-level because it is the primary blocker |
| `xpe_common` ready, image modules not ready | module readiness matrix remains top-level but below raw fixture workflow |
| first image module reaches `R3` | raw workflow and before/after comparison become the default visual focus |
| Phase 1 baseline reaches `R4` | health/readiness moves to `Diagnostics`; workflow, metrics, and reports become primary |
| release candidate | diagnostics remains accessible for support, but all acceptance demos start from `Workflow` |

### 8.2 Non-negotiable release UI rules

- The Test GUI shall never remove diagnostics completely.
- The Test GUI shall never label mock, fallback, identity, or version-only output as native image processing.
- The Test GUI shall always display native/mock/fallback/unavailable status in report artifacts.
- The Test GUI shall keep help/manual content accessible from the application itself.
- The Test GUI shall preserve raw input hashes and report whether any native module mutated input buffers.

---

## 9. Recommended Next Work Items

| Work ID | Task | Owner | Blocks |
|---|---|---|---|
| `GUI-NATIVE-BL-001` | Add module readiness probe script or tool that runs `dumpbin /exports`, version call, and struct layout checks. | Codex / Integration | native GUI binding |
| `GUI-NATIVE-BL-002` | Add `RealXpeCommonBackend` health-only adapter if GUI source is available. | GUI agent | module health panel |
| `GUI-NATIVE-BL-003` | Wait for or request `xpe_preprocess.dll` artifact and exported API list. | preprocessing agent | preprocessing native binding |
| `GUI-NATIVE-BL-004` | Build `RealXpePreprocessBackend` only after `GUI-PRE-GATE-001~005` pass. | GUI agent | real fixture validation |
| `GUI-NATIVE-BL-005` | Add GUI E2E assertion for backend mode visibility: native/mock/fallback/unavailable. | QA / E2E | user-visible trust |
| `GUI-NATIVE-BL-006` | Add issue-comment formatter that posts readiness results with `codex:` prefix. | Codex / Integration | project traceability |
| `GUI-NATIVE-BL-007` | Demote health/readiness panels into a Diagnostics area once the first image-processing module reaches `R3`. | GUI agent | release-level UX |
| `GUI-NATIVE-BL-008` | Add Workflow, Metrics, Reports, Diagnostics, and Help areas before Phase 1 release-candidate validation. | GUI agent | release-level UX |

---

## 10. Decision

Preprocessing remains the first target for image-transforming native GUI integration, but implementation shall wait until `xpe_preprocess.dll` or equivalent source/API is available and the readiness gates pass.

Until then, the correct action is:

- keep Test GUI mock/fallback behavior active;
- surface native module readiness status;
- avoid binding to missing or partial exports;
- prepare issue-backed checklist and E2E report contracts.

---

*Document End - XPE-GUI-NATIVE-INT-READINESS-001 v1.1.0*
