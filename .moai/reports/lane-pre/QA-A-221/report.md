# QA-A-221 — #233 마감 대조: 수출 64개에서 예외가 아직 C ABI 밖으로 나갈 수 있는가 (보고서, Refs #233)

기준: dev/preprocess `b2625d7a`(QA-A-212d 포함, 지금 브랜치 끝). main `3a991d7c`. 대조 대상: QA-A-201(`78b7dd18`, 기준 HEAD `da05b8c0`). 증거: `evidence/`(프로브 소스 `10_*.cpp.txt`, 결과 `08_probe_results.txt`·`09_*`, 정적 조사 `04_survey.txt`, 64행 표 `20_table64.md`). 프로브는 일회용이라 저장소에 넣지 않았다(임시 CMake 대상과 소스는 되돌려 삭제, `git status` 에 변경 없음).

## 결론

1. **QA-A-201 이 찾은 "예외가 C ABI 밖으로 나가는 함수 14개"는 지금 0개다.** 같은 방법(K번째 할당을 실패시키는 스윕, 함수별 별도 프로세스)으로 **프로세스 45개(탐침 42 + 설정 숫자 2 + 캐시 1)를 전부 정상 종료**(종료 코드 0, 크래시 0) 시켰고 `escaped=0`, `lockleak=0` 이다. 201 의 실측 6·코드 읽기 8 이 모두 닫혔다(아래 표).
2. **입력만으로 던지던 경로 둘**(파이프라인 설정·고스트 설정의 잘못된 숫자 7건)은 모두 `XPE_ERR_CONFIG_INVALID`(-4)를 돌려주고 예외·핸들 누수가 없다(블록 +0).
3. **부분 커밋은 201 의 둘(`xpe_alert_push`, `xpe_init`)이 닫혔고, 새로 보이는 한 종류가 남았다(R1)**: 호출이 OOM 으로 실패했는데 앞 단계에서 이미 성공한 **적재가 교정 저장소에 남는다**(캐시 적재 3종 각 5지점, 파이프라인(교정 경로) 16지점). 손상은 아니다(저장소는 온전한 맵이다).
4. **오류 코드 오분류는 하나가 남았다(R2)**: `xpe_preprocess_init` 이 OOM 을 `PROCESSING_FAILED`(-3)로 돌려준다(19/19). 나머지는 전부 `OUT_OF_MEMORY`(-2).
5. `extern "C"` 블록 안 도우미가 catch 를 없애는 꼴은 **남은 한 곳이 할당하지 않는 구조체 생성뿐**이다(위험 없음, 아래).
6. **#233 은 201 에서 시작한 범위(예외·잠금)에 대해서는 닫을 수 있다 — R2 한 줄 수정과 R1 의 결정 한 가지를 빼고.** 남은 것은 아래 "남은 것".

## 방법 (카드 1)

201 의 프로브(`evidence/10_probe_main.cpp.txt`, `10_probe_seh.cpp.txt`, `10_probe_log.cpp.txt` 를 그대로 가져옴)를 제품 소스·`xpe_common.cpp`·`xpe_logging.cpp`·`xpe_memory.cpp` 와 한 실행 파일에 컴파일(대체 `operator new`로 K번째 할당 실패 주입; 임시 CMake 대상 `10_probe_cmake_target.txt`). **프로세스 하나에 함수 하나**(`AUDIT_ONLY=<번호>`)라 예외가 C ABI 를 넘어 프로세스를 죽여도 그 자체가 결과로 남는다. 실측 열의 뜻은 201 과 같다: K = 주입이 닿은 할당 지점 수, 실패 = 오류 코드 또는 탈출, 탈출, 잠금 누수(함수가 잡는 뮤텍스가 남음), 부분 커밋(실패했는데 함수가 소유한 상태가 바뀜).

**201 프로브에서 바꾼 것(전부 기록)**:
- 반환 코드 분포 `rc{}` 열 추가(201 은 코드 오분류를 코드 읽기와 시험으로만 봤다).
- 고정물 보정: 파이프라인은 float 결과 자리(`dataSize = N*4`, QA-A-205 계약)를 요구해 201 의 입력이 `-8` 로 베이스라인 실패 → 입력 버퍼를 N 개 float 크기로; `xpe_verify_offset` 은 입력이 균일해 할당 지점이 0 이었던 것을 in-tree 시험과 같은 변하는 값으로.
- **201 이 "읽기만" 했던 8개와 그 밖의 할당 수출을 추가로 스윕**: `xpe_preprocess_pipeline`(교정 경로)·`_batch`, `xpe_verify_pipeline`, `xpe_defect_detect_runtime`, `xpe_bpm_generate`, `xpe_calib_generate_nonlin_lut`, `xpe_nonlinearity_correct`, 평범한 적재 3종, 캐시 적재 3종, `xpe_calib_save`, `xpe_calib_generate_offset`·`_gain`·`_gain_polynomial`, `xpe_calib_load_nonlin_lut`, `xpe_calib_check_expiry`, `xpe_preprocess_init`·`shutdown`. 합계 탐침 42개 + 설정 숫자 2가지 + 캐시 목록·색인 1가지.
- 상태 비교(`partial`)의 첫 반복 때문에 생기는 **탐침 자신의 오탐 두 가지를 고쳤다**: 출력 맵을 호출 안에서 채우면 첫 반복의 "이전" 이 앞선 성공 호출의 맵이라 부분으로 읽혔고(`defect_detect_runtime`·`bpm_generate` 각 1), `generate_offset` 은 호출 안에서 `std::vector` 를 만들어 **탐침 자신의 할당**이 제품 할당으로 세졌다(탈출 4). 두 경우 모두 호출 밖으로 옮겨 재측정해 0 이다(첫 실행 결과도 `evidence/12_detail.txt` 에 남김).

## 201 의 실측 6 · 코드 읽기 8 각각 (카드 2의 핵심)

| 201 의 판정 | 함수 | 201 당시 | 지금 | 고친 카드 |
|---|---|---|---|---|
| 실측 | `xpe_alert_push` | 탈출 3/3, 가득 찬 큐 부분 4/4 | K=3·4 실패 0(삼킴) 탈출 0 부분 0 | QA-A-204 1/2 `9abae87c` |
| 실측 | `xpe_offset_correct` | 탈출 1/1 + **잠금 누수** | **K=0**(맵 복사 제거로 할당 지점 없음) | QA-A-202 `babdc77d` |
| 실측 | `xpe_defect_correct` | 탈출 7/7 + **잠금 누수** + 블록 +6 | K=7 실패 7(전부 -2) 탈출 0 잠금 0 | QA-A-202 `babdc77d` |
| 실측 | `xpe_preprocess_pipeline_ex` | 탈출 12/14 + 잠금 누수 2 + 블록 +6; 설정 숫자 탈출 | K=28 실패 28(-2) 탈출 0 잠금 0; 설정 숫자 -4 | QA-A-202 `babdc77d`, 202b `189ec742` |
| 실측 | `xpe_ghost_create` | 설정 숫자 탈출 + 핸들 누수; 긴 값 5지점 중 탈출 2 | K=25·25 전부 -2, 탈출 0, 블록 +0; 설정 숫자 3건 -4 | QA-A-202 `babdc77d`, 209 `90c1b6b1` |
| 실측 | `xpe_verify_gain` | 탈출 2/2 | K=2 실패 2(-2) 탈출 0 | QA-A-204 3/3 `63559d35` |
| 읽기 | `xpe_preprocess_pipeline` | 읽기만 | K=76 실패 73(-2) 탈출 0 잠금 0 **부분 16**(R1) | QA-A-202b `189ec742` |
| 읽기 | `xpe_preprocess_pipeline_batch` | 읽기만 | K=76 실패 73(-2) 탈출 0 **부분 16**(R1) | QA-A-202b `189ec742` |
| 읽기 | `xpe_verify_offset` | 읽기만 | K=3 실패 3(-2) 탈출 0 | QA-A-204 3/3 `63559d35` |
| 읽기 | `xpe_verify_pipeline` | 읽기만 | K=4 실패 4(-2) 탈출 0 | QA-A-204 3/3 `63559d35` |
| 읽기 | `xpe_defect_detect_runtime` | 읽기만 | K=8 실패 8(-2) 탈출 0 부분 0 | QA-A-204 3/3 `63559d35` |
| 읽기 | `xpe_bpm_generate` | 읽기만 | K=72 실패 72(-2) 탈출 0 부분 0 | QA-A-204 3/3 `63559d35` |
| 읽기 | `xpe_calib_generate_nonlin_lut` | 읽기만 | K=11 실패 11(-2) 탈출 0, 파일 상태 변화 0 | QA-A-204 3/3 `63559d35` |
| 읽기 | `xpe_nonlinearity_correct` | 읽기만 | K=23 실패 23(-2) 탈출 0 부분 0 | QA-A-204 3/3 `63559d35`, 209b `9eb5c410` |
| (캐시) | `put_locked` | 색인·목록 불일치, 버퍼 누수 | 캐시 프로브: K=16 실패 15 탈출 0 **"A 가 더 이상 캐시에 없음" 0** | QA-A-203 `cd6b17ed` |
| (코드) | `xpe_configure`, `xpe_log_set_file`, `xpe_init` | 오분류(-4·-9), 부분 1 | -2 로 정정, 부분 0 | QA-A-204 1/2 `9abae87c`, 202c `1f5c072b` |

위 커밋은 전부 main 의 조상이다(`git merge-base --is-ancestor`, 12개 확인).

## 표: 수출 함수 64개 (카드 2)

`evidence/20_table64.md`(탐침 결과에서 자동 생성). "탐침 없음" 행은 할당 지점이 없거나(정적 조사 `allocHere=0`·읽기) 가드된 함수에 위임하는 것이다.

| # | 함수 | 201 당시 | 지금 (K = 주입 가능 지점, 실패, 탈출, 잠금 누수, 부분 커밋, 반환 코드) | 고친 카드·커밋 | 비고 |
|---|---|---|---|---|---|
| 1 | `xpe_alert_push` | 탈출 3/3, 가득 찬 큐에서 부분 4/4 | K=3 실패 0 탈출 0 잠금누수 0 부분 0 rc{}; K=4 실패 0 탈출 0 잠금누수 0 부분 0 rc{} | QA-A-204 1/2 `9abae87c` |  |
| 2 | `xpe_alloc_image` | 읽기: malloc, 예외 아님 | 탐침 없음 — try=-/catch=- | — |  |
| 3 | `xpe_clear_alerts` | K=0 | K=0 실패 0 탈출 0 잠금누수 0 부분 0 rc{} | — |  |
| 4 | `xpe_configure` | 19/19 가 OOM 인데 CONFIG_INVALID 로 오분류 | K=19 실패 19 탈출 0 잠금누수 0 부분 0 rc{-2x19} | QA-A-204 1/2 `9abae87c` | rc -4 → -2 |
| 5 | `xpe_copy_image` | 읽기: memcpy | 탐침 없음 — try=-/catch=- | — |  |
| 6 | `xpe_error_string` | 읽기 | 탐침 없음 — try=-/catch=- | — |  |
| 7 | `xpe_free_image` | 읽기 | 탐침 없음 — try=-/catch=- | — |  |
| 8 | `xpe_get_param_range` | 읽기: 할당 없음 | 탐침 없음 — try=-/catch=- | — | 탐침 없음(할당 없음, 정적 조사 allocHere=0) |
| 9 | `xpe_get_pending_alert` | K=0 | K=0 실패 0 탈출 0 잠금누수 0 부분 0 rc{} | — |  |
| 10 | `xpe_get_pending_alert_count` | K=0 | K=0 실패 0 탈출 0 잠금누수 0 부분 0 rc{} | — |  |
| 11 | `xpe_init` | 부분 1/1 (오류를 주고도 초기화됨) | K=1 실패 1 탈출 0 잠금누수 0 부분 0 rc{-2x1} | QA-A-204 1/2 `9abae87c` |  |
| 12 | `xpe_log_flush` | K=0 (쓰기 실패는 미측정) | K=0 실패 0 탈출 0 잠금누수 0 부분 0 rc{} | — | 쓰기 실패 여전히 미측정 |
| 13 | `xpe_log_set_file` | 3/3 OOM 이 IO_FAILED 로 오분류, 부분 커밋 읽기 | K=3 실패 3 탈출 0 잠금누수 0 부분 0 rc{-2x3} | QA-A-204 1/2 `9abae87c`, 202c `1f5c072b`(전환 시 이전 로거 보존), 202d `f2ca59d7` | rc -9 → -2 |
| 14 | `xpe_log_set_level` | K=0 | K=0 실패 0 탈출 0 잠금누수 0 부분 0 rc{} | — |  |
| 15 | `xpe_shutdown` | K=4 실패 0 (정상) | K=4 실패 0 탈출 0 잠금누수 0 부분 0 rc{} | — |  |
| 16 | `xpe_version` | 읽기 | 탐침 없음 — try=-/catch=- | — |  |
| 17 | `xpe_binning_correct` | 읽기: 할당 없음 | 탐침 없음 — try=-/catch=- | — |  |
| 18 | `xpe_bpm_generate` | 읽기만 | K=72 실패 72 탈출 0 잠금누수 0 부분 0 rc{-2x72} | QA-A-204 3/3 `63559d35` |  |
| 19 | `xpe_calib_cache_clear` | 읽기: 해제만 | 탐침 없음 — try=-/catch=- | — |  |
| 20 | `xpe_calib_cache_set_max_size` | 읽기: 퇴출만 | 탐침 없음 — try=-/catch=- | — |  |
| 21 | `xpe_calib_check_expiry` | 정상(읽기) | K=0 실패 0 탈출 0 잠금누수 0 부분 0 rc{} | — | K=0(할당 지점 없음) |
| 22 | `xpe_calib_generate_gain` | 정상(읽기) | K=8 실패 8 탈출 0 잠금누수 0 부분 0 rc{-2x8} | — |  |
| 23 | `xpe_calib_generate_gain_polynomial` | 정상(읽기) | K=547 실패 547 탈출 0 잠금누수 0 부분 0 rc{-2x547} | — | K=547 |
| 24 | `xpe_calib_generate_nonlin_lut` | 읽기만 + 파일 반쯤 쓰임 우려 | K=11 실패 11 탈출 0 잠금누수 0 부분 0 rc{-2x11} | QA-A-204 3/3 `63559d35` | 파일 상태 부분 0 |
| 25 | `xpe_calib_generate_offset` | 정상(읽기, 본문 전체 try) | K=7 실패 7 탈출 0 잠금누수 0 부분 0 rc{-2x7} | — | 첫 탐침에서 탈출 4 로 보였으나 **탐침 자신의 벡터 할당**이었음(밖으로 빼 재측정: 탈출 0) |
| 26 | `xpe_calib_get_max_points` | 읽기 | 탐침 없음 — try=-/catch=- | — |  |
| 27 | `xpe_calib_get_mode` | 읽기 | 탐침 없음 — try=-/catch=- | — |  |
| 28 | `xpe_calib_get_poly_degree` | 읽기 | 탐침 없음 — try=-/catch=- | — |  |
| 29 | `xpe_calib_get_quality_meta` | 읽기 | 탐침 없음 — try=-/catch=- | — |  |
| 30 | `xpe_calib_load_defect_cached` | 정상 | K=16 실패 15 탈출 0 잠금누수 0 부분 5 rc{-2x15} | QA-A-203 `cd6b17ed` | **부분 5/15**(R1) |
| 31 | `xpe_calib_load_defect_map` | K 정상 | K=10 실패 10 탈출 0 잠금누수 0 부분 0 rc{-2x10} | — |  |
| 32 | `xpe_calib_load_gain` | K 정상 | K=13 실패 13 탈출 0 잠금누수 0 부분 0 rc{-2x13} | — |  |
| 33 | `xpe_calib_load_gain_cached` | 정상 | K=19 실패 18 탈출 0 잠금누수 0 부분 5 rc{-2x18} | QA-A-203 `cd6b17ed` | **부분 5/18**(R1) |
| 34 | `xpe_calib_load_nonlin_lut` | K 정상(읽기) | K=10 실패 10 탈출 0 잠금누수 0 부분 0 rc{-2x10} | — |  |
| 35 | `xpe_calib_load_offset` | K 정상(QA-A-200 스윕) | K=10 실패 10 탈출 0 잠금누수 0 부분 0 rc{-2x10} | — |  |
| 36 | `xpe_calib_load_offset_cached` | 정상(QA-A-200 가드); put_locked 유령 엔트리·누수 실측 | K=16 실패 15 탈출 0 잠금누수 0 부분 5 rc{-2x15} | QA-A-203 `cd6b17ed`(put_locked) | **부분 5/15**(R1) |
| 37 | `xpe_calib_save` | 정상(읽기); 파일 반쯤 쓰임은 미관찰 | K=4 실패 4 탈출 0 잠금누수 0 부분 0 rc{-2x4} | 212b·212c·212d(쓰기 실패 알림·.tmp 정리) |  |
| 38 | `xpe_calib_set_mode` | 읽기 | 탐침 없음 — try=-/catch=- | — |  |
| 39 | `xpe_calib_state_load` | 읽기: 가드된 적재 함수 위임 | 탐침 없음 — try=-/catch=- | — | 자체 할당 0(정적 조사) |
| 40 | `xpe_calib_state_release` | 읽기 | 탐침 없음 — try=-/catch=- | — |  |
| 41 | `xpe_calib_unload_nonlin_lut` | 읽기: reset | 탐침 없음 — try=-/catch=- | — |  |
| 42 | `xpe_defect_correct` | 탈출 7/7 + **잠금 누수 1/1** + 블록 +6 | K=7 실패 7 탈출 0 잠금누수 0 부분 0 rc{-2x7} | QA-A-202 `babdc77d` |  |
| 43 | `xpe_defect_detect_runtime` | 읽기만(벡터 3개) | K=8 실패 8 탈출 0 잠금누수 0 부분 0 rc{-2x8} | QA-A-204 3/3 `63559d35` |  |
| 44 | `xpe_gain_correct` | K=2 정상(OOM→-2) | K=1 실패 1 탈출 0 잠금누수 0 부분 0 rc{-2x1} | — |  |
| 45 | `xpe_ghost_correct` | K=0 | K=0 실패 0 탈출 0 잠금누수 0 부분 0 rc{} | — | 215~219 이후에도 K=0(할당 없음) |
| 46 | `xpe_ghost_create` | 설정 숫자 stoi/stod 탈출 + **핸들 누수**; 긴 값 5 지점 중 탈출 2 | K=25 실패 25 탈출 0 잠금누수 0 부분 0 rc{-2x25}; K=25 실패 25 탈출 0 잠금누수 0 부분 0 rc{-2x25} | QA-A-202 `babdc77d`, 202 후속 209 `90c1b6b1`·`9eb5c410` | 설정 숫자 3건 → rc -4, 블록 +0 |
| 47 | `xpe_ghost_destroy` | 읽기: delete | 탐침 없음 — try=-/catch=- | — |  |
| 48 | `xpe_ghost_reset` | 읽기: 할당 없음 | 탐침 없음 — try=-/catch=- | — |  |
| 49 | `xpe_nonlinearity_correct` | 읽기만(설정 복사, 알림) | K=23 실패 23 탈출 0 잠금누수 0 부분 0 rc{-2x23} | QA-A-204 3/3 `63559d35`, 209b `9eb5c410` |  |
| 50 | `xpe_offset_correct` | 탈출 1/1 + **잠금 누수 1/1** | K=0 실패 0 탈출 0 잠금누수 0 부분 0 rc{} | QA-A-202 `babdc77d` | 맵 복사 제거 → 할당 지점 0 |
| 51 | `xpe_preprocess_get_param_range` | 읽기 | 탐침 없음 — try=-/catch=- | — |  |
| 52 | `xpe_preprocess_init` | 정상(읽기, 본문 전체 try) | K=19 실패 19 탈출 0 잠금누수 0 부분 0 rc{-3x19} | — | **OOM 이 -3(PROCESSING_FAILED) 으로 나감**(R2) |
| 53 | `xpe_preprocess_is_initialized` | 읽기 | 탐침 없음 — try=-/catch=- | — |  |
| 54 | `xpe_preprocess_pipeline` | 읽기만(`fromJson`, 단계 버퍼) | K=76 실패 73 탈출 0 잠금누수 0 부분 16 rc{-2x73} | QA-A-202b `189ec742` | **부분 16/73**(아래 R1) |
| 55 | `xpe_preprocess_pipeline_batch` | 읽기만 | K=76 실패 73 탈출 0 잠금누수 0 부분 16 rc{-2x73} | QA-A-202b `189ec742` | **부분 16/73**(R1) |
| 56 | `xpe_preprocess_pipeline_ex` | 탈출 12/14 + 잠금 누수 2 + 블록 +6; 설정 숫자 stof/stoi 탈출 | K=28 실패 28 탈출 0 잠금누수 0 부분 0 rc{-2x28} | QA-A-202 `babdc77d`, 202b `189ec742` | 설정 숫자 7건 → rc -4 |
| 57 | `xpe_preprocess_shutdown` | 정상 | K=0 실패 0 탈출 0 잠금누수 0 부분 0 rc{} | — | K=0 |
| 58 | `xpe_preprocess_version` | 읽기 | 탐침 없음 — try=-/catch=- | — |  |
| 59 | `xpe_temp_compensate` | 읽기 | 탐침 없음 — try=-/catch=- | — |  |
| 60 | `xpe_validate_readout_artifact` | 읽기 | 탐침 없음 — try=-/catch=- | — |  |
| 61 | `xpe_verify_defect` | 읽기: 할당 없음 | 탐침 없음 — try=-/catch=- | — | in-tree 시험이 K=0 단언 |
| 62 | `xpe_verify_gain` | 탈출 2/2 | K=2 실패 2 탈출 0 잠금누수 0 부분 0 rc{-2x2} | QA-A-204 3/3 `63559d35` |  |
| 63 | `xpe_verify_offset` | 읽기만(도우미 벡터) | K=3 실패 3 탈출 0 잠금누수 0 부분 0 rc{-2x3} | QA-A-204 3/3 `63559d35` |  |
| 64 | `xpe_verify_pipeline` | 읽기만 | K=4 실패 4 탈출 0 잠금누수 0 부분 0 rc{-2x4} | QA-A-204 3/3 `63559d35` |  |

## 입력으로 던지던 경로 (카드 1 후반)

| 호출 | 입력 | 201 | 지금 |
|---|---|---|---|
| `xpe_preprocess_pipeline_ex` | `{"detectorTempC":"abc"}`, `{"binningMode":"x"}`, `{"detectorTempC":"1e999"}`, `{"binningMode":"99999999999999999999"}` | `std::invalid_argument`/`out_of_range` 탈출 | 모두 `returned rc=-4`(`CONFIG_INVALID`), 호출자의 `try` 안에서 직접 불러도 같음(`cfgdirect`) |
| `xpe_ghost_create` | `{"alpha1":"abc"}`, `{"tier":"x"}`, `{"tau2":"1e999"}` | 탈출 + 핸들 누수(블록 +4) | 모두 `rc=-4`, **블록 +0**, `handleOut=null` |

## `extern "C"` 블록 안 도우미가 catch 를 없애는 꼴 (카드 3)

배경(QA-B-181 실측, 메모리 기록): MSVC `/EHsc` 는 C 연결 함수를 "던지지 않는다"고 가정해, `extern "C" { static … }` 안의 도우미를 부르는 함수의 `catch` 가 제거될 수 있다. 이 저장소에서의 현황은 `grep`:

- `extern "C" {` **블록이 정의를 담은 `.cpp`**: `modules/common/src/xpe_common.cpp:474`(내용은 `xpe_alert_push` 하나, 본문은 `noexcept` 도우미 `enqueue_alert` 호출), `modules/common/src/xpe_logging.cpp:44-189`(수출 로깅 함수들만; 안에 `static`·도우미 없음). `modules/preprocess/src/*.cpp` 에는 **블록이 없다**(수출은 `extern "C" XPE_API` 단일 지정, 도우미는 C++ 연결).
- 헤더의 블록: `runtime_detection.h:81-288` 은 정의를 담는다 — 안의 함수는 `inline RuntimeDetectionConfig RuntimeDetection_DefaultConfig()`(270행) **하나**이고 구조체를 값으로 만들 뿐 할당하지 않는다. 할당하는 도우미(`ComputeMedianGeneric` 등)는 288행 뒤 `namespace xpe::preprocess::internal`(C++ 연결)에 있다. `xpe_preprocess_internal.h:263-278`·`preprocess_api.h` 의 블록은 선언뿐.
- **대표 1개 실측**: 위 `RuntimeDetection_DefaultConfig` 를 스윕 — `points=0`(할당 지점 없음). 즉 그 꼴이 남아 있어도 던질 것이 없다. 수출 쪽 가드의 생존은 위 스윕 전체(실패가 전부 `-2` 로 돌아옴, 탈출 0)가 보여 준다. 오래된 위험이 재발하려면 블록 안에 할당하는 도우미를 새로 넣어야 한다.

## 우선순위 1~5 의 상태 (201 §우선순위, 카드 4)

| # | 201 의 항목 | 상태 | 근거 |
|---|---|---|---|
| 1 | 입력으로 던지는 둘(파이프라인 `fromJson`, 고스트 설정 숫자) | **닫힘** | 설정 숫자 7건 전부 -4, 탈출 0, 블록 +0 |
| 2 | `xpe_offset_correct`·`xpe_defect_correct` 의 가드·잠금 | **닫힘** | offset K=0, defect K=7 전부 -2, 잠금 누수 0 |
| 3 | `xpe_alert_push` | **닫힘** | 탈출 0, 부분 0(`K=3·4`) |
| 4 | `xpe_init` 부분 커밋, `xpe_log_set_file`/`xpe_configure` 오분류 | **닫힘** | `xpe_init` 부분 0·rc -2, `configure` 19/19 -2, `log_set_file` 3/3 -2 |
| 5 | `verify_*`·`bpm`·`runtime`·`nonlin_lut` 의 탈출 | **닫힘** | 위 표, 전부 탈출 0 |
| (6) | `put_locked` 유령 엔트리·누수 | **닫힘** | 캐시 프로브 0 |

## 남은 것 (카드 5)

**R1 — 앞 단계가 성공한 뒤의 실패가 그 단계의 효과를 저장소에 남긴다.** (설계 결정 한 가지)
- 관측: `xpe_calib_load_{offset,gain,defect}_cached` 의 **마지막 5개 할당 지점**(캐시 공개 단계; 예: offset K=12~16/16, `evidence/12_detail.txt`)과 `xpe_preprocess_pipeline`/`_batch` 의 **K=58~62·66~76**(교정 경로 적재 3종이 끝난 뒤의 단계들)에서 호출은 `OUT_OF_MEMORY` 인데 교정 저장소의 맵이 새 것으로 바뀌어 있다(탐침 `partial`).
- 성격: 두 경우 모두 **앞 단계(평범한 적재)는 성공해 커밋**되고 뒤 단계(캐시 공개, 프레임 처리)가 실패하는 것이다. 저장소의 내용은 온전하며 파일 내용 그대로다(반쯤 적재된 세트는 없다 — 세트 교체는 202c 가 원자적으로 만들었다). 어떤 시험도 "실패하면 저장소가 적재 전 그대로"를 단언하지 않는다 — in-tree `PipelineThatRunsOutOfMemoryLeavesEverythingAsItWas` 는 **같은 파일이 이미 적재된 상태**에서 시작해 디지스트가 같아 이 효과를 못 본다.
- 범위(한 줄): 결정이 필요하다 — "실패한 호출은 저장소를 건드리지 않는다"로 하면 캐시 적재는 캐시 삽입을 먼저 하고 설치를 나중에, 파이프라인은 적재 세트를 프레임 처리 성공 뒤 커밋해야 한다(둘 다 작지 않은 변경); "적재 성공은 유지"로 하면 계약 문구와 시험 한 건으로 끝난다.

**R2 — `xpe_preprocess_init` 이 OOM 을 `PROCESSING_FAILED`(-3) 로 돌려준다.** (한 줄 수정)
- 관측: K=19 전부 `rc=-3`. 나머지 수출은 모두 `-2`. 상태는 온전하다(`g_initialized` 는 맨 끝에서만 켜짐, 부분 0).
- 원인: `preprocess.cpp` 의 `catch (...) { return XPE_ERR_PROCESSING_FAILED; }` 한 곳.
- 범위: `catch (const std::bad_alloc&) { return XPE_ERR_OUT_OF_MEMORY; }` 를 앞에 두는 한 줄 + 시험 한 건(OOM 주입 → -2).

**측정하지 못한 것(미검증)**:
- `P/Invoke`(C# GUI) 경로에서 예외가 어떻게 되는지는 이번에도 측정하지 않았다 — 위 결과는 "예외가 나가지 않는다"이므로 그 질문은 지금은 닫혀 있다고 볼 수 있으나, 실제 호출자에서 확인한 것은 아니다.
- `xpe_log_flush` 의 쓰기 실패(spdlog 내부)는 K=0 이라 할당 실패로는 못 닿는다. 디스크 쓰기 오류에서의 동작은 미측정.
- 입력 8×8 이다(201 과 같은 한계). 큰 프레임에서 어느 할당이 먼저 실패하는지, 3072² 에서의 부분 커밋 패턴은 재지 않았다.
- 이 조사는 `modules/common`·`modules/preprocess` 만 다룬다(`ai`·`enhance_*`·`gsvg`·`dicom` 의 수출 함수는 보지 않았다 — 201 과 같음).
- 프로브용 `xpe_common.cpp`·`xpe_logging.cpp` 를 직접 `#include` 해 컴파일했으므로 DLL 의 실제 컴파일 옵션과 같은지는 확인하지 않았다(201 과 같음). 대신 in-tree OOM 시험(`xpe_preprocess_oom_tests` 60건, `xpe_common_oom_tests` 12건)은 이번 트리에서 전부 통과했다(`evidence/13_*`).
- "탐침 없음" 행(25개)은 할당이 없다는 정적 조사와 읽기에 기댄다 — 몇 개(`xpe_calib_state_load` 등)는 가드된 적재 함수에 위임하며, 그 위임 경로는 적재 탐침이 덮는다.

## 잔여 위험

- R1 은 앞으로 "실패한 호출 뒤의 저장소"를 가정하는 새 호출자가 생기면 처음 닿는다(현재 호출자 없음 — 조사하지 않음).
- 새 탐침은 일회용이라 회귀 방지는 in-tree OOM 시험이 맡는다. R2 같은 코드 오분류는 in-tree 시험이 `xpe_preprocess_init` 을 스윕하지 않아 못 잡았다.
