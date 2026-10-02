# GUI-C-192d — 한 번도 확인되지 못한 상태도 알린다

## 1. 주장 (Claim)

1. 읽기가 시작부터(또는 `Reset` 뒤부터) 15초 동안 돌아오지 않으면 192c 와 같은 자리에 알림이 뜬다. 문구가 "AI 세션 시작·재시작 뒤 한 번도 확인되지 못함"으로 원인을 구별한다.
2. AI 를 요청하지 않은 상태, 첫 읽기가 15초 안에 돌아온 경우에는 알림이 없다.
3. 최소 너비 1280 px 에서도 알림이 자동화 트리에 있다.
4. Native CI 에서 리더가 판정할 측정값이 로그에 찍힌다.

## 2. 증거 (Evidence)

**2-1. 상태 모델은 늘리지 않았다(카드 2번).** 192c 의 `Unconfirmed` 에 원인 표시 `NeverConfirmed`(bool, `AiWorkerStatus` 레코드의 끝 인자, 기본 false)만 더했다. 상태 종류는 그대로이고 "무엇이 마지막으로 확인됐나"만 구별한다. 표시 조건·재읽기·해제는 192c 와 같은 코드 경로다.

**2-2. "AI 를 요청하지 않음"이 코드에서 무엇으로 판별되는가(카드 1번 요구).** 판별은 "읽기가 **돌아오는가**"다. 돌아온 읽기는 `Unknown` 이라도 답이다.
- Native 에서 AI 를 켠 적이 없으면 `AiSessionTracker.OwnStatus()` 가 `!_started ? AiWorkerStatus.Unknown` 을 돌려준다(`AiBoneSuppressionStage.cs` 153~156행). `GuiAiRunner.QueryWorkerState` 는 모듈에 묻기 전에 이 값을 먼저 답한다(`GuiAiRunner.cs` 161~165행). 게이트를 잡고 있는 AI 프레임이 없으니 즉시 돌아온다.
- AI 세션이 없는 백엔드(Mock)는 `ReadAiWorkerStatus` 가 즉시 `Unknown` 을 답한다(`MainWindowViewModel.cs`, `backend is IAiSessionBackend ? … : Unknown`).
- 돌아오지 않는 읽기는 세션 게이트(AI 프레임이나 재시작이 쥠)에 막힌 것이므로, AI 가 쓰이고 있다는 뜻이다.
그래서 규칙은 "읽기가 진행 중이고, 마지막 답·`Reset`·읽기 시작 중 가장 늦은 시각부터 상한이 지남"이다(`CheckFreshness`, `_shown == Unknown` 분기). 읽기가 돌아오면 알림은 `Unknown` 으로 사라진다(AI 가 알고 보니 안 쓰이던 경우).

**2-3. `Reset` 뒤의 대기.** `Reset` 은 `Unknown` 을 적용하며 마지막 답 시각을 현재로 되돌린다. 아직 막혀 있는 옛 읽기가 있어도 새 세션의 대기는 `Reset` 부터 센다(시험 `AfterAReset_TheNeverConfirmedWait_CountsFromTheReset`).

**2-4. 시험(결정적, 가짜 시계).** 단위 88/88:
- `AReadThatNeverReturnsFromTheStart_RaisesTheNeverConfirmedNotice_AtTheBound`: 상한 −1틱 알림 없음 → 상한에 알림(`NeverConfirmed`, 문구에 "since the AI session started") → 한 번만 → 읽기가 돌아오면 사라짐.
- `AFirstReadThatAnswersInsideTheBound_RaisesNoNotice` (Active·Unknown 두 케이스), `AnUnknownThatWasNeverActive_IsNeverTurnedIntoANotice`(AI 미요청: 1~2시간이 지나도 없음), `AfterAReset…`, `TheMeasurements_…`.
- E2E **M03**(`--automation-fault ai-worker-silent:0` 신설: 읽기가 한 번도 답하지 않음, 시험 빌드 전용): 5.4초 알림 없음 → 14.6초에 알림(문구 "since the AI session started …") → 최소 너비 1280 px 에서 배너·Restart AI 가 트리에 있음. 통과. M01·M02 도 통과(`e2e_m01_m02_m03.txt`).
- 결함 인자 `ai-worker-silent` 를 bool 에서 "답하는 횟수"(`int?`)로 일반화했다: `ai-worker-silent` = 1회 답하고 침묵(192c), `ai-worker-silent:0` = 처음부터 침묵. 정확한 철자만 받는다(시험 있음).
- 새 결함 이름은 Release dll·pdb·xml 에 없다(`release_byte_search.txt`, Debug 대조군은 발견). Functional 393 통과 / 0 실패 / 1 건너뜀, SelfCheck 종료 코드 0.

**2-5. 반증**(`falsification_arms.txt`, 규칙마다 제거 → 해당 시험 빨강, 원본 바이트 동일 복원, 복원 뒤 단위 88/88·M02+M03 통과):

| 지운 규칙 | 빨강이 된 시험 |
|---|---|
| 진행 중인 읽기가 상한을 넘기면 알림 | 단위 2건 + **M03(UI)** |
| 대기를 `Reset` 부터 셈 | 단위 `AfterAReset…` |
| "돌아온 읽기는 답" (`_inFlight` 조건) | 단위 2건 (AI 미요청 포함) |
| 가장 느린 읽기 집계 | 단위 `TheMeasurements…` |
| 진단 줄에 측정값 포함(VM) | **M02(UI)** (진단 줄 단언) |

## 2-6. Native CI 에서 리더가 볼 측정값 (카드 4번, C-09 방식)

C-09(실제 AI 호출이 도는 Native 시험)가 끝에 한 줄을 찍는다: `C09 status-refresher measurements: refresher: reads=… maxReadMs=… maxAnswerGapMs=… boundMs=15000 rereads=… noticesWithdrawn=… noticesNeverConfirmed=…`. 같은 줄이 시도마다 찍히는 진단 줄(`diagnostics='…| refresher: …'`)에도 들어 있다. 아무것도 단언하지 않는다(상한을 실측으로 정하려는 것이므로). 읽는 법:

| 값 | 뜻 | 판정 |
|---|---|---|
| `noticesWithdrawn`, `noticesNeverConfirmed` | 이 실행 중 알림이 뜬 횟수 | **정상 AI 처리(C-09)에서 0 이 아니면 15초 상한이 거짓 경보** → 상한을 키우거나 원인을 본다 |
| `maxAnswerGapMs` / `boundMs` | 화면의 답이 다음 답을 기다린 가장 긴 시간 | 15000 에 가까우면 거짓 경보 직전. 한참 아래(예: 6000 미만)면 상한은 안전 |
| `maxReadMs` | 가장 느린 상태 읽기(시작~답) | 게이트 대기의 실측. IPC 시간 제한 5000 ms 와 비교 |
| `reads`, `rereads` | 읽기 횟수, 그중 5초 주기 재읽기 | 처리를 늦추는지는 같은 시험의 사전 CI 시간과 비교. `rereads` 가 작업 시간 대비 과하지 않은지 |

M02 의 진단 줄도 로그에 찍힌다(예: `reads=2 maxReadMs=2 maxAnswerGapMs=4 … noticesWithdrawn=1`): 줄이 자동화 트리까지 도달하는 배선은 Mock 로도 실행마다 확인된다.

## 3. 기준 귀속 (Baseline)

이번 실행으로 이 트리에서: `dotnet test` 단위(`AiStatusRefresherTests|AiBoneSuppressionStageTests`)·Functional·E2E M01~M03, `c192d_arms.py`, Release/Debug 바이트 검색.

## 4. 미검증 (Gaps)

- Native 에서의 15초·5초 값과 위 측정값은 **한 번도 실측되지 않았다**(#98). 위 표가 다음 Native CI 에서 읽을 항목이다.
- **첫 AI 프레임의 모델 로딩**: 모듈은 모델을 첫 호출에서 느리게 읽는다(`ai.cpp` 주석 "Loaded lazily on the first call"). 그 로딩이 15초를 넘고 그동안 첫 상태 읽기가 게이트 뒤에 막히면, 이 카드의 알림이 로딩 중에 뜬다(사실이기도 한 "확인 못 함"이지만 사용자는 거짓 경보로 읽을 수 있다). 실측 전이라 크기를 모른다 — `noticesNeverConfirmed` 가 C-09 에서 0 이 아니면 이것이 원인 후보다.
- 알림 문구는 영어 고정(앱의 다른 알림과 동일).

## 5. 잔여 위험 (Residual-risk)

- 진행 중인 읽기가 없는데 `Unknown` 인 채 끝나는 경우(예: 읽기가 계속 예외로 끝나 `null` 을 돌려줌)는 알림 대상이 아니다. 읽기가 돌아오지 않는 막힘만 다룬다. 예외로 끝나는 읽기가 반복되면 화면은 `Unknown` 으로 남는다(전에도 그랬고, 이번 규칙의 범위 밖).
- 측정값은 UI 스레드 타이머가 줄을 다시 내보낼 때만 갱신된다(1초 주기, 값이 바뀐 때만).
