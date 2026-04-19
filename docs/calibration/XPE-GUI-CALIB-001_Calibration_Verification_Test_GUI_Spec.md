# XPE Test GUI — Calibration Verification Specification

**Document ID:** XPE-GUI-CALIB-001 v1.0  
**IEC 62304 Clause:** 5.3 (Software Architectural Design), 5.5.1 (Software Unit Implementation)  
**Safety Classification:** Class B (QA/Test Tool — not deployed in clinical product mode)  
**Date:** 2026-04-19  
**Author:** XPE Calibration Development Team  
**Status:** Controlled Draft

---

## 1. Purpose and Scope

### 1.1 Purpose

This document specifies functional requirements, UI design, P/Invoke interface contracts, and acceptance criteria for the **calibration verification features** of `ImageProcTest.exe`. It targets two categories of work:

1. **Gap closure (Phase 1):** Detector-domain metrics, test fixture directory integration  
2. **New features (Phase 2):** BPM generation panel (FUNC-022~025), algorithm comparison  
3. **Advanced verification (Phase 3):** Remaining adapter-pending SWUs (1.6, 1.7, 1.8, 1.9, 1.10, 1.4)

This specification is the single authoritative source for C# WPF implementation in `clients/ImageProcTest/`. Refer to `SAD-CALIB-001 §3.3` for the corresponding C++ ABI design.

### 1.2 Scope Boundary

**In scope:**
- `clients/ImageProcTest/` — all C# source files
- New: BPM Generation Panel, Algorithm Comparison Panel, Detector-Domain Metrics
- New: Test fixture registration for CalData_6, Grid_abnormal, cyan_test

**Out of scope:**
- Native DLL implementation (governed by SRS-CALIB-001, SAD-CALIB-001)
- Clinical product UI (product mode mandates offset+gain, no bypass)
- FlaUI E2E test automation (see `XPE-GUI-E2E-001`)

### 1.3 IEC 62304 Exception Reference

Per **SRS-CALIB-SAFE-004** (Test GUI evaluation exception):

> `ImageProcTest.exe` may expose `Off`, `On`, and `Auto` controls for each calibration/preprocessing stage. This exception applies only to QA/Test GUI workflows; every bypass or forced-stage decision shall be recorded in the automation/evidence report.

All new calibration controls in this document operate under this exception clause.

---

## 2. Reference Documents

| ID | Document | Role |
|----|----------|------|
| SRS-CALIB-001 | Software Requirements Specification | Requirement source (FUNC-001~025) |
| SAD-CALIB-001 §3.3~3.4 | Software Architecture Document | SWU-1.11 BpmGenerator ABI |
| TDS-CALIB-001 §9.4 | Test Dataset Specification | CalData_6, Grid_abnormal, cyan_test dataset specs |
| PRIOR-ART-BPM-ALGORITHM.md | BPM Algorithm Analysis | MC vs Blue vs RMM comparison baseline |
| XPE-GUI-ARCH-001 | XAML/MVVM Architecture | UI pattern compliance |
| XPE-GUI-ACCESS-001 | Accessibility Guide | AutomationId naming rules |

---

## 3. Current Implementation Inventory

### 3.1 Implemented Features (as of 2026-04-19)

| Feature | Files | Status |
|---------|-------|--------|
| Offset correction (SWU-1.1) | `NativePreprocessPreviewService.cs:356-363` | ✓ Runnable |
| Gain correction (SWU-1.2) | `NativePreprocessPreviewService.cs:370-382` | ✓ Runnable |
| Defect correction (SWU-1.3) | `NativePreprocessPreviewService.cs:384-397` | ✓ Runnable |
| Off/On/Auto stage toggles | `MainWindow.xaml:422-449` | ✓ Wired (3 stages) |
| Calibration expiry check | `NativePreprocessPreviewService.cs:606-727` | ✓ Both ABI variants |
| XCal file generation (xcal write) | `NativePreprocessPreviewService.cs:479-561` | ✓ Offset/Gain/Defect |
| Fixture case discovery | `FixtureCatalogService.cs:10-25` | ✓ `calibration_cases/` only |
| Static BPM threshold mask | `NativePreprocessPreviewService.cs:532-561` | ✓ Threshold-only |
| Before/after swipe comparison | `MainWindow.xaml:495-558` | ✓ Swipe + zoom |
| Module readiness (R0-R3) | `ModuleReadinessService.cs` | ✓ R3 with oracle |
| Algorithm catalog (10 SWUs) | `AlgorithmValidationCatalogService.cs:21-32` | ✓ Catalog only |
| Evidence report (JSON + MD) | `GuiE2eReportService.cs` | ✓ Session + fixture |
| Basic metrics (13 items) | `MainWindow.xaml.cs:1004-1048` | ✓ Partial |

**P/Invoke delegates (xpe_preprocess.dll):**

```csharp
// NativePreprocessPreviewService.cs lines 82-164
InitDelegate          : XpeErrorCode(IntPtr config)
ShutdownDelegate      : void()
CalibrationExpiryDelegate : XpeErrorCode(IntPtr path, IntPtr first, IntPtr second)
OffsetCorrectionDelegate  : XpeErrorCode(ref XpeImageBuffer img, ref XpeImageBuffer offset)
GainCorrectionDelegate    : XpeErrorCode(ref XpeImageBuffer img, ref XpeImageBuffer gain)
DefectCorrectionDelegate  : XpeErrorCode(ref XpeImageBuffer img, ref XpeImageBuffer defect, IntPtr cfg)
```

### 3.2 Gap Analysis Matrix

| Category | Required | Currently Shown | Gap |
|----------|----------|-----------------|-----|
| **Detector-domain dark metrics** | DarkBias, DSNU_ADU, DarkReduction_dB | None | ❌ Missing |
| **Detector-domain flat metrics** | PRNU_CV, FlatResidualPct, LineArtifactScore | None | ❌ Missing |
| **Defect metrics** | DefectRecall, DefectFPR, DefectResidualADU | None | ❌ Missing |
| **BPM generation** | Interactive multi-frame generation (FUNC-022~024) | Static threshold mask only | ❌ Missing |
| **Grid robustness test** | LineArtifactScore gate (FUNC-025) | None | ❌ Missing |
| **Algorithm comparison** | MC vs Blue vs RMM side-by-side | None | ❌ Missing |
| **Fixture: CalData_6** | Multi-step gain + BPM generation fixtures | Not registered | ❌ Missing |
| **Fixture: Grid_abnormal** | Grid artifact BPM comparison fixture | Not registered | ❌ Missing |
| **Fixture: cyan_test** | Production calibration E2E fixture | Not registered | ❌ Missing |
| **SWU-1.11 in catalog** | BpmGenerator in AlgorithmValidationCatalog | Not present | ❌ Missing |

---

## 4. Phased Enhancement Plan

### Phase 1 — Detector-Domain Metrics + Fixture Integration (Priority: High)

**Goal:** Enable quantitative calibration quality assessment without new panels.  
**Changes:** Extend existing Metrics tab + FixtureCatalogService.

- F1-01: Add DarkBias, DSNU_ADU, DarkReduction_dB to Metrics tab
- F1-02: Add PRNU_CV, FlatResidualPct, FPN_Reduction_dB, LineArtifactScore to Metrics tab  
- F1-03: Add DefectRecall, DefectFPR, DefectResidualADU to Metrics tab (when oracle BPM present)
- F1-04: Extend `FixtureCatalogService` to discover CalData_6, Grid_abnormal, cyan_test
- F1-05: Update `InferCalibrationRole` for `calset_*`, `bright0[0-9]`, `masterDark`, `masterBright`

### Phase 2 — BPM Generation Panel + Algorithm Comparison (Priority: High)

**Goal:** Implement and verify FUNC-022~025 (SAD §3.3 SWU-1.11).

- F2-01: BPM Generation Panel (new sub-tab in Diagnostics tab)
- F2-02: P/Invoke binding for `xpe_bpm_generate`
- F2-03: BpmConfig parameter controls (lambda, mask size, tolerance, frame counts)
- F2-04: Generated BPM visualization (defect overlay on source image)
- F2-05: Algorithm Comparison Panel (MC / Blue / RMM triple-run)
- F2-06: Add SWU-1.11 to `AlgorithmValidationCatalogService`

### Phase 3 — Adapter-Pending SWUs (Priority: Medium)

**Goal:** Implement verification adapters for 6 currently blocked SWUs.

- F3-01: SWU-1.7 NonlinearityCorrector — LUT/polynomial profile loader + residual plot
- F3-02: SWU-1.9 RuntimeDefectDetector — 10-frame sequence loader + runtime BPM merge view
- F3-03: SWU-1.4 GhostCorrector — lag sequence loader + tier selection display
- F3-04: SWU-1.6 TempCompensator — temperature metadata entry + compensation delta display
- F3-05: SWU-1.8 BinningCorrector — binning mode selection + gain scaling evidence
- F3-06: SWU-1.10 SessionManager — multi-session concurrent fixture runner

---

## 5. Feature Specifications

### 5.1 Detector-Domain Metrics Extension (F1-01 ~ F1-03)

#### 5.1.1 Metric Definitions

All metrics computed in C# after native stage execution. No new P/Invoke required.

**Dark / Offset metrics** (require offset stage output vs dark input):

| Metric | Formula | Unit | Gate |
|--------|---------|------|------|
| `DarkBias` | `mean(I_corrected_dark)` | ADU | ≤ 5 ADU |
| `DSNU_ADU` | `stdev(I_corrected_dark)` | ADU | ≤ 20 ADU (target ≤ 10) |
| `DarkReduction_dB` | `20 × log10(stdev(I_raw) / stdev(I_corrected))` | dB | ≥ 10 dB |
| `ClampRate` | `count(I_corrected == 0) / total_pixels × 100` | % | ≤ 1% |

**Flat-field / Gain metrics** (require gain stage output):

| Metric | Formula | Unit | Gate |
|--------|---------|------|------|
| `PRNU_CV` | `stdev(I_gain_output) / mean(I_gain_output) × 100` | % | ≤ 1% |
| `FlatResidualPct` | same as PRNU_CV (alias) | % | ≤ 1.0% (release: ≤ 0.5%) |
| `FPN_Reduction_dB` | `20 × log10(PRNU_CV_before / PRNU_CV_after)` | dB | ≥ 20 dB |
| `LineArtifactScore` | see §5.1.2 | % | ≤ 10% (target ≤ 5%) |

**Defect / BPM metrics** (require reference oracle BPM):

| Metric | Formula | Unit | Gate |
|--------|---------|------|------|
| `DefectRecall` | `TP / (TP + FN) × 100` where oracle = ground truth | % | ≥ 95% |
| `DefectFPR` | `FP / (FP + TN) × 100` | % | ≤ 0.001% |
| `DefectResidualADU` | `mean(|I_corrected[bad_pixels]| )` | ADU | ≤ 2 ADU |
| `GoodPixelDeltaP99` | `percentile99(|I_corrected - I_input|[good_pixels])` | ADU | ≤ 0 (no change) |

#### 5.1.2 LineArtifactScore Calculation

```csharp
/// <summary>
/// Computes mid-frequency line artifact energy ratio using 2D FFT.
/// Low-frequency band: 0.0 – 0.05 cycles/pixel (DC and bulk signal)
/// Mid-frequency band: 0.05 – 0.30 cycles/pixel (periodic line artifacts)
/// High-frequency band: 0.30 – 0.50 cycles/pixel (noise)
/// </summary>
/// <returns>LineArtifactScore in range [0, 100] percent</returns>
public static double ComputeLineArtifactScore(float[] image, int width, int height)
{
    // 1. Apply Hanning window to suppress spectral leakage
    // 2. Compute 2D DFT via row-column decomposition (System.Numerics.Complex)
    // 3. Compute magnitude spectrum |F(u,v)|
    // 4. Sum energy in radial frequency bands:
    //    f_r = sqrt((u/width)^2 + (v/height)^2)  for each (u,v) in [0, W/2] x [0, H/2]
    //    E_mid = sum(|F|^2) where 0.05 ≤ f_r < 0.30
    //    E_total = sum(|F|^2) for all f_r
    // 5. Return 100.0 * E_mid / E_total
}
```

**Implementation location:** `Services/MetricsComputationService.cs` (new file)  
**Performance budget:** ≤ 500 ms for 3072×3072 image (use `System.Numerics.Complex` with row-wise FFT)

#### 5.1.3 UI Placement

Extend existing **Metrics tab** (`MainWindow.xaml:574-642`) with three new `DataGrid` sections:

```xaml
<!-- Detector-Domain Dark Metrics -->
<DataGrid x:Name="DarkMetricsGrid" AutomationProperties.AutomationId="DarkMetricsGrid"
          Margin="0,4,0,0" Height="100" IsReadOnly="True" CanUserAddRows="False">
    <!-- Columns: Metric | Value | Gate | Status -->
</DataGrid>

<!-- Detector-Domain Flat Metrics -->
<DataGrid x:Name="FlatMetricsGrid" AutomationProperties.AutomationId="FlatMetricsGrid"
          Margin="0,4,0,0" Height="120" IsReadOnly="True" CanUserAddRows="False">
</DataGrid>

<!-- Defect Metrics (visible only when oracle BPM selected) -->
<DataGrid x:Name="DefectMetricsGrid" AutomationProperties.AutomationId="DefectMetricsGrid"
          Margin="0,4,0,0" Height="100" IsReadOnly="True" CanUserAddRows="False"
          Visibility="{Binding OracleBpmPresent, Converter={StaticResource BoolToVisibility}}">
</DataGrid>
```

**State model:** `MetricsViewModel` (new class) with `ObservableCollection<MetricRow>` properties for each grid.

---

### 5.2 Fixture Catalog Extension (F1-04 ~ F1-05)

#### 5.2.1 Extended Discovery Paths

Modify `FixtureCatalogService.LoadCases()` to also search named test data directories:

```csharp
// Current (keep):
"tests/test_data/calibration_cases"

// New — additional BPM-specific fixture roots:
"tests/test_data/CalData_6"        → flat-structure case (no images/ sub-dir)
"tests/test_data/Grid_abnormal"    → flat-structure case
"tests/test_data/cyan_test"        → flat-structure case
```

**Flat-structure case handling:** When `images/` subdirectory is absent, treat the root directory's `.raw` files as both images and calibration inputs according to role inference.

#### 5.2.2 Extended Role Inference Rules

Extend `InferCalibrationRole(string fileName)`:

```csharp
// Add to existing logic (before the final Unknown return):

// CalSet files: CalSet_14037.raw, CalSet_17285.raw, ... (ADU level in name)
if (Regex.IsMatch(name, @"^calset_\d+$"))
    return CalibrationRole.Gain;

// Multi-frame bright files: bright01..bright06
if (Regex.IsMatch(name, @"^bright\d{2}$"))
    return CalibrationRole.Gain;

// Master files
if (name.StartsWith("masterdark", StringComparison.OrdinalIgnoreCase))
    return CalibrationRole.Offset;
if (name.StartsWith("masterbright", StringComparison.OrdinalIgnoreCase))
    return CalibrationRole.Gain;

// Pre/NonPre comparison images
if (name.EndsWith("_pre", StringComparison.OrdinalIgnoreCase) ||
    name.EndsWith("_nonpre", StringComparison.OrdinalIgnoreCase))
    return CalibrationRole.Reference;

// Result images
if (name.EndsWith("_result", StringComparison.OrdinalIgnoreCase))
    return CalibrationRole.Reference;
```

#### 5.2.3 Fixture Manifest for CalData_6

File: `tests/test_data/CalData_6/fixture.json`

```json
{
  "case_id": "CalData_6",
  "description": "Multi-step gain calibration: 6 dose levels for nonlinearity LUT generation",
  "srs_refs": ["SRS-CALIB-FUNC-022", "SRS-CALIB-FUNC-023", "SRS-CALIB-FUNC-024"],
  "layout": "flat",
  "roles": {
    "dark.raw":     "Offset",
    "bright01.raw": "Gain",
    "bright02.raw": "Gain",
    "bright03.raw": "Gain",
    "bright04.raw": "Gain",
    "bright05.raw": "Gain",
    "bright06.raw": "Gain",
    "BPMap.map":    "Defect_Oracle"
  },
  "verification_targets": {
    "min_gain_frames": 6,
    "expected_defect_density_pct_max": 5.0
  }
}
```

`FixtureCatalogService` shall read `fixture.json` when present and use its `roles` mapping in preference to automatic inference. `Defect_Oracle` maps to `CalibrationRole.DefectOracle` (new enum value for ground-truth BPM reference).

#### 5.2.4 Fixture Manifest for Grid_abnormal

File: `tests/test_data/Grid_abnormal/fixture.json`

```json
{
  "case_id": "Grid_abnormal",
  "description": "BPM algorithm comparison under grid artifact conditions",
  "srs_refs": ["SRS-CALIB-FUNC-025"],
  "layout": "flat",
  "roles": {
    "2G_Pre.raw":         "Reference",
    "Blue_Pre.raw":       "Reference",
    "Blue_NonPre.raw":    "Reference",
    "BPM.raw":            "Defect_Oracle",
    "MasterBright.raw":   "Gain",
    "MasterDark.raw":     "Offset"
  },
  "verification_targets": {
    "line_artifact_score_max_pct": 10.0,
    "blue_algo_line_artifact_target_pct": 5.0
  }
}
```

#### 5.2.5 Fixture Manifest for cyan_test

File: `tests/test_data/cyan_test/fixture.json`

```json
{
  "case_id": "cyan_test",
  "description": "Cyan detector production calibration + clinical E2E verification",
  "srs_refs": ["SRS-CALIB-FUNC-005", "SRS-CALIB-FUNC-006", "SRS-CALIB-FUNC-022"],
  "layout": "flat",
  "roles": {
    "Dark_07.raw":       "Offset",
    "Dark_08.raw":       "Offset",
    "Dark_09.raw":       "Offset",
    "Dark_10.raw":       "Offset",
    "Dark_11.raw":       "Offset",
    "Dark_12.raw":       "Offset",
    "Dark_13.raw":       "Offset",
    "Dark_14.raw":       "Offset",
    "Dark_15.raw":       "Offset",
    "Dark_16.raw":       "Offset",
    "Bright_17.raw":     "Gain",
    "CalSet_14037.raw":  "Gain",
    "CalSet_17285.raw":  "Gain",
    "CalSet_20985.raw":  "Gain",
    "CalSet_30868.raw":  "Gain",
    "CalSet_42677.raw":  "Gain"
  },
  "result_files": [
    "*_result.raw"
  ],
  "verification_targets": {
    "psnr_db_min": 40.0,
    "flat_residual_pct_max": 1.0
  }
}
```

---

### 5.3 BPM Generation Panel (F2-01 ~ F2-04)

#### 5.3.1 Panel Location

New sub-tab **"BPM Generator"** added inside the existing **Diagnostics tab** (alongside the existing "Preprocessing Fixture Test" section), at `MainWindow.xaml:367+`.

XAML anchor:
```xaml
<TabItem Header="BPM Generator" AutomationProperties.AutomationId="BpmGeneratorTab">
```

#### 5.3.2 Panel Layout Specification

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  BPM Generator  (AutomationId: BpmGeneratorPanel)                          │
├──────────────────────────┬──────────────────────────────────────────────────┤
│  INPUT FRAMES (330px)    │  CONFIGURATION                                   │
│                          │                                                  │
│  [Dark Frames]           │  Algorithm Preset:                               │
│  ┌────────────────────┐  │  ○ RMM (λ=8.0, 32×32, 128×128, 7%)  [default]  │
│  │ ListBox:           │  │  ○ Blue (σ=5, 32×32, 128×128, 7%)               │
│  │  dark_001.raw      │  │  ○ MC (256×7, 60×60, 15%)  [legacy]            │
│  │  dark_002.raw      │  │  ○ Custom                                       │
│  └────────────────────┘  │                                                  │
│  [Add Dark...] [Clear]   │  ── Dark Detection ──────────────────────────── │
│                          │  Window size: [32 ▲▼] px  (min 32)             │
│  [Bright Frames]         │  Lambda (λ): [8.0] (RMM) / Sigma k: [5] (Blue) │
│  ┌────────────────────┐  │                                                  │
│  │ ListBox:           │  │  ── Bright Detection ───────────────────────── │
│  │  bright01.raw      │  │  Window size: [128 ▲▼] px  (min 128)           │
│  │  bright02.raw      │  │  Tolerance: [7.0 ▲▼] %  (range 5~9)           │
│  └────────────────────┘  │                                                  │
│  [Add Bright...] [Clear] │  ── Frame Requirements ─────────────────────── │
│                          │  Min dark frames: [5] (have: N)                 │
│  Detector dimensions:    │  Min bright frames: [10] (have: N)              │
│  [Auto ▼]  [3072×3072]  │                                                  │
│                          │  [Generate BPM]  [Save as XCal...]              │
│                          │                                                  │
├──────────────────────────┼──────────────────────────────────────────────────┤
│  RESULTS (full width)    │                                                  │
│  Dark bad pixels:   32   │  ████████████░░░░░░░ 65% [progress bar]        │
│  Bright bad pixels: 798  │                                                  │
│  Total unique:      820  │  Density: 0.0087%  ✓ Within limit (< 5%)       │
│                          │  Lambda σ evidence: mean±k×σ range displayed    │
│  [Compare vs Oracle]     │  [View Defect Overlay]  [Export Report]         │
└──────────────────────────┴──────────────────────────────────────────────────┘
```

#### 5.3.3 Control Inventory

| Control | x:Name | AutomationId | Type | Enabled Condition |
|---------|--------|-------------|------|-------------------|
| Dark frames list | `DarkFramesListBox` | `BpmDarkFramesList` | `ListBox` | Always |
| Add dark frames | `BpmAddDarkButton` | `BpmAddDark` | `Button` | Always |
| Bright frames list | `BrightFramesListBox` | `BpmBrightFramesList` | `ListBox` | Always |
| Add bright frames | `BpmAddBrightButton` | `BpmAddBright` | `Button` | Always |
| Algorithm preset | `BpmAlgorithmPreset` | `BpmAlgoPreset` | `RadioButton` × 4 | Always |
| Dark window size | `BpmDarkWindowSize` | `BpmDarkWindow` | `IntegerUpDown` | Custom preset |
| Lambda / sigma | `BpmLambdaSigma` | `BpmLambda` | `NumericUpDown` | Custom preset |
| Bright window size | `BpmBrightWindowSize` | `BpmBrightWindow` | `IntegerUpDown` | Custom preset |
| Bright tolerance % | `BpmTolerancePct` | `BpmTolerance` | `NumericUpDown` | Custom preset |
| Generate BPM | `BpmGenerateButton` | `BpmGenerate` | `Button` | DarkFrames≥1 AND BrightFrames≥1 AND preprocess R3+ |
| Save as XCal | `BpmSaveButton` | `BpmSave` | `Button` | GenerationComplete |
| Progress bar | `BpmProgressBar` | `BpmProgress` | `ProgressBar` | Visible during generation |
| Dark bad pixel count | `BpmDarkCountLabel` | `BpmDarkCount` | `TextBlock` | After generation |
| Bright bad pixel count | `BpmBrightCountLabel` | `BpmBrightCount` | `TextBlock` | After generation |
| Total unique count | `BpmTotalCountLabel` | `BpmTotalCount` | `TextBlock` | After generation |
| Density label | `BpmDensityLabel` | `BpmDensity` | `TextBlock` | After generation |
| Density gate status | `BpmDensityGateLabel` | `BpmDensityGate` | `TextBlock` (colored) | After generation |
| Compare vs oracle | `BpmCompareOracleButton` | `BpmCompareOracle` | `Button` | GenerationComplete AND OracleBpmPresent |

#### 5.3.4 Algorithm Preset → XpeBpmConfig Mapping

```csharp
// Models/BpmConfigPreset.cs (new)
public enum BpmAlgorithmPreset { RMM, Blue, MC_Legacy, Custom }

public static XpeBpmConfig FromPreset(BpmAlgorithmPreset preset) => preset switch
{
    BpmAlgorithmPreset.RMM      => new XpeBpmConfig(lambda_dark: 8.0f, mask_dark: 32,
                                       tolerance_pct: 0.07f, mask_bright: 128,
                                       min_frames_dark: 5, min_frames_bright: 10),
    BpmAlgorithmPreset.Blue     => new XpeBpmConfig(lambda_dark: 5.0f, mask_dark: 32,
                                       tolerance_pct: 0.07f, mask_bright: 128,
                                       min_frames_dark: 5, min_frames_bright: 10),
    BpmAlgorithmPreset.MC_Legacy => new XpeBpmConfig(lambda_dark: 0.0f,  // uses scale=0.3
                                       mask_dark: 256, /* ignored: asymmetric */
                                       tolerance_pct: 0.15f, mask_bright: 60,
                                       min_frames_dark: 1, min_frames_bright: 1),
    BpmAlgorithmPreset.Custom   => throw new InvalidOperationException("Read from UI controls"),
    _                           => throw new ArgumentOutOfRangeException()
};
```

**Note:** MC_Legacy uses the `mask_dark=256` flag as a sentinel; the native `xpe_bpm_generate` implementation shall recognize this value and apply the original 256×7 + 1×45 two-stage algorithm for regression comparison purposes.

#### 5.3.5 P/Invoke Binding (F2-02)

Add to `NativePreprocessPreviewService.cs`:

```csharp
// --- P/Invoke: BPM Generation (SAD §3.3.1, SWU-1.11) ---

[StructLayout(LayoutKind.Sequential)]
public struct XpeBpmConfig
{
    public float    lambda_dark;       // RMM lambda   (default 8.0)
    public uint     mask_size_dark;    // window side   (default 32, min 32)
    public float    tolerance_pct;     // bright tolerance (default 0.07)
    public uint     mask_size_bright;  // window side   (default 128, min 128)
    public uint     min_frames_dark;   // min dark frames (default 5)
    public uint     min_frames_bright; // min bright frames (default 10, target 15~20)
}

[UnmanagedFunctionPointer(CallingConvention.Cdecl)]
private delegate XpeErrorCode BpmGenerateDelegate(
    IntPtr dark_frames,   // const uint16_t* [N_dark × W × H], row-major
    uint   n_dark,
    IntPtr bright_frames, // const uint16_t* [N_bright × W × H], row-major
    uint   n_bright,
    uint   width,
    uint   height,
    ref XpeBpmConfig cfg,
    IntPtr bpm_out        // uint8_t* [W × H], caller-allocated
);

private BpmGenerateDelegate? _bpmGenerate;

// Resolve in LoadNativeFunctions():
//   _bpmGenerate = GetDelegate<BpmGenerateDelegate>("xpe_bpm_generate");
//   Null-safe: absent export → BPM generation disabled, not a hard error
```

**Memory management contract:**
- `dark_frames` and `bright_frames` are caller-allocated, pinned `GCHandle` during call
- `bpm_out` is caller-allocated `byte[]`, `width × height` bytes, freed by caller
- Frame layout: frame 0 occupies bytes [0, W×H×2), frame 1 starts at W×H×2, etc.

```csharp
// Usage pattern in BpmGenerationService.cs (new)
public async Task<BpmGenerationResult> GenerateAsync(
    IReadOnlyList<string> darkFramePaths,
    IReadOnlyList<string> brightFramePaths,
    XpeBpmConfig config,
    IProgress<double> progress,
    CancellationToken ct)
{
    // 1. Load and validate all frames (dimensions must match)
    // 2. Allocate contiguous native buffers for stacked frames
    // 3. Pin buffers via GCHandle.Alloc(buf, GCHandleType.Pinned)
    // 4. Call _bpmGenerate with pinned pointers
    // 5. Unpin regardless of error (try/finally)
    // 6. Parse bpm_out into BpmGenerationResult (dark count, bright count, density)
}
```

#### 5.3.6 Defect Overlay Visualization

After generation, the BPM result is overlaid on the first input frame in the comparison viewer:

- Bad pixels rendered as **red dots** at original position
- Overlay opacity: 70%
- Zoom and pan synchronized with the existing comparison viewer
- Toggle: `BpmDefectOverlayToggle` checkbox controls overlay visibility
- Color coding: dark-only bad pixels (red), bright-only (orange), both (magenta)

---

### 5.4 Algorithm Comparison Panel (F2-05)

#### 5.4.1 Panel Location

New sub-tab **"Algorithm Compare"** added alongside **"BPM Generator"** in the Diagnostics tab.

```xaml
<TabItem Header="Algorithm Compare" AutomationProperties.AutomationId="AlgoCompareTab">
```

#### 5.4.2 Layout Specification

```
┌────────────────────────────────────────────────────────────────────────────┐
│  Algorithm Comparison  (Input: shared from BPM Generator panel)           │
│                                                                            │
│  Input frames: [use current BPM Generator selection]  [Refresh]           │
│  [Compare MC / Blue / RMM]                    ← triggers 3 parallel runs  │
├───────────────────┬────────────────────┬───────────────────────────────────┤
│  MC (Legacy)      │  Blue Algorithm    │  RMM (λ=8.0)  ← Recommended     │
│  [image preview]  │  [image preview]   │  [image preview]                 │
├───────────────────┴────────────────────┴───────────────────────────────────┤
│  Comparison Table (DataGrid)                                               │
│  Metric          │  MC         │  Blue       │  RMM       │  Winner       │
│  ─────────────── │ ─────────── │ ─────────── │ ────────── │ ──────────── │
│  Dark BP count   │  1          │  32         │  28        │  (n/a)       │
│  Bright BP count │  499        │  798        │  720       │  (n/a)       │
│  Total density % │  0.0005%    │  0.0087%    │  0.0080%   │  (n/a)       │
│  LineArtifactScore│ 15.2%      │  4.8%       │  4.2%      │  ✓ RMM       │
│  DefectRecall %  │  45.3%      │  88.7%      │  92.1%     │  ✓ RMM       │
│  Generation (ms) │  120        │  340        │  380       │  MC fastest  │
└────────────────────────────────────────────────────────────────────────────┘
```

#### 5.4.3 Comparison Run Sequence

The three algorithms run **sequentially** (not parallel) to avoid memory pressure with large 3072×3072 frames:

```
1. Run MC_Legacy config  → store BpmGenerationResult_MC
2. Run Blue config       → store BpmGenerationResult_Blue  
3. Run RMM config        → store BpmGenerationResult_RMM
4. Compute LineArtifactScore for each (C# FFT on corrected output)
5. Populate ComparisonDataGrid
6. Highlight winner row in green
```

**Progress:** Three-segment progress bar (each segment = 33%)

#### 5.4.4 Winner Determination

```csharp
// Winner = algorithm with lowest LineArtifactScore among results with DefectRecall >= 85%
// If all DefectRecall < 85%: winner = highest DefectRecall (quality-first)
// Winner column shows: "✓ {AlgorithmName}" in green
```

---

### 5.5 SWU-1.11 Registration in Algorithm Catalog (F2-06)

Add to `AlgorithmValidationCatalogService.cs:Definitions` array:

```csharp
new("xpe_preprocess", "SWU-1.11", "BpmGenerator",
    "SRS-CALIB-FUNC-022, SRS-CALIB-FUNC-023, SRS-CALIB-FUNC-024, SRS-CALIB-FUNC-025",
    "UT-BPM-001, UT-BPM-002, UT-BPM-003, UT-BPM-004, IT-BPM-001, IT-BPM-002",
    "PRE-BPM-E2E",
    "bpm-generator",  // new adapter type
    null,             // no single stage key; uses dedicated panel
    "Open the BPM Generator tab, add dark frames (≥5) and bright frames (≥10) from CalData_6 or Grid_abnormal fixture, then run Generate BPM and verify density < 5% and LineArtifactScore < 10%."),
```

New adapter type `"bpm-generator"` treated as `canRun = true` when `xpe_bpm_generate` export is resolved (xpe_preprocess R3+).

---

## 6. Stage Verification Procedures (QA Playbook)

### 6.1 PRE-QA-001: Offset Correction Verification

**Prerequisite:** Fixture case with `dark.raw` (CalibrationRole.Offset) and at least one target `.raw`

| Step | Action | Expected Result |
|------|--------|----------------|
| 1 | Load fixture case with dark.raw | CalibrationFilesListBox shows "dark.raw [Offset]" |
| 2 | Load target raw image | RawPreviewCanvas shows image |
| 3 | Set Offset stage to **Off** | Apply button active |
| 4 | Click Apply Calibration | Output = input (no change); DarkBias shown > 0 |
| 5 | Set Offset stage to **On** | |
| 6 | Click Apply Calibration | DarkBias ≤ 5 ADU; DarkReduction_dB ≥ 10 dB |
| 7 | Set Offset stage to **Auto** | Same result as On (dark.raw present) |
| 8 | Record verdict via Evaluation tab | Evidence report captures bypass decisions |
| **Gate** | DarkBias ≤ 5 ADU AND DarkReduction_dB ≥ 10 dB | **PASS** |

### 6.2 PRE-QA-002: Gain Correction Verification

**Prerequisite:** CalData_6 fixture (dark.raw + bright01~06.raw)

| Step | Action | Expected Result |
|------|--------|----------------|
| 1 | Load CalData_6 fixture | 6 Gain files + 1 Offset file shown |
| 2 | Load bright06.raw as target | Preview shows flat-field |
| 3 | Offset **On**, Gain **On**, Defect **Off** | |
| 4 | Apply Calibration | FlatResidualPct ≤ 1.0%; PRNU_CV ≤ 1% |
| 5 | Verify gain file count = 6 | Meets FUNC-024 min_frames_bright ≥ 5 |
| **Gate** | FlatResidualPct ≤ 1.0% AND PRNU_CV ≤ 1% | **PASS** |

### 6.3 PRE-QA-003: BPM Generation Verification (FUNC-022~025)

**Prerequisite:** CalData_6 fixture loaded; xpe_bpm_generate resolved

| Step | Action | Expected Result |
|------|--------|----------------|
| 1 | Open BPM Generator tab | Panel enabled |
| 2 | Add dark.raw (1 file) | Dark frame count = 1; warning if < 5 |
| 3 | Add bright01~06.raw (6 files) | Bright frame count = 6 |
| 4 | Select **RMM** preset | lambda=8.0, mask_dark=32, mask_bright=128, tol=7% |
| 5 | Click Generate BPM | Progress bar animates |
| 6 | Verify dark bad pixel count | 10~100 (typical for clean detector) |
| 7 | Verify total density | < 5% → gate ✓ |
| 8 | Click Compare vs Oracle (BPMap.map as oracle) | DefectRecall ≥ 95% |
| **Gate** | Density < 5% AND DefectRecall ≥ 95% | **PASS** (FUNC-022, 023, 024) |

### 6.4 PRE-QA-004: Grid Artifact Robustness (FUNC-025)

**Prerequisite:** Grid_abnormal fixture loaded in BPM Generator

| Step | Action | Expected Result |
|------|--------|----------------|
| 1 | Load Grid_abnormal: MasterDark + MasterBright | Shown in frame lists |
| 2 | Run **Algorithm Compare** (MC / Blue / RMM) | Three results computed |
| 3 | Verify LineArtifactScore for RMM | ≤ 10% (gate), target ≤ 5% |
| 4 | Verify LineArtifactScore for Blue | ≤ 10% (gate), target ≤ 5% |
| 5 | Verify LineArtifactScore for MC | May exceed 10% (expected; legacy) |
| 6 | Confirm RMM or Blue wins | Winner highlighted in comparison table |
| **Gate** | RMM LineArtifactScore ≤ 10% | **PASS** (FUNC-025) |

### 6.5 PRE-QA-005: Defect Correction + Metrics

**Prerequisite:** Any fixture with BPM (CalibrationRole.Defect) file

| Step | Action | Expected Result |
|------|--------|----------------|
| 1 | Load fixture with defect.raw/bpm.raw | Defect file listed |
| 2 | Offset **On**, Gain **On**, Defect **On** | Full pipeline |
| 3 | Apply Calibration | DefectResidualADU ≤ 2 ADU |
| 4 | Set Defect to **Off**, rerun | DefectResidualADU increases; changed pixels = 0 |
| 5 | Toggle Defect **On** again | DefectResidualADU returns to ≤ 2 ADU |
| **Gate** | DefectResidualADU ≤ 2 ADU when Defect On | **PASS** |

---

## 7. Error Injection Scenarios

### 7.1 Corrupted Calibration File (SRS-CALIB-SAFE-003)

| Step | Action | Expected |
|------|--------|----------|
| 1 | Copy a valid xcal file, flip one byte in CRC region | Corrupted file |
| 2 | Load corrupted file as offset calibration | Error dialog: "XPE_ERR_IO_FAILED" |
| 3 | Apply Calibration button | Remains disabled OR returns error |
| 4 | Evidence report | Records "CRC validation failed" event |
| **Gate** | Pipeline does not proceed with corrupted calibration | **PASS** |

### 7.2 Expired Calibration File (SRS-CALIB-SAFE-002)

| Step | Action | Expected |
|------|--------|----------|
| 1 | Generate xcal with `expiryEpochMs = now - 1 day` | Expired file |
| 2 | Load as gain calibration | Expiry warning shown in Calibration Loads grid |
| 3 | Apply Calibration | Error: "XPE_ERR_CALIBRATION_EXPIRED" |
| 4 | Evidence report | Records expiry timestamp and bypass outcome |
| **Gate** | Pipeline blocked; expiry date shown in Calibration Loads grid | **PASS** |

### 7.3 Insufficient BPM Generation Frames

| Step | Action | Expected |
|------|--------|----------|
| 1 | Open BPM Generator, add 1 dark + 2 bright frames | Frame counters show "1 (need ≥ 5)" warning |
| 2 | Click Generate BPM | Proceeds with warning badge |
| 3 | Verify warning badge in results | "Frame count below recommendation (FUNC-024)" |
| **Gate** | Warning shown; generation not blocked below minimum 1 frame | **PASS** |

### 7.4 BPM Density Exceeds 5% Gate (SRS-CALIB-FUNC-003)

| Step | Action | Expected |
|------|--------|----------|
| 1 | Use test fixture with known high-defect detector | |
| 2 | Generate BPM | Density > 5% |
| 3 | Verify gate status | Red "✗ Exceeds 5% limit" label |
| 4 | Evidence report | Records density and gate failure |
| **Gate** | Visual gate failure shown; save still permitted with user confirmation | **PASS** |

---

## 8. P/Invoke ABI Requirements Summary

### 8.1 Phase 1 — No New Exports Required

Detector-domain metrics (§5.1) computed entirely in C# using data already returned by existing `OffsetCorrectionDelegate`, `GainCorrectionDelegate`, `DefectCorrectionDelegate`.

### 8.2 Phase 2 — New Export

| Export Name | Header | Source File (SAD) |
|-------------|--------|-------------------|
| `xpe_bpm_generate` | `modules/preprocess/include/xpe_defect_gen.h` | `modules/preprocess/src/xpe_defect_gen.cpp` |

C# binding: see §5.3.5.

**Resilience:** If `xpe_bpm_generate` is not present (DLL predates Phase 2), the BPM Generator tab displays "BPM generation not available in this build" and remains read-only. The algorithm catalog shows SWU-1.11 status as "R2 — export pending".

### 8.3 Metrics Computation Exports (Optional — Phase 2 Enhancement)

If native-side metric computation is preferred for performance on large images, the following exports may be added in a later sprint:

| Export | Purpose | C# Delegate Signature |
|--------|---------|----------------------|
| `xpe_compute_dark_metrics` | DarkBias, DSNU, DarkReduction_dB | `XpeErrorCode(ref XpeImageBuffer, ref XpeDarkMetrics)` |
| `xpe_compute_flat_metrics` | PRNU_CV, FlatResidualPct, LineArtifactScore | `XpeErrorCode(ref XpeImageBuffer, ref XpeFlatMetrics)` |
| `xpe_compute_defect_metrics` | DefectRecall, DefectFPR, ResidualADU | `XpeErrorCode(ref XpeImageBuffer, IntPtr oracleBpm, ref XpeDefectMetrics)` |

These are not required for Phase 1/2; C# implementations in `MetricsComputationService.cs` suffice.

---

## 9. Implementation Mapping

### 9.1 New Files

| File | Package | Purpose |
|------|---------|---------|
| `Services/MetricsComputationService.cs` | Services | Detector-domain metrics computation (§5.1) |
| `Services/BpmGenerationService.cs` | Services | BPM generation orchestration (§5.3.5) |
| `Models/BpmGenerationResult.cs` | Models | BPM generation output data |
| `Models/BpmConfigPreset.cs` | Models | Algorithm preset → XpeBpmConfig mapping |
| `Models/MetricRow.cs` | Models | Row model for metrics DataGrid |
| `ViewModels/MetricsViewModel.cs` | ViewModels | Observable properties for Metrics tab |
| `ViewModels/BpmGeneratorViewModel.cs` | ViewModels | Observable properties for BPM panel |

### 9.2 Modified Files

| File | Change |
|------|--------|
| `Services/FixtureCatalogService.cs` | Extend `LoadCases()`, `InferCalibrationRole()` (§5.2.1~5.2.2) |
| `Services/AlgorithmValidationCatalogService.cs` | Add SWU-1.11 entry (§5.5) |
| `Services/NativePreprocessPreviewService.cs` | Add `BpmGenerateDelegate` + `XpeBpmConfig` struct (§5.3.5) |
| `MainWindow.xaml` | Add BPM Generator sub-tab, Algorithm Compare sub-tab, Metrics grids (§5.1.3, §5.3.2, §5.4.2) |
| `MainWindow.xaml.cs` | Wire new tab events, call MetricsComputationService, bind ViewModels |

### 9.3 Test Data Files (fixture.json)

| File | Purpose |
|------|---------|
| `tests/test_data/CalData_6/fixture.json` | CalData_6 fixture manifest (§5.2.3) |
| `tests/test_data/Grid_abnormal/fixture.json` | Grid_abnormal fixture manifest (§5.2.4) |
| `tests/test_data/cyan_test/fixture.json` | cyan_test fixture manifest (§5.2.5) |

---

## 10. Acceptance Criteria

### 10.1 Phase 1 Acceptance

| ID | Criterion | Measurement |
|----|-----------|-------------|
| AC-P1-01 | DarkBias displayed in Metrics tab after offset correction | Visual inspection + report JSON |
| AC-P1-02 | DSNU_ADU displayed | Visual inspection |
| AC-P1-03 | FlatResidualPct ≤ 1.0% shown and gated in Metrics tab | CalData_6 fixture, value + color |
| AC-P1-04 | LineArtifactScore computed and displayed | Grid_abnormal fixture, value + gate color |
| AC-P1-05 | CalData_6 appears in FixtureCaseComboBox | GUI startup check |
| AC-P1-06 | Grid_abnormal appears in FixtureCaseComboBox | GUI startup check |
| AC-P1-07 | cyan_test appears in FixtureCaseComboBox | GUI startup check |
| AC-P1-08 | `fixture.json` roles override filename inference | CalData_6: bright01 → Gain |
| AC-P1-09 | DefectOracle BPM role triggers oracle-mode metrics | DefectRecall shown when oracle present |

### 10.2 Phase 2 Acceptance

| ID | Criterion | Measurement |
|----|-----------|-------------|
| AC-P2-01 | BPM Generator tab visible when xpe_preprocess R3+ | Startup check |
| AC-P2-02 | Generate BPM runs with CalData_6 dark+bright | dark=1, bright=6; result density < 5% |
| AC-P2-03 | RMM preset produces BPM with density < 5% on CalData_6 | Numeric value |
| AC-P2-04 | Algorithm Compare: all 3 algorithms complete | 3 result columns populated |
| AC-P2-05 | LineArtifactScore: RMM ≤ 10% on Grid_abnormal | Numeric value |
| AC-P2-06 | Algorithm Compare winner = RMM or Blue on Grid_abnormal | Winner row green |
| AC-P2-07 | DefectRecall for RMM vs CalData_6 oracle BPM ≥ 80% | BPMap.map as oracle |
| AC-P2-08 | SWU-1.11 appears as "Runnable" in Algorithm Catalog | Catalog tab |
| AC-P2-09 | BPM saved as valid XCal with CRC32 | Load back and validate |
| AC-P2-10 | Evidence report includes BPM generation parameters | JSON output inspection |

### 10.3 IEC 62304 Evidence Requirements

Per SRS-CALIB-SAFE-004, the evidence report shall include for every test run:

```json
{
  "run_id": "uuid",
  "fixture_case": "CalData_6",
  "timestamp": "ISO-8601",
  "stage_decisions": [
    { "stage": "offset", "mode": "On", "bypass": false },
    { "stage": "gain",   "mode": "On", "bypass": false },
    { "stage": "defect", "mode": "Off", "bypass": true, "reason": "QA evaluation mode" }
  ],
  "bpm_generation": {
    "algorithm": "RMM",
    "config": { "lambda_dark": 8.0, "mask_dark": 32, "mask_bright": 128, "tolerance_pct": 0.07 },
    "dark_frame_count": 1,
    "bright_frame_count": 6,
    "dark_bad_pixels": 28,
    "bright_bad_pixels": 720,
    "total_density_pct": 0.0079,
    "gate_density_pass": true
  },
  "metrics": {
    "DarkBias": 2.1, "DSNU_ADU": 8.3,
    "FlatResidualPct": 0.42, "LineArtifactScore": 4.2,
    "DefectRecall": 92.1, "DefectFPR": 0.00021
  }
}
```

---

## 11. Traceability

| New Feature | SRS Requirement | SAD Unit | Test Case |
|-------------|----------------|----------|-----------|
| F1-01: DarkBias, DSNU | SRS-CALIB-FUNC-016 | SWU-1.1 | PRE-QA-001, AC-P1-01~02 |
| F1-02: FlatResidualPct, LineArtifactScore | SRS-CALIB-FUNC-017, FUNC-025 | SWU-1.2, 1.11 | PRE-QA-002, PRE-QA-004 |
| F1-03: DefectRecall, DefectFPR | SRS-CALIB-FUNC-019 | SWU-1.3 | PRE-QA-005 |
| F1-04: Fixture CalData_6 | SRS-CALIB-FUNC-024 (frame count) | SWU-1.11 | AC-P1-05 |
| F1-05: Fixture Grid_abnormal | SRS-CALIB-FUNC-025 (grid robust) | SWU-1.11 | AC-P1-06 |
| F2-01: BPM Generator Panel | SRS-CALIB-FUNC-022, 023 | SWU-1.11 | PRE-QA-003 |
| F2-02: xpe_bpm_generate binding | SAD §3.3.1 | SWU-1.11 | AC-P2-02 |
| F2-03: BpmConfig controls | SRS-CALIB-FUNC-022~024 | SWU-1.11 | AC-P2-03, 07 |
| F2-04: BPM defect overlay | SRS-CALIB-FUNC-019 (visual evidence) | SWU-1.3, 1.11 | AC-P2-09 |
| F2-05: Algorithm Comparison | SRS-CALIB-FUNC-025 | SWU-1.11 | PRE-QA-004, AC-P2-04~06 |
| F2-06: SWU-1.11 in catalog | SRS-CALIB-FUNC-022~025 | SWU-1.11 | AC-P2-08 |

---

## 12. Revision History

| Version | Date | Changes |
|---------|------|---------|
| 1.0 | 2026-04-19 | Initial release — Phase 1~3 specification, BPM Generator, Algorithm Comparison, Fixture manifests |

---

**Document End**

*Authored: 2026-04-19 | Classification: QA/Development Use | Not for Clinical Deployment*
