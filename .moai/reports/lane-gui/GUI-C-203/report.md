# GUI-C-203 — #225 남은 메뉴 현황 재집계 (코드 변경 없음)

기준: dev/gui = main `52a2b2a5`. 증거는 이 폴더: `axis1_axis2_app_counts.txt`, `axis3_xaml_parse.txt`, `uia_walk_mock.txt`·`uia_walk_native_with_ai_dll.txt`·`uia_walk_native_without_ai_dll.txt`, `menu_table.md`(항목별 한 줄), 문구 초안 `menu001_correction_draft.txt`·`issue225_correction_draft.txt`.

## 1. 결론 한 줄

**남은 정적 비활성 메뉴는 3개**(Open DICOM · QA Constancy · GSDF Calibrate)다. GUI-C-169 시점의 4개에서 **Run Deterministic Baseline 이 빠졌다**(명령으로 배선돼 Native 에서 켜진다). 조건부로 꺼지는 항목이 따로 3개 있다(Run Preprocessing·Run Deterministic Baseline·Run AI Bone Suppression — 백엔드/모듈에 따라).

## 2. 세 축 대조 (대조군)

| 축 | 무엇을 센 것 | Mock | Native(xpe_ai.dll 있음) | Native(xpe_ai.dll 없음) |
|---|---|---|---|---|
| ① 앱의 명령 이름 목록 | `DisabledFutureCommandCount`(앱의 손으로 쓴 13개 이름 중 `!IsEnabled`) — 앱을 실제로 자동화 실행해 보고서에서 읽음 | **3** | **3** | **3** |
| ② 실행 중 메뉴 트리 순회 | `UnimplementedMenuLeafCount`(앱 자신의 순회: 잎·ItemsSource 없음·`!IsEnabled`·`Command == null`) — 같은 보고서 | **3** | **3** | **3** |
| ③ XAML 파싱 | `MainWindow.xaml` 을 XML 파서로 읽어 `IsEnabled="False"` 인 MenuItem | **3** (OpenDicom, QaConstancy, GsdfCalibrate) | 같음 | 같음 |

세 축이 같은 답이다(시험 `AutomationReportBackendTests.A11` 이 같은 동등성을 CI 에서 단언한다). 이 수는 "**명령이 없어서** 꺼진 항목"이다.

네 번째 읽기(UIA 로 실행 중인 창을 열어 보며 센 **그 순간 꺼진 항목**)는 다른 질문에 답한다: Mock 6개(위 3 + Baseline·AI·Preprocessing), Native+xpe_ai.dll 3개, Native 에서 xpe_ai.dll 이 없으면 4개(위 3 + AI). 이 수는 위 3 과 같아야 하는 것이 아니다 — 차이가 정확히 조건부 3개이고, UIA 로 본 켜짐/꺼짐이 구성마다 아래 표와 같다. 항목 수 대조: UIA 가 여섯 메뉴 아래에서 찾은 58개 = XAML 의 최상위 아래 57개 + 동적 항목 1개(`RecentRawFileMenuItem`, 최근 파일 기록으로 만들어짐). 최상위 메뉴 6개는 컨테이너다. XAML 전체 63개 MenuItem(잎 55, 컨테이너 8 = 최상위 6 + Backend Mode + Compare Mode).

## 3. 항목별 현황 (`menu_table.md` 에 55개 잎 전부, 요약)

**켜짐/꺼짐이 구성에 따라 바뀌는 항목** (UIA 실측, Mock / Native+xpe_ai.dll / Native−xpe_ai.dll):

| 항목 | 켜지는 조건 | 실측 |
|---|---|---|
| Open Recent | 최근 파일 기록이 비어 있지 않을 때(`HasRecentRawFiles`) | 이 기계의 기록이 있어 세 구성 모두 켜짐 — 기록이 없으면 꺼짐(코드로만 확인) |
| Run Preprocessing | 백엔드가 전처리를 지원할 때(`CanRunPreprocessing` = Native) | off / on / on |
| Run Deterministic Baseline | 백엔드가 기준선을 지원할 때(`CanRunDeterministicBaseline` = Native) | off / on / on |
| Run AI Bone Suppression | AI 세션이 있는 백엔드·초기화됨·전환 중 아님·xpe_ai.dll 이 보임(`AiBoneSuppressionAvailability`) | off / on / off |

**배선 여부**: 55개 잎 중 51개가 명령 또는 Click 처리기·바인딩에 배선돼 있고, 배선이 없는 것은 위 정적 비활성 3개와 View 의 패널 토글 3개(체크 가능 항목: 명령이 아니라 설정 바인딩)다.

**행동을 단언하는 시험이 없는 항목** (id·명령·처리기 이름을 시험 소스에서 grep 한 뒤 행동까지 확인):

| 항목 | 상태 |
|---|---|
| Exit | 시험 없음 (앱을 닫는 처리기) |
| Native DLL Diagnostics | **시험 없음** — 명령 `ShowNativeDiagnostics` 는 상태줄과 로그에 DLL 감지 상태를 쓰는데 어떤 시험도 부르지 않는다 |
| Zoom In · Zoom Out | **시험 없음** — `ZoomInCommand`/`ZoomOutCommand` 를 가리키는 시험이 없다(Zoom Fit·100% 는 자동화 실행이 눌러 보지만 결과를 단언하지 않는다) |
| Export Evidence Snapshot | 자기 id 로는 시험 없음 — `ExportAutomationReportCommand` 와 **같은 명령**이다(File 쪽 항목은 시험됨) |
| About / Build Info | 시험 없음 |
| Quick Start · Scope and Limitations · Current Workflow Help | 쉘 E2E 가 컨트롤을 **가져오기만** 하고(이름 대조) 페이지가 열리는지는 아무도 확인하지 않는다. Current Workflow 는 자동화 보고서가 `IsEnabled` 만 읽는다 |

그 밖의 항목은 동작을 단언하는 시험(A01~A21, W·S·R·C·B·E·L 시나리오, 기능 시험)이 있다. 쉘 E2E(`ImageProcTest.E2E/Program.cs`)가 많은 항목에 대해 하는 일은 `Command is not null` 같은 **존재 확인**이며 행동이 아니다 — 이 보고서에서 "시험 있음"이라고 센 것은 그것이 아니라 행동을 읽는 시험이다.

## 4. 비활성으로 남은 3개 — 막는 것

| 항목 | 막는 것 | 분류 | gui 작업 범위 한 줄 |
|---|---|---|---|
| **Open DICOM** | `xpe_dicom` 의 읽기 수출(`xpe_dicom_open`·`xpe_dicom_read_image`·`xpe_dicom_get_metadata`)이 이미 있고 앱은 쓰기·검증·되읽기를 CI 에서 확인한다. 남은 것은 gui 쪽 읽기 경로. **사용자 지시로 후순위**(#225 행 2) | gui 작업만 남음(+ 후순위 결정) | 파일 대화상자 → `xpe_dicom_open/read_image/get_metadata` → `LoadedImageFrame`(비트 깊이·메타데이터 사상) + 시험(읽기 왕복은 BaselineDicomNativeTests 가 이미 가진 모듈 호출 재사용) |
| **QA Constancy** | 기준 영상·판정 기준이 저장소에 없다(`flat`/`uniform` 이름의 영상·자료 0건 — 이름만 본 것, 요구 문서에는 "QA constancy" 문구가 있으나 판정 기준 정의는 확인하지 못함) | 요구 미정(사람 필요) | 요구가 정해지면: 기준 영상 선택 → 지표 계산 → 판정·증거 파일(범위는 요구에 달림) |
| **GSDF Calibrate** | 모듈은 `xpe_gsdf_calibrate`(실측 광도 → Presentation LUT)를 이미 제공한다. 없는 것은 **실측 광도 입력**(디스플레이 측정 절차·값) | 사람 필요(측정) | 측정값 입력/가져오기 → `xpe_gsdf_calibrate` → LUT 적용·저장 + 시험(합성 곡선으로 가능) |

세 항목의 툴팁은 이유를 말한다. 단 Open DICOM 의 툴팁("Real DICOM read/write is owned by xpe_dicom.dll in Phase 1b")은 쓰기가 끝난 지금 **절반만 맞다**(읽기 경로는 gui 작업으로 남음).

## 5. 문서가 앱과 틀린 곳 (초안은 `.txt`)

**MENU-001** (`docs/project/XPE-GUI-MENU-001_Menu_and_Command_Strategy.md`) — 리더가 이미 반영한 §8 의 Baseline·AI 행은 맞다. 나머지:

1. §3 표: Pipeline 이 "future disabled shell" — 활성이다.
2. §4.1: "Export Evidence Bundle (future)" — 구현됨. "Open DICOM 은 `xpe_dicom.dll` 통합 전까지 비활성" 규칙 — 통합은 끝났는데 항목은 사용자 후순위 지시로 비활성(이유를 고쳐 적어야 함).
3. §4.2: "Native backend commands remain disabled until RealXpeBackend is implemented" — 구현됨. "Open Runtime Logs" — 실제 머리말은 **Export Runtime Logs**(AutomationId `OpenRuntimeLogsMenuItem`).
4. §4.3: 문서에 있으나 앱에 **없는** 항목 5개(Show Runtime Panel·Show Raw Settings·Show Calibration Evaluation·Show Alerts·Pan). 앱에 있으나 문서에 없는 것: Calibration Paths Panel·Display Settings Panel 토글(§9.2 에는 있음), Clear Logs·Clear Alerts, Reset Comparison View.
5. §4.5/§9.1: `Calibration Evaluation` — 메뉴에 없음(대신 Calibration Settings). `Dll Diagnostics` 는 §9.1 에서 Tools 인데 앱은 **Backend** 메뉴("Native DLL Diagnostics"). `Export Evidence Snapshot` 이 앱에 있고 문서에 없음(File 의 Export Automation Report 와 같은 명령). §9.1 E2E 열의 "—" 중 시험이 생긴 것: Run Self-Check(A04/A05/A15)·Run GUI E2E(A06)·Benchmark Runner(A14)·Open Evidence Folder(A09).
6. §8·§9.1 활성 조건: 앱에서 실제 규칙 — `Run Preprocessing` 은 **백엔드가 지원할 때**(영상·교정 경로는 눌렀을 때 상태줄이 답함), `Apply Display Pipeline`·`Stop Processing`·`Stage Timing`·`Open Pipeline Diagnostics`·`Run Self-Check`·`Run GUI E2E`·`Benchmark Runner`·`Export Automation Report` 는 **항상 켜져 있고** 조건은 눌렀을 때 답으로 처리된다(문서의 "IsProcessing == true", "Last pipeline execution completed", "Backend initialized", "At least one pipeline ready", "Any E2E run completed"는 앱의 규칙이 아님). 문서의 "[HARD] 비활성은 tooltip 으로 사유" 는 꺼진 항목 6개가 모두 지킨다(툴팁 확인: Open DICOM·QA·GSDF·Preprocessing 은 XAML 정적 툴팁, Baseline·AI 는 동적).
7. §8 의 `Apply Enhance (Basic/Advanced)`·`Apply Grid Suppression`·`Apply AI Assist` — 메뉴 항목이 **없다**(enhance_basic 은 Baseline 체인 안에서만, GSVG 는 알고리즘 바에서). 문서가 "계획"임을 적어야 한다.
8. §10 단축키 표: 앱에 **실제로 바인딩된 것은 F5·F6·F7·F8(비교 모드) 4개뿐**(`Window.InputBindings`, 메뉴의 `InputGestureText` 도 그 넷). Ctrl+O·Ctrl+S·F9·F10·Esc·Ctrl+B 체인·F1 등 나머지는 **미구현**인데 표의 "Phase" 열은 S0/1a 로 적혀 있다. §10.2 의 `RoutedCommand` 규칙·중복 감지 pre-commit 훅도 앱에 없다. (F10 은 "Run Full Pipeline" 이라 적혀 있는데 그 항목은 이제 AI 뼈 억제다.)
9. §11 Command Quality Contract: `AutomationId` 예(`XPE_Menu_Pipeline_RunPreprocessing_MenuItem`)와 `Id`/`HeaderKey`(RESX)/`Owner`/`Phase`/`EnabledCondition`/`HelpTarget` 필드 — 앱의 AutomationId 는 `RunPreprocessingMenuItem` 형태이고 위 필드를 가진 명령 정의 구조는 **없다**(RESX 도 없음).
10. 문서 머리말 Version 1.1.0 / 2026-04-18 — 이후 본문 수정(GUI-C-170·184·196·198)이 CHANGE HISTORY 에 없다.

**#225 본문**(2026-10-01 15:52 판):

1. 제목 "비활성 메뉴 15건"·본문 "`IsEnabled="False"` **21건**" — 지금 **3건**.
2. 분류 표(가능 15 / 선행 필요 4 / 사람 필요 2 / DICOM 후순위 1)·"## 남은 것" — 행 9(Baseline)·10(AI)·21(Troubleshooting)은 완료, 가능 15 전부 완료(코드 주석의 행 번호로 확인: 1·3·4·5·6·7·8·11·12·13·14·15·16·17·20 모두 "implemented/wired"). 남은 것은 행 2(Open DICOM)·18·19.
3. "`MainWindow.xaml.cs:318-339` 가 이 항목들을 `!IsEnabled` 로 센다" — 줄 번호가 낡았고, 그 방식은 GUI-C-169 에서 **이름 목록 vs 트리 순회 vs XAML 의 세 축 동등성**으로 바뀌었다(`AutomationReportBackendTests.A11`).
4. "행 10(Full Pipeline, `#130`)" — 항목은 이제 "Run AI Bone Suppression"이며 활성 규칙(MENU-001 §8)을 따른다(GUI-C-184·198).
5. "행 4 가 반례" 단락은 맞다(Native 모드 전환은 명령).

## 미검증·한계

1. "배선" 열의 "클릭이 하는 일"은 명령 이름·처리기·시험 이름을 읽은 것이다. 55개를 한 번씩 눌러 실행으로 확인한 것은 아니다. 실측한 것은 **켜짐/꺼짐(UIA, 세 구성)** 과 **앱이 보고하는 세 수**다.
2. 최근 파일 기록이 비어 있는 상태의 Open Recent(꺼짐)는 실행하지 않고 XAML 바인딩만 읽었다.
3. 이 기계에 doxygen·docfx 가 없어 API Reference·Troubleshooting 항목은 "생성 전" 갈래만 시험이 단언한다(GUI-C-169·181 보고 그대로).
4. QA Constancy 의 "요구 미정"은 **저장소 파일 이름 검색**과 C-152 의 기존 판정에 기댄 것이다(요구 문서를 끝까지 읽지 않음).
5. #225 본문은 `gh issue view` 로 읽은 본문만 봤다. 댓글에 갱신이 있었다면 보지 않았다.
6. 임시로 만든 UIA 순회 시험(`ZzMenuInventoryProbe`)은 증거(`uia_walk_*.txt`)를 얻은 뒤 지웠고 커밋하지 않았다.
