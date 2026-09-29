# GUI-C-152 — 비활성 21건의 선행조건

분류만 적습니다. **구현하지 않았고**, `IsEnabled` 를 건드리지 않았습니다(`#165` 의 자동 집계가 거짓말하지 않도록).

`지금 가능?` 값: **가능**(필요한 것이 저장소 안에 전부 있음) / **선행 필요**(다른 구현이 먼저) / **사람 필요**(실장비·기준 영상·임상 기준). DICOM 은 사용자 지시에 따라 **[후순위]** 를 따로 답니다.

---

## 1. 21행 표

| # | 항목 (`MainWindow.xaml`) | 무엇이 없어서 비활성인가 | 지금 가능? |
|---|---|---|---|
| 1 | `OpenRecentMenuItem` `:82` — Open Recent | 최근 파일 이력을 **저장하는 코드가 없습니다** — `RecentFile`·`recentFiles` 류 문자열 `gui/` 전체 **0파일**. `AppSettings`/`AppSettingsService` 는 있으므로 얹을 자리는 있습니다 | **가능** |
| 2 | `OpenDicomMenuItem` `:87` — Open DICOM (1b) | 라이브 코드에 DICOM 호출이 **없습니다**. `xpe_dicom_read`/`write` 선언은 **미참조 파일** `PipelineOrchestrator.cs` 안에만 있습니다(§3) | **선행 필요** `xpe_dicom` 연동 · **[후순위]** |
| 3 | `ExportEvidenceBundleMenuItem` `:101` — Evidence Bundle (1a+) | 결정론적 실행 산출물을 **묶는 규격이 없습니다.** `evidence` 문자열은 4파일에 있고 `ExportAutomationReportMenuItem`(활성)이 보고서 한 건은 이미 내보냅니다 — 없는 것은 **번들 규격**입니다 | **가능**(규격을 정하면 코드만) |
| 4 | `NativeBackendModeMenuItem` `:133` — Native (P0-07) | **라우팅은 이미 있습니다.** `XpeBackendFactory` 가 `AppSettings.BackendMode=="Native"` 면 `RealXpeBackend` 를 만들고, CI 의 `gui-e2e-native` 가 그 경로로 돕니다. 없는 것은 **메뉴에서 바꾸는 명령**뿐입니다(옆의 `_Mock` 은 활성) | **가능** |
| 5 | `OpenRuntimeLogsMenuItem` `:145` — Runtime Logs (1a+) | 런타임 로그를 **파일로 내보내는 경로가 없습니다.** 로그는 화면 목록(`Logs`)에만 있습니다 | **가능** |
| 6 | `PInvokeSmokeTestMenuItem` `:150` — P/Invoke Smoke (P0-07) | 툴팁이 요구하는 `xpe_common.dll` + `RealXpeBackend` 는 **둘 다 있습니다.** 없는 것은 **앱 안에서 띄우는 명령** | **가능** |
| 7 | `ShowCalibrationPanelMenuItem` `:178` — Calibration Paths 패널 (1a) | 패널 자체가 없습니다. 캘리브레이션 적재는 이미 라이브(`xpe_calib_load_offset`·`_gain`·`_defect_map`)이므로 **보여 줄 값은 있습니다** | **가능** |
| 8 | `ShowDisplaySettingsPanelMenuItem` `:184` — Display Settings 패널 (1b) | 패널이 없습니다. 디스플레이 파이프라인은 라이브(`apply_modality/voi/presentation_lut`, `gsdf_calibrate`)라 **조절 대상은 있습니다** | **가능** |
| 9 | `RunDeterministicBaselineMenuItem` `:319` — Deterministic Baseline (1b) | 툴팁이 `preprocess`·`enhance_basic`·`display`·`dicom` 넷을 요구하는데 **라이브는 preprocess·display 둘뿐**입니다(§3) | **선행 필요** `enhance_basic`·`dicom` 연동 |
| 10 | `RunFullPipelineMenuItem` `:324` — Full Pipeline (2/3) | 프리미엄·보조 모듈(AI 계열)이 필요합니다 — `#130` 계보 | **선행 필요** `#130` |
| 11 | `StopProcessingMenuItem` `:329` — Stop Processing | 툴팁이 *"real processing run command 가 생긴 뒤"* 라고 합니다. 실제 처리는 **이미 돕니다**(Apply → 체인). 없는 것은 **취소(cancellation) 배선** | **가능** |
| 12 | `StageTimingMenuItem` `:335` — Stage Timing | **타이밍 증거는 이미 있습니다** — 체인 상태 문자열이 `times: preprocess=… gsvg=…` 를, `RealXpeBackend` 가 `DisplayTimings` 를 냅니다(관련 10곳). 없는 것은 **그걸 보여 주는 화면** | **가능** |
| 13 | `OpenPipelineDiagnosticsMenuItem` `:340` — Pipeline Diagnostics (1a+) | 위와 같은 재료(추적·타이밍)가 있고, **진단 화면이 없습니다** | **가능** |
| 14 | `OpenEvidenceFolderMenuItem` `:362` — Evidence Folder (1a+) | 앱이 **증거 디렉터리를 소유·관리하지 않습니다**. 산출은 자동화 보고서 파일 단위로 나갑니다 | **가능** |
| 15 | `RunSelfCheckMenuItem` `:368` — Run Self-Check | 툴팁이 스스로 적습니다 — *"현재 테스트 러너로 가능, 앱 안 실행은 나중"*. **`gui/ImageProcTest.SelfCheck` 프로젝트가 있습니다** | **가능** |
| 16 | `RunGuiE2EMenuItem` `:373` — Run GUI E2E | 같은 형태. **`gui/ImageProcTest.E2E` 프로젝트가 있습니다** | **가능** |
| 17 | `BenchmarkRunnerMenuItem` `:378` — Benchmark Runner (1a+) | 툴팁이 **매니페스트 + 네이티브 실행**을 요구합니다. 매니페스트는 **1건 있습니다**(`benchmark/BP-01-05-preprocess-manifest.md`), 네이티브 실행도 라이브입니다 | **가능** |
| 18 | `QaConstancyMenuItem` `:383` — QA Constancy (1b+) | **기준 영상과 판정 기준이 저장소에 없습니다**(§4) | **사람 필요** |
| 19 | `GsdfCalibrateMenuItem` `:388` — GSDF Calibrate (1b+) | **네이티브는 이미 돕니다** — `RealXpeBackend.cs:181` 이 `xpe_gsdf_calibrate` 를 체인 안에서 호출합니다. 없는 것은 **운영자용 독립 캘리브레이션 워크플로**이고, 툴팁이 요구하는 *"validated PS3.14 luminance calibration"* 은 **실측 광도값**이 있어야 합니다 | **사람 필요**(측정 광도) |
| 20 | `OpenApiReferenceMenuItem` `:416` — API Reference | **DocFX/Doxygen 설정 2건이 저장소에 있습니다.** 없는 것은 **생성물 패키징과 여는 명령** | **가능** |
| 21 | `OpenTroubleshootingMenuItem` `:421` — Troubleshooting (1b) | 툴팁이 *"deterministic baseline troubleshooting 페이지가 패키징되면"*. 문서는 **1건 있습니다**(`docs/` 검색). 툴팁이 말하는 baseline 은 **행 9**(Deterministic Baseline)이고 그것이 선행입니다 | **선행 필요** 행 9 |

### 1.1 집계

손으로 세지 않고 스크립트로 분류·합산했습니다(항목마다 값을 붙이고 **미분류 0 · 합계 21** 을 단언).

| 값 | 건수 | 해당 행 |
|---|---|---|
| **가능** | **15** | 1 · 3 · 4 · 5 · 6 · 7 · 8 · 11 · 12 · 13 · 14 · 15 · 16 · 17 · 20 |
| **선행 필요** | **4** | 2 · 9 · 10 · 21 |
| **사람 필요** | **2** | 18 · 19 |
| 합계 | **21** | 미분류 0 |

**[후순위]** 표시는 DICOM 관련 **1건**(행 2)이고, 위 분류와 **직교**합니다 — 별도 축이라 합계에 더하지 않습니다.

## 2. 이 표가 말하지 않는 것

**"가능" 은 "쉽다" 가 아닙니다.** 필요한 재료가 저장소 안에 있다는 뜻일 뿐, 분량·설계 결정은 항목마다 다릅니다.
예를 들어 4·6·15·16 은 **명령 배선**에 가깝고, 7·8·12·13 은 **새 화면**이 필요합니다.
무엇부터 할지는 카드가 금지한 판단이라 쓰지 않았습니다.

---

## 3. `PipelineOrchestrator` (SWU-5.7) — **일부만 덮습니다**

카드 §2 가 시킨 단계별 대조입니다. 스텁 메서드 10개를 라이브 코드와 맞췄습니다.

| # | 스텁 단계 (`PipelineOrchestrator.cs`) | 라이브에 있나 | 어디 |
|---|---|---|---|
| 1 | `ReadDicomAsync` (`xpe_dicom_read`) | **없음** | 그 심볼은 **이 죽은 파일에만** |
| 2 | `PreprocessAsync` (`xpe_preprocess`) | **있음** | `StageIds.Preprocess` → `XpePreprocessInterop`(`offset`·`gain`·`defect`·`nonlinearity`) |
| 3 | `EnhanceAsync` (`contrast`·`edge`·`noise`) | **없음** | 세 심볼 모두 죽은 파일에만 |
| 4 | `CalculateExposureIndexAsync` (`xpe_calc_exposure_index`) | **없음** | 죽은 파일에만 |
| 5 | `ApplyDisplayPipelineAsync` (`xpe_display`) | **있음** | `RealXpeBackend`·`XpeDisplayInterop`(`modality`/`voi`/`presentation` LUT) |
| 6 | `WriteDicomAsync` (`xpe_dicom_write`) | **없음** | 죽은 파일에만 |
| 7 | `LoadCalibrationImageAsync` (균일 보정 영상) | **없음** | — |
| 8 | `CalculateSnr` | **없음** | — |
| 9 | `CalculateUniformity` | **없음** | — |
| 10 | `CountDefects` | **없음** | — |

**전부 덮지도, 전부 비지도 않습니다 — 10 중 2.** 그러므로 카드의 두 갈래 중 어느 쪽도 그대로는 맞지 않습니다:

- 파이프라인 절반(1~6): **2개는 라이브가 수행**하고 **4개(DICOM 읽기/쓰기·강화·EI)는 어디에도 없습니다**
- QA 절반(7~10): **넷 다 없습니다** — 이것이 `QA Constancy` 항목의 내용입니다

### 3.1 그래서 이 파일을 어떻게 볼 것인가

두 가지를 갈라 적습니다. **어느 쪽을 택할지는 제 판단이 아닙니다.**

| 사실 | 함의 |
|---|---|
| 외부 호출처 **0건**, 스텁이 **hardcoded success** 를 반환 | 지금 이 파일은 **아무 일도 하지 않습니다.** 남아 있는 동안 "파이프라인 오케스트레이터가 있다" 로 읽힐 여지가 있습니다 |
| 덮이지 않는 4+4 단계 | 걷어내면 **그 단계들의 기록도 함께 사라집니다.** 그 목록이 이 표입니다 |

### 3.2 **제 `C-151` 집계를 정정합니다**

`C-151` 에서 `gui` 의 네이티브 함수를 **31개**로 보고했습니다. 그중 **7개는 이 미참조 파일에만 선언**돼 있습니다 —
`xpe_calc_exposure_index` `xpe_contrast_enhance` `xpe_dicom_read` `xpe_dicom_write` `xpe_edge_enhance` `xpe_log_transform` `xpe_noise_reduce`.

**라이브 함수는 24개**입니다. 즉 `C-151` 의 *"`gui` 는 `dicom_read`/`write` 를 쓴다"* 는 기술이 **틀렸습니다** — 선언만 있고 호출되지 않습니다.
같은 실수를 방금 이 카드에서 반복할 뻔했습니다: **선언을 세고 도달 가능성을 안 봤습니다.**

---

## 4. `QA Constancy` (SWU-6.1) — **사람 필요**. 찾아서 판정했습니다

카드가 *"추측 말고 찾으라"* 했으므로 검색 범위를 적습니다.

| 필요한 것 | 찾은 결과 | 검색 범위 |
|---|---|---|
| **기준(균일) 영상** | **없음** | 저장소 전체에서 `*flat*`·`*uniform*` 파일명 **0건**. 픽스처는 `synthetic_1024x1024.raw`·`wrist_lat_3072x3072.raw` 둘뿐이고 둘 다 균일 프레임이 아닙니다 |
| **판정 기준(SNR·균일도·결함 수 임계)** | **없음** | `docs/`·`.moai/specs/` 에서 `SNR ≥`·`uniformity ≤` 류 수치 검색 — 걸린 것은 **AI 모듈의 PSNR≥33dB·SSIM≥0.97** 뿐이고 이는 다른 요구입니다 |
| 계획 근거 | 있음 | `product.md:159`(`SUP-05`→`SWU-6.1`), `MASTER/spec.md:329`(`P1b-13`, Should), `XPE-PRD-003:163`·`:266`(`BI-02.05.02`·`BI-05.05.03`, Must), `XPE-PLAN-001:132` |

**계획은 문서 다섯 곳에 있는데 판정에 쓸 숫자와 영상이 없습니다.** 그래서 코드만으로는 만들 수 없습니다.

---

## 5. 미검증 / 이 표의 한계

- **앱을 실행하지 않았습니다.** 전부 소스·문서 읽기입니다
- **"가능" 의 근거는 "재료가 저장소에 있다" 까지**입니다. 각 항목의 설계 결정(예: 증거 번들 규격, 진단 화면 구성)은 보지 않았습니다
- 툴팁 문구를 **선행조건의 진술로 받아들였습니다.** 툴팁이 낡았을 수 있습니다 — 4번 `Native` 가 그 예로, 툴팁은 *"RealXpeBackend 와 P/Invoke 연동 후"* 라고 하는데 **그 연동은 이미 끝나 CI 가 그 경로로 돕니다**
- 21건 중 **활성 메뉴와의 중복**은 보지 않았습니다(예: 15·16 이 이미 CI 로 도는 것과 앱 내 실행의 관계)
- 10번의 선행으로 적은 `#130` 은 **리더 메시지에서 인용**한 이슈 번호이고 제가 본문을 열어 확인하지 않았습니다. 21번의 선행은 이슈가 아니라 **이 표의 행 9** 로 적었습니다 — 툴팁이 말하는 것이 그것이기 때문입니다
- `benchmark` 매니페스트는 **1건**만 확인했습니다. 17번이 요구하는 범위를 그 1건이 덮는지는 보지 않았습니다

---

Refs #165
