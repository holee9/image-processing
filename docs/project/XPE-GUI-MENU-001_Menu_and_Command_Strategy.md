# GUI Menu and Command Strategy

**Document ID**: XPE-GUI-MENU-001  
**Version**: 1.0.0  
**Date**: 2026-04-16  
**Status**: Controlled Draft  
**Canonical Scope**: `docs/project/`

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
