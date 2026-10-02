# GUI-C-192b — 연속 요청에서도 끔 배너가 늦어지지 않게 (Codex #59)

## 1. 주장 (Claim)

1. 같은 세션(세대·백엔드 일치)에서 온 `Disabled` 답은 더 새 요청이 있어도 첫 답에서 바로 화면에 적용된다.
2. 세대나 백엔드가 달라진 뒤에 도착한 `Disabled` 는 적용되지 않는다.
3. 적용된 `Active` 가 15초 이상 갱신되지 못하면 `Unknown` 으로 철회된다. `Disabled` 는 철회되지 않는다.
4. 192 의 "더 새 요청에 추월당한 `Active` 는 화면에 오르지 않음" 은 그대로다.

## 2. 증거 (Evidence)

- 변경: `AiStatusRefresher.Complete` 에 "추월당했지만 `Disabled`" 분기, `CheckFreshness()`·`Show()` 추가, 생성자에 선택 인자 `clock`·`activeFreshFor`(기본값이라 기존 호출 그대로). 뷰모델이 1초 `DispatcherTimer` 로 `CheckFreshness()` 를 부른다(`StopAiStatusUpdates` 에서 멈춤).
- 모듈 근거(`modules/ai/src/ai.cpp`): `workerDisabled` 는 연속 실패가 상한에 닿을 때 한 번 `true` 가 되고(944행), 이후 호출은 입력을 그대로 돌려준다(925행). 되돌리는 코드는 없고 `xpe_ai_shutdown` 이 상태를 지우고 `xpe_ai_init` 이 새 상태를 만들 때만 사라진다(923행 주석). 그래서 같은 세션에서 `Disabled` 는 안전 쪽으로 낡을 수 없다.
- 시간 상한 15초의 근거: 모듈 기본 IPC 시간 제한 `XPE_AI_DEFAULT_TIMEOUT_MS = 5000` (`ai_worker_protocol.h:58`)의 세 배. 읽기는 프레임 하나가 게이트를 시간 제한만큼 쥐고 있는 동안, 또 같은 게이트 아래의 재시작 뒤에 줄을 설 수 있다. Native 게이트 대기 실측은 이 기계에서 못 했다(#98).
- 시험 4+1건(결정적: 동기 읽기·가짜 시계·장벽, 실제 시간 의존 없음) `AiStatusRefresherTests`: (a) 읽기마다 새 요청이 끼어도 첫 완료 직후 `[Disabled]`; (b) 14.999…초에는 그대로, 15초에 `[Active, Unknown]`, 반복 철회 없음, 막힌 읽기가 풀리면 `Active` 로 복귀; (c) 재시작·백엔드 교체 뒤 이전 세대의 `Disabled` 미적용(2 케이스); (d) 추월당한 `Active` 미적용; `Disabled` 는 1시간 뒤에도 철회 안 됨.
- 반증(`falsification_arms.txt`): 규칙 5개를 각각 없애면 해당 시험이 빨강, 원본 바이트 동일 복원 — 규칙1 제거 → (a) 빨강, 규칙2 제거 → (b) 빨강, 규칙3 제거(세대 확인 전에 Disabled 적용) → (c) 2건 포함 5건 빨강, 규칙4 제거 → (d) 포함 3건 빨강, 규칙5 제거(모든 상태 철회) → Disabled 시험 빨강. 복원 뒤 24/24 통과.
- 실행: Functional 383 통과 / 0 실패 / 1 건너뜀 (`local_runs.txt`), SelfCheck 종료 코드 0 (`selfcheck_run.txt`).
- 부수 수정: `BackendLifecycleTests` 의 "백엔드를 쓰는 멤버" 표에서 `AiStatus` 항목이 람다가 옮겨간 `CreateAiStatusRefresher` 로 바뀌었다(표가 코드를 따라가는 시험).

## 3. 기준 귀속 (Baseline)

모두 이 트리에서 이번 실행으로 측정: `dotnet test … --filter Category=Functional`, `--filter FullyQualifiedName~AiStatusRefresherTests`, `c192b_arms.py` 출력.

## 4. 미검증 (Gaps)

- `Unknown` 의 사용자 표시: `ShowsMark(Unknown)` 은 거짓이고 `BannerFor(Unknown)` 은 비어 있어 **화면에는 아무것도 나오지 않는다**. `Active` 도 아무것도 안 보이므로, 철회의 사용자 가시 효과는 없다. 바뀌는 것은 자동화용 한 줄(`worker=Unknown`)과, 그 뒤 `Disabled` 가 오면 바로 뜬다는 점뿐이다. 사용자에게 "확인 중" 문구를 보여 주려면 새 상태나 문구가 필요하고, 카드 범위(기존 `Unknown` 사용) 밖이라 만들지 않았다. 리더 판단 사항.
- 15초는 근거 있는 추정이지 실측이 아니다(Native 게이트 대기 실측 불가, #98).
- 실제 WPF 타이머가 1초마다 도는지는 단위 시험이 아니라 코드 검토뿐이다(타이머 생성부). E2E 에서 `Active` 를 15초 넘게 막는 시나리오는 만들지 않았다.

## 5. 잔여 위험 (Residual-risk)

- 타이머 틱은 UI 스레드 `Background` 우선순위라 UI 가 바쁘면 철회가 늦을 수 있다(안전 방향 지연이 아니라 표시 지연이며, 위 이유로 사용자 화면은 어차피 비어 있다).
- 추월당한 `Disabled` 를 적용한 뒤 새 요청을 위해 한 번 더 읽으므로, 요청이 계속 오는 동안 읽기가 계속 이어진다(전에도 그랬다).
