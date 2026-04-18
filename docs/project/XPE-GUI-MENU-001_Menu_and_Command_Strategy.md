# GUI Menu and Command Strategy

**Document ID**: XPE-GUI-MENU-001
**Version**: 1.1.0
**Date**: 2026-04-18
**Status**: Controlled Draft
**Canonical Scope**: `docs/project/`
**Cross-References**: XPE-GUI-ARCH-001 §3.2 (Commanding), XPE-GUI-ACCESS-001 §5 (Keyboard Navigation), XPE-GUI-DISP-INT-001 v2.0 §4.2, XPE-GUI-E2E-001 §4 (Scenarios)

---

## CHANGE HISTORY

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.1.0 | 2026-04-18 | manager-spec (GUI Lane) | Pipeline menu Phase 1a/1b/2/3 activation timing table (§8), Diagnostic menu/tools completion (§9), Keyboard shortcut master matrix (§10), command quality rules exact schema (§6 extended) |
| 1.0.0 | 2026-04-16 | (author) | Initial menu taxonomy |

---

## 1. Purpose

This document defines the long-term menu and command model for `ImageProcTest.exe`.

It resolves the GUI-S0 layout decision:

- keep a top-level menu bar,
- keep high-frequency controls in the toolbar,
- expand the menu bar into a complete command taxonomy as modules are implemented.

The intent is to make the application feel complete from the earliest sprint while avoiding premature implementation of native or DICOM-owned behavior.

---

## 2. Design Basis

The adopted design follows three principles:

1. **Menu bar for command taxonomy**: WPF `Menu` organizes command-related items hierarchically. It is appropriate for stable top-level groups such as File, View, Tools, and Help.
2. **Toolbar for frequent actions**: high-frequency sprint-demo commands remain directly visible as toolbar buttons.
3. **Shared command model**: the same command should be invokable from menu, toolbar, keyboard shortcut, automation, and later context surfaces where applicable.

Research basis:

- Microsoft Learn WPF Menu overview: `Menu` organizes elements associated with commands and event handlers in hierarchical order.
- https://learn.microsoft.com/en-us/dotnet/desktop/wpf/controls/menu-overview
- Microsoft Learn Windows commanding guidance: commands should be shareable across multiple command surfaces and input modes.
- https://learn.microsoft.com/en-us/windows/apps/develop/ui/controls/commanding
- Microsoft Learn WPF Commanding Overview: commanding separates command semantics from the object that invokes the command.
- https://learn.microsoft.com/dotnet/desktop/wpf/advanced/commanding-overview

---

## 3. Canonical Top-Level Menu Groups

`ImageProcTest.exe` shall converge on the following top-level menu bar:

| Menu | Purpose | GUI-S0 status |
|---|---|---|
| `File` | input/output, recent files, settings persistence, exit | planned shell |
| `Backend` | mock/native backend lifecycle, DLL diagnostics, P/Invoke smoke checks | planned shell |
| `View` | panel visibility, image zoom, comparison modes, layout reset, theme/display aids | planned shell |
| `Pipeline` | staged processing commands from Phase 1a onward | future disabled shell |
| `Tools` | calibration, fixture, benchmark, QA, and evidence tools | planned shell |
| `Help` | offline help, quick start, workflow help, API reference, about/build info | active from GUI-S0 |

The top-level `Help` menu is intentionally kept on the menu bar rather than moved into the toolbar, because additional top-level menus will join it as the GUI matures.

---

## 4. Menu Detail

### 4.1 File

Initial and planned commands:

- `Open Raw...`
- `Open Recent`
- `Save Settings`
- `Export Automation Report`
- `Export Evidence Bundle` (future)
- `Exit`

Rules:

- `Open DICOM...` shall remain disabled or absent until `xpe_dicom.dll` integration.
- GUI-S0 must not implement C# DICOM parsing.

### 4.2 Backend

Initial and planned commands:

- `Initialize Backend`
- `Shutdown Backend`
- `Backend Mode`
- `Native DLL Diagnostics`
- `Run P/Invoke Smoke Test` (Phase 0 integration)
- `Open Runtime Logs`

Rules:

- Mock backend commands must remain usable without native DLLs.
- Native backend commands remain disabled until `RealXpeBackend` is implemented.

### 4.3 View

Initial and planned commands:

- `Show Runtime Panel`
- `Show Raw Settings`
- `Show Calibration Evaluation`
- `Show Logs`
- `Show Alerts`
- `Reset Layout`
- `Zoom Fit`
- `Zoom 100%`
- `Zoom In`
- `Zoom Out`
- `Pan`
- `Compare Mode: Swipe / Split / Overlay / Difference / Source Only / Processed Only`
- `Detach Viewer`

Rules:

- View commands shall not mutate image data.
- View commands may be implemented before native DLL integration.
- Comparison commands shall operate on one synchronized viewport state and must not create unsynchronized source/processed image windows by default.

### 4.4 Pipeline

Initial and planned commands:

- `Run Preprocessing`
- `Run Deterministic Baseline`
- `Run Full Pipeline`
- `Stop Processing`
- `Stage Timing`
- `Open Pipeline Diagnostics`

Rules:

- Pipeline commands remain disabled until their owner DLL and API are present.
- Disabled pipeline commands shall expose a clear status reason and relevant Help link.

### 4.5 Tools

Initial and planned commands:

- `Calibration Settings`
- `Calibration Evaluation`
- `Fixture Manager`
- `Run Self-Check`
- `Run GUI E2E`
- `Benchmark Runner`
- `QA Constancy`
- `Open Evidence Folder`

Rules:

- Tool commands should produce explicit evidence files when they affect verification.
- Benchmark and QA commands shall point to frozen manifests when those manifests exist.
- Calibration evaluation controls are Test GUI tools, not product-mode clinical bypass controls; their `Off`/`On`/`Auto` state shall be saved in automation evidence.

### 4.6 Help

Initial and planned commands:

- `Help Home`
- `Quick Start`
- `Scope and Limitations`
- `Current Workflow Help`
- `API Reference`
- `Troubleshooting`
- `About / Build Info`

Rules:

- Help shall work offline.
- Help pages shall be version-matched to the running build.
- `F1` or equivalent context help should open the current workflow help once workflow context exists.

---

## 5. Progressive Rollout

| Phase | Required menu maturity |
|---|---|
| GUI-S0 | top menu bar present; `Help` active; `File`, `Backend`, `View`, and `Tools` shells may be present if they do not imply unsupported behavior |
| Phase 0 integration | `Backend` menu connects mock-to-real diagnostics and P/Invoke smoke evidence |
| Phase 1a | `Pipeline` exposes preprocessing run commands and timing evidence |
| Phase 1b | `File` and `Pipeline` cover deterministic baseline workflow and DICOM-owned actions |
| Phase 2 | optional premium commands are visible only when owner binaries are available |
| Phase 3 | assistive AI commands show confidence, fallback, and disabled-state reasons |

---

## 6. Command Quality Rules

Every menu command shall define:

- owner module or sprint,
- enabled/disabled rule,
- user-visible status or error message,
- automation ID,
- keyboard shortcut where useful,
- Help target,
- E2E or manual verification path.

Toolbar buttons are shortcuts for high-frequency menu commands, not separate behavior.

---

## 7. Rejected Alternatives

| Alternative | Decision | Reason |
|---|---|---|
| Move Help into toolbar only | Rejected | Help is a stable top-level category and should remain discoverable as the menu bar expands |
| Put all current commands only in toolbar | Rejected | Scales poorly once file, backend, view, tools, and pipeline commands are added |
| Implement all menus immediately | Rejected | Would create false affordances before owner modules exist |
| Default to two independent source/processed windows | Rejected | Large images make dual-window synchronization expensive and error-prone; a single comparison viewport keeps zoom, pan, cursor, and W/L state aligned |

---

## 8. Pipeline Menu Activation Timing Matrix (v1.1 Addendum)

각 Pipeline 명령의 활성화 조건과 구현 시점을 정의한다. 명령은 owner module DLL이 존재하고 상태가 `IsNativeReady`일 때만 enabled.

| Command | Owner DLL | Phase | Enabled When |
|---------|-----------|:-----:|--------------|
| `Run Preprocessing` | xpe_preprocess.dll | **1a** | P1A DLL present + image loaded + calibration paths valid |
| `Run Deterministic Baseline` | xpe_preprocess.dll | 1a | Same + deterministic test mode active |
| `Apply Display Pipeline` | xpe_display.dll | **1b** | P1B DLL present + image loaded + VOI params set |
| `Run Full Pipeline` | all modules | **1b** | Pre + Display + (optional Enhance) all ready |
| `Stop Processing` | N/A | 1a+ | IsProcessing == true |
| `Stage Timing` | N/A | 1a+ | Last pipeline execution completed |
| `Open Pipeline Diagnostics` | N/A | 1a+ | Always enabled after first pipeline run |
| `Apply Enhance (Basic)` | xpe_enhance_basic.dll | **2** | Basic enhance ready |
| `Apply Enhance (Advanced)` | xpe_enhance_advanced.dll | **2** | Advanced enhance ready (premium tier) |
| `Apply Grid Suppression` | gsvg.dll | 2 | GSVG DLL ready |
| `Apply AI Assist` | xpe_ai.dll | **3** | AI module ready + safety gate passed |

Rules:

- [HARD] 각 명령은 **disabled 상태에서도 메뉴에 표시**하되, tooltip으로 비활성 사유 (예: "xpe_preprocess.dll not staged") 제공
- [HARD] Phase 이전 DLL이 accidentally staged 될 경우 command 활성화 차단 (version pinning으로 방어, SPEC-XPE-GUI-IT REQ-GUI-IT-053)
- [HARD] AI 관련 명령은 반드시 confidence score 표시 및 fallback 경로 제공 (HAZ-GUI-005 연계)

---

## 9. Diagnostic Menu / Tools Command Completion (v1.1 Addendum)

### 9.1 Tools Menu 확장

| Command | Phase | Enabled When | Owner | E2E |
|---------|:-----:|--------------|-------|-----|
| `Calibration Settings` | S0 | Always | AppSettings | W-10 |
| `Calibration Evaluation` | 1a | Image + calibration loaded | Preprocess | (planned) |
| `Fixture Manager` | S0 | Always | Static | W-10 |
| `Run Self-Check` | 0 | Backend initialized | Backend | S-03 |
| `Run GUI E2E` | S0 | Always (headless mode) | E2E runner | — |
| `Benchmark Runner` | 1a+ | At least one pipeline ready | Benchmarks | — |
| `QA Constancy` | 2+ | Post-production ready | QA | — |
| `Open Evidence Folder` | S0 | Always | OS explorer | — |
| `Export Automation Report` | S0 | Any E2E run completed | E2E runner | W-09 |
| `GSDF Calibrate...` | **1b** | xpe_display.dll ready | Display | (planned) |
| `Dll Diagnostics` | 0 | Always | Backend | S-05 |

### 9.2 View Menu — Panel Visibility

| Toggle | Default | Phase Activated |
|--------|:-------:|:---------------:|
| `Show Runtime Panel` | ON | 0 |
| `Show Raw Settings` | ON | 0 |
| `Show Calibration Evaluation` | OFF | 1a |
| `Show Display Settings` | OFF | **1b** |
| `Show Logs` | OFF (toggle) | 0 |
| `Show Alerts` | OFF (badge) | 0 |
| `Show Preprocess Stage Timings` | OFF | 1a |
| `Show Display Stage Timings` | OFF | 1b |

### 9.3 Compare Mode Commands

| Mode | Description | Keyboard |
|------|-------------|----------|
| `Swipe` | 좌우 swipe bar로 비교 | F5 |
| `Split` | 수직/수평 분할, 독립 이동 불가 | F6 |
| `Overlay` | opacity slider로 겹침 | F7 |
| `Difference` | |source - processed| 히트맵 | F8 |
| `Source Only` | processed 숨김 | Ctrl+1 |
| `Processed Only` | source 숨김 | Ctrl+2 |

---

## 10. Keyboard Shortcut Master Matrix (v1.1 Addendum)

XPE-GUI-ACCESS-001 §5.2 Keyboard Navigation과 단일 source-of-truth. 모든 shortcut은 Microsoft WPF 컨벤션 준수.

### 10.1 Global Shortcuts

| Category | Command | Shortcut | Phase |
|----------|---------|----------|:-----:|
| **File** | Open Raw | `Ctrl+O` | S0 |
| | Save Settings | `Ctrl+S` | S0 |
| | Export Automation Report | `Ctrl+Shift+E` | S0 |
| | Exit | `Alt+F4` | S0 |
| **Backend** | Initialize Backend | `Ctrl+B, I` (chord) | 0 |
| | Shutdown Backend | `Ctrl+B, S` | 0 |
| | Toggle Backend Mode | `Ctrl+B, M` | 0 |
| **View** | Zoom In | `Ctrl++` | S0 |
| | Zoom Out | `Ctrl+-` | S0 |
| | Zoom Fit | `Ctrl+0` | S0 |
| | Zoom 100% | `Ctrl+1` | S0 |
| | Reset Layout | `Ctrl+Shift+R` | S0 |
| | Toggle Panel | `Ctrl+Tab` | S0 |
| **Compare** | Swipe Mode | `F5` | S0 |
| | Split Mode | `F6` | S0 |
| | Overlay Mode | `F7` | S0 |
| | Difference Mode | `F8` | S0 |
| **Pipeline** | Run Preprocessing | `F9` | 1a |
| | Run Full Pipeline | `F10` | 1b |
| | Stop Processing | `Esc` | 1a+ |
| | Apply Display Pipeline | `Shift+F10` | 1b |
| **Tools** | Run Self-Check | `Ctrl+T, S` | 0 |
| | Fixture Manager | `Ctrl+T, F` | S0 |
| | GUI E2E | `Ctrl+T, E` | S0 |
| **Help** | Help Home | `F1` | S0 |
| | Context Help | `Shift+F1` | S0 |
| | About | `Ctrl+F1` | S0 |

### 10.2 Rules

- [HARD] 모든 shortcut은 `RoutedCommand` + `InputBinding` + `KeyBinding`으로 구현 (ARCH-001 §3.2)
- [HARD] Chord shortcut (`Ctrl+B, I`)은 WPF `InputGestureCollection`으로 구현 — 복잡도 감안 Phase 2 이상 배정
- [HARD] Shortcut 충돌 검증: pre-commit hook에서 중복 gesture 감지
- [HARD] Mnemonic(Alt+글자)은 §5.3 Access Keys 규칙 준수
- [HARD] 한국어 locale에서도 동일 키 유지 (XPE-GUI-L10N-001 §4.3)

---

## 11. Command Quality Contract (v1.1 Extended)

§6 Command Quality Rules 확장. 모든 메뉴 명령은 다음 필드를 가진 **CommandSpecification** 기록 보유:

| Field | Required | Example |
|-------|:--------:|---------|
| `Id` | ✓ | `Cmd.Pipeline.RunPreprocessing` |
| `AutomationId` | ✓ | `XPE_Menu_Pipeline_RunPreprocessing_MenuItem` |
| `HeaderKey` (RESX key) | ✓ | `Menu_Pipeline_RunPreprocessing` |
| `Owner` | ✓ | `PreprocessPipelineViewModel` |
| `Phase` | ✓ | `1a` |
| `EnabledCondition` | ✓ | `IsNativeReady && HasImage && HasCalibration` |
| `Shortcut` | optional | `F9` |
| `HelpTarget` | ✓ | `help/pipeline/run-preprocessing.html` |
| `E2EScenario` | optional | `W-02` |
| `Hazard` | optional | `HAZ-GUI-005` |

이 명세는 `Commands/CommandSpecification.cs` 전역 레지스트리로 유지 — FlaUI E2E 및 접근성 도구가 consume.

---

*Document End — XPE-GUI-MENU-001 v1.1.0*