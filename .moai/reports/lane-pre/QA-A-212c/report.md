# QA-A-212c — 교체 실패 뒤 임시 파일 삭제를 확인하고 사실대로 알린다 (Codex #74 보류 1건, Refs #233)

기준: dev/preprocess `1a3d5885`(QA-A-220) 위. 코드: `xcal_writer.cpp`, 시험 `test_xcal_replace_retry.cpp`(신규 2건 + 기존 1건 강화). 증거: `evidence/`. 푸시하지 않음.

## 결론

1. 교체가 끝내 실패하면 임시 파일 삭제를 **한 번 시도하고 결과를 확인**해, 지워졌으면 지금 문구 그대로, **못 지웠으면 남은 경로와 삭제의 Windows 오류**를 알린다. 임시 파일이 다른 프로세스에 `FILE_SHARE_DELETE` 없이 열려 있으면 삭제 오류는 **32** 로 실측된다.
2. **남은 임시 파일은 다음 저장을 막지 않는다** — 점유를 놓은 뒤 같은 목적지로 다시 저장하면 성공하고(새 파일), 남았던 `.tmp` 는 교체 이동에 소비되어 사라지며, 알림은 없다. 이유: 임시 파일 이름 규칙과 여는 방식이 아래와 같아 남은 것을 덮어쓴다(시험으로 고정).
3. 같은 줄을 고치다 **원인 문구도 부정확함**을 발견해 함께 고쳤다: 32 는 "목적지"뿐 아니라 "임시 파일"이 열려 있을 때도 난다(QA-A-212 실측). 문구가 목적지만 말하고 있었다.

## 임시 파일 이름 규칙과 여는 방식 (확인한 것)

- 이름: `std::string(path) + ".tmp"` — 목적지마다 고정된 하나(`xcal_writer.cpp`, "Build tmp path").
- 열기: `std::ofstream(tmp_path, std::ios::binary | std::ios::trunc)` — 있으면 잘라서 덮어쓴다. 남은 `.tmp` 가 닫혀 있으면 막지 않는다.
- 그래서 "남은 `.tmp` 를 덮어쓰거나 새 이름을 쓴다"의 앞쪽 — 덮어쓴다 — 가 이미 성립했고, 새 이름 쓰기는 필요하지 않다.

## 변경 (`xcal_writer.cpp`)

- `remove_tmp()` 신설: Windows 는 `DeleteFileA`(오류 `GetLastError()`), POSIX 는 `std::remove`(`errno`). `ERROR_FILE_NOT_FOUND`/`ERROR_PATH_NOT_FOUND`(POSIX `ENOENT`)는 "원래 없었음" = 지워진 것으로 본다. **재시도하지 않는다**(점유 중 파일은 기다려도 안 지워진다).
- 최종 실패 경로: 이전엔 `std::remove(tmp_path)` 반환값을 버렸다. 이제 `const TmpCleanup cleanup = remove_tmp(tmp_path)` 를 `report_replace_failure()` 에 넘긴다. POSIX 의 알림은 이전처럼 없다(동작 불변).
- 알림의 맺음말이 삭제 결과에 따라 갈린다. 원인 문구를 "destination" → "destination or the temporary file" 로 정정.

### 알림 전체 문구 (레인 간 계약, `XPE_ALERT_ERROR`; `clients/`·`gui/` 에서 이 접두사를 매칭하는 곳은 없음 — grep)

공통 앞부분(`<cause>` 는 32·5 일 때 첫째, 그 외 둘째):
```
XPE_WARN_XCAL_REPLACE_FAILED: could not replace '<path>' with the newly written calibration file: Windows error <E> after <R> retries (<cause>).
```
- `<cause>`(재시도 가능한 오류): `the destination or the temporary file stayed open in another process, or the destination is read-only or may not be changed`
- `<cause>`(그 밖의 오류): `not a transient condition, so it was not retried`

**갈래 1 — 임시 파일이 지워졌음** (바이트 단위로 이전 문구와 같은 맺음말):
```
 The previous file, if any, is unchanged and the temporary file was removed
```
**갈래 2 — 임시 파일을 못 지움** (신규; 실측 예: 삭제 오류 32):
```
 The previous file, if any, is unchanged. The temporary file '<path>.tmp' could NOT be removed (Windows error <D>) and was left behind; the next save to this path replaces it once it is free
```

## 시험 (`test_xcal_replace_retry.cpp`)

| 시험 | 단언 |
|---|---|
| **신규** `ATemporaryFileHeldElsewhereIsReportedAsLeftBehindAndTheNextSaveStillWorks` | `.tmp` 를 `FILE_SHARE_DELETE` 없이 점유(홀더 클래스 `TempHolder`) → 저장 `IO_FAILED`, **옛 목적지 불변**(게인 2.0), 대조: `.tmp` 가 실제로 남음, 알림이 `could NOT be removed (Windows error 32)`·`cal.xcal.tmp`·`was left behind`·"destination or the temporary file" 를 담고 **"was removed" 는 없음**. 점유를 놓고 다시 저장 → `XPE_OK`, 게인 7.0, `.tmp` 소비됨, 알림 0 |
| **신규** `AStaleTemporaryFileFromEarlierIsOverwrittenBySave` | 아무도 안 잡은 옛 `.tmp`(쓰레기 바이트) 가 있어도 저장 성공, 새 파일, `.tmp` 없음, 알림 0 |
| **강화** `ADestinationHeldPastTheBudgetFails…` | 목적지가 점유된 경우(임시 파일은 지워짐): 알림이 `the temporary file was removed` 를 말하고 `could NOT be removed` 는 없음 — 갈래 1 고정 |

### 반증 (한 번에 하나, 전체 빌드, `evidence/30_arm_n*.txt`; 마지막에 복원 + 다시 빌드)

| 손상 | 빨강이 된 시험 |
|---|---|
| n1 결과 무시하고 늘 "removed" (옛 동작) | 신규 임시-점유 시험 **1건만** |
| n2 늘 "left behind" | 목적지-점유 시험 **1건만**(강화한 단언) |
| n3 삭제를 시도하지 않음 | 4건: 목적지-점유·읽기 전용·임시-점유·공개 저장(모두 "임시 파일이 안 남는다" 단언) |
| n4 삭제 실패를 지워진 것으로 셈 | 신규 임시-점유 시험 **1건만** |

**순서에 대한 정직한 기록**: 시험을 코드보다 먼저 빨갛게 보이지는 못했다(코드와 시험을 함께 써서 처음부터 초록). 대신 n1(옛 동작과 같은 거짓말)이 신규 시험만 빨갛게 만들어, 시험이 이 결함을 잡는다는 것은 확인했다. 삭제 오류가 32 임은 시험 단언이 통과해 실측으로 고정됐다.

## 검증

`evidence/verify/`(반증 뒤 다시 빌드): 빌드 `BUILD_EXIT=0`, preprocess 898 중 890 통과·8 건너뜀(기존), 셔플 890 동일, 할당 실패 60, common 69·12, ctest 총 1080, 공개 export 변화 0, 헤더 문서 0건, 프리셋 일치 12/12.

## 미검증 (Gaps) — 그리고 눈에 띈 것

- **임시 파일을 열지 못하면(`!f.is_open()`) 알림 없이 `IO_FAILED`** 가 된다 — 코드 읽기로만 확인(`xcal_writer.cpp` "Write to tmp file" 의 첫 검사), 측정하지 않았다. 점유자가 쓰기 공유까지 막으면(share 0) 다음 저장이 이 경로로 조용히 실패할 것이다. 이번 카드의 범위가 아니다(교체 실패 알림만).
- 같은 이유로, 헤더/데이터 쓰기 중 실패(`f.good()` 거짓, 디스크 가득 등)에서는 `.tmp` 가 남는 채로 반환된다 — 이것도 코드 읽기, 측정 안 함.
- 점유가 길게 이어지는 동안(놓기 전) 다음 저장은 점유자의 공유 모드에 따라 달라진다. 시험의 점유자는 읽기·쓰기 공유(삭제 공유 없음)이고, 이 경우 임시 파일 열기는 되고 이동이 다시 32 로 실패한다 — 그 두 번째 실패를 별도로 시험하지는 않았다.
- POSIX 경로는 이 환경에서 실행하지 않았다(컴파일되는 분기만 확인, 알림은 원래 없음).

## 잔여 위험

- 알림 문구 변경(원인 문구와 갈래 2)은 레인 간 계약이다. 접두사 `XPE_WARN_XCAL_REPLACE_FAILED:` 는 그대로다.
