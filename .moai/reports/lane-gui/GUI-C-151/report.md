# GUI-C-151 (#224) — `clients/ImageProcTest` 와 `gui/ImageProcTest` 의 차이

조사 보고입니다. **어느 앱도 수정하지 않았습니다.** 접을지 살릴지에 대한 판단은 쓰지 않았습니다 — 차이만 적습니다.

---

## 0. 한눈에

| | `clients/ImageProcTest` | `gui/ImageProcTest` |
|---|---|---|
| 화면 구성 | **탭 7개** 한 창 | 메뉴 6개 + 패널 6개(도킹) |
| C# 파일 / XAML | 58 / 4 | 53 / 9 |
| 구조 | 코드비하인드 중심, ViewModel 2개 | **MVVM** (`MainWindowViewModel`·`RelayCommand`) |
| 네이티브 함수 | **55개** | 31개 |
| CI 가 앱으로 빌드 | **안 함 (0건)** | 함 (`ci.yml:472`·`:542`) |
| CI 가 소스를 시험에 링크 | **11개 파일** | 14개 파일 |
| E2E 가 띄우는 exe | 아님 | 맞음 (`ApplicationFixture.cs:111`) |
| 다른 프로젝트가 참조 | 0건 | `ImageProcTest.E2E`·`SelfCheck` |
| 마지막 **내용 있는** 변경 | **2026-09-18** | **2026-09-28** |

---

## 1. 화면·기능 (카드 §2-(1))

### 1.1 `clients` — 탭 7개

`MainWindow.xaml` 한 파일에 전부 있습니다.

| 탭 | 줄 |
|---|---|
| Evaluation | `:174` |
| Calibration | `:440` |
| Diagnostics | `:691` |
| **Metrics** | `:790` ← `#224` 의 지표 12행이 실리는 곳 |
| Reports | `:918` |
| AI Module | `:950` |
| Help | `:1147` |

보조 컨트롤 2개: `Controls/EvaluationViewerPanel.xaml`, `Controls/HistogramControl.xaml`.

### 1.2 `gui` — 메뉴 6개 + 패널 6개

| 메뉴 (`MainWindow.xaml`) | 패널 (`Views/`) |
|---|---|
| File `:75` · Backend `:113` · View `:157` · Pipeline `:305` · Tools `:347` · Help `:395` | `TopBar` · `AlgorithmBar` · `StudyQueue` · `ViewportShell` · `AnalysisPanel` · `VerdictBar` |

그 밖에 `HelpWindow.xaml` 별창, 비교 모드 단축키 `F5`~`F8`(`MainWindow.xaml:47-50`).

### 1.3 기능 문자열 출현 (파일 수)

| 문자열 | `clients` | `gui` |
|---|---|---|
| `AiBridge` | **6** | **0** |
| `ReadinessSnapshot` | **9** | **0** |
| `DefectMap` | **1** | **0** |
| `Metrics` | **10** | 1 |
| `Histogram` | **11** | 3 |
| `Dicom` | **14** | 4 |
| `Evaluation` | 10 | 7 |
| `Calibration` | 15 | **18** |

---

## 2. **`clients` 에만 있는 것** (카드 §3 — 한 절로 분리)

### 2.1 화면

| 것 | 근거 | `gui` 쪽 |
|---|---|---|
| **AI Module 탭** (`:950`) + AI 브리지 상태 | `AiBridge*` 6파일 | **없음** (문자열 0건) |
| **Metrics 탭** (`:790`) — 오프셋/게인/결함 지표 12행 표 | `MetricsComputationService` | **없음** (`DarkBias`·`DSNU` 0건) |
| **모듈 준비도(Readiness) 등급 표시** | `ModuleReadinessSnapshot`·`ModuleReadinessViewModel` 9파일 | **없음** (문자열 0건) |
| **Reports 탭** (`:918`) | `GuiE2eReportService` | 내보내기는 메뉴 항목으로 있음(`ExportAutomationReportMenuItem`), 탭은 없음 |
| **Evaluation 뷰어 패널 · 히스토그램 컨트롤** | `Controls/*.xaml` 2개 | 전용 컨트롤 없음 |

### 2.2 네이티브 기능 — `clients` 에만 있는 함수 **29개**

| 묶음 | 함수 |
|---|---|
| **DICOM 서버·워크리스트** | `xpe_dicom_open` `xpe_dicom_close` `xpe_dicom_cfind_mwl` `xpe_dicom_cstore` `xpe_dicom_cancel` `xpe_dicom_validate` `xpe_dicom_get_metadata` `xpe_dicom_read_image` `xpe_dicom_write_j2k` |
| **캘리브레이션 생성·저장·만료** | `xpe_calib_generate_offset` `xpe_calib_save` `xpe_calib_check_expiry` |
| **결함 검출** | `xpe_defect_detect_runtime` |
| **공통 수명주기·로깅** | `xpe_init` `xpe_shutdown` `xpe_configure` `xpe_version` `xpe_error_string` `xpe_get_param_range` `xpe_log_set_level` `xpe_log_set_file` `xpe_log_flush` `xpe_copy_image` |
| 그 밖 | `xpe_noise_estimate_sigma` `xpe_log_inverse` `xpe_validate_readout_artifact` `xpe_preprocess_get_param_range` `xpe_preprocess_version` `xpe_enhance_basic_version` |

`gui` 에만 있는 함수는 **5개**: `xpe_gsvg_init` `xpe_gsvg_process_ex` `xpe_gsvg_shutdown` `xpe_nonlinearity_correct` `xpe_dicom_read`.
**공통은 26개**입니다.

즉 `gui` 는 **영상 처리 연쇄**(GSVG·비선형 포함)를 태우고, `clients` 는 거기에 더해 **DICOM 통신·캘리브레이션 생성·결함 검출·모듈 수명주기/로깅**을 태웁니다.

### 2.3 계측 주의 — 한 방법으로는 셀 수 없었습니다

두 앱이 **해상 방식이 다릅니다.** `clients` 는 대부분 `GetRequiredDelegate<...>(handle, "xpe_...")` 로 **문자열 동적 해상**하고,
`gui` 는 대부분 `[DllImport]` **선언**입니다. 처음에 따옴표 문자열만 세니 `gui` 가 17개로 나왔는데,
`DllImport` 선언까지 합치면 31개입니다(제 첫 수치가 `gui` 를 과소평가했습니다).

위 표는 **두 방법의 합집합**이고, 모듈 이름(`xpe_common`·`xpe_display` 등 DLL 기본명)은 함수가 아니므로 제외했습니다.

---

## 3. 마지막으로 **의미 있게** 바뀐 때 (카드 §2-(2))

커밋 제목이 아니라 **diff** 로 판단했습니다.

### 3.1 `clients/ImageProcTest`

| 커밋 | 날짜 | diff 가 실제로 한 것 | 의미 있나 |
|---|---|---|---|
| `0e6a60a` | 2026-09-18 | `PInvokeWrapper.cs` **1줄 추가**(오류 코드 상수 미러) | 아니오 — 상수 동기화 |
| `aa17f9b` | 2026-09-18 | `DetectorDefaults.cs` **신규 16줄** + 4파일에서 하드코딩 `0.143`→상수 **1줄씩** | **경계선** — 새 파일이 생겼으나 내용은 상수 모으기 |
| `3d97a97` | 2026-09-10 | `XpeCommonLibraryLocator.cs` **1줄** 치환 | 아니오 |

`aa17f9b` 의 커밋 본문을 읽으면 그 작업의 **주된 변경은 `gui` 쪽**(`AppSettings.PixelPitchMm`, 분석 패널 입력칸)이고,
`clients` 는 *"0.143 네 곳을 상수 하나로 모았다"* 는 **부수 정리**입니다.

→ **새 기능이 마지막으로 들어간 시점은 이 세 커밋보다 앞**입니다. 2026-09-10 이전으로, 이 조사에서는 그 경계를 더 좁히지 않았습니다.

### 3.2 `gui/ImageProcTest`

| 커밋 | 날짜 | diff 가 실제로 한 것 | 의미 있나 |
|---|---|---|---|
| `ab557c8` | 2026-09-28 | `AnalysisPanel.xaml(.cs)` — 선택 스크롤 처리기 신규 | **예 — 동작 변경** |
| `e5d86e3` | 2026-09-27 | `MainWindowViewModel` +36줄, XAML +25 — "Alerts only" 필터 신규 | **예 — 기능 추가** |
| `8bb3263` | 2026-09-27 | `App.xaml.cs` +4, `ImageComparisonViewport.cs` +48 — 렌더 내보내기 경로 | **예 — 기능 추가** |
| `d87c128` | 2026-09-27 | `MainWindow.xaml.cs` +11, 보고서 모델 +10 | **예 — 동작 변경** |
| `bff8001` | 2026-09-19 | `RealXpeBackend.cs` +15 — GSDF 광도 배열 | **예** |

→ `gui` 는 **10일 안에 기능 추가 3건·동작 변경 2건**이 있습니다.

---

## 4. `clients/README.md` 의 소개가 현재와 맞는가 (카드 §2-(3))

**틀린 것만 적습니다. 고치지 않았습니다.**

| README 의 말 | 실제 | 판정 |
|---|---|---|
| 제목: *"WPF app and **its** integration tests"* | `ImageProcTest.IntegrationTests` 도 `ImageProcTest.E2ETests` 도 이 앱을 `ProjectReference` 하지 **않습니다.** E2ETests 는 **`gui/` 의 exe** 를 띄웁니다(`ApplicationFixture.cs:111`). IntegrationTests 는 이 앱 소스 11개를 **링크 컴파일**하지만 앱을 시험하지는 않습니다 | **틀림** — "its" 가 소유를 말한다면 사실이 아닙니다 |
| `XPE_NATIVE_DEV_SEARCH=1` 로 폴백 재활성 | `clients` 1파일에 존재. **`gui` 에는 0건** | 맞음(단, clients 전용) |
| `XPE_NATIVE_DIR` | 양쪽 2파일씩 | 맞음 |
| `XPE_NATIVE_DIR_EXCLUSIVE=1` | 양쪽 1파일씩 | 맞음 |
| `ModuleReadinessSnapshot.ResolvedDllPath` 에 기록 | `clients` 5파일에 존재. **`gui` 에는 0건** | 맞음(단, clients 전용) |
| `native modules: …` 추적 줄 | `clients` 1파일. **`gui` 0건** | 맞음(단, clients 전용) |

**요약**: 네이티브 탐색에 관한 기술은 **전부 사실**이되 **`clients` 앱에만** 해당합니다.
제목의 *"its integration tests"* 한 곳이 현재와 어긋납니다 — 그 시험들은 이 앱의 것이 아닙니다.

---

## 5. 네이티브 연동 깊이 (카드 §2-(4))

**둘 다 네이티브를 태웁니다.** 한쪽만 껍데기가 아닙니다.

| | `clients` | `gui` |
|---|---|---|
| 보정 연쇄 | `offset`·`gain`·`defect_correct` 호출 | 같음 + `nonlinearity_correct` |
| GSVG | 버전만 조회(`xpe_gsvg_version`) | **실행**(`init`·`process_ex`·`shutdown`) |
| 캘리브레이션 | 적재 + **생성·저장·만료검사** | 적재만 |
| DICOM | 열기·읽기·**저장(C-STORE)·워크리스트(C-FIND MWL)·검증** | 읽기/쓰기 |
| 알림 큐 | 사용(`get_pending_alert*`·`clear_alerts`) | 같음 |
| 해상 방식 | 핸들 + 문자열 동적 해상 위주 | `DllImport` + 공용 리졸버(`GuiNativeLibraryResolver`) 위주 |

---

## 6. CI 가 각 앱의 코드를 만지는 방식

| | `clients/ImageProcTest` | `gui/ImageProcTest` |
|---|---|---|
| 앱으로 빌드 | **0건** | `ci.yml:472`·`:542` |
| exe 실행 | 없음 | `gui-automation`·`gui-e2e-native` |
| **소스를 시험에 링크 컴파일** | **11파일** (`IntegrationTests`) | 12파일 (`IntegrationTests`) + 2파일 (`E2ETests`) |

`clients` 소스 중 CI 가 컴파일하는 11개는 준비도·네이티브 탐색·AI 브리지 계열입니다:
`ModuleReadinessSnapshot` `AiBridgeConnectionStatus` `AiBridgePanelSnapshot` `AiBridgeStatusComputer`
`NativeModuleLibraryLocator` `ModuleReadinessGrading` `NativeSearchPolicy` `XpeEnhanceBasicLibraryLocator`
`XpePreprocessLibraryLocator` `XpeCommonLibraryLocator` `AlertDisplayFormatter`.

**`MetricsComputationService.cs` 는 이 목록에 없습니다** — `#224` 의 결함 넷이 있는 파일은 CI 가 컴파일하지 않습니다.

---

## 7. 미검증 / 이 조사가 답하지 않는 것

- **사람이 어느 앱을 실제로 쓰는지는 코드로 알 수 없습니다.** 빌드된다·CI 가 만진다는 것과 누가 연다는 것은 다릅니다
- `clients` 의 "새 기능이 마지막으로 들어간 시점" 을 2026-09-10 이전까지만 좁혔고 더 뒤로 추적하지 않았습니다
- 탭 안의 **개별 버튼·입력 단위**로는 비교하지 않았습니다(카드가 탭·패널 단위를 지시)
- 두 앱을 **실행해 보지 않았습니다.** 전부 소스·워크플로 읽기입니다
- 네이티브 함수 집계는 §2.3 의 두 방법 합집합이고, 그 밖의 해상 방식(예: 런타임 문자열 조립)이 있다면 놓칩니다
- `clients/README.md` 의 `#129` 정책을 `gui` 가 **따로 구현한 것은 diff 로 확인했습니다** — `3d97a97`(2026-09-10)이
  `gui/ImageProcTest/Services/Native/GuiNativeLibraryResolver.cs` **107줄 신규** + `App.xaml.cs` +5 +
  `XpeBackendFactory.cs` +11 + 시험 92줄을 넣었고, 같은 커밋에서 `clients` 쪽은 **1줄** 치환뿐입니다.
  즉 두 앱이 같은 정책을 **각자 구현**하고 있습니다. 다만 그 두 구현이 동작까지 같은지는 비교하지 않았습니다

---

Refs #224
