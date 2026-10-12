# QA-A-246c — 크기 부족·겹침 순서를 고정 배치로 재현하는 회귀 시험 (Codex #169 발견 1)

## 1. 결과

| 항목 | 결과 |
|---|---|
| 새 시험 2 건 | 입력과 출력을 **한 arena** 안의 정해진 위치에 놓아 힙 배치에 기대지 않는다 |
| 반증 | 246b 의 순서를 되돌리면 첫 시험이 **반복 1 회로 결정적으로** 빨강. 겹침 검사를 지우면 둘째가 빨강 |
| 결정성 | 같은 시험을 포함한 `A241Safety.*:A246*` 를 200 회 반복해 실패 0 |
| 기존 시험 | `PipelineOutRefusesWhatCannotHoldTheResult…` 는 그대로 둠 |

시험만 추가했다. 제품 코드 변경은 없다.

## 2. 시험 (`tests/test_a241_safety.cpp`, `A241Safety`)

두 시험 모두 `xpe_preprocess_pipeline_out` 에 float32 결과를 만드는 설정(`kCfg`)을 주고, arena 의 모든 바이트가 호출 뒤에도 같음을 비교한다 (입력 영역과 출력 영역 전부). 필요한 결과 크기 `needBytes = kN × 4`, 입력 `inBytes = kN × 2`.

1. `ATooSmallOutputJustBeforeTheInputWhoseRequiredRangeReachesTheInputIsBufferTooSmall`
   - arena `[출력 방 (inBytes) | 입력 (inBytes) | 여분]`, 출력은 arena 의 맨 앞에 `inBytes` 로 선언(필요한 `needBytes` 보다 작다), 입력은 그 바로 뒤.
   - 필요한 결과 범위 `[0, needBytes)` 는 입력 `[inBytes, 2·inBytes)` 를 지나간다. 크기와 겹침이 **둘 다** 해당되는 배치다.
   - 기대: `XPE_ERR_BUFFER_TOO_SMALL`, arena 불변, `meta.flags == 0`.
2. `AnOutputBigEnoughForTheResultThatOverlapsTheInputIsInvalidInput`
   - 같은 arena, 출력은 맨 앞에 `needBytes` 로 선언(충분), 입력은 그 안 `[inBytes, 2·inBytes)`.
   - 기대: `XPE_ERR_INVALID_INPUT`, arena 불변, `meta.flags == 0`. (크기는 충분하고 결과가 입력을 덮어쓸 배치.)

## 3. 반증 (`evidence/43_mutation_log.txt`, 한 스크립트의 연속 출력; 변형마다 DLL SHA-256, 원복 후 `touch` 재빌드, 소스 해시 `77519b963d13bdb1` 가 처음과 같음)

| 단계 | 소스 | 결과 |
|---|---|---|
| 0 | 지금 main 의 순서 (크기 → 겹침) | 네 시험(새 2 + 기존 2) 통과 |
| 1 | **변형 A**: 246b 이전 순서(겹침 → 크기) | **반복 1 회**에 첫 시험 빨강 (`INVALID_INPUT` 이 나옴), 나머지는 통과. 2회째 실행 필요 없음 |
| 2 | **변형 B**: 겹침 검사 삭제 | 둘째 시험 빨강 (`INVALID_INPUT` 대신 호출이 진행). 네 시험을 한꺼번에 돌리면 기존 겹침 시험이 겹친 메모리에 쓰다가 프로세스가 비정상 종료(종료 코드 127)하고, 둘째 시험만 따로 돌리면 정상적으로 실패 보고 (`evidence/44_mutant_b_run.txt`) |
| 3 | 원복 + `touch` + 재빌드 | 네 시험 통과 |

## 4. 검증

| 항목 | 결과 | 증거 |
|---|---|---|
| 반복 200 회 (`A241Safety.*:A246*`) | 200 회 모두 통과, 실패 0 | `10_repeat200_summary.txt` |
| `xpe_preprocess_tests` | 1044 통과 | `30_xpe_preprocess_tests.txt` |
| `xpe_preprocess_clock_tests` | 50 통과 | `30_xpe_preprocess_clock_tests.txt` |
| `xpe_preprocess_oom_tests` | 88 통과 | `30_xpe_preprocess_oom_tests.txt` |
| ctest 순차 | 100 %, 1286 개 중 실패 0 | `31_ctest.txt` |
| clang-tidy 기준선 게이트 | `GATE_EXIT=0` | `41_clang_tidy_gate.txt` |

(doxygen: 헤더·제품 코드 변경이 없어 doxygen 은 돌리지 않았다. cppcheck 는 이 기계에 없다.)

## 5. 변경 파일

시험: `modules/preprocess/tests/test_a241_safety.cpp`. 증거: `.moai/reports/lane-pre/QA-A-246c/`.

## Card Cross-Check

| milestone | card |
|---|---|
| 크기 부족·겹침 순서의 고정 배치 회귀 시험 | QA-A-246c (이 보고서) |
