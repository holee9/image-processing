# GUI-C-66 — 나머지 둘 제거, 거짓 로그 census, §9.2 입력 (#165)

> **이번 실행이 쓴 네이티브**: run **34535620953** · head `83ffa7ae…` ·
> `xpe_preprocess.dll` md5 `9f81d2f07fb7f7e23e90c86332467105`.

- 카드: GUI-C-66 · Refs #165 #149 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 1건 — 미푸시
- **결과: Native 3회 0 실패(81 s) · Mock 0/66/1/67 · 통합 0/198/1/199 · slnx 0경고 0오류**

---

## 1. 나머지 둘 제거

`ShowImageSummaryPanelMenuItem` · `ShowMetadataPanelMenuItem` 을 View 메뉴에서 뺐습니다.
**C-65 와 같은 단언 구조 그대로** — 제거 항목이 셋에서 다섯이 됐고, 대조 항목
(`ShowLogsPanelMenuItem`) 은 그대로입니다.

**반증**: `Metadata` 를 되살리면 **그 케이스 1건만** 실패합니다(8건 중 7건 통과).

**View 메뉴의 체크 항목은 이제 셋입니다** — `Logs Panel`(배선됨) ·
`Calibration Paths Panel (Phase 1a)`(비활성) · `Display Settings Panel (Phase 1b)`(비활성).

## 2. 거짓 로그 census — **둘입니다**

`gui/ImageProcTest` 의 모든 `StatusText = "…"` 와 `Log("…")` 리터럴 **36개**를 뽑아
"화면/상태가 그렇게 됐다" 고 주장하는 것만 골라, 각각 대응하는 변화가 있는지 확인했습니다.

| # | 문구 | 위치 | 판정 |
|---|---|---|---|
| 1 | `"Calibration settings panel visible."` + `Log("Menu command: calibration settings panel shown.")` | `MainWindowViewModel.cs:573-577` | **거짓** — 그 패널은 존재하지 않습니다. `ShowCalibrationPanel` 을 읽는 가시성 바인딩 0건(C-65) |
| 2 | `"Layout reset."` + `Log("Menu command: layout reset.")` | `MainWindowViewModel.cs:549-561` | **절반만 참** — 아래 |

**#2 의 내역**: `ResetLayout()` 은 여덟 번 씁니다.

```
ShowRuntimePanel · ShowRawSettingsPanel · ShowCalibrationPanel · ShowImageSummaryPanel
ShowMetadataPanel · ShowAlertsPanel · Settings.ShowDisplayPanel     ← 일곱 개, 아무도 안 읽음
ShowLogsPanel                                                       ← C-65 에서 배선됨
ResetComparisonView()                                               ← 줌·팬·모드, 실제로 바뀜
```

**여덟 중 일곱이 아무 화면도 바꾸지 않습니다.** 보이는 것은 비교 뷰 초기화와, C-65 이후로는
Logs 토글뿐입니다. **C-65 이전에는 "Layout reset." 이 사실상 전부 거짓이었습니다** — 배선이
그 문장을 조금 참으로 만들었습니다.

### 이 계열이 아닌 것 (확인하고 제외)

- `"Lane B overrides reset to defaults."` — **참**입니다. 두 값이 `AnalysisPanel.xaml:263,269`
  의 TextBox 에 묶여 있어 화면이 바뀝니다.
- `Log("Step 1: Reading DICOM file")` ~ `Step 6` — **실제 호출 바로 앞에 붙은 서술**이고 실패 시
  중단·보고합니다(`PipelineOrchestrator.cs:169-200`). 일어나지 않은 일을 보고하지 않습니다.
- `"Settings saved."` · `"Backend shutdown."` · `"Fixture pack available/missing"` — 파일·상태를
  실제로 다룹니다.
- `Log("RunOnAllQueuedCommand: not implemented (Slice 7).")` — **정직한 미구현 고지**입니다.

**고치지 않았습니다**(카드 지시).

## 3. §9.2 를 다시 쓰기 위한 입력

### (a) 제거·비활성 뒤 View 메뉴 전체 — 20항목

| 항목 | 동작 확인 상태 |
|---|---|
| `Logs Panel` (체크) | **동작**(C-66 S08: 자손 139↔155) |
| `Calibration Paths Panel (Phase 1a)` (체크) | **비활성** |
| `Display Settings Panel (Phase 1b)` (체크) | **비활성** |
| `Clear Logs` / `Clear Alerts` | 컬렉션을 비웁니다. Logs 는 화면에서 확인 가능 |
| `Reset Layout` | **절반**(§2 #2) |
| `Zoom Fit` / `Zoom 100%` / `Zoom In` / `Zoom Out` | 명령이 `ComparisonZoomScale` 을 바꾸고 뷰포트가 읽습니다. **이 카드에서 실행으로 재지는 않았습니다** |
| `Compare Mode` 하위 6 | **동작**(C-58 W-13, C-59 F6 포함) |
| `Reset Comparison View` | 줌·팬·모드를 되돌립니다 |
| `Detach Comparison Viewer` | **동작 안 함이 관측됨** — C-46 이 호출 뒤 새 창이 끝내 나타나지 않았고, 최상위 창은 작업 표시줄·메인 창·Program Manager 뿐이었습니다 |

### (b) 워크벤치 구성 요소 중 **메뉴로 제어 가능한 것: 없습니다**

| 구성 요소 | 가시성을 정하는 것 | 메뉴에서 제어되나 |
|---|---|---|
| `StudyQueue`(좌) | `FocusMode` + `LeftPanelOpen` | **아니오** |
| `AnalysisPanel`(우) | `FocusMode` + `RightPanelOpen` | **아니오** |
| `TopBar` · `AlgorithmBar` · `ViewportShell` · `VerdictBar` · 상태 표시줄 | **가시성 바인딩 없음**(항상 보임) | 아니오 |

- **`FocusMode` 는 `TopBar` 의 버튼**이 토글합니다(`TopBar.xaml:124`,
  `ToggleFocusModeCommand`). 메뉴에는 없습니다.
- **`LeftPanelOpen` · `RightPanelOpen` 은 앱 안에서 아무도 쓰지 않습니다.** 속성 setter 는
  있지만 호출자가 없고, 바인딩도 읽기 전용입니다 — **값은 `appsettings.json` 에서만 들어옵니다.**
  C-59 가 `comparisonMode` 에서 본 것과 같은 형태(설정 파일로만 도달 가능)입니다.

**§9.2 가 말할 수 있는 사실**: 지금 워크벤치에서 메뉴가 켜고 끌 수 있는 것은 **`AnalysisPanel`
의 log 영역 하나**입니다. 나머지 패널 가시성은 메뉴 밖(TopBar 버튼)이거나 어디서도 제어되지
않습니다.

### (c) `AnalysisPanel` 탭과 메뉴의 관계

탭은 넷입니다 — `metrics` · `parameters` · `runset` · `log`. **`TabControl` 이 아니라 버튼 4개 +
가시성 컨버터**이고, `SwitchAnalysisTabCommand` 가 `AnalysisTab` 을 바꿉니다.

- **메뉴와 연결된 것은 `log` 하나**뿐입니다(C-65 의 결합: 탭이 `log` **이고** 토글이 켜짐).
- **나머지 셋은 메뉴에 대응 항목이 없습니다.** 같은 결합이 기술적으로 불가능하지는
  않습니다 — `TabAndToggleVisibilityConverter` 는 탭 이름을 매개변수로 받으므로 그대로 쓸 수
  있습니다. **다만 대응하는 토글 속성이 없고, 만들지 않았습니다.**
- **의도적으로 아닌지는 코드로 알 수 없습니다** — `metrics`/`parameters`/`runset` 토글이 설계상
  불필요하다는 기록을 찾지 못했습니다. **판단하지 않고 사실만 적습니다.**

## 4. 실측 (verbatim)

```
dotnet test …E2ETests… --no-build --filter "…PanelToggleScenarios"
통과!  - 실패: 0, 통과: 8, 건너뜀: 0, 전체: 8

[cut=metadata] 실패 S06_ReplacedPanelToggle_IsNotInTheMenu(automationId: "ShowMetadataPanelMenuItem")
[cut=metadata] 실패!  - 실패: 1, 통과: 7, 전체: 8

dotnet test …E2ETests… --no-build                      (Mock)
통과!  - 실패: 0, 통과: 66, 건너뜀: 1, 전체: 67 (40 s)

dotnet test …IntegrationTests… -c Debug
통과!  - 실패: 0, 통과: 198, 건너뜀: 1, 전체: 199

Repeat-E2E.ps1 -Times 3 -Backend Native → runs with a failure: 0 / 3
run 1 81.4s · run 2 81.4s · run 3 82.2s
```

**시간 예산**: Native **79 s → 81 s**(+2 s, 단언 2건). 게이트 180 s 대비 여유 약 **99 s**.

Baseline 귀속: E2E C-65 시점 **65** → **67**(2건). 통합 199 불변.

## 5. 미검증 (Gaps)

- **Zoom 네 항목을 실행으로 재지 않았습니다.** 명령이 존재하고 뷰포트가 그 값을 읽는다는 것까지만
  코드로 확인했습니다.
- **`Detach Comparison Viewer` 를 이 카드에서 다시 재지 않았습니다** — C-46 의 관측을 인용했습니다.
- **`Clear Alerts` 의 효과를 화면에서 확인하지 못했습니다** — 알림을 표시하는 UI 자체가 없습니다
  (C-64).
- **거짓 로그 census 는 `StatusText`/`Log` 리터럴만 봤습니다.** 보간 문자열(`$"…"`) 중 변수만
  담은 것은 문구 판정 대상이 아니었고, 툴팁·헤더·`AutomationProperties.HelpText` 는 보지
  않았습니다. **범위를 이렇게 잡았다고 적습니다.**
- **탭 셋(`metrics`/`parameters`/`runset`)에 토글이 없는 것이 의도인지 모릅니다**(§3c).
- **`LeftPanelOpen`/`RightPanelOpen` 을 설정 파일로 바꿔 띄워 보지는 않았습니다** — 코드 경로로만
  확인했습니다. C-59 에서 `comparisonMode` 로 같은 형태를 실행 확인한 적은 있습니다.

## 6. 잔여 위험 (Residual risk)

- **거짓 로그 둘이 그대로 있습니다**(고치지 말라는 지시). 나중에 로그를 읽는 사람은 교정 패널이
  보였다고 믿습니다.
- **제거한 다섯 항목의 속성과 자동화 보고서 필드는 남아 있습니다.** 이제 아무도 쓰지 않으므로
  보고서의 패널 가시성 필드는 고정값입니다.
- **`ResetLayout` 이 여전히 일곱 개의 죽은 속성을 씁니다.** 무해하지만, 읽는 사람에게는 무언가
  복원되는 것처럼 보입니다.
- **메뉴로 제어 가능한 워크벤치 요소가 하나뿐**이라는 것은 §9.2 가 축소된다는 뜻입니다 — 표가
  줄어드는 것이 기능 축소로 읽히지 않도록 적는 것은 문서 쪽 몫입니다.

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
grep -rhoE '(StatusText = "[^"]*"|Log\("[^"]*")' gui/ImageProcTest --include=*.cs | sort -u
grep -rn "FocusMode\|RightPanelOpen\|LeftPanelOpen" gui/ImageProcTest --include=*.xaml --include=*.cs
dotnet test clients/ImageProcTest.E2ETests/… --no-build --filter "…PanelToggleScenarios"
powershell -File clients/ImageProcTest.E2ETests/Staging/Repeat-E2E.ps1 \
  -Times 3 -Backend Native -NativeDir build/ci-common/bin -ResultsDirectory build/e2e-c66
```
