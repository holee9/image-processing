# XPE-GUI-DISP-INT-001: GUI Display Integration Guide

**Document ID**: XPE-GUI-DISP-INT-001
**Version**: 1.1.0
**Date**: 2026-04-16
**Status**: Controlled Draft
**Canonical Scope**: `docs/project/`
**Related SPEC**: SPEC-XPE-P1B-DISP v1.0.0
**Target**: Agent implementing Phase 1b Display module integration into `ImageProcTest.exe`

---

## 1. Purpose

This document captures the cross-validation results between the implemented GUI (`ImageProcTest.exe` GUI-S0) and the implemented C++ display module (`xpe_display.dll` SPEC-XPE-P1B-DISP). It identifies all gaps, missing contracts, and required changes so that a separate implementing agent can integrate `xpe_display.dll` into the GUI without ambiguity.

---

## 2. Current Implementation Status (2026-04-16)

### 2.1 xpe_display.dll — COMPLETED (SPEC-XPE-P1B-DISP)

The C++ module is implemented and committed. Status per progress.md:

| Component | Status | File | Tests |
|-----------|--------|------|-------|
| SWU-3.1 `xpe_apply_modality_lut` | ✅ Done | `modules/display/src/modality_lut.cpp` | 11 tests |
| SWU-3.2 `xpe_apply_voi_lut` | ✅ Done | `modules/display/src/voi_lut.cpp` | 15 tests |
| SWU-3.2 `xpe_voi_preset_create` | ✅ Done | `modules/display/src/voi_lut.cpp` | (included in 15) |
| SWU-3.3 `xpe_apply_presentation_lut` | ✅ Done | `modules/display/src/presentation_lut.cpp` | 12 tests |
| SWU-3.3 `xpe_gsdf_calibrate` | ✅ Done | `modules/display/src/presentation_lut.cpp` | (included in 12) |
| `xpe_display_version()` | ✅ Done | `modules/display/src/display.cpp` | — |
| SWU-3.4 LUTManager | ❌ Deferred | — | Deferred to future SPEC |

**DLL exports (5 + 1 version):**

```c
// From modules/display/include/xpe/display/display_api.h
const char* xpe_display_version(void);
XpeErrorCode xpe_apply_modality_lut(XpeImageBuffer* img, const XpeModalityLutParams* params);
XpeErrorCode xpe_apply_voi_lut(XpeImageBuffer* img, const XpeVoiLutParams* params);
XpeErrorCode xpe_voi_preset_create(XpeVoiLutParams* params, XpeBodyPart bodyPart);
XpeErrorCode xpe_apply_presentation_lut(XpeImageBuffer* img, const XpePresentationLutParams* params);
XpeErrorCode xpe_gsdf_calibrate(const float* luminanceValues, uint32_t count, XpePresentationLutParams* outParams);
```

**C ABI types (defined in display_api.h):**

```c
typedef enum XpeModalityLutMode { XPE_MODALITY_LUT_LINEAR=0, XPE_MODALITY_LUT_TABLE=1 } XpeModalityLutMode;
typedef struct XpeModalityLutParams {
    XpeModalityLutMode mode;
    float rescaleSlope;
    float rescaleIntercept;
    const uint16_t* lutData;       // TABLE mode: pointer to LUT (owned by caller)
    uint32_t lutLength;
    int32_t lutFirstMapped;
    uint32_t lutBitsStored;
} XpeModalityLutParams;

typedef enum XpeVoiLutMode { XPE_VOI_LINEAR=0, XPE_VOI_LINEAR_EXACT=1, XPE_VOI_SIGMOID=2 } XpeVoiLutMode;
typedef enum XpeBodyPart { XPE_BODY_BONE=0, XPE_BODY_LUNG=1, XPE_BODY_ABDOMEN=2, XPE_BODY_HEAD=3 } XpeBodyPart;
typedef struct XpeVoiLutParams {
    XpeVoiLutMode mode;
    float center;
    float width;
    float minOut;  // typically 0.0f
    float maxOut;  // typically 1.0f
} XpeVoiLutParams;

typedef struct XpePresentationLutParams {
    uint16_t lutData[1024];  // 1024-entry LUT
    int32_t gsdfEnabled;
} XpePresentationLutParams;
```

### 2.2 ImageProcTest.exe GUI — GUI-S0 COMPLETED

**What is already working:**

| Component | Status | File |
|-----------|--------|------|
| `IXpeBackend` interface | ✅ Done | `gui/ImageProcTest/Services/IXpeBackend.cs` |
| `MockXpeBackend` | ✅ Done | `gui/ImageProcTest/Services/MockXpeBackend.cs` |
| `MainWindowViewModel` with all commands | ✅ Done | `gui/ImageProcTest/ViewModels/MainWindowViewModel.cs` |
| Raw image loading (file browser) | ✅ Done | `gui/ImageProcTest/Services/RawImageLoader.cs` |
| Calibration paths (Offset, Gain, Defect) | ✅ Done | `gui/ImageProcTest/Models/AppSettings.cs` |
| Settings persistence (`appsettings.json`) | ✅ Done | `gui/ImageProcTest/Services/AppSettingsService.cs` |
| Logs + Alerts panels (drain/clear) | ✅ Done | `MainWindowViewModel.cs` |
| Menu structure (6 groups: File, Backend, View, Pipeline, Tools, Help) | ✅ Done | `MainWindow.xaml` |
| Toolbar/menu command parity | ✅ Done | `MainWindow.xaml` |
| Help system (offline HTML) | ✅ Done | `gui/ImageProcTest/Services/HelpBundleService.cs` |
| Automation mode (headless E2E runner) | ✅ Done | `MainWindow.xaml.cs` |
| `XpeBackendFactory` | ✅ Done (Mock-only) | `gui/ImageProcTest/Services/XpeBackendFactory.cs` |

**What is NOT yet implemented (Phase 1b scope):**

| Component | Status | Notes |
|-----------|--------|-------|
| `RealXpeBackend` (P/Invoke wrapper) | ❌ Missing | Must wrap xpe_display.dll via P/Invoke |
| Display pipeline application on image load | ❌ Missing | `ProcessedImage == SourceImage` (no pipeline applied) |
| VOI LUT controls in UI | ❌ Missing | No WC/WW sliders or body part selector |
| Display settings in `AppSettings` | ❌ Missing | No `VoiWindowCenter`, `VoiWindowWidth`, etc. |
| Display version in `RuntimeInfo` | ❌ Missing | `BackendRuntimeInfo` lacks `DisplayVersion` field |
| Automation test extensions for display | ❌ Missing | Current E2E does not verify display pipeline |

---

## 3. Gap Analysis

### Gap 1: `IXpeBackend` — Missing Display Operations

The current interface (`gui/ImageProcTest/Services/IXpeBackend.cs`) does not expose any display pipeline methods.

**Required additions to `IXpeBackend`:**

```csharp
/// <summary>
/// Applies the full display pipeline (Modality LUT → VOI LUT → Presentation LUT)
/// to a float32 raw image and returns the processed uint16 preview.
/// </summary>
/// <returns>A LoadedImageFrame where ProcessedImage is the display-pipeline output.</returns>
LoadedImageFrame ApplyDisplayPipeline(LoadedImageFrame rawFrame, AppSettings settings);

/// <summary>
/// Returns the xpe_display.dll version string (e.g. "1.0.0").
/// </summary>
string GetDisplayVersion();

/// <summary>
/// Populates VOI LUT parameters with clinically validated presets for a body part.
/// </summary>
VoiPreset CreateVoiPreset(XpeBodyPartEnum bodyPart);
```

**Alternatively**, `LoadRawImage` can be extended to include display pipeline application:

```csharp
// Option B: extend LoadRawImage to accept display params
LoadedImageFrame LoadRawImage(string path, AppSettings settings, DisplayPipelineParams displayParams);
```

The recommended approach is **Option A** (separate `ApplyDisplayPipeline`) because it preserves the raw frame and allows toggling display pipeline on/off in the UI.

### Gap 2: `LoadedImageFrame` — Source vs. Processed Images

Current implementation in `MainWindowViewModel.LoadImageFromPath`:

```csharp
SourceImage = loadedFrame.Preview;
ProcessedImage = loadedFrame.Preview;  // SAME — display pipeline not applied
```

For Phase 1b integration:
- `SourceImage` = raw preview (uint16 rendering before display pipeline)
- `ProcessedImage` = result of applying Modality LUT → VOI LUT → Presentation LUT

The `LoadedImageFrame` model should carry both raw and processed image data, or the pipeline should be applied as a second step.

**Required change to `LoadedImageFrame.cs`:**

```csharp
public sealed class LoadedImageFrame
{
    // Existing
    public ImageSource? Preview { get; set; }           // Raw preview (unchanged)
    public string Summary { get; set; } = string.Empty;
    public string MetadataText { get; set; } = string.Empty;

    // New for Phase 1b
    public ImageSource? ProcessedPreview { get; set; }  // Post-display-pipeline preview
    public bool DisplayPipelineApplied { get; set; }    // Whether pipeline was applied
    public string DisplayPipelineSummary { get; set; } = string.Empty; // e.g. "Modality+VOI(Linear)+GSDF"
}
```

### Gap 3: `AppSettings` — Missing Display Parameters

Current `AppSettings.cs` does not include any display pipeline settings.

**Required additions:**

```csharp
// VOI LUT parameters
[JsonPropertyName("voiWindowCenter")]
public float VoiWindowCenter { get; set; } = 40.0f;

[JsonPropertyName("voiWindowWidth")]
public float VoiWindowWidth { get; set; } = 400.0f;

[JsonPropertyName("voiLutMode")]
public string VoiLutMode { get; set; } = "Linear";  // "Linear" | "LinearExact" | "Sigmoid"

[JsonPropertyName("selectedBodyPart")]
public string SelectedBodyPart { get; set; } = "Abdomen";  // "Bone" | "Lung" | "Abdomen" | "Head"

// Presentation LUT / GSDF
[JsonPropertyName("gsdfEnabled")]
public bool GsdfEnabled { get; set; } = false;  // false = linear LUT (safer default)

// Modality LUT
[JsonPropertyName("modalityRescaleSlope")]
public float ModalityRescaleSlope { get; set; } = 1.0f;

[JsonPropertyName("modalityRescaleIntercept")]
public float ModalityRescaleIntercept { get; set; } = -1024.0f;
```

### Gap 4: `BackendRuntimeInfo` — Missing Display Version

The current `BackendRuntimeInfo.cs` should include a `DisplayVersion` field to show the xpe_display.dll version in the runtime panel.

**Required addition to `BackendRuntimeInfo.cs`:**

```csharp
public string DisplayVersion { get; set; } = string.Empty;
```

### Gap 5: `RealXpeBackend` — P/Invoke Wrapper Not Implemented

No `RealXpeBackend.cs` exists. This is the class that will call `xpe_display.dll` via P/Invoke.

**Required P/Invoke declarations (C# struct layouts):**

```csharp
// File: gui/ImageProcTest/Services/Native/XpeDisplayInterop.cs

[StructLayout(LayoutKind.Sequential, Pack = 8)]
internal struct XpeModalityLutParams
{
    public int Mode;              // 0=LINEAR, 1=TABLE
    public float RescaleSlope;
    public float RescaleIntercept;
    public IntPtr LutData;        // uint16* (TABLE mode only, NULL for LINEAR)
    public uint LutLength;
    public int LutFirstMapped;
    public uint LutBitsStored;
}

[StructLayout(LayoutKind.Sequential, Pack = 8)]
internal struct XpeVoiLutParams
{
    public int Mode;              // 0=LINEAR, 1=LINEAR_EXACT, 2=SIGMOID
    public float Center;
    public float Width;
    public float MinOut;
    public float MaxOut;
}

[StructLayout(LayoutKind.Sequential, Pack = 8)]
internal struct XpePresentationLutParams
{
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 1024)]
    public ushort[] LutData;      // 1024-entry LUT
    public int GsdfEnabled;       // 0=disabled, non-zero=enabled
}

internal static class XpeDisplayNative
{
    private const string DllName = "xpe_display.dll";

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern IntPtr xpe_display_version();

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int xpe_apply_modality_lut(
        ref XpeImageBufferNative img,
        ref XpeModalityLutParams @params);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int xpe_apply_voi_lut(
        ref XpeImageBufferNative img,
        ref XpeVoiLutParams @params);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int xpe_voi_preset_create(
        out XpeVoiLutParams @params,
        int bodyPart);             // XpeBodyPart enum value

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int xpe_apply_presentation_lut(
        ref XpeImageBufferNative img,
        ref XpePresentationLutParams @params);

    [DllImport(DllName, CallingConvention = CallingConvention.Cdecl)]
    internal static extern int xpe_gsdf_calibrate(
        [In] float[] luminanceValues,
        uint count,
        out XpePresentationLutParams outParams);
}
```

**Note**: `XpeImageBufferNative` is already defined in `xpe_common.dll` interop. Verify its layout matches `XpeImageBuffer` in `xpe/common/xpe_types.h`.

---

## 4. UI Changes Required

### 4.1 Display Settings Panel (New Panel)

A new collapsible panel "Display Settings" should be added to the right panel stack, below the current "Calibration Paths" panel. It should contain:

| Control | Type | Binding | Range | Default |
|---------|------|---------|-------|---------|
| Window Center | NumericUpDown or Slider | `Settings.VoiWindowCenter` | -4096 to 4096 | 40 |
| Window Width | NumericUpDown or Slider | `Settings.VoiWindowWidth` | 1 to 8192 | 400 |
| VOI Mode | ComboBox | `Settings.VoiLutMode` | Linear / LinearExact / Sigmoid | Linear |
| Body Part Preset | ComboBox | `Settings.SelectedBodyPart` | Bone / Lung / Abdomen / Head | Abdomen |
| Apply Preset | Button | `ApplyBodyPartPresetCommand` | — | — |
| GSDF Enabled | CheckBox | `Settings.GsdfEnabled` | — | false |
| Show Display Panel | ToggleMenuItem | `Settings.ShowDisplayPanel` | — | true |

### 4.2 Toolbar and Menu Extensions

**View Menu** — add display panel toggle:
- `Show Display Settings` → `ShowDisplaySettingsPanel` command (mirrors panel checkbox)

**Pipeline Menu** — activate display commands (currently disabled shells):
- `Apply Display Pipeline` → `ApplyDisplayPipelineCommand` (enabled when image loaded + Native backend)
- `Stage Timing` → shows timing breakdown per LUT stage

**Tools Menu** — add GSDF calibration:
- `GSDF Calibrate...` → opens luminance input dialog → calls `xpe_gsdf_calibrate`

### 4.3 Image Preview Area

The current GUI displays `SourceImage` and `ProcessedImage` side-by-side (or overlaid). Once display pipeline is integrated:

- Left pane = raw preview (pre-pipeline)
- Right pane = processed preview (post-pipeline, shows VOI LUT + GSDF result)
- Status bar should show: `Display: Modality(1.0/-1024) → VOI(Linear,C=40,W=400) → GSDF(off)`

---

## 5. `MockXpeBackend` Extensions

The `MockXpeBackend` must implement the new `IXpeBackend` methods to remain runnable in Mock mode:

```csharp
public LoadedImageFrame ApplyDisplayPipeline(LoadedImageFrame rawFrame, AppSettings settings)
{
    // Mock: return same frame but set DisplayPipelineApplied = true
    return rawFrame with
    {
        ProcessedPreview = rawFrame.Preview,   // Mock: no actual transformation
        DisplayPipelineApplied = true,
        DisplayPipelineSummary = $"MOCK: VOI({settings.VoiLutMode}, C={settings.VoiWindowCenter}, W={settings.VoiWindowWidth})"
    };
}

public string GetDisplayVersion() => "v0.0.0-mock-display";

public VoiPreset CreateVoiPreset(XpeBodyPartEnum bodyPart) => bodyPart switch
{
    XpeBodyPartEnum.Bone    => new VoiPreset(Center: 500, Width: 2000),
    XpeBodyPartEnum.Lung    => new VoiPreset(Center: -600, Width: 1600),
    XpeBodyPartEnum.Abdomen => new VoiPreset(Center: 40, Width: 400),
    XpeBodyPartEnum.Head    => new VoiPreset(Center: 40, Width: 80),
    _ => throw new ArgumentOutOfRangeException(nameof(bodyPart))
};
```

---

## 6. `RealXpeBackend` — Implementation Requirements

`RealXpeBackend` must wrap the 5 xpe_display.dll functions. Minimum implementation:

```csharp
// gui/ImageProcTest/Services/RealXpeBackend.cs
public sealed class RealXpeBackend : IXpeBackend
{
    // Initialize: call xpe_display_version() for version check
    // LoadRawImage: load raw bytes, create XpeImageBuffer, call Modality+VOI+Presentation
    // ApplyDisplayPipeline: call Modality+VOI+Presentation on existing XpeImageBuffer
    // GetDisplayVersion: return xpe_display_version() string
    // CreateVoiPreset: call xpe_voi_preset_create()
    // Shutdown: free any held native buffers
}
```

**XpeBackendFactory.Create** should be updated to return `RealXpeBackend` when:
- `settings.BackendMode == "Native"`, AND
- `xpe_display.dll` is detected in the application directory.

---

## 7. `IXpeBackend` Version Contract

The current `IXpeBackend.GetVersion()` returns the common/backend version. For Phase 1b, the runtime panel should also show the display module version.

**Suggested approach** — extend `BackendRuntimeInfo`:

```csharp
public string DisplayVersion { get; set; } = string.Empty;   // e.g. "1.0.0"
public bool DisplayDllDetected { get; set; }                  // whether xpe_display.dll found
public string DisplayDllPath { get; set; } = string.Empty;
```

The `Initialize()` method should populate these fields using `xpe_display_version()` (or return defaults in Mock mode).

---

## 8. Automation Scenario Extensions

The current automation scenario in `MainWindow.xaml.cs::RunAutomationScenarioAsync` needs display pipeline verification:

**Required additions to `GuiAutomationReport`:**

```csharp
public bool DisplayPipelineApplied { get; set; }
public string DisplayPipelineSummary { get; set; } = string.Empty;
public bool DisplayPanelVisible { get; set; }
public string DisplayVersion { get; set; } = string.Empty;
public bool VoiPresetApplied { get; set; }
```

**Required E2E scenario steps (to be added after existing load step):**

```csharp
// After loading image:
ClickButton(ApplyBodyPartPresetButton);    // select Abdomen preset
await Task.Delay(100);

// Verify display applied
report.DisplayPipelineApplied = viewModel.ActiveImageFrame?.DisplayPipelineApplied ?? false;
report.DisplayPipelineSummary = viewModel.ActiveImageFrame?.DisplayPipelineSummary ?? string.Empty;
report.DisplayVersion = viewModel.RuntimeInfo.DisplayVersion;

// Pass condition includes:
report.Passed &= report.DisplayPipelineApplied;
report.Passed &= !string.IsNullOrWhiteSpace(report.DisplayVersion);
```

---

## 9. Body Part Enum for C# (New Type)

Define a C# enum that maps to `XpeBodyPart` in C++:

```csharp
// gui/ImageProcTest/Models/XpeBodyPartEnum.cs
public enum XpeBodyPartEnum
{
    Bone    = 0,   // maps to XPE_BODY_BONE
    Lung    = 1,   // maps to XPE_BODY_LUNG
    Abdomen = 2,   // maps to XPE_BODY_ABDOMEN
    Head    = 3    // maps to XPE_BODY_HEAD
}
```

---

## 10. VOI Preset Value Reference

Body part presets from `display_api.h` / `SPEC-XPE-P1B-DISP REQ-DISP-017`:

| XpeBodyPart | C++ Enum | Center | Width | Mode |
|-------------|----------|-------:|------:|------|
| `XPE_BODY_BONE` | 0 | 500 | 2000 | Linear |
| `XPE_BODY_LUNG` | 1 | -600 | 1600 | Linear |
| `XPE_BODY_ABDOMEN` | 2 | 40 | 400 | Linear |
| `XPE_BODY_HEAD` | 3 | 40 | 80 | Linear |

Note: These 4 presets are what `xpe_voi_preset_create()` supports. The extended preset library (chest_pa, chest_lateral, extremity, spine, pediatric, fluoroscopy) is part of SWU-3.4 LUT Manager which is **deferred** to a future SPEC.

---

### 10.1 Body-Part Preset Scope Finding (2026-04-16)

The four presets above are **not** the complete DR body-part preset taxonomy. They are Phase 1b quick presets used to validate the display P/Invoke path, VOI parameter propagation, and GUI command wiring.

Cross-check basis:

- DICOM PS3.16 Annex L maps many Body Part Examined defined terms to coded anatomic regions, including chest, abdomen, pelvis, skull, cervical/thoracic/lumbar spine, shoulder, humerus, elbow, forearm, wrist, hand, knee, ankle, foot, ribs, and combined regions.
- DICOM PS3.3 CR/DX modules separate Body Part Examined / Anatomic Region from View Position and positioning metadata. A display preset selected only by body part is incomplete for projection radiography.
- ACR DIR DR anatomical-view grouping and RadLex Playbook concepts both treat radiography exams as anatomy plus view/projection/context rather than a four-category body-part list.

Design implication:

- Keep `XpeBodyPartEnum` with four values for Phase 1b `xpe_voi_preset_create()` compatibility.
- Do not expand the four-value enum ad hoc in the GUI without the native LUT Manager contract.
- Track the full solution under SWU-3.4 LUT Manager as a table-driven preset library keyed by `bodyPart`, `viewPosition`, `laterality`, `patientGroup`, and `displayIntent`.

Minimum deferred factory library:

| Group | Required body/view profiles |
|---|---|
| Chest | chest_pa, chest_ap_portable, chest_lateral |
| Abdomen | abdomen_ap, abdomen_upright, kub |
| Pelvis/Hip | pelvis_ap, hip_ap, hip_lateral |
| Spine | cspine_ap_lateral, tspine_ap_lateral, lspine_ap_lateral |
| Upper extremity | shoulder, humerus, elbow, forearm, wrist, hand |
| Lower extremity | knee, tibia_fibula, ankle, foot |
| Head/skull | skull, sinus, facial_bone |
| Special groups | pediatric_low_dose, fluoroscopy_realtime |

Deferred SWU-3.4 acceptance:

- factory preset count shall be at least 15 body-part/view profiles,
- each preset shall carry source-standard mapping notes: DICOM Body Part Examined or Anatomic Region, View Position when applicable, and RadLex/Playbook-style exam label when available,
- GUI evidence shall record both the quick preset and the resolved extended preset ID once SWU-3.4 is implemented.

References:

- DICOM PS3.16 Annex L: https://dicom.nema.org/medical/dicom/current/output/chtml/part16/chapter_l.html
- DICOM PS3.3 CR/DX image modules and positioning attributes: https://dicom.nema.org/medical/Dicom/2023e/output/chtml/part03/sect_C.8.html
- RadLex Playbook: https://playbook.radlex.org/
- ACR DIR DR: https://nrdrsupport.acr.org/support/solutions/articles/11000065405-dir-digital-radiography-dr-

---

## 11. SWU-3.4 LUT Manager — Deferred Scope

Per `SPEC-XPE-P1B-DISP v1.0.0 Section 1.2`:

> LUT Manager (SWU-3.4: preset CRUD, auto-select) — deferred to separate SPEC or P1B-DISP iteration 2

Documents that reference SWU-3.4 (and are therefore ahead of implementation):
- `docs/display/README.md` — references LUT Manager API and factory presets
- `docs/display/xpe-display-prd.md` — Section 6 describes full LUT Manager
- `docs/display/SAD-DISPLAY-001` — includes SWU-3.4 in architecture diagram

The GUI implementation for Phase 1b should **not** attempt to implement LUT Manager functionality (preset CRUD, auto-select from body part string). Only the 4 presets from `xpe_voi_preset_create()` are supported in this phase.

---

## 12. Performance Budget for GUI Integration

The full display pipeline must complete within the interactive response budget:

| Stage | C++ Budget | Total Pipeline |
|-------|:----------:|:-------------:|
| `xpe_apply_modality_lut` | ≤ 20ms | |
| `xpe_apply_voi_lut` | ≤ 16ms | |
| `xpe_apply_presentation_lut` | ≤ 25ms | |
| **Total** | **≤ 65ms** | from click to display |

For the GUI, this means:
- `ApplyDisplayPipeline()` must not block the UI thread
- Use `Task.Run()` or `async/await` to call the native pipeline off the UI thread
- Show a loading indicator in StatusBar while pipeline runs
- Update `ProcessedImage` binding on completion (back on UI thread via `Dispatcher.InvokeAsync`)

---

## 13. File Change Summary

Files that need to be modified or created for Phase 1b Display integration:

| File | Change Type | Reason |
|------|-------------|--------|
| `gui/ImageProcTest/Services/IXpeBackend.cs` | Modify | Add `ApplyDisplayPipeline`, `GetDisplayVersion`, `CreateVoiPreset` |
| `gui/ImageProcTest/Services/MockXpeBackend.cs` | Modify | Implement new interface methods (mock) |
| `gui/ImageProcTest/Services/RealXpeBackend.cs` | Create | P/Invoke wrapper for xpe_display.dll |
| `gui/ImageProcTest/Services/Native/XpeDisplayInterop.cs` | Create | P/Invoke declarations for display DLL |
| `gui/ImageProcTest/Services/XpeBackendFactory.cs` | Modify | Return `RealXpeBackend` when native DLL detected |
| `gui/ImageProcTest/Models/AppSettings.cs` | Modify | Add VOI LUT + GSDF settings fields |
| `gui/ImageProcTest/Models/BackendRuntimeInfo.cs` | Modify | Add `DisplayVersion`, `DisplayDllDetected`, `DisplayDllPath` |
| `gui/ImageProcTest/Models/LoadedImageFrame.cs` | Modify | Add `ProcessedPreview`, `DisplayPipelineApplied`, `DisplayPipelineSummary` |
| `gui/ImageProcTest/Models/XpeBodyPartEnum.cs` | Create | C# enum matching `XpeBodyPart` in C++ |
| `gui/ImageProcTest/Models/GuiAutomationReport.cs` | Modify | Add display pipeline verification fields |
| `gui/ImageProcTest/ViewModels/MainWindowViewModel.cs` | Modify | Add `ApplyBodyPartPresetCommand`, `ShowDisplayPanel`, display status text |
| `gui/ImageProcTest/MainWindow.xaml` | Modify | Add Display Settings panel, display status bar |
| `gui/ImageProcTest/MainWindow.xaml.cs` | Modify | Add automation steps for display pipeline verification |
| `gui/ImageProcTest/fixtures/gui-s0/appsettings.template.json` | Modify | Add new settings fields with defaults |

---

## 14. Known Issues / Warnings

### GSDF Barten Model Validation (@MX:WARN)

The `xpe_gsdf_calibrate` implementation has an `@MX:WARN` annotation:

```
@MX:WARN: Numerical precision sensitive — validate with DICOM PS3.14 test vectors
@MX:REASON: Barten model uses empirical constants; different calibration data may require tuning
```

For GUI integration: the GSDF feature should default to `gsdfEnabled = false` until the Barten constants are validated against DICOM PS3.14 test vectors. The UI should show a warning when GSDF is enabled.

### Mock Mode Limitation

In Mock mode, `ApplyDisplayPipeline` returns the raw preview unchanged (no actual LUT transformation). This means the GUI's "before/after" display will look identical in Mock mode. This is expected behavior — the Mock backend is not meant to produce medically correct output.

---

## 15. Implementation Update - 2026-04-16 Codex

The first GUI Phase 1b display integration pass has been implemented in `ImageProcTest.exe`.

Implemented files:

- `gui/ImageProcTest/Services/IXpeBackend.cs`: added `ApplyDisplayPipeline`, `GetDisplayVersion`, and `CreateVoiPreset`.
- `gui/ImageProcTest/Services/RealXpeBackend.cs`: added native P/Invoke backend for the display pipeline.
- `gui/ImageProcTest/Services/Native/XpeDisplayInterop.cs`: added C ABI structs and P/Invoke declarations.
- `gui/ImageProcTest/Services/MockXpeBackend.cs`: added mock display pipeline and four VOI presets.
- `gui/ImageProcTest/Services/XpeBackendFactory.cs`: added native DLL detection plus required-export validation before switching to `RealXpeBackend`.
- `gui/ImageProcTest/Models/AppSettings.cs`: added display settings fields.
- `gui/ImageProcTest/Models/BackendRuntimeInfo.cs`: added display DLL/version fields.
- `gui/ImageProcTest/Models/LoadedImageFrame.cs`: added processed preview and display pipeline metadata.
- `gui/ImageProcTest/Models/XpeBodyPartEnum.cs` and `VoiPreset.cs`: added C# body-part and preset models.
- `gui/ImageProcTest/ViewModels/MainWindowViewModel.cs`: added display pipeline execution, preset application, status text, and telemetry handling.
- `gui/ImageProcTest/MainWindow.xaml`: added Display Settings panel and display pipeline commands.
- `gui/ImageProcTest/MainWindow.xaml.cs` and `GuiAutomationReport.cs`: added display pipeline verification fields.
- `gui/ImageProcTest.SelfCheck` and `gui/ImageProcTest.E2E`: added display integration verification.

Verification completed:

- `dotnet build gui\ImageProcTest\ImageProcTest.csproj -c Debug`
- `dotnet build gui\ImageProcTest.SelfCheck\ImageProcTest.SelfCheck.csproj -c Debug`
- `dotnet build gui\ImageProcTest.E2E\ImageProcTest.E2E.csproj -c Debug`
- `ImageProcTest.SelfCheck.exe`
- `ImageProcTest.E2E.exe`
- `tools\e2e\Invoke-ImageProcTestGuiRealE2E.ps1`

Native DLL ABI note:

- The currently available `build\enhance_test\bin\Release\xpe_display.dll` loads but does not expose the required Phase 1b symbols such as `xpe_apply_modality_lut`.
- The GUI therefore validates required exports before activating `RealXpeBackend`.
- If any required export is missing, the GUI safely falls back to Mock mode and keeps display workflow validation available.

Remaining work:

- Rebuild and publish a compatible `xpe_display.dll` that exports all six Phase 1b display ABI functions.
- Run a Native-mode E2E pass once the compatible DLL is available.
- Keep `gsdfEnabled=false` by default until DICOM PS3.14 validation vectors are accepted.

---

## 16. Large Image Comparison Viewer Requirement - 2026-04-16 Codex

The next GUI display-integration work package shall replace the temporary two-pane source/processed preview pattern with the approval-gated design in `XPE-GUI-COMPARE-001_Large_Image_Comparison_Viewer_Spec.md`.

Required direction:

- default comparison UX: one in-app `ImageComparisonViewport`,
- default mode: vertical swipe/wiper slider,
- supported modes: swipe, split locked, overlay opacity, difference heatmap, source only, processed only,
- interaction: synchronized zoom fit, 100%, zoom in/out, mouse-wheel zoom, pan, cursor pixel readout,
- data safety: processed layer updates shall not overwrite the source layer,
- large image target: 4096x4096 UInt16 source + processed comparison in Phase 1b,
- extension path: tile-backed rendering and LRU cache before claiming larger-than-4096 support,
- optional detached viewer: allowed, but it must reuse the same viewport state model.

No implementation shall begin until the user approves this comparison viewer work package.

---

## 17. Revision History

| Version | Date | Author | Description |
|---------|------|--------|-------------|
| 1.0.0 | 2026-04-16 | MoAI | Initial cross-validation: GUI-S0 vs SPEC-XPE-P1B-DISP. 5 gaps identified, integration guide created. |
| 1.1.0 | 2026-04-16 | Codex | Recorded GUI Phase 1b display integration implementation, verification, and native ABI fallback status. |
| 1.2.0 | 2026-04-16 | Codex | Added approval-gated large image comparison viewer requirement reference. |

---

*Document End — XPE-GUI-DISP-INT-001 v1.0.0*
