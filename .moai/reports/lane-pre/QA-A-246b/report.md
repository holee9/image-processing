# QA-A-246b — `pipeline_out` 의 출력 버퍼 검사 순서: 크기 부족을 겹침보다 먼저

## 1. 결과

| 항목 | 결과 |
|---|---|
| 증상 | `A241Safety.PipelineOutRefusesWhatCannotHoldTheResultAndWritesNothing` 이 간헐 실패: `BUFFER_TOO_SMALL(-8)` 을 기대했는데 `INVALID_INPUT(-1)` (QA-A-246 작업 중 60 회 반복에서 5 회, `QA-A-246/evidence/50_flaky_pipelineout_toosmall_before_fix.txt`) |
| 원인 | `pipeline_core` 가 겹침 검사를 크기 부족 검사보다 먼저 하고, 겹침 범위를 호출자가 준 `out->dataSize` 가 아니라 **필요한 결과 크기**로 잡는다. 크기가 모자란 출력 버퍼가 입력 버퍼 바로 앞에 할당되면(힙 배치) 필요한 float 범위가 입력과 겹쳐 INVALID_INPUT 이 먼저 나온다 |
| 수정 | 두 줄의 순서를 바꿨다: 크기 부족 → `BUFFER_TOO_SMALL`, 그다음 겹침 → `INVALID_INPUT` (`src/pipeline.cpp`, 이유를 코드 주석에) |
| 효과 | 같은 시험 묶음(`A241Safety.*:A246*`)을 200 회 반복해 실패 0 (`evidence/10_repeat200_summary.txt`) |

## 2. 범위와 의미

- 이 카드(QA-A-246)의 일은 아니다. 246 의 게이트를 돌리다 나온 간헐 실패여서 별도 커밋으로 분리했다. 246 의 변경은 이 검사 앞에 아무것도 넣지 않았다 (실패 원인은 241, Codex #157 의 검사 순서다).
- 바뀌는 동작: **크기도 모자라고 필요한 범위가 입력과 겹치는** 출력 버퍼의 오류 코드가 이제 항상 `BUFFER_TOO_SMALL` 이다. 이전에는 배치에 따라 둘 중 하나였다. 크기가 충분한 출력 버퍼가 입력과 겹치면 여전히 `INVALID_INPUT` 이다 (`PipelineOutRefusesAnyOverlapOfInputAndOutputAndLeavesBothAsTheyWere` 등 기존 겹침 시험이 그대로 통과). 어느 경우에도 출력·입력은 쓰이지 않는다.
- 반증: 이 순서 변경에 대한 새 시험은 만들지 않았다. 기존 시험이 결정적이 된 것이 근거이고, 되돌리면 시험이 다시 간헐 실패한다 (되돌린 빌드로 재현하는 것은 하지 않았다: 위 60 회 중 5 회가 되돌린 상태의 측정이다).

## 3. 검증

| 항목 | 결과 | 증거 |
|---|---|---|
| 반복 200 회 (`A241Safety.*:A246*`) | 실패 0, 200 회 모두 통과 | `10_repeat200_summary.txt` |
| `xpe_preprocess_tests` | 1042 통과 | `30_xpe_preprocess_tests.txt` |
| `xpe_preprocess_clock_tests` | 48 통과 | `30_xpe_preprocess_clock_tests.txt` |
| `xpe_preprocess_oom_tests` | 88 통과 | `30_xpe_preprocess_oom_tests.txt` |
| clang-tidy 기준선 게이트 | `GATE_EXIT=0` | `41_clang_tidy_gate.txt` |

doxygen·ctest 는 헤더·시험 목록 변경이 없어 돌리지 않았다. cppcheck 는 이 기계에 없다.

## 4. 변경 파일

`modules/preprocess/src/pipeline.cpp` (두 줄 순서와 주석). 증거: `.moai/reports/lane-pre/QA-A-246b/`.

## Card Cross-Check

| milestone | card |
|---|---|
| QA-A-246 중 발견된 간헐 실패 | QA-A-246b (이 보고서, 별도 커밋) |
