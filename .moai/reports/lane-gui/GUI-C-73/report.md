# GUI-C-73 — 창 위치는 아무도 정하지 않습니다. 틀린 것은 제 전제였습니다 (#166 후속)

- 카드: GUI-C-73 · Refs #166 #136 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 1건 — 미푸시. **제품 코드는 손대지 않았습니다**(`git diff gui/` 비어 있음).
- **판정: (b) 테스트가 틀렸습니다.** (a) 는 배제했습니다 — **배치 코드가 아예 없습니다**(§1).
- **귀속 가드는 약화시키지 않았습니다. 더 직접적인 것으로 바꿨습니다**(§2).
- **CI 기하를 로컬에서 재현해 고친 단언이 통과하는 것을 확인했습니다**(§3).
- **결과: Mock 0/71/1/72 · Native 0/71/1/72 · 통합 0/200/1/201 · slnx 0경고 0오류**

---

## 1. (a) 배제 — 분리 창의 위치를 정하는 코드가 **없습니다**

`OpenDetachedComparisonViewerCore()` 가 `Window` 에 주는 것은 여섯 가지뿐입니다.

```csharp
new System.Windows.Window { Title, Width = 1280, Height = 820, MinWidth = 900, MinHeight = 620, Content = grid }
// 그리고 Owner. 그게 전부입니다.
```

```
grep -rn "WindowStartupLocation|\.Left =|\.Top =|SystemParameters" gui/ImageProcTest --include=*.cs --include=*.xaml
  → 0건 (obj 제외)
```

**`WindowStartupLocation` 도, `Left`/`Top` 도, 화면 경계를 읽는 코드도 없습니다.** 창은 OS 가
놓는 자리에 열립니다. 요구 쪽도 봤습니다 — `MENU-001` §4.3(`Detach Viewer`)과
`COMPARE-001`(`GUI-CMP-FR-004`, 36행의 "full-screen or multi-monitor review")은 **위치를 말하지
않습니다.**

**찾은 범위**: `gui/ImageProcTest/**` 의 `*.cs`·`*.xaml` 전체(위 grep), 그리고
`docs/project/XPE-GUI-MENU-001_*.md` · `XPE-GUI-COMPARE-001_*.md` 에서
`monitor`·`position`·`배치` 검색.

**관측된 위치는 실행마다 다릅니다** — 로컬에서 `{26,26}` · `{208,208}` · `{286,286}` ·
`{338,338}` (OS 기본 계단 배치), CI 에서 **`{0,0}`**. 크기도 다릅니다: CI 에서 분리 창
**1044×788**, 주 창 **1280×788** — 1280×820 으로 만든 창이 그 크기로 나왔다는 것은 **CI 화면이
작아 클램프됐다**는 뜻입니다.

**따라서 `{0,0}` 은 결함이 아니라 환경입니다.** 제품을 고칠 자리가 없습니다.

## 2. 고친 것 — 기하 대신 **적중 판정**

휠은 **커서 아래 픽셀을 소유한 창**으로 갑니다. 그러니 "분리 뷰어가 받았나" 는 **그 픽셀에
대한 질문**입니다. 옛 판은 그 질문에 **기하**로 답했습니다 — 분리 창을 주 창 옆으로 옮기고
제스처 점이 주 창 사각형 **밖**일 것을 요구했습니다. **그 전제는 테스트가 스스로 지어낸
것**이고(§1), CI 에서 창이 `{0,0}` 에 열리고 화면 밖으로의 이동이 먹지 않자 **귀속 불가로
거부**했습니다 — 옳은 거부였지만, 애초에 성립할 수 없는 전제였습니다.

이제 트리에 묻습니다.

```csharp
// FromPoint 가 그 픽셀의 요소를 주고, 그 요소나 조상 중에 분리 창이 있으면 그 창의 것이다.
private bool OwnsPixel(Point point, AutomationElement detached)
```

**측정으로 확인한 사실 하나**: `FromPoint(분리창 중앙)` 은 **분리 창 요소를 직접** 돌려줍니다
(hwnd 일치). 그리고 그 위로 올라가면 **주 창**이 나옵니다 — C-71 이 관측한 "소유 창은 소유자의
자손" 이 그대로 보입니다. 그래서 **최상위까지 올라가면 안 되고**(그러면 항상 주 창),
**분리 창을 만나면 멈춰야** 합니다. 첫 시도가 이 함정에 빠져 `owner='ImageProcTest GUI-S0'` 로
실패했고, 그 실패가 이 구조를 알려 줬습니다.

### 대조군 — 프로브가 "예" 만 말하지 않는다는 것

같은 프로브를 **분리 창이 덮지 않는 픽셀**에 대고 "아니오" 가 나오는지 봅니다. 그 픽셀은
**주 창 안쪽에서 고릅니다**(두 창이 서로 주장할 만한 자리라야 대조가 날카롭습니다) — 주 창의
네 모서리 안쪽 40 px 중 분리 창 밖인 첫 점. 없으면 분리 창 가장자리 바로 밖으로 물러섭니다.
**그 픽셀에 아무것도 없으면(`<none>`) 대조군이 공허**하므로 그것도 거부합니다.

**게이트를 약하게 만들지 않았습니다** — 옛 판은 "겹치지 않는다" 라는 **대리 지표**였고,
지금은 **휠이 따르는 바로 그 적중 판정**을 직접 읽습니다.

## 3. CI 기하 재현 — 고친 단언이 그 조건에서 통과합니다

로컬에서는 창이 `{0,0}` 에 열리지 않으므로, **일부러 옮겨 CI 조건을 만들었습니다**(임시 한 줄,
제거함).

```
W15 detachedRect={X=0,Y=0,Width=1280,Height=820} point={X=640,Y=410} mainRect={X=182,Y=182,...}
W15 control point={X=1702,Y=222} name=''          ← 주 창 안, 분리 창 밖 → 프로브 "아니오"
W15 zoom fit -> 85%                                ← 통과
```

**옛 판이 실패한 바로 그 배치에서 새 판이 통과합니다.**

## 4. 반증 (카드 할 일 3)

`ZoomScaleProperty` 바인딩을 끊고 돌렸습니다.

```
W15 zoom fit -> fit
실패 W15_ScrollingTheDetachedViewer_MovesTheMainWindowsZoom
통과 W16_ChangingTheModeInTheMainWindow_ReachesTheDetachedWindow
```

**값 단언에서 실패했습니다 — 귀속 가드가 아니라.** 단언이 눈멀지 않았습니다.

## 5. Mock · Native 양쪽 (카드 할 일 4)

| | Mock (`dotnet test`) | Native (`Repeat-E2E.ps1 -Backend Native`) |
|---|---|---|
| 결과 | **0 실패 / 71 통과 / 1 건너뜀 / 72** | **0 실패 / 71 통과 / 1 건너뜀 / 72** (103.9 s) |
| Smoke | 23.9 s (22건) | 24.9 s (22건) |
| Workflow | 33.9 s (27건) | 36.9 s (27건) |
| Rendering | 0.6 s | 0.6 s |
| W15 / W16 | 5.55 s / 3.69 s | 5.52 s / 3.62 s |

## 6. 건너뛴 케이스가 무엇인지 (카드 할 일 6)

**두 종류가 있고, 서로 다른 이유입니다.**

| 케이스 | 사유(테스트가 적은 그대로) | 성격 |
|---|---|---|
| `NativeRun_NamesTheBinariesItExercises` | "XPE_E2E_BACKEND is not 'Native' — this run stages no native binaries." | **설계대로** — Mock 실행에서만 건너뜁니다. **CI Mock 의 `Skipped 1` 이 이것입니다.** |
| `ReadableWindow_IsKept_AndLeavesNoNote` | "Not measured: the real re-acquire fired on this launch — The launched window did not support AutomationId (framework=Win32); re-located it by process id …" | **환경 의존** — #30011 재획득 결함이 그 기동에서 터지면 건너뜁니다. |

- **Mock 로컬 첫 배치는 2건**이었습니다(위 둘 다). 두 번째 배치와 CI Mock 은 1건(첫 줄)뿐 —
  차이는 재획득 결함이 그 기동에서 터졌는지입니다.
- **Native 로컬 이번 판은 두 번째 것 1건**을 건너뛰었습니다. **CI Native 는 0건**이었습니다.
- **주의할 점 하나**: `ci.yml` 의 `gui-e2e-native` 에는 **"건너뛴 케이스가 있으면 잡을 실패시킨다"**
  스텝이 있습니다. 즉 **재획득 결함이 CI Native 기동에서 터지면 그 자체로 빨간불**이 됩니다.
  이번 카드와 무관한 기존 조건이지만, 로컬에서 실제로 터지는 것을 봤으므로 적습니다.

## 7. 실측 (verbatim)

```
grep -rn "WindowStartupLocation|\.Left =|\.Top =|SystemParameters" gui/ImageProcTest → 0건

[첫 시도 — 최상위까지 올라간 판]
W15 rect={X=208,Y=208,...} point={X=848,Y=618} owner='ImageProcTest GUI-S0'   ← 실패
PROBE[0] type=Window name='ImageProcTest Comparison Viewer' hwnd=0x6703B0
PROBE[1] type=Window name='ImageProcTest GUI-S0'            hwnd=0x80803C2
PROBE[2] type=Pane   name='데스크톱 2'
detached hwnd=0x6703B0

[고친 판 — 평소 기하]
W15 detachedRect={X=338,Y=338,Width=1280,Height=820} point={X=978,Y=748}
W15 control point={X=1702,Y=222} name=''      W15 zoom fit -> 85%

[고친 판 — CI 기하 재현(임시로 {0,0} 이동, 제거함)]
W15 detachedRect={X=0,Y=0,Width=1280,Height=820} point={X=640,Y=410}   zoom fit -> 85%

[반증] ZoomScale 바인딩 제거 → W15 만 실패 ("fit -> fit"), W16 통과

dotnet build clients/ImageProcTest.slnx -c Debug     경고 0개 / 오류 0개
dotnet test …E2ETests… --no-build (Mock)   통과! 실패 0, 통과 71,  건너뜀 1, 전체 72 (1 m)
Repeat-E2E.ps1 -Times 1 -Backend Native    run 1 103.9s pass · runs with a failure: 0 / 1
dotnet test …IntegrationTests… --no-build  통과! 실패 0, 통과 200, 건너뜀 1, 전체 201
git diff --stat gui/                        (비어 있음)
```

## 8. 미검증 (Gaps)

- **CI 에서 돌려 보지 않았습니다.** `{0,0}` 재현은 **로컬에서 창을 옮겨 만든 것**이고, CI 의
  화면 크기·DPI·클램프(분리 창이 1044×788 로 줄어든 것)까지 재현하지는 못했습니다.
- **`APixelOutside` 의 네 후보가 모두 분리 창 안에 드는 경우**(분리 창이 주 창을 완전히 덮는
  경우)는 실행으로 보지 못했습니다 — 가장자리 밖으로 물러서는 대체 경로는 코드로만 있습니다.
- **되돌려 쓰기는 여전히 `ZoomScale` 하나만** 실행으로 확인했습니다(C-72 와 같은 자리).
- **원본 → 창(영상) 방향은 여전히 관측점이 없습니다**(C-72 §2).
- **`ReadableWindow_…` 가 CI Native 기동에서 터질 빈도**는 재지 않았습니다 — 로컬 두 배치에서
  각각 한 번씩 봤을 뿐입니다.
- **Native 는 1회만** 돌렸습니다(로컬). 편차는 모릅니다.
- **`FromPoint` 가 커서를 옮기기 전의 좌표를 쓴다**는 점 — 프로브와 실제 휠 사이에 창이 움직이면
  어긋납니다. 그 창은 이 시나리오 안에서 아무도 옮기지 않지만, 보장은 아닙니다.

## 9. 잔여 위험 (Residual risk)

- **분리 창의 위치는 앞으로도 환경마다 다릅니다.** 이 시나리오는 이제 위치를 전제하지 않지만,
  **위치를 전제하는 다른 것을 나중에 누가 또 쓰면 같은 일이 납니다.**
- **`gui-e2e-native` 의 "건너뛰면 실패" 게이트와 #30011 재획득 결함이 겹치면 CI 가 빨개집니다**
  (§6). 이번 카드가 만든 조건은 아닙니다.
- **Workflow 가 33.9~36.9 s** 입니다(180 s 의 19~21 %). Smoke 는 23.9~24.9 s 로 이 카드가
  건드리지 않았습니다.

## 부록 — 사용한 명령

```bash
grep -rn "WindowStartupLocation\|\.Left =\|\.Top =\|SystemParameters" gui/ImageProcTest --include=*.cs --include=*.xaml
dotnet test …E2ETests… --filter "FullyQualifiedName~DetachedViewerSyncScenarios" --logger "console;verbosity=detailed"
powershell -File clients/ImageProcTest.E2ETests/Staging/Repeat-E2E.ps1 \
  -Times 1 -Backend Native -NativeDir build/ci-common/bin -ResultsDirectory build/e2e-c73n2
```
