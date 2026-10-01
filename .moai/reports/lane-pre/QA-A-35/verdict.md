# QA-A-35 검증 보고서 — FUNC-033 품질 메타데이터 배선 (#140 #120)

- 레인: Lane A (pre) / 브랜치 `dev/preprocess`
- 카드: QA-A-35 (`.moai/lanes/pre/inbox/QA-A-35.md`)
- 정본: `docs/calibration/SRS-CALIB-001_Software_Requirements_Specification.md:170` **SRS-CALIB-FUNC-033**, `RTM-CALIB-001:76`
- 커밋 3건: `2bf1e8e`(R² 계산·기록) → `beb30c6`(XCal 기록·복원) → `28992f5`(테스트)

## 1. 주장 (Claim)

1. **임계는 SRS 원문에 있다 — 멈출 사유 없음.** §FUNC-033 (2) 원문: *"If `fit_r_squared < 0.999` after fitting, system shall log `XPE_WARN_CALIB_POOR_FIT` and include recommendation to increase dose levels or check detector stability."* `XPE_CALIB_R_SQUARED_GATE = 0.999` 로 상수화하고 주석에 인용을 붙였다. **값을 발명하지 않았다.**
2. A-34 가 삭제한 죽은 네임스페이스를 되살리지 않고, 살아 있는 경로(`xpe_calib_generate_gain_polynomial`)에 새로 배선했다.
3. R² 를 실제 적합 잔차에서 계산하고, 메타를 전역 저장소에 기록하며, XCal config JSON 에 써서 로드 시 복원한다.
4. 테스트 10건 전부 **프로덕션을 호출한다** — A-34 가 지운 가짜 4건의 이름을 재사용하되 내용은 정반대다.
5. ci-preprocess **556 → 566/566 PASS**, ci-common **69/69 PASS**, **export 50 names, diff 빈 출력**.

## 2. SRS 필드 대조표

| SRS 필드 (§FUNC-033 (1)) | 채우는 위치 | 저장 위치 | 상태 |
|---|---|---|---|
| `calibration_mode` | `xpe_calib_mode.cpp` `xpe_calib_record_quality_meta` (현재 모드에서) | 구조체 + config JSON | ✅ |
| `actual_dose_levels` | `xpe_calib_generate_gain.cpp` `quality.num_points = num_levels` | 구조체(`num_points`) + JSON | ✅ |
| `polynomial_degree` | 같은 곳, 픽셀별 최종 차수의 최대값 | 구조체 + JSON | ✅ |
| `fit_r_squared` | 같은 곳, `1 - SS_res/SS_tot` | 구조체(`r_squared`) + JSON | ✅ |
| `max_residual_pct` | 픽셀 루프에서 누적한 최대 잔차 비율 | **JSON 만** (구조체에 필드 없음) | ⚠ §5 |
| `mean_residual_pct` | 같은 루프의 평균 | **JSON 만** | ⚠ §5 |
| `acquisition_duration_s` | — | — | ❌ 미구현 (§5) |
| `detector_temperature_c` | — | — | ❌ 미구현 (§5) |
| §(2) 게이트 + 경고 | `xpe_calib_record_quality_meta` + `xpe_alert_push` | 구조체(`calibration_pass`) | ✅ |
| §(3) 이전 비교 | `previous_r_squared` 로 직전 R² 이월 | 구조체 | ⚠ 부분 (§5) |
| §(4) 모드별 필드 | — | — | ❌ 미구현 (§5) |
| §(5) XCal 헤더 JSON 저장 | 생성기가 config JSON 기록, 로더가 복원 | XCal config 블록 | ✅ |

**필드명 표기**: JSON 키는 **SRS 원문 표기**(`fit_r_squared`, `actual_dose_levels`)를 따랐고, C 구조체 멤버명(`r_squared`, `num_points`)과 다르다. 구조체는 기존 공개 ABI 라 바꿀 수 없고, 파일 포맷은 SRS 가 정본이기 때문이다.

## 3. 구현 요지

### R² 계산 (커밋 1)

픽셀별 적합 직후 Horner 로 예측값을 평가해 `SS_res` / `SS_tot` 를 전 픽셀·전 도즈에 걸쳐 누적하고, 마지막에 `R² = 1 − SS_res/SS_tot` 를 구한다.

완전 평탄 입력은 `SS_tot == 0` 이라 0/0 이 되므로 **정의상 1.0** 으로 둔다 — SRS 가 "1.0 for single-point by definition" 이라 한 것과 같은 취급이다.

### 경고 채널 — SRS 가 지정한 코드가 없다

§(2)는 `XPE_WARN_CALIB_POOR_FIT` 을 로그하라고 한다. **그 코드는 `xpe_error.h` 에 존재하지 않는다**(`grep -rn "XPE_WARN_"` → 일치 0). 새 코드를 추가하면 공개 헤더 변경이고, 카드는 "새 export 필요하면 멈추고 보고" 다.

그래서 **알림 큐**(모듈의 유일한 운영자 가시 채널)에 그 식별자를 접두사로 단 `XPE_ALERT_WARNING` 을 올렸다. SRS 가 요구한 "recommendation" 도 같은 메시지에 담았다:

```
XPE_WARN_CALIB_POOR_FIT: fit_r_squared=0.412345 below 0.999
(max_residual=87.500%, mean_residual=41.667%);
increase dose levels or check detector stability
```

**판정 요청**: 이것이 §(2)를 충족하는지, 아니면 `XPE_WARN_CALIB_POOR_FIT` 을 실제 코드로 추가해야 하는지는 계약 결정이다.

### 구버전 호환 — 분기가 아니라 구조에서

메타를 **헤더 구조체가 아니라 config JSON** 에 넣었으므로 152바이트 레이아웃이 그대로다. 포맷 버전을 올리지 않았고, QA-A-35 이전 파일은 키가 없어 각 필드가 "자료 없음" 값(-1.0 / 0)을 갖는다. 호환이 별도 코드 경로가 아니라 설계에서 나온다.

### 게이트 판정은 파일을 믿지 않는다

로드 시 `calibration_pass` 를 **파일에서 읽지 않고** 복원한 R² 로 다시 계산한다. `fit_r_squared=0.5` 인데 `calibration_pass=1` 이라고 적힌 파일을 로드하면 판정은 0 이다 — `LoadedGateVerdictIsRecomputedNotTrusted` 가 이것을 고정한다.

## 4. 증거 (Evidence)

| 단계 | 결과 | 로그 |
|---|---|---|
| 커밋 1 (R² 계산·기록) | 556/556 PASS | `a35-item1b.log` (exit=0) |
| 커밋 2 (XCal 기록·복원) | 556/556 PASS | `a35-item2.log` (exit=0) |
| 커밋 3 첫 실행 | **2건 실패** (레벨 파일 재사용 → −9 IO_FAILED) | `a35-item3.log` (exit=8) |
| 커밋 3 정정 후 | **566/566 PASS** | `a35-item3b.log` (exit=0) |
| ci-common | **69/69 PASS** | `a35-common.log` (exit=0) |
| export | **50 functions / 50 names, diff 빈 출력** | `a35-dumpbin.log`, `a35-export-diff.txt` |

변경 규모 (`a35-diffstat.txt`): 6 files, **+520 / −3**.

### 실측으로 정정한 가정 2건

1. **`/WX` 가 잡은 미사용 변수** — 커밋 1 초안에서 `mean_residual_pct` 를 계산만 하고 쓰지 않아 C4189 가 오류로 승격돼 빌드가 멈췄다(A-27 이 확인한 `/WX` 가 실제로 동작한 사례). 잔차 수치를 경고 메시지에 함께 싣도록 고쳤고, 그것이 SRS 가 요구한 "recommendation" 과도 맞았다.
2. **임시 파일 재사용 −9** — 한 케이스에서 `generatePoly` 를 두 번 부르면 두 번째 레벨 파일 쓰기가 `IO_FAILED` 로 실패했다. `write_xcal_file` 이 `.tmp` 를 rename 하는데 첫 호출이 남긴 파일이 막았다. 레벨 파일명에 호출별 태그를 붙이고 사유를 주석에 남겼다.

## 5. 미검증 (Gaps)

- **SRS 필드 4개가 미구현이다** — `acquisition_duration_s`, `detector_temperature_c` (§1), §(4) 모드별 필드(`gain_uncertainty`, `per_point_r_squared[]`). 앞의 둘은 생성기가 그 정보를 받지 않아 배선할 소스가 없고, §(4)는 `XpeCalibQualityMeta` 에 담을 자리가 없다(배열 필드 = 공개 ABI 변경). **이 카드는 FUNC-033 을 완결하지 않았다.**
- **§(3) 비교 지표 3개 미구현** — `dark_bias_delta`, `prnu_delta_pct`, `defect_count_delta`. 구현한 것은 `previous_r_squared` 이월뿐이고, SRS 가 나열한 세 지표는 계산하지 않는다.
- **`max_residual_pct` / `mean_residual_pct` 는 JSON 에만 있다.** `xpe_calib_get_quality_meta` 로는 읽을 수 없다 — 구조체에 필드가 없기 때문이다. 로더도 이 두 키를 파싱하지 않는다.
- **`xpe_calib_generate_gain`(비-다항 경로)은 메타를 기록하지 않는다.** 배선한 것은 다항 생성기뿐이다. SRS 는 "every generated XCal gain file" 이라고 했으므로 이 경로도 대상이다.
- **`XCAL_TYPE_GAIN_POLY` 파일을 읽는 로더가 없다**(`grep` 결과 생성기 외 참조 0). 즉 다항 파일에 기록한 메타를 공개 API 로 되읽는 경로가 없어, 왕복 테스트는 GAIN 타입 파일로 했다.
- **R² 는 전 픽셀 풀링 값이다.** 픽셀별 R² 분포(§(4)의 `per_point_r_squared[]`가 요구하는 것)는 계산하지 않는다. 일부 픽셀만 나빠도 전체 R² 는 통과할 수 있다.
- **ASan 재측정 없음.**

## 6. 잔여 위험 (Residual risk)

- **부분 구현이 완전 구현으로 오인될 수 있다.** `xpe_calib_get_quality_meta` 가 이제 값을 돌려주므로 FUNC-033 이 충족된 것처럼 보이지만, §5의 필드 7개가 비어 있다. RTM `UT-META-001~003` 이 무엇을 요구하는지 대조하지 않았다.
- **경고가 알림 큐로만 간다.** 큐를 폴링하지 않는 호출자는 R² 게이트 실패를 알 수 없다. 생성 함수는 여전히 `XPE_OK` 를 반환한다 — SRS 가 "log" 라고만 했으므로 규격 위반은 아니지만, 조용한 실패에 가까운 형태다.
- **풀링 R² 의 임상적 의미를 검증하지 않았다.** 0.999 라는 임계가 전 픽셀 풀링 값에 대해 의도된 것인지, 픽셀별 값에 대한 것인지 SRS 원문만으로는 단정할 수 없다. 현재 구현은 풀링을 택했고 그 선택을 문서화했다.
- **JSON 키와 구조체 멤버명이 다르다.** 파일을 읽는 쪽과 API 를 쓰는 쪽이 서로 다른 이름을 보게 되며, 한쪽만 바뀌면 조용히 어긋난다.

## Card Cross-Check

| 마일스톤 | 카드 |
|---|---|
| SRS 임계 인용 확인(멈춤 불필요) | QA-A-35 |
| R² 계산 + 메타 기록 (커밋 1) | QA-A-35 |
| XCal 기록·복원 + 구버전 호환 (커밋 2) | QA-A-35 |
| RED→GREEN 테스트 10건 (커밋 3) | QA-A-35 |
| export diff 0 + 양쪽 프리셋 재실측 | QA-A-35 |
| **`XPE_WARN_CALIB_POOR_FIT` 코드 신설 여부** | **leader 판정 필요** |
| SRS 미구현 필드 7개(§1 2개, §3 3개, §4 2개) | 신규 카드 필요 |
| 비-다항 `xpe_calib_generate_gain` 메타 기록 | 신규 카드 필요 |
| `XCAL_TYPE_GAIN_POLY` 로더 부재 | 신규 카드 필요 |

## 인용 로그 (같은 디렉터리)

| 파일 | 내용 |
|---|---|
| `a35-item1b.log` | 커밋 1 후 556/556 PASS |
| `a35-item2.log` | 커밋 2 후 556/556 PASS |
| `a35-item3.log` | 커밋 3 첫 실행 — 2건 실패(−9 IO_FAILED) |
| `a35-item3b.log` | 정정 후 566/566 PASS |
| `a35-common.log` | ci-common 69/69 PASS |
| `a35-dumpbin.log`, `a35-export-diff.txt` | export 50/50, diff 빈 출력 |
| `a35-diffstat.txt` | 커밋 3건 합계 `git diff --stat` |
