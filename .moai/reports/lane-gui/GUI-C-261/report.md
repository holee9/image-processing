[2026-11-13 14:20 KST] F261-1 반영 — 개정은 실패의 필수 조건이 아님, 추가 영향·전체 순서 효과는 미판정; 실패 4개는 기존 로컬 실패, 원인 미확정

리더 판정6(카드 시각 정정 14:17 KST): 도움말 개정 e61be663을 main에 병합하고 이후 main CI 전체 E2E로 검증한다는 결정이다. 병합·CI 실행·실패 시 되돌림은 리더 담당이며 이 레인은 카드 문서만 커밋하고 push하지 않는다. 이 문서의 A/B 결과는 전체 시험 성공을 뜻하지 않는다.

### 리더 판정5 A/B 최종

- Claim: 실패 4개(A06/EB02/IB01/IB02)를 같은 PC에서 부모 A와 현재 B로 A1→B1→A2→B2 각 2회 비교 완료. 네 회차 각각 4개 모두 FAIL, 총16개 관측(0P/16F/0SKIP). **부모에서도 재현되므로 개정이 실패의 필수 조건은 아님. 개정의 추가 영향·전체 순서 효과는 미판정.** 실패 4개는 기존 로컬 실패, 원인 미확정으로 남긴다. 이 관측만으로 개정의 모든 영향을 배제하지 않는다.
- Evidence: `ab-results.md`에 시험×트리×회차 16행, PASS/FAIL과 실패 메시지 첫 줄을 TRX에서 발췌했다. A06은 네 번 모두 `GUI E2E FAILED (exit -532462766)... WPF E2E failed.`; EB02/IB02는 각 트리 경로의 `No backend.json...`; IB01은 `An evidence folder is missing.`. 날짜·랜덤 run id 및 트리 경로는 다르지만 실패 유형과 문구는 같다. 각 회차 명령·시각·HEAD·exit는 `ab-A1/B1/A2/B2.result.json`, TRX·stdout/stderr도 같은 접두사로 보존했다. 회차는 14:07:37→14:09:15 / 14:09:15→14:10:52 / 14:10:52→14:12:28 / 14:12:28→14:14:03 KST. 최초 착수 줄은 14:08로 적었으나 실제 시작 시각은 이 원문 기록을 따른다.
- 동일 빌드: 두 트리 각각 `dotnet build <project> -c Debug --nologo`, project 순서 GUI / SelfCheck / 내부 E2E / clients E2ETests. 8개 모두 exit0, `ab-{A,B}-build-{0,1,2,3}.{stdout.txt,stderr.txt,result.json}`. 두 트리에 같은 명령을 사용하고 test에는 `--no-restore`와 정확히 네 FullyQualifiedName OR 필터만 추가했다. StopOnFail·보안 옵션 없음. `run-ab.ps1`에 재실행 없이 정해진 4회만 명시했다.
- Baseline-attribution: A `b9be6ae3b7f74413ccb6284d13a3220f2a8cde24`, B `e61be66303aa37ba482da5d85332eaf6148cced2`. `ab-preflight.json`에 동일 Native 환경·실행 정책, 각 result에 측정 HEAD. XPE_GUI_DATA_DIR·보정·raw·baseline도 두 트리에서 같은 절대경로 값을 사용했다. 작업 사본별 상대 경로에서 실행해 각각의 앱을 빌드·시험했다.
- evidence 사전 상태: A1/A2 모두 해당 `gui/.../evidence` 없음(0개). B1/B2 모두 1302개, 이름 정렬 첫 `20260930-120004-c851ea`, 끝 `snapshots`. `ab-*-before.json` 원문 유지. 이름 순서이며 생성시각 정렬을 주장하지 않는다. evidence 폴더를 삭제·초기화하지 않았다.
- 정리: A의 `git status --short` 출력 없음, HEAD 확인, A evidence 폴더 없음 및 절대 대상 `D:\workspace-github\xpe-gui-ab` 확인 후 **force 없이** `git worktree remove D:/workspace-github/xpe-gui-ab` 성공. 경로 없음 재확인. 결과 원문은 B 카드 폴더에 전부 남아 있다. B는 `dev/gui` / `e61be663...` 그대로, `git diff --name-only -- gui clients` 출력 없음. 제품·시험 소스 수정, commit/push 없음.
- Gaps / Residual-risk: 두 트리의 앱 폴더/evidence 과거 상태는 같지 않으며 이 조건도 위에 공개했다. Native 환경은 같지만 모든 PC 상태를 격리한 실험은 아니다. 실패의 근본 원인은 여전히 미확정이고 전체 E2E가 성공한 것으로 바꾸지 않는다. 추가 시험·수정 없이 리더/Reviewer 검토를 기다린다.

리더 판정5에 따라 이전 전체 결과는 수용됐으나 병합 근거는 부족하다. 부모는 `D:/workspace-github/xpe-gui-ab`, 현재 트리는 기존 `D:/workspace-github/xpe-gui`; dev/gui 상태 변경 없음. 이번에만 허용된 4개 시험 필터로 각 트리 2회 비교한다. evidence는 삭제하지 않고 매 회차 직전 상태만 기록한다. 제품·시험 소스 수정 없음. 원문은 같은 카드 폴더의 `ab-*` 고정 파일로 구분하며 종료 후 부모 worktree를 제거한다.

### 재개4 최종 결과

- Claim: 필터·StopOnFail 없이 E2E 프로젝트 전체 1회를 끝까지 실행했다. **371 = 342 PASS + 4 FAIL + 25 SKIP**, exit 1. 중단된 재개3은 이 분모에 포함하지 않는다. Integration은 앞서 수용된 재개2 **1026P/0F/3SKIP**를 인용하며 재측정하지 않았다.
- Evidence: `e2e.result.json` 시작 `2026-11-13T13:18:13.9721807+09:00`, 종료 `14:02:12.2014876+09:00`. 명령은 아래 재개3과 같은 전체 프로젝트 명령이며 StopOnFail/필터 없음. `e2e.stdout.txt` 끝 원문: `Total tests: 371 / Passed: 342 / Failed: 4 / Skipped: 25 / Total time: 43.9509 Minutes`. `e2e.trx` Counters `total=371 executed=346 passed=342 failed=4 aborted=0 timeout=0`; UnitTestResult의 NotExecuted 25개를 SKIP 사유와 대조했다. **4개 FAIL의 메시지·stack 및 25개 SKIP의 정확한 이름·사유는 `resume4-results.md`**에 TRX 원문 그대로 발췌했다. 독립 도우미 로그는 `independent-resume4.stdout.txt` / `.stderr.txt`, 시작 정보 `independent-resume4.json`이다.
- FAIL 1: `AutomationReportBackendTests.A06_GuiE2E_RunsFromTheApp_AndTheVerdictIsReported` 33.4188192 s, `AutomationReportBackendTests.cs:186` — `GUI E2E FAILED (exit -532462766): Unhandled exception. System.InvalidOperationException: WPF E2E failed.`. 내부 Program.cs:219 단언은 이전 12:33 회차 Event1026에서 확인한 것이며 이번 회차의 내부 이벤트를 새로 관측한 것으로 취급하지 않는다. 이번 앱 JSON `a06-automation-resume4.json`, SHA256 `9B86F44F2A1376D583E13E8AD0AD07D9C5E255671E2EAB4A91E80D6348384AC9`, 원본 Temp JSON과 복사 SHA 일치 확인.
- FAIL 2–4: `EvidenceBundleScenarios.EB02_TwoRunSets_KeepSeparateEvidence` — `No backend.json in D:\workspace-github\xpe-gui\gui\ImageProcTest\bin\Debug\net8.0-windows\evidence\20261113-133613-2b51c0.` / `IB01_InitializeBackend_StartsANewRunSet` — `An evidence folder is missing.` / `IB02_InitializeAfterTheDllsAppear_RecordsMockThenNative` — `No backend.json in ...\evidence\20261113-133606-cd8ca7.`. 실패 원문만 보존하며 helper·제품·시험 수정으로 해소하지 않았다. 이전 범위 밖 판정을 새 수정 승인으로 간주하지 않는다.
- D258 `MetricsFooterAndPipelineOrder_AreVisibleAndDistinct` PASS 1.1591935 s. stdout:570 꼬리말 `preprocess skipped · enhance_basic applied · gsvg off · ai_bone_suppress off · auto C 4021.869 / W 1395.963`, `enhance_basic 37.653 ms`. main CI 로그:2623–2643의 `Chain not run · window not measured` / `Not found: "C "` / `enhance_basic 77.662 ms` 실패 모양은 이번 실행에서 관측되지 않았다. W5 `WrongRawSize_WhatTheOperatorSees` PASS 24.1960598 s. 이번 PASS가 이전 실패 원인 해소를 입증하지는 않는다.
- Baseline-attribution: `D:\workspace-github\xpe-gui`, HEAD `e61be66303aa37ba482da5d85332eaf6148cced2` 종료 후 재확인. `git diff --name-only -- gui clients` 출력 없음. 환경·소스 해시는 `preflight-resume4.json`; Native 변수 3개는 이전과 같은 값, 전역 입력 금지 유지. main CI는 다른 HEAD(a617a8e5)이므로 동일 baseline의 환경 대조라고 주장하지 않는다.
- 정리: 종료 결과 `appAfter=[]`, 이후 조회에서도 독립 도우미37972와 dotnet32600/2904/512는 없었다. A05 PID512 임시 폴더는 실행 구간 생성·일반 파일17개·하위 폴더/링크 없음 확인 후 ZIP 항목별 SHA 일치 검증 → 파일별 삭제 → 빈 폴더 삭제했다. 증거 `temporary-selfcheck-A05-512.zip`, SHA256 `16AEAF902BB99415C9559ED21F70E93DC8884187A663940C5C6C6BF3BD85E79A`. `xpe-a12-calib-512`, `xpe-a21-native-512`는 이미 없음. Temp 전체 정리나 기존 데이터·초안 삭제는 하지 않았다.
- Gaps: A06 로컬 실패와 다른 HEAD CI 통과 차이의 원인은 미확정. 이번 A06 내부 예외 전체는 별도 이벤트로 재조회하지 않았다. SKIP25는 실행 검증 공백이다. 다른 Temp 전수 조사는 하지 않았다.
- Residual-risk / 다음 행동: 전체 suite는 실패 상태이므로 성공·병합 가능 판정을 내리지 않는다. 리더의 실패 분류·병합 판정 및 별도 Reviewer 검수를 기다린다. 제품·시험·대기·보안 설정 변경, commit/push 없음. 승인된 전체 1회 외의 재실행 없음.

### 리더 판정 4 / 재개4

리더는 서버 재시작 중단 회차를 시험 결과로 세지 않는다고 판정했다. 아래 83P/1F/2SKIP는 미완결 원문 보존용 집계이며 전체 시험 분모·판정에 합산하지 않는다. main CI는 a617a8e5, 도움말 변경 e61be663 없이 실행됐으므로 CI 통과를 로컬 환경 원인 확정 근거로 사용하지 않는다.

읽기 조사: `MainWindow.xaml:349–353` 메뉴는 `CanRunPreprocessing` 바인딩, `MainWindowViewModel.cs:835`는 `_backend.SupportsPreprocessing`; `RealXpeBackend.cs:325` true, `MockXpeBackend.cs:349` false. `XpeBackendFactory.cs:20–43`은 Native 탐색 경로·common/display DLL 존재·사용 가능 여부와 설정 `BackendMode == Native`에 따라 Real/Mock을 선택한다. `MainWindow.xaml.cs:140–178`은 실행 선택/automation 여부에 따라 설정을 읽고, 비automation이면 기존 설정을 로드한다(:157–161). 내부 `Program.cs:81`은 `new MainWindow()`로 생성하며 내부 runner에 자동으로 Mock을 강제하는 인수는 없다. `RunnerProcess.cs:52–64,70`의 ProcessStartInfo는 환경을 제거/대체하지 않아 Native 변수는 부모에서 상속된다. 즉 환경·설정이 메뉴 상태에 영향을 주는 코드 경로는 확인했지만 12:33 회차의 실제 내부 BackendMode/설정값을 관측한 것은 아니다.

`git diff origin/main...e61be663 --stat`: 22 files, 8697 insertions, 26 deletions. 제품 변경은 도움말 HTML 2개, 시험 변경은 `MenuBehaviorScenarios.cs:312–318`의 `XPE_E2E_HELP_CAPTURE` 조건부 캡처 7줄이며 나머지는 보고서·로그다. 메뉴 XAML/ViewModel/BackendFactory/RunnerProcess/내부 Program 경로 diff는 비어 있다. 도움말 변경이 메뉴 사용 가능 조건을 변경한 diff는 없다. 이 비교는 현재 로컬 origin/main 참조 기준이며 새 fetch는 하지 않았다.

중단 stdout/stderr·도우미·preflight-resume3은 `.attempt4` 복사 후 SHA 일치를 확인했다. 이전 TRX/result는 생성되지 않았다. 재개4는 Integration을 반복하지 않고 같은 전체 E2E 명령을 StopOnFail 없이 1회 실행하며, 도우미는 preflight 파일명만 `preflight-resume4.json`으로 구분한다. Windows Start-Process를 Hidden으로 사용해 독립 실행하고 시작 정보·stdout/stderr를 고정 파일에 받는다. 실행 정책 옵션·필터·제품/시험 수정 없음.

### 재개3 중단 결과

- Claim: A06 내부 실패 위치는 12:33 회차 Windows 기록으로 확인했다. 전체 E2E 완료 주장은 하지 않는다. 12:53:28.5276813 KST에 시작한 실행은 서버 재시작 전후 중간에 끊겼으며 재실행하지 않았다.
- Evidence: 실행 명령은 `dotnet test clients/ImageProcTest.E2ETests -c Debug --no-restore --logger "console;verbosity=detailed" --logger "trx;LogFileName=e2e.trx" --results-directory "D:\workspace-github\xpe-gui\.moai\reports\lane-gui\GUI-C-261"`. 필터·StopOnFail 없음. 복구 후 `write_stdin(91599)`는 `Unknown process id 91599`. `Get-CimInstance Win32_Process`의 이름·PID·부모PID·생성시각 조회에 기존 helper PID33984와 dotnet PID5588/6436/30616, ImageProcTest가 없었다. `e2e.trx`·`e2e.result.json`도 없다. stdout 126,976바이트(마지막 파일 시각 13:13:33.121 KST), stderr 0바이트를 그대로 보존한다. 종료코드는 관측하지 못했다. 서버 복구 시점에 사라진 사실만 확인했으며 OS 수준 종료 원인은 미확정이다.
- 콘솔에 완전한 결과 줄이 남은 부분은 **83 PASS / 1 FAIL / 2 SKIP (86개)**. 최종 시험 분모가 아니다. 유일한 기록된 FAIL은 `AutomationReportBackendTests.A06_GuiE2E_RunsFromTheApp_AndTheVerdictIsReported`(34 s), `AutomationReportBackendTests.cs:186`, `GUI E2E FAILED (exit -532462766): Unhandled exception. System.InvalidOperationException: WPF E2E failed.`. 이 회차 앱 JSON은 `a06-automation-resume3.json`에 원본 SHA 일치로 복사했다(SHA256 `FBED529B935557E3667E07E20B9132F94F6E478BD9BB10C8143F5B6AE484C478`). 원본 임시 JSON도 유지한다.
- D258 `MetricsFooterAndPipelineOrder_AreVisibleAndDistinct`는 이번 stdout:884에서 PASS(1 s). stdout:886 꼬리말은 `preprocess skipped · enhance_basic applied · gsvg off · ai_bone_suppress off · auto C 4021.869 / W 1395.963` / `enhance_basic 40.841 ms`. 기존 main CI 로그:2623–2643의 `Chain not run · window not measured`, `Not found: "C "`, `enhance_basic 77.662 ms`와 이번 관측 결과가 다르다. 원인 차이는 미확정이다. W5 결과 줄은 보존 stdout에서 확인되지 않았다.
- 기록된 SKIP 1: `WindowReacquireTests.ReadableWindow_IsKept_AndLeavesNoNote` — `Not measured: the real re-acquire fired on this launch — The launched window did not support AutomationId (framework=Win32); re-located it by process id 29692. hwnd launched=0x4D70C64 reacquired=0x4D70C64 topLevelWindowsForProcess=1 XPE-SKIP-ALLOWED:170`.
- 기록된 SKIP 2: `RealTestScenarios.R1_OneFrame_CorrectedWithASet_PicturesAndDataSaved` — `XPE_C231_NATIVE_DIR/XPE_C250_CALIB/XPE_C247_RAW/XPE_C250_OUT/XPE_C250_TAG not set - the real-frame set is not in the repository. XPE-SKIP-ALLOWED:273`.
- Baseline-attribution: `D:\workspace-github\xpe-gui`, HEAD `e61be66303aa37ba482da5d85332eaf6148cced2` 재확인, `git diff --name-only -- gui clients` 출력 없음. 환경 원문은 `preflight-resume3.json`. 수용된 Integration 1026P/0F/3SKIP는 재개2 결과이며 이번에 다시 실행하지 않았다.
- 보존·정리: 이전 실행 증거는 `.attempt3`, 현재 미완결 stdout/stderr·preflight·도우미도 유지한다. 시험 PID30616의 `xpe-selfcheck-A05-30616`(생성시각이 실행 구간 내임)은 17개 일반 파일·하위 폴더/링크 없음 확인 → `temporary-selfcheck-A05-30616.zip`의 17개 항목별 SHA 일치 확인 → 파일별 삭제 후 빈 폴더 삭제. ZIP SHA256 `621213BED99B0C96751CFFB9251391F965E25FAD30572441E86BE7575C8663A4`. 명시된 두 AI 임시 폴더는 조회 시 이미 없었다. 다른 Temp 전체를 검색·삭제하지 않았다.
- Gaps / Residual-risk: TRX·종료코드·전체 분모·미실행/미기록 시험 수는 미확인. A06의 CI/로컬 차이 원인 미확정. 전체 완료를 위해서는 리더의 중단 회차 판정과 후속 실행 지시가 필요하다. 제품·시험·대기 수정, 보안 옵션 변경, commit/push 없음.

재개3 조사: `RunGuiE2EAsync`는 `MainWindowViewModel.cs:2174–2175`에서 내부 실행기를 호출한다. `gui/ImageProcTest.E2E/Program.cs:58–70`은 내부 예외를 잡아 `WPF E2E failed.`로 감싼다. `RunnerProcess.cs:80–98`은 stdout/stderr를 메모리로 읽고 첫 stderr 줄만 반환하며, 이 경로에는 전체 출력을 파일에 쓰는 코드가 없다. 기존 Windows Application/.NET Runtime Event 1026, RecordId 40882, `2026-11-13T12:33:53.8793050+09:00`을 좁은 시간 조회로 확인하여 `a06-event1026.txt`에 보존했다. 내부 원문: `Preprocessing menu must be disabled until Phase 1a.` / `Program.cs:219`, Assert 함수 `:650`. 단언 실패 위치는 확인했으나 CI와 다른 원인은 미확정이다. 제품·시험은 수정하지 않는다.

이전 E2E 4개 원문·도우미·preflight는 `.attempt3` 복사 SHA 일치를 확인했다. 12:53 도우미는 단일 배열 항목이 풀려 `dotnet 2`를 호출하고 exit 1로 끝났다. 실제 시험 시작 전 도우미 오류이며 `e.stdout.txt`, `e.stderr.txt`, `e.result.json`, `preflight-resume.json`을 그대로 보존했다. 단일 항목 배열만 바로잡고 새 preflight 파일명을 `preflight-resume3.json`으로 구분한다. Integration은 수용된 1026P/0F/3SKIP를 유지하며 다시 돌리지 않는다.

리더는 아래 538P/1F/1SKIP 실행의 실패를 E2E 전용 Native 환경이 Integration에 전달된 조건 차이로 판정했다. 이를 제품·시험 결함으로 취급하지 않는다는 판정을 기록하며, 시험이 XPE_NATIVE_DIR를 자체 격리하지 않는 점은 별도 발견으로 남긴다. 제품·시험은 고치지 않는다. 현재 `integration.*`·도우미·`preflight-resume.json` 원문을 `.attempt2`로 같은 폴더에 보존한 뒤 `run-once.ps1`만 수정한다. Integration 자식에서 지정된 Native 변수 3개를 제거하고 E2E 자식에만 설정한다. 기존 성공 빌드는 다시 하지 않으며, 시험 기본 build는 유지한다. 지시 표기는 12:20 KST이나 최초 실제 시계는 `2026-11-13T12:15:14+09:00`였다.

### 재개2 최종 결과·확인 근거

- 2차 원문 6개를 `.attempt2`로 같은 폴더에 보존했고 이동/복사 전후 SHA256 일치를 확인했다. `.attempt1`·최초 `preflight.json`·성공한 빌드 원문도 그대로 남겼다. 수정한 실행 도우미의 구문 분석 오류는 0개였다.
- 도우미만 수정해 실제 ProcessStartInfo의 환경을 분리했다. Integration 자식에는 `XPE_NATIVE_DIR`·`XPE_NATIVE_DIR_EXCLUSIVE`·`XPE_C231_NATIVE_DIR`가 모두 없음을 시작 시 확인했고, 종료 result에도 세 값이 null로 기록됐다. E2E 자식에는 기존 Native 경로·exclusive=1·같은 C231 Native 경로를 설정했다. 전역 입력 허용은 0이다. 정책 Bypass나 제품/시험 변경은 없다.
- GUI-C-259의 `verification-commands.txt`는 Integration 명령에 Native 변수를 지정하지 않고, Native 변수는 E2E 환경 절에만 적었다. `preflight-sha256.txt`는 소스 SHA·앱 수 등을 기록했지만 당시 전체 환경값을 담지는 않았다. 이번 분리는 문서에 명시된 Integration/E2E 구분과 맞고, 과거 상속 환경까지 완전히 동일하다고 증명하는 것은 아니다. Native 경로·전역 입력 0·C231 자료·기존 `gui-c-259-check` 출력 사용은 259 문서와 대조했다. 재개의 전체 부모 환경은 `preflight-resume.json`, 실제 자식의 세 Native 변수는 각 result JSON으로 구분한다.
- **새 Integration 결과:** `2026-11-13T12:18:24.7893270+09:00`→`12:18:55.7319861+09:00`, 필터 없이 전체 선택·exit 0. 새 원문/TRX는 1026 PASS / 0 FAIL / 3 SKIP / 총 1029다. 이전 검색 정책 `ByDefault_NoBuildDirectoryOrSiblingCheckoutIsOffered`의 네 locator 항목은 모두 PASS였다. 결과는 이번 실행에서 읽었으며 과거 수치를 재사용한 것이 아니다.
- **E2E 실행:** `2026-11-13T12:18:55.7827114+09:00`→`12:34:01.5778013+09:00`, 필터 없이 전체 프로젝트를 선택해 1회 실행했다. 첫 실패 뒤 StopOnFail로 중단됐으며 exit 1이다. 결과는 19 PASS / 1 FAIL / 0 SKIP / 총 20, 전체 소요 원문은 15.0361 Minutes다. 실제 전체 프로젝트 완료는 아니고, 중단 뒤 미실행 시험 개수는 측정하지 않았다. 추가 실행·재시도는 하지 않았다.
- 진행 관찰 12:30 KST: 동일 실행의 종료 출력과 E2E result JSON은 아직 없다. 제어 세션은 56261이며 재기동하지 않는다. 작성 중인 원문 파일의 읽기는 EBUSY로 거절됐고, 잠금을 우회하지 않았다. 현재 시험 이름·중간 PASS/FAIL 분모는 관측하지 못했으므로 진행 중 메타데이터만으로 통과·실패·정체를 판정하지 않는다.

| 프로젝트 | 이번 실제 결과 | 원문·환경·분모 근거 |
|---|---|---|
| Integration | 1026 PASS / 0 FAIL / 3 SKIP / 총 1029, exit 0 | `integration.stdout.txt`, `integration.stderr.txt`, `integration.trx`, `integration.result.json` |
| E2E 전체 선택·첫 실패 중단 | 19 PASS / 1 FAIL / 0 SKIP / 총 20, exit 1 | `e2e.stdout.txt`, `e2e.stderr.txt`, `e2e.trx`, `e2e.result.json` |

### 최초 E2E 실패 보존과 정지점

실패 이름은 `ImageProcTest.E2ETests.Scenarios.Smoke.AutomationReportBackendTests.A06_GuiE2E_RunsFromTheApp_AndTheVerdictIsReported`다. TRX의 회차 시각은 `2026-11-13T12:33:25.0829681+09:00`→`12:34:01.4614332+09:00`, duration은 `00:00:36.3725959`다.

```text
The GUI E2E runner RAN and FAILED when launched from the app:
'GUI E2E FAILED (exit -532462766): Unhandled exception.
System.InvalidOperationException: WPF E2E failed.'.
at ImageProcTest.E2ETests.Scenarios.Smoke.AutomationReportBackendTests
  .A06_GuiE2E_RunsFromTheApp_AndTheVerdictIsReported()
  .../Scenarios/Smoke/AutomationReportBackendTests.cs:line 186
```

실제 단언은 위 파일 `:180–187`에서 앱이 보고한 `GuiE2EPassed`를 읽고 L186 `Assert.True`로 확인하는 부분이다. stdout L130에는 `[xUnit.net 00:15:01.02] Canceling due to test failure...`, L131–147에는 실패·stack·20건 분모·TRX 경로가 있다. 도우미는 `12:34:01.6060660 KST`에 E2E exit 1을 출력하고 `STOP: failure preserved; no subsequent project or retry.`로 자연 종료했다. 보고된 E2E SKIP은 0개이며, 이후 미실행 시험을 환경 SKIP으로 세지 않는다.

E2E 자식의 Native 변수 3개는 `e2e.result.json`에 기존 Native 경로·exclusive=1로 기록됐다. 다만 **실패한 A06 자체는 원형 코드 L182에서 Mock을 명시적으로 요청하는 시나리오**다. 원문 출력은 `A06 exit=0 BackendMode=Mock ActualBackendMode=Mock BackendMatchesRequest=True Passed=True`이고, 추가 automation JSON은 `GuiE2EPassed=false`와 위 실패 상태를 담는다. 앱 자동화의 exit/일반 Passed와 내부 GUI E2E verdict를 혼동하지 않는다. 이 보고서·TRX의 wrapper 메시지만으로 내부 WPF 단언 원인은 확정하지 않는다.

실패 회차의 기존 파일 `C:/Users/drake/AppData/Local/Temp/xpe-automation-A06-38744.json`을 같은 카드 폴더의 `a06-automation.json`으로 복사했다. 원본/복사본 SHA256은 `1A51DA097D123EB003D1D11DC2B81F93671E9CD3B04DA5908D9262FD6D06D77B`로 일치했다. 원본도 삭제하지 않았다.

**D258·W5와 CI 비교 범위:** 이번 TRX에 기록된 20개는 AutomationReportBackendTests 시나리오이며 D258·W5 결과는 없다. GUI-C-260의 각 20회 단독 통과를 이번 전체 순서의 결과로 대체하지 않는다. 카드가 지정한 `D:/workspace-github/image-processing/.moai/reports/lead/COV-20261112/job-131200334499.log:2623–2650`도 읽었다. CI run 43216007023의 D258은 L56에서 `Chain not run · window not measured`에 `C `가 없다는 실패였고, 같은 footer의 enhance_basic 시간은 77.662 ms였다. 이번 정지점은 A06 L186이므로 그 D258 표시 모양을 재현·비교한 실행이 아니다. D258/W5 가설과 A06을 공통 원인으로 연결하지 않는다.

### 정리·남긴 증거·종료 검토

- **지운 임시 폴더 1개:** `C:/Users/drake/AppData/Local/Temp/xpe-selfcheck-A05-38744`. 원형 A05가 복사본을 만드는 소스 `AutomationReportBackendTests.cs:120`, `:266–274`, 해당 회차 시각과 생성 시각 `12:26:16.4415434 KST`를 대조했다. 절대 경로가 명시한 Temp 하위 대상과 같고 reparse point·하위 디렉터리·링크가 없음을 확인했다.
- 현재 빌드 원본과의 바이트 비교에서 차이가 있어 최초 삭제 사전 검사는 중단했고 그 단계에서는 지우지 않았다. 이후 원형 복사본 17파일을 `temporary-selfcheck-A05-38744.zip`에 보존하고, ZIP의 각 항목 SHA256과 당시 임시 파일의 SHA256을 모두 대조한 뒤 평면 파일과 빈 폴더만 삭제했다. ZIP SHA256은 `C3FADAC61D6ADEA9A119035C1ED39B40A866B1E49B69A36DABAC223439306F20`이다. 삭제 후 해당 폴더 부재를 확인했다. 복사본과 현재 빌드의 바이트 차이 원인은 추가 조사하지 않았다.
- 확인한 `xpe-a12-calib-38744`, `xpe-a21-native-38744`와 Integration의 `xpe_c225b_symlink_dee7456547444f5f88395c92991fc5d0` 폴더는 이미 없었다. 다른 OS 임시 폴더 전체를 조사·삭제한 것은 아니다. 기존 `gui-c-259-check`, 과거 259/260/INV 증거와 외부 원본은 유지했다.
- **남긴 증거:** 같은 카드 폴더의 두 프로젝트 stdout/stderr·TRX·result, `preflight-resume.json`, `.attempt1` 4개와 `.attempt2` 6개, 기존 빌드 원문, 실패 A06 automation JSON, 임시 SelfCheck 보존 ZIP이다. 새 회차 하위 폴더는 만들지 않았다.
- 종료 후 읽기 관찰에서 ImageProcTest 0개, testhost 0개, A06 report를 작성한 E2E host PID 38744는 없었다. HEAD는 `e61be66303aa37ba482da5d85332eaf6148cced2`/dev/gui이며 `gui`·`clients` 미커밋 diff는 없었다. 제품·시험·단언·대기·재시도는 고치지 않았고, 실제 E2E 실패 뒤 재실행·커밋·푸시는 하지 않았다. 전체 E2E 완료나 병합 수용은 판정하지 않으며 리더/Reviewer 검수로 인계한다.

새 Integration의 SKIP 원문(TRX의 NotExecuted 3개, console Skipped=3):

| 이름 | 사유 원문 |
|---|---|
| `ImageProcTest.IntegrationTests.Functional.BaselineFingerprintProbe.PrintTheStageHashesOfTheBaselineChainOnRealData` | `XPE_NATIVE_DIR, XPE_C231_CALIB_DIR and XPE_C231_RAW are needed.` |
| `ImageProcTest.IntegrationTests.Functional.OffsetQualityTests.TheRealModule_OnDark16AndTheCleanSetMap_MeasuresTheSignedBiasThePaperReports` | `XPE_C253_NATIVE_DIR/XPE_C253_DARK/XPE_C253_OFFSET not set - the dark frame and the map are not in the repository. XPE-SKIP-ALLOWED:273` |
| `ImageProcTest.IntegrationTests.Safety.DllSearchPathSafetyTests.AFileLinkInsideAnApprovedFolder_ThatPointsOutside_IsRejected` | `a file symbolic link could not be created here (needs developer mode or elevation): 클라이언트가 필요한 권한을 가지고 있지 않습니다. : 'C:\Users\drake\AppData\Local\Temp\xpe_c225b_symlink_dee7456547444f5f88395c92991fc5d0\approved\xpe_common.dll'` |

아래 21:24 실행 기록의 원문 파일명은 보존 후 각각 `.attempt2` 접미사 파일을 가리킨다. 접미사 없는 새 결과와 구분한다.

## 2차 시도 기록 (보존)

[2026-11-12 21:24 KST] 재개 결과 — Integration 538P/1F/1SKIP·총540에서 실패 중단, E2E 미실행 (재개 21:17 KST)

리더의 21:15 판정은 1차 중단을 실행 도우미 오류로 수용했고 시험 실패로 분류하지 않았다. 아래 19:43 기록과 1차 원문을 보존한다. `integration.*`·`run-once.ps1` 원본은 같은 폴더의 `.attempt1` 접미사 파일로 보존한 뒤, 경로 결합만 고치고 이미 성공한 빌드는 생략한다. 원래 preflight도 덮지 않고 재개 preflight를 같은 카드 폴더의 고정 이름으로 남긴다. 시험·제품·단언·대기·재시도 변경과 보안 옵션·push 금지는 유지한다.

### 재개 실행 결과 — 최초 시험 실패 보존·정지

- `integration.stdout.txt.attempt1`·`integration.stderr.txt.attempt1`·`integration.result.json.attempt1`·`run-once.ps1.attempt1` 보존 전후 SHA256 일치를 확인했다. 최초 `preflight.json`과 managed 빌드 4개의 원문/result도 그대로 남겼다.
- 도우미의 두 경로 결합식을 괄호로 감쌌고, 이미 성공한 빌드 4단계는 생략했다. 원래 preflight를 덮지 않도록 이번 snapshot은 `preflight-resume.json`이라는 고정 이름을 사용한다. 새 회차 폴더는 만들지 않는다.
- PowerShell 구문 분석 결과 오류 0개, Integration/E2E 배열 항목은 각각 정확히 2개이며 인수 결합은 ParenExpressionAst였다. 이 분석은 시험 실행이 아니다.
- 정상 기동 명령: `GUI-C-254c/env.cmd powershell.exe -NoProfile -File .moai/reports/lane-gui/GUI-C-261/run-once.ps1`. 실행 정책 옵션은 없다.
- 실제 시작: `2026-11-12T21:19:54.6350007+09:00` Integration 전체. 원문 stdout/stderr는 고정 파일 스트림에 실시간 보존한다. `--filter`는 없고, 마지막 인수는 `-- xUnit.StopOnFail=true`다. Integration이 성공한 경우에만 E2E 전체를 한 번 시작한다. 실패 시 원문·TRX를 보존하고 멈추며 다시 실행하지 않는다.

| 프로젝트 | 이번 결과와 분모 | 원본 |
|---|---|---|
| Integration 전체 선택·첫 실패 후 중단 | 538 PASS / 1 FAIL / 1 SKIP / 총 540, exit 1. TRX counters의 total=540, executed=539다. 프로젝트 전체 완료는 아니며 나머지 미실행 개수는 측정하지 않았다. | `integration.stdout.txt`, `integration.stderr.txt`, `integration.trx`, `integration.result.json` |
| E2E | Integration 실패로 미실행. PASS/FAIL/SKIP 분모 없음. | 새 E2E 원문·TRX는 생성되지 않음 |

실패 이름: `ImageProcTest.IntegrationTests.Functional.NativeSearchPolicyTests.ByDefault_NoBuildDirectoryOrSiblingCheckoutIsOffered(locator: "NativeModuleLibraryLocator")`.

```text
Assert.All() Failure: 1 out of 2 items in the collection did not pass.
NativeModuleLibraryLocator: default search offered a build/sibling candidate:
D:\workspace-github\image-processing\build\e2e-native-dlls\gsvg.dll
NativeSearchPolicyTests.cs:line 49
```

실제 stack의 첫 시험 프레임은 `D:/workspace-github/xpe-gui/clients/ImageProcTest.IntegrationTests/Functional/NativeSearchPolicyTests.cs:49`다. TRX의 실패 회차 시각은 `2026-11-12T21:20:20.8311313+09:00`→`21:20:20.8394059+09:00`이며 duration 원문은 `00:00:00.0020977`다. stdout에 `[xUnit.net 00:00:21.47] Canceling due to test failure...`가 기록됐다. 도우미는 `21:20:20.9988256+09:00`에 Integration exit 1을 기록하고 `STOP: failure preserved; no subsequent project or retry.`로 자연 종료했다. 이번 실패는 1차의 경로 인수 오류와 달리 실제 시험 단언 실패다.

이번에 실제로 보고된 SKIP은 1개다. 이름은 `ImageProcTest.IntegrationTests.Diagnostics.PreprocessOracleVerdictsTests.TheImportTable_NamesTheModulesDependencies_AndTheSnapshotFollowsThem`, 사유 원문은 `xpe_preprocess.dll and xpe_common.dll are not staged in the test output.`이다. TRX의 해당 UnitTestResult는 NotExecuted이며 console 요약은 Skipped=1이다. 첫 실패 중단 뒤 미실행 시험을 이 환경 SKIP으로 환산하지 않는다.

확인 사실: `preflight-resume.json`에는 이 Integration 자식 프로세스에 전달한 `XPE_NATIVE_DIR=D:/workspace-github/image-processing/build/e2e-native-dlls`, exclusive=1, global input=0이 있다. 실패 후보의 부모 경로와 설정한 Native 경로가 일치한다. 해당 시험 `:43–52`는 후보 목록을 단언하며, PolicyScope `:83–95`는 dev-search와 exclusive를 일시 변경·복원하지만 XPE_NATIVE_DIR를 변경하지 않는다. 도우미는 E2E용 Native 환경을 Integration에도 공통 적용했다. 이 환경과 시험의 기본 검색 전제가 관련됐을 가능성이 있으나, 환경을 바꾸거나 재실행해 인과를 검증하지 않았다. 제품 DLL을 실제로 어디서 로드했는지나 제품 결함까지 확정하지 않는다. 제품·시험·환경 설정을 추가 수정하지 않고 멈춘다.

D258·W5는 E2E가 시작되지 않아 이번 재개에서 관측하지 못했다. GUI-C-260의 미재현 결론을 이번 전체 E2E 결과로 대체하지 않는다.

정리·보존: 카드 폴더 안 새 회차 하위 폴더는 0개다. 수동 삭제한 임시 폴더도 0개이며, 원형 시험의 자체 정리 동작은 변경하지 않았다. 개별 OS 임시 폴더 생성·삭제를 전수 관측한 것은 아니다. 원문·TRX·`.attempt1` 파일, 최초/재개 preflight와 기존 빌드 4개의 근거를 같은 폴더에 남겼다. 실행 종료 result의 appAfter는 빈 배열이었다. 부모가 stdout 요약·stderr 실패 이름·TRX의 실패/SKIP 이름과 사유·실제 명령/exit를 읽기로 대조했다. `gui`·`clients` 미커밋 diff는 없었고 HEAD는 e61be663 그대로였다. 커밋·푸시·시험 재실행은 하지 않았다. 전체 시험 완료·병합 수용 판정은 아직 할 수 없으며 리더/Reviewer 판단으로 인계한다.

## 1차 중단 기록 (보존)

아래 1차 기록에 적힌 `integration.*` 원본은 현재 각각 `integration.*.attempt1`로 보존돼 있다. 접미사가 없는 파일은 위 재개 실행 결과다. 1차의 `preflight.json`도 그대로 남겼으며 재개 환경은 별도 `preflight-resume.json`에 있다.

[2026-11-12 19:43 KST] 중지·검토 인계 — managed 빌드 4개 성공, Integration 기동 도우미 오류(exit 1)·시험 분모 미관측, E2E 미실행

O: 도움말 개정 e61be663의 병합 판단에 필요한 전체 시험 결과를 회수한다. KR: 빌드 후 필터 없는 Integration 전체 1회 → E2E 전체 1회, 원문·TRX·분모·SKIP 사유와 실패 정지점 보존.

카드: `D:/workspace-github/image-processing/.moai/lanes/gui/inbox/GUI-C-261.md`.
기준: `D:/workspace-github/xpe-gui`, `dev/gui`, HEAD `e61be66303aa37ba482da5d85332eaf6148cced2`.
수신 지시의 표기는 19:45 KST이나 최초 실제 시계는 `2026-11-12T19:32:19+09:00`였다. 관찰한 시스템 시각을 기록한다.

GUI-C-260은 리더가 40회 미재현·원인 미확정으로 수용했으며 Reviewer 검수는 별도다. 이번 카드에서는 제품·시험·단언·대기·재시도를 바꾸지 않고, 승인 밖 보안 옵션과 push를 사용하지 않는다. 기존 출력 폴더와 고정된 증거 경로를 사용하며 실패를 재실행으로 덮지 않는다.

현재 상태: 실행 도우미의 인수 조합 오류로 중지했다. 최초 ACK 착수 시각은 2026-11-12 19:32 KST다. 전체 시험 통과·완료를 주장하지 않는다.

## 관측 결과

정상 정책의 `powershell.exe -NoProfile -File .moai/reports/lane-gui/GUI-C-261/run-once.ps1`로 19:39:57에 시작했다. operator(19:40:01), legacy(19:40:03), SelfCheck(19:40:05), E2E runner(19:40:06) managed build는 각각 exit 0이다. 각 `build-*.stdout.txt`·`.stderr.txt`·`.result.json`에 원문과 실행 시각을 보존했다.

19:40:06 Integration CLI는 실제 다음처럼 잘렸다. PowerShell 배열 요소 안에서 경로 문자열을 결합한 실행 도우미 오류이며 제품·시험 실패로 분류하지 않는다.

```text
dotnet test clients/ImageProcTest.IntegrationTests -c Debug --no-restore --logger "console;verbosity=detailed" --logger "trx;LogFileName=integration.trx" --results-directory "
Unhandled exception: The path is empty. (Parameter 'path')
```

`integration.stdout.txt`는 비어 있고 stderr에 위 원문이 있으며 `integration.result.json`은 exit 1과 빈 appAfter를 기록했다. 시험 결과/TRX가 없으므로 PASS/FAIL/SKIP 시험 분모는 미관측이다. E2E는 시작하지 않았다. 도우미는 자연 종료했고 추가 실행·재시도·강제 종료·파일 덮어쓰기는 하지 않았다. 부모에게 실행 도우미 오류와 후속 중지를 즉시 보고했다. 원문 오류를 고치거나 재실행하지 않고 판단을 기다린다.

기준 HEAD는 시작 및 각 실행 종료 때 `e61be66303aa37ba482da5d85332eaf6148cced2`였다. 시작 전 app/testhost는 없었다. `preflight.json`에 소스 해시·정책 조회·환경·명령 배열을 보존했다. 수동 삭제한 임시 폴더는 0개다. 기존 GUI-C-259/260 자료와 원문을 보존했다. 현재 카드의 원문·driver·manifest만 작성했으며 제품·시험 소스 및 설정은 수정하지 않았다.

당초 계획(아래 올바른 형태의 두 시험 명령은 실제로 실행되지 않음): `run-once.ps1`에 명령을 고정하려 했다. operator, legacy, SelfCheck, E2E runner를 각각 `dotnet build <csproj> -c Debug --no-restore`로 빌드하고, 이어 아래 명령을 한 번씩 실행하며 Integration 실패 시 E2E를 시작하지 않는 계획이었다. 각 시험의 기본 build를 유지하여 현재 소스로 test DLL도 빌드할 계획이었으나 시험 단계에 도달한 결과는 없다.

```text
dotnet test clients/ImageProcTest.IntegrationTests -c Debug --no-restore --logger "console;verbosity=detailed" --logger "trx;LogFileName=integration.trx" --results-directory "D:\workspace-github\xpe-gui\.moai\reports\lane-gui\GUI-C-261" -- xUnit.StopOnFail=true
dotnet test clients/ImageProcTest.E2ETests -c Debug --no-restore --logger "console;verbosity=detailed" --logger "trx;LogFileName=e2e.trx" --results-directory "D:\workspace-github\xpe-gui\.moai\reports\lane-gui\GUI-C-261" -- xUnit.StopOnFail=true
```

필터는 없으며 공식 StopOnFail의 비동기 특성상 이미 실행 중인 시험은 끝날 수 있다. 중단 미실행과 환경 SKIP을 구분한다. 파일·단언·대기·retry 설정은 바꾸지 않는다. stdout/stderr는 실행 중 파일 스트림으로 보존하고 고정 증거 파일이 있으면 시작하지 않는다. 실행 정책 옵션을 지정하지 않는다. GUI-C-259의 Native/exclusive1/global0/C231 입력과 기존 `gui-c-259-check`를 사용하며 GUI-C-260의 ExistingCalib opt-in은 사용하지 않는다.

## 최종 대조·미완료 범위

- 부모가 4개 build stdout의 `Build succeeded`·오류 0개와 result JSON의 각 exit 0을 읽기로 확인했다. 경고는 operator 11개, legacy 0개, SelfCheck 1개, E2E runner 0개였다.
- 부모가 `integration.result.json`의 실제 잘린 명령·19:40:06.632→19:40:06.785 KST·exit 1, 비어 있는 stdout과 위 stderr 원문을 대조했다. 카드 증거 폴더의 TRX는 0개다. 도우미 `run-once.ps1:15`의 배열 내부 경로 결합이 괄호 없이 작성되어 명령 인수 요소가 잘렸다. 원본 도우미는 수정하지 않고 보존한다.
- Integration PASS/FAIL/SKIP은 **분모 미관측**이며 SKIP 이름·사유 목록도 생성되지 않았다. 이를 0건 통과나 제품 시험 1건 실패로 환산하지 않는다. E2E는 **미실행**이다. D258·W5도 이번 카드에서 관측하지 못했으므로 GUI-C-260의 단독 통과로 전체 실행을 대체하지 않는다.
- 제품/시험 경로 `gui`, `clients`의 미커밋 diff는 없었다. HEAD는 e61be663 그대로다. 새 회차 폴더를 만들지 않았고 수동 삭제한 임시 폴더는 0개다. 남긴 근거는 이 카드 폴더의 `build-*` 원문/result, Integration 원문/result, preflight와 실행 당시 도우미다. 기존 자료와 원문은 삭제하지 않았다.
- 전체 시험 완료나 병합 가능을 판정할 근거는 확보되지 않았다. 카드의 실패 보존·중단 지시에 따라 추가 기동·재실행·도우미 수정·커밋·푸시 없이 멈춘다. 리더의 판정과 Reviewer 독립 검수는 아직 받지 않았다.
[2026-11-13 12:52 KST] 재개3 12:52 KST — 내부 GUI E2E 실패·로그 위치를 읽기 조사한 뒤, StopOnFail 없이 E2E 전체 1회 실행. 이전 실패 증거는 보존한다.
