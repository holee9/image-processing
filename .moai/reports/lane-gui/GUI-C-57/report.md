# GUI-C-57 — 차분 히트맵을 진짜 차분으로 (#149 G-4)

> **이번 실행이 쓴 네이티브**: run **34535620953** · head `83ffa7ae…` ·
> `xpe_preprocess.dll` md5 `9f81d2f07fb7f7e23e90c86332467105`.

- 카드: GUI-C-57 · Refs #149 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 1건 — 미푸시
- **세 성질 전부 목표 도달 · 성질별 반증이 각각 따로 터진다**
- **결과: Native 3회 0 실패 · Mock 0/48/1/49 · 통합 0/180/1/181 · slnx 0경고 0오류**

---

## 1. 주장 (Claim)

`DifferenceHeatmap` 이 **`|source − processed|` 를 선형 회색조(0 = 검정)로** 그린다.

| # | 성질 | 고치기 전 | **고친 뒤** |
|---|---|---|---|
| P1 | 동일 입력 → 단일 색 | 편차 **255**/255 | **0**/255 (4쌍 전부) |
| P2 | 부호 무관 | 간격 **38.0** | **0.0** |
| P3 | 국소 분리도 보존 | 참 차분의 **37 %** | **99.7 %** (63.6 vs 63.8) |

곁가지로 **Δ8 의 신호/바닥이 1.61x → 18.70x**(바닥 3.3 → 0.4). C-54 가 "묻히지는 않지만
여유가 거의 없다" 고 적은 상태가 해소됐다.

**반증은 성질별로 따로 터진다** — 셋이 같은 것을 보고 있지 않다:

| 주입 | P1 단언 | P2 단언 | P3 단언 |
|---|---|---|---|
| 입력 의존 오프셋(`+lhs/4`) | **실패** | 통과 | 통과 |
| 부호 보존 — 순수형(한쪽 부호만 1/2) | 통과 | **실패** | 통과 |
| 상수배 압축(`/3`) | 통과 | 통과 | **실패** |

## 2. 증거 (Evidence)

### (a) 구현

`ImageComparisonViewport` 의 `DifferenceHeatmap` 분기에서 **source 위에 processed 를 42 %
불투명도로 합성하고 붉은 판을 덮던 것**을 지우고, 두 층의 픽셀별 절대차를 구해 회색 비트맵으로
그린다. 회색값은 **세 채널 차이 중 최댓값** — 회색조 입력에서는 채널별 절대차와 같고, 한 채널만
바뀐 입력도 크기를 잃지 않으며, 출력이 항상 회색이라 리더가 정한 색 대응을 지킨다.

두 층의 크기가 다르거나 비트맵이 아니면 `null` 을 돌려주고 호출부가 source 를 그린다 —
**검정을 그리면 "차이 없음" 으로 읽히기 때문**이다. 결과는 두 원본 참조를 열쇠로 캐시한다
(`OnRender` 는 팬·줌·리사이즈마다 돈다).

### (b) 실측 (verbatim)

```
identical mid-grey:       source=128 processed=128 |Δ|=0   -> max channel deviation 0/255, mean 0.0/255
near-identical (Δ8):      source=128 processed=136 |Δ|=8   -> max channel deviation 0/255, mean 0.0/255
moderate (Δ64):           source=96  processed=160 |Δ|=64  -> max channel deviation 0/255, mean 0.0/255
opposite extremes (Δ255): source=255 processed=0   |Δ|=255 -> max channel deviation 0/255, mean 0.0/255
largest deviation: identical mid-grey (0/255)

identical white input -> #0B101E (A=255); black image -> #0B101E (A=255)
DifferenceHeatmap white->#0B101E  black->#0B101E      (SourceOnly white->#565B69 black->#0B101E)

(4) reference path: +64 -> 63.8/255, -64 -> 63.8/255 (gap 0.0)
(4) heatmap path:   +64 -> 63.6/255, -64 -> 63.6/255 (gap 0.0)
(1) identical inputs: separation -0.4/255
(2) reference Δ255: separation 254.8/255 = 99.9% of the range
(3) aligned 126.6/255 · patch moved 64px off the mask -3.2/255

P3 Δ64 patch 32px: heatmap 63.6/255 · true difference 63.8/255 · ratio 99.7% (GUI-C-54 measured 37 %)
Δ8 (961px located): heatmap 7.6/255 · true 7.8/255 · floor 0.4/255 · signal-to-floor 18.70x
patch 4/8/16/32/64px (Δ255): heatmap 126.6 · true 126.8/126.7  (모든 크기에서 동일)
gradient background, Δ64: heatmap 63.6/255, true 63.8/255
```

### (c) 반증 4회 — 각 주입마다 빌드 0오류 확인 후 실행

```
[P1]      실패 NoPairDeviates_FromAPerPixelDifference
[P1]      실패 DifferenceHeatmap_OfAnImageWithItself_IsOneColour
[P1]      실패 DifferenceHeatmap_OfIdenticalInputs_IsBlack
[P1] 실패!  - 실패: 3, 통과: 2, 전체: 5

[P2-1차]  실패 LocalSeparation_MatchesTheTrueDifference      ← 과도한 손상, §4
[P2-1차]  실패 NoPairDeviates_FromAPerPixelDifference
[P2-1차]  실패 FlippedSign_IsTheRenderersDoing_NotTheMetrics
[P2-1차] 실패!  - 실패: 3, 통과: 2, 전체: 5

[P2-순수] 실패 FlippedSign_IsTheRenderersDoing_NotTheMetrics
[P2-순수] 실패 NoPairDeviates_FromAPerPixelDifference
[P2-순수] 실패!  - 실패: 2, 통과: 3, 전체: 5

[P3]      실패 LocalSeparation_MatchesTheTrueDifference
[P3]      실패 NoPairDeviates_FromAPerPixelDifference
[P3] 실패!  - 실패: 2, 통과: 3, 전체: 5
```

### (d) 최종 검증

```
dotnet test …E2ETests… --no-build                     (Mock)
통과!  - 실패: 0, 통과: 48, 건너뜀: 1, 전체: 49 (16 s)

dotnet test …IntegrationTests… -c Debug
통과!  - 실패: 0, 통과: 180, 건너뜀: 1, 전체: 181

Repeat-E2E.ps1 -Times 3 -Backend Native → runs with a failure: 0 / 3
run 1 57.6s · run 2 58.7s · run 3 58.4s

dotnet build clients/ImageProcTest.slnx -c Debug
    경고 0개 / 오류 0개
```

## 3. Baseline 귀속

- E2E 는 C-56 시점 **48건** → **49건**(P3 단언 1건 추가). 통합 181 불변.
- 고치기 전 수치(255 / 38.0 / 37 %)는 C-53·C-54·C-55 보고서의 값이고, 이 카드에서 같은
  harness 로 다시 확인한 뒤 갱신했다.
- Native 벽시계가 73 s → 58 s 로 줄었는데 **이 카드의 변경 때문이라고 말하지 않는다** —
  기계 부하를 통제하지 않았다.

## 4. 이 카드가 고친 측정 쪽 문제 3건

**고치기 전 수치를 그대로 믿으면 안 되는 이유가 세 개 나왔다.** 전부 이번에 발견했다.

### (1) C-55 의 "harness 추출" 은 절반만 이뤄져 있었다

C-55 는 지표를 `ComparisonRenderHarness` 로 빼고 "C-54 측정과 반증이 같은 코드를 쓴다" 고
적었다. **실제로는 C-54 의 `DifferenceLocalContrastTests` 가 자기 복사본을 그대로 들고 있었다**
(알고리즘은 동일했다 — 대조해 확인했다). 이번에 그 파일을 harness 로 옮겨 **이제 정말 같은
코드**를 쓴다. C-55 의 주장은 그 카드의 파일에 대해서만 참이었다.

### (2) 크롬이 배경 평균에 섞여 있었다 (harness 수정)

마스크는 `SourceOnly` 참조 렌더에서 찾는데, **차분 모드만 그리는 하단 라벨은 참조에 없다.**
그래서 라벨이 늘 **배경 마스크**에 들어가 배경 평균을 올리고 있었다. 히트맵이 중간 회색을
합성하던 시절엔 묻혔지만, 진짜 차분은 안 바뀐 영상을 검정으로 그리므로 **라벨이 배경에서 가장
밝은 것**이 된다. 실측: 동일 입력 바닥이 크롬 포함 **−5.6**, 제외 **0.0**. HUD 판과 라벨 영역을
배경에서 뺐다.

### (3) Δ255 참조는 입력에 없는 차이를 기준으로 삼고 있었다

배경 128 에 +255 패치는 255 로 **클램프**되므로 두 영상의 실제 차이는 **127** 이다. C-54 는
참조를 요청한 Δ(255)로 만들어 **참 차분 분리도를 251** 로 보고했다. 그 행들의 비율(16 %)은
**과소 보고**다. 이번엔 참조를 실제로 실린 차이로 만든다(126.8).

## 5. 미검증 (Gaps)

- **실제 해부 구조 영상은 쓰지 않았다.** 합성 패치·기울기뿐이다(#151 자리).
- **색 입력에서 재지 않았다.** 채널 최댓값 규칙은 회색조 입력에서 채널별 절대차와 같다는 것만
  확인했고, 유채색에서의 거동은 안 쟀다.
- **확대/축소 상태에서 재지 않았다.** fit 한 조건, 256×256 한 크기다.
- **크기가 다른 두 층의 대체 경로(source 를 그림)는 단언하지 않았다.** 코드로만 있다.
- **캐시의 효과를 재지 않았다.** 팬/줌 반복에서 재계산이 안 일어나는지 실측하지 않았다.
- **P1 의 넓은 단언(`NoPairDeviates`)은 P2·P3 위반도 잡는다.** 성질을 가르는 것은 §1 표의 세
  전용 단언이고, 이 단언은 의도적으로 더 강하다.
- **사람이 보기에 나아졌는지는 재지 않았다.** 숫자까지가 이 카드다.

## 6. 잔여 위험 (Residual risk)

- **`|source − processed|` 는 부호를 지운다.** 무엇이 밝아지고 무엇이 어두워졌는지는 이 모드로
  알 수 없다 — MENU-001 이 정한 값이고 리더 결정이지만, 읽는 사람에게는 정보가 준 것이다.
- **차분이 매 렌더가 아니라 참조가 바뀔 때 계산된다.** 비트맵 내용이 제자리에서 바뀌는(같은
  참조, 다른 픽셀) 경로가 생기면 캐시가 낡는다. 현재 그런 경로는 없다.
- **하단 라벨 문구를 바꿨다**("Difference heatmap preview" → "Difference |source - processed|").
  문구를 단언하는 테스트는 없지만, 스크린샷 비교를 하는 곳이 있으면 걸린다.
- **harness 의 크롬 제외 사각형은 넉넉하게 잡았다.** 배경 표본을 몇 개 더 버리는 쪽으로
  틀렸고, 컨트롤의 HUD 배치가 크게 바뀌면 다시 봐야 한다.

## 부록 — 사용한 명령

```bash
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.E2ETests/… --no-build \
  --filter "Category=Rendering" --logger "console;verbosity=detailed"
dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
powershell -File clients/ImageProcTest.E2ETests/Staging/Repeat-E2E.ps1 \
  -Times 3 -Backend Native -NativeDir build/ci-common/bin -ResultsDirectory build/e2e-c57
dotnet build clients/ImageProcTest.slnx -c Debug
```
