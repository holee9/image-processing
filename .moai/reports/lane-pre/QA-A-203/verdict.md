# QA-A-203 — Codex #21 보류 2건: 적중의 오류 우선순위, 삽입 실패의 누수·불일치

기준 커밋 `78b7dd18`. 증거는 같은 디렉터리 `evidence/` (번호 순).

## 1. 주장

| # | 주장 | 근거 |
|---|---|---|
| ① | 캐시 적중이 파일을 판정하는 순서가 일반 리더와 같다: 크기·시각 → **열기** → 만료. 만료됐으면서 열 수 없는 파일은 `XPE_ERR_IO_FAILED`이고, 그때 엔트리는 지워지지 않는다. offset·gain·defect 세 종류 모두. | 빨강 `02_red_run.txt`(세 종류 모두 기대 -5 에 실제 -9), 초록 `04_green_cache.txt`, 반증 `arm_order_*` |
| ② | 삽입이 중간에 실패해도 캐시는 실패 전과 같고(리스트 수 == 색인 수, 노드 하나도 안 남음) 화소 버퍼는 새지 않는다. | 스윕 검사 추가 `05_green_oom.txt`, 반증 `arm_raii_*`·`arm_indexlast_*` |

## 2. 무엇을 바꿨나

- `calibration_cache.cpp`
  - `get_copy` 가 두 단계가 됐다. `openable == nullptr` 이면 크기·시각·종류 검사를 통과한 엔트리에 대해 `NeedOpenCheck` 만 답한다(지우지도 복사하지도 않는다). 호출자(`lookup_hit`)가 **캐시 잠금 밖에서** 파일을 열어 보고 다시 부른다. 열 수 없으면 `Unreadable`(엔트리 유지), 열 수 있을 때만 만료를 판정한다. 두 번째 호출은 모든 검사를 다시 하므로 그 사이에 바뀐 엔트리는 지금 상태로 판정된다.
  - 화소 버퍼를 `malloc` 에서 `operator new` 로 옮겼다(`cache_buffer_alloc/free`). 할당 실패가 다른 모든 실패와 같은 예외 하나가 되고, 스윕이 이 할당을 실패시키고 셀 수 있다.
  - `publish_and_view`: 버퍼를 `CacheBufferPtr`(RAII)가 소유하다가 캐시가 가져간 뒤에 `release`.
  - `put_locked`: 던질 수 있는 일(리스트 노드, 경로 문자열, 색인 슬롯)을 **먼저** 하고, 그 뒤(교체될 엔트리 제거, 퇴출, 버퍼 인수, 노드 연결)는 던지지 않는다.
  - `consistent()` / `xpe_calib_cache_is_consistent()`(수출 안 됨): 리스트의 모든 노드가 자기를 가리키는 색인 슬롯을 갖고 개수가 같은지 본다. 스윕 전용 이음매.
- `preprocess_api.h`: 세 로더의 "Hit" 설명에서 열기 확인을 만료 앞으로 옮기고 이유를 적었다.
- `QA-A-198/spec_text_final.md`: REQ-P1A-102 의 순서 문장, 계약 줄, 검증 표 행을 정정·추가했다. REQ-P1A-104·106 은 같은 파일의 102 를 가리키므로 별도 문장이 없다.
- 시험
  - `CacheSameVerdict.AnExpiredFileThatCannotBeOpenedIsIoFailedNotExpired`: 세 종류, 통제(읽을 수 있으면 평범한 로더가 EXPIRED, 읽을 수 없으면 IO_FAILED)를 함께 단언하고, 거부 뒤 엔트리가 살아 있어 읽을 수 있게 되면 EXPIRED 가 오는지도 본다.
  - `xpe_preprocess_oom_tests` 의 `sweep`: 매 실패마다 ① 캐시 일관성, ② 남은 블록 수(저장소·캐시를 비운 뒤의 `operator new` 살아 있는 블록 수가 호출 전과 같은가)를 단언. 모든 스윕에 적용.

## 3. 실측 (이 트리, 이 실행)

| 항목 | 명령 | 관측 |
|---|---|---|
| 빌드 | `build200.bat` | `BUILD_EXIT=0` (`11_build_final.txt`) |
| DLL 연결 시험 | `xpe_preprocess_tests.exe` | 760 실행 / 752 통과 / 8 건너뜀, 종료 0 (`12_full.txt`). 직전 759/751/8 |
| 섞기 | `--gtest_shuffle --gtest_random_seed=20260` | 760 / 752, 종료 0 (`13_shuffle.txt`) |
| OOM 실행 파일 | `xpe_preprocess_oom_tests.exe` | 13 통과, 종료 0 (`14_oom.txt`). 스윕 점수: gain miss 15→17, offset miss 13→15, defect miss 13→15(버퍼·노드 할당이 추가됨) |
| 시험 수 | `ctest -N` | `Total Tests: 882` (직전 881) (`15_ctest_n.txt`) |
| 수출 이름 | `dumpbin /exports` | 48개, QA-A-200 증거와 이름 차이 없음 (`17_exports_diff.txt` 비어 있음) |
| 헤더 문서 | `check_header_docs.py` | 20 headers, 0 findings (`18_header_docs.txt`) |
| 프리셋 | `check_cache_matches_preset.py build/ci-preprocess ci-preprocess` | 12 declared, 12 compared, 종료 0 (`19_preset.txt`) |

## 4. 반증 (한 번에 하나, 빌드 후 두 실행 파일 모두 실행, 복원 뒤 `diff` 로 동일 확인)

| 팔 | 손상 | 결과 |
|---|---|---|
| order | 만료 판정을 열기 확인 앞으로 되돌림 | `AnExpiredFileThatCannotBeOpenedIsIoFailedNotExpired` **하나만** 빨강. OOM 은 통과 |
| raii | `publish_and_view` 의 RAII 소유자를 날 포인터로 | 시험 exe 통과, OOM exe 에서 cached miss 스윕 **셋**(gain·offset·defect)만 빨강: "allocation #13 failed (rc -2) and 1 block(s) were left behind" 등 |
| indexlast | 색인 슬롯 준비를 연결 뒤로 되돌림(`index_[path] =` 가 마지막) | OOM exe 가 **힙 손상(0xC0000374)으로 비정상 종료**. 소유권을 넘긴 뒤 예외가 나면 캐시 노드와 RAII 가 같은 버퍼를 둘 다 해제한다 |

세 팔 모두 복원 뒤 `diff` 가 동일(`RESTORED: identical`)이고, 복원본으로 위 3절을 다시 쟀다.

## 5. 미검증 (Gaps)

- **indexlast 팔은 깨끗한 단언이 아니라 충돌로 잡혔다.** 힙 손상으로 죽는 것은 탐지이긴 하나, 어느 단언이 빨개졌는지는 남지 않는다. 일관성 단언(`cacheConsistent`)이 그 팔을 먼저 잡는지는 관측하지 못했다(그 전에 해제가 일어남).
- `get_copy` 의 적중 복사(`new (std::nothrow) T[n]`)는 이번에 건드리지 않았다. 스윕은 적중도 덮지만(점수 3), nothrow new 의 실패는 `operator new(nothrow)` 경로로 주입되는지 확인하지 않았다.
- `ci-preprocess` 구성 하나에서만 돌렸다. Mock/Native 백엔드 잡, Linux/GCC 는 돌리지 않았다.
- 성능은 재지 않았다. 적중 경로에 파일 열기가 한 번 늘어난 것은 아니다(전에도 열었다). 달라진 것은 열기가 잠금 밖의 별도 단계가 된 것뿐이다.
- 같은 크기·같은 수정 시각으로 바뀐 파일을 알아채지 못하는 한계는 그대로다(리더 결정, QA-A-200).

## 6. 잔여 위험

- 두 단계 조회의 사이에 다른 스레드가 같은 경로를 `put` 하면, 두 번째 호출이 그 엔트리를 다시 판정한다(크기·시각이 다르면 미스). 의도한 동작이며 시험으로 직접 조인하지는 않았다.
- `xpe_calib_cache_is_consistent()` 는 수출되지 않은 C++ 함수다. 제품 DLL 안에 심볼이 있지만 DLL 연결 시험 exe 는 부를 수 없고, OOM exe(제품 소스를 직접 컴파일)만 쓴다.
