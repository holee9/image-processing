# GUI-C-233j 보고 — 화면 바인딩 전수, 비교 화면 빈 상태, 누수 검사 양방향 (Codex #178, Refs #251)

dev/gui 에서 작업, 233i(`4c701839`) 위. 푸시 안 함.

## 1. 화면 바인딩 전수 표
방법: 모든 XAML 의 `{Binding X}` 164개 이름을 뽑아 `MainWindowViewModel` 의 해당 프로퍼티 getter 가 `Settings` 를 읽는지, `_render` 에서 파생되는지 분류했다(`binding_survey_script.txt`, 출력 `binding_survey_output.txt`). 아래는 "그려진 영상이나 그 상태를 말하는 자리" 와, 분류가 Settings 로 나온 것 전부. 수정 전 분류에서 Settings 를 읽으면서 **적용값 자리**였던 것은 둘이었다 (표의 "고침").

| 바인딩 (XAML 위치) | 소스 프로퍼티 | 그 프로퍼티가 읽는 곳 | 판정 |
|---|---|---|---|
| 창 제목 `[MOCK]` (MainWindow:11) | `WindowTitle` | 현재 런타임(`RuntimeInfo`) | 런타임을 말하는 자리. 233g 이후 백엔드 교체 때 처리 영상이 같이 사라지므로 어긋나지 않는다. 유지 |
| Mock 배너 (MainWindow:59-61) | `IsMockBackend`, `MockBackendWarning` | 현재 런타임 + 요청 모드(불일치 안내) | 런타임/요청을 말하는 자리. 유지 |
| 상태줄 메시지 (MainWindow:518) | `StatusText` | 마지막 메시지 필드 | 렌더 기록이 아니라 메시지. 유지 |
| 상태줄 체인 (MainWindow:521), 주 뷰포트 `ChainStatus`, 분석 패널 (AnalysisPanel:347) | `ChainStatus` | `_render` | 적용값, 기록 파생 ✓ |
| 상태줄 런타임 (MainWindow:525) | `RuntimeVersionSummary` | 현재 런타임 | 런타임. 유지 |
| 주 뷰포트 그림 (ViewportShell:22-23) | `SourceImage`, `ProcessedImage` | 필드(불러오기·커밋·`InvalidateRender` 가 쓴다) | 한 곳(`InvalidateRender`)이 원본으로 되돌림. 주 뷰포트의 RAW 대체 표시는 요청대로 그대로 |
| 주 뷰포트 비교 보기 설정 (ViewportShell:25-30) | `Settings.Comparison*` | Settings | 보기 상태(바꾸면 화면이 즉시 따라감), 렌더 입력이 아님. 유지 |
| 레인 그림 (ViewportShell, 레인 행) | `LaneAImage`, `LaneBImage` | `_render` (233h) | ✓ + 아래 빈 상태 |
| stale 배너 (ViewportShell, DisplaySettingsPanel:49-51, PipelineDiagnosticsPanel:50-52) | `IsPreviewStale`, `PreviewStaleReason` | 렌더 이벤트가 쓰는 필드 + `RefreshParametersStale`(기록의 `Inputs` 와 설정 비교) | ✓ |
| 영상 요약 (ViewportShell:186) | `ActiveImageSummary` | 필드(불러오기·커밋·`InvalidateRender`) | ✓ |
| HUD 창 값 (ViewportShell:211-217, DisplaySettingsPanel:80-100) | `RenderedVoiCenter/Width/Mode` | `_render` | ✓ |
| **HUD 교정 단계 수 (ViewportShell:221)** | `CalibStageCountDisplay` | **`Settings` (수정 전)** → `_render` | **고침**: 기록의 교정 모드 7개로 센다. 기록이 없으면 "—" |
| **레인 알고리즘 표지 (ViewportShell 두 곳)** | `LaneAAlgorithm`/`LaneBAlgorithm` | **`Settings` (수정 전)** → `RenderedLaneAAlgorithm`/`RenderedLaneBAlgorithm` = `_render.Inputs` | **고침** (항목 3) |
| 알고리즘 선택기 (AlgorithmBar:24,43) | `LaneAAlgorithm`/`LaneBAlgorithm` (TwoWay) | Settings | **요청값** 입력(선택기). 유지 |
| Candidate stale 표시 (AlgorithmBar:59, 그리고 Candidate 타일의 새 배지) | `LaneBIsStale` | `_render.Inputs` 와 현재 Settings 의 차이 | ✓ (정의상 두 값의 비교) |
| 진단 패널 "측정 없음" (PipelineDiagnosticsPanel:68) | `HasPipelineDiagnostics` | `_render` | ✓ |
| 분석 패널 지표 행 (AnalysisPanel:138-140 `{Binding LaneA}`/`LaneB`) | — | `ItemsSource` 없는 자리표시 행 | 데이터 없음, 아무것도 안 그린다 |
| `MetadataText` | — | XAML 어디에도 바인딩 없음 | 화면에 안 나오지만 `InvalidateRender` 가 원본 것으로 되돌리도록 같이 고침 |
| 저장 가능 여부 (`CanSaveCorrected`) | — | `_committedCorrected` + 활성 프레임 | `InvalidateRender` 가 후보를 버린다 (233g) |
| 나머지 입력 컨트롤(`Settings.*` TwoWay, `SelectedBodyPart`, `FocusMode`, `AnalysisTab`, `LaneBDenoiseK*`) | | Settings / UI 상태 | 요청값 입력 또는 화면 상태. 유지 |

한계: 위 분류는 getter 본문에 `Settings.` 가 있는지 여부로 가른 것이다. getter 가 다른 메서드를 거쳐 설정을 읽는 경우는 이 방법으로 안 보인다(그런 프로퍼티는 이번 164개에서 찾지 못했지만 전수를 사람이 읽어 확인한 것은 아니다).

## 2. 비교 화면 빈 상태 (높음)
- 사실 정정: `ViewportShell.xaml` 의 주석은 레인 행이 "두 레인이 그려질 때까지 접혀 있다" 고 했지만 코드는 그렇게 한 적이 없다(`LaneRow` 는 항상 보임). 그리고 `ImageComparisonViewport` 는 처리 영상이 null 이면 원본을 대신 그린다 — 주 뷰포트에는 맞고(RAW 가 렌더 전에 보인다) 레인에는 틀리다(레인의 주장은 "이것이 적용된 결과").
- 고침(레인에만 한정): 레인마다 뷰포트를 `LaneAImage`/`LaneBImage` 가 null 이면 `Collapsed` 로 하고, 그 자리에 "No applied image" 텍스트(`LaneAEmptyText`/`LaneBEmptyText`)를 보인다. 주 뷰포트는 건드리지 않았다. `LaneBIsStale` 은 AlgorithmBar 에 이미 바인딩돼 있었고, Candidate 타일에 "Candidate: settings changed - apply again" 배지(`LaneBStaleText`)를 더했다.
- 시험 (UIA, 전경 입력 없음): `TwoLaneWorkbenchScenarios.L07` — 렌더 후 레인이 그려져 있고 자리표시가 안 보임 → Backend > Initialize Backend → 두 레인 모두 뷰포트가 자동화 트리에 없고 "No applied image" 가 보이며, 주 뷰포트(`WorkbenchViewport`)는 그대로, stale 표시에 "backend was replaced" → 다시 적용하면 레인이 돌아오고 자리표시가 사라짐. 통과 (`e2e_l07_pass_raw.txt`, 10 s). 반증: XAML 을 옛 것으로 되돌리면 빨강 — "Lane A still has a drawn viewport after the backend was replaced (it would show the source as if it were a result)", `viewport in tree=True` (`e2e_l07_falsification_old_xaml_raw.txt`).
- 같은 클래스의 L01·L02·L03 도 통과, L04·L06 은 Native 전용이라 건너뜀(`e2e_two_lane_class_raw.txt`: 통과 4, 건너뜀 2).
- 새 프레임 실패와 Lane B 실패 두 경우는 **UIA 가 아니라 SelfCheck(뷰모델)** 로 확인했다: 새 프레임 실패는 시나리오 13(레인 A/B 모두 `-`), Lane B 실패는 시나리오 14(Candidate 가 던지면 주 렌더는 커밋되고 `LaneAImage`·`LaneBImage` 둘 다 null — 기존 동작 그대로 고정). 두 경우에서 XAML 이 실제로 자리표시를 보이는지는 L07(백엔드 교체)로 같은 데이터 트리거를 확인한 것이지 각각 UIA 로 재현한 것은 아니다.

## 3. 레인 알고리즘 표지 (중간)
`RenderedLaneAAlgorithm`/`RenderedLaneBAlgorithm` 을 `CurrentRender.Inputs` 에서 읽고 ViewportShell 의 두 표지가 그것을 바인딩한다. SelfCheck 14: 렌더 뒤 알고리즘 설정을 바꾸고 재적용 안 한 상태에서 표지가 이전 렌더의 것 그대로이고(`tagA`/`tagB`), HUD 교정 단계 수도 그대로(`7/7` → 설정은 5/7 로 바뀐 상태), Candidate 는 stale. 반증: 둘을 `Settings` 로 읽게 하면 14 가 빨강 ("the lane tags followed the settings instead of the render", "the HUD stage count followed the settings: was '7/7 stages', now '5/7 stages'"; `falsification_tags_read_settings.txt`). 이 변경으로 `SettingsProcessingConnectionTests` 의 "읽기 전용 `LaneAAlgorithm` 바인딩이 있다" 는 단언이 틀려졌고, 의도한 변화이므로 "없다 + 표지는 `RenderedLane*` 를 바인딩한다" 로 바꿨다.

## 4. 누수 검사 양방향 (중간)
- `PreprocessModuleLeakTests` 에 시험 둘을 더했다: `TheFileList_EqualsTheFilesThatCallTheModule`(스윕이 덮는 파일 = 가드 시험이 찾는 것과 같은 호출 표지로 찾은 파일, 빠진 것·없어진 것 모두 빨강)과 `TheMethodList_EqualsTheTestMethodsOfThoseFiles`(실제 시험 메서드 ↔ 목록, 양방향, 의도적 제외는 이유와 함께 `IntentionallyNotSwept`, 중복 이름 검사). 목록은 컴파일되는 호출이라 시험 메서드를 지우거나 이름을 바꾸면 **빌드가 먼저 실패**한다(`falsification_d_renamed_listed_test_fails_the_build.txt`).
- 스윕이 개별 시험의 실패를 삼키지 않는다: 스윕 안에서 실패·예외가 난 시험은 이름과 메시지로 `failures` 에 모여 별도 단언으로 실패한다(건너뜀만 무시). 이번 실행에서는 스윕 안 실패 0.
- `BaselineReviewFixTests` 는 거의 모든 시험이 모듈과 무관해서 접두사 필터(`TheRealRunner_`, `WhenPreparingTheMaps`)로 모듈을 만지는 시험만 스윕에 넣었고, 필터가 가리키는 메서드 집합도 같은 양방향 검사를 받는다.
- 반증: 가드가 찾을 파일(`ZzTmpCaller.cs`, 표지 호출 포함)과 목록에 없는 시험 메서드를 임시로 추가하면 `TheFileList…`, `TheMethodList…`, 그리고 기존 `PreprocessModuleCollectionGuardTests` 가 빨강 (`falsification_bc_unlisted_file_and_method.txt`).

## 5. 예외 경로 shutdown (낮음)
- `PreprocessCorrectionBoundaryTests.LoadCalibrated`: init 성공을 추적해 예외 경로에서 shutdown. 예외 주입(`generate` 매개변수) 뒤 첫 init 이 받아들여짐을 단언하는 시험 추가. 반증: 그 shutdown 을 빼면 "the module was left initialised by LoadCalibrated's exception path (init answered INVALID_INPUT)" (`falsification_a_no_shutdown_on_exception_path.txt`).
- **시험 설계 결함을 반증이 드러냈다**: 첫 판은 시험이 라이브러리를 `Free` 한 뒤 다시 불러서, 다른 핸들이 없으면 DLL 이 내려가며 모듈 상태도 사라져 shutdown 을 빼도 통과했다(첫 반증이 안 터졌다). 시험이 주입 내내 라이브러리를 붙들도록 고친 뒤 터진다.
- `BaselineReviewFixTests` 실모듈 러너 시험: 맵 준비를 `PrepareMapsThroughTheModule`(finally shutdown)로 뽑고, 주입 시험 `WhenPreparingTheMapsFailsAfterTheInit_TheModuleIsLeftUninitialised` 추가. 반증: finally 를 비우면 "the module was left initialised after a failure while preparing the maps (init answered -1)" (`falsification_a2_runner_maps_no_shutdown.txt`).

## 6. `git diff --check`
233h~233i 구간의 `git diff --check` 위반은 `GUI-C-233h/ps1_run_native_c233.txt` 의 끝 빈 줄 한 파일뿐이었다. 정리했다. 이 카드의 증거 파일은 모두 끝 빈 줄·줄 끝 공백을 제거했고, 커밋 전에 스테이지한 전체에 `git diff --check` 를 직접 돌렸고 종료 코드 0 이었다.

## 통과·범위
- SelfCheck 전 시나리오 통과 (`selfcheck_final.txt`). 통합 시험 연속 3회 954 통과 / 0 실패 / 2 건너뜀 (`integration_suite_runs.txt`).
- 처음 세 번의 전체 실행은 `SettingsProcessingConnectionTests.Collection_FindsTheKnownBindings` 한 건이 매번 실패했다: 위 3번 변경이 원인(읽기 전용 바인딩 소멸)이라 단언을 의도에 맞게 고쳤다.
- 이번 카드에서 돌린 UIA 는 `TwoLaneWorkbenchScenarios` 클래스뿐이다(Mock 백엔드 fixture). Native E2E 전체와 결합 CI 는 돌리지 않았다. 화면을 눈으로 확인하거나 스크린샷을 찍지는 않았다(UIA 트리와 VM 상태로만 확인): 자리표시의 모양·위치·글자 크기가 읽기 좋은지는 미확인.
- 자리표시 문구("No applied image", "Candidate: settings changed - apply again")는 영어 한 가지뿐이고 도움말 페이지(quick-start)에는 반영하지 않았다.
