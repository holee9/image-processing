# QA-A-42 검증 보고서 — SPEC 규칙(3×3 중심 제외) 적용 + 픽셀당 할당 제거

- 레인: Lane A (`xpe-pre`), 브랜치 `dev/preprocess`
- 커밋: `1e71700` (SPEC 규칙), `a6218cc` (버퍼 재사용), `b640a73` (하네스 재기준화 + 개명)
- 증거 디렉터리: `.moai/reports/lane-pre/QA-A-42/`
- Refs #143 #144

> **먼저 읽을 것 — 이 카드는 요구 하나를 새로 깨뜨렸다.**
> SPEC 의 알고리즘 절을 그대로 구현하자, 같은 SPEC 의 출력 상한 절
> ("clean 입력에서 결함 플래그 ≤ 1%")이 깨진다. clean 프레임 오검출이
> 0.047% → **1.72%** 로 올라갔다. 숨기지 않고 KnownDivergence 로 고정했지만,
> **이 상태를 그대로 둘지는 리더 판단이 필요하다** — §4 참조.
>
> 성능은 반대로 크게 좋아졌다. 3072² 가 ~12초(A-41 환산) → **1004 ms** 실측,
> SPEC 목표 35 ms 대비 345배 → **28.7배**.

---

## 0. 원문 인용 (합격 조건 1항)

`.moai/specs/SPEC-XPE-P1A/spec.md` REQ-P1A-013 알고리즘 절 1단계:

> 1. For each pixel `p(x,y)`, compute local median `m(x,y)` over **3x3 neighborhood excluding center (8 values)**

같은 항목 Pixel Accuracy 절:

> - Edge-of-image pixels (where 3x3 neighborhood is incomplete): processed with available subset; **at least 5 neighbors required or pixel is skipped** (defectMapOut = 0)
> - Output is boolean-like UINT8 (0 or 1); guaranteed **`sum(defectMapOut)` does not exceed `width*height * 0.01` for clean input**

구현은 `RUNTIME_DETECTION_DEFAULT_WINDOW_SIZE = 5` 로 **5×5 창**을 쓰고
`CollectWindowValues` 가 **중심을 포함**해 25개를 모으고 있었다. 두 군데가
동시에 어긋난 상태였고, 중심 포함 쪽이 더 해롭다 — 결함 픽셀이 자기가
비교당할 중앙값과 MAD 에 기여해 스스로를 가린다.

---

## 1. 주장 (Claim)

| # | 주장 | 판정 |
|---|------|------|
| C1 | 중앙값·MAD 가 3×3 이웃 8값(중심 제외)에서 계산된다 | PASS |
| C2 | 이웃 5개 미만 픽셀은 판정하지 않는다 (모서리 skip) | PASS |
| C3 | 버퍼 재사용이 동작을 바꾸지 않는다 (픽셀 단위 동일) | PASS |
| C4 | 픽셀당 할당 제거로 유의미한 시간 절감 | PASS — 623→226 ms (64%) |
| C5 | `avx2_parity` 파일명이 실제 내용(결정성)과 일치 | PASS |
| C6 | ci-preprocess 재실측 회귀 없음 | PASS — 605/605 |
| C7 | clean 입력 1% 상한 유지 | **FAIL — 1.72%** (§4) |

---

## 2. 증거 (Evidence)

### 2-1. RED → GREEN (커밋 1)

RED (`a42-red-ctest.log`, `a42-red.log`): SPEC 규칙 적용 직후 **6건 FAIL**

```
487 - RuntimeDetectionFunctionalTest.GaussianNoiseKeepsFalsePositivesLow (Failed)
491 - RuntimeDetectionFunctionalTest.EdgeAndCornerOutliersAreFlagged (Failed)
599 - RuntimeDetectionRatesTest.KnownDivergence_FprOnCleanFramesExceedsTheRequirement (Failed)
600 - RuntimeDetectionRatesTest.KnownDivergence_TprStaysBelowTheFloorThroughTenSigma (Failed)
601 - RuntimeDetectionRatesTest.RatesAreInvariantUnderNoiseScaling (Failed)
602 - RuntimeDetectionRatesTest.CleanFrameStaysFarUnderTheOnePercentCeiling (Failed)
```

갱신 근거는 케이스별로 다르다:

| 케이스 | 왜 실패했나 | 조치 |
|---|---|---|
| `EdgeAndCornerOutliersAreFlagged` | 모서리가 플래그된다고 단언했는데 SPEC 은 skip 하라고 쓴다. 옛 5×5 창에서 모서리에 8개가 남아 최소이웃 규칙이 한 번도 작동하지 않아 우연히 통과하던 케이스다 | **전제가 SPEC 과 충돌** → `CornersAreSkippedAndEdgesAreJudged` 로 개명, 모서리 skip·가장자리 판정을 각각 고정 |
| `GaussianNoiseKeepsFalsePositivesLow` | 90/4096 = 2.2% 로 1% 상한 초과 | **새 위반** → `KnownDivergence_GaussianNoiseExceedsTheOnePercentCeiling` |
| `CleanFrameStaysFarUnderTheOnePercentCeiling` | 18,011/1,048,576 = 1.72% | **새 위반** → `KnownDivergence_CleanFrameBreachesTheOnePercentCeiling` |
| A-40 KnownDivergence 2건 | 수치가 달라짐 | 카드 지시대로 **실측값으로 갱신**(요구는 여전히 미달) |
| `RatesAreInvariantUnderNoiseScaling` | 18011 vs 18009 — 정확한 일치가 깨짐 | 상대오차 1e-3 이내로 완화 + 사유 명시(§5-3) |

GREEN: `100% tests passed, 0 tests failed out of 601`

### 2-2. 규칙 변경의 효과 (1024², 961 주입, σ=10, 시드 20260911)

| 지표 | 변경 전 (5×5 중심 포함) | 변경 후 (3×3 중심 제외) | 방향 |
|---|---|---|---|
| TPR @ 5σ | 0.500520 | **0.610822** | 개선 +22% |
| TPR @ 6σ | 0.711759 | **0.761707** | 개선 |
| TPR @ 8σ | 0.955255 | 0.927159 | **악화** |
| TPR @ 10σ | 0.998959 | 0.986472 | **악화** |
| FPR (clean) | 4.730e-4 | **1.718e-2** | **36배 악화** |

낮은 진폭은 좋아지고 높은 진폭은 나빠진다. 중심을 빼면 결함이 자기 중앙값을
끌어올리지 못해 아슬아슬한 신호가 임계를 넘지만, 표본이 25 → 8 로 줄어 σ 추정이
거칠어지므로 넉넉히 넘어야 할 신호를 놓치고 평범한 잡음을 플래그한다.

### 2-3. 동일성 단언 (커밋 2, `a42-parity.log`)

`BufferReuseParityTest` 4건 — 기준은 **할당형 오버로드**를 테스트 TU 에서
픽셀마다 돌린 맵, 대상은 **출하 진입점**의 맵.

```
[  PASSED  ] 4 tests.
```

케이스는 재사용 버퍼가 내용이나 크기를 흘리면 갈라지는 지점을 고른 것이다:
가우시안 프레임(전 픽셀 완전 이웃), 20σ 주입(가장 극단적인 버퍼 내용),
평탄장+테두리(MAD==0 분기와 최소이웃 skip 분기), 폭 5 세로 프레임(잘린 창이
온전한 창 바로 뒤에 오는 배치). 전부 픽셀 단위 완전 일치.

### 2-4. 시간 전/후 (합격 조건)

**진입점 격리 측정** — 같은 트리에서 배선 한 줄만 바꿔 재빌드, 같은 명령:

| 형태 | 1024² (저잡음/고잡음) | TPR@5σ | FP |
|---|---|---|---|
| 픽셀당 할당 | 623.3 ms / 620.6 ms | 0.610822 | 18011 / 18009 |
| 버퍼 재사용 | **226.3 ms / 225.6 ms** | 0.610822 | 18011 / 18009 |

**64% 절감(2.75배).** TPR·FP 가 자릿수까지 동일하므로 동작 변화가 아니다.

**프레임 크기별 실측** (`a42-time.log`, best of 3, RelWithDebInfo):

```
[time] 1024x1024  best of 3:    111.1 ms   flagged 17973 (1.714%)
[time] 3072x3072  best of 3:   1004.3 ms   flagged 160886 (1.705%)
```

SPEC 성능 절 대비:

| | SPEC 목표 | A-41 시점 | A-42 이후 | 개선 |
|---|---|---|---|---|
| 3072² 스칼라 | < 35 ms | ~12,100 ms (환산) | **1,004 ms (실측)** | 345배 초과 → **28.7배 초과** |

두 요인이 함께 작용했다: 표본 25 → 8 (정렬 비용 감소)과 할당 제거.
여전히 목표를 28.7배 넘지만, 자릿수가 하나 줄었다.

### 2-5. 단계 분해 재측정 (`a42-profile-after.log`)

3×3 창, 1024², 버퍼 재사용 루프:

| 단계 누적 | 시간 | 단계 비용 | 비중 |
|---|---|---|---|
| gather only | 11.0 ms | 11.0 ms | 3.4% |
| + 중앙값 1회 | 70.5 ms | 59.5 ms | 18.3% |
| + 복사 + MAD | 114.4 ms | 43.9 ms | 13.5% |
| 할당형 `DetectDefectivePixel` | 324.8 ms | **210.4 ms** | **64.8%** |

A-41 에서 5×5 로 41.6% 였던 할당 비중이 3×3 에서는 **64.8%** 로 더 커졌다 —
실제 계산이 줄어든 만큼 할당의 상대 비중이 올라간 것이며, 커밋 2 가 제거한
것이 바로 이 부분이다.

### 2-6. 하네스 재기준화 (커밋 3, `a42-sweep.log`)

시드 3개 평균, 새 수집 규칙(3×3 중심 제외, 최소이웃 5) 기준:

| 변형 | TPR@5σ | TPR@10σ | FPR | ms |
|---|---|---|---|---|
| **baseline 3×3 κ=5 (출하)** | **0.640652** | ~0.989 | **1.719e-2** | 319.0 (재사용 161.3) |
| (a) 5×5 κ=5 | 0.555671 | ~0.999 | 8.233e-4 | 567.9 |
| (a) 7×7 κ=5 | 0.528963 | ~0.999 | 1.084e-4 | 1373.8 |
| (b) 3×3 하한 α=0.8 | 0.580992 | 0.989 | 1.062e-4 | **163.5** |
| (b) 3×3 하한 α=1.0 | 0.379813 | 0.989 | **3.179e-6** ✅ | 170.1 |
| (c) 2단계 3→7 κ1=4 | 0.483177 | ~0.999 | 8.043e-5 | 233.6 |
| (d) 7×7 κ=4 [진단] | 0.819979 | 1.000 | 9.874e-4 | 1324.0 |
| (d) 3×3 κ=4 [진단] | 0.798821 | ~0.999 | 3.309e-2 | 148.1 |

A-41 대비 **순위가 바뀌었다.** 그때는 (c) 2단계가 비용 대비 최선이었으나,
지금은 **(b) α=0.8 이 우세**하다 — baseline 과 같은 속도(163.5 ms)로 FPR 을
1.72e-2 → 1.06e-4 로 **162배** 낮추면서 TPR@5σ 는 오히려 baseline 의 91% 를
유지한다. FPR 요구를 만족하는 것은 여전히 (b) α=1.0 뿐이다(3.18e-6).

### 2-7. 개명 (커밋 3)

`test_runtime_detection_avx2_parity.cpp` → `test_runtime_detection_determinism.cpp`
(`git mv`, 클래스명 `RuntimeDetectAVX2ParityTest` → `RuntimeDetectDeterminismTest`).

근거는 A-41 실측(`../QA-A-41/a41-avx2-evidence.txt`): 진입점은 스칼라
`DetectDefectivePixel` 이중 루프이고 `src/runtime_detection.cpp` +
`include/runtime_detection.h` 에 `avx2` / `__m256` / `immintrin` 이 **0건**이다.
4개 케이스가 실제로 단언하는 것은 "같은 입력 → 같은 맵" 이라는 결정성이다.

---

## 3. Baseline 귀속

- **테스트 총수**: QA-A-41 시점 601 → **605** (+4 = `BufferReuseParityTest`).
  이름이 바뀐 3건(`EdgeAndCorner…`, `GaussianNoise…`, `CleanFrame…`)은 총수 불변.
- **빌드 프리셋**: `ci-preprocess` (RelWithDebInfo, `/WX`). 최종 빌드 경고 0건.
- **전/후 시간 비교**: 같은 트리·같은 프리셋에서 배선 한 줄만 바꿔 두 번
  빌드해 측정했다. 기계 부하 차이가 아니라 코드 차이다.
- **1024² 수치가 두 개인 이유**: 레이트 스위트의 226 ms 는 단발 측정,
  `--time` 의 111 ms 는 best-of-3(캐시 워밍 포함)이다. 3072² 의 1004 ms 도
  best-of-3 이므로 SPEC 대비 비율은 **낙관적인 쪽** 수치로 계산한 것이다.

---

## 4. ★ 이 카드가 만든 새 위반 — 리더 판단 필요

REQ-P1A-013 은 서로 다른 두 절에서 다음을 함께 요구한다:

- 알고리즘 절: 3×3 이웃, 중심 제외, 8값, λ = 5.0
- Pixel Accuracy 절: clean 입력에서 `sum(defectMapOut) ≤ width*height * 0.01`

**λ = 5.0 에서 둘은 동시에 성립하지 않는다.**

| | 적용 전 (5×5 중심 포함) | 적용 후 (3×3 중심 제외, SPEC) |
|---|---|---|
| clean 1024² 플래그 | 496 (0.047%) — 상한 준수 | **18,011 (1.72%) — 상한 위반** |
| clean 64² 플래그 | 상한 준수 | **90 / 4096 (2.2%) — 위반** |

실무적 의미: 이 상태로 출하하면 **모든 프레임에서 화소의 1.7% 가 결함으로
마킹된다.** 3072² 기준 160,886 픽셀이다. 결함 보정이 이 맵을 신뢰한다면
정상 화소 16만 개를 이웃값으로 덮어쓰게 된다.

선택 가능한 축(측정 근거는 §2-6 표):

- **A. λ 를 올린다** — 사용자 결정 대기 중인 항목. 이 카드는 건드리지 않았다.
- **B. (b) 전역 σ 하한을 도입한다** — α=0.8 이면 FPR 1.06e-4(상한의 1/94)로
  떨어지고 속도는 baseline 과 같다. TPR@5σ 손실은 0.641 → 0.581.
- **C. 커밋 `1e71700` 을 되돌린다** — SPEC 위반 상태로 복귀하지만 1% 상한은
  지켜진다. 두 SPEC 절 중 어느 쪽을 정본으로 볼지의 문제다.

이 레인은 카드 지시("SPEC 이 정본")대로 A안 없이 규칙만 적용했고, 결과를
사실대로 고정했다. **되돌릴지 여부는 리더 결정이다** — 커밋 3건 모두 미푸시라
되돌리기 비용은 낮다.

---

## 5. 미검증 (Gaps)

1. **`CollectWindowValues` 는 그대로 두었다.** 중심 포함 수집 함수로 남아 있고,
   `test_runtime_detection_functional.cpp` 등이 여전히 쓴다. 이 카드에서
   의미를 바꾸지 않았으므로 "죽은 코드인지" 는 판정하지 않았다.
2. **`windowSize` 가 3 이 아닌 값으로 들어오는 경로를 시험하지 않았다.**
   `RuntimeDetectionConfig` 는 여전히 임의의 홀수를 받고, 최소이웃 5 규칙은
   창 크기와 무관하게 적용된다. 5×5 이상에서는 모서리도 8개 이상이라 규칙이
   작동하지 않는다 — 3×3 전용 동작이라는 뜻이며 문서화만 했다.
3. **`--time` 은 best-of-3 이고 단일 스레드다.** 실제 파이프라인에서 다른
   스테이지와 경합할 때의 시간은 재지 않았다.
4. **3072² 에서 TPR/FPR 을 재지 않았다.** 시간만 쟀다. flagged 비율(1.705%)이
   1024²(1.714%)와 사실상 같다는 것이 유일한 간접 증거다.
5. **커밋 2 의 절감이 다른 컴파일러/플랫폼에서도 같은 비율인지 모른다.**
   MSVC RelWithDebInfo 한 조합에서만 측정했다.
6. **A-41 이 남긴 Gap 은 그대로다** — 클러스터 결함 미시험, 합성 가우시안
   한계, α·2단계 파라미터 탐색 부족, 시드 3개.

---

## 6. 잔여 위험 (Residual risk)

1. **§4 의 1% 위반이 이 카드의 최대 위험이다.** 결함 보정 경로가 이 맵을
   소비한다면 정상 화소를 대량으로 덮어쓴다. `xpe_defect_correct` 가 런타임
   맵을 어떻게 병합하는지는 이 카드에서 확인하지 않았다.
2. **최소이웃 규칙이 테두리 한 줄의 검출을 완전히 끈다.** 모서리 4점만이
   아니라, 3×3 중심 제외에서 이웃이 5개인 가장자리는 통과하고 3개인 모서리만
   막히므로 실제 손실은 4픽셀이다. 다만 `windowSize` 를 키우면 이 경계가
   조용히 이동한다(Gap 2).
3. **버퍼 재사용은 스레드 안전성을 진입점 호출 단위로 좁힌다.** 버퍼가
   `xpe_defect_detect_runtime` 지역 변수이므로 호출 간 공유는 없지만, 앞으로
   이 루프를 병렬화하면 버퍼를 스레드별로 나눠야 한다. 현재 주석에 그 사실이
   적혀 있지 않다.
4. **A-40·A-42 의 KnownDivergence 가 이제 5건이다.** 전부 "PASS" 로 보이므로
   요약만 읽으면 요구가 충족된 것으로 오독될 수 있다. 이름 접두와 이 보고서
   첫 단락이 유일한 방어선이다.
