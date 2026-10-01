# QA-A-202d — Codex #32 보류 4건: 품질 메타데이터의 출처, 프레임 단위 교정 세트, 로거 전환 순서, append

기준 커밋 `1e6ecd07`(QA-A-205b) 위. 감사 원문은 `evidence/00_codex32_audit_original.md`. 증거는 `evidence/` (번호 순). 이슈 #233.

## 1. 주장

| # | 주장 | 근거 |
|---|---|---|
| 1 | `xpe_calib_get_quality_meta()` 가 서빙하는 품질 기록이 맵과 **같은 임계 구역에서** 확정되고 **같은 잠금 아래서** 읽힌다. 새 맵 옆에 옛 품질이 놓이는 구간과, 잠금 없는 읽기/쓰기(데이터 경쟁)가 없다. | 빨강 `02_red_quality_snapshot.txt`, 초록 `05_green_oom.txt`, 반증 `arm_quality_late_*`, `arm_get_unlocked_*` |
| 2 | 파이프라인의 한 프레임은 하나의 교정 세트로 처리된다: 프레임 시작 때(경로를 받는 두 진입점은 방금 적재한 세트를 확정한 같은 임계 구역에서) 세 맵·치수·다항식 범위·초기화 상태를 한 번의 잠금 아래 스냅샷으로 잡고, 모든 단계가 그 스냅샷으로 실행된다. 개별 보정 함수는 호출 시점의 맵을 쓴다(헤더 계약). | 빨강 `02_red_quality_snapshot.txt`, 초록 `05_green_oom.txt`, 반증 `arm_snapshot_ignored_*` |
| 3 | `xpe_log_set_file` 은 이전 로거의 flush 를 교체 **전에** 끝내고, 실패하면 그 오류(`XPE_ERR_IO_FAILED`)를 돌려주며 이전 로거·기본 로거를 그대로 둔다. 설치 이후에는 실패할 수 있는 연산이 없다. | 빨강 `11_red_log.txt`, 초록 `13_green_log.txt`, 반증 `arm_flush_after_*`, `arm_flush_via_logger_*` |
| 4 | 로그 파일은 헤더대로 **append** 로 열린다: 같은 경로를 다시 지정해도 앞서 쓴 행이 남고, 싱크 생성 뒤의 OOM 에서도 남는다. | 빨강 `11_red_log.txt`, 초록 `13_green_log.txt`, 반증 `arm_truncate_*` |
| 5 | (리더 추가 요청) `extern "C"` 블록 점검과 컴파일 설정 비교 — 5절. | `build.ninja` 의 FLAGS, 파일 점검 |

## 2. 무엇을 바꿨나

- **품질 기록(1).** `xpe_calib_mode.cpp` 의 전역 `g_quality_meta` 를 없애고 `CalibrationData::quality_meta`(= `g_calib` 안, `g_calib_mutex` 아래)로 옮겼다. `xpe_calib_get_quality_meta` 는 같은 잠금 아래서 복사한다. `xpe_calib_record_quality_meta`(생성기가 씀)도 잠금을 잡는다. 게인 로더의 확정 함수 `xpe_calib_commit_gain_locked` 가 맵과 같은 임계 구역에서 `xpe_calib_commit_quality_meta_locked` 를 부른다(전에는 잠금을 놓은 뒤 `xpe_calib_after_gain_commit` 에서). 캐시 적중의 `install_gain` 도 맵과 품질을 한 잠금에서 설치한다. `xpe_preprocess_shutdown` 의 `g_calib = CalibrationData{}` 가 품질 기록도 시작값(`previous_r_squared = -1`)으로 되돌리므로 별도 초기화 코드가 사라졌다.
- **스냅샷(2).** `CalibSnapshot`(세 맵의 `shared_ptr`, 치수, 다항식 범위, `initialized`)과 `xpe_calib_snapshot[_locked]()`. 세 보정 함수는 본문을 `xpe_offset_correct_in / xpe_gain_correct_in / xpe_defect_correct_in(calib, …)` 로 옮기고(수출 함수는 호출 시점의 스냅샷을 떠서 부르는 얇은 wrapper), 파이프라인은 프레임의 스냅샷을 모든 단계에 넘긴다. 오류 코드와 검사 순서는 그대로다(스냅샷 채취는 실패하지 않는다). 이를 위해 `gain_map`·`gain_poly_coeffs` 를 `unique_ptr` → `shared_ptr` 로 바꿨다(오프셋·결함 맵은 202 에서 이미 그랬다). 배치는 세트를 한 번 적재해 모든 프레임에 같은 스냅샷을 쓴다(헤더 문서가 이미 "all frames share the same calibration maps" 였다). 게인 함수는 스냅샷의 맵을 제자리에서 읽어 맵 복사(잠금 아래의 `assign`)가 없어졌다.
- **로거(3·4).** 순서: 디렉터리 검사 → 이전 로거의 **싱크를 직접** flush → 새 싱크·로거 완성 → `spdlog::set_default_logger`(설치; 던져도 이전이 기본으로 남는다) → 이후엔 `shared_ptr` 이동과 이전 로거 해제뿐. `basic_file_sink_mt(path, truncate=false)`.
- **시험용 관측점**(`XPE_CACHE_TEST_HOOKS` / `XPE_COMMON_TEST_HOOKS` 를 정의한 시험 실행 파일에만 컴파일): 세트 확정 직후, 확정 구역 안, 프레임의 오프셋 단계 직후 훅, 그리고 flush 가 실패하는 싱크를 뒤에 세우기 위한 `xpe_log_adopt_logger_for_test`.

## 3. 리더 결정과 다르게 한 곳 (이유와 함께)

리더 결정은 "외부 품질 조회의 단일 출처를 `g_calib.gain_quality`" 였다. 그대로는 하지 않았다. `gain_quality` 는 **적재한 게인 파일이 말한 값 그대로**(캐시가 항목에 같이 보관하는 사본, QA-A-200)이고, `xpe_calib_get_quality_meta` 가 돌려주는 것은 그보다 넓다:

- 생성기가 쓴다(`xpe_calib_record_quality_meta` — 맵을 적재하지 않고도 "마지막 보정의 품질"이 생긴다),
- 캐시 적중이 맵을 교체하지 않고(품질만) 다시 현재로 만든다,
- `previous_r_squared` 에 직전 기록의 R² 를 이어 붙인다(이력).

`gain_quality` 를 그 기록으로 겸하게 하면 생성기가 캐시용 사본을 덮어쓰고 이력 필드를 잃는다. 그래서 Codex 가 제안한 두 길 중 둘째("`g_quality_meta` 까지 같은 임계 구역에서 갱신하도록 저장소 구조를 통합")를 택했다: 기록은 하나(`g_calib.quality_meta`), 잠금 안에 있고, 맵과 같은 구역에서 확정된다. `gain_quality`(파일 사본)는 캐시용으로 남는다 — 둘은 서로 다른 사실이다. 리더가 "정말 하나로"를 원하면 `gain_quality` 를 캐시 쪽으로 옮기는 별도 정리가 된다(이번 범위 밖).

## 4. 계약 단락 (202c 의 3절 단락을 이것으로 교체 — 리더가 api-spec 에 옮김)

> **교정 세트의 범위.** (a) **적재의 원자성.** 파이프라인 진입점(`xpe_preprocess_pipeline`, `xpe_preprocess_pipeline_batch`)에 교정 디렉터리를 주면 `offset.xcal`, `gain.xcal`, `defect.xcal` 은 하나의 세트로 읽힌다. 세 파일이 모두 읽히고 검증되고 메모리에 올라온 뒤에야 저장된 세 맵·치수·시각·세션 id 와 **품질 기록**(`xpe_calib_get_quality_meta` 가 돌려주는 것)이 한 번의 잠금 아래서 함께 교체된다. 그 전에 어떤 오류(파일 없음·손상·만료·형식 불일치·품질 필드 불량·메모리 부족)가 나도 저장소와 품질 기록은 호출 전 그대로다. 품질 조회는 같은 잠금 아래서 일어나므로 이전 파일의 품질이나 새 파일의 품질을 볼 뿐, 새 맵 옆의 옛 품질이나 교체 도중의 기록은 보지 못한다. (b) **프레임의 일관성.** 파이프라인의 한 프레임은 하나의 세트로 처리된다: 프레임 시작 때 — 경로를 받는 두 진입점은 방금 적재한 세트를 확정한 같은 임계 구역에서, `_ex` 는 그 시점에 저장소에 있는 세트를 — 세 맵과 치수가 스냅샷으로 잡히고 모든 단계가 그것을 읽는다. 프레임이 도는 동안 다른 스레드가 다른 세트를 적재해도 그 프레임에는 닿지 않고, 다음 프레임부터 적용된다. 배치는 세트를 한 번 적재해 모든 프레임에 같은 세트를 쓴다. 비선형성 표는 세트에 속하지 않는다(그 단계가 돌 때 저장소에서 읽는다). (c) **개별 호출.** `xpe_offset_correct`, `xpe_gain_correct`, `xpe_defect_correct` 를 따로 부르면 각 호출은 호출 시점에 저장소에 있는 맵을 쓴다 — 여러 단계를 손으로 부르며 그 사이에 적재하면 호출마다 그때의 맵이 쓰인다. 세트 일관성은 파이프라인만 보장한다. (d) **세트가 확정된 뒤** 프레임 처리가 실패(예: 메모리 부족)하면 새 세트가 남는다. 개별 적재 함수(`xpe_calib_load_*`, 캐시 적재)와 `xpe_calib_state_load` 는 이 (a)의 영향을 받지 않는다.

## 5. 리더 추가 요청 — `extern "C"` 와 컴파일 설정

- **`xpe_common.cpp`**: `extern "C" {` 블록은 474–481행 하나이고 안에는 `xpe_alert_push` 하나뿐이다. QA-A-204 1/2 에서 넣은 도우미(`enqueue_alert`, `xpe_init` 확정 등)는 블록 밖이다. 31–33행의 `extern "C" void xpe_log_internal_reset();` 은 블록이 아닌 단일 선언이다.
- **`xpe_logging.cpp`**: 블록은 44–171행이고 안에는 수출 함수(`xpe_log_set_level`, `xpe_log_set_file`, `xpe_log_internal_reset`, `xpe_log_flush`)뿐이다. 도우미 `to_spdlog_level` 은 블록 앞에, 이번에 더한 시험용 `xpe_log_adopt_logger_for_test` 는 블록 뒤(파일 끝)에 있다. `xpe_log_set_file` 의 try/catch 는 자기 본문 안에서 C++ 함수(spdlog, `std::`)를 부르는 것이라 컴파일러가 지우지 않는다.
- **`runtime_detection.cpp`**: 블록은 42–157행이고 수출 함수 `xpe_defect_detect_runtime` 하나다. 이 파일은 이번 일련의 커밋(202c·205b·202d·204 1/2)에서 **바꾸지 않았다**(`git log`: 마지막 변경은 QA-A-164 등 이전 카드). 204 3/3 에서 이 파일에 OOM 가드를 넣을 때는 도우미를 블록 **밖**에 둔다(인계).
- **새 수출 wrapper**: 오프셋·게인·결함 wrapper 는 C 연결 함수이고 try 영역이 없었다. post 의 QA-B-181 측정(리더 전달; 나는 재현하지 않았다)대로면 그런 함수는 예외가 지나갈 때 지역 임시 객체(스냅샷의 `shared_ptr`)가 풀리지 않을 수 있어, **wrapper 에 try/catch 를 더했다**(안쪽 `_in` 은 C++ 연결이라 핸들러가 유지된다). 새 선언들(`CalibSnapshot`, `xpe_*_in`)은 내부 헤더의 `extern "C"` 구간(170–185행) 밖이라 C++ 연결이다.
- **OOM 시험이 다른 컴파일 설정 덕에 통과한 것은 아니다.** `build.ninja` 에서 뽑은 오브젝트 플래그가 DLL 과 OOM exe 에서 같다: `/EHsc /Zi /O2 /Ob1 /arch:AVX2 /W4 /WX` — `xpe_common.dir` 와 `xpe_common_oom_tests.dir` 의 `xpe_logging.cpp`·`xpe_common.cpp`, `xpe_preprocess.dir` 와 `xpe_preprocess_oom_tests.dir` 의 `pipeline.cpp` 모두(`evidence/40_compile_flags.txt`, `build.ninja` 의 FLAGS 줄에서 뽑음). 즉 같은 예외 모델에서 모든 K 번째 할당 실패를 주입했고 예외가 밖으로 나간 경우는 없었다.

## 6. 실측 (이 트리, 이 실행)

| 항목 | 관측 |
|---|---|
| 빌드 | 전체 타깃 `BUILD_EXIT=0` (`21_build_final.txt`) |
| preprocess DLL 시험 | 776 실행 / 768 통과 / 8 건너뜀(원래 건너뛰던 8건), 섞기도 동일 (`24`, `25`) |
| preprocess OOM exe | 33 통과 (`26_pre_oom.txt`; 직전 29) |
| common OOM exe / common 기존 | 12 / 69 통과 (직전 9 / 69) |
| `ctest -N` | 930 (직전 923, 새 시험 7건) |
| 수출 이름 | preprocess 48, common 16 불변 (`29_exports_pre_diff.txt`) |
| 헤더 문서 / 프리셋 | 0 findings / 12 of 12 |

## 7. 반증 (한 번에 하나, 전체 빌드 후 두 OOM exe 실행, 복원 뒤 `cmp` 동일)

| 팔 | 손상 | 빨개진 시험 |
|---|---|---|
| snapshot_ignored | 게인·결함 단계가 프레임 스냅샷 대신 호출 시점의 저장소를 읽음 | `AFrameIsProcessedWithOneCalibrationSet…` |
| quality_late | 품질 확정을 잠금 밖(`after_gain_commit`)으로 되돌림 | `TheQualityMetadataIsCurrentTheMomentTheMapsAre` |
| get_unlocked | `xpe_calib_get_quality_meta` 의 잠금 제거 | 처음엔 **어느 시험도 빨개지지 않았다**(보조 시험은 확률적이라 통과) → 확정 구역 안에서 읽기 스레드를 시작해 구역이 끝나기 전에는 읽기가 끝나지 않음을 보는 `AQualityReadStartedInsideTheCommitWaits…` 를 추가했고, 그 뒤 이 팔이 그 시험을 빨갛게 만든다 (`arm_get_unlocked_preoom.txt` — 첫 실행의 출력 파일은 같은 이름으로 덮어써졌다) |
| flush_after | 이전 로거 flush 를 설치 뒤로 | `AFlushThatFailsRefusesTheSwitch…` |
| flush_via_logger | 싱크 직접 flush 대신 `logger::flush()` (spdlog 1.14 는 `flush_()` 가 싱크별로 예외를 오류 처리기에 넘기고 삼킴 — `logger-inl.h:148-153` 에서 확인) | `AFlushThatFailsRefusesTheSwitch…` |
| truncate | `truncate=true` 복귀 | `NamingTheSameLogFileAgainKeepsTheLinesAlreadyThere` |

## 8. 미검증 (Gaps)

- **개별 적재 함수·캐시 적재는 한 번에 한 맵을 교체한다.** 서로 다른 호출로 적재된 맵들이 저장소에서 섞이는 것은 설계다(세트 일관성은 파이프라인의 몫). `xpe_calib_state_load` 는 그대로다.
- **비선형성 표(`nonlin_lut`)는 스냅샷에 넣지 않았다.** 파이프라인이 적재하지 않는 별도 표이고, 카드의 범위는 세 맵이다. 계약 (b)에 적었다.
- **직접 `spdlog::info` 호출(모듈 안의 로그 호출)은 `g_logMutex` 밖에서 실행된다**(Codex 확인 사항). 파일 전환 도중의 한 줄이 A 와 B 중 어디에 쓰이는지는 경계 시점에 달려 있고, 모든 줄이 원자적으로 한 파일에 모인다는 시험은 없다. 이번에 다루지 않았다.
- **flush 실패는 실제 파일이 아니라 시험용 싱크로 주입했다**(`xpe_log_adopt_logger_for_test`). 디스크가 가득 찬 실제 파일에서의 flush 실패는 시험하지 않았다. 싱크 생성·로거 생성·설치 단계의 실패는 할당 실패 스윕과 "디렉터리를 파일로 연다" 시험이 덮는다.
- **읽기 잠금 시험(`AQualityReadStartedInsideTheCommit…`)은 초록 쪽이 시간에 기댄다**: 읽기 스레드가 150 ms 안에 스케줄되지 않으면 "아직 기다리는 중"으로 보여 통과할 수 있다. 빨강 쪽(잠금이 없으면 곧바로 끝남)은 결정적이다. 동시 읽기 보조 시험(`AQualityReadWhileTheStoreIsBeingReplaced…`)은 증거가 아니다.
- **성능을 재지 않았다.** 게인 함수가 맵 복사를 하지 않고, 모든 보정 호출이 스냅샷(포인터 몇 개 복사)을 뜨는 것은 핫 경로의 변화다. 방향은 이득이지만 숫자가 없다.
- `ci-preprocess` 구성 하나, Windows/MSVC 에서만 돌렸다.

## 9. 잔여 위험

- 같은 파일을 두 핸들이 동시에 여는 것(같은 경로 재지정에서 이전 로거와 새 로거가 잠시 함께 연다)은 Windows 의 `_SH_DENYNO` 공유에 기댄다. 시험은 이 구성에서 통과하지만 다른 플랫폼에서는 확인하지 않았다.
- 게인 맵 등의 타입이 `shared_ptr` 로 바뀌었다. 저장소 밖에서 `g_calib.gain_map` 을 `unique_ptr` 로 다루는 코드가 있다면 컴파일 오류로 드러난다(이 트리의 전체 빌드는 통과).
- `previous_r_squared` 는 여전히 "교체되는 기록의 R²" 이므로 캐시 적중이 품질 없이 지나가면(`hasQuality` 거짓) 이력이 이어지지 않는다 — 전부터 그랬고 바꾸지 않았다.
