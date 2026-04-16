# XPE Production Integration Guide

**Document ID**: XPE-PROD-INT-001  
**Version**: 1.2.0  
**Date**: 2026-04-16  
**Status**: Controlled Draft  
**Purpose**: Define the production-host integration contract for XPE, with emphasis on explicit path ownership, deployment layout, initialization order, and degraded-mode handling.  
**Audience**: medical-device software integrators using XPE from a host application such as `RadiConsole`

---

## 1. Integration Principle

XPE uses an explicit-path integration model.

This means:

- XPE DLLs do not assume a fixed installation root,
- XPE DLLs do not scan arbitrary directories on their own,
- the host application owns all runtime paths,
- every calibration file, LUT, model, and output destination is passed explicitly.

Reference examples:

- `xpe_dicom_read("C:\\clinical\\data\\patient_001.dcm", ...)`
- `xpe_calib_load_offset("C:\\xpe\\data\\calibration\\offset\\offset_map_20260415.xpe_calib", ...)`
- `gsvg_load_scatter_lut("C:\\xpe\\data\\calibration\\gsvg\\grid_lut.dat", ...)`

---

## 2. Recommended Deployment Layout

```text
RadiConsole/
|-- RadiConsole.exe
|-- xpe_common.dll
|-- xpe_preprocess.dll
|-- xpe_enhance_basic.dll
|-- xpe_enhance_advanced.dll
|-- xpe_display.dll
|-- xpe_dicom.dll
|-- xpe_ai.dll
|-- xpe_ai_worker.exe
|-- gsvg.dll
|-- data/
|   |-- calibration/
|   |   |-- offset/
|   |   |-- gain/
|   |   |-- defect/
|   |   `-- gsvg/
|   |-- models/
|   |-- lut/
|   `-- config/
|-- user_data/
|   |-- processed_images/
|   `-- calibration_records/
`-- logs/
```

### 2.1 Ownership model

| Path area | Owner | Typical mutability |
|---|---|---|
| runtime DLLs | installer / IT | low |
| `data/calibration/` | field service / IT | controlled |
| `data/models/` | release management | controlled |
| `data/lut/` | release management | low |
| `user_data/` | host application | high |
| `logs/` | host application | high |

---

## 3. Host Path Manager Responsibilities

The host should centralize path decisions in one component.

Minimum responsibilities:

- resolve installation root,
- resolve shared data root,
- resolve user-data root,
- locate latest valid calibration files,
- locate AI model manifests,
- locate LUT files,
- persist operator convenience paths,
- expose only absolute paths to XPE.

### 3.1 Recommended host object

```csharp
public sealed class XpePathManager
{
    public string InstallRoot { get; }
    public string DataRoot { get; }
    public string CalibrationRoot => Path.Combine(DataRoot, "calibration");
    public string OffsetDir => Path.Combine(CalibrationRoot, "offset");
    public string GainDir => Path.Combine(CalibrationRoot, "gain");
    public string DefectDir => Path.Combine(CalibrationRoot, "defect");
    public string GsvgDir => Path.Combine(CalibrationRoot, "gsvg");
    public string ModelDir => Path.Combine(DataRoot, "models");
    public string LutDir => Path.Combine(DataRoot, "lut");
    public string UserDataDir { get; }
    public string ProcessedImageDir => Path.Combine(UserDataDir, "processed_images");
    public string LogDir => Path.Combine(InstallRoot, "logs");
}
```

The host may discover its install root from configuration first and fall back to the executable directory if configuration is absent.

---

## 4. Initialization Sequence

Recommended startup order:

1. create or resolve the path manager,
2. verify required directories exist,
3. select the latest valid calibration files,
4. verify model and LUT availability,
5. call `xpe_init()`,
6. call calibration load functions with explicit paths,
7. optionally initialize AI and GSVG,
8. start acquisition or image import workflows.

### 4.1 Mandatory startup checks

- `xpe_common.dll` loadable
- `xpe_preprocess.dll` loadable
- `xpe_enhance_basic.dll` loadable
- `xpe_display.dll` loadable
- `xpe_dicom.dll` loadable
- required calibration files present and valid

### 4.2 Optional startup checks

- `xpe_enhance_advanced.dll`
- `gsvg.dll`
- `xpe_ai.dll`
- `xpe_ai_worker.exe`
- model manifest and model files

Missing optional components must not block the deterministic baseline.

### 4.3 Test GUI native integration readiness

The Test GUI shall not bind execution controls to a native module only because a DLL exists. Native GUI integration must satisfy the staged readiness gates in:

- `XPE-GUI-NATIVE-INT-READINESS-001_GUI_Native_Module_Integration_Readiness.md`

Minimum rule:

- `R1` allows version/health display only.
- `R2` allows dry-run ABI checks.
- `R3` allows synthetic native execution.
- `R4` allows local real fixture execution.
- `R5` allows user-facing GUI validation.

Preprocessing is the first preferred image-transforming native target, but it must pass `GUI-PRE-GATE-001` through `GUI-PRE-GATE-010` before real fixture validation is enabled in the Test GUI.

---

## 5. Calibration Discovery Policy

The host decides how to choose the active calibration file.

Recommended selection order:

1. matching detector serial,
2. matching calibration type,
3. latest valid date,
4. valid CRC,
5. not expired.

### 5.1 Suggested helper policy

- scan only the target directory for the requested calibration type,
- reject files that fail header or CRC validation,
- reject expired files,
- prefer the newest valid `session_id`,
- log the exact file selected.

---

## 6. Runtime Processing Sequence

Typical production-host flow:

1. import raw or DICOM image,
2. map acquisition metadata to `XpeImageMetadata`,
3. allocate working buffers with `xpe_alloc_image()`,
4. run pre-processing,
5. run Phase 1b deterministic path,
6. optionally run Phase 2 deterministic premium path,
7. optionally run Phase 3 assistive path,
8. export display image and DICOM output,
9. persist outputs under `user_data/processed_images/`,
10. collect alerts and logs.

The host remains responsible for output destinations and file naming.

---

## 7. Settings Persistence

The host may store convenience paths in `appsettings.json`.

Recommended keys:

- `lastImageDir`
- `lastDicomDir`
- `calibOffsetDir`
- `calibGainDir`
- `calibDefectDir`

Rules:

- persist only directories, not raw credentials or secrets,
- reject empty persisted values,
- validate existence at startup,
- fall back to controlled defaults when missing.

---

## 8. AI Worker Integration

Phase 3 integration uses a worker process. The detailed IPC contract is defined in `xpe-implementation-reference.md` Section 12.

Production rules:

- the host loads `xpe_ai.dll`,
- `xpe_ai.dll` manages `xpe_ai_worker.exe`,
- host code does not talk directly to the worker pipe,
- worker failure disables assistive features only,
- deterministic image delivery continues.

### 8.1 Deployment checks

- `xpe_ai_worker.exe` present beside runtime binaries or in the configured worker path,
- model manifest present,
- model files present,
- host can write worker diagnostics to logs.

---

## 9. Failure Handling

| Failure | Host action | User-visible outcome |
|---|---|---|
| missing calibration | stop deterministic processing and raise actionable error | no unsafe image generation |
| expired calibration | reject load and request service action | controlled stop |
| missing GSVG LUT | bypass GSVG and continue | deterministic image preserved |
| missing AI model | disable Phase 3 and continue | deterministic image preserved |
| AI worker timeout | restart worker or disable Phase 3 | deterministic image preserved |
| DICOM export path invalid | report export failure without corrupting in-memory result | processing result retained |

---

## 10. Deployment Checklist

- [ ] all mandatory DLLs are deployed
- [ ] optional DLLs are labelled optional in installer logic
- [ ] calibration directories exist
- [ ] latest calibration files pass validation
- [ ] model manifest exists if Phase 3 is enabled
- [ ] LUT files exist if GSVG is enabled
- [ ] `user_data/` is writable
- [ ] `logs/` is writable
- [ ] host path manager returns absolute paths only
- [ ] degraded-mode alerts are surfaced to the host UI or log sink

---

## 11. Cross-Reference

Use the companion documents for detail:

- `api-spec.md` for exported ABI
- `pipeline-spec.md` for canonical stage order
- `XPE-GUI-NATIVE-INT-READINESS-001_GUI_Native_Module_Integration_Readiness.md` for Test GUI native module readiness gates
- `Preprocessing-E2E-Automated-Evaluation-Protocol.md` for preprocessing fixture metrics and gates
- `xpe-implementation-reference.md` for file formats, logging, alerts, and AI worker IPC
- `sprint-plan.md` for delivery sequencing

---

*Document End -- XPE-PROD-INT-001 v1.2.0*
