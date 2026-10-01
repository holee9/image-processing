# GUI-C-186b — Codex #25 보류 수정: AI 한 프레임 전체를 같은 잠금으로, 모델 경로는 한 번만 절대화 (#225 행 10, #130)

레인: gui · 푸시 없음(푸시는 리더). 감사 원문: 메인 `.moai/state/codex-archive/25.md`. 5절 형식(주장 / 증거 / 기준 귀속 / 미검증 / 잔여 위험). 이 위에 GUI-C-185b(`0a8a6112`)가 있다.

## 1. 주장

1. **AI 한 프레임의 세션 사용 전체 — 모델 파일 확인 → 필요하면 init → `xpe_bone_suppress` — 가 한 번의 `Gate` 보유 안에서 일어난다.** 다른 프레임의 디렉터리 변경이나 `Restart AI` 가 그 사이에 끼어들 수 없다.
2. **상대 모델 디렉터리는 앱 시작 때 한 번 정한 기준 디렉터리로 절대화**되고(리더 결정), **렌더 한 번에 한 번만** 절대화되어 **같은 문자열**이 파일 확인과 init 에 간다.
3. **경로 별칭**(8.3 짧은 이름·정션·심볼릭 링크·UNC)을 오가면 새 세션(실패 횟수 0)이 되는 것을 예상 동작으로 코드 주석에 적었다(`NeedsNewSession` 의 `<remarks>`). 대소문자 구분 디렉터리는 지원 범위 밖.

## 2. 증거

### 2-1. 잠금 범위 (finding 1)

수정 전 `GuiAiSession.Init` 의 `WithLock` 은 `xpe_ai_init` 이 돌아오면 끝났고 `xpe_bone_suppress` 는 잠금 없이 불렸다(감사가 인용한 `GuiAiRunner.cs` 의 init 과 호출 사이). 지금은 순서·잠금을 순수 코드 `AiFrame.Run(gate, ops, 절대 디렉터리)` 가 맡고, `GuiAiRunner` 는 실제 연산(`RealAiFrameOps`)과 버퍼를 대 준다:

| | 어디서 | 이유 |
|---|---|---|
| **게이트 안** | 모델 파일 확인(`CheckModelFileAt`), `InitSession`(필요하면 옛 세션 shutdown 포함), `xpe_bone_suppress` 와 그 출력 읽기 | 세션을 쓰거나 정하는 모든 것. 세션 교체가 이 사이에 끼면 안 된다 |
| **게이트 밖** | 입력 검증, 두 이미지 버퍼의 `xpe_alloc_image`·채우기, 끝난 뒤 `xpe_free_image` | AI 모듈 상태를 쓰지 않는다: `xpe_common` 을 거치고 이 프레임이 가진 버퍼만 만진다. 세션이 바뀌어도 이 일의 결과는 달라지지 않는다. 밖에 두어 게이트를 잡고 있는 시간이 필요한 부분으로 줄어든다 |
| **같은 게이트, 별도 보유** | 상태 조회(`QueryWorkerState`), `Restart`, 모든 `Init`/`Shutdown` | 상태 조회는 UI 스레드에서 렌더 직후에 읽으므로 프레임 보유에 접는 대신 **같은 게이트를 짧게 잡는다**(아래) |

**상태 조회는 한정 대기다**(내가 정한 부분): 게이트는 재진입 모니터라 프레임이 있는 동안 UI 스레드의 상태 읽기가 막힐 수 있고, 프레임은 조용한 워커 앞에서 모듈의 시간 예산(기본 5초)까지 보유한다. 그래서 상태 조회는 `TryWithLock(250 ms)` 로 잡고, 못 잡으면 **`null`("지금 못 읽음")** 을 돌려 화면은 보이던 것을 그대로 둔다. 모듈 헤더도 상태 조회는 진행 중인 호출을 기다리지 않는다고 하므로(`ai_api.h` `xpe_ai_worker_state`) 이 방식이 뜻과 맞는다. 대가: 프레임이 250 ms 넘게 돌고 있을 때 표시 갱신이 한 번 건너뛸 수 있다.

**시험** (실제 `AiFrame`·`AiSessionGate` 에 가짜 연산을 꽂은 것, DLL 없음):
- 렌더 A 가 호출 안에서 멈춘 사이 다른 스레드가 `Restart` 와 같은 일(shutdown+init)을 시도 → **A 가 끝날 때까지 못 들어간다**(로그 순서 `A: in the call` → `A: call returned` → `B: shutdown + init`) — `ARestart_AttemptedWhileAFrameIsInTheCall_WaitsForTheFrame`.
- 디렉터리가 다른 두 번째 프레임의 init 은 첫 프레임의 호출이 끝난 뒤 — `ASecondFrameWithAnotherDirectory_…`.
- 상태 읽기는 프레임 중에 지정한 시간 안에 포기하고(`false`), 프레임이 끝나면 즉시 읽힘 — `TheStatusRead_GivesUpWhileAFrameIsRunning_…`.
- 순서(파일 확인→init→호출), 모델 없으면 init 도 안 부름, init 실패면 호출 안 부름, 게이트가 재진입 가능.
- **소스 시험**: `GuiAiRunner.Run` 이 `AiFrame.Run(GuiAiSession.Gate, ops, directory)` 로 프레임을 부르고, `xpe_bone_suppress` 호출이 정확히 한 곳(`RealAiFrameOps`)이며, init·shutdown·상태 호출은 자기 메서드의 `WithLock(` 람다 안(C-185 시험)에 있음.
- 타이밍을 쓰는 시험이므로 **연속 6회**를 돌렸다: 11/11 통과를 6번(`local_runs.txt`).

### 2-2. 경로 (finding 2, 리더 결정)

- `AiBoneSuppressionStage.CaptureBaseDirectory()` 가 `App.OnStartup` 에서 **가장 먼저**(리소스 해석기 설치 직후, 명령줄 파싱 앞) 불려 시작 시 작업 디렉터리를 고정한다. 첫 호출이 이기고 이후는 무시한다. 상대 설정은 `Path.GetFullPath(설정, 기준)` 로 절대화되므로 **그 뒤 작업 디렉터리가 바뀌어도 같은 설정은 같은 절대 경로** → 새 세션 없음. 절대 경로는 기준을 건드리지 않는다. 설치 디렉터리 기준으로 바꾸는 것은 하지 않았다(배포 위치 결정과 묶임).
- **렌더당 한 번**: `GuiAiRunner.Run` 이 `NormalizeDirectory(...)` 를 한 번 부르고 그 문자열을 `AiFrame.Run` 이 파일 확인(`CheckModelFileAt`, 다시 정규화하지 않는 변형)과 `InitSession` 에 그대로 넘긴다. 프레임은 **상대 경로를 받으면 예외**를 던진다 — 호출자가 한 번 풀어 줘야 한다는 계약.
- **시험**: 기준을 캡처한 뒤 같은 상대 설정이 항상 `기준\data\models`, 작업 디렉터리와 다른 기준에서도 같음, 첫 캡처가 이김, 같은 설정=새 세션 아님·다른 디렉터리=새 세션, 절대 경로는 기준 무시(`ARelativeDirectory_IsResolvedAgainstTheBaseFixedAtStartup_…`; 정적 기준을 만지므로 같은 클래스 안에서 순차 실행하고 끝에 되돌림); 확인과 init 이 **같은 문자열**을 받음(가짜 연산으로 기록, `TheCheckAndTheInit_ReceiveTheSameDirectoryString`); 상대 경로를 프레임이 거부; `OnStartup` 이 기준을 캡처하는 위치(소스 시험).

### 2-3. 반증 7팔 (실제 소스, 빌드 성공, 소스 바이트 동일 복구, 복구 후 66/66)

| 팔 | 결과 |
|---|---|
| 게이트가 init 에서 끝나고 호출이 밖(**수정 전 동작**) | 교차 시험 3건 빨강 |
| 게이트가 잠그지 않음 | 교차 시험 3건 빨강 |
| 프레임이 상대 경로를 받아들임 | 1건 |
| 상대 경로를 다시 작업 디렉터리로 풂 | 기준 시험 1건 |
| 시작 때 기준을 캡처하지 않음 | 소스 시험 1건 |
| 러너가 디렉터리를 두 번 절대화 | 소스 시험 1건 |
| 상태 읽기가 한정 대기가 아님 | 소스 시험 1건 |

(`falsification_arms.txt`)

### 2-4. 로컬 실행

`gui.slnx`·`clients.slnx` 빌드 0 오류. `Category=Functional` **313 통과 · 건너뜀 1(기존) · 실패 0**. E2E(Mock): A06(러너 실행)·A11(세 축 개수)·C-08·비적용 설정 시나리오 통과.

## 3. 기준 귀속 (측정 대상)

- 위 숫자는 모두 이 트리(`dev/gui`, `0a8a6112` 위의 작업 트리)에서 이 실행으로 얻은 것이다. 감사 원문의 코드 줄 인용(`GuiAiRunner.cs:80-97`, `:194-222`, `AiBoneSuppressionStage.cs:111-122`)은 **감사 시점 코드의 것**이고, 수정 후 줄 번호는 다르다.
- 모듈 쪽 use-after-free(`xpe_ai_shutdown` 이 잠금 가드 범위를 끝내고 상태를 삭제하는 문제)는 post 의 QA-B-178 몫이라 건드리지 않았다. 이 수정은 **GUI 가 그 상황을 만들지 않게** 하는 쪽이다.

## 4. 미검증

1. **네이티브 경로는 실행되지 않았다.** `RealAiFrameOps`, 게이트 아래의 실제 `xpe_ai_init`·`xpe_bone_suppress`·`xpe_ai_shutdown` 은 컴파일됐을 뿐이다. 교차 시험은 **가짜 연산으로 순서와 잠금**을 검증하며, 실제 DLL 에서 shutdown 이 호출과 겹치는 장면은 만들지 않았다.
2. **E2E 로는 교차를 만들 수 없다**(Native 잡에서 프레임은 하나씩 돈다). 렌더와 `Restart AI` 를 동시에 누르는 화면 시험은 하지 않았다.
3. `App.OnStartup` 의 캡처가 **실제 시작 순서에서** 첫 번째로 실행되는 것은 소스 시험(텍스트 순서)과 읽기로만 확인했다. 앱을 시작해 기준 값을 읽어 본 것은 아니다.
4. 시험·하네스처럼 `App` 을 거치지 않는 곳에서는 기준이 **처음 쓰일 때의 작업 디렉터리**로 늦게 잡힌다(코드 주석에 적음). 제품 경로(`App`)는 영향 없음.
5. 한정 대기 250 ms 는 내가 고른 값이다. 근거 측정은 없다.
6. 열린 질문(감사 finding 1 의 "별도 수명 참조/실행 중 카운트" 설계)은 택하지 않았다 — 리더 지시가 같은 `Gate` 였다.

## 5. 잔여 위험

- **프레임이 길면 `Restart AI` 와 디렉터리 변경이 그만큼 기다린다.** 조용한 워커 앞에서는 모듈의 시간 예산(기본 5 s)까지다. `Restart AI` 는 `Task.Run` 에서 돌아 UI 는 막히지 않지만 버튼이 즉시 반응하지 않는 것처럼 보일 수 있다 — 관측하지 않았다.
- 상태 표시가 프레임 중에 한 번 건너뛸 수 있다(위). 다음 렌더 끝에 따라잡는다.
- 경로 별칭 전환은 새 세션을 만든다(예상 동작, 주석·보고서에 명시). 실제로 별칭을 오가는 사용은 없다고 보았으나 확인하지 않았다.
- 파일 확인과 `xpe_ai_init`/호출 사이에 **파일 자체**가 지워지는 경쟁은 막지 않는다(디렉터리 문자열 일관성과 다른 문제).

## 증거 파일

`falsification_arms.txt` · `local_runs.txt` · `text_lint.txt`

🗿 MoAI
