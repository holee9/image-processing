# Product Overview: X-ray Image Processing Engine (XPE)

**Document ID**: XPE-PRODUCT-001  
**Version**: 1.3.0  
**Date**: 2026-04-16  
**Status**: Controlled Draft  
**Canonical Scope**: `docs/project/`

---

## 1. Purpose

XPE is a modular X-ray flat panel detector image-processing engine. It converts detector-domain raw frames into diagnostic-ready DICOM images while preserving a regulated boundary between detector correction, enhancement, presentation, and optional AI-assisted functions.

The delivery plan is staged:

- **Phase 0**: foundation, common ABI, orchestration, QA scaffolding
- **Phase 1a**: deterministic detector correction
- **Phase 1b**: deterministic enhancement, display, DICOM, whole-image EI
- **Phase 2**: deterministic premium processing and GSVG
- **Phase 3**: assistive AI features with worker isolation

---

## 2. Product Boundary

### 2.1 Mandatory release baseline

The baseline product that must work without any optional modules includes:

- `xpe_common.dll`
- `xpe_preprocess.dll`
- `xpe_enhance_basic.dll`
- `xpe_display.dll`
- `xpe_dicom.dll`
- `ImageProcTest.exe`

This baseline shall deliver:

- detector-domain correction,
- whole-image Exposure Index / Deviation Index,
- presentation LUT / GSDF-aligned display path,
- diagnostic DICOM export,
- graceful diagnostics and alerting,
- a version-matched offline Help entry point for the host application.

### 2.2 Optional deterministic premium scope

Phase 2 remains optional at deployment time but deterministic in behavior:

- `xpe_enhance_advanced.dll`
- `gsvg.dll`

Phase 2 adds:

- baseline collimation detection,
- ROI-aware EI refinement by re-invoking `xpe_calc_exposure_index`,
- multiscale processing,
- fractional processing,
- grid suppression / virtual grid.

### 2.3 Optional AI assistive scope

Phase 3 is assistive only and must never block baseline image delivery:

- `xpe_ai.dll`
- `xpe_ai_worker.exe`

Phase 3 adds:

- body-part recognition,
- AI collimation refinement,
- image stitching,
- bone suppression,
- DL denoising.

If AI fails, the product shall fall back to deterministic Phase 1/2 output.

---

## 3. Canonical Unit Count

### 3.1 Counting rule

This project uses two unit types:

- **SWU**: XPE software units governed inside the XPE architecture
- **SI**: GSVG software items governed by the independent GSVG package

**Canonical total**: **42 executable units**

- **38 XPE SWU**
- **4 GSVG SI**

The previous `43` total is retired by this revision.

### 3.2 Unit summary

| Category | Count | Notes |
|---|---:|---|
| Common infrastructure | 7 SWU | `xpe_common.dll` |
| Pre-processing | 9 SWU | `xpe_preprocess.dll` |
| Core processing | 12 SWU | `xpe_enhance_basic.dll`, `xpe_enhance_advanced.dll`, `xpe_ai.dll` |
| Display | 4 SWU | `xpe_display.dll` |
| DICOM I/O | 4 SWU | `xpe_dicom.dll` |
| C# orchestration and QA | 2 SWU | `ImageProcTest.exe` |
| GSVG | 4 SI | `gsvg.dll` |
| **Total** | **42 units** | **38 SWU + 4 SI** |

---

## 4. Binary Deliverables

| Binary | Type | Phase | Responsibility |
|---|---|:---:|---|
| `xpe_common.dll` | Native DLL | 0 | ABI types, lifecycle, alerts, logging, AED |
| `xpe_preprocess.dll` | Native DLL | 1a | detector correction and calibration application |
| `xpe_enhance_basic.dll` | Native DLL | 1b | log, noise, contrast, edge, whole-image EI |
| `xpe_display.dll` | Native DLL | 1b | modality/VOI/presentation LUT |
| `xpe_dicom.dll` | Native DLL | 1b | DICOM IO and network SCU |
| `xpe_enhance_advanced.dll` | Native DLL | 2 | collimation baseline, multiscale, fractional |
| `gsvg.dll` | Native DLL | 2 | grid suppression and virtual grid |
| `xpe_ai.dll` | Native DLL | 3 | in-process assistive proxy |
| `xpe_ai_worker.exe` | Native EXE | 3 | sandboxed inference worker |
| `ImageProcTest.exe` | C# WPF EXE | 0+ | orchestration, QA, integration harness |

---

## 5. Key Feature Map

| Research / Product ID | Canonical ownership | Delivery rule |
|---|---|---|
| `PRE-01` Readout validation | `SWU-1.9` | advisory only, no pixel mutation |
| `PRE-02/03/06/07/08/09` detector correction | `SWU-1.1~1.8` | deterministic release baseline |
| `PRE-04/05` lag and ghost correction | `SWU-1.4` | deterministic release baseline with tier downgrade |
| `SUP-01` calibration management | `SWU-1.5`, `SWU-5.6` | release baseline |
| `SUP-02` AED | `SWU-5.8` | release baseline infrastructure |
| `SUP-03` Exposure Index | `SWU-2.10` | one unit reused across Phase 1b baseline and Phase 2 ROI refinement |
| `SUP-04` DICOM conformance | `SWU-4.1~4.4` | release baseline |
| `SUP-05` QA / constancy | `SWU-6.1` | release baseline validation surface |
| `POST-05/07/10/11` advanced deterministic features | `SWU-2.5`, `SWU-2.8`, `SI-001~004` | optional Phase 2 |
| `POST-06/08/09` AI features | `SWU-2.7`, `SWU-2.9`, `SWU-2.11`, `SWU-2.12` | optional Phase 3, assistive only |

---

## 6. Product Rules That Shall Not Change

1. `SWU-2.10` is the only canonical EI unit identifier.
2. EI/DI apply only to detector-domain, single-irradiation images.
3. Body-part recognition and stitching are Phase 3 features, not Phase 2.
4. `XPE_FLAG_*` values are state bits only. Error details go to alerts or diagnostic JSON.
5. GSVG may fail open. When skipped, the pipeline records `XPE_FLAG_GSVG_SKIPPED` and a diagnostic reason, but continues with non-GSVG output.
6. AI modules are assistive. Their failure must not block the deterministic path.
7. `docs/project/` is the only canonical documentation tree for this architecture.
8. Operator help and API reference shall be version-matched to the shipped build and accessible from the host application.

---

## 7. Product Success Criteria

| Area | Minimum release criterion |
|---|---|
| Phase 1a latency | pre-processing completes within 500 ms for 3072x3072 |
| Phase 1 total latency | deterministic baseline completes within 3000 ms |
| Memory discipline | no unbounded frame-to-frame growth in steady-state loops |
| Traceability | every planned SWU/SI maps to owner binary, API contract, and validation evidence |
| Degraded operation | missing Phase 2/3 binaries degrade gracefully with explicit diagnostics |
| Regulatory boundary | release claims stay inside deterministic enhancement and assistive AI boundaries documented in `Regulatory-Feature-Boundary-Matrix.md` |
| Help and onboarding | offline help opens from the host UI and matches the shipped build version |
