# GUI-C-58 — 비교 기능에 닿을 방법을 만든다 (#149 G-1·G-2·G-3)

> **이번 실행이 쓴 네이티브**: run **34535620953** · head `83ffa7ae…` ·
> `xpe_preprocess.dll` md5 `9f81d2f07fb7f7e23e90c86332467105`.

- 카드: GUI-C-58 · Refs #149 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 1건 — 미푸시
- **키 6개 중 3개만 배선했습니다 — 나머지 셋은 문서끼리 충돌합니다**(§3). 카드 지시대로 배선하지 않고 보고합니다.
- **결과: Native 3회 0 실패 · Mock 0/56/2/58 · 통합 0/180/1/181 · slnx 0경고 0오류**

---

## 1. 먼저 — 무엇이 이미 있었는지 셌습니다

카드가 "표는 문서 쪽 근거이고 코드 쪽 현황은 당신이 재는 것" 이라고 했으므로, 손대기 전에
종류별로 셌습니다.

| 있던 것 | 수 | 위치 |
|---|---|---|
| 렌더러가 처리하는 모드 | **7** | `ImageComparisonViewport.OnRender` |
| 뷰모델의 모드 목록 | **7** | `MainWindowViewModel.cs:75-83` (`CompareModeOptions`, 바인딩 대상 없음) |
| 모드 선택 버튼 | **4** | `Views/ViewportShell.xaml:40-66` (Swipe·Split·Overlay·Difference) |
| View 메뉴의 비교 항목 | **0** | `MainWindow.xaml` ViewMenu — Reset/Detach 는 있고 모드 선택은 없음 |
| `KeyBinding`/`InputBindings` | **0 파일** | 저장소 전체 |
| 모드 선택 **명령**(ICommand) | **0** | 버튼이 코드비하인드에서 설정에 직접 씀 |

**표와 다른 점 하나**: 카드 표는 G-3 을 "`SourceOnly`/`ProcessedOnly` 전환 UI 없음" 으로
적는데, 정확히는 **그 둘만이 아니라 `SwipeHorizontal` 까지 셋**이 어떤 진입 수단도 없었습니다.
`SwipeHorizontal` 은 렌더러와 뷰모델 목록에는 있지만 **문서 §9.3 의 6개 목록에는 없습니다** —
이번에 배선하지 않았고, 문서에 없는 7번째 모드가 있다는 사실을 보고합니다.

**그리고 진짜 부재는 "명령이 없다" 였습니다.** 버튼은 코드비하인드에서
`vm.Settings.ComparisonMode = mode` 를 직접 쓰므로, 메뉴 항목이나 키 제스처가 **바인딩할 대상이
없었습니다.** 그래서 이번 변경의 중심은 메뉴가 아니라 `SetComparisonModeCommand` 라는 seam
입니다 — 버튼·메뉴·키가 모두 그 하나를 지납니다.

## 2. 무엇을 만들었나

| # | 만든 것 | 수 |
|---|---|---|
| G-1 | `View → Compare Mode` 하위 메뉴 | 항목 **6** |
| G-2 | `Window.InputBindings` 키 제스처 | **3** (F5·F7·F8) |
| G-3 | `SourceOnly`/`ProcessedOnly` 진입 수단 | 메뉴 항목 **2** (이전엔 0) |
| — | 모드 선택 명령 `SetComparisonModeCommand` | 1 |
| — | E2E 시나리오 W-13(메뉴 6) · W-14(키 3) | **9** |

명령은 알 수 없는 이름을 **무시**합니다(`CompareModeOptions` 에 없으면 쓰지 않음). 매개변수가
XAML 에서 오므로 오타가 컴파일 오류가 아니고, 잘못 쓰면 뷰포트가 기본 모드로 되돌아간 채
이유를 말하지 않기 때문입니다.

## 3. 배선하지 않은 것 — 키 3개, 문서끼리 충돌합니다

MENU-001 §10 은 ACCESS-001 §5.2 와 **단일 source-of-truth** 라고 적습니다. 실제로는 어긋납니다.

| 키 | MENU-001 §9.3 | MENU-001 **§10.1** | ACCESS-001 §5.2 | 조치 |
|---|---|---|---|---|
| `F5` | Swipe | Swipe | Swipe | **배선** |
| `F6` | Split | Split | **Compare Difference** | **보류** |
| `F7` | Overlay | Overlay | 없음 | **배선** |
| `F8` | Difference | Difference | 없음 | **배선** |
| `Ctrl+1` | **Source Only** | **Zoom 100 %** | **Zoom 100 %** | **보류** |
| `Ctrl+2` | Processed Only | 없음 | 없음 | **보류**(짝) |

두 가지를 구분해 적습니다:

- **`Ctrl+1` 은 MENU-001 내부에서 어긋납니다.** §9.3 은 Source Only, §10.1 은 Zoom 100 % 입니다.
  같은 문서 안의 두 표가 서로 다릅니다. ACCESS-001 은 §10.1 편입니다.
- **`F6` 은 문서 사이에서 어긋납니다.** MENU-001 은 Split, ACCESS-001 은 Difference 입니다.

`Ctrl+2` 는 어디와도 충돌하지 않지만 **보류했습니다** — 짝의 한쪽만 배선하면 "Ctrl+1 은 왜
안 되나" 를 만들고, 그 답이 문서 결정에 달려 있기 때문입니다. **보류한 셋은 전부 메뉴로
닿습니다.**

**Zoom 100 % 자체도 아직 키가 없습니다**(`InputBindings` 0건이었으므로). 즉 `Ctrl+1` 은 지금
비어 있고, 충돌은 구현끼리가 아니라 **문서끼리**입니다. 선택은 문서 결정입니다.

## 4. 현재 모드를 읽을 수단 — 있습니다(셋)

카드가 "없으면 보고하라" 고 한 항목입니다. **재서 말하면 셋 있고, 하나는 이번에 생겼습니다.**

| 수단 | 사람이 보나 | 비고 |
|---|---|---|
| 뷰포트 HUD 좌상단 `{mode} \| zoom … \| pan …` | **예** | `ImageComparisonViewport.DrawHud` — 모드 이름을 그대로 그림 |
| 차분 모드의 하단 라벨 | 예 | 차분 모드일 때만 |
| `ViewportShell` 의 `HelpText` | 아니오 | UIA 전용, #149 G-6 (GUI-C-47) |
| **메뉴 항목의 체크 표시** | **예** | 이번에 추가 |

**버튼 4개에는 선택 상태 표시가 없습니다** — 어느 것이 현재 모드인지 버튼만 보고는 알 수
없습니다. 이건 그대로 두고 보고합니다(카드: 만들지 말고 보고).

메뉴 체크는 상태 표시를 새로 만든 것이 아니라 **모드 선택 메뉴에 붙는 표준 동작**이라 넣었고,
`IsChecked` 는 **단방향**입니다 — 양방향이면 설정에 두 번째 쓰는 주체가 생기고, 그건 C-47 이
걷어낸 모양입니다.

## 5. 증거 (Evidence)

### (a) 반증 — 배선을 하나씩 끊었습니다

끊지 않고 **약화**했습니다(요소는 남기고 `Command` 바인딩만 제거) — 지우면 시나리오가
"요소가 없다" 로 실패해 **무엇을 확인한 것인지 흐려집니다.** 매번 빌드 0오류를 확인했습니다.

| 끊은 것 | W-11 버튼 존재 | W-12 버튼 전환 | W-13 메뉴 | W-14 키 |
|---|---|---|---|---|
| **키 바인딩 3개** | 통과 | 통과 | 통과 | **3건 실패** |
| **메뉴 Command 6개** | 통과 | 통과 | **6건 실패** | 통과 |
| 버튼 핸들러(§6) | 통과 | **4건 실패** | 6건 실패 | 2건 실패 |

```
[cut=keys] 실패!  - 실패: 3, 통과: 14, 전체: 17
[cut=menu] 실패!  - 실패: 6, 통과: 11, 전체: 17
[cut=button] 실패! - 실패: 12, 통과: 5, 전체: 17
```

**앞의 두 줄이 카드가 요구한 것이고, 깨끗하게 갈립니다** — 키를 끊으면 키 단언만, 메뉴를
끊으면 메뉴 단언만 실패합니다. 진입 수단 셋은 서로 독립입니다.

### (b) 실측 (verbatim)

```
W-13 CompareSwipeMenuItem: viewport reports SwipeVertical
W-13 CompareSplitMenuItem: viewport reports SplitLocked
W-13 CompareOverlayMenuItem: viewport reports OverlayOpacity
W-13 CompareDifferenceMenuItem: viewport reports DifferenceHeatmap
W-13 CompareSourceOnlyMenuItem: viewport reports SourceOnly
W-13 CompareProcessedOnlyMenuItem: viewport reports ProcessedOnly
W-14 F5: viewport reports SwipeVertical
W-14 F7: viewport reports OverlayOpacity
W-14 F8: viewport reports DifferenceHeatmap

dotnet test …E2ETests… --no-build                      (Mock)
통과!  - 실패: 0, 통과: 56, 건너뜀: 2, 전체: 58 (27 s)

dotnet test …IntegrationTests… -c Debug
통과!  - 실패: 0, 통과: 180, 건너뜀: 1, 전체: 181

Repeat-E2E.ps1 -Times 3 -Backend Native → runs with a failure: 0 / 3
run 1 69s · run 2 70s · run 3 68.9s

dotnet build clients/ImageProcTest.slnx -c Debug
    경고 0개 / 오류 0개
```

### (c) 시간 예산 — 카드가 물은 것

| 시점 | Native 벽시계 | 180 s 게이트 대비 여유 |
|---|---|---|
| C-57 최종 (이 세션) | 57.6 / 58.7 / 58.4 s | 약 121 s |
| **C-58 (이 카드)** | **69 / 70 / 68.9 s** | **약 110 s** |

**증가분 약 11 s**(시나리오 9건). Mock 도 16 s → 27 s 입니다.

**다만 카드가 인용한 "C-57 시점 73 s" 와 제가 이 세션에서 잰 58 s 가 다릅니다** — 같은 코드에서
15 s 차이가 났으므로 **기계 부하가 통제되지 않았습니다.** 그래서 11 s 를 시나리오 비용이라고
단정하지 않습니다. 말할 수 있는 것은 **지금 70 s 이고 게이트까지 110 s 남았다**는 것입니다.

## 6. 미검증 (Gaps) · 자기 시나리오의 약점

- **사전 전환(priming)을 버튼으로 합니다.** 그래서 버튼 배선을 끊으면 W-13·W-14 도 무너집니다
  (§5a 세 번째 줄). 진입 수단으로서는 독립이지만 **시나리오로서는 버튼에 의존**합니다. 일부러
  그렇게 했습니다 — 시험 대상과 다른 경로로 사전 전환해야 하기 때문입니다.
- **사전 전환 단언은 "이미 그 모드였던 경우" 를 못 거릅니다.** 버튼을 끊은 반증에서 W-14 F7 이
  통과했는데, 그때 앱이 이미 기본값 `SwipeVertical` 이라 사전 전환 단언이 **공허하게** 참이
  됐습니다. 끝 상태만 보고 클릭이 원인인지는 보지 않습니다.
- **메뉴 시나리오가 다른 시나리오의 뒤끝에 민감합니다.** 단독 실행 6/6 통과인데 전체 실행에서
  마지막 하나가 실패했습니다(키 시나리오가 포커스를 가져간 뒤). Escape + 포커스 재취득으로
  막았고 이후 재현되지 않았지만, **원인을 확정하지는 못했습니다** — 상관만 봤습니다.
- **접근성은 문서만 대조했습니다.** 실제 스크린리더·Tab 순서·Alt 니모닉 충돌은 재지 않았습니다.
  `Alt+V` 아래 `_Compare Mode` 의 `C` 가 기존 항목과 겹치는지도 실행으로 확인하지 않았습니다.
- **`SwipeHorizontal` 은 여전히 진입 수단이 없습니다**(§1). 문서 §9.3 에 없어서 배선하지
  않았습니다.
- **키 제스처는 창이 포그라운드일 때만 봤습니다.** 다른 컨트롤에 포커스가 있을 때(예: 텍스트
  입력 중) F5 가 어떻게 되는지 재지 않았습니다.
- **모드가 실제로 그렇게 그려지는지는 이 카드가 보지 않습니다** — 설정값까지입니다. 그 뒤는
  C-57 의 렌더 단언이 봅니다.

## 7. 잔여 위험 (Residual risk)

- **키 3개가 비어 있습니다.** F6·Ctrl+1·Ctrl+2 를 누르면 아무 일도 없고, 메뉴에도 단축키가
  적혀 있지 않습니다. 문서 결정이 나면 배선은 한 줄씩입니다.
- **`Ctrl+1` 은 Zoom 100 % 쪽이 먼저 가져갈 수도 있습니다.** 그러면 §9.3 의 Source Only 배정이
  폐기되는 것이고, 그건 요구 변경입니다.
- **메뉴 체크는 단방향이라 설정이 밖에서 바뀌어도 따라갑니다** — 반대로 체크를 눌러서 해제할
  수는 없습니다(라디오 성격). 의도한 동작입니다.
- **시나리오 9건이 Native 실행에 약 11 s 를 더합니다**(§5c의 단서 포함). 진입 수단이 더 늘면
  같은 비율로 늘어납니다.

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.E2ETests/… --no-build \
  --filter "FullyQualifiedName~ComparisonEntryPointScenarios" --logger "console;verbosity=detailed"
dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
powershell -File clients/ImageProcTest.E2ETests/Staging/Repeat-E2E.ps1 \
  -Times 3 -Backend Native -NativeDir build/ci-common/bin -ResultsDirectory build/e2e-c58
dotnet build clients/ImageProcTest.slnx -c Debug
```
