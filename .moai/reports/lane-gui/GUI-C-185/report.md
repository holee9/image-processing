# GUI-C-185 — #225 행 10: "AI 워커 중단" 영속 표시 · 복구 버튼 · 호출 전 모델 파일 확인

레인: gui · 이슈: `#225`, `#130` · 시작 전 `origin/main`(`b659829b`, C-184 와 `xpe_ai_worker_state` 포함)을 `dev/gui` 에 병합했다(fast-forward). 푸시 없음(푸시는 리더).

## 결론

세 가지를 구현했다. **네이티브 호출(`xpe_ai_worker_state`, 재시작의 shutdown→init)은 이 트리에서 한 번도 실행되지 않았다**(컴파일만, §5 의 1번). 화면 쪽(표시가 나타나고 사라지는 것)은 진짜 앱에서 관측했지만 **상태를 리플렉션으로 주입한 것**이다.

| | 내용 |
|---|---|
| ① 영속 표시 | 툴바에 `AI worker switched off for this session after N of M failures in a row: images are returned unchanged until it is restarted.` — **모듈이 `DISABLED` 라고 보고할 때만** 보인다. N·M 은 모듈이 준 `consecutiveFailures`·`ceiling` 이고 3 은 코드 어디에도 쓰지 않았다 |
| ② 복구 | `Restart AI` 버튼(표시와 함께만 보임) → `GuiAiSession.Restart` = **shutdown → init 을 한 번의 `WithLock` 안에서**. 상태 조회(`QueryWorkerState`)도 같은 `WithLock` |
| ③ 사전 확인 | 호출 전에 `<모델 디렉터리>/bone_suppress.onnx` 가 있는지 GUI 가 직접 본다. 없으면 `AI bone suppression not attempted: no model at <경로>. The AI worker was not started and no failure was counted; …` |

## 1. 근거와 설계

- **상태의 출처는 알림이 아니라 `xpe_ai_worker_state`** (헤더: 알림은 읽는 쪽이 비우고 넘칠 수 있지만 상태는 남는다). `NOT_USED`·`ACTIVE` 는 아무것도 보이지 않고, 세션이 시작되지 않았거나 이전 DLL(수출 없음)이면 `Unknown` — 그때는 DLL 을 새로 읽지도 않는다.
- **조회·init·shutdown 을 한 잠금으로**(Codex #17, 헤더: "`xpe_ai_init`/`shutdown` 과 동시에 호출하면 use-after-free"). `QueryWorkerState`·`Restart` 가 `WithLock` 아래에 있고, 호출 위치를 소스 시험이 고정한다(§2).
- **언제 읽나**: 체인 결과를 보고한 직후(`ReportChain`), 재시작 직후, 백엔드 종료 직후. 렌더 중에 읽으면 모듈이 "마지막으로 끝난 호출 기준"으로 답한다(헤더) — 세 번째 실패가 진행 중이면 아직 `ACTIVE 2` 이므로 표시는 그 렌더가 **끝난 뒤**에 뜬다.
- **재시작 버튼은 `Task.Run`** 에서 돈다(init 이 워커 쪽 일을 할 수 있어 UI 스레드를 막지 않으려고).
- **모델 파일을 먼저 본다**(post 의 QA-B-176 재현): 워커 경로에서 `-9` 는 모델 부재, 워커 exe 못 찾음, 프로세스 생성 실패, 하트비트 이상 모두에서 나와 **코드만으로 원인을 못 가른다**. 파일이 없으면 모듈을 부르지 않으므로 워커가 뜨지 않고 실패 횟수도 소모되지 않는다. 있으면 지금처럼 부르고 코드를 그대로 보고한다.
- **실패 문구에서 코드 단정을 걷어냈다**: "A build without an inference runtime always ends here"(-3 전제)를 지우고 `NOT applied (code N)` 와 "코드는 모듈 것이며 같은 코드가 여러 원인일 수 있어 더 읽지 않는다" 로 바꿨다. `-3`/`-9` 를 이름으로 구별하지 않는다.
- **모델 디렉터리 입력칸**을 Parameters 탭에 추가했다(`AiModelDirectoryInput`). 설정은 있었지만 화면에서 바꿀 곳이 없었고, 사전 확인과 C-08/C-09 가 경로를 정해 주려면 필요했다.
- 입력을 되돌려 주는 워커 경로 실패와 출력을 건드리지 않는 in-process 실패(post 가 알려 준 차이)는 **둘 다 rc≠0 이면 화소를 쓰지 않으므로 영향 없음** — 시험 `AFailedCall_WhateverItLeftInTheOutput…` 에 두 경우를 넣어 고정했다.

## 2. 시험

| 시험 | 단언 |
|---|---|
| `AiBoneSuppressionStageTests` 신규 + 갱신(현재 이 클래스 전체) | 표시는 `Disabled` 일 때만(NotUsed·Active·Unknown 은 빈 문자열), 숫자는 모듈의 것(7 of 7 → 문구에 3 없음), `Active 0` 이면 표시 없음(복구 후), 상태 응답 읽기(`-6` 이면 출력 미사용, 모르는 상태값은 Unknown), 재시작 응답, 모델 파일 없음 → "not attempted: no model at <경로>"·코드 없음·"no failure was counted", 파일 있으면 통과·다른 이름은 불인정, 실패 문구가 원인을 단정하지 않음, 입력 복사/무변경 두 실패 모양 → RequestedNotApplied, **조회·init·shutdown 호출이 소스에서 정확히 한 곳(GuiAiRunner.cs)이며 앞에 `WithLock(`**, `Restart`·`QueryWorkerState` 선언이 `=> WithLock(` 로 시작 |
| `Collection_FindsTheKnownBindings` | 41 → 42(새 입력칸 `AiModelDirectory`, 이름 명시) |
| E2E **C-08**(갱신) | 모델 디렉터리를 **없는 경로**로 지정하고 메뉴 호출 → Mock: `requires the native backend`; Native: `not attempted: no model at <그 경로>`; 해시 불변, `AI-processed` 없음 |
| E2E **C-09**(신규, Native 전용) | 이름만 `bone_suppress.onnx` 인 파일을 둔 디렉터리 → 모듈이 호출돼 **반환 코드가 있는** 실패(`(code N`, DLL 없음·사전 거절 문구 아님)를 반복 → 표시(`AiWorkerBanner`)가 나타남 → 문구의 두 숫자가 같음(천장 도달, 3 을 쓰지 않음) → `Restart AI` → 표시가 사라짐 |

로컬: `Functional` **285 통과 · 건너뜀 1(기존) · 실패 0**. `AutomationReportBackendTests` 16 통과 · 건너뜀 1(A03 기존). `ProcessingChainScenarios`+`UnappliedSettingsScenarios`(Mock + 합성 영상): 통과, C-09 는 Native 전용이라 건너뜀, **C-08 통과**.

**반증 7팔**(실제 소스, 빌드 성공, 바이트 동일 복구, 복구 후 50/50): 활성 워커에도 표시 → 2건 · 천장 3 하드코딩 → 1건 · 모델 확인 생략 → 2건 · 호출 실패인데 상태를 읽음 → 1건 · 실패 문구가 원인을 다시 단정 → 1건 · 상태 조회가 잠금 밖 → 2건 · 입력칸 바인딩 소실 → 바인딩 개수 시험 1건 (`falsification_arms.txt`).

## 3. 화면 관측 (진짜 gui 앱, 상태는 주입)

저장소 밖 임시 도구(`harness_Program.cs.txt`)가 진짜 앱(Mock 백엔드)을 띄우고 뷰모델의 상태 필드를 **리플렉션으로** 바꾼 뒤 XAML 이 그린 것을 읽었다(`observation.txt`):
- 시작: 패널 접힘, 표시·버튼 안 보임.
- `Disabled 3 of 3`: 패널 보임, 문구 `…after 3 of 3 failures in a row…`, `Restart AI` 보임·활성. `Disabled 7 of 7`: `7 of 7`.
- `Active 2 of 3`: 다시 접힘.
- Mock 에서 `Restart AI` 를 누르면 상태줄 `AI session restart needs the native backend.` 그리고 표시가 사라진다(AI 세션이 없는 백엔드는 `Unknown`). 이 마지막 동작은 관측에서 **내 결함을 하나 찾았다**: 처음에는 그 경로에서 상태를 다시 읽지 않아 주입한 표시가 남았고, 거기서 다시 읽도록 고쳤다.

## 4. 범위 · 한 일 아닌 것

실제 입력 시험은 로컬에서 돌리지 않았다. C-08/C-09 의 Native 갈래는 CI 몫이다.

## 5. 미검증 · 한계

1. **네이티브 호출은 실행되지 않았다.** `xpe_ai_worker_state` 의 P/Invoke(`out int/uint` 서명), `QueryWorkerState`, `Restart` 의 shutdown→init 은 컴파일만 됐다. 모듈이 실제로 3번째 실패 뒤 `DISABLED 3 of 3` 을 돌려주는지, 그 상태가 **렌더가 끝난 뒤** 읽히는 시점에 맞는지, 재시작 뒤 `ACTIVE 0` 이 되는지는 **C-09 가 CI 에서 처음 관측**한다. C-09 는 "스텁 + 파일만 있는 가짜 모델 + 워커 경로" 에서 실패가 계속 세어져 `DISABLED` 에 닿는다는 **가정** 위에 있다(post 의 재현은 호출 1번씩이었다). 안 닿으면 C-09 는 6번 시도 뒤 "표시가 안 뜸"으로 빨강이 되며, 그것은 가정이 틀렸다는 관측이다.
2. **화면 관측은 상태를 리플렉션으로 넣은 것**이다. 뷰모델이 모듈 응답을 읽어 그 값을 만드는 경로(`RefreshAiWorkerStatus` ← `GetAiWorkerStatus` ← `QueryWorkerState`)는 이 트리에서 실행되지 않았다.
3. **렌더가 아닌 시점의 갱신이 없다.** 표시는 렌더 직후·재시작 직후·백엔드 종료 직후에만 새로 읽는다. 렌더 없이 모듈 상태가 바뀌는 경로(예: 다른 곳에서 호출)는 없다고 보고 폴링을 두지 않았다 — 이 가정은 확인하지 않았다.
4. 사전 확인은 **상대 경로를 프로세스의 작업 디렉터리 기준**으로 본다(모듈도 같은 프로세스에서 같은 기준으로 읽는다고 가정). 기본값 `data/models` 가 실제 배포 위치와 맞는지는 모른다.
5. `Restart AI` 는 `AiModelDirectory` 설정 값으로 init 한다. 모델 디렉터리를 바꾼 직후에는 적용(렌더) 전의 값일 수 있다(설정 스냅샷 규칙은 확인하지 않았다).
6. `-3`/`-9` 의 원인을 가르는 일은 하지 않았다(지시대로). 모델이 **있는데** `-9` 가 나는 경우(워커 exe 없음 등)는 "code -9" 로만 보고된다.
7. 흉부 한정(§4-9)·입력 척도 확인(QA-B-173 는 "호출자가 모델 스케일로 넣는다"고 답함 — ÷65535 가 그 스케일인지는 모델 카드가 정할 때까지 가정) 은 그대로다.

## 증거 파일

`falsification_arms.txt` · `local_runs.txt` · `observation.txt` · `harness_Program.cs.txt` · `harness_csproj.txt` · `text_lint.txt`

🗿 MoAI
