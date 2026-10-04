# QA-B-209c — Codex #152 보류 2건: DEBUG 종료 줄은 항상, 실패면 ERROR 줄을 따로

기준: `dev/postprocess` 9b4c05ec 위. Refs #251. 11개 전부 진입 로그, 고정 파일 공유 제거, `xpe_dicom_version` 안전성은 닫혀 있었다(그대로).

## 1. 실패 호출의 DEBUG 종료 (높음)
**지적이 맞다.** REQ-DICOM-043 은 "entry/exit at DEBUG **and** error conditions at ERROR" 라 둘 다다. 209b 에서 카드 문구("실패면 종료 줄을 ERROR 로")를 그대로 옮겨 실패 시 DEBUG 종료 줄을 ERROR 줄로 **바꿨고**, 그 대체가 요구의 절반을 지운다는 점을 그때 짚지 못했다(요구 문구와 카드 문구를 대조하지 않았다). 시험도 "실패 호출에 DEBUG 종료가 없어야 한다" 를 단언해 그 오류를 굳히고 있었다.
수정 (`dicom.cpp`): 종료는 **rc 와 무관하게 항상** `[debug] [xpe_dicom] <fn> exit rc=<N>`, 그리고 `rc != XPE_OK` 이면 **별도의** `[error] [xpe_dicom] <fn> exit rc=<N> (<에러 문자열>)` 를 추가로. 성공은 DEBUG 줄만. void 함수는 `exit`.
진입 줄도 `[debug] [xpe_dicom] <fn> entry [details]` 로 통일했다(`xpe_dicom_open({})` 같은 괄호 형식과 인자 없는 형식이 섞여 있어 함수명 뒤 구분자가 일정하지 않았다).

## 2. 함수 줄 식별이 앞부분 일치 (낮음)
**지적이 맞다.** `find("[debug] [xpe_dicom] <fn>")` 는 진입·종료를 가르지 못하고 `xpe_dicom_write` 가 `xpe_dicom_write_j2k` 줄에도 걸렸다.
수정 (`test_dicom_logging.cpp`): 줄 전체를 정규식으로, 함수명 뒤 구분자까지 — 진입 `\[debug\] \[xpe_dicom\] <fn> entry( .*)?$`, DEBUG 종료 `... <fn> exit rc=<N>$`, ERROR 결과 `\[error\] \[xpe_dicom\] <fn> exit rc=<N> \(.+\)$`. 각 export 가 독립으로 정확히 1줄씩.

## 시험 (7건)
- `EveryPublicFunctionLogsEntryDebugExitAndAnErrorLineWhenItFails`: **11개** 함수 각각 — 진입 1줄, DEBUG 종료 1줄(rc 일치, **실패에도**), ERROR 결과 1줄(rc 일치, 상태 코드가 있는 8개). close·cancel·version 은 진입 1 + DEBUG 종료 1 + ERROR 0. `write` 와 `write_j2k` 가 같은 실행에 있어 접두어가 겹치지만 이름별로 정확히 1.
- 세 대표 경우: 조기 거부(진입 + DEBUG 종료 + ERROR 결과, 안쪽 ERROR 없음), 내부 실패(안쪽 원인 ERROR + DEBUG 종료 + ERROR 결과), 성공(write → open → close 가 DEBUG 줄만, ERROR·warning 0줄).
- 종료는 진입 뒤, 호출당 한 번. 전송 파일 읽기 실패와 저장 실패의 ERROR(그대로).

## 반증 (유효하게 빨강인 것)
| 반증 | 산출 | 결과 |
|---|---|---|
| 209b 의 코드(실패 시 ERROR 로 **대체**)에 새 시험 | `c16c_red.txt` | **5/7 빨강** — 진입 줄 형식, 실패 호출의 DEBUG 종료 줄 없음 |
| `xpe_dicom_write_j2k` 의 진입 줄만 지움 | `c16c_arm.txt` | 11개 함수 시험이 `xpe_dicom_write_j2k: exactly one DEBUG entry line` 으로 **빨강**, `xpe_dicom_write` 의 진입은 여전히 정확히 1줄 — 한쪽이 다른 쪽을 대신하지 못함 |
수정 후 7/7 (`c16c_green.txt`).

## 증거
| 주장 | 산출 | 관측 |
|---|---|---|
| 수정 후 | `c16c_green.txt` | 7/7 passed |
| 병렬 | `c16c_ctest_j8.txt` | `ctest -j 8 -R DicomLogging` x5, 매번 7/7 |
| ci-dicom 직렬 전체 | `c16c_ctest_dicom_serial.txt` | 380/380 passed |
| (참고) 전체를 `-j 4` 로 | `c16c_ctest_dicom_all_j4.txt` | 83건 실패 — 209b 보고서의 발견 그대로(DicomLogging 은 포함되지 않음), 이 변경과 무관 |

## Gaps
- ERROR 결과 줄의 `(<에러 문자열>)` 은 `.+` 로만 단언한다(문자열 내용은 `xpe_error_string` 의 몫).
- 35곳 warn→error 중 시험이 직접 보는 곳은 3곳(209 와 같음).
- 네트워크 함수의 성공 DEBUG 종료 줄은 시험하지 않았다(래퍼 한 곳이라 경로 차이 없음).

## Residual risk
- 로그 문구를 읽는 다른 레인 소비자(진입 줄 형식 `entry` 로 바뀜, 실패 시 줄이 두 개)는 리더가 확인.
