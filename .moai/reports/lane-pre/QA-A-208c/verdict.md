# QA-A-208c — Codex #43 보류: 설정 블록 중간의 NUL 뒤를 검사하지 않음 (+ 리더 추가: 품질 필드 범위)

기준 커밋 `7e9752b4`(QA-A-202f) 위. 감사 원문은 `evidence/00_codex43_audit_original.md`. 이슈 #233, #234.

**리더 결정과 다르게 한 곳이 둘 있다 — 둘 다 실측 근거가 있고, 먼저 읽을 것.** (1) NUL 별도 검사를 넣었다. (2) `fit_r_squared` 의 범위를 `[0,1]` 이 아니라 "1 이하, 단 -1.0 은 금지"로 걸었다.

## 1. 주장

| # | 주장 | 근거 |
|---|---|---|
| 1 | 품질 파서는 XCal 설정 블록의 **저장된 길이 전체**를 읽는다. NUL 바이트는 텍스트의 끝이 아니라 `XPE_ERR_CONFIG_INVALID`(깨진 JSON)이며, Codex 재현 두 건과 문자열 안·끝·앞·여러 개의 NUL 이 모두 거부되고 게인 맵·품질 메타데이터는 호출 전 그대로다. | `evidence/02_red.txt`, `04_dll_full.txt`, 반증 `arm_nul_not_refused`, `arm_strlen_length` |
| 2 | 품질 네 필드는 **정의상의 범위**를 벗어나면 거부된다: `polynomial_degree` 0..4, `actual_dose_levels` 1..10, `calibration_mode` 0..4, `fit_r_squared` ≤ 1 이고 -1.0 이 아님. 경계값은 받는다. 제품 생성기가 쓴 파일은 전부 다시 적재된다. | 범위 시험, `FixtureGenGainPoly*` |
| 3 | BOM 과 잘못된 UTF-8 행이 시험에 들어갔다(레인 미검증이던 것). | 15행 × 4필드 |

## 2. 리더 결정과 다르게 한 곳 (증거 포함)

### 2.1 strict 모드는 NUL 을 거부하지 않는다 → 별도 NUL 검사를 넣었다

결정문: "strict 모드에서 문자열 밖 NUL 은 토큰 오류 … 별도 NUL 검사를 덧붙이지 말고 그것으로 거부되는지를 시험으로 확인한다". **시험으로 확인했고, 거부되지 않았다.**

- 실제 길이를 넘기고 NUL 검사는 넣지 않은 상태로 새 시험을 돌렸다(`evidence/03a_real_length_without_a_nul_check.txt`): `"{}<NUL>{\"@\":\"bad\"}"`, `"{\"@\":v}<NUL>garbage"`, `"{...}<NUL>"`, `"{...}<NUL><NUL><NUL>"` 네 행이 네 필드 모두에서 **적재 성공**(예상한 거부가 아님; 행마다 24건의 단언 실패). 문자열 안의 NUL 과 앞쪽·키 뒤의 NUL 행은 거부됐다.
- 원인(설계서가 아니라 이 빌드의 동작): nlohmann-json 3.11.3 의 어휘 분석기는 문자열 밖의 NUL(`\0`)을 **입력의 끝**(`end_of_input` 토큰)으로 읽는다. `strict=true` 는 "값 뒤에 `end_of_input` 이 와야 한다"인데 NUL 이 그 토큰이라 통과하고, NUL 뒤는 읽지 않는다. 그래서 길이만 넘겨서는 모자랐다 (Codex #43 이 제안한 두 번째 방법과 같다).
- 조치: 파싱 전에 `std::memchr(json, 0, len)` 로 NUL 이 있으면 `Malformed`. 유효한 JSON 텍스트에는 NUL 이 없으므로 거부되는 정상 입력은 없다.
- 이 검사 때문에 "길이를 넘긴다"는 결정은 쓸모가 없어진 것이 아니다: 길이를 넘기지 않으면 `memchr` 검사도 첫 NUL 앞에서 끝난다(`arm_strlen_length` 가 빨강).

### 2.2 `fit_r_squared` 는 `[0,1]` 이 아니다 — 제품 생성기가 음수를 쓴다

결정문: "XCal 의 `fit_r_squared` 가 [0, 1] 밖이면 CONFIG_INVALID … 생성기 산출물이 범위 안인지 확인." **확인했고, 범위 밖이다.**

- `[0,1]` 로 걸자 `FixtureGenGainPolyTest.GeneratedPolyFileLoads`, `LoadAloneIsSilent`, `PolyAlertIsPushed` 세 건이 빨개졌다(적재 `-4`). 진단 출력(임시, 되돌림)에 찍힌 제품 생성기의 설정 블록: `{"polynomial_degree":2,…,"calibration_mode":3,…,"actual_dose_levels":4,…,"fit_r_squared":-0.076637433,…,"calibration_pass":0}`.
- 즉 제품 생성기가 4개 선량 단계 사다리에서 **음수 R²(-0.0767)** 를 쓴다(합산 `1 - SS_res/SS_tot` 가 음수 = 평균보다 못한 적합). `[0,1]` 을 걸면 제품이 쓴 파일을 제품이 거부한다.
- 그렇다고 상한도 풀 이유는 없다: `SS_res ≥ 0` 이므로 1 을 넘는 값은 생성기가 쓸 수 없다. 그래서 `fit_r_squared ≤ 1` 이고, 기록의 "없음" 표지값 `-1.0` 은 **파일이 담을 수 없다**(`XPE_R_SQUARED_NOT_GIVEN`): 그래야 "R² 없음"과 실제 값이 섞이지 않는다는 리더의 목적이 지켜진다.
- 202f 의 이력 규칙도 같이 고쳤다. 202f 는 "R² 가 알려진 기록 = valid 이고 `r_squared >= 0`" 이었는데, 헤더가 적은 "0.0 to 1.0"을 그대로 믿은 것이었다(**202f 보고서의 "문서화된 범위 0..1" 서술이 틀렸다**). 음수 R² 는 실제 값이므로 이제 "알려진 R² = valid 이고 `r_squared != -1.0`". 새 시험 `ANegativeGeneratedR2IsKnownAndBecomesTheHistory` 가 `>= 0` 으로 되돌리면 빨강.
- **관찰만 하고 조사하지 않았다**: 생성기가 왜 음수 R² 를 내는지(합산 방식 때문인지, 적합 모델 때문인지). 결함인지 정의인지는 알 수 없다. 별건으로 두는 것이 맞다.

## 3. 범위 표 (무엇에 어떤 범위를 걸었나)

| 필드 | 범위 | 근거 | 거부되는 값 | 받는 경계 |
|---|---|---|---|---|
| `fit_r_squared` | ≤ 1.0, ≠ -1.0 | `1 - SS_res/SS_tot` (생성기, 음수 실측), 표지값 분리 | 1.0001, 2, -1.0, -1 | 1, 1.0, 0, 0.0, -0.01, -0.0766, -1.0001 |
| `polynomial_degree` | 0..4 | 생성기가 `max_degree` 1..4 만 받음(`:400`) | -1, 5 | 0, 4 |
| `actual_dose_levels` | 1..10 | 모드의 점 수: SINGLE 1 .. MULTI_POINT_10 10 | 0, 11 | 1, 10 |
| `calibration_mode` | 0..4 | `XpeCalibrationMode` SINGLE..MULTI_POINT_10, 저장되는 것은 풀린 모드(AUTO=5 아님) | -1, 5 | 0, 4 |

값은 따옴표 없는 숫자와 따옴표 있는 문자열 두 형식 모두 같은 판정이다. 헤더의 `polynomial_degree` 주석("0..3")에 `4=quartic (MULTI_POINT_10)`을 더했다(생성기가 4를 쓴다).

## 4. 경로 표 — 길이 지정과 NUL 종단

| 경로 | 입력 | 이번 처리 |
|---|---|---|
| `xpe_calib_load_gain` → `xpe_calib_parse_quality_meta_json` (품질 파서) | XCal 설정 블록, **길이 지정**(`config_copy.data()`, `.size()`) | 전체 길이를 읽고 NUL 은 거부 |
| `xpe_calib_load_gain` → `xpe_json_get_double(config_copy.c_str(), "dose_min"/"dose_max")` (다항식 선량 범위) | 같은 블록을 **C 문자열**로 | 바꾸지 않음. 첫 NUL 앞만 보지만 이 경로는 품질 파서가 먼저 블록 전체를 검증한 뒤에 온다(NUL 이 있으면 이미 거부) |
| 파이프라인 설정 JSON (`xpe_json_get_string`, QA-A-209 의 몫) | 호출자의 **NUL 종단** C 문자열 | strlen 이 곧 실제 길이 — 이번에 안 건드림 |
| `xpe_json_top_level_scalar` 의 호출 | 품질 파서만(`read_u8_field` 3 + R² 1) | 시그니처가 `(json, len, key, value)` 로 바뀜. 209 가 이 도우미를 쓰면 NUL 종단 입력은 `strlen` 을 넘긴다 |

## 5. 시험

- `AQualityConfigIsParsedToItsRealLengthNotToTheFirstNulByte`: 15행 × 4필드. NUL 8행(Codex 두 건, 문자열 안, 끝 하나, 끝 여러 개, 앞, 키 뒤, NUL 만으로 된 설정) 거부, BOM 3행(완전한 EF BB BF 는 건너뛰고 읽힘; 불완전·낱개 첫 바이트는 거부), 잘못된 UTF-8 3행(연속 바이트 오류, 절대 못 쓰는 0xFF, 짝 없는 서로게이트 escape) 거부, 정상 멀티바이트 UTF-8 은 읽힘. 거부 행은 `CONFIG_INVALID` 와 맵·메타데이터 불변.
- `AQualityFieldOutsideItsRangeIsRefusedAndTheEndsOfTheRangeAreAccepted`: 23 케이스 × (숫자/문자열). 3절 표 그대로.
- `ANegativeGeneratedR2IsKnownAndBecomesTheHistory` (OOM exe): 202f 규칙의 음수 R² 갈래.
- **기존 시험 수정 (알림)**: 208 의 11행 시험과 208b 의 22행 시험은 "좋은 값"으로 7 을 모든 필드에 썼는데, 7 은 R²(≤1)·차수(0..4)·모드(0..4)의 범위 밖이다. 필드별 값(`goodValue`: 0.75 / 3 / 7 / 4, `otherValue`: 0.25 / 2 / 5 / 1)으로 바꿨다. 행의 구조·기대(거부/읽힘)·행 수는 그대로이고, 숫자 자리가 자리표시자(`$`, `%`)가 됐을 뿐이다. 이중 값이 같은 키 중복 행의 거부 이유가 "범위"가 아니라 "중복"이어야 하므로 모든 값이 범위 안이게 했다.

## 6. 수정 전 / 후, 반증

수정 전: 새 NUL 시험 빨강(`02_red.txt`: NUL 행 6종 × 필드 수, 144 단언). 실제 길이만 넘긴 중간 상태(`03a`)는 위 2.1 의 4행이 여전히 적재 성공. 최종: DLL 780 실행 / 772 통과 / 8 건너뜀.

반증(한 번에 하나, 전체 빌드, 복원 뒤 `cmp` 동일):

| 팔 | 손상 | 빨개진 시험 |
|---|---|---|
| nul_not_refused | NUL 검사 삭제 | NUL/길이 시험 |
| strlen_length | 적재가 `strlen` 길이를 넘김(리더의 반증) | NUL/길이 시험 |
| marker_accepted | -1.0 허용 | 범위 시험 |
| r2_above_one_accepted | R² 상한 삭제 | 범위 시험 |
| r2_lower_bound_zero | R² 하한 0 (리더 원안) | 범위 시험 **그리고 `FixtureGenGainPoly` 세 건** (생성기 파일이 안 읽힘) |
| u8_ranges_widened | 세 정수 범위를 0..255 로 | 범위 시험 |
| history_r2_nonnegative | 이력 규칙을 `>= 0` 으로 | 음수 R² 이력 시험 |

(marker_accepted 의 첫 실행에서는 NUL/길이 시험 중 `write_xcal_file` 이 한 번 -9(입출력 오류)를 내 그 시험도 빨개졌다. 재실행에서는 범위 시험만 빨갰고 이후 같은 시험 3회와 전체 검증에서 재현되지 않았다. 원인은 조사하지 않았다. 첫 출력은 `arm_marker_accepted_dll_first_run_transient_io.txt` 에 남겼다.)

## 7. 실측 (이 트리, 이 실행)

| 항목 | 관측 |
|---|---|
| 빌드 | `BUILD_EXIT=0` (`21_build_final.txt`) |
| preprocess DLL 시험 | 780 실행 / 772 통과 / 8 건너뜀, 섞기도 동일 |
| preprocess OOM exe | 43 통과 (직전 42 + 1) |
| 게인 적재 할당 실패 스윕 | 91 / 91 / 99 / 101 / 93 / 4 지점 전부 통과(208b 와 같음; 이번 변경은 할당을 늘리지 않음) |
| common OOM / common | 12 / 69 통과 |
| `ctest -N` | 944 (직전 941 + 3) |
| 수출 이름 / 헤더 문서 / 프리셋 | 48·16 불변 / 0 findings / 12 of 12 |

## 8. 미검증 (Gaps)

- 생성기가 음수 R² 를 내는 **이유**(2.2 끝). 조사하지 않았다.
- 범위 밖 `fit_r_squared` 가 들어 있던 **현장 XCal** 은 점검할 수 없다. 1 을 넘거나 -1.0 인 값을 가진 파일은 이제 적재가 거부된다(생성기는 그런 값을 쓸 수 없다).
- 다항식 선량 범위(`dose_min`/`dose_max`)는 여전히 첫 출현 규칙(`xpe_json_get_double`)이다. 품질 파서가 먼저 블록 전체를 검증해 NUL 은 막히지만, 키가 중첩 객체에 있는 경우 등은 별건(209 이 파이프라인 설정에서 같은 규칙을 다루고 이 경로가 후속 카드가 될 수 있음).
- 1 MiB 깊이 스트레스, 매우 큰 설정은 실행하지 않았다(Codex #43 도 같은 점을 적었다).
- `ci-preprocess` 구성 하나, Windows/MSVC 에서만.

## 9. 잔여 위험

- nlohmann 의 NUL 처리는 라이브러리 버전에 묶인 동작이다. 버전이 오르면 NUL 을 다르게 처리할 수 있지만, 이제 우리 검사가 먼저 걸러 영향이 없다.
- 이력의 "알려진 R²" 가 `!= -1.0` 이라 정확히 -1.0 인 실제 R² 는 "없음"과 같은 취급을 받는다. 파일은 -1.0 을 못 담지만(거부), 생성기가 정확히 -1.0 을 내면(사실상 불가능) 이력에서 빠진다.
- 위 2.2 의 결정이 리더 원안과 다르다. 원안(≥0)으로 가려면 생성기를 먼저 고쳐야 한다(`r2_lower_bound_zero` 팔이 보인 대로 지금은 제품 파일이 거부된다).
