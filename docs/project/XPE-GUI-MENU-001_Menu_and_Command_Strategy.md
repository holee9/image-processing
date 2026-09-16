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
| `Source Only` | processed 숨김 | (없음 — 아래 정정) |
| `Processed Only` | source 숨김 | (없음 — 아래 정정) |

> **정정 2026-09-16 (leader, GUI-C-58 / #149) — `Ctrl+1` 은 Zoom 100% 입니다.**
>
> 이 표는 원래 `Source Only` 에 `Ctrl+1` 을, `Processed Only` 에 `Ctrl+2` 를 배정했습니다. **같은 문서 §10.1 이 `Ctrl+1` 을 Zoom 100% 로 배정하고 있고**, `XPE-GUI-ACCESS-001` §5.2 도 Zoom 100% 로 적습니다. 두 곳이 일치하고 이 표만 다르므로 **이 표가 틀렸습니다.**
> 
> §10 이 이 문서와 `ACCESS-001` §5.2 를 단일 source-of-truth 로 선언하고 있으므로, 그 두 곳이 일치하는 값이 기준입니다.
>
> **`Source Only` / `Processed Only` 는 당분간 단축키 없이 메뉴로만 닿습니다**(`View → Compare Mode`). 대체 키를 지금 정하지 않는 이유는 키 배정이 접근성 탐색·니모닉과 함께 봐야 하는 판단이고, 이 정정의 범위가 아니기 때문입니다. 필요해지면 두 문서에 동시에 적습니다.
>
> **GUI-C-58 이 이 충돌 때문에 두 키를 배선하지 않았고, 그 판단이 옳았습니다.**

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
>
> ---
>
> ## 결정 (2026-09-16, 사용자) — 진짜 차분 영상으로 고칩니다
>
> **값 정의는 `XPE-GUI-MENU-001` L288 의 절댓값 `|source − processed|` 입니다.** `XPE-GUI-COMPARE-001` L68 의 "signed or absolute" 는 절댓값을 포함하므로 두 문서가 모두 만족됩니다. 문서 문구는 고치지 않습니다 — 고칠 것은 구현입니다.
>
> 결정에 따라 만족되어야 할 성질 셋. 앞선 측정이 전부 현재 구현에서 위반된다고 보인 것들입니다.
>
> | # | 성질 | 현재 측정값 | 목표 |
> |---|---|---|---|
> | P1 | 두 입력이 **동일**하면 출력은 **단일 색**(차이 0에 대응하는 색) | 편차 255/255 | 0 |
> | P2 | **부호에 무관** — 같은 크기의 밝은/어두운 변화가 같은 출력 | 간격 38.0 | 0 |
> | P3 | 국소 차이의 **분리도가 보존** | 참 차분의 16% | 참 차분과 같은 수준 |
>
> **P1 과 P2 는 색 대응 선택과 무관하게 성립해야 합니다** — 절댓값이 부호를 지우므로 두 경우가 색 대응에 같은 값을 건네기 때문입니다. P3 만 색 대응에 의존합니다.
>
> ### 색 대응 — 선형 회색조 (leader 결정, 근거와 함께)
>
> 어느 문서에도 없었으므로 정합니다: **차이 0 = 검정, 최대 차이 = 흰색, 그 사이 선형.**
>
> 근거 셋. **이 제품의 표시 경로가 회색조입니다** — `xpe_display` 의 GSDF 교정이 회색조 지각 균일성을 다루고 있어, 진단 영상에 색을 들이면 그 교정 바깥의 축이 하나 생깁니다. **가짜 구조를 만들지 않습니다** — 무지개류 색 대응은 값이 연속인 곳에 경계를 만들어 보이게 하고, 차분 영상에서 그 경계는 없는 결함으로 읽힙니다. **P3 를 재기 쉽습니다** — 분리도가 출력값 차이로 바로 나오므로 GUI-C-54/55 의 지표가 그대로 쓰입니다.
>
> **이 선택은 되돌릴 수 있습니다.** 임상 판독에서 색 대응이 필요하다는 근거가 나오면 그때 바꾸며, P1·P2 는 어느 색 대응에서도 유지되므로 그 변경이 위 성질을 건드리지 않습니다.
>
> ---
>
> ## 완료 (2026-09-16, GUI-C-57) — 세 성질 모두 목표 도달
>
> | # | 성질 | 고치기 전 | 고친 뒤 |
> |---|---|---|---|
> | P1 | 동일 입력 → 단일 색 | 편차 255/255 | **0** (4쌍 전부) |
> | P2 | 부호 무관 | 간격 38.0 | **0.0** |
> | P3 | 국소 분리도 보존 | 참 차분의 37% | **99.7%** (63.6 대 63.8) |
>
> 곁가지로 작은 차이의 가독성이 함께 올랐습니다 — Δ8 의 신호/바닥이 **1.61배 → 18.70배**(바닥 3.3 → 0.4).
>
> ### 앞 문단들의 수치 중 셋을 정정합니다
>
> 구현을 고치면서 **측정 쪽 결함 세 가지**가 함께 드러났습니다. 위 표의 "고치기 전" 열은 정정된 값입니다.
>
> 1. **P3 의 "16%" 는 과소 보고였습니다.** 배경 128 에 +255 패치는 **클램프되어 실제 차이가 127** 인데, 참조를 요청한 Δ(255)로 만들어 참 차분을 251 로 보고했습니다. 참조를 실제 실린 차이로 고치면(126.8) 그 행의 비율이 올라갑니다. **위 표의 37% 가 정정값입니다.**
> 2. **배경 마스크에 하단 라벨이 섞여 있었습니다.** 차분 모드만 그리는 라벨인데, 진짜 차분은 안 바뀐 영상을 검정으로 그리므로 **라벨이 배경에서 가장 밝은 것**이 됩니다. 동일 입력 바닥이 −5.6(크롬 포함) → **0.0**(제외)으로 바뀝니다.
> 3. **지표 추출이 절반만 돼 있었습니다.** GUI-C-55 가 harness 로 옮긴 것은 그 카드의 파일이고, GUI-C-54 의 파일은 **자기 복사본을 그대로 들고 있었습니다**(알고리즘은 동일). "측정과 반증이 같은 코드를 쓴다"는 주장은 **그 카드의 파일에 대해서만 참**이었습니다.
>
> 앞 문단들의 나머지 수치(동일 입력 편차 255/255, 부호 간격 38.0)는 정정 대상이 아니며, 고친 뒤 0 과 0.0 이 됐습니다.
>
> **다른 비교 모드는 바뀌지 않았습니다** — `SourceOnly` / `ProcessedOnly` 의 기존 단언이 그대로 통과합니다.
>
> ### 미검증
>
> 실제 해부 구조 영상 없음(합성 입력만), 색 입력 미측정, 확대/축소 배율 미측정, 크기 불일치 대체 경로 미단언, **사람이 보기에 나아졌는지는 재지 않았습니다.**

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