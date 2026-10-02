# GUI-C-191 — C-09 진단값이 나왔다: 모듈은 끔(state=2)을 알렸고, 앱이 표시를 안 했다 (#225)

레인: gui · 푸시 없음(푸시는 리더). 카드: `.moai/lanes/gui/inbox/GUI-C-191.md`(+ 리더 추가 절: 건너뜀/실패 결정). 5절 형식. GUI-C-190b(a120c547) 위에 쌓았다.

## 1. 주장

1. **표시가 없던 이유(확정 범위 안에서)**: 배너와 Restart AI 버튼이 `ToolBar` 의 마지막 항목이었다. WPF `ToolBar` 는 자리가 모자란 항목을 오버플로 팝업으로 보내고, 그 항목은 화면에도 UI Automation 트리에도 없다. **실제 앱을 띄워 관측했다**: 상태를 `Disabled` 로 읽히게 해 둔 채 창 너비 1560·1400 에서는 배너와 버튼이 트리에 있고, **앱 자신의 `MinWidth` 인 1280 에서는 둘 다 없다.** 패널을 ToolBar 밖 별도 행으로 옮기면 1280 에서도 있다. 사용자에게도 같은 결함이다(끔 알림이 오버플로 메뉴 뒤로 숨는다). **CI 러너의 창 너비는 관측하지 않았다**: 러너 창이 1280 에 가까웠다는 것은 이 관측과 CI 로그(배너 없음, 요약은 Disabled)가 서로 맞는다는 추정이다.
2. **시도마다의 보이지 않는 AI 호출 = 하위 레인(Candidate)의 체인**: 두 레인이 다르면 `RenderLanesAsync` 가 Candidate 체인을 한 번 더 돌리고, 그 체인은 메인 체인과 같은 설정(AI 단계 포함)이라 AI 세션을 한 번 더 부른다. 의도된 동작이다(비교의 두 레인은 레인 덮어쓰기만 다른 같은 파이프라인이어야 한다). 이것이 실패 한도 3 이 시도 2회 만에 찬 이유와 맞는다(§2-3). **CI 의 공유 앱에서 그 시점에 레인이 달랐는지는 직접 관측하지 않았다**(로그의 init 번호 +2 와 failures 산술이 그 설명과 일치할 뿐이다).
3. **GUI 쪽 실제 결함 하나를 고쳤다**: 워커 상태 읽기는 `ReportChain`(메인 체인 직후)에서 요청되고 레인보다 앞이다. 그래서 *하위 레인의 호출이* 워커를 끈 경우 다음 Apply 까지 마크가 안 떴다. 이제 레인이 끝난 뒤에도 읽기를 요청한다.
4. **C-09 를 고쳤다**: 최소 너비(1280)에서 돌리고(끝나면 원래 너비로 복원), 고정 2.5 초 `Sleep` 을 배너가 나타나는 사건 대기로 바꿨으며(상한은 같은 2.5 초, 늘리지 않았다), 실패 메시지가 **읽은 것을 그대로** 적는다. 이전 메시지("모듈이 끔을 알리지 않았다")는 같은 메시지 안의 요약이 `worker=Disabled` 였는데도 원인을 모듈로 돌렸다.
5. **리더 결정 반영**: SelfCheck/E2E 러너 시나리오의 "한도 안에 안 끝남·시작 못 함·원인 불명"은 건너뜀이 아니라 **실패**, 건너뜀은 "실행 파일 없음"에만 남겼다.
6. 단언은 느슨하게 하지 않았다(C-09 의 표시·두 수의 동일·재시작 뒤 `worker=Active; failures=0` 단언 그대로, 시험이 대조한다).

## 2. 증거

### 2-1. 관측: 배너가 트리에 있는가 (실제 앱, Mock + 임시 시험, 커밋 안 함)

상태를 `Disabled(3,3)` 로 읽히게 하는 임시 패치(뷰모델의 상태 읽기)와 FlaUI 임시 시험으로 같은 앱을 창 너비만 바꿔 관측했다. 임시 패치와 시험은 되돌리고 삭제했다(`observation_*.txt` 는 그때의 출력).

| 레이아웃 | 창 너비 | AiWorkerBanner | AiRestartButton |
|---|---|---|---|
| 원래(ToolBar 항목) | 1560 | 있음 | 있음 |
| 원래 | 1400 | 있음 | 있음 |
| 원래 | **1280** | **없음** | **없음** (같은 실행에서 `ClearAlertsButton` 은 있음) |
| 옮김(ToolBar 밖 별도 행) | 1560 / 1400 / **1280** | 있음 | 있음 |

(`observation_width_before.txt`, `observation_width_after.txt`. 초기 관측 `observation_before.txt`/`observation_after_move.txt` 는 상태를 시작부터 고정한 것이고, 처음 시도는 상태 읽기가 Unknown 으로 덮어 써서 무효였다 — 읽기 자체를 고정해 다시 쟀다.)

### 2-2. 원인 후보 중 기각한 것

- "요약과 배너가 다른 상태를 읽는다": **기각**. 둘 다 같은 `_aiWorkerStatus` 를 읽는다(`AiWorkerStatusSummary`, `AiWorkerBannerText`, `AiWorkerMarkVisible`; 한 곳 `ApplyAiWorkerStatus` 가 네 속성을 함께 알린다).
- "배너가 비동기로 늦게 붙는다": 로컬에서 시작부터 `Disabled` 인 경우와 읽기 몇 번 뒤 `Disabled` 가 되는 경우(처음 관측은 이미 읽기가 지난 뒤라 완전히 분리하지는 못했다) 모두 1560 에서 배너가 있었다 → 늦게 붙는 문제로는 설명되지 않는다. CI 의 6번 시도 동안 2.5 초씩 기다린 뒤에도 없었던 것과도 맞지 않는다.

### 2-3. 보이지 않는 호출

CI 로그(run 36903101428, 리더가 카드에 인용): 시도 1 뒤 읽기 `failures=1`, 시도 2 의 init 직전 `failures=2`, init 번호 #1,#3,#5…. 설명: 시도마다 호출 둘(메인 = 홀수 init, Candidate = 짝수 init). 상태 읽기는 메인 직후라 시도 1 은 `failures=1`, 그 뒤 Candidate 호출이 2 로 올리고, 시도 2 의 메인이 3(끔)을 만든다.

시험(SelfCheck 시나리오 5·5b, 실제 뷰모델 + 스크립트된 AI 세션):
- **5**: 레인이 다르면 한 번의 Apply 가 AI 호출 2번(메인+Candidate, 둘 다 AI 단계 요청). 끔(천장 2)이 레인 호출로 만들어졌을 때 마크가 화면에 나온다.
- **5b**: 레인이 같으면 AI 호출 1번.

### 2-4. 고침

| 조각 | 위치 | 내용 |
|---|---|---|
| 패널 이동 | `MainWindow.xaml` | 배너+버튼을 `ToolBar` 밖, 툴바 아래 별도 행(`Border`, `DockPanel.Dock="Top"`)으로. 기본 Collapsed, 마크 플래그가 Visible, AutomationId 그대로. 폭이 넓어져 배너는 줄바꿈(`TextWrapping`) |
| 레인 뒤 읽기 | `MainWindowViewModel.cs` | `RenderLanesAsync` 뒤(점검 통과 시) `RefreshAiWorkerStatus()` |
| 내부 노출 | `ImageProcTest.csproj` | `InternalsVisibleTo ImageProcTest.SelfCheck`(스크립트된 AI 세션을 SelfCheck 가 쓰기 위함) |
| C-09 | `ProcessingChainScenarios.cs` | 최소 너비 + 복원, 배너 사건 대기(상한 2.5 초 그대로), 읽은 것을 적는 메시지 |
| 규칙 | `RunnerVerdictWording.cs`, `AutomationReportBackendTests.cs` | `SkipsInsteadOfFailing`: 건너뜀은 NoExecutable 뿐, 나머지는 `XunitException`(상태줄 인용) |

### 2-5. 시험 · 반증 · 실행

- 새 시험: `AiWorkerPanelLayoutTests` 6건(패널·배너·버튼이 ToolBar 밖, 패널 구조·기본 Collapsed·트리거, 창 도크 한 행, C-09 소스 대조), `RunnerVerdictWordingTests` 5건 추가(갈래별 건너뜀/실패, 게이트 소스 대조), SelfCheck 시나리오 5·5b.
- 반증 7팔(실제 소스, 빌드 성공, 바이트 동일 복구): `falsification_arms.txt`(패널을 원래 레이아웃으로 되돌림 → 레이아웃 시험 4건 빨강 / 레인 뒤 읽기 제거 → 시나리오 5 빨강, 요약이 `worker=Active; failures=1` 로 남음 / C-09 최소 너비 제거 → 빨강 / 실패 문구가 다시 모듈 탓 → 빨강), `falsification_arms_skip_rule.txt`(모든 갈래 건너뜀 → 3건 빨강 / 느린 러너가 건너뜀에 합류 → 1건 빨강 / 게이트가 아무거나 건너뜀 → 소스 대조 빨강).
- 빌드: 오류 0, xUnit1031 경고 0. `Category=Functional` **360 통과 · 건너뜀 1(기존) · 실패 0**. SelfCheck: 11 시나리오, **12회 연속 0 실패**(1.8–2.1 초, 평균 1.9). Mock E2E(+TwoLane·AlertVisibility 포함): 종료 0, **41 통과 · 건너뜀 9 · 실패 0**(건너뜀 9 = Native 전용 7 + 레인 시험 L04·L06 — "프리셋이 체인 단계가 달라 Mock 이 거절"이라는 기존 사유; 앞선 카드의 실행 범위 밖이었다).

### 2-6. 스크린샷 — 새 행의 가독성 (리더 요청, 커밋 후 추가)

`screens/` (이 워크트리 `D:\workspace-github\xpe-gui\.moai\reports\lane-gui\GUI-C-191\screens\` 에만 있다 — `*.png` 가 .gitignore 대상이라 커밋하지 않았다): **실제 `MainWindow`** 를 같은 프로세스에서 그린 이미지(`RenderTargetBitmap`), 상태는 `Disabled(3,3)` 로 고정(임시 패치와 임시 코드, 둘 다 되돌리고 삭제). FlaUI 의 화면 복사와 `PrintWindow` 는 이 환경에서 창 안쪽이 하얗게만 나와 쓰지 못했다(그 시도의 이미지는 버렸다).

| 파일 | 레이아웃 | 너비 | 창 안에서 읽은 값 |
|---|---|---|---|
| `before-1560.png` | a120c547 (ToolBar 항목) | 1560 | 배너 보임(폭 555 px) |
| `before-1280.png` | a120c547 | **1280** | 배너 **보이지 않음**(`IsVisible=False`); 툴바 오른쪽 끝에 오버플로 화살표만 있다 |
| `after-1560.png` | 이 카드 (별도 행) | 1560 | 배너 보임(폭 1449 px) |
| `after-1280.png` | 이 카드 | **1280** | 배너 보임(폭 1169 px) |

`after-1280.png` 를 눈으로 봤다: 툴바 바로 아래 주황색 한 줄에 "AI worker switched off for this session after 3 of 3 failures in a row: images are returned unchanged until it is restarted." 가 한 줄로 다 보이고 오른쪽 끝에 Restart AI 버튼이 있다. 줄바꿈 없이 들어가므로 이 문구에서 행 높이는 늘지 않는다(§5 의 "두 줄" 우려는 현재 문구와 최소 너비에서는 해당 없음). `InitFailed` 문구(`AI session is not running: <사유> …`)는 사유 길이에 따라 길어질 수 있어 이 이미지로는 보지 않았다.

### 2-7. `gui-e2e-native` 잡 시간 (적기만, 범위 밖)

이 잡의 `timeout-minutes` 는 **30**(`ci.yml:737`)이다 — 카드의 "45분"이 아니다. 오늘의 실행: 성공 21–26 분(8d9c2185, 8bec255c, b659829b …), 53ec370a 25.5 분(실패), 82692af9 약 30 분(타임아웃으로 실패), 12f18398 30 분에 취소(= 같은 한도). 12f18398 의 로그에 소요가 찍힌 시험 114건의 합이 약 25.6 분이고, 앱을 새로 띄우는 시험(A07–A17 등)이 하나에 약 40 초, 최장 A12 135 초였다. **무엇이 늘렸는지는 가르지 않았다** — 시험이 늘고 앱 기동 비용이 시험마다 드는 것이 큰 몫이라는 정황이다. 시험별 시간표는 `gui_e2e_native_durations.txt`.

## 3. 기준 귀속

모든 숫자는 이 트리(`dev/gui`, a120c547 위 작업 트리)에서 이 실행으로 얻은 것이다. CI 시간·로그 인용은 `gh run view` 의 읽기 전용 조회(run 36903101428 의 job 110512052686 과 최근 실행 목록)다.

## 4. 미검증

1. **C-09 를 Native 로 돌리지 못했다**(#98). 카드가 요구한 "로컬 5회 + CPU 부하"는 불가능하다. 대신 같은 레이아웃·너비를 Mock 앱에 상태를 고정해 관측했다(§2-1). 네이티브에서 마크가 보이는지와 C-09 전체 통과는 다음 CI 가 말한다.
2. **CI 러너의 창 너비**를 관측하지 않았다(§1-1). C-09 가 이제 너비를 로그에 찍는다(`C09 window width before: …`).
3. **CI 에서 시도 중 레인이 달랐는지**를 직접 보지 않았다(§1-2). 공유 앱이 앞 시험의 Lane B 설정을 가진 채 C-09 를 시작했다는 것이 설명의 전제다.
4. 레이아웃 시험은 XAML 구조를 읽는다(렌더링 아님). 렌더링은 §2-1 의 일회성 관측, §2-6 의 이미지, C-09 가 본다. `Disabled` 문구가 1280 에서 한 줄에 들어가는 것은 이미지로 봤다. `InitFailed` 문구(사유가 길 수 있음)의 모습은 보지 않았다.
5. C-09 의 창 너비 복원이 공유 앱의 이후 시험에 영향을 주지 않는지는 Native 에서 확인하지 못했다(복원은 `finally` 에서 한다).
6. `gui-e2e-native` 시간 증가의 원인(§2-7)은 가르지 않았다.

## 5. 잔여 위험

- `InitFailed` 처럼 사유가 긴 문구는 줄바꿈으로 행 높이가 늘어 아래 작업 영역이 조금 줄 수 있다(마크가 보일 때만). `Disabled` 문구는 1280 에서 한 줄이다(§2-6).
- 하위 레인이 AI 호출을 하는 것은 의도이지만, **레인 하나로 실패 한도가 두 배로 빨리 찬다**는 점은 사용자에게 설명되지 않는다(마크의 숫자 "3 of 3" 만 보인다). 설계 결정 사항이라 건드리지 않았다.
- `InternalsVisibleTo` 로 SelfCheck 가 내부 타입을 본다(시험용 친구 어셈블리).
- 건너뜀을 실패로 올려서, 로컬에서 러너 실행이 느리거나 못 뜨는 환경의 A04·A06 은 이제 빨갛다(의도).

## 증거 파일

`falsification_arms.txt` · `falsification_arms_skip_rule.txt` · `observation_width_before.txt` · `observation_width_after.txt` · `observation_before.txt` · `observation_after_move.txt` · `screens/*.png`(4) · `gui_e2e_native_durations.txt` · `selfcheck_runs.txt` · `local_runs.txt` · `text_lint.txt`

🗿 MoAI
