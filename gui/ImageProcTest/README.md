# ImageProcTest GUI-S0

`ImageProcTest` is the GUI-first WPF shell for the XPE program. This sprint stays intentionally narrow:

- raw binary image viewer only
- `IXpeBackend` mock contract only
- settings UI, log panel, alert panel
- offline packaged Help window with quick-start and scope pages
- top-level menu bar: File, Backend, View, Pipeline, Tools, Help
- no real DICOM parsing
- no native P/Invoke yet

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
- `raw/synthetic_1024x1024.raw`
- `calibration/offset|gain|defect/`
- settings save/load round-trip
- raw fixture SHA-256 integrity
- mock backend version plus expected log/alert counts
- raw image loading and preview creation

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

The E2E step then launches the actual desktop app in built-in automation mode, loads the prepared raw fixture from the runtime fixture tree, saves settings, clears logs and alerts, and verifies the emitted `automation-report.json`.

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
- `lastRawDir`

## Scope boundary

- Real DICOM read/write remains owned by `xpe_dicom.dll` in Phase 1b.
- Real native backend activation remains owned by `SPRINT-P0-07`.
