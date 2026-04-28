# Handoff: ImageProcTest GUI Redesign — Algorithm Evaluation Workbench

## Overview

This handoff describes the redesigned ImageProcTest GUI, refocused around its
single purpose: **evaluating image-processing algorithms** that ship with this
same project's worktree.

The redesign keeps GUI-S0 / Phase 1b's existing scope boundary (raw binary
viewer, mock + native backend factory, display pipeline command path,
calibration evaluation `Off/On/Auto`) and reorganizes the surface around three
ideas:

1. **Two algorithm "lanes"** — pick a Reference and a Candidate from the
   algorithms compiled into this build, then compare them on the same fixture.
2. **A study queue with run-set summary** — fixtures from
   `gui-s0/golden-26.manifest` are evaluated one by one; pass / fail / defer
   verdicts roll up into a per-run-set summary.
3. **A right-side analysis panel** with quantitative metrics, parameters,
   run-set rollup, and log — replacing the loose Runtime / Raw / Calibration /
   Image Summary / Metadata / Logs / Alerts panels in `MainWindow.xaml`.

This bundle contains **HTML/JSX prototypes** of the redesign. They are
references for look, structure, and behavior — **not** code to ship. The task
is to recreate them as WPF views and view-model changes inside the existing
`ImageProcTest` solution.

## About the Design Files

The design lives in `prototype/`:

- `XPE GUI Redesign.html` — entry point. Renders a design canvas with all
  artboards.
- `variant-c.jsx` — **the recommended design**: the Algorithm Evaluation
  Workbench (1560 × 920). This is the file to implement.
- `roi-local.jsx` — C.1 · ROI / profile-line local measurement screen.
- `calibration-dialog.jsx` — C.2 · Calibration Settings modal (per-stage
  `Off / On / Auto`, lookup directories, fallback, per-detector overrides).
- `variant-a.jsx`, `variant-b.jsx` — earlier explorations (A: dense engineer
  workstation, B: clinical viewer). Kept for reference; **do not implement**.
- `design-canvas.jsx` — the canvas wrapper used by the prototype shell. Not
  part of the product.

Open `XPE GUI Redesign.html` locally to see the live prototypes.

## Fidelity

**High-fidelity.** Colors, type sizes, paddings, and component structure are
final. Recreate the workbench layout pixel-faithfully in WPF, but reuse the
existing solution's controls, MVVM patterns, and conventions —
`ObservableObject`, `RelayCommand`, the `IXpeBackend` factory, the existing
`AppSettings` / `AppSettingsService`, and the existing comparison viewport.

The HTML/JSX should be read for **layout intent and naming**, not for code to
port.

---

## Scope of this redesign

Only the Workbench (variant-c) and its two supporting modals (ROI, Calibration)
are in scope. Everything else listed in `gui-README.md` ("Scope boundary"
section) stays as-is:

- Real DICOM remains owned by `xpe_dicom.dll`.
- `Off / On / Auto` are evaluation-only Test GUI controls.
- Body-part presets remain limited to the four native quick presets until
  SWU-3.4 lands.
- GSDF default off.
- Tile-backed rendering for >4096² is still a planned extension.

The redesign does **not** introduce:

- An algorithm "registry" / lifecycle manager / audit trail (explicitly
  rejected — algorithm changes are code changes in this same project's
  worktree, not something the GUI tracks at runtime).
- Any external-fetch / sync / "new build detection" logic.
- Any approval, promotion, or deprecation workflow.

If a new algorithm is added or improved, it lands in the worktree as a code
change and appears in the Lane dropdowns on the next rebuild. The GUI does
nothing extra to "discover" it.

---

## Top-level layout (Workbench)

Total frame: **1560 × 920 px**. Dark theme (`#08090c` background).

```
┌─────────────────────────────────────────────────────────────────────────┐
│ TOP BAR (56h) — title · run-set badge · Run on all · Export · Focus · ⌕⚙? │
├──────────┬────────────────────────────────────────────┬─────────────────┤
│          │ ALGORITHM BAR (56h)                        │                 │
│  STUDY   │  Lane A — REFERENCE   │  Lane B — CANDIDATE│   ANALYSIS      │
│  QUEUE   ├────────────────────────────────────────────┤   PANEL         │
│  (280w)  │                                            │   (380w)        │
│          │     COMPARISON VIEWPORT                    │                 │
│  per-row │     Swipe / Split / Overlay / Difference   │   Tabs:         │
│  status  │     (existing ComparisonViewport)          │   Metrics       │
│  + delta │                                            │   Parameters    │
│          │     Floating view toolbar (top)            │   Run-set       │
│          │     Floating zoom dock (bottom-right)      │   Log           │
│          ├────────────────────────────────────────────┤                 │
│          │ VERDICT BAR (64h) — notes · Pass/Defer/Fail · Save & next →  │
└──────────┴────────────────────────────────────────────┴─────────────────┘
```

Focus mode (toolbar toggle, key `F`) collapses the left and right panels into
36 px edge rails that can be expanded back individually.

---

## Mapping to the existing code

| Redesign element | Existing equivalent | Action |
|---|---|---|
| Top bar title + run badge | window chrome | New top bar `UserControl`. Run-set badge is new. |
| Lane A / Lane B dropdowns | (none — single-pipeline today) | New. Two `ComboBox`es bound to a hard-coded list of algorithm names compiled into the app. |
| Comparison viewport | existing `ComparisonViewport` + `CompareModeOptions` | **Reuse as-is.** Wire its mode to the floating view toolbar. |
| View toolbar (Swipe/Split/Overlay/Difference + Histogram + ROI) | `CompareModeOptions` already supports the four modes | Replace the current mode `ComboBox` with the floating segmented toolbar. Histogram + ROI buttons open the existing histogram surface and the new ROI tool (see C.1). |
| Zoom dock (`−` / `+` / `⛶` / `1:1`) | `ZoomFitCommand`, `ZoomActualCommand`, `ZoomInCommand`, `ZoomOutCommand` | Reuse commands; restyle as the floating dock. |
| Study queue (left, 280w) | (none) | New. Loads from `gui-s0/golden-26.manifest`. Reuses `LoadImageCommand` per row. |
| Verdict bar | (none) | New. Captures `Pass / Defer / Fail` + free-text notes per fixture. |
| Analysis panel — Metrics tab | (none) | New. Computes PSNR, SSIM, CNR, noise σ, edge sharpness, uniformity, stage timing on the active fixture. |
| Analysis panel — Parameters tab | existing calibration `Off/On/Auto` radios + VOI Mode/Center/Width + body-part preset | Reuse all existing bindings; restyle into the tab. |
| Analysis panel — Run-set tab | (none) | New rollup view. |
| Analysis panel — Log tab | existing `Logs` `ObservableCollection<string>` + `Alerts` | Reuse. Replaces the bottom Logs/Alerts panels. |
| Calibration Settings modal (C.2) | `ShowCalibrationSettingsCommand` | Already wired. Restyle the modal contents per `calibration-dialog.jsx`. |
| ROI / profile-line tool (C.1) | (none) | New. See "Screen C.1" below. |
| Detached comparison viewer | `DetachComparisonViewerCommand` | Keep. |
| Settings save/load | `SaveSettingsCommand`, `AppSettings` | Keep. Add new keys (see "Settings additions"). |

---

## Screen C — Algorithm Evaluation Workbench

### Top bar (56h, `#14171e`)

- **Brand block** — 28×28 cyan→blue gradient square with "X" glyph, then
  title `"XPE Evaluation Workbench"` (13px, 600) and subline
  `"ImageProcTest GUI · Run #<run-id>"` (10px, `#5a5f6a`).
- **Run-set badge** — pill (`rgba(255,255,255,0.04)`, 10px radius). Inside:
  - Label `"RUN-SET"` (9px, letter-spacing 1.2, `#5a5f6a`).
  - `"<done> of <total> evaluated"` (12px, 600).
  - 120 × 4 px segmented bar: green (pass) / red (fail) / amber (defer) widths
    proportional to counts.
  - Inline counts: `"<n> pass"` (`#86efac`), `"<n> fail"` (`#fca5a5`),
    `"<n> defer"` (`#fcd34d`).
- **`Run on all queued`** — outlined pill (36h, 8px radius, 1px hairline).
- **`Export evidence bundle`** — filled cyan pill (`#7dd3fc` bg, `#0c1220`
  text, 600). Bundles current run-set into a `.zip`.
- **Focus mode toggle** — pill, becomes filled cyan when active. Keyboard `F`.
- **`⌕`, `⚙`, `?`** — 36×36 ghost icon buttons.

### Algorithm bar (56h, immediately under top bar)

Two equal-flex lanes, separated by a 1px hairline.

- **Lane A — REFERENCE** (cyan accent `#7dd3fc`)
- **Lane B — CANDIDATE** (violet accent `#c4b5fd`)

Each lane:
- 6 × 22 px colored capsule on the left.
- Label `"LANE A — REFERENCE"` / `"LANE B — CANDIDATE"` (9px, 1.4 letter
  spacing, `#5a5f6a`).
- `ComboBox` (transparent, 14px 600) bound to the **hard-coded algorithm list
  compiled into the app**. Initial seed list (replace at code level when the
  set changes):

  ```
  Baseline v1.0
  Production v1.2
  Candidate v1.4
  Candidate v1.5-rc
  ```

  No NEW / UPDATED / EXPERIMENTAL flags. No build-hash, no sync timestamp,
  no source label. The dropdown is just a list.

### View toolbar (floating, top-center)

Glass pill (`rgba(20,23,30,0.85)` + `backdropFilter: blur(20px)`,
12px radius, 6px padding, 24px shadow). Buttons (8×14 padding, 7px radius,
11px 600):

- `Swipe` · `Split` · `Overlay` · `Difference` — segmented, exclusive. Map to
  existing `CompareModeOptions`:
  - `Swipe` → `SwipeVertical` (or `SwipeHorizontal` for the H toggle)
  - `Split` → `SplitLocked`
  - `Overlay` → `OverlayOpacity` (shows the opacity slider below viewport)
  - `Difference` → `DifferenceHeatmap`
- 1px divider.
- `Histogram` — opens an in-panel histogram (existing surface).
- `ROI` — enters ROI drag-to-draw mode (see C.1).

Only `Swipe / Split / Overlay / Difference` are exclusive; `Histogram` and
`ROI` are independent toggles.

### Comparison viewport

- Reuse the existing `ComparisonViewport` exactly.
- Top-left HUD overlay: `STUDY` block (10px label, then ID, filename,
  `1024 × 1024 · UInt16LE · <body part>`) — JetBrains Mono, with subtle text
  shadow. Bind to `ActiveImageSummary` / `MetadataText`.
- Top-right HUD overlay: `WINDOW` block — `C <center> · W <width>`,
  `VOI: <mode>`, `Calib: <enabled>/<total> stages`. Same type style.
- Bottom corners: cyan `Tag` for Lane A's algorithm name; violet `Tag` for
  Lane B's. Hidden in `Difference` mode.
- In `Overlay` mode, show the opacity slider in a glass pill at bottom center
  (320 px wide).

### Zoom dock (floating, bottom-right)

Glass pill: `−`, `<zoom>%`, `+`, divider, `⛶` (fit), `1:1` (actual).
Wire to existing zoom commands.

### Verdict bar (64h, fixed bottom of viewport column)

- Left label block: `VERDICT` (9px) + `"Record evaluation result for <study-id>"`.
- Notes input — 9×12 padding, dark surface, hairline border.
- Three pill buttons: `✓ Pass` (green), `⏸ Defer` (amber), `✗ Fail` (red).
  Active state fills the pill at ~25% opacity and applies the matching color
  to text + border.
- `Save & next →` — filled cyan, advances to the next queued study.

### Study queue (left, 280w)

- Header: title, count badge, `+` button (adds a fixture).
- Filter segmented: `All` / `Queued` / `Done`.
- Scrollable list. Each row (10×12 padding, 7px radius):
  - 36×36 thumb (placeholder SVG today; later, the loaded RAW preview).
  - Title `<study-id>` (11px 600). Subline body part (10px, `#8a909c`).
  - Right side: status glyph (`✓` green / `✗` red / `⏸` amber / `●` cyan /
    `○` muted) + delta string (`+12.4%`, JetBrains Mono).
- Footer (12×16, top hairline): `FIXTURE SET` label, then
  `gui-s0/golden-26.manifest` and `<n> studies loaded`.
  (No "external sources" / "last sync" / "SHA verified" lines — those would
  imply runtime fetching, which the GUI does not do.)

### Analysis panel (right, 380w)

Tabs (14×6 padding, 11px 600). Active tab gets `#7dd3fc` 2px underline.

#### Metrics tab

1. **Quantitative quality** table (rounded card, 1px hairline).
   Header row: `METRIC | LANE A | LANE B | Δ`.
   Rows for: PSNR (dB), SSIM, CNR, Noise σ (DN), Edge sharp. (px),
   Uniformity (%). Each row has metric name, target spec (10px muted),
   Lane A value, Lane B value, Δ absolute + Δ%. Δ colored green when Lane B
   improves on the metric's `better` direction (higher for PSNR/SSIM/CNR/
   Uniformity, lower for Noise σ / Edge sharp.), red otherwise.

2. **Stage timing** stacked bars per stage (`Preprocess`, `VOI LUT`,
   `Presentation`, `Total`). For each stage, two flexed bars (cyan = Lane A,
   violet = Lane B) at 8 px height with 4 px gap. Numbers in JetBrains Mono.

3. **Histogram** — existing histogram surface. Two overlaid distributions:
   cyan = Lane A (45% opacity), violet = Lane B (55% opacity, `screen` blend
   mode). 5 axis ticks `0 / 16k / 32k / 48k / 65k` (UInt16 range).

4. **ROI measurement** — empty-state card prompting "Drag to draw ROI on
   viewport for local CNR." Clicking enters ROI mode (Screen C.1).

#### Parameters tab — reuses existing bindings

- `Calibration stages` — list of seven stages (Offset, Gain, Defect, Ghost,
  Temperature, Nonlinearity, Binning), each with a `Auto / On / Off`
  segmented control bound to the existing `Calib<X>Mode` properties.
- `VOI LUT` — segmented control for `Linear / LinearExact / Sigmoid` bound to
  `VoiLutMode`. Two inputs side by side: `Center` (`VoiWindowCenter`) and
  `Width` (`VoiWindowWidth`).
- `Lane B parameter overrides` — info banner explaining Lane A is locked to
  the production reference for fair comparison. Lane B exposes
  `Sharpening σ` and `Denoise strength` numeric inputs (new). `Reset Lane B
  to defaults` button.

#### Run-set tab

- 4-up summary cards: `PSNR mean Δ`, `SSIM mean Δ`, `Runtime Δ`,
  `Regressions <n> / <total>`. Color: green if good, amber if mixed, red if
  regression.
- Per-study delta list: row per fixture with id, Δ string, and a centered
  bar showing direction + magnitude.
- `Export evidence bundle (.zip)` filled-cyan button.

#### Log tab

- Reuse `Logs` and `Alerts`. Render rows:
  - Timestamp (10px muted) + level pill (`INFO` cyan, `WARN` amber, `ERR` red).
  - Message line (11px, JetBrains Mono).

---

## Screen C.1 — ROI / Profile-line measurement

Modal-like overlay anchored over the viewport.

- ROI rectangle drag — show coordinates and size live.
- Local stats panel: `Mean`, `Std`, `Min`, `Max`, `CNR (vs background)` for
  Lane A and Lane B side by side, plus Δ.
- Profile-line tool: drag a line; show line-pixel intensity profile for both
  lanes overlaid. Cyan = Lane A, violet = Lane B.
- `Save ROI to evidence bundle` button.

This is a new `UserControl` that draws on top of the comparison viewport.
Suggested entry point: `ToggleRoiCommand` on `MainWindowViewModel`.

## Screen C.2 — Calibration Settings dialog

Reuse `ShowCalibrationSettingsCommand`. The dialog body (per
`calibration-dialog.jsx`) contains:

- **Per-stage table** — Offset, Gain, Defect, Ghost, Temperature,
  Nonlinearity, Binning. Each row: stage name, segmented `Auto / On / Off`,
  lookup directory path with `Browse…` (only enabled for stages that have
  `BrowseXCalibrationDirectoryCommand` today: Offset, Gain, Defect), fallback
  policy dropdown.
- **Per-detector overrides** — collapsed section. Stub for future use.
- **Footer** — `Reset to Auto / Save / Cancel`.

All wiring uses existing `Calib<X>Mode` and `Calib<X>Dir` keys in
`AppSettings`. Save through `SaveSettingsCommand`.

---

## Interactions & behavior

- **Lane selection** — changing a Lane dropdown re-runs the display pipeline
  for that lane on the active study. Implementation: extend
  `ApplyDisplayPipelineAsync` to accept a lane parameter, or run two
  pipelines in parallel and store the results in two `ImageSource` fields
  (`SourceImage` already exists; add `LaneAImage`, `LaneBImage`).
- **View mode** — bound to existing `CompareMode`. The viewport already
  reacts to mode changes; no new behavior needed.
- **Verdict capture** — writes to a per-fixture record:
  `{ studyId, verdict, notes, timestamp, laneA, laneB }`. Persist to
  `evidence/<run-id>/verdicts.json` next to the executable. Reuse existing
  `ExportAutomationReportCommand` plumbing.
- **`Save & next →`** — saves the verdict, advances `ActiveStudy` to the
  next `Queued` row, runs both lane pipelines.
- **Focus mode (`F` key)** — collapses left + right panels to 36 px edge
  rails. Each rail independently expandable. Persist state in
  `AppSettings.FocusMode`.
- **Histogram / ROI buttons** — independent toggles.
- **Export evidence bundle** — zips the run-set's verdicts, metric snapshots,
  histogram exports, and ROI captures. Reuse existing automation report
  format and extend.

## State management

Add to `MainWindowViewModel`:

| Property | Type | Purpose |
|---|---|---|
| `LaneAAlgorithm` | `string` | Selected reference algorithm |
| `LaneBAlgorithm` | `string` | Selected candidate algorithm |
| `LaneAImage` | `ImageSource?` | Reference render |
| `LaneBImage` | `ImageSource?` | Candidate render |
| `ActiveStudyId` | `string` | Currently focused fixture |
| `Studies` | `ObservableCollection<StudyEntry>` | Loaded queue |
| `RunSet` | `RunSetState` | passed/failed/deferred counters |
| `ActiveVerdict` | `Verdict?` | `Pass / Defer / Fail` |
| `VerdictNotes` | `string` | Free-text |
| `FocusMode` | `bool` | Collapses side panels |
| `LeftPanelOpen` | `bool` | Used only when `FocusMode` is true |
| `RightPanelOpen` | `bool` | Used only when `FocusMode` is true |
| `AnalysisTab` | `string` | `metrics / parameters / runset / log` |
| `RoiActive` | `bool` | ROI tool toggle |
| `LaneBSharpeningSigma` | `double` | Candidate-only override |
| `LaneBDenoiseStrength` | `double` | Candidate-only override |

New commands:

- `RunOnAllQueuedCommand`
- `RecordVerdictCommand` (parameter: `Pass / Defer / Fail`)
- `SaveAndNextCommand`
- `ToggleFocusModeCommand`
- `ToggleRoiCommand`
- `ExportEvidenceBundleCommand` (extends existing automation export)
- `SwitchAnalysisTabCommand`
- `ResetLaneBOverridesCommand`

`StudyEntry`:
```csharp
public sealed class StudyEntry : ObservableObject
{
    public string Id { get; init; }
    public string Name { get; init; }
    public string BodyPart { get; init; }
    public string RawPath { get; init; }
    public StudyStatus Status { get; set; } // Queued, Active, Pass, Fail, Defer
    public string DeltaSummary { get; set; } // e.g. "+12.4%" or "—"
}
```

## Settings additions

Add to `AppSettings` (and persist in `appsettings.json`):

- `laneAAlgorithm`
- `laneBAlgorithm`
- `focusMode`
- `leftPanelOpen`
- `rightPanelOpen`
- `analysisTab`
- `laneBSharpeningSigma`
- `laneBDenoiseStrength`
- `lastRunSetId`

Existing keys (`comparisonMode`, `comparisonZoomScale`, `voiWindowCenter`,
calibration modes, etc.) are reused.

## Design tokens

```
Background           #08090c
Surface (solid)      #14171e
Surface (glass)      rgba(20,23,30,0.85) + backdrop-filter: blur(20px)
Surface alt          rgba(28,32,41,0.9)
Hairline             rgba(255,255,255,0.06)
Hairline strong      rgba(255,255,255,0.12)

Text primary         #f0f2f5
Text dim             #8a909c
Text mute            #5a5f6a

Accent (Lane A)      #7dd3fc      (cyan)
Accent bg            rgba(125,211,252,0.12)
Candidate (Lane B)   #c4b5fd      (violet)
Pass / good          #86efac
Warn / defer         #fcd34d
Fail / regression    #fca5a5

Spacing              4 / 8 / 12 / 16 / 20 / 24
Radius               4 / 6 / 7 / 8 / 10 / 12
Body type            Inter (system fallback) — 11/12/13/14
Mono                 JetBrains Mono / Consolas — 10/11/12

Top bar height       56
Algorithm bar height 56
Verdict bar height   64
Left panel width     280
Right panel width    380
Edge rail width      36 (focus mode)
```

## Assets

Only one bitmap reference is used in the prototype: a placeholder SVG x-ray
thumbnail (`assets/xray-placeholder.svg`). In WPF, replace with the actual
RAW-derived preview generated by the existing image-loading path
(`LoadImageCommand` produces `SourceImage`).

No new icon assets are required; the prototype uses Unicode glyphs
(`◰ ◱ ⌕ ⚙ ⊟ ⛶ ✓ ✗ ⏸ ▶▶ 📦`). Replace with the icon font already in use by the
WPF app, or with vector path resources.

## Files in this bundle

```
prototype/
  XPE GUI Redesign.html      ← open this in a browser
  variant-c.jsx               ← Workbench (implement this)
  roi-local.jsx               ← C.1 ROI tool
  calibration-dialog.jsx      ← C.2 Calibration modal
  variant-a.jsx               ← reference only — do not implement
  variant-b.jsx               ← reference only — do not implement
  design-canvas.jsx           ← prototype shell, not part of the product
  assets/
    xray-placeholder.svg      ← replace with real RAW preview at runtime

reference/
  MainWindow.xaml             ← current view to refactor
  MainWindowViewModel.cs      ← current VM to extend
  gui-README.md               ← scope, build, self-check, E2E

IMPLEMENTATION_GUIDE.md       ← step-by-step plan for Claude Code CLI
```
