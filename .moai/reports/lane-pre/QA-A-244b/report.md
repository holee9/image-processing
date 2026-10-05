# QA-A-244b — Codex #164 낮음 2건 정리

## 1. 결과

| 항목 | 결과 |
|---|---|
| `DISABLED_B4_SafeBehaviour` 의 6 초 벽시계 창 | 고쳤다. 고정 8 초 대기를 "만료 시각 + 0.5 초까지 대기" 로 바꾸고, 첫 프레임이 만료 전에 돌았는지를 측정해 출력한다 |
| 훅 매크로가 `oom_tests` 에도 정의된다는 사실을 주석에 반영 | 헤더와 CMake 주석을 고쳤다 |

제품 동작 변경은 없다 (주석과 측정 하니스만).

## 2. B4 하니스

이 시험(`A240.DISABLED_B4_SafeBehaviour`)은 `DISABLED_` 측정 하니스이고 CI 의 게이트가 아니다. DLL 에 링크된 실행 파일(`xpe_preprocess_tests`)에 있어서 244 의 주입 시계(`XPE_CACHE_TEST_HOOKS` 빌드 전용)를 쓸 수 없다. 그래서 주입 대신 두 가지를 바꿨다.

- 대기는 고정 8 초가 아니라 **실제 만료 시각 + 0.5 초까지**(`sleep_until`)다. 두 번째 프레임은 항상 만료 뒤에 돈다.
- 첫 프레임이 **만료 전에 돌았는지** 시작·종료 시각으로 측정해 출력한다. 느린 기계가 6 초 창을 넘겨서 "적재 중에 만료" 라는 관측이 성립하지 않으면 출력이 `AFTER` 로 말해 준다. 이전에는 가정이었다.

이 하니스는 게이트가 아니라 관측이고, 판정(통과/실패)은 보고서에 값을 인용할 때 이 출력을 보고 한다. 실행 결과 (`evidence/10_b4_run.txt`): 첫 프레임은 만료 5859 ms 전에 시작해 5792 ms 전에 끝났다(`BEFORE`), 만료 뒤 프레임은 `-5 XPE_ERR_CALIBRATION_EXPIRED`, 호출자 버퍼 불변.

## 3. 훅 매크로

`XPE_CACHE_TEST_HOOKS` 는 `xpe_preprocess_clock_tests` 와 `xpe_preprocess_oom_tests` 둘 다에서 정의되고 (둘 다 제품 소스를 직접 컴파일해 넣는 시험 실행 파일), `oom_tests` 는 주입 시계를 `AHitJudgesTheExpiryAfterTheOpenCheckNotBefore` 에서 쓴다. `xpe_preprocess_internal.h` 의 훅 설명과 `CMakeLists.txt` 의 두 주석이 "clock-test 대상만" 이라고 읽혔던 것을 고쳤다. 출하 라이브러리에는 매크로도 포인터도 없다는 설명은 그대로다. 244 보고서 §2 의 같은 문장도 이 보고서가 바로잡는다 ("clock-test 빌드에서만" → "clock-test 와 oom-test 두 실행 파일에서만").

## 4. 검증

| 항목 | 결과 | 증거 |
|---|---|---|
| `xpe_preprocess_tests` | 1037 통과 | `30_suite.txt` |
| `xpe_preprocess_clock_tests` | 40 통과 | (같은 실행) |
| clang-tidy 기준선 게이트 | `GATE_EXIT=0` | `41_clang_tidy_gate.txt` |
| doxygen | 종료 0, 경고 0 | `42_doxygen.txt` |
| B4 하니스 재실행 | 위 §2 | `10_b4_run.txt` |

cppcheck 는 이 기계에 없다. 순차 ctest 는 돌리지 않았다 (시험 목록 변경 없음).

## 5. 변경 파일

`include/xpe/preprocess/xpe_preprocess_internal.h` (주석), `CMakeLists.txt` (주석), `tests/test_zz_a240_checklist.cpp` (B4 하니스). 증거: `.moai/reports/lane-pre/QA-A-244b/`.

## Card Cross-Check

| milestone | card |
|---|---|
| Codex #164 낮음 2건 | QA-A-244b (이 보고서) |
