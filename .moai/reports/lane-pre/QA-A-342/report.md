[2026-11-17 08:20 KST] [xpe-pre] QA-A-342 — 착수 전 차단: 카드의 두 요구가 서로 맞지 않음 (코드 변경 없음)

## 임계 4.48 의 출처 (file:line)
- `modules/preprocess/src/xpe_bpm_v2.cpp:1178` `threshold[5] = varianceThreshold(nuBright, p)` — P(χ²_ν/ν > τ) = p 인 τ(`:117-129`). 판정 `:1182`: noiseSum/ν > τ 이고 비용 ≥ 2.0(`kNoiseCostLimit`, `:38`)인 선량 그룹이 절반 이상(`:1040`, persistHits).
- 통계량은 그룹별 이득 보정 분산 비(비용 c 의 화소에서 평균 c)의 χ² 합. 즉 "비용 = 1" 가설에서 오경보율 p.

## 카드 문구대로 바꾸면: 같은 오경보율을 비용 2.0 경계에 적용 → 임계 = 2.0·τ
계산(`χ²` 꼬리, 아래 명령 원문):
```
nu 8 n 65536 tau 4.542 | cost3 detect: tau 0.1463  2tau 0.00210 | cost1.67 flag: tau 5.39e-03 2tau 7.03e-07 | cost2 flag at 2tau 1.53e-05
nu 13 n 9437184 tau 4.484 | cost3 detect: tau 0.1104  2tau 0.00021 | cost1.67 flag: tau 8.76e-04 2tau 8.70e-10 | cost2 flag at 2tau 1.06e-07
```
- 시험 `modules/preprocess/tests/test_bpm_v2.cpp` TheNoiseCostIgnoresTheExposureVariationBetweenTheFramesOfADoseGroup(4 선량 × 3장 → ν 8, 256×256)은 비용 3 화소 144개 중 **≥ 10 검출**을 요구(`EXPECT_GE(eff, 10)`, 이론 0.152). 2.0·τ 에서 기대 검출률 0.0021 → 144개 중 약 0.3 → **시험 실패**.
- 실데이터(ν 13): 비용 1.67 오경보 기대율 8.8e-4 → 8.7e-10(사실상 0), 비용 3 검출률 0.110 → 0.00021.

## 결론 (사실)
- "비용 > 2.0 가설 + 같은 오경보율"과 "비용 3 검출 시험 유지(기대값 불변)"는 이 통계량으로 동시에 만족되지 않는다. 화소 하나·그룹 4~5개로는 비용 2 와 3 을 그 정도로 가를 표본이 없다(분산 비 자유도 8~13).
- 31개 오경보 감소 자체는 2.0·τ 로 달성 가능(계산상), 대가로 비용 3 검출을 거의 잃는다.
- 측정·구현·시험 변경 없음. 결정 대기.
