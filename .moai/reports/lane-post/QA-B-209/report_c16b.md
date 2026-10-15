# QA-B-209b — Codex #149 보류 3건: DICOM 로그 (C16)

기준: `dev/postprocess` 7c4b8465 위. Refs #251. 래퍼 구조와 warn→error 35곳 분류는 지적되지 않았다(그대로).

## 1. 11번째 공개 함수 (높음)
**지적이 맞다.** 공개 export 는 11개이고 `xpe_dicom_version` 을 빼기로 승인된 기록이 없다. 내가 "요구 해석이라 건드리지 않았다" 고 한 것은 승인 없는 제외였다.
수정: `xpe_dicom_version` 도 진입 줄 `[debug] [xpe_dicom] xpe_dicom_version` 과 종료 줄 `... xpe_dicom_version exit` 을 남긴다. 로그 호출은 try/catch 로 막아 로그가 실패해도 반환값(`"1.0.0"`)과 ABI 는 그대로(`log_entry_noexcept`, `log_exit_void`).

## 2. 조기 실패에 ERROR 가 없음 (높음)
**지적이 맞다.** `log_exit` 가 rc 와 무관하게 DEBUG 였다.
수정: 종료 줄을 `rc == XPE_OK` 이면 DEBUG, 그 외는 **ERROR** `... exit rc=<N> (<xpe_error_string>)`.
**중복 정책(보고서에 정하라고 한 것)**: 같은 실패에 ERROR 가 두 줄 나는 것을 **허용**한다. 안쪽 줄은 *원인*(`not a DICOM Part 10 file ...`), 래퍼 줄은 공개 호출의 *결과*(어떤 코드를 돌려줬는지)다. 조기 INVALID_INPUT 처럼 안쪽 로그 지점이 없는 반환은 래퍼 줄이 유일한 ERROR 이므로, 모든 오류 반환이 ERROR 로 보이는 곳은 래퍼 하나다. 래퍼만 쓰고 안쪽을 지우면 원인 정보를 잃고, 안쪽만 두면 조기 실패가 안 보인다. 시험이 두 줄을 따로 단언한다.

## 3. 고정 로그 파일 (중간)
**지적이 맞다.** `gtest_discover_tests` 가 시험마다 별도 프로세스를 만들고 고정 경로를 지우고 다시 만들었다.
수정: 로그와 스크래치 파일 이름에 **PID 와 시험 이름**을 넣는다(`xpe_dicom_logging_<pid>_<test>.log/.dcm`). 프로세스별로 로거 설정도 독립이다. 증거: `ctest -j 8 -R DicomLogging` 을 **5번** 돌려 모두 통과 (`c16b_ctest_j8.txt`: 7/7 x 5, exit 0).

## 시험 (7건, `test_dicom_logging.cpp`)
- `EveryPublicFunctionLogsItsEntryAndExit`: **11개** 함수. 상태 코드가 있는 8개는 DEBUG 진입 + **ERROR** 종료(`exit rc=<코드>`)이고 DEBUG 종료 줄은 없음; close·cancel·version 은 DEBUG 진입·종료; version 의 반환값 `"1.0.0"` 불변.
- 세 대표 경우의 **수준** 단언: 조기 거부(`AnEarlyRefusalIs...`: ERROR 한 줄, DEBUG 종료 줄 없음, 안쪽 ERROR 없음), 내부 실패(`AnInternalFailure...`: 원인 ERROR + 결과 ERROR 각 1줄, warning 없음), 성공(`ASuccessIsLoggedAtDebugLevelOnly`: write → open → close 가 DEBUG 만, **ERROR·warning 0줄**).
- 기존: 종료 줄 순서·1회, 전송 파일 읽기 실패, 저장 실패의 ERROR.

## 반증 (옛 코드에서 빨강)
`c16b_red.txt`: 209 의 코드(수정 전)에서 **3건 빨강** — 11개 함수 시험(version 줄 없음·종료가 DEBUG), 조기 거부, 내부 실패의 종료 줄. 성공 경로 시험은 옛 코드에서도 초록(대조군: 성공의 DEBUG 는 원래 맞았다). 수정 후 7/7 (`c16b_green.txt`).

## 증거
| 주장 | 산출 | 관측 |
|---|---|---|
| 수정 전 빨강 3/7 | `c16b_red.txt` | 3 FAILED, 4 OK(성공 대조군 포함) |
| 수정 후 | `c16b_green.txt` | 7/7 passed |
| 병렬 실행 | `c16b_ctest_j8.txt` | `ctest -j 8 -R DicomLogging` x5, 매번 7/7 |
| ci-dicom 직렬 전체 | `c16b_ctest_dicom_serial.txt` | 376/376 passed |

## 발견 (이 변경과 무관, 보고만)
`ctest -j 4` 로 **전체** dicom 스위트를 돌리면 83건이 실패한다(`c16b_ctest_dicom_all.txt`: DicomReaderTest 74, DicomWriterTest 3, DicomValidatorTest 2, DicomNetworkTest 1 등). 직렬은 376/376. 이 레인의 기존 스위트(이 카드에서 건드리지 않음)가 병렬 실행을 가정하지 않는다 — 원인(공유 임시 픽스처 파일 가설)은 확인하지 않았다. CI 가 `-j` 로 dicom 전체를 돌리면 걸릴 수 있다; 병렬 안전은 `DicomLogging` 에 대해서만 보였다.

## Gaps
- 35곳 warn→error 중 시험이 직접 보는 곳은 3곳(209 에서와 같음). 래퍼 ERROR 는 모든 비OK 코드에 적용되나 시험은 INVALID_INPUT, DICOM_INVALID, IO_FAILED 만 본다.
- `xpe_dicom_validate` 가 비적합 파일에 비OK 를 돌려주는 설계라면 그 호출은 이제 ERROR 로 기록된다(검증 결과를 보고하는 정상 흐름도 "오류 상태" 로 보임). 이 함수의 반환 의미는 확인하지 않았다.
- 네트워크 함수(cstore, cfind_mwl)의 성공 DEBUG 종료 줄은 시험하지 않았다(래퍼 한 곳이라 경로 차이는 없다).

## Residual risk
- 로그 문구를 정규식으로 잡는 다른 레인의 소비자는 리더가 확인(`[warning]` → `[error]`, 새 종료 줄).
- 정상 흐름에서 비OK 를 돌려주는 호출(예: 타임아웃을 기대하는 시험)이 ERROR 를 남겨 로그가 시끄러워질 수 있다.
