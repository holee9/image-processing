# QA-A-193 (#216) — 캐시 적중이 전역 보정 저장소를 채우고, shutdown 이 캐시를 비운다 (TDD)

시작 HEAD `28650606` (QA-A-192 위, `evidence/00_head.txt`). 결정 출처: 사용자 2026-10-01 — "⚠1·⚠2 를 고친 뒤 요구 작성, ⚠3 은 코드 변경 없이 명시".

## 한눈에

| 항목 | 결과 |
|---|---|
| ⚠1 적중이 전역 저장소를 채움 | **고침** — 오프셋·게인·결함 세 `*_cached` 모두. 적중도 그 맵을 `g_calib` 에 올린다 |
| ⚠2 shutdown 이 캐시를 비움 | **고침** — `xpe_preprocess_shutdown()` 이 `xpe_calib_cache_clear()` 를 부른다 (`api-spec.md:499` "모듈 종료까지 유효"와 일치) |
| ⚠3 키는 경로 문자열 | **코드 변경 없음**, 세 `*_cached` 의 헤더 문서와 QA-A-191 문안에 명시, 시험이 계약을 고정 |
| 게인 다항식 파일의 `gain_cached` | **관측**: 전역 저장소에는 올라가고 `xpe_gain_correct` 도 동작하지만 이 함수는 `XPE_ERR_NOT_INITIALIZED` 를 돌려주고 캐시에 넣지 않는다 (결정 항목으로 남김) |
| QA-A-191 문안 | 같은 보고서(`QA-A-191/verdict.md`)를 고친 동작에 맞게 갱신 |

전체 시험 1회: **734 통과, 0 실패** (직전 728 + 신규 6), 종료 코드 0 (`evidence/24_full_suite.txt`). 셔플(시드 19300) 종료 0, `check_header_docs.py` 20 헤더 0 findings, 내보내기 이름 53 = 53 동일 (`26_`), `ctest -N` 총계 851·DISABLED 40, 프리셋 점검 OK. 수출 이름과 시그니처는 바뀌지 않았다.

## TDD 순서

1. **시험 먼저 (`test_calib_cache_global_store.cpp`, 6건)**. 빨강 (`02_red_run.txt`): 5건 실패, 1건(⚠3 고정) 통과.
   - 오프셋·게인·결함 각각: 경로 A 를 캐시로 올린 뒤 **다른** 맵 B 를 평범한 로더로 전역 저장소에 올리고 A 를 다시 캐시로 부른다 (적중). 직후 보정 결과가 A 의 것이어야 한다. 빨강일 때 보정 결과는 B 의 것(오프셋 700, 기대 900)이었다. 같은 캐시 포인터가 돌아오는지(적중이 맞는지)를 전제로 먼저 단언했고, 그 전제는 통과했다.
   - 결정에 적힌 시나리오: 세 맵을 캐시로 올리고 shutdown+init 뒤 다시 캐시로 올리면 보정 세 개가 `OK` 여야 한다. 빨강일 때 `XPE_ERR_CALIB_NOT_LOADED`(-16).
   - ⚠2: 파일을 덮어쓰고 shutdown+init 한 뒤 캐시로 올리면 새 내용이 나와야 한다. 빨강일 때 옛 내용(100, 기대 300). (포인터 비교는 쓰지 않았다 — 해제 직후 `malloc` 이 같은 주소를 돌려줄 수 있어 신뢰할 수 없다.)
   - ⚠3: 파일을 덮어써도 같은 경로의 두 번째 호출은 옛 맵을 돌려주고, `xpe_calib_cache_clear()` 뒤에야 새 맵이 나온다. 현재 동작 그대로 통과 — 계약을 고정하는 시험이다.
2. **구현**: `calibration_cache.cpp` 의 엔트리에 파일의 타임스탬프·세션 id 를 함께 보관하고(`CachedMap`), 적중 때 같은 잠금 안에서 픽셀을 한 번 복사해 잠금 밖에서 전역 저장소에 올린다(`get_copy`, 설치 함수 세 개). `preprocess.cpp` 의 shutdown 이 캐시를 비운다 (`:79`). 초록 (`04_green_run.txt`).
3. **반증 팔** (제품을 망가뜨림, 최종 구현에서 다시 돌림, 매 팔 `BUILD_EXIT` 확인 — `evidence/21_arms_summary_final.txt`):

| 팔 | 망가뜨린 곳 | 빨개진 시험 |
|---|---|---|
| 1a | 오프셋 적중이 설치하지 않음 | `OffsetCacheHitLeavesThatMapInTheGlobalStore` 만 |
| 1b | 게인 적중이 설치하지 않음 | `GainCacheHitLeavesThatMapInTheGlobalStore` 만 |
| 1c | 결함 적중이 설치하지 않음 | `DefectCacheHitLeavesThatMapInTheGlobalStore` 만 |
| 2 | shutdown 이 캐시를 비우지 않음 | `ShutdownEmptiesTheCache` 만 |

네 팔 모두 `BUILD_EXIT=0`, 각자 자기 시험 하나만 빨개졌고 원복 후 파일이 동일하다. (같은 네 팔을 첫 구현에서도 돌렸고 결과가 같았다: `06_arms_summary.txt`.)

## 적중 비용 (측정)

적중이 맵 전체를 전역 저장소로 복사하게 됐으므로 3072×3072 오프셋 맵(FLOAT32 37.7 MB)으로 쟀다 (임시 프로브, 30회 중앙값, 3회 반복):

| 구현 | 적중 | 미스 |
|---|---|---|
| 첫 구현 (픽셀을 벡터로 한 번, 전역 배열로 또 한 번 — 복사 두 번) | 14.0~14.6 ms (`17_`) | 68~72 ms |
| 최종 (곧바로 전역 배열로 — 복사 한 번) | **6.5~6.7 ms** (`19_`) | 68~73 ms |

고치기 전 적중은 구조체 복사만 했으므로 사실상 공짜였고(측정하지 않음), 지금은 파일 읽기 없이 약 6.6 ms, 미스의 1/10 이다. 복사 하나는 피할 수 없다 — 캐시와 전역 저장소가 각자 자기 배열을 소유한다. 프로브 소스는 `evidence/22_timing_probe_source.cpp.txt`, 파일은 지웠다.

## 게인 다항식 파일 (관측)

임시 프로브(`evidence/08_poly_observed.txt`, 소스 `09_`, 파일은 지움): `XCAL_TYPE_GAIN_POLY` 파일에 `xpe_calib_load_gain_cached` 를 두 번 부르면 두 번 모두 `-6` (`XPE_ERR_NOT_INITIALIZED`). 그런데 직후 `xpe_gain_correct` 는 `0` 을 돌려주고 보정이 적용된다 (다항식이 전역 저장소에 올라가 있다). 이 함수는 전역 저장소의 **스칼라** 게인 맵만 읽고(`gain_map` 이 NULL 이면 `NOT_INITIALIZED`), 다항식 적재는 `gain_map` 을 비우기 때문이다.

- 헤더 문서에 사실대로 적었다 (`xpe_calib_load_gain_cached`: "A gain POLYNOMIAL file … returns XPE_ERR_NOT_INITIALIZED for it and caches nothing").
- 동작은 바꾸지 않았다. 결정에 필요한 것: 모듈이 초기화된 상태에서 적재가 성공했는데 `NOT_INITIALIZED` 를 돌려주는 현 동작을 요구로 굳힐지, 의미가 맞는 코드(예: 지원하지 않는 형식)로 바꿀지.

## 변경 파일

| 파일 | 변경 |
|---|---|
| `modules/preprocess/src/calibration_cache.cpp` | 엔트리에 타임스탬프·세션 id, `get_copy`, 설치 함수 3개, 세 로더의 적중 경로, 헤더 주석 |
| `modules/preprocess/src/preprocess.cpp` | shutdown 이 `xpe_calib_cache_clear()` 호출 |
| `modules/preprocess/include/xpe/preprocess_api.h` | 세 `*_cached` 문서를 다시 씀(키, 적중이 저장소를 채움, 소유권·유효 기간, 오류 코드, 다항식 파일), `xpe_calib_cache_clear`·`set_max_size` 주석 |
| `modules/preprocess/tests/test_calib_cache_global_store.cpp` | 신규 6건 (`CacheGlobalStore.*`) |
| `modules/preprocess/CMakeLists.txt` | 시험 파일 등록 1줄 |
| `.moai/reports/lane-pre/QA-A-191/verdict.md` | 요구 문안 REQ-P1A-102~104 와 해결 내역을 고친 동작에 맞게 갱신, 줄 번호를 이 카드 이후 기준으로 |

## Gaps / Residual-risk

- 적중이 설치하는 맵은 **캐시에 있던 복사본**이다. 파일이 바뀌었는데 캐시를 비우지 않으면 적중은 옛 맵을 전역 저장소에 올린다 (⚠3 의 결과; 문서에 명시).
- 적중은 파일 읽기의 **만료·SHA-256·세션 검사를 반복하지 않는다**. 엔트리가 만료 시각을 들고 있지 않아, 캐시된 뒤에 만료된 교정 파일도 적중으로는 계속 올라간다 (코드 읽기; 관측하지 않음).
- 게인 적중은 `xpe_calib_get_quality_meta` 가 보고하는 **품질 메타데이터를 복원하지 않는다**. 그 값은 마지막 파일 읽기의 것이다 (코드 읽기).
- shutdown 은 캐시 **용량 설정**(`xpe_calib_cache_set_max_size`)을 되돌리지 않는다. 이것도 "모듈 전역 전부" 로 볼지는 결정 항목이다 (QA-A-120 이 `xpe_calib_mode_reset_globals` 에서 같은 질문을 처리했다).
- 캐시 잠금과 `g_calib_mutex` 를 함께 잡는 곳은 없다 (`get_copy` 는 캐시 잠금만, 설치는 `g_calib_mutex` 만, shutdown 은 캐시를 먼저 비우고 나서 `g_calib_mutex` 를 잡는다). 기존 동시성 시험 `test_calib_cache_concurrency.cpp` 는 통과했지만 적중 설치와 shutdown 이 동시에 일어나는 경우를 겨냥한 시험은 추가하지 않았다.
- 같은 경로를 서로 다른 종류의 `*_cached` 로 부르는 경우(공유 캐시, 경로 키)는 여전히 관측하지 않았다. 설치 함수는 엔트리 크기가 `너비 × 높이 × 원소 크기` 와 다르면 `XPE_ERR_PROCESSING_FAILED` 로 막는다.
