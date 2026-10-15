# QA-B-214b — Codex #167: 자동 창 API 경계 2건 보고서

작성: Lane B (xpe-post), `dev/postprocess`. 제품 코드 변경, Codex 재검토 대상. 커밋은 이 보고서와 함께 올라가는 하나(해시는 보고 메시지).
Codex #167 의 post 몫 둘(발견 3: 1/16 표본이 해부를 놓침, 발견 4: ±FLT_MAX 에서 무한 폭 창을 성공으로 반환)을 고쳤다. 발견 1·2(앱 연결, 골든)는 gui 의 GUI-C-233, 발견 5(MONOCHROME1 극성 계약)는 DICOM 단계로 미룬다(리더 기록).

## 결론

| 항목 | 상태 |
|---|---|
| 발견 3 — 표본이 해부를 놓치는 영상 | 고침. 작은 영상은 전체 화소 히스토그램, 큰 영상은 표본이 대표성 없거나 대체 창이면 전체로 다시 계산. 대체 창은 실제 전체 영상 분위수 |
| 발견 4 — 무한 폭 창 | 고침. float 로 못 담는 창은 `XPE_ERR_INVALID_INPUT` + 출력 구조체 불변 |
| 요구 문구 초안 | `draft_1_…_v2.txt`(whole-image 1..99% 를 실제 동작에 맞게), `draft_2_…_v2.txt`("default window" 문장은 GUI-C-233 병합 전에는 쓰지 않는다고 표시) |
| 시험 | 자동 창 시험 17개(기존 9 + 신규 8), 표시 시험 전체 154개 통과, 기존 E2E 12개 통과, doxygen 오류 0 |
| 반증 | 4개 arm 모두 의도한 시험이 빨강, 복원 후 초록 |
| 회귀 | P6 누수 기울기 +0.00073 MiB/프레임, 실영상 창 `[1981.37, 3180.07]`(214 의 `[1981.34, 3179.77]` 과 0.3 로그단위 차이) |

**먼저 알려야 할 것**
1. **시간이 늘었다**: 대표성 있는 실영상 경로가 214 의 2.8–3.1 ms 에서 **3.7–4.0 ms** 로 약 1 ms 늘었다(같은 하니스에서 214 커밋 코드와 번갈아 빌드해 잰 값, `timing_head_214.txt` 대 `timing_working_214b.txt`). 표본이 대표성 없어 전체 히스토그램으로 가는 경로는 **11.5 ms**(여전히 16 ms 안). 1000×1000 은 0.35 → 1.2 ms.
2. **처음 반증 arm 셋이 빨개지지 않았다**: 두 안전장치(표본 대표성 검사, 대체 창을 전체에서 확인)가 서로를 덮었기 때문이다. 시험을 변별력 있게 고친 뒤에야 각 arm 이 빨개졌다(5절).
3. 표본 대표성 검사는 한 영상으로 정한 상수(0.2%)에 기대지만, 실영상 11장의 표본 밖 화소는 0.0001–0.0004% 라 500배 여유다.

## 1. 발견 3 — 표본이 해부를 놓치는 영상

**원인**: 214 는 영상 전체를 1/16 표본(4행·4열 간격)으로만 히스토그램에 넣었다. 8×8 영상은 표본이 네 점뿐이다. Codex 재현(표본 네 점만 배경 3000, 나머지 60화소가 해부 1000~2000)에서 214 는 3000 근처 한 칸 폭 창을 돌려주고 해부가 전부 잘렸다. 같은 214 코드로 **3072×3072** 짜리 비슷한 영상(격자 화소 전부 3000, 나머지 두 클래스)을 돌리면 창이 `[2998.05, 3000.00]` 이다(`timing_head_214.txt`).

**수정(`voi_auto_window.cpp`)**:
1. 화소 수가 2^20(1024×1024) 미만이면 **표본 없이 모든 화소**를 히스토그램에 넣는다(`kAutoWindowFullPassPixelLimit`).
2. 그 이상이면 먼저 표본의 값 범위를 구하고, **영상 전체를 한 번** 훑는다. 이 한 번의 훑기가 비유한 화소 검사, 전체 최솟값·최댓값, 그리고 **표본의 값 범위 밖에 있는 화소 수**를 함께 센다. 그 수가 영상의 0.2% 를 넘거나 표본이 값 범위를 못 가졌으면 표본을 믿지 않고 전체 히스토그램으로 간다(`kAutoWindowMaxUnsampledMass = 0.002`).
3. 표본으로 얻은 결과가 **대체 창**(두 클래스 아님)이면, 그 결정을 **전체 히스토그램으로 다시 확인**한다. 그래서 대체 창은 언제나 실제 전체 영상의 분위수다(칸 하나, 값 범위의 0.1% 해상도).
4. 표본 히스토그램의 범위는 이제 **표본 자신의 값 범위**다(전체 영상의 최솟값·최댓값은 표본이 못 본 고립 화소일 수 있다). 214 와 달리 이상치가 칸 폭을 키우지 않는다.

**상수와 근거**:

| 상수 | 값 | 근거(측정) |
|---|---|---|
| `kAutoWindowFullPassPixelLimit` | 2^20 | 1000×1000 전체 히스토그램 1.2 ms. 이상은 1/16 표본이 65536점 이상 |
| `kAutoWindowMaxUnsampledMass` | 0.002 | 해부 하위 분위수 0.5% 보다 작아야 한다(보이지 않는 질량이 그 분위수를 못 움직임). 실영상 6장(손목, 평탄장 bright01·04·06, dark 와 그 선형·로그 변형 11개)의 표본 밖 화소 11–37개(0.00012–0.00039%), 500배 여유 |

표본 밖 화소 실측(`numpy`, 표본 4 간격, 전체 영상 대비): 손목 USM 후 33개, 손목 선형 21개, bright01 24개, bright04 37개, bright06 27개, dark 11개(로그 영역은 선형과 같은 수). 실영상은 표본 경로를 그대로 쓰고 두 번째 히스토그램을 치르지 않는다.

**시간(3072², 21회, 중앙값 ms; `timing_head_214.txt` 대 `timing_working_214b.txt`, 같은 하니스에서 코드만 바꿔 번갈아 빌드)**:

| 경로 | 214(HEAD) | 214b |
|---|---|---|
| 실영상(선형 ADU / 로그 영역, 표본, 대표성 있음) | 3.09 / 2.79 | 4.00 / 3.74 |
| 표본이 대표성 없음(전체 히스토그램) | 3.24 (창이 틀림) | 11.51 |
| 1000×1000(전체 히스토그램) | 0.35 | 1.17 |

값이 늘어난 과정: 처음 구현(표본 뒤 별도의 float 비교 훑기)은 약 10.0 ms(`timing_paths_run3.txt`), 정수 키로 바꾸고 64비트 카운터를 청크로 쪼개 8.3 → 7.4 ms(`run4/5`), 비유한 검사·극값·표본 밖 세기를 **한 번의 훑기로 합치자** 3.6–4.2 ms(`run7/8/9`, 마지막 비교 3.7–4.0 ms). 16 ms 예산(REQ-DISP-016 의 VOI 단계)과의 관계: 자동 창은 별도 호출이고, 합쳐도 VOI 단계 약 4–5 ms + 4 ms 이다. 표본이 실패하는 드문 경로도 11–12 ms 라 16 ms 안이다.

**시험(`test_voi_auto_window.cpp`, 신규 5 + 아래 발견 4 의 3)**:
- `CodexReproductionEightByEightKeepsTheAnatomyInsideTheWindow` — Codex 재현 그대로. 창이 해부 전체를 포함한다(214 에서는 한 칸 폭 창). 이 8×8 은 배경이 6%·해부가 넓어 오츠 분리도가 약 0.63(손계산)이라 대체 창으로 가고 Info 경보가 뜬다(경보는 시험에서 관측) — 요점은 경보가 아니라 창이 64화소 전체로 만들어져 해부를 포함한다는 것이다.
- `TwoClassesOffTheSampleGridInALargeImageAreStillFound` — 1280×1280, 격자 화소 전부 3000, 나머지가 해부 85% / 배경 15%. 창이 전체 영상의 해부 창(아래 약 1005, 위 약 2907)이다.
- `FallbackOfTheSampleIsConfirmedOnTheWholeImage` — 표본만 보면 종 모양(대체 창)인데 나머지 15/16 은 두 클래스인 영상. 표본의 값 범위가 나머지를 모두 덮게 만들어(종 800–3200) 대표성 검사는 조용하고 대체 창 확인만이 구한다.
- `SampleThatMissesTheLowerPartOfTheAnatomyIsNotTrusted` — 표본이 두 클래스를 다 보이지만(대체 창 아님) 해부의 아랫부분(1000–1500)을 놓친다. 표본 밖 화소 7% 라 대표성 검사만이 구한다.
- `FallbackWindowOfALargeImageIsTheWholeImageQuantileRange` — 큰 종 모양 영상의 대체 창이 정확한 전체 1·99% 분위수에서 1.5칸 안.
- (발견 4 시험은 아래.)

## 2. 발견 4 — ±FLT_MAX 에서 무한 폭 창

**원인**: 범위는 double 로 계산되지만 center·width 를 float 로 좁힌 뒤 유한성·양수 검사가 없었다. −FLT_MAX 와 +FLT_MAX 두 클래스의 폭은 2·FLT_MAX 라 float 폭이 무한대가 되었는데 `XPE_OK` 로 나갔다.

**수정**: double 로 구한 center·width 를 float 로 좁히기 **전에** `|center| ≤ FLT_MAX`, `0 < width ≤ FLT_MAX` 를 비교로 확인하고(범위 밖 double 의 float 변환은 정의되지 않은 동작이라 비교가 먼저), 좁힌 **뒤에** 비트 검사로 유한성과 `width > 0` 을 다시 확인한다. 하나라도 어기면 `XPE_ERR_INVALID_INPUT` 이고 출력 구조체는 건드리지 않는다. 계약은 헤더에 적었다: `XPE_OK` 이면 center·width 는 유한 float 이고 width 는 양수다.

**시험(신규 3)**:
- `ClassesAtPlusAndMinusFltMaxAreRefusedNotReturnedWithInfiniteWidth` — 두 클래스가 ∓FLT_MAX: `XPE_ERR_INVALID_INPUT`, 출력 불변.
- `ExtremeFiniteRangesNeverGiveANonFiniteOrNonPositiveWindow` — 8가지 극단 범위(−FLT_MAX..FLT_MAX, 0..FLT_MAX, −FLT_MAX..0, FLT_MAX/2..FLT_MAX, 비정규 한 칸, 1.0 의 인접 float, FLT_MAX 의 인접 float, 1e-30..2e-30) × 해부 비율 2가지에 대한 **불변식**: `OK` 이면 center·width 유한이고 width>0, 아니면 `INVALID_INPUT` 이고 출력 불변. 어느 쪽이 나오는지는 요점이 아니고 불변식이 요점이다.
- `FlatImageOfAnExtremeValueIsStillAWindow` — ±FLT_MAX 평탄 영상은 center=그 값, width=1.

## 3. 요구 문구 초안 (카드 3항)

- `draft_1_REQ-DISP_polarity_and_auto_window_v2.txt`: (a)(e)(g) 를 실제 동작으로 고침 — 큰 영상은 표본·작은 영상은 전체, 대표성 검사(0.2%), 대체 창은 **모든 화소의 히스토그램**에서의 1·99% 분위수(칸 하나 해상도, 표본에서 도달한 대체는 전체에서 다시 계산), float 로 못 담는 창은 오류, `XPE_OK` 이면 center·width 유한·width 양수. 새 항 (i), 성능 수치와 시험 목록과 반증을 갱신.
- `draft_2_REQ-DISP-017_revision_v2.txt`: REQ-DISP-017a 를 둘로 갈랐다 — "지금 채택 가능"(프리셋은 고정 창이고 VOI 도메인·출력 범위를 호출자가 맞춘다)과 "**GUI-C-233 병합 전에는 쓰지 않는다**"("프리셋은 기본 창이 아니며 파이프라인의 기본 창은 `xpe_voi_auto_window` 가 돌려주는 것이다" — 모듈에서는 참이지만 앱이 아직 그 함수를 부르지 않으므로 제품에 대해서는 아직 참이 아니다).

## 4. 회귀

| 항목 | 결과 |
|---|---|
| 표시 시험 | 11개 실행 파일 154개 모두 통과(`display_tests_results.txt`) |
| 기존 E2E | 12개 통과(`e2e_default_tests.txt`) |
| doxygen | 오류 0(`doxygen_err.txt` 빈 파일) |
| 실영상 체인(`p1_chain.txt`) | 창 `[1981.3684, 3180.0662]`(center 2580.7173, width 1198.6976), 첫 호출 4.28 ms. 214 의 `[1981.34, 3179.77]` 과 0.3 로그단위 차이(히스토그램 범위가 표본 범위로 바뀐 효과) |
| 최종 영상 | 214 의 PNG 와 226783 화소가 다르고 최대 차이 65/65535(0.1%). `m2-post-v2/` PNG 를 새 코드로 다시 만들었고 README 의 창·수치를 고쳤다. 체인 경로와 변형 렌더러(`v7`)는 차이 0화소 |
| P6 누수(자동 창 매 프레임 호출) | 기울기 +0.00073 MiB/프레임, 끝−시작 0.000 MiB. 양성 대조군 +1.006 |
| P2(`p2_times_run1/2.txt`) | 기계가 바쁜 상태였다: 코드가 바뀌지 않은 Presentation 중앙값이 29.6·29.2 ms(조용할 때 23.6). 자동 창 중앙값 4.8·4.4 ms(부하 포함). 깨끗한 값은 경로별 시간 하니스의 3.7–4.0 ms. 이 실행들은 214b 의 시간 근거로 쓰지 않고 위 경로별 표를 쓴다 |

## 5. 반증 (카드 4항)

`arms_falsification.txt`(스크립트 `arms_214b.py.txt`) — 제품을 일부러 고쳐 빌드하고 자동 창 시험을 돌린 뒤 원본을 복원(SHA-256 전후 동일)하고 다시 빌드해 초록임을 확인했다.

| arm | 변형 | 빨강이 된 시험 |
|---|---|---|
| 4 | 대표성 검사가 절대 발동하지 않음(표본을 항상 믿음) | `SampleThatMissesTheLowerPartOfTheAnatomyIsNotTrusted` |
| 5 | 표본의 대체 창을 전체에서 다시 확인하지 않음 | `FallbackOfTheSampleIsConfirmedOnTheWholeImage` |
| 6 | 214 코드로 되돌림(작은 영상도 표본, 표본 항상 신뢰, 대체 창 미확인, 히스토그램 범위 = 전체 극값) | `CodexReproductionEightByEight…`, `TwoClassesOffTheSampleGrid…`, `FallbackOfTheSampleIsConfirmed…`, `SampleThatMisses…` — 4개 |
| 7 | float 검사 둘 다 제거 | `ClassesAtPlusAndMinusFltMax…`, `ExtremeFiniteRanges…` — 2개 |
| 복원 | — | 0개, 17개 통과 |

**처음에는 arm 4·5·6 이 모두 초록(0개 빨강)이었다.** 표본이 평탄하면 "한 클래스가 2% 미만"으로 대체 창이 되고, 대체 창은 전체에서 다시 확인하므로 대표성 검사를 꺼도 답이 맞았다. 반대로 대체 창 확인을 꺼도 표본 범위 밖 화소가 많아 대표성 검사가 구했다. 두 안전장치가 서로를 덮고 있었다. 한 장치씩 따로 변별하는 시험(`SampleThatMisses…`: 표본이 두 클래스를 다 보이지만 아랫부분을 놓침 / `FallbackOfTheSampleIsConfirmed…`: 값 범위가 다 덮이는 종 모양 표본)을 추가하고 나서야 각 arm 이 빨개졌다. 이 서술이 이 보고서에서 가장 쓸모 있는 줄이다: **보이지 않는 곳에서 서로를 덮는 방어는, 하나를 끄는 반증으로만 드러난다.**

## 5절 요약 (verification-claim-integrity §3)

**Claim**: 위 결론 표. **Evidence**: 이 폴더의 `arms_falsification.txt`, `timing_head_214.txt`·`timing_working_214b.txt`·`timing_paths_run*.txt`, `p1_chain.txt`, `p2_times_run*.txt`, `p6_leak.txt`, `p8_variants.txt`, `display_tests_results.txt`, `e2e_default_tests.txt`, `doxygen_err.txt`. 표의 수는 그 원문에서 옮겼다.
**Baseline-attribution**: 제품 214(`b796ba00`) 대비 214b 변경, `ci-post`(RelWithDebInfo), 입력 `d152e8eb…fdfd4`. 시간 비교는 같은 하니스에서 214 코드와 214b 코드를 번갈아 빌드해 얻었고 파일이 같은 기계 상태에 있었다.
**Gaps (미검증)**:
- 대표성 검사는 "표본의 값 범위 밖 화소 질량"만 본다. 표본이 값 범위는 덮되 클래스 구성이 크게 어긋난 적대적 영상(예: 격자 화소와 나머지가 같은 값 범위에서 다른 분포)은 이 검사가 못 잡는다. 이 경우 분위수가 그 어긋남만큼 움직일 수 있다. 시험 `FallbackOfTheSampleIsConfirmedOnTheWholeImage` 는 대체 창 쪽의 그런 경우만 덮는다.
- 표본 격자는 축에 정렬된 4 간격이라 4화소 주기 패턴(검출기 격자, 모아레)과 겹치면 편향될 수 있다. 엇갈린 표본은 넣지 않았다.
- 0.2%·2^20 은 손목 한 장과 평탄장·암 프레임 몇 장으로 정했다. 다른 부위·해상도는 측정하지 않았다.
- 표본이 실패하는 경로(약 11.5 ms)와 실영상 경로(3.7–4.0 ms) 사이의 시간 차이는 코어 수·캐시에 따라 달라질 수 있다(로컬 20스레드 공유 기계).
- P2 는 기계 부하 탓에 이번 실행에서 214b 의 근거로 쓰지 못했다. 경로별 시간은 별도 하니스의 값이다.
- 극단 float 범위 8가지는 불변식으로만 확인했다(어느 쪽이 나오는지는 정하지 않음).
**Residual-risk**: 표본 신뢰 기준이 질량(0.2%)이라 그 이하의 보이지 않는 영역은 계속 허용된다. 진짜 안전을 원하면 항상 전체 히스토그램(약 11 ms)이 답이고, 이는 요구(성능)와 정확성 사이의 선택이다.
