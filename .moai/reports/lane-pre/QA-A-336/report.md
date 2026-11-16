[2026-11-16 23:06 KST] [xpe-pre] QA-A-336 — 비선형 보정·다항식(다선량) 게인 기능 제거 (보정 모드 축소는 미수행, 아래 참조)

## 지운 것
- 파일(git rm): `src/nonlinearity_correct.cpp`, `src/xpe_calib_generate_nonlin_lut.cpp`, `src/xpe_calib_load_nonlin_lut.cpp`; 시험 `test_nonlin_lut_apply/_lut_generate/_poly_apply/_no_mode_rejection/_noop_report.cpp`, `test_gain_poly_classify/_dose_range/_policy.cpp`, `gain_poly_real_crop.h`, `gain_poly_policy_golden.inc`. CMake 목록 줄도 함께.
- 공개 API(`preprocess_api.h`): `xpe_nonlinearity_correct`, `xpe_calib_generate_nonlin_lut`, `xpe_calib_load_nonlin_lut`, `xpe_calib_unload_nonlin_lut`, `xpe_calib_generate_gain_polynomial` 선언과 문서. 파이프라인 설명(단계 순서, `panel.*` 키, 숫자 키 목록), 저장·캐시 로더의 다항식 문구 정리.
- XCal 형식: `XCAL_TYPE_GAIN_POLY(3)`·`XCAL_TYPE_NONLIN_LUT(4)` 열거값 삭제(번호 재사용 금지를 주석에 명시). 검사기·리더(압축 경로 포함)의 유형 상한을 `XCAL_TYPE_DEFECT(2)`로 낮춤. 다항식 페이로드 길이·형식 규칙 삭제.
- 내부 상태: `g_calib` 의 `nonlin_lut*`·`gain_poly_*`, `CalibSnapshot`·`StagedGain` 의 다항식 필드, `xpe_gain_poly_eval_f32`, `xpe_nonlinearity_apply(_doc)`.
- 적용기(`gain_correct.cpp`): 다항식 분기 전부(선량 범위 클램프와 그 경고, 프레임별 결함 분류와 경고). 스칼라 경로 코드는 그대로.
- 로더·저장·캐시·만료·세션 검사의 다항식 분기 삭제(`xpe_calib_load_gain.cpp`, `xpe_calib_save.cpp`, `calibration_cache.cpp`, `calib_expiry_check.cpp`, `xpe_calibration.cpp`).
- 생성기(`xpe_calib_generate_gain.cpp`): `xpe_calib_generate_gain_polynomial` 과 적합 보조 함수 삭제(953→293줄). 단일 선량 `xpe_calib_generate_gain` 은 무변경.
- 파이프라인(`pipeline.cpp`): 3단계(비선형) 호출, `bypassNonlinearity` 파싱, `XPE_FLAG_NONLINEARITY_CORRECTED` 설정, 다항식 프레임 결함 목록 전달 삭제. 남은 3단계 자리는 이전 `"bypassNonlinearity":true` 경로와 같은 코드(복사 없음).
- 도구(`xpe_calib_fixture_gen.cpp`): `--gain-poly`/`--poly-degree`/`--poly-levels` 와 다항식 생성 블록. 이제 "unknown argument" 로 거부되고 파일을 쓰지 않는다.

## 다항식 gain.xcal 의 명시적 거부
- 형식 3 파일을 `xpe_calib_load_gain`(캐시 로더 포함)에 주면 `XPE_ERR_CONFIG_INVALID` 와 알림 `XPE_ERR_XCAL_TYPE: calibration file type 3 (gain polynomial, removed in QA-A-336) is not supported: ...`(XPE_ALERT_ERROR). 형식 4 는 "(nonlinearity table, ...)". 스칼라로 읽지 않고 저장소는 그대로.
- 시험: `GainPolyLoadTest.APolynomialGainFileIsRefusedWithAnExplicitErrorAndNothingIsLoaded` (1면·3면·선량 범위 포함 파일; 빈 저장소에서 거부 후 `XPE_ERR_CALIB_NOT_LOADED`; 스칼라 2.0 적재 상태에서 거부 후에도 출력 500; 캐시 로더도 거부; 같은 작성기로 만든 스칼라 파일은 적재되는 대조군). 보조: `XCalValidatorTest.GainPolyIsRefusedWhateverItsPayloadAndTheAlertNamesIt`, `CacheSameVerdict.AGainPolynomialFileIsRefusedByTheCachedLoaderAsByThePlainOne`, `PreE2ERun.APolynomialGainFileFailsTheRunInsteadOfBeingMeasured`, `FixtureGenGainPolyTest.TheGainPolyOptionIsRefusedAndNothingIsWritten`.

## 판단과 근거
- **보정 모드(XpeCalibrationMode)의 다점 모드 삭제는 하지 않았다.** 단일 선량 생성기가 gain.xcal 설정 블록에 활성 모드(`"calibration_mode"`, 기본 MULTI_POINT_8 = 3)를 기록한다. 다점 모드를 지우면 기본이 SINGLE_POINT(0)가 되어 gain.xcal 바이트가 바뀐다 — "단일 선량 스칼라 경로 바이트 동일" 요구와 충돌. enum·`xpe_calib_set_mode`·`get_max_points`·`get_poly_degree` 는 그대로 두고 헤더 문서에서 다항식 생성기 언급만 고쳤다. 리더 결정 필요(아래).
- **XPE_STAGE_HOOK(3)** 은 같은 자리(오프셋→게인 사이)에서 계속 부른다. 시험이 관찰하는 훅 번호는 모두 그대로.
- **공용 플래그 `XPE_FLAG_NONLINEARITY_CORRECTED`** 는 정의를 남기고 "더 이상 켜지지 않음, 비트 예약" 주석만 덧붙였다. 시험은 꺼져 있음을 계속 단언.
- **설정 키 `bypassNonlinearity`, `panel.*`** 는 모르는 키로 무시된다(QA-A-335 의 bypassTemp 와 같은 처리). 시험·도구의 설정 문자열은 그대로.
- **`xpe_gain_correct_in` 의 `frame_defects` 매개변수와 결함 단계의 프레임 목록 인자는 남겼다.** 채우던 쪽(다항식)만 지웠고 파이프라인은 nullptr 를 넘긴다. 결함 단계에서 nullptr 와 빈 목록은 같은 경로(`frame_defects ? size : 0`).
- **BPM 결함 검출의 선형성(DOSE_LINEAR, LOF, `XPE_BPM_LINE_CAUSE_LINEARITY`, 주입 결함 "nonlinear")** 은 비선형 보정과 무관하여 무변경(`xpe_bpm_v2.cpp`, `tools/xpe_bpm_eval.cpp`, `test_bpm_*`).
- **E2E 도구 점수 항목 "nonlinearity"** 는 평가 프로토콜 §6 측정 스키마라 남겼다. 비스칼라 분기의 문구만 "non-scalar gain" 으로 고침.
- **줄어든 시험 범위**: 생성기로 FUNC-033 R2 게이트·R2 이력·잔차 필드를 보던 `test_calib_quality_meta.cpp` 사례, 모드 표의 N≥3 행(`test_calib_mode_enforcement.cpp`)은 다항식 생성기로만 닿던 것이라 함께 지웠다. AUTO 알림 사례는 단일 선량(모드 0)으로 옮김.
- `test_gain_poly_not_applied.cpp`·`test_calib_gain_poly_load.cpp`·`test_fixture_gen_gain_poly.cpp` 는 이름을 유지(헤더가 `GainPolyNotAppliedTest.KvpAndSidDoNotChangeTheCorrection` 을 인용). 남은 사례는 스칼라·거부 시험.

## 빌드
`cmake --build build/ci-preprocess` → `build_rc=0` (`evidence/08_build_final.txt`, /WX).

## 전체 시험 (필터 없음, `ctest --output-on-failure -j 1`)
`100% tests passed, 0 tests failed out of 1493`, Total 304.35 sec, ctest_rc=0 (`evidence/10_ctest_full_serial.txt`). 코드 변경 직후 1차 실행도 같은 결과(1493/1493). QA-A-335 의 1630 에서 지운 사례만큼 줄었다.

## 출력 sha (cyan_test Bright_29, 깨끗한 보정 세트, 기본 설정, QA-A-335 의 `02_out_sha.py.txt`)
- 제거 전(HEAD a72d59c9 빌드): `c5f458c5d2044f70f182f692f53f9357b389c7bf540b887bd4082cb01a67cfd3`, flags 0x201c (`evidence/03_sha_before.txt`)
- 제거 후(새 fixture_gen + 새 DLL): `c5f458c5d2044f70f182f692f53f9357b389c7bf540b887bd4082cb01a67cfd3`, flags 0x201c (`evidence/06_sha_after.txt`)
- 보정 세트 파일 sha256 전후 동일(diff 0): offset `5569bacf…`, gain `a7ddadbd…`, defect `ba947dcd…`, manifest `a0fdc516…` (`04_cset_hash_before.txt` 대 `07_cset_hash_after.txt`).

## OutputUnchanged237b
기록 해시 재기록 없음. 비선형 플래그는 LUT 가 없으면 원래 켜지지 않았고, 3단계 복사는 화소값을 바꾸지 않아 남은 행 모두 그대로 통과. 다항식 경로 행만 지움: `pipeline/poly_gain/tier2`(두 표), `gain/poly/output`, `gain/poly/classified_list`, `GainWrittenOverItsOwnInputEqualsSeparateBuffers` 의 다항식 반복.

## grep (build·.moai 제외)
`grep -rniE 'nonlin|polynomial|gain_poly|dose_levels|FUNC-006|FUNC-027|CALIB_MULTI' modules/preprocess modules/common tools` → `evidence/09_grep_after.txt`(397줄, 줄마다 사유). 분류: `bypassNonlinearity` 설정 문자열(무시되는 키), BPM 선형성·주입 결함 "nonlinear", FUNC-033 품질 필드 `polynomial_degree`, 보정 모드 API(미수행 항목), 제거 이력 주석, NLCSC(고스트), 예약 플래그, 파일·시험 이름, E2E 점수 항목, 무관 도구(mcsim·packaging 바이너리). 자동 분류 밖 26줄은 직접 확인: 파일 이름 `test_temp_nonlinearity_binning.cpp`, 일반 FMA 설명(`gain_correct.cpp:175,185`), `pipeline/combo/nonlin_off` 해시 키, 단계값 시험의 조합·사례 이름, 거부 시험 자체 — 기능 코드 아님.

## 범위 밖 — 다른 소유자 후속 필요
- GUI: `gui/ImageProcTest/Services/Native/XpePreprocessInterop.cs` 가 지운 내보내기를 P/Invoke 로 선언(호출 시 EntryPointNotFound).
- clients: `NonlinearityBypassedScenarios.cs`, `NonlinearityLutCallSiteTests.cs`, `NonlinearityStageWiringTests.cs`.
- 리더 소유: `tools/ci/clang_tidy_baseline_preprocess.txt`, `tools/docs/api_requirement_census.py`, docs/ 의 SRS·API 문서.

## 리더 결정 필요
보정 모드 축소(SINGLE_POINT 만 남기기)를 하면 gain.xcal 설정 블록의 `calibration_mode` 값이 3→0 으로 바뀐다(화소 페이로드·출력 영상은 그대로). 진행 여부.

## 커밋에서 뺀 것
작업 전부터 수정돼 있던 `modules/preprocess/tests/test_ghost_oracle.cpp`(스테이징 안 함). 같은 상태였던 비선형 시험 3개는 파일째 삭제(이전 변경분은 `evidence/00_preexisting_uncommitted_nonlin_tests.patch.txt` 에 보존).

## 임시 폴더
`D:/workspace-github/xpe-data/qa-336-tmp` 삭제.
