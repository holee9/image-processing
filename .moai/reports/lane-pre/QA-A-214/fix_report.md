# QA-A-214b — `xpe_defect_correct` 가 비유한 프레임을 입구에서 거부한다 (수정 보고, Refs #233)

기준: dev/preprocess `dfb40cda`. 코드 변경: `defect_correct.cpp`(입구 검사), `preprocess_api.h`(문서). 증거: `evidence/`. 푸시하지 않음.

## 결론

`xpe_defect_correct` 가 프레임에 NaN 또는 ±무한대가 하나라도 있으면(마스크 화소 포함) **`XPE_ERR_INVALID_INPUT`** 을 돌려주고 **아무것도 쓰지 않는다**: 출력 버퍼의 바이트가 그대로이고, 제자리 호출이면 입력의 바이트가 그대로다. 알림 1건(Error)으로 개수와 첫 화소 위치를 알린다. 유한 프레임의 출력은 비트 동일(9개 프레임 해시 일치). 검사 비용은 3072² 프레임당 약 **+1.3 ms**.

## 변경

입구 검사 순서에서 **기존 검사 전부 뒤, 마스크·출력 쓰기 앞**에 둔다(널·형식·크기·겹침·초기화·맵 차원 오류가 이전과 같은 우선순위로 먼저 나온다 — 시험으로 고정). 방법: 프레임을 한 번 훑으며 지수 비트가 모두 1 인지(NaN 또는 ±무한대) OR 로 모으고(분기 없는 한 번의 선형 패스), 걸린 경우에만 다시 훑어 개수와 첫 위치를 구한다.

### 알림 전체 문구 (레인 간 계약, `XPE_ALERT_ERROR`)

```
XPE_WARN_DEFECT_INPUT_NOT_FINITE: <N> pixel(s) of the input frame are NaN or infinite (first: index <I>, x=<X>, y=<Y>); the frame was not corrected and the output was not written
```

## 직접 호출자 (리더 지정 세 곳 + 확인한 것)

공개 함수를 클라이언트가 직접 부른다(`grep -rn xpe_defect_correct clients`):

| 호출자 | 위치 |
|---|---|
| GUI 미리보기 서비스 | `clients/ImageProcTest/Services/NativePreprocessPreviewService.cs:167` (델리게이트 가져오기; 호출은 같은 파일 아래) |
| 진단 합성 오라클 | `clients/ImageProcTest/Diagnostics/XpePreprocessSyntheticOracle.cs:54`, `:63` |
| 통합 시험 | `clients/ImageProcTest.IntegrationTests/P1AReady/PreprocessCorrectionChainSmokeTests.cs:78` |
| (준비 확인 목록만) | `Diagnostics/XpePreprocessReadinessProbe.cs:21`, `PInvoke/XpePreprocessNative.cs:74` — 이름만 등록, 호출 아님 |

이 호출자들이 **넘기는 입력이 유한한지**(그쪽 앞 단계: 게인 보정 출력 등)는 리더가 gui 에 확인시킨다 — 이 카드는 확인하지 않았다.

## 비유한 입력에 대한 함수별 동작 (정리 항목, 이번에는 고치지 않음)

| 함수 | 비유한 입력일 때 | 근거 |
|---|---|---|
| `xpe_defect_correct` (이제) | 입구에서 `XPE_ERR_INVALID_INPUT`, 쓰기 없음 | `defect_correct.cpp` 입구 검사 |
| `xpe_binning_correct` | 입력은 안 본다. **스케일한 결과**가 비유한이면 `XPE_ERR_PROCESSING_FAILED` — 이미 일부 화소를 곱해 쓴 뒤라 **출력이 바뀐 채** | `binning_correct.cpp:39` (곱셈 뒤 검사) |
| 고스트(`xpe_ghost_correct`) | 입력 화소가 비유한이면 `XPE_ERR_PROCESSING_FAILED`; 평균·이웃 평균 계산은 비유한 값을 건너뛴다 | `ghost_correct.cpp:97`, `:121`, `:151`, `:180`, `:200` 외 |
| `xpe_defect_detect_runtime` | **검사 없음**(float 입력; 평균은 비유한을 건너뛰는 보조 함수가 있으나 검출 경로의 입구 거부는 없다) | `runtime_detection.cpp:76-100` 입력 검증 목록에 유한성 없음 |
| `xpe_verify_gain` | 비유한·0 이하 게인 값 항목을 건너뜀 | `xpe_verify_metrics.cpp:537` |
| 게인·오프셋·비선형·온도 보상 | 입력이 uint16 이라 해당 없음(게인 **맵**의 비유한은 적재 때 분류, `XPE_WARN_GAIN_PIXELS_CLASSIFIED_DEFECT`) | — |

불일치: 비닝·고스트는 `PROCESSING_FAILED`(그것도 비닝은 쓴 뒤), 결함 단계는 원칙대로 `INVALID_INPUT`(쓰기 전). 통일은 별도 카드의 몫이다.

## 시험 (`test_defect_input_finite.cpp`, 5건)

| 시험 | 단언 |
|---|---|
| 비유한 프레임 7 가지(유효 화소의 NaN·+inf·−inf, 단독 결함 자신의 NaN, 덩어리 화소의 +inf, 0 번 화소, 마지막 화소의 −inf) | `INVALID_INPUT`, **출력 버퍼 바이트 불변**(패턴이 채워진 버퍼), 입력 불변, 알림 있음 |
| 제자리 호출 | `INVALID_INPUT`, 입력 바이트 불변(NaN != NaN 이라 바이트로 비교) |
| 알림 문구 | 개수 3, 첫 화소 index 53 (x=5, y=4) — 전체 문자열 단언 |
| 유한 프레임 | `OK`, 알림 없음, 결과가 이전과 같음; `float` 최대·최저·비정규화 최소(유한)는 통과 |
| 우선순위 | 비유한 프레임에 잘못된 차원 → `BUFFER_TOO_SMALL`, 잘못된 형식 → `UNSUPPORTED_FORMAT` 이 먼저 |

### 반증 (한 번에 하나, 전체 빌드, `evidence/30_arm_g*.txt`; 마지막에 다시 빌드함)

| 손상 | 빨강이 된 시험 |
|---|---|
| g1 입구 검사 제거 | 3건(거부·제자리·알림) |
| g2 NaN 만 검사(무한대는 통과) | 거부 시험 1건 |
| g3 마스크 화소는 검사에서 제외 | 거부 시험 1건(단독 결함 자신의 NaN) |
| g4 오류 코드를 `PROCESSING_FAILED` 로 | 3건 |
| g5 알림 제거 | 2건 |

"쓰기 전 거부" 자체(출력 바이트 불변)는 구조로 지켜지고 — 검사가 복사·마스크 읽기보다 앞에 있다 — 시험이 그 순서를 단언한다. 순서를 뒤집은 반증은 만들지 않았다(Gaps).

## 검사 비용 (`evidence/40_…txt`, `41_m214_time.py`; 같은 실행에서 옛(dfb40cda)/새 번갈아 3회, 최소값, ms)

| 프레임(3072²) | 검사 전 | 검사 후 |
|---|---|---|
| 마스크 없음(조기 반환) | 4.3 | **5.6** (+1.3) |
| 단독 2 + 쌍 1 | 7.0 | 8.3 |
| 686×686 한 덩어리 | 40.4 | 42.3 |
| 실제 결함 맵(CalData_6) | 13.0 | 14.4 |
| cyan_test `BPM.raw` | 6.9 | 8.8 |

9개 프레임 모두 출력 sha256 이 옛 빌드와 일치. 검사는 메모리 대역에 묶인 한 번의 읽기(37.7 MB)라 프레임 크기에 비례하고 마스크와 무관하다.

## 검증

`evidence/verify/`(반증 뒤 다시 빌드한 상태): preprocess 869 통과(셔플 869), 할당 실패 60, common 69·12, ctest 총 1059, 공개 export 변화 없음(diff 0), 헤더 문서 0건, 프리셋 일치 12/12.

## 미검증 (Gaps)

- GUI 가 넘기는 입력이 유한한지(리더가 gui 에 확인시킴).
- "쓰기 전에 거부" 의 순서를 뒤집은 반증.
- 검사 비용은 한 기계에서의 값이다. 다른 기계는 메모리 대역에 비례해 달라진다.
- 같은 원칙을 비닝(쓴 뒤 거부)·고스트·런타임 검출에 적용하는 일은 하지 않았다(표로만).

## 잔여 위험

- 이미 비유한 값을 넘기던 직접 호출자(있다면)는 이제 `XPE_OK` 대신 `INVALID_INPUT` 을 받는다 — 그런 호출은 이전에 NaN 이 섞인 출력을 받았을 것이다.
