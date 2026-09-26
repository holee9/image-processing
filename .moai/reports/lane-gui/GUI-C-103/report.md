# GUI-C-103 (#180) — 무엇을 재고 있었는지 맞추고, "GUI 공통 경로 2.0 초" 를 분해한다

## 0. 먼저: GUI-C-102 의 핵심 수치를 정정합니다

C-102 에서 "GUI 몫 2080~2415 ms" 라고 보고했습니다. **틀렸습니다.** 그 2 초는 앱이 아니라 **제 시험 도구**였습니다.

| 항목 | C-102 보고 | C-103 실측 |
|---|---|---|
| 화면 갱신까지(바깥 측정) | 2080~2515 ms | 2071~2487 ms (변함없음) |
| 그중 앱이 쓴 시간 | "GUI 공통 경로 2.0 초" | **16~202 ms** |
| 그중 UI 자동화가 쓴 시간 | 계상하지 않음 | **1941~2371 ms** |

C-102 의 잘못은 "전체 − 단계 = GUI 몫" 이라는 뺄셈을 **검산 없이** GUI 에 귀속시킨 것입니다. 남은 것을 앱에 돌리는 계산은, 측정 도구의 비용이 0 일 때만 성립합니다. 그 전제를 확인하지 않았습니다.

**단계 시간(27~116 ms)과 기본값 민감도(Δ0.279 / 0.534) 는 앱 안에서 잰 값이라 그대로 유효합니다.** 영향을 받는 것은 "GUI 몫" 한 줄뿐입니다.

## 1. 주장

| # | 주장 |
|---|---|
| C1 | 화면 갱신까지 2.4 초 중 **1941~2371 ms 는 적용 버튼을 누르는 UI 자동화 호출**이다. 앱이 일을 시작하기도 전이다 |
| C2 | 앱이 새 이미지를 받아 **프레임을 그리는 데는 1.7~3.1 ms** 다 |
| C3 | 앱의 처리 전체(연쇄 + 디스플레이 + 미리보기 비트맵)는 **16~202 ms** 이며, 네 구간으로 나뉜다 |
| C4 | 가상 그리드의 post 대비 차이(488 → 27 ms)는 **`vg_pyramid_levels` 미설정**으로 설명된다. GUI 는 보내지 않아 기본 0 이고, 그때 `PyramidContrast` 가 아예 실행되지 않는다 |
| C5 | 격자 억제의 차이(493 → 102 ms)는 **설명하지 못한다.** 후보는 입력 영상 차이지만 크기를 재지 않았다 |
| C6 | 두 모드 모두 조기 종료가 아니다 — 모듈이 실제로 적용했다(`grid=1` / `virtualGrid=1`) |
| C7 | 비네팅을 켜는 경로는 GUI 에 **없다**. 모듈에는 있다(설정 + 이득 맵 둘 다 필요) |
| C8 | 스테이징 재생성은 **막혀 있다** — `-17` 이 들어간 뒤 main 에서 성공한 CI 실행이 아직 없다 |

## 2. 증거

### C1~C3 — 2.4 초의 분해 (P-07, Native, 3072² 프레임)

```
P07 outside total=2487 ms
P07 inside work=16 ms; vm=0 ms; display: marshal-in=3 ms, native=3 ms, marshal-out=1 ms, preview=10 ms
P07 apply invoke (UI automation, before the app does anything) = 2371 ms
P07 in-app render = 1.7 ms (viewport handed a new image -> frame drawn)
P07 poll cycle cost = 159.6 ms (of which 20 ms is the deliberate sleep)
```

읽는 법:

- **`apply invoke` 2371 ms** — 시험이 적용 버튼을 UI 자동화로 찾아 호출하는 데 쓴 시간입니다. 앱은 아직 아무것도 하지 않았습니다. 바깥 총시간의 95 % 입니다.
- **`work` 16 ms** — 배경 작업(연쇄 + 디스플레이 파이프라인) 전체. 그 안이 `marshal-in 3 / native 3 / marshal-out 1 / preview 10 ms` 입니다. **가장 큰 몫은 미리보기 비트맵 생성(preview)** 입니다.
- **`vm` 0 ms** — 뷰모델의 UI 스레드 몫.
- **`in-app render` 1.7 ms** — 뷰포트가 새 이미지를 받은 시점부터 그 프레임을 다 그릴 때까지. 앱 안에서 쟀습니다.
- **`poll cycle` 160 ms** — 완료를 감지하는 한 바퀴 비용. 이 때문에 바깥 측정의 **분해능 자체가 160 ms** 입니다.

앱 기준 "적용에서 화면까지" 는 대략 **20~210 ms** 입니다. 3072² 한 장으로는 REQ-GSVG-019 의 1.0 s 안에 있습니다 — 다만 §4 대로 반복이 적습니다.

측정 방법: `work`·`vm` 은 뷰모델에서 `Stopwatch`, 네 구간은 `RealXpeBackend.ApplyDisplayPipelineCore` 안에서 구간별 `Stopwatch`, 렌더는 뷰포트에서 `ProcessedImage` 변경 시각과 `OnRender` 완료 시각의 차. 값은 자동화 보고서 `processingChain.pipelineTimings` 와 피어의 `renderMs=` 로 나가고, P-07 이 그것을 읽습니다.

기기·반복: 개발 기기 1 대, P-07 4회 실행. `work` 가 16 / 21 / 24 / 202 ms 로 흔들립니다(§4).

**줄일 수 있어 보이는 것 — 목록만 적습니다(이번 카드에서 고치지 않음):**
1. `preview` (10~16 ms): 미리보기 비트맵을 매번 새로 만듭니다. 크기가 같으면 `WriteableBitmap` 재사용이 가능합니다.
2. `marshal-in` (3 ms): `ushort[] → float[]` 중간 배열을 만든 뒤 복사합니다. 고정 버퍼에 직접 쓰면 한 번 줄어듭니다.
3. 시험 쪽: `apply invoke` 2.4 초가 실측의 95 % 입니다. 성능 측정은 UI 자동화를 통과하지 않는 경로에서 재야 합니다.

### C4~C6 — post 493/488 ms 와 GUI 102/27 ms 의 대조표

post 원본: `xpe-post/.moai/reports/lane-post/QA-B-105/_scale.log`
(`suppression/threads=0 … med=493103 us`, `virtualgrid/threads=0 … med=487631 us`)

| 항목 | post (QA-B-105) | GUI (P-01/P-06) | 같은가 |
|---|---|---|---|
| 영상 크기 | 3072×3072 | 3072×3072 | 같음 |
| 기기 | 같은 개발 기기 | 같은 개발 기기 | 같음 |
| 빌드 구성 | RelWithDebInfo (`build/ci-post` CMakeCache) | RelWithDebInfo (CI 파이프라인) | 같음 |
| 스레드 | `threads=0` (자동) | 설정 안 함 → 자동 | 같음 |
| 진입점 | `xpe_gsvg_process` | `xpe_gsvg_process_ex` | 다름(같은 코어) |
| 재는 구간 | `process` 호출만, 7회 중앙값 | **init + process_ex + shutdown**, 1회 | GUI 가 **더 넓다** |
| 억제 설정 | `{"grid_suppression": true}` | 같음 | 같음 |
| 억제 입력 | 합성: 해부 배경 + 103 lpi 격자, pitch 0.139 | 실제 픽스처 `wrist_lat_3072x3072.raw` | **다름** |
| 가상 표 | 합성 표 `virtual_grid_synthetic_table.csv` | 제품 표 `vg_table_water_csi600_victre.csv` | **다름** |
| 가상 kVp / ratio / pitch | 80 / 10 / 0.139 | 70 / 10 / 0.14 | 다름(작음) |
| 가상 공기신호 / 반복 | 60000 / 3 | 60000 / 3 | 같음 |
| **가상 피라미드** | `vg_pyramid_levels=6`, `gain=1.3`, `denoise_k=2` | **키를 보내지 않음 → 기본 0** | **다름(큼)** |
| 가상 입력 | 모델로 직접 만든 산란 영상 | 실제 픽스처 | **다름** |

**가상 그리드 — 설명됩니다.** `virtual_grid.h:114` 가 `int pyramidLevels = 0;` 이고 주석이 "pyramidLevels == 0 means neither step runs" 입니다. `virtual_grid.cpp:899` 는 `if (st.pyramidLevels) PyramidContrast(out, width, height, st.pyramidLevels, st.pyramidGain, st.denoiseK);` 입니다. GUI 는 키를 보내지 않으므로 **라플라시안 피라미드 6단계 + 잡음 억제가 통째로 실행되지 않습니다.** post 는 실행합니다. 두 측정은 **다른 작업량**을 재고 있었습니다.
다만 그 단계가 450 ms 중 **정확히 얼마인지는 재지 않았습니다**(§4).

**격자 억제 — 설명하지 못합니다.** 설정도 진입점도 같고, 두 실행 모두 실제로 적용했습니다:

```
P06 GsvgModeGridSuppression stage=100 ms status=Applied
P06 GsvgModeGridSuppression reason=GSVG applied: reason=Applied (0), vignette=0, grid=1, virtualGrid=0, restoredOriginal=0, code=0
```

즉 **조기 종료가 아닙니다** — 처음 세웠던 "손목 영상에 격자가 없어 빨리 끝났다" 는 가설은 이 관측으로 **반증되었습니다.**
남은 후보는 입력 영상 차이입니다: `grid_dwt.cpp:485` 부근에서 DWT 하강은 검출이 없는 첫 레벨에서 멈추고(auto-stop), 필터링한 레벨 수는 영상에 따라 달라집니다. 첫 레벨이 가장 비싸므로 하강 깊이가 다르면 시간이 크게 달라집니다. **그러나 두 영상의 하강 깊이를 재지 않았으므로, 이것은 후보이지 설명이 아닙니다.**

측정 창은 차이를 **반대 방향으로** 만듭니다: GUI 숫자는 `init`·`shutdown` 까지 포함하는데도 더 작습니다. 창 때문에 작아 보이는 것이 아닙니다.

### C7 — 비네팅 대조군

`gsvg_api.h:143` — "Optional vignette gain map, width*height float32 entries. **NULL disables the vignette step regardless of config.**"
`gsvg.cpp:331` — `h->vignette_enabled = json_get_bool(configJsonOrNull, "vignette_correction", false);`

켜려면 **둘 다** 필요합니다: 설정 `"vignette_correction": true` 와 NULL 이 아닌 이득 맵. GUI 는 둘 다 주지 않습니다(`GuiGsvgRunner.cs:120` 이 `null, 0, null, 0` 으로 호출). **GUI 에 이득 맵의 출처가 없으므로 대조군을 만들 수 없습니다.** 이 사실을 P-05 의 주석에 적었습니다 — 그 단언은 "꺼져 있다" 와 "그 칸이 아예 안 써진다" 를 구별하지 못하는 약한 단언이라고 명시했습니다.

### C8 — 스테이징 재생성 (막힘)

```
gh run list --branch main --workflow "XPE CI Pipeline" --status success
35277554935 cedce22   /  35275125717 c72f0e8   /  35229666116 f3b3830
git show cedce22:modules/common/src/xpe_common.cpp | grep -c INVALID_CALIB_DATA  →  0
```

`-17` 문자열은 `4377b90` 에서 들어왔고, 그 이후 main 의 CI 파이프라인 실행은 `b890fac` 취소(더 새 푸시가 덮음), `79c5814` 실패, `57bfd33` 실패입니다. **성공한 실행이 없어 최신 산출물을 받을 수 없습니다.** 따라서 `xpe_error_string(-17)` 시험은 이번에도 실패 상태이며, 원인은 여전히 낡은 바이너리로 봅니다 — 그러나 **새 DLL 로 통과하는지는 확인하지 못했습니다.**

### 이번 카드가 잡은 회귀 (별도 커밋 `198855b`, 병합 `59200e7`)

`ViewStateRenderScenarios` 5건이 main CI 와 로컬 Mock 양쪽에서 실패했습니다. 리더는 "CI 환경 차이" 로 보았고 저도 C-102 에서 "Mock 통과" 라고 적었지만, **둘 다 틀렸습니다.** 제 C-102 변경이 만든 회귀입니다: 판독 정규식이 문자열 끝에 고정되고 뒤따르는 필드를 `processed` 하나만 허용했는데, 제가 `processedMean`·`hud` 를(C-103 은 `renderMs` 를) 덧붙이면서 일치가 깨졌습니다. `Read` 가 null 을 돌려주어 다섯 건이 대기 초과로 끝났고, **실패 메시지가 인용한 값은 정작 옳았습니다** — 그래서 환경 문제처럼 보였습니다.

제 C-102 "Mock 통과" 보고가 틀렸던 이유: 그 실행이 `GsvgLargeFrame`·`ProcessingChain` 두 묶음만 돌렸습니다. 전체를 돌리지 않고 "Mock 통과" 라고 쓴 것이 잘못입니다.

### 빌드·시험

```
BUILD_EXIT=0
Native  GsvgLargeFrameScenarios : 총 10,  통과 10           (build/e2e-c102/final_native.txt)
Mock    E2E 전체                 : 총 124, 통과 107, 건너뜀 17, 실패 0 (build/e2e-c102/mock_all.txt)
```

통합시험의 `xpe_error_string(-17)` 실패는 C8 대로 남아 있습니다.

## 3. 기준 귀속

- GUI 수치는 모두 이번 실행에서 `build/ci-c72f0e8/bin` (CI run 35275125717) 의 DLL 로 측정했습니다. 로컬 네이티브 빌드 없음.
- post 수치는 `xpe-post/.moai/reports/lane-post/QA-B-105/_scale.log` 의 원문에서 읽었습니다(요약이 아니라 로그).
- 대조표의 post 설정은 `modules/gsvg/tests/test_grid_suppression.cpp:374` 와 `test_virtual_grid.cpp:785` 의 **시험 소스**에서 읽었습니다. post 가 실제로 돌린 임시 시험(`TmpB105Scale`)은 커밋되지 않아 읽지 못했습니다 — 설정이 같다고 가정했습니다(§4).

## 4. 미검증

- **`TmpB105Scale` 의 실제 설정.** 커밋되지 않아 읽지 못했고, 커밋된 벤치와 같다고 **가정**했습니다. 다르면 대조표의 post 열이 틀립니다.
- **피라미드 단계가 450 ms 중 얼마인지.** 코드로 "안 돈다" 까지만 보였고 크기는 재지 않았습니다.
- **격자 억제 차이의 원인.** DWT 하강 깊이가 후보이지만 두 영상의 깊이를 재지 않았습니다.
- **`work` 가 16~202 ms 로 흔들리는 이유.** 4회로는 가르지 못했습니다(GC, 첫 접촉, 스케줄링 모두 후보).
- **`renderMs` 가 합성까지 포함하는지.** `OnRender` 반환까지만 재므로, WPF 합성·표시 지연은 들어 있지 않습니다. 화면에 실제로 빛이 바뀌는 시점은 이보다 뒤입니다.
- **새 DLL 로 `-17` 이 통과하는지**(C8).
- **비네팅을 켰을 때의 동작** — 대조군을 만들 수 없어 여전히 미검증입니다.

## 5. 잔여 위험

- 이 분해는 개발 기기 1대·소수 반복입니다. **성능 게이트를 이 숫자로 잡지 마십시오** — 리더 판단(C-102 §5)을 그대로 유지합니다.
- `apply invoke` 가 실측의 95 % 라는 것은, 현재 E2E 로는 앱의 성능 변화를 감지할 수 없다는 뜻이기도 합니다. 앱이 2 배 느려져도 바깥 총시간은 거의 안 움직입니다.
- 정정된 수치가 이전 보고(#180 C-102 코멘트)에 남아 있습니다. 이 보고서와 이번 코멘트가 정정본입니다.

## Card Cross-Check

| 이정표 | card |
|---|---|
| 측정 대상 대조표 (1번) | GUI-C-103 |
| 2.0 초 분해 (2번) | GUI-C-103 |
| 비네팅 대조군 확인 (3번) | GUI-C-103 |
| 스테이징 재생성 (4번) | GUI-C-103 — **막힘**, CI 초록 후 재시도 |
| ViewState 판독 회귀 수정 | GUI-C-103 (커밋 `198855b`) |
| 피라미드 단계의 비용 측정 | 새 카드 필요 |
| 격자 억제 하강 깊이 대조 | 새 카드 필요 |
| 미리보기 비트맵 재사용 | 새 카드 필요 |
