# QA-A-229c — Codex #94 보류 3건 (229 M5 ghost 핸들 등록표, #245)

등록표 자체(조회·등록·제거)의 판정은 문제 없다는 Codex 의 결론은 그대로다. 제품 코드 변경 0 — 시험 파일 주석·관찰용 프로브·공개 헤더 문장만 바꿨다.

## 1. 사용·파괴 경합은 지원 범위 밖이며 실제로 크래시한다 (보류 1)

**코드로 본 경합** (`ghost_correct.cpp`):

- `xpe_ghost_reset`(436행): 438행 `GhostCorrectorHandle::isValid(handle)` 가 등록표를 조회하고 **조회 잠금을 풀고 반환**한다. 그 뒤 440행 `std::lock_guard<std::mutex> lock(gh->mtx)` 가 핸들 자신의 뮤텍스를 잠그고 이어서 버퍼를 지운다.
- `xpe_ghost_correct`(352행): 355행 `isValid` 뒤 365행 `std::lock_guard<std::mutex> lock(gh->mtx)`, 이후 이력 버퍼를 읽고 쓴다.
- `xpe_ghost_destroy`(449행): 454행 `ghost_unregister(handle)` 로 등록표에서 빼는 데 성공한 호출자가 457행 `delete gh`.
- 조회(isValid)와 뮤텍스 잠금 사이에 다른 스레드의 destroy 가 끼면, 사용하는 쪽은 **해제된 핸들의 뮤텍스를 잠그고 해제된 버퍼에 쓴다**. 등록표는 "이 포인터가 한때 살아 있는 핸들이었는가"를 조회 순간에만 답하며, 그 뒤 핸들이 살아 있는 동안 붙잡아 두지 않는다. 그래서 이 경합은 등록표를 넣기 전과 같은 부류의 UAF 다 — 헤더가 "호출자가 막아야 한다"고 적은 그 경합이다.

**격리 프로세스에서 관측** (`evidence/race_probe_exit_codes.txt`): `GhostRegistry.DISABLED_UnsupportedUseRacingDestroyCanCrash`(스레드 4개가 한 핸들에 `xpe_ghost_reset` 을 반복하는 동안 주 스레드가 `xpe_ghost_destroy`, 3000라운드)를 새 프로세스에서 30번 실행했다: **30번 모두 비정상 종료**(종료 코드 139, 세그멘테이션 오류), 3000라운드를 끝까지 간 실행은 0번. 이 증거는 시험으로 고정하지 않았다 — 크래시는 지원하지 않는 사용의 허용된 결과이므로 통과·실패 판정이 아니라 관찰이다(프로브는 `DISABLED_` 이고 `--gtest_also_run_disabled_tests` 로만 돈다). 시험 파일 머리 주석에 지원 경계를 적었다:

| 지원함 | 지원 안 함 |
|---|---|
| 호출이 끝난 뒤의 destroy | 같은 핸들에서 호출이 진행·진입 중일 때의 destroy |
| destroy 뒤의 호출(직렬 — `INVALID_INPUT` 로 거부) | (그 경합에서의 안전한 오류 반환은 약속하지 않으므로 시험하지 않음) |
| 같은 핸들의 중복·동시 destroy(정확히 하나만 해제) | |
| 서로 다른 핸들의 create / 사용 / destroy 를 여러 스레드에서 | |

기존 시험이 이 경계 안쪽을 이미 고정한다: `UseAfterDestroyIsRefused`(직렬), `DestroyingTwiceIsHarmless`, `ConcurrentDestroyOfOneHandleFreesItOnce`, `CreateUseDestroyFromManyThreadsKeepsTheRegistryIntact`(서로 다른 핸들).

공개 헤더 `xpe_ghost_destroy` 문서에 "NOT SUPPORTED" 항목을 넣어 위 메커니즘(조회 후 잠금 풀림 → 뮤텍스 잠금)과 결과(크래시·메모리 손상 가능, 안전한 오류 반환을 약속하지 않음)를 적었다.

## 2. 파이프라인 1회 비용: 구/신 교대 측정 (보류 2)

**방법**: 같은 시험 실행 파일(`xpe_preprocess_tests.exe`)에 DLL 만 바꿔 끼웠다. 신 DLL = 현재 코드, 구 DLL = 현재 코드에서 등록표만 되돌린 것(`isValid` 가 `magic` 을 읽고, create 는 등록하지 않고, destroy 는 `isValid` 후 해제 — 229 M5 이전의 동작). 구 DLL 이 정말 구 동작인지는 **대조**로 확인했다: 같은 실행 파일로 `AForgedHandleCarryingTheMagicIsRefused` 를 돌리면 구 DLL 에서는 스택 객체를 `delete` 해 힙 손상으로 종료(종료 코드 3221226356), 신 DLL 에서는 통과(0).
측정 대상 `GhostRegistry.DISABLED_PipelineOnceForAB`: 3072×3072 에 offset·gain·defect 를 `xpe_calib_state_load` 로 적재하고 보정되는 ghost 핸들을 만든 뒤 `xpe_preprocess_pipeline_ex` 를 9번 부르고(첫 번은 예열로 버림) 8번의 지연을 낸다. **각 호출이 `XPE_OK` 를 반환해야 기록된다**(아니면 시험이 중단). 구 → 신, 신 → 구 순서를 번갈아 10라운드, 각 프로세스는 새로 시작한다.

**결과** (ms/호출, 각 80개; `evidence/ab_pipeline_once_*.txt`):

| | 최소 | 1사분위 | 중앙값 | 3사분위 | 최대 | 평균 | 표준편차 |
|---|---|---|---|---|---|---|---|
| 구 | 116.88 | 120.35 | 122.34 | 128.43 | 146.62 | 125.41 | 7.32 |
| 신 | 117.66 | 120.65 | 122.85 | 127.65 | 158.20 | 125.22 | 7.20 |

중앙값 차이 신−구 **+0.51 ms**, 평균 차이 −0.18 ms. 라운드별 중앙값 짝 차이(신−구): −17.9 ~ +6.2 ms, 중앙값 +0.80, 신이 느린 라운드 6/10(`ab_pipeline_once_paired.txt`). 즉 차이는 분포의 흔들림(표준편차 약 7 ms) 안에 묻혀 부호도 일정하지 않다. 1회 호출에 검사가 1번이라는 점에서 이론값은 약 14 ns(0.000014 ms)이고, 측정이 그것을 분해하지 못한다. 결론: **이 방법으로 분해되는 비용은 없다** — 신이 구보다 느린지 빠른지조차 라운드마다 갈린다. 신뢰구간은 계산하지 않았고, 라운드별 짝 차이가 -17.9~+6.2 ms 로 흔들리므로 그보다 작은 차이(이론값 0.000014 ms 포함)는 구분되지 않는다. 보조 추정치: 229 M5 보고서의 `xpe_ghost_reset` 200만 회 측정(구 23~24 ns, 신 36~40 ns, 약 +14 ns)은 호출 한 번의 검사 비용에 대한 보조 추정으로만 쓴다.

## 3. 헤더 문장 (보류 3)

`xpe_ghost_destroy` 문서(`preprocess_api.h`)의 "every ghost function refuses it (XPE_ERR_INVALID_INPUT)" 를 반환형별로 풀었다:

- `xpe_ghost_correct`·`xpe_ghost_reset`: `XPE_ERR_INVALID_INPUT`
- `xpe_ghost_destroy`: 무동작(두 번째 destroy, 동시 destroy 의 패자 포함 — 정확히 한 호출이 해제)
- 모듈 내부 `xpe_ghost_is_calibrated`(비공개): `false`

doxygen 1.12.0 종료 0·경고 0.

## 4. 검증

전체 `xpe_preprocess_tests` 1002 통과·8 건너뜀·종료 0(DISABLED 42 — 프로브 2개 추가, 삭제한 성능 시험 1개 감소 후), `xpe_preprocess_oom_tests` 73 통과. 증거: `evidence/full_run.txt`, `oom_run.txt`, `doxygen_run.txt`.

## 5. 미검증 (Gaps) · 잔여 위험

- 크래시 관측은 한 기계(i7-12700)·한 빌드 구성(RelWithDebInfo, MSVC)·4 스레드에서의 30회다. 크래시 확률은 구성에 따라 다를 수 있으며, "경합이 항상 크래시한다"는 주장이 아니라 "크래시할 수 있고 이 구성에서는 30/30"이다. 크래시 지점(해제된 뮤텍스 잠금인지 버퍼 쓰기인지)은 덤프를 열어 확인하지 않았다 — 코드 읽기로 두 후보를 적었다.
- A/B 는 한 구성·한 기계에서의 10라운드이고, 구 DLL 은 "현재 코드에서 등록표만 되돌린 것"이지 실제 이전 커밋의 빌드가 아니다(그 사이 다른 변경이 섞이지 않게 한 의도적 선택이며, 대조 시험으로 구 동작임을 확인).
- 파이프라인 1회 측정은 3072² 한 크기·한 보정 조합이다. ghost 티어 2·3, 비닝이 켜진 경우, 다중 스레드 동시 호출(핸들 하나를 공유하는 스레드들의 등록표 잠금 경합)은 측정하지 않았다.
- 프로브 `DISABLED_PipelineOnceForAB` 는 실행 파일을 둘 두고 DLL 을 바꿔 끼워 돌리는 수동 절차에서만 의미가 있다(스크립트는 저장소에 넣지 않았다: 구 DLL 을 만들려면 등록표만 되돌리는 임시 편집이 필요하다). 재현이 필요하면 이 보고서의 방법 절을 따른다.
