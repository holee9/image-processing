2026-11-04T09:01:33+09:00 — GUI-C-258b / F283-1 / Refs #282 결과 보고

O: footer 표시와 상세 action 상태를 구분하면서 W01·W10의 기존 oracle를 보존한다.
KR: HEAD88c34bce에서 두 시나리오의 실패를 먼저 재현하고, 지정 reader 두 곳만 HelpText로 정정한 뒤 같은 두 시나리오를 통과시킨다.

승인 카드: D:/workspace-github/image-processing/.moai/state/codex-msgs/20261104-0855-gui.md. 별도 커밋·미푸시, 회귀는 W01·W10에 한정한다.

기준: D:/workspace-github/xpe-gui, dev/gui, HEAD88c34bce67bbcb9922c2dc1c973cde6317f4524e. 대상 두 source파일의 사전 차이는 없었다.
확인: WorkflowScenarios.cs:70은 status.Name에서 CalibrationEval을 요구한다. WorkflowMenuScenarios.cs:222는 HelpText로 기다리지만 :228에서 Name을 읽어 :233~234의 available/missing 판정을 한다. 기존 FullStatusText helper는 StatusBarText.HelpText를 읽는다.

RED: 기존 Mock fixture, 지정한 두 시나리오만 실행하여 Total2/Failed2/Exit1을 재현했다. W01은 경고 Name에 CalibrationEval이 없어 실패했고, W10도 같은 경고 Name에 available이 없어 실패했다. 원문은 red.txt다.
수정: W01의 읽기를 기존 FullStatusText(window)로, W10 최종 text의 읽기를 status.HelpText로 정정했다. CalibrationEval 및 파일시스템에 따른 available/missing 양방향 단언은 바꾸지 않았다. 제품 source·footer·fixture는 수정하지 않았다.
실행은 기존 Mock fixture의 UIA Expand/Invoke 패턴만 사용한다. 전역 키보드·마우스 입력, Native DLL/모델 실행, 새 보정 생성은 하지 않는다.

## 실행 근거

수신·착수 시계 관측값은 2026-11-04T08:57:18+09:00이다. 기존 env.cmd와 설치된 dotnet을 사용했으며, 다음 환경과 명령은 같은 프로세스 호출에 묶었다. 별도 data root는 자체 test bin 하위 절대경로다.

```text
XPE_E2E_BACKEND=Mock
DOTNET_CLI_UI_LANGUAGE=en
XPE_GUI_DATA_DIR=D:/workspace-github/xpe-gui/clients/ImageProcTest.E2ETests/bin/Debug/net8.0-windows/gui-c-258b-check
FILTER=FullyQualifiedName=ImageProcTest.E2ETests.Scenarios.Workflows.WorkflowScenarios.W01_RawImageLoaded_ViewportAndStatusReflectIt|FullyQualifiedName=ImageProcTest.E2ETests.Scenarios.Workflows.WorkflowMenuScenarios.W10_FixtureManager_ReportsWhetherTheFixturePackIsPresent
RED: dotnet test clients/ImageProcTest.E2ETests/ImageProcTest.E2ETests.csproj -c Debug --no-build --filter FILTER --logger console;verbosity=detailed
GREEN: dotnet test clients/ImageProcTest.E2ETests/ImageProcTest.E2ETests.csproj -c Debug --no-restore --filter FILTER --logger console;verbosity=detailed
```

실제 명령은 FILTER 전체 문자열을 하나의 인수로 전달했다. PowerShell에서 dotnet stdout/stderr를 합쳐 UTF-8 원문 파일로 저장했고 종료 코드를 따로 관측했다. 실행한 시험은 아래 두 개뿐이다.

| 기준·시나리오 | RED(red.txt) | GREEN(green.txt) |
|---|---|---|
| W01_RawImageLoaded_ViewportAndStatusReflectIt | CalibrationEval을 WARN 표시 Name에서 찾다 실패 | 기존 CalibrationEval 단언 통과 |
| W10_FixtureManager_ReportsWhetherTheFixturePackIsPresent | 실제 폴더가 있는데 경고 Name에서 available을 찾다 실패 | HelpText의 Fixture pack available과 실제 폴더 존재 대조·반대 missing 부재 단언 통과 |
| 분모·종료 | Total2/Failed2, Exit1 | Total2/Passed2/skip0, Exit0 |

GREEN 출력 발췌:
```text
W-10 status: Fixture pack available: D:\workspace-github\xpe-gui\gui\ImageProcTest\bin\Debug\net8.0-windows\fixtures\gui-s0
Test Run Successful.
Total tests: 2
     Passed: 2
```

GREEN은 변경한 E2E assembly를 빌드한 뒤 실행했다. CS1717/xUnit2013/xUnit2031 경고가 다른 시험 파일에서 출력됐으나 빌드 오류 없이 시험까지 완료했다. 해당 파일은 이번 카드에서 고치지 않았다.

## 정리·한계

- source diff는 지정 두 파일 각각 1줄이며 기존 단언·대기·fixture·제품 동작을 바꾸지 않았다. 두 파일의 git diff --check는 Exit0/출력 없음이었다.
- fixture 소유 RED PID27268/GREEN PID25856가 종료된 것을 확인했다. 자체 data root의 해석된 절대경로가 지정 test bin 하위임을 확인하고 그 폴더만 삭제했다. 출력: owned test processes=0 / own data root exists=False.
- W10은 이번 실행에서 fixture pack이 실제 존재한 경우를 검증했다. missing 상태를 새로 만들거나 asset을 삭제하지 않았다.
- 다른 시나리오/전체 suite/Native/최신 DLL/품질/CI는 실행·판정하지 않았다. 앞 NOTE-enhance-advanced-entry.md와 타인 변경은 이 카드 커밋에 포함하지 않는다.
- 별도 정상 git 커밋과 리더/Reviewer 회수 후에도 push는 하지 않는다. 훅이 차단하면 우회 없이 원문 오류를 보고한다.
- Reviewer는 지정 두 파일 각 1줄, 기존 oracle/fixture, RED2FAIL→GREEN2PASS/skip0 원문을 읽기 대조해 F283-1 해소·기술 수용·신규 차단 없음으로 회신했다. Reviewer는 시험을 재실행하지 않았다.
- staged source 공백 검사 대상 두 파일은 Exit0이었다. 원문까지 포함한 git diff --cached --check는 red.txt:19의 빈 줄 끝 공백 1건으로 Exit2다. 원문 출력의 공백을 바꾸거나 검사 설정을 끄지 않았으며 전체 공백 검사 통과를 주장하지 않는다.
