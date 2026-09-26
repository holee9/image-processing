# GUI-C-64 — 표와 코드 대조 (#165)

> **이번 실행이 쓴 네이티브**: run **34535620953** · head `83ffa7ae…` ·
> `xpe_preprocess.dll` md5 `9f81d2f07fb7f7e23e90c86332467105`.

- 카드: GUI-C-64 · Refs #165 #149 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- **커밋 없음 — 배선하지 않았습니다.** 이유는 §3 입니다.
- **할 일 1(대조) 완료 · 할 일 2(배선) 보류 — 판정 필요**
- **결과: slnx 0경고 0오류 · 소스 변경 없음**

---

## 1. 대조표 — 표 8행 대 코드 8항목

### 이름 대응

| §9.2 표 | Phase | 코드 메뉴 항목 (AutomationId) | 코드 Header | 속성 | 이름 판정 |
|---|:---:|---|---|---|---|
| `Show Runtime Panel` | 0 | `ShowRuntimePanelMenuItem` | "Runtime Panel" | `ShowRuntimePanel` | 일치 |
| `Show Raw Settings` | 0 | `ShowRawSettingsPanelMenuItem` | "Raw Settings **Panel**" | `ShowRawSettingsPanel` | 사실상 일치 |
| `Show Logs` | 0 | `ShowLogsPanelMenuItem` | "Logs **Panel**" | `ShowLogsPanel` | 사실상 일치 |
| `Show Alerts` | 0 | `ShowAlertsPanelMenuItem` | "Alerts **Panel**" | `ShowAlertsPanel` | 사실상 일치 |
| `Show Calibration **Evaluation**` | 1a | `ShowCalibrationPanelMenuItem` | "Calibration **Paths** Panel" | `ShowCalibrationPanel` | **다름 — 판정 필요** |
| `Show Display Settings` | 1b | `ShowDisplaySettingsPanelMenuItem` | "Display Settings Panel" | `Settings.ShowDisplayPanel` | 일치 (속성만 `Settings.` 아래) |
| `Show Preprocess Stage Timings` | 1a | **없음** | — | — | **코드에 없음** |
| `Show Display Stage Timings` | 1b | **없음** | — | — | **코드에 없음** |
| — | — | `ShowImageSummaryPanelMenuItem` | "Image Summary Panel" | `ShowImageSummaryPanel` | **표에 없음** |
| — | — | `ShowMetadataPanelMenuItem` | "Metadata / Notes Panel" | `ShowMetadataPanel` | **표에 없음** |

**요약: 6행은 대응, 2행은 코드에 없음, 2항목은 표에 없음.** §4.3 의 열거(`Show Runtime Panel` ·
`Show Raw Settings` · `Show Calibration Evaluation` · `Show Logs` · `Show Alerts`)도 같은 다섯이고,
Image Summary·Metadata 는 거기에도 없습니다.

### 기본값 대조 — 표와 코드가 다릅니다

| 표 (§9.2) | 표의 기본값 | 코드의 초기값 | |
|---|:---:|:---:|---|
| `Show Runtime Panel` | ON | `true` | 일치 |
| `Show Raw Settings` | ON | `true` | 일치 |
| `Show Logs` | **OFF** (toggle) | `true` | **다름** |
| `Show Alerts` | **OFF** (badge) | `true` | **다름** |
| `Show Calibration Evaluation` | OFF | `true` | 다름 |
| `Show Display Settings` | OFF | `true` | 다름 |

코드는 일곱 속성 모두 `= true` 로 시작하고(`MainWindowViewModel.cs:32-38`),
`Settings.ShowDisplayPanel` 도 마찬가지입니다. **표가 OFF 라고 한 넷이 전부 ON 입니다.**

### 활성화 단계 — **여덟 전부 활성입니다**

카드가 물은 것입니다. 실행으로 쟀습니다:

```
PHASEPROBE ShowRuntimePanelMenuItem:      enabled=True check On->Off
PHASEPROBE ShowRawSettingsPanelMenuItem:  enabled=True check On->Off
PHASEPROBE ShowCalibrationPanelMenuItem:  enabled=True check On->Off
PHASEPROBE ShowDisplaySettingsPanelMenuItem: enabled=True check On->Off
PHASEPROBE ShowImageSummaryPanelMenuItem: enabled=True check On->Off
PHASEPROBE ShowMetadataPanelMenuItem:     enabled=True check On->Off
PHASEPROBE ShowLogsPanelMenuItem:         enabled=True check On->Off
PHASEPROBE ShowAlertsPanelMenuItem:       enabled=True check On->Off
```

**표가 1a·1b 로 지정한 넷(Calibration Evaluation · Display Settings · 두 Timings) 중 코드에
있는 둘이 지금 활성입니다.** `MainWindow.xaml.cs:254` 의 "비활성이어야 한다" 목록에는 이 여덟이
하나도 없습니다 — 그 목록은 `OpenRecentMenuItem`·`OpenRuntimeLogsMenuItem` 등 다른 항목들입니다.

**`ShowRuntimePanelMenuItem` 도 이번에 쟀습니다** — C-63 에서 못 쟀던 그것이고, 나머지 일곱과
같습니다. (C-63 은 첫 순회에서 메뉴가 열리기 전에 찾아 놓쳤습니다. 이번엔 메뉴를 한 번 미리
열고 시작했습니다.)

> **수치 읽는 법**: 이 프로브의 `descendants 228->146` 은 **열린 메뉴가 닫힌 것**이지 패널 변화가
> 아닙니다(전은 메뉴가 열린 상태, 후는 닫힌 상태에서 셈). 패널이 안 바뀐다는 근거는 C-63 의
> **146→146**(양쪽 다 메뉴가 닫힌 상태)입니다.

## 2. 그래서 대조에서 나온 판정 대상 넷

| # | 판정할 것 |
|---|---|
| A | `Show Calibration Evaluation`(표) 대 `Calibration Paths Panel`(코드) — **같은 것인가**, 이름만 다른가, 아니면 서로 다른 기능인가. "경로" 와 "평가" 는 다른 말입니다 |
| B | 코드에만 있는 둘(`Image Summary` · `Metadata / Notes`) — 표에 넣을 것인가, 코드에서 뺄 것인가 |
| C | 표에만 있는 둘(`Preprocess Stage Timings` · `Display Stage Timings`) — 1a·1b 이므로 지금은 없어도 결함이 아닙니다. 확인만 |
| D | 기본값 — 표는 Logs·Alerts 를 OFF 로, 코드는 ON 으로 시작합니다. **배선하면 이 차이가 보이기 시작합니다** |

## 3. 배선을 멈춘 이유 — **넷 중 셋은 켜고 끌 대상이 없습니다**

카드는 "Phase 0 넷만 배선" 을 지시했습니다. 대상을 찾다가 멈췄습니다.

| Phase 0 토글 | 화면에 그 패널이 있는가 | 근거 |
|---|---|---|
| `Show Logs` | **있습니다** | `AnalysisPanel.xaml:337-370`, `AnalysisTab == "log"` 일 때 보이는 영역 |
| `Show Runtime Panel` | **없습니다** | 런타임 정보는 **상태 표시줄 항목 한 개**뿐입니다(`MainWindow.xaml:431`, `RuntimeVersionSummary`). "패널" 이라 부를 영역이 없습니다 |
| `Show Raw Settings` | **없습니다** | `RawWidth`/`RawHeight`/`RawPixelFormat` 를 바인딩하는 XAML 이 **0건**입니다 |
| `Show Alerts` | **없습니다** | `Alerts` 컬렉션을 표시하는 XAML 이 **0건**입니다 — 메뉴의 `Clear Alerts` 와 툴바 버튼만 있습니다 |

**현재 레이아웃은 워크벤치 구조입니다** — `TopBar` · `AlgorithmBar` · `StudyQueue`(좌) ·
`ViewportShell`(중앙) · `AnalysisPanel`(우) · `VerdictBar`(하) · 상태 표시줄. 메뉴가 이름 부르는
"Runtime / Raw Settings / Alerts 패널" 은 **이 레이아웃에 존재하지 않습니다.**

**따라서 셋은 "배선" 이 아니라 "패널을 만드는 일" 입니다.** 없는 것을 보이고 감출 수는 없습니다.
카드가 "먼저 배선하면 이름이 굳는다" 고 한 것과 같은 이유로, **없는 패널을 지금 만들면 그
설계가 굳습니다** — 그건 제 판단 범위가 아니라고 봤습니다.

`Show Logs` 하나만 배선하는 것도 하지 않았습니다. 그 영역의 가시성은 이미 `AnalysisTab` 이
쥐고 있어서, 토글을 더하려면 **두 조건의 결합**(탭이 log 이고 + 토글이 켜짐)을 정해야 하고,
그것도 설계 결정입니다. **넷 중 하나만, 그것도 결합 규칙을 제가 정해서 넣는 것**은 이 카드가
요청한 모양이 아니라고 판단했습니다.

## 4. 미검증 (Gaps)

- **표의 두 Timings 항목이 어떤 기능을 뜻하는지 확인하지 않았습니다.** 1a·1b 이므로 범위 밖으로
  두었습니다.
- **`Calibration Paths Panel` 이 실제로 무엇을 보여 주는지 코드로 확인하지 않았습니다** — 이름
  대조까지만 했습니다. A 판정에 필요하면 재겠습니다.
- **`ResetLayout` 이 배선 뒤에 실제로 되살리는지**는 배선을 안 했으므로 확인할 수 없었습니다.
- **화면 픽셀은 보지 않았습니다.** "패널이 없다" 의 근거는 XAML 검색과 UIA 트리입니다.
- **상태 표시줄의 런타임 항목을 "Runtime Panel" 로 볼 수 있는지**는 제가 정할 문제가
  아닙니다 — §2 A 와 같은 종류의 판정입니다.

## 5. 잔여 위험 (Residual risk)

- **여덟 토글이 여전히 아무 일도 하지 않습니다.** 체크만 바뀌고 화면은 그대로입니다(C-63 측정).
- **표가 OFF 라고 한 넷이 ON 으로 시작합니다.** 배선되는 순간 사용자는 표와 다른 초기 화면을
  보게 됩니다 — 배선과 기본값을 같이 정해야 합니다.
- **1a·1b 토글 둘이 지금 활성입니다.** 눌러도 아무 일이 없으므로 당장 해는 없지만, 그 단계가
  오면 "이미 켜져 있던 것" 으로 보일 수 있습니다.

## 부록 — 사용한 명령

```bash
grep -n "### 9.2" -A 14 docs/project/XPE-GUI-MENU-001_Menu_and_Command_Strategy.md
grep -rn "RawWidth\|RawHeight\|RawPixelFormat" gui/ImageProcTest --include=*.xaml   # 0건
grep -rn "Alerts" gui/ImageProcTest --include=*.xaml                                 # 메뉴/버튼뿐
dotnet test clients/ImageProcTest.E2ETests/… --no-build \
  --filter "FullyQualifiedName~TempTogglePhaseProbe" --logger "console;verbosity=detailed"   # 임시, 삭제됨
dotnet build clients/ImageProcTest.slnx -c Debug
```
