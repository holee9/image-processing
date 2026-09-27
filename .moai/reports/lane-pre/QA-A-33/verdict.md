# QA-A-33 검증 보고서 — runtime defect detection 기능 테스트 재작성 (#120 #112)

- 레인: Lane A (pre) / 브랜치 `dev/preprocess`
- 카드: QA-A-33 (`.moai/lanes/pre/inbox/QA-A-33.md`)
- 삭제 원본: `git show e59825c^:tests/preprocess/test_runtime_detection.cpp` (이 디렉터리에 `a33-deleted-suite.cpp.txt` 로 동봉)
- 커밋 2건: `235f57d`(정답 기반 19케이스), 오류 경로 커밋(아래)

## 1. 주장 (Claim)

1. 삭제된 스위트의 **실제 케이스 수는 24가 아니라 22** 다(`a33-deleted-case-list.txt`, `grep -c "^TEST_F"` = 22). A-27 보고서와 이 카드가 24로 적은 것은 부정확했다 — 아래 대조표는 실측한 22건 기준이다.
2. 22건 전부를 **재작성 / 현행 ABI 에서 의미 없음** 으로 판정하고 표로 남겼다(§2). 재작성 19, 의미 없음 3 — 재작성 19건이 신규 18케이스에 대응한다(일부는 한 케이스가 둘을 겸하고, 일부는 하나가 둘로 갈렸다).
3. 정답 기반 18케이스 + 오류 경로 10케이스 = **28케이스**를 추가했다. 검출 결과를 개수가 아니라 **좌표로** 단언한다.
4. ci-preprocess **529 → 557/557 PASS**, 회귀 0. AVX2 동등성 4케이스는 그대로 뒀다.

## 2. 삭제된 22케이스 대조표

| # | 삭제된 케이스 | 판정 | 대응 |
|---|---|---|---|
| 1 | `DetectWithNullImageReturnsInvalid` | **의미 없음(중복)** | QA-A-14 `test_error_precedence.cpp` 가 널 required 포인터를 미초기화·초기화 두 상태에서 프로브한다. 재작성하면 한 계약에 갱신 지점이 둘 |
| 2 | `DetectWithNullDefectMapReturnsInvalid` | **의미 없음(중복)** | 같음 |
| 3 | `DetectWithDimensionMismatchReturnsInvalid` | 재작성 | `WidthMismatchIsRejected` + `HeightMismatchIsRejected` — 폭만 보는 검사가 통과하지 못하도록 축을 나눴다 |
| 4 | `DetectWithDefaultConfigSucceeds` | 재작성 | `UniformImageFlagsNothing` |
| 5 | `CleanUniformImageZeroDefects` | 재작성 | `UniformImageFlagsNothing` (개수 0 단언) |
| 6 | `CleanGaussianNoiseLowFPR` | 재작성 | `GaussianNoiseKeepsFalsePositivesLow` — **고정 시드**로 결정론화(원본은 `std::random_device`) |
| 7 | `CleanImageMultipleFramesConsistent` | **의미 없음(무상태)** | `xpe_defect_detect_runtime` 은 프레임 간 상태를 갖지 않는다. 같은 입력에 같은 출력이라는 주장은 순수 함수에 대한 동어반복이고, 상태를 가진 ghost 쪽과 달리 검증 가치가 없다 |
| 8 | `SingleExtremeOutlierDetected` | 재작성 | `BrightOutlierIsFlaggedAtItsOwnCoordinate` — **이웃 4방향 음성까지** 단언(원본은 개수만) |
| 9 | `SingleDarkOutlierDetected` | 재작성 | `DarkOutlierIsFlagged` |
| 10 | `MultipleOutliersDetected` | 재작성 | `MultipleSeparatedOutliersAreAllFlagged` — 좌표 4개 + 그 외 0건 |
| 11 | `EdgePixelOutlierDetected` | 재작성 | `EdgeAndCornerOutliersAreFlagged` — 모서리 2 + 가장자리 2 |
| 12 | `HampelFilterWithSmallWindow` | 재작성 | `EveryWindowSizeDetectsALoneOutlier` (창 3) — 내부 API 경유, 사유는 §3 |
| 13 | `HampelFilterWithDefaultWindow` | 재작성 | 같은 케이스 (창 5) |
| 14 | `HampelFilterWithLargeWindow` | 재작성 | 같은 케이스 (창 7) + `WindowSizeControlsHowManySamplesAreCollected` (9/25/49) |
| 15 | `ConfigWithInvalidJsonReturnsOk` | **의미 없음(도달 불가)** | 공개 진입점에 config 인자가 없다. 잘못된 JSON 을 넘길 자리가 없으므로 폴백을 관측할 방법이 없다 |
| 16 | `ConfigWithSigmaThreshold` | 재작성 | `SigmaThresholdControlsSensitivity` — 내부 API 경유(20σ 음성 / 2σ 양성) |
| 17 | `MedianAbsoluteDeviationCalculated` | 재작성 | `MadIsTheScaledSigmaEstimate` — 손계산 값 + §4 의 정정 |
| 18 | `SlidingWindowMedianRobustToOutliers` | 재작성 | `OneOutlierMovesNeitherMedianNorMad` — 중앙값과 MAD 둘 다 |
| 19 | `LowNoiseImageWithSparseDefects` | 재작성 | `SparseDefectsInLowNoiseField` |
| 20 | `HighNoiseImageWithDefects` | 재작성 | `DefectSurvivesHighNoise` |
| 21 | `SmallImageDetection` | 재작성 | `SmallestImageIsHandled`(1×1) + `ImageSmallerThanTheWindowIsHandled`(3×3, 창보다 작음) |
| 22 | `LargeImageDetection` | 재작성 | `LargeImageDetectsItsInjectedDefect` (1024×1024, 성능 단언 없음 — 카드 지시) |

원본에 없었으나 추가한 것: `ComputeMedian` 손계산(홀/짝/단일), 창의 모서리 절단(9개 수집), NULL **데이터 포인터**(구조체는 유효), 포맷 불일치 2종, 출력 버퍼 부족(`BUFFER_TOO_SMALL`), 입력 크기 부족, NULL metadata 허용, 0 차원.

## 3. 공개 진입점으로 도달할 수 없는 것 — 내부 API 를 쓴 사유

`xpe_defect_detect_runtime(image, metadata, defect_map_output)` 에는 **config 인자가 없다.** 구현은 내부에서

```cpp
config.windowSize     = ParseWindowSize(nullptr, config.windowSize);
config.sigmaThreshold = ParseSigmaThreshold(nullptr, config.sigmaThreshold);
```

로 **`nullptr` 을 넘겨** 항상 기본값(5×5, 5σ)으로 돈다(`runtime_detection.cpp:156-157`). 즉 창 크기와 시그마 임계값은 공개 경로에서 변경 불가능하다.

따라서 창 크기 3종과 시그마 민감도 케이스는 `xpe::preprocess::internal::DetectDefectivePixel` 을 직접 호출한다. 이것은 우회가 아니라 **공개 경로가 픽셀마다 호출하는 바로 그 함수**이고(`runtime_detection.cpp:172-174`), 헤더 `modules/preprocess/include/runtime_detection.h` 에 선언돼 있어 테스트에서 정당하게 접근할 수 있다.

`ParseWindowSize` / `ParseSigmaThreshold` 자체는 **호출자가 항상 `nullptr` 을 주므로 파싱 본문이 죽은 코드**다. 이 카드에서 손대지 않았다(§6).

## 4. 실측으로 정정한 가정 1건 — `ComputeMAD` 는 MAD 가 아니다

손계산: 값 `{1,2,3,4,100}` → 중앙값 3 → 편차 `{2,1,0,1,97}` → 정렬 `{0,1,1,2,97}` → **원시 MAD = 1**.

`EXPECT_FLOAT_EQ(1.0f, ComputeMAD(...))` 로 단언했더니 관측값은 **1.4826** 이었다(`a33-run1.log`):

```
error: Expected equality of these values:
  1.0f
    Which is: 1
  ComputeMAD(forMad, median)
    Which is: 1.4826
```

`ComputeMAD` 의 마지막 줄이 `return mad * RUNTIME_DETECTION_MAD_SCALE;`(`runtime_detection.h:143`)다. 이름은 MAD 지만 돌려주는 값은 **1.4826 × MAD**, 즉 정규분포 가정에서의 시그마 추정값이다.

**이중 적용은 없다**: `DetectDefectivePixel` 은 `threshold = config.sigmaThreshold * mad`(`:233`)로 재적용하지 않는다. 헤더 주석의 `5 * (1.4826 * MAD)` 는 합성 결과를 정확히 서술한다. 케이스 이름을 `MadIsTheScaledSigmaEstimate` 로 바꾸고 두 형태(상수 곱, 리터럴 1.4826)로 단언해 다음 사람이 같은 가정을 반복하지 않게 했다.

## 5. 증거 (Evidence)

| 단계 | 결과 | 로그 |
|---|---|---|
| 정답 기반 18케이스 첫 실행 | 1건 실패 (`MadMatchesHandComputedValue`, 1.0 vs 1.4826) | `a33-run1.log` (exit=8) |
| 정정 후 | **547/547 PASS** | `a33-green1.log` (exit=0) |
| 오류 경로 10케이스 추가 | **557/557 PASS** (첫 실행에 전부 통과) | `a33-run2.log` (exit=0) |

## 6. baseline 귀속

| 대상 | 직전 baseline | 이번 실측 | 차이 |
|---|---|---|---|
| ci-preprocess | 529/529 PASS (QA-A-32, `../QA-A-32/a32-verify2.log`) | **557/557 PASS** (`a33-run2.log`) | +28 |

산술이 맞는다: `grep -c "^TEST_F"` 로 실측한 케이스 수는 정답 기반 **18**, 오류 경로 **10** 이고, 529 + 18 = 547(`a33-green1.log`), 547 + 10 = 557(`a33-run2.log`) 이다.

**커밋 `235f57d` 의 메시지는 "19건" 이라고 적었는데 실제는 18건이다.** 케이스를 세지 않고 어림한 값이었고, 위 `grep` 이 정정한다. 이력을 다시 쓰지 않고 여기에 남긴다.

## 7. 미검증 (Gaps)

- **커버리지 수치 미측정.** 이 카드가 겨냥한 `runtime_detection.cpp` 48/65(0.74)가 얼마나 올랐는지 모른다. QA-A-32 와 같은 이유로 로컬 OpenCppCoverage 가 32비트라 측정 불가 — leader dispatch 필요.
- **`ParseWindowSize` / `ParseSigmaThreshold` 는 죽은 코드로 남아 있다.** 호출자가 항상 `nullptr` 을 준다. 이 카드 범위 밖이라 손대지 않았고, 그만큼 미커버 줄이 남는다.
- **TPR/FPR 를 정량 지표로 측정하지 않았다.** 원본이 계산한 `CalculateTPR/CalculateFPR` 헬퍼는 옮기지 않았다. `GaussianNoiseKeepsFalsePositivesLow` 는 1% 상한이라는 느슨한 경계이지 REQ-P1A-013 이 말하는 FPR < 0.001% 를 검증하지 않는다.
- **AVX2 경로와의 교차 확인은 하지 않았다.** 새 케이스가 스칼라·AVX2 중 무엇을 탔는지 구분하지 않는다. 동등성 4케이스가 그 축을 담당하지만, 두 축을 함께 본 케이스는 없다.
- **ASan 재측정 없음.**

## 8. 잔여 위험 (Residual risk)

- **REQ-P1A-013 의 정량 요구(TPR ≥ 99.9%, FPR < 0.001%)는 여전히 검증되지 않는다.** 이번 케이스는 "심은 결함을 찾고 멀쩡한 픽셀을 건드리지 않는다" 는 정성 수준이다. 정량 검증에는 통계적으로 유의한 표본과 허용 오차 설계가 필요하며 별도 카드가 맞다.
- **창 크기·시그마를 공개 API 로 조정할 수 없다는 사실 자체가 위험**이다. 검출기 튜닝이 필요한 현장에서 재컴파일 말고는 방법이 없고, 파싱 코드는 있는데 호출되지 않아 "설정 가능하다" 는 인상만 남는다.
- **고정 시드 노이즈 케이스는 표준 라이브러리 구현에 의존한다.** `std::mt19937` 은 표준이 정한 결정론적 엔진이지만 `std::normal_distribution` 의 변환 방식은 구현정의다. 다른 표준 라이브러리에서는 다른 표본이 나올 수 있고, 그때 1% 상한이 흔들릴 수 있다.
- **내부 API 를 테스트에서 직접 호출한다.** `DetectDefectivePixel` 의 시그니처가 바뀌면 공개 계약이 그대로여도 이 테스트가 깨진다. 공개 경로로는 도달할 수 없으므로 감수한 결합이다.

## Card Cross-Check

| 마일스톤 | 카드 |
|---|---|
| 삭제된 22케이스 대조표 | QA-A-33 |
| 정답 기반 18케이스 (커밋 1) | QA-A-33 |
| 오류 경로 10케이스 (커밋 2) | QA-A-33 |
| ci-preprocess 재실측 | QA-A-33 |
| `runtime_detection.cpp` 커버리지 수치 | leader dispatch 필요 |
| TPR/FPR 정량 검증 | 신규 카드 필요 |
| `ParseWindowSize`/`ParseSigmaThreshold` 죽은 코드 처분 | 신규 카드 필요 |

## 인용 로그 (같은 디렉터리)

| 파일 | 내용 |
|---|---|
| `a33-deleted-suite.cpp.txt` | 삭제된 원본 468줄 (`e59825c^` 에서 복원) |
| `a33-deleted-case-list.txt` | 그 파일의 `TEST_F` 22건 목록 (행 번호 포함) |
| `a33-run1.log` | 정답 기반 케이스 첫 실행 — MAD 가정 실패 (exit=8) |
| `a33-green1.log` | 정정 후 547/547 PASS (exit=0) |
| `a33-run2.log` | 오류 경로 추가 후 557/557 PASS (exit=0) |
