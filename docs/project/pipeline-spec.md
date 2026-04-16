# X-Ray Image Processing Pipeline Specification

**Document ID**: PIPE-SPEC-001  
**Version**: 1.6.0  
**Date**: 2026-04-16  
**Status**: Controlled Draft  
**Project**: X-ray Image Processing Engine (XPE)  
**Canonical Scope**: `docs/project/`

**Changelog**:

- `v1.3.0 -> v1.5.0`: fixed canonical phase ownership, added AED acquisition gate, reinstated EI baseline stage in the normative body, removed internal Phase 2/Phase 3 contradictions, and normalized GSVG fallback semantics
- `v1.5.0 -> v1.6.0`: EI baseline 단계를 xpe_enhance_basic.dll로 이동 (SWU-2.10), AED subsystem 통합 완료, xpe_common API 18함수로 확정, 전체 파이프라인 단계 재정렬 반영

---

## 1. Purpose

This document defines the only canonical execution order for XPE.

It answers four questions:

1. which stage runs on which data domain,
2. which binary owns that stage,
3. what happens when an optional stage is unavailable,
4. which timing and memory budget applies at each phase.

Normative algorithm detail is carried by `xpe-algorithm-spec-deepsync.md`. Normative ABI detail is carried by `api-spec.md`.

---

## 2. Invariants

1. Detector-domain corrections complete before enhancement-domain presentation work.
2. `SWU-2.10` is the only EI unit identifier.
3. Phase 3 AI features are assistive and non-blocking.
4. `XPE_FLAG_*` fields contain state only; failure reasons go to alerts or diagnostic JSON.
5. GSVG failure must degrade gracefully.
6. EI/DI are valid only for detector-domain, single-irradiation images.

---

## 3. Data Domains

| Domain | Typical stages | Allowed outputs |
|---|---|---|
| Detector domain | AED, readout validation, temperature, offset, nonlinearity, gain, binning, defect, lag/ghost, EI, collimation baseline | corrected frame, detector metrics, ROI sidecar |
| Enhancement domain | log, noise, contrast, edge, multiscale, fractional, GSVG, AI enhancement | enhanced float image, AI advisory outputs |
| Presentation domain | modality LUT, VOI LUT, presentation LUT, DICOM write | display-ready image, exported DICOM |

The orchestrator must not feed presentation-domain images into EI, detector QC, or lag/ghost validation.

---

## 4. Phase-Gated Binary Loading

| Phase | Required binaries | Optional binaries | Degraded behavior |
|---|---|---|---|
| 0 | `xpe_common.dll`, `ImageProcTest.exe` | none | foundation cannot degrade |
| 1a | `xpe_preprocess.dll` | none | detector correction cannot degrade below mandatory gates |
| 1b | `xpe_enhance_basic.dll`, `xpe_display.dll`, `xpe_dicom.dll` | none | deterministic release baseline |
| 2 | `xpe_enhance_advanced.dll`, `gsvg.dll` | yes | skip Phase 2 features, preserve Phase 1 output |
| 3 | `xpe_ai.dll`, `xpe_ai_worker.exe` | yes | disable AI features, preserve deterministic output |

---

## 5. Canonical Execution Contract

### 5.1 Acquisition and startup stages

| Label | Stage | Owner | Domain | Mandatory | Notes |
|---|---|---|---|:---:|---|
| `BOOT-0` | Calibration manifest load | `xpe_preprocess.dll` | N/A | yes | startup-only, may lazy-load heavy maps and LUTs after validation |
| `AED-0` | Automatic exposure detection and event capture | `xpe_common.dll` | acquisition sidecar | yes | asynchronous, records timing and state before first frame enters stage `(0.5)` |

### 5.2 Per-frame canonical order

| Label | Stage | Owner | Phase | Input domain | Output / side effect | Bypass rule |
|---|---|:---:|:---:|---|---|---|
| `(0.5)` | Readout artifact validation | preprocess | 1a | detector | alert only, no pixel mutation | advisory |
| `(0.7)` | Temperature compensation | preprocess | 1a | detector | corrected detector frame | conditional |
| `(1)` | Offset correction | preprocess | 1a | detector | corrected detector frame | mandatory |
| `(1.5)` | Nonlinearity correction | preprocess | 1a | detector | linearized detector frame | conditional |
| `(2)` | Gain correction | preprocess | 1a | detector | normalized float frame | mandatory |
| `(2.5)` | Binning correction | preprocess | 1a | detector float | detector float | conditional |
| `(3)` | Defect correction | preprocess | 1a | detector float | detector float | conditional |
| `(4)` | Lag / ghost correction | preprocess | 1a | detector float | detector float | conditional with tier downgrade |
| `(EI-0)` | Whole-image EI / DI | enhance_basic | 1b | detector float | EI/DI values | mandatory for single-irradiation release flow |
| `(5)` | Log transform | enhance_basic | 1b | enhancement entry | log-domain image | mandatory |
| `(5b)` | Baseline collimation detection | enhance_advanced | 2 | detector float side copy | ROI sidecar | optional |
| `(EI-1)` | ROI-aware EI refinement | orchestrator + enhance_basic API | 2 | detector float ROI crop | refined EI/DI | optional |
| `(6)` | Noise reduction | enhance_basic | 1b | enhancement | enhanced image | mandatory |
| `(7)` | Contrast enhancement | enhance_basic | 1b | enhancement | enhanced image | mandatory |
| `(8)` | Edge enhancement | enhance_basic | 1b | enhancement | enhanced image | mandatory |
| `(9)` | GSVG / virtual grid | gsvg | 2 | enhancement | enhanced image or skipped state | optional |
| `(10)` | Multiscale processing | enhance_advanced | 2 | enhancement | enhanced image | optional |
| `(11)` | Fractional processing | enhance_advanced | 2 | enhancement | enhanced image | optional |
| `(5a)` | Body-part recognition advisory branch | ai | 3 | preview copy | body-part label and confidence | optional, non-blocking |
| `(5c)` | AI collimation refinement advisory branch | ai | 3 | preview copy | refined ROI proposal | optional, non-blocking |
| `(12)` | Image stitching | ai | 3 | multi-frame set | panorama image | optional, exam-type gated |
| `(13)` | Bone suppression | ai | 3 | enhancement | assistive secondary image | optional |
| `(13b)` | DL denoising | ai | 3 | enhancement | assistive enhanced image | optional |
| `(14)` | Modality LUT | display | 1b | presentation | presentation image | mandatory |
| `(15)` | VOI LUT | display | 1b | presentation | presentation image | mandatory |
| `(16)` | Presentation LUT / GSDF | display | 1b | presentation | display-ready image | mandatory |
| `(17)` | DICOM write / export | dicom | 1b | presentation | exported DICOM object | mandatory |

### 5.3 Parallel branch rules

- `(5a)` and `(5c)` are advisory branches, not blocking linear stages.
- `(5b)` must complete before `(EI-1)` only.
- `(12)` is entered only for multi-frame studies explicitly marked stitchable.
- `(13)` and `(13b)` must never overwrite the deterministic baseline image silently; they produce tagged assistive outputs.

---

## 6. Bypass and Fallback Policy

### 6.1 Mandatory stages

The following stages cannot be bypassed in release mode:

- `(1)` offset correction
- `(2)` gain correction
- `(EI-0)` whole-image EI for single-irradiation general radiography flows
- `(5)` through `(8)` deterministic enhancement baseline
- `(14)` through `(17)` presentation and DICOM path

### 6.2 Conditional stages

Conditional stages may bypass only when the reason is explicit and auditable:

| Stage | Valid bypass reason |
|---|---|
| `(0.7)` | temperature sensor unavailable or inside no-correction deadband |
| `(1.5)` | panel profile explicitly declares effectively linear response |
| `(2.5)` | detector operates in native 1x1 mode |
| `(3)` | BPM empty and runtime defect detection disabled |
| `(4)` | first frame, empty history, or operator-selected single-shot mode |
| `(5b)` | Phase 2 binaries absent |
| `(9)` | `gsvg.dll` absent or grid not applicable |
| `(10)`, `(11)` | Phase 2 binaries absent |
| `(5a)`, `(5c)`, `(12)`, `(13)`, `(13b)` | Phase 3 binaries absent or worker unavailable |

### 6.3 Flag and diagnostic policy

| Condition | Metadata flag | Diagnostic channel |
|---|---|---|
| temperature compensation applied | `XPE_FLAG_TEMP_COMPENSATED` | optional alert |
| defect correction applied | `XPE_FLAG_DEFECT_CORRECTED` | optional alert |
| lag/ghost correction applied | `XPE_FLAG_GHOST_CORRECTED` | optional alert |
| GSVG skipped | `XPE_FLAG_GSVG_SKIPPED` | required alert or diagnostic JSON with error reason |
| stitched image | `XPE_FLAG_STITCHED` | required exam metadata |
| bone suppression applied | `XPE_FLAG_BONE_SUPPRESSED` | assistive output manifest |

`XPE_FLAG_GSVG_SKIPPED` does not embed an error code. The reason is stored outside the bitfield.

---

## 7. Performance and Memory Budgets

| Budget ID | Scope | Target |
|---|---|---:|
| `PERF-BOOT-001` | startup manifest validation | <= 200 ms |
| `PERF-P1A-001` | pre-processing `(0.5)` through `(4)` | <= 500 ms |
| `PERF-P1-001` | full deterministic Phase 1 output | <= 3000 ms |
| `PERF-P2-001` | incremental Phase 2 cost | <= 2500 ms |
| `PERF-P3-001` | incremental Phase 3 cost | <= 3000 ms |
| `MEM-P1-001` | Phase 1 peak memory | <= 190 MB |
| `MEM-P2-001` | Phase 2 peak memory | <= 440 MB |
| `MEM-P3-001` | Phase 3 peak memory | <= 740 MB |

Tier targets for lag / ghost correction:

- Tier 1 LTI: <= 150 ms
- Tier 2 exposure-weighted: <= 190 ms
- Tier 3 NLCSC: <= 240 ms

---

## 8. Clinical and Regulatory Constraints

1. EI/DI shall reject or explicitly flag stitched and other multi-irradiation inputs.
2. AI outputs shall be labelled assistive and must expose confidence and failure conditions.
3. Detector-domain QA metrics shall not be computed on presentation-domain images.
4. GSDF alignment is a presentation obligation; it does not redefine detector-domain image quality.
