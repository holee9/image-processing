# Project Structure

**Document ID**: XPE-STRUCTURE-001  
**Version**: 1.3.0  
**Date**: 2026-04-16  
**Status**: Controlled Draft  
**Canonical Scope**: `docs/project/`

---

## 1. Repository Model

This repository contains both:

- **implemented baseline assets**: build, CI, `xpe_common`, smoke tests, governance docs
- **planned target assets**: the remaining XPE and GSVG modules described in this document

The structure document describes the target code and binary layout that all plans and specs must reference.

---

## 2. Current Snapshot vs Target Layout

| Area | Current repository state | Target architectural state |
|---|---|---|
| `modules/common/` | present | foundation module |
| `modules/preprocess/` | not yet present | Phase 1a module |
| `modules/enhance_basic/` | not yet present | Phase 1b module |
| `modules/enhance_advanced/` | not yet present | Phase 2 module |
| `modules/ai/` | not yet present | Phase 3 proxy module |
| `modules/display/` | not yet present | Phase 1b module |
| `modules/dicom/` | not yet present | Phase 1b module |
| `gsvg/` | not yet present in source tree | independent Phase 2 module |
| `gui/` | not yet present in source tree | C# orchestrator and QA application |
| `tests/common_smoke/` | present | initial native smoke test |
| `docs/project/` | present | canonical design and plan set |

---

## 3. Target Repository Layout

```text
image-processing/
|-- CMakeLists.txt
|-- CMakePresets.json
|-- cmake/
|-- modules/
|   |-- common/
|   |-- preprocess/
|   |-- enhance_basic/
|   |-- enhance_advanced/
|   |-- ai/
|   |-- display/
|   `-- dicom/
|-- gsvg/
|-- gui/
|   |-- ImageProcTest/
|   |   `-- fixtures/
|   |-- ImageProcTest.SelfCheck/
|   `-- ImageProcTest.E2E/
|-- tests/
|   |-- common_smoke/
|   |-- unit/
|   |-- integration/
|   `-- benchmark_data/
|-- data/
|   |-- calibration/
|   |-- models/
|   |-- lut/
|   `-- config/
`-- docs/
    |-- project/
    `-- post-processing/
```

---

## 4. Canonical Module-to-Binary Mapping

| Source area | Binary output | Layer | Phase | Unit count | API count |
|---|---|:---:|:---:|---:|---:|
| `modules/common/` | `xpe_common.dll` | 0 | 0 | 7 SWU | 18 |
| `modules/preprocess/` | `xpe_preprocess.dll` | 1 | 1a | 9 SWU | 18 |
| `modules/enhance_basic/` | `xpe_enhance_basic.dll` | 1 | 1b | 5 SWU | 7 |
| `modules/enhance_advanced/` | `xpe_enhance_advanced.dll` | 1 | 2 | 3 SWU | 3 |
| `modules/ai/` | `xpe_ai.dll` | 1 | 3 | 4 SWU | 7 |
| `modules/display/` | `xpe_display.dll` | 1 | 1b | 4 SWU | 11 |
| `modules/dicom/` | `xpe_dicom.dll` | 1 | 1b | 4 SWU | 10 |
| `gsvg/` | `gsvg.dll` | 1-G | 2 | 4 SI | 8 |
| `gui/` | `ImageProcTest.exe` | 2 | 0+ | 2 SWU | N/A |

### 4.1 Canonical totals

- **XPE SWU total**: 38
- **GSVG SI total**: 4
- **Executable unit total**: 42
- **Native exported API total**: 82

---

## 5. Unit Ownership Rules

### 5.1 Exposure Index ownership

`SWU-2.10 ExposureIndexCalc` is one logical unit and one exported function:

- implemented in `xpe_enhance_basic.dll`
- called in Phase 1b for whole-image EI baseline
- re-invoked in Phase 2 with a collimation ROI-cropped image for ROI refinement

### 5.2 AI ownership

Phase 3 ownership is fixed:

- `SWU-2.7 BodyPartRecognizer`
- `SWU-2.9 ImageStitcher`
- `SWU-2.11 BoneSuppressionEngine`
- `SWU-2.12 DLDenoiser`

`xpe_ai_worker.exe` is a deployment component, not an extra counted SWU.

### 5.3 GSVG ownership

GSVG remains independent:

- no `xpe_common` type dependency in its exported ABI
- no lateral dependency on other XPE DLLs
- integration occurs only through orchestration and agreed image buffer contracts

---

## 6. Dependency Rules

| Rule ID | Rule |
|---|---|
| `DEP-001` | Layer 1 XPE DLLs may depend only on `xpe_common.dll` |
| `DEP-002` | `gsvg.dll` must remain independently buildable and independently testable |
| `DEP-003` | `ImageProcTest.exe` is the only component that may orchestrate multiple DLLs in-process |
| `DEP-004` | `xpe_ai.dll` may launch `xpe_ai_worker.exe` over IPC but may not create direct lateral DLL dependencies |
| `DEP-005` | Runtime data ownership is caller-controlled through the explicit ABI in `api-spec.md` |

---

## 7. Implementation Priority

| Order | Workstream | Why |
|---:|---|---|
| 1 | `modules/common/` | required by every native module |
| 2 | `modules/preprocess/` | required for detector-corrected data and EI |
| 3 | `modules/enhance_basic/`, `modules/display/`, `modules/dicom/` | completes release baseline |
| 4 | `modules/enhance_advanced/` and `gsvg/` | deterministic premium layer |
| 5 | `modules/ai/` and `gui/` advanced features | optional assistive layer |
