# GUI-C-274 재작업 1 — 수정·검증 결과 제출, 리더/main CI 판정 대기

## Claim
O: 선택 문구·보기 탭·Metrics를 읽을 수 있게 한다. KR: 세 지적의 재현→최소 수정→시험·실제 캡처, 메인 창의 나머지 문자 조합 대비 목록을 남긴다.
상태: 재작업 수정·로컬 관측을 제출한다. 카드 종료/병합 판정을 주장하지 않는다. 다음 행동은 리더 검토와 main CI 판정이다.

|지적|변경 전 관측|수정·변경 후 관측|근거|
|---|---|---|---|
|LANE A/B 선택 상자|실제 활성 상태. WPF glyph F0F2F5 / template EAEAEA, 1.072681:1. disabled는1.016122:1.|OS 템플릿/선택 계약을 유지하고 공통 UiControlText Black만 적용. normal17.455715:1, disabled18.427342:1, 열린 selected popup19.431637:1.|AlgorithmBar.xaml, Presentation.xaml; red-pickers-v2.txt, green-pickers.txt, green-popup.txt. 실제 최소/일반 창 캡처 두 장 모두 검정 글자 확인.|
|STUDY가 보기 탭을 가림|baseline 실제 UIA Swipe1431/Split1242/Overlay1674/Difference999 px² 교차. 별도 WPF layout RED2F.|toolbar에 Auto 행, 영상/HUD/배너 본문에 * 행을 둔다. 두 실제 창 크기에서 여섯 탭 모두 STUDY 교차0. 헤더 밴드 밖 영상 화소 계산은 바꾸지 않음.|ViewportShell.xaml; red/green-geometry.txt, header-band-semantic-proof.json, 두 result.jsonl. STUDY 글자8.794117:1, 탭 정의값12.327135:1.|
|Metrics RequestedNotApplied 중간 끊김|실제 표와 WPF 줄 경계에서 RequestedNotAppli / ed. RED8=3P/5F.|표시 converter만 Requested / not applied 등 별칭을 사용. 상태 enum/저장/엔진/시간 suffix 불변. GREEN8P, 실제 두 창에서 단어 사이 줄바꿈. 캡처값은 A/B 모두0.001ms, 시험 oracle0.002ms 원문 보존.|MetricCellDisplayConverter.cs, AnalysisPanel.xaml2셀. LaneA7DD3FC/14171E=10.755605:1, LaneBC4B5FD/14171E=9.713562:1.|

추가 저대비 메타데이터는 이 메인 창의 AlgorithmBar·AnalysisPanel·StudyQueue·VerdictBar·ViewportShell에서만 기존 본문색 B0B6C0을 재사용했다. STUDY/WINDOW·Opacity 정보판은 기존14171E, amber3판은 기존B45309를 불투명하게 사용한다. 다른 창·공용 StatusColor 모델·차이 영상 BGRA·상태 enum·AutomationId·엔진 계약은 변경하지 않았다. 영어 UI 방침을 유지했다.

## Evidence
실제 명령은 WT `.moai/reports/lane-gui/GUI-C-274/rework1/run-*.cmd`, 원문은 같은 폴더 txt에 있다. 이 문서의 P/F/S는 통과/실패/건너뜀이다. 원문·명령·TRX·Sequence56개를 canonical rework1/implementation-evidence에 복사하고 archive-final.json으로 바이트·SHA readback을 확인했다(Sequence 동일사본 중복계수 안 함). env.cmd는 Windows 기본 경로만 복원하며 검증/훅 우회 변수가 없다.

|검증|명령/원문|관측|
|---|---|---|
|선택 상자 RED→GREEN|run-red-pickers-v2.cmd→run-green-pickers.cmd; 공통 dotnet test clients/ImageProcTest.E2ETests -c Debug --no-restore --filter FullyQualifiedName~C274R1_ActualPickerTemplate|4F→4P; 정상/disabled 실제 WPF brush+opacity 계산. popup2P 별도.|
|Metrics RED→GREEN|run-red-metrics.cmd→run-green-metrics.cmd; --filter FullyQualifiedName~C274R1_Metric / red-metrics.txt→green-metrics.txt|3P/5F→8P. 화면 별칭과 raw 상태·시간 보존, 실제 줄 경계 단언.|
|배치 RED→GREEN|run-red-geometry.cmd→run-green-geometry.cmd; --filter FullyQualifiedName~C274R1_StudyAndModeButtons / red-geometry.txt→green-geometry.txt|2F→2P. source-derived 실제 WPF layout의 겹침→0.|
|최초 C274 전체 영향|run-green-impact-c274.cmd: dotnet test clients/ImageProcTest.E2ETests -c Debug --no-restore --filter FullyQualifiedName~C274 --logger console;verbosity=detailed|총32/통과32, exit0. 기존16+신규16.|
|앱 빌드|dotnet build gui/ImageProcTest/ImageProcTest.csproj -c Debug --no-restore --verbosity minimal / build-app-final.txt|경고22/오류0, exit0. 경고 증감 baseline은 미측정.|
|HUD 영향|run-green-hud.cmd, StateDisplayScenarios/ViewportTruthScenarios/ViewStateRenderScenarios / green-hud.txt|총16=9P/1F/6S, exit1. V05 메뉴 조회 null.|
|최초 필터 없는 전체|run-full-rework1-mock.cmd / full-rework1-mock.txt|호스트중단. 총수불명/288P/2F/94S. 전체완주 아님. C274는20P만 완료 관측.|
|Rendering 분리진단|run-rendering-blame.cmd / rendering-blame.txt, TRX|74=72P/2F/0S 정상완주, aborted0. 새16모두P. host중단 미재현·Sequence없음. 전체 대체 아님.|
|2차 필터 없는 전체 진단|run-full-blame.cmd / full-blame.txt, TRX, Sequence|호스트중단. 콘솔59P/16S·총수불명, Sequence77완료/1미완료(popup LaneB). 콘솔과Sequence의 완주 개수는 서로 다른 관측이며 맞춰 쓰지 않음.|
|popup 수명 RED→GREEN|red-popup-lifetime.txt→green-popup-lifetime.txt|2F(HasShutdownFinished=False)→2P(True), 원래 popup대비19.431637 유지.|
|수명 보완 후 C274 영향|run-green-impact-c274-lifetime.cmd / green-impact-c274-lifetime.txt|32/32P, 기존16+신규16.|
|수명 보완 후 Rendering|run-rendering-lifetime.cmd / rendering-lifetime.txt, TRX|74=73P/1F/0S 정상완주. C270만F, C271이번P. 원인 해소 단정 안 함.|
|수명 보완 후 마지막 필터 없는 전체|run-full-lifetime.cmd / full-lifetime.txt, TRX|402=302P/2F/98S, 정상완주·exit1(단언2F), TRX error0/timeout0/aborted0. C27432P·V05S. Sequence없음. 이전 FailFast 원인 해소를 뜻하지 않음.|
|소스 검토|git diff --check; 일반 독립 코드 리뷰|diff check 출력 없음/exit0. Critical/Important/Minor로 확인된 결함 없음. 리뷰어는 빌드/시험/앱을 실행하지 않았고 로그·manifest는 읽기만 했음.|

전체 환경은 XPE_E2E_BACKEND=Mock, XPE_NATIVE_DIR unset, XPE_NATIVE_DIR_EXCLUSIVE=1, XPE_E2E_ALLOW_GLOBAL_INPUT unset, 별도 XPE_GUI_DATA_DIR이다. 환경 scrub과 실행은 각 한 cmd 안에서 한다. 전역 입력은 허용하지 않았다.
첫 전체 원문: `테스트 실행이 중단되었습니다. / 총 테스트 수: 알 수 없음 / 통과:288 / 실패:2 / 건너뜀:94`. 두 번째도 같은 리소스 무한재귀 FailFast였고 console/Sequence의 관측은 위 표대로 분리한다. `MS.Win32.HwndSubclass.SubclassWndProc` stack만으로 원인을 특정하지 않았다.
마지막 전체 원문:
```text
테스트 실행이 실패했습니다.
총 테스트 수: 402
     통과: 302
     실패: 2
    건너뜀: 98
```
마지막 TRX는 total402/executed304/passed302/failed2/error0/timeout0/aborted0이다. 98건은 조건부 건너뜀이며 실제 Native/실데이터 시나리오를 실행한 것으로 세지 않는다. 마지막 실패는 C270·C271 두 건이다.

2차 Sequence의 유일 미완료 시험은 C274R1_SelectedPopupText_IsReadable(LaneBAlgorithmPicker)이며, 이 시험이 근본 원인이라는 증거는 아니다. source와 수명 RED에서 새 시험 소유 STA의 Dispatcher 명시 종료 부재를 확인했다. 제품·공용 OnStaThread를 건드리지 않고 popup 시험의 finally에 로컬 Dispatcher 종료를 보완했다. 본문 예외는 보존하고 cleanup 실패를 통과로 숨기지 않으며, 다음 시험은 새 thread를 사용한다. 이는 [WPF Dispatcher 수명 계약](https://learn.microsoft.com/en-us/dotnet/api/system.windows.threading.dispatcher?view=windowsdesktop-8.0)에 따른 정리이며 원래 FailFast 인과는 미확정이다. [dotnet test의 --blame](https://learn.microsoft.com/dotnet/core/tools/dotnet-test-vstest)은 중단 시 Sequence를 남기는 기본 옵션만 사용했고 dump 옵션/설치는 하지 않았다.

V05 영향 실패 원문은 `View → Reset Comparison View did not appear after the View menu was expanded.`이다. ResetView의 기존350ms 뒤 ResetComparisonViewMenuItem 조회가 null(:354)이며 그 회차 메뉴 bounds는 미관측. 전체에서는 이 조회를 지나간 후 실제 전역 입력을 막는 guard로 SKIP했다. 이전 제출 전체에서도 SKIP이었으므로 원인 해소나 PASS로 바꾸어 주장하지 않는다. 메뉴/대기/단언/skip 조건은 수정하지 않았다.

최초 전체·마지막 전체와 수명 보완 전 Rendering의 C270 ComparisonImageA null(ImportedComparisonTests.cs230/232), C271 ComparisonMarkerList null(ImportedMarkerTests.cs64)은 별도 실패로 기록한다. 수명 보완 후 Rendering에서는 C271이 통과했으나 인과 해결을 주장하지 않는다. 재작업 인과/원인은 미확정이며 카드 지시대로 리더 main CI 판정에 남긴다. 이를 제품 결함 확정이나 로컬 전체 통과로 해석하지 않는다.

### 캡처와 대비
기존 제출 `report-submission-1f8782fb.md`, main-neutral.png, comparison-neutral.png와 모든 로그는 보존했다. 새 캡처는 Mock·기존 synthetic_1024x1024.raw·별도 settings/data로 같은 수정 앱을 실행했다. UIA는 PID17108 소유 창의 패턴만 사용했고 전역 입력 없이 관측했다. 캡처 뒤 closed=true/killed=false로 정상 종료한 다음 전체 E2E를 시작했다.

|새 PNG|창 크기 / 영상 크기|bytes / SHA256|
|---|---|---|
|rework1/after-minimum/main.png|1280×720 /1266×713|640236 / be264e3cda0cb5f89b9fb5283eadf2576b242f48f41cd9bc34daf8c4f19cc89f|
|rework1/after-normal/main.png|1560×920 /1546×913|698925 /115ac8266e03b352e514121de4bf4e881be4b5bab4237996d39c4eb1aa0f3887|

두 PNG에 세 지적의 변경 후 상태가 함께 보인다. 원본 window 캡처 JPEG를 PNG로 저장하고 decoded RGB 완전 동일을 단언했다. 자르기/내용 수정은 없으며 이미지는 커밋하지 않는다. capture-manifest.json에 실제 UIA 선택값·9개 Metrics 라벨·6개 교차면적·바이너리 SHA를 보존했다. UIA의 미지원 색 속성으로 대비를 추정하지 않았다.

소스 대비 감사는 문자 요소330·상태 조합31·관련 파일14로 분모를 유지하고 read→종료 SHA변경0을 확인했다. baseline과 after를 별도 보존했다. 계산은 WCAG 상대 휘도 및 alpha 합성 정의값이며 화면 pixel 실측과 구분한다.

|추가 조합|변경 후 정의값 대비|
|---|---|
|count/thumbnail B0B6C0|6.604344:1|
|Runset label|7.729063:1|
|STUDY/WINDOW 제목|8.794117:1|
|HUD metadata/Opacity|5.593284:1|
|amber FFFBEB/B45309|4.842409:1|

4.5 미만 잔여 전체 목록(정의/미관측 분리): StudyEntry.cs66-79의 StatusColor fallback5A5F6A glyph는 StudyQueue 기본2.800679/hover2.461490/selected2.274949:1이며 실제 도달은 미관측이다. 공용 모델을 임의 변경하지 않았다. Combo Black을 문법상 dark 조상 배경과 계산한1.171050:1은 실제 template 선택면 대비가 아니므로 결함/통과 판정에 쓰지 않는다. 실제 template 값은 위 별도 WPF 관측이다. 배경 없는 renderer 흰 Difference 범례는 영상값에 의존하며 실제 pixel 대비 미관측이다. 따라서 전 Main 모든 상태가4.5이상이라고 주장하지 않는다.

## Baseline-attribution
WT `C:/Users/drake/.moai/worktrees/projects/image-processing/GUI-C-274`, branch `WT-gui-shared-presentation`, parent `1f8782fbd9bb4c9884333d0bbe90b6a508ebecf4`+동결 수정. 최초 제출의 parent는1e5037353430cecef97d4d86b23fe96a87cc21de이며 새 재작업과 구분한다. 제품7경로·시험2경로 변경에 이 보고서를 더한다. 수명 보완에서는 시험파일1개만 바뀌었고 최종 SHA는42080573a5a69bd32b553640092cc4f8fc8f86e338094a87ce06712f54c54626이다. 제품7파일과 App DLL은 캡처 이후 동일함을 freeze-final.json으로 다시 확인했다. .codex/hooks.json 등 기존 별도 작업/로그/초안은 포함하지 않는다.
캡처 DLL SHA256 `cb781a226178df0c0e0617e58f9deddae47df6893c3562b334fc77be095d3408`; ProductVersion은 빌드 당시 HEAD1f8782fb 표기라 미커밋 수정 식별을 단독으로 대신하지 못한다. final-source-snapshot.json(수명보완전)·final-source-snapshot-lifetime.json(보완후) 및 capture-manifest.json의 파일별 SHA로 귀속한다. HEADER semantic proof는 색 보완 전 header 이동 단계에 대한 기계 대조이며 최종본 모든 XML이 동일하다는 뜻이 아니다. 최종 ID는 동일, Analysis 두 셀 converter 바인딩만 의도 변경했다.
fetch 후 origin/main은README2행 전용 ebf0b344로 분기1/1. 이 WT의 HEAD/branch/source는 바뀌지 않았고 merge/rebase하지 않았다. 측정값을 최신 main의 관측으로 쓰지 않는다.
`git diff --cached --check`: 제품/시험9경로를 명시 staging한 상태에서 출력없음·exit0(commit-source-check.txt). 보고서 포함 최종10경로의 `git diff --cached --check`도 출력없음·exit0(commit-final-check-v2.txt). 첫 최종check에서 보고서 끝공백1건을 발견해 제거했으며 원문은 commit-final-check.txt에 보존했다. core.hooksPath는 원래 image-processing/.git/hooks이며 SKIP_MOAI_PRECOMMIT은unset. 현재 셸 PATH에는 moai가 없어 설치된3.2.0(a85be1244) 경로만 커밋 프로세스 PATH에 추가해 정상 hook이 moai를 찾도록 한다. 훅/품질설정은 변경하지 않는다. 커밋 결과는 별도 관측 후 응답에 기록한다.

## Gaps
로컬 전체0실패는 미충족이다. 앞선 두 전체는 호스트가 중단돼 suite 분모·완주를 주장할 수 없다. 마지막 SKIP98개·38사유를 full-lifetime-skip-reasons.json에 개별 시험/원문 이유로 보존했다. Native DLL/데이터, Mock 체인 차이, 전역 입력 제한, legacy EXE·SelfCheck·E2E runner 미빌드 및 측정 환경변수 등의 조건을 분리했다. 앞선 중단 실행의94개와 섞지 않는다.
증거 공백: 수명 단언 추가 직전 시험 source exactbytes 사본을 보존하지 못했다. 당시 SHA0da1562233145cb6922ff78a136991d52b5a58625a911aa4a80c703b911e8d1a·snapshot·로그는 있으나 이를 원문 사본 보유로 바꾸어 쓰지 않는다. 추정 복원하지 않았다. 이후 수명 RED exactcopy b1509fa9cda8e6d98bbd268e4fc90d79c5c9401eb9c13c4094a62f2ab4212709는 ReworkPresentationTests.lifetime-red.cs.txt에 보존했다.
Native NOT CORRECTED 배너의 실제 geometry(D258)는 Mock에서 SKIP이므로 미측정이다. 다른 DPI/OS테마/실제 pointer-hover·메뉴 전 상태, 키보드 전역 순회, renderer 영상종속 범례·fallback 도달, Native/SelfCheck/legacy EXE와 main CI는 미측정이다. 비교 창은 이번 재작업에서 새로 캡처하지 않았고 이전 제출 캡처와 기존 검증을 보존했다. 새로 재관측했다고 주장하지 않는다.

## Residual-risk
세 지적의 실제 화면과 수정 회귀는 관측했으나 모든 사용자 환경을 보증하지 않는다. C270/C271와 V05는 인과 미확정이며 공유 데스크톱/OS 템플릿에 의존하는 미관측 상태가 있다. leader가 main에 병합한 뒤 main CI 전체 화면 E2E로 최종 판정해야 한다. 훅 우회·이력 재작성·push·main 병합은 하지 않는다.
