# QA-A-204 (2/3) — #233 5순위: 게인 파일 품질 메타데이터의 엄격 변환

QA-A-204 의 두 번째 커밋. 이 카드가 맡은 나머지(`verify_*`·`bpm`·`runtime`·`nonlin_lut` 의 OOM 가드)는 QA-A-205(#234, 리더 지시로 선행) 뒤의 3/3 에서 한다. 기준 커밋 `189ec742`. 증거는 `evidence/` (번호 순).

## 1. 주장

게인 파일의 설정 블록이 싣는 `fit_r_squared`, `polynomial_degree`, `actual_dose_levels`, `calibration_mode` 는 존재하는데 숫자가 아니거나 범위를 벗어나면 `xpe_calib_load_gain`(과 그것을 부르는 캐시 로더의 미스 경로)이 `XPE_ERR_CONFIG_INVALID` 를 돌려주고, 저장소(게인 맵·품질 메타데이터)는 호출 전 그대로다. 전에는 `atof`/`atoi` 가 잘못된 값을 조용히 0 으로 읽거나(`"abc"`), 잘라냈다(`"256"` → 0). 정상 값은 그대로 읽힌다(통제).

## 2. 무엇을 바꿨나

- `xpe_calib_mode.cpp`: `xpe_calib_parse_quality_meta_json` 이 `bool` 대신 `XpeErrorCode` 를 돌려주고 "하나라도 있었는가" 는 `found` 출력 인자로 낸다. 실수 칸은 `xpe_strict::parse_double`(유한), 세 `uint8` 칸은 `parse_int` 에 `[0, 255]` 범위 검사. 표기 규칙은 QA-A-202b 의 것과 같다(앞 공백·앞 `+` 수락, 뒤 글자·`nan`·`inf` 등 거부).
- `xpe_calib_load_gain.cpp`: 파싱은 원래 커밋 앞에 있었으므로 실패 코드를 그대로 돌려주면 저장소는 바뀌지 않는다.
- `xpe_preprocess_internal.h`: 선언과 설명 갱신. 호출자는 이 한 곳뿐이다(저장소 전체 grep).
- 시험 `ConfigStrictParse.AMalformedQualityFieldInAGainFileRefusesTheLoadAndLeavesTheStore`(12개 잘못된 값), `...WellFormedQualityFieldsInAGainFileAreStillRead`(통제).

## 3. 빨강 → 초록

- 빨강: 잘못된 값 시험 빨강(72개 단언 실패 — 값이 조용히 읽혀 로드가 성공하고, 이어지는 행들은 바뀐 저장소 때문에 연쇄로 실패). 통제 시험은 구현 전에도 초록(정상 값 `" +0.95"` 등은 원래 읽혔다).
- 초록: `ConfigStrictParse` 10 통과 (`04_green_tests.txt`).

## 4. 실측 (이 트리, 이 실행)

| 항목 | 관측 |
|---|---|
| 빌드 | 전체 타깃 `BUILD_EXIT=0` (`21_build_final.txt`) |
| preprocess DLL 시험 | 772 실행 / 764 통과 / 8 건너뜀, 섞기도 동일 (`24`, `25`) |
| preprocess OOM exe / common OOM exe / common 기존 | 21 / 8 / 69 통과 |
| `ctest -N` | 910 (직전 908) |
| 수출 이름 | preprocess 48, common 16 모두 불변 |
| 헤더 문서 / 프리셋 | 0 findings / 12 of 12, 종료 0 |

## 5. 반증 (한 번에 하나, 복원 뒤 `cmp` 동일)

| 팔 | 손상 | 결과 |
|---|---|---|
| norange | 세 정수 칸의 `[0, 255]` 검사 제거 | 잘못된 값 시험만 빨강(`"256"`, `"-1"` 이 받아들여져 저장소가 바뀜) |
| lenient | 옛 읽기(`atoi`/`atof`)로 복원 | 같은 시험만 빨강(실수 칸 `abc`·`0.9x`·`1e999`·`nan`, 정수 칸 `x`·`2x` 포함 전부) |

## 6. 미검증 (Gaps)

- 이 카드는 게인 파일 품질 메타데이터만 바꿨다. `xpe_calib_mode.cpp` 의 다른 `atoi`/`atof` 는 이 함수의 네 곳뿐이었다(저장소 grep).
- 생성기(`xpe_calib_generate_gain.cpp`)가 `fit_r_squared` 에 `nan` 을 쓰는 경로가 있는지는 확인하지 못했다. 쓴다면 그 파일은 이제 `CONFIG_INVALID` 로 거부된다(전에는 `atof("nan")` 로 읽혔다). 생성기는 `ss_tot_total > 0.0` 일 때만 나눈다(604행)는 것까지만 읽었다.
- 기존 현장 파일에 숫자 칸에 공백·부호가 아닌 군더더기가 붙은 것이 있는지는 알 수 없다.
- `ci-preprocess` 구성 하나, Linux/GCC 미검증.

## 7. 잔여 위험

- 범위를 벗어난 정수(예: `"polynomial_degree":"300"`)는 전에는 8비트로 잘려 읽혔고 이제 파일 전체의 적재 거부다. 의도한 엄격화다.
