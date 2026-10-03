# QA-A-228 — SPEC-XPE-P1A 작업표·요구 실태 대조 (보고서, 코드 변경 없음)

카드: QA-A-228 · 트리: `dev/preprocess` `f5b314ea` · 증거: 같은 디렉터리(`evidence_greps.txt`, `evidence_verified_claims.txt`, `req_census_out.txt`, `func_census_out.txt`, 스크립트 3개는 `.txt` 로 저장) · 수정안: `spec_table_draft.txt`, `rtm_draft.txt` (`.moai/specs/`·`docs/` 는 리더 소유라 고치지 않았다)

판정 기준(카드 그대로): **구현됨** = 시험이 그 요구 문구를 독립된 기대값으로 단언한다. 자기 비교·상수 단언·"OK 만 반환" 확인은 **부분**. **없음** = 코드가 없다. REQ 문구별 판정은 읽기 전용 에이전트 셋이 시험 본문을 읽어 1차로 모았고, 이 보고서에 쓴 핵심 주장은 제가 코드에서 다시 확인했다(`evidence_verified_claims.txt` — 다시 확인하지 않은 것은 §6 에 적었다).

## 0. 결론 먼저

1. **표가 틀린 곳은 `pending` 만이 아니고, 방향이 셋이다.** 정의된 REQ-P1A-* 48개 중 **구현됨 7 · 부분 40 · 단언 없음 1**.
   - **표 `Pending` → 실제 구현됨(표가 낡음)**: REQ-010·011·012·013 (`spec.md §8`·`progress.md` 가 "M2 대기"라 적지만 2026-04-19 에 구현됐고 frontmatter 는 "M2 Complete"). 040 도 같다.
   - **표 `Implemented` → 부분**: REQ-014·015·018·019 와 캐시 102~105 (+ 016·017 은 엄격 규칙으로 부분). 문구가 코드와 다르거나 구현이 없는 문구가 들어 있다.
   - **표 `Existing` → 부분/없음**: REQ-002·003·004·005·020·022·030~033. 특히 **REQ-004 는 단언이 하나도 없다**.
2. **세션 일치 검증(M3-9, REQ-014 의 "check session matching")은 구현이 없는데 공개 헤더가 오류 코드까지 약속한다.** 세션 ID 를 저장·복사만 하고 비교하는 코드가 0건(대조군 포함)이며, 헤더는 `XPE_ERR_CONFIG_INVALID if session mismatch`(`preprocess_api.h:135`)라 적는다. 이것이 이번 대조에서 나온 가장 분명한 결함 후보다.
3. **REQ 문구 중 현재 코드와 어긋난 것이 최소 14건**이고, 그중 3건(087·088·096)은 **이번 세션의 QA-A-226 계열 변경 뒤에 spec 문구를 갱신하지 않아 생긴 것**이다(제가 코드를 바꿀 때 spec 은 리더 소유라 건드리지 않았고, 갱신 요청도 하지 않았다 — 제 누락이다).
4. **plan.md 작업표의 파일 이름 24개 중 실제로 있는 것은 `xpe_calibration.cpp` 하나뿐**(대조군 `offset_correct.cpp` 존재). 작업 행 38개 중 REQ ID 를 적은 행은 **0개**라, 작업↔REQ 연결은 행에서 읽을 수 없다.
5. **정의된 REQ 48개 중 18개는 작업표·`§8`·`progress.md` 어디에도 안 걸린다**(§2). 이름으로 센 "시험이 인용하지 않는 REQ 17개"는 행위로 다시 세면 **0개**다(17개 모두 일부 시험이 있다). 이름 census 는 양쪽으로 틀린다(§3).
6. **RTM 의 `✓`(§4 "24/24 traced") 는 시험이 아니라 문서의 시험 사례 ID 를 가리킨다.** 그 UT/IT/ST ID 는 어떤 시험 소스에도 **0건**이다. SRS 가 이름 붙인 기능이 코드에 아예 없는 FUNC 는 011·021·028·029·030 이고, 그중 **011·021 은 RTM 이 `✓`**(§4).
7. **새 이슈가 필요하다**(`Refs #233` 은 리더가 방금 닫았다). 결함 후보 목록은 §5, 수정안은 `.txt` 초안 둘.

## 1. 작업표 대조 — `plan.md` 의 작업 행 38개

"표 상태" 칸에는 작업 행에 상태 열이 없어서 그 작업이 맡은 REQ 가 `spec.md §8`/`progress.md` 에서 가진 상태를 적었다(작업→REQ 연결은 행이 적지 않아 **제가 추정**했다). 시험 이름은 `Suite.Name` 이고 줄 번호는 에이전트 또는 직접 확인한 것이다.

| 작업 | 표 상태 | 실제 | 근거 | 문구 중 시험이 단언하지 않는 것 |
|---|---|---|---|---|
| M1-1 CMake | — | **구현됨** | `modules/preprocess/CMakeLists.txt` | (시험 대상 아님) |
| M1-2 API 헤더 "14개 함수" | — | **구현됨(수치 낡음)** | `preprocess_api.h`: 헤더 머리말 "15 API functions", `XPE_API` 선언 **48** | 헤더 이름 `xpe_preprocess_api.h`·함수 수 14 가 낡았다 |
| M1-3 lifecycle | `Existing`(001) | **구현됨** | `LC001_InitWithDefaultConfig`, `TheProbeDetectsAnInitializedModule` | 유효 설정이 내부 상태를 바꾼다는 단언 없음 |
| M1-4 빌드 통합 | — | 미확인 | CI 가 빌드한다는 것 외에 이번에 확인 안 함 | — |
| M1-5 통합 시험 골격 | — | **부분** | `tests/test_integration.cpp` 존재(이름이 계획과 다름); 성능 시험은 `DISABLED_PipelinePerformance3072x3072`(500 ms) | 파이프라인 500 ms 기준이 꺼져 있다 |
| M2-1 offset | **Pending**(010) | **부분 — 구현됨, 표 낡음** | `OffsetCorrectTest.SubtractsOffsetFromRawPixels`(800), `ClampsUnderflowToZero`, `InitializedWithoutCalibrationReturnsCalibNotLoaded`, `ShippedPathMatchesTheScalarReference`(차이 0) | 성능 <55/15 ms, 잔차 <2 ADU 시험 없음; 알고리즘 줄의 `_mm256_subs_epu16` 은 낡음(실제는 float 경로) |
| M2-2 gain | **Pending**(011) | **부분 — 구현됨, 표 낡음** | `AppliesGainCorrection`(2000/1.5), `GoldenGainTest.FormulaMatchesExactFloat`(16행), 패리티 ≤1 ULP | 성능·flat-field 잔차 없음; "UINT16 output no overflow" 줄이 같은 REQ 의 FLOAT32 결과와 모순 |
| M2-3 defect bilinear | **Pending**(012) | **부분 — 구현됨, 문구 다름** | `InPlaceMatchesOutOfPlace`, `DefectFill*`, `CorrectionRecallOnSyntheticDefects`(≥0.99) | **작업 문구는 "bilinear"**, REQ 는 4이웃 평균+군집 중앙값. 성능 <21 ms 게이트·그래디언트 억제·">50% 이웃 결함" 규칙 시험 0 |
| M2-4 nearest/median 모드 | Pending | **부분(문구가 없는 기능을 말함)** | 호출 단위 모드 선택이 없다(REQ-012: "no per-call configJson"); 군집만 중앙값 | "nearest/median **모드**"는 선택 가능한 모드로는 존재하지 않는다 |
| M2-5 runtime 검출 | **Pending**(013) | **부분 — 구현됨, 시그니처 낡음** | `BrightOutlierIsFlaggedAtItsOwnCoordinate`, `RuntimeDetectionRatesTest.TprReachesTheFloorAtTenSigma`, `FprOnCleanFramesMeetsTheRequirement` | REQ 시그니처 `(img, defectMapOut, configJsonOrNull)` ↔ 헤더 `(image, metadata, defect_map_output)`; `hampel_threshold` 0건; 줄무늬 프레임 TPR 0.9865 는 알려진 미달; 성능 1.3× 단언은 정지(절대 400 ms); 0/1 값 도메인 단언 없음 |
| M2-6 입력 검증·불일치 가드 | `Existing`(005·021·022) | **부분**(021 은 구현됨) | `CalibDimMismatch.*` 4건(출력 불변까지), `NullRequiredPointerWinsOverInitializationState`(28행) | 005: 0 치수 시험이 일부 함수만, "출력 불변" 거의 없음; 022: defect 만 정확 단언, offset·gain 시험은 세 코드 중 아무거나 허용 |
| M3-1 XCal 파서 | — | **구현됨(파일명 다름)** | `xcal_reader.cpp`; `test_xcal_reader.cpp`·`test_xcal_validator.cpp` | 시험 본문은 이번에 읽지 않음(§6) |
| M3-2 SHA-256 | — | **구현됨** | `LoadOffset_TamperedPayload_ReturnsConfigInvalid`(`test_xpe_calib_load.cpp:152`) | SHA-256 값을 독립 계산해 대조하는 시험은 없고, 변조 거부만 단언 |
| M3-3 load_offset | `Implemented`(014) | **부분** | `LoadOffset_ExpiredFile_ReturnsCalibrationExpired`, `OffsetCacheHitLeavesThatMapInTheGlobalStore` | **세션 일치: 구현 0·시험 0**; mutex 하 커밋 단언 없음 |
| M3-4 load_gain | `Implemented`(015) | **부분** | `AppliesGain...`, `GainCacheHitLeavesThatMapInTheGlobalStore`(250) | **kVp 보간 표: 구현 0**(헤더 `:142-143` 은 "multi-SID interpolation" 광고) |
| M3-5 load_defect_map | `Implemented`(016) | **부분(엄격)** | `DefectCacheHitLeavesThatMapInTheGlobalStore`, `RoundTrip_Defect_BitIdentical` | mutex 하 커밋 단언 없음(에이전트는 REQ 의 나머지 문장은 구현됨으로 판정) |
| M3-6 generate_offset | `Implemented`(017) | **부분(엄격)** | `SigmaClipProducesADifferentMapThanMean`(184.6/106.5), `NMinFormulaMatchesTheSpec`, `NMinMarksLandInTheGlobalDefectMap` | winsor 등은 공개 API 가 아니라 내부 shim 으로만 단언 |
| M3-7 check_expiry | `Implemented`(018) | **부분 — REQ 문구가 코드와 다름** | `CheckExpiryTest.*`(`is_expired`·`remaining_days` 를 단언) | 문구: `XPE_ERR_CALIBRATION_EXPIRED` 반환 + `(filePath, expiryEpochMsOut)` ↔ 코드 `(filepath, bool*, int32_t*)`, 만료 시에도 `XPE_OK` |
| M3-8 save | `Implemented`(019) | **부분** | `RoundTrip_*_BitIdentical`, `CalibSaveExpiryTest.*` | 맵 없음: 문구 `CALIB_NOT_LOADED` ↔ 코드 `INVALID_INPUT`; 그것을 시험하는 `SaveOffset_WhenNotLoaded_ReturnsInvalidInput` 은 **`GTEST_SKIP`**; gain·defect 의 만료 기록 시험 없음 |
| **M3-9 세션 일치** | — | **없음** | `session_id` 를 저장·복사만 한다(46건 언급), 비교 `strcmp/memcmp` 0건(대조군 8건) | 요구된 동작 전부. 헤더는 약속하고 시험 헬퍼는 `session_id` 인자를 무시한다(`test_xpe_preprocess_calibration.cpp:34`) |
| M4-1~M4-3 보정 시험 | — | **구현됨(파일명 다름)** | `test_offset_correct.cpp`, `test_gain_correct.cpp`, `test_defect_correct.cpp`, `test_golden_reference.cpp` | — |
| M4-4 calibration 시험 | — | **구현됨(파일명 다름)** | `test_calibration_manager.cpp`, `test_calibration_roundtrip.cpp`, `test_xpe_calib_*.cpp` | — |
| M4-5 파이프라인 통합 | — | **구현됨** | `test_pipeline_stages.cpp`, `test_pipeline_stage_values.cpp`, `test_pipeline_ex.cpp` | M1-5 의 성능 시험은 꺼짐 |
| M4-6 합성 데이터 스크립트 | — | 미확인 | 계획의 `tests/test_data/` 는 없다 | — |
| M4-7 SIMD 패리티 harness | — | **구현됨(다른 형태)** | `test_offset_correct_avx2_parity.cpp`, `test_gain_correct_avx2_parity.cpp`, `test_runtime_detection_avx2_parity.cpp` | 계획 이름 `test_simd_parity.cpp` 는 없음(M5 정정이 이미 적음) |
| M4-8 1000회 누수 시험 | — | **부분** | `NoMemoryLeakAfter1000Frames`(+`ControlLeakIsCaught`), `EnduranceTest.LoadCycles_*` | 반복에 offset·gain·defect 보정이 없다; 힙 걷기는 Windows 전용(그 외 `GTEST_SKIP`); common 의 1000회 시험은 `SUCCEED()` 로 끝난다 |
| M4-9 커버리지 85% | — | **없음(근거 없음)** | `QA-A-08/gate.md:169` "커버리지 타깃 없음"; lane-pre 보고서에서 preprocess 측정값을 찾지 못함 | 85% 달성 증거 |
| M5-1~M5-5 | **철회/설계 변경**(표에 적혀 있음) | 표와 맞음 | `plan.md` M5 정정(QA-A-75) | — |
| M5-6 성능 벤치(3072²) | — | **부분** | `DISABLED_PipelinePerformance3072x3072`(꺼짐); 검출만 절대 400 ms 게이트; 보정 경로는 "회귀 게이트 없음"(spec 이 스스로 적음) | 보정 세 함수의 성능 기준 |
| M6-1 readout 검증 | `Pending`(041) | **부분** | `AllZeroColumnReportsDroppedColumn`, `FullySaturatedImageReportsNonuniformGain` | **선 잡음 구현 0**(#232); 포화는 행 평균만 |
| M6-2 param range | `Pending`(042) | **부분** | `EveryNamedParameterReturnsAnOrderedRange`(순서만) | **부위별** 한계: 코드 0·시험 0; 한계 값 단언 0 |
| M6-3 readout 시험 | — | **구현됨(파일명 다름)** | `test_readout_validate.cpp` | — |

### 1.1 표 상태 요약

| 표 상태 → 실제 | 작업·REQ |
|---|---|
| **`Pending` → 구현됨(표 낡음)** | M2-1~M2-5 (REQ-010~013), REQ-040 |
| **`Implemented` → 부분** | M3-3·M3-4·M3-7·M3-8 (014·015·018·019), M3-5·M3-6 (016·017, 엄격 규칙) |
| **표에 없음 → 없음** | **M3-9 세션 일치**, M4-9 커버리지 증거 |
| 표와 맞음 | M1-1·M1-3, M5-1~M5-5 |
| 계획의 파일 이름이 현실과 다름 | M1~M4 대부분(§2.2) |

`spec.md` frontmatter 는 "M2 Complete", `plan.md` frontmatter 는 "In Progress (SUP-01 complete, M2 pending)" 로 **서로 모순**이고(`plan.md:11`), `spec.md §8` 의 "Pending Features" 와 `progress.md` 도 010~013 을 대기로 적는다.

## 2. 어떤 작업에도 걸리지 않은 REQ

### 2.1 연결 census

파서: `spec.md` 의 `#### REQ-P1A-NNN` 제목. 대조군 `REQ-P1A-010` 인식(true), 없는 `REQ-P1A-999` 인식 안 함(false). **정의 48개**(`req_census_out.txt`).

| 연결 대상 | 걸린 REQ |
|---|---|
| `plan.md` 작업 행(M1-1~M6-3, 38행) | **0개** — 어느 행도 REQ ID 를 적지 않는다 |
| `spec.md §8` 표 | 이름으로 21개, 그 밖에 범위 표기 `001~009`·`020~022`·`030~033` 으로 일부 |
| `progress.md` | 13개 |
| 위 셋 중 어디에도 없음 | **18개** |

작업에 안 걸린 18개: `016a`, `020a`, `080`, `081`, `082`, `085`, `086`, `087`, `088`, `090`, `091`, `095`, `096`, `097`, `098`, `099`, `100`, `101` (온도 보상·고스트·비닝·파이프라인 블록 전체와 `#211` 이후 신설된 가드 둘). 이 블록은 `spec.md` 가 2026-09-28 에 신설했고(`#211`) plan·§8·progress 는 그 이후 갱신되지 않았다.

### 2.2 계획이 적은 이름과 현실

`plan.md` 가 적은 파일 이름 24개(`xpe_offset_correction.cpp`, `xpe_gain_correction.cpp`, `xpe_defect_correction.cpp`, `xcal_parser.cpp`, `test_offset_correction.cpp`, `test_simd_parity.cpp` 등) 중 **존재하는 것은 `xpe_calibration.cpp` 뿐**이다. 실제 파일은 `offset_correct.cpp`, `gain_correct.cpp`, `defect_correct.cpp`, `xcal_reader.cpp`, `test_offset_correct.cpp` 등이다(`evidence_greps.txt` G3). 이름이 어긋난 채 "작업 행에서 파일로" 추적하는 것은 불가능하다.

## 3. 이름 census 대 행위 census (REQ 별)

시험 파일이 REQ ID 를 인용하는 수(이름 축)는 **17개가 0건**이다: `016a 020a 022 030 042 086 095 096 097 098 099 100 101 102 103 104 105`. 행위로 다시 세면 **17개 모두 일부 시험이 있다**(아래), 반대로 ID 를 여러 번 인용하는 REQ 도 문구를 단언하지 않는 것이 있다.

| 이름으로는 0건 | 실제 시험 |
|---|---|
| 016a, 102~105 | `PipelineExTest.*`, `CacheGlobalStore.*`, `CacheSameVerdict.*`(REQ 문구와 독립 기대값 있음) |
| 095~101 | `PipelineStageTest.*`, `PipelineStageValueTest.*`, `PipelineComboTest.*` |
| 020a, 022, 086 | `NullRequiredPointerWinsOverInitializationState`, defect 의 `ThePrecedenceOfTheOlderErrorsIsKept`, NULL 핸들 시험 |
| 030, 042 | `OomInjection` 스윕, `ParamRangeNamesTest.*` |

반대 방향: **REQ-004 는 시험이 인용한 REQ 이지만 반환값 집합을 단언하는 시험이 없다**(인용 1건은 `test_readout_validate.cpp`). 인용 건수는 판정 근거가 되지 못한다.

그 외 인용 문제: `test_req_p1a_066.cpp` 는 **정의되지 않은 `REQ-P1A-066`**("4 error path unit tests")을 이름으로 쓴다(`quality-report.md:147` 도 같다). spec 은 옛 번호(007·008·034·043·045·046·049)를 "옛 REQ 와 다르다"는 설명 문장에서만 언급하고, 정의 제목으로는 48개뿐이다.

## 4. REQ 별 대조 — 정의 48개

판정: 7 구현됨 · 40 부분 · 1 단언 없음. 표의 "단언하지 않는 것"은 REQ 문장 중 시험이 독립된 기대값으로 고정하지 않은 부분과, **REQ 문구가 현재 코드와 다른 곳(문구 낡음)** 이다.

| REQ | 표 상태(`§8`) | 실제 | 시험이 단언하지 않는 것 / 문구가 코드와 다른 곳 |
|---|---|---|---|
| 001 초기화 | Existing | **구현됨** | (유효 설정이 상태를 바꾼다: 플래그만) |
| 002 ABI | Existing | 부분 | `extern "C"`·`__cdecl` 열거 시험 0; `XpeImageBuffer` 40·`XpeImageMetadata` 96 만 `static_assert`(`xpe_types.h:128-142`), `XpeCalibrationMetrics`·`XpeBpmConfig` 등은 크기 단언 0; C# 인자 수 가드는 48개 중 5개 |
| 003 스레드 안전 | Existing | 부분 | 게인·검출·파이프라인 동시 실행 시험 없음; "전역 가변 상태 변경 없음" ↔ 처리 함수가 알림 큐에 쓴다(문구 거짓) |
| **004 오류 코드** | Existing | **단언 없음** | 반환값 집합 소속 단언 0; 범위가 `NETWORK_FAILED`(-10)에서 끝나나 -16·-17 도 반환; "모든 함수가 `XpeErrorCode` 반환"은 헤더에서 거짓(`const char*`·`void`·`bool`·`uint32_t` 반환 존재) |
| 005 입력 검증 | Existing | 부분 | 0 치수 시험은 일부 함수만; "출력 불변" 거의 없음; "모든 포인터" ↔ `NullMetadataIsAccepted`·`init(NULL)` 은 OK |
| 010 offset | **Pending** | 부분 | 성능·잔차 줄; 알고리즘 줄 `_mm256_subs_epu16`(낡음) |
| 011 gain | **Pending** | 부분 | 성능·flat-field 잔차; "UINT16 output no overflow" 줄(FLOAT32 와 모순) |
| 012 defect | **Pending** | 부분 | 성능 <21 ms 게이트·그래디언트 억제·>50% 규칙; 군집 중앙값 정확값; 부분 겹침 "쓰기 전 반환" |
| 013 runtime 검출 | **Pending** | 부분 | 시그니처·`hampel_threshold`(낡음); 줄무늬 TPR; 성능 1.3×; 0/1 도메인 |
| 014 offset 적재 | Implemented | 부분 | **세션 일치(구현 0)**; mutex |
| 015 gain 적재 | Implemented | 부분 | **kVp 보간 표(구현 0)**; mutex |
| 016 defect 적재 | Implemented | 부분(엄격) | mutex 하 커밋 |
| 016a 상태 적재 | (행 없음) | 부분 | "defect 맵만 calibState 에서"(코드는 `(void)calibState`, 문구 반대); 일부 파일만 있을 때 플래그 독립; "caller 구조체에 맵 없음" |
| 017 offset 생성 | Implemented | 부분(엄격) | winsor 등 공개 API 경유 단언 |
| 018 만료 검사 | Implemented | 부분 | **반환 코드·시그니처가 코드와 다름** |
| 019 저장 | Implemented | 부분 | 맵 없음 코드(`INVALID_INPUT` ↔ 문구 `CALIB_NOT_LOADED`), 시험은 `GTEST_SKIP`; gain·defect 만료 기록 |
| 020 미초기화 가드 | Existing | 부분 | "모든 처리 함수"(온도·고스트·binning 은 init 없이 OK 를 기대하는 시험이 있다); 출력 불변; 시험이 대부분 코드 반환만 |
| 020a 미적재 가드 | (행 없음) | 부분 | 출력 불변 |
| 021 치수 불일치 | Existing | **구현됨** | — |
| 022 형식 불일치 | Existing | 부분 | offset·gain 은 세 코드 중 아무거나 허용하는 시험, defect 만 정확 |
| 030 예외 불투과 | Existing | 부분 | `bad_alloc` 주입은 48개 중 약 17개 함수; 나머지는 정적 try/catch |
| 031 누수 없음 | Existing | 부분 | 보정 경로가 반복에 없음; common 의 1000회 시험은 `SUCCEED()` |
| 032 미초기화 출력 금지 | Existing | 부분 | NOT_INITIALIZED·CALIB_NOT_LOADED·NULL 경로의 출력 불변 |
| 033 NaN/Inf 금지 | Existing | 부분 | 커널에 `isfinite`·클램프 없음(적재·입력 거부로 보장); 문구 "clamp or replace" 는 코드와 다름; `GainCorrect_NoNaNInfInOutput` 은 호출이 실패하는 구성이라 **본문이 안 돈다(vacuous)** |
| 040 SIMD | **Pending** | 부분 | 결함 보간 AVX2 **없음**(문구에는 있음); "intrinsics 사용" 시험 없음 |
| 041 readout | **Pending** | 부분 | 선 잡음(구현 0, #232), 포화 패턴(행 평균만) |
| 042 파라미터 범위 | **Pending** | 부분 | **부위별(코드 0)**, 한계 값 단언 0 |
| 080 온도 보상 | (행 없음) | 부분 | UINT16 전용 거부·65535 클램프; 37 °C 기대값이 시험 안 공식 복사(자기 비교) |
| 081 온도 입력 가드 | (행 없음) | 부분 | NaN→25 °C 는 OK 만 확인; 이미지 불변 |
| 082 온도 플래그 | (행 없음) | **구현됨** | — |
| 085 고스트 생성 | (행 없음) | 부분 | 생성 OOM 시험 0 |
| 086 고스트 핸들 가드 | (행 없음) | 부분 | 파괴/쓰레기 비-NULL 시험 0; **"역참조하지 않고"는 `isValid` 가 `magic` 을 역참조하므로 거짓**; `Boundary.GhostUseAfterDestroyReturnsError` 는 이름과 달리 `reset(nullptr)` 만 시험 |
| 087 고스트 보정 | (행 없음) | 부분 | **문구 낡음(226)**: 미보정 핸들은 통과 |
| 088 고스트 reset | (행 없음) | 부분 | **문구 낡음(226b)**: `lastAcqTimeSec`(spec:706)는 삭제됨; 노출 상태 초기화 시험 없음 |
| 090 binning | (행 없음) | **구현됨** | — |
| 091 binning 가드 | (행 없음) | 부분 | **비유한 → 문구 `PROCESSING_FAILED` ↔ 코드·시험 `INVALID_INPUT`**; 음수 모드 |
| 095 단계 순서 | (행 없음) | 부분 | 8단계 중 인접 2쌍만 고정(offset→nonlin→gain); 순서 스파이 없음 |
| 096 단계 플래그 | (행 없음) | 부분 | **문구 낡음**: 미보정 고스트·binning 모드 1 은 플래그를 안 켠다 |
| 097 단계 바이패스 | (행 없음) | 부분 | 플래그 미설정 단언 8단계 중 3단계; "true" 외 값 |
| 098 고스트 핸들 의존 | (행 없음) | **구현됨** | — |
| 099 고스트 버퍼 격리 | (행 없음) | **구현됨(결과로)** | "own copy" 자체는 구조적 |
| 100 데이터 영역 전이 | (행 없음) | 부분 | 중간 영역 미관측; **게인 바이패스 시 변환이 게인 단계 밖에서 일어남(문구 낡음)**; 96구성 시험은 Windows 전용 |
| 101 defect 단계 가드 | (행 없음) | 부분 | "이후 단계도 안 돌림" |
| 102 캐시 offset | Implemented | 부분 | LRU 순서(용량 1 시험만), 기본 용량 4, 엔트리 삭제, 정확한 OOM 코드, 반환 버퍼 bits/dataSize 리터럴, 히트 시 timestamp·session 설치 |
| 103 캐시 gain | Implemented | 부분 | **문구 낡음**: "[0.1,10] 검사를 통과한 맵만 캐시" ↔ 코드는 범위 밖 화소를 결함으로 분류(5% 초과만 거부); 다항식 경로 약함 |
| 104 캐시 defect | Implemented | 부분 | 반환 버퍼 필드 리터럴 |
| 105 캐시 위 파이프라인 | Implemented | 부분 | "파일을 읽지 않음"·"calibState 안 읽음" 시험 없음; `meta` NULL |
| 106 버전 문자열 | Implemented | **구현됨** | — |

REQ-016a 와 105 는 **서로 모순**이다: 016a 는 `calibState` 를 defect 맵에 쓴다고, 105 는 읽지 않는다고 적는다. 코드는 105 쪽이다(`pipeline.cpp:612-615`).

## 5. 결함 후보 (고치지 않았다)

| # | 후보 | 근거 | 성격 |
|---|---|---|---|
| D1 | **세션 일치 미구현 + 헤더가 오류 코드를 약속** | §0.2; `preprocess_api.h:130,135`; `test_xpe_preprocess.cpp:493 LoadOffset_SessionMismatch` 는 **빌드에서 빠진 파일**(`CMakeLists.txt:360` 주석 처리)에 있고 단언은 `OK || CONFIG_INVALID`(항상 참) | 코드 + 문서 + 시험 |
| D2 | REQ-018 문구·시그니처가 코드와 다름 | `xpe_calib_check_expiry.cpp:27-29,62-81` | 문서(코드가 시험으로 고정됨) |
| D3 | REQ-019 맵 없음 코드 불일치, 해당 시험 `GTEST_SKIP`(`test_xpe_calib_save.cpp:158-168`), 헤더 주석은 `NOT_INITIALIZED` 도 적음 | `xpe_calib_save.cpp:44,64,84` | 문서·시험 |
| D4 | REQ-015 kVp 보간 표 구현 0, 헤더(`:142-143`)가 광고 | grep 0 | 코드 또는 문서 |
| D5 | REQ-086 `isValid` 가 파괴된 비-NULL 포인터를 역참조(미정의 동작), 문구는 "역참조하지 않고" | `xpe_preprocess_internal.h:89-92` | 설계 결정 필요 |
| D6 | REQ-091 문구 불일치(비유한 → `INVALID_INPUT`) | `binning_correct.cpp:44-50` | 문서 |
| D7 | spec 문구가 226 계열 뒤에 안 갱신됨: REQ-087(미보정 통과)·088(`lastAcqTimeSec`)·096(고스트 플래그 조건) | `ghost_correct.cpp:327`, `pipeline.cpp:425` | **제 누락**, 문서 |
| D8 | REQ-013 시그니처·`hampel_threshold`·§4.6 표의 "결함 보간 AVX2" | `preprocess_api.h:659`; 0건; `CMakeLists.txt:373-379` | 문서 |
| D9 | REQ-042 부위별 한계 없음; REQ-041 선 잡음 없음(#232 기존) | `preprocess.cpp:92`; `readout_validate.cpp:4` | 코드 또는 문서 |
| D10 | REQ-016a ↔ 105 모순; REQ-103 범위 검사 문구 | `pipeline.cpp:612-615`, `xpe_calib_load_gain.cpp:130-147` | 문서 |
| D11 | **본문이 안 도는/항상 참인 시험**: `GainCorrect_NoNaNInfInOutput`(보정 미적재라 호출이 실패해 `isfinite` 루프 미실행), `BP01`(출력 비교가 항상 참), `OffsetCorrect_FormatMismatch`·`GainCorrect_FormatMismatch`(세 코드 중 아무거나 허용), `Boundary.GhostUseAfterDestroyReturnsError`(이름과 내용 불일치) | 에이전트 판독, `test_xpe_preprocess_correction.cpp:240,269,336,352`, `test_boundary.cpp:96-105` | 시험 |
| D12 | 꺼진 시험이 요구의 유일한 단언인 곳: `DISABLED_PipelinePerformance3072x3072`(500 ms) | `test_integration.cpp:161` | 시험·성능 |
| D13 | `REQ-P1A-066` 이 정의 없이 시험·보고서에 남음 | `test_req_p1a_066.cpp`, `quality-report.md:147` | 문서 |
| D14 | `plan.md`: 파일 이름 24개 중 23개 부재, "14개 함수"(헤더 48), frontmatter 와 spec 의 상태 모순 | §2.2 | 문서 |

**새 이슈 필요**(카드의 `Refs #233` 은 닫혔다). D1·D4·D5 는 코드 결정이 있어 별도 항목, 나머지는 문서·시험 정리로 묶을 수 있다.

## 6. SRS-CALIB-FUNC-* 중 RTM 이 `✓` 인데 시험이 없는 것

### 6.1 census (이름 축 + 행위 축, 서로 대조)

파서: SRS 정의 38개(`| **SRS-CALIB-FUNC-NNN**` 행; 대조군 001 인식·999 미인식), RTM 정의 행 29개. SRS 에는 있고 RTM 정의 행에 없는 것 9개: `018 019 020 021 034 035 036 037 038`. RTM `§4` 는 "Functional Req 24 / 24 traced ✓"라 적는데 **정의 행은 29개, SRS 는 38개**라 24 가 무엇을 센 것인지 알 수 없다.

- **이름 축**: FUNC ID 를 인용하는 시험이 없는 것 17개 — `001 004 007 008 009 010 011 012 013 014 015 021 028 029 030 034 038`.
- **RTM 의 시험 열(`UT-1.5-001` 등)**: 어떤 시험 소스에도 **0건**. RTM `§3` 이 문서 안에서 정의한 "시험 사례"일 뿐이라 `✓`/"traced" 는 **문서의 사례 ID 까지** 추적한 것이다.
- **행위 축**: SRS 행이 이름 붙인 API(`xpe_*`)를 호출하는 시험이 있는지. 이름 붙인 API 가 있는 행 중 시험이 0인 것은 029 하나, API 이름이 없는 행 23개는 기계로 못 센다.

### 6.2 RTM 이 `✓` 인데 구현·시험이 없는 것 (직접 확인)

| FUNC | SRS 내용 | RTM | 코드 | 시험 |
|---|---|---|---|---|
| **011** | `xpe_calib_session_create()` 세션 관리(UUID v4) | 정의 행 + `✓` | **0**(src·include·tests 0, 대조군 `xpe_binning_correct` 존재) | 0 |
| **021** | Calibration Effect Score(CES) | `§5.1` "Updated"(`FUNC-015 / FUNC-021 … pipeline verification metric tests`) | **없음**(`CES` 는 `xpe_verify_metrics.cpp:881` 의 주석 한 줄) | CES 계산 시험 0 |
| **014** | 노출 이력 링 버퍼(≥8프레임, 최대 16) | 정의 행 + `✓` | 고스트는 화소별 누산기 둘이며 프레임 링 버퍼가 없다 | "8~16프레임 이력 길이" 단언 0 (리셋·첫 프레임 통과는 REQ-088·087 시험이 있다) |
| **015** | `xpe-pre-e2e-report-v1` 스키마 보고서 | 정의 행 + `✓` | 스키마 문자열이 코드(py·cs·cpp·h·ps1·json·yml)에 **0건**(문서 제외) | 0 |
| 028 | `xpe_calib_field_generate()` | 정의 행, `✓` 아님 | **0**(`§5b` 가 017 을 중복으로 폐기했으나 028 은 SRS 에 남음) | 0 |
| 029 | `xpe_calib_check_drift()` | 정의 행, `✓` 아님 | **0** | 0 |
| 030 | 실시간 오프셋 적응(`calibration.realtime_offset_adapt`) | 정의 행, `✓` 아님 | **0** | 0 |

011·021·014·015 는 `✓` 가 붙어 있고 구현이 없다. 028~030 은 `✓` 는 없으나 RTM `§4` 의 "100%"에 섞여 세어졌을 수 있다(24 가 무엇인지 모른다).

### 6.3 RTM `✓` 인데 구현·시험이 있는 것 (이름 census 가 틀리게 센 것)

ID 를 인용하지 않아 이름 축으로는 "시험 없음"이지만 행위로는 시험이 있다: 001·004(offset), 007(defect), 008(온도), 009(만료), 010(검출), 012(binning), 013(고스트 3티어), 034(offset 방법: `test_calib_generate_offset_config.cpp` 가 공개 API 로 단언). **이 항목들은 시험이 있고 RTM 이 ID 를 안 달았을 뿐**이다.

### 6.4 RTM 자체의 어긋남

RTM `§2` 는 FUNC-004~017 14행이 옛 SRS 판본을 추적한다고 스스로 적는다(`#195`). 그 상태에서 `§4` 의 "100% traced"와 `§5` 의 "✓ Designed"는 그대로다. 제 대조는 그 위에서 **SRS 의 현재 FUNC 번호**를 기준으로 했다. FUNC-024/025 의 오매핑(카드가 언급)은 RTM 행에서 "FUNC-024 = Multi-gain frame count, FUNC-025 = Grid artifact"로 SRS 와 **일치**하고(`§2` 131~132행), 시험은 `test_defect_gen.cpp`·`test_calib_generate_gain.cpp` 가 ID 를 인용한다. 오매핑으로 보인 것은 `§5.1` 의 `FUNC-016 / REQ-010` 처럼 FUNC 번호와 REQ 번호를 섞어 적은 줄(`FUNC-016` 은 SRS 에서 "dark/offset 지표"이고 `REQ-P1A-010` 은 offset 보정)일 가능성이 있으나 이번에 확정하지 않았다.

## 7. 증거 — 5절 형식

**주장**: §0 의 7개.

**증거**: `req_census_out.txt`(REQ 48개의 연결·인용 표), `func_census_out.txt`(FUNC 38개), `evidence_greps.txt`(세션 비교 0·대조군 8, `plan.md` 파일 이름, 정의 없는 REQ 인용, DISABLED 시험 목록), `evidence_verified_claims.txt`(에이전트 주장 중 제가 다시 확인한 것의 명령과 출력), 스크립트 3개(`*_script.txt`).

**baseline 귀속**: 위는 모두 이번 실행, 이 작업 트리(HEAD `f5b314ea`)에서 측정했다. REQ 문구별 시험 판정의 줄 번호는 에이전트가 같은 트리에서 읽은 것이다.

**미검증(Gaps)**:

- **REQ 문구별 판정 48개는 읽기 전용 에이전트 셋의 1차 수집**이다. 그 안의 개별 단언 줄 번호·시험 본문 설명 중 제가 다시 열어 본 것은 `evidence_verified_claims.txt` 에 있는 항목들(018·019·016a·103·014·091·088·086·042·041·096·013·010·004·020·005 등 핵심 주장)뿐이고, **나머지 개별 줄 번호는 에이전트 보고를 믿은 것**이다. 두 에이전트의 "구현됨"(016·017)은 제 엄격 규칙에 맞춰 부분으로 내렸다(그 절에 단언 안 된 문장이 있음).
- M3-1(XCal 파서)·M4-1~M4-4 의 시험 본문은 읽지 않았다(파일 존재와 이름만).
- M1-4·M4-6 은 확인하지 않았다. M4-9 는 "lane-pre 보고서에서 측정값을 못 찾음"이며 검색은 단어 `coverage` 하나였다.
- REQ-005 의 "48개 중 35개에 NULL 시험이 있다"는 에이전트의 휴리스틱(시험 본문에서 NULL 인자와 `INVALID_INPUT` 을 찾음)이고 증명이 아니다.
- RTM `§4` 의 "24" 가 무엇을 센 것인지, 그리고 SAFE·PERF 블록의 어긋남은 확인하지 않았다(`#203` 도 미검증으로 남김).
- FUNC 행위 축에서 "API 이름이 없는 행 23개"는 이름 추출이 안 되어 기계로 세지 못했다. 그 행들(004~010 등)은 시험이 있는지 REQ 쪽 판정으로 갈음했다.
- 6.4 의 FUNC-024/025 오매핑 가설은 확정하지 않았다.

**잔여 위험**:

- §4 의 판정은 "시험이 문구를 단언하는가"이지 "코드가 옳은가"가 아니다. 단언이 있어도 옳음을 보장하지 않고, 단언이 없어도 옳을 수 있다.
- 에이전트가 읽지 않은 시험 파일에 문구를 단언하는 시험이 더 있을 수 있어 "단언 없음" 일부는 과소 판정일 수 있다(대조군 grep 으로 완화했으나 모든 REQ 에 같은 강도는 아니다).
- 수정안(`.txt`)을 리더가 spec·plan·RTM 에 반영하면 이 보고서의 줄 번호 인용이 다시 밀린다.
