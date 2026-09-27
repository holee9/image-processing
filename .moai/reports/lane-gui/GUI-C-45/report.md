# GUI-C-45 — §4.2 구현 불가 3건이 요구인지 확인 (#136)

> **이번 실행이 쓴 네이티브**: run **34535620953** · head `83ffa7ae…` ·
> `xpe_preprocess.dll` md5 `9f81d2f07fb7f7e23e90c86332467105`.

- 카드: GUI-C-45 · Refs #136 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui` · 병합 `e537d2c`
- **커밋 없음** — 조사 카드다(앱 UI 변경·계획서 수정 모두 카드가 금지).
- **결과: Native E2E 0/16/0/16 · Mock 0/15/1/16 · 통합 0/180/1/181 · slnx 0/0**
- **판정: 3건 모두 "요구다". 그리고 C-43 의 결론 하나를 정정한다 — 기능은 앱에 있다.**

---

## 0. 먼저, C-43 의 내 결론을 정정한다

C-43 §1 에 이렇게 적었다:

> W-04 | View→**Compare Mode→Swipe** | **그런 메뉴가 없다.** … | 계획서 오류 → 구현 불가

**"메뉴가 없다" 는 맞고, "구현 불가" 는 틀렸다.** 이번에 `gui/` 전체를 다시 재니:

| 실측 | 위치 |
|---|---|
| 비교 모드 6종 상수 | `MainWindowViewModel.cs:75` — `SwipeVertical` `SwipeHorizontal` `SplitLocked` `OverlayOpacity` `DifferenceHeatmap` `SourceOnly` `ProcessedOnly` |
| 렌더링 | `Controls/ImageComparisonViewport.cs:131-178` — 7개 모드를 `DrawingContext` 로 직접 그린다 |
| **사용자 조작 UI** | `Views/ViewportShell.xaml:38-66` — **버튼 4개**(`Swipe` `Split` `Overlay` `Difference`), `Tag` 로 모드 전달 |
| 상태 | `AppSettings.ComparisonMode` · `ComparisonSwipePosition` · `ComparisonOverlayOpacity` |

C-43 은 `MainWindow.xaml` 의 **메뉴만** 훑었다. 기능은 뷰포트 셸 안에 있었고, 그 파일을 보지
않았다. **조사 범위를 좁게 잡고 그 범위의 결과를 전체 결론으로 말한 것**이 오류의 형태다 —
"메뉴에 없다" 까지만 말했어야 했다.

이 정정이 C-45 의 판정 전체를 바꾼다: 아래 3건은 "빠진 기능" 이 아니라 **대부분 있는데 접근
경로가 문서와 다른 것**이다.

## 1. 요구 확인 — 인용과 판정

### (1) 두 영상 비교(Compare Mode) — Swipe / Difference → **요구다**

`docs/project/XPE-GUI-COMPARE-001_Large_Image_Comparison_Viewer_Spec.md`:

> **L79** `GUI-CMP-FR-002` | The default comparison mode shall be vertical swipe with a draggable
> divider. | Mouse drag updates the divider without reprocessing the image.
>
> **L82** `GUI-CMP-FR-005` | The viewer shall expose source only, processed only, split locked,
> overlay opacity, and difference heatmap modes. | Mode switching does not reload source data.

`docs/project/XPE-GUI-MENU-001_Menu_and_Command_Strategy.md`:

> **L120** `Compare Mode: Swipe / Split / Overlay / Difference / Source Only / Processed Only`
>
> **L62** | `View` | panel visibility, image zoom, **comparison modes**, layout reset, … | planned shell |
>
> **L285-290** (§9.3 Compare Mode Commands) `Swipe` → **F5**, `Split` → F6, `Overlay` → F7,
> `Difference` → **F8**, `Source Only` → Ctrl+1, `Processed Only` → Ctrl+2

**판정: 요구다.** 전용 스펙 문서에 FR 번호까지 붙어 있다.

**구현 상태(실측)** — 요구를 셋으로 쪼개면 갈린다:

| 요구 조각 | 상태 |
|---|---|
| 모드 렌더링(FR-005 의 6모드) | **구현됨** (`ImageComparisonViewport.cs`) |
| 사용자 모드 전환 | **부분** — 뷰포트 셸 버튼 4개(Swipe/Split/Overlay/Difference). `SourceOnly`·`ProcessedOnly` 는 버튼이 없다 |
| **View 메뉴 접근**(MENU-001 L62) | **없음** — View 메뉴에 비교 항목이 없다 |
| **단축키 F5~F8 / Ctrl+1,2**(§9.3) | **없음** — `MainWindow.xaml`·`ViewportShell.xaml` 에 `KeyBinding` 이 0건 |

즉 **없는 것이 아니라 안 한 것**이다 — leader 가 말한 B-45 와 같은 형태다.

한 가지 더: `ImageComparisonViewport.cs:131` 의 코드 주석이 스스로 한계를 적어 두었다 —
`DifferenceHeatmap is a visual tint approximation, not a true per-pixel difference image`.
FR-005 는 "difference heatmap 모드를 노출한다" 까지만 요구하므로 문언상 충족이지만,
`|source - processed|`(MENU-001 L288)를 문자 그대로 읽으면 **근사이지 차분 영상이 아니다.**

### (2) GSDF 를 **View 메뉴에서** 접근 → **요구가 아니다** (다른 위치가 요구다)

`XPE-GUI-MENU-001` L265:

> | `GSDF Calibrate...` | **1b** | xpe_display.dll ready | **Display** | (planned) |

메뉴 소유 문서는 **Display** 를 지정한다. 계획서 §4.2 의 `View→Display LUT→GSDF On` 은
**어느 문서와도 맞지 않는다** — View 도 Tools 도 아니다.

앱 실측: `GsdfCalibrateMenuItem` 이 **Tools 아래**, `IsEnabled="False"`,
헤더 `"_GSDF Calibrate... (Phase 1b+)"`. 그리고 **앱에 `Display` 메뉴 자체가 없다.**

**판정: "View 메뉴에서 접근" 은 요구가 아니다.** 계획서 §4.2 의 W-06 경로는 근거 없는 서술이다.
다만 GSDF 명령 자체는 요구이며, 위치는 Display 메뉴이고 단계는 1b, 상태는 `(planned)` 다.

### (3) (덤) `GsdfCalibrateMenuItem` 이 비활성인 이유가 문서에 있나 → **있다**

같은 L265 행이 두 조건을 적는다: 단계 **1b**, 조건 **`xpe_display.dll ready`**, 상태 `(planned)`.
앱의 헤더 `(Phase 1b+)` 와 `IsEnabled="False"` 는 그 문서와 **일치한다.**

**판정: 비활성은 문서에 근거가 있는 의도된 상태다.** 결함이 아니다.

### 판정 요약

| 항목 | 판정 | 근거 |
|---|---|---|
| Compare Swipe / Difference | **요구다** | `GUI-CMP-FR-002` · `FR-005` · MENU-001 L120 |
| GSDF **를 View 메뉴에서** | **요구가 아니다** | MENU-001 L265 는 **Display** 메뉴를 지정 |
| GSDF 비활성 사유 | **문서에 있다** | MENU-001 L265 (1b · dll ready · planned) |

"문서에 없다" 로 분류된 항목은 **없다.**

## 2. 기능 부재 정리 (leader 이슈용) — 앱은 고치지 않았다

요구 대비 빠진 것만 추린다. **구현은 별도 카드**라는 카드 지시를 따른다.

| # | 빠진 것 | 근거 | 크기(실측 기준) |
|---|---|---|---|
| G-1 | View 메뉴에 비교 모드 항목 없음 | MENU-001 L62 | 메뉴 항목 6개 + 기존 명령 배선 |
| G-2 | 비교 단축키 F5~F8 · Ctrl+1,2 없음 (`KeyBinding` 0건) | MENU-001 §9.3 | `InputBinding` 6개 |
| G-3 | `SourceOnly` / `ProcessedOnly` 전환 UI 없음 (렌더러는 지원) | MENU-001 L120, FR-005 | 버튼 2개 |
| G-4 | `DifferenceHeatmap` 이 tint 근사 — 실제 차분 영상 아님 | 코드 주석 스스로 명시 / MENU-001 L288 | 알고리즘 변경, 범위 큼 |
| G-5 | `Display` 메뉴 자체가 없음 (GSDF 의 지정 위치) | MENU-001 L265 | 1b 단계 작업 |

**G-4 는 성격이 다르다** — 나머지는 배선이지만 이것은 출력값이 달라지는 변경이고,
"차분 영상" 을 의료영상 평가에 쓰는 순간 **근사가 결론을 바꿀 수 있다.** 별도 판단이 필요하다.

## 3. C-43 미검증 재현 시도 — **재현 못 함**

C-43 에서 Mock 실행 1회가 1건 실패했다가 재실행에서 사라졌다. 추정 원인은 "측정 중 앱 출력
폴더의 네이티브 DLL 이 바뀐 것" 이었다. **그 조건을 의도적으로 만들었다** — 스위트 시작 시
DLL 을 치우고, 4초 뒤 되돌리는 것을 3회 반복:

```
시도1: 통과!  - 실패: 0, 통과: 15, 건너뜀: 1, 전체: 16 (10 s)
시도2: 통과!  - 실패: 0, 통과: 15, 건너뜀: 1, 전체: 16 (10 s)
시도3: 통과!  - 실패: 0, 통과: 15, 건너뜀: 1, 전체: 16 (10 s)
```

**재현되지 않았다.**

카드 기준대로 **"재현 못 함" 으로 적고 넘어간다 — 통과를 안전의 증거로 쓰지 않는다.**
3회 통과가 말하는 것은 "이 조건에서 3번 안 났다" 뿐이고, 원인 가설이 맞는지 틀렸는지 어느
쪽도 증명하지 못했다. 실패한 테스트 이름을 그때 잡아 두지 못한 것이 이 조사의 상한이다.

(재현 시도는 4초 후 되돌리는 짧은 파일 이동이고 `wait` 로 회수를 보장했다 — 정리 없이
남는 부하를 만들지 않았다.)

## 4. 실측 (verbatim)

```
git merge origin/main                                                    # → e537d2c

XPE_E2E_BACKEND=Native XPE_NATIVE_DIR=<staging> dotnet test …E2ETests…   (앱 폴더 비움)
통과!  - 실패: 0, 통과: 16, 건너뜀: 0, 전체: 16 (1 m 5 s)

dotnet test clients/ImageProcTest.E2ETests/… -c Debug                    (Mock)
통과!  - 실패: 0, 통과: 15, 건너뜀: 1, 전체: 16 (10 s)

dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
통과!  - 실패: 0, 통과: 180, 건너뜀: 1, 전체: 181

dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개 / 오류 0개
```

Baseline 귀속: 전부 C-44 시점(`9f45bf0`)과 **동일한 수치**다. 이 카드는 코드를 바꾸지 않았으므로
수치가 같아야 하고, 같다.

## 5. 미검증 (Gaps)

- **비교 버튼을 실제로 눌러 보지 않았다.** `ViewportShell.xaml` 에 버튼이 있고 핸들러가
  `CompareModeButton_Click` 이라는 것까지 읽었을 뿐, **눌렀을 때 모드가 실제로 바뀌는지는
  측정하지 않았다.** C-43 이 "메뉴만 보고 결론" 을 낸 것과 같은 유형의 한계다 — 존재는
  동작이 아니다. 시나리오로 고정하려면 별도 카드가 필요하다.
- **`SourceOnly`/`ProcessedOnly` 를 전환할 다른 경로가 있는지 전수 조사하지 않았다.**
  버튼 4개만 확인했고, 설정 파일을 직접 편집하는 경로는 세지 않았다.
- **`DifferenceHeatmap` 이 근사라는 것은 코드 주석을 근거로 한다.** 렌더링 결과를 실제
  차분 영상과 비교하지 않았다 — 주석이 틀렸을 가능성은 배제하지 않았다.
- **SRS 원문을 보지 못했다.** `docs/project/` 의 GUI 계열 문서(COMPARE-001, MENU-001,
  DISP-INT-001)를 근거로 판정했다. `SPEC-XPE-MASTER.md` 는 열지 않았다 — 세 문서가 FR
  번호까지 달고 있어 충분하다고 판단했으나, **더 상위 문서가 다른 말을 할 가능성은 남는다.**
- **C-43 의 실패 1건은 끝내 무엇이었는지 모른다**(§3).

## 6. 잔여 위험 (Residual risk)

- **C-43 보고서가 이미 leader 에게 PASS 판정을 받았고 계획서 정정(`e537d2c`)의 근거가 됐다.**
  그 보고서의 "구현 불가" 가 §0 대로 부정확하므로, **계획서 정정도 그 부정확함을 물려받았을
  수 있다** — W-04/W-05 행을 "앱에 없음" 으로 정정했다면 사실과 다르다. leader 확인 필요.
- **문서가 셋으로 갈려 있다**: 계획서(§4.2)는 View, MENU-001 은 Display(GSDF)/View(비교),
  앱은 Tools(GSDF)/뷰포트 셸(비교). 어느 하나를 고쳐도 나머지 둘과 어긋난 채 남는다.
- **G-4(차분 근사)는 조용한 유형이다.** 화면에 무언가 그려지므로 "동작한다" 로 보이고,
  값이 틀렸다는 신호가 없다 — 이 레인이 반복해서 만난 형태다.
- 이 카드는 코드를 바꾸지 않았으므로 회귀 위험이 없다.

## 부록 — 사용한 명령

```bash
git merge origin/main
grep -n -i "swipe\|difference" docs/project/XPE-GUI-COMPARE-001_Large_Image_Comparison_Viewer_Spec.md
grep -n -i "gsdf\|compare\|comparison" docs/project/XPE-GUI-MENU-001_Menu_and_Command_Strategy.md
grep -rn "CompareModeOptions\|ComparisonMode" gui/ --include=*.xaml --include=*.cs | grep -v bin/
grep -rn "KeyBinding\|InputBinding" gui/ImageProcTest/MainWindow.xaml gui/ImageProcTest/Views/ViewportShell.xaml

export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.E2ETests/… -c Debug            # x3, DLL 이동과 겹쳐서
XPE_E2E_BACKEND=Native XPE_NATIVE_DIR="$(pwd)/build/ci-common/bin" dotnet test …E2ETests…
dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
```
