# GUI-C-46 — 비교 모드를 실제로 눌러 본다 (#149 #136)

> **이번 실행이 쓴 네이티브**: run **34535620953** · head `83ffa7ae…` ·
> `xpe_preprocess.dll` md5 `9f81d2f07fb7f7e23e90c86332467105`.

- 카드: GUI-C-46 · Refs #149 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui` · 병합 `e537d2c`
- 커밋 1건: `1ad0f77` — 미푸시
- **결과: Native E2E 0/23/0/23 · Mock 0/22/1/23 · 통합 0/180/1/181 · slnx 0/0**
- **G-4 는 실행으로 확정. 다만 "어느 모드가 선택됐는지" 는 E2E 로 관측 불가 — 새 발견이다.**

---

## 1. 버튼을 눌렀다 (W-11)

`ViewportShell` 의 버튼 4개(`Swipe` `Split` `Overlay` `Difference`)를 Theory 로 각각 눌렀다.
누르고 나서 **뷰포트가 살아 있는지**까지 본다 — 렌더러가 던지면 창이 죽고, "버튼이 있다" 만
확인하는 시나리오는 그래도 통과한다.

| 시나리오 | Mock | Native |
|---|---|---|
| W-11 Swipe | 402 ms | 측정됨 |
| W-11 Split | 379 ms | 측정됨 |
| W-11 Overlay | 438 ms | 측정됨 |
| W-11 Difference | 430 ms | 측정됨 |

버튼은 AutomationId 도 `x:Name` 도 없어 **표시 텍스트로** 찾는다(모드는 `Tag` 로 간다).
앱을 고치지 않는다는 카드 지시대로 우회하지 않고 #149 에 남길 사항으로 적는다.

## 2. 그런데 "어느 모드가 선택됐는지" 는 관측할 수 없다 — 후보 3곳이 모두 실패했다

카드는 "버튼 클릭 → 뷰모델 상태가 해당 모드로" 를 요구했다. **그 관측점을 찾지 못했다.**
세 곳을 시도했고 각각 다른 이유로 실패했다. **셋 다 실측이다.**

### (1) opacity 슬라이더 — **반증이 잡았다**

처음 W-12 는 "Overlay 를 누르면 슬라이더가 뜬다" 로 썼고 **통과했다.** 그런데 반증에서
`vm.Settings.ComparisonMode = mode;` 를 주석 처리하자 **스위트 전체가 녹색이었다**(23건 중 0 실패).

원인은 핸들러 자신이다:

```csharp
vm.Settings.ComparisonMode = mode;          // 검증하려던 대입
UpdateOpacitySliderVisibility(mode);        // 지역 변수로 슬라이더를 따로 설정
```

**슬라이더는 설정이 쓰이든 말든 뜬다.** 관측점이 두 번째 대입의 하류에 있었고, 첫 번째 대입에
대해 아무것도 증명하지 못했다. 통과한 반증은 실험 실패다 — 여기서 진짜 지렛대를 찾아 나섰다.

### (2) 레인 배지 — 실측 결과 **두 모드가 동일**

`ViewportShell.xaml:214,241` 의 `DataTrigger` 가 `Settings.ComparisonMode == "DifferenceHeatmap"`
일 때 배지를 접는다고 읽고, Swipe 와 Difference 의 UIA 요소 이름 집합을 비교했다.

```
PROBE swipe(84): … | A  Production v1.2 | … | LANE A — REFERENCE | …
PROBE diff (84): … | A  Production v1.2 | … | LANE A — REFERENCE | …
```

**84개로 완전히 같다.** 그 트리거는 이 스위트가 볼 수 있는 것을 움직이지 않는다.

### (3) 분리 비교 뷰어 — **창이 열리지 않았다**

`ComparisonStatus`("Mode=…, Zoom=…")를 표시하는 분리 창이 설정을 직접 읽는 유일한 경로다.
`View→Detach Comparison Viewer` 를 누르고 최상위 창을 훑었다:

```
Top-level windows: 작업 표시줄 | ImageProcTest GUI-S0 | Program Manager
```

**분리 창이 없다.** 여기서 멈췄다 — 클릭이 안 닿은 것인지 명령이 창을 안 만드는 것인지
**구분하지 못했고**, 그 구분 없이 "Detach 가 깨졌다" 고 쓸 수 없다(§5).

### 그래서 W-12 를 쓰지 않았다

관측점 없이 통과하는 시나리오를 남기면 **이 카드가 닫으려던 구멍을 그대로 되돌린다.**
세 번의 실측을 클래스 주석에 그대로 적어 다음 사람이 같은 세 곳을 다시 파지 않게 했다.

**이것이 #149 의 새 항목이다 — G-6: 비교 모드에 외부에서 읽을 수 있는 상태가 없다.**
C-43·C-45·C-46 이 세 번 연속 같은 층에서 걸린 구조적 이유가 이것이다.

## 3. G-4 — 주석이 아니라 실행으로 확정했다

컨트롤을 E2E 프로젝트에 **링크드 소스**로 붙이고(`UseWPF`), STA 스레드에서
`RenderTargetBitmap` 으로 직접 렌더링해 중앙 픽셀을 읽었다.

**실험**: 같은 이미지를 source 와 processed 에 넣는다. 참 차분이라면 **입력과 무관하게 0**이다.

```
SourceOnly        white->#565B69  black->#0B101E
DifferenceHeatmap white->#534856  black->#1E1321
```

**출력이 입력에 따라 바뀐다.** `difference(x, x) = 0` 이어야 하는데 x 가 움직이면 결과도 움직인다
→ **차분을 계산하지 않는다.** 이것이 "검정이 아니다" 보다 강한 근거인 이유: 컨트롤이 어두운
배경 위에 합성하므로 절대값은 눌려 있고, "검정이 아님" 만으로는 배경 탓으로 읽힐 수 있다.
입력 의존성은 그렇게 읽을 수 없다.

두 번째 관측 — SourceOnly 대비 델타:

```
delta vs SourceOnly: R -3, G -19, B -19
```

빨강만 덜 잃는다. **공통 바탕 위의 빨간 wash** 형태다.

### 이 측정에서 내 단언 하나가 틀렸다

첫 판은 `difference.R > difference.G && difference.R > difference.B`(절대 우세)로 썼고
`#534856` 에서 실패했다. 배경이 청회색이라 **결과도 절대값으로는 파랑 우세**다 — 모드가 아니라
배경을 재고 있었다. 델타 비교로 바꿨다. 그리고 `SourceOnly` 의 흰 입력이 `#565B69` 로 나온 것을
"이미지가 안 그려진다" 로 넘기지 않고 검정 입력(`#0B101E`)과 대조해 **그려진다는 것을 먼저
확인**했다 — 기준선이 의심스러운 채로 결론을 내지 않기 위해서다.

## 4. G-3 — 도달 경로는 있으나 그 경로로 E2E 를 붙이지 않았다

| 경로 | 실측 |
|---|---|
| UI | **없음** — `SourceOnly`/`ProcessedOnly` 는 뷰모델 옵션 배열(바인딩 0건)과 렌더러에만 있다 |
| 자동화 인자 | **없음** — `AutomationArgs.cs` 에 compare/comparison 매치 **0건** |
| 설정 파일 | **있음** — `appsettings.json:25` `"comparisonMode": "SwipeVertical"` |

**설정 파일 경로로 E2E 를 붙이지 않았다.** E2E 기동은 자동화 모드가 아니라 **출하
`appsettings.json` 을 읽고 쓴다**(C-38 §2) — 테스트가 그 파일을 편집하면 개발자 설정을
덮어쓴다. C-43 에서 W-08 을 구현하지 않은 것과 같은 이유다.

대신 **렌더러를 직접 돌려** 두 모드가 각자 이름의 레이어를 그리는 것을 확인했다:

```
source=white processed=black: SourceOnly    -> #565B69
source=white processed=black: ProcessedOnly -> #0B101E
```

카드의 질문("이 모드들이 무언가를 하는가")에 누구의 설정도 건드리지 않고 답한다.

## 5. 반증 (빌드 결과 포함)

**약화 방식**: `DifferenceHeatmap` 의 빨간 wash를 파란 wash로 바꿨다(`FromArgb(76,220,38,38)` →
`FromArgb(76,38,38,220)`). 삭제가 아니라 값 변경이므로 컴파일이 깨지지 않는다.

```
=== 빌드:
    경고 0개
    오류 0개                        ← 빌드는 정상
=== 테스트:
  실패 …ComparisonRenderObservationTests.DifferenceHeatmap_LeavesTheImageVisible_AndShiftsItTowardRed
   Expected red to be preserved relative to green and blue (a red wash),
   measured deltas R -19, G -19, B -3.
실패!  - 실패: 2, 통과: 20, 건너뜀: 1, 전체: 23
```

델타가 정확히 뒤집혔다(R -3/B -19 → R -19/B -3). **의도한 가드가 의도한 이유로 실패한다.**

**다만 2건이 실패했다.** 함께 떨어진 것은 `S01_Launch_HasMainWindow` 이며, 이 약화와 인과가
없다(그 시나리오는 창 존재만 본다). 원복 후 23건 전부 통과 — **원인을 특정하지 못했으므로
"1건만 실패" 라고 쓰지 않는다.** C-42·C-43 에서도 같은 형태를 겪었고(앱 폴더 DLL 조건),
이번에는 그 조건이 아니었다.

## 6. 실측 (verbatim)

```
XPE_E2E_BACKEND=Native XPE_NATIVE_DIR=<staging> dotnet test …E2ETests…   (앱 폴더 비움)
통과!  - 실패: 0, 통과: 23, 건너뜀: 0, 전체: 23 (1 m 55 s)

dotnet test clients/ImageProcTest.E2ETests/… -c Debug                    (Mock)
통과!  - 실패: 0, 통과: 22, 건너뜀: 1, 전체: 23 (12 s)

dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
통과!  - 실패: 0, 통과: 180, 건너뜀: 1, 전체: 181

dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개 / 오류 0개
```

Baseline 귀속: E2E 는 C-45 시점 **16건** → **23건**(W-11 4 + 렌더 3). Mock 의 건너뜀 1은
Native 전용 출처 가드(C-41). 통합 181 은 불변.

**Native 가 1 m 4 s → 1 m 55 s 로 늘었다.** 새 시나리오 7건이 붙은 만큼이며 게이트(3 min)
안이지만, 다음 카드가 더 붙이면 여유가 줄어든다.

## 7. 미검증 (Gaps)

- **어느 모드가 선택됐는지 E2E 로 확인하지 못했다**(§2). W-11 이 보장하는 것은 "눌렸고
  앱이 살아 있다" 까지다.
- **Detach 명령이 창을 만들지 않은 이유를 규명하지 못했다**(§2-3). 클릭 미도달과 명령
  무동작을 구분하지 못했으므로 "Detach 가 깨졌다" 는 주장은 하지 않는다.
- **반증에서 S-01 이 함께 실패한 원인을 특정하지 못했다**(§5).
- **렌더 관측은 중앙 픽셀 1개만 읽는다.** 화면 전체가 아니고, 64×64 한 크기·단색 두 장만
  시험했다. 실제 영상에서의 동작은 다를 수 있다.
- **`SplitLocked`/`SwipeVertical`/`SwipeHorizontal` 의 렌더링은 관측하지 않았다** — G-4 와
  G-3 에 해당하는 모드만 렌더링했다.
- **설정 파일 경로(G-3)를 실제로 타 보지 않았다**(§4).

## 8. 잔여 위험 (Residual risk)

- **G-6(외부 관측 불가)이 남는 한 이 층의 질문은 계속 이 형태로 걸린다.** 비교 모드에
  AutomationId 나 상태 표시가 붙기 전까지, 어떤 E2E 도 "모드가 적용됐다" 를 증명할 수 없다.
- **렌더 테스트가 컨트롤의 소스를 링크한다.** 그 파일이 이동·개명되면 빌드가 깨진다 —
  조용한 실패가 아니라 즉시 드러나는 쪽이다.
- **`UseWPF` 가 E2E 프로젝트의 암시적 using 집합을 바꿨다.** `System.IO` 를 명시 복구했지만,
  앞으로 이 프로젝트에 파일을 더할 때 다른 암시적 using 이 빠져 있을 수 있다.
- W-11 이 버튼을 **표시 텍스트**로 찾는다. 라벨을 바꾸거나 번역하면 깨진다 — #149 에
  AutomationId 부여가 들어가면 함께 정리할 자리다.

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.E2ETests/… --filter "…ComparisonRenderObservationTests" \
       --logger "console;verbosity=detailed"
dotnet test clients/ImageProcTest.E2ETests/… --filter "…ComparisonModeScenarios" \
       --logger "console;verbosity=detailed"
# 반증: FromArgb(76,220,38,38) -> FromArgb(76,38,38,220)
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
XPE_E2E_BACKEND=Native XPE_NATIVE_DIR="$(pwd)/build/ci-common/bin" dotnet test …E2ETests…
dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
```
