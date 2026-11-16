[2026-10-15 23:09 KST] GUI-C-248 보고서 (작성 중: 앱을 띄우는 W12 1회는 리더 알림 대기) — W12 5회차 60초 시간 초과 (Refs #251)

## 1. 원인 (코드로 확정, 재현은 하지 않음 — 리더 지시 23:07)

CI main `04cde5f3` run 38723968216 의 W12 는 1~4회차가 약 2초에 끝나고 5회차만 60초 시간 초과였다(리더 발췌 `w12_extract.txt`).

- 대기 조건(수정 전, `QuickStartWalkthroughScenarios.cs:558`): `chain != previousTimes && chain.Contains("enhance_basic=Applied") && StatusBarText.Contains("Exposure index:")`. `previousTimes` 는 직전 회차의 `ChainStatusText` 문자열이다.
- 그 문자열은 앱이 만든다(`MainWindowViewModel.cs:3200`): `$"{chain.Summary}; {chain.Timings}; display input=…"`. `chain.Timings`(`ProcessingChain.cs:103`)는 단계 시간을 `{ElapsedMs:0} ms` 로, 즉 **정수 밀리초**로 적는다.
- CI 1~4회차 값: `preprocess` 334/334/332/337 ms, `enhance_basic` 393/412/390/386 ms, `gsvg`/`ai_bone_suppress` 0 ms. 값의 범위가 좁아서 5회차가 4회차와 **글자 그대로 같은 줄**을 낼 수 있다.
- 인과: 줄이 같으면 `chain != previousTimes` 가 거짓이다. 그 조건은 줄이 바뀌어야만 참이 되므로, 5회차가 정상적으로 끝나도 시험은 "변화 없음"으로 읽고 60초를 채운다. 앱이 멈춘 것이 아니라 시험의 대기 조건이 결과를 알아보지 못하는 것이다.
- 한계: 발췌의 타임아웃 스냅샷에는 상태줄 값이 없어서(66줄로 잘림) 5회차 줄이 4회차와 같았다는 것을 CI 기록에서 **직접 읽지는 못했다**. 같은 줄이면 반드시 멈춘다는 것은 코드로 확정이고, "이번에 실제로 같았다"는 것은 추론이다. 앱이 실제로 멈췄을 가능성(#268 계열)은 이 증거로 배제하지 못한다.

## 2. 수정

시간을 늘리지 않고, 회차마다 반드시 바뀌는 신호로 대기를 바꾼다. 앱은 렌더 번호(`RenderRecord.Id`, `++_renderSequence`, `MainWindowViewModel.cs` `SetRender` 앞의 커밋)를 UI 에 내놓지 않아서, 앱에 한 줄을 더했다.

- `MainWindowViewModel.cs`: `RenderIdText` (`"render #N"`, 첫 렌더 전 0)를 추가하고 `RenderDerivedProperties` 에 올려 렌더가 바뀔 때마다 알림이 나가게 했다.
- `MainWindow.xaml`: `ChainStatusText` 상태줄 항목에 `AutomationProperties.HelpText="{Binding RenderIdText}"` 를 붙였다(화면에 보이는 것은 바뀌지 않음, UIA 만 읽는다).
- `QuickStartWalkthroughScenarios.cs`: `GuiApp.ReadHelpText`, 순수 함수 `RenderIdOf`/`NewRenderIsDone`을 추가하고 W12 가 `NewRenderIsDone(이전 번호, 현재 번호, 체인 줄, 상태줄)` 를 쓰게 했다. 새 조건은 번호가 이전보다 커야 참이고, 체인 완료(`enhance_basic=Applied`)와 `Exposure index:` 조건은 그대로 둔다.

## 3. 반증 (앱 없이, 1회)

새 Fact `W12w_TheWaitForANewRender_EndsWhenTheChainLineRepeats` (옛 조건은 `OldRenderIsDone` 으로 시험 파일에만 남김). 같은 줄을 두 번 주었을 때:

- 옛 조건: 1000번 물어도 거짓 (영원히 끝나지 않음).
- 새 조건: 번호가 같으면(4,4) 거짓, 번호가 4→5 로 움직이면 같은 줄이어도 참, 체인이 덜 끝났거나 번호를 못 읽으면(-1) 거짓.

실행(`run_w12w.txt`): 통과 1/1, 5 ms. 가용 메모리 5800→5881 MB, 남은 ImageProcTest/testhost/xpe_ai_worker 0.

새 조건을 `>=` 로 약화시킨 변형(반증, `run_w12w_mutated.txt`): 실패 `Assert.False() Failure Expected: False Actual: True`. 원래대로 되돌린 뒤 다시 빌드했고 오류 0, 되돌린 파일은 변형 줄 0건·원래 줄 1건.

빌드: 앱 `ImageProcTest.csproj` 와 `ImageProcTest.E2ETests.csproj` 모두 오류 0 (앱 DLL 23:09 갱신).

## 4. 앱을 띄우는 W12 1회

리더 알림 대기. (post 의 QA-B-221 시험이 끝난 뒤.) 결과는 이 절에 추가한다.

## 5. 한계

- 위 1절의 한계: 실제 CI 실패가 "같은 줄" 때문이었는지는 직접 관측하지 못했다.
- 렌더 번호는 앱이 새로 내놓는 값이다. 앱 코드 변경이 한 줄이지만 `gui/` 변경이므로 앱을 띄우는 W12 1회로 `render #N` 이 실제로 UIA 로 읽히는지 확인해야 한다(미실행).
- Native 전체·IntegrationTests 전체는 돌리지 않았다.
- 로컬 기존 변경(`.claude/scheduled_tasks.lock`, GUI-C-231/232 증거 등)은 건드리지 않았다.

## Card Cross-Check

| 카드 항목 | card |
|---|---|
| 1. 대기 조건이 읽는 것·놓칠 수 있는 갱신을 줄 인용으로 확정 | GUI-C-248 (1절) |
| 2. 재현 | 리더 지시로 생략(1회 반복으로 안 잡힘), 앱 W12 1회는 4절 대기 |
| 3. 원인 수정(60초를 늘리지 않음) | GUI-C-248 (2·3절) |
