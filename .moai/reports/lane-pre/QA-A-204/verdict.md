# QA-A-204 (1/2) — #233 3·4순위: 알림 큐 추가, 초기화·설정·로그 파일의 부분 커밋과 오류 코드

이 문서는 QA-A-204 의 첫 번째 커밋(③④, `modules/common`)을 다룬다. ⑤(엄격 변환)와 `verify_*`·`bpm`·`runtime`·`nonlin_lut` 의 OOM 가드는 QA-A-202b(Codex #23 보류, 리더 지시로 선행) 뒤에 이어서 한다. 기준 커밋 `d74588ad`. 증거는 `evidence/` (번호 순).

## 1. 주장

| # | 주장 | 근거 |
|---|---|---|
| ③ | `xpe_alert_push` 는 예외를 밖으로 내보내지 않고, 알림이 큐에 들어가지 못하면 정확히 한 번 드롭 수에 센다. 곧 모든 알림은 "큐 안" 아니면 "센 손실" 이다(수지가 맞는다). 손실 알림의 문구는 다음 알림에서 정확한 수를 따라잡는다. | 빨강 `02_red_run.txt`(예외 탈출), 초록 `04_green_run.txt`, 반증 `arm_dropuncounted_*` |
| ④a | `xpe_init` 이 오류를 돌려주면 라이브러리는 호출 전 그대로다(초기화 안 됨 / 재초기화라면 큐의 알림·초기화 상태 유지). 성공했는데 로그 한 줄을 못 써서 오류가 되는 일도 없다. | 빨강("returned an error but the library is initialized", "queued alerts were cleared"), 반증 `arm_initlate_*` |
| ④b | `xpe_configure` 의 할당 실패는 `XPE_ERR_OUT_OF_MEMORY` 이지 `XPE_ERR_CONFIG_INVALID` 가 아니다. `xpe_log_set_file` 의 할당 실패는 `XPE_ERR_OUT_OF_MEMORY` 이지 `XPE_ERR_IO_FAILED` 가 아니다. 진짜 오류(깨진 JSON, 없는 디렉터리)는 전과 같은 코드. | 빨강(rc -4, rc -9), 반증 `arm_configclass_*`, `arm_logclass_*`. 통제는 시험 안에 있다 |

## 2. 무엇을 바꿨나

- `xpe_common.cpp`
  - `enqueue_alert` 는 `noexcept`. 알림 객체(문자열 복사)를 **잠금과 큐에 손대기 전에** 만든다. 만들지 못하면, 큐에 넣지 못하면 드롭 수에 센다(퇴출이 이미 센 1건과 별개로 새 알림도 센다).
  - `sync_loss_alert_locked`: 손실 알림 항목을 연결하기 전에 완성하고(문구용 공간을 64바이트로 미리 잡아 둔 뒤의 갱신은 할당이 없다), 문구는 "자리 비우기 퇴출" 을 센 **뒤에** 만든다. 항목을 못 만들면 큐는 그대로이고 드롭 수는 정확하다. 호출자용 `…_best_effort_locked` 는 예외를 삼킨다(문구는 다음 알림이 따라잡는다).
  - `xpe_init`: 설정 문자열 복사를 `g_initialized = true` 앞으로 옮기고, 확정 구간은 던지지 않는 연산(`swap`, `clear`)만. 확정된 뒤의 `internal_log` 실패는 삼킨다. 잡는 예외는 `bad_alloc`→OOM, 그 밖→PROCESSING_FAILED.
  - `xpe_configure`: 복사본을 잠금 앞에서 만들고 `swap`. `bad_alloc`→OOM.
  - 시험 전용 `xpe_common_alerts_dropped_for_test()` (`XPE_COMMON_TEST_HOOKS` 일 때만, 수출 아님).
- `xpe_logging.cpp`: `xpe_log_set_file` 의 `bad_alloc`→OOM.
- `modules/common/CMakeLists.txt`: 새 타깃 `xpe_common_oom_tests` — 라이브러리 소스를 실행 파일에 직접 컴파일해 넣는다(공유 라이브러리 안의 할당은 실행 파일이 바꾼 `operator new` 에 닿지 않는다). `XPE_COMMON_TEST_HOOKS` 는 이 타깃에만 정의된다.
- 시험 `tests/test_common_oom_injection.cpp`(8개): 주입이 닿는지 통제 1, 알림 수지 3(가득 찬 큐 / 여유 있는 큐 / 손실 알림이 이미 있는 큐), init 2(첫 init / 재init), configure 1, log_set_file 1.

## 3. 빨강 → 초록

- 빨강(구현 전, `02_red_run.txt`): 8개 중 7개 빨강 — 알림 3개는 "an exception left the C ABI function when allocation #1 failed", init 2개는 부분 커밋, configure 는 rc -4, log_set_file 은 rc -9. 통제 1개는 초록(주입이 닿음).
- 중간에 한 번 더 빨강: 손실 알림 문구를 퇴출 **앞** 에서 만들도록 순서를 바꿨더니 문구가 1 낮게 나왔다("loss alert says 2, the count is 3"). 수지 시험의 "따라잡음" 단언이 잡았고, 퇴출 뒤로 되돌려 고쳤다.
- 초록(`04_green_run.txt`): 8개 통과. 스윕 점수: 알림(가득) 4, 알림(여유) 2, 알림(손실 알림) 2, init 1+1, configure 21, log_set_file 4.

## 4. 실측 (이 트리, 이 실행)

| 항목 | 관측 |
|---|---|
| 빌드 | **전체 타깃** `cmake --build build/ci-preprocess` `BUILD_EXIT=0` (`21_build_final.txt`). 앞선 카드들은 시험 타깃 둘만 빌드했다 — 이번부터 전체 |
| common OOM exe | 8 통과 (`22_common_oom.txt`) |
| common 기존 시험 | 69 통과 (`23_common_tests.txt`) |
| preprocess DLL 시험 | 768 실행 / 760 통과 / 8 건너뜀, 섞기도 동일 (`24`, `25`) |
| preprocess OOM exe | 18 통과 (`26_pre_oom.txt`) |
| `ctest -N` | 903 (직전 895 = +8) (`27_ctest_n.txt`) |
| 수출 이름 | preprocess 48 차이 없음, common 16 QA-A-201 증거와 동일 |
| 헤더 문서 / 프리셋 | 0 findings / 12 of 12, 종료 0 |

## 5. 반증 (한 번에 하나, 두 실행 파일 모두 실행, 복원 뒤 `cmp` 동일)

| 팔 | 손상 | 빨개진 시험 (그 외 초록) |
|---|---|---|
| dropuncounted | 큐에 못 넣은 알림을 세지 않고 삼킴 | 알림 수지 셋 ("65 alerts pushed but 63 in the queue and 1 counted as lost" 등) |
| initlate | 옛 순서: 플래그를 먼저, 설정 복사를 뒤에 | init 둘 ("partial commit") |
| configclass | `xpe_configure` 의 `bad_alloc` 갈래 제거 | `AConfigureThatRunsOutOfMemory…` (rc -4) |
| logclass | `xpe_log_set_file` 의 `bad_alloc` 갈래 제거 | `ALogFileThatRunsOutOfMemory…` (rc -9) |

## 6. 미검증 (Gaps)

- **`xpe_log_set_file` 의 부분 상태는 이번에 닫지 않았다.** 함수는 새 싱크를 만들기 전에 이전 로거를 이미 내려놓는다. 할당 실패로 새 로거를 못 만들면 이전 로거가 없어진 채 오류를 돌려준다. 리더 범위는 오류 코드의 오분류였고 시험은 코드만 단언한다.
- 알림 수지 시험은 시험 이음매(`xpe_common_alerts_dropped_for_test`)로 정확한 손실 수를 읽는다. 제품 쪽 보고 수단(손실 알림 문구)은 "다음 알림이 따라잡는다" 까지만 확인했고, 따라잡을 알림이 없으면 문구가 낡은 채 남는다.
- `xpe_get_pending_alert`·`internal_log` 의 로그 파일 쓰기 실패는 측정하지 않았다.
- `ci-preprocess` 구성에서만 돌렸다. `ci-common` 프리셋(별도 vcpkg 매니페스트)으로는 빌드하지 않았다 — 새 타깃이 그 구성에서도 빌드되는지는 미확인.
- Linux/GCC 미검증.

## 7. 잔여 위험

- 손실 알림 문구를 만들 때 공간을 64바이트로 미리 잡는 것은 현재 문구("alert queue overflow: N alert(s) dropped", 최대 약 60바이트)에 맞춘 값이다. 문구가 길어지면 갱신이 다시 할당을 하게 되고, 그 경우 예외는 삼켜지고 문구가 낡는다(수는 정확).
