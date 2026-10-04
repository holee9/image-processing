# QA-B-209 C16 — 로깅 (REQ-DICOM-043)

기준: `dev/postprocess` 3049916b 위. Refs #251. 전제(공유 여부)는 183494fc 의 `report_e2_c16_facts.md`: dicom 로그는 xpe_common 의 `[xpe_file]` 로거로 나가고 `xpe_log_set_level`/`xpe_log_set_file` 에 반응한다 → 카드의 "공유한다" 갈래.

## 재현 (수정 전)
`test_dicom_logging.cpp` 5건이 모두 빨강 (`c16_red.txt`): 공개 함수 10개는 진입 줄만 있고 종료 줄이 없다(`xpe_dicom_open: no DEBUG exit line with rc=-1` 등 8건), 실패(파일 거부·전송 파일 읽기 실패·저장 실패)는 `[warning]` 로 기록된다.

## 수정
1. **종료 로그**: `dicom.cpp` — 기존 본문을 `dicom_*_impl`(static, 본문 그대로)로 두고, 내보내는 함수는 한 줄 래퍼가 `[xpe_dicom] <fn> exit rc=<코드>` (DEBUG) 를 남긴다. void 함수(close, cancel)는 `exit` 만. 모든 return 경로(앞쪽 INVALID_INPUT 포함)가 덮이고 동작은 바뀌지 않는다. 로그가 던져도 반환값에 영향이 없도록 try/catch.
2. **실패는 ERROR**: DicomReader 13, DicomWriter 10, DicomNetworkSCU 12 — 에러 코드를 돌려주는 실패 경로의 `spdlog::warn` → `spdlog::error` (35곳). **그대로 둔 3곳**: DicomReader 의 `XPE_ALERT_WARNING` 알림과 짝인 경고(조건은 허용되고 경고만 한다, 실패 아님).

## 시험 (수정 후, `c16_green.txt`)
- `EveryPublicFunctionLogsItsExitWithTheReturnCode`: 공개 함수 10개 각각의 진입 줄과 `exit rc=<반환 코드>` 줄(반환 코드와 일치).
- `TheExitLineFollowsTheEntryLineAndIsWrittenOncePerCall`: 종료 줄은 진입 줄 뒤에, 호출당 한 번.
- `ARefusedFileIsLoggedAtErrorLevel` / `AnUnreadableFileToSendIsLoggedAtErrorLevel` / `AFailedSaveIsLoggedAtErrorLevel`: 같은 조건이 `[error]` 로 한 줄, 같은 조건의 `[warning]` 는 없음. 저장 실패는 `IO_FAILED` 가 실제로 나와 `saveFile failed` 줄을 확인했다(우회 분기 안 탐).
- 읽는 곳은 xpe_common 의 로그 파일(`xpe_log_set_file` + 수준 TRACE), 시험 뒤 INFO 로 복원.

## 증거
| 주장 | 산출 | 관측 |
|---|---|---|
| 수정 전 빨강 5/5 | `c16_red.txt` | 5 FAILED, 진입 줄은 있고 종료 줄 없음 |
| 수정 후 | `c16_green.txt` | 5/5 passed |
| ci-dicom 전체 | `c16_ctest_dicom.txt` | 374/374 passed (C8 때 369 + 새 5) |

## Gaps
- ERROR 로 올린 35곳 중 로그 시험이 직접 보는 것은 3곳(읽기 거부, 전송 파일 읽기, 저장 실패). 나머지는 같은 치환이며 소스 대조로만 확인했다(`grep spdlog::warn` 에 3곳만 남음).
- `xpe_dicom_version` 은 진입 줄도 종료 줄도 없다(상수를 돌려주는 한 줄). REQ 의 "each function" 에 넣을지는 요구 해석이라 건드리지 않았다.
- 성공 경로의 종료 줄은 void 함수(close, cancel)로만 확인했다(`open` 성공은 픽스처가 필요해 시험하지 않음). 래퍼가 반환값을 가리지 않고 한 곳에서 기록하는 구조라 경로별 차이는 없다.
- 반증(한 곳을 warn 으로 되돌리면 빨강)은 돌리지 않았다. 수정 전 빨강이 그 역할이다.

## Residual risk
- 로그 문구를 정규식으로 잡는 소비자가 있다면 `[warning]` → `[error]` 변경으로 깨진다. `modules/dicom/tests` 에는 `[warning]` 문자열 의존이 없었다. 다른 레인(`clients/`, `gui/`)은 확인하지 않았다.
