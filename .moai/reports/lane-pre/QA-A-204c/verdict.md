# QA-A-204 3/3 판정 — 할당 실패가 C ABI 밖으로 새던 나머지 공개 함수 (Refs #233)

## 1. 주장 (Claim)

| # | 주장 |
|---|------|
| C1 | `xpe_verify_offset/gain/pipeline` 은 할당 실패 시 `XPE_ERR_OUT_OF_MEMORY` 를 돌려주고 `*metrics` 를 비운 채 둔다. 예외는 밖으로 나가지 않는다 |
| C2 | `xpe_bpm_generate` 는 같은 조건에서 OUT_OF_MEMORY, 출력 맵은 호출 전 그대로 |
| C3 | `xpe_defect_detect_runtime` 도 같다. 출력 맵 초기화(memset)는 설정 단계의 할당이 모두 끝난 뒤로 옮김 |
| C4 | `xpe_calib_generate_nonlin_lut` 은 OUT_OF_MEMORY, 파일(및 .tmp)을 남기지 않는다 |
| C5 | 할당하는데 `noexcept` 였던 도우미(verify 2개, BPM 5개)에서 `noexcept` 를 제거 — 실패는 `std::terminate` 가 아니라 예외로 올라와 가드에 잡힌다 |
| C6 | `xpe_verify_defect` 는 할당을 하지 않는다 (가드할 것이 없음을 시험으로 고정) |

(`xpe_nonlinearity_correct` 는 QA-A-209b 에서 처리 — 이 카드에서 제외.)

## 2. 증거 (Evidence) — 증거 폴더 `evidence/`

- 빨강 먼저 (소스를 HEAD 로 되돌리고 시험만 추가한 상태, `02_red_*`): verify 3건 · runtime · LUT 생성은 "예외가 C ABI 함수를 빠져나갔다", `xpe_bpm_generate` 는 프로세스가 종료 코드 3 으로 죽음(noexcept 도우미 → terminate). `xpe_verify_defect` 는 처음부터 초록(할당 0 — 가드할 것이 없다는 사실을 고정하는 시험).
- LUT 시험은 처음에 시험 코드 자신이 호출 안에서 `std::string` 을 만들어 할당 실패를 시험 쪽에서 터뜨리는 결함이 있어(가드와 무관하게 빨강) 경로 문자열을 호출 밖으로 빼고 다시 확인했다 — 이후 초록 전환은 가드 때문이다.
- 초록 (`04_green_oom.txt`, 최종 `60_verify_summary.txt`): preprocess OOM 54 (기준 47, +7), DLL 799 실행 / 791 통과 / 8 건너뜀(변화 없음), 셔플 동일, common OOM 12, common 69, `ctest -N` 974 (기준 967, +7 일치), 내보내기 차이 0 / common 16, 헤더 문서 0건, 프리셋 12/12.
- 스윕 지점 수: verify_offset 3, verify_gain 3, verify_pipeline 4, bpm_generate 24, defect_detect_runtime 8, nonlin_lut 생성 11.
- 반증 9건 (한 번에 하나, 전체 빌드, 복원 후 `cmp` — `50_arms.txt`):

| 손상 | 결과 |
|------|------|
| verify 가드의 OOM 코드를 PROCESSING_FAILED 로 | verify 3건 빨강 |
| verify 가드가 metrics 를 비우지 않음 | verify_gain 1건 빨강 (offset/pipeline 은 실패 시점이 채우기 이전이라 이 손상으로는 안 드러남) |
| `compute_robust_mean` 에 noexcept 복원 | verify 시험이 종료 코드 3 으로 죽음 |
| `compute_flatness` 에 noexcept 복원 | 종료 코드 3 |
| BPM 도우미 `generate_dark_bpm` 에 noexcept 복원 | 종료 코드 3 |
| BPM 가드 코드 변경 | BPM 시험 빨강 |
| runtime 의 memset 을 앞으로 되돌림 | runtime 시험 빨강 (실패 시 맵이 지워짐) |
| runtime 가드 코드 변경 | runtime 시험 빨강 |
| LUT 가드 코드 변경 | LUT 시험 빨강 |

- noexcept 스캔 (`61_noexcept_scan.txt`, `scan_noexcept.py`): 할당 구문을 담고 catch 가 없는 noexcept 정의 0건 (42개 파일). 이 카드 전에는 verify/BPM 의 7건이 걸렸다.
- 린트: 통과 (`70_lint.txt`). 린트를 돌리다 **직전 커밋 9eb5c410 의 증거 텍스트 5개에 줄 끝 공백 112곳**이 있음을 발견해 같은 커밋에서 정리했다(커밋 전 린트는 추적 파일만 봐서 통과로 보였다).

## 3. 기준선 귀속

기준: 직전 커밋 9eb5c410 의 OOM 47 / DLL 799·791 / ctest 967. 위 수는 이번 실행의 `verify204c.sh` 가 같은 트리에서 측정한 값이다.

## 4. 미검증 (Gaps)

- 스윕은 이 로컬 MSVC·`ci-preprocess` 구성 한 곳에서만 돌렸다. 다른 컴파일러의 예외 모델은 관측하지 못함.
- verify 계열의 "metrics 비움" 반증은 verify_gain 만 잡았다. offset/pipeline 은 실패 지점이 채우기 이전이라 그 손상을 구별하지 못한다.
- LUT 생성 실패 시 `write_xcal_file` 이 지우는 `.tmp` 는 시험이 확인하지만, 디스크가 가득 찬 것 같은 I/O 실패 경로는 이 카드 범위 밖.

## 5. 잔여 위험

- 가드의 `catch (...)` 는 모든 비-bad_alloc 예외를 PROCESSING_FAILED 로 접는다. 원인 구분이 필요해지면 로그 추가가 필요하다.
- `noexcept` 스캔은 휴리스틱(정규식)이라 할당을 숨긴 다른 함수 호출은 못 본다 — 호출을 통한 할당은 위 스윕이 본다.
