# GUI-C-71 — 창이 열립니다, 그리고 안 열리면 말합니다 (#166)

- 카드: GUI-C-71 · Refs #166 #136 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 1건 — 미푸시
- **`ProcessedImage` 도 같은 이유로 던진다는 것을 실행으로 확인했습니다**(§2) — 선언만 보고 적지 않았습니다.
- **관측 방법 하나를 정정합니다: 분리 창은 데스크톱 자식이 아니라 메인 창의 자손입니다**(§3).
- **할 일 2 census: 같은 습관의 추가 사례 0건**(§5) — 찾은 범위 포함.
- **결과: Mock E2E 0/69/1/70 · 통합 0/200/1/201 · slnx 0경고 0오류**

---

## 1. 고쳤습니다 — 모드를 하나씩

`BindDetachedViewport` 가 **모드를 인자로 받습니다.** 여덟 자리를 하나씩 확인해 걸었습니다.

| # | 대상 | 소스 | setter | 건 모드 |
|---|---|---|---|---|
| 1 | `SourceImageProperty` | `SourceImage` | **private**(`:301`) | **OneWay** |
| 2 | `ProcessedImageProperty` | `ProcessedImage` | **private**(`:307`) | **OneWay** |
| 3 | `CompareModeProperty` | `Settings.ComparisonMode` | public(`:298`, 게이트 있음) | TwoWay |
| 4 | `ZoomScaleProperty` | `Settings.ComparisonZoomScale` | public(`:322`) | TwoWay |
| 5 | `PanXProperty` | `Settings.ComparisonPanX` | public(`:332`) | TwoWay |
| 6 | `PanYProperty` | `Settings.ComparisonPanY` | public(`:342`) | TwoWay |
| 7 | `SwipePositionProperty` | `Settings.ComparisonSwipePosition` | public(`:352`) | TwoWay |
| 8 | `OverlayOpacityProperty` | `Settings.ComparisonOverlayOpacity` | public(`:362`) | TwoWay |

**`SourceImage` 의 setter 는 열지 않았습니다**(카드 지시). 분리 창이 원본을 바꾸는 동작은 요구에
없습니다. `MENU-001` §4.3 의 "동기화되지 않은 창을 만들지 말 것" 은 **원본 → 창** 방향으로
충족되고, 그 방향이 `OneWay` 입니다.

**무신호도 고쳤습니다.** 메서드를 `OpenDetachedComparisonViewer`(try/catch) + `…Core`(본체)로
갈라, 실패하면 **상태 표시줄과 로그 둘 다**에 적습니다.

```csharp
StatusText = $"Detached comparison viewer failed to open: {ex.GetType().Name}.";
Log($"DetachComparisonViewerCommand failed: {ex.GetType().Name}: {ex.Message}");
```

## 2. `ProcessedImage` — **실행으로 확인했습니다**

카드가 "선언만 보고 같은 이유라고 단정하지 말라" 고 했으므로, **①번만 `OneWay` 로 바꾸고
②번은 `TwoWay` 인 채로** 돌렸습니다. 새 실패 신호가 그대로 답을 줬습니다.

```
statusAfterDetach='Detached comparison viewer failed to open: InvalidOperationException.'
LOG: DetachComparisonViewerCommand failed: InvalidOperationException: TwoWay 또는 OneWayToSource
     바인딩은 …형식의 읽기 전용 속성 'ProcessedImage'에서 작동하지 않습니다.
```

②를 `OneWay` 로 바꾸자 창이 열렸습니다. **③~⑧은 ②보다 뒤에 있으므로, 창이 열렸다는 것이
여섯 개 전부 `TwoWay` 를 받아들였다는 뜻입니다** — 하나라도 거부했으면 같은 자리에서 멈췄습니다.

## 3. **관측 방법을 정정합니다 — 분리 창은 메인 창의 자손입니다**

창이 열린 뒤에도 **데스크톱 자식 수는 1 그대로**였습니다. 앱 안을 계측해 보니 창은 실재합니다.

```
PROBE visible=True loaded=True hwnd=0x2B10648 appWindows=2 state=Normal w=1280 h=820 owner=set
poll1s processWindows=1 desktopChild=False underMain=True desktopDesc=True
poll2s …  poll3s …   (세 번 모두 같음)
```

`Owner` 가 걸려 있어 **UIA 가 소유 창 밑에 중첩**합니다. **그래서 C-46 과 제 C-70 이 쓴
"데스크톱의 최상위 창 목록" 은 이 창을 찾을 수 없는 도구였습니다** — 열려 있어도 안 보입니다.

**C-70 의 결론 자체는 살아 있습니다.** 그때는 예외가 **창이 만들어지기 전에** 났다는 것을
앱 안에서 직접 봤고, 결론은 그 증거가 지탱합니다. 다만 **데스크톱 목록은 근거가 아니었고**,
S11 은 그 방법을 쓰지 않습니다.

## 4. 단언 2건 — 따로 두었습니다

| 단언 | 자리 | 성격 |
|---|---|---|
| **S11** `DetachComparisonViewer_OpensTheWindow` | `Scenarios/Smoke/DetachViewerScenarios.cs` | **실행 관측** — 메뉴를 눌러 창이 생기는지 |
| **T1** `TheDetachCommand_ReportsAFailureOnBothSurfacesTheUserReads` | `IntegrationTests/Functional/DetachFailureIsReportedTests.cs` | **소스 가드** (한계는 아래) |
| T2 `TheSuccessClaim_StillFollowsShow` | 같은 파일 | 소스 가드 — 성공 문구가 `Show()` **뒤**에 있을 것 |

**S11 은 "예외가 안 난다" 가 아니라 "창이 있다" 를 단언합니다** — 옛 빌드는 예외를 내면서
조용했으므로 예외 모양의 단언은 엉뚱한 것을 쟀을 것입니다.

**T1 이 소스 가드인 이유와 그 약점을 그대로 적습니다.** 실패 경로를 앱 밖에서 만들 수
없습니다 — 그 예외가 바로 이 카드가 없앤 것이고, **두 테스트 프로젝트 모두 WPF 어셈블리를
참조하지 않습니다**(개별 소스 파일을 링크만 합니다). 그래서 T1 은 **"보고 코드가 지워지는
것"** 은 잡지만 **"보고 코드가 잘못 동작하는 것"** 은 못 잡습니다. 실행 관측은 S11 뿐이고
그것은 성공 경로만 봅니다.

T2 를 같이 둔 이유: T1 만 있으면 **항상 실패를 보고하는 구현**으로도 통과합니다.

### 반증 — 각각 하나씩만 터집니다

| 주입 | 결과 |
|---|---|
| `ProcessedImage` 를 `TwoWay` 로 되돌림 | **E2E 실패 1 / 통과 0** · 통합 **200 전부 통과** |
| catch 의 보고 두 줄 제거 | 통합 **실패 1 / 통과 199** · E2E S11 **통과** |

**하나를 고쳐 둘 다 통과하는 일은 없습니다.**

## 5. 할 일 2 — census: **추가 사례 0건**

### 찾은 범위

```
코드:  gui/ · clients/ 의 *.cs 에서  BindingMode.TwoWay  와  Mode = BindingMode
XAML:  gui/ · clients/ 의 *.xaml 에서  Mode=TwoWay        (그리고 Mode= 전체 분포)
추가:  BindsTwoWayByDefault 로 등록된 DP + 그 DP 를 Mode 없이 거는 자리
       읽기 전용(private set) 속성 13개가 XAML 바인딩에 등장하는 자리 전부
(obj/ 제외)
```

### 결과

| 자리 | 건수 | 대상이 쓰기 가능한가 |
|---|---|---|
| 코드 `BindingMode.TwoWay` | **6** (전부 이 카드의 `BindDetachedViewport` ③~⑧) | 예 — 전부 `Settings.*` 공개 setter |
| XAML `Mode=TwoWay` | **38** (경로 23종, 파일 5개) | **예 — 23종 전부 공개 setter** |
| XAML `Mode=OneWay` | 10 | — |
| `BindsTwoWayByDefault` DP | **5** (`ZoomScale`·`PanX`·`PanY`·`SwipePosition`·`OverlayOpacity`) | 쓰이는 자리 5곳 모두 **명시 `Mode=TwoWay` + `Settings.*`** |
| 읽기 전용 13개가 XAML 에 등장 | **4** (`StatusText`·`ActiveImageSummary`·`LaneAImage`·`LaneBImage`) | **전부 Mode 미지정**, 대상 DP 기본이 OneWay(`Content`·`Text`·`SourceImage`·`ProcessedImage`) |

**읽기 전용 13개**: `PreprocessRan` · `PreprocessStages` · `StatusText` · `ActiveImageSummary` ·
`MetadataText` · `DisplayPipelineSummary` · `SourceImage` · `ProcessedImage` · `ActiveImageFrame` ·
`LaneAImage` · `LaneBImage` · `LastAppliedVoiPreset` · `AppSettings.RejectedComparisonMode`.

**이 중 어느 것도 `TwoWay` 로 걸린 자리가 없습니다.** `SourceImage`/`ProcessedImage` DP 는
`BindsTwoWayByDefault` 가 **아니어서**, `LaneAImage`/`LaneBImage` 를 Mode 없이 거는 자리도
안전합니다. **고치지 않았습니다**(카드 지시) — 고칠 것이 없었습니다.

## 6. 시간 — Smoke 가 한 건 늘었습니다

| | Smoke | Workflow | Rendering |
|---|---|---|---|
| C-67 (로컬) | 23.2 s (21건) | 27.1 s | 0.6 s |
| **C-71 (로컬)** | **23.8 s (22건)** | 24.3 s | 0.6 s |

**S11 자체는 2.09 s**(처음 2.52 s 에서 줄임). 카드가 "싸게 만들 수 있으면 그것부터" 라고 해서
먼저 봤습니다: **대기가 아니라 상태 만들기**였고(메뉴 열기 + 창 닫고 정리), 줄일 수 있는 것은
**폴링 간격**이었습니다 — 창은 측정한 모든 판에서 **첫 폴에 잡혔으므로** 500 ms 를 250 ms 로
줄였습니다(상한 5 s 유지). 닫기 대기도 600 → 400 ms.

**CI 는 재지 않았습니다.** 마지막 CI 값(런 35071370089, Mock)이 Smoke 25.5 s 였으므로
**+2.0 s 면 약 27.5 s = 30 s 의 약 92 %** 로 **추정**됩니다. **추정이지 측정이 아닙니다.**
넘으면 카드 지시대로 시나리오를 빼지 않고 보고하겠습니다.

**트레이트는 붙이지 않았습니다** — C-69 판정 1("정본은 폴더", "먼저 붙이면 분류가 굳습니다")
그대로입니다. 새 클래스는 `Scenarios/Smoke/` 에 있고 `Category` 트레이트가 없습니다.

## 7. 실측 (verbatim)

```
dotnet build clients/ImageProcTest.slnx -c Debug        경고 0개 / 오류 0개
dotnet test …E2ETests… --no-build (Mock)   통과! 실패 0, 통과 69,  건너뜀 1, 전체 70 (49 s)
dotnet test …IntegrationTests… --no-build  통과! 실패 0, 통과 200, 건너뜀 1, 전체 201

[반증 A] ProcessedImage → TwoWay
  E2E(DetachViewerScenarios)  실패! 실패 1, 통과 0
  통합                         통과! 실패 0, 통과 200
[반증 B] catch 의 보고 두 줄 제거
  통합                         실패! 실패 1, 통과 199
  E2E(DetachViewerScenarios)  통과! 실패 0, 통과 1

[프로브, 삭제함] ②만 TwoWay 로 남긴 판
  statusAfterDetach='Detached comparison viewer failed to open: InvalidOperationException.'
  LOG: … 읽기 전용 속성 'ProcessedImage'에서 작동하지 않습니다.
[프로브, 삭제함] 창 위치
  poll1s processWindows=1 desktopChild=False underMain=True desktopDesc=True   (3회 동일)
  PROBE visible=True loaded=True hwnd=0x2B10648 appWindows=2 w=1280 h=820 owner=set

build/e2e-c71/c71b.trx   Smoke 23.8 (22) · Workflow 24.3 (25) · Rendering 0.6 (23)
  S11: 2089 ms
```

## 8. 미검증 (Gaps)

- **CI 에서 재지 않았습니다.** §6 의 27.5 s 는 **추정**입니다.
- **T1·T2 는 실행 관측이 아닙니다**(§4). 보고 코드가 있는지만 봅니다.
- **사람이 마우스로 누르는 경로는 여전히 못 쟀습니다** — C-70 과 같은 이유로 도구가 없습니다.
  따라서 **실패 시 `DispatcherUnhandledException` 대화상자가 뜨는지도 모릅니다**(이제 catch 가
  먼저 잡으므로 뜨지 않을 것으로 보이지만, 확인하지 않았습니다).
- **Native 백엔드에서 S11 을 돌리지 않았습니다.** 이번 배치는 Mock 만입니다.
- **분리 창의 내용이 실제로 원본을 따라 그리는지**는 단언하지 않았습니다 — S11 은 **창이
  생기는지**만 봅니다. 이미지를 로드한 뒤 두 창이 같은 것을 그리는지는 재지 않았습니다.
- **③~⑧이 TwoWay 로 실제 되돌려 쓰는지**(분리 창에서 줌을 바꾸면 본 창도 바뀌는지)도
  재지 않았습니다. 확인한 것은 **바인딩이 거부되지 않는다**는 것까지입니다.
- **census 는 정적 검색입니다.** 범위는 §5 에 적었고, 문자열로 조립되는 바인딩이나
  코드비하인드의 다른 `SetBinding` 경로는 위 패턴에 걸리지 않으면 보이지 않습니다.
- **S11 이 다른 시나리오와의 순서에 의존하지 않는다**는 것은 전체 실행 2회로만 확인했습니다.

## 9. 잔여 위험 (Residual risk)

- **Smoke 가 30 s 게이트의 약 92 %(추정)** 입니다. 다음 Smoke 시나리오 한 건이면 넘습니다.
- **S11 은 창을 닫고 끝냅니다.** 닫기가 실패하면 **S08 의 자손 수 단언이 흔들립니다** — 분리
  창이 메인 창의 자손으로 붙기 때문입니다(§3). 닫기 실패 자체를 단언하지는 않았습니다.
- **T1 이 소스 가드라, 보고가 잘못 동작하는 형태는 잡히지 않습니다**(§4).
- **`MENU-001` §4.3 은 `Detach Viewer` 를 "planned" 로만 적고 단계를 지정하지 않습니다.**
  이 명령이 어느 Phase 소속인지는 문서에 없습니다.

## 부록 — 사용한 명령

```bash
grep -rn "BindingMode.TwoWay\|Mode = BindingMode" gui clients --include=*.cs | grep -v obj
grep -rhoE 'Mode=[A-Za-z]+' gui clients --include=*.xaml | sort | uniq -c
grep -rn "BindsTwoWayByDefault" gui/ImageProcTest --include=*.cs
dotnet test …E2ETests… --filter "FullyQualifiedName~DetachViewerScenarios" --logger "console;verbosity=detailed"
dotnet test …E2ETests… --no-build --logger "trx;LogFileName=c71b.trx" --results-directory build/e2e-c71
```
