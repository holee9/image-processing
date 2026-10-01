# QA-A-87 — 다항 차수 불일치 재현·수정, 잔차 값 단언 (#140)

**카드**: `.moai/lanes/pre/inbox/QA-A-87.md` · **브랜치**: `dev/preprocess` · **커밋**: `70d9039` (미푸시) · **병합 기준**: `21be69d` · **기계**: Intel Core i7-12700

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | **재현됐다.** 수정 전 코드에서 생성 직후 차수 **1**, 같은 파일 로드 후 **2**. 대조군(직선 입력)은 2 = 2 |
| C2 | 다항 경로 JSON 의 `polynomial_degree` 를 **실제 적합 차수**로 바꿨고, 요청 상한은 **새 키 `max_polynomial_degree`** 로 분리했다 |
| C3 | 로더는 JSON 이 아니라 페이로드 크기로 보폭을 계산한다 — 이 변경이 로드를 깨지 않는다. 이 키를 읽는 곳은 저장소 전체에서 preprocess 모듈 안뿐이다 |
| C4 | **생성된 파일을 로드하는 왕복 시험** 2건(차수 탈락 + 대조군)을 넣었다 |
| C5 | 잔차 두 필드에 **손 계산 값 단언**(36.923077 / 23.076923)과 0/0 대조군을 넣었다 |
| C6 | `PreviousCalibration_Comparison_*` → `PreviousRSquared_*` |
| C7 | **반증 3건 모두 해당 시험 하나만 빨강**, 나머지 30건 통과. 매번 DLL 재컴파일을 로그로 확인했다 |
| C8 | 원복 후 `ci-preprocess` **647/647**, `ci-common` **69/69**, 경고 0 |
| C9 | `#140` 에 사실 코멘트를 달았다 |

---

## 2. 증거 (Evidence)

### C1 — 재현

**입력 설계(손 계산)**: 도스 x = 1,2,3,4, 게인 y = 1,4,4,4, `max_degree` = 2.
t = x − 2.5 로 직교화하면 t = −1.5, −0.5, 0.5, 1.5, q = t² − 1.25 = 1, −1, −1, 1.

```
a = mean(y)          = 13/4            = 3.25
b = Σ t·y / Σ t²     = 4.5 / 5         = 0.9
c = Σ q·y / Σ q²     = −3 / 4          = −0.75
y(t) = 3.25 + 0.9 t − 0.75 (t² − 1.25)
y'(t) = 0.9 − 1.5 t = 0  →  t = 0.6  →  x = 3.1   (구간 [1,4] 안)
```

정점 뒤에서 감소하므로 `validate_monotonicity`(단조 증가 검사, `:142-172`)가 2차를 거부하고, 1차 `y = 3.25 + 0.9t` 는 증가하므로 채택된다.

**수정 전 코드로 시험을 먼저 넣고 돌렸다:**

```
BUILD_EXIT_repro=0
TEST_EXIT_repro=1

test_calib_quality_meta.cpp(348): error: Expected equality of these values:
  generated.polynomial_degree
    Which is: '\x1' (1)
  loaded.polynomial_degree
    Which is: '\x2' (2)
[  FAILED  ] CalibQualityMetaTest.GeneratedPolyFileKeepsTheFittedDegree_WhenTheFitDropsADegree (15 ms)
[       OK ] CalibQualityMetaTest.GeneratedPolyFileKeepsTheFittedDegree_WhenNoDegreeIsDropped (10 ms)
```

- 전제 단언 `ASSERT_EQ(1u, generated.polynomial_degree)` 가 통과했다 → **입력이 조건을 만족했다.**
- 새 시험 이름이 실행 목록에 있다 → 낡은 바이너리가 아니다.
- 대조군 통과 → 시험 장치 자체는 옳다.

### C2 · C3 — 수정

```diff
-            "{\"polynomial_degree\":%d,\"num_coefficients\":%d,\"num_dose_levels\":%d,"
+            "{\"polynomial_degree\":%d,\"max_polynomial_degree\":%d,"
+            "\"num_coefficients\":%d,\"num_dose_levels\":%d,"
 ...
+            static_cast<int>(highest_degree),
             static_cast<int>(max_degree),
             static_cast<int>(sMaxCoeffsPoly),
```

`num_coefficients` 는 그대로 `max_degree + 1` 이다 — 모든 화소가 그 개수로 저장되는 페이로드 보폭이기 때문이다(주석으로 남겼다).

**로더가 JSON 을 보폭에 쓰지 않는다는 확인** (`xpe_calib_load_gain.cpp`):

```cpp
const size_t num_coeffs = is_poly ? (payload.size() / plane) : 1;
```

`xcal_validator.cpp:112-120` 도 페이로드가 한 평면의 정수배인지만 본다("The degree itself is derived from that multiple").

**이 키를 읽는 곳** (저장소 전체, `build/` 제외, `.cpp .h .hpp .cs .py .json .xaml`):

```
./modules/preprocess/include/xpe/preprocess_api.h
./modules/preprocess/src/xpe_calib_generate_gain.cpp
./modules/preprocess/src/xpe_calib_mode.cpp
./modules/preprocess/tests/test_calib_gain_poly_load.cpp
./modules/preprocess/tests/test_calib_mode.cpp
./modules/preprocess/tests/test_calib_quality_meta.cpp
```

### C4 · C5 · C6 — 시험

| 시험 | 입력 | 단언 |
|---|---|---|
| `GeneratedPolyFileKeepsTheFittedDegree_WhenTheFitDropsADegree` | 1,4,4,4 / max 2 | 전제: 생성 직후 1. 로드 후 == 생성 직후. 파일 `polynomial_degree` == 1, `max_polynomial_degree` == 2 |
| `GeneratedPolyFileKeepsTheFittedDegree_WhenNoDegreeIsDropped` | 1,2,3,4 / max 2 | 전제: 생성 직후 2. 로드 후 == 생성 직후 |
| `ResidualFieldsCarryTheHandComputedValues` | 1,4,4,4 / max 1 | max 36.923077, mean 23.076923 (±1e-5) |
| `ResidualFieldsAreZeroForAnExactFit` | 1,2,3,4 / max 1 | 둘 다 0 (±1e-5) |

**잔차 손 계산** (1차 적합, 모든 화소 동일):

```
예측    1.9, 2.8, 3.7, 4.6
잔차   −0.9, 1.2, 0.3, −0.6      |잔차| 합 = 3.0
평균 게인 3.25
max_residual_pct  = 1.2 / 3.25 × 100       = 36.923077
mean_residual_pct = (3.0 / 4) / 3.25 × 100 = 23.076923
```

`jsonNumber` 도우미는 키가 없으면 NaN 을 돌려준다 — 키 부재가 0 으로 읽혀 통과하지 않도록.

**대조군이 따로 필요한 이유**: 정확 적합(0/0)만 있으면 "항상 0 을 쓴다"는 결함이 통과한다. 반대로 0 이 아닌 입력만 있으면 시험 장치(`jsonNumber`)가 엉뚱한 값을 읽어도 모른다. 반증 2 에서 실제로 대조군은 초록으로 남았다(0 / (n+1) = 0).

**이름 변경**: 두 시험은 `previous_r_squared` 를 본다. 위에 주석으로 옛 이름과 바꾼 이유를 남겼다. `test_calib_mode.cpp:203` 의 옛 이름 언급은 **QA-A-34 때 지운 시험 목록의 역사 기록**이라 그대로 뒀다.

### C7 — 반증

실행 범위: `--gtest_filter=CalibQualityMetaTest.*:GainPolyLoadTest.*:CalibModeTest.*` (31건). 매번 빌드 로그에 `[1/4] Building ... xpe_calib_generate_gain.cpp.obj` / `[2/4] Linking ... xpe_preprocess.dll` 가 있음을 확인했다 — 주입이 실제로 바이너리에 들어갔다.

**반증 1** — JSON 차수를 `max_degree` 로 되돌림:

```
BUILD_EXIT_f1=0  TEST_EXIT_f1=1
[  FAILED  ] CalibQualityMetaTest.GeneratedPolyFileKeepsTheFittedDegree_WhenTheFitDropsADegree
[  PASSED  ] 30 tests.
  generated.polynomial_degree  Which is: '\x1' (1)
  loaded.polynomial_degree     Which is: '\x2' (2)
{"polynomial_degree":2,"max_polynomial_degree":2,...}
```

카드 요구("이 시험만 빨강")와 일치. 대조군은 초록이다(탈락이 없으면 두 값이 같으므로).

**반증 2** — `mean` 을 `count + 1` 로 나눔:

```
BUILD_EXIT_f2=0  TEST_EXIT_f2=1
[  FAILED  ] CalibQualityMetaTest.ResidualFieldsCarryTheHandComputedValues
[  PASSED  ] 30 tests.
jsonNumber(json, "mean_residual_pct") evaluates to 22.721893
```

**반증 3** — `max` 에 0.99 곱함:

```
BUILD_EXIT_f3=0  TEST_EXIT_f3=1
[  FAILED  ] CalibQualityMetaTest.ResidualFieldsCarryTheHandComputedValues
[  PASSED  ] 30 tests.
jsonNumber(json, "max_residual_pct") evaluates to 36.553846
```

원복 확인: `grep -c FALSIFY xpe_calib_generate_gain.cpp` → 0. 원복 후 `git diff` 는 의도한 변경(JSON 키 분리 + 주석)뿐.

### C8 — 전체 (원복 후)

```
ci-preprocess
  PRE_BUILD_EXIT=0      a87-pre-build.log   169 bytes
                        [1/7] Building CXX object ...xpe_calib_generate_gain.cpp.obj
                        [2/7] Linking CXX shared library bin\xpe_preprocess.dll
  warnings=0
  PRE_CTEST_EXIT=0      a87-pre-ctest.log   127789 bytes
  100% tests passed, 0 tests failed out of 647

ci-common
  (빌드) 종료 0         a87-com-build.log   549 bytes
  com warnings=0
  (ctest) 종료 0        a87-com-ctest.log   12434 bytes
  100% tests passed, 0 tests failed out of 69
```

- 647 = 이전 643 + 새 시험 4. 새 시험 이름이 ctest 로그에서 12줄 잡힌다.
- **`ci-common` 스크립트는 `ci-preprocess` 스크립트를 복사해 프리셋과 로그 이름만 바꾼 것이라, 화면의 echo 라벨이 `PRE_BUILD_EXIT`/`PRE_CTEST_EXIT` 로 찍혔다.** 값은 `ci-common` 실행의 것이고, 로그 파일명(`a87-com-*`)과 69 건수로 구분된다.
- 원복 후 빌드가 `xpe_calib_generate_gain.cpp` 를 다시 컴파일했다 → 마지막 반증 바이너리가 아니라 원복된 소스로 돈 결과다.

### C9 — 코멘트

```
https://github.com/holee9/image-processing/issues/140#issuecomment-5706884179
check: n=6 has70d=true has647=true
```

---

## 3. baseline 귀속 (Baseline-attribution)

- 재현은 **수정 전 소스 + 새 시험**으로 먼저 실행했다(`a87-repro-*`).
- 수정 후(`a87-fixed-*`), 반증 3건(`a87-f1/f2/f3-*`), 원복 후 전체(`a87-pre-*`, `a87-com-*`) 순서로 실행했다. 각 단계 로그 파일이 존재하고 크기를 기록했다.
- 이전 전체 수 643 은 QA-A-82·A-83 의 `ci-preprocess` 결과와 같다.

---

## 4. 미검증 (Gaps)

- **단일 도스 경로의 잔차 상수(`0.000000`)는 값 단언을 넣지 않았다.** 상수라 계산을 지키는 단언이 아니다.
- **`fit_polynomial_ls` 가 실패해 차수가 내려가는 경로**는 시험하지 않았다. 이번 입력은 단조성 검사로 내려가는 경로다.
- **화소마다 다른 차수로 적합되는 입력**(일부 화소만 탈락)은 시험하지 않았다. 모든 화소가 같은 값이다.
- **CI 에서 돌리지 않았다.** push 하지 않았다.
- **기존에 생성된 다항 파일**(수정 전 형식, `polynomial_degree` = 상한)을 로드하면 여전히 상한이 차수로 읽힌다. 호환 처리는 하지 않았다 — 카드 범위 밖이고, 그런 파일이 배포돼 있는지 모른다.
- **시험 순서에 따른 전역 모드 누수**를 봤다: 반증 로그의 JSON 에 `calibration_mode":5`(AUTO)가 찍혔다. 같은 프로세스에서 먼저 돈 `CalibModeTest` 가 남긴 전역 상태로 보이지만 확인하지 않았다. 이번 단언에는 영향이 없다.

---

## 5. 잔여 위험 (Residual-risk)

- **수정 전에 만들어진 다항 파일은 계속 요청 상한을 차수로 보고한다.** 새 키 `max_polynomial_degree` 의 유무로 구분할 수는 있지만 로더는 구분하지 않는다.
- **`polynomial_degree` 의 뜻이 바뀌었다.** 파일을 읽는 외부 도구가 있다면(저장소 안에는 없음) 보폭 계산에 이 값을 쓰고 있었을 경우 깨진다. 보폭은 `num_coefficients` 또는 페이로드 크기로 구해야 한다.
- **`highest_degree` 는 화소 중 최댓값이다.** 화소마다 차수가 다를 때 이 값 하나로는 분포를 말하지 못한다 — SRS 가 그 이상을 요구하지는 않는다.

---

Refs #140
