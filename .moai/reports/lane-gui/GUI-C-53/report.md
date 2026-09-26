# GUI-C-53 — DifferenceHeatmap 이 실제 차분과 얼마나 다른지 (#149 G-4)

> **이번 실행이 쓴 네이티브**: run **34535620953** · head `83ffa7ae…` ·
> `xpe_preprocess.dll` md5 `9f81d2f07fb7f7e23e90c86332467105`.

- 카드: GUI-C-53 · Refs #149 · 워크트리 `D:/workspace-github/xpe-gui` / `dev/gui`
- 커밋 1건: `1a539e2` — 미푸시
- **렌더링은 고치지 않았다.** 현행 동작을 수치로 고정하고 보고만 한다.
- **결과: Native 0/34/0/34(벽시계 75 s) · Mock 0/33/1/34 · 통합 0/180/1/181 · slnx 0/0**

---

## 1. "실제 차분" 의 정의 — 문서에 얼마나 있나

| 문서 | 인용 | 지정하는 것 |
|---|---|---|
| `XPE-GUI-MENU-001` **L288** | `` | `Difference` | \|source - processed\| 히트맵 | F8 | `` | **값**: 절댓값 픽셀 차분 |
| `XPE-GUI-COMPARE-001` **L68** | `` | `DifferenceHeatmap` | Shows **signed or absolute** pixel/display difference | … | `` | 부호를 **고정하지 않는다**(둘 다 허용) |
| 어느 문서든 | — | **색 대응(colormap)은 없다** |

`grep -i "colormap\|palette\|색상\|jet\|grayscale"` → 두 문서 모두 **0건**.

**따라서:**

- **값의 정의는 있다**(MENU-001: 절댓값). 다만 COMPARE-001 은 부호 있는 차분도 허용하므로
  **두 문서가 같은 것을 말하지 않는다.**
- **색 대응은 어느 문서도 지정하지 않는다.** 그래서 아래 참조 영상을 회색으로 표현한 것은
  **이 측정의 선택이지 요구가 아니다.**
- **"요구 위반" 이라고 단언하지 않는다**(카드 지시이자, 위 상태에서 단언할 수 없는 것이기도
  하다). 단언하는 것은 **"두 영상이 이만큼 다르다"** 와 **"문서가 무엇을 말한다"** 까지다.

## 2. 어떻게 쟀나 — 참조도 같은 렌더러를 통과시킨다

컨트롤은 자기 배경 위에 합성하고 HUD 를 그린다. 원본 비트맵과 비교하면 **배경을 재게 된다**
(C-46 에서 절대 채널 우세로 비교했다가 배경을 잰 것과 같은 함정).

그래서 참조 영상은 **`|source − processed|` 를 만든 뒤 `SourceOnly` 로 렌더링**한다 — 같은 배경,
같은 스케일, 같은 HUD. 두 렌더 사이에 남는 것은 **차분 계산뿐**이다.

## 3. 수치 (중앙 crop, 가장자리 16px 제외)

| 입력 | \|Δ\| | **최대 채널 편차** | **평균 채널 편차** |
|---|---|---|---|
| 동일 mid-grey (128,128) | 0 | **255** / 255 | 97.1 |
| 근접 (128,136) | 8 | **247** / 255 | 93.4 |
| 중간 (96,160) | 64 | **225** / 255 | 58.7 |
| 반대극단 (255,0) | 255 | **205** / 255 | 73.9 |

**작아 보이는 입력과 큰 입력을 둘 다 냈다**(카드 지시). 가장 작은 편차는 **반대극단**(205),
가장 큰 편차는 **동일 입력**(255)이다.

**편차가 입력에 따라 움직인다.** 상수 오프셋이면 보는 사람이 눈으로 보정할 수 있지만, 이건
입력에 따라 변하므로 보정할 수 없다 — 판독에 직접 영향을 주는 성질이 이것이다.

방향도 직관과 반대다: **차이가 없을 때 가장 크게 어긋나고**, 차이가 최대일 때 가장 덜 어긋난다.
참 차분이라면 동일 입력에서 검정(0)이어야 하는데 실제로는 밝은 화면이 그려지기 때문이다
(C-46 이 `#534856` 로 관측한 그 값).

### crop 에 대한 자기 정정

crop 은 **`DifferenceHeatmap` 만 그리는 "Difference heatmap preview" 라벨**과 좌상단 HUD 판
때문에 넣었다. "최댓값이 라벨일 것" 이라고 의심했고, **재 보니 틀렸다**:

| | 전체 프레임 | 중앙 crop |
|---|---|---|
| 최댓값 (4쌍) | 255 · 247 · 225 · 205 | **255 · 247 · 225 · 205 (동일)** |
| 평균 (동일 mid-grey) | 102.9 | 97.1 |
| 평균 (반대극단) | 93.9 | 73.9 |

**최댓값은 하나도 움직이지 않았다** — 최대 편차는 chrome 이 아니라 영상 내용이다.
crop 이 걷어낸 것은 평균의 작은 오염뿐이다. 의심은 합리적이었고 결론은 틀렸으며, 코드 주석도
그 실측대로 고쳤다.

## 4. 반증 (빌드 결과 포함)

렌더러의 붉은 wash 를 제거하고 processed 를 전체 불투명으로 그리도록 **약화**했다.

```
=== 빌드: 경고 0개 / 오류 0개
반대극단 (255,0): max 205 → 255,  mean 73.9 → 138.9
동일 mid-grey    : mean 97.1 → 101.2
중간 (96,160)    : mean 58.7 → 79.3
```

**수치가 렌더러 변경에 반응한다** — 이 측정이 배경이나 상수가 아니라 **렌더러를 재고 있다**는
증거다.

**주의해서 적는다**: 이 약화는 차분 구현이 아니라 wash 제거일 뿐이고, 수치는 **더 나빠졌다**.
반증이 보인 것은 **민감도**이지 "wash 가 원인의 전부" 가 아니다. 원복 후 재확인.

## 5. 실측 (verbatim)

```
dotnet test …E2ETests… --filter "…DifferenceMagnitudeTests" --logger "console;verbosity=detailed"
identical mid-grey:       source=128 processed=128 |Δ|=0   -> max 255/255, mean 97.1/255
near-identical (Δ8):      source=128 processed=136 |Δ|=8   -> max 247/255, mean 93.4/255
moderate (Δ64):           source=96  processed=160 |Δ|=64  -> max 225/255, mean 58.7/255
opposite extremes (Δ255): source=255 processed=0   |Δ|=255 -> max 205/255, mean 73.9/255
smallest gap: opposite extremes (205) · largest: identical mid-grey (255)
통과!  - 실패: 0, 통과: 5, 건너뜀: 0, 전체: 5

XPE_E2E_BACKEND=Native XPE_NATIVE_DIR=<staging> dotnet test …E2ETests… --no-build
통과!  - 실패: 0, 통과: 34, 건너뜀: 0, 전체: 34 (기간 1 m 12 s / 벽시계 75 s)

dotnet test clients/ImageProcTest.E2ETests/… --no-build                        (Mock)
통과!  - 실패: 0, 통과: 33, 건너뜀: 1, 전체: 34 (16 s)

dotnet test clients/ImageProcTest.IntegrationTests/… -c Debug
통과!  - 실패: 0, 통과: 180, 건너뜀: 1, 전체: 181

dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
    경고 0개 / 오류 0개
```

Baseline 귀속: E2E 는 C-52 시점 **29건** → **34건**(측정 5건). 통합 181 불변.
Native 벽시계 75 s 는 C-48~52 의 69~72 s 보다 **3~6초 길다** — 새 렌더 5건 몫이며,
게이트 180 s 대비 여유 **105 s**.

## 6. 미검증 (Gaps)

- **단색 입력만 썼다.** 실제 의료영상처럼 구조가 있는 입력에서는 수치가 다를 수 있다 —
  특히 국소적으로 차이가 큰 영역이 있는 경우.
- **64×64 한 크기만** 쟀다. 스케일링이 다른 큰 영상에서의 편차는 모른다.
- **회색 참조는 이 측정의 선택이다**(§1). 색 대응이 문서화되면 그 기준으로 다시 재야 하고,
  그때 수치가 바뀔 수 있다.
- **부호 있는 차분으로는 재지 않았다.** COMPARE-001 이 허용하지만 MENU-001 의 절댓값 정의를
  기준으로 했고, 부호 기준의 편차는 별도 값이다.
- **"판독에 영향을 준다" 를 실제 판독으로 확인하지 않았다.** 수치가 크다는 것까지가 이 측정이고,
  사람이 그 화면으로 무엇을 잘못 읽는지는 재지 않았다.
- **두 문서의 불일치(절댓값 vs signed-or-absolute)를 leader 에게 보고할 뿐 판정하지 않았다.**

## 7. 잔여 위험 (Residual risk)

- **수치가 현행 동작을 고정한다.** 렌더링이 바뀌면 이 테스트가 실패하는데, 그것은 **의도한
  신호**다 — 화면 출력을 바꾸는 결정이 조용히 지나가지 않는다. 다만 실패했을 때 "고정값을
  갱신" 으로 처리하면 그 신호가 사라진다.
- **최대 편차 255 는 채널 상한이라 더 커질 수 없다.** 다른 입력에서 더 심한 상태가 있어도
  이 지표로는 구분되지 않는다 — 평균이 그 구분을 일부 해 준다.
- **G-4 의 결론은 여전히 "얼마나 다른가" 까지다.** 고칠지, 문서를 고칠지, 그대로 둘지는
  #154·#155·#156 선례대로 별도 결정이다.

## 부록 — 사용한 명령

```bash
grep -n -i "difference\|heatmap" docs/project/XPE-GUI-MENU-001_Menu_and_Command_Strategy.md
grep -n -i "difference\|signed\|absolute\|colormap" \
     docs/project/XPE-GUI-COMPARE-001_Large_Image_Comparison_Viewer_Spec.md
export PATH="/c/Program Files/dotnet:$PATH"
dotnet test clients/ImageProcTest.E2ETests/… --no-build \
  --filter "FullyQualifiedName~DifferenceMagnitudeTests" --logger "console;verbosity=detailed"
# 반증: ImageComparisonViewport.cs 의 DifferenceHeatmap wash 제거 후 재측정
dotnet build clients/ImageProcTest.slnx -c Debug -t:Rebuild
```
