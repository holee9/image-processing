# GUI-C-192c — 철회된 상태를 사용자에게 보이게 (리더 결정)

## 1. 주장 (Claim)

1. 적용된 `Active` 가 15초 안에 갱신되지 못하면 끔 배너와 같은 자리(`AiWorkerPanel`)에 "AI worker status unknown …" 알림이 뜬다.
2. 앱 시작 직후의 `Unknown`, 백엔드 교체·재시작(`Reset`) 직후의 `Unknown`, `Active` 인 채 한가한 앱은 알림을 띄우지 않는다.
3. 새 답이 오면(Active·Disabled 등) 알림이 사라진다. 알림이 떠 있는 동안에도 상태를 다시 읽는다.
4. 알림은 최소 너비 1280 px 에서도 자동화 트리에 있다.

## 2. 증거 (Evidence)

**2-1. 새 상태를 만든 이유(`AiWorkerState.Unconfirmed`).** `Unknown` 은 "첫 읽기 전, 재시작·교체 직후, 답 없음"의 정상 상태라 아무것도 보이지 않아야 한다. 철회로 생긴 상태가 같은 `Unknown` 이면 정상과 이상을 구별할 수 없어 알림 조건이 "철회됐는가"를 알 길이 없다. 그래서 GUI 쪽 상태 하나를 더했다(`InitFailed` 가 같은 이유로 생긴 선례). `Detail` 에 상한(초)을 실어 문구가 코드 상수와 어긋나지 않는다.

**2-2. 문구와 근거.** `AI worker status unknown: it has not been confirmed for over 15 s. AI results may not be applied and images may be returned unchanged. Use Restart AI if this stays.` 기존 끔 배너(`… images are returned unchanged until it is restarted.`)의 구조를 따랐다: 무슨 일인가 → 영향("images are returned unchanged") → 할 일(Restart AI). Restart 버튼은 같은 패널에 이미 있어 알림에서도 보인다. 사용자에게 상태가 "모름"임을 말하고, 영향은 "may" 로 단정하지 않았다(모르기 때문이다).

**2-3. Reset 직후의 Unknown 은 첫 읽기 전과 같은 취급**(알림 없음). 근거: 교체·재시작은 곧바로 새 읽기를 요청하고(`InitializeBackend`·`RestartAiSession`), 그 사이의 `Unknown` 은 이전 세션 정보를 지운 정상 과정이다. 시험 `AnUnknownThatWasNeverActive_IsNeverTurnedIntoANotice` 가 `Unknown` 이 한 시간·두 시간 이어져도 알림이 안 뜸을 단언한다. `Reset` 은 떠 있던 알림도 지운다(시험 `TheNotice_GoesWhenANewAnswerArrives…`).

**2-4. 192b 코드의 결함을 이번에 찾아 고쳤다(내 오류).** 192b 의 `CheckFreshness` 는 마지막 답으로부터 15초만 재고 아무도 다시 읽지 않았다. 알림이 안 보이던 192b 에서는 화면에 드러나지 않았지만, 이번 카드로 철회가 보이게 되면 **아무 일 없는 한가한 앱이 15초 뒤 거짓 경보를 띄웠을 것이다.** 고침: `Active` 가 상한의 1/3(5초)보다 오래되고 진행 중인 읽기가 없으면 다시 읽는다. 상한에 닿았는데도 답이 없을 때만(읽기가 막혔거나 실패 중) `Unconfirmed` 로 철회한다. 시험 `AnIdleActive_IsReadAgain_NotWithdrawn`(120초 동안 매초 틱, 알림 0건, 읽기 10회 초과)이 이것을 지킨다.

**2-5. 시험.**
- 단위(결정적, 가짜 시계): `AiStatusRefresherTests` 철회·표시 4건 신규/갱신, `AiBoneSuppressionStageTests` 문구·표시·요약 1건. 83/83 통과.
- E2E **M02**(`AiWorkerUnconfirmedAtMinimumWidthScenarios`, 새 시험용 결함 `--automation-fault ai-worker-silent`: 상태 읽기가 한 번 "active" 로 답하고 이후 영원히 답하지 않음): 실행 5.4초 뒤 알림 없음(`Assert.Null`) → 14.7초에 알림 출현(문구 단언) → 창을 실제 최소 너비 1280 px 로 줄인 뒤에도 배너와 Restart AI 가 트리에 있음. 통과(`e2e_m01_m02.txt`). M01 도 같이 통과.
- 새 결함은 191b·193 과 같은 조건: 시험 빌드(`XPE_TEST_FAULTS`)에만 존재. `FaultSeamCompiledOutTests` 의 이름 목록에 새 이름을 더했고, **Release dll·pdb·xml 에 새 이름 없음**(`release_byte_search.txt`, Debug 대조군은 발견).
- Functional 388 통과 / 0 실패 / 1 건너뜀, SelfCheck 종료 코드 0.

**2-6. 반증**(`falsification_arms.txt`, 규칙마다 제거, 원본 바이트 동일 복원, 복원 뒤 단위 83/83·M02 통과):

| 지운 규칙 | 빨강이 된 시험 |
|---|---|
| 표시 조건(`ShowsMark` 가 Unconfirmed 무시) | 단위 1건 + **M02(UI)** |
| 철회가 알림 상태가 아닌 `Unknown` (192b 동작) | 단위 2건 + **M02(UI)** |
| 한가한 `Active` 재읽기 | 단위 2건 (`AnIdleActive…` 포함) |
| 알림 중 재읽기 | 단위 1건 |
| 시작 직후 `Unknown` 도 철회 | 단위 2건 (`AnUnknownThatWasNeverActive…` 포함) |
| 알림 문구 | 단위 1건 |

## 3. 기준 귀속 (Baseline)

모두 이번 실행으로 이 트리에서 측정: `dotnet test … AiStatusRefresherTests|AiBoneSuppressionStageTests`, `--filter Category=Functional`, `dotnet test` E2E `AiWorkerMarkAtMinimumWidth|AiWorkerUnconfirmedAtMinimumWidth`, `c192c_arms.py` 출력, Release/Debug 바이트 검색.

## 4. 미검증 (Gaps)

- **15초 상한은 근거 있는 추정이며 Native 실측이 없다**(요청대로 문구·코드 주석이 아니라 여기에만 적는다). 근거는 192b 보고서: 모듈 기본 IPC 시간 제한 5000 ms(`ai_worker_protocol.h:58`)의 3배. 다음 Native CI 에서 리더가 확인해 달라: 정상 AI 처리 중(프레임이 게이트를 길게 쥐는 동안)에도 알림이 뜨는지, 또 `Active` 인 정상 상태에서 5초마다 하는 재읽기가 처리를 늦추는지. 둘 다 Native 에서 못 돌려 봤다(#98).
- **시작 직후부터 읽기가 막혀 한 번도 `Active` 를 못 본 경우**는 알림이 뜨지 않는다(`Unknown` 은 알림 대상이 아니라는 결정의 귀결). 위험이 같은 모양이므로 원하면 "Reset·시작 후 상한 안에 첫 답이 없으면 알림" 을 후속으로 할 수 있다. 이번에는 카드 2번을 따라 경고하지 않았다.
- 시험은 Mock 백엔드 + 시험용 결함으로만 돌렸다. Native 의 실제 `Active → 침묵` 은 보지 못했다.

## 5. 잔여 위험 (Residual-risk)

- 재읽기 주기 5초는 `Active` 인 동안만 돈다. 읽기는 세션 게이트를 기다리므로(백그라운드) UI 는 안 막히지만 게이트를 짧게 쥔다. `Disabled`·`Unknown` 에서는 돌지 않는다.
- 타이머는 UI `Background` 우선순위라 UI 가 바쁘면 알림이 늦을 수 있다.
- 알림 문구가 영어 고정이다(앱의 다른 알림과 동일).
