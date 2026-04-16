# XPE-GUI-COMPARE-001: Large Image Comparison Viewer Specification

**Document ID**: XPE-GUI-COMPARE-001  
**Version**: 0.2.0  
**Date**: 2026-04-16  
**Status**: Implemented / Verification Passed  
**Owner**: GUI/System Integration  
**Tracking Issue**: GitHub Issue #8  
**Applies To**: `ImageProcTest.exe`, Phase 1b Display integration, Phase 2+ advanced review workflows  

---

## 1. Purpose

This specification defines how `ImageProcTest.exe` shall compare a source image and a processed image when images can be as large as 4096x4096 16-bit RAW frames and may grow beyond that size in later phases.

The goal is to make visual verification practical without forcing the user to manage two independent large image windows.

Traceability:

- market requirements: `MR-IMG-006`, `MR-OPS-005`,
- product requirements: `PR-GUI-001`, `PR-GUI-002`, `PR-GUI-003`,
- software requirements: `SRS-FUNC-024`, `SRS-SAFE-013`, `SRS-PERF-007`, `SRS-PERF-008`,
- display interface requirements: `IF-GUI-301` through `IF-GUI-304`.
- calibration evaluation requirement: `PR-FUNC-011`, `PR-GUI-004`.

---

## 2. Design Decision

The approved design candidate is:

- default viewer: one in-app `ComparisonViewport`,
- default comparison mode: source/processed swipe, also called wiper slider,
- secondary modes: split, overlay opacity, difference heatmap, source only, processed only,
- optional detached viewer window: available for full-screen or multi-monitor review, but not the default path,
- rejected default: two independent source/processed windows.

Rationale:

- one viewport guarantees synchronized zoom, pan, cursor, window/level, and pixel coordinate mapping,
- a swipe divider gives fast local before/after comparison without consuming the screen with two large panes,
- detached mode is still useful for full-screen review but should reuse the same viewport model,
- two independent windows are error-prone because zoom and pan synchronization becomes a permanent maintenance burden.

---

## 3. User Workflow

1. User loads a RAW or DICOM image.
2. App renders the source image in the comparison viewport.
3. User applies display or processing pipeline.
4. App keeps the source image immutable and attaches a processed image layer.
5. User may set calibration/preprocessing stages to `Off`, `On`, or `Auto` for evaluation-only A/B testing.
6. User compares source and processed outputs through one of the comparison modes.
7. User can zoom, pan, inspect pixel coordinates, export evidence, or detach the same viewer into a separate window.

---

## 4. Comparison Modes

| Mode | Behavior | Primary Use |
|---|---|---|
| `SwipeVertical` | Left of divider shows source, right shows processed | Default before/after comparison |
| `SwipeHorizontal` | Top of divider shows source, bottom shows processed | Long anatomy or vertical artifact review |
| `SplitLocked` | Source and processed panes are side-by-side but share zoom/pan/cursor | Teaching, screenshots, demonstrations |
| `OverlayOpacity` | Processed image overlays source with opacity slider | Subtle tone or registration checks |
| `DifferenceHeatmap` | Shows signed or absolute pixel/display difference | Algorithm regression and artifact review |
| `SourceOnly` | Shows source layer only | Raw/reference inspection |
| `ProcessedOnly` | Shows processed layer only | Final output inspection |

---

## 5. Interaction Requirements

| ID | Requirement | Acceptance |
|---|---|---|
| `GUI-CMP-FR-001` | The viewer shall show source and processed images in one synchronized coordinate system. | Cursor pixel coordinate is identical for both layers. |
| `GUI-CMP-FR-002` | The default comparison mode shall be vertical swipe with a draggable divider. | Mouse drag updates the divider without reprocessing the image. |
| `GUI-CMP-FR-003` | The viewer shall support zoom fit, 100%, zoom in/out, mouse wheel zoom, and pan. | Zoom and pan apply to both source and processed layers. |
| `GUI-CMP-FR-004` | The viewer shall support a detached window that reuses the same comparison state model. | Detach does not fork processing state or create an unsynchronized viewer. |
| `GUI-CMP-FR-005` | The viewer shall expose source only, processed only, split locked, overlay opacity, and difference heatmap modes. | Mode switching does not reload source data. |
| `GUI-CMP-FR-006` | The viewer shall preserve the original source frame or source frame reference while processed outputs are updated. | Re-running a pipeline never overwrites the source layer. |
| `GUI-CMP-FR-007` | The viewer shall expose image metadata, current zoom, pan, divider position, pixel coordinate, and sampled source/processed values for evidence capture. | Evidence export contains viewer state and image identifiers. |
| `GUI-CMP-FR-008` | The Test GUI shall expose one-click calibration stage radio controls for offset, gain, defect, ghost, temperature, nonlinearity, and binning using `Off`, `On`, and `Auto`. | User can switch each stage independently before re-running the pipeline without opening a dropdown. |
| `GUI-CMP-FR-009` | The Test GUI shall label and record stage bypass/force decisions as evaluation-only behavior. | Automation report contains the selected stage mode for each controllable stage. |

---

## 6. Performance and Resource Requirements

| ID | Requirement | Target |
|---|---|---|
| `GUI-CMP-NFR-001` | 4096x4096 16-bit source plus processed comparison shall be supported in the in-app viewer. | No out-of-memory condition under the system memory budget. |
| `GUI-CMP-NFR-002` | Zoom, pan, and swipe divider updates shall avoid full pipeline reprocessing. | Interaction remains visually responsive during local review. |
| `GUI-CMP-NFR-003` | Initial implementation may use WPF `BitmapSource` / `WriteableBitmap` with clipping and transform-based rendering. | Suitable for 4096x4096 Phase 1b validation. |
| `GUI-CMP-NFR-004` | Images larger than the Phase 1b comfort envelope shall use a tile-backed rendering path before release claim expansion. | Visible tiles only are decoded/rendered; off-screen tiles are cached or evicted. |
| `GUI-CMP-NFR-005` | The design shall keep a future path open for GPU-backed rendering. | Rendering abstraction does not hard-code WPF `Image` controls as the permanent architecture. |

Phase 1b implementation target:

- source RAW: 4096x4096, 16-bit,
- processed display preview: 4096x4096,
- comparison display: one viewport, not two independent large windows,
- larger-than-4096 support: specified and tested as an architectural extension path, not required as a Phase 1b release claim unless tile rendering is implemented.

---

## 7. Rendering Architecture

### 7.1 Phase 1b implementation path

Use a custom WPF control, tentatively named `ImageComparisonViewport`.

The control owns:

- `SourceImage`,
- `ProcessedImage`,
- `CompareMode`,
- `Zoom`,
- `PanOffset`,
- `SwipePosition`,
- `ViewportRect`,
- `CursorPixel`.

The control shall draw both images through the same transform and clip each layer according to the comparison mode. The default swipe mode shall not create two independent `ScrollViewer` instances.

### 7.2 Large-image extension path

For images larger than 4096x4096 or for workflows that keep multiple intermediate outputs, introduce:

- tile pyramid generation,
- visible-tile renderer,
- LRU tile cache,
- memory-mapped source frame access where appropriate,
- display-LUT tile generation without reprocessing off-screen tiles,
- shared viewport state between main and detached viewer windows.

### 7.3 Rejected permanent architecture

The current `ScrollViewer + Image` source pane and `ScrollViewer + Image` processed pane pattern is acceptable as a temporary GUI-S0/Phase 1b bridge, but it shall not be the permanent comparison architecture because it does not guarantee synchronized interaction.

---

## 8. Verification Requirements

| ID | Verification | Evidence |
|---|---|---|
| `GUI-CMP-VER-001` | Load 4096x4096 UInt16 RAW and render source/processed comparison. | E2E report and screenshot artifact. |
| `GUI-CMP-VER-002` | Drag swipe divider at fit zoom and 100% zoom. | Automation state and screenshot artifact. |
| `GUI-CMP-VER-003` | Zoom and pan after processing output changes. | Shared viewport coordinate log. |
| `GUI-CMP-VER-004` | Switch all comparison modes without reloading source image. | GUI automation report. |
| `GUI-CMP-VER-005` | Detach viewer and verify same state model. | Detached-window automation or manual UAT checklist. |
| `GUI-CMP-VER-006` | Record viewer state in evidence export. | JSON evidence bundle. |
| `GUI-CMP-VER-007` | Switch calibration stage modes between `Off`, `On`, and `Auto` and verify the summary/evidence state updates. | GUI automation report includes calibration evaluation state. |

---

## 9. Implementation Backlog Candidate

| Backlog ID | Summary | Approval State |
|---|---|---|
| `BI-02.04.03` | Implement `ImageComparisonViewport` with swipe, synchronized zoom/pan, and source/processed layers. | Implemented |
| `BI-02.04.04` | Add comparison mode commands and evidence-state export. | Implemented |
| `BI-05.05.05` | Connect display pipeline output to comparison viewport. | Implemented |
| `BI-05.05.06` | Add RAW comparison E2E fixture and automation. | Implemented with `wrist_lat_3072x3072.raw` fixture plus 4096 synthetic automation |
| `BI-05.05.07` | Design tile-backed rendering extension for images larger than 4096x4096. | Documented, implementation deferred |
| `BI-05.05.08` | Add calibration evaluation stage controls with `Off`/`On`/`Auto` modes. | Implemented for Test GUI wiring and evidence capture; native preprocess bridge deferred |

---

## 10. Implementation Evidence

Implemented in `gui/ImageProcTest`:

- `Controls/ImageComparisonViewport.cs`,
- `ViewModels/MainWindowViewModel.cs`,
- `MainWindow.xaml`,
- `Models/AppSettings.cs`,
- `Models/CalibrationStageMode.cs`,
- `Models/GuiAutomationReport.cs`,
- `Services/MockXpeBackend.cs`,
- `ImageProcTest.E2E`,
- `ImageProcTest.SelfCheck`,
- offline Help and GUI README updates.

Committed test fixture:

- `gui/ImageProcTest/fixtures/gui-s0/raw/wrist_lat_3072x3072.raw`,
- dimensions: 3072x3072,
- pixel format: UInt16LE,
- SHA256: `C823233F2196A217F4512BFEC47E62A17F003CC894EF7947E1F40DD0B59A3D70`.

Verification completed:

- `dotnet build gui\ImageProcTest\ImageProcTest.csproj -c Debug`,
- `dotnet build gui\ImageProcTest.SelfCheck\ImageProcTest.SelfCheck.csproj -c Debug`,
- `dotnet build gui\ImageProcTest.E2E\ImageProcTest.E2E.csproj -c Debug`,
- `gui\ImageProcTest.SelfCheck\bin\Debug\net8.0-windows\ImageProcTest.SelfCheck.exe`,
- `gui\ImageProcTest.E2E\bin\Debug\net8.0-windows\ImageProcTest.E2E.exe`,
- `ImageProcTest.exe` automation with `wrist_lat_3072x3072.raw`: `Passed=true`,
- `ImageProcTest.exe` automation with generated 4096x4096 UInt16 RAW: `Passed=true`.

Issue trace:

- GitHub Issue #8 records approval, implementation, verification, and fixture update comments using the `codex:` prefix.
