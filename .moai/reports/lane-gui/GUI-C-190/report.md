# GUI-C-190 — 같은 백엔드 위 두 Apply 의 순서 역전 (Codex #42 잔여, 높음) (#225, #130)

레인: gui · 푸시 없음(푸시는 리더). 카드: 메인 `.moai/lanes/gui/inbox/GUI-C-190.md`. 5절 형식(주장 / 증거 / 기준 귀속 / 미검증 / 잔여 위험). GUI-C-189(`12f18398`, 푸시됨) 위에 쌓았다.

## 1. 주장

1. **고쳤다**: 같은 백엔드에서 Apply A 와 B 가 겹쳐도, 늦게 *시작한* B 의 화면을 일찍 시작한 A 가 늦게 끝나 덮어쓰지 않는다. 메인 영상, Lane A/B, 상태 문구, 타이밍 줄, 오류 경로(알림·미리보기 stale 표시) 모두.
2. **결정**: 옛 요청을 *취소하지 않고 결과만 버린다.** 이유: 네이티브 호출은 중간에 끊을 수 없다(체인과 표시 호출이 동기 P/Invoke). 취소 토큰은 이미 있으나(`_renderCancellation`) 그것은 "결과를 적용할지"만 정한다 — 새 Apply 가 그 토큰을 취소하게 하면 같은 호출을 기다린 뒤 버리는 것 외에 얻는 것이 없고, 사용자의 Stop 버튼 의미(사용자가 누른 것만 센다 — `StoppedRenderCount`)가 섞인다. 그래서 별도 번호로 갈랐다.
3. xUnit1031 `#pragma` 를 파일 전체에서 실제 걸리는 **16곳**(21개 대기 줄)으로 좁혔다. 빌드 경고 xUnit1031 은 0건.

## 2. 증거

### 2-1. 원인 (코드로 확인)

`BackendLifecycle.IsCurrent` 는 생성 번호(`Generation`)와 백엔드 객체만 비교한다. 이 번호는 *종료·교체*에서만 오른다(`Begin`, `Bump`). 같은 백엔드의 Apply A·B 는 둘 다 같은 번호·같은 객체를 쥐므로 서로를 "현재" 로 판정한다 → A 의 Lane B 가 B 보다 늦게 끝나면 `LaneBImage = candidateImage` 가 B 의 레인을 덮는다(Codex #42).

### 2-2. 고침

| 조각 | 위치 | 내용 |
|---|---|---|
| 요청 번호 | `BackendLifecycle.cs` | `Request`(Apply 마다 `TakeRequest` 가 올림), `BackendTicket.Request`(0 = Apply 아님), `IsCurrent` 에 `ticket.Request == 0 \|\| ticket.Request == Request` 추가, `IsSuperseded(ticket)` |
| Apply 가 번호를 받음 | `MainWindowViewModel.cs` | `ApplyDisplayPipelineAsync` 가 `TakeRequestTicket()`. 이미 있던 점검 지점(await 뒤 메인 결과, Lane B 결과 직전, 두 `catch`, 렌더 뒤 타이밍 줄)이 그대로 새 조건을 쓴다 — 지점을 늘리지 않았다 |
| 로그 | 같은 파일 | `WhyStale(ticket)` — "새 Apply 가 시작됐다" 와 "백엔드가 종료·교체됐다" 를 구별해 적는다 |
| 다른 티켓 | AI 재시작 | `Take`(번호 0): Apply 가 재시작을 stale 로 만들지 않는다(시험 대조) |

### 2-3. 시험

- **실제 뷰모델 시나리오 4개** (`gui/ImageProcTest.SelfCheck/LifetimeScenarios.cs` 시나리오 4, `ScenarioBackend`): Apply A 의 체인 한 호출을 붙잡고(메인 체인 / Lane B 체인) → Lane B 설정을 바꿔 Apply B 를 같은 백엔드에서 끝까지 → A 해제(성공 / 실패). 기대: 메인 영상·Lane A/B 이미지 참조 동일, `LaneBIsStale` 거짓(A 가 자기 설정으로 되돌려 놓지 않음), 상태 문구·타이밍 줄 동일, 알림 수·미리보기 stale 사유 동일, B 이후 체인 호출 추가 없음.
- **단위 3건** (`BackendLifecycleTests`): 같은 생성의 두 요청에서 먼저 것이 stale / 번호 0 티켓은 Apply 에 영향받지 않음 / 가장 최신 Apply 도 교체되면 stale(이유는 "새 Apply" 아님). 기존 "백엔드를 쓰는 멤버" 표에 `TakeRequestTicket` 추가.
- 기존 SelfCheck 시나리오 5개는 그대로 통과(총 9).

### 2-4. 반증 5팔 (실제 소스, 빌드 성공, 소스 바이트 동일 복구) — `falsification_arms.txt`

| 팔 | 결과 |
|---|---|
| `IsCurrent` 가 요청 번호를 무시 | SelfCheck 시나리오 4 네 변형 전부 빨강 + 단위 1건 빨강 |
| Apply 가 요청 번호를 안 받음(`Take`) | 시나리오 4 네 변형 전부 빨강 |
| Lane B 결과 적용 직전 점검 제거 | 시나리오 3b 와 4(Lane B 성공) 빨강 |
| 메인 결과 점검 제거 | 시나리오 1 과 4(메인 성공) 빨강 |
| 실패 경로 점검 제거 | 시나리오 3 과 4(Lane B 실패) 빨강 |

복구 뒤: SelfCheck 통과(9개), 수명 단위 시험 16 통과. 소스 해시 `fbb83df76b5d`(뷰모델) · `4fe7b89e2e33`(수명).

### 2-5. 로컬 실행

- 빌드(gui·clients): 오류 0, xUnit1031 경고 0.
- `Category=Functional`: **354 통과 · 건너뜀 1(기존) · 실패 0**.
- Mock E2E(`ProcessingChainScenarios`+`AutomationReportBackendTests`+`MenuCommandScenarios`): 종료 코드 0, **22 통과 · 건너뜀 8 · 실패 0**, 앱 프로세스 잔류 0. (§4-2 참조: 직전 GUI-C-189 실행은 23/7 이었다.)
- 시험 중 최초 Functional 1건이 빨강이었다: 내 로그 문구(삼항식)가 `TheCandidateLane_RunsItsChainInTheBackground…` 의 "적용 직전 300자 안에 점검" 거리 조건을 넘겼다. 시험을 넓히지 않고 코드를 `WhyStale` 도우미로 옮겨 점검이 적용 지점에 붙어 있게 했다.

## 3. 기준 귀속

모든 숫자는 이 트리(`dev/gui`, `12f18398` 위 작업 트리)에서 이 실행으로 얻은 것이다. Native 백엔드는 로컬에서 돌리지 못했다(#98) — Native 에서의 동작은 관측하지 않았다.

## 4. 미검증

1. **E2E 로 두 Apply 가 겹치는 경우는 관측하지 않았다.** 겹침은 SelfCheck 의 실제 뷰모델 + 스크립트 백엔드에서만 만들었다. UI 자동화로 연속 Apply 를 눌러 같은 결과가 나오는지는 보지 않았다.
2. **Mock E2E 건너뜀이 7 → 8 로 늘었다.** 새로 건너뛴 것은 `A04_SelfCheck_ActuallyRuns…` 이고 사유는 "앱 옆에 `ImageProcTest.SelfCheck.exe` 가 없다"(그 테스트가 정한 정상 건너뜀; 같은 출력에 `Passed=True`). 직전 GUI-C-189 실행에서는 통과였다. 왜 이번엔 옆에 없었는지 **원인은 확인하지 않았다**(내 반증 팔이 빌드 산출물을 다시 만든 것이 후보이나 입증하지 않았다). 이 카드의 코드와 직접 이어진다는 근거는 없다. CI 의 `gui-shell-runners` 가 SelfCheck 를 직접 돌린다.
3. 같은 백엔드에서 *세 개 이상*의 Apply 가 겹치는 경우나, Apply 가 `ApplyBodyPartPreset`·로드 경로에서 연쇄로 시작되는 경우는 시나리오로 만들지 않았다(구조상 같은 `ApplyDisplayPipelineAsync` 를 지나므로 같은 번호 규칙이 적용된다는 읽기일 뿐).
4. 옛 Apply 의 *시작 효과*(상태 줄 "Applying display pipeline…" 같은 시작 문구)는 B 가 시작되면서 덮어쓰므로 이 카드의 대상이 아니다. 옛 요청이 끝난 뒤 B 가 아직 진행 중이면 화면은 B 의 시작 문구를 보인다는 것도 시험으로 단언하지 않았다.

## 5. 잔여 위험

- 옛 요청은 끝까지 돈다(취소하지 않음). 네이티브 쪽 부하(예: AI 게이트를 오래 쥐는 체인)는 줄지 않는다 — 결과만 안전해졌다.
- `Request` 는 단조 증가 `int`. 한 세션에서 21억 번 Apply 가 있어야 넘친다(실질 위험 아님).
- pragma 를 좁히면서 새로 추가되는 Task 차단 대기는 xUnit1031 경고로 보이게 됐다(의도). 단, 경고를 오류로 올리는 설정은 건드리지 않았다.

## 증거 파일

`falsification_arms.txt` · `local_runs.txt` · `text_lint.txt`

🗿 MoAI
