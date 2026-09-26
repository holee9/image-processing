# GUI-C-60 — 세 표시를 한 곳에서 (#161)

> **이번 실행이 쓴 네이티브**: run **34535620953** · head `83ffa7ae…` ·
> `xpe_preprocess.dll` md5 `9f81d2f07fb7f7e23e90c86332467105`.

- 카드: GUI-C-60 · Refs #161 #149 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 1건 — 미푸시
- **할 일 2 의 제 가설은 틀렸습니다 — 관측으로 배제했습니다**(§3).
- **결과: Native 3회 0 실패 · Mock 0/58/1/59 · 통합 0/193/1/194 · slnx 0경고 0오류**

---

## 1. ② 먼저 — 세 표시가 무엇을 읽는지 쟀습니다

바인딩을 따라가 확인한 결과입니다.

| 표시 | 무엇을 읽나 | 해석하나 |
|---|---|---|
| 뷰포트 HUD | `NormalizeMode(CompareMode)` ← `CompareMode` 는 `Settings.ComparisonMode` 에 양방향 바인딩(`ViewportShell.xaml:18`) | **예** |
| `HelpText` (UIA) | `{Binding Settings.ComparisonMode}` (`MainWindow.xaml`) | 아니오 |
| 메뉴 체크 | `Settings.ComparisonMode` 를 각 항목의 `ConverterParameter` 리터럴과 비교 | 아니오 |

**셋은 같은 속성을 읽습니다.** 갈라진 이유는 **셋 중 하나만 그 문자열을 해석**했기 때문이고,
그래서 저장값이 "스스로로 해석되는 값" 일 때만 셋이 일치했습니다.

**따라서 고칠 것은 "표시를 맞추는 것" 이 아니라 "속성이 셋을 갈라놓을 수 있는 값을 담지 못하게
하는 것" 입니다.** `Models/ComparisonModes.cs` 가 그 문을 맡습니다 — 모드 목록(`All`),
`IsKnown`, `Normalize` 하나씩. 같은 저장소의 `CalibrationStageMode` 가 이미 같은 문제를 같은
모양으로 풀고 있어서 그것을 따랐습니다.

**모드 어휘가 세 벌 있었습니다**: 뷰모델의 배열, 렌더러의 `switch`, 설정 속성의 기본값. 셋을
지우고 `ComparisonModes` 를 참조하게 했습니다.

**하나에서 못 나오게 할 이유는 없었습니다.** UIA 가 다른 형식을 요구하지도 않고(`HelpText` 는
문자열), 메뉴 리터럴은 XAML 이라 컴파일 검사가 없을 뿐입니다 — 그래서 그 리터럴이 실제로
지원 모드인지 **테스트로** 확인합니다(§2 `EveryMenuParameter_IsASupportedMode`).

## 2. ① 값 검증 — 거절하고, 이름을 부릅니다

`AppSettings.ComparisonMode` 의 setter 가 **유일한 문**입니다. 지원 모드가 아니면 기본값으로
바꾸고, **거절한 원문을 `RejectedComparisonMode` 에 남깁니다.** 셸이 기동 시 한 번 읽어
로그합니다:

```
appsettings.json: comparisonMode 'NotAMode' is not a supported comparison mode;
using 'SwipeVertical'. Supported: SwipeVertical, SwipeHorizontal, SplitLocked,
OverlayOpacity, DifferenceHeatmap, SourceOnly, ProcessedOnly.
```

조용히 바꾸지 않는 이유는 이 저장소의 선례입니다 — **#150**(자르지 말고 거절),
**QA-B-60**(읽지 못한 키를 이름으로 말한다).

### 저장된 값은 고치지 **않습니다** — 명시합니다

카드가 물은 항목입니다. **파일을 다시 쓰지 않습니다.** 사용자가 요청하지 않은 시점에 설정
파일을 고치는 것은 보고보다 큰 행위이기 때문입니다. **비용은 반복**입니다 — 파일을 고치거나
사용자가 설정을 저장하기 전까지 매 기동마다 같은 경고가 나옵니다. 파일이 **실제로 여전히
틀렸으므로** 그 반복은 정직한 쪽이라고 봤습니다. 다르게 판단하시면 한 줄입니다.

## 3. 할 일 2 — 제 가설이 틀렸습니다 (관측 3회)

C-59 에서 제가 적은 가설: *"키 제스처가 `MenuItem` 으로 포커스를 옮겨 메뉴가 열린 채 남는다."*

**직접 관측했고, 틀렸습니다.**

```
MENUPROBE baseline:  mode=SwipeVertical focus=Window/'ImageProcTest GUI-S0'
                     FileMenu=Collapsed BackendMenu=Collapsed ViewMenu=Collapsed PipelineMenu=Collapsed
MENUPROBE after F5:  … 전부 Collapsed, focus=Window
MENUPROBE after F6:  mode=SplitLocked        … 전부 Collapsed, focus=Window
MENUPROBE after F7:  mode=OverlayOpacity     … 전부 Collapsed, focus=Window
MENUPROBE after F8:  mode=DifferenceHeatmap  … 전부 Collapsed, focus=Window
MENUPROBE after F8 + ViewMenu click: ViewMenu=Expanded CompareModeMenuItem=Collapsed focus=MenuItem/'View'
```

**F5~F8 뒤 어떤 메뉴도 열려 있지 않고 포커스는 창에 있습니다.** 그리고 F8 직후의 메뉴 열기도
정상 동작합니다. → **가설 1 배제.**

**따라서 "제 Escape 가 실제 사용자 증상을 테스트에서만 지운다" 는 걱정도 함께 사라집니다** —
지울 증상이 없습니다. 이게 이 관측의 실질적 소득입니다.

이어서 두 개를 더 세워 배제했습니다:

- **가설 2 — 메뉴 항목 `Invoke()` 가 메뉴를 열어 둔다.** 관측: 항목 invoke 직후
  `ViewMenu=Collapsed`, 하위 메뉴 `absent`. 3회 반복 전부 정상. **배제.**
- **가설 3 — 사전 전환 버튼 클릭이 직후의 메뉴 클릭을 방해한다.** 버튼 클릭까지 넣어 3회
  반복. 전부 정상. **배제.**

### 재현 조건 — 간헐이 아니라 결정적입니다

Escape 회피를 빼고 재실행했습니다.

| 실행 범위 | 결과 |
|---|---|
| 진입점+비교 시나리오 18건 × 3회 | **3/3 모두 1건 실패**, 매번 `CompareProcessedOnlyMenuItem` |
| W-13 6건만 | 1건 실패 — 이번엔 **`CompareSourceOnlyMenuItem`** |
| 진입점 10건만 | 1건 실패 — `CompareProcessedOnlyMenuItem` |

**C-58 이 "간헐" 이라고 적은 것은 틀렸습니다 — 결정적입니다.** 다만 **어느 케이스가 실패하는지는
실행 집합에 따라 달라지고, 한 실행에 정확히 1건**입니다. 특정 항목(ProcessedOnly)의 문제가
아닙니다.

**원인은 여전히 미확정입니다.** 후보 셋이 관측으로 배제됐고, 남은 것은 "한 앱 인스턴스에서
여러 케이스가 누적될 때" 라는 방향뿐인데 그건 아직 가설도 아닙니다. **Escape 회피는 카드
지시대로 유지합니다.**

## 4. 증거 (Evidence)

### 반증 — 표시를 하나씩 어긋나게

| 어긋나게 한 표시 | 정상 7모드 단언 | 무효값 단언 | 메뉴 리터럴 단언 |
|---|---|---|---|
| **HelpText** (setter 가 raw 저장) | 통과 | **5건 실패** | 통과 |
| **HUD** (`Normalize` 가 상수 반환) | **6건 실패** | 통과 | 통과 |
| **메뉴** (리터럴 오타) | 통과 | 통과 | **1건 실패** |

```
[raw-setting]  실패! - 실패: 5, 통과: 8, 전체: 13
[constant-hud] 실패! - 실패: 6, 통과: 7, 전체: 13
[menu-typo]    실패! - 실패: 1, 통과: 12, 전체: 13
```

**가운데 줄이 카드가 요구한 "정상 쪽 단언" 의 값입니다** — `Normalize` 가 전부 같은 상수를
돌려주면 무효값 단언은 통과하지만 정상 7모드 단언이 6건 실패합니다. (`SwipeVertical` 만
통과하는데, 그것이 곧 그 상수이기 때문입니다.)

### 실측 (verbatim)

```
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개 / 오류 0개

dotnet test …E2ETests… --no-build                     (Mock)
통과!  - 실패: 0, 통과: 58, 건너뜀: 1, 전체: 59 (27 s)

dotnet test …IntegrationTests… -c Debug
통과!  - 실패: 0, 통과: 193, 건너뜀: 1, 전체: 194

Repeat-E2E.ps1 -Times 3 -Backend Native → runs with a failure: 0 / 3
run 1 70.2s · run 2 69.6s · run 3 70.4s
```

Baseline 귀속: 통합은 C-59 시점 **181건** → **194건**(단일 출처 단언 13건). E2E 59 불변.
Native 벽시계 70 s 로 C-59 의 71 s 와 같은 범위.

## 5. 미검증 (Gaps)

- **HUD 를 픽셀로 읽지 않았습니다.** HUD 표시는 그려진 글자라 UIA 로 읽을 수 없어, 그 표시의
  **입력**(`NormalizeMode` 의 반환)을 단언했습니다. "화면에 그 글자가 찍혔다" 는 재지
  않았습니다.
- **메뉴 체크 표시가 실제로 켜지는지 실행으로 보지 않았습니다.** XAML 리터럴이 지원 모드인지만
  확인했습니다. `IsChecked` 바인딩 자체는 C-58 에서 넣은 뒤 단언한 적이 없습니다.
- **기동 시 로그가 실제로 나오는지 실행으로 보지 않았습니다.** 로그 경로는 코드로만 확인했고,
  이를 E2E 로 보려면 배포된 `appsettings.json` 을 고쳐야 해서(커밋 금지 사유, C-46) 하지
  않았습니다.
- **§3 의 실패 원인을 찾지 못했습니다.** 후보 셋을 배제한 것이 결과입니다.
- **`SwipeHorizontal` 은 메뉴 항목이 없어 메뉴 표시가 "아무것도 안 켜짐" 입니다.** 이 카드는
  그것을 단언으로 고정만 했고(#149 의 미결), 고치지 않았습니다.
- **대소문자 다른 값(`"sourceonly"`)을 거절하도록 했습니다.** 관대하게 받는 편이 나은지는
  판단하지 않았습니다 — XAML 비교가 대소문자를 구분하므로 받아 주면 그 층에서 다시 갈라집니다.

## 6. 잔여 위험 (Residual risk)

- **경고가 매 기동 반복됩니다**(§2). 파일을 고치지 않기로 한 대가입니다.
- **`RejectedComparisonMode` 는 한 번 설정되면 지워지지 않습니다.** 나중에 유효한 모드를 골라도
  남습니다 — 셸이 기동 시 한 번만 읽기 때문에 의도한 동작이지만, 다른 곳에서 읽으면 낡은
  정보로 보일 수 있습니다.
- **Escape 회피가 남아 있고 원인은 모릅니다.** 관측된 세 후보는 아니므로, 실제 사용자 증상을
  가리고 있을 가능성은 낮아졌지만 0 은 아닙니다.
- **링크된 소스가 4개 늘었습니다**(통합 테스트). 이 파일들이 WPF 를 참조하게 되면 그 테스트
  프로젝트가 깨집니다 — 링크 패턴의 알려진 대가입니다.

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug \
  --filter "FullyQualifiedName~ComparisonModeSingleSourceTests"
dotnet test clients/ImageProcTest.E2ETests/… --no-build \
  --filter "FullyQualifiedName~ComparisonEntryPointScenarios" --logger "console;verbosity=detailed"
powershell -File clients/ImageProcTest.E2ETests/Staging/Repeat-E2E.ps1 \
  -Times 3 -Backend Native -NativeDir build/ci-common/bin -ResultsDirectory build/e2e-c60
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
```
