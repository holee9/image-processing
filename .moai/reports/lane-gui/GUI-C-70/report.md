# GUI-C-70 — 명령은 실행되고, 첫 줄에서 예외로 끝납니다 (#136 계열)

- 카드: GUI-C-70 · Refs #136 #165 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- **커밋 없음** — 조사 카드입니다. 프로브는 만들었다 지웠고, 앱 계측도 원복했습니다(작업 트리 clean).
- **가름: "아무것도 없는 것" 이 아니라 "시도가 있는데 실패하는 것" 입니다.** 그리고 **실패가 완전히 조용합니다.**
- **원인까지 나왔습니다**: `BindDetachedViewport` 가 여덟 바인딩을 전부 `TwoWay` 로 걸고,
  첫 번째 대상 `SourceImage` 가 **setter 가 private** 이라 그 한 줄에서 예외가 납니다.
- **결과: Mock E2E 0/68/1/69 · 통합 0/198/1/199 · slnx 0경고 0오류**

---

## 1. 다시 관측했습니다 — C-46 과 같은 결과

C-46 이후 여러 카드가 지났으므로 **현재 빌드에서 다시 쟀습니다.** 임시 프로브
(`Scenarios/Smoke/DetachProbe.cs`, 관측 후 삭제)로 Mock 앱에서:

```
statusBefore='Backend initialized: v0.0.0-mock'
[before] processWindows=1   name='ImageProcTest GUI-S0'
controlItem found=True
statusAfterControlClick='Layout reset.'          ← 대조군: 같은 메뉴·같은 경로가 명령을 실행한다
menuItem found=True enabled=True
[after-click] processWindows=1   name='ImageProcTest GUI-S0'
menuReopens=False
statusAfterDetachClick='Layout reset.'           ← 바뀌지 않음
desktopDialogs=0                                 ← #32770 대화상자 0개
desktopByTitle=0                                 ← 'ImageProcTest Comparison Viewer' 0개
```

**대조군을 먼저 넣었습니다.** 같은 View 메뉴의 `ResetLayoutMenuItem` 을 같은 UIA Invoke 로
눌러 상태 표시줄이 `Layout reset.` 으로 바뀌는 것을 확인한 뒤에 Detach 를 눌렀습니다.
**대조군 없이 "아무 일도 없었다" 는 "클릭이 명령에 닿지 않았다" 와 구별되지 않습니다** — 실제로
마우스 `Click()` 경로로 먼저 시도했을 때는 **대조군도 아무 일을 안 했고**, 그 판의 Detach 결과는
전부 폐기했습니다.

**결론(관측)**: 새 창은 나타나지 않습니다. **상태 표시줄도, 대화상자도, 로그도 없습니다.**

## 2. 코드가 무엇을 하는가 — **시도가 있습니다**

`MainWindowViewModel.cs:989` `OpenDetachedComparisonViewer()` 는 뷰포트·상태 줄·Grid·`Window` 를
만들고 `window.Show()` 까지 호출하는 **완전한 구현**입니다. 명령도 배선돼 있습니다
(`:93` 생성, `:215` 공개, `MainWindow.xaml:289` 바인딩).

**따라서 #165 의 죽은 토글(뒤가 없는 것)과는 다른 자리입니다.**

## 3. 원인 — 첫 번째 바인딩에서 던집니다

앱에 임시 계측(진입 표시 + `try/catch` + 상태 표시줄 출력)을 넣고 같은 프로브를 돌렸습니다.

```
statusAfterInvoke='PROBE C-70: threw InvalidOperationException:
  TwoWay 또는 OneWayToSource 바인딩은 'ImageProcTest.ViewModels.MainWindowViewModel' 형식의
  읽기 전용 속성 'SourceImage'에서 작동하지 않습니다.'
```

**두 가지가 동시에 확정됩니다**: ① **명령은 실행됩니다**(진입 후 예외), ② **그 예외는
`BindDetachedViewport` 의 첫 호출**에서 납니다.

```csharp
// :1043 — 여덟 호출 전부 이 모드를 씁니다
BindingOperations.SetBinding(target, property, new DataBinding(path) { Source = this, Mode = BindingMode.TwoWay, … });

// :997 첫 호출 대상
public System.Windows.Media.ImageSource? SourceImage { get => _sourceImage; private set => …; }   // :298
```

`SourceImage` 와 `ProcessedImage` 는 **setter 가 private** 이라 바인딩에서 읽기 전용입니다.
`TwoWay` 는 거기서 성립할 수 없고, `SetBinding` 이 그 자리에서 던집니다. **`Window` 는 그 뒤에
만들어지므로 아예 생성되지 않습니다.** 나머지 여섯 경로(모드·줌·팬·스와이프·오버레이)는
`Settings.*` 의 공개 setter라 문제가 없습니다.

**계측은 전부 원복했습니다**(`git diff` 비어 있음).

## 4. 거짓 신호는 없습니다 — 다만 **조용합니다**

성공 문구 `RefreshComparisonStatus("Detached comparison viewer opened.")` 는 `:1040`,
**`window.Show()` 바로 뒤**에 있습니다. 예외가 `:997` 에서 나므로 **그 줄에는 도달하지 않습니다.**

**그래서 C-67 census 가 놓친 거짓 문구는 아닙니다** — 그 문구는 창이 실제로 떴을 때만 나옵니다.
**대신 사용자에게 아무 신호도 가지 않습니다.** 상태 표시줄은 직전 값 그대로이고, 로그에도 아무
줄이 붙지 않으며, 대화상자도 없습니다. **눌렀는지조차 알 수 없는 형태**입니다.

`App.xaml.cs:60` 에는 `DispatcherUnhandledException` 핸들러가 있어 **MessageBox 를 띄우고
`Handled = true`** 로 삼킵니다. 그런데 **이번 관측에서는 그 대화상자가 나오지 않았습니다**
(`desktopDialogs=0`).

- **가설**: UIA provider 호출 안에서 난 예외는 그 경로로 가지 않는다.
- **미검증입니다.** 사람이 실제 마우스로 누르는 경로는 **재지 못했습니다** — §1 에 적은 대로
  FlaUI 의 `Click()` 은 이 메뉴에서 대조군조차 실행시키지 못했고, 그 경로로는 아무 결론도
  낼 수 없습니다. **사람이 누르면 오류 대화상자를 볼 수도 있습니다.**

## 5. 요구에 있는가 — **있습니다**

| 자리 | 내용 |
|---|---|
| `MENU-001` **§4.3 View**, 121행 | `- Detach Viewer` — "Initial and planned commands" 목록 |
| `MENU-001` §4.3 Rules | "Comparison commands … **must not create unsynchronized source/processed image windows by default**" |
| `docs/design/README.md:131` | `Detached comparison viewer \| DetachComparisonViewerCommand \| **Keep.**` |

**§9.2(패널 가시성)·§9.3(비교 모드)·§10(단축키)에는 없습니다.** 단계 지정도 §5 에 없습니다.

**찾은 범위**: `gui/` · `clients/` · `docs/` 아래 `*.cs` · `*.xaml` · `*.md` 전체에서 대소문자
무시 `detach` 검색. 위 세 자리 외에는 `gui/ImageProcTest/README.md`(기능 소개 한 줄)과
`docs/design/reference/` 의 사본들뿐입니다.

**따라서 #165 에서 제거한 것들과 같은 자리가 아닙니다** — 요구가 있고, 구현도 있고, **한 줄이
잘못돼 있습니다.** §4.3 의 "동기화되지 않은 창을 만들지 말 것" 규칙도 이 구현의 의도와 맞습니다
(양방향 바인딩으로 상태를 공유하려던 것이 바로 그 규칙을 지키려는 설계입니다).

## 6. 실측 (verbatim)

```
# 프로브 (임시, 삭제함) — Mock, ApplicationCollection
statusBefore='Backend initialized: v0.0.0-mock'
controlItem found=True   statusAfterControlClick='Layout reset.'
menuItem found=True enabled=True
[after-click] processWindows=1  name='ImageProcTest GUI-S0' class='Window' offscreen=False
menuReopens=False   statusAfterDetachClick='Layout reset.'
desktopDialogs=0    desktopByTitle=0

# 마우스 Click() 경로 — 대조군도 실행되지 않아 폐기한 판
controlItem found=True   statusAfterControlClick='Backend initialized: v0.0.0-mock'   ← 안 바뀜

# 계측판 (원복함)
statusAfterInvoke='PROBE C-70: threw InvalidOperationException: TwoWay 또는 OneWayToSource
  바인딩은 …형식의 읽기 전용 속성 'SourceImage'에서 작동하지 않습니다.'

# 정리 후
git status --short → .claude/settings.json.bak-* 2건뿐 (lead 소유, 손대지 않음)
git diff --stat    → (비어 있음)
dotnet build clients/ImageProcTest.slnx -c Debug   경고 0개 / 오류 0개
dotnet test …E2ETests… --no-build (Mock)   통과! 실패 0, 통과 68,  건너뜀 1, 전체 69 (48 s)
dotnet test …IntegrationTests… --no-build  통과! 실패 0, 통과 198, 건너뜀 1, 전체 199
```

**시나리오를 늘리지 않았습니다**(카드 지시). E2E 69건 그대로이고, **Smoke 시간은 변하지
않았습니다.**

## 7. 미검증 (Gaps)

- **사람이 마우스로 누르는 경로를 재지 못했습니다.** `Click()` 은 이 메뉴에서 대조군조차
  실행시키지 못했고(§1), 그래서 **"오류 대화상자가 안 뜬다" 는 UIA Invoke 경로에 한한
  관측**입니다. 실제 사용자에게는 MessageBox 가 보일 수 있습니다.
- **Native 백엔드에서는 재지 않았습니다.** 이 결함은 백엔드와 무관해 보이지만(바인딩 모드),
  확인하지 않았습니다.
- **이미지가 로드된 상태에서는 재지 않았습니다.** 예외는 이미지 유무와 무관한 자리에서
  나지만, 로드 후에 같은지는 실행으로 보지 않았습니다.
- **`ProcessedImage` 도 같은 이유로 던지는지**는 확인하지 않았습니다 — 첫 줄에서 끝나므로
  **두 번째 줄은 한 번도 실행된 적이 없습니다.** 선언만 보고 적었습니다.
- **`menuReopens=False`** 를 관측했지만 **원인은 재지 않았습니다**(C-58·C-61 의 메뉴 뒤끝과
  같은 것인지, 실패한 명령 때문인지 구분 못 함).
- **이 결함이 언제 들어왔는지** 이력을 보지 않았습니다.
- **고치지 않았습니다**(카드 지시).

## 8. 잔여 위험 (Residual risk)

- **조용한 실패입니다.** 사용자가 눌러도 아무 표시가 없으므로, 고치기 전까지는 "메뉴가
  반응하지 않는다" 로만 보입니다.
- **같은 형태가 다른 곳에도 있을 수 있습니다** — `BindDetachedViewport` 처럼 **대상의 쓰기
  가능 여부를 보지 않고 `TwoWay` 를 일괄로 거는** 자리. 이번 카드에서 세지 않았습니다.
- **E2E 가 이 명령을 누르는 시나리오는 없습니다.** 지금 고쳐도 회귀를 잡아 줄 단언이
  없습니다 — 구현 카드에서 어느 스위트에 넣을지 정해야 하고, **Mock Smoke 가 85 %** 입니다.

## 부록 — 사용한 명령

```bash
grep -rn -i "detach" gui clients docs --include=*.cs --include=*.xaml --include=*.md
sed -n '989,1056p' gui/ImageProcTest/ViewModels/MainWindowViewModel.cs
sed -n '105,127p;268,293p' docs/project/XPE-GUI-MENU-001_Menu_and_Command_Strategy.md
dotnet test …E2ETests… --filter "FullyQualifiedName~DetachProbe" --logger "console;verbosity=detailed"
```
