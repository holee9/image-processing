# QA-A-219 — #232 마감 대조: 2026-10-01 사용자 결정 3건이 코드에 들어갔나 (Refs #232)

기준: dev/preprocess `7e2da41b`(QA-A-218b). main `7bc3c7e8`. 증거: `evidence/`(전체 검증 `verify/`, 시험 `10_`, 잠금 반증 `30_`, 재현률 `20_`·`21_`).

## 결론 — #232 를 닫을 수 있다

세 결정 모두 **이미 코드에 들어가 있다.** 커밋 `fd01fd7c`("#232 결정 반영 — ghost 핸들별 뮤텍스, 맵 치수 불일치 INVALID_INPUT, 041 미구현 주석", **QA-A-185**, 2026-10-01)가 셋을 한 번에 넣었고, 이 커밋은 main 과 dev/preprocess 양쪽의 조상이다(`git merge-base --is-ancestor` 둘 다 참). 그 커밋에는 `evidence/` 40여 개와 `verdict.md` 가 딸려 있어 "확인한 기록이 없다"는 것은 사실과 다르다 — 다만 #232 에 그 결과를 알리는 코멘트가 없었을 뿐이다. 미구현 항목은 **없음**. 이 카드에서 새로 한 것은 217 이후의 잠금 경로 시험 1건(아래)이고, 제품 코드는 바꾸지 않았다.

## 1. 항목별 대조 (main / dev/preprocess 동일)

| 항목 | 코드 위치(이름·`grep -n`) | 고정하는 시험(`grep` 확인) | 들어간 커밋 |
|---|---|---|---|
| **REQ-P1A-021** 맵≠입력 4곳 `INVALID_INPUT`, 출력 불일치는 `BUFFER_TOO_SMALL` 유지 | `offset_correct.cpp:184-185` (`calib.offset_width/height != input`), `gain_correct.cpp:311-313` (스칼라 게인)·`:327-329` (다항식 게인), `defect_correct.cpp:278-279` (main: 각각 `:184`·`:310`·`:326`·`:180`). 출력 쪽은 그대로 `BUFFER_TOO_SMALL`: `offset_correct.cpp:159`·`gain_correct.cpp:253`·`defect_correct.cpp:229` | `CalibDimMismatch.OffsetMapDifferentFromInputReturnsInvalidInput`, `.GainMapDifferentFromInputReturnsInvalidInput`, `.GainPolynomialMapDifferentFromInputReturnsInvalidInput`, `.DefectMapDifferentFromInputReturnsInvalidInput` (`test_calib_dim_mismatch.cpp:74·86·99·114`); 출력 쪽 4건은 기존 시험이 그대로 통과 | `fd01fd7c` (gain 두 곳의 `return XPE_ERR_INVALID_INPUT` 줄은 blame 이 이 커밋; offset·defect 의 비교 줄은 이후 `f2ca59d74` 에서 줄이 다시 쓰여 blame 이 그쪽을 가리키지만 `fd01fd7c` 의 통계에 네 파일이 모두 있고 시험 4건이 4곳을 고정) |
| **REQ-P1A-041** 선 잡음 미구현 명기, 밝은 행 검사임을 문서화, 필드 이름 유지 | SPEC 본문 주석(main·HEAD `spec.md` REQ-P1A-041 아래, `선 잡음 검출은 미구현`); 헤더 `preprocess_api.h:866` (`@param has_nonuniform_gain … any row mean > 0.9 * UINT16_MAX (a bright-row …`); 소스 `readout_validate.cpp:42-45` (`BRIGHT-ROW check`, `QA-A-184 measured it unset…`). 필드 이름 `has_nonuniform_gain` 유지(헤더·소스·`:60`). C#·GUI 의 사용: `grep` 0건 | 코드 변경 없음(주석만) — `QA-A-185/verdict.md` 에 "주석만 변경" 증명 | `fd01fd7c` (주석), SPEC 주석은 `35486840` |
| **SRS-CALIB-NFR-003** 핸들별 뮤텍스 | `xpe_preprocess_internal.h:82` (`std::mutex mtx`), `ghost_correct.cpp:245` (`xpe_ghost_correct` 의 `lock_guard`), `:320` (`xpe_ghost_reset`) | `GhostThreadSafety.SharedHandleLosesNoUpdates`, `.ControlExternalMutexMatchesSerialReplay`, `.ControlOneHandlePerThreadMatchesSerialReplay`, `.ResetAndCorrectOnOneHandleEndInASerialisedState` (`test_ghost_thread_safety.cpp:135·143·150·187`) | `fd01fd7c` |

021 의 C#·GUI `-8` 분기 재확인: `grep -rnE "BufferTooSmall|BUFFER_TOO_SMALL|== *-8|case -8" --include=*.cs clients gui` → `BUFFER_TOO_SMALL = -8` 열거형 정의(`PInvokeWrapper.cs:43`)와 **알림 큐 버퍼·`dataSize` 계약 시험**(`NativeErrorTranslationTests`·`AlertQueueJunctionTests`·`DataSizeContractTests`)뿐이고, **보정 단계의 맵 불일치를 `-8` 로 분기하는 곳은 없다**(184 실측과 같음).

## 2. 뮤텍스 재측정 (현재 = 217·218b 가 들어간 빌드)

QA-A-185 당시 수치: 잠금 전 60/60 빨강(1코어 30/30), 잠금 후 60 호출 중 0 빨강.

- **잠금 있음(현재)**: `GhostThreadSafety.*` 5건 × **60 반복 = 60/60 통과**(`evidence/20_rate_normal.txt`, 종료 코드 0). `start /affinity 1` 로 한 코어에 고정한 30 반복도 **30/30 통과**(`21_rate_onecore.txt`). 갱신 유실 재현 0 — 단 코어 고정이 실제로 걸렸는지는 독립적으로 관측하지 못했다(Gaps).
- **217 경로가 잠금 안에 있는가 — 코드**: `lock_guard`(`:245`) 아래에 입구 비유한 검사, `backup` 복사, 티어 호출, 실패 시 복원, 이력 `swap`·시각·노출 커밋이 전부 있다(같은 함수, `:245` 이후). `xpe_ghost_reset`(`:320`)도 잠금 안.
- **217 경로가 잠금 안에 있는가 — 시험(신규 1건)**: `backup` 평면은 핸들당 하나라 잠금 밖에서 두 호출이 겹치면 한 스레드의 복원이 다른 스레드의 프레임을 자기 버퍼에 복사할 수 있다. `GhostThreadSafety.FailedFramesGetTheirOwnPixelsBackWhileSharingAHandle` 는 시드 프레임(3e38) 뒤 두 스레드가 서로 다른 값(2.5e38 / 2.4e38)의 **반드시 실패하는** 프레임을 각 2000 번 넘기고, 모든 호출이 `PROCESSING_FAILED` + **자기 화소 불변**이며 끝 이력이 시드 그대로임을 20 회 확인한다(통과, 21 ms).
- **반증**(`evidence/30_arm_no_lock_in_correct.txt`, `xpe_ghost_correct` 의 `lock_guard` 만 제거, 프로세스 30개씩): `SharedHandleLosesNoUpdates` **30/30 비초록**, `FailedFramesGetTheirOwnPixels…`(신규) **30/30 비초록**, `ResetAndCorrect…` **30/30 비초록**. 주의: 비초록 중 상당수가 **판정 없이 프로세스가 죽는 크래시**였다(20/30, 0/30, 30/30) — 잠금이 없으면 갱신 유실을 넘어 벡터 `swap` 경합으로 메모리가 깨진다. QA-A-185 의 "빨강 수"는 `FAILED` 줄만 셌다면 이 크래시를 놓쳤을 수 있다(그때의 집계 방식은 이 카드에서 확인하지 않았다). 복원 후 다시 빌드해 5건 통과 확인.

## 3. 전체 검증 (`evidence/verify/`)

빌드 `BUILD_EXIT=0`, preprocess 896 중 888 통과·8 건너뜀(기존), 셔플 888 동일, 할당 실패 60, common 69·12, ctest 총 1078, 공개 export 변화 0(common 16), 헤더 문서 0건, 프리셋 일치 12/12.

## 4. #232 의 나머지 항목

- `REQ-P1A-066`(결정 4번째): 시험 이름 유지, VVP 94행에 "요구 추적이 아님" 표시 — 확인(`VVP-PREPROCESS-001.md:94` 의 `REQ-P1A-066 ⚠` 행, `c88294cd`).
- 이슈 본문의 추가 2건 중 066 은 위와 같고, NFR-003 은 위 표.

## 미검증 (Gaps)

- `start /affinity 1` 이 실제로 시험 프로세스를 한 코어에 묶었는지(작업 관리자·`GetProcessAffinityMask` 로) 관측하지 않았다 — QA-A-185 가 같은 방법을 썼다는 것에만 기댄다.
- 021 의 "시험 변경 0(실측)" 주장(출력 쪽 4건 그대로 통과)은 이번에 다시 돌려 보지 않았다 — 지금 전체 시험이 통과한다는 사실로 갈음.
- 021·041 의 SPEC 본문 주석이 사용자 승인 문구와 일치하는지는 위치만 확인했고 본문 전체를 대조하지 않았다.
- 한 호출의 잠금 비용(QA-A-185 가 "분리되지 않음"으로 기록)은 이 카드에서도 재지 않았다.

## 잔여 위험

- 없음(#232 의 결정 사항 기준). 단 잠금 제거 시 크래시가 난다는 점은 앞으로 핸들 구조를 바꿀 때(예: `backup` 평면 분리) 잠금 범위를 같이 봐야 한다는 뜻이다.
