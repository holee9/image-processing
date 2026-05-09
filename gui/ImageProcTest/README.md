# ImageProcTest GUI

`ImageProcTest` is the GUI-first WPF shell for the XPE program. The current implementation covers GUI-S0 plus the Phase 1b display integration shell:

- raw binary image viewer only
- `IXpeBackend` mock and native display backend contracts
- `RealXpeBackend` P/Invoke wrapper for `xpe_common.dll` and `xpe_display.dll`
- display pipeline command path: Modality LUT -> VOI LUT -> Presentation LUT
- one-click calibration evaluation radio controls for preprocessing stages using `Off`, `On`, and `Auto`
- display settings panel for VOI mode, window center/width, body-part preset, GSDF flag, and modality rescale (defaults tuned for 16-bit flat-panel DR: C=32768, W=65535, intercept=0)
- source-vs-processed comparison viewport with swipe, split, overlay, difference, zoom, pan, and optional detached viewer
- settings UI, log panel, alert panel
- offline packaged Help window with quick-start and scope pages
- top-level menu bar: File, Backend, View, Pipeline, Tools, Help
- resizable diagnostics layout for Logs and Alerts
- no real DICOM parsing
- native display backend is enabled only when required DLL exports match the Phase 1b ABI; otherwise the app safely falls back to Mock

## Build

```powershell
dotnet build gui\ImageProcTest\ImageProcTest.csproj -c Debug
```

## Self-check

```powershell
dotnet run --project gui\ImageProcTest.SelfCheck\ImageProcTest.SelfCheck.csproj -c Debug
```

The self-check validates the precreated fixture pack under `gui/ImageProcTest/fixtures/gui-s0/`:

- `fixture-manifest.json`
- `appsettings.template.json`
- `raw/wrist_lat_3072x3072.raw`
- `calibration/offset|gain|defect/`
- settings save/load round-trip
- raw fixture SHA-256 integrity
- mock backend version plus expected log/alert counts
- wrist lateral 3072x3072 raw image loading and preview creation
- display settings defaults: `voiWindowCenter=32768`, `voiWindowWidth=65535`, `modalityRescaleIntercept=0` (flat-panel DR 16-bit range)
- calibration stage mode defaults: Offset/Gain/Defect/Ghost/Temperature/Nonlinearity/Binning all start as `Auto`
- mock display pipeline application
- comparison viewport defaults, source preservation, and processed preview separation
- VOI body-part preset values (flat-panel DR: Bone C=40000/W=30000, Lung C=25000/W=50000, Abdomen C=32768/W=65535, Head C=35000/W=40000)

## E2E

```powershell
dotnet build gui\ImageProcTest.E2E\ImageProcTest.E2E.csproj -c Debug
.\gui\ImageProcTest.E2E\bin\Debug\net8.0-windows\ImageProcTest.E2E.exe
```

The E2E runner:

- executes the self-check first
- launches `ImageProcTest.exe`
- opens the packaged quick-start Help page
- verifies the main window and required controls
- verifies mock version text and initial log/alert population
- verifies the display settings panel and display version text
- verifies calibration evaluation `Off`/`On`/`Auto` radio groups and live summary updates
- verifies `Apply Body Part Preset` and `Apply Display Pipeline` command wiring
- verifies comparison mode controls, zoom commands, and viewport presence
- verifies menu/toolbar parity and resizable diagnostics splitters
- clicks `Clear Logs` and `Clear Alerts`
- verifies both lists are emptied
- closes the window cleanly

## Real-click E2E

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\e2e\Invoke-ImageProcTestGuiRealE2E.ps1
```

This variant first prepares a runtime fixture from the precreated project assets:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\e2e\Prepare-ImageProcTestFixture.ps1
```

The prep step:

- validates the raw fixture hash against `fixture-manifest.json`
- copies `fixtures/gui-s0/` into the built output directory
- materializes runtime `appsettings.json` from `appsettings.template.json`
- creates empty calibration directories in the runtime fixture tree

The E2E step then launches the actual desktop app in built-in automation mode, loads the prepared raw fixture from the runtime fixture tree, applies the display preset and display pipeline, saves settings, clears logs and alerts, and verifies the emitted `automation-report.json`.

The emitted report includes:

- `DisplayPipelineApplied`
- `DisplayPipelineSummary`
- `DisplayPanelVisible`
- `DisplayVersion`
- `CalibrationEvaluationSummary`
- `VoiPresetApplied`
- `ResizableDiagnosticsLayoutDetected`
- `ComparisonViewportDetected`
- `ComparisonSourcePreserved`
- `ComparisonEvidenceExported`

## Automation E2E with actual detector raw data

Run end-to-end verification against a real flat-panel detector raw image:

```powershell
dotnet run --project gui\ImageProcTest\ImageProcTest.csproj -c Debug -- `
  --automation-raw "tests\test_data\cyan_test\Bright_Chest phantom_75kv_320ma_25.6(SID_110)_00.raw" `
  --automation-width 3072 --automation-height 3072 `
  --automation-report gui\ImageProcTest\bin\Debug\net8.0-windows\e2e-test-report.json
```

Expected result: `Passed=true`, `ActiveImageSummary` starting with `RAW 3072x3072`, `VOI(Linear, C=32768, W=65535)`.

> **Note**: Default VOI values are calibrated for flat-panel DR detectors (16-bit raw, no CT HU offset). Using CT-centric defaults (C=40, W=400, intercept=-1024) causes the entire image to clip to white because the raw pixel range (15000–65535) lies entirely above the VOI upper bound (240 HU).

## Large-image comparison check

The built-in automation mode can validate the 4096x4096 UInt16 comfort envelope without adding large binary fixtures to git:

```powershell
.\gui\ImageProcTest\bin\Debug\net8.0-windows\ImageProcTest.exe `
  --automation-raw "$env:TEMP\imageproc_synthetic_4096x4096.raw" `
  --automation-report "$env:TEMP\imageproc_4096_automation_report.json" `
  --automation-width 4096 `
  --automation-height 4096
```

The report must show `Passed=true`, `ActiveImageSummary` beginning with `RAW 4096x4096`, and `ComparisonSourcePreserved=true`.

## Native display backend

Set `backendMode` to `Native` and place compatible `xpe_common.dll` and `xpe_display.dll` next to `ImageProcTest.exe`.

The factory switches to `RealXpeBackend` only when these exports are present:

- `xpe_common.dll`: `xpe_alloc_image`, `xpe_free_image`
- `xpe_display.dll`: `xpe_display_version`, `xpe_apply_modality_lut`, `xpe_apply_voi_lut`, `xpe_voi_preset_create`, `xpe_apply_presentation_lut`, `xpe_gsdf_calibrate`

If any required export is missing, the app keeps running in Mock mode and reports the DLL detection state in the Runtime panel. This prevents a stale DLL from crashing the GUI during development.

## Help

The packaged help bundle lives under `help/` next to the executable.

- `index.html`
- `quick-start.html`
- `scope.html`

Use the top-level `Help` menu in the app to open these pages offline.

## Menu model

The GUI-S0 menu bar follows the project command taxonomy:

- `File`: raw loading, settings persistence, automation report export, exit
- `Backend`: mock backend lifecycle, diagnostics, future native backend commands
- `View`: panel visibility and layout reset
- `Pipeline`: disabled placeholders for future processing commands
- `Tools`: calibration, fixture, evidence, and future QA commands
- `Help`: offline quick-start, scope, and future generated reference entry points

Unsupported native, DICOM, premium, and AI commands are disabled until their owner modules exist.

## appsettings.json schema

`appsettings.json` is stored next to the executable. Current keys:

- `backendMode`
- `rawWidth`
- `rawHeight`
- `rawPixelFormat`
- `calibOffsetDir`
- `calibGainDir`
- `calibDefectDir`
- `calibOffsetMode`
- `calibGainMode`
- `calibDefectMode`
- `calibGhostMode`
- `calibTemperatureMode`
- `calibNonlinearityMode`
- `calibBinningMode`
- `lastRawDir`
- `voiWindowCenter`
- `voiWindowWidth`
- `voiLutMode`
- `selectedBodyPart`
- `gsdfEnabled`
- `modalityRescaleSlope`
- `modalityRescaleIntercept`
- `showDisplayPanel`
- `comparisonMode`
- `comparisonZoomScale`
- `comparisonPanX`
- `comparisonPanY`
- `comparisonSwipePosition`
- `comparisonOverlayOpacity`

## PipelineOrchestrator

`PipelineOrchestrator` loads the Phase 1 native DLLs (`xpe_preprocess.dll`, `xpe_enhance_basic.dll`, `xpe_display.dll`, `xpe_dicom.dll`) via P/Invoke and reports the result through `PipelineLoadResult`:

- `LoadedDlls` — DLLs found and export-verified successfully
- `MissingDlls` — DLLs not found on disk (module directory absent or file missing)
- `FailedDlls` — DLLs found but with one or more missing required exports

**Degraded mode**: callers inspect `MissingDlls` and `FailedDlls` to decide whether to fall back to Mock backend. When the module directory does not exist, all four Phase 1 DLL names are added to `MissingDlls` before the early return so callers can detect the degraded state correctly.

## Scope boundary

- Real DICOM read/write remains owned by `xpe_dicom.dll` in Phase 1b.
- Calibration `Off`/`On`/`Auto` controls are evaluation-only Test GUI controls. Product-mode mandatory offset/gain policy remains owned by `xpe_preprocess.dll`.
- SWU-3.4 LUT Manager remains deferred; the GUI uses the four native quick presets from `xpe_voi_preset_create` only for Phase 1b display-pipeline validation.
- The final DR preset library must not be limited to Bone/Lung/Abdomen/Head. It is tracked as a body-part/view-aware SWU-3.4 backlog item keyed by body part, view/projection, laterality when applicable, patient group, and display intent.
- GSDF defaults to off until DICOM PS3.14 validation vectors are accepted.
- Tile-backed rendering for images larger than 4096x4096 remains a planned extension before larger-size release claims.
