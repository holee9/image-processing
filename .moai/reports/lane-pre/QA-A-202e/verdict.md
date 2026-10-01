# QA-A-202e — Codex #38 보류 3건: 현재 게인의 품질만 답한다, 캐시 적중의 품질 사본, 읽기 잠금 시험의 도달 동기화

기준 커밋 `c7369562`(QA-A-208) 위. 감사 원문은 `evidence/00_codex38_audit_original.md`. 증거는 `evidence/` (번호 순). 이슈 #233.

**순서 보고.** 리더의 마지막 지시는 "208b → 202e → 209 → 204 3/3" 이다. 그 지시가 도착했을 때 202e 는 이미 `xpe_calib_mode.cpp`·품질 시험을 고친 상태였고, 208b 는 같은 두 파일을 다시 고친다. 같은 파일에 걸친 두 카드의 변경은 경로 지정(pathspec)으로 나눠 커밋할 수 없어서(대화형 `add -p` 는 쓸 수 없다), **202e 를 먼저 끝내 커밋하고 이어서 208b 를 한다**. 208b 는 이 커밋 위에 쌓인다.

## 1. 주장

| # | 주장 | 근거 |
|---|---|---|
| 1 | 품질 기록(`xpe_calib_get_quality_meta`)은 **현재 게인 보정**의 것만 답한다: 게인을 확정하는 모든 경로(개별 적재, 파이프라인 세트 2종, 캐시 miss·hit)에서 기록이 항상 교체된다 — 파일에 품질이 있으면 그 값, **없으면 "없음" 기록**(`valid = 0`, `previous_r_squared` 외 전부 0). 이전 파일의 값이 현재 것처럼 남는 경로가 없다. 이력(`previous_r_squared`)은 현재 기록과 구분되어 남고, 품질 없는 파일이 사이에 끼어도 지워지지 않는다. | 빨강 `02_red.txt`, 초록 `05_green_oom.txt`, 반증 `arm_none_not_committed_*`, `arm_hit_none_not_committed_*`, `arm_history_erased_*` |
| 2 | 캐시 적중은 맵·치수·시각·세션과 같은 임계 구역에서 파일 품질 사본(`gain_quality`, `gain_has_quality`)도 설치한다(품질 없음은 `false`/0 으로 덮는다). | 빨강 `02_red.txt`, 반증 `arm_hit_copy_not_installed_*` |
| 3 | 읽기 잠금 시험은 시간에 기대지 않고 읽기 스레드가 **getter 의 잠금 시도 지점에 도달했음을 관측**한 뒤 대기를 단언한다. Codex 가 제시한 무력화(잠금 제거 + 도달 신호 뒤 지연)에서도 빨갛다. | 반증 `arm_getter_unlocked_*`, `arm_getter_unlocked_delayed_*` |
| 4 | (기록만) `xpe_log_set_file` 의 헤더에 "호출자 직렬화" 계약을 적었고, 202d 보고서의 "전환 중 직접 로그는 A 또는 B" 는 스레드 안전성 근거가 아니다 — 아래 6절 정정. | `xpe_common_api.h` |

## 2. "없음"을 어떻게 돌려주나 — 선택과 이유 (카드: "반환 코드 또는 유효성 필드, 덜 깨지는 쪽")

**선택: 유효성 필드, 반환 코드는 그대로 `XPE_OK`.** 구조체 `XpeCalibQualityMeta` 에 `uint8_t valid` 를 `num_points` 바로 뒤, **전에는 패딩이던 자리**에 넣었다.

- **호출자 확인.** 모듈 밖(`clients/`, `gui/`, 다른 모듈)에서 `xpe_calib_get_quality_meta` 나 `XpeCalibQualityMeta` 를 쓰는 곳은 **0건**이다. 같은 검색(`.cs`·`.cpp`·`.h`·`.xaml`·`.py`, 모듈 안 소스·시험·빌드 산출물 제외)이 `xpe_preprocess_init` 은 `clients/`·`gui/` 에서 찾아내므로 대조가 성립한다. 호출자는 이 모듈의 시험뿐이다.
- **반환 코드를 바꾸지 않는 이유.** 품질 없는 게인 파일은 정상 파일이고 적재도 성공한다. 그 상태에서 조회가 오류 코드를 돌려주면 "조회 실패"와 "조회했더니 알려진 품질이 없음"이 섞이고, 호출자가 `XPE_OK` 를 기대하는 기존 시험·코드가 깨진다. 새 오류 코드도 필요 없다.
- **ABI.** 필드는 패딩에 들어가므로 `sizeof` 와 다른 모든 멤버의 오프셋이 그대로다. 시험이 컴파일 시간에 단언한다(`static_assert`: 크기 88, `valid` 3, `r_squared` 8, `calibration_timestamp` 16, `detector_serial` 24, `firmware_version` 56, `calibration_pass` 72, `previous_r_squared` 80). 같은 값이 **변경 전**(HEAD) 헤더에서도 성립함을 실제로 컴파일해 확인했다(`07_old_header_layout_probe.txt`, `CL_EXIT=0`) — 그래서 "그대로"는 계산이 아니라 관측이다. 구조체 정의를 복제한 외부 코드(P/Invoke 등)는 이 저장소에 없다.
- **"없음"의 값.** `valid = 0`, 나머지 전부 0(시작 상태와 같다: `previous_r_squared = -1.0` 만 이력으로 남는다). 품질이 일부 키만 있는 파일은 `valid = 1` 이고 없는 필드는 전부터의 "no data" 값(R² −1.0)이다.

**한 가지 한계를 정직하게.** 리더 결정은 "현재 **맵**의 품질만"이었다. 생성기(`xpe_calib_generate_gain…`)는 맵을 적재하지 않고 파일만 쓰지만(생성기 소스의 `g_calib` 참조는 0건이고 `load` 호출도 없다; 같은 `grep` 이 오프셋 생성기에서는 8건을 센다 — 대조) FUNC-033 이 "생성된 보정의 품질을 조회"를 요구하므로, 생성 직후의 기록은 **생성된 보정**을 가리킨다 — 저장소에 이전에 적재한 맵이 있어도 그것이 아니다. 다음 게인 적재가 기록을 교체한다. 이 예외는 `xpe_calib_get_quality_meta` 의 문서에 적었다(문서 블록의 "After a generation…"). "다른 맵의 품질을 현재로 보고하는 경로"는 적재 경로에는 없다; 생성 직후만 이 예외다.

## 3. 무엇을 바꿨나

- `preprocess_api.h`: `valid` 필드(패딩), 품질 조회·게인 적재·파이프라인 계약 문서.
- `xpe_calib_mode.cpp`: 파싱 결과와 생성기 기록에 `valid = 1`; `xpe_calib_commit_no_quality_locked()` 신설; 이력은 `qm.valid ? qm.r_squared : qm.previous_r_squared` 로 잇는다(품질 없는 기록을 건너뛴다). 호출자가 없어진 잠금 감싸개 `xpe_calib_commit_quality_meta` 는 지웠다.
- `xpe_calib_load_gain.cpp` `xpe_calib_commit_gain_locked`: `qualityFound` 면 그 값, 아니면 "없음" — 항상 교체. (개별 적재·파이프라인 세트 2종이 이 함수를 쓴다.)
- `calibration_cache.cpp` `install_gain`: 같은 임계 구역에서 `gain_quality`·`gain_has_quality` 를 설치(없음은 덮어쓰기)하고 기록을 교체.
- 시험 관측점 `xpe_calib_quality_before_lock_hook`(getter 맨 앞, 잠금 전; 시험 실행 파일에만 컴파일).

## 4. 시험

- `ALoadOfAGainWithoutQualityLeavesNoPreviousFilesQualityBehind` / `…WithDifferentQualityReplacesTheRecordOnEveryPath`: 다섯 경로(`xpe_calib_load_gain`, 캐시 miss, 캐시 **hit**, `xpe_preprocess_pipeline`, `xpe_preprocess_pipeline_batch`) × (A 품질 있음 → B 품질 없음: 조회가 "없음", R² 0, 이력 0.91 / A → C 다른 값: C 의 값, 이력 0.91). hit 경로는 `xpe_cache_after_open_check_hook` 호출 횟수로 **실제로 적중이었음**을 단언한다(전엔 miss 와 구별되지 않았다).
- `AFileWithoutQualityThenOneWithQualityKeepsTheHistoryAcrossTheGap`: A → 품질 없음 → C 에서 이력이 A 의 R² 로 이어진다.
- `ACacheHitInstallsTheFilesQualityCopyBesideTheMap`: A 적재 → B 캐시 적중 → 잠금 아래 `gain_map` 이 B 이고 `gain_quality`·`gain_has_quality` 도 B 의 것(품질 없는 B 는 `false`/0).
- `AQualityReadStartedInsideTheCommitWaitsForItAndSeesTheNewSet` 재작성: 읽기 스레드를 확정 구역 **안에서** 시작하고, getter 맨 앞 훅이 "잠금을 시도하기 직전"에 도달했음을 알려 준다(관측, 최대 10 초 대기). 그 뒤 잠금 없는 읽기에게 1 초를 준다 — 잠금이 없는 getter 는 곧바로(또는 지연이 있어도 1 초 안에) 끝나서 빨갛고, 잠금이 있는 getter 는 구역이 끝나야 끝난다. 구역이 끝난 뒤에는 읽기가 새 세트의 품질을 본다.
- 208 의 `AQualityFieldIsTakenFromTheTopLevel…` 의 기대값을 새 계약에 맞췄다: "주어지지 않음" 행은 이제 변화 없음이 아니라 **"없음" 기록**(valid 0, 전부 0)이고, 읽힌 행은 `valid 1` + 값 7.

## 5. 수정 전 / 후

- 수정 전(`03_red_summary.txt`): 다섯 경로 **모두** 품질 없는 게인을 적재한 뒤 이전 파일의 R² 0.91 이 현재 것으로 남았다(5/5). 캐시 hit 의 품질 사본은 이전 파일의 것이었다.
- 수정 후: 전부 통과. 계약은 4절 시험이 고정한다.

## 6. 202d 보고서 정정 (기록만 항목)

202d 보고서 8절은 "직접 `spdlog::info` 호출은 `g_logMutex` 밖이라 전환 도중의 한 줄이 A 와 B 중 어디에 쓰이는지는 경계 시점에 달려 있다"고 적었다. 그것은 **스레드 안전성의 근거가 아니다.** spdlog 1.14 의 `registry-inl.h`(94–95행)는 기본 로거 읽기가 `set_default_logger()` 와 동시에 쓰일 수 없다고 명시한다. 그러므로 전환 중 다른 스레드가 같은 레지스트리로 로그를 쓰면 경쟁이고, 결과(A 인지 B 인지)는 정의되지 않는다. 이번에 `xpe_log_set_file` 헤더에 "호출자가 직렬화한다 — 같은 spdlog 레지스트리로 다른 스레드가 로그하는 동안 호출하지 말 것"을 계약으로 적었다. 내부 `g_logMutex` 는 모듈 자신의 호출만 직렬화한다. 이 모듈 밖의 직접 호출이 같은 레지스트리를 쓰는지(`SPDLOG_BUILD_SHARED=OFF` 구성에서 다른 DLL 이 자기 사본을 쓰는지)는 확인하지 않았다.

## 7. 실측 (이 트리, 이 실행)

| 항목 | 관측 |
|---|---|
| 빌드 | 전체 타깃 `BUILD_EXIT=0` (`21_build_final.txt`) |
| preprocess DLL 시험 | 777 실행 / 769 통과 / 8 건너뜀(원래 건너뛰던 8건), 섞기도 동일 (`24`, `25`) |
| preprocess OOM exe | 39 통과 (`26_pre_oom.txt`; 직전 35) |
| common OOM exe / common 기존 | 12 / 69 통과 (변화 없음) |
| `ctest -N` | 937 (직전 933, 새 시험 4건) |
| 수출 이름 | preprocess 48, common 16 불변 (`29_exports_pre_diff.txt`) |
| 헤더 문서 / 프리셋 | 0 findings / 12 of 12 |

## 8. 반증 (한 번에 하나, 전체 빌드 후 OOM exe 와 DLL 시험 실행, 복원 뒤 `cmp` 동일)

| 팔 | 손상 | 빨개진 시험 |
|---|---|---|
| none_not_committed | 품질이 없을 때 기록을 교체하지 않음(옛 동작) | `ALoadOfAGainWithoutQuality…`, 208 의 `AQualityFieldIsTakenFromTheTopLevel…` |
| hit_copy_not_installed | 캐시 적중이 `gain_quality`·`gain_has_quality` 를 설치하지 않음 | `ACacheHitInstallsTheFilesQualityCopy…` |
| hit_none_not_committed | 캐시 적중이 품질 없음이면 기록을 그대로 둠 | `ALoadOfAGainWithoutQuality…` (hit 경로) |
| getter_unlocked | getter 의 잠금 제거 | `AQualityReadStartedInsideTheCommit…` |
| getter_unlocked_delayed | getter 의 잠금 제거 + 도달 신호 뒤 200 ms 지연 (Codex 의 무력화) | 같은 시험 — 1 초 창 안에서 끝나므로 빨강 |
| history_erased | "없음" 기록이 이력을 −1.0 으로 지움 | `AFileWithoutQualityThenOneWithQuality…`, `ALoadOfAGainWithoutQuality…` |

`history_erased` 는 처음 빌드가 실패해(`/WX`, 미사용 변수) 그 실행 결과가 **이전 팔의 낡은 바이너리**였다. 무효로 하고 변수를 사용하는 형태로 고쳐 빌드 성공을 확인한 뒤 다시 돌린 결과가 위 행이다.

## 9. 미검증 (Gaps)

- **`previous_r_squared` 연쇄의 미세 변화.** 적재 경로의 첫 기록에서 이력이 예전엔 `0.0`(시작 상태의 R²)이었고 이제는 `-1.0`("이전 보정 없음", 헤더가 말하던 값)이다. 생성기 경로는 전부터 그 값이었다(특수 처리가 있었다). 적재 경로의 `0.0` 을 기대하는 코드·시험은 이 저장소에 없다(전체 시험 통과).
- **생성 직후의 기록은 생성된 보정을 가리킨다**(2절의 한계) — 리더가 "적재된 맵만"으로 더 좁히고 싶으면 생성기가 기록을 쓰는 방식을 바꿔야 한다.
- **읽기 잠금 시험은 여전히 시간 창을 쓴다**(1 초): 시간에는 이제 "도달"을 기다리는 데에만 의존하지 않지만, "끝나지 않는다"는 부정 단언은 정의상 창이 필요하다. 잠금이 없는 getter 가 1 초보다 오래 지연되면 통과한다. ThreadSanitizer 같은 보조 계측은 이 빌드에 없다.
- **`xpe_calib_load_offset`·`xpe_calib_load_defect_map` 은 품질 기록에 손대지 않는다**(게인의 품질이다).
- 스레드 안전성 계약의 경계(6절): 모듈 밖 직접 spdlog 호출과 다른 DLL 의 레지스트리 사본은 확인하지 않았다.
- `ci-preprocess` 구성 하나, Windows/MSVC 에서만 돌렸다.

## 10. 잔여 위험

- `XpeCalibQualityMeta` 를 `valid` 없이 정의한 외부 코드는 그 바이트를 알지 못하고, 우리는 거기에 1/0 을 쓴다. 패딩이었던 자리이므로 외부 코드가 읽거나 쓰지 않았다면 영향이 없고, 구조체를 `memset 0` 후 받는 호출자는 `valid` 를 그대로 읽을 수 있다.
- 품질이 없는 게인을 적재하는 호출자는 이제 이전에 읽던 "마지막으로 알려진 값"을 더 이상 받지 못한다(의도된 변화, 이력은 `previous_r_squared` 로 남는다).
