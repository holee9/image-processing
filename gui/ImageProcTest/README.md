# ImageProcTest GUI

> **There are two WPF app projects named `ImageProcTest`.** This one (`gui/ImageProcTest`) is the
> UI the FlaUI suite (`clients/ImageProcTest.E2ETests`) launches; `clients/ImageProcTest` holds the
> native readiness diagnostics and backends. Check which one a path points at before editing.

`ImageProcTest` is the GUI-first WPF shell for the XPE program. The current implementation covers GUI-S0 plus the Phase 1b display integration shell:

- raw binary image viewer only
- `IXpeBackend` mock and native display backend contracts
- `RealXpeBackend` P/Invoke wrapper for `xpe_common.dll` and `xpe_display.dll`
- display pipeline command path: Modality LUT -> VOI LUT -> Presentation LUT
- one-click calibration evaluation radio controls for preprocessing stages using `Off`, `On`, and `Auto`
- display settings for VOI mode, window center/width, body-part preset, GSDF flag, and modality rescale, in the Analysis panel (defaults: `Models/AppSettings.cs`)
- source-vs-processed comparison viewport with swipe, split, overlay, difference, zoom, pan, and an optional detached viewer (opens since #166; what is checked about it: `clients/ImageProcTest.E2ETests/Scenarios/Smoke/DetachViewerScenarios.cs` and `.../Workflows/DetachedViewerSyncScenarios.cs`)
- settings UI, and a log region in the Analysis panel's Log tab, shown by **View → Show Logs** (off at start)
- offline packaged Help window with quick-start and scope pages
- top-level menu bar: File, Backend, View, Pipeline, Tools, Help
- no real DICOM parsing
- native display backend is enabled only when required DLL exports match the Phase 1b ABI; otherwise the app safely falls back to Mock

### Removed from the list above (GUI-C-76, #165)

This list used to advertise three things the app no longer has. They were removed rather than
reworded so nobody re-investigates whether they exist:

- **alert panel** — the Alerts panel was removed with the Evaluation Workbench layout (#165,
  GUI-C-65; see the comment at `MainWindow.xaml` near the View menu). Alerts are still collected
  and **Clear Alerts** still empties them, but no region of the window displays them.
- **resizable diagnostics layout for Logs and Alerts** — there is no `GridSplitter` anywhere in the
  app's XAML. The automation report used to claim one through `ResizableDiagnosticsLayoutDetected`,
  which was set to `true` unconditionally and held `Passed` green; it was removed in GUI-C-77.
- **Runtime panel** (was referenced under *Native display backend*) — removed with the same layout;
  the detection state it showed is now read with **Backend → Native Diagnostics**.

## Build

```powershell
dotnet build gui\ImageProcTest\ImageProcTest.csproj -c Debug
```

The Debug build also compiles the test fault switches (`--automation-fault display-pipeline-after:N`, `ai-worker-disabled`, `ai-worker-silent`,
`ai-worker-silent:0`) that the E2E suite uses. A Release (shipped) build does not contain them: the parser has no branch for them, so the exe refuses
the switch (exit code 2), and Release with `-p:XpeTestFaults=true` fails with error `XPE0001`. The CI job `gui-shipped-build`
(`tools/ci/Test-ShippedBuild.ps1`) checks this.

## Self-check

```powershell
dotnet run --project gui\ImageProcTest.SelfCheck\ImageProcTest.SelfCheck.csproj -c Debug
```

> **Status (GUI-C-84):** the self-check and the fixture template now use the flat-panel raw-DN defaults
> (C=32768, W=65535, intercept 0), the same as `Models/AppSettings.cs`. Until GUI-C-84 they asserted the CT
> values (C=40, W=400, intercept −1024) and failed at the VOI default (measured GUI-C-76). It is not run by CI.

The self-check was written to validate the precreated fixture pack under `gui/ImageProcTest/fixtures/gui-s0/`:

- `fixture-manifest.json`
- `appsettings.template.json`
- `raw/wrist_lat_3072x3072.raw`
- `calibration/offset|gain|defect/`
- settings save/load round-trip
- raw fixture SHA-256 integrity
- mock backend version plus expected log/alert counts
- wrist lateral 3072x3072 raw image loading and preview creation
- display settings defaults (values: see the status note above)
- calibration stage mode defaults: Offset/Gain/Defect/Ghost/Temperature/Nonlinearity/Binning all start as `Auto`
- mock display pipeline application
- comparison viewport defaults, source preservation, and processed preview separation
- VOI body-part preset values (Mock: `Services/MockXpeBackend.cs`; Native: `xpe_voi_preset_create` in `xpe_display.dll`)

## E2E

```powershell
dotnet build gui\ImageProcTest.E2E\ImageProcTest.E2E.csproj -c Debug
.\gui\ImageProcTest.E2E\bin\Debug\net8.0-windows\ImageProcTest.E2E.exe
```

> **Status (measured GUI-C-76): this runner currently fails** — it runs the self-check first, and
> that fails (above). It is not run by CI. It also looks up `ShowRuntimePanelMenuItem`, which was
> removed in #165; that step has not been reached, so whether it is the next failure is unmeasured.
> The maintained UI suite is `clients/ImageProcTest.E2ETests` (FlaUI, run by CI).

The E2E runner was written to:

- executes the self-check first
- launches `ImageProcTest.exe`
- opens the packaged quick-start Help page
- verifies the main window and required controls
- verifies mock version text and initial log/alert population
- verifies the display settings panel and display version text
- verifies calibration evaluation `Off`/`On`/`Auto` radio groups and live summary updates
- verifies `Apply Body Part Preset` and `Apply Display Pipeline` command wiring
- verifies comparison mode controls, zoom commands, and viewport presence
- verifies menu/toolbar parity
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
- `ComparisonViewportDetected`
- `ComparisonSourcePreserved`
- `ComparisonEvidenceExported`
- `BaselineMenuEnabled`, `BaselineRan`, `BaselineStatus`
- `BaselineStatusText`
- `BaselineBitIdentical`, `BaselineFirstDifference`, `BaselineOutputSha256`
- `BaselineStageTimes`, `BaselineTotalMs`
- `BaselineDicomValid`, `BaselineDicomRoundTripIdentical`
- `BaselineExposureIndex`, `BaselineEvidenceFolder`

**`BaselineStatus` and `Passed`.** `Pass` - the Deterministic Baseline command ran and passed. `Fail` - it ran and failed, **or it was attempted and produced
no result** (it threw, its result was dropped, or it did not finish in 60 s). `NotRun` - it was not attempted: the menu item was disabled (Mock), or
preprocessing did not run in this automation run, so no calibration set was given. A report with `Fail` has `Passed=false` (the process exits 1); `NotRun`
leaves `Passed` unchanged. `NotRun` is never a pass and never a fail: nothing was compared. In the Native CI job an unexplained skip of the baseline tests
fails the job separately.

## Run Deterministic Baseline (Pipeline menu)

Runs the Phase 1b chain **twice** on the loaded raw image, in the same process, and reports whether the two outputs are bit-identical. Native backend
only; the menu item is disabled on Mock. "Deterministic" here is the property being checked, not a mode, and not the product's non-AI image path that
other documents also call the deterministic baseline.

**Chain (fixed; no user setting is read).** preprocess (offset, nonlinearity, gain, defect; the uncalibrated EI is measured on its float image) ->
enhance_basic (log, bilateral noise reduction sigma-space 3.0 / sigma-range 50.0, CLAHE clip 3.0 with 8x8 tiles, unsharp mask amount 0.5 / radius 2.0 /
threshold 10.0, all on one float image, converted to 16 bits once at the end with round-half-even and clamping) -> display (modality slope 1 / intercept 0,
linear VOI centre 32768 / width 65535, GSDF off). The log normalisation factor is 65535/log10(65536); no document specifies it, so it is an assumption of
this command only. Every stage must be applied: a stage the module refuses fails the command (nothing is compared and no DICOM file is written).

**Pass criteria (all of them).** (1) both runs applied every stage; (2) the loaded raw frame is unchanged (SHA-256 before = after); (3) no NaN/Inf was
counted in any float intermediate (the preprocess and enhance_basic float images, and the display step's float image after the modality LUT and after the VOI LUT); (4) the two final 16-bit outputs are bit-identical; (5) the first output was written as DICOM, the module's own validator
reported `valid:true`, and reading the file back returned the same pixels and the same body part, kVp and pixel pitch; (6) the evidence file
`baseline.json` was written. Timing is measured and logged; the 3000 ms figure of the product requirements is **not** asserted.

**What the DICOM metadata carries.** Body part, kVp and pixel pitch from the settings. mAs, source-to-image distance and acquisition time are not known to
the app and are written as 0 (unknown), never as invented values.

**Evidence.** `evidence/<RunId>/baseline-<n>/` holds exactly two files: `baseline.dcm` and `baseline.json`. A run that is not a pass leaves no
`baseline.dcm`: the file is written under a `.partial` name and renamed only after every check, including writing `baseline.json`, succeeded.
`baseline.json` records `status`, `failureReason`, `startedAt`, `width`, `height`, `runsExecuted`, `inputPreserved`, `inputSha256Before/After`,
`bitIdentical`, `difference` (first index, count, maximum), `nanInfCount`, `nonFiniteByStageRun1` (one entry per stage plus `display`), `outputSha256`, `stageHashesRun1/2` (SHA-256 of each
stage's output), `stageTimes`, `runTotalsMs`, `totalMs`, `budgetMs` (3000, "measured against, not asserted"), `exposureIndex`, and `dicom` (`path`,
`finalFileWritten`, `passed`, `valid`, `report`, `pixelsIdentical`, `metadataAgrees`, `summary`, `cleanupProblem`).

**Uncalibrated EI.** The command logs and records `uncalibrated EI (보정 안 된 EI) = <EI>, DI = <DI>`: the Exposure Index and Deviation Index the enhance_basic module
computes (`EI = EIT x mean / S0`, `DI = 10 x log10(EI / EIT)`) on the corrected float image before it is scaled to 16 bits. It is a **measurement, not a
pass criterion**: the module's S0 reference (1000) has not been checked against this app's gain scale. On the synthetic 1024x1024 fixture it read
EI 3098.64, DI 10.93 (one observation on one machine).

**Does not touch.** Settings, the image on screen, the chain status and the stage timing the last render left. It takes a backend lifetime ticket but no
Apply request number, so an Apply in flight is neither superseded by it nor supersedes it.

**Known limits.** The DICOM path is passed to the module as ANSI: a folder name with characters outside the system code page makes the write fail
(`XPE_ERR_IO_FAILED`; measured with Thai text on a Korean Windows, code page 949; tracked as #239). The two runs share one process, so state that survives
inside a process is not caught. The display step's 16-bit output (after the presentation LUT) cannot hold a NaN/Inf, so only its float image before that LUT is scanned. The time on a 3072x3072 image has not been
measured; on a synthetic 1024x1024 image the whole command took about 210-290 ms on the development machine.

## Automation E2E with actual detector raw data

Run end-to-end verification against a real flat-panel detector raw image:

```powershell
dotnet run --project gui\ImageProcTest\ImageProcTest.csproj -c Debug -- `
  --automation-raw "tests\test_data\cyan_test\Bright_Chest phantom_75kv_320ma_25.6(SID_110)_00.raw" `
  --automation-width 3072 --automation-height 3072 `
  --automation-report gui\ImageProcTest\bin\Debug\net8.0-windows\e2e-test-report.json
```

Expected result: `Passed=true`, `ActiveImageSummary` starting with `RAW 3072x3072`, `VOI(Linear, C=32768, W=65535)`.

> **Note**: Default VOI values are calibrated for flat-panel DR detectors (16-bit raw, no CT HU offset). The modality LUT
> is identity, so VOI acts on raw DN. Measured (GUI-C-84) with C=40/W=400 on the bundled fixtures: every wrist pixel
> (raw 2481–15451) and all but 36 synthetic pixels lie above the window's upper bound (240), so the window saturates.
> The Mock backend shows that as white; the native backend shows the wrist as **black**, because the saturated output
> is a single value and the preview stretches min..max.

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

If any required export is missing, the app keeps running in Mock mode. The DLL detection state is reported by **Backend → Native Diagnostics** (status bar and log). This prevents a stale DLL from crashing the GUI during development.

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
- `View`: the Logs toggle, two panel toggles scheduled for later phases (disabled), comparison mode, zoom, the detached viewer, and layout reset — see `MainWindow.xaml` for the current items
- `Pipeline`: disabled placeholders for future processing commands
- `Tools`: calibration, fixture, evidence, and future QA commands
- `Help`: offline quick-start, scope, and future generated reference entry points

Unsupported native, DICOM, premium, and AI commands are disabled until their owner modules exist.

## appsettings.json schema

`appsettings.json` is stored next to the executable. Its keys are the `[JsonPropertyName]`
attributes in `Models/AppSettings.cs` — read them there. (A copied list used to live here and had
drifted from that file by GUI-C-76; it was removed rather than updated, so it cannot drift again.)

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
