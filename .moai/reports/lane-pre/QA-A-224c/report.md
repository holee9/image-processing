# QA-A-224c — 보정 전 쪽도 같은 측정 조건으로: 죽은 센서(raw 전부 0)는 합격하지 않는다 (Refs #242)

기준: dev/preprocess `cb1b7f92`(QA-A-224b) 위. 코드 `xpe_verify_metrics.cpp`, 헤더 `preprocess_api.h`, 시험 `test_verify_metrics.cpp`. 증거: `evidence/`. 푸시하지 않음.

## 결론

1. **리더 결정 (b) 구현**: 보정 전 프레임의 평균이 유한·양수가 아니면(죽은 센서의 전부 0) `overall_pass = false`, `snr_improvement_db = 0.0`, `measured_mask` 의 `XPE_METRIC_PRNU`·`XPE_METRIC_SNR` 해제. 224b 가 보정 후에 세운 조건과 같은 규칙이라 두 쪽이 대칭이다. 정상 경로(평탄 → 평탄 합격, 완벽한 보정 합격 등)는 그대로다.
2. **다른 검증 함수의 비유한 입력은 고치지 않고 측정했다**(§3). **충돌은 없다**: 15개 케이스 모두 별도 프로세스에서 `exit=0`.
3. 측정에서 나온 가장 큰 사실: `xpe_verify_defect` 의 합격은 **보정 영상의 값을 전혀 보지 않는다** — 영상이 전부 NaN 이어도 `pass = true` 다(§3.1).

## 1. 수정

`xpe_verify_gain`: `before_measurable = isfinite(mean_before) && mean_before > 0` 를 224b 의 `after_measurable` 옆에 두고,

- `overall_pass = before_measurable && after_measurable && …`,
- `measured_mask` 의 `PRNU | SNR` 는 둘 다 측정 가능할 때만,
- 무한 개선(200 dB)은 이미 `prnu_before > 0`(= 보정 전이 측정 가능하고 퍼짐이 있음)을 요구하므로 그대로.

보정 전이 UINT16 이라 **NaN·무한대·음수일 수 없고** 평균 조건을 못 지키는 길은 0 하나뿐이다. 그래서 카드가 말한 "raw 에 NaN 하나"·"raw 음수 평균" 시험은 만들 수 없다(타입이 막는다). 적용한 곳은 헤더 `@note` 와 시험 머리 주석에 같이 적었다.

## 2. 시험과 반증

신규 2건. 수정 전(`evidence/10`): `VerifyGain_AllZeroRawFrameDoesNotPass` 만 **빨강**(raw 전부 0 + 평탄 1000 보정 후 → 합격하던 것). 수정 후(`evidence/11`): `*Verify*` 47건 전부 통과.

| 시험 | 입력 | 수정 전 |
|---|---|---|
| `VerifyGain_AllZeroRawFrameDoesNotPass` | raw 전부 0, 보정 후 평탄 1000 → 불합격, `snr_improvement_db == 0.0`, PRNU·SNR 비트 없음, 게인 커버리지 비트는 있음 | **빨강** |
| `VerifyGain_AllZeroRawAndAllZeroCorrectedFramesDoNotPass` | 둘 다 전부 0 → 불합격 | 초록(224b 가 보정 후 쪽으로 이미 막음 — 통제) |

기존 정상 경로 시험(평탄 → 평탄 합격, 완벽한 보정 합격, 개선 없음 불합격 등)은 그대로 초록이다.

### 반증 (한 번에 하나, 전체 빌드, `evidence/30_arm_*`; 마지막에 복원 + 다시 빌드)

| 손상 | 빨강이 된 시험 |
|---|---|
| b1 보정 전 측정 조건을 통째로 제거(항상 참) | `VerifyGain_AllZeroRawFrameDoesNotPass` **하나만** |
| b2 판정식에서만 보정 전 조건 제거 | 같은 시험 하나만(마스크 단언으로 잡힘 아님 — 판정이 합격으로 바뀜) |
| b3 마스크 조건에서만 제거 | 같은 시험 하나만(마스크 단언) |

카드가 말한 "보정 전 측정 조건을 빼면 raw 0 시험만 빨강" 과 같다. b2·b3 가 각각 판정과 마스크를 따로 지킨다.

## 3. 다른 검증 함수의 비유한 입력 — 측정 결과 (고치지 않았다, 225 결정 항목)

`evidence/12`(스크립트 `evidence/21`), 32×32, **케이스마다 별도 프로세스**, 실제 DLL. 모든 케이스가 `exit=0`(접근 위반 없음).

### 3.1 `xpe_verify_defect` — 보정 영상 값을 보지 않는다

| 입력(보정 영상, 결함 맵 3화소 고정) | `overall_pass` | `correction_error` |
|---|---|---|
| 대조: 평탄 1000, 비유한 없음 | **True** | 0 |
| 결함 위치 하나가 NaN | **True** | **nan** |
| 정상 위치 하나가 NaN / +inf / -inf | **True** | 0 |
| **전부 NaN** | **True** | 0 |

합격 판정은 결함 **맵**의 밀도(`defect_density < 5%`)뿐이라 보정 영상이 전부 NaN 이어도 합격한다. 필드 `correction_error` 만 결함 위치의 NaN 에서 NaN 이 되고 판정에는 영향이 없다. 이것은 224·224b 가 gain 쪽에서 막은 것과 **같은 계열의 의미 문제**이지만, 이 함수의 합격은 애초에 영상 값에 의존하지 않으므로 "측정 불가" 규칙을 어디에 붙일지부터 정해야 한다(예: 보정 영상이 전부 유한이어야 한다).

### 3.2 `xpe_verify_gain`

| 입력 | `overall_pass` | 비고 |
|---|---|---|
| 게인 맵 한 화소가 NaN / +inf | **True** | 무효 게인 1 → 커버리지 0.999 ≥ 0.99. 설계된 동작(1% 허용)이고 충돌 없음 |
| 보정 후 +inf 화소 | False | `prnu_after = nan`, mask 8 |
| 보정 후 -inf 화소 | False | mask 8 |
| 보정 후 NaN 화소 | False | mask 8 (224b 수정 뒤) |

### 3.3 `xpe_verify_pipeline`

| 입력(최종 영상) | `overall_pass` | `snr_improvement_db` |
|---|---|---|
| NaN 화소 | False | **nan** |
| +inf 화소 | False | **-inf** |
| -inf 화소 | False | **-inf** |

판정은 모두 실패이고 **필드가 NaN·-inf** 다(224 가 "보장 밖" 으로 둔 그대로). `measured_mask` 는 SNR 비트를 켠 채다.

### 3.4 `xpe_verify_offset`

입력이 둘 다 UINT16 이라 비유한이 있을 수 없다(타입이 막는다). 정상 호출이 도는 대조만 했다.

### 3.5 이 측정이 말해 주는 것

- **충돌은 `compute_flatness` 하나**였고 224b 에서 이미 막았다. 이 15개 케이스에는 접근 위반이 없다.
- **판정 의미**가 남은 문제다: ① `verify_defect` 는 영상 값 무관(§3.1), ② `verify_pipeline` 의 필드가 비유한(§3.3), ③ `verify_gain` 은 224b 로 정리됐다. 225 의 "검증 함수 입력 비유한" 결정(소비자는 입구에서 거부 vs 측정값으로 보고)에 이 표가 근거다.

## 4. 검증 (`evidence/verify/`, 반증 뒤 다시 빌드)

빌드 `BUILD_EXIT=0`, preprocess 시험 **917**(기존 915 + 신규 2) 중 909 통과·8 건너뜀(기존), 셔플 909 동일, 할당 실패 시험 65, common 69·12, ctest 총 **1104**(1102 + 2), 공개 export 변화 0, 헤더 문서 0건, 프리셋 일치 12/12.

## 5. 미검증 (Gaps)

- 비유한 측정은 32×32, 한 화소(또는 전부) 위치만이다. 다른 크기·여러 위치·`compute_robust_mean` 의 정렬이 NaN 에서 보장하지 않는 순서(strict weak ordering 위반)를 의도적으로 건드리는 입력은 보지 않았다 — 충돌이 없었다는 것이 모든 입력에서 없다는 뜻은 아니다.
- `xpe_verify_defect` 에서 결함 맵의 값(UINT8)이 이상할 때(예: 모두 1)의 동작은 이 카드의 범위 밖이라 보지 않았다.
- 보정 전 평균이 **양수이지만 매우 작은** 경우(예: 한 화소만 1)의 PRNU 는 측정 가능으로 취급된다 — "측정 가능" 의 하한을 두지 않았다.

## 6. 잔여 위험

- `xpe_verify_gain` 은 이제 보정 전·후 어느 쪽이든 측정 불가면 실패한다. "죽은 센서 영상이 합격" 을 의도한 호출자가 있다면 달라진 동작이지만, 저장소 안의 호출자는 시험뿐이다(QA-A-189 가 확인한 범위).
