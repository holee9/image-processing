# GUI-C-153 (#225) — 행 4 구현. **한 항목을 끝까지** 했고 여기서 멈춥니다

상태: **행 4 완료**(기능 → `IsEnabled` → 툴팁 → 집계 −1 확인 → 반증). **행 6·11·12·15·16 은 착수하지 않았습니다.**

카드 §2 가 *"여섯을 동시에 열지 마십시오"* 라고 했고, 한 항목을 끝까지 가는 데 필요한 것이 생각보다 많았습니다 — 특히 기존 시험 하나를 **다시 써야** 했습니다(§4).

---

## 1. 무엇을 만들었나

| 자리 | 변경 |
|---|---|
| `ViewModels/MainWindowViewModel.cs` | `SetBackendModeCommand`(`RelayCommand<string>`) + `SetBackendMode(string)` 추가 |
| `MainWindow.xaml` `:128-146` | `_Mock`·`_Native` 두 항목에 `Command`/`CommandParameter` 배선, `IsChecked` 를 `ActualBackendMode` 에 바인딩, `_Native` 의 `IsEnabled="False"` 제거, **툴팁 둘 다 교체** |

### 1.1 왜 `Settings.BackendMode` 만 바꾸지 않는가

백엔드 객체는 팩토리가 한 번 만듭니다. 속성만 바꾸면 **요청**만 달라지고 도는 백엔드는 그대로여서,
`RequestedBackendMode` 와 `ActualBackendMode` 가 어긋난 채 **아무 일도 일어나지 않습니다.**
그래서 `InitializeBackend()` 를 불러 팩토리로 다시 만듭니다 — 그것이 메뉴를 말한 대로 동작하게 하는 부분입니다.

### 1.2 체크 표시를 **요청이 아니라 실제**에 묶었습니다

`XpeBackendFactory` 는 네이티브 DLL 을 못 읽으면 **조용히 Mock 으로 떨어집니다.**
그래서 `IsChecked` 를 `ActualBackendMode` 에 묶었습니다 — Native 를 눌렀는데 Mock 이 도는 상태가
**성공처럼 보이면 안 됩니다.** 요청/실제의 어긋남은 상태바가 이미 드러냅니다(`#175`).

### 1.3 툴팁

옛 문구 *"Native backend activation starts after RealXpeBackend and P/Invoke integration"* 은 **낡았습니다** —
그 연동은 끝났고 CI 의 `gui-e2e-native` 가 그 경로로 돕니다(`C-152` 행 4 에서 지적한 그것).
새 문구는 **무엇을 하는 명령인지**와 **폴백이 있다는 사실**을 적습니다.

## 2. 집계 −1 (카드 §2-4 — 건너뛰지 않았습니다)

`DisabledFutureCommandCount` 를 **앱을 실제로 돌려** 양쪽에서 쟀습니다(같은 인자, 같은 픽스처).

| | `DisabledFutureCommandCount` | `Passed` |
|---|---|---|
| **변경 전**(`HEAD` 의 두 파일로 빌드) | **21** | True |
| **변경 후** | **20** | True |

정확히 하나 줄었습니다. 기준선은 `git show HEAD:` 로 두 파일을 되돌려 **다시 빌드해** 잰 것이고,
잰 뒤 제 변경을 복원했습니다.

`MainWindow.xaml.cs:426` 의 게이트는 `>= 10` 이라 이 감소로 깨지지 않습니다. **정확한 수를 단언하는 시험은 없습니다**(`grep` 으로 확인, 전체 2곳뿐).

## 3. 반증 — **메뉴가 켜진 것은 증거가 아닙니다**

카드 §3 이 요구한 것: *"`AppSettings.BackendMode` 가 바뀌었는지가 아니라 **결과**를 보라."*

### 3.1 관측 지점을 둘 골랐습니다

| 지점 | 왜 |
|---|---|
| **MOCK BACKEND 배너** | `IsMockBackend` 에 바인딩 — **백엔드 객체**입니다. 요청만 바뀌면 안 나타납니다 |
| **상태바 버전 문자열** | 목 백엔드는 `v0.0.0-mock`, 네이티브는 실제 DLL 버전을 냅니다 |

### 3.2 실측 (E2E, 앱을 띄워 메뉴를 눌렀습니다)

**Native 로 띄운 실행:**

```
W-03  launched mode: Native
W-03  after Mock: banner='MOCK BACKEND — images and results are synthetic; not for clinical judgement.'
                  runtime='mode=Mock  |  common=v0.0.0-mock  |  display=v0.0.0-mock-display'
W-03b after Native: runtime='mode=Native  |  common=xpe_display 1.0.0  |  display=1.0.0  |  src=bin'
```

**버전 문자열이 `v0.0.0-mock` ↔ `xpe_display 1.0.0` 로 바뀝니다.** 설정값 읽기가 아니라 **다른 코드가 돌았다는 증거**입니다.
배너도 나타났다 사라집니다.

**Mock 으로 띄운 실행:**

```
W-03  launched mode: Mock
W-03  after Mock: banner='MOCK BACKEND …' runtime='mode=Mock | common=v0.0.0-mock | display=v0.0.0-mock-display'
W-03b: 건너뜀
```

## 4. 기존 시험 W-03 을 **다시 썼습니다** — 완화가 아닙니다

`WorkflowMenuScenarios.W03` 이 `NativeBackendModeMenuItem.IsEnabled == false` 를 단언하고 있었고,
**그 시험이 스스로 지시를 적어 두었습니다**:

> *"When this starts failing, the switch became live and W-03 must be rewritten to actually drive it — not relaxed."*

그대로 했습니다. 이제 W-03 은 **메뉴를 눌러 결과를 봅니다**.

| 시험 | 무엇을 단언 | 구성 |
|---|---|---|
| `W03_BackendMenu_DrivesTheBackendAndTheAppFollows` | Native 항목이 **활성**이고, Mock 으로 몰면 **배너 + `mode=Mock`** 이 따라온다 | **Mock·Native 둘 다** |
| `W03b_SwitchingBackToNative_RestoresTheNativeBackend` | Mock → Native 왕복이 실제로 돌아온다(배너 사라짐, `mode=Native`) | **Native 전용** |

### 4.1 Mock 건너뛰기에 **구조적 사유**를 적었습니다

Mock 실행에는 네이티브 DLL 이 없어 *"Native 로 되돌린다"* 는 **관측 자체가 불가능**합니다.
양쪽을 통과시키려 단언을 느슨하게 하면 **"전환이 된다" 와 "DLL 이 없다" 가 같은 초록**이 됩니다.
그래서 `Skip.If` 에 그 사유를 적었습니다. W-03 본체는 양쪽에서 돕니다.

### 4.2 그 파일의 머리말도 고쳤습니다

클래스 주석이 *"Every scenario here runs in BOTH backend modes … rather than skipped"* 라고 단언하고 있었는데
제가 건너뛰기를 하나 넣었습니다. **주석이 코드를 거짓으로 말하게 두지 않았습니다** — 예외와 그 이유를 같은 자리에 적었습니다.

## 5. 회귀

| 스위트 | 결과 |
|---|---|
| `gui/ImageProcTest` 빌드 | **오류 0**(경고 12건은 제 파일 아님 — 변경 전에도 있던 것) |
| `ImageProcTest.E2ETests` 빌드 | **오류 0** |
| `WorkflowMenuScenarios` (Native) | **통과 4 / 실패 0** |
| `WorkflowMenuScenarios` (Mock) | **통과 3 / 건너뜀 1 / 실패 0** |
| 자동화 보고서 | `Passed=True` (양쪽) |

## 6. 왜 여기서 멈추는가

카드가 행 4 를 *"나머지를 검증할 발판"* 으로 먼저 하라고 했고, 그 발판이 생겼습니다 —
이제 **앱 안에서 네이티브로 바꿔 놓고** 행 6(P/Invoke 스모크)·11(중지)·12(단계 시간)을 눌러 볼 수 있습니다.

한 항목에 **기능 + 시험 재작성 + 기준선 재빌드 측정**이 들어갔으므로 다음 항목은 별도로 갑니다.
행 6·11·12·15·16 은 **손대지 않았습니다** — `IsEnabled` 도 그대로입니다.

## 7. 미검증 / 잔여 위험

- **CI 에서 안 돌았습니다.** 위 수치는 전부 로컬 — **로컬 실측, CI 상한 추정**
- 전체 E2E 스위트를 돌리지 않았습니다. `WorkflowMenuScenarios` 만 돌렸고, 백엔드를 **런타임에 바꾸는 것이 처음** 생겼으므로 다른 시나리오가 이 전환을 가정하지 않는지는 확인하지 않았습니다.
  W-03 은 끝에서 **띄운 모드로 되돌려 놓지만**, 공유 컬렉션에서 순서가 바뀌면 영향이 있을 수 있습니다
- `SetBackendMode` 는 **같은 모드를 다시 눌러도 재초기화**합니다(Initialize 메뉴와 같은 동작). 재초기화가 비싼 상황은 재지 않았습니다
- 네이티브 DLL 이 **중간에 사라지는** 경우(초기화 후 삭제)는 재지 않았습니다
- `_Mock` 항목은 이전에 **활성인데 `Command` 가 없어 아무 일도 하지 않았습니다**. 이번에 명령이 붙었지만, 그것이 `#165` 가 말한 "enabled 인데 안 도는" 형태였다는 점은 **집계에 잡히지 않았습니다**(집계는 `!IsEnabled` 만 셉니다) — 별건으로 보고만 합니다

---

Refs #225
