2026-11-04 08:22 KST — GUI-C-258 (Refs #282, #274) 구현 인계. 부모 08:35~08:37 손목 화면·Log 재검증과 Reviewer 08:38 읽기 검토 회수 완료, 별도 커밋 준비.
## 확인한 결과

- Metrics는 해당 렌더의 레인별 preprocess/enhance_basic/gsvg/ai 상태·시간, 실제 VOI C/W·자동/수동, 실행한 보정 단계 수, 기존 EI/DI를 표시한다. 화질 합격 지표를 새로 계산하지 않는다. EI/DI에는 측정값이며 합격 기준이 아니라는 문구를 유지한다.
- B가 아직 끝나지 않으면 A의 측정값을 복제하지 않는다. B가 A 결과를 공유하면 공유 사실을 명시하고 독립 Delta를 비운다. 두 그림·독립 facts·해당 수치가 실제로 있을 때만 B−A를 표시한다. 실행하지 않은 설정 변경이나 실패한 새 실행은 화면에 남은 이전 그림의 측정값을 유지하며, backend 교체는 지운다.
- 빨간 Native+Skipped 경고 정책을 유지했다. 배너를 실제 STUDY HUD 높이 아래로 배치하고 ZIndex=100으로 그린다. 최종 Native UIA에서 전체 텍스트와 배너가 뷰포트 안에 있고, HUD/toolbar와 겹치지 않음을 정상 폭과 실제 최소 폭 1280 px에서 확인했다.
- 하단은 ① chain 상태·실제 자동 창 ② 단계 시간 ③ 마지막 경고/오류의 세 칸이다. 판정 거절 사유는 기존 alert 정책을 바꾸지 않고 세 번째 칸에 우선 표시한다. 성공 메시지를 경고로 분류하지 않는다. 긴 action/chain 문장은 tooltip/log와 표준 UIA 상세 속성으로 보존했다.
- Pipeline 메뉴는 PRE → Baseline → AI → 구분선 → Display → Automatic window → Stop/Timing/Diagnostics 순서다. 최종 Native UIA로 실제 메뉴 순서를 확인했다.
- 카드 최신 영어 UI 결정을 적용했다. 한국어 도움말, AutomationId, 설정 키, 저장 JSON의 고정값·스키마, reason code·로그 식별자는 유지했다. 도구 창의 생성/평가/2G 실행 버튼은 시험에서 누르지 않았다.

## 변경 파일과 논리 단위

제품·SelfCheck 경로는 `gui/`, E2E 경로는 `clients/ImageProcTest.E2ETests/` 기준이다. 파일별로 순차 수정했고 git stage/commit/push는 하지 않았다.

| 파일 | 변경 |
|---|---|
| `gui/ImageProcTest/ViewModels/MainWindowViewModel.cs` | MetricRows/MetricMeasurementNote, footer 세 역할, 판정 거절 이유의 표시 알림, 표시용 EI 영어 문구. 원래 stage Reason은 보존 |
| `gui/ImageProcTest/MainWindow.xaml` | 세 칸 footer·전체 문장 tooltip/UIA 상세 속성·메뉴 순서·영문 Tools 이름 |
| `gui/ImageProcTest/Views/AnalysisPanel.xaml`, `.xaml.cs` | 실제 측정 행 바인딩/줄바꿈, UIA 실제 TextBlock 식별자, 영어 라벨·tooltip, 원문을 보존하는 Log 표시 converter |
| `gui/ImageProcTest/Views/ViewportShell.xaml`, `.xaml.cs` | HUD/toolbar/배너 실제 컨테이너 노출, HUD 높이에 따른 배너 위치·최상단 그리기 |
| `gui/ImageProcTest/Controls/AutomationBorder.cs` (신규) | 배너/HUD/toolbar/Metrics 컨테이너의 실제 UIA bounds용 표준 peer |
| `gui/ImageProcTest/Views/TopBar.xaml` | 기존 RuntimeCommonVersionText ID·실제 값은 header로 이동, 영어 Focus 문구 |
| `gui/ImageProcTest/Views/AlgorithmBar.xaml` | 영어 VOI width/Stale/tooltip |
| `gui/ImageProcTest/Views/CalibrationWorkflowWindow.cs`, `ComparisonImportWindow.cs` | 제목·라벨·버튼·안내·상태·표시 예외 영어화. 실행/저장 로직은 변경하지 않음 |
| `gui/ImageProcTest/Services/CalibrationWorkflowService.cs`, `Services/Comparison/ComparisonReasonText.cs`, `Services/Comparison/ComparisonBackend.cs` | 창으로 전달되는 표시용 예외/거절 사유 영어화. 저장 고정값·프로토콜·reason code는 보존 |
| `gui/ImageProcTest.SelfCheck/Program.cs`, `LifetimeScenarios.cs` | `--display-contract`, 실제 WPF 셀/영어 창/경고 정책 회귀 |
| `Fixtures/ApplicationFixture.cs` | `XPE_E2E_EXISTING_CALIB_DIR` 지정 시 기존 3xcal 존재 확인 후 generator 생략, 잘못 지정하면 명시 실패. 외부 폴더를 삭제하지 않음 |
| `Scenarios/Workflows/StateDisplayScenarios.cs` (신규) | 실제 Native geometry·Metrics/footer/메뉴 UIA·기존 보정 경로 오류 시험, 자체 설정 파일 정리 |
| `Scenarios/Workflows/FailedRenderScenarios.cs` | W23/W26의 단일 호출 전제를 자체 명시 설정으로 준비·정리. 기존 호출수/실패 검출 단언 유지 |

Footer 상세 읽기 위치만 조정한 기존 E2E 파일: `Scenarios/Workflows/WorkbenchObservation.cs`, `DeterministicBaselineScenarios.cs`, `EvidenceBundleScenarios.cs`, `GsvgWristSliceScenarios.cs`, `HiddenSelectionCopyObservation.cs`, `LaneDenoiseScenarios.cs`, `MenuBehaviorScenarios.cs`, `MenuCommandScenarios.cs`, `ProcessingChainScenarios.cs`, `SettingsWarningRecoveryScenarios.cs`, `TwoLaneWorkbenchScenarios.cs`, `UnreadableSettingsScenarios.cs`, `WorkflowMenuScenarios.cs`, `WorkflowScenarios.cs`; `Scenarios/Operator/QuickStartWalkthroughScenarios.cs`. `UnappliedSettingsScenarios.cs`는 상세 읽기와 영어 기대문을 함께 바꿨다. 최종 제품 14개·SelfCheck 2개·E2E 19개, 합계 소스 35개(신규 2개 포함)다. 최초 34개에서 Log 표시 보완으로 기존 View 코드 1개가 추가됐다.

## 기준과 재현 로그

측정 트리: `D:/workspace-github/xpe-gui`, `dev/gui`, HEAD `84c2551f9e02c85ffcf447621dfd9a31516852c8` + 이번 변경. 256/257 및 기존 설정·프레임워크·과거 보고서 변경은 보존했다. 각 로그는 이 구현 과정에서 해당 시험을 실제 실행한 출력이며, 이전 카드의 통과 수를 이번 수에 섞지 않았다.

| RED/가정 | 관측과 처분 |
|---|---|
| 빈 Metrics/footer | `red-metrics.txt`: rows=0/UI=0, 단계·실제 VOI·보정·EI/DI·Delta·footer 역할 단언 실패. 구현 전에 실행 |
| 폐기한 실패 가정 | 같은 첫 로그의 “failed render invalidation” timeout은 제품 결함으로 세지 않는다. 실패해도 기존 그림/record를 유지하는 계약을 시험이 잘못 가정했다. 이를 이전 측정값 보존, backend 교체 시 제거로 고쳤고 통과 |
| 배너 계측 seam | `red-geometry.txt`: Border 자체 peer가 없어 컨테이너를 조회하지 못함. 빈 화면이나 정책 실패의 근거가 아님 |
| 실제 배너 겹침 | `red-geometry-measured.txt`: banner `{X=936,Y=756,W=524,H=46}`, STUDY `{X=766,Y=666,W=234,H=148}`. 실제 직사각형 교차를 재현한 뒤 위치 수정 |
| 영어 기대문 | `red-english.txt`: 선택 5개 중 2개 통과·3개 실패. 실제 기존 한국어 mark와 새 영어 기대문 불일치. 이후 U01–U05 모두 통과 |
| W23/W26 준비 | `red-fault-preparation.txt`(원 출력 `green-related-uia.txt`): 선택 10개 중 8개 통과·2개 실패. 기존 bin 설정의 동명/다른 실효 mode 때문에 시작 호출이 2회였다. 정본 B 요청은 정상이며 제품 counter/oracle를 낮추지 않았다 |
| Metrics UIA 현재 행 | `red-metrics-scroll.txt`, `red-metrics-peer.txt`, `red-metrics-visibility.txt`, `red-metrics-current-peer.txt`: 값이 같은 Calibration 행의 이전 item peer가 0×0/offscreen이거나 실제 자식 조회에서 빠짐. ScrollViewer는 380×594, verticallyScrollable=false라 단순 fold 가설은 기각. 실제 WPF arrange에서는 모든 행 label/A/B TextBlock 값·RenderSize가 양수였음 |

마지막 문제는 같은 텍스트의 새 MetricRow를 value-record로 교체하는 경계였다. 행의 값/바인딩/직접 TextBlock ID/positive rect 단언을 고정한 채 **record만 읽기전용 참조 class로 바꾼 한 변수 대조**에서 Native 시험이 통과했다. 해당 목록 교체/peer 동일성 문제로 판단하며 WPF 전반의 버그로 일반화하지 않는다. 단순히 컨테이너 이름에 VM 값을 복제해 통과시킨 시험은 사용하지 않는다.

## 실제 실행 명령·최종 결과

각 명령은 기존 `.moai\reports\lane-gui\GUI-C-254c\env.cmd`로 Windows 환경을 초기화한 뒤 설치된 `C:\Progra~1\dotnet\dotnet.exe`를 사용했다. E2E/Integration 명령에는 `DOTNET_CLI_UI_LANGUAGE=en`; Native 표시 시험에는 다음 환경을 설정했다.

```text
XPE_E2E_BACKEND=Native
XPE_NATIVE_DIR=D:/workspace-github/image-processing/build/e2e-native-dlls
XPE_E2E_EXISTING_CALIB_DIR=D:/workspace-github/xpe-data/calib-real-v2
```

Native 표시 fixture는 위 세 맵의 존재만 검증하고 generator를 호출하지 않는다. 빨간 경고 시험은 그 뒤 명시적인 존재하지 않는 calibration 경로를 넘겨 Native+Skipped를 만든다. 기본 fixture 전체를 재설계하지 않았다.

```text
dotnet build gui/ImageProcTest.SelfCheck/ImageProcTest.SelfCheck.csproj -c Debug --no-restore --nologo -v:q
dotnet build clients/ImageProcTest.E2ETests/ImageProcTest.E2ETests.csproj -c Debug --no-restore --nologo -v:q
dotnet build clients/ImageProcTest.IntegrationTests/ImageProcTest.IntegrationTests.csproj -c Debug --no-restore --nologo -v:q
dotnet gui/ImageProcTest.SelfCheck/bin/Debug/net8.0-windows/ImageProcTest.SelfCheck.dll --display-contract
dotnet gui/ImageProcTest.SelfCheck/bin/Debug/net8.0-windows/ImageProcTest.SelfCheck.dll --study-contract
dotnet gui/ImageProcTest.SelfCheck/bin/Debug/net8.0-windows/ImageProcTest.SelfCheck.dll --lane-contract
dotnet test clients/ImageProcTest.E2ETests/ImageProcTest.E2ETests.csproj -c Debug --no-build --filter FullyQualifiedName~StateDisplayScenarios --logger console;verbosity=detailed
```

| 영역 | 실제 분모·결과 | 원문 |
|---|---|---|
| Mock VM/WPF, 258 | 3/3 통과: 실제 배치한 9행의 TextBlock label/A/B 값과 양의 크기, status/ms/VOI/EI·공유/대기 B·Delta, 실패/교체, 거절 사유 footer 바인딩; 영어 창 25+24 라벨; Native/Mock 경고 정책 | `green-metrics.txt` |
| Mock VM/WPF, 256 인접 | 1/1 통과 | `green-256.txt` |
| Mock VM/WPF, 257 인접 | 6/6 통과: PRE 보존·동일 실효 설정 단일 호출·현재 실패/늦은 성공/늦은 실패 facts 계약 포함 | `green-257.txt` |
| 기존 Mock 제품 UIA, 영어+footer reader | U01–U05 및 W01b/W02/W07 8/8 통과. 같은 실행의 W23/W26 최초 실패는 위 RED에 분리 | `green-related-uia.txt` |
| 기존 Mock 제품 UIA, fault 재준비 | W23/W26 2/2 통과. 원래 단언 1→2→3, 1→2 그대로 유지 | `green-fault-uia.txt` |
| Native 제품 UIA | 실제 표시 시험 2/2 통과: geometry 1개, Metrics/footer/메뉴 1개. 같은 선택 실행의 보정 경로 validator 1/1 통과. 합계 3/3, skip 0 | `green-geometry-final.txt` |
| 소스/표시 인용·진단 Integration | QuickStartCitationTests/AiDiagnosticsTests 8/8 통과 | `green-citations.txt` |

Mock 제품 UIA filter는 다음과 같았다. 전체 E2E 또는 Operator 전체를 실행하지 않았다.

```text
FullyQualifiedName~UnappliedSettingsScenarios|FullyQualifiedName~WorkflowScenarios.W01b|FullyQualifiedName~WorkflowScenarios.W02|FullyQualifiedName~WorkflowScenarios.W07|FullyQualifiedName~W23_FailedRender|FullyQualifiedName~W26_FailedRender
```

Fault fixture 수정 후에는 두 fault case만 선택 재실행했다. 고정 자체 JSON은 다음과 같다. 다른 bin/사용자 설정을 읽지 않는다.

```json
{"settingsSchemaVersion":1,"preprocessInChain":true,"laneAAlgorithm":"No grid correction","laneBAlgorithm":"No grid correction","gsvgMode":"None","laneBVoiWindowWidth":0,"gsvgDenoiseK":2,"laneBGsvgDenoiseK":2}
```

주요 GREEN 원문 발췌(전체 출력은 각 로그):

```text
Lifetime scenarios passed (3 scenarios).
Lifetime scenarios passed (1 scenarios).
Lifetime scenarios passed (6 scenarios).
W23 start: title='[MOCK] ImageProcTest GUI-S0 — FAULT INJECTION ARMED' status='faultInjection=display-pipeline-after:2 calls=1'
W23 preset: v2 -> v3 indicator='' status='faultInjection=display-pipeline-after:2 calls=2'
W23 failed apply: v3 indicator='STALE — the display pipeline failed; the image shown is from before the failed attempt.' status='faultInjection=display-pipeline-after:2 calls=3'
D258 actual calibration TextBlock: id=MetricLabel:Calibration stages; text=Calibration stages; rect={X=1600,Y=878,Width=100,Height=13}; offscreen=False
D258 actual calibration TextBlock: id=MetricA:Calibration stages; text=not run; rect={X=1766,Y=878,Width=38,Height=13}; offscreen=False
D258 actual calibration TextBlock: id=MetricB:Calibration stages; text=not run; rect={X=1870,Y=878,Width=38,Height=13}; offscreen=False
Test Run Successful.
Total tests: 3
     Passed: 3
```

최종 Native geometry: launch viewport 884×638, minimum viewport 604×638. 배너 544×46, 실제 문장 TextBlock 520×32. 최소 창 폭은 1280 DIP, 96 dpi에서 실제 1280 px로 측정했다. HUD와 toolbar 교차 0, viewport/banner/text containment 단언을 모두 통과했다. Native 실제 식별은 `common=0.1.0 | display=1.0.0 | src=e2e-native-dlls`였다. 이는 기존 DLL의 표시 관측이며 최신 엔진 검증을 뜻하지 않는다.

최종 앱+SelfCheck 빌드 Exit 0, 경고 23개·오류 0개(`build.txt`). E2E 빌드 Exit 0, 경고 7개·오류 0개(`build-e2e.txt`). 증분 빌드와 WPF 임시 프로젝트 재컴파일 범위가 달라 경고 집합 전체의 무증가를 주장하지 않는다. `git diff --check -- gui clients/ImageProcTest.E2ETests .moai/reports/lane-gui/GUI-C-258` Exit 0; 기존 CRLF→LF 안내 외 공백 오류 없음. `git diff --exit-code -- gui/ImageProcTest/help/quick-start.ko.html` Exit 0, 출력 없음.

## 영어화 목록·기존 계약 보존

| 위치 | 표시 변경 목록 |
|---|---|
| MainWindow Tools | PRE Calibration: Validate, Generate, Reload and Evaluate / Compare Imported 2G and New PRE Outputs |
| AlgorithmBar | VOI 폭→VOI width, 오래됨→Stale, 두 picker와 stale tooltip |
| TopBar | 미적용→Not applied, Focus 미연결 설명 |
| AnalysisPanel | 기본 3단계 연결→3 calibration stages connected, 연결 대기→Connection pending, 미적용→Not applied, Ghost/temperature/nonlinearity/binning bypass·kVp·Lane B denoise 설명 |
| CalibrationWorkflowWindow | 제목, 단계 1–6, 도구/출력 경로·크기·RAW 목록·첫 Dark·expiry·입력 선언·holdout/oracle/reference/gain 의미, validation/generation/reload/apply/evaluation/취소 거절·busy/실패 안내 |
| CalibrationWorkflowService | 입력 크기/검사/생성/재로드·SHA/독립 holdout/평가/로컬 도구의 표시용 예외. acquisitionMetadata/quality/oracle/reference 등 저장 고정 문자열은 그대로 |
| ComparisonImportWindow | 제목·Browse/Open/Compute/Cancel/WL/Fit/blocked 2G, 출처·계약 검토·SHA·미검토 계약·취소/거절·ROI/미리보기·통계·저장 위치·미귀속/품질 비판정 안내 |
| ComparisonReasonText | 사유 code 35개의 설명과 기본 설명. code와 거절 정책은 그대로 |
| 추가로 발견한 ComparisonBackend | 창으로 전달되는 Python/script/manifest/layout/result/schema/status/run_id/취소 오류 문장. WriteRecord의 프로토콜 키·원본 script 출력은 그대로 |
| 표시 EI | 원래 Reason의 `보정 안 된 EI`는 보존하고 ExposureIndexText/tooltip 표시에서만 영어로 바꿈. 로그 식별자·기존 측정 계산은 변경하지 않음 |

Footer ID는 보존했다. `ChainStatusText.Name`은 새 요약이며 `ItemStatus`는 전체 chain, `HelpText`는 기존 `render #N` 계약 그대로다. `StatusBarText.Name`은 마지막 경고/거절 이유이며 `HelpText`는 전체 action status다. `FrameTimingText.Name`은 단계 시간이며 `HelpText`는 기존 frame timing 전체값이다. 기존 reader는 이 상세 속성을 읽고, 새 Native UIA는 **실제 보이는 Name의 세 역할과 실제 셀·메뉴·직사각형**을 별도로 검증한다. 기존 action 문장을 UI Name에 복제해 통과시키지 않았다. RuntimeCommonVersionText의 ID와 전체 실제 값은 header에 유지했다.

## 정리·미확인·위험

- 258 Native UIA는 `--automation-settings`로 자체 `gui-c-258-settings/appsettings.json`만 사용하고 fixture 종료 때 그 자체 폴더를 정리한다. 두 fault fixture도 자체 `gui-c-258-fault-main`, `gui-c-258-fault-detached` 설정만 사용한다. 최종 확인에서 세 폴더 `exists=False`, SelfCheck 고정 임시 폴더 없음, ImageProcTest 프로세스 0개였다. 외부 map/기존 폴더는 삭제하지 않았다. 최초 배너 계측 실행은 기존 공용 bin 설정을 사용했으며 최종 시험은 자체 설정으로 분리했다.
- 구현 담당은 synthetic 1024 RAW의 Native UIA 및 Mock/WPF를 검증했다. 부모의 실제 손목 화면은 아래 별도 기록이다. 실제 손목 품질·최신 DLL·새 보정 생성·배포는 검증하지 않았다.
- 신규 PSNR 등 품질 지표, 비교 엔진 실행 권한, 저장 JSON 스키마·고정값, #274 경고 정책, Tuner 보류 범위는 변경하지 않았다.
- 수정한 모든 상세 reader가 걸리는 전체 E2E/Operator suite, 전체 저장소 시험, 커버리지 85% 및 LSP 서비스 진단은 미측정이다. 실행한 관련 filter와 미실행 범위를 위에 분리했다. 통합 브랜치 CI `gui-shell-runners`가 저장소 전체 판정의 소유자이며 현재 PENDING이다.

구현 에이전트는 이 보고서 인계 뒤 파일 쓰기를 중단한다. 부모가 최종 실제 GUI·독립 검토·정상 훅 커밋을 수행한다.

## 부모 실제 손목 GUI — 2026-11-04 08:24~08:27 KST

- 기준 트리 `D:/workspace-github/xpe-gui`, `dev/gui`, HEAD `84c2551f9e02c85ffcf447621dfd9a31516852c8` + 이 카드 source 변경. 최종 Debug EXE는 `gui/ImageProcTest/bin/Debug/net8.0-windows/ImageProcTest.exe`, 부모 검증 프로세스 PID28944였다. 기존 DLL `D:/workspace-github/image-processing/build/e2e-native-dlls`, 기존 보정 `D:/workspace-github/xpe-data/calib-real-v2`, width/height3072와 기존 interactive `--automation-raw` 진입점으로 저장소 손목 원본을 열었다. 프로세스 전용 `XPE_GUI_DATA_DIR`는 자체 bin 아래 `gui-card-check`였다.
- 원본 경로는 `gui/ImageProcTest/fixtures/gui-s0/raw/wrist_lat_3072x3072.raw`. 새 RAW/보정/DLL을 생성하지 않았다. DLL 경로는 256에서 STALE provenance6b8477ef로 관측한 기존 폴더다. 이번 화면의 실제 runtime은 Native/common0.1.0/display1.0.0으로 표시됐다. 최신 DLL·임상/영상 품질 수용으로 확장하지 않는다.
- `wrist-metrics.jpg`: 실제 9개 행의 레인별 status/ms, automatic VOI C2580.347/W1198.496, 양쪽 보정 3 stages, EI219.82/DI−0.56 및 measured/not a pass criterion 문구를 화면에서 확인했다. A PRE297.411ms/B294.378ms, A basic263.617ms/B281.47ms 등 서로 다른 실측값이 표시됐다. 이것은 이 실행 관측이며 성능 benchmark가 아니다.
- Offset/Gain/Defect를 각각 Off로 바꾸되 Preprocess 체크는 켠 채 Run Preprocessing을 실행했다. `wrist-off-banner.jpg`: render#2/PRE skipped, C3659.338/W459.577, `NOT CORRECTED - preprocessing is on but was skipped (no selected calibration). Load calibration data and apply again.` 전체 문장이 HUD 아래에 보이며 toolbar/HUD에 가려지지 않는 것을 직접 확인했다. 정량적 rectangle oracle는 위 Native UIA 시험이며 부모 관측은 별도 시각 확인이다.
- `wrist-footer.jpg`: 기존 View → Logs Panel 토글을 켜 Log 탭을 표시한 뒤 캡처했다. 세 칸은 각각 `preprocess skipped ... auto C3659.338/W459.577`, `preprocess0.053ms ... enhance_basic298.469ms ...`, `WARN: Preprocessing is on but was skipped...`로 서로 달랐다. 긴 문장은 기존 Log와 UIA 상세값/tooltip에 보존되고 화면의 한 줄은 길면 말줄임한다. Log는 기존 tab+toggle 조건을 유지하므로 탭 클릭만으로는 보이지 않았다; 토글을 켠 뒤 실제 목록과 문장을 확인했다.
- 첫 비활성 창 snapshot의 이미지가 다른 창을 보여 해당 이미지는 증거로 저장하지 않았다. 정확한 EXE의 반환된 창을 활성화해 GUI임을 재확인한 뒤 위 세 장만 저장했다. 다른 앱에 입력하지 않았다.
- 검증용 GUI는 종료했으며 별도 runtime 폴더만 정리한다. 원본 RAW·보정 지도·DLL·사용자 사용 중 EXE/설정에는 쓰기를 하지 않았다.

## 부모 Log 최종 보완 재검증 — 2026-11-04 08:35~08:37 KST

- 앞 08:24 실행의 보존 UIA tree를 추가 대조하여 실제 Log item/TextBlock에 `uncalibrated EI (보정 안 된 EI)`가 남은 것을 확인했다. 이 때문에 최종 완료를 미루고 표시 converter만 보완했다. 원래 3장과 RED 로그는 덮어쓰지 않았다. 08:33 보완의 WPF/관련 Copy UIA 결과와 범위는 위 기록을 따른다.
- source 기준은 HEAD84c2551f + 최종 35개 source 변경이다. 마지막 Log 보완 빌드로 자체 PID1656을 새로 시작했다. 원본·DLL·보정·실행 인수·별도 data root는 앞 부모 실행과 같고 기존 data root를 삭제한 상태에서 시작했다.
- 최종 수용 캡처는 `wrist-metrics-final.jpg`, `wrist-off-banner-final.jpg`, `wrist-footer-final.jpg`다. 같은 최종 빌드에서 Metrics9행/실제 양lane3 stages/EI219.82·DI−0.56 비판정 표시, 세 보정 Off+PRE on 실행 뒤 빨간 전체문장/HUD 비중첩, footer3역할/Log를 다시 확인했다. 실제 시간은 이전 실행과 다른 새 관측값이며 성능 비교로 쓰지 않는다.
- 실제 Native Log ListBoxItem과 TextBlock의 영어 `uncalibrated EI (uncalibrated measurement)` 표시를 확인했다. `sky.get_window_state(... include_text=true)`의 현재 전체 tree에서 대상 `보정 안 된 EI` 검색 결과는 **0건**이었다. 긍정 문장·방법·출력은 `native-log-display-final.txt`에 보존했다. 원문 Logs 보존은 앞 실제 WPF oracle이며 부모가 원문 로그 저장이나 실제 EI clipboard까지 새로 측정했다고 주장하지 않는다.
- PID28944/1656의 검증용 창 종료를 확인했다. 각 실행 후 해석된 자체 bin 하위 경로를 대조해 `gui-card-check`만 삭제했으며 두 번 모두 `gui-card-check exists=False`였다. 외부 원본/맵/DLL/기존 사용자 EXE 설정은 삭제하지 않았다.

## 08:33 KST 잔여 Log 표시 보완 인계

부모가 실제 손목 로그에서 관측한 `uncalibrated EI (보정 안 된 EI)`는 위 최초 영어화의 잔여 항목이다. 부모의 기존 화면·로그 증거는 보존했다. 이번 추가 변경은 `Views/AnalysisPanel.xaml`, `Views/AnalysisPanel.xaml.cs`, `SelfCheck/LifetimeScenarios.cs` 세 파일뿐이다. 전체 카드 소스 수는 최초 34개에 기존 View 코드 1개가 추가되어 **35개**다.

기존 View 코드에 대상 EI 문구만 치환하는 `EnglishLogDisplayConverter`를 넣고, Log TextBlock의 보이는 Text와 ListBoxItem의 `AutomationProperties.Name`에 같은 변환을 적용했다. `Logs`/stage Reason/SelectedLog 원문·Copy 명령·로그 식별자·저장 계약은 변경하지 않았다. Native/Metrics/배너 source는 이번 추가 보완에서 변경하지 않았다.

| 단계 | 실제 결과 |
|---|---|
| RED, `red-log-display.txt` | 기존 `--display-contract`에서 실제 WPF 로그 TextBlock과 실제 ListBoxItem peer의 영어 이름 단언 2개 실패. 같은 실행의 원문 Logs/SelectedLog/Copy 입력 보존 단언은 통과 |
| GREEN, `green-log-display.txt` | 같은 display 3개 시나리오 모두 통과. 실제 배치한 TextBlock의 영어 전체 문자열·양의 크기, accessible Name의 같은 문자열, 원문 Logs/선택 항목/SelectedLog 및 Copy command 입력을 대조 |
| 기존 Log/Copy UIA, `green-log-uia.txt` | `AlertVisibilityScenarios` 선택 2/2 통과, skip 0. 실제 Log item 이름의 alert/severity/code·일반 로그 구분, 선택 전후 Copy enable, 실제 Copy 버튼과 clipboard 결과의 기존 원문 marker/code 보존 확인 |

이번 WPF EI 입력은 원문 `[08:24:30.498] Chain preprocess: Applied — uncalibrated EI (보정 안 된 EI)=219.82; DI=-0.56`이며, 화면/accessible Name만 `uncalibrated EI (uncalibrated measurement)`로 바뀐다. 해당 EI 문자열의 clipboard 쓰기를 새 WPF 시험에서 수행하지는 않았다. Copy의 입력인 SelectedLog가 원문임을 단언했고, 기존 UIA는 자체 SETTINGS_UNREADABLE 로그로 실제 복사를 검증했다.

실행 명령은 기존 env.cmd 및 설치된 dotnet으로 다음 범위에 한정했다.

```text
dotnet build gui/ImageProcTest.SelfCheck/ImageProcTest.SelfCheck.csproj -c Debug --no-restore --nologo -v:q
dotnet gui/ImageProcTest.SelfCheck/bin/Debug/net8.0-windows/ImageProcTest.SelfCheck.dll --display-contract
XPE_E2E_BACKEND=Mock; DOTNET_CLI_UI_LANGUAGE=en
dotnet test clients/ImageProcTest.E2ETests/ImageProcTest.E2ETests.csproj -c Debug --no-build --filter FullyQualifiedName~AlertVisibilityScenarios --logger console;verbosity=detailed
```

빌드 Exit 0, 경고 23개·오류 0개(`build-log-display.txt`); display Exit 0; UIA Exit 0. 최종 확인에서 SelfCheck 자체 폴더 없음, 기존 Log 시험의 `xpe-c125-*` 자체 폴더 없음, ImageProcTest 프로세스 0개였다. 관련 파일 diff-check Exit 0(기존 CRLF 안내 외 오류 없음). 기준 HEAD는 `84c2551f9e02c85ffcf447621dfd9a31516852c8` 그대로이며 커밋하지 않았다.

이 추가 인계 뒤 구현 에이전트는 다시 모든 쓰기를 중단했다. 최종 3072 손목의 Log UIA 영어 표시·대상 한국어 0건 및 새 화면은 위 부모 08:35~08:37 절에서 실제 확인했으며 아래 최종 자료에 연결한다. 기존 Native/geometry 시험을 원인 없이 반복하지 않았다.

## 최종 자료·독립 읽기 검토 회수 — 2026-11-04 08:38 KST

- 최종 빌드의 수용 화면: [정상 Metrics](wrist-metrics-final.jpg), [보정 Off 배너](wrist-off-banner-final.jpg), [영어 Log와 세 칸 footer](wrist-footer-final.jpg). 마지막 두 실행의 PID28944/1656 종료 및 각 자체 runtime 폴더 삭제 확인은 위 부모 실행 절에 기록했다.
- [최종 Native Log UIA 관측 원문](native-log-display-final.txt): 영어 긍정 문장과 **현재 캡처 tree에서** 대상 한국어 0건을 함께 확인했다. 전역 로그 전체에서 한국어가 사라졌다는 주장이나 원문 Logs를 번역해 저장했다는 주장이 아니다.
- xpe-reviewer가 동결 source/마지막 3파일 보완/RED·GREEN oracle/최종 3JPG와 위 txt를 읽어 신규 코드 차단 없음·GUI-C-258 기술 범위 수용으로 회신했다. Reviewer는 제품 실행·시험 재실행·프로세스 조사를 하지 않았으며, 종료·정리는 부모의 실제 관측에 귀속한다.
- EI 문자열 자체의 실제 clipboard 쓰기, 모든 reader/Operator/저장소 suite, 전체 CI, 커버리지/LSP, 최신 DLL 및 임상·영상 품질은 위 미확인 범위를 유지한다. 기존 로그·첫 3장·실패 근거를 보존했다. Tuner 보류와 push 보류는 유지한다.
- 커밋 전 실제 검사: source 35개 대상 `git diff --cached --check -- gui/ImageProcTest gui/ImageProcTest.SelfCheck clients/ImageProcTest.E2ETests`는 Exit0/출력 없음. 원문 로그까지 포함한 `git diff --cached --check`는 Exit2이며 보고서 폴더의 원문 txt 10개에서 줄 끝 공백 22건을 관측했다(`nonReportPaths=[]`). 원문을 바꾸거나 검사를 끄지 않았으며 전체 공백 검사 통과로 주장하지 않는다. 기존 정상 git 훅으로 커밋하고, 훅이 막으면 그 오류를 보고한다.
