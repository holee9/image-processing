# QA-A-185 — #232 사용자 결정 3건의 코드 변경

시작 HEAD `35486840` (`evidence/00_head.txt`, 리더의 로컬 main 을 `git merge --ff-only main` 으로 받음). 결정 출처: #232 · 변경 이력 §6-1 · SPEC 주석 35486840.

## 한눈에

| 항목 | 결정 | 변경 | 시험 |
|---|---|---|---|
| ③ NFR-003 | ghost 핸들별 뮤텍스 | `GhostCorrectorHandle::mtx`, `xpe_ghost_correct`·`xpe_ghost_reset` 에서 잠금 | `GhostThreadSafety.*` 3건 신규 (상시) |
| ① REQ-P1A-021 | 맵≠입력 4곳만 INVALID_INPUT, 출력≠입력은 BUFFER_TOO_SMALL 유지 | offset:186, gain:309·327, defect:179 | `CalibDimMismatch.*` 4건 신규, 기존 출력 쪽 4건 그대로 통과 |
| ② REQ-P1A-041 | 선 잡음 미구현 명기, 이름 유지 | `readout_validate.cpp`·`preprocess_api.h` 주석만 | 주석만 변경 증명 (아래) |

전체 시험 1회: **728 실행, 720 통과, 8 건너뜀, 0 실패** (종료 코드 0, `evidence/27_full_suite_last.txt`). 직전(QA-A-184 기준선) 713 + 신규 7 = 720. 셔플 2종(시드 7, 12345) 종료 코드 0 (`23_*`). `ctest -N`: 총계 835, DISABLED 38, 실행 797 (이전 826/36 → 프로브 정리와 신규 7건 반영). 프리셋 점검 OK (`20_preset.txt`). dumpbin 내보내기 이름 53 = 53, 차이 없음 (`17_exports_after.txt` 와 QA-A-183 `04_exports_after.txt` 비교).

## ③ NFR-003 — 핸들별 뮤텍스 (TDD)

**빨강 먼저.** 수정 전 코드에서 `GhostThreadSafety.SharedHandleLosesNoUpdates` (QA-A-184 프로브를 상시 시험으로 올림: 16×16, τ1=τ2=1e12, 스레드당 2000 호출, 20 회):

- 일반 실행: 60/60 호출에서 빨강 (`02_red_run*.txt`, `03_red_rate_normal.txt`)
- 한 코어에 고정(`start /affinity 1`): 30/30 빨강, 호출당 20 회 중 1~8 회가 직렬 재생과 다름 (`03_red_rate_onecore.txt`). 1 코어 CI 러너에서도 재현률 100% 라는 뜻.
- 대조군 2건(`ControlExternalMutexMatchesSerialReplay`, `ControlOneHandlePerThreadMatchesSerialReplay`)은 수정 전에도 초록 — 방법이 스스로 실패하지 않는다는 증거.
- 시험 시간: 한 호출 약 0.1 초 (CI 부담 작음).

**초록.** 뮤텍스 추가 후 같은 시험 60 호출 중 0 빨강 (`06_green_rate.txt`), 최종 빌드에서도 7개 신규 시험 60 호출 중 0 빨강 (`24_final_rate.txt`).

**반증 팔.** `xpe_ghost_correct` 의 잠금만 제거한 빌드: `SharedHandleLosesNoUpdates` 30/30 호출 빨강 (`07_arm_rate.txt`). 복원 후 잠금 2곳 확인.

**성능** (`15_perf_ab.txt`): 뮤텍스 없는 빌드(old)와 있는 빌드(new)의 단일 스레드 `xpe_ghost_correct` 한 호출 중앙값을 6 라운드 번갈아 측정(라운드마다 old→new).

| 크기 | old 중앙값 범위 (ms) | new 중앙값 범위 (ms) |
|---|---|---|
| 64×64 | 0.0078 ~ 0.0083 | 0.0079 ~ 0.0083 |
| 1024×1024 | 2.074 ~ 2.143 | 2.067 ~ 2.121 |

차이는 라운드 간 흩어짐 안쪽이다(1024²: new 가 6 라운드 중 5 라운드에서 같거나 약간 낮음). 한 호출의 잠금 비용은 이 측정으로 분리되지 않았다. old 빌드는 `mtx` 멤버는 있고 잠금만 뺀 것이다(구조체 크기 차이는 이 측정에 포함 안 됨).

## ① REQ-P1A-021 — 맵≠입력만 INVALID_INPUT

- **빨강 먼저**: 수정 전 `CalibDimMismatch.*` 4건 전부 실패, 값 -8 (`09_021_red.txt`).
- **초록**: 4곳 수정 후 4건 통과, 출력 버퍼는 센티널 그대로 (시험이 단언).
- **기존 출력 쪽 4건 그대로 통과**: `OffsetCorrectTest`/`GainCorrectTest`/`DefectCorrectTest.DimensionMismatchReturnsError`, `GainCorrectReciprocalFMATest.DimensionMismatchReturnsInvalidInput` 및 `||` 로 받던 4건 (`11_021_green.txt`, 전체 시험).
- **반증 팔 4개** (각 사이트를 하나씩 옛 코드로 되돌림, `12_arms021_summary.txt`): offset:186 → `OffsetMap…` 만, gain:309 → `GainPolynomialMap…` 만, gain:327 → `GainMap…` 만, defect:179 → `DefectMap…` 만 빨강. 각 팔에서 자기 시험 하나만 빨개져 사이트와 시험이 1:1 이다.
- 공개 헤더 반환 코드 설명(`preprocess_api.h` offset/gain/defect 3곳)을 새 동작에 맞게 고침. 호출자(pipeline, GUI, clients)는 QA-A-184 에서 -8 분기 없음을 확인했고, 이번에도 전체 시험이 통과.

## ② REQ-P1A-041 — 주석만 변경

`readout_validate.cpp` 주석과 `preprocess_api.h` 의 `@param has_nonuniform_gain` 설명에 "밝은 행 검사, 선 잡음 검출 아님, 미구현(#232)"을 적음. 이름은 그대로. `25_comment_only_check.txt`: 두 파일 모두 주석을 지운 뒤 HEAD 와 동일, 대조군(코드 토큰 한 개 변경)은 감지됨.

## 프로브 정리

`test_zz_a184_probes.cpp` 에서 승격·대체된 3건(`SharedGhostHandleTwoThreads`, `SharedGhostHandleDefaultTaus`, `DimensionMismatchReturnCodes`)을 지웠다. 남긴 것: `LineNoiseVersusTheReadoutCheck` (REQ-P1A-041 미구현 근거 측정)와 새로 둔 `GhostSingleThreadTime` (성능 비교 프로브, 위 표). 둘 다 `DISABLED_`.

## 중간에 잡은 것

- 새 다항식 시험이 알림 1건을 남겨 다음 시험의 누수 검사에 걸림 (전체 실행 종료 코드 1, "0 FAILED"로 보임). `TearDown` 에서 `xpe_clear_alerts()` 로 해결, 전체 종료 코드 0 확인 (`18b_*` 가 문제, `22_*` 가 해결 후).
- 성능용 "뮤텍스 없는 빌드"를 처음 만들 때 CRLF 때문에 치환이 실패해 잠금이 남은 빌드를 old 로 복사했다가 폐기함. 잠금 수(`grep -c lock_guard` = 0)를 확인하고 다시 만듦.

## Gaps / Residual-risk

- 미검증: `xpe_ghost_reset` 의 잠금은 전용 시험이 없다(코드로만 추가). `xpe_ghost_destroy` 와 다른 호출의 동시성은 의도적으로 범위 밖(헤더 주석에 명시). 스레드 3개 이상, 서로 다른 해상도·tier 2/3 조합은 시험 안 함.
- 미검증: CI 구성(Mock/Native 백엔드 축)에서의 실행. 신규 시험은 백엔드와 무관한 순수 모듈 시험이라 구성 축 의존은 없다고 보지만 CI 로그로는 아직 관측 안 함.
- 잔여 위험: 경합 시험의 빨강 재현률은 이 머신 기준이다(1 코어 고정에서도 100% 였음). 반대로 초록 쪽은 "경합이 안 일어나서" 통과할 수 있으나, 대조군과 반증 팔이 같은 방법으로 빨강을 낸다는 점이 그 해석을 막는다.
- 성능: 잠금 비용은 측정 해상도 아래. 많은 스레드가 한 핸들을 두고 경쟁하는 경우의 대기 시간은 측정 안 함(제품 호출자는 아직 없음).

---

## 후속 (Codex #14 보류 사유 반영, 같은 카드)

**① 공개 헤더 계약 (주석만).** `preprocess_api.h` 의 `xpe_ghost_create` 가 "Handle is NOT thread-safe; do not share across threads" 라고 적고 있어 새 계약과 충돌했다. 고친 곳 4곳: create 의 `@note`(핸들 하나를 여러 스레드가 공유 가능, correct/reset 은 핸들 안에서 직렬화, 다른 핸들끼리는 막지 않음, 호출자가 같은 이미지 버퍼를 동기화 없이 동시에 만지는 것은 보장 밖), correct·reset 설명(서로 직렬화), destroy 설명(그 핸들의 진행 중·새로 시작하는 호출과 동시 실행 금지, 뮤텍스가 핸들과 함께 해제됨). 같은 낡은 문장이 `test_req_p1a_066.cpp` T4 주석에도 있어 함께 고침. 두 파일 모두 주석을 지운 뒤 HEAD 와 동일, 대조군 감지됨 (`38_comment_only_check.txt`). `modules/preprocess/src` diff 0줄.

**② reset/correct 교차 시험.** `GhostThreadSafety.ResetAndCorrectOnOneHandleEndInASerialisedState`: 한 스레드는 correct 400 회(프레임 값 1, τ=1e12 라 매 호출 모든 원소에 정확히 +1), 다른 스레드는 그동안 reset 을 계속 호출. 가능한 모든 직렬 순서의 끝 상태는 "hist1·hist2 의 모든 원소가 같은 정수 k (0~400)" 이다. 균일하지 않거나 hist1≠hist2 이거나 정수가 아니면 직렬 순서로는 만들 수 없는 상태다. 64×64, 60 런.

- 초록(잠금 2곳 있음): 60 호출 중 0 빨강 (`37_reset_green_60runs.txt`), 시험 시간 약 0.26 초.
- 반증 팔(reset 의 잠금만 제거): 20 런 판은 39/40 호출 빨강(`33_reset_arm.txt`, 첫 실패 "hist1 is not uniform at element 4092"), 런을 60 으로 늘린 판은 60/60 빨강 (`35_reset_arm_60runs.txt`). 복원 후 잠금 2곳 확인.

전체 시험 1회: 729 실행, 721 통과, 8 건너뜀, 0 실패(종료 0), 셔플(시드 99) 종료 0. `ctest -N` 총계 836, DISABLED 38. 프리셋 점검 OK. 미검증: reset 팔은 correct 쪽 잠금이 있는 상태에서의 reset 잠금만 본다(둘 다 없는 경우는 앞의 `SharedHandleLosesNoUpdates` 가 본다).
