[2026-11-16 22:25 KST] [xpe-pre] QA-A-335 — 온도 보정 기능 제거 (완료)

## 지운 것
- 파일: `modules/preprocess/src/temp_compensate.cpp` (git rm), CMake 목록 줄(`modules/preprocess/CMakeLists.txt` 의 `src/temp_compensate.cpp`).
- 공개 API: `xpe_temp_compensate` 선언과 문서 주석(`preprocess_api.h` Phase 7 머리말 수정), 설정 키 설명에서 `detectorTempC` 3곳.
- 파이프라인(`pipeline.cpp`): 설정 필드 `detectorTempC`·`bypassTemp`, 그 파싱, 온도 단계 호출과 `XPE_FLAG_TEMP_COMPENSATED` 설정, 단계 설명 주석 2곳. 남은 단계 1 은 이전 `"bypassTemp":true` 경로와 같은 코드(입력 그대로, 복사 없음)이고 `XPE_STAGE_HOOK(1)` 번호는 유지.
- 설정 키 `detectorTempC`·`bypassTemp` 는 이제 다른 모르는 키와 같이 무시된다(파서에 모르는 키 거부 규칙 없음 — `pipeline.cpp` 에 unknown 키 처리 문자열 0건).
- 시험: `test_temp_nonlinearity_binning.cpp`(TempCompensate 5개), `test_golden_reference.cpp`(GoldenTempTest 4개), `test_preprocess_degraded.cpp`(BP05_TempCompensateReferenceIsIdentity), `test_integration.cpp`·`test_xpe_preprocess_memleak.cpp`·`test_pipeline_performance_3072.cpp`(직접 호출·선언), `test_oom_injection.cpp`(설정 문자열의 `detectorTempC` 4곳), `test_pipeline_stages.cpp`(온도 플래그 기대 → 켜지지 않음 기대 2곳), `test_config_strict_parse.cpp`(아래).

## 판단과 그 근거
- **공통 헤더 `XPE_FLAG_TEMP_COMPENSATED`(`xpe_types.h:161`)는 남겼다.** 카드 grep 대상이 아니고 공개 ABI 비트다. 이제 켜지는 곳이 없고, 시험이 "켜지지 않음"을 단언한다.
- **도구 설정의 `"bypassTemp":true`(`xpe_bpm_eval.cpp:2343-2344`, `xpe_pre_e2e.cpp:308`)는 그대로 뒀다.** 모르는 키로 무시되며, 이전에도 온도 단계를 건너뛰던 값이라 동작이 같다. 시험들의 같은 문자열도 마찬가지.
- **`test_config_strict_parse.cpp` 의 실수 표기 표가 파이프라인에서 대상 키를 잃었다.** 파이프라인 설정에서 실수 값을 받던 키는 `detectorTempC` 하나뿐이었다(`pipeline.cpp` 의 `parse_float` 0건 — 남은 숫자 키는 정수). `runTable` 은 실수 키가 nullptr 이면 실수 행을 건너뛰고, 정수 행(`binningMode`)은 그대로 돈다. 즉 파이프라인 진입점의 "nan/inf/1e999 실수 거부" 시험 범위가 줄었다. 나쁜 숫자·좋은 숫자 목록과 설정 거부/적재 대조는 `binningMode` 로 바꿨다.
- **`OutputUnchanged237b` 기록 해시 50행 재기록.** 이 시험은 화소와 메타데이터 플래그를 함께 해시한다(`test_output_unchanged_237b.cpp:292`, `:388`). 온도 플래그가 없어져 25개 사례(일반·레거시 모드 각각)가 어긋났다. 해시에서 온도 비트만 빼는 수정(`evidence/09_temp_mask_probe.patch.txt`)을 넣고 **제거 전 소스와 제거 후 소스로 각각** 돌린 계산값이 두 모드 모두 똑같았다(`12_old_*.lines` 대 `12_new_*.lines`, diff 0줄, 각 75줄; 원문 `10_new.*.txt`·`11_old.*.txt`). 그래서 마스크를 정식으로 두고, 그 값으로 기록을 바꿨다(`13_update_golden.py.txt` → `14_update_golden_out.txt`: legacy 25, firstBypass 22, golden 과 같아 바꿀 것 없는 3).
- **중간에 내가 만든 결함 하나를 고쳤다.** 처음엔 단계 1 에서 입력을 항상 uint16 복사로 바꿨는데, float 입력에서 크기가 맞지 않아 `PipelineStageValueTest.GhostFlagDistinguishesFirstFrameAndResetFromAnActiveFrame` 가 rc -3 으로 실패했다(`08_ctest_failed_serial.txt`). 이전 `"bypassTemp":true` 경로와 같게(복사 없음) 되돌려 해결.

## grep 0건
명령: `grep -rn 'temp_compensate\|detectorTempC\|TempCompensat' modules cmake CMakeLists.txt tools clients gui` (빌드 폴더 제외) → `evidence/04_grep_after.txt` 1줄:
`clients/ImageProcTest/Services/AlgorithmValidationCatalogService.cs:27` (`TempCompensator` 문자열) — 카드상 GUI 레인 담당. 모듈·도구·CMake 0건.

## 전체 시험 (필터 없음)
- 빌드: `cmake --build build/ci-preprocess` build_rc=0 (`05_build_all.txt`).
- `ctest -j 4`(병렬): 1차 30개, 2차 24개 실패 — 실패한 것만 `-j 1` 로 다시 돌리면 2차 24개 모두 통과(`16_ctest_failed_serial_run2.txt`). 병렬 실행에서 시험끼리 부딪친 것으로 보이나 원인은 확인하지 않았다(미확인).
- **`ctest -j 1` 전체: `100% tests passed, 0 tests failed out of 1630`, Total 304.63 sec, ctest_rc=0** (`17_ctest_full_serial.txt`).

## 출력 sha (cyan_test Bright_29, 깨끗한 보정 세트, 기본 설정, `02_out_sha.py.txt`)
- 제거 전(HEAD ea8d1f41 DLL): `c5f458c5d2044f70f182f692f53f9357b389c7bf540b887bd4082cb01a67cfd3`, flags 0x203c (`03_sha_before.txt`)
- 제거 후: `c5f458c5d2044f70f182f692f53f9357b389c7bf540b887bd4082cb01a67cfd3`, flags 0x201c (`18_sha_after.txt`)
- 영상 sha 동일. flags 차이 0x20 = `XPE_FLAG_TEMP_COMPENSATED`.

## 커밋에서 뺀 것
작업 전부터 수정돼 있던 `modules/preprocess/tests/test_ghost_oracle.cpp`, `test_nonlin_lut_apply.cpp`, `test_nonlin_lut_generate.cpp`, `test_nonlin_poly_apply.cpp` 는 이 카드와 무관해 스테이징하지 않았다.

## 임시 폴더
`D:/workspace-github/xpe-data/qa-335-tmp` 삭제.

## 기록 정정 [2026-11-16 22:34 KST] (Reviewer 수용 후 리더 요청, 커밋 재작성 없음)
- F335-1: 위의 "기록 해시 50행 재기록"은 "검토 50사례, 실제 상수 47행 변경(legacy 25 + firstBypass 22), 기존 golden 값 재사용 3"으로 고친다 (`evidence/14_update_golden_out.txt` 의 분류: legacy 25, normal/firstBypass 22, normal/golden(same as legacy) 3).
- F335-2: 전체 시험 상태를 둘로 나눈다.
  - serial `ctest -j 1`: 1630 PASS (`17_ctest_full_serial.txt`).
  - parallel `ctest -j 4`: FAIL — 1차 30개, 2차 24개 (`06_ctest_full.txt` 는 2차 실행 결과로 덮였다; 1차 목록은 `07_failed_ids.txt`). 원인 미확인. 변경 전(ea8d1f41)에서도 재현되는지 미확인 — QA-A-336 뒤 기준선 측정 예정.

## 병렬 -j4 기준선 (변경 전 ea8d1f41) [2026-11-16 23:10 KST]
- 방법: 저장소 밖 임시 작업 폴더 `D:/workspace-github/xpe-data/qa-base-ea8d`(git worktree --detach ea8d1f41)에서 `cmake --preset ci-preprocess` → 빌드 build_rc=0 → `ctest --output-on-failure -j 4` (`evidence/20_base_configure.txt`, `21_base_build.txt`, `22_base_ctest_j4.txt`). 이번 새 빌드 폴더에서는 rules.ninja 가 남았다(RULES_NINJA_MISSING 0건).
- 결과: `88% tests passed, 199 tests failed out of 1640`, ctest_rc=8. 묶음별 상위: clock 24, SaturationTest 21, OomInjection 15, OomPipeline 11, ConfigStrictParse 11, XcalReplaceRetryTest 10.
- 판정: 병렬 실행 실패는 변경 전에도 있다. 다만 이 측정은 QA-A-336 후속 빌드가 같은 기계에서 동시에 돌던 중이라 부하 조건이 QA-A-335 측정(실패 24·30)과 같지 않다 — 실패 수의 비교는 하지 않는다. 원인 미확인.
- 임시 작업 폴더는 측정 뒤 제거(`git worktree remove`).
