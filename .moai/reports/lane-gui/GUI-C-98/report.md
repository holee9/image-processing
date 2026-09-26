# GUI-C-98 보고 — "화면 상태" 설정이 실제로 화면을 바꾸는지 (#182 후속)

- 커밋: `dev/gui` `bf66a97` (미푸시, `a580905` 위)
- 증거 디렉터리: `build/e2e-c98/`

## 1. 주장

1. **뷰포트가 그릴 때 실제로 쓴 값을 자동화 HelpText 로 낸다.**
   - 형식: `rendered=<모드>; zoom=<ZoomScale|fit>; scale=<그린 배율>; offset=<x>,<y>; swipe=<분할선|->; opacity=<불투명도|->`
   - 각 값은 그리기 코드가 그 값을 쓰는 자리에서 기록한다(`ImageComparisonViewport.cs` `OnRender`, `DrawVerticalSwipe`/`DrawHorizontalSwipe`, Overlay 분기).
   - `offset` 은 그린 영상의 중심이 뷰포트 중심에서 떨어진 거리이고, 이것이 팬이다.
2. **여섯 개 설정마다 그려진 값을 읽는 판독을 추가했다** (`ViewStateRenderScenarios`).

| 설정 | 판독 | 경로 |
|---|---|---|
| `ComparisonOverlayOpacity` | V-01: 슬라이더 0.9 → `opacity=0.9`, 0.2 → `opacity=0.2` | 슬라이더 → 설정 → 뷰포트 |
| `ComparisonZoomScale` | V-02: 휠로 확대(`zoom=1.0296`) → Reset → `zoom=fit; scale=0.5959` | Reset(설정만 씀) → 뷰포트 |
| `ComparisonPanX` | V-03: 오른쪽 버튼 드래그(`offset=80,60`) → Reset → x=0 | 같음 |
| `ComparisonPanY` | V-04: 같은 드래그 → Reset → y=0 | 같음 |
| `ComparisonSwipePosition` | V-05: 분할선 드래그(`swipe=0.25`) → Reset → `swipe=0.5` | 같음 |
| `FocusMode` | V-06: 포커스 토글 뒤 두 옆 패널이 트리에서 사라지는지 본다. **기본 설정에서는 사라지지 않아 이유와 함께 건너뛴다** | 발견, 3절 |

   - **포인터 조작 뒤 Reset 으로 되돌리는 이유**: 드래그와 휠은 뷰포트 자신의 속성을 먼저 바꾼다. 그래서 드래그만 보면 바인딩을 지워도 영상이 움직인다. Reset(`MainWindowViewModel.cs` `ResetComparisonView`)은 **설정만** 쓰므로, 설정에서 화면으로 가는 경로를 본다.
3. **#182 연결 조사 시험에 표시 판독 단언을 넣었다.**
   - `ViewStateReadings` 가 화면 상태마다 판독을 적는다: E2E 메서드 이름, 또는 사유가 있는 `NONE:`.
   - `EveryViewStateSetting_HasADisplayReading` 은 다음 셋 중 하나라도 해당하면 빨강이다.
     - 판독이 없다.
     - 적힌 메서드가 E2E 소스(`clients/ImageProcTest.E2ETests/**/*.cs`)에 없다.
     - 목록에서 빠진 설정의 판독이 남아 있다.
   - `NONE` 두 건과 그 분류 근거:
     - `ShowDisplayPanel` — 대응 화면 없음. View 메뉴 토글이 비활성이고(`PanelToggleScenarios` S07 이 단언), MENU-001 §9.2 에 대응 화면이 없다.
     - `AnalysisTab` — 간접. 시나리오들이 `OpenParameters` 뒤 파라미터 탭 요소를 찾지만, 탭 전환 자체를 단언하는 경우는 없다.
4. **GUI-C-95 결함을 고쳤다.**
   - `IsWritable` 의 정규식 두 줄에 `\b` 대신 **백스페이스 문자(0x08)** 가 들어가 있었다. 이 코드는 main 에도 있다(`origin/main` 에서 `grep -c $'\x08'` = 2).
   - 그래서 명시적 `Mode=TwoWay/OneWay` 판정이 한 번도 맞지 않았고, 모든 답이 기본 양방향 표에서 나왔다. C-95 의 분류 결과는 표 덕분에 같았다.
   - 고친 뒤 `IsWritable_HonoursAnExplicitMode` 를 추가했다.

## 2. 증거

**빌드** (`build-last.txt`)
- `BUILD_EXIT_GUI=0`, `BUILD_EXIT_E2E=0`, `BUILD_EXIT_INT=0`, 경고 0

**전체 실행**

| 실행 | 결과 | 파일 |
|---|---|---|
| 통합 | 실패 0 / 통과 219 / 건너뜀 1 (216 + 새 시험 3) | `full-int-last.txt` |
| Mock E2E 전체 | 실패 0 / 통과 102 / 건너뜀 3 (97 + V-01~05) | `full-mock.txt`, `full-mock.trx` |

- 건너뜀: V-06(이유 문구), IB-02, NativeProvenance.

**반증 — 뷰포트 바인딩을 하나씩 삭제하고 앱 재빌드**
- 재빌드는 모두 `BUILD_EXIT=0` 이었다. 복원 뒤 재빌드도 `BUILD_EXIT=0` 이다.

| 삭제한 바인딩 | 결과 | 실패 문구 (그려진 값) |
|---|---|---|
| `OverlayOpacity` | V-01 실패 | `The slider was set to 0.9; … opacity=0.5` (`fx-OverlayOpacity2.txt`) |
| `ZoomScale` | V-02 실패 | `Reset wrote ComparisonZoomScale=0; … zoom=1.0296` |
| `PanX` | V-03 실패 | `Reset wrote ComparisonPanX=0; … offset=80,0` |
| `PanY` | V-04 실패 | `Reset wrote ComparisonPanY=0; … offset=0,60` |
| `SwipePosition` | V-05 실패 | `Reset wrote ComparisonSwipePosition=0.5; … swipe=0.25` |

- **첫 불투명도 반증은 무효였다.**
  - 실패 문구가 `Overlay was not drawn` 이었다(`fx-OverlayOpacity.txt`). 불투명도에 닿기 전에 멈춘 것이다.
  - 정상 빌드에서도 V-01 단독 실행이 2/2 같은 문구로 실패했다(`v01-normal-1..2.txt`). 앱 시작 직후에는 F7 이 창에 닿지 않는다.
  - Overlay 전환을 툴바 버튼으로 바꾼 뒤 정상 빌드 V-01 단독은 2/2 통과했다(`v01-fixed-1..2.txt`). 반증도 불투명도 문구로 실패했다.

**반증 — 통합 시험**

| 약화 | 결과 | 파일 |
|---|---|---|
| `IsWritable` 두 줄에 0x08 다시 넣기 (주입 확인: 2개) | `IsWritable_HonoursAnExplicitMode` 실패 1 | `fs1-backspace.txt` |
| `ViewStateReadings` 에서 `ComparisonPanX` 줄 삭제 | `EveryViewStateSetting_HasADisplayReading` 실패 1 | `fs2-reading-removed.txt` |

- 복원 뒤 통과했다(`fs-restored.txt`).
- 첫 0x08 주입은 셸 인용 때문에 들어가지 않았다(0개). 그 실행(통과 219)은 무효로 보고 스크립트 파일로 다시 했다.
- 상시 대조군 `Control_ViewStateWithoutAReading_IsRed` 는 세 경우를 본다: 판독 없는 새 항목, 없는 메서드 이름, 사유 없는 `NONE`.

## 3. 발견 — 포커스 모드는 기본 설정에서 화면을 바꾸지 않는다

- `FocusPanelVisibilityConverter` 동작: 포커스 모드가 켜지면 `LeftPanelOpen` / `RightPanelOpen` 을 따르고, 꺼지면 늘 Visible 이다.
- 두 값의 기본값은 `true` 다(`AppSettings.cs`).
- 앱에는 두 값을 바꾸는 호출자가 없다. MENU-001 L293 도 "setter 가 있는데 앱 안에 호출자가 없어" 라고 적는다.
- 따라서 기본 설정에서 **Focus 버튼을 눌러도 화면이 바뀌지 않는다**. 측정 결과 `V06 focus on: analysis=True queue=True`(`v1.txt`, `v2.txt`).
- 설계 문서(`docs/design/IMPLEMENTATION_GUIDE.md` Slice 8)는 "FocusMode 가 true 이면 좌우 패널을 36 px 레일로 접는다"고 한다. 레일은 구현되어 있지 않다.
- 관련 사실: 버튼을 UIA Toggle 패턴으로 누르면 `IsChecked` 만 바뀌고 명령은 실행되지 않는다. 그래서 V-06 은 실제 클릭을 쓴다. 클릭으로도 결과는 같았다.
- **V-06 의 한계**: 지금은 늘 건너뛴다. 건너뛰는 시험은 바인딩을 지워도 건너뛸 수밖에 없다(돌려 보지는 않았다). 따라서 **이 판독은 아직 아무것도 가려내지 못한다**. 포커스 모드가 무언가를 숨기게 되면 그때부터 단언한다.
- 조치(레일 구현, 기본값 변경, 토글 비활성)는 리더가 판단할 일이다.

## 4. 미검증

- **V-01~05 의 단독 실행 안정성**: V-01 만 확인했다(2/2). 나머지는 클래스 실행과 전체 실행에서 각각 통과했다.
- **Native 실행**: 하지 않았다.
- **`FocusMode` 반증**: V-06 이 건너뛰므로 돌려도 의미가 없다. 돌리지 않았다.
- **분리 뷰어**: 분리 뷰어의 같은 값(HelpText)은 이 시나리오에서 보지 않았다. 같은 컨트롤이므로 형식은 같다.
- **`scale`/`offset` 의 반올림**: 소수 네 자리·한 자리 표기라 1 픽셀 미만 차이는 보이지 않는다.

## 5. 잔여 위험

- **HelpText 사용처**: 뷰포트의 HelpText 가 길어졌다. 스크린 리더가 이 문자열을 읽는다.
- **포인터 입력 의존**: V-02~05 는 실제 마우스 입력을 쓴다. 다른 창이 뷰포트를 덮으면 실패한다(시작 시 분리 뷰어는 닫는다).
- **Reset 경로**: V-02~05 가 모두 Reset 명령 하나에 기대므로, Reset 이 깨지면 네 건이 함께 빨강이 된다. 원인은 실패 문구의 `Reset wrote …` 로 알 수 있다.
- **C-95 결함의 범위**: main 의 `SettingsProcessingConnectionTests` 는 이 커밋이 병합되기 전까지 명시적 모드를 무시한다.

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| 6개 설정의 그려진 값 판독 + 바인딩 삭제 반증 | GUI-C-98 (5개 반증 빨강, FocusMode 는 3절) |
| ShowDisplayPanel / AnalysisTab 분류 근거 | GUI-C-98 |
| 연결 조사 시험의 판독 단언 | GUI-C-98 |
| C-95 `IsWritable` 백스페이스 결함 | GUI-C-98 에서 수정 |
| 포커스 모드 무효과 | 리더 판단 필요 (새 카드 후보) |
| 픽셀 연쇄 구현 | GUI-C-99 |
