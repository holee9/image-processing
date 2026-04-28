# Implementation guide — for Claude Code CLI

This file is a step-by-step plan for the developer (or Claude Code) actually
landing the redesign in the `ImageProcTest` solution. It assumes the existing
solution layout described in `reference/gui-README.md`.

Work in **small, verifiable slices**. After each slice, run the existing
self-check + E2E suites — they are the contract this redesign must keep
passing.

```powershell
dotnet build gui\ImageProcTest\ImageProcTest.csproj -c Debug
dotnet run --project gui\ImageProcTest.SelfCheck\ImageProcTest.SelfCheck.csproj -c Debug
```

---

## Slice 0 — branch + golden snapshot

1. `git checkout -b feature/evaluation-workbench`
2. Run the existing self-check and the real-click E2E. Capture green logs.
   These are the regression baseline.
3. Add this `design_handoff_workbench/` folder to the repo at
   `docs/design/evaluation-workbench/` (or keep it out of source — your call).

## Slice 1 — settings + study model (no UI yet)

Goal: persistence and the data model land first, decoupled from any view.

1. Extend `AppSettings` with the new keys listed in `README.md → Settings
   additions`. Default `focusMode = false`, `leftPanelOpen = true`,
   `rightPanelOpen = true`, `analysisTab = "metrics"`,
   `laneBSharpeningSigma = 0.85`, `laneBDenoiseStrength = 0.42`.
2. Update `AppSettingsService` save/load round-trip.
3. Add `StudyEntry` and `StudyStatus` to `Models/`.
4. Add `RunSetState` (counts, started-at, run-id).
5. Update the self-check fixture-pack tests to cover the new settings keys
   (round-trip).

Verify: self-check still green. No UI changes yet.

## Slice 2 — view-model surface

1. Add the new properties and commands listed in
   `README.md → State management` to `MainWindowViewModel`.
2. Wire `RecordVerdictCommand` to write a `verdicts.json` under
   `evidence/<run-id>/`. Reuse the existing automation-export plumbing.
3. Wire `SaveAndNextCommand` to advance `ActiveStudyId` to the next
   `Queued` `StudyEntry`.
4. Stub `RunOnAllQueuedCommand` and `ExportEvidenceBundleCommand` (just log
   "not implemented"; we will fill in Slice 7).
5. Add `LaneAAlgorithm` / `LaneBAlgorithm` defaults to
   `"Production v1.2"` / `"Candidate v1.4"`. The list of algorithm names is
   a `static readonly string[]` constant inside the VM — that is the only
   place changes happen when a new algorithm is added to the project. There
   is no runtime discovery.

Verify: self-check + E2E still green.

## Slice 3 — top bar + algorithm bar

1. Extract the current window chrome / menu into `Views/TopBar.xaml`.
2. Build the new top bar matching the spec in `README.md → Top bar`. Bind:
   - Run-set badge ↔ `RunSet`
   - `Run on all queued` ↔ `RunOnAllQueuedCommand`
   - `Export evidence bundle` ↔ `ExportEvidenceBundleCommand`
   - Focus toggle ↔ `ToggleFocusModeCommand`
3. Build `Views/AlgorithmBar.xaml` with two `ComboBox`es bound to
   `LaneAAlgorithm` / `LaneBAlgorithm`. ItemsSource is the static algorithm
   list. **No "from algo-lib" label, no build hash, no NEW/UPDATED badges.**
4. Keep the existing menu bar accessible (File / Backend / View / Pipeline /
   Tools / Help) — wire it under the top bar's overflow `⚙` or keep as a
   thin secondary strip while migrating.

Verify: app launches; menus still work; lane selection persists across
restart.

## Slice 4 — viewport + view toolbar + zoom dock

1. Keep the existing `ComparisonViewport`. Wrap it in a new
   `Views/ViewportShell.xaml` that adds:
   - Floating view toolbar (top center) — segmented mode buttons bound to
     `CompareMode` (existing `CompareModeOptions`).
   - Floating zoom dock (bottom right) — bound to existing zoom commands.
   - HUD overlays (top-left study, top-right window) bound to
     `ActiveImageSummary` / `MetadataText` / VOI center+width.
   - Lane corner tags bound to `LaneAAlgorithm` / `LaneBAlgorithm`.
2. Implement two-lane rendering: extend `ApplyDisplayPipelineAsync` to take
   a `LaneId` and produce `LaneAImage` / `LaneBImage`. The existing single
   `ProcessedImage` becomes Lane B's output during the transition; Lane A's
   output is the new field. The viewport reads both.

Verify: switching lane B's algorithm causes a re-render. Mode switching
still works. Existing E2E `ComparisonModeControls` tests pass.

## Slice 5 — study queue (left)

1. New `Views/StudyQueue.xaml` (280w). Loads from
   `gui-s0/golden-26.manifest`. Each row binds to a `StudyEntry`.
2. Selecting a row sets `ActiveStudyId` and triggers `LoadImageCommand` for
   the row's `RawPath`, then `ApplyDisplayPipelineCommand` for both lanes.
3. Filter segmented (`All / Queued / Done`) is a local `CollectionView`
   filter — no new VM work.
4. Footer: `FIXTURE SET` block bound to manifest path + studies count.
   **No "EXTERNAL SOURCES" / sync / SHA lines.**

Verify: the existing wrist-lateral fixture still loads via this row.
Self-check `Active image summary` assertion still passes.

## Slice 6 — verdict bar

1. New `Views/VerdictBar.xaml` (64h). Notes `TextBox`, three verdict pill
   buttons, `Save & next →`.
2. Wire to `RecordVerdictCommand` and `SaveAndNextCommand`.
3. Update the run-set badge live as verdicts roll in.

Verify: recording verdicts updates the badge counts and writes
`verdicts.json`.

## Slice 7 — analysis panel (right)

1. New `Views/AnalysisPanel.xaml` (380w) with four tabs.
2. **Metrics tab** — implement metric computation for the active study:
   - PSNR / SSIM / CNR / noise σ / edge sharpness / uniformity. Use
     `xpe_common.dll` helpers if exposed; otherwise compute in managed code
     against the source + processed buffers.
   - Stage timing: capture timestamps around each stage call in the
     pipeline runner. Aggregate per lane.
   - Histogram: existing histogram surface, render two distributions.
3. **Parameters tab** — restyle existing calibration / VOI / body-part
   controls. Add `LaneBSharpeningSigma` / `LaneBDenoiseStrength` numeric
   inputs and `ResetLaneBOverridesCommand`.
4. **Run-set tab** — 4 summary cards + per-study delta list +
   `Export evidence bundle (.zip)`.
5. **Log tab** — bind to existing `Logs` + `Alerts`.

Verify: existing E2E checks for calibration `Off/On/Auto` and VOI controls
all still pass — they were just moved into a tab.

## Slice 8 — focus mode + edge rails

1. Add the toolbar toggle to the top bar (Slice 3 stubbed it).
2. When `FocusMode` is true, collapse left/right panels to 36 px rails.
   Independent expand state per rail (`LeftPanelOpen`, `RightPanelOpen`).
3. Keyboard `F` toggles `FocusMode`. Persist via `SaveSettings`.

Verify: state survives restart.

## Slice 9 — Calibration Settings modal (C.2)

`ShowCalibrationSettingsCommand` is already wired. Replace the dialog body
with the layout in `prototype/calibration-dialog.jsx`:

- Per-stage rows for all 7 stages with `Auto / On / Off` segmented and
  lookup directory + `Browse…` (only Offset/Gain/Defect have browse today).
- Fallback policy dropdown per stage.
- Per-detector overrides section (stub).
- `Reset to Auto / Save / Cancel`.

Verify: the existing E2E for calibration radios still passes against the
new dialog layout.

## Slice 10 — ROI tool (C.1)

1. Add `ToggleRoiCommand` and an ROI overlay `UserControl` that draws on top
   of `ComparisonViewport`.
2. Mouse-drag draws a rectangle. Compute Mean / Std / Min / Max / CNR per
   lane.
3. Profile-line tool — drag a line, render an intensity profile chart.
4. `Save ROI to evidence bundle` writes a screenshot + stats JSON into
   `evidence/<run-id>/roi/`.

Verify: ROI mode does not break mode switching or zoom.

## Slice 11 — evidence bundle export

1. Implement `ExportEvidenceBundleCommand`: zip
   `evidence/<run-id>/{verdicts.json, metrics/, roi/, automation-report.json}`
   into `evidence/bundles/<run-id>.zip`.
2. Reuse / extend the existing `ExportAutomationReportCommand` writer.

Verify: real-click E2E `ComparisonEvidenceExported` assertion is preserved
or extended.

## Slice 12 — cleanup

1. Remove the old loose panels (`Runtime`, `Raw Settings`,
   `Image Summary`, `Metadata`, bottom `Logs`, `Alerts`) once their data is
   visible in the new analysis panel and HUD overlays.
2. Remove `Show<X>Panel` toggles that no longer have a target.
3. Update `gui-README.md` "Menu model" and self-check expectations to match
   the redesigned surface.
4. Run real-click E2E end-to-end. Update any assertions whose targets moved.

---

## Things to **not** add

These are explicit anti-goals based on the design review:

- ❌ Algorithm registry / catalog screen.
- ❌ "NEW" / "UPDATED" / "EXPERIMENTAL" badges on algorithms.
- ❌ Build hash, sync timestamp, SHA-verified, or "from algo-lib"
  annotations.
- ❌ Any UI implying the GUI fetches algorithms from outside its own build.
- ❌ Approval / promotion / deprecation workflow for algorithms.

If a new algorithm is added to the project, you change the static algorithm
list constant in the view-model and rebuild. That is the entire mechanism.

---

## Quick cross-reference

| Spec | Where to read |
|---|---|
| Visual layout | `prototype/variant-c.jsx` |
| ROI tool | `prototype/roi-local.jsx` |
| Calibration modal | `prototype/calibration-dialog.jsx` |
| Tokens | `README.md → Design tokens` |
| State + commands | `README.md → State management` |
| Existing scope | `reference/gui-README.md` |
| Existing VM | `reference/MainWindowViewModel.cs` |
| Existing view | `reference/MainWindow.xaml` |
