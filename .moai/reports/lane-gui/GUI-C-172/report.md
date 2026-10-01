# GUI-C-172 — 사람의 실제 조작(마우스로 메뉴 열기·Tab 이동)을 소수 시험으로 되찾기

레인: gui · 이슈: `#225` · Codex 감사 `#8` med 후속. 커밋은 푸시하지 않았다. **이 카드의 시험은 한 번도 실행하지 않았다**(카드 지시: 로컬 금지, 검증은 CI TRX).

## 결론

전용 시험 2개를 `GlobalInput` 게이트 뒤에 두었다(`Scenarios/Workflows/RealInputScenarios.cs`). **카드가 요구한 두 번째 시험("실제 Tab 으로 값이 커밋되는가")은 그대로 만들 수 없었다** — 이 앱에는 Tab 이 커밋을 좌우하는 텍스트 상자가 없다. 그래서 같은 사람의 조작 경로를 정직하게 겨누는 시험으로 바꿨다(아래 §1).

| 시험 | 실제 입력 | 대조(통제) | 관측 |
|---|---|---|---|
| `R01_TheViewMenu_OpensWithARealMouseClick` | 실제 마우스 클릭(`GlobalInput.Click`) | 클릭 전에는 `ShowLogsPanelMenuItem` 이 트리에 없음 | 클릭 후 2초 안에 그 항목이 보임 |
| `R02_RealKeystrokes_EditTheVoiBox_AndARealTabMovesOn` | 실제 Ctrl+A·키 입력·Tab(`GlobalInput`) | 시작 시 stale 표시 없음 · 상자가 키보드 포커스를 가짐 · 입력한 값이 입력 전과 다름 | 상자 글자가 입력값과 같고, stale 표시("parameters changed")가 뜨고, **Tab 직전·직후의 키보드 포커스 소유자를 프로세스 단위로 확인**(§1-1) |

## 1. 카드 전제의 정정 — Tab 은 커밋에 필요하지 않다

카드는 "실제 Tab 키로 값이 커밋되는지" 를 요구했다. 코드를 읽어 보니 **XAML 에 선언된 텍스트 상자 15개가 전부 `UpdateSourceTrigger=PropertyChanged`** 다(`gui/ImageProcTest` 의 `.xaml` 에서 `<TextBox` 를 전수 훑은 결과: `VoiWindowCenterInput` 외 14개, 하나도 `LostFocus` 가 아니다. C# 코드로 만드는 텍스트 상자는 훑지 않았다). 값은 입력되는 순간 뷰모델에 닿으므로, "타이핑만 하면 stale 이 아니고 Tab 을 눌러야 stale 이 된다" 는 대조가 **성립하지 않는다.** 그런 시험을 쓰면 대조 단계에서 즉시 빨강이거나, 대조를 빼면 Tab 을 증명하지 못하는 시험이 된다.

그래서 R02 는 **"실제 키 입력이 상자에 닿아 편집으로 반영된다"** 와 **"실제 Tab 이 포커스를 옮긴다"** 를 각각 따로 관측한다. 이름에도 "커밋" 을 넣지 않았다.

**내 이전 설명도 틀렸다.** GUI-C-171 의 `UiaInput` 설명과 `report.md §5` 가 "바인딩은 포커스를 잃을 때 값을 받는다" 고 적었는데, 이 앱의 텍스트 상자에는 틀렸다. `UiaInput.cs` 의 설명을 고쳤고 `GUI-C-171/report.md` 에 §5-1 로 정정과 카드가 요구한 범위 축소 문단을 넣었다. 코드 자체(`CommitByMovingFocus`)는 바꾸지 않았다 — 포커스 이동이 해롭지 않다는 것은 C-171 의 W07·W21·W22·W25 통과로 보였고, 필요하다는 근거는 없다. 지우는 쪽이 더 단순하지만 실행해서 확인할 수 없는 변경이라 남겼고 설명만 바로잡았다.

## 1-1. Codex 감사 #9 med 반영 — R02 의 Tab 검증이 다른 창으로 간 Tab 을 통과시키던 것

**지적.** 처음 R02 는 Tab 뒤 "원래 상자의 `HasKeyboardFocus` 가 거짓" 만 봤다. 키 입력에 600 ms 가 걸리는 동안 다른 창이 포커스를 가져가 Tab 이 그 창으로 가도 원래 상자는 포커스를 잃은 상태이므로 통과한다 — C-171 의 바로 그 실패 모양이다.

**수정.**
1. **Tab 직전**: 키보드 포커스가 아직 이 앱의 중심 상자인지 다시 단언한다(`HasKeyboardFocus` + UI Automation 이 보고하는 포커스 요소의 AutomationId·프로세스 id). 아니면 Tab 은 그 상자에 보내지지 않는다.
2. **Tab 직후**: 포커스가 **앱 프로세스 안**에 있는지(프로세스 id 비교), **원래 상자가 아님**, 그리고 **다음 텍스트 상자 `VoiWindowWidthInput`** 인지를 이 순서로 단언한다. 순서가 중요하다 — 앱 밖이면 "Tab 이 다른 창으로 갔다" 가 먼저 나온다. 포커스가 상자를 떠날 때까지 최대 2초 폴링한다(고정 300 ms 대기를 대체).
3. 실패 메시지마다 전경 창 정보(`DescribeWhatIsInFront`)와 포커스 요소 설명을 넣었다.

**기대 후속 요소를 확정한 근거.** `AnalysisPanel.xaml` 에서 Center 와 Width 상자는 한 `Grid` 안의 두 `StackPanel` 에 문서 순서대로 있고, 패널·`MainWindow` 어디에도 `TabIndex`·`IsTabStop`·`KeyboardNavigation` 이 없다. 기본 탭 순서(문서 순서)에서 Center 다음은 Width 다. **XAML 을 읽은 정적 근거일 뿐 실행으로 확인하지 않았다** — 틀렸다면 CI 에서 "Width 가 아니라 X" 라는 메시지로 바로 드러난다.

**대조 증거** `r02_tab_check_proof.txt`: 소스에서 추출한 6개 점검이 모두 성립한다 — Tab 직전 재단언, Tab 직후 앱 안(프로세스 id)·원래 상자 아님·다음 상자, **Tab 뒤에 `HasKeyboardFocus` 만 보는 단언이 남아 있지 않음**, 실패 메시지에 전경 창 정보.

## 2. 실제 입력 경로를 쓰는지 — 코드 대조

`real_input_proof.txt`(`RealInputScenarios.cs` 에서 본문을 추출해 생성). 요약:

- 두 시험 모두 **첫 문장이 `GlobalInput.Require(...)`** 다 — 게이트가 닫혀 있으면 무엇이 전송되기 전에 건너뛴다.
- **R01 의 행동 줄은 `GlobalInput.Click(viewMenu!)` 하나**이고, **R02 의 행동 줄은 `GlobalInput.SelectAll()`·`Type(...)`·`Press(TAB)` 셋**이다.
- 본문 안에서 앱에 **작용하는 UIA 호출**(`Expand`·`Collapse`·`Invoke`·`.Text =`·`Patterns.`·`UiaMenu.Open`·`UiaInput`)을 찾았을 때 **0건**이다. UIA 는 준비(`UiaMenu.CollapseAll`, `OpenParameters`, `SelectBodyPart`)와 읽기·`Focus()`(캐럿 위치)에만 쓴다. 이 목록도 같은 파일에 있다.
- `input_scan_after.txt`: E2E 프로젝트의 **직접** 입력 호출 0건(게이트 파일 제외). 새 호출은 전부 `GlobalInput.cs` 안에 있다(`Type`·`SelectAll` 추가).

## 3. 검증 계획 — 로컬에서는 돌리지 않았다

CI 의 Mock·Native E2E TRX 에서 다음 두 이름이 **`Passed`** 여야 한다(건너뜀이면 게이트가 닫힌 것이고 리더가 넣은 "게이트 건너뜀 0건" 검사가 잡는다):

- `ImageProcTest.E2ETests.Scenarios.Workflows.RealInputScenarios.R01_TheViewMenu_OpensWithARealMouseClick`
- `ImageProcTest.E2ETests.Scenarios.Workflows.RealInputScenarios.R02_RealKeystrokes_EditTheVoiBox_AndARealTabMovesOn`

빌드는 오류 0(`dotnet build clients/ImageProcTest.slnx`)이다.

## 4. 미검증 · 위험 — 중요한 것부터

1. **두 시험을 한 번도 실행하지 않았다.** 컴파일과 코드 대조뿐이다. 첫 CI 실행이 빨갛다면 시험 쪽 가정이 틀린 것일 수 있다.
2. **R01 의 대조**("메뉴를 닫으면 항목이 트리에 없다")는 C-170/171 의 S06·S07 이 보인 동작에 기대고 있다(닫힌 메뉴에서 항목이 null). 같은 앱·같은 UIA 이므로 성립할 것으로 보지만 이 시험 자체로 확인한 것은 아니다.
3. **R02 의 가정 넷**: `input.Focus()` 뒤 `HasKeyboardFocus` 가 참(창이 활성이어야 함), 입력값 `"4321"` 이 프리셋 직후의 중심값과 다름(`Assert.NotEqual` 이 대조로 있다), 입력 뒤 600 ms 안에 stale 표시가 뜸, **Tab 뒤 포커스가 `VoiWindowWidthInput` 에 놓임(XAML 문서 순서에서 읽은 것이며 실행으로 확인하지 못했다, §1-1)**. 마지막은 W22 가 같은 동작을 TypeCenter 직후 곧바로 관측해 통과한 것에 기대지만 키 입력은 글자마다 갱신되므로 시간 여유를 600 ms 로 두었다.
4. **R02 는 공유 픽스처를 건드린다.** `finally` 에서 프리셋을 다른 것으로 바꿨다가 원래로 되돌려 입력값과 stale 표시를 지운다(W22 와 같은 방식). 이 정리 자체도 실행해 보지 못했다.
5. 두 시험은 **다른 창이 없는 데스크톱에서만** 의미가 있다. 공유 데스크톱에서 `XPE_E2E_ALLOW_GLOBAL_INPUT=1` 로 돌리면 C-171 의 문제가 그대로 재현된다. 실패 메시지에 전경 창 정보(`DescribeWhatIsInFront`)가 들어가므로 CI 에서 빨갛다면 원인을 가리는 데 쓸 수 있다.
6. "Tab 으로 커밋" 은 시험하지 않았다(§1). 앞으로 `LostFocus` 바인딩의 텍스트 상자가 생기면 그때 그 대조가 성립한다.

## 5. text-lint

커밋 전에 `powershell -NoProfile -ExecutionPolicy Bypass -File tools/ci/Test-TrackedTextFiles.ps1` 를 파이프 없이 돌렸다. 결과는 `text_lint.txt`.

## 증거 파일

`real_input_proof.txt` · `r02_tab_check_proof.txt` · `input_scan_after.txt` · `text_lint.txt`

🗿 MoAI
