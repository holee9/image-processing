# QA-A-212d — 임시 파일을 못 열거나 쓰기 도중 실패해도 호출자가 이유를 안다 (Refs #233)

기준: dev/preprocess `0c6584e3`(QA-A-212c) 위. 코드: `xcal_writer.cpp`, 시험 `test_xcal_replace_retry.cpp`(신규 4건). 증거: `evidence/`. 푸시하지 않음.

## 결론

1. **임시 파일을 열지 못함**: 알림 없이 `IO_FAILED` 이던 것이 이제 Error 알림 1건 `XPE_WARN_XCAL_TEMP_OPEN_FAILED` 를 낸다(원인: errno + 문자열, Windows 오류 코드).
2. **쓰기 도중 실패**: **실측으로 `.tmp` 가 남았다**(옛 코드에서 `fs::exists(tmp) == true`, 알림 없음, `evidence/11_old_code_leftover_tmp.txt`). 이제 212c 의 `remove_tmp()` 로 지우고, 지웠는지·남았는지 사실대로 `XPE_WARN_XCAL_TEMP_WRITE_FAILED` 에 알린다.
3. `write_xcal_file_ex` 의 반환 경로 전수: 알림 없는 `IO_FAILED` 는 이제 **하나도 없다 — 알림 구성이 성공하는 한**(아래 표). 알림을 만드는 문자열 구성이나 푸시가 메모리 부족으로 예외를 던지면 그 예외는 삼켜지고 알림 없이 `IO_FAILED` 만 돌아간다(QA-A-221c, Codex #81 보류 3). 남은 알림 없는 실패는 인자 오류 3곳과 예외 2곳이며 그 코드가 이유를 말한다.

## 변경 (`xcal_writer.cpp`)

- 임시 파일 쓰기 블록을 재구성: 이전엔 블록 안 곳곳에서 `return IO_FAILED`(스트림이 열린 채 반환)였다. 이제 결과(`opened`/`written`)를 블록 밖으로 내고, **스트림이 닫힌 뒤** 삭제한다(열린 핸들이 있으면 자기 파일도 못 지운다). 쓰는 순서·내용은 그대로. `flush` 뒤에 `close()` 를 명시하고 그 실패도 쓰기 실패로 센다.
- `IoReason`: 실패 직후 `errno` 와 `GetLastError()` 를 담는다. **관측 직전에 두 값을 0 으로 지운다**: 기존 파일에 대한 성공한 `CreateFile` 이 `ERROR_ALREADY_EXISTS`(183)를 남기므로, 지우지 않으면 뒤의 무관한 실패 이유로 읽힌다.
- 알림 두 개는 **크로스 플랫폼**(교체 알림은 Windows 전용이지만 열기·쓰기 실패는 POSIX 에도 있다). POSIX 의 삭제 오류 라벨은 `errno`.

### 알림 전체 문구 (레인 간 계약, `XPE_ALERT_ERROR`; `clients/`·`gui/` 에서 이 접두사 매칭 0 — `XPE_WARN_` 접두사 grep 은 215 때 리더가 확인, 새 접두사 둘은 이번에 만든 것)

**열기 실패** (실측 예 1: 없는 디렉터리, 예 2: 임시 파일을 공유 없이 점유):
```
XPE_WARN_XCAL_TEMP_OPEN_FAILED: could not create the temporary file '<path>.tmp' for the calibration file '<path>' (errno 2: No such file or directory, Windows error 3). Nothing was written and the previous file, if any, is unchanged
XPE_WARN_XCAL_TEMP_OPEN_FAILED: could not create the temporary file '<path>.tmp' for the calibration file '<path>' (errno 13: Permission denied, Windows error 32). Nothing was written and the previous file, if any, is unchanged
```
**쓰기 실패 — 임시 파일을 지웠음** (실측: 바이트 범위 잠금으로 만든 `ERROR_LOCK_VIOLATION` 33):
```
XPE_WARN_XCAL_TEMP_WRITE_FAILED: writing the temporary file '<path>.tmp' for the calibration file '<path>' failed (errno 13: Permission denied, Windows error 33). The previous file, if any, is unchanged and the temporary file was removed
```
**쓰기 실패 — 임시 파일을 못 지움** (삭제 오류 32 실측):
```
XPE_WARN_XCAL_TEMP_WRITE_FAILED: writing the temporary file '<path>.tmp' for the calibration file '<path>' failed (errno 13: Permission denied, Windows error 33). The previous file, if any, is unchanged. The temporary file could NOT be removed (Windows error 32) and was left behind; the next save to this path replaces it once it is free
```
(`<path>` 는 실제 경로. 실측 원문은 `evidence/20_alert_texts.txt`. 이유 괄호가 비면 `the system reported no error code`.)

## 쓰기 실패를 만든 방법과 근거

- **택한 방법**: 다른 핸들이 임시 파일의 **첫 1 MiB 에 바이트 범위 잠금**(`LockFile`)을 잡는다 → 작성자의 `WriteFile`/flush 가 운영체제 수준에서 `ERROR_LOCK_VIOLATION`(33)으로 실패한다. 홀더가 `FILE_SHARE_DELETE` 를 허용하면 삭제는 되고(지웠음 갈래), 허용하지 않으면 삭제가 32 로 실패한다(남았음 갈래).
- **버린 방법**: 작은 볼륨·디스크 가득(VHD 마운트는 관리자 권한이 필요하고 CI 에서 보장되지 않음), 제품 코드 안의 시험 훅(훅을 시험하는 것이 되고 출하 코드에 시험 전용 분기가 들어감), 매우 큰 `payload_len`(버퍼 밖을 읽어 충돌).
- **실제 경로를 지난다는 증거**: 작성자는 보통의 `std::ofstream` 이고 시험이 건드리는 것은 파일뿐이다. 반증 p7(flush·close 검사 제거)이 이 두 시험을 빨갛게 만든다.

## 시험 (`test_xcal_replace_retry.cpp`, 신규 4건; 모두 옛 코드에서 먼저 빨강)

| 시험 | 단언 |
|---|---|
| `ATemporaryFileThatCannotBeCreatedIsReportedWithItsReason` | 없는 디렉터리 → `IO_FAILED`, `TEMP_OPEN_FAILED` 에 경로·`cal.xcal.tmp`·`No such file or directory`·`Nothing was written`, 교체·쓰기 알림 없음, 디렉터리가 만들어지지 않음 |
| `ATemporaryFileHeldExclusivelyCannotBeOpened…` | 임시 파일을 공유 0 으로 점유 → `IO_FAILED`, 열기 알림, 옛 목적지 불변; 놓은 뒤 저장 성공·알림 0 |
| `AWriteThatFailsHalfWayRemovesTheTemporaryFileAndSaysWhy` | 잠금 + 삭제 공유 → `IO_FAILED`, 쓰기 알림에 `Windows error 33`·`the temporary file was removed`, 교체 알림 없음, 옛 목적지 불변, **`.tmp` 없음**, 다음 저장 성공 |
| `AWriteThatFailsWhileTheTemporaryFileCannotBeDeleted…` | 잠금 + 삭제 공유 없음 → `.tmp` 가 남음(대조), 알림에 `could NOT be removed (Windows error 32)`·`was left behind`, "removed" 주장 없음; 놓은 뒤 저장 성공(남은 파일은 덮어써져 소비됨) |

### 반증 (한 번에 하나, 전체 빌드, `evidence/30_arm_p*.txt`; 마지막에 복원 + 다시 빌드)

| 손상 | 빨강이 된 시험 |
|---|---|
| p1 열기 알림 제거 | 열기 시험 **2건만** |
| p2 쓰기 알림 제거 | 쓰기 시험 **2건만** |
| p3 삭제하지 않고 "removed" 라고 알림 | 쓰기 시험 2건(`.tmp` 가 남음 / 거짓 문구) |
| p4 삭제 결과를 무시하고 늘 "removed" | 남았음 시험 **1건만** |
| p5 `flush` 뒤 `good()` 검사 제거 | **빨개지지 않음**(11건 통과) |
| p6 `close` 뒤 `fail()` 검사 제거 | **빨개지지 않음**(11건 통과) |
| p7 **둘 다** 제거 | 쓰기 시험 2건 |

p5·p6 이 안 터진 것은 **두 검사가 서로 가려서**이다: 시험이 만드는 실패는 flush 에서 드러나고 close 에서 다시 드러나므로 한쪽만 빼면 다른 쪽이 잡는다. 둘을 함께 빼야(p7) 터진다. 이를 "중복이니 지워도 된다"로 읽지 않는다 — 시험이 구별하지 못할 뿐, 실제로는 서로 다른 시점의 실패(flush 는 남은 버퍼, close 는 마지막 닫기)를 보는 방어다.

## `write_xcal_file_ex` 반환 경로 전수 (카드 4)

`grep -n "return XPE_ERR" xcal_writer.cpp` 의 `write_xcal_file_ex` 안 반환:

| 위치(이름) | 코드 | 알림 | 평가 |
|---|---|---|---|
| 인자 검사 3곳 (`path == nullptr`, `payload_len > 0 && payload == nullptr`, `config_json_len > 0 && config_json == nullptr`) | `INVALID_INPUT` | 없음 | **적절** — 입출력이 아니라 호출 오류이고 코드가 이유를 말한다 |
| 임시 파일 열기 실패 | `IO_FAILED` | **이제 있음**(`TEMP_OPEN_FAILED`) | 이번에 닫음 |
| 임시 파일 쓰기·flush·close 실패 | `IO_FAILED` | **이제 있음**(`TEMP_WRITE_FAILED`), `.tmp` 정리 | 이번에 닫음 |
| 교체(이동) 실패 | `IO_FAILED` | 있음(`REPLACE_FAILED`, 212b·212c) | 이전에 닫음 |
| `catch (const std::bad_alloc&)` | `OUT_OF_MEMORY` | 없음 | 코드가 이유를 말한다(할당 실패). 알림을 만들려면 또 할당이 필요해 일부러 두지 않았다 |
| `catch (...)` | `PROCESSING_FAILED` | 없음 | **알림 없는 포괄 오류** — 어떤 예외인지 알 수 없다. 이 함수 안에서 던질 수 있는 것은 `std::string`/`vector` 할당(`bad_alloc`, 위)과 `rle_encode`·`compute_sha256_two_parts` 의 예외 정도로 보이나 확인하지 않았다 |

즉 "알림 없는 `IO_FAILED`"는 알림 구성이 성공하는 한 남지 않았다(구성 자체가 실패하면 알림 없이 -9, QA-A-221c). 남은 `PROCESSING_FAILED` 포괄 경로는 입출력 실패가 아니며 이번 범위 밖이다.

## 검증

`evidence/verify/`: 빌드 `BUILD_EXIT=0`, preprocess 902 중 894 통과·8 건너뜀(기존), 셔플 894 동일, 할당 실패 60, common 69·12, ctest 총 1084, 공개 export 변화 0, 헤더 문서 0건, 프리셋 일치 12/12.

## 미검증 (Gaps)

- **`close()` 만 실패하는 경우**(flush 는 성공)를 만드는 시험은 없다(p6 이 안 터진 이유). 코드상 방어이고 시험으로 고정되지 않았다.
- 디스크가 실제로 가득 찬 경우(`ENOSPC`)는 만들지 못했다. 바이트 범위 잠금이 같은 코드 경로(스트림 쓰기 실패 → `capture_io_reason`)를 타지만 `errno` 값은 다르다.
- POSIX 는 이 환경에서 실행하지 않았다(컴파일되는 분기만; `strerror` 경로).
- `capture_io_reason` 이 읽는 `errno`·`GetLastError()` 가 스트림 실패 시점의 값임은 측정으로만(두 시험의 33/32/13/2/3) 확인했고 CRT 구현에 대한 보장은 아니다.
- 마지막 두 열기 실패 사례의 `errno 13` 이 사용자에게 "Permission denied" 로 보이는 것은 공유 위반(Windows 32)의 CRT 번역이다 — 알림이 Windows 오류를 함께 적어 구별된다.

## 잔여 위험

- 알림 두 개가 새 레인 간 계약이다(접두사 `XPE_WARN_XCAL_TEMP_OPEN_FAILED:`, `XPE_WARN_XCAL_TEMP_WRITE_FAILED:`).
