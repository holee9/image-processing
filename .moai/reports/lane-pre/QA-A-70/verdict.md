# QA-A-70 — 교정 품질 메타데이터 (#140)

**카드**: `.moai/lanes/pre/inbox/QA-A-70.md` · **브랜치**: `dev/preprocess` · **정본**: `docs/calibration/SRS-CALIB-001` §SRS-CALIB-FUNC-033

---

## 0. 먼저 — 카드의 전제가 틀렸습니다

> `xpe_calib_get_quality_meta` 가 있고 채우는 코드는 호출자 0인 죽은 네임스페이스에만 있었습니다(A-34 에서 삭제). … 지금은 초기값만 돌려줍니다. 삭제된 테스트 4건은 상수 비교라 검증하지 않았고요.

**A-34 까지는 맞습니다. 그러나 QA-A-35 가 이미 배선했습니다.**

```
$ grep -rn "xpe_calib_record_quality_meta" --include=*.cpp modules/
modules/preprocess/src/xpe_calib_generate_gain.cpp:301:  const bool gate_passed = xpe_calib_record_quality_meta(quality);
modules/preprocess/src/xpe_calib_generate_gain.cpp:563:  const bool gate_passed = xpe_calib_record_quality_meta(quality);
modules/preprocess/src/xpe_calib_load_gain.cpp:108:  xpe_calib_apply_quality_meta_json(json.c_str());
```

두 생성 경로 모두에서 호출되고, 파일에서 다시 읽습니다. R² 는 상수가 아니라 잔차에서 계산됩니다(`xpe_calib_generate_gain.cpp:550`). 그리고 **검증도 있습니다** — `test_calib_quality_meta.cpp` 10건 + `test_calib_gain_poly_load.cpp` 의 단일점 메타 확인.

**그래서 이 카드는 "배선"이 아니라 "이미 있는 것을 검증하고, 남은 것을 가려내는" 카드가 됐습니다.** 아래가 그 결과입니다.

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | FUNC-033 의 **(2) 게이트와 (5) XCal 기록은 구현되어 있고 실제로 검증된다**. 읽어서가 아니라 반증으로 확인했다 |
| C2 | 기존 단언들이 **상수 반환을 통과시키지 않는다**. 다만 "통과" 쪽 단언 하나만으로는 통과시킨다 — 그것을 측정으로 보였다 |
| C3 | 그래서 **움직임 단언**을 하나 추가했다: 다른 것을 고정하고 적합 이탈만 키우면 R² 가 단조 감소해야 한다 |
| C4 | FUNC-033 (1) 8개 필드 중 **6개가 기록되고 2개는 기록되지 않는다** — 현재 시그니처로 **도달하지 않는 값**이다 |
| C5 | FUNC-033 (3)·(4) 는 **구현되어 있지 않고, 해석 없이는 구현할 수 없다**. 해석하지 않고 올린다 |
| C6 | 전체 640/69 통과, 경고 0 |

---

## 2. 증거 (Evidence)

### C1 · C2 — 반증으로 확인한 것

카드가 지정한 두 반증을 그대로 했다. 매번 `BUILD_EXIT=0` 확인.

**반증 1 — R² 를 상수 `1.0` 으로:**

```
[       OK ] R2QualityGate_Pass_WhenAboveThreshold      <-- 통과한다
[  FAILED  ] R2QualityGate_Fail_WhenBelowThreshold
[  FAILED  ] PreviousCalibration_Comparison_Regression
[       OK ] LoadedGateVerdictIsRecomputedNotTrusted
```

**반증 2 — 게이트 상수를 `0.999` → `-1.0` 으로:**

```
[       OK ] R2QualityGate_Pass_WhenAboveThreshold      <-- 통과한다
[  FAILED  ] R2QualityGate_Fail_WhenBelowThreshold
[       OK ] PreviousCalibration_Comparison_Regression
[  FAILED  ] LoadedGateVerdictIsRecomputedNotTrusted
```

**두 주입이 서로 다른 테스트를 깬다.** 상수 R² 는 값이 데이터를 따라간다는 주장을 깨고(`Comparison_Regression`), 게이트 이동은 임계 의미를 보는 주장을 깬다(`LoadedGateVerdict...`). 카드의 판정 기준 — "둘 다 실패하면 두 단언이 같은 것을 보고 있다" — 에 걸리지 않는다.

**그리고 카드가 경고한 형태가 실제로 확인됐다**: `R2QualityGate_Pass_WhenAboveThreshold` 는 **두 주입 모두에서 통과한다.** 한쪽만 있었으면 상수 반환이 지나갔을 것이다. 양쪽을 다 보는 것이 이 스위트를 쓸모 있게 만든다.

### C3 — 추가한 것: 움직임 단언

기존 두 게이트 테스트는 **서로 다른 입력으로 한 쪽씩** 단언한다. 그것만으로는 "좋은 입력엔 1.0, 나쁜 입력엔 0.5 를 돌려주는 표"도 통과한다. 그래서 다른 것을 전부 고정하고 하나만 바꾸는 테스트를 넣었다(QA-B-58 의 형태):

`R2FallsAsTheSeriesDepartsFromTheFit` — 도스 5단계, 같은 도스, **선형** 적합 고정. 직선에서 벗어나는 진폭만 0 → 0.25 → 0.5 → 0.75 로 키우고, **R² 가 단계마다 떨어질 것**과 마지막에는 게이트 아래로 내려갈 것을 요구한다.

적합 차수를 선형으로 고정한 이유를 코드에 적었다 — 다른 테스트가 쓰는 2차로는 곡선 모양의 이탈이 적합에 흡수되어 잔차로 안 나타난다.

**반증**: R² 를 `0.9995`(게이트 위 상수)로 고정하면

```
step 1 (amplitude 0.25): R2 0.99950000000000006 did not fall below the previous 0.99950000000000006
step 2 (amplitude 0.5):  ... 같은 값
step 3 (amplitude 0.75): ... 같은 값
[  FAILED  ] R2FallsAsTheSeriesDepartsFromTheFit
```

진단이 "무엇이 안 움직였는지"를 그대로 말한다.

### C4 — FUNC-033 (1): 8개 중 6개

정본 §FUNC-033 (1) 이 요구하는 필드와 현재 상태:

| 필드 | 단일점 경로 | 다항 경로 | 비고 |
|---|---|---|---|
| `calibration_mode` | ✅ | ✅ | |
| `actual_dose_levels` | ✅ (1) | ✅ | |
| `polynomial_degree` | ✅ (0) | ✅ | |
| `fit_r_squared` | ✅ (1.0) | ✅ 계산됨 | SRS 가 단일점은 1.0 으로 정의 |
| `max_residual_pct` | ✅ (0.0) | ✅ 계산됨 | |
| `mean_residual_pct` | ✅ (0.0) | ✅ 계산됨 | |
| `acquisition_duration_s` | ❌ | ❌ | **도달하지 않음** |
| `detector_temperature_c` | ❌ | ❌ | **도달하지 않음** |

**왜 도달하지 않는가** — 두 진입점의 시그니처가 그 값을 받지 않는다:

```c
xpe_calib_generate_gain(const XpeImageBuffer* flat_frames, int32_t num_frames,
                        const XpeImageBuffer* dark_reference,
                        const char* output_path, const char* metadata_json);
xpe_calib_generate_gain_polynomial(const char** gain_file_paths, const double* dose_levels,
                                   int32_t num_levels, int32_t max_degree,
                                   const char* output_path);
```

취득 시간도 검출기 온도도 인자에 없다. 단일점 쪽은 `metadata_json` 에 실어 보낼 수 있겠지만 **다항 쪽에는 그런 인자 자체가 없다**. 함수 안에서 만들어 낼 수 있는 값이 아니다 — 취득은 이 모듈 밖에서 일어나고, 온도는 하드웨어에서 온다.

**해석해서 채우지 않았다.** 예컨대 "생성 함수 실행 시간"을 `acquisition_duration_s` 로 적는 것은 이름이 말하는 것과 다른 값을 그 자리에 넣는 일이다.

이 사실은 이미 코드 주석에 적혀 있었다(`xpe_calib_generate_gain.cpp:293`, A-35). **이번에 한 것은 그것이 여전히 사실인지 확인한 것**이다.

### C5 — FUNC-033 (3)·(4): 구현 없음, 그리고 해석 없이는 불가능

```
$ for f in dark_bias_delta prnu_delta_pct defect_count_delta gain_uncertainty per_point_r_squared;
  do grep -rl "$f" --include=*.cpp --include=*.h modules/preprocess/; done
(다섯 개 모두 출력 없음)
```

**(3) 이전 교정과의 비교** — `dark_bias_delta`, `prnu_delta_pct`, `defect_count_delta`. 구조체에 있는 것은 `previous_r_squared` 뿐이고, **그 필드는 SRS 목록에 없다.** 세 값이 막히는 지점:

- `dark_bias_delta` 는 **오프셋 파일**의 양이다. 게인 생성 경로는 오프셋 교정을 읽지 않는다.
- `defect_count_delta` 는 **BPM** 의 양이다. 마찬가지로 이 경로에 없다.
- `prnu_delta_pct` 는 게인 맵에서 계산할 수 있지만, 다항 경로의 출력은 **게인 맵이 아니라 계수 배열**이다. 계수 배열의 PRNU 는 정의되지 않는다. 그리고 "이전"을 읽으려면 덮어쓸 파일을 먼저 읽어야 하는데 지금은 읽지 않는다.

**(4) 모드별** — `gain_uncertainty`(SINGLE_POINT, "단일 프레임 잡음 모델에서 추정"), `per_point_r_squared[]`(MULTI_POINT).

- `gain_uncertainty`: 어떤 잡음 모델인지, 단위가 무엇인지 정본이 말하지 않는다. 프레임 간 표준편차/√N 이 자연스러운 후보지만 **그것은 제 선택이지 요구가 아니다.**
- `per_point_r_squared[]`: R² 는 표본 분산이 있어야 정의되는데 **한 도스 점은 표본이 하나**다. 화소 방향으로 정의하면 수는 나오지만, 정본은 그렇게 말하지 않는다.

**해석하지 않고 올립니다.** 정본을 고칠지, 시그니처를 늘릴지, 요구를 좁힐지는 판정 사항입니다.

### C6 — 회귀

```
PRE_BUILD_EXIT=0 / COMMON_BUILD_EXIT=0
100% tests passed, 0 tests failed out of 640
100% tests passed, 0 tests failed out of 69
```

640 = A-69 의 639 + 이번 1건. 경고 0.

---

## 3. baseline 귀속 (Baseline-attribution)

- 반증 3회 모두 `BUILD_EXIT=0` 을 확인한 뒤의 실행 결과다.
- 필드 표는 `grep` 결과와 두 JSON 작성부(`:302`, `:606`)를 직접 읽어 만들었다.
- 시그니처는 `preprocess_api.h:285`, `:314` 원문이다.
- SRS 인용은 `docs/calibration/SRS-CALIB-001_...md:170` 원문이며, **옮겨 적은 임계값·필드명은 전부 그 줄에서 왔다.**

---

## 4. 미검증 (Gaps)

- **단일점 경로의 게이트는 반증하지 않았다.** 단일점은 R² 가 정의상 1.0 이라 게이트가 항상 통과하고, 실패 쪽을 만들 입력이 없다. 그 경로의 메타데이터 존재는 `test_calib_gain_poly_load.cpp` 가 확인한다.
- **`previous_r_squared` 의 의미를 검증하지 않았다.** SRS (3) 목록에 없는 필드이고, 비교 테스트 2건이 그 동작을 확인하지만 **요구에 대응하지 않는다.**
- **XCal 헤더 JSON 의 크기 한계를 확인하지 않았다.** 단일점 경로는 512바이트 고정 버퍼에 `snprintf` 하고 호출자 메타를 뒤에 붙인다. 넘칠 때 어떻게 되는지 재지 않았다.
- **다른 교정 종류(오프셋·BPM)의 메타데이터**는 이 카드 범위 밖이고 보지 않았다.
- **Doxygen 로컬 확인 불가** — 여전(#159). 이번에 헤더를 바꾸지 않았다.

---

## 5. 문서 쪽에서 본 것 (고치지 않았습니다)

카드가 "문서는 리더 소유, 어긋나면 보고"라고 했다. 정본을 읽다가 본 것:

**`SRS-CALIB-001` 의 Mode-to-Parameter Mapping 표에서 두 행의 열이 뒤바뀌어 있습니다.**

```
| MULTI_POINT_8  | 3 (cubic)   | 8  | ...
| MULTI_POINT_10 | 4 (quartic) | 10 | ...
```

열 제목은 `Max Dose Levels | Max Poly Degree` 인데, 위 두 행은 값이 반대로 들어가 있습니다(8점·3차, 10점·3차가 맞습니다 — `preprocess_api.h:934`, `xpe_calib_mode.cpp:63` 의 테이블과 위 5행이 모두 그렇게 말합니다). 또 MULTI_POINT_10 의 "4 (quartic)" 는 API 주석·구현 어느 쪽과도 맞지 않습니다(둘 다 3차).

**구현을 고치지 않았습니다.** 구현은 API 문서·다른 행들과 일관되고, 어긋난 것은 그 두 칸입니다.

---

## 6. 판정 요청

1. **`acquisition_duration_s` / `detector_temperature_c`** — 시그니처를 늘릴지(공개 API 변경), `metadata_json` 을 다항 경로에도 추가할지, 요구에서 뺄지.
2. **FUNC-033 (3) 비교 지표 셋** — 게인 생성 경로에서는 세 값 중 둘이 다른 교정 산출물의 양입니다. 요구를 게인에 한정할지, 별도 경로로 옮길지.
3. **FUNC-033 (4) 모드별 둘** — 정본에 정의가 없습니다. 정의를 문서에 추가해 주시면 구현하겠습니다.
4. **SRS 표 두 행** (§5).

---

Refs #140 #120
