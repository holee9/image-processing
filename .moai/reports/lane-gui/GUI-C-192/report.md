# GUI-C-192 — AI 상태 읽기가 낡은 답을 먼저 그린다 (#225)

레인: gui · 푸시 없음(푸시는 리더). 카드: `.moai/lanes/gui/inbox/GUI-C-192.md`. 5절 형식. 시작 전에 main(`0135cfd9`)을 dev/gui 로 받았다(fast-forward: main 이 191·191b 를 이미 담고 있었다).

## 1. 주장

1. **고쳤다**: `AiStatusRefresher` 의 모든 요청이 번호를 갖고, 읽기는 시작한 번호를 기억한다. 답이 도착했을 때 그보다 **새 요청이 이미 있으면 그 답은 화면에 적용하지 않고** 가장 새 읽기를 다시 한다. 이전에는 낡은 답을 먼저 적용하고(`_another` 가 서 있어도) 바로 다시 읽었다 — 그 사이가 낡은 "Active"(또는 늦은 "끔")였다.
2. **규칙은 190 의 Apply 요청 번호와 같지만 상태는 공유하지 않았다.** "더 늦게 시작한 것이 있으면 앞선 것의 결과는 낡았다"는 같은 규칙이다(코드 주석에 연결을 적었다). 상태를 공유하지 않은 이유: 읽기는 Apply 말고도 요청된다(AI 재시작, 백엔드 교체 뒤의 재설정). Apply 만 번호를 매기는 `BackendLifecycle.Request` 로는 그 요청들의 순서를 못 가린다. 읽기 요청이 자기 번호를 갖는 편이 모든 요청 경로를 한 규칙으로 덮는다.
3. **시험으로 고정했다**: 191b 의 재현(첫 읽기를 붙잡고 새 요청 뒤에 놓음)을 단위 시험과 실제 뷰모델 시나리오(SelfCheck 7)로. 둘 다 **적용 지점의 변경 기록**으로 센다(폴링이 아님). 요청 번호 대조를 지우면 둘 다 빨강.
4. **C-09 의 `finally` 정리**: 창 너비 복원을 **맨 먼저** 하고, 모든 단계를 각각 보호한다. 정리 중 예외가 나도 복원에는 닿는다. 시나리오 자체가 실패했으면 그 실패를 정리 실패가 덮지 않고, 시나리오가 통과했는데 정리가 실패하면 `AggregateException` 으로 알린다.
5. **191b 에서 내가 만든 틀린 문구를 고쳤다**: `AnnounceFaultInjection` 의 로그가 `--automation-fault ai-worker-disabled` 만 켠 앱에서도 "표시 파이프라인 호출이 일부러 던진다"고 적었다(거짓). 켜진 장치별로 문장을 쓴다.
6. **(기록) 시험 플래그의 통제를 적은 저장소 문서는 없다.** §2-5.

## 2. 증거

### 2-1. 고침

| 조각 | 위치 | 내용 |
|---|---|---|
| 요청 번호 | `AiBoneSuppressionStage.cs` `AiStatusRefresher` | `_requests` 가 `Request()` 마다 오른다. `Start()` 가 번호를 기억하고 `Complete` 가 대조: 세대·백엔드가 다르거나 번호가 낡았으면 적용하지 않고 `Start()` 로 다시 읽는다. `_another` 는 필요 없어져 지웠다(요청 번호가 같은 일을 한다) |
| 정리 | `ProcessingChainScenarios.cs` C-09 `finally` | 복원 먼저, 단계별 `Guard`, `bodyCompleted` 로 통과한 시나리오의 정리 실패만 던진다 |
| 문구 | `MainWindowViewModel.cs` `AnnounceFaultInjection` | 장치별 문장 |

### 2-2. 시험

- 단위(`AiStatusRefresherTests`): `AnAnswerOlderThanANewerRequest_IsNeverApplied_OnlyTheNewestIs`(첫 읽기를 붙잡고, 새 요청, 놓음 → 적용 기록은 정확히 `[fresh]`, 낡은 값은 한 번도 없음), `AnAnswerWithNoNewerRequest_IsStillApplied`(규칙이 모든 답을 버리지 않음). 기존 `RequestsThatPileUpWhileAReadRuns…` 는 새 규칙에 맞춰 고쳤다: 읽기 2번은 그대로, **적용은 2번 → 1번**(낡은 답은 안 보임).
- 실제 뷰모델(SelfCheck 시나리오 7): 첫 읽기가 모듈 안에서 붙잡히고(읽기를 시작할 때 본 상태 `failures=0` 로 답한다), Apply B 가 끝나 새 읽기가 요청된 뒤 놓는다. 화면 변경 기록이 **정확히 `worker=Active; failures=2; ceiling=3` 한 번**.
- `AiWorkerPanelLayoutTests` 의 C-09 소스 대조에 정리 순서·보호·`bodyCompleted` 추가. `FaultAnnouncementTests` 3건(소스 대조).

### 2-3. 반증 (실제 소스, 빌드 성공, 복구 바이트 동일)

`falsification_arms.txt`, `falsification_arm_c09_order.txt`:

| 팔 | 결과 |
|---|---|
| 요청 번호 규칙 제거 | 단위 2건 빨강(`…IsNeverApplied…`, 기존 파일업 시험) + **SelfCheck 7 빨강**: "the screen showed 1 change(s) [worker=Active; failures=0; ceiling=3]" — 낡은 답이 실제 화면에 오름 |
| 새 요청이 번호를 올리지 않음 | 같은 2건 + SelfCheck 7 빨강 |
| C-09 가 복원을 맨 뒤로 | 레이아웃 시험(소스 대조) 빨강 — **첫 시도는 아무것도 안 깨서 초록이었다**(팔이 잘못 쓰여 복원이 여전히 맨 앞이었다). 팔을 고쳐 다시 돌렸다(`falsification_arm_c09_order.txt`) |
| 공지가 표시 장치 문장을 항상 씀 | `FaultAnnouncementTests` 1건 빨강 |

### 2-4. 실행

빌드(gui·clients): 오류 0, xUnit1031 0. `Category=Functional`: **372 통과 · 건너뜀 1(기존) · 실패 0**. SelfCheck: 13 시나리오, **12회 연속 0 실패**(1.9–2.0 초). Mock E2E(ProcessingChain·AutomationReportBackend·MenuCommand·RunnerVerdictWording·M01·WindowMinimumWidth·FailedRender·ViewportTruth): 종료 0, **53 통과 · 건너뜀 7(Native 전용) · 실패 0**.

시나리오 7 을 만드는 과정에서 알게 된 것: 처음 두 번은 시나리오가 틀렸다 — (1) 새 요청이 **만들어지기 전에** 붙잡힌 읽기를 놓았다(Apply B 의 AI 호출이 시작됐다는 사건은 B 의 요청이 아니다; 요청은 B 의 체인이 보고될 때 나온다), (2) 그 사건을 `LastChain` 변경으로 잡으려 했는데 B 의 체인이 A 의 것과 같아(레코드 동등) 변경이 없다. 체인 보고 로그 줄로 기다리게 했다. 또 이 시나리오에서 Apply A 는 B 가 곧바로 시작해서 **190 의 Apply 요청 번호로 버려진다**("a newer Apply started meanwhile") — 두 규칙이 서로 겹쳐 작동하는 것을 본 것이다.

### 2-5. 항목 4 — 시험 플래그의 문서 통제 (찾아본 곳과 결과)

찾은 범위: 추적 중인 `docs/` 전체, `README.md`, `CHANGELOG.md`, `gui/ImageProcTest/README.md`, `docs/help/content/*.md`(도움말 5쪽), `docs/project/XPE-GUI-MENU-001`, `docs/post-processing/xpe/SHA-GUI-001`(소프트웨어 위험분석), `RTM-GUI-001`. 저장소 안의 "운영 문서"는 이 범위가 전부다(사용자 매뉴얼·IFU·릴리스 노트로 이름 붙은 문서는 추적되지 않는다).

- **`--automation-fault`(두 장치)를 언급한 문서: 없다.** `--automation-*` 스위치는 `gui/ImageProcTest/README.md`(E2E 절)와 `ci.yml` 에만 나오고, README 는 `--automation-raw/-report/-width/-height` 의 사용 예일 뿐 "시험용이며 출하 빌드에서 어떻게 다루는가"는 적지 않았다.
- **가장 가까운 문서**: `SHA-GUI-001` 의 HAZ-GUI-005("오해 유발 진단 정보(Mock)": 지속 경고, 운영 hard-fail 이 통제), 그리고 `XPE-GUI-MENU-001` 줄 162("보정 평가 조절은 시험 GUI 도구이며 제품 모드 임상 우회 조절이 아니다"). 둘 다 명령줄 시험 장치가 출하 빌드에서 켜질 수 있는 경우는 다루지 않는다.
- **코드가 이미 하는 통제**(문서에는 없음): 명령줄 전용, 인자 없으면 구성되지 않음, 켜면 창 제목에 "— FAULT INJECTION ARMED" 와 로그 한 줄.

**위치 제안(작성은 하지 않았다)**: (1) `SHA-GUI-001` HAZ-GUI-005 의 위험 통제에 "명령줄 시험 장치(`--automation-fault`)가 출하 빌드에서 켜질 수 있다 — 통제: 제목 표시줄 표지 + 로그 + 출하 빌드에서 제외할지의 결정" 행 추가(출하 제외는 결정 사항: 지금은 제외하지 않는다). (2) `gui/ImageProcTest/README.md` 의 자동화 절에 스위치 전체 목록(특히 `--automation-fault` 두 값)과 "시험 전용" 문구. (3) 릴리스 노트(`CHANGELOG.md`)에 시험 장치가 있다는 한 줄.

## 3. 기준 귀속

모든 숫자는 이 트리(`dev/gui`, 0135cfd9 위 작업 트리)에서 이 실행으로 얻었다. 문서 검색은 `git ls-files` 와 `git grep` 으로 추적 중인 파일에 한정했다.

## 4. 미검증

1. **실제 Native 게이트 대기에서 이 규칙이 만드는 지연**: 낡은 답을 안 보이는 대신, 읽기가 게이트 뒤에서 오래 막히고 그 사이 새 요청이 계속 오면(Apply 마다 요청) **그동안 화면의 상태가 갱신되지 않는다**(낡은 값이 아니라 더 이전 값이 그대로 보인다). 요청이 멈추고 마지막 읽기가 끝나면 최신이 나온다. 이 지연이 임상적으로 문제가 되는 길이인지는 실측하지 않았다.
2. 요청 번호 규칙의 Native 동작(C-09 에서 마크가 나오는 시점의 변화)은 다음 네이티브 CI 가 말한다.
3. 시나리오 7 의 붙잡힌 읽기는 "모듈이 AI 호출을 아직 보지 못한 시점"의 읽기라(`failures=0`) 실제 앱에서 어느 요청이 그런 읽기를 만드는지(시작 시 요청인지)는 가르지 않았다. 규칙은 어느 읽기든 같다.
4. 문서 검색은 추적 파일 한정이라 저장소 밖 문서(조직의 QMS, 매뉴얼 PDF)는 보지 못했다.

## 5. 잔여 위험

- 위 4-1: 읽기가 막히면 마크가 늦는다(낡은 값은 없다). 필요하면 "N 초 넘게 갱신이 없으면 '상태 확인 중' 표시" 같은 별도 설계가 필요하다 — 이 카드 범위 밖.
- `_another` 를 지웠다: 이전에 이 필드를 읽던 곳은 이 클래스 안뿐이었다(`grep` 확인). 기존 시험 한 건의 기대(적용 횟수)가 바뀐 것은 의도된 변화다.
- 정리 실패를 시나리오 통과 뒤에 던지게 해서, 예전엔 조용히 지나가던 정리 문제가 이제 C-09 를 빨갛게 한다(의도).

## 증거 파일

`falsification_arms.txt` · `falsification_arm_c09_order.txt` · `selfcheck_runs.txt` · `local_runs.txt` · `text_lint.txt`

🗿 MoAI
