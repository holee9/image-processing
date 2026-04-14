# SPEC-XPE-MASTER: X-ray Image Processing Engine Master Specification

**Document ID**: SPEC-XPE-MASTER  
**Version**: 2.1.0  
**Date**: 2026-04-15  
**Status**: Controlled Draft  
**Canonical Scope**: `docs/project/`

**Source Set**: `product.md v1.2.0`, `structure.md v1.2.0`, `pipeline-spec.md v1.5.0`, `api-spec.md v1.3.0`, `xpe-algorithm-spec-deepsync.md v3.2.0-ds4`, `XPE-ALG-001 v1.1`, `XPE-Module-Reinforcement-Plan.md`, `XPE-Brainstorming-DeepSync-Execution.md`, `Algorithm-Benchmark-Pack-Spec.md`, `Algorithm-Evaluation-Protocol.md`, `Regulatory-Feature-Boundary-Matrix.md`

---

## 1. Purpose

This document is the master tie-breaker for the XPE project document set.

It does not replace the detailed specs. Instead, it fixes the canonical answers for:

- product boundary,
- executable-unit counts,
- binary ownership,
- pipeline order,
- exported API totals,
- algorithm promotion rules,
- remaining synchronization debt.

If two project documents disagree, this document wins until both lower-level documents are updated.

---

## 2. Canonical Decisions

| Topic | Canonical decision |
|---|---|
| Canonical document root | `docs/project/` only |
| XPE software units | 38 SWU |
| GSVG software items | 4 SI |
| Executable-unit total | 42 |
| Native exported APIs | 82 |
| Phase 0 | common ABI, orchestration, QA scaffolding |
| Phase 1a | detector-domain deterministic correction |
| Phase 1b | deterministic enhancement, display, DICOM, whole-image EI |
| Phase 2 | deterministic premium processing and GSVG |
| Phase 3 | assistive AI with worker isolation |
| EI unit identifier | `SWU-2.10` only |
| EI baseline | Phase 1b, whole-image, detector-domain |
| EI refinement | Phase 2, same API re-invoked on ROI-cropped detector image |
| Body-part recognition | Phase 3 only |
| Image stitching | Phase 3 only |
| Flags | state-only bitfield |
| GSVG failure reason | alert or diagnostic JSON, never embedded in flags |
| AI dependency model | `xpe_ai.dll` proxy plus `xpe_ai_worker.exe` sandboxed worker |

---

## 3. Document Authority Stack

Use the following order when reconciling project documents:

1. `SPEC-XPE-MASTER.md`
2. `product.md`
3. `structure.md`
4. `pipeline-spec.md`
5. `api-spec.md`
6. `xpe-algorithm-spec-deepsync.md` — algorithm contract (high-level, normative)
7. `docs/post-processing/xpe/XPE-ALG-001_Unified_Algorithm_Development_Specification.md` — algorithm implementation detail (IEC 62304 §5.4, subordinate to item 6)
8. `sprint-plan.md`
9. analysis, reinforcement, benchmark, and review logs

Notes on XPE-ALG-001 authority:
- XPE-ALG-001 is the detailed design artifact for all XPE algorithms.
- It is subordinate to `xpe-algorithm-spec-deepsync.md` for contractual decisions.
- Where XPE-ALG-001 provides more specific implementation detail not contradicted by items 1–6, it governs the C++ and Python implementation.
- XPE-ALG-001 does NOT override normative decisions in `SPEC-XPE-MASTER.md` or `product.md`.

Raw JSON research transcripts and `.moai/` working copies are not normative.

---

## 4. Product Boundary

### 4.1 Mandatory release baseline

The minimum releasable product consists of:

- `xpe_common.dll`
- `xpe_preprocess.dll`
- `xpe_enhance_basic.dll`
- `xpe_display.dll`
- `xpe_dicom.dll`
- `ImageProcTest.exe`

This baseline must deliver:

- detector-domain correction,
- whole-image EI and DI,
- display-ready presentation LUT path,
- DICOM export,
- deterministic operation without AI or GSVG.

### 4.2 Optional deterministic premium layer

Phase 2 is optional at deployment time but deterministic in behavior:

- `xpe_enhance_advanced.dll`
- `gsvg.dll`

Phase 2 adds:

- baseline collimation detection,
- ROI-aware EI refinement,
- multiscale processing,
- fractional processing,
- grid suppression and virtual grid.

### 4.3 Optional assistive AI layer

Phase 3 is assistive only:

- `xpe_ai.dll`
- `xpe_ai_worker.exe`

Phase 3 adds:

- body-part recognition,
- AI collimation refinement,
- image stitching,
- bone suppression,
- DL denoising.

If Phase 3 fails, the system shall still deliver the deterministic Phase 1 or Phase 2 image path.

---

## 5. Canonical Architecture

### 5.1 Layer model

```text
Layer 0  xpe_common.dll
Layer 1  xpe_preprocess.dll
         xpe_enhance_basic.dll
         xpe_enhance_advanced.dll
         xpe_display.dll
         xpe_dicom.dll
         xpe_ai.dll
Layer 1G gsvg.dll
Layer 2  ImageProcTest.exe
```

### 5.2 Dependency rules

| Rule ID | Rule |
|---|---|
| `DEP-001` | Layer 1 XPE DLLs may depend only on `xpe_common.dll` |
| `DEP-002` | `gsvg.dll` remains independently buildable and independently testable |
| `DEP-003` | `ImageProcTest.exe` is the only in-process multi-DLL orchestrator |
| `DEP-004` | `xpe_ai.dll` may use `xpe_ai_worker.exe` only through IPC |
| `DEP-005` | caller-owned ABI buffers remain the canonical runtime ownership model |

---

## 6. Binary and Unit Inventory

| Binary | Phase | Unit count | Exported API count | Notes |
|---|:---:|---:|---:|---|
| `xpe_common.dll` | 0 | 7 SWU | 18 | lifecycle, memory, logging, config, alerts, AED |
| `xpe_preprocess.dll` | 1a | 9 SWU | 18 | detector correction and calibration application |
| `xpe_enhance_basic.dll` | 1b | 5 SWU | 7 | log, noise, contrast, edge, whole-image EI |
| `xpe_enhance_advanced.dll` | 2 | 3 SWU | 3 | collimation baseline, multiscale, fractional |
| `xpe_ai.dll` | 3 | 4 SWU | 7 | assistive proxy API |
| `xpe_display.dll` | 1b | 4 SWU | 11 | modality, VOI, presentation LUT |
| `xpe_dicom.dll` | 1b | 4 SWU | 10 | DICOM I/O and network SCU |
| `gsvg.dll` | 2 | 4 SI | 8 | independent GSVG package |
| `ImageProcTest.exe` | 0+ | 2 SWU | N/A | orchestration and QA |

### 6.1 Special ownership rules

- `SWU-2.10 ExposureIndexCalc` is one logical unit and one exported function.
- `SWU-2.10` runs first in Phase 1b on the whole detector-domain image.
- Phase 2 refinement reuses the same API on an ROI-cropped detector-domain image.
- `xpe_ai_worker.exe` is a deployment component, not an extra counted SWU.
- `SWU-5.7` and `SWU-6.1` are the only C# SWU.

---

## 7. Canonical Pipeline

### 7.1 Startup and acquisition gates

| Label | Stage | Owner | Mandatory |
|---|---|---|:---:|
| `BOOT-0` | calibration manifest load | `xpe_preprocess.dll` | yes |
| `AED-0` | acquisition event capture | `xpe_common.dll` | yes |

### 7.2 Per-frame order

| Label | Stage | Owner | Phase | Input domain |
|---|---|---|:---:|---|
| `(0.5)` | readout artifact validation | preprocess | 1a | detector |
| `(0.7)` | temperature compensation | preprocess | 1a | detector |
| `(1)` | offset correction | preprocess | 1a | detector |
| `(1.5)` | nonlinearity correction | preprocess | 1a | detector |
| `(2)` | gain correction | preprocess | 1a | detector |
| `(2.5)` | binning correction | preprocess | 1a | detector float |
| `(3)` | defect correction | preprocess | 1a | detector float |
| `(4)` | lag and ghost correction | preprocess | 1a | detector float |
| `(EI-0)` | whole-image EI and DI | enhance_basic | 1b | detector float |
| `(5)` | log transform | enhance_basic | 1b | enhancement |
| `(5b)` | baseline collimation detection | enhance_advanced | 2 | detector side copy |
| `(EI-1)` | ROI-aware EI refinement | orchestrator + enhance_basic API | 2 | detector ROI crop |
| `(6)` | noise reduction | enhance_basic | 1b | enhancement |
| `(7)` | contrast enhancement | enhance_basic | 1b | enhancement |
| `(8)` | edge enhancement | enhance_basic | 1b | enhancement |
| `(9)` | GSVG / virtual grid | gsvg | 2 | enhancement |
| `(10)` | multiscale processing | enhance_advanced | 2 | enhancement |
| `(11)` | fractional processing | enhance_advanced | 2 | enhancement |
| `(5a)` | body-part recognition advisory branch | ai | 3 | preview copy |
| `(5c)` | AI collimation refinement advisory branch | ai | 3 | preview copy |
| `(12)` | image stitching | ai | 3 | multi-frame set |
| `(13)` | bone suppression | ai | 3 | enhancement |
| `(13b)` | DL denoising | ai | 3 | enhancement |
| `(14)` | modality LUT | display | 1b | presentation |
| `(15)` | VOI LUT | display | 1b | presentation |
| `(16)` | presentation LUT / GSDF | display | 1b | presentation |
| `(17)` | DICOM write / export | dicom | 1b | presentation |

### 7.3 Non-negotiable pipeline rules

- Detector-domain metrics are computed before presentation processing.
- Presentation-domain images must not be fed back into EI, detector QA, or lag validation.
- GSVG is optional; failure degrades to a valid non-GSVG output.
- AI branches are non-blocking and cannot silently overwrite the deterministic baseline.

---

## 8. Release and Promotion Rules

### 8.1 Release-safe now

- dynamic dark and temperature-aware correction
- nonlinearity plus gain correction
- deterministic defect correction
- tiered lag and ghost correction
- whole-image EI and DI
- deterministic enhancement and LUT stack
- deterministic Phase 2 premium features if benchmarked

### 8.2 Research-gated

- AI collimation refinement
- body-part recognition guided presets
- DL denoising
- bone suppression
- advanced defect correction beyond deterministic bounded aids
- learned scatter estimation

### 8.3 Regulatory-hold

- pathology-aware enhancement claims
- dose recommendation or ALARA advice
- repeat or reject workflow claims
- diagnostic triage or diagnosis-oriented confidence

Promotion from research-gated to releasable requires:

1. benchmark-pack coverage,
2. evaluation-protocol metrics,
3. regulatory-boundary approval,
4. deterministic fallback.

---

## 9. Phase Exit Gates

| Gate | Mandatory outcome |
|---|---|
| `G0 -> G1a` | common ABI, build, CI, orchestration scaffolding, benchmark schema |
| `G1a -> G1b` | full detector correction under 500 ms and validated calibration behavior |
| `G1b -> G2` | deterministic end-to-end Phase 1 image path, EI/DI, display, DICOM |
| `G2 -> G3` | deterministic premium processing and GSVG under Phase 2 budgets |
| `G3` | AI isolation, confidence reporting, deterministic fallback, bounded performance |

The sprint plan carries the operational checklist for each gate. This document fixes only the gate intent.

---

## 10. Document Synchronization Status

### 10.1 Closed project-document debt

The following project-document issues are treated as closed by this revision:

- corrupted legacy text removed from the master spec,
- canonical total fixed to 42 executable units,
- `SWU-2.10` EI ownership fixed,
- Phase 3 ownership for body-part recognition and stitching fixed,
- state-only flag policy fixed,
- deterministic versus assistive feature boundary fixed.

### 10.2 Remaining external synchronization debt

The following items remain outside `docs/project/` and still require a follow-on sync pass:

- `docs/post-processing/xpe/XPE-SRS-001`
- `docs/post-processing/xpe/XPE-SDD-001`
- `docs/post-processing/xpe/XPE-RTM-001`
- `docs/post-processing/xpe/XPE-VVP-001`

### 10.3 Archive policy

- `.moai/project/` and `.moai/specs/` are working copies only.
- `XPE-PreProcess-DeepResearch.json` and `XPE-PostProcess-DeepResearch.json` are archival transcripts only.
- normative project decisions must be reflected in Markdown under `docs/project/`.

---

## 11. Implementation Focus

The next implementation focus remains unchanged:

1. complete `modules/common/` parity with the documented ABI,
2. implement `xpe_preprocess.dll`,
3. implement `xpe_enhance_basic.dll`, `xpe_display.dll`, and `xpe_dicom.dll`,
4. freeze benchmark manifests and evaluation harnesses,
5. promote only benchmarked premium features.

This is the shortest path to a high-quality, high-confidence release baseline without collapsing the regulated boundary.

---

*Document End -- SPEC-XPE-MASTER v2.1.0*
