# GUI-C-94 보고 — GSVG 를 GUI 에 드러낼 자리 (판독, #180)

코드 변경 없음. 판독 기준은 **`origin/main` `6fde6fc`** 이고, 조사 마스크 여부만 `dev/postprocess` `3788c88` 까지 봤다. 아래 파일:줄은 모두 이 기준이다.

## 0. 요약

- **GUI 안의 GSVG**: `gui/` 에 GSVG 참조는 **0건**이다(`git grep -i "gsvg|grid.supp|virtual.grid" origin/main -- gui`). GUI 의 네이티브 해석기는 `xpe_common`/`xpe_display`/`xpe_preprocess` 세 DLL 만 안다(`Services/Native/GuiNativeLibraryResolver.cs:24-26`). GSVG DLL 이름은 `gsvg.dll` 이다(`modules/gsvg/CMakeLists.txt:20` `OUTPUT_NAME "gsvg"`).
- **픽셀 연쇄**: GUI 에는 전처리 → 후처리 → 디스플레이 순서의 **픽셀 연쇄가 없다.** 디스플레이는 항상 raw 픽셀에서 시작하고, 전처리 결과는 화면에만 표시된다(1절).
- **kVp**: 네이티브 전처리 러너에 **70 kVp 가 하드코딩**돼 있다. 영상 메타데이터에서 오는 kVp 는 없다(2절).
- **조사 마스크**: 모듈 공개 헤더에 **조사/콜리메이션 검출 API 가 없고**, GUI 에도 검출 결과가 없다. GSVG 의 마스크 진입점은 main 과 `dev/postprocess` 어디에도 **아직 없다**(3절).

## 1. 현재 경로 — GSVG 가 들어갈 자리

### 1.1 GUI 에서 실제로 도는 경로

| 단계 | 위치 | 입력 픽셀 |
|---|---|---|
| RAW 로드 | `ViewModels/MainWindowViewModel.cs:935-951` `LoadImageFromPathAsync` → `_backend.LoadRawImage`, 끝에서 `ApplyDisplayPipelineAsync()` | 파일 |
| 디스플레이 (Native) | `Services/RealXpeBackend.cs:97-190` `ApplyDisplayPipelineCore` — `rawFrame.RawPixels` 를 float 로 옮긴 뒤(`:130`) modality → VOI → presentation | **항상 raw** (`ActiveImageFrame.RawPixels`) |
| 전처리 (Phase 1a, 별도 명령) | `MainWindowViewModel.cs:1021-1056` `RunPreprocessing` → `_backend.RunPreprocessing` | raw |
| 전처리 결과 | `:1039-1045` — `ProcessedImage` (보이는 미리보기)만 바꾼다. **`ActiveImageFrame` 은 바꾸지 않는다** | — |

- **결과**: 전처리를 돌린 뒤 "Apply Display Pipeline" 을 누르면 **다시 raw 에서** 디스플레이를 만든다. 전처리 → 디스플레이 연쇄가 없다.
- **후처리(enhance) 단계**: GUI 실행 경로에 **없다.**
  - `Services/PipelineOrchestrator.cs:150-214` 에 Raw → Pre-process → Enhance → EI → Display 사슬이 적혀 있지만, 이 클래스를 참조하는 곳은 README 뿐이다(`git grep PipelineOrchestrator`: `gui/ImageProcTest/README.md:202,204`).
  - 그 안의 Enhance 는 `// TODO: xpe_enhance_basic.dll P/Invoke 구현` 이다(`:334-336`).
- **Mock 백엔드**: `Services/MockXpeBackend.cs` 도 같은 `IXpeBackend` 계약만 따른다. GSVG 에 해당하는 멤버는 없다.

### 1.2 `IXpeBackend` 계약 (`Services/IXpeBackend.cs`)

- **현재 멤버**: `LoadRawImage`(:26), `ApplyDisplayPipeline`(:31), `RunPreprocessing`(:37), `SupportsPreprocessing`(:40), `CreateVoiPreset`(:50), 경보·로그·수명 멤버.
- GSVG 용 멤버는 없다.

### 1.3 두 문서의 위치 주장과 GUI 의 대조

| 출처 | 위치 | 근거 |
|---|---|---|
| 설계 문서 | **RAW 직후** | `docs/post-processing/gsvg/GSVG-SAD-001_Architecture.md:16` `RAW --> GSVG`, `GSVG_IEC62304_ClassB_Document_Package.md:235`, `docs/project/api-spec.md:1445` "raw 16-bit pixel buffer" |
| 모듈 e2e | **enhance 뒤, VOI 앞** | `tests/e2e_post_pipeline/test_e2e_full_pipeline.cpp`: log(:151) → CLAHE(:159) → USM(:163) → **gsvg(:180)** → VOI(:196). 같은 파일 :14 "GSVG operates on uint16; we convert a snapshot of the float buffer to uint16" |
| GSVG API | uint16 입력·출력, `vg_air_signal` 은 **검출기 DN** | `modules/gsvg/include/xpe/gsvg/gsvg_api.h:83-85,186-194` |

**판독**:
- GUI 에는 enhance 단계가 없고, 디스플레이 입력이 raw uint16 이다.
- GSVG 입력은 uint16 이고, 가상 그리드의 `vg_air_signal` 은 검출기 DN 이다.
- 따라서 **"RAW 직후"(디스플레이의 float 변환 전) 가 GUI 의 실제 데이터와 맞는다.** e2e 의 "enhance 뒤" 는 GUI 에 대응하는 단계가 없다.

### 1.4 선택지 (결정은 lead)

| 선택지 | 위치 | 영향 |
|---|---|---|
| A | `RealXpeBackend.ApplyDisplayPipelineCore` 안, `:127-130` float 변환 **전** raw uint16 에 GSVG 적용 | 디스플레이를 다시 적용할 때마다 GSVG 도 다시 돈다(성능, 결과 캐시 필요 여부). 새 계약 멤버 없이 설정만으로 켤 수 있다. Mock 도 같은 자리에 흉내를 넣어야 한다 |
| B | `IXpeBackend` 에 별도 단계(예: `ApplyGridCorrection`)를 두고, 결과로 `ActiveImageFrame` 의 픽셀을 **교체** | 처음으로 픽셀 연쇄가 생긴다. 원본 보존(REQ-GSVG-022)을 위해 raw 와 교정본을 따로 들고 있어야 한다. 전처리 결과를 연쇄에 넣을지도 함께 정해야 한다 |
| C | 전처리 러너(`GuiPreprocessRunner`) 뒤에 붙이기 | 전처리 결과가 디스플레이로 이어지지 않는 현재 구조(1.1)를 먼저 바꿔야 의미가 있다 |

## 2. 설정 표면

### 2.1 지금 있는 것

- **`Models/AppSettings.cs`** (JSON 키, 줄): 백엔드·RAW(:65-95), 교정 경로·모드 7종(:105-195), VOI·모달리티·GSDF(:215-275), 비교·레이아웃(:285-412), Lane B(:419-426), `lastRunSetId`(:433).
  - GSVG 관련 키는 **없다.**
- **분석 패널 `Views/AnalysisPanel.xaml`** "parameters" 탭(:154-280):
  - Calibration Stages 7종(:155-215)
  - VOI LUT(:217-250)
  - **Lane B Overrides**(:252-280, Sharpening sigma / Denoise strength)
- **Lane B Overrides 는 처리에 쓰이지 않는다.** 뷰모델의 대입·초기화만 있다(`MainWindowViewModel.cs:605-614`, `:1524-1530`).
- **교정 모드 7종도 네이티브 처리에 쓰이지 않는다.** `RealXpeBackend.cs:170` 의 요약 문자열에 "preprocess native bridge pending" 과 함께 찍힐 뿐이다(`BuildCalibrationEvaluationSummary` :390).
  - 선례: "설정은 보이는데 처리에 안 닿는" 패턴이 이미 있다. GSVG 설정을 추가할 때 같은 형태가 되지 않도록 시험이 필요하다(5절).
- **설정 파일**: `appsettings.json` 을 `Services/AppSettingsService.cs` 가 읽고 쓴다. 템플릿은 `fixtures/gui-s0/appsettings.template.json` 이다.

### 2.2 넣어야 할 항목과 GSVG 측 요구

| 항목 | GSVG 설정 키 (`gsvg_api.h`) | 제약 | GUI 에 지금 있는가 |
|---|---|---|---|
| 그리드 억제 켜기 | `"grid_suppression"` (:72) | 가상 그리드와 **동시에 켤 수 없다** (:21, :109-112 → `XPE_ERR_CONFIG_INVALID`) | 없음 |
| 가상 그리드 켜기 | `"virtual_grid"` (:73) | 켜면 아래 6개가 **필수** (:78-86) | 없음 |
| 가상 격자비 6/8/10/12 | `"vg_grid_ratio"` (:83) | **표의 `[grid]` 절에 있는 행**이어야 한다. REQ-GSVG-016(`SPEC-XPE-GSVG/spec.md:188-194`)의 네 값이 표에 있는지는 표 파일에 달렸다 | 없음 |
| 표 파일 경로 | `"vg_table_path"` (:81) | JSON 문자열이라 `\` 를 이스케이프해야 한다. 형식은 `modules/gsvg/src/virtual_grid.h` | 없음 (교정 경로 3종과 같은 형태로 둘 수 있다) |
| kVp | `"vg_kvp"` (:82) | 표의 kVp 범위 밖이면 process 가 `CONFIG_INVALID`, 원본 유지 (:174-178) | **하드코딩 70**: `Services/Native/GuiPreprocessRunner.cs:97` `XpeImageMetadataNative.Create(bodyPart, kVp: 70.0f, …)` |
| 화소 간격 mm | `"vg_pixel_pitch_mm"` (:84) | 필수 | 없음 |
| 공기 신호 DN | `"vg_air_signal"` (:85) | 필수 | 없음 |
| 반복 횟수 | `"vg_iterations"` (:86) | 1..100 | 없음 |
| (선택) 피라미드·잡음 | `vg_pyramid_levels/gain`, `vg_denoise_k` (:88-94) | levels 4..8 | 없음 |

- **kVp 의 출처**:
  - GUI 가 여는 것은 RAW 뿐이고(DICOM 열기 메뉴는 비활성, `MainWindow.xaml` `OpenDicomMenuItem` `IsEnabled="False"`), RAW 에는 메타데이터가 없다. **사용자 입력이 유일한 출처**다.
  - DICOM 입력이 생기면 태그에서 읽을 수 있지만, 이 앱의 현재 범위 밖이다.
- **설정 문법의 함정**: GSVG 설정은 JSON 파서가 아니라 키 스캔으로 읽는다(:61-66). **키 오타는 조용히 기능을 끈다**(`grid_suppression` 의 경우). GUI 가 설정 문자열을 직접 조립하면 오타를 시험으로 막아야 한다.

### 2.3 선택지

| 선택지 | 영향 |
|---|---|
| 분석 패널 "parameters" 탭에 "Grid" 절 추가(교정 단계 아래) | 기존 패턴과 같다. 켜기 둘은 상호 배타라서 라디오(없음/억제/가상)가 자연스럽다 |
| 설정 파일 전용(UI 없음) + 상태 표시만 | 구현이 작다. REQ-GSVG-016 "사용자가 고른다" 를 UI 로 만족하지 못한다 |
| 격자비: 콤보 4값 고정 vs 표에서 읽은 행 | 고정이면 표에 없는 값을 고를 수 있다(process 오류). 표에서 읽으면 GUI 가 표 형식을 파싱해야 한다 |

## 3. 조사 마스크

- **GSVG API**: main `gsvg_api.h` 의 `xpe_gsvg_process`(:186-194)에 마스크 인자가 **없다.**
  - `dev/postprocess` `3788c88`(QA-B-95)에도 헤더에 마스크 진입점이 없다(`git grep -i mask dev/postprocess -- modules/gsvg/include` 0건).
  - 같은 브랜치의 `modules/gsvg/tests/test_virtual_grid_mc.cpp:100-101` 이 "The virtual grid has no collimation mask" 라고 적고 있다. **QA-B-96 은 아직 들어오지 않았다.**
- **검출 API**: 모듈 공개 헤더 전체(`modules/*/include`)에 `collimat|irradiat|shutter|detect` 가 0건이다.
  - CI 산출물에 `xpe_detect_experiment.exe` 가 있지만(C-88 스테이징 목록) 공개 API 는 아니다.
- **GUI**: `collimat|irradiat|shutter` 0건이다.
  - GUI 에 있는 영역 개념은 표시용 ROI 토글(`MainWindowViewModel.cs:54,117,656`; `Views/ViewportShell.xaml:112-114`)과, 쓰이지 않는 `PipelineOrchestrator` 의 고정 `RoiRegion`(:457)뿐이다.
  - 둘 다 조사 영역이 아니다.
- **판독: GUI 는 조사 마스크를 가지고 있지 않다.** 마스크를 넘기려면 누군가(검출 모듈, 사용자 지정, DICOM 태그) 마스크를 만들어야 하고, 그 형태(uint8 w×h 등)는 QA-B-96 의 진입점이 정해지는 것을 봐야 한다.

## 4. 안전 표시 — 붙일 수 있는 자리

| 자리 | 위치 | 원천 | 비고 |
|---|---|---|---|
| 디스플레이 요약(상태줄 텍스트) | `LoadedImageFrame.DisplayPipelineSummary` (`Models/LoadedImageFrame.cs:25`) ← `RealXpeBackend.cs:170` 문자열, 상태줄 `StatusBarText` (`MainWindow.xaml:435`) | 백엔드 | `-> GSVG(virtual 10:1, 80 kVp)` 같은 조각을 넣기 가장 쉽다. 자동화 보고서에도 이미 복사된다(`GuiAutomationReport.cs:80`) |
| HUD (영상 옆) | `Views/ViewportShell.xaml:164` `HudVoiWindow` 와 같은 줄들 | 렌더 입력 스냅샷(`RenderedVoi*`, #171 ②) | "영상을 만든 값" 원칙(#171)에 맞추려면 **렌더 입력 스냅샷에 GSVG 설정을 함께 담아야** 한다 |
| 오래됨 배너 | `Views/ViewportShell.xaml:32` `PreviewStaleBanner` + `DisplayInputsDiffer` (#171 ①) | 설정 대 렌더 입력 | GSVG 설정을 바꾸고 적용하지 않은 경우도 "오래됨" 이 되려면 `DisplayInputsDiffer` 에 추가해야 한다 |
| 경고 배너(해제 불가) | `MainWindow.xaml:56` `MockBackendBanner` (HAZ-GUI-005 선례) | `RuntimeInfo` | Mock 전용이다. 가상 그리드 표시를 같은 형태로 둘지는 결정 사항 |
| 창 제목 접두 | `MainWindowViewModel.cs:462` `WindowTitle` (`[MOCK]` 선례) | — | 같은 방식으로 붙일 수 있다 |
| 보고서 3종 | 자동화 보고서 `GuiAutomationReport.cs:16,24` / `menu-command-report.json` `MainWindowViewModel.cs:790` / 증거 묶음 `backend.json` `:1463-1465` | — | 각각에 GSVG 적용 여부와 설정을 넣을 자리가 있다(C-82/C-85 에서 필드를 추가한 곳) |
| 분리 뷰어 | `MainWindowViewModel.cs:1239` 부근(C-80 의 분리 뷰어 HUD·배너) | 같은 뷰모델 상태 | 메인 창에 붙이면 분리 뷰어에도 붙여야 한다(#171 교훈) |
| DICOM "Processed" 표시 (REQ-GSVG-023, `spec.md:258-264`) | **해당 없음** — GUI 에 DICOM 쓰기가 없다(`OpenDicomMenuItem` 비활성, `PipelineOrchestrator` 의 DICOM Write 는 미사용) | — | GUI 쪽에서 충족할 자리가 없다 |

- **REQ-GSVG-024 실패 처리**: 실패하면 원본 픽셀을 되돌리고 경보 큐에 사유를 넣는다(`gsvg_api.h:122-124, 174-178`).
  - GUI 는 이미 네이티브 경보를 수거한다(`RealXpeBackend` `DrainNativeAlerts`, #110).
  - "가상 그리드를 요청했지만 적용되지 않음" 을 **적용 표시와 구별해** 보여야 한다. HAZ-GUI-005 의 "요청과 실제" 표기(`mode=Mock (requested Native)`)와 같은 형태가 선례다.

## 5. E2E 뼈대 (제안만)

- **기반으로 쓸 기존 파일**:
  - `Scenarios/Workflows/WorkbenchObservation.cs`: 뷰포트 버전·HUD·오래됨·상태줄 읽기
  - `ViewportTruthScenarios.cs`: 설정 변경 → 적용 → 버전 증가 패턴
  - `AutomationReportBackendTests.cs`: 자동화 보고서 파일 읽기
  - `MockBackendDisclosureScenarios.cs`: 요청과 실제의 표기

| 시험 | 조작 | 단언 | 반증 |
|---|---|---|---|
| G-01 끄기 → 켜기가 출력에 닿는다 | 가상 그리드 끔 → 적용 → 켬 → 적용 | 처리 영상 버전이 오르고 **처리 픽셀 지문이 다르다**, 상태줄에 GSVG 조각 | 백엔드에서 GSVG 호출을 빼면 지문이 같아져 실패 |
| G-02 격자비 변경이 출력에 닿는다 | 8:1 → 12:1 → 적용 | 지문이 다르고, HUD/요약의 격자비가 적용한 값 | 격자비를 설정 문자열에 넣지 않으면 실패 |
| G-03 적용 안 한 설정 변경은 오래됨 | 격자비만 바꿈 | 오래됨 표시, HUD 는 이전 격자비 | `DisplayInputsDiffer` 에서 빼면 실패 |
| G-04 표 파일 없음 | 없는 경로로 켬 → 적용 | 원본 픽셀 유지(지문 = 끔 상태), 경고(경보 목록·상태줄), "요청했지만 적용 안 됨" 표시 | 경보를 표시하지 않으면 실패 |
| G-05 억제와 가상 동시 켜기 | 둘 다 켬 | 거부(설정 오류 표시), 처리 없음 | — |
| G-06 kVp 가 표 범위 밖 | kVp=200 | G-04 와 같은 실패 경로 | — |
| G-07 보고서 | 적용 뒤 자동화 보고서/증거 묶음 | GSVG 적용 여부·격자비·kVp 필드 | 필드를 끄면 실패 |

- **Mock 에서 할 것**: G-01·G-02 의 "출력에 닿는다" 를 Mock 에서도 보려면 Mock 도 설정에 따라 픽셀을 바꿔야 한다. 그러지 않으면 Mock 실행에서는 지문 단언을 건너뛰어야 한다(C-79 의 백엔드별 분기 선례).
- **픽셀 지문 계측 방법**:
  - GUI-C-77 의 픽셀 지문 계측과 GUI-C-84 의 프로브(8-bit 미리보기 통계)가 있다.
  - 16-bit 처리 결과를 직접 보려면 계측점(자동화 보고서 필드 등)이 새로 필요하다. 8-bit 미리보기는 min..max 로 다시 늘려 표시하므로(`RealXpeBackend.CreatePreview`) 작은 차이를 지울 수 있다.

## 6. 변경이 필요한 곳 목록 (결정 후)

1. **네이티브 로드**: `Services/Native/` 에 `gsvg.dll` P/Invoke 선언과 해석기 등록(`GuiNativeLibraryResolver.cs:24-26` 옆), 준비 상태 표기(clients 쪽 `NativeReadinessProbe` 선례).
2. **백엔드 계약**: `IXpeBackend` (선택지 A 면 변경 없음, B 면 새 멤버), `RealXpeBackend`, `MockXpeBackend`, 결함 주입 래퍼 `FaultInjectingBackend` (새 멤버가 생기면 위임 추가).
3. **설정**: `AppSettings` 키, `AppSettings.Snapshot` 비교 대상(`DisplayInputsDiffer`), 설정 템플릿, SelfCheck 기본값.
4. **UI**: `AnalysisPanel.xaml` parameters 탭, 뷰모델 옵션 목록.
5. **표시**: 요약 문자열, HUD(렌더 입력), 분리 뷰어, 보고서 3종.
6. **kVp**: `GuiPreprocessRunner.cs:97` 의 하드코딩 70 과 새 kVp 입력의 관계를 정해야 한다(같은 값을 쓸지).
7. **시험**: 5절.

## Card Cross-Check

| 항목 | 카드 |
|---|---|
| GSVG GUI 노출 판독 | GUI-C-94 (이 카드) |
| 위치 선택지 A/B/C, 설정 UI 형태, 격자비 목록 출처 | lead 결정 |
| 조사 마스크 진입점 | QA-B-96 (post) — 미착륙 |
| 교정 모드·Lane B 설정이 처리에 닿지 않음 | 기존 관찰, 신규 카드 후보 |
