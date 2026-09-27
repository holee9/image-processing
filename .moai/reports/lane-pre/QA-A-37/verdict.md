# QA-A-37 검증 보고서 — `XCAL_TYPE_GAIN_POLY` 읽기 경로 + 비다항 게인 FUNC-033 메타

- 레인: Lane A (`xpe-pre`), 브랜치 `dev/preprocess`
- 선행: `git merge origin/main` → `6d7063e` (fast-forward, 충돌 없음)
- 커밋: `366f584` (POLY 읽기 경로), `e702d2c` (단일 도즈 메타)
- 증거 디렉터리: `.moai/reports/lane-pre/QA-A-37/`
- Refs #140

---

## 1. 주장 (Claim)

| # | 주장 | 판정 |
|---|------|------|
| C1 | `xpe_calib_generate_gain_polynomial` 이 쓴 파일을 `xpe_calib_load_gain` 이 읽는다 | PASS |
| C2 | POLY 파일의 FUNC-033 메타가 `xpe_calib_get_quality_meta` 로 왕복한다 | PASS |
| C3 | POLY 적재는 스칼라 게인 맵을 비운다 — 낡은 맵이 조용히 적용되지 않는다 | PASS |
| C4 | 타입 불일치 오류 코드는 기존 계약(`XPE_ERR_CONFIG_INVALID`) 그대로다 | PASS |
| C5 | POLY 페이로드는 계수 평면의 정수배여야 하고, 아니면 거절된다 | PASS |
| C6 | `xpe_calib_generate_gain`(단일 도즈)도 FUNC-033 (1) 필수 필드를 기록한다 | PASS |
| C7 | 호출자 `metadata_json` 이 유실되지 않는다 | PASS |
| C8 | export 불변 (preprocess 50, common 16) | PASS |
| C9 | ci-preprocess / ci-common 재실측 회귀 없음 | PASS |

**결함은 "로더 부재" 한 겹이 아니라 세 겹이었다.** 카드의 전제(로더가 없다)는
맞지만 불완전했다 — 로더를 추가하는 것만으로는 파일이 읽히지 않는다.

1. `xcal_validator.cpp` Check 3 이 `type > XCAL_TYPE_DEFECT(=2)` 를 전부 거절.
   `XCAL_TYPE_GAIN_POLY` 는 3 이므로 **검증 단계에서** `CONFIG_INVALID` 로 잘렸다.
2. Check 8 이 `payload_len == W*H*bpp` 를 강제. 계수 배열은 `(degree+1)*W*H` 라
   이 조건을 절대 만족할 수 없다.
3. `xpe_calib_load_gain` 이 리더에 `XCAL_TYPE_GAIN` 만 요구.

즉 **생성기는 자기 DLL 의 검증기가 거절하는 파일을 쓰고 있었다.**

---

## 2. 증거 (Evidence)

### C1~C5 — RED → GREEN (`a37-red.log`, `a37-green.log`)

명령: `build\ci-preprocess\bin\xpe_preprocess_tests.exe --gtest_filter=GainPolyLoadTest.*`

RED (수정 전, 신규 5케이스):

```
test_calib_gain_poly_load.cpp(131): error: Expected equality of these values:
    Which is: -4
[  FAILED  ] GainPolyLoadTest.PolyFileWrittenByTheApiLoads (8 ms)
[  FAILED  ] GainPolyLoadTest.PolyMetadataSurvivesTheRoundTrip (6 ms)
[  FAILED  ] GainPolyLoadTest.LoadingPolyClearsTheScalarGainMap (7 ms)
[  FAILED  ] GainPolyLoadTest.PayloadThatIsNotAWholeNumberOfPlanesIsRejected (12 ms)
[  PASSED  ] 1 test.
```

`-4` 는 `XPE_ERR_CONFIG_INVALID` 다 — 로더에 닿기도 전에 검증기가 거절했다는
증거. 통과한 1건은 `TypeMismatchStillReportsConfigInvalid` 로, 당시에는 모든
POLY 파일이 거절됐으므로 우연히 통과한 것이다(수정 후에도 통과 — §4 참조).

GREEN (수정 후):

```
[  PASSED  ] 5 tests.
```

### C6 · C7 — RED → GREEN (`a37-red2.log`, `a37-green2.log`)

RED (단일 도즈 메타 3케이스 추가 직후):

```
SRS-CALIB-FUNC-033 (1) field missing from the file: calibration_mode
SRS-CALIB-FUNC-033 (1) field missing from the file: actual_dose_levels
SRS-CALIB-FUNC-033 (1) field missing from the file: polynomial_degree
SRS-CALIB-FUNC-033 (1) field missing from the file: fit_r_squared
SRS-CALIB-FUNC-033 (1) field missing from the file: max_residual_pct
SRS-CALIB-FUNC-033 (1) field missing from the file: mean_residual_pct
SRS-CALIB-FUNC-033 (1) field missing from the file: calibration_pass
[  FAILED  ] GainPolyLoadTest.SingleDoseGainFileCarriesTheMandatoryFields (8 ms)
[  FAILED  ] GainPolyLoadTest.SingleDoseMetadataSurvivesTheRoundTrip (6 ms)
[  FAILED  ] GainPolyLoadTest.CallerMetadataIsNotDiscarded (4 ms)
[  PASSED  ] 5 tests.
```

7개 필드가 전부 부재였다. GREEN:

```
[  PASSED  ] 8 tests.
```

### SRS 인용 (카드 3항 요구)

> **SRS-CALIB-FUNC-033 (1) Mandatory Metadata Fields**: Every generated XCal gain
> file shall include: `calibration_mode` …, `actual_dose_levels` (number of dose
> levels used), `polynomial_degree` (fitted polynomial degree; **0 for
> single-point**), `fit_r_squared` (coefficient of determination; **1.0 for
> single-point by definition**), `max_residual_pct` …, `mean_residual_pct` …,
> `acquisition_duration_s` …, `detector_temperature_c` …

기록한 값: `calibration_mode = xpe_calib_get_mode()`, `actual_dose_levels = 1`,
`polynomial_degree = 0`, `fit_r_squared = 1.0`, `max_residual_pct = 0.0`,
`mean_residual_pct = 0.0`, `calibration_pass` = 게이트가 재계산.

잔차 두 필드에 0 을 쓴 근거는 소스 주석에 남겼다: 곡선 적합이 없으므로 잔차가
존재하지 않고, SRS 가 단일 점의 잔차 값을 명시하지 않는다. 임계를 발명하는
대신 1점 정확 적합의 산술적 귀결인 0 을 택했다.

### C8 — export 불변 (`a37-exports.txt`, `a37-export-diff.txt`)

명령: `dumpbin /exports build\ci-preprocess\bin\xpe_preprocess.dll` (및 common)

```
          50 number of names      <- xpe_preprocess.dll
          16 number of names      <- xpe_common.dll
```

`diff a37-old-names.txt a37-new-names.txt` → 빈 출력 (exit 0).
old 는 QA-A-35 의 `a35-dumpbin.log` 에서 추출한 50개 이름이다.

### C9 — 재실측

```
ctest --output-on-failure  (build/ci-preprocess)
100% tests passed, 0 tests failed out of 580

ctest --output-on-failure  (build/ci-common)
100% tests passed, 0 tests failed out of 69
```

로그: `a37-ctest-preprocess.log`, `a37-ctest-common.log`.

### 변경 규모

커밋 1 (`a37-diff-commit1.txt`):

```
 modules/preprocess/CMakeLists.txt                  |   2 +
 .../xpe/preprocess/xpe_preprocess_internal.h       |  10 +
 modules/preprocess/src/xcal_reader.cpp             |   4 +-
 modules/preprocess/src/xcal_validator.cpp          |  19 +-
 modules/preprocess/src/xpe_calib_load_gain.cpp     |  50 ++++-
 modules/preprocess/tests/test_calib_gain_poly_load.cpp | 203 +++++
 6 files changed, 278 insertions(+), 10 deletions(-)
```

커밋 2 (`a37-diff-commit2.txt`):

```
 modules/preprocess/src/xpe_calib_generate_gain.cpp | 52 ++++++---
 modules/preprocess/tests/test_calib_gain_poly_load.cpp | 78 +++++++++
 modules/preprocess/tests/test_xcal_validator.cpp   | 30 +++-
 3 files changed, 149 insertions(+), 11 deletions(-)
```

---

## 3. Baseline 귀속

- **테스트 총수**: QA-A-36 측정 571 → 580 (+9). 내역: 신규 `GainPolyLoadTest`
  8건 + `XCalValidatorTest.GainPolyAcceptsWholeNumberOfPlanes` 1건.
  기존 케이스의 상태 변화는 아래 1건뿐이며, 그것도 정정이지 회귀가 아니다.
- **`XCalValidatorTest.TypeOutOfRange_ReturnsConfigInvalid` (전제 정정)**:
  이 케이스는 `hdr.type = 3; // Only 0,1,2 are valid` 로 "3은 무효" 를
  인코딩하고 있었다. 그러나 3 은 `xcal_format.h:44` 의 `XCAL_TYPE_GAIN_POLY`
  로, 포맷이 작성될 때부터 존재한 타입이다. 즉 이 테스트는 검증기의 오류와
  맞물려 통과하고 있었고, 검증기를 고치자 1건 FAIL 로 드러났다
  (`a37-ctest-preprocess.log` 최초 실행: `579개 중 1건 실패`).
  4(대응 타입 없음)로 정정했고, 대신 POLY 평면 규칙 케이스 6b 를 추가했다.
- **빌드 프리셋**: `ci-preprocess`, `ci-common` 동일. `/WX` 활성 상태 유지,
  최종 빌드 경고 0건.
- **export baseline**: QA-A-35 의 `a35-dumpbin.log` (50 names). 같은 명령으로
  이번에 다시 측정해 비교했다.

---

## 4. 미검증 (Gaps)

1. **계수 배열의 내용을 직접 확인하지 않았다.** `g_calib` 은 export 되지 않아
   테스트에서 계수 값을 읽을 수 없다. 확인한 것은 (a) 적재가 성공하고
   (b) 평면 개수 규칙이 강제되며 (c) 스칼라 맵이 비워진다는 관측 가능한
   효과뿐이다. "계수가 파일과 바이트 동일하게 실렸다" 는 검증하지 않았다.
2. **POLY 계수를 실제로 적용하는 보정 경로는 만들지 않았다.** 카드 범위가
   "적재 + 메타" 이므로 `xpe_gain_correct` 는 POLY 적재 후
   `CALIB_NOT_LOADED` 를 보고한다. 이는 의도이자 테스트로 고정한 계약이지만,
   다항 게인 보정 자체는 여전히 미구현이다.
3. **`TypeMismatchStillReportsConfigInvalid` 는 수정 전후 모두 통과했다.**
   수정 전에는 "POLY 파일이 전부 거절돼서" 통과했으므로 RED 감도가 없다.
   수정 후에는 의도한 이유(offset 로더가 gain-poly 를 거절)로 통과한다.
   이 케이스만으로는 계약 유지를 강하게 주장할 수 없다.
4. **`acquisition_duration_s` / `detector_temperature_c` 미기록.** FUNC-033 (1)
   이 명시하지만 두 값은 현재 시그니처로 함수에 도달하지 않는다. #140 잔여로
   남겼고 소스 주석에 사유를 적었다 — API 입력 필드 추가는 이 카드 범위 밖이다.
5. **FUNC-033 (3)(4) 미이행.** 이전 캘리브레이션 대비 델타 3종,
   SINGLE_POINT 의 `gain_uncertainty`, MULTI_POINT 의 `per_point_r_squared[]`
   배열은 이 카드에서 건드리지 않았다(#140 잔여, 카드 지시대로).
6. **압축 POLY 파일은 실측하지 않았다.** `xcal_reader.cpp` 의 압축 경로에도
   같은 타입 상한 사본이 있어 함께 정정했으나, 압축된 POLY 파일을 실제로
   쓰고 읽는 케이스는 만들지 않았다 — 생성기가 압축을 쓰지 않기 때문이다.

---

## 5. 잔여 위험 (Residual risk)

1. **검증기 상한을 올린 범위 효과.** Check 3 이 이제 3까지 허용하므로,
   `XCAL_TYPE_GAIN_POLY` 헤더를 가진 파일이 offset/defect 로더에 도달할 수
   있다. 도달해도 Check 10(expected_type)에서 `CONFIG_INVALID` 로 거절되며
   테스트로 고정했지만, 새 로더를 추가할 때 타입 검사를 빠뜨리면 이전보다
   한 겹 얇아진 상태다.
2. **`source_metadata` 중첩은 호출자 JSON 이 유효할 때만 유효 JSON 을 만든다.**
   이전 코드는 호출자 문자열을 그대로 config JSON 으로 썼으므로 같은 취약성이
   이미 있었고 회귀는 아니다. 다만 FUNC-033 필드는 중첩 밖에 있으므로
   호출자가 깨진 JSON 을 넘겨도 우리 필드의 파싱(부분 문자열 스캔)은 살아남는다.
3. **`calibration_pass` 는 저장소가 재계산한다.** 파일이 주장하는 값을 믿지
   않는 것은 A-35 에서 고정한 계약이지만, 이는 파일과 저장소의 값이 달라질 수
   있다는 뜻이기도 하다. 감사 시 둘 중 저장소 값이 정본이다.
4. **단일 도즈의 잔차 0 은 "적합 오차 없음" 이지 "품질 좋음" 이 아니다.**
   메타만 보는 소비자가 `mean_residual_pct = 0` 을 품질 근거로 읽으면 오해다.
   `actual_dose_levels = 1` 과 함께 읽어야 한다 — 소스 주석에 명시했으나
   소비자 쪽 가드는 없다.
