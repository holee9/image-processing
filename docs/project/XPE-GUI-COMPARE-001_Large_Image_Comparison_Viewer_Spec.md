# XPE-GUI-COMPARE-001: Large Image Comparison Viewer Specification

**Document ID**: XPE-GUI-COMPARE-001
**Version**: 1.0.0
**Date**: 2026-04-28
**Status**: Implemented / Verification Passed / Production Ready  
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

> **DifferenceHeatmap — 정의 부족과 실측 (2026-09-16, GUI-C-53 / #149).** 이 모드의 정의는 두 문서에 나뉘어 있고 서로 어긋납니다. `XPE-GUI-MENU-001` L288 은 값을 **절댓값**(`|source − processed|`)으로 못 박고, `XPE-GUI-COMPARE-001` L68 은 **"signed or absolute"** 로 부호를 고정하지 않습니다. **색 대응(colormap)은 어느 문서에도 없습니다** — `colormap` / `palette` / `색상` / `jet` / `grayscale` 전부 0건. 색 대응이 없으면 어떤 렌더링도 문서에 대해 맞다고도 틀리다고도 판정할 수 없습니다.
>
> 현재 구현은 스스로를 색조 근사라고 적습니다(`ImageComparisonViewport.cs:131`). **그 차이를 쟀습니다**(단색 64×64, 중앙 crop, 참조도 같은 렌더러 통과):
>
> | 입력 | 참 \|Δ\| | 최대 편차 | 평균 편차 |
> |---|---|---|---|
> | 동일 mid-grey | 0 | **255** / 255 | 97.1 |
> | 근접 Δ8 | 8 | 247 | 93.4 |
> | 중간 Δ64 | 64 | 225 | 58.7 |
> | 반대극단 Δ255 | 255 | **205** | 73.9 |
>
> **방향이 직관과 반대입니다 — 차이가 없을 때 가장 크게 어긋납니다.** 편차가 입력에 따라 움직이므로 관찰자가 눈으로 보정할 수 없습니다.
>
> **색 대응이 정해지지 않아도 말할 수 있는 것이 하나 있습니다.** 두 입력이 **동일**하면 참 차분은 모든 화소에서 같은 값이므로, 어떤 색 대응을 쓰든 부호 규약이 무엇이든 **출력은 단일 색**이어야 합니다. 측정된 동일-입력 출력은 그렇지 않습니다. 이것은 빠진 명세에 의존하지 않는 관찰입니다.
>
> **이 문단은 관찰이며 요구의 변경이 아닙니다.** 부호 규약을 어느 쪽으로 정할지, 색 대응을 무엇으로 할지, 구현을 고칠지는 모두 미결이며 #149 에서 다룹니다.
>
> ---
>
> **추가 측정 (2026-09-16, GUI-C-55 / #149) — 구현이 이미 한쪽을 골랐습니다.** 같은 크기의 차이를 **밝은 쪽**과 **어두운 쪽**으로 각각 주고 분리도를 쟀습니다.
>
> | 경로 | +Δ64 | −Δ64 | 간격 |
> |---|---|---|---|
> | 참 차분(참조) | 60.2 | 60.2 | **0.0** |
> | 현재 히트맵 | 22.3 | −15.7 | **38.0** |
>
> **`XPE-GUI-MENU-001` L288 의 값 정의 아래에서는 이 간격이 0이어야 합니다.** `|source − processed|` 는 부호를 지우므로 두 입력이 색 대응에 **같은 값**을 건네고, 따라서 **어떤 색 대응을 쓰든** 출력이 같아야 합니다. 측정된 간격은 38.0 입니다. 앞 문단의 동일-입력 관찰과 마찬가지로 **빠진 색 대응 명세에 의존하지 않는 판별**입니다.
>
> 원인은 합성 방식입니다 — 현재 구현은 processed 를 source 위에 얹으므로 **밝아진 변화는 밝게, 어두워진 변화는 어둡게** 나옵니다. 그것은 부호를 보존하는 동작이고, 절댓값 차분은 그럴 수 없습니다.
>
> **따라서 두 문서의 충돌은 실무에서 이미 한쪽으로 기울어 있습니다.** 현재 렌더링은 `XPE-GUI-COMPARE-001` L68 의 "signed" 읽기와는 일관되고, `XPE-GUI-MENU-001` L288 의 절댓값 정의와는 일관되지 않습니다. **결정으로 고른 것이 아니라 아무도 고르지 않아서 그렇게 된 상태**이며, 그 사실 자체가 #149 의 미결 항목입니다.
>
> 분리도 지표 자체는 반증으로 확인됐습니다 — 참조 경로가 범위의 **98.5%** 를 내고 부호 간격이 **0.0** 이므로, 위 수치의 압축과 간격은 지표가 아니라 **렌더러에 귀속됩니다**.

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

**Phase 1b 완료 상태 (2026-04-28):**
- 4096x4096 UInt16 RAW 비교 지원 완료
- 모든 비교 모드 구현 완료 (Swipe, Split, Overlay, Difference, Source Only, Processed Only)
- 동기화된 zoom/pan/cursor 상태 공유 완료
- Detached viewer 상태 모델 재사용 완료
- Calibration stage controls 연동 완료

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
