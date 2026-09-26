# GUI-C-77 — 오래된 영상은 실재합니다. 다만 null 경로가 아니라 VOI 직접 입력 경로입니다 (#171)

- 카드: GUI-C-77 · Refs #171 #165 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 1건 — 미푸시 (파일 3개: `GuiAutomationReport.cs`, `MainWindow.xaml.cs`, `README.md`)
- **1순위: 재현했습니다 — 대조군 둘 사이에서.** VOI 값을 입력란에서 바꾸면 **처리 영상은 그대로인데
  영상 옆 HUD 는 새 값을 표시**합니다(§1.4).
- **정정 하나**: C-76 에서 866행을 "새 결과가 없으면 이전 것을 유지" 로 읽었는데, **그 줄은
  `ProcessedPreview is not null` 가드 안에 있습니다.** 그 줄의 `??` 는 죽은 코드입니다(§1.1).
- **2순위: 상수 `true` 가 `gui-automation` CI 게이트를 초록으로 붙잡고 있었습니다.** 실측하면
  `Passed=False`. 필드를 제거했고 `Passed=True`(§2).
- **3순위: GUI VOI 기본값을 정하는 곳은 SRS·SPEC 에 없고, 가이드 DISP-INT-001 §4 만 40/400**(§3).
- **결과: Mock 0/71/1/72 · 통합 0/200/1/201 · slnx 0경고 0오류 · 자동화 `Passed=True`**

---

## 1. 1순위 — 오래된 영상 경로

### 1.1 정정 — 866행은 null 이 되지 않습니다

```
861  if (result.Ran && result.ProcessedPreview is not null)
866      ProcessedImage = result.ProcessedPreview ?? ProcessedImage;
```

**C-76 보고서가 이 줄을 가드 없이 읽었습니다.** 866행에서 `result.ProcessedPreview` 는 절대 null 이
아니고, `?? ProcessedImage` 는 도달하지 않습니다. **#171 이 물은 "null 이 되는가" 의 답은 "그 줄에서는
안 된다" 입니다.** 다만 질문의 본체(오래된 영상이 새 것처럼 남는가)는 다른 경로로 **예**입니다.

### 1.2 `ProcessedImage` 대입문 전수

범위: `gui/ImageProcTest` 의 `*.cs`(`obj`/`bin` 제외), `grep "ProcessedImage = "`.

| 행 | 대입 | 시점 |
|---|---|---|
| 773 | `loadedFrame.ProcessedPreview ?? loadedFrame.Preview` | 영상 로드 |
| 810 | `processedFrame.ProcessedPreview ?? processedFrame.Preview` | 디스플레이 파이프라인 성공 |
| 866 | (위) | 전처리 성공 |

**`ProcessedImage` 를 비우는 대입은 0건입니다.** 한 번 그려진 처리 영상은 위 셋 중 하나가 새로 쓸
때까지 남습니다.

### 1.3 입력이 바뀌어도 처리 영상이 갱신되지 않는 경로 — 넷

범위: 위 파일 + `Services/MockXpeBackend.cs` + `Services/RealXpeBackend.cs` 를 **읽어서** 열거.

| # | 경로 | 사용자에게 가는 신호 | 실행 |
|---|---|---|---|
| **a** | **Analysis 패널 `VoiWindowCenterInput`(과 Width) 직접 수정** — `OnSettingsPropertyChanged`(1092행)가 VOI 변경에 파이프라인을 다시 돌리지 않음 | **없음** | **재현함** |
| b | `ApplyDisplayPipelineAsync` 예외 → catch(819행) | 상태 표시줄 + ERROR 알림 | 실행 안 함 |
| c | `RunPreprocessing` 거절(`Ran=false` — Mock 은 **항상**) | 상태 표시줄 + WARN 알림 | 실행 안 함 |
| d | `RunPreprocessing` 이 `Ran=true` 인데 `Pixels` 가 null(`RealXpeBackend.cs` 277행 분기) | 상태 표시줄만 | 실행 안 함, **발생 가능한지 모름** |

**알림은 화면에 표시되지 않습니다**(C-76: `Alerts` 를 바인딩하는 XAML 0건). b·c 의 "신호" 중
알림은 사용자에게 닿지 않습니다.

**바디파트 변경은 이 목록에 없습니다** — `ApplyBodyPartPreset`(883행)이 끝에서
`ApplyDisplayPipelineAsync()` 를 부릅니다(902행). 그래서 대조군으로 썼습니다.

### 1.4 재현 (a) — 대조군 둘 사이에서

**관측 도구가 먼저 문제였습니다.** 픽셀 캡처는 이 앱에서 눈멀었고(C-72), 뷰포트는 UIA 에 없습니다.
그래서 **앱에 임시 계측**을 넣었습니다: `ProcessedImage` 가 대입될 때마다 **픽셀 SHA-256 앞 12자 +
객체 해시**를 계산해 `ZoomPercentText` 의 `AutomationProperties.ItemStatus` 로 내보냄(측정 후 원복,
`git diff` 로 확인). **화면에 넘겨진 바로 그 비트맵의 지문**입니다.

Workflow 픽스처(Mock, `synthetic_1024x1024.raw`), 임시 프로브 `StaleProbe`(삭제함):

| 단계 | 처리 영상 지문 | 영상 옆 HUD | 요약(영상을 만든 파라미터) |
|---|---|---|---|
| 시작 | `079BA92AB360@375856C` | `C 32768 · W 65535` | `VOI(Linear, C=32768, W=65535)` |
| **대조군 1**: 바디파트 → Bone | **`522A8D50F4BA@5F1DBF`** ← 바뀜 | `C 40000 · W 30000` | `C=40000, W=30000` |
| **VOI center 에 `12345` 입력** | **`522A8D50F4BA@5F1DBF`** ← 그대로 | **`C 12345 · W 30000`** | **`C=40000`** |
| +3 s | `522A8D50F4BA@5F1DBF` ← 그대로 | `C 12345 · W 30000` | `C=40000` |
| **대조군 2**: Pipeline → Apply Display Pipeline | **`6E9C8ACA250A@A39E1A`** ← 바뀜 | `C 12345 · W 30000` | `C=12345` |

**읽는 법**:

- **도구는 바뀌는 것을 봅니다** — 대조군 두 번 모두 지문이 바뀌었습니다. 그러니 가운데 두 줄의
  "그대로" 는 **실제로 같은 비트맵**이라는 뜻입니다(픽셀 해시와 객체 해시 둘 다 동일).
- **HUD 는 영상이 아니라 설정을 봅니다** — `Views/ViewportShell.xaml:136-138` 이
  `Settings.VoiWindowCenter`·`VoiWindowWidth` 에 바인딩됩니다. 그래서 **C=40000 으로 그려진 영상
  바로 옆에 `C 12345` 가 표시됩니다.** 영상이 오래됐다는 것을 알려 주는 장치가 없을 뿐 아니라,
  **영상 옆 라벨이 그 영상을 만들지 않은 값을 말합니다.**
- 영상을 만든 값은 **상태 표시줄과 STUDY 블록의 요약**에만 남아 있습니다(다른 영역).

**HAZ-GUI-004 의 위험 상황 문장과 같습니다**: *"파라미터 변경(예: VOI center) 후 preview가 이전
결과를 표시 → 사용자가 잘못된 결과를 실제 결과로 오인"*. **고치지 않았습니다**(카드 지시).

## 2. 2순위 — `ResizableDiagnosticsLayoutDetected`

### 2.1 판단: **측정이 아니라 제거**

- 그 필드가 말하던 것은 **`GridSplitter` 둘**입니다 — `docs/design/reference/MainWindow.xaml:733`
  (`DiagnosticsColumnSplitter`), `:765`(`LogsAlertsSplitter`). 옛 레이아웃의 사본에만 있습니다.
- 현재 앱 XAML 의 `GridSplitter`: **0건**(C-76).
- **그 레이아웃을 요구하는 문서가 없습니다.** 검색 범위: `origin/main` 의 `docs/`·`.moai/specs/`,
  `resizable|GridSplitter|splitter`(대소문자 무시). 나온 것은 `docs/design/reference/` 의 옛 사본
  둘(XAML·README)뿐입니다.
- **측정으로 바꾸면 영원히 false** 이고, 그러면 **의도적으로 없앤 레이아웃 때문에 CI 가 빨개집니다.**
  재는 대상이 사라졌으니 필드를 지우는 것이 맞다고 판단했습니다.

### 2.2 `Passed` 가 어떻게 되는가 — **세 판을 실제로 돌렸습니다**

CI `gui-automation` 과 같은 인자(`--automation-raw … synthetic_1024x1024.raw --automation-backend Mock
--automation-width 1024 --automation-height 1024`)로 앱을 자동화 모드로 실행했습니다.

| 판 | `ResizableDiagnosticsLayoutDetected` | `Passed` |
|---|---|---|
| A. 기준선(현재 코드, 상수 `true`) | `True` | **True** |
| **B. 임시 실측**(시각 트리에서 `GridSplitter` 개수 > 0) | **`False`** | **False** |
| C. 필드 제거 후 | (필드 없음) | **True** |

**B 가 발견입니다.** `ci.yml` 의 `gui-automation` 은
`if ($p.ExitCode -ne 0 -or -not $r.Passed) { … exit 1 }` 로 **`Passed` 를 게이트로 씁니다.** 이 상수는
**없는 레이아웃을 "감지" 로 적어 CI 게이트를 초록으로 붙잡고 있었습니다.**

(참고: B 판에서도 앱 종료 코드는 0 이었습니다 — 게이트는 `Passed` 를 따로 읽어서 잡습니다.)

### 2.3 제거한 것

- `Models/GuiAutomationReport.cs` — 속성 삭제, **그 자리에 왜 지웠는지 주석 4줄**
- `MainWindow.xaml.cs` — 대입 1줄, `Passed` 조건 1줄 삭제
- `README.md` — 보고서 필드 목록에서 삭제, "지운 것" 절의 문장 갱신

**소비자**: 이 필드를 읽는 곳은 `Passed` 조건과 README 뿐이었습니다(`ci.yml` 은 `Passed` 만 읽음).
`docs/design/reference/gui-README.md:103` 에도 이름이 있지만 **리더 소유의 옛 사본**이라 손대지 않았습니다.

### 2.4 같은 형태의 census — **대입문으로**

범위: `gui/ImageProcTest` 의 `*.cs` 전체에서, `GuiAutomationReport` 의 **불리언 속성 17개** 각각에 대한
모든 대입(`X.Prop = …;` 과 객체 초기화자 `Prop = …,`, 여러 줄 우변 포함).

**상수로만 채워지는 것: 3건.** 각각의 맥락을 읽었습니다.

| 속성 | 대입 | 조건 | 판정 |
|---|---|---|---|
| **`ResizableDiagnosticsLayoutDetected`** | `= true` (265행) | **무조건** | **거짓** — 실측 false. **제거함** |
| **`ComparisonViewportDetected`** | `= true` (213행) | **무조건** | **재지 않음, 그러나 지금은 참** — 같은 임시 판에서 시각 트리의 `ImageComparisonViewport` 를 세니 **true**. **안 고쳤습니다**(카드는 이 필드를 세라고만 함) |
| `HelpWindowOpened` | `= true` (331행) | `if (OwnedWindows.OfType<HelpWindow>().FirstOrDefault() is { } helpWindow)` 안 | **측정임** — 조건이 곧 측정. 기본값 false |

나머지 14건은 전부 식(expression)으로 채워집니다(보고서 §6 에 전체 출력).

**`ComparisonViewportDetected` 는 `Resizable` 과 같은 모양입니다** — 뷰포트가 사라져도 `true` 로 남습니다.
오늘은 참이라 드러나지 않을 뿐입니다.

## 3. 3순위 — VOI 기본값은 어느 쪽이 요구에 맞는가

**검색 범위**: `origin/main` 의 `.moai/specs/`, `docs/`(`docs/design/reference` 제외).
`window ?center|voiWindowCenter|WindowCenter|window_center` 를 찾고 그중 `default|기본|32768|65535|40|400|abdomen`
이 같이 있는 줄, 그리고 `32768|65535` 를 `voi|window|default|기본` 과 함께. SRS-001 과
`SPEC-XPE-GUI-IT` 는 `voi|window` 로 따로 봤습니다.

| 출처 | 성격 | 무엇을 말하나 |
|---|---|---|
| `SPEC-XPE-P1B-DISP/spec.md:315` **REQ-DISP-017** | **SHALL** | **네이티브 `xpe_voi_preset_create`** 의 바디파트별 값(HU): ABDOMEN **C=40 W=400**, BONE 500/2000, LUNG −600/1600, HEAD 40/80. **GUI 설정 기본값이 아닙니다** |
| `XPE-SRS-001:61` SRS-FUNC-021 | Must | VOI LUT 모드 3종, 바디파트별 ≥20 프리셋, W/L drag ≤16 ms. **기본값 없음** |
| `SPEC-XPE-GUI-IT` | — | VOI 기본값 없음 |
| **`XPE-GUI-DISP-INT-001` §4 "Required additions"** | 가이드(Controlled Draft) | `AppSettings.VoiWindowCenter = 40.0f`, `VoiWindowWidth = 400.0f` |
| `XPE-GUI-DISP-INT-001` §4.1 표 | 가이드 | Window Center **기본 40, 범위 −4096 to 4096** / Width 기본 400, 범위 1–8192 / 바디파트 기본 Abdomen |

**셋과의 대조**:

| 쪽 | 값 | 가이드(DISP-INT-001)와 |
|---|---|---|
| SelfCheck | 40 / 400 | **일치** |
| 픽스처 템플릿 | 40 / 400 | **일치** |
| 앱 `AppSettings.cs` | 32768 / 65535 | **불일치** — 그리고 **32768 은 가이드의 범위(−4096..4096) 밖** |

**앱 값이 언제 왜 바뀌었는가**: 커밋 **`3dc84f7`**(2026-05-09) *"fix(gui): VOI 기본값 CT → 평판 X선
검출기 값으로 수정 (E2E 검증 통과)"*. README 의 근거 문단: 16-bit raw(15000–65535)에 CT 기본값을
쓰면 **VOI 상한(240) 위로 전부 넘어가 하얗게 잘린다**. **가이드는 그 뒤로 갱신되지 않았습니다.**

**사실로 정리하면**:

- **SHALL 급 요구는 GUI 기본값을 정하지 않습니다.** REQ-DISP-017 의 40/400 은 **네이티브 프리셋 API
  의 HU 값**입니다.
- **GUI 기본값을 적은 유일한 문서는 가이드**이고, 그 값(40/400)은 SelfCheck·템플릿과 같고 앱과 다릅니다.
- **두 값은 서로 다른 영역(domain)의 값입니다** — 40/400 은 HU, 32768/65535 는 raw DN. 앱의 모달리티
  기본값(slope=1, intercept=0)에서는 raw → "HU" 가 항등이라, **가이드 값을 그대로 쓰면 README 가 적은
  대로 하얗게 잘립니다.** 어느 쪽을 고칠지는 **이 영역 문제**에 달려 있고, 판정은 리더 몫입니다.
- 덤: `SPEC-XPE-GUI-IT/research.md:25` 가 `MainWindow.xaml.cs` 에서 `CompositeXpeBackend(…)` 를 쓴다고
  적는데, 그 타입은 `gui/`·`clients/` 어디에도 없습니다(C-76 census 결과).

## 4. 두 WPF 앱 — README 에 한 줄

`gui/ImageProcTest/README.md` 맨 위에 인용 블록으로 넣었습니다: 이쪽이 FlaUI 스위트가 띄우는 UI,
`clients/ImageProcTest` 는 네이티브 진단·백엔드, **편집 전에 어느 쪽인지 확인할 것.**

## 5. `#171` — 사실 코멘트 1건

https://github.com/holee9/image-processing/issues/171#issuecomment-5706599750

861·866행의 가드, 대입문 3건 전수, 경로 넷(실행 여부 표기), 재현 표(지문·HUD·요약), HUD 바인딩 위치,
증거 경로. **해석·판정 문장은 넣지 않았습니다.** C-76 의 866행 서술이 가드를 읽지 않았다는 사실은
적었습니다.

## 6. 실측 (verbatim)

```
[1순위 — 임시 계측 + StaleProbe, Mock Workflow 픽스처]
[start] hash=079BA92AB360@375856C   HUD='C  32768  · W  65535'  input='32768'  summary='VOI(Linear, C=32768, W=65535)…'
[after body part -> Bone] hash=522A8D50F4BA@5F1DBF  HUD='C  40000  · W  30000'  input='40000'  summary='VOI(Linear, C=40000, W=30000)…'
[after typing VOI center 12345] hash=522A8D50F4BA@5F1DBF  HUD='C  12345  · W  30000'  input='12345'  summary='VOI(Linear, C=40000, W=30000)…'
[3 s later] hash=522A8D50F4BA@5F1DBF  HUD='C  12345  · W  30000'  summary='…C=40000…'
applyMenuItem found=True enabled=True
[after Apply Display Pipeline] hash=6E9C8ACA250A@A39E1A  HUD='C  12345  · W  30000'  summary='VOI(Linear, C=12345, W=30000)…'

[2순위 — 자동화 모드, CI gui-automation 과 같은 인자]
A. baseline    exit=0 Passed=True  Resizable=True  ComparisonViewport=True HelpWindowOpened=True
B. real probe  exit=0 Passed=False Resizable=False ComparisonViewport=True HelpWindowOpened=True
C. removed     exit=0 Passed=True  Resizable=      ComparisonViewport=True  (has Resizable property: False)

[불리언 대입문 census — 17 속성]
Passed 4 · PreprocessRan 2 · LastRawDirPersisted 1 · HelpWindowOpened 1 <== CONST ONLY (guarded by if)
HelpDocumentLoaded 1 · CanonicalMenuGroupsDetected 1 · PlannedMenuPlaceholdersDetected 1 · ToolbarMenuCommandParity 1
ResizableDiagnosticsLayoutDetected 1 <== CONST ONLY · DisplayPipelineApplied 3 · CalibrationEvaluationEvidenceExported 1
DisplayPanelVisible 1 · VoiPresetApplied 1 · ComparisonViewportDetected 1 <== CONST ONLY
ComparisonSourcePreserved 1 · ComparisonEvidenceExported 1 · MenuCommandReportCreated 1

[3순위]
SPEC-XPE-P1B-DISP/spec.md:315  REQ-DISP-017 … ABDOMEN (center=40, width=400) …
XPE-GUI-DISP-INT-001 §4   public float VoiWindowCenter { get; set; } = 40.0f;
XPE-GUI-DISP-INT-001 §4.1 | Window Center | … | -4096 to 4096 | 40 |
git log -S"_voiWindowCenter = 32768" → 3dc84f7 2026-05-09 fix(gui): VOI 기본값 CT → 평판 X선 검출기 값으로 수정

dotnet build clients/ImageProcTest.slnx -c Debug     경고 0개 / 오류 0개
dotnet test …E2ETests… --no-build (Mock)   통과! 실패 0, 통과 71,  건너뜀 1, 전체 72 (1 m 1 s)
dotnet test …IntegrationTests… --no-build  통과! 실패 0, 통과 200, 건너뜀 1, 전체 201
```

## 7. 미검증 (Gaps)

- **경로 b·c·d 는 실행하지 않았습니다.** d 는 발생할 수 있는지조차 모릅니다.
- **Native 백엔드에서 재현하지 않았습니다.** Mock 은 VOI 를 픽셀에 적용하므로(`CreateDisplayPreview`)
  재현이 성립했지만, Native 가 같은 모양인지는 재지 않았습니다 — **경로 a 는 VM 코드라 백엔드와
  무관해 보이지만, 보이는 것과 잰 것은 다릅니다.**
- **Window Width 입력란**은 같은 경로로 보이지만 **Center 만 쟀습니다.**
- **사람이 보기에 두 영상이 구별되는지**(C=40000 과 C=12345 의 차이가 눈에 띄는지)는 재지 않았습니다 —
  지문이 다르다는 것은 픽셀이 다르다는 것이지 눈에 띈다는 뜻이 아닙니다.
- **`ComparisonViewportDetected` 를 고치지 않았습니다.** 같은 모양이지만 카드가 세라고만 했습니다.
- **상수 불리언이 다시 들어오는 것을 막는 가드**는 만들지 않았습니다.
- **`gui-automation` 을 CI 에서 돌려 보지 않았습니다.** A/B/C 는 같은 인자로 로컬에서 돌린 것입니다.
- **3순위의 "영역(HU vs raw)" 판단**은 코드·README 를 읽은 것이고, 모달리티 변환을 실행으로 따라가지 않았습니다.
- 3순위 검색은 **패턴에 걸리는 표현만** 봤습니다. 다른 말로 기본값을 정한 문서가 있으면 못 봤습니다.

## 8. 잔여 위험 (Residual risk)

- **영상 옆 HUD 가 영상을 만들지 않은 값을 표시합니다**(§1.4). 오래된 영상에 **틀린 라벨이 붙습니다.**
- **`ComparisonViewportDetected` 도 상수입니다.** 뷰포트가 사라져도 `Passed` 는 그 줄로는 안 떨어집니다.
- **`gui-automation` 의 `Passed` 조건에는 SelfCheck 가 실패하는 VOI 기본값 문제가 걸려 있지 않습니다** —
  `VoiPresetApplied` 는 Abdomen 프리셋 적용만 봅니다.
- **VOI 기본값의 요구·앱·도구가 세 갈래**이고, 영역이 다른 두 값이 같은 칸을 두고 경쟁합니다(§3).

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| `ProcessedPreview` null 경로 열거 + 재현 + 대조군 | GUI-C-77 §1 — 완료(null 경로는 866행에서 불가, 오래된 영상은 경로 a 로 재현) |
| `#171` 사실 코멘트 | GUI-C-77 §1 — 완료 |
| `ResizableDiagnosticsLayoutDetected` 측정 또는 제거 + `Passed` 보고 | GUI-C-77 §2 — 제거, A/B/C 세 판 |
| 상수 불리언 census (대입문) | GUI-C-77 §2 — 3건(거짓 1 · 미측정 참 1 · 조건부 측정 1) |
| VOI 기본값 요구 위치 | GUI-C-77 §3 — 완료(가이드만, SHALL 없음) |
| README 에 두 앱 한 줄 | GUI-C-77 §4 — 완료 |
| `ComparisonViewportDetected` 상수 | **새 카드 후보** — 리더 판정 |
| 오래된 영상 옆 HUD 라벨 불일치 | `#171` 에 사실로 기록 — 리더 판정 |

## 부록 — 사용한 명령

```bash
grep -rn "ProcessedImage = " gui/ImageProcTest --include=*.cs
python boolcensus.py                                   # GuiAutomationReport 불리언 17개의 대입문 전수
powershell -NoProfile -File auto77.ps1                 # CI gui-automation 과 같은 인자로 자동화 모드 실행
dotnet test …E2ETests… --filter "FullyQualifiedName~StaleProbe" --logger "console;verbosity=detailed"
MSYS_NO_PATHCONV=1 git grep -n -i -E "window ?center|voiWindowCenter|…" origin/main -- docs .moai/specs
git log -S"_voiWindowCenter = 32768" -- gui/ImageProcTest/Models/AppSettings.cs
gh issue comment 171 --body-file c171.md
```
