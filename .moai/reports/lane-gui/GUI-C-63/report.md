# GUI-C-63 — 거절 신호를 살리고, 죽은 토글을 잰다 (#161 #149)

> **이번 실행이 쓴 네이티브**: run **34535620953** · head `83ffa7ae…` ·
> `xpe_preprocess.dll` md5 `9f81d2f07fb7f7e23e90c86332467105`.

- 카드: GUI-C-63 · Refs #161 #149 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 1건 — 미푸시
- **반증을 처음에 잘못 돌렸고, 스스로 잡았습니다**(§3).
- **죽은 토글은 4개가 아니라 7개입니다** — C-62 의 제 수치를 정정합니다(§5).
- **결과: Native 3회 0 실패 · Mock 0/57/2/59 · 통합 0/198/1/199 · slnx 0경고 0오류**

---

## 1. 이동 — 판정대로

`ReportRejectedComparisonMode()` 를 `InitializeBackend()` **뒤로** 옮겼습니다. 한 줄 이동이고,
왜 뒤여야 하는지를 그 자리에 적었습니다.

## 2. 실행 확인 — 양쪽 방향 다

임시 프로브로 앱을 띄워 로그 목록을 읽었습니다(커밋 전 삭제).

| 코드 상태 | 설정의 `comparisonMode` | 로그 줄 수 | 거절 줄 |
|---|---|---|---|
| **이동 후(현재)** | `NotAMode` | 7 | **1** |
| **이동 후(현재)** | `SwipeVertical` | 6 | **0** |
| 이동 전 | `NotAMode` | 6 | 0 |
| 이동 전 + `Logs.Clear()` 제거 | `NotAMode` | 8 | 1 |

```
REJPROBE >> [15:52:28.192] appsettings.json: comparisonMode 'NotAMode' is not a supported
comparison mode; using 'SwipeVertical'. Supported: SwipeVertical, SwipeHorizontal, SplitLocked,
OverlayOpacity, DifferenceHeatmap, SourceOnly, ProcessedOnly.
```

**정상 쪽도 쟀습니다** — 유효한 모드로 띄우면 그 줄이 없습니다. 카드가 지적한 "항상 경고를 쓰는
구현" 은 이 행에서 걸립니다.

## 3. 반증을 처음에 잘못 돌렸습니다 — 그리고 그것이 답을 바꿨습니다

**첫 번째 A/B 반증에서 `dotnet build` 를 E2E 프로젝트에만 걸었습니다.** 앱 exe 는 `gui` 프로젝트가
만들므로 **이동을 되돌린 코드가 실행되지 않았고**, "이동 전인데 줄이 남는다(7줄/1건)" 라는
엉뚱한 결과가 나왔습니다. 그대로 적었으면 §2 의 진단을 제 손으로 뒤집을 뻔했습니다.

`dotnet build clients/ImageProcTest.slnx` 로 다시 돌려 §2 의 표를 얻었습니다. **이 저장소가
기록해 둔 "반증이 안 터지는 세 번째 이유"(빌드가 안 됐다) 그대로입니다.**

### 리더가 물은 것 — 답: 예, 결과 단언은 `Clear()` 를 봅니다

표의 마지막 행이 그 답입니다. **`Logs.Clear()` 를 지우면 이동 전 코드에서도 줄이 남습니다.**
따라서 **"기동 후에도 줄이 있다" 는 결과 단언은 이동과 `Clear()` 제거를 구분하지 못합니다.**
사용자에게 중요한 것은 결과지만, **단언이 지켜야 하는 것은 순서**입니다.

## 4. 단언 — 순서를 겨눕니다 (통합 197 → 199)

| 단언 | 무엇을 본다 |
|---|---|
| `TheRejectionIsReported_AfterTheBackendClearsTheLog` | 생성자 본문에서 보고 호출이 `InitializeBackend()` **뒤**에 있다 |
| `InitializeBackend_IsWhatClearsTheLog` | 그 순서가 필요한 **이유**(= `Logs.Clear()`)가 아직 거기 있다 |

**소스 가드인 이유 둘**, 둘 다 측정에 근거합니다:

1. 실행 단언은 **배포된 `appsettings.json` 을 고쳐야** 하고, 그런 테스트는 커밋할 수
   없습니다(C-46). 이번 수치를 낸 프로브도 지웠습니다.
2. **결과 단언은 순서를 못 봅니다**(§3). 지금 통과시키고 나중에 순서가 조용히 되돌아가도
   `Clear()` 만 빠져 있으면 계속 통과합니다.

C-38(`ApplyRunSelection` 본문 검사)·C-39(헤더 파싱)와 같은 모양이고, 본문은 **중괄호 균형**으로
잘랐습니다 — 생성자가 길고 람다가 많아 순진한 스캔은 틀립니다(두 카드가 같은 이유로 고쳤던
자리).

**전제까지 단언하는 이유**: `Logs.Clear()` 가 사라지면 순서 규칙은 의미가 없어집니다. 그때
가드가 **조용히 남아 있는 것**보다 **실패해서 전제가 바뀌었다고 말하는 것**이 낫습니다.

### 반증 — 4회

| 주입 | 실패한 단언 |
|---|---|
| 호출 순서를 원래대로 | **순서 단언 1건만** |
| `Logs.Clear();` 삭제 | **전제 단언 1건만** |
| `Logs.Clear();` **주석 처리** | **전제 단언 1건만** |
| (첫 판) `Logs.Clear();` 주석 처리 | **아무것도 실패 안 함** ← 가드가 주석에 눈이 멀었음 |

**마지막 행이 이 가드에서 제일 값있습니다.** 첫 판은 원문을 그대로 검색해서 `// Logs.Clear();`
가 그대로 매치됐습니다. **슬래시 두 개로 무력화되는 가드는 가드가 아닙니다** — 주석을 제거한
뒤 검사하도록 고쳤고, 그 경위를 코드에 적었습니다.

## 5. 죽은 토글 — 4개가 아니라 **7개**입니다 (C-62 정정)

C-62 에서 저는 "패널 토글 4개" 라고 적었습니다. **View 메뉴의 체크 항목은 8개이고, 그중 7개가
같은 모양입니다.**

**검색 범위를 넓혔습니다** — `Visibility` 바인딩만이 아니라 코드비하인드·스타일·트리거를 포함해
속성 이름으로 전부 찾았습니다. 일곱 개 모두 참조가 **셋뿐**입니다:

```
ShowRuntimePanel      : 속성 정의 · ResetLayout 의 = true · 자동화 보고서
ShowRawSettingsPanel  : 〃
ShowCalibrationPanel  : 〃 (+ 한 곳에서 = true)
ShowImageSummaryPanel : 〃
ShowMetadataPanel     : 〃
ShowLogsPanel         : 〃
ShowAlertsPanel       : 〃
Settings.ShowDisplayPanel : 속성 정의 · ResetLayout · 자동화 보고서 2곳
```

**어떤 레이아웃도 읽지 않습니다.** 실제 패널 가시성은 `FocusMode`/`RightPanelOpen` 과
`AnalysisTab` 이 결정합니다.

### 눌러서 관측했습니다

```
TOGGLEPROBE ShowRawSettingsPanelMenuItem:      check On -> Off | descendants 146 -> 146
TOGGLEPROBE ShowCalibrationPanelMenuItem:      check On -> Off | descendants 146 -> 146
TOGGLEPROBE ShowDisplaySettingsPanelMenuItem:  check On -> Off | descendants 146 -> 146
TOGGLEPROBE ShowImageSummaryPanelMenuItem:     check On -> Off | descendants 146 -> 146
TOGGLEPROBE ShowMetadataPanelMenuItem:         check On -> Off | descendants 146 -> 146
TOGGLEPROBE ShowLogsPanelMenuItem:             check On -> Off | descendants 146 -> 146
TOGGLEPROBE ShowAlertsPanelMenuItem:           check On -> Off | descendants 146 -> 146
TOGGLEPROBE AnalysisPanel 'Log' button: LogListBox=present | descendants 146 -> 155   ← 대조
```

- **속성은 실제로 바뀝니다** — 체크가 `On → Off` 로 갑니다. "그것조차 없는" 것은 아닙니다.
- **화면은 안 바뀝니다** — UIA 자손 수가 146 에서 그대로입니다.
- **대조가 분명합니다** — 패널의 `Log` 버튼은 같은 척도에서 **146 → 155**.

**두 경로는 서로 다른 것을 가리킵니다.** 메뉴 토글은 `Show*Panel`(아무도 안 읽음), 패널 버튼은
`AnalysisTab`(가시성이 실제로 읽음)입니다.

**`ShowRuntimePanelMenuItem` 1개는 측정하지 못했습니다** — 첫 순회에서 메뉴가 열리기 전에 찾아
`not found` 로 건너뛰었습니다. 나머지 일곱과 같은 코드 모양이지만 **누른 결과는 안 봤습니다.**

## 6. 실측 (verbatim)

```
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개 / 오류 0개

dotnet test …E2ETests… --no-build                      (Mock)
통과!  - 실패: 0, 통과: 57, 건너뜀: 2, 전체: 59 (28 s)

dotnet test …IntegrationTests… -c Debug
통과!  - 실패: 0, 통과: 198, 건너뜀: 1, 전체: 199

Repeat-E2E.ps1 -Times 3 -Backend Native → runs with a failure: 0 / 3
run 1 69.9s · run 2 69.5s · run 3 69.7s
```

Baseline 귀속: 통합 C-62 시점 **197** → **199**(가드 2건). E2E 59 불변 — 건너뜀 2는 실제
재획득이 난 실행(#136, C-55 이후의 정상 변동)입니다.

## 7. 미검증 (Gaps)

- **가드는 순서만 봅니다.** 그 순서가 실제로 줄을 살리는지는 §2 의 프로브가 봤고, **CI 에서는
  다시 확인되지 않습니다.**
- **`ShowRuntimePanelMenuItem` 은 눌러 보지 못했습니다**(§5).
- **자손 수 146 은 UIA 기준**입니다. 화면 픽셀은 보지 않았습니다 — 토글이 색이나 크기만 바꾸는
  경우라면 이 척도로는 안 보입니다(가능성은 낮습니다; 어떤 코드도 그 속성을 안 읽으므로).
- **거절 줄이 사람 눈에 읽히는지**는 C-62 의 사각형 측정(356×156)까지이고, 글자 판독은 하지
  않았습니다.
- **알림 큐로 옮길 때의 동작은 재지 않았습니다** — 판정이 후보 1이라 후보 2는 확인 대상이
  아니었습니다.

## 8. 잔여 위험 (Residual risk)

- **소스 가드는 리팩터링에 약합니다.** 호출이 헬퍼로 빠지거나 생성자가 분할되면 이름을 못 찾고
  실패합니다 — 실패가 조용한 통과보다 낫지만, 소음이 될 수 있습니다.
- **주석 제거는 `//` 만 봅니다.** `/* */` 로 감싸면 여전히 통과합니다.
- **View 메뉴 항목 8개 중 7개가 눌러도 아무 일도 없습니다.** 사용자가 패널을 끄려 해도 꺼지지
  않고, 체크만 바뀌어 **꺼졌다고 표시됩니다** — 화면과 표시가 어긋납니다. 이 카드는 재기만
  했습니다.
- **`ResetLayout` 이 그 속성들을 `true` 로 되돌립니다** — 체크가 다시 켜지므로, 사용자가 보기엔
  "레이아웃 초기화가 패널을 되살렸다" 로 읽힐 수 있습니다. 실제로는 아무 일도 없었습니다.

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
dotnet build clients/ImageProcTest.slnx -c Debug      # ← 앱 exe 를 만드는 것은 이쪽
dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug \
  --filter "FullyQualifiedName~StartupRejectionSurvivesTests"
grep -rn "\bShowLogsPanel\b" gui/ImageProcTest --include=*.cs --include=*.xaml
powershell -File clients/ImageProcTest.E2ETests/Staging/Repeat-E2E.ps1 \
  -Times 3 -Backend Native -NativeDir build/ci-common/bin -ResultsDirectory build/e2e-c63
```
