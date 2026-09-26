# GUI-C-96 보고 — 비교 기능 접근성 남은 항목 (#149 G-1·G-2·G-3·G-5)

- 기준: `dev/gui` 를 main `1820bb3` 로 fast-forward 한 뒤 커밋 `a580905` (미푸시)
- 증거 디렉터리: `build/e2e-c96/`
- 기준선: `build/c96-base/` — `git archive 1820bb3 gui clients` 를 풀어 따로 빌드한 것

## 1. 주장

### 요구 문서와 현재 코드 대조

| 항목 | 요구 (문서 줄) | 현재 코드 (a580905 이전, main 1820bb3) | 판정 |
|---|---|---|---|
| G-1 View 메뉴의 비교 항목 | MENU-001 L62(View 가 comparison modes 소유), L120(`Compare Mode: Swipe / Split / Overlay / Difference / Source Only / Processed Only`) | `MainWindow.xaml:243-293` `CompareModeMenuItem` 아래 6개 항목, 모두 `SetComparisonModeCommand` | **이미 구현됨** (GUI-C-58). 이번에 바꾸지 않음 |
| G-2 F5~F8 | MENU-001 §9.3 L301-304, §10.1 L427-430 | `MainWindow.xaml:47-50` `KeyBinding` 4개 → `SetComparisonModeCommand` | **이미 구현됨**. 이슈 본문의 "KeyBinding 0건"은 작성 시점 기준 |
| G-2 Ctrl+1 / Ctrl+2 | MENU-001 §9.3 원표가 Source/Processed Only 에 배정 → L308-314 정정: `Ctrl+1` 은 Zoom 100%(§10.1 L424, ACCESS-001 §5.2) | 미배정. `MainWindow.xaml:238-241` 주석이 결정을 적음 | **충돌이 이미 문서에서 결정됨**(Source/Processed 는 메뉴만). 구현하지 않음 |
| G-3 SourceOnly / ProcessedOnly 전환 UI | COMPARE-001 L168 `GUI-CMP-FR-005` | 메뉴 항목은 있음(G-1). 뷰포트 툴바 버튼은 4개(Swipe/Split/Overlay/Difference)뿐이고, `Click` 처리기가 설정에 직접 쓰기 | **이번에 구현**: 버튼 2개 추가, 6개 모두 같은 명령 |
| G-5 Display 메뉴 | 이슈가 인용한 MENU-001 L265 | 없음 | **구현하지 않음 — 문서 판독 문제**. 아래 참고 |

#### G-5 를 구현하지 않은 이유

- L265 는 §9.1 **Tools Menu 확장** 표의 한 줄이다. 표 머리는 `| Command | Phase | Enabled When | Owner | E2E |`(L254)이고, `Display` 는 **Owner** 열의 값이다.
- 최상위 메뉴 정의(§3 L58-65)에는 File / Backend / View / Pipeline / Tools / Help 만 있다. Display 메뉴는 없다.
- 앱의 `Tools → GSDF Calibrate... (Phase 1b+)`(`MainWindow.xaml:390`)는 §9.1 과 일치한다.
- 이슈 본문의 부수 문단도 "앱의 비활성 Tools 항목은 문서와 일치" 라고 적고 있어, G-5 행과 서로 모순된다.
- `docs/project/*.md` 에서 `Display 메뉴|Display menu|Display Menu` 를 검색하니 0건이었다.

### 이번 변경

1. **뷰포트 툴바 버튼**
   - 6개(Swipe / Split / Overlay / Difference / **Source** / **Processed**)가 모두 `Command="{Binding SetComparisonModeCommand}"` 다. 메뉴·F5~F8 과 같은 명령이다.
   - `ViewportShell.xaml.cs` 의 `CompareModeButton_Click` 을 지웠다. 불투명도 슬라이더는 기존 `OnSettingsPropertyChanged` 가 계속 따라간다.
   - 각 버튼에 AutomationId(`CompareButton<모드>`)를 붙였다.
2. **뷰포트가 그린 모드를 노출**
   - `ImageComparisonViewport` 가 렌더 패스에서 `_renderedMode` 를 기록한다. 소스 영상이 없으면 `none` 이다.
   - 자동화 피어의 HelpText 가 `rendered=<모드>` 다.
3. **E2E 판독 변경**
   - `ComparisonEntryPointScenarios` 가 모드를 `ViewportShell` HelpText 대신 `WorkbenchViewport` 의 `rendered=` 에서 읽는다. `ViewportShell` HelpText 는 `Settings.ComparisonMode` 에 묶여 있다.
   - 준비(priming)도 버튼을 AutomationId 로 찾는다.
4. **W-27 추가**: 버튼 6개가 각각 그 모드를 **그리게** 하는지 본다. 준비는 키 입력으로 한다(버튼과 다른 경로).

## 2. 증거

**빌드** (`build-final.txt`, `build-rename.txt`)
- `BUILD_EXIT_GUI=0`, `BUILD_EXIT_E2E=0`, `BUILD_EXIT_INT=0`, 경고 0

**전체 실행**

| 실행 | 결과 | 파일 |
|---|---|---|
| 통합 | 실패 0 / 통과 216 / 건너뜀 1 | `full-int.txt` |
| Mock E2E 전체 | 실패 0 / 통과 97 / 건너뜀 2 (C-95 때 91 + W-27 6건) | `full-mock.txt`, `full-mock.trx` |
| 이름 변경(W15→W27) 뒤 W-27 + W-14 | 통과 10 | `w27.txt` |

- 전체 실행은 이름을 바꾸기 전 빌드로 돌렸다. 이름 변경 커밋과의 차이는 메서드 이름·주석뿐이고, 바뀐 두 시나리오는 이름 변경 뒤 따로 다시 돌렸다.

**반증: 뷰포트의 `CompareMode` 바인딩을 삭제** (설정은 바뀌지만 화면은 Swipe 에 머무는 상태)

| 대상 | 결과 | 파일 |
|---|---|---|
| 이번 코드, W-14 + W-27 | **10/10 실패**. `F6 should select SplitLocked; the viewport reports 'SwipeVertical'`. Swipe 경우는 준비 단계에서 실패 | `fx-unbound.txt` (빌드 `BUILD_EXIT=0`) |
| 기준선(main 1820bb3, 옛 판독), W-14 | **4/4 통과** — 옛 판독은 이 상태를 보지 못했다 | `fx-base-unbound.txt` (빌드 `BUILD_EXIT=0`) |

- 복원 뒤 빌드했다(`fx-restore-build.txt`, `BUILD_EXIT=0`).
- 기준선 폴더는 약화된 채 남아 있다(`build/c96-base`, 커밋 대상 아님).

**#163 경로 결과** (카드 3번). `ComparisonEntryPointScenarios` **클래스만 단독으로** 돌렸을 때, W-13 의 첫 메뉴 경우가 "not found after opening View → Compare Mode" 로 실패한 횟수:

| 빌드 | 실패/실행 | 파일 |
|---|---|---|
| 기준선 main 1820bb3 (W-13, W-14 만 있음) | **4/5** | `base-1..5.txt` |
| 이번 코드, 버튼 Command 바인딩 | 5/5 | `entry2..5.txt`, `probe-run.txt` |
| 이번 코드, 버튼만 옛 Click 방식 | 1/3 | `click-run1..3.txt` |
| 이번 코드, Click → 명령 `Execute` | 3/5 | `final-entry1..5.txt` |
| 이번 코드, W-27 제외(W-13 + W-14) | 3/5 | `noW15-1..5.txt` |

- 전체 Mock 실행 안에서는 W-13 6건이 모두 통과했다(`full-mock.txt`).
- 실패한 경우의 탐침(`probe.log`, 탐침 코드는 커밋하지 않음):
  - 메뉴를 열기 전 포커스는 툴바 버튼(`CompareButtonSwipeVertical`)이다.
  - `Compare Mode` 를 클릭한 직후에는 `CompareModeMenuItem` 자체가 트리에 없다(`compareKids=[(no compare item)]`). 메뉴 전체가 닫혀 있다.
  - 통과한 경우에는 같은 시점에 포커스가 `CompareModeMenuItem` 이고, 자식 7개가 보인다.
  - 통과한 경우에도 메뉴를 열기 전 포커스는 같은 툴바 버튼이다. 따라서 포커스 위치로는 둘을 구별하지 못한다.
- 첫 W-27 초안은 준비 단계를 메뉴로 했고, 같은 증상으로 1건 실패했다(`entry1.txt`). 그래서 준비를 키 입력으로 바꿨다.

## 3. 기준과 판단

- **버튼은 Command 바인딩으로 두었다.**
  - Click 방식과 Command 방식의 #163 실패율 차이(4/8 대 5/5)는 기준선(4/5)과 구별되지 않는다.
  - 카드의 "모두 같은 명령"을 그대로 따르는 형태다.
- **Source / Processed 버튼을 추가했다.**
  - 이슈 G-3 이 "버튼이 없음"을 부재로 적었다.
  - COMPARE-001 FR-005 는 노출만 요구하므로 메뉴로도 충족되기는 한다. 버튼 추가는 이슈 문구를 따른 선택이다.
- **Ctrl+1/2 와 G-5 는 구현하지 않았다.** 카드 4번(충돌·불일치는 구현하지 않고 적기)에 따랐다.

### C-95 판정 2번 — "화면 상태" 9개의 표시 시험

`clients/ImageProcTest.E2ETests`, `clients/ImageProcTest.IntegrationTests` 의 `*.cs` 를 검색했다(`SettingsProcessingConnectionTests` 제외).

| 설정 | 표시가 바뀌는지 보는 시험 | 비고 |
|---|---|---|
| `ComparisonMode` | **있음** — W-13, W-14, W-27 이 뷰포트가 그린 모드를 읽는다(이번 카드) | W-11/W-12(`ComparisonModeScenarios`)는 버튼 경로 |
| `ComparisonOverlayOpacity` | **없음** | `ComparisonModeScenarios.cs:23,93` 에 슬라이더가 나오지만 반증 설명이고, 불투명도 단언은 없다 |
| `ComparisonPanX` | **없음** | 검색어 `PanX\|PanY\|\bPan\b\|Drag` 0건 |
| `ComparisonPanY` | **없음** | 같음 |
| `ComparisonSwipePosition` | **없음** | 검색어 `SwipePosition\|divider\|Swipe position` 0건 |
| `ComparisonZoomScale` | **부분** — `DetachedViewerSyncScenarios` W-15 가 분리 뷰어 휠 뒤 메인 창 `ZoomPercentText` 가 바뀌는지 본다 | 표시 문자열만 보고, 그린 배율은 보지 않는다 |
| `ShowDisplayPanel` | **해당 화면 없음** — 토글(`MainWindow.xaml:184-189`)은 비활성이고, `PanelToggleScenarios` S07 이 비활성을 단언한다 | MENU-001 L276 "대응 화면 없음" |
| `AnalysisTab` | **간접** — `OpenParameters` 뒤 파라미터 탭의 요소를 찾는 시나리오들(`ViewportTruthScenarios`, `WorkflowScenarios`, `FailedRenderScenarios`, `UnappliedSettingsScenarios`) | 탭 전환 자체를 단언하는 시험은 없다 |
| `FocusMode` | **없음** | 검색 결과는 `window.Focus()` 호출뿐 |

## 4. 미검증

- **#163 원인**: 이번에도 확정하지 못했다. 클래스 단독 실행에서 기준선 실패율이 4/5 로 높다는 것, 실패 시점에 메뉴 전체가 닫혀 있다는 것만 측정했다.
- **Native 실행**: 하지 않았다. 바뀐 경로는 백엔드와 무관한 UI 경로다.
- **rendered 값의 의미 범위**: `OnRender` 에서 모드를 계산한 시점의 값이다. 각 모드의 픽셀이 요구대로 그려졌는지(예: Split 의 위치)는 이 판독으로 보지 않는다.
- **새 버튼의 외형**: 툴바 너비가 넓어진 것을 눈으로 확인하지 않았다(스크린샷 없음).

## 5. 잔여 위험

- **#163 과 E2E 결과**: 클래스를 단독으로 돌리면 W-13 이 자주 빨강이 된다. 전체 실행에서는 초록이지만, 실행 순서가 바뀌면 전체 실행에서도 나올 수 있다.
- **HelpText 의미 변경**: `WorkbenchViewport` 의 HelpText 가 비어 있다가 `rendered=…` 가 됐다. 보조기술에도 이 문자열이 읽힌다.
- **Swipe 준비 경로**: W-27 Swipe 경우는 F8 로, 나머지는 F5 로 준비한다. 키 바인딩이 깨지면 W-27 도 준비 단계에서 실패한다. 이 경우 원인이 버튼이 아니라는 것은 메시지(`Priming with …`)로 구별된다.

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| G-1/G-2/G-3/G-5 대조 표 | GUI-C-96 |
| G-3 버튼 + 같은 명령 + 그린 모드 E2E | GUI-C-96 |
| #163 경로 결과 | GUI-C-96 (#163 은 열린 채) |
| G-5 Display 메뉴 — 문서 판독 문제 | 리더 판정 필요 |
| 화면 상태 9개의 표시 시험 표 | GUI-C-96 (C-95 판정 2번) — 있음 1, 없음 5, 부분 1, 간접 1, 해당 화면 없음 1 (합 9) → 후속 카드 여부는 리더 판단 |
| Lane A/B 알고리즘 확장 #182 코멘트 | GUI-C-96 (C-95 판정 3번) |
