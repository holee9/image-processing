# GUI-C-191b — Codex #52 보류: C-09 가 최소 너비에서 돌았는지 단언하지 않음 (#225)

레인: gui · 푸시 없음(푸시는 리더). 카드: `.moai/lanes/gui/inbox/GUI-C-191b.md`. 5절 형식. GUI-C-191(fa15495d) 위에 쌓았다.

## 1. 주장

1. **C-09 는 이제 창이 정말 최소 너비에 있는지 단언하고, 아니면 실패한다.** 창을 요구 한계보다 훨씬 작게(200 px) 줄여 창이 멈추는 너비를 읽고, 그것이 앱의 `MinWidth`(MainWindow.xaml 에서 읽음)를 창의 DPI 로 환산한 픽셀 폭과 같은지(±2 px), 검사가 끝날 때도 그 자리에 있는지 본다. Transform 패턴이 없거나 `CanResize=false` 여도 실패한다. 읽은 값이 메시지에 들어간다.
2. **"실제 최소 너비에서 도는 UI 시험" — 만들었다(M01).** Mock 에서 워커 상태를 `Disabled` 로 만드는 경로는 없었다 → **명령줄 전용 시험 장치 `--automation-fault ai-worker-disabled`** 를 추가했다(기존 `display-pipeline-after:N` 과 같은 조건: 명령줄에서만, 없으면 아무것도 안 바뀜, 켜면 창 제목에 표시). 이 앱을 띄워 창을 실제 최소 너비로 줄인 뒤 배너와 Restart AI 가 UI Automation 트리에 있는지 본다(`AiWorkerMarkAtMinimumWidthScenarios.M01`).
3. **반증**: 패널을 이전 ToolBar 레이아웃(a120c547)으로 되돌리면 **M01 이 빨개진다** — 소스 대조가 아니라 실제 창에서(§2-2).
4. **2.5 초 상한의 근거 수집**: C-09 가 시도마다 "체인 텍스트가 보인 뒤 배너까지의 시간"을 로그에 찍는다(`after the chain text: N ms` 또는 `not within N ms`). 상한은 늘리지 않았다.
5. **한계 기록(고치지 않음) — `AiStatusRefresher` 의 낡은 읽기**: **재현된다**(모델에서). §2-4.

## 2. 증거

### 2-1. DPI 환산의 근거 (실측)

이 기계: `GetDpiForWindow` = **96** → 앱의 `MinWidth` 1280 DIP = 1280 px. 같은 실행에서 창을 200 px 로 줄이라고 했을 때 창이 멈춘 너비 = **1280 px** (M01 로그: `MinWidth 1280 DIP at 96 dpi = 1280 px expected; the window shrank to 1280 px; width after the check 1280 px; CanResize=True`). 환산식(`DIP × dpi / 96`)은 순수 계산 시험으로 96·120·144·192 dpi 에서 1280·1600·1920·2560 px 을 고정했다. **다른 DPI 에서 실제 창으로 재지는 못했다** — 이 기계의 DPI 를 바꾸지 않았다(§4-1).

### 2-2. 반증 (실제 소스, 빌드 성공, 복구 바이트 동일) — `falsification_arms.txt`

| 팔 | 결과 |
|---|---|
| 패널을 ToolBar 로 되돌림(a120c547 의 MainWindow.xaml) | **M01 빨강**: "At the window's minimum width (1280 px) the mark is not in the automation tree: banner NOT found, Restart AI NOT found … the window shrank to 1280 px" + 레이아웃 시험 4건 빨강 |
| 창을 줄이지 않음(`Resize` 제거) | **M01 빨강**: "The window is not at its minimum width … shrank to 1560 px … PROBLEM" — 넓은 창에서 우연히 통과하던 길이 막힘 |
| 판정 규칙이 항상 문제 없음 | 규칙 시험 1건 빨강 |
| 주입 상태 무시 | SelfCheck 시나리오 6 + M01 빨강 |
| 파서가 장치를 무시 | 파서 시험 2건 + M01 빨강 |
| C-09 가 최소 너비 단언을 지움 | **소스 대조 시험만** 빨강(C-09 는 Native 전용이라 UI 로는 안 돈다; 이 단언의 UI 증명은 M01 이 한다) |

### 2-3. 시험·실행

- 새 시험: `AiWorkerMarkAtMinimumWidthScenarios.M01`(UI), `WindowMinimumWidthTests` 8건(환산·판정 규칙·XAML 의 MinWidth 읽기), `AutomationArgsTests` 7건 추가(장치 수용·정확한 철자만·두 장치 동시·기본 꺼짐), SelfCheck 시나리오 6(장치가 없으면 백엔드 그대로, 켜면 정확히 "switched off 3 of 3", 두 장치 동시 표기), `AutomationRunSelectionConsumptionTests` 제외 목록에 새 필드 사유 추가(이 시험이 새 필드를 "소비 안 됨"으로 잡았다 — 의도된 보호).
- 빌드: 오류 0, xUnit1031 0. `Category=Functional`: **367 통과 · 건너뜀 1(기존) · 실패 0**. SelfCheck 12 시나리오, **12회 연속 0 실패**(1.8–2.3 초). Mock E2E(ProcessingChain·AutomationReportBackend·MenuCommand·RunnerVerdictWording·M01·WindowMinimumWidth·FailedRender·ViewportTruth): 종료 0, **53 통과 · 건너뜀 7(Native 전용: A03, R06, C02, C04, C06, C07, C09) · 실패 0**.

### 2-4. 항목 4 — `AiStatusRefresher` 낡은 읽기 (재현 시도 결과)

코드를 읽으면 `Complete` 는 요청 번호가 아니라 **세대·백엔드 동일성**만 대조하고, 읽기 중에 새 요청이 와 있어도(`_another`) 도착한 답을 **먼저 적용한 뒤** 다시 읽는다. 그래서 낡은 답이 한 번 화면에 오를 수 있다.

실험(임시 SelfCheck 코드, 제거함; `stale_status_read_observation.txt`): 실제 뷰모델 + 스크립트된 AI 세션, **첫 읽기가 모듈 안에서 붙잡혀** 들어갈 때 본 상태로 답한다. 그 사이 Apply B 가 끝나고(상태 읽기는 합쳐져 대기), 붙잡힌 읽기를 놓는다. 5회 모두:

```
t=0    summary='worker=Unknown'                      (B 의 화면: 읽기가 아직 안 와서 Unknown)
t=154  summary='worker=Active; failures=0; ceiling=3'   ← 낡은 읽기의 답(읽기 시작 시점의 상태)이 적용됨
t=154  summary='worker=Active; failures=2; ceiling=3'   ← 바로 이어 다시 읽은 새 값
```

- **재현됨**: 낡은 값이 속성 변경으로 한 번 나타나고, 바로 다음 읽기로 고쳐진다. 이번 모델에서는 두 변경이 같은 밀리초였다.
- 지속 시간은 "두 번째 읽기 한 번"이다. **실제 모듈에서는 읽기가 AI 세션 게이트(프레임이 최대 모듈 시간 예산만큼 쥔다) 뒤에서 기다릴 수 있어** 그만큼 길어질 수 있다 — 실측하지 않았다.
- 첫 시도는 2 ms 간격 폴링으로 샘플링했고 낡은 값을 **놓쳤다**("한 번도 안 보임"으로 읽힘). 속성 변경 이벤트로 기록하자 보였다 — 측정 장치가 결과를 바꾼 사례라 적어 둔다.
- 영향 범위: 마크(끔)는 `failures` 가 늘기만 하는 모듈 상태라, 낡은 답이 "끔"을 지우는 경우는 이 실험에서 만들지 않았다(낡은 답이 Active 인데 새 답이 Disabled 이면 잠깐 마크가 늦는 방향). 지웠다 다시 켜는 방향은 확인하지 않았다.

## 3. 기준 귀속

모든 숫자는 이 트리(`dev/gui`, fa15495d 위 작업 트리)에서 이 실행으로 얻었다. 96 dpi 단일 기계다.

## 4. 미검증

1. **96 dpi 외의 DPI 에서 실제 창으로** 최소 너비가 환산식과 맞는지(120·144·192 dpi)는 재지 못했다(환산식은 계산 시험으로만 고정).
2. **C-09 를 Native 로 돌리지 못했다**(#98): 최소 너비 단언과 배너 시간 로그가 네이티브 CI 에서 어떻게 나오는지는 다음 CI 가 말한다. M01 은 주입된 상태로 **배치**를 본 것이지 모듈을 본 것이 아니다.
3. 낡은 읽기가 실제 모듈의 게이트 대기에서 얼마나 오래 보이는지(§2-4)는 실측하지 않았다.
4. M01 이 CI Native 잡에서도 같은 결과인지(래핑된 Native 백엔드 위) 보지 못했다.

## 5. 잔여 위험

- `--automation-fault ai-worker-disabled` 는 출하 앱에 남는 시험 장치다(명령줄 전용, 켜면 창 제목에 표시, 없으면 구성되지 않음). 기존 장치와 같은 조건이다.
- `AutomationRunSelectionConsumptionTests` 의 제외 목록에 사유를 한 줄 더했다 — 목록은 시험이 "소비되지 않는 필드"를 잡는 장치라, 필드를 추가하는 사람이 사유를 쓰게 한다.
- M01 은 앱을 한 번 더 띄운다(Mock 잡 시간 증가, 약 20초). `gui-e2e-native` 는 30분 한도에 가깝다는 191 보고서의 기록을 보면 이 비용도 합산 대상이다.

## 증거 파일

`falsification_arms.txt` · `stale_status_read_observation.txt` · `selfcheck_runs.txt` · `local_runs.txt` · `text_lint.txt`

🗿 MoAI
