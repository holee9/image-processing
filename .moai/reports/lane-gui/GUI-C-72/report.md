# GUI-C-72 — 값이 움직이는가 (#166 후속)

- 카드: GUI-C-72 · Refs #166 #136 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 1건 — 미푸시
- **되돌려 쓰기는 실제로 일어납니다** — 대조군과 함께 쟀습니다(§1). **`TwoWay` 는 필요했습니다.**
- **원본 → 창(영상) 방향은 관측점이 없습니다**(§2). 픽셀 캡처는 **대조군이 눈멀었다고 말했습니다.**
- **CI 를 측정으로 바꿨습니다**(§3) — Mock Smoke **26.4 s(88 %)**. 다만 **Native 는 98.1 s** 입니다.
- **결과: Mock E2E 0/71/1/72 · 통합 0/200/1/201 · slnx 0경고 0오류**

---

## 1. 되돌려 쓰기(창 → 본체)는 **일어납니다** — 대조군 포함

분리 창 안에서 휠을 굴리면 **메인 창의 `ZoomPercentText`** 가 움직입니다.

```
W15 control  fit -> fit    at {X=1864,Y=504}      ← 분리 창이 없을 때 같은 점
W15 detached fit -> 85%                            ← 분리 창을 그 점 위로 옮긴 뒤
```

**대조군이 먼저 필요했습니다.** 처음에는 분리 창의 중앙에서 굴렸는데, **그 점은 메인 창의
뷰포트와 겹칩니다.** 대조군(분리 창 없이 같은 점)에서 **`fit` → `120%`** 가 나왔습니다 — 같은
제스처가 평범한 이유로도 확대됩니다. **그 판은 폐기했습니다**: 어느 창이 휠을 받았는지 말할 수
없습니다. 그래서 **분리 창을 메인 창 밖으로 옮기고**(`Move(main.Right + 20, main.Top)`) 겹치지
않는 점에서 다시 쟀습니다.

**결론: ③~⑧을 `TwoWay` 로 건 것은 옳았습니다.** `OneWay` 로 바꾸면 이 동작이 사라집니다.

**다만 실제로 확인한 것은 `ZoomScale` 하나입니다.** 나머지 다섯(PanX·PanY·Swipe·Overlay·Mode)의
되돌려 쓰기는 **재지 않았습니다** — §5 에 미검증으로 적습니다.

## 2. 원본 → 창 방향 — **관측점이 없습니다**

§4.3 의 본체인 **영상이 따라 그려지는가**는 이 하네스로 볼 수 없습니다. 두 도구를 다 시도했고
둘 다 막혔습니다.

### (a) UIA — 분리 창이 내놓는 요소가 7개뿐입니다

```
detached descendants=7
  [TitleBar] [MenuBar '시스템'] [MenuItem '시스템'] [Button 최소화] [Button 최대화] [Button 닫기]
  [Text] 'Mode=SwipeVertical, Zoom=120%, Pan=(0,0), Swipe=50%, Overlay=50%'
```

여섯은 타이틀바 크롬이고, **뷰포트는 없습니다** — `ImageComparisonViewport : FrameworkElement`
이고 automation peer 가 없습니다. 남은 하나는 **상태 줄**인데, 이것은 **뷰모델**에 바인딩돼
있어 **뷰포트가 따라가든 말든 같은 값을 읽습니다.**

### (b) 픽셀 캡처 — **대조군이 눈멀었다고 말했습니다**

분리 창을 캡처하면 **하얗게** 나오고, F8/F5/줌 어느 것에도 **해시가 1비트도 안 움직입니다.**
그대로 적었으면 "분리 창이 아무것도 안 그린다" 가 됐을 것입니다.

**대조군: 같은 방법으로 메인 창을 찍었더니 그것도 하얗습니다**(`main-control.png`). 메인 창은
UIA 로 `fit`·`85%` 를 읽을 수 있는, 분명히 살아 있는 창입니다. **따라서 이 캡처는 이 앱의 WPF
내용을 볼 수 없는 도구이고, 하얀 분리 창 사진은 분리 창에 대해 아무 말도 하지 않습니다.**

**C-71 에서 한 번, 여기서 두 번째입니다** — 도구가 못 보는 것을 부재로 읽을 뻔했습니다.

### 그래서 단언한 것은 **창의 내용이 공유 상태를 따라간다**까지입니다

```
W16 afterF8='Mode=DifferenceHeatmap, Zoom=Fit, …'
    afterF5='Mode=SwipeVertical,    Zoom=Fit, …'
```

메인 창에서 모드를 바꾸면 **분리 창의 상태 줄이 따라갑니다.** 이것은 같은 메서드가 만든
바인딩이 살아 있다는 실행 증거이지만, **뷰포트의 `CompareMode` 가 따라간다는 증거는
아닙니다.** 시나리오 주석에 그대로 적었습니다.

## 3. 할 일 2 — CI, 이제 **측정**입니다

**런 35076029873**(`XPE CI Pipeline`, main `64dd8551` = C-71 병합, conclusion success).
아티팩트 `xpe-gui-e2e-smoke-results` / `xpe-gui-e2e-native-results`.

| 스위트 | **Mock** | Native | 게이트 | 소진(Mock) |
|---|---|---|---|---|
| **Smoke** | **26.4 s** (22건) | **98.1 s** (22건) | 30 s | **88 %** |
| Workflow | 28.8 s (25건) | 32.1 s (25건) | 180 s | 16 % |
| Rendering | 0.6 s | 1.1 s | — | — |

- **Mock 이 정본입니다**(§5.3). **26.4 s, 넘지 않았습니다.** C-71 의 추정 27.5 s 와 1.1 s 차이.
- **S11 은 CI 에서 1.91 s(Mock) · 1.72 s(Native)** 입니다.
- **Native 98.1 s 는 C-68 §4 가 예고한 그것입니다.** `ReadableWindow_IsKept_AndLeavesNoNote` 한
  건이 **72.37 s**(같은 테스트가 Mock 에서는 **0.53 s**) — XCal 교정 생성이 그 픽스처에 귀속된
  실행입니다. **그 한 건을 빼면 Native Smoke 는 25.7 s** 입니다.
  **로컬에서 본 38 s 보다 큽니다**(CI 기계가 느립니다).
- **이 카드는 Smoke 에 아무것도 넣지 않았습니다**(§4). 로컬 Smoke 25.2 s 로 변화 없습니다.

**덧 — main 에 빨간 워크플로가 하나 있습니다.** 같은 커밋 `64dd8551` 에서
**`Documentation Generation` / `Doxygen Native API Reference` 가 failure** 입니다(런
35076029851). **제 소유가 아니고 이 카드와 무관하지만**, "읽는 사람 없는 빨간불" 선례가 있어
적습니다.

## 4. 새 시나리오 2건 — **Workflow 에 넣었습니다**

`clients/ImageProcTest.E2ETests/Scenarios/Workflows/DetachedViewerSyncScenarios.cs`

| 시나리오 | 주장 | 로컬 |
|---|---|---|
| **W-15** | 분리 창에서 휠을 굴리면 **메인 창의 줌 표시가 움직인다**(대조군 포함) | 7.56 s |
| **W-16** | 메인 창에서 모드를 바꾸면 **분리 창의 상태 줄이 따라간다** | 3.95 s |

**성격으로 Workflow 입니다**(여유가 아니라, C-69 판정대로): 둘 다 **영상이 로드된 픽스처**와
**두 번째 창**, **창 이동**, **휠 제스처**가 필요합니다. §4.1 의 케이스들은 하나를 눌러 하나를
읽습니다. 로컬 Workflow 24.3 s → **35.7 s (27건)**, 180 s 의 **20 %**.

**트레이트는 붙이지 않았습니다**(C-69 판정 1).

### 반증 — 방향이 갈립니다

| 주입 | 결과 |
|---|---|
| `ZoomScaleProperty` 바인딩 제거 | **W-15 만 실패** (W-16 통과) |
| 상태 줄의 `SetBinding` 제거 | **W-16 만 실패** (W-15 통과) |

## 5. 할 일 3 — C-46 인용 census

**찾은 범위**: `xpe-gui` 의 `.moai/` · `docs/` · `gui/` · `clients/` (`*.md` `*.cs` `*.xaml`) 와
`image-processing` 의 `.moai/lanes/` · `.moai/reports/` · `docs/` (`*.md`). 검색어는
`never appeared` · `끝내 나타나지` · `나타나지 않` · `Program Manager` · `최상위 창` · `Detach`.

| 자리 | 문구 | 소유 | 상태 |
|---|---|---|---|
| `xpe-gui/.moai/reports/lane-gui/GUI-C-46/report.md:64-67` | "최상위 창을 훑었다 … 작업 표시줄 \| ImageProcTest GUI-S0 \| Program Manager" | 레인(기록) | **원본** |
| `clients/…/Workflows/ComparisonModeScenarios.cs:29-31` | "the window never appeared … the only top-level windows were …" | 레인(코드) | **이번에 고쳤습니다** |
| `xpe-gui/.moai/reports/lane-gui/GUI-C-66/report.md:72` | "C-46 이 호출 뒤 새 창이 끝내 나타나지 않았고, 최상위 창은 …" | 레인(기록) | 그대로 |
| `xpe-gui/.moai/reports/lane-gui/GUI-C-70/report.md:36` | "결론(관측): 새 창은 나타나지 않습니다" | 레인(기록) | 그대로 |
| `image-processing/.moai/lanes/gui/inbox/GUI-C-47.md:12,38` | "Detach 창은 안 열림" · "Detach 창이 안 열리는 원인 규명" | **리더** | — |
| `…/GUI-C-67.md:52` | "C-46 의 '새 창이 끝내 안 나타남'" | **리더** | — |
| `…/GUI-C-70.md:1,27` | 제목 + "C-46 이 관측한 것: …" | **리더** | — |
| `…/GUI-C-71.md:7` | "Detach 가 아무 일도 안 한다" | **리더** | — |

**고친 것은 코드 주석 하나입니다.** 살아 있는 문서라 틀린 채로 두면 다음 사람을 오도합니다.
**보고서 넷은 기록이라 고치지 않았습니다** — 정정은 C-71 §3 과 이 보고서에 남습니다.

### 같은 도구를 쓴 다른 결론 하나

`GUI-C-56/report.md:28` 이 **"창이 둘이다" 가설을 "프로세스 최상위 창 1개"** 로 배제했습니다.
**같은 눈먼 도구**입니다 — 두 번째 창이 소유 창이면 그 수에 안 잡힙니다. **다만 그 결론에는
독립적인 다리가 있습니다**(`launched == reacquired` 의 HWND 동일). **그래서 결론은 서지만,
그 한 다리는 근거가 아니었습니다.**

## 6. 요구 쪽에서 하나 더 — `COMPARE-001`

census 중에 나왔습니다. **`MENU-001` §4.3 보다 구체적인 요구가 있습니다.**

```
docs/project/XPE-GUI-COMPARE-001_…_Spec.md
  167: GUI-CMP-FR-004  The viewer shall support a detached window that reuses the same
                       comparison state model. | Detach does not fork processing state or
                       create an unsynchronized viewer.
  239: GUI-CMP-VER-005 Detach viewer and verify same state model.
                       | Detached-window automation or manual UAT checklist.
  277: - Detached viewer 상태 모델 재사용 완료
```

**W-15·W-16 이 `GUI-CMP-VER-005` 의 "Detached-window automation" 입니다.** 그리고 **277행이
"완료" 라고 적는데, C-70 시점에는 창이 아예 열리지 않았습니다** — 문서는 main 소유라 읽기만
했습니다. **판단은 리더 몫입니다.**

## 7. 실측 (verbatim)

```
dotnet test …E2ETests… --no-build (Mock)   통과! 실패 0, 통과 71,  건너뜀 1, 전체 72 (1 m 2 s)
dotnet test …IntegrationTests… --no-build  통과! 실패 0, 통과 200, 건너뜀 1, 전체 201
dotnet build clients/ImageProcTest.slnx    경고 0개 / 오류 0개

W15 control  fit -> fit  at {X=1864,Y=504}
W15 detached fit -> 85%                       W15: 7555 ms
W16 afterF8='Mode=DifferenceHeatmap, …'  afterF5='Mode=SwipeVertical, …'   W16: 3952 ms

[폐기한 판] 분리 창 중앙(메인과 겹침)에서 굴림 → 대조군도 fit -> 120%
[폐기한 판] 픽셀 해시: detached 81459214 (open/F8/F5/scroll 전부 동일) — 메인도 하얗게 나옴

[반증 A] ZoomScale 바인딩 제거  → 실패! 실패 1, 통과 1 (W15 만)
[반증 B] 상태 줄 SetBinding 제거 → 실패! 실패 1, 통과 1 (W16 만)

CI 런 35076029873 (XPE CI Pipeline, main 64dd8551, success)
  e2e-smoke.trx        Smoke 26.4 (22) · Workflow 28.8 (25) · Rendering 0.6 (23)
  e2e-smoke-native.trx Smoke 98.1 (22) · Workflow 32.1 (25) · Rendering 1.1 (23)
  ReadableWindow_IsKept_AndLeavesNoNote  Mock 00:00:00.526  /  Native 00:01:12.365
  S11_DetachComparisonViewer_OpensTheWindow  Mock 1.912  /  Native 1.720

build/e2e-c72/c72.trx  Smoke 25.2 (22) · Workflow 35.7 (27) · Rendering 0.6 (23)
```

**`SourceImage` setter 는 열지 않았습니다. 고치지 않았습니다** — 이 카드는 재고 보고하는
카드이고, 되돌려 쓰기가 살아 있으므로 `TwoWay` 를 바꿀 이유도 없었습니다.

## 8. 미검증 (Gaps)

- **원본 → 창(영상) 방향은 재지 못했습니다**(§2). 관측점이 없습니다.
- **되돌려 쓰기는 `ZoomScale` 하나만 쟀습니다.** PanX·PanY·Swipe·Overlay·Mode 는 **바인딩이
  거부되지 않는다까지**입니다 — C-71 과 같은 자리에 남아 있습니다.
- **W-16 은 뷰포트가 아니라 상태 줄을 봅니다**(§2). 뷰포트의 `CompareMode` 는 미관측입니다.
- **사람이 마우스로 누르는 경로는 여전히 도구가 없습니다.**
- **Native 에서 W-15·W-16 을 돌리지 않았습니다** — 이번 로컬 배치는 Mock 입니다.
- **CI 는 한 런입니다**(35076029873). 편차는 모릅니다.
- **`COMPARE-001` 277행의 "완료" 주장이 언제 적혔는지** 이력을 보지 않았습니다.
- **`GUI-C-56` 의 배제를 다시 재지 않았습니다**(§5) — 독립적인 다리가 있다는 것만 읽었습니다.
- **Doxygen 잡 실패의 내용은 보지 않았습니다** — 실패했다는 사실만 읽었습니다.

## 9. 잔여 위험 (Residual risk)

- **분리 창이 실제로 영상을 그리는지 아무도 모릅니다.** 창이 열리고 상태가 흐르는 것까지가
  확인된 전부입니다. **`GUI-CMP-FR-004` 의 "same state model" 은 이 두 시나리오가 닿는 만큼만
  검증됩니다.**
- **Native Smoke 가 98.1 s** 입니다. 게이트가 Mock 에 걸려 있어 지금은 문제가 아니지만,
  **누가 Native 를 같은 게이트로 읽으면 3배 초과로 보입니다.**
- **W-15 는 창을 화면 밖으로 옮깁니다**(`main.Right + 20`). 메인 창이 화면 오른쪽 끝에 있으면
  그 점이 화면 밖이 되어 제스처가 어디에도 닿지 않습니다 — **이번 CI 해상도에서는 들어왔지만
  좁은 화면에서는 위치 단언이 먼저 실패합니다**(그렇게 되도록 단언을 넣어 뒀습니다).
- **분리 창을 닫지 못하면 다음 시나리오가 흔들립니다** — 둘 다 시작할 때 닫고 시작합니다.

## 부록 — 사용한 명령

```bash
gh run list --branch main --limit 5 --json databaseId,workflowName,conclusion,headSha
gh run download 35076029873 -n xpe-gui-e2e-smoke-results -n xpe-gui-e2e-native-results
grep -rn -i "never appeared|끝내 나타나지|나타나지 않|Program Manager|최상위 창|Detach" \
  --include=*.md --include=*.cs --include=*.xaml   (xpe-gui 와 image-processing 양쪽)
dotnet test …E2ETests… --filter "FullyQualifiedName~DetachedViewerSyncScenarios" \
  --logger "console;verbosity=detailed"
```
