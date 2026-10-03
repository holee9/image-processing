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
