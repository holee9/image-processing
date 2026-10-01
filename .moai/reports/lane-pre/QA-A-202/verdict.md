# QA-A-202 — #233 1·2순위: 설정 JSON 의 잘못된 숫자, 보정 두 함수의 예외·잠금·맵 복사

기준 커밋 `cd6b17ed`(QA-A-203, Codex #21 반영) 위. 증거는 `evidence/` (번호 순). 오프셋·결함 보정은 이 카드에서 처음 손댔고, QA-A-203 보다 먼저 작업한 분을 따로 보관했다가 그 위에 다시 얹었다.

## 1. 주장

| # | 주장 | 근거 |
|---|---|---|
| ① | 파이프라인 설정(`detectorTempC`, `binningMode`)과 `xpe_ghost_create` 설정(`tier`, `alpha1`, `tau1`, `alpha2`, `tau2`, `tier2Threshold`, `nlcscBeta`)의 숫자가 "전체가 유한한 수, 범위 안"이 아니면 `XPE_ERR_CONFIG_INVALID` 이고, 이미지·메타데이터·적재된 교정·핸들·할당이 그대로다. 정상 값은 그대로 통과한다. | 빨강 `02_red_run.txt`, 초록 `22_full.txt`, 반증 `arm_endcheck_*` |
| ②a | `xpe_offset_correct`·`xpe_defect_correct` 는 예외를 C ABI 밖으로 내보내지 않는다(`bad_alloc`→OOM, 그 외→PROCESSING_FAILED). | 빨강 `04_red_oom_run.txt`, 반증 `arm_guard_*` |
| ②b | 두 함수가 맵 전체를 복사하지 않는다. 잠금 아래에서는 `shared_ptr` 하나만 잡고, 읽기는 잠금 밖에서 제자리에서 한다. 동시 재적재 중에도 한 프레임은 한 맵의 결과뿐이다. | 시험 `...AllocatesNothing`, `...DoesNotCopyTheMap`, `CorrectMapSharing.*`, 반증 `arm_copy_*`·`arm_unowned_*`, 시간 `13_perf.txt` |

## 2. 무엇을 바꿨나

- `src/xpe_strict_parse.hpp`(새): `std::from_chars` 기반 `parse_float/double/int`. 전체 문자열이 한 수여야 하고, 범위 밖·`inf`·`nan`·뒤 글자는 거부. 로케일 무관, 공백·`+`·16진수 불허. 예외를 던지지 않으며 실패하면 출력은 그대로.
- `pipeline.cpp`: `PipelineConfig::fromJson` 이 `XpeErrorCode` 를 돌려준다. 세 진입점 모두 **교정 파일을 읽기 전에** 설정부터 읽어, 거부된 설정이 아무것도 적재하지 않는다.
- `ghost_correct.cpp`: 핸들을 `unique_ptr` 가 소유하다가 모든 단계가 끝난 뒤 `release`. 설정은 이력 버퍼를 만들기 전에 파싱한다. 예외는 OOM/PROCESSING_FAILED 로 바꾼다(전에는 핸들과 이력 버퍼 4블록이 샜다).
- `xpe_preprocess_internal.h`: `offset_map`, `defect_map` 을 `unique_ptr` → `shared_ptr`. 설치된 맵은 제자리에서 고치지 않는다는 규칙을 주석으로 적었다.
- `offset_correct.cpp`·`defect_correct.cpp`: function-try-block 으로 예외 가드, 잠금 아래에서는 포인터 복사만.
- `xpe_calib_generate_offset.cpp`: 정적 결함 마스크 병합이 제자리 수정이었으므로(읽는 쪽과 경합) 옆에 새 맵을 만들어 병합한 뒤 교체하도록 바꿨다. 실패하면 저장소는 그대로다.
- `preprocess_api.h`: 네 함수의 반환값 설명에 `XPE_ERR_CONFIG_INVALID` 조건 추가.
- 시험: `test_config_strict_parse.cpp`(여섯), `test_correct_map_sharing.cpp`(둘), `xpe_preprocess_oom_tests` 에 넷 추가(`AnOffsetCorrectionAllocatesNothing`, `ADefectCorrectionThatFailsLeavesTheStoreUntouchedAndTheLockFree`, `ADefectCorrectionDoesNotCopyTheMap`, `AGhostCreationThatIsRefusedLeavesNoBlocksBehind`).

## 3. 빨강 (구현 전, 이 트리)

| 시험 | 관측 |
|---|---|
| `ConfigStrictParse` 네 개(파이프라인 `_ex`·교정 미적재·배치·ghost 거부) | 모두 빨강: 예외가 함수 밖으로 나감, 교정 파일이 적재됨, 핸들 누수 |
| 같은 시험의 대조군(정상 값 통과) 둘 | 구현 전에도 초록 — 거부만 값 때문임을 보인다 |
| `CorrectMapSharing` 둘 | 구현 전에도 초록(복사 방식도 한 맵의 결과를 낸다). 이 시험은 "소유 없이 읽기" 팔을 잡는 용도 |
| OOM: `AnOffsetCorrectionAllocatesNothing` | 빨강: 프레임이 할당함(맵 복사) |
| OOM: 결함 스윕 | 빨강: 할당 #1 실패에 예외 탈출 + 잠금 유지 |
| OOM: ghost 거부 | 빨강: 거부된 생성이 블록을 남김 |

## 4. 실측 (이 트리, 이 실행)

| 항목 | 관측 |
|---|---|
| 빌드 | `BUILD_EXIT=0` (`21_build_final.txt`) |
| DLL 연결 시험 | 768 실행 / 760 통과 / 8 건너뜀(원래 건너뛰던 8건과 동일), 종료 0. 직전 760/752/8 → 새 시험 8개 (`22_full.txt`) |
| 섞기 | 같은 수, 종료 0 (`23_shuffle.txt`) |
| OOM 실행 파일 | 17 통과 (직전 13) (`24_oom.txt`). 결함 스윕 할당 점수 6 |
| `ctest -N` | 894 (직전 882) (`25_ctest_n.txt`) |
| 수출 이름 | 48개, QA-A-203 증거와 차이 없음 (`27_exports_diff.txt` 비어 있음) |
| 헤더 문서 | 20 headers, 0 findings |
| 프리셋 | 12 declared, 12 compared, 종료 0 |
| 시간 (3072², 같은 라운드 교대 4회, 중앙값) | `xpe_offset_correct` 9.40/9.67/9.64/9.23 ms → 2.93/2.90/2.76/2.92 ms. `xpe_defect_correct` 8.76/9.08/8.91/8.72 ms → 7.34/7.29/7.19/7.23 ms (`13_perf.txt`). 입력은 균일 프레임 + 고립 결함 200개, 오프셋 맵은 균일 |

## 5. 반증 (한 번에 하나, 빌드 후 두 실행 파일 모두 실행, 복원 뒤 `cmp` 로 동일 확인)

| 팔 | 손상 | 빨개진 시험 (그 외는 초록) |
|---|---|---|
| endcheck | 끝 검사(`ptr != last`) 제거 | `ConfigStrictParse` 의 파이프라인 `_ex`·배치·ghost 거부 셋 ("25.5x", "2x", "0.5x" 입력) |
| guard | 두 함수의 예외 가드 제거 | `ADefectCorrectionThatFailsLeavesTheStoreUntouchedAndTheLockFree` |
| copy | 두 함수 모두 맵 복사로 되돌림 | `AnOffsetCorrectionAllocatesNothing`, `ADefectCorrectionDoesNotCopyTheMap` |
| unowned | `shared_ptr` 대신 날 포인터로 잠금 밖에서 읽기 | `CorrectMapSharing` 둘 |

주의: 처음 쓴 입력 넷("abc", "x", "1e999", 20자리 정수)만으로는 endcheck 팔이 빨개지지 않는다 — 넷 모두 변환 자체가 실패한다. 뒤 글자가 붙은 입력 셋을 더해서 그 팔이 잡히게 했다. 같은 이유로, 20자리 수는 `double` 로는 유효한 값이라 `tier2Threshold` 에는 쓰지 않고 정수 칸(`tier`, `binningMode`)에만 썼다.

## 6. 미검증 (Gaps)

- **`xpe_offset_correct` 의 예외 가드는 주입으로 관측되지 않는다.** 공유 소유권으로 바꾸고 나면 이 함수의 프레임은 할당이 0 이라 실패시킬 지점이 없다(`AllocatesNothing` 이 그것을 단언한다). 가드는 방어로 넣었고, guard 팔에서 이 함수 쪽은 빨개지지 않는다.
- 같은 이유로 "복사를 잠금 안으로 되돌림" 은 *잠금 보유 시간* 이 아니라 *할당* 으로 잡는다. 동시 재적재 시험은 한 맵의 결과인지만 보고 잠금 시간은 재지 않는다.
- `ci-preprocess` 구성 하나에서만 돌렸다. Mock/Native 백엔드 잡, Linux/GCC 는 안 돌림.
- 성능은 균일 프레임·균일 오프셋 맵에서만 쟀다. 결함 보정 시간은 결함 분포에 따라 달라진다.
- `xpe_calib_mode.cpp` 의 `atoi`/`atof` 는 예외를 던지지 않고 잘못된 값을 조용히 0 으로 읽는다. 이번 범위(예외 탈출)가 아니라 손대지 않았다.

## 7. 잔여 위험

- 맵이 `shared_ptr` 가 되었으므로 이전 맵은 마지막 사용자가 끝나야 해제된다. 재적재가 잦으면 프레임이 끝나기 전까지 맵 두 장이 동시에 살 수 있다(맵 하나 분량의 순간 메모리 증가).
- `xpe_calib_generate_offset` 의 결함 병합은 이제 맵 한 장을 복사한다(오프라인 교정 경로, 프레임 경로 아님).
