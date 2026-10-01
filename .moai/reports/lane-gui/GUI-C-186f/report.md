# GUI-C-186f — Codex #36 보류 4건: 수명 세대값, 전환 거부 범위, 옛 Lane B, 로그 동시 접근 (#225 행 10, #130)

레인: gui · 푸시 없음(푸시는 리더). 카드: 메인 `.moai/lanes/gui/inbox/GUI-C-186f.md`, 감사 원문: 메인 `.moai/state/codex-archive/36.md`. 5절 형식(주장 / 증거 / 기준 귀속 / 미검증 / 잔여 위험). 이 위에 GUI-C-186e(`ee7ab9b2`)와 main `53ec370a` 병합(`62e6da04`)이 있다.

## 1. 주장

리더 결정대로 **수명 세대값 하나**로 처리했다. 네 건 모두 "시작할 때 한 번만 검사"에서 나왔고, 지금은 시작 때 티켓(백엔드 참조 + 세대값)을 잡아 **각 `await` 뒤, 후속 작업을 예약하기 직전, 실패 경로**에서 다시 대조한다.

1. **(1, 높음) 시작된 Apply 가 종료 중에 결과를 적용하고 Lane B 를 새로 시작** — 막았다. 종료·교체가 시작되는 순간 세대가 오르므로, 체인이 gate 에서 풀려도 메인 결과는 적용되지 않고 Lane B `RunChain` 은 시작되지 않는다. 실패 경로(알림·상태줄·"stale" 표시)도 같다.
2. **(2, 보통) 전환 거부가 모든 진입점을 덮지 않음** — `Load image`·`VOI preset` 에 거부를 더했고, 거부 대상은 이제 5개 진입점이다(§2-2 표). 표는 **소스 검색으로 만들었고 시험이 그 표와 소스를 대조**한다.
3. **(3, 보통) 실패한 옛 Lane B 가 새 레인을 지움** — 성공·예외 양쪽을 티켓으로 대조한다. 옛 작업의 실패는 현재 레인에 영향이 없다.
4. **(4, 보통) 백엔드 로그·경고 동시 접근** — 두 백엔드의 로그·경고를 백엔드 내부 잠금(`BackendTelemetry`)으로 보호하고, 개수와 항목을 **하나의 원자적 스냅샷 호출**(`GetTelemetrySince`)로 준다. 인터페이스의 네 메서드(`GetLogCount`·`GetLog`·`GetAlertCount`·`GetAlert`)는 없어졌고 호출자(뷰모델·SelfCheck·장애 주입 래퍼)를 모두 갱신했다.

비차단(카드 지시, 고치지 않음): §4 의 "한계 — 관측 안 함, 동작은 이것".

## 2. 증거

### 2-1. 구조

| 조각 | 위치 | 내용 |
|---|---|---|
| 세대·티켓 | `Services/BackendLifecycle.cs`(순수) | `Generation`(종료 `Begin` 과 교체 `Bump` 의 **시작 순간** 증가, 감소 없음), `Take(backend)` → `BackendTicket`, `IsCurrent(ticket, 현재 백엔드)` = 세대 같음 ∧ 같은 객체 ∧ 전환 중 아님 |
| 뷰모델 | `MainWindowViewModel` | `TakeTicket()`/`IsCurrent(ticket)`; Apply·Restart AI 는 시작 때 티켓을 잡고, `RenderLanesAsync` 는 Apply 의 티켓을 받는다; `InitializeBackend` 는 거부 직후 `Lifecycle.Bump()` |
| 텔레메트리 | `Services/BackendTelemetry.cs`(순수) | 잠금 안에서 쓰고, `Since(logsSeen, alertsSeen)` 가 복사본과 합계를 한 번에 돌려줌. 커서가 합계보다 크면(비운 뒤) 처음부터 |
| 인터페이스 | `IXpeBackend` | `GetTelemetrySince(int, int)` 하나로 교체; `RealXpeBackend`·`MockXpeBackend`·`FaultInjectingBackend` 갱신 |

### 2-2. 진입점 표 (카드 2번 요구: `_backend` 를 쓰는 뷰모델 멤버 전수)

`MainWindowViewModel.cs` 에서 접근 한정자로 시작하는 멤버 중 `_backend` 를 쓰거나 `TakeTicket(` 을 부르는 것을 소스 검색으로 뽑았다(시험 `EveryMemberThatUsesTheBackend_IsInTheTable_…` 가 같은 검색으로 표와 대조한다 — 표에 없는 새 멤버가 생기면 빨갛다).

| 멤버 | 구분 | 전환 중 | 티켓 | SelfCheck 시나리오 |
|---|---|---|---|---|
| `LoadImageFromPathAsync` | 진입점 | **거부**(`Load image`; 파일 대화상자 응답이 도착하는 곳이기도 함) | — | 2 |
| `ApplyBodyPartPreset` | 진입점 | **거부**(`VOI preset`) | — | 2 |
| `ApplyDisplayPipelineAsync` | 진입점 | 거부(`Display pipeline`) | 잡음, 체인 뒤·실패·레인 뒤 확인 | 1, 1b, 2, 3, 3b |
| `RestartAiSession` | 진입점 | 거부(`Restart AI`) | 잡음, await 뒤·실패 확인 | 2(거부만; 아래 한계) |
| `InitializeBackend` | 진입점 | 거부(`Initialize backend`) | 세대 `Bump` | 2, 3 |
| `RenderLanesAsync` | Apply 가 부름 | (Apply 가 거부) | Apply 의 티켓: 시작 전·await 뒤·실패 | 1, 3, 3b |
| `BeginShutdown`·`FinishShutdown`·`ShutdownBackendBlocking` | 수명 전환 | (전환 자체) | 세대 증가 | 1, 1b, 2 |
| `DrainBackendTelemetry` | 텔레메트리 읽기 | — | — | (원자적 스냅샷, §2-5) |
| `AiStatus`(상태 읽기) | AiStatusRefresher | — | 자체 세대·식별자(186d) | — |
| `CanRunPreprocessing` | 읽기 전용 속성 | — | — | — |
| `TakeTicket`·`IsCurrent`·`_backend`·생성자 | 도우미/필드/생성 | — | — | — |

전환 중 시나리오 2 가 단언하는 것: 각 진입점에서 **백엔드 호출 0**(Load·Preset·Chain·Apply 합계 불변), **백엔드 빌드 0**, **화면 불변**(원본·처리·Lane B 이미지 참조 동일), 거부 로그가 진입점마다 남음; 전환이 끝난 뒤에는 각 진입점이 다시 허용된다(호출 횟수가 오름).

### 2-3. 시험

- **실제 뷰모델 SelfCheck 5시나리오**(`gui/ImageProcTest.SelfCheck/LifetimeScenarios.cs`, 앱을 참조하는 콘솔 러너; CI `gui-shell-runners` 잡이 `dotnet run … --no-build` 로 돌린다 — 시험이 `ci.yml` 에 그 명령이 있는지까지 대조):
  1. 메인 체인을 gate 에서 멈춤 → 종료 → 해제 → 메인 결과 미적용, Lane B 호출 증가 없음, 로그에 drop
  1b. 같은 Apply 의 체인이 해제되며 **실패** → 알림·"stale"·상태줄 불변
  2. 진입점 전수 거부/재허용(위 표)
  3. 옛 Lane B 를 지연 → 백엔드 교체 → 새 레인 → 옛 작업이 예외로 끝남 → 새 레인 유지
  3b. 종료가 시작·끝난 뒤 옛 Lane B 가 **성공**으로 끝남 → Lane B 불변, 옛 렌더의 타이밍 줄도 안 써짐
- **순수 시험**: `BackendLifecycleTests`(티켓 의미 3건 + 표 대조 + 186e 것), `BackendTelemetryTests`(경계 의미, 비운 뒤 커서, **장벽으로 겹친 쓰기 4스레드 × 60라운드와 드레인**: 누락·중복·예외 0, 합계와 항목 수 불일치 0, 소스 시험).
- SelfCheck 는 이전 점검 두 줄(`GetLogCount` 등)을 스냅샷으로 바꿔 쓴다(대조값은 그대로).

### 2-4. 반증 16팔 + 부록 2팔 (실제 소스, 빌드 성공, 소스 바이트 동일 복구 18/18)

| 팔 | 빨강이 된 것 |
|---|---|
| 텔레메트리 저장소가 잠금을 안 잡음 | 동시성 시험(3회 반복 모두 빨강, 시험 시간 2 ms 로 즉시) |
| 종료가 세대를 올리지 않음 | 티켓 시험 |
| 티켓이 백엔드 객체를 무시 / 세대를 무시 / 진행 중 전환을 무시 | 각각 티켓 시험 1건 |
| 뷰모델 `IsCurrent` 가 항상 참 | 표 시험 + **SelfCheck 시나리오 1·1b·3·3b** |
| Apply 가 체인 뒤 재검사 안 함 | 표 시험 + **시나리오 1** |
| Apply 가 실패 경로에서 재검사 안 함 | 표 시험 + **시나리오 1b**(부록) |
| Apply 가 레인 뒤 재검사 안 함 | 표 시험 + **시나리오 3b**(부록) |
| Lane B 를 티켓 확인 없이 예약 | 표 시험(소스만) |
| 옛 Lane B 성공이 적용됨 | 표 시험 + 소스 시험 + **시나리오 3b** |
| 옛 Lane B 실패가 레인을 지움 | 표 시험 + **시나리오 3** |
| Restart AI 가 await 뒤 재검사 안 함 | 표 시험(소스만) |
| Load image 거부 제거 / VOI preset 거부 제거 | 표 시험 + **시나리오 2** |
| 교체가 세대를 올리지 않음 | 표 시험(소스만) |

(`falsification_arms.txt`. 시작과 끝의 소스 해시 `b76215f6e47c f4ff2f3a2048 799905f84fd6` 가 같다.) **내가 바로잡은 것**: 첫 실행에서 Apply 실패 경로와 레인 뒤 재검사 두 팔에 SelfCheck 를 붙이려던 스크립트 수정이 **아무것도 바꾸지 못했고**(파이썬 치환이 앵커 0건), 그래서 그 두 팔은 소스 시험에만 걸렸다. 편집 도구로 고쳐 그 두 팔만 다시 돌렸고 결과를 부록에 이어 붙였다. 또 반증 전에 SelfCheck 시나리오 1b 와 3b 의 타이밍 단언을 **보강**했다(그 전에는 이 두 경로가 실제 실행으로는 잡히지 않았다).

### 2-5. 텔레메트리 (카드 4번)

반증은 **안정적이었다**: 잠금을 빼면 3회 반복 모두 빨강(시험 시간 2 ms — 정확히 어느 라운드에서, 어떤 실패 형태로 걸렸는지는 읽지 않았다). 불안정하면 횟수를 적으라는 지시에 따라 적는다: 라운드 60, 쓰기 4스레드(스레드당 로그 400 + 경고 100), 3회 반복에서 3회 모두 빨강.

### 2-6. 로컬 실행 (`local_runs.txt`)

두 솔루션 빌드(다른 작업이 돌지 않는 상태) 0 오류·0 경고. 33건(`BackendLifecycleTests`+`BackendTelemetryTests`+`AiStatusRefresherTests`) **연속 6회 33/33**. `Category=Functional` **346 통과 · 건너뜀 1(기존) · 실패 0**. SelfCheck 3회 연속 exit 0. E2E(Mock): **다른 작업 없이 다시 빌드한 뒤 처음부터 재실행해 23 통과 · 0 실패 · 7 건너뜀(사유 있음)**, 앱 프로세스 잔류 없음. main 병합 뒤 새 린트 스크립트(`-Encoding UTF8`) 통과.

## 3. 기준 귀속 (측정 대상)

모든 숫자는 이 트리(`dev/gui` `62e6da04` 위 작업 트리)에서 이 실행으로 얻은 것이다. SelfCheck 시나리오는 **Mock 백엔드를 감싼 스크립트 백엔드**로 돈다: 체인 호출 하나를 붙잡거나 종료를 붙잡을 뿐 다른 답은 Mock 의 것이다. 진짜 AI 게이트가 아니라 이벤트로 "오래 걸림"을 재현했다.

## 4. 미검증 · 한계

1. **네이티브 경로**(진짜 `xpe_bone_suppress` 프레임이 gate 를 쥔 채 종료·교체·Apply 가 겹치는 장면)는 실행되지 않았다. CI 첫 관측이 필요하다.
2. **`Restart AI` 의 티켓 확인은 실행으로 검증하지 못했다.** 스크립트 백엔드는 `IAiSessionBackend`(내부 인터페이스)를 구현할 수 없어, SelfCheck 는 거부만 보고 await 뒤 재검사는 소스 시험(`IsCurrent(ticket)` 횟수)과 반증 1팔로만 잡는다.
3. **"Lane B 를 티켓 확인 없이 예약"과 "교체가 세대를 올림"도 소스 시험에만 걸린다**(반증 표). 두 번째는 시나리오가 교체 때 새 객체를 만들기 때문에, 세대만 달라지고 같은 객체인 교체를 실행으로 만들지 못했다(종료는 시나리오 3b 가 그 경우다).
4. **비차단(카드 지시), 고치지 않음 — 관측 안 함, 동작은 이것:** `Application.Shutdown`·`SessionEnding` 은 `_forceClose` 로 닫기 취소를 우회하고, `OnClosed → ShutdownBackendBlocking` 은 호출 스레드에서 AI gate 를 기다린다. 로그오프 중 프레임이 gate 를 쥐고 있으면 그만큼 지연될 수 있다. 실행해 본 적 없다.
5. **E2E 첫 실행은 무효다.** 4건(A06·A14·R12·R11b) 실패는 내가 E2E 가 도는 중에 **비증분 재빌드**를 시작해 앱 실행 파일 복사가 막힌(MSB3021) 조건에서 나왔다. 다른 작업 없이 다시 빌드해 처음부터 다시 돌린 결과는 0 실패이므로 동시 재빌드 탓으로 **보지만 증명한 것은 아니다**(첫 실행의 실패 메시지를 읽지 않았다).
6. 텔레메트리 외의 백엔드 내부 상태(`_runtimeInfo` 등)는 여전히 잠금 없이 읽고 쓴다. 이번 카드의 대상이 아니었고 시험하지 않았다.

## 5. 잔여 위험

- **같은 백엔드 위의 두 Apply 는 이번 세대값이 가르지 못한다.** 세대값은 수명(종료·교체)만 본다. 같은 백엔드에서 렌더 둘이 겹쳐 옛 Lane B 가 늦게 끝나 새 렌더의 레인을 덮는 문제(렌더 대체)는 기존 구조 그대로이고 이번 카드가 다루지 않았다.
- 거부 대상은 다섯 진입점이다. 표는 `_backend`/`TakeTicket(` 을 쓰는 **뷰모델 멤버**만 본다: 뷰모델 밖에서 백엔드를 만지는 곳(서비스, 자동화 러너)은 이 표에 없다.
- 인터페이스 변경(`IXpeBackend` 네 메서드 제거): 이 저장소의 호출자는 모두 갱신했고(`gui`·`clients` 소스 검색 + 두 솔루션 clean 빌드) 저장소 밖 호출자는 확인할 수 없다.
- 진행 중 렌더가 종료와 겹치면 결과는 버려진다(로그만 남는다). 사용자에게 보이는 알림은 없다.

## 6. 정정 (이전 카드 보고서에 대한 내 오류)

**빌드 경고.** GUI-C-186d·186e 보고서에 "빌드 경고 12+1 개는 내가 바꾼 줄에서 나온 것이 아니다"라고 적었다. 그 빌드는 증분이라 경고가 다시 출력되지 않았다. 이번에 전체 재빌드를 하자 **내 시험 파일 4개**(`AiFrameTests` 9, `AiStatusRefresherTests` 6, `BackendLifecycleTests` 5, `BackendTelemetryTests` 2)에서 xUnit1031(시험에서 블로킹 `Wait`) 22건이 나왔다. 이 시험들은 진짜 스레드가 gate 에 막히는 것을 재는 것이 목적이라 블로킹이 의도이므로, 이유를 적은 `#pragma warning disable xUnit1031` 을 네 파일 머리에 넣었다. 전체(비증분) 재빌드에서 xUnit1031 은 0건이 됐고, 이어서 다른 작업 없이 한 빌드는 경고 0 이다.

## 증거 파일

`falsification_arms.txt` · `local_runs.txt` · `text_lint.txt`

🗿 MoAI
