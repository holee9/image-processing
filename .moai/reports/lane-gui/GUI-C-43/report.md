# GUI-C-43 — E2E Workflow 스위트 §4.2 (#136)

> **이번 실행이 쓴 네이티브**: run **34535620953** · head `83ffa7ae…` ·
> `xpe_preprocess.dll` md5 `9f81d2f07fb7f7e23e90c86332467105`.

- 카드: GUI-C-43 · Refs #136 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui` · 병합 `cbae11f`
- 커밋 1건: `1a5c479` — 미푸시
- **결과: Native E2E 0/16/0/16 · Mock 0/15/1/16 · 통합 0/180/1/181 · slnx 0/0**
- **계획서 §4.2 10행 중 구현 가능 7 · 구현 불가 3** (앱을 바꾸지 않고 실측만 보고)

---

## 1. 계획서 §4.2 실측 대조 (커밋 없음)

기준: `docs/project/XPE-GUI-E2E-001_FlaUI_E2E_Plan.md` §4.2 표 vs `MainWindow.xaml` +
`MainWindowViewModel.cs` 실제.

| # | 계획서 | 실측 | 판정 |
|---|---|---|---|
| W-01 | File→Open Raw → 뷰포트에 이미지 | `OpenRawMenuItem` 존재. 자동화 경로로 로드 | **구현됨**(C-29~34) |
| W-02 | Pipeline→Run Preprocessing | `RunPreprocessingMenuItem` 존재, Native 에서 활성 | **구현됨**(C-36·C-37) |
| W-03 | Backend→Backend Mode→**Mock↔Real 전환** | `NativeBackendModeMenuItem` 이 **`IsEnabled="False"`** — 런타임 전환이 없다. 모드는 기동 시 `--automation-backend` 로 정해진다 | **계획서 오류** → 좁혀서 구현 |
| W-04 | View→**Compare Mode→Swipe** | **그런 메뉴가 없다.** View 에 있는 비교 관련 항목은 `ResetComparisonViewMenuItem` / `DetachComparisonViewerMenuItem` 둘뿐 | **계획서 오류** → 구현 불가 |
| W-05 | View→**Compare Mode→Difference** | 위와 같음 — Swipe/Difference 명령이 XAML·뷰모델 어디에도 없다 | **계획서 오류** → 구현 불가 |
| W-06 | View→**Display LUT→GSDF On** | 그 경로가 없다. `GsdfCalibrateMenuItem` 은 Tools 아래이고 **`IsEnabled="False"`**("Phase 1b+"). Display 파이프라인 적용은 Pipeline→`ApplyDisplayPipelineMenuItem` | **계획서 오류** → W-01b 가 이미 실제 경로를 덮음 |
| W-07 | Display Panel→Body Part→Lung | 프리셋 명령 존재 | **구현됨**(C-34·C-35) |
| W-08 | 설정 변경 → **재시작** → 값 유지 | `SaveSettingsMenuItem` 존재. 다만 E2E 기동은 자동화 모드가 아니라(리포트 미지정, C-38) **출하 `appsettings.json` 을 읽고 쓴다** — 테스트가 저장을 누르면 개발자의 실제 설정을 덮어쓴다 | **구현 보류**(§2 사유) |
| W-09 | **Tools**→Export Automation Report → **TRX** | 항목은 **File** 아래(`ExportAutomationReportMenuItem`)이고, `ExportAutomationReport` 는 실행 파일 옆에 **`menu-command-report.json`** 을 쓴다 — JSON, TRX 아님 | **계획서 오류(2곳)** → 실제대로 구현 |
| W-10 | Tools→Fixture Manager→Select → **fixture SHA-256 표시** | `ShowFixtureManager` 는 상태바에 `Fixture pack available: <path>` 또는 `missing: <path>` 를 쓴다. **그 경로에 해시 계산이 없다** | **계획서 오류** → 실제 주장대로 구현 |

**앱은 한 줄도 바꾸지 않았다.** 카드 지시대로 실측만 보고하고, 계획서 정정은 leader 몫이다.

C-29 에서 같은 유형이 이미 한 번 있었다(스모크 표의 항목이 앱과 달랐다). §4.2 는 10행 중
**5행이 실제와 다르다** — 계획서가 앱보다 먼저 쓰였다는 사실의 결과이지 결함 보고가 아니다.

## 2. W-08 을 구현하지 않은 이유 (실측 근거)

계획서는 "재시작 후 값 유지" 를 요구한다. E2E 픽스처는 `--automation-report` 를 **일부러 주지
않으므로**(자기주행 시나리오가 창을 닫아 단언과 경합한다 — C-29) `App.IsAutomationMode` 가
false 이고, `CreateSettings` 의 격리 분기를 타지 않는다(C-38 §2). 즉 **이 기동은 출하
설정 파일을 읽고 쓴다.**

따라서 W-08 을 그대로 구현하면 테스트가 개발자의 실제 `appsettings.json` 을 덮어쓴다.
피하려면 설정 파일 경로를 지정하는 스위치가 필요한데, 그것은 **앱 변경**이고 이 카드가
금지한 범위다. 그래서 **구현하지 않고 사유를 남긴다** — 부분 구현으로 "재시작 유지" 를
주장하는 것보다 낫다.

## 3. 구현 (`1a5c479`)

`Scenarios/Workflows/WorkflowMenuScenarios.cs` — W-03 · W-09 · W-10.

| 시나리오 | 무엇을 고정하나 |
|---|---|
| **W-03** | 기동한 모드를 상태바가 보고하고, **Native 전환 항목이 무력**하다 |
| **W-09** | Export 클릭이 **이번 실행에서** 리포트 파일을 만든다 |
| **W-10** | 픽스처 팩 존재 여부 보고가 **디스크 실제와 일치**한다 |

세 가지 설계 결정:

**(a) W-03 은 "없음" 을 계약으로 고정한다.** 나중에 그 항목이 활성화됐는데 실제 전환이
동작하지 않으면 사용자가 Native 를 골라 Mock 을 얻는다 — C-31 이 명령줄에서 실측한 것과 같은
유형이다. 단언 메시지에 "활성화됐다면 이 시나리오를 실제로 구동하도록 다시 쓰라, 완화하지
말라" 를 적어 뒀다.

**(b) W-09 는 클릭 전에 기존 파일을 지운다.** 지우지 않으면 이전 실행이 남긴 파일이 단언을
만족시켜, **명령이 아예 실행되지 않아도 통과**한다.

**(c) W-10 은 앱의 보고를 디스크와 대조한다.** `Assert.Contains("available")` 만으로는 항상
"available" 이라고 말하는 앱도 통과한다. 존재 여부를 먼저 재고, 반대 낱말이 **없다는 것까지**
단언한다.

**세 건 모두 Mock·Native 양쪽에서 돈다** — Skip 없음(카드 지시). 백엔드에 따라 달라지는 값은
`app.BackendMode` 로 분기한다.

## 4. 시나리오별 실행 시간 (Native, verbatim)

| 시나리오 | Mock | Native |
|---|---|---|
| W-01 | — | 2 088 ms |
| W-01b | — | 2 878 ms |
| W-02 | — | 3 773 ms |
| W-07 | — | 2 169 ms |
| **W-03** | 790 ms | 289 ms |
| **W-09** | 739 ms | 251 ms |
| **W-10** | 435 ms | 247 ms |
| S-01~S-05 | — | 2 / 104 / 79 / 371 / 14 ms |

스위트 전체 Native **1 m 4 s**, Mock **10 s** — §4.2 게이트(3 min) 안이다.

## 5. 반증 (빌드 결과 포함)

앱 쪽 검증 대상을 무력화했다 — `ExportAutomationReport` 의 산출 파일명을
`menu-command-report-DISABLED.json` 으로 바꿨다(약화, 삭제 아님).

```
=== 빌드:
    경고 0개
    오류 0개                       ← 빌드는 정상
=== 테스트:
  실패 …WorkflowMenuScenarios.W09_ExportAutomationReport_WritesAReportFileAndNamesIt [10 s]
   Export Automation Report did not produce …\menu-command-report.json. …
실패!  - 실패: 1, 통과: 14, 건너뜀: 1, 전체: 16
```

**빌드가 0/0 인 상태에서 W-09 한 건만** 실패한다 — 컴파일이 깨져 낡은 바이너리로 통과가
위장될 여지가 없다(C-42 에서 보강된 규약). 원복 후 재확인.

## 6. 실측 (verbatim)

```
XPE_E2E_BACKEND=Native XPE_NATIVE_DIR=<staging> dotnet test …E2ETests…   (앱 폴더 비움)
통과!  - 실패: 0, 통과: 16, 건너뜀: 0, 전체: 16 (1 m 4 s)

dotnet test clients/ImageProcTest.E2ETests/… -c Debug                    (Mock)
통과!  - 실패: 0, 통과: 15, 건너뜀: 1, 전체: 16 (11 s)

dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
통과!  - 실패: 0, 통과: 180, 건너뜀: 1, 전체: 181

dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개 / 오류 0개
```

Baseline 귀속: E2E 는 C-42 시점 **13건** → **16건**(W-03·W-09·W-10). Mock 의 건너뜀 1은
Native 전용 출처 가드(C-41)다. 통합 181 은 불변 — 이 카드는 통합을 건드리지 않았다.

## 7. 미검증 (Gaps)

- **W-04·W-05·W-06 은 구현하지 않았다.** 해당 메뉴·명령이 앱에 없다는 것까지만 실측했고,
  "그 기능이 다른 형태로 존재하는지" 는 전수 조사하지 않았다 — XAML 과 뷰모델을 grep 한
  범위다.
- **W-08 은 구현하지 않았다**(§2). 재시작 후 설정 유지는 **여전히 미측정**이다.
- **W-03 은 전환을 구동하지 못한다.** 항목이 비활성이라는 사실만 고정한다 — 전환이 실제로
  동작하는지는 이 카드가 답할 수 없는 질문이다.
- **W-09 는 파일이 생겼다는 것과 비어 있지 않다는 것까지만 본다.** 내용이 옳은지는 보지 않는다.
- **W-10 은 해시를 보지 않는다.** 계획서가 요구한 SHA-256 은 앱에 없으므로 측정 대상이 없다.
- **Mock 실행 1회가 1건 실패했다가 재실행에서 재현되지 않았다.** 그 실행은 앱 출력 폴더로
  네이티브 DLL 을 되돌리는 명령과 같은 턴이었다 — 그럴듯하지만 **확인하지 못했고**, 실패한
  테스트 이름을 잡아 두지 못했다.

## 8. 잔여 위험 (Residual risk)

- **계획서와 구현이 §4.2 에서 갈라진 채로 남는다.** 정정은 leader 몫이며, 그 전까지 계획서를
  읽는 사람은 없는 기능 3건을 있다고 읽는다.
- **W-09 가 실행 파일 폴더에 파일을 쓴다.** 테스트가 앱의 출력 디렉터리를 오염시키며, 매 실행
  삭제 후 재생성한다. 병렬 실행 시 서로를 방해할 수 있다(현재 어셈블리 직렬화로 회피 —
  C-36).
- **W-03 의 단언은 "비활성" 이 옳은 상태라고 전제한다.** 그 전제가 바뀌는 날(전환 구현)
  이 테스트는 실패하고, 그때 해야 할 일은 완화가 아니라 재작성이다 — 메시지에 적어 뒀다.
- 스위트가 16건으로 늘며 Native 실행이 1 m 4 s 다. 게이트(3 min)에는 여유가 있으나
  스모크 게이트(30 s)와는 다른 규모다.

## 부록 — 사용한 명령

```bash
sed -n '/## 4.2/,/## 4.3/p' docs/project/XPE-GUI-E2E-001_FlaUI_E2E_Plan.md
grep -n "MenuItem Header\|AutomationProperties.AutomationId" gui/ImageProcTest/MainWindow.xaml
grep -n -A12 "private void ExportAutomationReport\|ShowFixtureManager" \
     gui/ImageProcTest/ViewModels/MainWindowViewModel.cs

export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.E2ETests/… --filter "…WorkflowMenuScenarios" \
       --logger "console;verbosity=detailed"
XPE_E2E_BACKEND=Native XPE_NATIVE_DIR="$(pwd)/build/ci-common/bin" dotnet test …E2ETests…
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
```
