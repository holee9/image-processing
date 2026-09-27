# GUI-C-136 — `#201`(a): 눌러 봤다. **사용자가 보는 것이 변하지 않는다**

> 단계 A(관측)와 단계 B(비용)만 했다. **고치지 않았고 방향도 고르지 않았다.**

## 1. 단계 A — 표

`Clear Alerts` 를 자동화로 **단독으로** 눌렀다(Mock 백엔드, 앱 인스턴스 1개).

| 대상 | 전 | `Clear Alerts` 후 | (대조군) `Clear Logs` 후 |
|---|---|---|---|
| 화면 로그 줄 수 (`LogListBox`) | **11** | **11** | **0** |
| 그중 알림 줄 수 (`ALERT ` 접두) | **3** | **3** | **0** |
| `Alerts` 컬렉션 개수 | — 측정 불가 (§1.2) | — | — |

**`#201`(a) 가 관측으로 확정됐다.** 누르기 전후로 화면이 **한 줄도** 바뀌지 않는다.

### 1.1 대조군을 **양성**으로 바꿨다 — 그 이유

카드는 "알림 0건 상태" 를 대조군으로 지시했다. 그 팔은 **"변화 없음"** 을 내는데, 그것은
본 실험의 결과와 **같은 읽기**다 — 즉 *"도구가 버튼을 못 눌렀다"* 와 구별되지 않는다.
카드가 경고한 바로 그 문제다.

그래서 **같은 실행에서 `Clear Logs` 를 눌러** 화면이 실제로 줄어드는 것을 요구했다
(11 → 0). **누르기와 관측이 살아 있다**는 증거가 같은 실행 안에 있다.

### 1.2 `Alerts` 컬렉션 개수는 자동화로 볼 수 없다 — 그리고 그것이 (a)의 일부다

`Alerts` 에 바인딩된 요소가 없으므로 UIA 로 읽을 길이 없다. **개수를 기록하는 계기가
하나 있는데, 그것이 두 버튼을 함께 누른다:**

```
MainWindow.xaml.cs:378-384   ClickButton(ClearLogsButton);
                             ClickButton(ClearAlertsButton);
                             report.LogCountAfterClear   = viewModel.Logs.Count;
                             report.AlertCountAfterClear = viewModel.Alerts.Count;
:419-420  Passed 조건:  LogCountAfterClear == 0 && AlertCountAfterClear == 0
```

**그래서 기존 자동화 증거로는 "어느 버튼이 무엇을 했는지" 를 가릴 수 없다.** 둘 다 0 이
되지만 그것은 `Clear Logs` 가 로그를 비웠기 때문일 수 있다. 이 카드가 `Clear Alerts` 만
누른 이유다.

## 2. 단계 B — 두 방향의 비용

### 2.1 방향 1 — `Alerts` 에 전용 표시 요소

**바뀌는 파일: 2 (심각도 구별까지 하면 3)**

| 파일 | 무엇 |
|---|---|
| `gui/ImageProcTest/MainWindow.xaml` | 패널 + 메뉴 항목 + `AutomationId` |
| `gui/ImageProcTest/ViewModels/MainWindowViewModel.cs` | `ShowAlertsPanel`(:728)이 다시 읽히게 |
| (+`(b)` 시) 변환기 1개 | 심각도 → 색·아이콘 |

**영향받는 기존 시험: 1건 — 그리고 그 1건이 "되돌리기" 라는 것을 말해 준다**

```
PanelToggleScenarios.cs:35-46  RemovedItems()
  "Toggles whose panels the workbench redesign replaced; they are gone from the menu."
  [..., "ShowAlertsPanelMenuItem", ...]        ← 없어진 것을 단언한다
```

`ShowAlertsPanelMenuItem` 은 **`GUI-C-65` 에서 의도적으로 제거**됐고, VM 주석이 그것을
적어 둔다:

```
MainWindowViewModel.cs:688-691
  "No readers since GUI-C-68: … ShowAlertsPanel name panels that do not exist —
   their menu items were removed (C-65) …"
```

**즉 방향 1 은 새로 만드는 것이 아니라 워크벤치 재설계가 걷어낸 것을 되살리는 것이다.**
그 시험 1건은 "고치면 깨지는" 것이 아니라 **"결정을 뒤집는다는 표시"** 다 — 목록에서
옮겨야 하고, 옮기는 순간 C-65 판단을 되돌린 기록이 된다.

**중복은 따로 결정해야 한다**: 패널과 로그 양쪽에 같은 알림이 뜬다. `AlertVisibilityScenarios`
는 알림이 **로그에** 있는 것을 단언하므로(`:63-71`) 중복을 두면 **깨지지 않는다**(0건).
중복을 없애려면 그 시험이 대상이 된다.

### 2.2 방향 2 — 로그 한 곳으로 통일

**두 하위 변형의 비용이 크게 다르다.**

#### 2a. `Alerts` 컬렉션은 두고, `Clear` 가 **로그의 알림 줄까지** 지운다

**바뀌는 파일: 1**

| 파일 | 무엇 |
|---|---|
| `MainWindowViewModel.cs` | `ClearAlertsCommand`(:121)가 `Logs` 에서 `ALERT ` 줄도 제거 |

**영향받는 기존 시험: 0건.** 새로 깨지는 것은 **이 카드가 만든 관측 시험 1건**인데, 그것은
설계대로다 — (a)가 고쳐지면 "변하지 않는다" 단언이 뒤집히고, 그것이 §4 의 반증이 된다.

#### 2b. `Alerts` 컬렉션을 없앤다

**바뀌는 파일: 5**

| 파일 | 참조 수 |
|---|---|
| `MainWindowViewModel.cs` | 3 |
| `MainWindow.xaml` | 6 |
| `MainWindow.xaml.cs` | 2 (+ 보고서 필드 기록) |
| `Models/GuiAutomationReport.cs` | 알림 개수 필드 3개 |
| `gui/ImageProcTest.E2E/Program.cs` | 2 |

그리고 알림을 **생산하는** 쪽도 `Alerts` 를 지난다 — `IXpeBackend`, `MockXpeBackend`,
`RealXpeBackend`, `NativeAlertDrain`, `FaultInjectingBackend`, `Models/AlertEntry.cs`.

**영향받는 기존 시험: 2건**

| 시험 | 왜 |
|---|---|
| `StartupRejectionSurvivesTests.cs:82` | `Assert.DoesNotContain("Alerts.Clear();", body)` — **소스 가드**다. 컬렉션이 없어지면 가드가 지킬 대상이 사라져 다시 써야 한다 |
| `AlertVisibilityScenarios.cs` | `ClearAlertsButton` 을 참조한다(1곳) |

**자동화 보고서의 알림 개수 필드를 읽는 시험은 0건**이다(측정: `InitialAlertCount` ·
`AlertCountAfterClear` · `AlertCountAfterLoad` 를 참조하는 시험 파일 없음). 즉 그 필드를
없애도 시험은 깨지지 않는데, **그것이 곧 §1.2 의 계기가 아무 시험의 보호도 받지 않는다는
뜻**이다.

### 2.3 비용 요약 — 판정은 lead 몫

| 방향 | 바뀌는 파일 | 깨지는 기존 시험 | 덧붙는 결정 |
|---|---|---|---|
| 1. 전용 표시 요소 | 2 (+1 with (b)) | **1** (`PanelToggleScenarios` RemovedItems) | C-65 제거 판단을 되돌리는 것 / 로그와의 중복 |
| 2a. Clear 가 로그 알림까지 | **1** | **0** | `Alerts` 컬렉션이 왜 남는지 |
| 2b. 컬렉션 제거 | **5** (+생산 측 6파일) | **2** | 일반 로그는 남겨야 하는가 |

**(b) 심각도 구별은 구현하지 않았다.** 위 표가 그 판정의 입력이다 — 방향 1 이면 (b)가
패널의 표현으로 자연히 풀리고, 방향 2 면 (b)는 **로그의 표현 문제**로 남아 색·아이콘·필터를
`LogListBox` 의 `DataTemplate` 에 넣는 일이 된다(그 템플릿은 `GUI-C-123` 이후 의도적으로
손대지 않은 곳이다).

## 3. 카드 §5 의 낡은 주석 — 확인했고, 정리하지 않았다

카드가 *"`clients/` 세 파일의 래치 서술 주석이 낡았다(`f772b31` 로 래치 제거)"* 고 적었다.
**확인했고 이 커밋에서는 건드리지 않았다** — 이 카드의 산출은 단계 A 의 표와 단계 B 의
비용이고, 주석 정리를 섞으면 커밋이 두 가지를 말한다. 별 커밋으로 올릴 수 있으니 지시해
주시면 하겠다.

## 4. 단계 C 는 하지 않았다 (방향 미정)

방향이 정해진 뒤에 할 것을 카드가 적어 두었고, 그중 하나는 **이미 준비돼 있다** — §1 의
시험이 그대로 반증이 된다. 고치면 그 단언이 뒤집히므로 "눌러도 안 변한다" 가 깨지고,
되돌리면 다시 통과한다. **"지우면 안 되는 것까지 지우지 않는가" 를 단언하는 시험은 아직
없다** — 방향이 정해지면 그것을 먼저 추가해야 한다(일반 로그 줄이 남아야 한다면).

## 5. 빌드·시험

```
BUILD_EXIT=0
Mock E2E (ClearAlertsObservationScenarios) : 통과 1, 실패 0
```

새 시험은 **전용 앱 인스턴스**를 쓴다 — 대조군이 로그를 비우므로 공유 컬렉션 픽스처를
쓰면 같은 컬렉션의 다른 시험을 오염시킨다(`GUI-C-126` 에서 공유 픽스처 때문에
"before == 0" 단언이 깨진 전례).

## 6. 미검증 (Gaps)

- **`Alerts` 컬렉션의 전후 개수를 재지 못했다**(§1.2) — 바인딩이 없어 UIA 로 볼 수 없고,
  기록하는 계기는 두 버튼을 함께 누른다. 코드상 `Alerts.Clear()` 가 불리는 것은 읽었지만
  **관측하지 않았다.**
- **`ClearAlertsMenuItem`(메뉴 쪽)은 누르지 않았다.** 버튼과 같은 명령에 묶여 있는 것은
  읽었지만, 메뉴 경로를 눌러 같은 결과가 나오는지는 재지 않았다.
- **Native 백엔드에서 재지 않았다.** 알림 3건은 Mock 기동 시의 것이다.
- **알림 0건 팔을 실행하지 않았다** — §1.1 의 이유로 양성 대조군으로 대체했다. 카드의
  지시와 다르므로 여기 적어 둔다.
- **비용 표의 "깨지는 시험" 은 정적 분석이다.** 실제로 그 방향을 구현해 깨뜨려 보지는
  않았다. 파일 수는 참조 수를 센 것이고, 구현하면 더 늘 수 있다.

## 7. 잔여 위험

- 로그 줄 수 11·알림 3 은 **Mock 기동 직후**의 값이다. 실제 사용 중에는 로그가 길어져
  알림이 스크롤로 밀려나는데(`GUI-C-122` 축), 그 상태에서 `Clear Alerts` 의 무의미함이
  **더 커지는지**는 재지 않았다.
- 방향 2a 가 가장 싸지만, **`Alerts` 컬렉션이 남는 이유가 설명되지 않은 채로 남는다** —
  생산 측 6파일이 그 컬렉션에 쓰고 있는데 읽는 곳은 `ClearAlertsCommand` 뿐이다.

🗿 MoAI

---

# 8. 단계 C — 방향 2a 를 구현했다 (lead 판정)

## 8.1 시험을 먼저 넣었다

lead 지시대로 **"지우면 안 되는 것까지 지우지 않는가" 를 고침 전에** 고정했다
(`PressingClearAlerts_KeepsTheOrdinaryLogLines`, 커밋 `787a3e4`). 그때는 버튼이 아무것도
바꾸지 않으므로 통과가 자명했다 — 그 가드의 일은 명령이 줄을 지우기 시작하는 순간부터다.
**고침 뒤에 쓰면 구현에 맞춰 쓰게 된다.**

개수가 아니라 **내용으로** 비교한다. 개수는 유지하면서 줄을 바꿔치기하는 구현은 통과하지
못한다.

## 8.2 착수 전 확인 — 표식은 있었다

`RaiseAlert` 가 로그에 넣는 곳은 **한 곳**이고 접두가 붙는다
(`MainWindowViewModel.cs:1966`).

**그래도 문자열 대조는 쓰지 않았다.** `"ALERT "` 를 본문에 담은 일반 메시지가 함께 걸릴 수
있다 — 지금은 그런 호출이 없지만 그것은 **오늘의 사실이지 계약이 아니다.** 대신 삽입한
줄을 기억한다: `Log` 가 넣은 줄을 돌려주고, `RaiseAlert` 가 그것을 `_alertLogLines` 에
담고, `ClearAlerts` 가 그 줄들만 `Logs` 에서 제거한다.

`ClearLogs` 도 그 장부를 비운다 — 안 그러면 다음 `Clear Alerts` 가 사라진 줄을 지우려
하거나, 우연히 같은 문자열이 된 새 줄을 지운다.

## 8.3 측정 — §1 과 같은 표로

| 대상 | 전 | `Clear Alerts` 후 | (대조군) `Clear Logs` 후 |
|---|---|---|---|
| 화면 로그 줄 수 | 11 | **8** | 0 |
| 그중 알림 줄 수 | 3 | **0** | 0 |
| 일반 줄 (내용 비교) | 8 | **8 — 전부 남음** | 0 |

## 8.4 반증 — 되돌리면 다시 빨강

`ClearAlerts` 를 `Alerts.Clear()` 로 되돌렸다.

```
AFTER Clear Alerts  log lines: 11, alert lines: 3      ← 고침 전으로 돌아간다
실패 1 (관측 사례) / 통과 1 (가드는 그대로)
원복 확인: FALSIFY136 잔존 0
```

**가드가 두 방향에서 다 통과한 것**도 의미가 있다 — 그것은 고침을 설명하는 시험이 아니라
고침을 **넘어 살아남는** 시험이다.

## 8.5 버튼별 계기 (lead 요청)

`MainWindow.xaml.cs` 가 두 버튼을 함께 누르고 한 번만 기록했다. 하나씩 누르고 사이에
기록하도록 바꿨다.

```
CI 와 같은 방식으로 자동화 실행: exit=0, Passed=True
  InitialLogCount=11           InitialAlertCount=3
  LogCountAfterClearAlerts=30  AlertCountAfterClearAlerts=0     ← 새 필드
  LogCountAfterClear=0         AlertCountAfterClear=0
```

**이제 계기가 버튼별로 말한다.** `Clear Alerts` 뒤 알림 0 / 일반 로그 30줄 — 예전에는 둘 다
0 이라 이 결함이 보고서 안에서 보이지 않았다.

## 8.6 비용 표 정정

| 방향 | 카드 때 추정 | 실제 |
|---|---|---|
| 2a | 파일 **1**, 깨지는 시험 0 | **파일 3**, 깨지는 시험 **0** |

늘어난 둘은 lead 가 요청한 **계기 분리** 때문이다 — `MainWindow.xaml.cs`,
`Models/GuiAutomationReport.cs`. 결함 고침 자체는 여전히 `MainWindowViewModel.cs`
한 파일이다.

## 8.7 이름과 주석도 고쳤다

시험 이름이 옛 동작을 말하고 있었다 —
`PressingClearAlerts_ChangesNothingTheUserSees…` →
`PressingClearAlerts_TakesTheAlertLinesOffScreen…`. 클래스 주석도 "고치기 전에 관측한다" 에서
"관측했고 2a 로 고쳤다" 로 바꿨다. **이름이 코드와 다르면 다음 사람이 이름을 믿는다.**

## 8.8 카드 §5 의 낡은 래치 주석 — 별 커밋으로 정리했다

`QA-A-142`(`f772b31`)가 `#196` 억제 래치를 제거했고, **그 근거가 이 레인의 측정**이다 —
드레인이 `finally` 에서 돌아 축출이 없다는 것과, 종료 함수가 래치를 매 실행 되돌린다는 것.

| 파일 | 무엇 |
|---|---|
| `NonlinearityNoopAlertScenarios.cs` | "once per condition" → "per frame, and that is now the design". 시험 이름·단언 메시지·대조군 설명까지 |
| `NonlinearityLatchRearmTests.cs` → `NonlinearityLutCallSiteTests.cs` | 클래스·파일 이름이 사라진 래치를 가리켰다. **남는 사실 둘은 그대로 유효**하므로 파일은 남기고 이름과 서술만 |

**단언은 바뀌지 않았다** — 래치가 있든 없든 이 호스트는 프레임당 1건이므로 수치가 같다.
**재측정하지 않았고 그 사실을 파일에 적었다**: `f772b31` 은 이 레인이 스테이징한 산출물
(`4a62fb0`)에 없고, 그것을 담은 CI 런은 아직 돌고 있었다.

## 8.9 빌드·시험

```
BUILD_EXIT=0 (gui, IntegrationTests, E2ETests)
IntegrationTests 전체                      : 통과 265, 건너뜀 1, 실패 0
Mock E2E (ClearAlertsObservationScenarios) : 통과 2, 실패 0
자동화 보고서 (CI 방식)                    : exit=0, Passed=True
```

## 8.10 단계 C 의 미검증

- **Native 백엔드에서 재지 않았다.** 알림 3건은 Mock 기동 시의 것이다.
- **`ClearAlertsMenuItem`(메뉴 경로)를 누르지 않았다** — 같은 명령에 묶인 것은 읽었지만
  메뉴로 눌러 같은 결과가 나오는지는 재지 않았다.
- **`f772b31` 이후 바이너리로 네이티브 알림 시나리오를 다시 돌리지 않았다**(§8.8).
- **Mock E2E 전체를 2a 이후 다시 돌리지 않았다** — 관측 2건과 통합 전체만 돌렸다. 새 시험이
  **전용 인스턴스**를 쓰므로 공유 상태는 건드리지 않지만, 그것으로 전체를 대신하지는 못한다.
- **알림이 많은 상태(수십 건)에서 제거 비용을 재지 않았다** — `Logs.Remove` 는 선형 탐색이고
  알림 수만큼 반복한다.
