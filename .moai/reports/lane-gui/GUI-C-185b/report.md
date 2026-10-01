# GUI-C-185b — Codex #24 (B) 보류 수정: 재시작 실패 표시와 C-09 성공 단언 (#225 행 10, #130)

레인: gui · 푸시 없음(푸시는 리더). 감사 원문: 메인 `.moai/state/codex-archive/24.md` §(B) 발견 B1. 5절 형식(주장 / 증거 / 기준 귀속 / 미검증 / 잔여 위험)으로 쓴다.

## 1. 주장

1. **재시작이 실패해도 표시와 `Restart AI` 버튼이 남는다.** 실패는 새 상태 `InitFailed`(이 GUI 의 것)로 영속되고 문구에 원인(init 반환 코드 또는 예외 종류)이 들어간다. 다음 재시작이 성공하면 사라진다.
2. **C-09 가 "표시가 사라짐"만이 아니라 재시작 결과와 이후 상태까지 읽는다**: 성공 문구, 그리고 모듈이 보고한 `worker=Active; failures=0`.
3. GUI-C-186 의 디렉터리 변경(shutdown→init) 경로도 같은 실패 기록과 맞물린다(§3 의 답).

## 2. 증거

**B1 의 원인 그대로 확인**: `GuiAiSession.Restart` 가 먼저 `Shutdown()` 으로 시작 여부를 지운 뒤 `Init()` 이 실패하면, 상태 조회는 `Unknown`(표시 없음)이었다(감사 지적과 동일, `GuiAiRunner.cs` 수정 전 `QueryWorkerState` 가 `!_started → Unknown`).

**수정**
- 순수 클래스 `AiSessionTracker`(시작 여부·시작한 디렉터리·**마지막 시작 실패 사유**)를 `AiBoneSuppressionStage.cs` 에 두고 `GuiAiSession` 이 하나를 쓴다. `Init` 이 **시작이 실패하는 네 가지 모두**를 기록한다: 반환 코드 ≠ 0, `DllNotFoundException`, `EntryPointNotFoundException`, 그 밖의 예외(예외는 그대로 위로). 성공하면 지우고, **의도한 종료(`Shutdown`)도 지운다**(아무것도 안 돌고 문제도 없음).
- 상태 조회는 실패 사유가 있으면 모듈에 묻지 않고 `InitFailed(사유)` 로 답한다(`OwnStatus`). `AiWorkerState.InitFailed` 추가, `BannerFor` 문구: `AI session is not running: <사유> Images are returned unchanged. Use Restart AI to try again.`
- 표시·버튼의 가시성 규칙을 `AiWorkerMarkVisible`(= worker 꺼짐 **또는** 시작 실패)로 바꿨다. 툴바 바인딩이 이 규칙을 쓴다.
- **화면에서 읽을 수 있는 상태**: 값이 화면에 없어서(실패 횟수) 읽을 곳을 정했다 — AI 체크박스(`AiBoneSuppressionInChainCheckBox`)의 **`AutomationProperties.HelpText`**(= `AiWorkerStatusSummary`, 예 `worker=Active; failures=0; ceiling=3`). 근거: 이 저장소의 E2E 가 이미 같은 통로를 쓴다 — 뷰포트의 그려진 화소 해시를 `WorkbenchViewport` 의 `HelpText` 에서 읽는다(`ProcessingChainScenarios.DrawnHash`). 숫자는 모듈이 준 값이다.

**시험** (모두 Functional, 로컬 `Category=Functional` **300 통과 · 건너뜀 1(기존) · 실패 0**, 이 클래스 53건)
- 감사가 말한 순서 그대로: `Active` → 재시작의 shutdown 절반(`Unknown`) → init 실패 → **`InitFailed`**, 표시 보임, 문구에 `code -9` 와 `Restart AI`; 두 번째 실패가 사유를 교체; 재시도 성공이면 모듈에 묻는 상태로 복귀하고 표시가 사라짐 — `AFailedRestart_LeavesAnErrorState_NotUnknown_AndTheNextSuccessClearsIt`.
- 의도한 종료가 기록된 실패를 지움, 디렉터리 변경 경로(GUI-C-186)에서 init 이 실패해도 같은 상태 — `ADirectoryChange_WhoseInitFails_LeavesTheSameErrorState`.
- 상태 한 줄(`DescribeStatus`)이 모듈의 숫자와 실패 사유를 담음.
- 소스 결합 시험: `Init` 이 네 가지 실패를 모두 기록함(`Tracker.InitFailed(` 4곳)·`InitSucceeded`·`OwnStatus` 사용, 툴바가 `AiWorkerMarkVisible` 를 쓰고 `AiWorkerDisabled` 만으로 켜지지 않음. (호출 위치 시험은 고정 글자 창에서 "자기 메서드의 `WithLock(` 람다 안"으로 바꿨다 — 실패 기록이 잠금과 호출 사이에 들어와 글자 창으로는 부족해졌다.)
- **E2E C-09 보강**(Native 전용): 재시작 전 상태가 `worker=Disabled; failures=N; ceiling=N`(두 숫자 같음), 재시작 뒤 상태줄이 `AI session restarted`, 표시가 사라짐, **그리고** 상태가 `^worker=Active; failures=0; ceiling=\d+$`. C-08(Mock)은 같은 통로를 읽어 `worker=Unknown` 을 단언하므로 **HelpText 바인딩이 읽히는 것은 로컬에서 관측됐다**(C-08 통과).
- **반증 6팔**(실제 소스, 빌드 성공, 소스 바이트 동일 복구, 복구 후 53/53): 실패 시 상태를 다시 Unknown 으로 덮음(**감사 지적 그대로**) → 2건 빨강 · 성공이 옛 실패를 안 지움 → 1건 · 의도한 종료가 실패를 안 지움 → 1건 · 표시가 `Disabled` 에만 → 1건 · init 이 거절 코드를 기록 안 함 → 소스 시험 1건 · 툴바가 옛 플래그에 다시 묶임 → 1건 (`falsification_arms.txt`).
- **화면 관측**(진짜 앱, 상태는 리플렉션 주입, `observation.txt`): `InitFailed(code -9)` 에서 패널·문구·`Restart AI` 모두 **보임**, 두 번째 사유로 문구가 바뀜, `Active 0/3` 이면 사라짐, 각 단계의 `AiWorkerStatusSummary` 가 기대한 한 줄.

## 3. GUI-C-186 의 디렉터리 변경 경로와의 맞물림 (카드 질문의 답)

렌더 중 init 이 실패하면 `GuiAiRunner.Run` → `Init`(디렉터리가 달랐다면 먼저 `Shutdown`) → 아래 중 하나:
- **거절 코드**: 트래커에 `InitFailed` 기록, `Run` 은 `InterpretInit` 의 **기존 `not started`** 문구(`AI bone suppression not started: xpe_ai_init refused the configuration (code N)…`)를 돌려준다 → 체인 사유로 상태줄에 보인다.
- **DLL 없음 / 수출 없음**: 기록 후 예외가 `Run` 의 두 `catch` 에 잡혀 기존 `not started` 문구(`xpe_ai.dll was not found…` / `…does not export…`).
- 그 다음 `ReportChain` 이 상태를 다시 읽어 표시와 `Restart AI` 가 **남는다**(수정 전에는 `Unknown` 이라 사라졌다).
- **한 가지 어긋남**: 그 밖의 예외(위 둘이 아닌 것)는 `Run` 이 잡지 않아 체인 러너가 `ai_bone_suppress threw: …` 로 보고한다 — "not started" 문구가 아니다. 트래커는 기록하므로 표시는 맞게 남는다. 이 갈래는 고치지 않았다(카드가 요구한 확인이었고, 그 예외가 실제로 나는 경우를 아직 모른다).

## 4. 미검증 (Gaps)

1. **네이티브 경로는 실행되지 않았다.** `Init` 의 새 `catch`·재시작 실패·`xpe_ai_shutdown`→`xpe_ai_init` 는 컴파일과 Functional 시험(트래커 순서)으로만 확인했다. 시험은 **트래커의 상태 기계와 소스 텍스트**를 검증하지, 실제 DLL 이 init 에서 실패하는 장면을 만들어 보지는 않았다.
2. **E2E 로는 재시작 실패를 만들 수 없다**: Native 잡에서 `xpe_ai_init` 은 항상 성공한다. 그래서 실패 쪽은 Functional 과 화면 관측(주입)이 전부이고, C-09 는 성공 쪽만 보강했다.
3. C-09 의 새 단언(`worker=Active; failures=0`, 성공 문구)은 **CI 에서 처음 관측된다**. 로컬에서는 Native 가 없어 건너뜀. 기존 가정(스텁 + 가짜 모델 + 워커 경로에서 실패가 `DISABLED` 에 닿는다)은 그대로다.
4. 화면 관측은 상태를 **주입**한 것이다. 뷰모델이 `GuiAiSession` 의 답으로 그 상태를 만드는 경로는 이 트리에서 실행되지 않았다.
5. `Init` 이 던진 예외 중 `Restart` 가 잡지 않는 것의 사용자 문구(§3 의 어긋남)는 확인하지 않았다.

## 5. 잔여 위험

- `InitFailed` 는 DLL 이 없는 환경(Native 백엔드인데 `xpe_ai.dll` 이 없음)에서 **AI 단계를 한 번이라도 요청하면** 영속 표시를 띄운다. 의도(원인 표시)지만, AI 를 쓰지 않는 사용자는 단계를 요청하지 않으므로 표시가 뜨지 않는다 — 이 점은 관측하지 않았다.
- 표시는 렌더 직후·재시작 직후·백엔드 종료 직후에만 새로 읽는다(C-185 와 같음). 렌더 없이 시작 실패가 생기는 경로는 없다고 보았다.
- 사유 문구는 한 줄(코드 또는 예외 종류)이다. 같은 `-9` 의 원인을 가르지 않는다(C-185 의 결정 유지).

## 증거 파일

`falsification_arms.txt` · `local_runs.txt` · `observation.txt` · `harness_Program.cs.txt` · `text_lint.txt`

🗿 MoAI
