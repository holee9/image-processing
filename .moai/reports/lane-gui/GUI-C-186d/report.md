# GUI-C-186d — Codex #31 보류: 상태 조회를 백그라운드로, 적용은 현재일 때만 (#225 행 10, #130)

레인: gui · 푸시 없음(푸시는 리더). 카드: 메인 `.moai/lanes/gui/inbox/GUI-C-186d.md`, 감사 원문: 메인 `.moai/state/codex-archive/31.md`. 5절 형식(주장 / 증거 / 기준 귀속 / 미검증 / 잔여 위험). 이 위에 GUI-C-186c(`9a9be97b`)·188(`0ad58912`)·린트 기록 정정(`fca4f78c`)이 있다.

## 1. 주장

리더 결정대로 **재시도 숫자를 바꾸지 않고 구조를 바꿨다.**

1. 상태 조회(게이트 획득 + `xpe_ai_worker_state`)는 **백그라운드 스레드**에서, 게이트를 **시간 제한 없이** 기다리며 한다. `TryWithLock`·250 ms·재조회 타이머·재시도 횟수는 앱 소스에서 사라졌다.
2. 결과는 UI 디스패처로만 돌아오고, 적용 전에 **세대 번호와 백엔드 식별자**(참조 동일성)를 확인한다. 더 새로운 `Reset`/`Stop` 이 있었거나 백엔드가 바뀌었으면 버린다.
3. 조회는 한 번에 하나만 돈다. 진행 중에 온 요청은 "하나 더" 표시만 하고 끝난 뒤 **정확히 한 번** 더 읽는다.
4. `InitializeBackend`·`ShutdownBackend` 는 시작 때 `Reset`(세대 +1, 표시 Unknown)하고, 끝난 뒤(성공이든 실패든) 다시 읽는다. `SetBackendMode` 는 `InitializeBackend` 를 거친다.
5. 창이 닫힐 때(`OnClosed`)는 백엔드를 끄기 **전에** `Stop` 한다: 돌던 조회도, 그 뒤 요청도 화면을 건드리지 않는다.

## 2. 증거

### 2-1. 구조 (코드)

| 조각 | 위치 | 내용 |
|---|---|---|
| `AiStatusRefresher` | `Services/AiBoneSuppressionStage.cs`(순수, 테스트에 링크) | `Request`/`Reset`/`Stop`, 필드: 세대·진행 중·하나 더·정지. 읽기는 `runInBackground` 로, 완료는 `postToUi` 로 |
| 읽기 | `GuiAiSession.QueryWorkerState()`(`Services/Native/GuiAiRunner.cs`) | 반환형 `AiWorkerStatus`(null 없음), `WithLock` 만 — 한정 대기 없음 |
| 계약 | `IAiSessionBackend.GetAiWorkerStatus()` | null 을 없앴고, "UI 스레드에서 부르지 말 것"을 문서에 적음 |
| 연결 | `MainWindowViewModel` | `new AiStatusRefresher(() => _backend, ReadAiWorkerStatus, ApplyAiWorkerStatus, work => Task.Run(work), PostToUi)`, `RefreshAiWorkerStatus() => AiStatus.Request()`, 디스패처 종료 중이면 게시 안 함 |
| 교체 | `InitializeBackend` | 맨 앞 `AiStatus.Reset()`, try/catch 뒤 `RefreshAiWorkerStatus()` |
| 종료 | `ShutdownBackend`, `MainWindow.OnClosed` | `Reset` → 종료 → 읽기; 닫을 때 `StopAiStatusUpdates()` 후 `ShutdownBackend()` |

범위: 카드 지시대로 `AiFrame.Run`(프레임 게이트)은 건드리지 않았다. 게이트 클래스에서는 상태 조회 전용이던 `TryWithLock` 만 걷어냈다.

### 2-2. 시험 (`AiStatusRefresherTests`, 실제 `AiStatusRefresher`·`AiSessionGate` + 가짜 UI 큐·가짜 읽기)

카드의 시험 항목과 대응:

| 카드 항목 | 시험 |
|---|---|
| 게이트를 오래 잡은 프레임 뒤 추가 이벤트 없이 회복(#31 발견 1) | `AFrameThatHoldsTheGateForAsLongAsItLikes_ThenEnds_LeavesTheScreenCurrent_WithNoFurtherEvent` — 읽기 1회, 재시도 0, 그 뒤 아무 이벤트도 보내지 않고 화면이 최신 |
| 조회 중 UI 스레드가 막히지 않음(발견 3) | `WhileTheGateIsHeld_RequestReturnsAtOnce_AndTheUiThreadKeepsWorking` — 게이트가 잡힌 채 `Request` 가 1 초 안에 돌아오고, 그동안 쌓인 UI 작업이 처리됨 |
| Native Disabled → Mock 교체 → 표시 사라짐, 낡은 조회 버림(발견 2) | `AfterTheBackendIsReplaced_AStaleReadIsDropped_…` — `Reset` 직후 Unknown, 옛 백엔드의 Disabled 는 한 번도 적용되지 않음, 새 백엔드를 읽어 Unknown |
| 종료 중 시작된 조회는 적용되지 않음 | `AfterStop_ARunningReadIsNotApplied_AndANewRequestStartsNothing` — 돌던 조회·`Reset`·이후 요청 모두 무반응, 읽기 수 1 |

추가로 둔 것: 같은 백엔드에서 세대만 다른 경우(`ARead_StartedBeforeAReset_OfTheSameBackend_…`: 종료는 백엔드 객체를 바꾸지 않는다), 세대는 같고 식별자만 다른 경우(`ARead_ForABackendThatIsNoLongerTheCurrentOne_…`), 쌓인 요청 6개가 정확히 한 번의 추가 읽기가 됨, 읽기가 던져도 "진행 중"이 남지 않음, 실제 게이트 읽기가 프레임을 끝까지 기다렸다 통과함(`AiFrameTests`, 한정 대기 시험을 대체), 뷰모델 소스 시험(백그라운드 실행기·교체 순서 `Reset` < 새 백엔드 생성 < try/catch < 읽기 요청·종료 순서·닫기 순서, 그리고 앱 소스 전체에 `TryWithLock`·`StateReadWait`·`ScheduleOnUiThread`·`MaxRetries`·`RetryDelay` 가 없음).

"5초 넘게 잡힌 프레임"은 **시간이 아니라 이벤트**로 재현했다(프레임이 이벤트를 기다림). 이 구조의 성질이 프레임 길이에 의존하지 않는다는 점이 곧 시험이 보여 주는 것이다.

### 2-3. 반증 11팔 (실제 소스, 빌드 성공, 소스 바이트 동일 복구, 복구 후 82/82)

| 팔 | 빨강이 된 시험 |
|---|---|
| 세대 검사 제거 | 같은 백엔드 `Reset` 시험 1건 |
| 백엔드 식별자 검사 제거 | 식별자 시험 1건 |
| `Reset` 이 Unknown 을 보이지 않음 | 2건(교체, 같은 백엔드 `Reset`) |
| `Stop` 이 정지시키지 않음 | 1건 |
| 요청이 쌓일 때 병렬 읽기를 시작 | 2건(쌓임, UI 비차단) |
| 읽기를 호출 스레드에서 실행(조회를 UI 스레드로 되돌림) | 3건(회복, 쌓임, UI 비차단) |
| 던지는 읽기를 막지 않음 | 1건 |
| 뷰모델이 `work => work()`(UI 스레드 읽기) | 소스 시험 1건 |
| 백엔드 교체 시 `Reset` 제거 | 소스 시험 1건 |
| 초기화 실패 뒤 다시 읽지 않음 | 소스 시험 1건 |
| 닫을 때 `Stop` 제거 | 소스 시험 1건 |

(`falsification_arms.txt`. 시작과 끝의 소스 해시 `dffd5531fbe2 cdbb97c59d47 5307e1815f20 dc04c0133b25` 가 같다.) 첫 반증 실행 전에 **빈 구멍을 하나 발견해 메웠다**: 같은 백엔드에서 세대만 다른 경우를 가리는 시험이 없어 세대 검사를 지워도 빨개지지 않았을 것이다 — 그 시험을 먼저 추가했다.

### 2-4. 로컬 실행 (`local_runs.txt`)

`gui/ImageProcTest.slnx`·`clients/ImageProcTest.slnx` 빌드 0 오류. 타이밍을 쓰는 `AiStatusRefresherTests` **연속 6회 16/16**. `Category=Functional` **329 통과 · 건너뜀 1(기존) · 실패 0**. E2E(Mock, UI Automation 패턴만): `ProcessingChainScenarios`·`AutomationReportBackendTests` 통과(C-09·A03 등은 사유 있는 건너뜀 — C-09 는 네이티브 전용). 매 시나리오 종료마다 `OnClosed` 의 `Stop` 경로가 돌았고 멈춤이 없었다.

## 3. 기준 귀속 (측정 대상)

모든 숫자는 이 트리(`dev/gui` 의 `fca4f78c` 위 작업 트리)에서 이 실행으로 얻은 것이다. 빌드 경고 12+1 개는 이전 카드와 같은 개수이고 내가 바꾼 줄에서 나온 것이 아니다(이번에는 개수만 비교했고 줄 단위 재확인은 하지 않았다).

## 4. 미검증

1. **네이티브 경로는 실행되지 않았다.** 실제 `xpe_ai_worker_state` 를 부르는 백그라운드 읽기, 실제 프레임(모듈 시간 예산까지 게이트를 쥠) 뒤의 회복, 네이티브 C-09 는 CI 첫 관측이 근거다(로컬에 `xpe_ai.dll`·worker 없음, 로컬 네이티브 빌드 금지 — 186c 보고서 §2-4).
2. **실제 `Dispatcher.BeginInvoke` 로 UI 에 돌아오는 경로가 상태 변화를 화면까지 가져가는 것**은 본 적이 없다. Mock E2E 는 이 경로를 실행하지만 Mock 은 항상 Unknown 이라 화면 값이 바뀌지 않는다. 순수 클래스는 가짜 UI 큐로, 뷰모델 연결은 소스 시험으로 확인했다.
3. `Task.Run` 이 쓰는 스레드가 프로세스 종료를 막지 않는다는 것(풀 스레드는 배경 스레드)은 .NET 의 일반 성질로 알고 있으나 이 앱에서 관측하지 않았다.
4. 창이 닫히는 도중 `Stop` 과 읽기 완료의 순서(디스패처 종료와의 경쟁)는 시험하지 않았다. 코드는 종료 중 게시를 버리고 `_stopped` 면 적용하지 않는다.
5. 빌드 경고의 줄 단위 비교(§3).

## 5. 잔여 위험

- **프레임이 끝나기 전에는 낡은 표시가 그대로다.** 구조가 바뀐 것은 "영영 낡음"이 "프레임이 끝나면 갱신"으로 된 것이지, 프레임 도중의 최신 상태를 보여 주는 것이 아니다. 조용한 워커 앞에서는 모듈의 시간 예산(기본 5 초)까지 걸린다.
- **풀 스레드 하나가 그동안 게이트에서 막힌다**(최대 한 개, 진행 중 읽기 하나만 허용하므로). 풀이 포화된 상황은 보지 않았다.
- **상태 갱신이 이전보다 늦게 보일 수 있다.** 이전에는 UI 스레드에서 즉시 읽었다. 지금은 디스패처 큐를 한 번 거친다. 네이티브 E2E 가 렌더 직후 바로 값을 읽는 곳이 있으면 거기서 보일 것이다 — C-09 는 배너가 보일 때까지 기다리고 같은 필드에서 값을 읽으므로 영향이 없을 것으로 보지만 **실행해 보지 않았다.**
- **백엔드 종료·교체는 여전히 UI 스레드에서 게이트를 기다릴 수 있다**(`GuiAiSession.Shutdown` 이 게이트를 잡음). 이 카드의 범위 밖이고 바꾸지 않았다. 이전부터 있던 성질이며 이번에 관측하지 않았다.
- `IAiSessionBackend` 계약이 바뀌었다(null 제거). 구현체는 `RealXpeBackend`·`FaultInjectingBackend` 둘이고 둘 다 맞췄다(Mock 은 구현하지 않음).

## 증거 파일

`falsification_arms.txt` · `local_runs.txt` · `text_lint.txt`

🗿 MoAI
