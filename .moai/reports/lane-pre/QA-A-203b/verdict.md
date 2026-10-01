# QA-A-203b — Codex #22: 적중의 만료 판정 시각은 열기 확인 뒤에 잰다

기준 커밋 `babdc77d`. 증거는 `evidence/` (번호 순).

## 1. 주장

캐시 적중이 만료를 판정하는 시각은 파일을 연 **뒤**, 판정 순간에 잰다. 열기 확인이 느린 사이(네트워크 경로 등) 만료가 지나간 엔트리는 CALIBRATION_EXPIRED 가 되고 설치되지 않는다. offset·gain·defect 세 종류 모두.

## 2. 무엇을 바꿨나

- `calibration_cache.cpp`: `get_copy` 에서 `nowMs` 인자를 없애고, 만료 판정 줄에서 `now_epoch_ms()` 를 직접 부른다(캐시 잠금 안, 열기 확인 결과를 받은 뒤, 화소 복사 앞). `lookup_hit` 은 더 이상 시각을 재지 않는다. 일반 리더가 열고 읽은 뒤 시계를 보는 것과 같은 순서다.
- 시험 이음매(제품 빌드에 남지 않음): `XPE_CACHE_TEST_HOOKS` 가 정의된 경우에만 `xpe_cache_after_open_check_hook` (함수 포인터)가 선언·정의되고 `lookup_hit` 이 열기 확인 직후 부른다. 정의는 `xpe_preprocess_oom_tests` 타깃에만 있다(CMake). 공개 수출이 아니다.
- 시험 `OomInjection.AHitJudgesTheExpiryAfterTheOpenCheckNotBefore`(세 종류): 만료 600 ms 인 파일을 캐시에 넣고 → 통제(바로 다음 호출은 OK) → 열기 확인 뒤에 900 ms 지연을 넣은 호출이 EXPIRED 여야 한다.

## 3. 빨강 → 초록

- 빨강(이음매만 있고 시각은 옛 위치, `02_red_run.txt`): 세 종류 모두 기대 EXPIRED 에 실제 OK(0).
- 초록: OOM exe 18 통과(`04_green_oom.txt`, 직전 17).

## 4. 실측 (이 트리, 이 실행)

| 항목 | 관측 |
|---|---|
| 빌드 | `BUILD_EXIT=0` (`21_build_final.txt`) |
| DLL 시험 | 768 실행 / 760 통과 / 8 건너뜀(원래 건너뛰던 8건), 종료 0. 섞기도 같음 (`22_full.txt`, `23_shuffle.txt`) |
| OOM exe | 18 통과 (`24_oom.txt`) |
| `ctest -N` | 895 (직전 894) (`25_ctest_n.txt`) |
| 수출 이름 | 48개, 차이 없음 (`27_exports_diff.txt` 비어 있음) |
| 헤더 문서 / 프리셋 | 0 findings / 12 of 12, 종료 0 |
| 이음매의 범위 | `XPE_CACHE_TEST_HOOKS` 는 `modules/preprocess/CMakeLists.txt` 의 `xpe_preprocess_oom_tests` 한 줄에만 있다(저장소 전체 grep) |

## 5. 반증 (한 번에 하나, 복원 뒤 `diff` 동일)

| 팔 | 손상 | 결과 |
|---|---|---|
| clockearly | 시각을 열기 확인 앞에서 재서 판정에 넘김 | `AHitJudgesTheExpiryAfterTheOpenCheckNotBefore` **하나만** 빨강 (`arm_clockearly_*`) |
| noindex | Codex 제안: 던지는 곳도 이중 해제도 없이, 새 키에 색인 슬롯만 만들지 않음 | OOM 스윕이 깨끗한 단언으로 빨강: "allocation #1 failed (rc -3) and the cache's list and index no longer agree" (gain·offset·defect miss 스윕). 이 팔은 적중이 아예 불가능해지므로 DLL 쪽 `CacheSameVerdict` 도 다수 빨강 (`arm_noindex_*`) |

noindex 팔은 앞선 `indexlast` 팔(힙 손상 종료)이 단언으로 보여 주지 못한 것, 즉 `cacheConsistent` 단언이 실제로 색인 누락을 잡는다는 것을 직접 보여 준다.

## 6. 미검증

- 열기 확인의 지연은 시험 이음매로 만든 가짜다. 실제 느린 네트워크 경로는 시험하지 않았다.
- 시각은 판정 순간에 재지만, 판정 뒤의 화소 복사·설치 시간은 만료 여부에 반영되지 않는다(복사 후 만료되는 경계는 그대로이며, 일반 리더도 읽기 뒤 한 시점에서만 본다).
- `ci-preprocess` 구성 하나에서만 돌렸다.
