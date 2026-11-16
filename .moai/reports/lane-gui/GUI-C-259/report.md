[2026-11-04 19:06:05 KST] GUI-C-259 · Refs #280 #281 #282 · 커밋 전 검토: Integration 1026P/0F/3SKIP, 두 E2E 클래스 전체 25P/1F/2SKIP, W5 단독 1P·최초 실패 원인 미확정, 문서·Help 캡처 Reviewer 기술 수용

## 수신·착수

- O: 사용자가 도움말만 보고 현재 운영자 앱을 사용할 수 있게 한다.
- KR: 한영 8요구 반영·지정 시험 범위 전체 실행 원문·실제 Help 개정본 캡처 1장·부모의 정상 커밋과 미푸시 상태를 근거로 확인한다.
- 현재 상태: 최종 문서·Integration 전체·지정 두 E2E 클래스 전체 실행과 실제 Help 캡처를 마쳤다. W5의 최초 UIA 조회 실패는 보존했고 단독 1회에서는 재현되지 않았다. 지정 E2E 전체 통과를 주장하지 않는다.
- 다음 행동: 부모의 최종 diff·staged 검사·정상 훅·커밋 검토. 과거 공통 규칙 파일을 복구하거나 없는 규칙을 재구성하지 않는다.

- 카드: `D:/workspace-github/image-processing/.moai/lanes/gui/inbox/GUI-C-259.md` (리더 표기 18:32 KST).
- 기준 HEAD: `b9be6ae3b7f74413ccb6284d13a3220f2a8cde24`, `D:/workspace-github/xpe-gui`, `dev/gui`.
- 문서 담당은 두 quick-start HTML, 시험 담당은 영향 시험 3파일과 이 보고서·원문 로그를 소유한다. 다른 세션의 설정 및 GUI-C-258e 보고서/BLOCKED 변경을 보존한다.
- 기존 인용·제목·단계 파서 계약을 읽어 문서 담당과 공유했다. 실제 앱에 맞는 설명을 우선하고 기존 독립 처리·저장 SHA oracle을 유지한다.
- 부모 승인으로 MenuBehavior H01에 선택적 `XPE_E2E_HELP_CAPTURE` 진단 7줄만 추가했다. Quick Start 메뉴 case가 기존 단언을 통과한 뒤 UIA help 영역을 FlaUI Capture.Element로 저장한다. 기존 메뉴 Invoke/Window Close·단언은 바꾸지 않았으며 키·마우스를 주입하지 않는다.

## 현재 관찰

| 측정 상태 | 명령 | 결과·종료 코드 | 원문 |
|---|---|---|---|
| 기존 문서·시험 수정 전 | `dotnet test clients/ImageProcTest.IntegrationTests -c Debug` (필터 없음) | 1026 PASS / 0 FAIL / 3 SKIP / 총 1029, exit 0 | [baseline-integration.txt](baseline-integration.txt) |
| 문서 초안 패키징 | `dotnet build gui/ImageProcTest/ImageProcTest.csproj -c Debug --no-restore --nologo` | 오류 0 / 기존 경고 11, exit 0 | [build-gui.txt](build-gui.txt) |
| 첫 HTML 초안+선택적 capture 변경 | `dotnet test clients/ImageProcTest.IntegrationTests -c Debug` (필터 없음) | 1026 PASS / 0 FAIL / 3 SKIP / 총 1029, exit 0 | [draft-integration.txt](draft-integration.txt) |
| 정정 후 최종 HTML 패키징 | `dotnet build gui/ImageProcTest/ImageProcTest.csproj -c Debug --no-restore --nologo` | 오류 0 / 증분 빌드 경고 0, exit 0 | [build-gui-final.txt](build-gui-final.txt) |
| 최종 HTML 고정 후 | `dotnet test clients/ImageProcTest.IntegrationTests -c Debug` (필터 없음) | 1026 PASS / 0 FAIL / 3 SKIP / 총 1029, exit 0 | [green-integration.txt](green-integration.txt) |
| 지정 두 E2E 클래스 전체 | `dotnet test ... --filter <두 클래스 전체>` | 25 PASS / 1 FAIL / 2 SKIP / 총 28, exit 1 | [e2e-selected.txt](e2e-selected.txt), `e2e-results/gui-c-259-e2e.trx` |
| 부모 승인 후 W5 단독 1회 | `dotnet test ... --filter <W5 전체 이름>` | 1 PASS / 총 1, exit 0 | [e2e-w5-recheck.txt](e2e-w5-recheck.txt), `e2e-w5-results/gui-c-259-w5.trx` |

각 명령은 기존 `.moai/reports/lane-gui/GUI-C-254c/env.cmd` 환경 helper를 통해 해당 트리에서 순차 실행했다. 첫 초안 시험이 끝난 뒤 부모/문서 담당이 실제 앱 설명(Save & Next 선택 동작, Notes 위치·tooltip, HUD 위치, EI 표기)의 추가 정정을 확인했다. 문서 담당에게 시험 종료와 쓰기 해제를 알렸고 최종 고정 통보 뒤에만 최종 build와 Integration을 다시 실행했다. 초안 통과를 최종 결과로 세지 않는다. 최종 증분 빌드의 경고 0은 앞선 기존 경고 11의 해소를 의미하지 않는다.

최종 source HTML과 packaged HTML의 SHA256은 각각 동일했다([packaged-help-sha256.txt](packaged-help-sha256.txt)). E2E 직전 대상 HTML 2개와 시험 3파일의 SHA256도 [preflight-sha256.txt](preflight-sha256.txt)에 기록했다. 그때 ImageProcTest 프로세스는 0개였고 전용 data root는 없었다.

## 문서 요구·소스 대조와 검토

다음 행 매핑은 문서 담당이 최종 HTML과 실제 소스를 읽어 전달한 근거이며 부모가 최종 HTML을 검토했다. Integration 인용 시험은 최종 상태에서 통과했지만, 아래 모든 운영 설명을 자동으로 증명하는 시험이라고 확대하지 않는다.

소스 기준 루트는 **`D:/workspace-github/xpe-gui/gui/ImageProcTest/` 운영자 앱**이다. 아래 약칭의 실제 파일 존재를 확인했다. `MainWindowViewModel.cs`는 `ViewModels/MainWindowViewModel.cs`, `AlgorithmPreset.cs`는 `Models/AlgorithmPreset.cs`, `ProcessingChainPlan.cs`는 `Services/ProcessingChainPlan.cs`다. `TopBar.xaml`, `VerdictBar.xaml`, `AnalysisPanel.xaml`, `ViewportShell.xaml`은 모두 이 루트의 `Views/` 아래에 있고 `MainWindow.xaml`은 루트 바로 아래다. `xpe-pre/gui/ImageProcTest`나 `xpe-gui/clients/ImageProcTest` 진단 앱과 합쳐 설명하지 않는다. 문서 KO/EN은 같은 운영자 앱 루트의 `help/quick-start.ko.html` / `help/quick-start.html`이다.

| 카드 요구 | 최종 KO / EN 행 | 실제 앱 소스 근거 |
|---|---|---|
| 1. 실행 파일·DLL·보정 폴더와 Native 확인 | 18–24·37–39 / 19–25·38–40 | 기존 준비 절차를 유지하고 `TopBar.xaml:38`의 runtime provenance 위치에 맞춤. |
| 2. 영상 열기와 manual 스터디 큐 | 72 / 73 | `MainWindowViewModel.cs:2900-2916`: backend load 성공 후 경로 대조 및 Source=manual 추가. |
| 3. 자동 PRE→기본 강조→표시, footer/HUD/미보정 배너 | 49·74 / 50·75 | VM `:2942`, `:3007`; `ProcessingChainPlan.cs:22-25`; `MainWindow.xaml:513-530`; `ViewportShell.xaml:283-334`; VM `:1071-1075`. |
| 4. A/B 네 preset·B radio·stale 재적용 | 76–86 / 77–87 | `AlgorithmPreset.cs:33-36`, `:89-97`; VM `:1675-1715`; `AnalysisPanel.xaml:266-275`; `ViewportShell.xaml:111`. |
| 5. Metrics 아홉 행·측정값과 합격 기준 구분 | 54·88 / 55·89 | VM `:1129-1160`, `:1183-1201`, `:816`의 영어 표시 변환. |
| 6. 메모·판정·집계·Save & Next·bundle | 90–91 / 91–92 | `VerdictBar.xaml:27-35`, `:41/62/83/105`; VM `:3801-3868`, `:3887-3909`, `:1777-1780`, `:3917-3922`, `:4171-4200`; `TopBar.xaml:49-66`, `AnalysisPanel.xaml:432-469`. |
| 7. 현재 Pipeline 메뉴 순서 | 93 / 94 | `MainWindow.xaml:349-381`. |
| 8. AI 모델 필요·고급 강조 경로 부재·미보정 정책 미정 | 94 / 95 | `AnalysisPanel.xaml:240-253`, AI availability 평가, `MainWindow.xaml:361-363`, `ProcessingChainPlan.cs:22-25`. #283/#274 상태는 카드 요구에 근거하며 별도 정책 승인을 추정하지 않음. |

부모가 받은 Reviewer 재검토 응답은 **문서 부분 수용**이었다. F259-1은 Save & Next가 다음 선택만 바꾸고 자동 load하지 않음을 명시해 해소했다(KO91/EN92). F259-2는 EI 괄호의 실제 영어 표시 변환에 맞췄다(KO54/EN55). Notes는 보이는 독립 라벨을 가정하지 않고 입력 위치와 실제 tooltip을 안내했다(KO90/EN91). HUD는 영상 위의 좌측 STUDY·우측 WINDOW이며 알고리즘은 lane 표시라는 소스 배치를 반영했다(KO74/EN75). Reviewer는 이 네 정정의 한영·소스 정합성을 수용했고 신규 지적은 없었다. 시험 결과와 실제 캡처는 해당 재검토에서 미검토였으므로 전체 카드 수용으로 기록하지 않는다.

Reviewer는 선택적 capture 7줄 diff도 읽어 기존 단언·finally Close·Dispose가 유지되며 기술 범위 차단이 없다고 회신했다. 이는 캡처 파일의 실제 내용 검토와 다르다. PNG는 부모가 직접 시각 검토하며, W5 단독 재현 실행은 Reviewer가 아닌 부모가 카드 검증 범위 안에서 승인했다.

기존 HTML title/h1과 `<ol class="steps">` 여섯 단계·다섯 번째 save 위치는 유지했다. 이 절차의 launch·calibration panel·open raw·run PRE와 독립적인 corrected float SHA 비교 oracle은 바꾸지 않았다. 기존 인용 시험의 예외 목록·수집기·최소 개수도 변경하지 않았다. 새 운영 절은 별도 section으로 추가했다.

## 최종 검증 범위와 미관찰 항목

- 최종 GUI rebuild → Integration 프로젝트 전체 → 지정 두 E2E 클래스 전체(`QuickStartWalkthroughScenarios`, `MenuBehaviorScenarios`)를 순차 실행했다. 정확한 환경·두 클래스 필터·TRX/hang 진단 옵션은 [verification-commands.txt](verification-commands.txt)에 있다. 두 클래스 전체는 총 28개 중 25 PASS / 1 FAIL / 2 SKIP, exit 1이며 통과로 덮지 않는다. H01 세 case, 개정 페이지 메뉴 경로를 읽는 W0, 페이지 순서를 따르는 W14와 독립 corrected float SHA 검증은 통과했다.
- W5 `WrongRawSize_WhatTheOperatorSees`의 최초 실패는 COMException `Operation timed out. (0x80131505)`다. stack은 UIA FindAll → Window.ModalWindows → DriveDialog `:1892` → W5 `:703`의 파일 대화상자 조회다. 같은 실행에서 W2·W8·W7 등 유사 대화상자 경로가 통과했으나 원인을 확정하거나 문서와 무관하다고 단정하지 않는다. 부모 승인 후 동일 최종 source/env와 별도 data root의 새 앱에서 원형 W5를 단독 1회 실행했고 1 PASS / 총 1, exit 0이었다. 이 한 번에서는 같은 조회 실패가 재현되지 않았다는 결과일 뿐 원인 해소 증명이 아니다. 최초 전체 실패를 원문·TRX·분모와 함께 보존했고 helper·oracle·timeout은 바꾸지 않았다.
- Native는 기존 외부 `e2e-native-dlls`, 프로세스별 C231 기존 자료 환경, 전용 data root 및 `XPE_E2E_ALLOW_GLOBAL_INPUT=0`을 사용한다. 새 Native 빌드·설치를 하지 않는다. 기존 DLL의 최신 CI provenance 동등성은 미확인이다.
- 사용자 전역 입력 금지를 유지하므로 CI의 opt-in 조건과 차이를 명시하고, SKIP·미발견·PASS를 분리한다.
- 이번 카드의 전체 E2E 프로젝트 실행은 부모 결정으로 수행하지 않는다. 지정 두 클래스 이외의 E2E와 기존 A06 문제는 이번 범위에서 미관찰이며 재개하지 않는다.
- 실제 [quick-start-help.png](quick-start-help.png)는 77386 bytes, SHA256 `382BAAC123D3B4E5FBC0D74484F040CD47A9DC71E084D895A76B71EE8D708EDD`다. 부모가 직접 시각 검토하여 실제 Help 창 제목·packaged 경로·개정 lede의 “then compare A/B results and record a verdict and notes”가 보임을 확인했다. Reviewer도 원문 H01/W14 결과와 PNG를 직접 읽고 문서/캡처 기술 수용을 유지했다. 상단 렌더 관찰에 한정하며 새 운영 절 전체 스크롤·한국어판 전수 시각 검토는 아니다. 파일 존재만을 렌더 증거로 삼지 않았다.

두 E2E SKIP은 `W16_ARealFrameCalibrationSet_IsUsedByTheApp_WithoutWarnings`(XPE_C247_CALIB/XPE_C247_RAW 미설정, XPE-SKIP-ALLOWED:272)와 `W10_AnOldDllWithoutPipelineOut_FailsWithAnUpdateInstruction_AndProcessesNothing`(XPE_C232B_OLD_NATIVE_DIR 미설정, XPE-SKIP-ALLOWED:265)이다. 해당 원문 사유는 [e2e-selected.txt](e2e-selected.txt)에 보존했다. 이 둘을 global input 금지 때문이라고 추정하지 않는다. 최종 Integration의 3 SKIP 이름은 `OffsetQualityTests.TheRealModule_OnDark16AndTheCleanSetMap_MeasuresTheSignedBiasThePaperReports`, `BaselineFingerprintProbe.PrintTheStageHashesOfTheBaselineChainOnRealData`, `DllSearchPathSafetyTests.AFileLinkInsideAnApprovedFolder_ThatPointsOutside_IsRejected`이며 PASS에 포함하지 않았다.

실행 전후 HTML 2개·영향 시험 3파일의 SHA256 다섯 개를 대조해 모두 같음을 확인했다([preflight-sha256.txt](preflight-sha256.txt), [postflight-sha256.txt](postflight-sha256.txt)). source/packaged HTML도 빌드 후 동일했다. 최종 ImageProcTest/testhost 프로세스 수는 각각 0이었다. 지정 data root `gui-c-259-check`는 실행 근거 보존을 위해 남겼고 단독 root `gui-c-259-w5-recheck`는 없었다. 외부 원본·다른 세션의 작업을 되돌리거나 정리하지 않았다.

변경은 HTML 2개와 선택적 capture를 위한 MenuBehavior 7줄뿐이며, `QuickStartCitationTests.cs`와 `QuickStartWalkthroughScenarios.cs`는 수정하지 않았다. 원문 로그는 도구가 반환한 출력 전체에서 줄 끝 공백과 EOF 빈 줄만 정리하고 마지막 개행 하나를 유지했다. PNG/TRX는 로컬 원본을 보존하며 ignore 정책 변경이나 force-add를 하지 않는다.

Reviewer의 최종 읽기 판정은 문서 8요구·한영 동등성·정정 내용·선택적 캡처 7줄·실제 Help 상단 증거의 기술 수용이며 신규 차단은 없었다. 최초 전체 E2E 실패와 단독 통과의 분리 기록도 수용했다. W5 원인 해소나 최초 전체 GREEN을 뜻하지 않으며 최종 카드 종료는 이 잔여 실패를 포함해 리더가 판단한다. Reviewer는 시험을 새로 실행하지 않았고 정상 커밋·CI는 미검토다.

`git diff --cached --check`: exit 0, 출력 없음(이 트리에서 GUI-C-259 명시 경로 15개를 staging한 뒤 관찰).

현재는 **커밋 전 검토 단계이며 SHA는 부모가 별도 회신**한다. 구현 담당자는 stage/commit/push하지 않았다. 정상 훅·최종 커밋 수용을 미리 주장하지 않으며, 임상 화질·배포·다른 E2E 클래스·전체 repository CI 결과도 이번 관찰 범위 밖이다. Tuner 보류를 유지한다.
