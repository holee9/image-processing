GUI-C-274 구현·로컬 검증 결과 제출 — 전체 Mock E2E 2실패, 리더/Reviewer 검토 및 main CI 판정 대기. Refs #282.

## Claim
O: 메인 창과 가져온 2G/PRE 비교 창의 공통 표시 정의 및 가독성을 정리한다.
KR: 공통 정의 1개, 카드의 직접 지정 9곳 교체, 대비·초점·색 외 의미·잘림 점검, 무채색 영상 배경, 두 창 캡처, 영향 시험과 화면 E2E 전체 실행.
UI 언어는 기존 사용자 결정인 영어로 통일했다. 한국어 도움말·설정 키·reason code·파일명·AutomationId는 변경하지 않았다. 스킬의 스타일/색/글꼴 조합은 적용하지 않았으며 새 폰트를 설치하지 않았다.

| 제품 파일 | 변경·보존 범위 |
|---|---|
| gui/ImageProcTest/Resources/Presentation.xaml (신규) | 공통 색·SystemFonts.MessageFontFamily/MessageFontSize·기존 간격·초점 정의를 한 물리 파일에 둔다. 이름 있는 키만 사용하며 전역 implicit control style은 없다. |
| gui/ImageProcTest/App.xaml | 위 dictionary를 앱에 연결한다. |
| gui/ImageProcTest/MainWindow.xaml | 해당 창에만 글꼴/초점 style 적용, Mock/AI 안내 정의 참조, 상태줄 3칸 ellipsis를 Wrap으로 변경. 역할·바인딩은 보존. |
| gui/ImageProcTest/Views/ComparisonImportWindow.cs | 같은 정의 사용. 흰색/주황 안내 대비 개선, warning와 포화 설명 영어 통일. 원래 간격·검정 배경·Lime/Yellow 표식·12px 기준 글꼴 및 차이 픽셀 계산은 보존. |
| gui/ImageProcTest/Views/TopBar.xaml | Main 전용 호스트를 확인한 뒤 metadata 2곳·활성 아이콘 3곳만 기존 보조 글자색 #8A909C를 공통 참조하도록 변경. 배치 불변. |
| gui/ImageProcTest/Views/AnalysisPanel.xaml | Main 전용 Metrics의 METRIC/Delta 헤더 2곳만 같은 공통 글자색 참조. 값·행 생성 로직은 보존. |
| gui/ImageProcTest/Controls/ImageComparisonViewport.cs | Background DP를 표준 AddOwner/AffectsRender로 추가. frozen 기본값 RGB(8,13,23)은 유지하고 렌더가 선택한 배경을 읽는다. |
| gui/ImageProcTest/Views/ViewportShell.xaml | Main 전용 3개 뷰포트만 UiImageBackground(Black)를 선택. 별도 Detached 창의 명시되지 않은 기본색은 기존 값 유지. |

시험/프로젝트 변경은 ImageProcTest.E2ETests.csproj의 동일 dictionary Page 연결, 신규 Rendering/SharedPresentationTests.cs, ImportedMarkerTests.cs의 영어 warning 기대 갱신이다. 제품 8파일·시험/프로젝트 3파일이며 처리 엔진·다른 창·docs/design 리디자인·AutomationId는 수정하지 않았다.

#282의 Metrics 9행과 배너/상태줄 역할 분리는 이전 수용 기록과 현재 코드에 이미 있었다. 이를 새로 구현했다고 주장하지 않는다. MainWindowViewModel.MetricRows의 기존 단계4/VOI3/보정1/EI1과 render 통지, 실제 Mock 화면의 9행을 확인했다. 기존 배너 배치도 보존했다.
이슈 근거: https://github.com/holee9/image-processing/issues/282 및 issue-282.json/issue-282-comments.json. 이전 수용 기록과 이번 관측은 구분했다.

## Evidence
모든 아래 명령은 전용 카드 트리에서 기존 env.cmd 래퍼와 C:/Progra~1/dotnet/dotnet.exe를 사용했다. 정확한 환경과 실행 명령은 각 run-*.cmd, 원문 stdout/stderr는 같은 폴더에 보존했다.

| 관측 | 실제 명령·원문 | 결과 |
|---|---|---|
| 최초 표시 RED | dotnet test clients/ImageProcTest.E2ETests -c Debug --filter FullyQualifiedName~C274 --logger "console;verbosity=normal" / red.txt | 3F. 실제 white/DarkOrange 대비 2.3325:1<4.5, 공통 키·초점 정의 부재. |
| 메타데이터/헤더 RED | red-metadata.txt, red-metrics-headers.txt와 각 run 파일 | 공통 키 부재 2F, 실제 Metrics source 색 조합 2.800679:1<4.5로 1F. |
| 토큰·가독성 영향 | green-targeted.txt → green-targeted-final-v2.txt | 초기26/26P, 확장 후29=27P/2F(UIA 두 검색). 신규11은 모두P. |
| 중립 배경 RED→GREEN | run-neutral-red-v2.cmd → run-neutral-green.cmd / red-neutral-canvas-v2.txt → green-neutral-canvas.txt | 5F→5/5P. 아래 실제 픽셀 oracle 포함. |
| 최종 Rendering 영향 | dotnet test clients/ImageProcTest.E2ETests -c Debug --no-restore --filter FullyQualifiedName~ImageProcTest.E2ETests.Rendering --logger "console;verbosity=detailed" / neutral-rendering-impact.txt | 58=56P/2F. 신규 C274 16개 모두P. |
| 최종 앱 빌드 | dotnet build gui/ImageProcTest/ImageProcTest.csproj -c Debug --no-restore --verbosity minimal / build-app-neutral.txt | 오류0/경고22, exit0. 경고 증감 baseline은 미측정. |
| 중립 배경 전 전체 | run-full-mock.cmd / full-mock.txt | 381=282P/1F/98S, exit1. C270 실패, 당시 C271 통과. |
| 최종 전체 | run-full-neutral-mock.cmd / full-neutral-mock.txt | 386=286P/2F/98S, exit1. 최종 source의 C274 16/16P. |

최종 전체 명령:
`dotnet test clients/ImageProcTest.E2ETests -c Debug --no-restore --logger "console;verbosity=normal"`
환경은 XPE_E2E_BACKEND=Mock, XPE_NATIVE_DIR 미설정, XPE_NATIVE_DIR_EXCLUSIVE=1, XPE_E2E_ALLOW_GLOBAL_INPUT 미설정, 별도 mock-data-neutral-full이다. 시작 전 GUI/E2E 출력의 xpe_*.dll이 각각0인 것을 확인했다.
최종 집계 원문: `총 테스트 수: 386`, `통과: 286`, `실패: 2`, `건너뜀: 98`. source는 전체 실행 중과 이후 동결했다.
실패는 C270_ImportedOutputs_DisplayBothAndToggleWithoutComparabilityBlock의 ComparisonImageA 검색(:230/:232), C271_RegionsAreListed_AndSelectingOneZoomsAndCentersBothViews의 ComparisonMarkerList 검색 null(:64)이다. 이번 변경과의 인과·환경 문제·회귀 유무는 미확정이다. 대기·재시도·단언 완화를 추가하지 않았다. 영어 기대는 숫자25/20과 전체 warning 의미를 그대로 유지한 번역이다.

중립 배경 재현은 검정 설정 후 Expected BGRA `[0,0,0,255]`, Actual `[23,13,8,255]`였다. 수정 후 검정 코너, 원본 중심 화소17와 source bytes 불변, unstyled 뷰포트의 기존 navy/frozen 기본값, Main3개 배경 참조를 확인했다. 이 추가 동작 때문에 전체 E2E를 다시 실행했다. 앞선 실패·컴파일/준비 오류·중간 캡처도 삭제하지 않았다. 마지막 두 줄 들여쓰기만 정리한 근거와 LF/BOM 보존은 format-only-diff-final.json, 인코딩은 neutral-source-encodings.json에 있다. 그 정리는 최종 중립 배경 구현과 최종 전체 실행 전에 끝났다.

### 납품 전 4항목 점검
| 항목 | 확인한 근거 | 관측 범위·한계 |
|---|---|---|
| 글자 대비 4.5:1 이상 | WPF 실제 리소스/속성 시험과 별도 sRGB 계산 일치. 아래 표 참조. | 변경한 안내·정보 색 조합의 측정이며 화면 전체 WCAG 인증이 아니다. |
| 키보드 초점 표시 | WPF FocusVisualStyle/IsKeyboardFocused border, 레이아웃 크기 불변 단언. 실제 Main LoadRawImageButton·Compare ComparisonOpenButton UIA HasKeyboardFocus=true 및 파란 테두리 캡처. | 전역 Tab/키 입력 없이 해당 앱 요소 SetFocus만 사용. 전체 키보드 순회는 미실행. |
| 색만으로 의미 전달하지 않음 | MOCK/경고의 명시 문장, A/B 글자, marker 번호·목록, 차이 부호·범위·비유한 범례 유지. | 측정 영상의 색 매핑 자체는 변경하지 않았다. |
| 글자 잘림 없음 | Main1280×720에서 footer3칸 전체 문장 Wrap·본문 부모 bounds 포함, Metrics9행 관측. Compare1180×1200에서 안내/경고/범례/초점 버튼 확인. | 스크롤 영역은 스크롤을 허용한다. Native NOT CORRECTED 배너/HUD 기하 및 다른 DPI·극단 장문 상태는 미관측. |

| 색 조합 | 대비 |
|---|---:|
| 정상 chrome black/white | 21.000000:1 |
| Mock 흰 글자/#B91C1C | 6.470045:1 |
| provenance 검정 글자/DarkOrange | 9.003233:1 |
| AI #9A4D00/#1AB45309를 white 위 합성 | 5.300784:1 |
| warning #9A4D00/white | 6.112548:1 |
| Main metadata·Metrics 헤더 #8A909C/#14171E | 5.593284:1 |
| run-set badge 합성 배경 위 metadata | 4.910622:1 |
| focus #005FCC/white | 5.984935:1 |
| 기존 Lime/Black 표식 글자 | 15.304000:1 |
계산은 sRGB 상대 휘도, 8bit alpha 합성 후 대비비를 사용했다. contrast-measurements.json에 입력 RGB·방법·출력값을 보존했다. 최소 4.910622는 위 검증한 텍스트 조합의 최소이며 모든 가능 상태의 최솟값이 아니다.

실제 최종 앱 캡처:
- [메인](D:/workspace-github/xpe-gui/.moai/reports/lane-gui/GUI-C-274/main-neutral.png): 1266×713, 610772B, SHA256 9ea83838cde9294d1c4d4fdc971cce01b9327b0ba31f76e6fe4b9e09d341d695.
- [비교](D:/workspace-github/xpe-gui/.moai/reports/lane-gui/GUI-C-274/comparison-neutral.png): 1166×1193, 1346021B, SHA256 bd7360996a081ece9bd24f13824691d0f7872afd1ba2d14f24c7ac9310ee45a6.
최종 Mock 앱으로 기존 synthetic_1024x1024.raw를 열었고, 비교 창은 기존 compare-s5_csi_post-001.e2e.json을 가져왔다. Main 영상 바탕은 검정이며 actual UIA Metrics 라벨9개·상태줄3칸 원문을 확인했다. Compare는 기존 범위±2.10347/포화588094/1966080/M1377을 유지했다. comparable=false/계약 미검토 안내도 유지했다. 영상 품질·비교 적합성 통과를 뜻하지 않는다.
UIA 소유 PID/EXE 검증, Invoke/Value/SetFocus/Resize 패턴과 Sky 읽기 캡처만 사용했다. 전역 입력·XPE 처리 엔진은 실행하지 않았다. 캡처 JPEG를 PNG로만 인코딩하고 디코딩 RGB 동일성을 확인했다. 영상 커밋 금지에 따라 모든 이미지는 로컬에만 보존한다.
캡처 PID29640 정상 종료(closed=true,killed=false), Sky의 해당 작업 트리 잔류 창 조회0. 전체 제품 프로세스 목록이 없다는 주장으로 확대하지 않는다.

## Baseline-attribution
전용 트리 C:/Users/drake/.moai/worktrees/projects/image-processing/GUI-C-274, 브랜치 WT-gui-shared-presentation. origin/main을 fetch한 뒤 해당 트리만 fast-forward하여 기준 HEAD1e5037353430cecef97d4d86b23fe96a87cc21de에서 시작했다. primary 브랜치 상태는 변경하지 않았다.
최종 source11파일 변경 상태에서 최종 빌드·캡처·전체 시험을 수행했다. 최종 GUI DLL SHA256 be54453761028092eb547c3682e74b7f936de9fdf1dc5f0673adac3bc66c7e87은 캡처·전체 시작·종료 뒤 측정값이 같다. apphost EXE SHA256 f74cd9e7ed0eaf0339ef274db509968e01f9b9049804fab7a9877b2dcb8433d7, ProductVersion1.0.0+1e50373534. EXE 해시만으로 변경 코드를 식별하지 않는다.
최종 화면 원문은 capture-neutral-*-result.jsonl, 이미지/바이너리 식별은 capture-neutral-manifest.json, 소스 보존 점검은 scope-preservation-final.json에 있다. 대상5개 XAML/C#의 AutomationId literal이 동일하고 DifferenceBitmap의 범례 외 픽셀 계산 코드가 동일함을 baseline과 기계 대조했다. 새 Main 배경은 단색 바탕만 바꾸며 원본 화소 보존을 별도 단언했다.

## Gaps
전체 E2E 0실패는 미충족이다. UIA 두 실패의 원인과 main CI의 최종 판정은 아직 없다. SKIP98개/38사유 원문은 neutral-skip-reasons.json이다. 전역 입력15, legacy EXE 미빌드9, SelfCheck 미빌드2, GUI E2E 실행기 미빌드1 및 Native/데이터/Mock 제약 등이 포함된다. 발견된386case를 모든 실데이터 시나리오의 분모로 확대하지 않는다. 일부 실데이터 walkthrough는 미발견·미측정 sentinel이다.
Mock 정책상 Native 전처리 경고가 숨겨져 실제 NOT CORRECTED 배너/HUD 비겹침은 미측정이다. 기존 StateDisplayScenarios.D258_NativeSkippedBanner_IsFullyInsideViewport_AndClearOfHudAndToolbar도 Mock에서 SKIP했다. 메인 최소 크기의 Native geometry는 main CI에서 확인해야 한다.
별도 Native/SelfCheck/legacy 앱 실행, 전역 키보드 순회, 다른 DPI/테마, 커버리지/LSP 및 경고22개의 증감 baseline은 미측정이다. 다른 창 전체를 실행해 검사했다고 주장하지 않는다.

## Residual-risk
이 변경은 공통 표시 정의·가독성·메인 중립 배경의 검토 가능한 구현이다. UIA 두 실패를 환경 탓이나 비회귀로 단정하지 않으며, 해당 검토와 main CI 화면 E2E 판정 전 카드 종료로 보지 않는다. 다른 창의 기본 배경은 보존 시험으로 확인했고 처리/저장/원본 픽셀 계약·차이 측정 색은 변경하지 않았다. 로컬 커밋만 제출하며 병합·push·최종 통합 판정은 리더가 담당한다.
