# QA-A-86 — `#140` FUNC-033: 지금 무엇이 남았는가 (코드 기준)

**카드**: `.moai/lanes/pre/inbox/QA-A-86.md` · **브랜치**: `dev/preprocess` · **HEAD**: `9be1cc9` · **코드 변경 없음 · 새 실행 없음**

---

## 1. 주장 (Claim)

### 항목별 상태

| 항목 | 상태 | 근거 |
|---|---|---|
| (1) `calibration_mode` | **구현+시험(부분)** | 두 경로 파일에 기록·로드 복원. 시험은 키 존재만, 값 단언 없음 |
| (1) `actual_dose_levels` | **구현+시험** | 단일: 값 `1` + 왕복. 다항: 생성 직후 `num_points==4` |
| (1) `polynomial_degree` | **구현+시험 — 다항 경로에 불일치** | 단일: 값 `0` + 왕복. 다항: 파일엔 `max_degree`, 구조체엔 `highest_degree` (§2) |
| (1) `fit_r_squared` | **구현+시험** | 단일: 상수 1.0 + 왕복. 다항: 잔차 계산 + 값 단언 |
| (1) `max_residual_pct` | **구현만** | 파일에 기록. **로드 시 복원 안 함**(구조체에 필드 없음). 시험은 키 존재만 |
| (1) `mean_residual_pct` | **구현만** | 위와 같음 |
| (1) `acquisition_duration_s` | **없음** | 코드 0건. SRS 기록: 호출자 몫 |
| (1) `detector_temperature_c` | **없음** | 코드 0건. SRS 기록: 호출자 몫 |
| (2) R² 게이트 `< 0.999` | **구현+시험** | 상수·판정·경고·로드 재판정 전부 시험 있음 |
| (3) `dark_bias_delta` · `prnu_delta_pct` · `defect_count_delta` | **없음** | 코드·시험 0건. SRS 기록: 다른 계층 |
| (4) `gain_uncertainty` | **정의 불가** | 코드 0건. SRS 기록: 잡음 모델·단위 미정의 |
| (4) `per_point_r_squared[]` | **정의 불가** | 코드 0건. 도스 점당 표본 1개 — 코드와 일치 |
| (5) XCal 헤더 JSON 저장 | **구현+시험 — 저장 위치가 SRS 문언과 다름** | 헤더 뒤 config JSON 블록. 헤더에 예약 바이트 필드가 없다 |

### 주장

| # | 주장 |
|---|---|
| C1 | **SRS 기록(L218·L227·L231·L237)은 현재 코드와 맞는다.** 인용 행 3개(`:301`, `:563`, `load_gain:108`)와 서술 3개(다항 경로 `metadata_json` 없음, 다항 출력은 계수 배열, 도스 점당 표본 1개)를 확인했다 |
| C2 | **기록에 없는 것 셋을 찾았다** (§2) — 다항 `polynomial_degree` 의 두 값, 잔차 두 필드의 로드 미복원, (5) 저장 위치 |
| C3 | **"구현만"인 필드의 시험은 키 존재만 본다.** 잔차 두 필드는 두 경로 모두 값 단언이 없다 |
| C4 | **(3) 을 다룬다고 읽히는 이름의 시험 2개는 `previous_r_squared` 를 본다** — (3) 의 세 필드가 아니다 |
| C5 | `#140` 에 사실 코멘트를 달았다. 게시 전 인용 행 36개를 기계적으로 대조해 3개를 고쳤다 |

---

## 2. 증거 (Evidence)

### SRS 기록 대조 (C1)

| SRS 기록 | 코드 |
|---|---|
| L218 "`xpe_calib_generate_gain.cpp:301, :563`" | `:301` `const bool gate_passed = xpe_calib_record_quality_meta(quality);` / `:563` 같음 |
| L218 "`xpe_calib_load_gain.cpp:108`" | `:108` `xpe_calib_apply_quality_meta_json(json.c_str());` |
| L220 "8개 중 6개 기록" | 두 경로 JSON 이 여섯 키를 모두 쓴다(단일 `:305-307`, 다항 `:608-610`) |
| L227 "다항 경로에는 `metadata_json` 인자조차 없습니다" | 다항 시그니처 `(gain_file_paths, dose_levels, num_levels, max_degree, output_path)` (`:366-371`) |
| L231 "다항 경로의 출력은 게인 맵이 아니라 계수 배열" | `hdr.type = XCAL_TYPE_GAIN_POLY` (`:590`), 페이로드 `coeff_array` |
| L237 "한 도스 점에 표본이 하나" | `y_vals[i] = gain_maps[i][pix]` (`:454`) — 화소·도스 점마다 값 하나 |

### 기록에 없던 것 1 — 다항 경로 `polynomial_degree` (C2)

```
xpe_calib_generate_gain.cpp:559   quality.polynomial_degree = static_cast<uint8_t>(highest_degree);
xpe_calib_generate_gain.cpp:608   "\"calibration_mode\":%d,\"actual_dose_levels\":%d,"
                     (앞줄)       "{\"polynomial_degree\":%d,\"num_coefficients\":%d,..."
xpe_calib_generate_gain.cpp:611   static_cast<int>(max_degree),        <- JSON 의 polynomial_degree
```

`highest_degree` 는 화소마다 `max_degree` 부터 내려가며 적합하고, `validate_monotonicity` 를 통과한 차수의 최댓값이다(`:457-525`). 모든 화소가 최고 차수에서 단조성 검사에 떨어지면 `highest_degree < max_degree` 가 된다.

그러면:
- 생성 직후 `xpe_calib_get_quality_meta().polynomial_degree` = `highest_degree`
- 같은 파일 로드 후 = `max_degree` (`xpe_calib_mode.cpp:231` 이 JSON 에서 읽음)

SRS (1) 은 `polynomial_degree` 를 "fitted polynomial degree" 로 정의한다.

**시험이 이 차이를 지나치는 이유**: 다항 경로 필드 시험(`test_calib_quality_meta.cpp:299-310`)은 키 존재만 보고, 다항 왕복 시험(`:314-327`)은 **생성된 파일이 아니라 손으로 쓴 JSON**(`writeGainWithJson`)을 로드한다.

**이것은 코드 읽기다. 이 경로를 실행하지 않았다.** 차수가 실제로 내려가는 입력을 만들어 보지 않았다.

### 기록에 없던 것 2 — 잔차 두 필드 (C2, C3)

```
XpeCalibQualityMeta (preprocess_api.h:968-978):
  calibration_mode, polynomial_degree, num_points, r_squared,
  calibration_timestamp, detector_serial[32], firmware_version[16],
  calibration_pass, previous_r_squared
```

잔차 필드가 구조체에 없다. `xpe_calib_apply_quality_meta_json` 은 `fit_r_squared`·`polynomial_degree`·`actual_dose_levels`·`calibration_mode` 넷만 읽는다(`xpe_calib_mode.cpp:226-243`). **파일에 쓰이지만 API 로는 돌아오지 않는다.**

시험의 잔차 언급:

```
test_calib_gain_poly_load.cpp:249   "max_residual_pct", "mean_residual_pct"  <- 키 목록
test_calib_quality_meta.cpp:305     "max_residual_pct", "mean_residual_pct"  <- 키 목록
(그 외: 주석 2줄)
```

**값을 보는 단언은 없다.** 단일 경로의 상수 `0.000000` 도, 다항 경로의 계산값도 시험되지 않는다.

### 기록에 없던 것 3 — (5) 저장 위치 (C2)

```
xcal_format.h:100-112  XCalFileHeader:
  magic[4], version, type, pixel_format, width, height,
  created_epoch_ms, expiry_epoch_ms, session_id[64],
  config_json_len, payload_len, sha256[32]
```

**예약 바이트 필드가 없다.** 메타데이터는 헤더 뒤 config JSON 블록에 쓰이고 헤더는 길이만 가진다(`hdr.config_json_len` — 단일 `:348`, 다항 `:625`). SRS (5) 문언은 "stored in XCal file header section (JSON-encoded in reserved header bytes)".

### C4 — 이름과 내용

```
test_calib_quality_meta.cpp:269  TEST_F(CalibQualityMetaTest, PreviousCalibration_Comparison_Regression)
                          :279   EXPECT_DOUBLE_EQ(firstR2, secondMeta.previous_r_squared)
test_calib_quality_meta.cpp:285  TEST_F(CalibQualityMetaTest, PreviousCalibration_Comparison_Stable)
                          :292   EXPECT_NEAR(meta.previous_r_squared, meta.r_squared, 0.01)
```

(3) 의 `dark_bias_delta`·`prnu_delta_pct`·`defect_count_delta` 는 `src`·`include`·`tests` 모두 0건.

### 필드 계수 (같은 실행)

```
calibration_mode        src=9 test=6
actual_dose_levels      src=6 test=8
polynomial_degree       src=9 test=10
fit_r_squared           src=7 test=8
max_residual_pct        src=8 test=4
mean_residual_pct       src=7 test=4
acquisition_duration_s  src=1 test=0     (src 1 = 주석 :293)
detector_temperature_c  src=1 test=0     (src 1 = 주석 :293)
gain_uncertainty        src=0 test=0
per_point_r_squared     src=0 test=0
dark_bias_delta         src=0 test=0
prnu_delta_pct          src=0 test=0
defect_count_delta      src=0 test=0
```

범위: `modules/preprocess/src`, `modules/preprocess/include`, `modules/preprocess/tests`. 대조군은 같은 실행의 위 여섯 줄(0 이 아닌 값).

### C5 — 코멘트

```
https://github.com/holee9/image-processing/issues/140#issuecomment-5706808991
check: n=4 state=OPEN has625=true has633=false
```

게시 전, 코멘트에 인용한 행 36개를 `sed -n` 으로 한 줄씩 대조했다. 어긋난 것 3개:
- `:633` → `config_json_len` 이 아니었다(`catch` 줄). 실제 단일 `:348`, 다항 `:625`
- `:457-526` → `:526` 은 닫는 괄호. `:457-525` 로
- `xpe_calib_mode.cpp:250-251` → `:250` 은 주석. `:251-252` 로

---

## 3. baseline 귀속 (Baseline-attribution)

- 모든 인용은 `origin/main` 병합 후 HEAD `9be1cc9` 의 파일을 `grep -n`·`sed -n` 으로 읽은 원문이다.
- 필드 계수는 한 번의 실행이다.
- **빌드·시험을 실행하지 않았다** — 카드가 "코드에서 확정"을 요구했고 코드 변경이 없다.

---

## 4. 미검증 (Gaps)

- **다항 경로 차수 불일치를 실행으로 확인하지 않았다.** 모든 화소가 최고 차수에서 단조성에 떨어지는 입력을 만들어 보지 않았다. 코드 경로상 가능하다는 것까지만이다.
- **`fit_polynomial_ls` 가 `XPE_OK` 가 아닌 값을 내는 조건**은 보지 않았다(차수가 내려가는 다른 경로).
- **`calibration_mode` 의 값 단언이 정말 없는지**는 두 시험 파일만 봤다. 다른 시험 파일에서 값을 보는지는 계수(test=6)만 있고 내용은 읽지 않았다.
- **`detector_serial`·`firmware_version`·`calibration_timestamp`** (구조체에 있고 SRS (1) 에 없는 필드)는 조사 범위 밖이다.
- **SRS 의 "When overwriting an existing calibration file" 조건**(3) — 두 경로가 기존 파일을 감지하는지는 보지 않았다.
- **push 하지 않았다.** (이 카드는 커밋 없음)

---

## 5. 잔여 위험 (Residual-risk)

- **다항 파일을 로드한 뒤의 `polynomial_degree` 는 요청 상한일 수 있다.** 적합이 차수를 낮춘 교정에서, 로드된 메타데이터가 실제보다 높은 차수를 말한다. 시험은 이 경로를 지나지 않는다.
- **잔차 두 필드는 "기록됨"으로 분류되지만 값이 한 번도 검증되지 않았다.** 단일 경로의 `0.000000` 은 상수이고 다항 경로의 계산값은 단언이 없다 — 계산이 틀려도 모든 시험이 통과한다.
- **(5) 의 문언과 구현이 다르다.** 헤더 크기 고정이 중요한 소비자가 문언을 믿으면 예약 영역을 찾는다.
- **시험 이름 "Comparison" 이 (3) 이 다뤄진 것처럼 읽힐 수 있다.**

---

Refs #140
