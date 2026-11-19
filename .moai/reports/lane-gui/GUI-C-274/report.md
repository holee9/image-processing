# GUI-C-274 재작업 2 — 로컬 지정 검증 통과, 리더/main CI 판정 대기

## Claim
O: App 없이 생성되는 창·컨트롤도 공통 표시 정의를 찾도록 한다.
KR: 자원 오류를 재현하고 독립 생성·SelfCheck·A04/A06으로 확인한다. 상태는 로컬 지정 검증 통과이며 카드 종료/병합/CI 통과를 주장하지 않는다. 다음 행동은 정상 훅 커밋 후 리더 재병합과 main CI 판정이다.

MainWindow와 6개 UserControl(AlgorithmBar·AnalysisPanel·StudyQueue·TopBar·VerdictBar·ViewportShell)의 Resources에 `/ImageProcTest;component/Resources/Presentation.xaml`을 병합했다. 정의값을 복제하지 않고 같은 파일을 참조한다. 기존 ComparisonImportWindow의 자체 병합 방식은 변경하지 않았다.
MainWindow의 root StaticResource Style 속성은 로컬 Resources보다 먼저 해석돼 실패했다. 동일 UiWindowStyle 참조를 Resources 뒤의 Window.Style property element로 이동해 조회 순서를 명시했다. DynamicResource로 오류를 지연시키지 않았다.

SelfCheck의 PresentationResourceScenarios는 App.Current가 null인 전용 STA에서 8개 실제 constructor, 로컬 사전·키·brush 값, Main Style 참조 일치를 검증한다. Lifetime.Run에서 호출하므로 기존33개 시나리오 카운트 계약은 그대로다. Program.cs는 HEAD와 exactbytes 동일하다.
자원 문제가 풀린 뒤 기존 Metrics UI 단언이 raw 상태문구와 표시 별칭을 동일하게 비교해3건 실패했다. 시험 fixture의 승인된3표시(`Requested / not applied`, `Applied / no change`, `Not requested`)만 literal 기대값으로 대조하도록 보완했다. production converter로 기대값을 만들지 않았고 Metric label·raw 모델·시간·Delta의 기존 단언은 유지했다.

## Evidence
원문·실제 cmd·소스 사본은 WT `.moai/reports/lane-gui/GUI-C-274/rework2/`에 있으며 canonical `rework2/implementation-evidence/`에도32개를 바이트 동일하게 보존했다. `archive-final.json`에 SHA와 읽기 확인을 기록했다. 아래 P/F/S는 통과/실패/건너뜀이다.

|단계|명령/원문|관측|
|---|---|---|
|main CI 실패 읽기|run44417040901, gui-shell-runners/job135200090655 및 gui-e2e-native/job135219631134|SelfCheck UiMetadataText missing, A04/A06 실패 확인. decoded 로그를 ci/에 UTF8로 내용 변경 없이 보존.|
|최초 로컬 SelfCheck RED|dotnet run --project gui/ImageProcTest.SelfCheck/ImageProcTest.SelfCheck.csproj -c Debug|동일 자원 오류를 담당 실행에서 관측했으나 최초 원문 파일은 후속 내부 redirect로 덮였다. 아래 별도 RED로 바꾸어 보유했다고 하지 않음.|
|보존된 constructor RED|red-standalone-constructors.txt|App 없는 생성 실패. 기존 ComparisonImportWindow 자체 사전 경로는 통과.|
|첫 병합 후보|run-green-selfcheck-candidate.cmd / green-selfcheck-candidate.txt|6개 UserControl+기존Comparison 생성 통과, Main root Style forward lookup 실패 유지.|
|Style 순서 수정 후보|run-green-selfcheck.cmd / green-selfcheck.txt|8개 독립 생성 통과 후 기존 Metrics UI 기대3건 불일치. 이 실패를 자원 실패와 구분.|
|최종 SelfCheck|run-green-selfcheck-alias.cmd: dotnet run --project gui/ImageProcTest.SelfCheck/ImageProcTest.SelfCheck.csproj -c Debug|exit0. 독립 생성8/8·기존Lifetime33·GUI-S0 self-check 통과.|
|GUI E2E runner 빌드|run-build-gui-runner.cmd: dotnet build gui/ImageProcTest.E2E/ImageProcTest.E2E.csproj -c Debug --verbosity minimal|exit0. 기존 .NET 프로젝트만 빌드, 설치 없음.|
|지정 A04/A06|run-target-a04-a06.cmd: dotnet test clients/ImageProcTest.E2ETests -c Debug --no-restore --filter FullyQualifiedName~A04_SelfCheck_ActuallyRuns_AndThePassIsReported 또는 FullyQualifiedName~A06_GuiE2E_RunsFromTheApp_AndTheVerdictIsReported (실제 OR/인용은 cmd 원문)|2P/0F/0S, exit0. 둘 다 실제 실행, 건너뜀 없음. 기존 A04/A06 시험 소스는 변경하지 않음.|
|C274 영향|run-impact-c274.cmd: dotnet test clients/ImageProcTest.E2ETests -c Debug --no-restore --filter FullyQualifiedName~C274 --logger console;verbosity=detailed|32P/0F/0S, exit0. 이번 수정 소스에서 새로 측정.|
|source cached check|git diff --cached --check / source-cached-check.txt|제품·회귀9경로를 명시 staging한 상태에서 출력없음·exit0. 보고서 포함 최종 check와 정상 훅 결과는 커밋 관측에 별도 기록.|

최종 SelfCheck 원문:
```text
GUI-C-274 standalone presentation resources passed: 8/8.
Lifetime scenarios passed (33 scenarios).
GUI-S0 self-check passed.
```
A04/A06 원문:
```text
총 테스트 수: 2
     통과: 2
```
C274 영향 원문:
```text
총 테스트 수: 32
     통과: 32
```
8개 독립 생성과 기존 Lifetime33은 서로 다른 지표다. 전체 SelfCheck 분모를41개로 바꾸어 주장하지 않는다. A04/A06은 Mock 조건에서 runner 결과·실제 backend·pass 상태를 검증하는 기존 단언을 그대로 실행했다. 환경은 Mock, NativeDir unset/exclusive1, global-input 허용 변수 unset, 별도 GUI data이다.

일반 독립 코드 검토에서 확인된 Critical/Important/Minor 결함은 없었다. 검토자는 소스·기존 원문만 읽었고 빌드/시험을 재관측하지 않았다. 새 검증은 Window.Close 호출과 전용 Dispatcher 종료를 수행하지만 MainWindow.Close의 비동기 종료 완료까지 입증한 것은 아니다.

## Baseline-attribution
같은 WT `C:/Users/drake/.moai/worktrees/projects/image-processing/GUI-C-274`, branch `WT-gui-shared-presentation`, parent `766c10fed6a8ca5f56043eb303604c5a150707e0`+동결 수정이다. 수신 시 tracked변경0. 제품7XAML·SelfCheck2경로 변경에 보고서1경로를 더한다. 기존 `.codex/hooks.json`·이전 로그/초안은 제외·보존한다.
`before-manifest.json`의 초기 Program+7XAML8건은 HEAD와 exactbytes 동일한 사본임을 확인했다. Lifetime 사본도 추가 보존했다. `xaml-semantic-proof.json`은 승인된 merge wrapper와 Main Style 순서 이동을 역정규화하면7파일 XML tree가 기존과 동일함을 확인한다. 색·ID·기존 binding·스타일 정의값 변경은 없다.
공통 Presentation.xaml, App.xaml, Program.cs는 HEAD와 exactbytes 동일하다. 최종9소스 SHA는 `final-source-snapshot.json`과 다시 일치했다. 검증 App DLL SHA256은 `bc7c0416e625b12cbdc8340385b793a20ccee50b1fa9ed97fe3004b607d3761f`이다.
원격 읽기 갱신 후 origin/main은ad786dc7, ownHEAD는766c10fe로 원격2/로컬0 차이다. 리더 지시대로 같은 트리에서 측정했고 merge/rebase하지 않았다. 로컬 결과를 최신 main CI 관측으로 쓰지 않는다.
재작업1 제출 보고서는 `report-submission-766c10fe.md`(SHA d6fa1673d40f895befed9cf1f994553a64862986669b232dc0d0e68beab86b96)에 보존했다. 이전402전체 결과는 이번 재작업2의 새 측정이 아니다.

## Gaps
최초 로컬 RED exact원문 파일은 덮어쓰기 실수로 보존하지 못했다. CI 원문·별도 constructor RED·중간 두 실패·최종 GREEN을 서로 다른 증거로 남기고 추정 복원하지 않았다. 사고 후 실행은 고유 로그와 exists 가드, 단일 redirect를 사용했다.
이번에는 지정된 SelfCheck/A04/A06/C274 영향만 실행했고 전체402·Native417·새 main CI는 실행하지 않았다. 전역 입력·다른 DPI/테마·새 화면 캡처·컴파일 경고 증감은 미측정이다. 빌드에는 경고가 있으며 경고0이라고 주장하지 않는다.

## Residual-risk
로컬 검증은 App 독립 자원 공급과 지정 runner 계약을 확인한 것이다. Native 전체와 통합 main 환경까지 보증하지 않는다. 리더가 재병합한 뒤 main CI로 최종 판정한다. 훅 우회·설정 변경·push·main 병합은 하지 않는다.
