# GUI-C-65 — 메뉴를 현재 레이아웃에 맞춘다 (#165)

> **이번 실행이 쓴 네이티브**: run **34535620953** · head `83ffa7ae…` ·
> `xpe_preprocess.dll` md5 `9f81d2f07fb7f7e23e90c86332467105`.

- 카드: GUI-C-65 · Refs #165 #149 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 1건 — 미푸시
- **제 단언 하나가 틀린 이유로 통과하고 있었고, Native 실행이 그걸 잡았습니다**(§4).
- **결과: Native 3회 0 실패(79 s) · Mock 0/64/1/65 · 통합 0/198/1/199 · slnx 0경고 0오류**

---

## 1. 세 갈래 처리

| 대상 | 처리 | 근거 |
|---|---|---|
| `Show Logs` | **배선** | 대상 영역이 존재합니다(`AnalysisPanel` 의 log 영역) |
| `Runtime Panel` · `Raw Settings Panel` · `Alerts Panel` | **메뉴에서 제거** | 대상이 없습니다 — 워크벤치 재설계로 교체된 패널들 |
| `Calibration Paths Panel`(1a) · `Display Settings Panel`(1b) | **비활성** | 예정된 단계가 있습니다. 헤더에 단계를 적었습니다 |

**배선 규칙은 판정대로**: 탭이 `log` **이고** 토글이 켜졌을 때 보입니다. **토글은 탭을
건드리지 않습니다** — 새 `TabAndToggleVisibilityConverter` 가 두 조건을 AND 로 읽고, 되돌리기는
지원하지 않습니다(그러면 컨버터가 쓰기 주체가 됩니다).

**기본값은 표대로 `ShowLogsPanel = false`.** 나머지 여섯 속성의 초기값은 건드리지
않았습니다 — 어차피 아무도 읽지 않고, 지금 바꾸면 자동화 보고서의 값만 흔듭니다.

**비활성 둘은 기존 기제에 등록했습니다** — `MainWindow.xaml.cs` 의
`DisabledFutureCommandCount` 배열에 넣어, 보고서가 "예정된 비활성 명령" 으로 세게 했습니다.

## 2. 단언 3종 — 서로 다른 주장

| 시나리오 | 주장 |
|---|---|
| `S06_ReplacedPanelToggle_IsNotInTheMenu` (3건) | 제거한 것은 메뉴에 없다 |
| `S07_ScheduledPanelToggle_IsPresentButDisabled` (2건) | 예정된 것은 있고, 비활성이다 |
| `S08_LogsToggle_ShowsAndHidesTheLogRegion` | 배선된 것은 화면을 바꾸고, **끄면 돌아온다** |

```
S08 descendants off=139 on=155 offAgain=139 (LogListBox off=False on=True)
```

C-63 의 척도를 그대로 썼습니다. **양쪽 다 메뉴가 닫힌 상태**에서 셉니다 — 열린 메뉴는 팝업
요소를 더하고(C-64 에서 228), 그 차이를 가로질러 비교하면 메뉴를 재게 됩니다.

### 반증 — 각각 따로 터집니다

| 주입 | 실패한 단언 |
|---|---|
| 제거한 항목(`Alerts`)을 되살림 | **S06 1건만** |
| 비활성을 풀어 활성으로 | **S07 1건만** |
| 로그 토글 바인딩을 다른 속성으로 | **S08 1건만** |

## 3. 할 일 2 — A 측정: **같은 것이 아닙니다**

**코드의 `Calibration Paths Panel` 이 실제로 보여 주는 것: 없습니다.**

- `ShowCalibrationPanel` 을 읽는 `Visibility` 바인딩 0건 — 다른 여섯과 같습니다.
- 그 이름이 가리키는 개념은 **교정 디렉터리 경로**입니다 —
  `AppSettings.OffsetCalibrationDirectory`/`Gain`/`Defect` 와
  `Browse*CalibrationDirectoryCommand` 3개. **그 셋을 바인딩하는 XAML 도 0건**입니다.
- 표의 `Show Calibration **Evaluation**`(1a)은 **교정 결과를 평가**하는 것이고, 코드에 대응물이
  없습니다.

**결론: "경로" 와 "평가" 는 다른 개념이고, 코드 쪽은 어느 쪽도 화면에 없습니다.**

**덤으로 나온 것**: `Tools → Calibration Settings` 명령이
`StatusText = "Calibration settings panel visible."` 라고 적고 `ShowCalibrationPanel = true` 를
씁니다(`MainWindowViewModel.cs:573-577`). **보이는 패널이 없으므로 그 문장은 사실이 아닙니다.**
이 카드가 만든 것이 아니고 고치지 않았습니다.

### C 측정 — Stage Timings

표의 `Show Preprocess Stage Timings`·`Show Display Stage Timings` 에 해당하는 **View 토글은
없습니다.** 가장 가까운 것은 **Pipeline 메뉴의 `StageTimingMenuItem`("Stage _Timing")** 이고,
이미 `IsEnabled="False"` 이며 기존 비활성 목록에 들어 있습니다. **View 패널 토글과는 다른
항목**입니다.

## 4. 제 단언이 틀린 이유로 통과하고 있었습니다

첫 Native 3회에서 **1회가 붉었고**, 그 실행의 실패 넷이 전부 "요소를 못 찾음" 이었습니다:

```
S05_RuntimePanel_ShowsBackendVersion : RuntimeCommonVersionText was not found.
S08_LogsToggle_…                     : NullReferenceException
S07_… (Display/Calibration)          : '…MenuItem' is gone
```

**S05 가 상태 표시줄을 못 찾았다는 것은 창 요소 전체가 읽히지 않았다는 뜻입니다**(#136 계열,
그 실행의 재획득 노트 26건). 그런데 **같은 실행에서 `S06` 은 3건 모두 통과했습니다** —
**부재를 단언하는데 아무것도 안 읽히면 그냥 통과하기 때문**입니다.

**대조를 넣어 고쳤습니다**: 같은 메뉴에서 **반드시 있어야 하는 항목**(`ShowLogsPanelMenuItem`)을
함께 찾고, 그것이 없으면 "이 실행은 판단할 수 없다" 로 실패합니다.

**이 수정은 간헐 실패를 없애지 않습니다 — 오히려 붉게 만들 수 있습니다.** 고친 뒤 Native 3회가
0 실패였지만, **그것은 우연이지 증거가 아닙니다**(원인은 #136/#163 이고 이 카드 밖입니다).
**수정이 바꾼 것은 "조용히 통과" 가 "정직하게 실패" 가 된 것**입니다.

## 5. 실측 (verbatim)

```
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개 / 오류 0개

dotnet test …E2ETests… --no-build                      (Mock)
통과!  - 실패: 0, 통과: 64, 건너뜀: 1, 전체: 65 (37 s)

dotnet test …IntegrationTests… -c Debug
통과!  - 실패: 0, 통과: 198, 건너뜀: 1, 전체: 199

Repeat-E2E.ps1 -Times 3 -Backend Native → runs with a failure: 0 / 3
run 1 79s · run 2 78.7s · run 3 78.9s
```

**시간 예산**: Native 벽시계 **70 s → 79 s**(+9 s, 시나리오 6건). 게이트 180 s 대비 여유 약
**101 s**. Mock 은 28 s → 37 s.

Baseline 귀속: E2E C-63 시점 **59** → **65**(6건). 통합 199 불변.

## 6. 미검증 (Gaps)

- **간헐 실패의 빈도를 다시 재지 않았습니다.** 수정 전 1/3, 수정 후 0/3 — **둘 다 표본이 3
  입니다.** C-61 에서 3회로 "결정적" 을 말한 것이 틀렸던 그 자리입니다.
- **제거한 세 속성(`ShowRuntimePanel` 등)은 남겨 두었습니다.** 메뉴 항목만 지웠고, 속성과
  자동화 보고서 필드는 그대로입니다 — 이제 아무도 쓰지 않으므로 보고서가 항상 같은 값을 냅니다.
- **`Image Summary` · `Metadata / Notes` 토글은 그대로 두었습니다**(판정 B). **다만 이 둘도
  측정상 제거한 셋과 같은 상태입니다** — 대상 영역이 없고 눌러도 화면이 안 바뀝니다(C-63).
  판정 B 의 근거("동작하는 것")가 이 둘에는 해당하지 않습니다. **고치지 않고 올립니다.**
- **`ResetLayout` 이 배선 뒤에 실제로 되살리는지 확인하지 않았습니다.** `ShowLogsPanel = true` 로
  되돌리므로 코드상으로는 켜지지만, 실행으로 보지 않았습니다.
- **화면 픽셀은 보지 않았습니다.** "화면이 바뀐다" 의 근거는 UIA 자손 수입니다.

## 7. 잔여 위험 (Residual risk)

- **거절 신호가 한 걸음 멀어졌습니다.** `Show Logs` 기본값이 OFF 가 되어, C-63 이 살린 기동
  거절 줄을 보려면 **토글을 켜고 Log 탭을 고르는 두 단계**가 필요합니다. 판정(표의 기본값)을
  따른 결과이고, #161 의 도달성이 그만큼 낮아집니다.
- **`Tools → Calibration Settings` 가 없는 패널을 보였다고 말합니다**(§3).
- **자동화 보고서의 패널 가시성 필드 셋이 이제 고정값입니다** — 읽는 쪽이 있으면 의미가
  바뀝니다.
- **새 시나리오 6건이 Native 에 9 s 를 더했습니다.**

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.E2ETests/… --no-build \
  --filter "FullyQualifiedName~PanelToggleScenarios" --logger "console;verbosity=detailed"
grep -rn "OffsetCalibrationDirectory\|BrowseOffset" gui/ImageProcTest --include=*.xaml   # 0건
powershell -File clients/ImageProcTest.E2ETests/Staging/Repeat-E2E.ps1 \
  -Times 3 -Backend Native -NativeDir build/ci-common/bin -ResultsDirectory build/e2e-c65b
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
```
