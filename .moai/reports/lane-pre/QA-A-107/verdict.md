# QA-A-107 — 다항식 게인 적재의 조용한 실패 제거, 게인 값 범위 검사 (#187, #188)

커밋 `0348337`(1부), `2ea598c`(2부). dev/preprocess, 미푸시, 기준 main `cedce22`

## 1. 주장

### 1.1 #187 (구현함)
1. 카드의 (a)·(b) 를 그대로 고르면 각각 다른 근거와 부딪힌다.
   - (a) 적재 거부: 이 적재 경로가 다항식 파일의 FUNC-033 (5) 품질 메타데이터를 읽는 유일한 경로다(QA-A-37/#140, 시험 `GainPolyLoadTest.PolyMetadataSurvivesTheRoundTrip`). 거부하면 그 기능이 사라진다.
   - (b) 스칼라 맵 유지: 운영자가 다항식 파일을 지정했는데 이전 스칼라 맵으로 영상이 보정된다. SRS-CALIB-SAFE-003 은 부분·혼합 교정을 금지한다("either all calibration files load successfully, or none are loaded").
2. 그래서 **실패를 없애는 대신 실패를 말하게** 했다. 조용한 실패의 정의(아무 설명 없이 "교정 없음"으로 보고)를 없앤다.
   - 적재: 지금처럼 성공하고 메타데이터도 읽는다. 다항식 파일이면 `XPE_ALERT_WARNING` 으로 "계수는 읽었지만 적용되지 않는다(#187), 스칼라 맵을 적재할 때까지 게인 보정은 거부된다"를 올린다.
   - 보정: 스칼라 맵이 없고 다항식 계수만 있으면 `XPE_ERR_CALIB_NOT_LOADED` 대신 **`XPE_ERR_UNSUPPORTED_FORMAT`** 을 내고 `XPE_ALERT_ERROR` 를 올린다.
   - 스칼라 맵 삭제는 유지한다(위 SAFE-003).
3. QA-A-105 의 동작 고정 시험 3건과 기존 시험 1건을 새 동작으로 바꿨다. 옛 기대가 틀린 이유는 §2.3.

### 1.2 #188 게인 값 범위 검사 (리더 결정 뒤 구현함)
4. 두 번 멈춰 보고했고, 리더 결정을 받아 구현했다(경과는 §4).
   - 오류 코드 `XPE_ERR_INVALID_CALIB_DATA = -17` 을 열거형 **끝에** 추가했다. 기존 -1..-16 은 그대로다.
   - `xpe_calib_load_gain` 이 스칼라 게인 맵의 값이 [0.1, 10.0] 밖이면 그 코드로 거부한다. 다항식 계수는 이 검사 대상이 아니다.
   - 출처 없는 상수(0.001 / 1000)에 맞춰져 있던 기존 시험 13건을 요구 기준으로 바꿨다(§4c).
   - `gain_correct.cpp` 의 `AC-GAIN-005` 인용을 SRS-CALIB-001 FUNC-002 로 바꾸고, 그 검사가 이제 공개 경로로 도달하지 않는 방어선이라는 사실을 주석에 적었다.

## 2. 증거

### 2.1 코드
- `gain_correct.cpp`: 초기화·스칼라 맵 검사 사이에 다항식 분기를 넣었다.
- `xpe_calib_load_gain.cpp`: 커밋 블록에서 `poly_loaded` 를 기록하고, 메타데이터 적용 뒤 경보를 올린다.

### 2.2 시험 (모두 통과, `a107-run-p1b.log`)
- 대조: 스칼라 맵이면 1000/2 = 500 이 나온다(변화 없음).
- 다항식 적재 → 경보 문구 "no correction applies G(x,y,E)" 가 대기열에 있다.
- 그 상태의 보정 → `XPE_ERR_UNSUPPORTED_FORMAT`, 경보 "does not apply it", 출력 버퍼는 그대로(-1).
- 스칼라 맵이 있던 상태에서 다항식을 적재 → 보정은 `XPE_ERR_UNSUPPORTED_FORMAT`. 버퍼에는 직전 결과(500)가 남아 있고 새로 쓰이지 않았다.
- `BUILD_EXIT=0`, warning C 0. 전처리 `ctest` 686건 통과, 종료 0.

### 2.3 옛 기대가 틀린 이유
- QA-A-105 의 세 시험은 `XPE_ERR_CALIB_NOT_LOADED` 를 기대했다. 그 코드의 뜻은 "모듈에 교정이 없다"이다. 그러나 적재는 **성공**했고 계수는 메모리에 있다. 호출자는 "없다"는 답에서 자기가 적재한 파일이 원인이라는 사실에 이를 수 없다 — 이것이 조용한 실패다.
- 기존 시험 `GainPolyLoadTest.LoadingPolyClearsTheScalarGainMap` 도 같은 코드를 기대했다. 맵이 지워지는 것(의도)은 그대로이고, 그 상태를 알리는 코드만 바뀌었다.

### 2.4 반증
각각 한 곳만 원래대로 되돌려 다시 빌드했다.

| 되돌린 것 | BUILD_EXIT | 결과 |
|---|---|---|
| `gain_correct` 의 다항식 분기 삭제 | 0, warning 0 | 3건 FAILED |
| 적재 경보 블록 삭제 | 0, warning 0 | 1건 FAILED |

- 1차 시도(`if (false && …)`)는 C4127/C4702 로 `BUILD_EXIT=1` 이었다. 그때 찍힌 "PASSED 11" 은 낡은 바이너리라 버렸다.

## 3. 기준 귀속
- 기기·빌드는 QA-A-102~106 과 같다(i7-12700, `build/ci-preprocess` RelWithDebInfo).
- 대조(스칼라 경로 500)는 같은 시험 파일 안에서 독립적으로 계산한 값이다(1000/2).

## 4. #188 — 2차 시도와 두 번째 막힘 (리더 결정 (i) 적용 후)

리더가 (i) 새 오류 코드로 결정해서 다음을 구현하고 측정했다가 **되돌렸다**(현재 트리에는 없다).
- `XPE_ERR_INVALID_CALIB_DATA = -17` 을 열거형 **끝에** 추가(기존 번호 변경 없음: -1..-16 그대로).
- `xpe_calib_load_gain` 적재 시점에 [0.1, 10.0] 검사(스칼라 맵만. 다항식 계수는 게인 값이 아니다).
- 시험 4건(범위 안 6값 통과, 범위 밖 5값 거부, 한 화소만 불량이어도 거부, 거부된 파일이 기존 교정을 대체하지 않음) → 통과.
- 반증(검사 제거) → 3건 빨강, `BUILD_EXIT=0`, warning 0.

**되돌린 이유**: 전체 ctest 에서 **기존 13건이 빨강**이 되었고(`a107-ctest2.log`), 원인이 요구끼리의 충돌이었다.

| 출처 | 범위 | 오류 코드 |
|---|---|---|
| SRS-CALIB-001 FUNC-002 | [0.1, 10.0] | `XPE_ERR_INVALID_CALIB_DATA` |
| 구현 `gain_correct.cpp:30-31` | [0.001, 1000.0] | `XPE_ERR_CONFIG_INVALID` |

- 구현 쪽 근거로 코드가 인용하는 `AC-GAIN-005` 는 SPEC 에 없다(검색 범위 `.moai/specs`, `docs` 전체: AC-GAIN-001/002/003 만 존재).
- 빨강이 된 13건: `GoldenGainTest` 2건, `GainCorrectReciprocalFMATest` 11건. 모두 `xpe_calib_load_gain` 으로 0.0005·0.001·0.01·0.5·100·1000·0·음수·NaN·Inf 같은 값을 적재해 **보정 단계** 검사를 시험한다.
- 적재에서 먼저 막히면 보정 단계의 `is_valid_gain` 은 공개 API 로 도달할 수 없게 된다. 리더 지시는 그 검사를 그대로 두라는 것이었으므로, 이 상태로는 지시를 동시에 만족할 수 없다.
- 교정 픽스처 자체는 문제없다(`MakeGainXCal` 기본 1.0, 생성기는 평균 1.0 정규화). 범위 밖 값은 **시험이 일부러 넣는 값**이다.
- 현재 트리는 되돌린 상태이고 전체 686건 통과(`a107-ctest3.log`).

**리더 결정(받음)**: 범위는 SRS 의 [0.1, 10.0], 적재 시점 거부, 코드 -17. 13건은 "적재가 거부되는 것"을 단언하도록 바꾼다. `is_valid_gain` 은 방어선으로 남기고 도달하지 않는다는 사실을 주석에 적는다. `AC-GAIN-005` 부재는 리더가 #188 에 기록한다.

## 4c. 결정 적용 결과 (커밋 `2ea598c`)

- 적재 검사와 오류 코드를 다시 넣었다. 시험 5건 추가(범위 안 6값, 범위 밖 5값, 한 화소만 불량, 거부된 파일이 기존 교정을 대체하지 않음, 다항식 계수는 이 범위에 매이지 않음) → 통과.
- 기존 13건 갱신

| 시험 | 옛 기대 | 새 기대 |
|---|---|---|
| `GoldenGainTest.ZeroGainReturnsConfigInvalid` → `…ZeroGainIsRefusedAtLoad` | 보정이 `XPE_ERR_CONFIG_INVALID` | 적재가 `XPE_ERR_INVALID_CALIB_DATA` |
| `GoldenGainTest.FormulaMatchesExactFloat` | 표에 S=100 | S=10(범위 최대). 나머지 15개 값은 그대로 |
| `GainCorrectReciprocalFMATest` 0 / 음수 / NaN / Inf / 0.0005 / 1001 (6건) | 보정이 `CONFIG_INVALID` | 적재가 `INVALID_CALIB_DATA`, 이름도 `…IsRefusedAtLoad` 로 |
| `GainAtMinimumValueSucceeds`(0.001) → `GainAtTheRequirementMinimumSucceeds` | 0.001 에서 성공 | 0.1 에서 성공 |
| `GainAtMaximumValueSucceeds`(1000) → `GainAtTheRequirementMaximumSucceeds` | 1000 에서 성공 | 10.0 에서 성공 |
| `SmallGainProducesLargeOutput` | 0.01 → 200000 | 0.1 → 20000 |
| `ReciprocalAccuracyAcrossGainRange` | {0.5, 1, 2, 10, 100} | {0.1, 0.5, 1, 2, 10} |
| `LargeInputWithLargeGain` | 게인 100 | 게인 10 |

- 옛 기대가 틀린 이유(각 시험 파일 주석에 기록): 기대의 기준이 `MIN_GAIN_VALUE`/`MAX_GAIN_VALUE`(0.001 / 1000)였고, 그 상수가 인용하는 `AC-GAIN-005` 는 SPEC 에 없다. 요구는 FUNC-002 의 [0.1, 10.0] 이다.
- 시험 보조 함수: 적재 결과를 단언하지 않고 **돌려주는** `publishAndLoad` / `load` / `writeCalibMap` 을 넣어, 거부를 시험할 수 있게 했다.
- 반증: 적재 검사를 빼면 10건 빨강(`BUILD_EXIT=0`, warning 0, `a107-run-f4-range.log`).
- 전체: 전처리 `ctest` **691건 통과**, 종료 0(`a107-ctest4.log`).
- C# 미러 두 파일(`clients/ImageProcTest.IntegrationTests/PInvoke/XpeCommonNative.cs`, `clients/ImageProcTest/PInvokeWrapper.cs`)은 clients 소유라 손대지 않았다. `ErrorCodeHeaderParityTests` 가 헤더와 대조하므로, 새 코드 -17 이 추가될 때까지 그 시험은 빨강이 된다(리더가 gui 레인에 넘김).

## 4b. 최초 막힘(오류 코드 부재)과 선택지

| 선택지 | 내용 | 영향 |
|---|---|---|
| (i) 새 오류 코드 추가 | `XPE_ERR_INVALID_CALIB_DATA` 를 `xpe_error.h` 에 추가 | 공개 헤더에 코드가 하나 는다. 기존 값과 겹치지 않는 번호를 골라야 하고, C# 쪽 열거형(`XpeCommonNative`)도 맞춰야 한다(Lane C 소유) |
| (ii) 기존 코드 사용 | 범위 밖이면 `XPE_ERR_CONFIG_INVALID` | 헤더 변경 없음. 현재 게인 값 유효성 검사(`is_valid_gain`)가 보정 단계에서 쓰는 코드와 같아 일관적이다. SRS 문구와 이름이 다르다 |
| (iii) SRS 를 코드에 맞춤 | 요구 문구를 `XPE_ERR_CONFIG_INVALID` 로 정정 | 코드 변경 없음. 판단은 lead |

- 참고 사실: 보정 단계에는 이미 값 검사가 있다. `gain_correct.cpp` 의 `is_valid_gain` 이 유한하지 않거나 0 이하인 값을 `XPE_ERR_CONFIG_INVALID` 로 거부한다. SRS 의 [0.1, 10.0] 범위와는 다르다.
- 적재 단계에는 검사가 없다(검색: `xpe_calib_load_gain.cpp`).
- 기존 픽스처·시험 데이터가 [0.1, 10.0] 을 지키는지도 아직 확인하지 않았다(검사를 넣지 않았으므로). 결정이 오면 함께 확인한다.

## 5. 미검증
- 경보 대기열이 가득 찬 상태에서 이 경보가 밀려나는 경우는 시험하지 않았다(`xpe_error.h` 는 가장 오래된 INFO → WARNING → ERROR 순으로 밀어낸다고 적는다).
- 파이프라인 경로(`xpe_preprocess_pipeline`)에서 다항식 게인을 적재했을 때의 동작은 이 카드에서 따로 시험하지 않았다. 게인 단계가 같은 함수를 부르므로 같은 코드가 나온다(판독).
- C# 미러 갱신 뒤의 `ErrorCodeHeaderParityTests` 결과는 이 레인에서 확인하지 않았다(clients 소유).
- 범위 검사는 적재 경로에만 있다. 교정 맵을 다른 경로로 주입하는 API 는 없다(검색: `modules/preprocess/src`).

## 6. 잔여 위험
- `XPE_ERR_UNSUPPORTED_FORMAT` 은 원래 화소 형식·크기 미지원에 쓰던 코드다. 이제 "모델 미지원"도 같은 코드를 쓴다. 호출자가 두 경우를 코드만으로 구별할 수 없다(경보 문구로는 구별된다).
- 경보는 큐에 쌓인다. 읽지 않는 호출자에게는 반환 코드만 남는다.
