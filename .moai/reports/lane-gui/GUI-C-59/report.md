# GUI-C-59 — 보류한 키, 7번째 모드, 접근성 대조 (#149)

> **이번 실행이 쓴 네이티브**: run **34535620953** · head `83ffa7ae…` ·
> `xpe_preprocess.dll` md5 `9f81d2f07fb7f7e23e90c86332467105`.

- 카드: GUI-C-59 · Refs #149 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 1건 — 미푸시
- **C-58 에서 제가 적은 진술 하나가 틀렸습니다 — `SwipeHorizontal` 은 문서에 있습니다**(§2).
- **결과: Native 3회 0 실패 · Mock 0/58/1/59 · 통합 0/180/1/181 · slnx 0경고 0오류**

---

## 1. 할 일 1 — `F6` 배선 완료

판정(`6dffce0`)대로 **F6 = Split** 을 배선했습니다. C-58 의 `SetComparisonModeCommand` 를 그대로
쓰고, 메뉴 항목에 `InputGestureText="F6"` 을 붙였습니다. `Ctrl+1`·`Ctrl+2` 는 배선하지
않았습니다.

**반증**: F6 바인딩만 약화(요소는 남기고 `Command` 제거)했더니 **F6 케이스 1건만 실패**하고
나머지 17건이 통과했습니다.

```
[cut=F6] 실패 W14_BoundKeyGesture_SelectsThatMode(gesture: "F6", mode: "SplitLocked")
[cut=F6] 실패!  - 실패: 1, 통과: 17, 전체: 18
```

키 배선 현황: **F5·F6·F7·F8 배선 · Ctrl+1·Ctrl+2 미배선**(판정에 따라).

## 2. 할 일 2 — 7번째 모드. 먼저 제 오류를 정정합니다

### 정정 — "문서에 없는 7번째 모드" 는 틀렸습니다

C-58 에서 저는 `SwipeHorizontal` 이 **"문서 §9.3 의 6개 목록에 없다"** 고 적었고, 그 말 자체는
맞습니다. 그런데 보고와 메시지에서 그것을 **"문서에 없는 7번째 모드"** 로 일반화했고, **그것은
틀렸습니다.**

**`XPE-GUI-COMPARE-001` §4(L61-L71)가 7개 모드를 전부 적고 있습니다.**

```
| `SwipeHorizontal` | Top of divider shows source, bottom shows processed
                    | Long anatomy or vertical artifact review |   ← COMPARE-001 L65
```

코드의 7개와 COMPARE-001 §4 의 7개는 **정확히 일치**합니다. MENU-001 §9.3 에 없는 이유는 그
표가 **모드 목록이 아니라 단축키 표**이기 때문입니다.

**그러니 실제 결과는 "요구가 없는 모드" 가 아니라 "진입 수단이 지정되지 않은 모드" 입니다.**
한 문서의 한 표에 없는 것을 "문서에 없다" 로 옮긴 것이고, C-45 가 `MainWindow.xaml` 만 보고
"기능이 없다" 고 한 것과 **같은 형태의 오류**입니다.

### 무엇을 하는가

`DrawHorizontalSwipe` 는 `DrawVerticalSwipe` 와 **분할선의 방향만** 다릅니다.

| | SwipeVertical | SwipeHorizontal |
|---|---|---|
| 분할선 | 세로 (x = SwipePosition × 폭) | 가로 (y = SwipePosition × 높이) |
| processed 가 보이는 쪽 | 분할선 **오른쪽** | 분할선 **아래쪽** |
| 드래그가 움직이는 축 | `point.X / ActualWidth` | `point.Y / ActualHeight` |
| `IsSwipeMode` | 참 | 참 (드래그로 분할선 이동 가능) |

COMPARE-001 의 설명("Top of divider shows source, bottom shows processed")과 코드가 일치합니다.

### 언제 들어왔는가

```
977df22  2026-04-16  [GUI] 대용량 비교 뷰어 및 wrist fixture 구현
```

`-S SwipeHorizontal` 로 추적한 결과 **커밋 1건**뿐입니다 — 나머지 6개 모드와 **같은 커밋에서
비교 뷰어 전체와 함께** 들어왔습니다. 나중에 슬쩍 추가된 것이 아닙니다. 그 커밋은 요구 문서
여러 개를 함께 고쳤습니다(RTM·SAD·SDD·SRS 등).

### 저장된 설정으로 들어갈 수 있는가 — **예. 실행으로 쟀습니다**

`AppSettings.ComparisonMode` 의 setter 는 **빈 문자열만** 막습니다:

```csharp
set => SetProperty(ref _comparisonMode, string.IsNullOrWhiteSpace(value) ? "SwipeVertical" : value);
```

`AppSettingsService` 가 `appsettings.json` 을 그대로 역직렬화하므로 파일의 값이 그대로 들어옵니다.
**추론에서 멈추지 않고 실제로 띄워 봤습니다**(임시 프로브, 커밋 전 삭제):

```
comparisonMode: "SwipeHorizontal"  →  PROBE at launch: viewport reports 'SwipeHorizontal'
                                      PROBE 'Swipe' button present: True
                                      PROBE after pressing Swipe: 'SwipeVertical'

comparisonMode: "NotAMode"         →  PROBE at launch: viewport reports 'NotAMode'
                                      PROBE after pressing Swipe: 'SwipeVertical'
```

- **들어갈 수 있습니다.** 저장된 설정이 진입 수단 없는 모드로 앱을 띄웁니다.
- **갇히지는 않습니다.** 버튼 4개(이제 메뉴 6개도)로 빠져나옵니다.
- **덤으로 나온 것: 아무 문자열이나 저장됩니다.** `NotAMode` 가 그대로 살아남고, 그때
  **세 개의 표시가 서로 다른 말을 합니다** — 뷰포트 HUD 는 `NormalizeMode` 를 거쳐
  `SwipeVertical`, `HelpText`(#149 G-6)는 `NotAMode`, 메뉴 체크는 **아무것도 켜지지 않습니다.**
  고치지 않았습니다(카드: 재서 보고만).

## 3. 할 일 3 — 접근성, 문서 대조와 XAML 계수만

### Alt 니모닉 — 새로 만든 것이 겹치지 않습니다

XAML 의 `Header` 에서 `_` 다음 글자를 기계적으로 세어 비교했습니다.

| 범위 | 니모닉 | 중복 |
|---|---|---|
| 최상위 메뉴 6개 | F · B · V · P · T · H | **0** |
| View 메뉴 직계 | L · A · R · F · 1 · I · O · V · D + **C**(새) | **0** |
| Compare Mode 하위 6개 | S · L · O · D · N · P | **0** |

새로 쓴 글자는 `C`(Compare Mode) 와 하위 6개이고, **어느 범위에서도 겹치지 않습니다.**
(하위 메뉴의 `L`·`O`·`D` 는 View 직계의 `L`·`O`·`D` 와 글자가 같지만 **범위가 달라** 충돌이
아닙니다 — WPF 니모닉은 열려 있는 메뉴 안에서만 경쟁합니다.)

### Tab 순서 — 이전부터 선언이 없습니다

```
grep -c TabIndex  →  MainWindow.xaml 0 · Views/*.xaml 전부 0
IsTabStop / KeyboardNavigation 지정: 0건
```

**저장소 전체에 `TabIndex` 가 0건**입니다. ACCESS-001 §5.1 은 "`TabIndex` 명시로 예측 가능하게
지정" 을 요구하므로 **그 요구는 이 카드 이전부터 미충족**입니다. 이번 변경은 메뉴 항목만
더했고, 메뉴는 Alt/F10 으로 들어가는 별도 탐색이라 **기존 Tab 순서를 바꾸지 않습니다.**

**스크린리더 실측은 하지 않았습니다**(카드 지시). 니모닉이 실제로 동작하는지, 읽어 주는 이름이
무엇인지도 재지 않았습니다 — **센 것은 XAML 의 선언뿐**입니다.

## 4. 미결 하나 — 회피가 무엇을 가릴 수 있는가 (가설)

C-58 에서 메뉴 시나리오가 키 시나리오 뒤에 1건 실패했고 Escape + 포커스 재취득으로 막았습니다.
**가설(미검증)**: 키 제스처가 `MenuItem` 으로 포커스를 옮겨 **메뉴가 열린 채로 남고**, 다음
시나리오의 `ViewMenu` 클릭이 열린 메뉴를 **닫는 데 쓰여** 항목까지 도달하지 못한다.

**그 회피가 가릴 수 있는 것**: 만약 위 가설이 맞다면, **실제 사용자도 F5~F8 을 누른 뒤 메뉴가
열린 상태로 남는 것**을 보게 됩니다 — 제 Escape 는 그 증상을 테스트에서만 지웁니다. 재현
조건을 잡지 못했으므로 **가설로만 적습니다.**

## 5. 미검증 (Gaps)

- **니모닉·Tab 은 XAML 선언을 센 것이지 동작을 잰 것이 아닙니다.** `Alt+V`, `Alt+C` 를 실제로
  눌러 보지 않았습니다.
- **스크린리더는 재지 않았습니다**(카드 지시).
- **`SwipeHorizontal` 의 렌더 결과를 단언하지 않았습니다.** 코드를 읽고 표로 적었을 뿐,
  C-57 처럼 픽셀로 재지 않았습니다.
- **잘못된 설정값의 영향 범위를 끝까지 보지 않았습니다.** 저장 시 되쓰이는지, 다음 실행에
  남는지는 재지 않았습니다.
- **§4 의 가설은 검증하지 않았습니다.**
- **키 제스처는 창이 포그라운드일 때만 봤습니다**(C-58 과 동일).

## 6. 잔여 위험 (Residual risk)

- **설정 파일이 모드 이름을 검증하지 않습니다.** 오타 하나로 세 표시가 서로 다른 값을 말하는
  상태가 되고, 그 상태에서 메뉴 체크는 전부 꺼져 있어 "지금 어느 모드인가" 에 답이 없습니다.
- **`SwipeHorizontal` 은 여전히 진입 수단이 없습니다.** COMPARE-001 이 요구하는 모드이므로
  "구현은 있는데 부를 수 없다" 가 남아 있습니다 — 판정은 리더 몫입니다.
- **`Source Only`/`Processed Only` 는 키가 없습니다**(판정대로). 메뉴가 유일한 경로입니다.
- **§4 의 회피가 실제 증상을 테스트에서만 가릴 수 있습니다.**

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
git log --oneline -S "SwipeHorizontal" -- gui clients
grep -c TabIndex gui/ImageProcTest/MainWindow.xaml gui/ImageProcTest/Views/*.xaml
dotnet test clients/ImageProcTest.E2ETests/… --no-build \
  --filter "FullyQualifiedName~ComparisonEntryPointScenarios" --logger "console;verbosity=detailed"
dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
powershell -File clients/ImageProcTest.E2ETests/Staging/Repeat-E2E.ps1 \
  -Times 3 -Backend Native -NativeDir build/ci-common/bin -ResultsDirectory build/e2e-c59
dotnet build clients/ImageProcTest.slnx -c Debug
```
