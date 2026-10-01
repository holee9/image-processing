# GUI-C-186e — 종료·Lane B 체인이 UI 스레드에서 AI gate 를 기다림 (Codex #33 발견 1, #225 행 10, #130)

레인: gui · 푸시 없음(푸시는 리더). 카드: 메인 `.moai/lanes/gui/inbox/GUI-C-186e.md`, 감사 원문: 메인 `.moai/state/codex-archive/33.md`. 5절 형식(주장 / 증거 / 기준 귀속 / 미검증 / 잔여 위험). 이 위에 GUI-C-186d(`d406df54`)가 있다. `main` 은 아직 받지 않았다(리더: push 신호 뒤).

## 0. 리더와 합의한 범위 (메시지 기준)

1. **카드 전제 정정을 받아들임**: "백엔드 교체의 `Dispose` 가 UI 스레드에서 gate 를 기다린다"는 현재 코드에서 성립하지 않는다. 교체 경로는 **비동기화하지 않고** 소스 시험으로 사유만 고정한다.
2. **`RenderLanes`→`RunChain` 이 UI 스레드에서 AI 프레임을 돌리는 경로를 이 카드에 넣는다.** 먼저 가짜 gate 로 재현하고, 재현되면 백그라운드로 옮긴다.
3. 설계 동의, 단 **"대기 중 한 번 더 닫기를 누르면 기다리지 않고 닫음"은 뺀다.** "종료 중…" 표시만 둔다.
4. push 전이라도 `dev/gui` 에서 시작해도 됨.

## 1. 주장

1. **종료 버튼과 창 닫기가 UI 스레드에서 gate 를 기다리지 않는다.** 시작만 UI 스레드가 하고(`BeginShutdown`), gate 대기와 `Shutdown` 은 백그라운드에서 한다. 그동안 화면은 "Backend shutting down..." 이고 UI 는 응답한다.
2. **전환 중 처리 요청은 거부된다**(`Apply display pipeline`, `Restart AI`, `Initialize backend`). 끝난 뒤에는 새 백엔드로 처리된다(§2-3).
3. **창 닫기**: 첫 닫기 요청은 취소되고, 백그라운드 종료가 끝나면 창이 스스로 닫힌다. 상한도 "그냥 닫기"도 없다(이유 §2-5).
4. **Lane B 체인은 이제 백그라운드**에서 돈다. 재현에서 UI 디스패처 최악 지연이 **3028 ms → 85 ms**.
5. 교체 경로가 gate 를 잡지 않는 이유를 소스 시험으로 고정했다(리더 결정 ①).
6. `AiFrame.Run` 과 `AiStatusRefresher` 는 건드리지 않았다(카드 지시).

## 2. 증거

### 2-1. Lane B 재현 (카드 ②, 리더 요청: "먼저 가짜 gate 로 재현")

도구(`harness_Program.cs.txt`, 저장소 밖 임시 프로젝트, UI Automation·키보드·마우스 입력 없음)가 **진짜 `MainWindowViewModel`** 을 만들고, `RunChain` 이 gate 를 기다리는 백엔드를 끼운다(다른 작업이 gate 를 3000 ms 쥔 채 Lane B 의 호출이 도착). Lane B 를 켜고(`LaneBVoiWindowWidth=500`) 앱 자신의 경로(`LoadImageFromPathAsync` → `ApplyDisplayPipelineAsync` → 레인 렌더)로 이미지를 읽으며, UI 디스패처에 50 ms 마다 탐침을 던져 실행 지연을 잰다.

| | `RunChain` 호출 2(Lane B) | UI 디스패처 최악 지연 | 렌더 전체 |
|---|---|---|---|
| **수정 전**(`repro_before_fix.txt`) | **UI 스레드**에서 실행 | **3028 ms** | 3148 ms |
| 수정 후(`repro_after_fix.txt`) | 풀 스레드 | **85 ms** | 3147 ms |

**재현됐다**: 수정 전 코드에서 Lane B 체인은 UI 스레드에서 돌며 gate 를 기다렸다. 렌더 전체 시간은 같다(gate 를 기다리는 것은 그대로) — 달라진 것은 그동안 UI 가 막히는지다.

### 2-2. 종료·교체 구조

| 조각 | 위치 | 내용 |
|---|---|---|
| `BackendLifecycle` | `Services/BackendLifecycle.cs`(순수, 테스트에 링크) | `Begin`(백그라운드 작업 + UI 쪽 완료), `WhenIdle`(전환이 끝나면 실행), `TryAdmit`(전환 중 요청 거부). 한 번에 전환 하나 |
| 뷰모델 | `MainWindowViewModel` | `BeginShutdown(whenDone)`, `FinishShutdown`, `IsBackendTransitioning`, `RefusedWhileTransitioning`; `ShutdownBackendCommand` 는 `BeginShutdown()`; 옛 동기 `ShutdownBackend()` 는 없어짐 |
| 거부 지점 | `ApplyDisplayPipelineAsync`, `RestartAiSession`, `InitializeBackend` 맨 앞 | `"The backend is shutting down; the request was not run."` |
| 창 | `MainWindow.OnClosing`/`OnClosed` | 첫 닫기 취소 → `BeginShutdown(() => Close())`; `OnClosed` 의 동기 종료는 닫기가 기다릴 수 없던 경우(자동화 자체 실행의 `Application.Shutdown`, 세션 종료)만 |
| Lane B | `RenderLanesAsync` | 후보 체인을 `await Task.Run(() => RenderLane(backend, …))`; 결과는 백엔드가 그대로이고 전환 중이 아닐 때만 적용(아니면 로그) |
| 자동화 | `MainWindow.xaml.cs` 자체 실행 | 종료 버튼 뒤 전환이 끝나기를 기다린 뒤 상태를 읽음 |

### 2-3. 종료 시나리오 (진짜 뷰모델, `shutdown_after_fix.txt`)

같은 도구의 둘째 시나리오: 활성 백엔드의 `Shutdown` 이 gate 를 기다리고, 다른 작업이 gate 를 3000 ms 쥔 채 `BeginShutdown` 을 부른다.

```
BeginShutdown returned after 0 ms; IsBackendTransitioning=True; status='Backend shutting down...'
Apply during the shutdown: display calls on the active backend 1 -> 1; status='The backend is shutting down; the request was not run.'
Initialize backend during the shutdown: backends built 2 -> 2
whenDone ran = True after 3006 ms; IsBackendTransitioning=False; runtime state='Shutdown'; status='Backend shutdown.'
Initialize backend after the shutdown: backends built = 3
Apply after the new backend: new backend display calls = 1; old (shut down) backend calls = 1
WORST UI-dispatcher probe latency over the whole run = 3 ms
```

카드의 시험 항목과 대응: UI 가 응답한다(0 ms 반환, 최악 지연 3 ms) · 교체 중 처리 요청이 거부된다(호출 1→1, 교체 2→2) · 교체 뒤 새 백엔드로 처리된다(새 백엔드 호출 1, 옛 백엔드 그대로 1).

**반증(진짜 뷰모델)**: 종료를 UI 스레드로 되돌리면(`work => work()`) `shutdown_arm_on_ui_thread.txt`: `BeginShutdown returned after 3010 ms`, 최악 지연 **3013 ms**, 요청이 거부되지 않고 처리됨. 복구 후 소스 바이트 동일.

### 2-4. 커밋되는 시험 (`BackendLifecycleTests`, 실제 `BackendLifecycle`·`AiSessionGate` + 가짜 UI 큐)

- 프레임이 gate 를 쥔 동안 `Begin` 이 1초 안에 돌아오고 UI 작업이 처리되며, 프레임이 끝나면 완료된다.
- 전환 중 거부 / 끝나면 허용, 둘째 전환은 시작하지 않음(작업 1회), `WhenIdle` 은 끝난 뒤 정확히 한 번(전환이 없으면 즉시), 던지는 종료는 오류를 알리고 전환에서 빠져나옴(영영 거부 상태가 되지 않음).
- 소스 시험: 뷰모델이 `Task.Run` 실행기와 `BackendLifecycle` 을 쓰고 **동기 `_backend.Shutdown()` 은 막는 대체 경로 하나뿐**, 세 경로가 정확한 거부 가드(`if (RefusedWhileTransitioning("…")) { return; }`)로 시작, Lane B 가 백그라운드이고 결과를 현재 백엔드와 대조, 창이 취소 → 시작 → 스스로 닫음이며 "그냥 닫기" 없음(`Close();` 가 콜백에 하나뿐, `_closeScheduled` 가드).
- **리더 결정 ① 고정**: 세 백엔드 중 `IDisposable` 인 것이 없고, `RealXpeBackend.Initialize` 가 `GuiAiSession` 을 만지지 않으며, 앱에서 `GuiAiSession.` 을 쓰는 파일이 `GuiAiRunner.cs`·`RealXpeBackend.cs` 둘뿐(대조: 스캔이 그 둘을 찾음). 하나라도 바뀌면 교체도 같은 처리가 필요하다.
- 186d 의 소스 시험은 새 구조(`BeginShutdown`/`FinishShutdown`/`OnClosing`)에 맞게 고쳤다.

### 2-5. 상한을 두지 않은 이유 (카드 3번, 리더 확정)

네이티브 호출은 중간에 끊을 수 없고, 프레임은 모듈 시간 예산(약 5초) 안에서 스스로 끝난다. 상한 뒤 프로세스를 끝내려면 워커 프로세스가 고아로 남는지 알아야 하는데 **로컬에서 확인할 수 없다**. 그래서 상한도, 두 번째 닫기로 기다리지 않고 닫는 길도 두지 않았고, 대기 중에는 "Backend shutting down..." 표시만 있다.

### 2-6. 반증 14팔 (실제 소스, 빌드 성공, 소스 바이트 동일 복구, 복구 후 25/25)

| 팔 | 결과 |
|---|---|
| 종료 작업을 호출 스레드에서 실행 | 4건 빨강(10 초씩 기다리다 실패 — UI 가 막힘) |
| 요청을 항상 허용 | 1건 |
| 둘째 전환을 거부하지 않음 | 1건 |
| 전환 중에도 뒤따르는 동작을 즉시 실행 | 1건 |
| 전환이 끝나지 않음 | 4건 |
| 뷰모델이 종료를 UI 스레드에서 | 소스 시험 1건 |
| 종료 호출이 백그라운드 작업 밖으로 | 소스 시험 1건 |
| Lane B 를 UI 스레드에서 다시 실행 | 소스 시험 1건 |
| 교체 뒤에도 Lane B 결과 적용 | 소스 시험 1건 |
| 표시 파이프라인이 전환 중 거부하지 않음 | 소스 시험 1건 |
| 백엔드 교체가 차례를 기다리지 않음 | 소스 시험 1건 |
| 첫 닫기 요청이 바로 닫음 | 소스 시험 1건 |
| 둘째 닫기 요청이 또 닫기를 예약 | 소스 시험 1건 |
| `Initialize` 가 AI 세션을 만짐(리더 전제 깨기) | 소스 시험 1건 |

(`falsification_arms.txt`. 시작과 끝의 소스 해시 비교는 파일에 있다.) **첫 반증 실행에서 세 팔이 초록으로 남았다** — 시험의 구멍이었다: 거부 가드 둘은 헬퍼 이름이 텍스트에 남는 한 통과하는 문자열 포함 검사였고, 둘째 닫기 팔은 `_closeScheduled` 이름만 보고 `if (true)` 를 못 잡았다. 시험을 정확한 형태(거부 후 `return` 하는 `if`, `if (!_closeScheduled) { _closeScheduled = true;`)로 조이고 14팔을 다시 돌려 전부 빨강을 확인했다.

### 2-7. 로컬 실행 (`local_runs.txt`)

두 솔루션 빌드 0 오류. 타이밍을 쓰는 시험(`BackendLifecycleTests`+`AiStatusRefresherTests` 25건)을 **연속 6회 25/25**. `Category=Functional` **338 통과 · 건너뜀 1(기존) · 실패 0**. E2E(Mock, UI Automation 패턴만): `ProcessingChainScenarios`·`AutomationReportBackendTests`·`MenuCommandScenarios` 통과(C-09 등은 사유 있는 건너뜀). 앱이 창 닫기 경로로 매번 종료됐고 `ImageProcTest.exe` 가 남지 않았다(대조: `dotnet.exe` 는 조회됨).

## 3. 기준 귀속 (측정 대상)

모든 숫자는 이 트리(`dev/gui` `d406df54` 위 작업 트리)에서 이 실행으로 얻은 것이다. 재현 도구의 gate 는 진짜 `AiSessionGate`/AI 세션이 아니라 **일반 잠금**이다(`GuiAiSession` 의 gate 도 잠금이고, 같은 모양으로 기다린다는 것이 전제). 진짜 AI 프레임이 gate 를 쥐는 장면은 만들지 않았다.

## 4. 미검증

1. **네이티브 경로**(진짜 `xpe_bone_suppress` 프레임이 gate 를 쥔 채 창을 닫는 장면, 진짜 `xpe_ai_shutdown`)는 실행되지 않았다. 로컬에 `xpe_ai.dll`·worker 가 없고 로컬 네이티브 빌드는 금지(186c 보고서 §2-4)이다.
2. **보이는 창의 응답성과 "종료 중" 표시**를 눈으로 본 것이 아니다. UI 디스패처 지연을 측정했고(진짜 뷰모델) E2E 가 Mock 앱을 닫았다. 하지만 **gate 를 쥔 동안 실제 창을 닫아 본 적은 없다.** 상태줄에 "Backend shutting down..." 이 실제로 보이는지는 확인하지 않았다(상태 텍스트가 상태줄에 바인딩된다고 읽었을 뿐).
3. **`Application.Shutdown`(자동화 자체 실행)과 Windows 세션 종료 경로**(`_forceClose`, `OnClosed` 의 동기 종료)는 실행해 보지 않았다. 앞의 것은 A06 이 자체 실행을 돌리며 통과했으나 `_forceClose` 가 실제로 그 경로에서 닫기를 막지 않았는지 따로 관측하지 않았다.
3b. WPF 가 `Application.Shutdown` 중 취소된 닫기를 어떻게 다루는지는 문서로 확인하지 않았다. 지금은 `_forceClose` 로 취소하지 않게 했다.
4. 종료 중 **진행 중인 렌더**는 취소하지 않는다: 종료는 gate 를 쥔 렌더 뒤에서 기다리고, 렌더가 끝나면 진행한다. 이 순서를 렌더가 실제로 gate 를 쥔 상태로 관측하지 않았다.
5. 전환 중 거부 대상은 세 경로(표시 파이프라인, Restart AI, 백엔드 초기화)다. 다른 진입점(벤치마크 실행, 자체 점검 등)이 백엔드를 만지는지는 조사하지 않았다.

## 5. 잔여 위험

- **백엔드 로그 목록의 동시 접근.** `RealXpeBackend`/`MockXpeBackend` 의 `_logs` 는 잠금 없는 `List<string>` 인데, 이제 백그라운드 `Shutdown` 이 거기에 쓴다. 이미 풀 스레드의 `RunChain` 이 같은 목록에 쓰고 UI 가 읽는 구조였고(기존 성질), 이번에 쓰기 하나가 더 생겼다. 이번에 시험하지 않았고 바꾸지도 않았다.
- 종료가 **모듈 시간 예산 안에 끝난다는 전제**에 기대고 있다. 모듈이 그보다 오래 gate 를 쥐면 창은 닫기 요청 뒤에도 계속 열려 있다("종료 중…" 만 보임). 리더 결정이고 이 카드는 그 선택을 되돌릴 길(강제 닫기)을 일부러 두지 않았다.
- Lane B 렌더가 길어지면(AI 가 체인에 켜져 있을 때) **전체 `Apply` 시간은 이전과 같다.** 바뀐 것은 그동안 UI 가 막히지 않는다는 점이고, 그만큼 사용자가 그 사이에 다른 동작을 시작할 수 있게 됐다(재진입은 기존 메인 렌더와 같은 처리: 이 카드는 건드리지 않음).
- `ShutdownBackend()` 공개 메서드를 없앴다. 이 저장소 안의 호출자는 모두 고쳤지만(`gui`·`clients` 의 `.cs` 검색), 저장소 밖에서 부르는 곳은 확인할 수 없다.

## 증거 파일

`repro_before_fix.txt` · `repro_after_fix.txt` · `shutdown_after_fix.txt` · `shutdown_arm_on_ui_thread.txt` · `harness_Program.cs.txt` · `harness_ShutdownScenario.cs.txt` · `harness_csproj.txt` · `falsification_arms.txt` · `local_runs.txt` · `text_lint.txt`

🗿 MoAI
