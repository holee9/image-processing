# QA-A-244 — main CI 빨강: 만료 시험이 벽시계 창에 기대어 CI 에서 실패

## 1. 결과

| 항목 | 결과 |
|---|---|
| 실패 원인 | `A241Safety.AMapThatExpiresWhileLoadedStopsTheFrame` 가 `nowMs() + 400` 창 안에서 적재와 첫 프레임을 끝내야 하는 시험이었다. 느린 CI 러너에서는 그 사이에 만료돼 첫 프레임이 `-5` 였다 |
| 수정 | 만료를 판단하는 모든 곳이 읽는 시계에 **시험 전용 주입 지점**을 하나 두고(공개 라이브러리에는 없음), 벽시계 창에 기대던 시험 8건을 주입 시각으로 판정하도록 바꿨다 |
| 창을 넓혔는가 | 아니다. 시험에 벽시계 창이 없다. 시험이 시각을 정한다 |
| 반증 | 만료 경계(1 ms 전 / 정확히 / 1 ms 후 / 시각을 되돌림), "느린 환경" 시험(실제 1 초를 적재와 프레임 사이에 둠), 주입을 무시하는 변형 둘이 빨강 |
| 시험 시간 | 만료 시험 8건이 합쳐 약 20 초 걸리던 것이 합쳐 약 0.4 초(느린 환경 시험 1 초 제외) |

## 2. 원인과 접근

실패한 시험은 파일의 만료 시각을 `현재 + 400 ms` 로 잡고, 적재와 첫 프레임이 그 안에 끝난다고 가정한 뒤 600 ms 를 잤다. "첫 프레임은 정상, 만료 뒤 프레임은 거부" 를 보려던 시험인데, 앞쪽(정상)이 속도에 의존했다. 같은 형태의 시험이 같은 파일에 더 있었다.

### 왜 241b 의 시계 주입을 그대로 쓸 수 없었나

241b 에서 만든 것은 순수 함수 `xpe_calib_snapshot_expiry_check_at(snapshot, mask, nowMs)` 이고, 시험이 손으로 만든 스냅샷과 시각을 넘긴다. 이 시험들은 **파이프라인 전체**(`xpe_preprocess_pipeline_ex`, 단계별 함수, 캐시 적중, 로더)가 읽는 시계를 건드려야 하는데, 그 호출들은 DLL 안에서 시스템 시계를 직접 읽는다. 주입 지점이 없었다.

### 만든 것

- `xpe_calib_now_ms()` (`xpe_preprocess_internal.h`): 만료를 판단하는 시각을 읽는 한 곳. 라이브러리 빌드에서는 시스템 시계 읽기 그대로다.
- `XPE_CACHE_TEST_HOOKS` 빌드에서만 존재하는 함수 포인터 `xpe_clock_now_ms_hook`. 이 매크로는 공개 라이브러리에 정의되지 않으므로 **출하 DLL 에는 시계를 옮길 방법이 없다** (만료 검사의 시각을 바꿀 수 있는 지점을 출하 코드에 두면 SRS-CALIB-SAFE-002 를 우회하는 길이 되기 때문이다).
- 만료를 판단하는 네 곳이 이 함수를 읽는다: 로더의 적재 시 검사(`xcal_reader.cpp`), 캐시 적중 시 검사(`calibration_cache.cpp`), 프레임·단계 시 검사(`calib_expiry_check.cpp`), `xpe_calib_check_expiry`. 파일 생성 시각 등 만료와 무관한 시계 읽기는 그대로 둔다.
- 새 시험 실행 파일 `xpe_preprocess_clock_tests` (`CMakeLists.txt`): 제품 소스를 `XPE_CACHE_TEST_HOOKS` 로 직접 컴파일해 넣은 것이다. 기존 `xpe_preprocess_oom_tests` 와 같은 방식이고, ctest 이름은 `clock.` 접두사로 구분한다.

## 3. 바꾼 시험 (벽시계 창에 기대던 것 전부)

`grep -n "nowMs() +\|sleep_for"` 로 찾아 목록을 만들었다 (`modules/preprocess/tests/`):

| 파일 | 시험 | 이전 | 지금 |
|---|---|---|---|
| `test_a241_safety.cpp` | `AMapThatExpiresWhileLoadedStopsTheFrame` | +400 ms, 600 ms 잠 (3 종류, 1878 ms) | 주입 시계, 600 ms 전진 |
| 같음 | `ABypassedMapThatExpiredStillStopsTheFrame` | +300 ms, 500 ms 잠 | 주입 시계 |
| 같음 | `TheThreeSingleStageFunctionsRefuseAnExpiredMapAndWriteNothing` | +400 ms, 600 ms 잠 (1892 ms) | 주입 시계 |
| 같음 | `AnExpiredMapStopsAllFourPipelineEntryPoints` | +400 ms, 600 ms 잠 | 주입 시계 |
| 같음 | `ACacheHitInstallsTheExpiryOfTheMapItInstalls` | +1500 / +1200 ms, 1700 / 1400 ms 잠 (**9473 ms**) | 주입 시계 (92 ms) |
| `test_calib_cache_same_verdict.cpp` | `AMapCachedBeforeItsFileExpiredIsRefusedLikeAMissAndNotInstalled` | +800 ms, 1100 ms 잠 | 주입 시계 |
| 같음 | `AnExpiredFileThatCannotBeOpenedIsIoFailedNotExpired` | +800 ms, 1100 ms 잠 (3361 ms) | 주입 시계 |
| `test_oom_injection.cpp` | `AHitJudgesTheExpiryAfterTheOpenCheckNotBefore` | +600 ms, 열기 검사 뒤 900 ms 지연 | 주입 시계, 지연은 시계를 900 ms 전진시키는 훅 |

바꾸지 않은 것과 이유:
- 만료를 1 시간·600000 ms 뒤로 잡은 시험(`A241Safety` 의 `3600 * 1000`, `+ 600000`)과 며칠·몇 십 일 뒤로 잡은 시험(`test_calib_save_expiry`, `test_calibration_roundtrip`, `test_xpe_calib_check_expiry`, `test_calib_fixture_gen`)은 시간이 지나기를 기다리지 않는다. 경쟁 창이 없다.
- `test_xcal_reader`, `test_xpe_calib_load` 는 과거 시각(1 시간 전)을 만료로 쓴다. 이미 지나 있는 시각이라 경쟁이 없다.
- `test_ghost_handle_registry`, `test_xcal_replace_retry`, `test_zz_a240_checklist` 의 `sleep_for` 는 만료와 무관하다(스레드 교대, 파일 점유 시간, 시간 측정 시험).

## 4. 시험이 보장하는 것과 한계

새 시험 3건 (`A241Safety`, 주입 시계 빌드):
- `TheExpiryBoundaryIsJudgedAtTheInjectedTime`: 만료 1000 ms 뒤. 시각을 만료 −1 ms → OK, 정확히 만료 → OK (시각이 만료보다 **클 때** 만료), +1 ms → `CALIBRATION_EXPIRED`, 시각을 되돌리면(−500 ms) 다시 OK. 시계가 유일한 입력임을 보인다.
- `ASlowRealClockDoesNotChangeTheOutcome`: 실패 원인을 일부러 만든다. 만료 +400 ms, 적재 뒤 **실제로 1 초를 자고** 프레임: 주입 시각이 안 움직였으므로 OK. 시계를 401 ms 전진 → 거부. 느린 환경에서 결과가 달라지지 않는다.
- `TheLoadersJudgeExpiryAtTheInjectedTimeToo`: 로더(일반, 캐시 미스)도 주입 시각으로 만료를 판단한다 (정확히 만료 → 적재 OK, +1 ms → 거부).

반증 (`evidence/43_mutation_log.txt`, 한 스크립트의 연속 출력, 변형마다 해시와 원복 후 `touch` 재빌드):
- 변형 A (프레임·단계 시 검사가 주입 시계를 안 읽고 실제 시계를 읽음): 만료 시험 6건이 빨강 (`ASlowRealClock…` 포함).
- 변형 B (캐시 적중 검사가 실제 시계를 읽음): `CacheSameVerdict.AMapCachedBeforeItsFileExpired…` 빨강.
- 원복 뒤 전부 초록. 같은 소스의 두 빌드에서 실행 파일 바이트는 다르다(링커의 타임스탬프와 디버그 식별자). 같은 코드임은 소스 해시(`217e5425ca7191c9`, `c9937b42676d6a4d`)와 시험 결과로 보인다.

**한계 (숨기지 않는다)**
- 만료가 "지나가는" 동작은 이제 **출하 DLL 바이너리가 아니라 같은 소스를 직접 컴파일한 시험 실행 파일**(`xpe_preprocess_clock_tests`)에서 시험한다. 소스는 같고 시계 읽기 한 곳만 다르지만, DLL 빌드에서만 생기는 차이(컴파일 옵션, 링크)는 이 시험이 보지 못한다. DLL 쪽에는 만료가 이미 지난 파일을 적재 시 거부하는 시험(`test_xpe_calib_load` 등)과 `xpe_calib_snapshot_expiry_check_at` 경계 시험(241b)이 남아 있다. 프레임 시점의 만료를 DLL 로 직접 보는 시험은 이제 없다.
- `xpe_calib_now_ms()` 를 쓰지 않는 시계 읽기(생성기의 생성 시각, 모드 파일 등)는 주입되지 않는다. 만료 판단이 아니라서 그대로 뒀다.
- 이 변경은 제품 헤더와 소스 4곳을 건드린다 (출하 동작은 같다: 시계 읽기를 한 함수로 모았다). 시스템 시계 읽기 코드가 한 줄 한 줄 같은지는 diff 로 확인할 수 있다.

## 5. 검증

| 항목 | 결과 | 증거 |
|---|---|---|
| `xpe_preprocess_tests` (DLL) | 1037 통과, 실패 0 (`DISABLED_` 62개 제외). 만료 시험 7건은 이쪽에서 빠졌다 | `30_xpe_preprocess_tests.txt` |
| `xpe_preprocess_clock_tests` (신규) | 40 통과, 실패 0 | `30_xpe_preprocess_clock_tests.txt` |
| `xpe_preprocess_oom_tests` | 88 통과, 실패 0 | `30_xpe_preprocess_oom_tests.txt` |
| ctest 순차 | 100 %, 1269개 중 실패 0 (`clock.` 접두사 40건 포함) | `31_ctest.txt` |
| clang-tidy 기준선 게이트 (`preprocess`) | `GATE_EXIT=0` | `41_clang_tidy_gate.txt` |
| doxygen 1.12.0 | 종료 0, 경고 0 | `42_doxygen.txt` |
| 만료 시험 8건 + 새 시험 3건 | 통과, 변형 둘이 빨강 | `10_clock_tests.txt`, `43_mutation_log.txt` |

CI 러너에서의 확인은 못 했다(로컬 확인). cppcheck 는 이 기계에 없고 ASan 은 안 돌렸다.

## 6. 변경 파일

제품: `include/xpe/preprocess/xpe_preprocess_internal.h` (`xpe_calib_now_ms`, 시험용 훅 선언), `src/calib_expiry_check.cpp`, `src/xcal_reader.cpp`, `src/calibration_cache.cpp`, `src/xpe_calib_check_expiry.cpp`, `CMakeLists.txt` (모듈, 신규 시험 실행 파일).
시험: `tests/test_a241_safety.cpp`, `tests/test_calib_cache_same_verdict.cpp`, `tests/test_oom_injection.cpp`.
증거: `.moai/reports/lane-pre/QA-A-244/`.

## Card Cross-Check

| milestone | card |
|---|---|
| main CI 빨강: 벽시계 만료 시험을 주입 시계로 | QA-A-244 (이 보고서) |
