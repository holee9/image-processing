# GUI-C-29 — FlaUI E2E 스모크 스위트 S-01~S-05 (XPE-GUI-E2E-001 §4.1, Mock)

- 카드: GUI-C-29 · Refs #136 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 2건: `503d494`(프로젝트 골격), `b071811`(S-01~S-05) — 미푸시
- 선행: `git merge origin/main` → `032e5ac` (C-28 병합본, `.claude/settings` 변경 0건)
- **결과: 스모크 5/5 통과, 시나리오 합 584 ms (게이트 30 s) · 창 숨김 실행 통과(벽시계 4 s) · slnx 0/0 · 통합 스위트 0/167/1/168 무회귀**

---

## 1. AutomationId 실측 (카드 2항 — "먼저 실측 목록")

앱에는 이미 **76개**의 AutomationId 가 있다(`MainWindow.xaml` 73 + `HelpWindow.xaml` 3, `Views/*.xaml` 5 별도). 계획서 §4.1 이 지정한 `XPE_*` 이름은 **0개**다.

| 계획서 Id | 앱에 있음? | 실제 대응 | 조치 |
|---|---|---|---|
| `XPE_Main_Window` | ✗ | (창에 Id 자체가 없었다) | **추가** `AutomationId="MainWindow"` |
| `XPE_Menu_File` | ✗ | `FileMenu` | 기존 사용 |
| `XPE_Menu_Backend` | ✗ | `BackendMenu` | 〃 |
| `XPE_Menu_View` | ✗ | `ViewMenu` | 〃 |
| `XPE_Menu_Pipeline` | ✗ | `PipelineMenu` | 〃 |
| `XPE_Menu_Tools` | ✗ | `ToolsMenu` | 〃 |
| `XPE_Menu_Help` | ✗ | `HelpMenu` | 〃 |
| `XPE_Toolbar_Open` | ✗ | `LoadRawImageButton` | 〃 |
| `XPE_Toolbar_Run` | ✗ | `InitializeBackendButton` | 〃 |
| `XPE_Toolbar_Reset` | ✗ | `ClearLogsButton` | 〃 |
| `XPE_Menu_Help_Home` | ✗ | `OpenHelpIndexMenuItem`("Help Home") | 〃 |
| `XPE_Runtime_CommonVersion` | ✗ | **대응 없음** | §2 |

**추가한 것은 창의 Id 하나뿐이다.** 나머지 11개를 `XPE_*` 로 개명하지 않은 이유: 76개 중 11개만 바꾸면 규칙이 둘로 갈리고, 76개 전부를 바꾸는 것은 이 카드가 제외한 UI 전반 변경이다. 기존 Id 는 `x:Name` 과 같은 값이라 코드비하인드와도 짝을 이룬다. **개명은 별도 결정 사항으로 남긴다.**

## 2. S-05 는 대상이 없다 — 좁혀서 구현했다

계획서 S-05 는 "Runtime Panel 버전 표시(non-empty semver)" 를 요구한다. 실측:

```
grep -rn "RuntimeInfo" gui/ImageProcTest/ --include=*.xaml   → 0건
grep -nE "Version" gui/ImageProcTest/MainWindow.xaml         → 메뉴 헤더 문자열뿐
```

**버전을 렌더하는 요소가 어느 XAML 에도 없다.** `RuntimeInfo` 는 뷰에 바인딩되지 않으며, 버전 문자열은 `Log($"Initialized backend '...' ({_backend.GetVersion()}).")` 로 로그에만 들어간다.

라벨을 신설하는 것은 UI 변경이라 카드가 금지한다. 그래서 S-05 는 **있는 것**(`StatusBarText` 존재 + 문구 비어 있지 않음)으로 좁혔고, **버전 표시 부재를 결과로 보고한다.** 계획서 S-05 를 원문대로 만족시키려면 UI 추가가 선행되어야 한다.

## 3. 구현

| 파일 | 내용 |
|---|---|
| `clients/ImageProcTest.E2ETests/ImageProcTest.E2ETests.csproj` | net8.0-windows·x64, FlaUI.UIA3 **4.0.0 고정**, xunit 2.9.3 + `Xunit.SkippableFact` |
| `Fixtures/ApplicationFixture.cs` | gui exe 기동/종료 보장 |
| `Scenarios/Smoke/SmokeScenarios.cs` | S-01~S-05 + 시간 기록 |
| `clients/ImageProcTest.slnx` | 편입(C-26 방식) |
| `gui/ImageProcTest/MainWindow.xaml` | 창에 AutomationId 1개 추가 (UI 변경 없음) |

픽스처에서 의식적으로 고른 것 셋:

- **`--automation-backend Mock` 으로 기동**(C-26 인자) — 네이티브 DLL 스테이징에 의존하지 않는다.
- **`--automation-report` 는 주지 않는다** — 주면 `App.IsAutomationMode` 가 켜져 자기 주행 시나리오가 창을 스스로 닫고 모든 단언과 경합한다.
- **종료는 `Close` 가 아니라 `Kill`** — WPF 창의 Close 는 거부되거나 대화상자에 막힐 수 있고, 샌 프로세스의 창을 다음 테스트 클래스가 찾아 조작하게 된다.
- exe 가 없으면 예외가 아니라 **스킵**(`Xunit.SkippableFact`, GUI-C-06 판정과 같은 메커니즘). "빌드 안 함" 이 앱 결함처럼 보이는 실패로 잡히면 안 된다.

## 4. S-04 에서 배운 것 — "XAML 에 있다" ≠ "자동화가 볼 수 있다"

첫 실행에서 S-04 만 실패했다:

```
Toolbar/Help: OpenQuickStartHelpMenuItem was not found.
```

그런데 그 Id 는 `MainWindow.xaml:296` 에 분명히 있다. 원인은 **WPF 가 하위 메뉴 항목을 지연 생성**하는 것이다 — 부모 메뉴를 펼치기 전에는 UIA 트리에 존재하지 않는다. `HelpMenu.Expand()` 후 폴링해서 찾도록 고치니 통과했다(385 ms).

이 레인의 gate 목록 형태 그대로다 — **정의 존재를 동작으로 오귀속**(#2). 소스에 있다는 것이 자동화가 볼 수 있다는 뜻은 아니다.

## 5. 실행 결과 (카드 2·3항)

### 창 표시 실행 — 시나리오별 시간

| 시나리오 | 시간 | 내용 |
|---|---|---|
| S-01 | 3 ms | 창 존재, 제목에 "ImageProcTest", `AutomationId=MainWindow` |
| S-02 | 105 ms | 메뉴 6그룹 존재 + `ControlType.MenuItem` |
| S-03 | 73 ms | 툴바 3버튼 `IsEnabled` (Mock) |
| S-04 | 385 ms | Help 펼침 → Help Home 존재·활성 |
| S-05 | 18 ms | StatusBarText 존재 + 문구 있음 |
| **합** | **584 ms** | **게이트 30 s 대비 약 1.9 %** |

```
통과!  - 실패: 0, 통과: 5, 건너뜀: 0, 전체: 5 (571 ms)
```

### 창 숨김 실행 (CI 전제)

```
Start-Process dotnet test … -WindowStyle Hidden -Wait
exit=0  wall=4s
통과!  - 실패: 0, 통과: 5, 건너뜀: 0, 전체: 5 (678 ms)
```

C-25 에서와 같은 한계가 남는다 — **숨김 실행은 "데스크톱 없는 세션" 의 증거가 아니다.** 다만 스모크가 사람 조작 없이 4초에 끝난다는 것은 확인됐다.

### 반증 (`step4-falsify.log`)

S-03 의 Id 하나를 `LoadRawImageButtonX` 로 바꿨다:

```
실패!  - 실패: 1, 통과: 4, 전체: 5
S03_ToolbarButtons_AreEnabledInMockMode: Toolbar button 'LoadRawImageButtonX' was not found.
```

해당 시나리오만 실패한다. 원복 후 재확인.

## 6. 실측 (verbatim)

```
dotnet test clients/ImageProcTest.E2ETests/ImageProcTest.E2ETests.csproj -c Debug
통과!  - 실패:     0, 통과:     5, 건너뜀:     0, 전체:     5

dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개
    오류 0개

dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
통과!  - 실패:     0, 통과:   167, 건너뜀:     1, 전체:   168
```

Baseline 귀속: 통합 스위트는 C-28 최종 `0/167/1/168` 과 **동일** — E2E 는 별도 프로젝트라 통합 카운트를 바꾸지 않는다(예상값이자 실측값). E2E 5건은 새 스위트의 첫 측정이라 비교 기준이 없다.

## 7. 미검증 (Gaps)

- **CI 에서 돌려 보지 않았다.** 로컬 창 숨김까지가 이번 관측이고, 데스크톱 없는 러너에서 UIA 가 창을 찾을 수 있는지는 여전히 미확인이다(C-25 §4 와 같은 한계). ci.yml 배선은 카드가 leader 몫으로 뒀다.
- **S-04 는 Help Home 을 클릭하지 않는다.** 열리는 창의 정리가 이 시나리오보다 오래 살아 다음 시나리오로 샐 수 있어 존재·활성까지만 본다 — 계획서의 "오프라인 열림" 을 절반만 덮는다.
- **S-03 은 버튼이 활성인지만 본다.** 눌렀을 때 무슨 일이 일어나는지는 Workflow 스위트(§4.2, 범위 밖)의 몫이다.
- **Mock 모드만 돌렸다.** Native 모드 스모크는 카드가 제외했다.
- 계획서 §3 의 PageObjects·Utilities·Resources 는 만들지 않았다(카드 지시: 스모크에 필요한 것만).
- 스모크가 **연속 실행에서 서로 간섭하지 않는지** 확인하지 않았다. 한 클래스가 앱 인스턴스 하나를 공유하고 S-04 가 메뉴를 펼쳤다 접으므로 순서 의존이 생길 수 있다 — 이번 두 번의 실행에서는 관측되지 않았다.

## 8. 잔여 위험 (Residual risk)

- **AutomationId 규칙이 계획서와 갈라져 있다.** 이 스위트는 앱 쪽 규칙을 따랐고, 계획서를 따르는 다른 문서·후속 스위트가 생기면 두 이름이 공존한다. 개명이든 계획서 수정이든 **한쪽으로 정리하는 결정이 필요하다**(leader).
- **지연 생성 함정은 S-04 만의 것이 아니다.** 컨텍스트 메뉴·탭·팝업 안의 요소는 모두 같은 방식으로 "없다" 고 나온다. 앞으로 시나리오가 늘 때마다 같은 실패를 다시 만날 수 있다 — `WaitFor` 헬퍼를 두었지만 펼치는 동작은 시나리오가 직접 해야 한다.
- 앱 인스턴스를 클래스 단위로 공유해 속도를 얻었다(합 584 ms). 대신 한 시나리오가 UI 상태를 남기면 다음 시나리오가 영향을 받는다 — 지금은 S-04 만 상태를 바꾸고 되돌린다.
- `Kill()` 로 종료하므로 앱의 정상 종료 경로(설정 저장 등)는 이 스위트에서 한 번도 실행되지 않는다.

## 부록 — 사용한 명령

```bash
git merge origin/main                                   # → 032e5ac
grep -rno "AutomationProperties.AutomationId=\"[^\"]*\"" gui/ImageProcTest/*.xaml   # 실측 76개
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.E2ETests/ImageProcTest.E2ETests.csproj -c Debug --logger "console;verbosity=detailed"
powershell -NoProfile -NonInteractive -Command "Start-Process dotnet … -WindowStyle Hidden -Wait"
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet test clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug
```
