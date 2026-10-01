# QA-A-40 검증 보고서 — REQ-P1A-013 runtime defect detection TPR/FPR 정량 검증

- 레인: Lane A (`xpe-pre`), 브랜치 `dev/preprocess`
- 커밋: `5e9fe84` (측정 하네스), `3a9c73a` (단언 — 3건 KnownDivergence)
- 증거 디렉터리: `.moai/reports/lane-pre/QA-A-40/`
- Refs #120 #112

> **한 줄 결론: REQ-P1A-013 의 TPR·FPR 요구는 현재 구현에서 충족되지 않는다.**
> TPR 은 5σ 에서 0.50, 10σ 에서도 0.9990 으로 0.999 에 못 미치고, FPR 은
> 요구치의 47배다. 단언을 낮추지 않고 실측값을 `KnownDivergence_` 로 고정했다.

---

## 0. 원문 인용 (합격 조건 1항)

`.moai/specs/SPEC-XPE-P1A/spec.md` REQ-P1A-013 — **Pixel Accuracy**
(research.md v2.0.0 Section 8.3), 원문 그대로:

> - True-positive rate (TPR) on injected 5-sigma transients: >= 99.9%
> - False-positive rate (FPR) on clean clinical frames: < 0.001% (< 9 false pixels per 3072x3072)
> - Edge-of-image pixels (where 3x3 neighborhood is incomplete): processed with available subset; at least 5 neighbors required or pixel is skipped (defectMapOut = 0)
> - Output is boolean-like UINT8 (0 or 1); guaranteed `sum(defectMapOut)` does not exceed `width*height * 0.01` for clean input

같은 항목의 알고리즘 절:

> **Algorithm (Hampel 5-sigma detector, research.md v2.0.0 Section 8.3, item 4)**:
> 1. For each pixel `p(x,y)`, compute local median `m(x,y)` over 3x3 neighborhood excluding center (8 values)
> 2. Compute local MAD (median absolute deviation): `MAD(x,y) = median(|neighbor - m|)`
> 3. Modified z-score: `z = 0.6745 * (p(x,y) - m(x,y)) / MAD(x,y)`
> 4. `defectMapOut[x,y] = 1` if `|z| > lambda` (default `lambda = 5.0`)

### 결함 정의에 대한 판단 (카드 1항: "문서에 없으면 멈추고 보고")

**멈추지 않았다.** SPEC 은 핫/데드/스파이크를 진폭별로 나눈 표를 갖고 있지
않지만, 요구가 이름 붙인 진폭은 **"injected 5-sigma transients"** 하나이며
그것으로 시험을 구성하기에 충분하다. 즉 결함 정의는 문서에서 왔다 — 다만
"5-sigma" 라는 한 종류뿐이고, 핫/데드/스파이크를 구분한 정의는 문서에 없다.
없는 것을 발명하지 않았고, 대신 6/8/10σ 스윕을 **요구가 아니라 관측**으로
덧붙였다.

문서에서 오지 않은 것(= 이 하네스의 실험 조건, 요구치가 아님):

| 항목 | 값 | 근거 |
|---|---|---|
| 프레임 크기 | 1024 × 1024 | 카드 지정 |
| 잡음 준위 | σ = 10 ADU / 50 ADU (평균 3000) | 카드의 "저/고노이즈 2조합" |
| 주입 개수 | 961 (32픽셀 격자, 테두리 16 여유) | 카드의 "N ≥ 1000" 에 근접 — §4 Gap 1 |
| 시드 | `std::mt19937(20260911)` | 결정성 |

---

## 1. 주장 (Claim)

| # | 주장 | 판정 |
|---|------|------|
| C1 | 5σ 트랜지언트에서 TPR ≥ 99.9% | **FAIL — 0.500520** |
| C2 | clean 프레임에서 FPR < 0.001% | **FAIL — 0.0473%** |
| C3 | 어떤 진폭에서든 TPR ≥ 99.9% 도달 | **FAIL — 10σ 에서도 0.998959** |
| C4 | clean 프레임 플래그가 1% 상한 미만 | PASS (496 < 10,485) |
| C5 | 두 잡음 준위가 동일 결과 (스케일 불변) | PASS — §3 참조 |
| C6 | ci-preprocess 재실측 회귀 없음 | PASS |

---

## 2. 증거 (Evidence) — 조합별 TPR/FPR 표

명령: `build\ci-preprocess\bin\xpe_preprocess_tests.exe --gtest_filter=RuntimeDetectionRatesTest.*`
빌드: `ci-preprocess` (RelWithDebInfo, `/WX`)

측정 출력 (`a40-assert.log`, 하네스 단독 측정은 `a40-harness.log`):

```
[rates] low noise, 5 sigma    injected=961 TP=481 FN=480  TPR=0.500520 | FP=496/1048576 FPR=0.000473022 | 1340.2 ms
[rates] high noise, 5 sigma   injected=961 TP=481 FN=480  TPR=0.500520 | FP=496/1048576 FPR=0.000473022 | 1339.1 ms
[rates] low noise, 6 sigma    injected=961 TP=684 FN=277  TPR=0.711759 | FP=0/0 FPR=0.000000000 |  941.2 ms
[rates] low noise, 8 sigma    injected=961 TP=918 FN=43   TPR=0.955255 | FP=0/0 FPR=0.000000000 |  931.6 ms
[rates] low noise, 10 sigma   injected=961 TP=960 FN=1    TPR=0.998959 | FP=0/0 FPR=0.000000000 |  914.6 ms
```

표로 정리:

| 조합 | 진폭 | TP | FN | **TPR** | FP / 전체 | **FPR** | 요구 대비 |
|---|---|---|---|---|---|---|---|
| 저잡음 σ=10 | 5σ | 481 | 480 | **0.500520** | 496 / 1,048,576 | **4.730e-4** | TPR 0.999↓, FPR 1e-5↑ |
| 고잡음 σ=50 | 5σ | 481 | 480 | **0.500520** | 496 / 1,048,576 | **4.730e-4** | 동일 |
| 저잡음 σ=10 | 6σ | 684 | 277 | 0.711759 | — | — | 미달 |
| 저잡음 σ=10 | 8σ | 918 | 43 | 0.955255 | — | — | 미달 |
| 저잡음 σ=10 | 10σ | 960 | 1 | 0.998959 | — | — | **한 자리 모자람** |

FPR 을 SPEC 의 표현으로 환산하면: 요구는 "< 9 false pixels per 3072×3072",
1024² 로 스케일하면 **1픽셀 미만**이어야 한다. 실측은 **496픽셀**이다.

### 단언 결과 (합격 조건 4항)

| 케이스 | 형태 | 결과 |
|---|---|---|
| `KnownDivergence_TprAtFiveSigmaIsBelowTheRequirement` | 미달 고정 | PASS(고정 성립) |
| `KnownDivergence_FprOnCleanFramesExceedsTheRequirement` | 미달 고정 | PASS(고정 성립) |
| `KnownDivergence_TprStaysBelowTheFloorThroughTenSigma` | 미달 고정 | PASS(고정 성립) |
| `CleanFrameStaysFarUnderTheOnePercentCeiling` | 요구 충족 | PASS |
| `RatesAreInvariantUnderNoiseScaling` | 성질 고정 | PASS |

`KnownDivergence_` 케이스가 "PASS" 인 것은 **요구를 만족한다는 뜻이 아니라
미달 상태가 기록된 대로 유지된다는 뜻**이다. 알고리즘이 개선되면 이 세 건이
붉게 변하고, 그때가 단언을 요구치로 되돌릴 시점이다(A-26 → A-38 과 같은 방식).

### 재실측 (`a40-ctest.log`)

```
100% tests passed, 0 tests failed out of 601
Total Test time (real) =  66.31 sec
```

### 실행 시간 (합격 조건 3항)

- 이 스위트 5케이스 합계 **14.78초**
- 1024² 검출 1회 **0.91 ~ 1.86초** (RelWithDebInfo, 스칼라 경로)
- 전체 ctest 66.31초 — A-39 시점 대비 약 15초 증가분이 이 스위트다

### 변경 규모

```
커밋 1 (a40-diff-commit1.txt)   CMakeLists.txt +2, test_runtime_detection_rates.cpp +214
커밋 2 (a40-diff-commit2.txt)   test_runtime_detection_rates.cpp +87 -8
```

---

## 3. Baseline 귀속 + 원인 분석

- **테스트 총수**: QA-A-39 측정 596 → 601 (+5 = 신규 `RuntimeDetectionRatesTest`).
  기존 케이스 상태 변화 없음.
- **빌드 프리셋**: `ci-preprocess` (RelWithDebInfo). 최적화된 빌드이므로 시간
  수치는 Debug 값이 아니다.
- **측정 → 단언 순서**: 커밋 `5e9fe84` 는 측정만 하고 요구치를 단언하지 않는다.
  커밋 `3a9c73a` 의 단언은 그 커밋의 로그(`a40-harness.log`)에 남은 수치를
  보고 작성했다. 단언이 먼저 있고 수치를 맞춘 것이 아니다.

### 왜 5σ 에서 정확히 절반인가 (관측된 메커니즘)

검출기는 `|value - median| > 5.0 × (MAD × 1.4826)` 로 플래그하고,
`MAD × 1.4826` 은 국소 표준편차의 **추정치**다(`runtime_detection.h:53-59`).
정확히 5σ 인 트랜지언트는 임계 위에 걸터앉으므로, 24개 이웃에서 얻은 σ 추정이
참값보다 작게 나온 자리에서는 검출되고 크게 나온 자리에서는 놓친다. 481/961 =
0.5005 는 그 추정 산포가 대칭임을 보여준다.

같은 산포가 FPR 의 원인이기도 하다. 헤더 주석은 "5-sigma corresponds to
approximately 1 in 3.5 million false positives"(`runtime_detection.h:47-51`)로
추론하지만, 그 계산은 **σ 가 알려진 경우**의 정규분포 꼬리 확률이다. 픽셀마다
24개 표본으로 σ 를 추정하면 추정치가 작게 나오는 자리가 생기고, 그 자리에서
평범한 잡음이 임계를 넘는다. 즉 TPR 미달과 FPR 초과는 **같은 원인의 양면**이다.

### 두 잡음 조합이 독립 증거가 아닌 이유

트랜지언트 진폭도 임계도 σ 에 비례하므로 검출기는 스케일 불변이다. 실제로
저잡음(σ=10)과 고잡음(σ=50)의 TP·FP 가 **완전히 동일**하다(481 / 496). 카드가
요구한 "2조합" 을 만족은 했으나, 두 행을 넓은 스윕으로 읽어서는 안 된다 —
이 사실 자체를 `RatesAreInvariantUnderNoiseScaling` 로 고정했다.

---

## 4. 미검증 (Gaps)

1. **주입 개수가 961 로 카드의 "N ≥ 1000" 에 39개 못 미친다.** 32픽셀 격자에
   테두리 16픽셀 여유를 두면 31×31 = 961 이 된다. 격자를 촘촘히 하면 1000을
   넘길 수 있으나 5×5 창을 공유하는 결함이 생겨 서로를 가린다. 개수를 맞추기
   위해 측정 조건을 나쁘게 만들지 않았고, 실제 개수를 출력에 적었다.
2. **음의 트랜지언트(데드 픽셀)를 시험하지 않았다.** 주입은 `+amplitude` 뿐이다.
   검출기는 `|z|` 를 쓰므로 대칭일 것으로 보이지만 **측정하지 않았다.**
   SPEC 에 핫/데드 구분 정의가 없어 발명을 피한 결과이기도 하다.
3. **클러스터 결함을 시험하지 않았다.** 주입 좌표는 서로 5×5 창을 공유하지
   않도록 떨어뜨렸다. SPEC 의 Rationale 은 "Median + MAD is robust to clustered
   outliers" 라고 주장하지만, 이 하네스는 그 주장을 검증하지 않는다.
4. **가장자리 규칙("이웃 5개 미만이면 skip")을 직접 시험하지 않았다.** 주입
   좌표를 테두리에서 16픽셀 떼어 규칙에 걸리지 않게 했을 뿐이다. clean 프레임의
   FP 496건 중 가장자리 기여분도 분리하지 않았다.
5. **실제 임상 프레임이 아니라 합성 가우시안이다.** SPEC 은 "clean clinical
   frames" 라고 쓴다. 고정 시드 가우시안은 재현 가능하지만 임상 프레임의 구조
   (해부학적 그라디언트, 산란, 라인 아티팩트)를 담지 않는다. 실제 프레임에서는
   FPR 이 이보다 나쁠 가능성이 높다 — 국소 중앙값이 구조적 경사를 따라가야
   하기 때문이다.
6. **성능은 단언하지 않았다.** 관측만 했다 — §5-1 참조.
7. **AVX2 경로를 시험하지 않았다.** `xpe_defect_detect_runtime` 은 스칼라
   `DetectDefectivePixel` 을 픽셀마다 호출한다. SPEC 의 `< 12ms (AVX2)` 목표에
   대응하는 벡터 경로가 이 진입점에 연결돼 있는지 확인하지 않았다.

---

## 5. 잔여 위험 (Residual risk)

1. **성능 목표와의 거리가 크다 (부수 관측, 단언 안 함).** 1024² 스칼라 1회가
   ~1.3초다. 픽셀 수 비례로 3072² 를 환산하면 **약 12초**로, SPEC 의
   `< 35ms (scalar)` 목표의 **340배를 넘는다.** 성능은 이 카드의 단언 대상이
   아니고 CI 시간 측정은 불안정하므로 단언하지 않았지만, 이 수치는 REQ-P1A-013
   의 Performance 절이 현재 구현과 크게 어긋나 있음을 시사한다. 별도 카드 필요.
2. **KnownDivergence 고정은 "괜찮다" 는 뜻이 아니다.** 세 건이 PASS 로 보이므로
   요약만 읽으면 요구가 충족된 것으로 오독될 수 있다. 케이스 이름에
   `KnownDivergence_` 접두를 붙이고 보고서 첫 줄에 결론을 못박은 이유다.
3. **브래킷 단언은 미세한 개선을 놓친다.** TPR 5σ 는 `0.30 < tpr < 0.90` 로
   묶었다. 알고리즘이 0.85 까지 좋아져도 이 케이스는 조용히 통과한다. 정확한
   비율을 고정하면 컴파일러 부동소수 차이로 깨지므로 택한 타협이며, 정확한
   현재값은 `RecordProperty` 와 이 보고서에만 남는다.
4. **시드 하나에만 의존한다.** 모든 수치가 `mt19937(20260911)` 한 시드의 결과다.
   여러 시드로 평균을 내지 않았으므로, 0.500520 같은 값의 신뢰구간은 모른다.
   961 시행에서 절반이라는 결과는 메커니즘상 우연으로 보기 어렵지만,
   **통계적으로 확인하지는 않았다.**
