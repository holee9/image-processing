[{{NOW}} KST]
# QA-A-274 1단계 — 미커밋 캘리브레이션 워크플로 통합 인벤토리 (읽기 전용)

이슈 #278. 작성: xpe-pre. 기준: HEAD `871f7fbe`, 브랜치 `dev/preprocess`. 이 문서는 stage·commit·제품 수정·빌드·제조사 DLL 실행을 하지 않고 읽기만 해서 만들었다. 인벤토리 문서와 생성 스크립트 외에 변경한 파일은 없다.

## 0. 결론 (먼저 읽을 것)

1. 대상은 `.moai/reports/**` 를 뺀 dirty 파일 **{{TOTAL}}개**(untracked 폴더를 파일 단위로 펼친 수. `git status --short` 줄 수는 82)다. 이 가운데 제품 파일은 {{PRODUCT}}개, 도구 상태 {{NX}}개와 빌드 산출물 {{NY}}개는 통합 대상이 아니다.
2. 출처는 모두 근거 파일의 해시로 갈랐다. 이 레인(273)이 만든 변경은 이미 커밋된 비교 창 메뉴뿐이고, 남은 dirty 에는 273 의 신규 hunk 가 없다. 다만 `MainWindow.xaml.cs` 에서 273 핸들러(`PreComparisonImport_Click`)가 파일 아래쪽(HEAD)이 아니라 위쪽에 있어 위치가 다르다(§4).
3. 기능 단위는 **10개**(§3)로 묶인다. 한 파일에 둘 이상의 단위가 섞인 파일은 10개이며 hunk 단위로 갈랐다(§4). `quick-start.ko.html` 의 혼합 여부는 확인하지 못했다.
4. **4개 진입점**(`PreCalibrationWorkflowMenuItem`, `PreCalibrationWorkflow_Click`, 헤더 "PRE 캘리브레이션 입력·생성·재로드·평가", `OptionalSettingsPanels`)을 HEAD 단독으로 컴파일하는 데 필요한 파일은 §6 에 정리했다. 새 C 내보내기 함수(export)는 필요하지 않다. GUI 가 부르는 네이티브 함수는 모두 HEAD 에 이미 있다.
5. **결정이 필요한 충돌 한 가지를 확인했다(§6.3, §7-1).** 워크플로 GUI 의 "증거 스냅샷" hunk 는 자동화 보고서를 고정 이름 `menu-command-report.json` 이 아니라 `evidence/snapshots/menu-command-report-<시각>-<GUID>.json` 으로 쓴다. `clients/ImageProcTest.E2ETests` (Lane C 소유, 변경 없음)의 시험 두 파일 네 곳이 고정 이름을 읽는다. 이 hunk 를 그대로 커밋하면 그 시험이 깨질 것으로 읽힌다. 실행해서 확인한 것이 아니라 소스를 읽어 확인한 것이다.
6. 검토 상태(§5): 해시 일치 {{RV_MATCH}}개 / 검토 후 변경 {{RV_CHANGED}}개 / 검토 없음 {{RV_NONE}}개 (제품 {{PRODUCT}}개 기준). 검토 없음은 §7-3 에 따로 모았다.
7. 소유권이 불명확하거나 이 레인의 경계 밖인 항목은 §7 에 모았다. `cmake/`, `docs/`, `tools/`, `.claude/` 는 레인 규칙상 리더 소유다.

## 1. 방법과 근거

| 항목 | 내용 |
|---|---|
| 현재 목록 | `git status --porcelain -uall` 에서 `.moai/reports/` 를 뺐다. 파일별 SHA-256 은 작업 트리 파일을 직접 읽어 계산했다(`collect.py`, `make_inventory.py`). |
| 이전부터 있던 dirty | `PRE-GUI-20261019T063546Z-7f92a1/baseline.json`(Codex 임시리더가 GUI 작업을 시작한 2026-10-19 06:39 UTC 의 상태: `pre_status`, 소스 해시 `sources`)와 `baseline-product.diff`. baseline 상태에 있고 baseline 해시와 지금 해시가 같으면 "이후 변경 없음"으로 적었다. |
| Codex 임시리더 GUI 변경 | `frozen-current-v2.json`(변경 14 + 새 파일 9 = 23), `final-audit.json`(40개 파일 기대 해시), `frozen-final-build7.json`. |
| Codex 임시리더 설치기(#277) 변경 | `PRE-INSTALLER-20261020-leader/**/source-1.0.11.json`. 파일 수정 시각(2026-10-20 09:40~10:22)과 `GuiDataPaths` 사용 여부를 보조 근거로 썼다. |
| 저수량(#273/#245) 보조 도구 | `PRE-LOWCOUNT-20261020-*` 폴더. |
| 검토 기록 | `image-processing/.moai/state/reviews/CODEX-TEMP-*.md` 9건(#247 은 `CODEX-TEMP-E2E-247.md`), Reviewer 기록 `PRE-INSTALLER-20261020-reviewer-01a0f0f3/review.md`, `PRE-GUI-.../build7-independent-acceptance.md`, `PRE-LOWCOUNT-20261020-reviewer-01a0f0f3/`. 문서 안의 64자리 해시를 지금 파일 해시와 대조했다. |
| 한계 | "해시 일치"는 그 해시가 검토 문서에 적혀 있다는 뜻이다. 그 문서가 무엇을 수용하고 무엇을 보류했는지는 §5.2 에 적었지만, 판정 범위를 이 문서에서 다시 심사하지는 않았다. |

## 2. 파일별 표

열: 상태(M 수정 / ?? 새 파일 / D 삭제), 경로, 기능 단위(§3), 출처, 검토 상태. 빌드 산출물 {{NY}}개는 한 줄로 합쳤다(목록은 `inventory-rows.json`).

{{TABLE}}

## 3. 기능 단위

| 코드 | 단위 | 포함 파일 | 출처 |
|---|---|---|---|
| A1 | MSVC Ninja 의 include 접두사·헤더 의존 검사 | `cmake/Platform.cmake`(+2줄), `cmake/MsvcNinjaIncludePrefix.cmake`, `tools/ci/msvc_include_output.py`, `tools/ci/test_msvc_include_output.py`, `tools/ci/verify_msvc_header_dependencies.py` | 이전부터 있던 dirty |
| A2 | XCal 저장 상태: offset/gain 의 config 바이트 보존, defect v2 저장, 압축 메타 | `src/xcal_reader.*`, `src/calibration_cache.cpp`, `src/xpe_calib_load_gain.cpp`, `src/xpe_calib_load_offset.cpp`, `src/xpe_calib_save.cpp`, `tests/test_xpe_calib_save.cpp`, 그리고 §4 의 공유 파일 hunk | 이전부터 있던 dirty |
| A3 | Ghost 첫 프레임 우회와 이력 메모리 축소(72 MiB) | `src/ghost_correct.cpp`, `src/pipeline.cpp`, `tests/test_ghost_mix_compare.cpp`, `tests/test_ghost_oracle_derive.cpp`, `tests/test_ghost_tiers.cpp`, `tests/test_pipeline_stage_values.cpp`, `tests/test_output_unchanged_237b.cpp`, 그리고 §4 의 공유 파일 hunk | 이전부터 있던 dirty |
| A4 | PRE E2E 도구 `xpe_pre_e2e`, 결함 지표, 줄무늬(stripe) 지표, 시험 증거(#245/#247) | `tools/xpe_pre_e2e.cpp/.hpp`, `tools/xpe_pre_e2e_report.hpp`, `tools/xpe_pre_defect_metrics.hpp`, `tools/xpe_pre_stripe.hpp`, `tools/xpe_pre_test_evidence.py`, `tests/test_pre_e2e.cpp`, `tests/test_pre_test_evidence.py`, `CMakeLists.txt` hunk 4개, 평가 프로토콜 문서 | 이전부터 있던 dirty |
| A5 | 합성 지표 기록(`RecordProperty("xpe_synthetic_metrics_v1")`) | `tests/test_ghost_oracle.cpp`, `tests/test_nonlin_lut_apply.cpp`, `tests/test_nonlin_lut_generate.cpp`, `tests/test_nonlin_poly_apply.cpp` | 이전부터 있던 dirty |
| A6 | 캘리브레이션 사양·절차 문서 | `docs/calibration/` 4개, `docs/help/content/index.md`, `docs/help/content/dark-quality-metrics.md`, `docs/project/api-spec.md`(DOC-06 hunk), 평가 프로토콜 문서 | 이전부터 있던 dirty |
| B1 | 캘리브레이션 워크플로 GUI(입력·생성·재로드·평가) | 새 파일 9개(`Views/CalibrationWorkflowWindow.cs`, `Services/CalibrationWorkflowService.cs`, `Services/CalibrationStageSelection.cs`, `Services/EvidenceSnapshotStore.cs`, `Services/NativeRuntimeEvidence.cs`, `Services/Native/CalibrationSetInspector.cs`, `ImageProcTest.CalibrationChecks/` 2개) + 수정 파일 hunk(§4) + `modules/preprocess/tools/xpe_calib_fixture_gen.cpp` 의 `--validate-only` hunk | Codex 임시리더 GUI 변경 |
| B6 | 워크플로 사용 문서 | `docs/development/PRE-GUI-CALIBRATION-WORKFLOW.md`, `...-DOC-SUPPLEMENT.md` | Codex 임시리더 |
| C1 | 설치기와 사용자 데이터 루트(#277) | `Services/GuiDataPaths.cs`, `Services/AppSettingsService.cs`, 뷰모델·`App.xaml.cs` 의 일부 hunk, `help/quick-start*.html`, `tools/packaging/pre-gui/**` | Codex 임시리더 설치기 |
| D1 | 저수량 신호 측정 보조 도구 | `tools/calibration/measure_signal_stack.py`, `tools/calibration/test_measure_signal_stack.py` | Codex 임시리더 저수량 |
| Q | 273 비교 창 메뉴 | 이미 HEAD 에 커밋됨(`b4c690b3`). 남은 것은 `MainWindow.xaml.cs` 핸들러 위치 차이뿐(§4) | 이 레인 |

단위 밖: 도구 상태 {{NX}}개(`.claude/*`, `.moai/harness`, `.moai/lessons-inbox.jsonl`, `modules/preprocess/**/.moai/state/*`), 빌드 산출물 {{NY}}개(`tools/packaging/pre-gui/InstallHost/obj/**`), 출처 불명 1개(`docs/superpowers/plans/2026-10-20-pre-comparison-workbench.md`).

## 4. 한 파일에 여러 단위가 섞인 경우 — hunk 표

hunk 는 `git diff HEAD -U0` 의 변경 덩어리다. 줄 번호는 지금 작업 트리 기준이다.

| 파일 | hunk (줄) | 내용 | 단위 |
|---|---|---|---|
| `gui/ImageProcTest/MainWindow.xaml` | −72..−83 | 옛 진단·캘리브레이션·표시 패널 3개를 따로 배치한 부분 삭제 | B1 (OptionalSettingsPanels 와 한 쌍) |
| | +408..+410 | 메뉴 항목 `PreCalibrationWorkflowMenuItem` (헤더 "PRE 캘리브레이션 입력·생성·재로드·평가") | B1 |
| | +512..+521 | 상태 줄 `WrapPanel`·툴팁·최대 폭 | B1 (표시) |
| | +619..+637 | `OptionalSettingsPanels` 스크롤 열(패널 3개를 한 열에 모음) | B1 (OptionalSettingsPanels) |
| | (HEAD 와 같음) | 273 의 `PreComparisonImportMenuItem` 은 이미 HEAD 에 있다 | Q |
| `gui/ImageProcTest/MainWindow.xaml.cs` | +21..+25 | `PreCalibrationWorkflow_Click` — `Views.CalibrationWorkflowWindow(model)` 을 연다 | B1 |
| | +27..+29 (그리고 HEAD 기준 −842..−844) | 273 핸들러 `PreComparisonImport_Click` 이 파일 아래에서 위로 **옮겨졌다**. 내용은 같고 위치만 다르다 | Q (정리 필요) |
| | +667..+676 | 자동화 보고서 경로를 `viewModel.LastEvidenceSnapshotPath` 로 바꿈 | B1 (증거 스냅샷) |
| `gui/ImageProcTest/ViewModels/MainWindowViewModel.cs` | 1032 | 단계 수 표기 `ExecutedFromReason(...)` ("N native stages") | B1 |
| | 1516 | `RuntimeInfo.CommonVersion` | B1 |
| | 2563, 3085, 3678, 3952, 4017, 4025 | `AppContext.BaseDirectory` → `GuiDataPaths.Root` (증거 폴더) | C1 |
| | 2594..2700 | 자동화 보고서: `provenance = NativeRuntimeEvidence.Capture()`, 고정 파일 대신 `EvidenceSnapshotStore.Write(GuiDataPaths.Root, report)`, `LastEvidenceSnapshotPath` 속성 | B1 **과 C1 이 한 줄에 얽힘**(`GuiDataPaths.Root` 를 인수로 씀) |
| `gui/ImageProcTest/App.xaml.cs` | +65..+67 | `XPE_GUI_RENDER_MODE=SoftwareOnly` 렌더 모드(#277) | C1 |
| | +72..+78 | `--inspect-calibration-set` 인수 처리 → `CalibrationSetInspector.Execute` | B1 |
| `gui/ImageProcTest/Services/AppSettingsService.cs` | 45, 224..247 | 설정 파일 위치 `GuiDataPaths.Root`, 새 스키마 거부, 임시 파일 후 교체 저장 | C1 (전부 #277) |
| `modules/preprocess/CMakeLists.txt` | +115..+132 | `xpe_pre_e2e` 실행 파일 타깃 | A4 |
| | +477 | 시험 목록에 `tests/test_pre_e2e.cpp` | A4 |
| | +572 | 시험 실행 파일에 `tools/xpe_pre_e2e.cpp` | A4 |
| | +617..+620 | `add_dependencies(xpe_preprocess_tests xpe_pre_e2e)` 와 `xpe_pre_e2e_path.h` 생성 | A4 |
| | +677..+681 | `preprocess.legacy_ghost_fingerprints` 시험 | A3 |
| `modules/preprocess/include/xpe/preprocess_api.h` | +718..+725 | 저장 의미 주석(offset/gain config, v2 defect) | A2 |
| | 834..890 | Ghost 첫 프레임·메모리 주석 | A3 |
| `modules/preprocess/include/xpe/preprocess/xpe_preprocess_internal.h` | +53..+80, +169..+172, +356..+357 | `hasHistory`, `xpe_ghost_correct_with_status`, 시험 전용 `xpe_ghost_legacy_first_frame` | A3 |
| | +550..+575, +712..+721 | `offset_config`/`gain_config`(공유 포인터) | A2 |
| `modules/preprocess/tests/test_oom_injection.cpp` (+439줄) | 파일 안 | offset/gain config 의 OOM 시험(A2)과 ghost 이력 시험(A3)이 한 파일에 있다 | A2+A3 (hunk 분리 가능 여부 미확인) |
| `modules/preprocess/tests/test_xpe_preprocess_calibration.cpp` (+425줄) | 파일 안 | `read_xcal_file` config 보존 시험(A2)과 ghost 호출 시험(A3) | A2+A3 (hunk 분리 가능 여부 미확인) |
| `docs/project/api-spec.md` | 4..6 | 버전 1.4.1, DOC-06 개정 문구 | A6 |
| | 15..16, 34, 224, 231 | `xpe_preprocess` 선언 수 49 → 56, 통합 재집계 보류 표기 | A6 (56 은 "작업 트리 공개 헤더 기준"이라 적혀 있다 — §8 미검증) |
| | 836 | `xpe_verify_offset_signed(_file)` 를 예외 경계 목록에 추가 | A6 — **이 레인의 QA-A-266 이 이미 커밋한 API 의 문서** |
| | 901..953 | 새 절 6.22 `xpe_verify_offset_signed` — 같은 이유로 이 레인 API 의 문서 | A6 — 위와 같음 |
| `gui/ImageProcTest/help/quick-start.ko.html` | 파일 안 | 설치기 안내(C1)와 워크플로 안내(B1)가 섞였는지 **미확인** | C1 (b7 해시와 달라진 것은 확인) |

## 5. 기존 Codex 검토와 지금 해시의 대조

### 5.1 상태별 요약 (제품 {{PRODUCT}}개)

| 상태 | 개수 | 뜻 |
|---|---|---|
| 검토됨-해시 일치 | {{RV_MATCH}} | 검토 문서(또는 Reviewer 수용 기록)에 지금 파일 해시가 적혀 있다 |
| 검토됨-이후 변경 | {{RV_CHANGED}} | 검토한 해시와 지금 해시가 다르다 |
| 검토 없음 | {{RV_NONE}} | 어떤 검토 기록에도 이 파일의 해시나 경로가 없다 |

### 5.2 검토 문서별 판정 범위 (원문에서 읽은 문장의 요지)

| 검토 문서 | 덮는 단위 | 판정 범위 (원문에서 읽은 요지) |
|---|---|---|
| `CODEX-TEMP-FIRSTFRAME-FINAL.md` | A3 | 첫 프레임 우회 구현의 해시 고정본. 문서 안에 과거 실행 XML 의 63중 1, 64중 1 실패 기록이 있다(그 뒤 해소는 이 문서 뒷부분을 확인하지 않았다). |
| `CODEX-TEMP-GHOST-MEMORY.md` | A3 | 이력 메모리 축소를 해당 범위에서 수용. 모든 오류 우선순위가 불변이라는 주장은 수용하지 않음. |
| `CODEX-TEMP-OFFSET-CONFIG-FINAL.md` | A2 | offset config 보존을 수용한 뒤 "압축 offset 의 config/payload 불일치 [P1]" 로 수용 범위를 정정했다. |
| `CODEX-TEMP-SAVE-STATE-FINAL.md` | A2, A4, A1 | 50개 해시 고정본을 읽기 수용하고 새 차단 결함 없음. 원래 저장 범위 종료와 후속 부분 구현은 구분해 유지. DOC-06 변경표는 "GUI253c 확인 필요·SDD/VVP 미완료"를 그대로 기록. |
| `CODEX-TEMP-E2E-247.md` | A4 | 고정 해시 수정본에서 P1 지적 해소를 수용. 최종 회귀와 실영상 품질은 별도. |
| `CODEX-TEMP-STRIPE.md` | A4 | stripe 계산·flat gate 연결에서 새 차단점 없음. 실영상 stripe 품질은 별도. |
| `CODEX-TEMP-FIRSTFRAME.md`, `-real-image-requirements.md`, `-DECISIONS.md` | A6 | 계약·요구 해석 문서. 파일 해시를 고정하지 않았다(해시 0건 또는 소수). |
| Reviewer build6/build7 독립 수용(`build7-independent-acceptance.md`, `final-audit.json`) | B1 | 최상위 점검 4개 PASS, 보호 파일 110/110 일치. 기능 재검증과 좁은 UI 영향 수용이며 모든 모니터·장시간 그리기·실영상 품질 승인이 아님. |
| Reviewer 설치 검토(`PRE-INSTALLER-...-reviewer-01a0f0f3/review.md`) | C1 | `source-1.0.11.json` 의 제품 소스 8개가 설치 후보와 동일함을 확인. 다른 PC 준비 상태는 검증하지 않음. |
| Reviewer 저수량 검토(`signal-helper-review.md`) | D1 | `measure_signal_stack.py` 해시 수용. |

### 5.3 검토 없음 목록

`tests/test_ghost_oracle.cpp`, `tests/test_nonlin_lut_apply.cpp`, `tests/test_nonlin_lut_generate.cpp`(A5), `docs/calibration/ALGORITHM-VERIFICATION-GUIDE.md`, `docs/help/content/index.md`, `docs/help/content/dark-quality-metrics.md`(A6), `tools/ci/test_msvc_include_output.py`, `tools/ci/verify_msvc_header_dependencies.py`(A1), `tools/calibration/test_measure_signal_stack.py`(D1), `tools/packaging/pre-gui/SettingsChecks/**`(C1), `gui/ImageProcTest/help/quick-start.html`(C1; 설치기 안내 문서 수용표 `INSTALLER-DOC-.../research-and-acceptance.md` 에 해시는 있으나 Reviewer 가 아니라 Doc 세션의 기록), `docs/development/PRE-GUI-CALIBRATION-WORKFLOW-DOC-SUPPLEMENT.md`(B6), `docs/superpowers/plans/2026-10-20-pre-comparison-workbench.md`(불명).

## 6. HEAD 단독 빌드를 위한 의존성 닫힘 (4개 진입점)

방법: `CalibrationWorkflowWindow` 에서 출발해 소스를 읽고 참조를 따라갔다. 빌드는 하지 않았다(이번 단계 금지). "필수"는 컴파일에 필요, "실행 시"는 컴파일은 되지만 해당 기능을 쓸 때 필요, "선택"은 없어도 4개 진입점이 동작한다.

### 6.1 진입점별

| 진입점 | 위치 | 필요한 것 | 구분 |
|---|---|---|---|
| `PreCalibrationWorkflowMenuItem` + 헤더 "PRE 캘리브레이션 입력·생성·재로드·평가" | `MainWindow.xaml` +408..+410 | 같은 파일의 클릭 연결 → 아래 `PreCalibrationWorkflow_Click` | 필수 |
| `PreCalibrationWorkflow_Click` | `MainWindow.xaml.cs` +21..+25 | `Views/CalibrationWorkflowWindow.cs`. 그 창이 쓰는 뷰모델 멤버(`Settings`, `IsBackendTransitioning`, `IsMockBackend`)와 설정 속성(`OffsetCorrectionMode` 등)은 **HEAD 에 이미 있음**(확인함) | 필수 |
| `CalibrationWorkflowWindow` 의 참조 | `Services/CalibrationWorkflowService.cs`, `Services/NativeRuntimeEvidence.cs` | 창이 직접 호출한다 | 필수 |
| `OptionalSettingsPanels` | `MainWindow.xaml` −72..−83 / +619..+637 | HEAD 의 `PipelineDiagnosticsPanel`, `CalibrationPathsPanel`, `DisplaySettingsPanel` 만 쓴다. 워크플로 창과 독립이므로 따로 커밋 가능 | 필수(이 진입점 자체), 워크플로 창과는 독립 |
| 프로젝트 파일 | `gui/ImageProcTest/ImageProcTest.csproj` | 변경 없음. 새 `.cs` 는 기본 포함 규칙으로 들어간다(`Compile Remove` 가 있는 SDK 형식) | 해당 없음 |

### 6.2 단위 B1 을 같이 넣어야 하는 파일

| 파일 | 이유 | 구분 |
|---|---|---|
| `Services/CalibrationStageSelection.cs` (새) | `GuiPreprocessRunner` 와 뷰모델 hunk 가 `ShouldLoad`, `ExecutedFromReason` 을 부른다 | 필수 |
| `Services/Native/GuiPreprocessRunner.cs` (수정) | 단계 선택(Off/Auto/On)·bypass 설정·실제 적용 단계 표시. 파이프라인 설정 키 `bypassTemp/Nonlinearity/Binning/Gain/Defect/Ghost` 는 HEAD `pipeline.cpp` 에 있음(확인함) | 필수 |
| `Services/RealXpeBackend.cs`, `Models/BackendRuntimeInfo.cs`, `Models/RenderRecord.cs`, `Services/Native/XpeDisplayInterop.cs` | `CommonVersion`/`xpe_version`. `xpe_version` 은 HEAD `modules/common` 이 이미 내보냄(확인함) | 필수 |
| `Services/EvidenceSnapshotStore.cs` (새) | 뷰모델의 증거 스냅샷 hunk 가 부른다. `NativeRuntimeEvidence.Capture()` 도 같은 hunk | 필수 |
| `Services/Native/CalibrationSetInspector.cs` (새) + `App.xaml.cs` +72..+78 | 명령줄 `--inspect-calibration-set` | 선택(없어도 4개 진입점은 열린다. 점검 도구 `ImageProcTest.CalibrationChecks` 가 쓴다) |
| `ImageProcTest.CalibrationChecks/` 2개 (새 프로젝트) | `gui/ImageProcTest.slnx` 에 **아직 등록되지 않았다**(slnx 는 변경 없음). 단독 `dotnet build <csproj>` 로만 빌드된다 | 선택(회귀 시험용) |
| `ImageProcTest.E2E/Program.cs`, `ImageProcTest.SelfCheck/LifetimeScenarios.cs` | "3 native stages" 문구와 `LastEvidenceSnapshotPath` 기대값을 새 동작에 맞춘 시험 갱신. B1 을 넣으면 같이 넣어야 기존 시험이 맞는다 | 필수(B1 을 넣을 때) |
| `Views/AnalysisPanel.xaml` | "기본 3단계 연결" 문구와 비활성 범위 변경 | 필수(B1 의 표시 변경; 컴파일에는 불필요) |

### 6.3 네이티브·도구·타 레인 의존성

| 항목 | 내용 | 구분 |
|---|---|---|
| 네이티브 export | GUI 가 부르는 것: `xpe_preprocess_init/shutdown`, `xpe_calib_load_offset/gain/defect_map`, `xpe_preprocess_pipeline_out`, `xpe_version/alloc_image/free_image`. **모두 HEAD 에 있다.** 미커밋 `modules/preprocess` 에서 새 export 를 부르는 곳은 없다(`preprocess_api.h` 의 dirty 변경은 주석뿐) | 새 export 없음 |
| `xpe_calib_fixture_gen.exe --validate-only` | 워크플로의 "검증" 단계. `modules/preprocess/tools/xpe_calib_fixture_gen.cpp` 의 +14줄(B1 단일 hunk). HEAD 의 도구는 이 인수를 모른다 | 실행 시 필수. 이 레인(modules/preprocess) 경계 안 |
| `xpe_pre_e2e.exe` | 워크플로의 "평가" 단계. HEAD 에 **없는 실행 파일**이다. 만들려면 A4 전체(CMake 타깃, `tools/xpe_pre_e2e*.hpp/.cpp`, `xpe_pre_defect_metrics.hpp`, `xpe_pre_stripe.hpp`)가 필요하고, 그 소스가 `xpe_preprocess_internal.h`·`xcal_reader.hpp` 의 dirty 변경(A2/A3)을 `#include` 한다 | 실행 시 필수(평가 단계). 컴파일에는 불필요 |
| `ImageProcTest.csproj`·솔루션 | 변경 없음 | 해당 없음 |
| **`clients/ImageProcTest.E2ETests`** (Lane C 소유) | `Scenarios/Workflows/WorkflowMenuScenarios.cs`(W-09) 와 `GsvgWristSliceScenarios.cs` 가 `<실행 파일 폴더>/menu-command-report.json` 을 읽는다. B1 의 증거 스냅샷 hunk 는 이 고정 파일을 더 쓰지 않는다. 소스를 읽어 확인했고 실행하지는 않았다 | **충돌**. 결정 필요(§7-1) |
| 사용자 데이터 루트 | 뷰모델 hunk 2594..2700 이 `GuiDataPaths.Root` 를 쓴다. B1 만 단독으로 HEAD 에 올리려면 `GuiDataPaths.cs`(C1)를 같이 넣거나 그 hunk 의 인수를 `AppContext.BaseDirectory` 로 되돌려야 한다 | 필수(B1 단독 시) |

### 6.4 단위 사이 선후 관계 (요약)

- A3 와 A2 는 `xpe_preprocess_internal.h`, `preprocess_api.h`, 시험 파일 4개를 공유한다.
- A4 는 A2/A3 의 헤더 변경 위에서만 컴파일된다.
- B1 은 C1(`GuiDataPaths`) 위에서만 컴파일된다(§6.3 마지막 줄). 반대로 C1 은 B1 없이도 컴파일된다.
- B1 의 "평가" 단계는 A4 가 만든 실행 파일이 있어야 동작한다.
- 문서 단위(A6, B6)는 코드 단위 뒤에 둔다.

## 7. 소유권이 불명확하거나 경계 밖인 항목

| # | 항목 | 문제와 근거 | 필요한 결정 |
|---|---|---|---|
| 1 | `clients/ImageProcTest.E2ETests/**` 와 B1 의 증거 스냅샷 hunk | B1 이 고정 이름 보고서를 없애면 Lane C 소유 시험 4곳이 읽는 파일이 사라진다(§6.3). 파일 쪽은 Codex 임시리더 변경, 시험 쪽은 Lane C | (a) 이 hunk 를 B1 에서 빼 고정 파일도 계속 쓰게 한다 (b) 시험을 Lane C 가 새 경로 규약으로 갱신한다 (c) 둘 다 쓴다. 리더 판정 필요 |
| 2 | 리더 소유 경로를 건드리는 변경 | `cmake/Platform.cmake`, `cmake/MsvcNinjaIncludePrefix.cmake`, `docs/**` 11개, `tools/ci/*`, `tools/packaging/**`, `tools/calibration/*`, `.claude/*`. 레인 규칙(`.claude/rules/xpe/lane-session-rules.md`, `CLAUDE.local.md`)상 `cmake/`·`docs/`·`.moai/` 는 리더(main) 소유이고 `tools/` 도 리더 소유로 처리해 왔다. 이 레인 경계는 `modules/common`, `modules/preprocess` 와 그 시험이다 | 리더가 직접 커밋하거나, 이 레인이 커밋하도록 위임을 명시 |
| 3 | 검토 없음 {{RV_NONE}}개 | §5.3. 특히 A5 의 합성 지표 시험 3개는 `test_pre_test_evidence.py`(검토됨)가 소비하는 쪽이다 | 재검토 필요 여부 |
| 4 | 출처 불명 1개 | `docs/superpowers/plans/2026-10-20-pre-comparison-workbench.md` — 2026-10-20 22:37 수정, 어느 baseline·freeze·검토 기록에도 해시가 없고(이 레인의 273 기준선 목록에만 있다), 본문은 "Task 2 GUI 재설계는 사용자 검토가 필요"라는 2G 비교 작업대 계획이다. 273 의 범위(비교 창)와는 다르다 | 작성 세션 확인, 포함/제외 |
| 5 | 검토 후 변경된 파일 9개 | `docs/calibration/IAP`, `RTM`, `SRS`, `docs/project/Preprocessing-E2E-...Protocol.md`, `docs/project/api-spec.md`, `MainWindow.xaml(.cs)`, `quick-start.ko.html`, `PRE-GUI-CALIBRATION-WORKFLOW.md` | 변경분만 재검토할지 |
| 6 | `api-spec.md` 의 6.22 절과 개수 | 이 레인의 QA-A-266 이 커밋한 `xpe_verify_offset_signed` 를 설명하는데도 문서만 미커밋으로 남았고, 같은 파일에 "작업 트리 기준 56" 개수가 섞여 있다 | 이 문서 hunk 를 코드 단위와 따로 커밋할지 |
| 7 | 빌드 산출물 26개 | `tools/packaging/pre-gui/InstallHost/obj/**` 는 untracked 이고 무시되지 않았다. `.gitignore` 는 리더 소유 | 무시 규칙 추가 또는 제외 |
| 8 | 데이터 루트 도우미가 둘 | 이 레인의 `ComparisonPaths.DataRoot`(커밋됨)와 `GuiDataPaths.Root`(미커밋)가 같은 환경 변수 `XPE_GUI_DATA_DIR` 를 각자 읽는다 | C1 이 들어간 뒤 하나로 합칠지(이 레인의 후속 작업이며 지금은 하지 않음) |
| 9 | 273 핸들러 위치 | `PreComparisonImport_Click` 이 `MainWindow.xaml.cs` 위쪽으로 옮겨져 있다(§4) | 커밋할 때 이동분을 포함하지 않도록 hunk 에서 뺀다 |

## 8. 제안 커밋 단위와 순서

모든 단위는 hunk 를 명시해 index 에만 올리고(`git apply --cached`), 올린 뒤 HEAD 단독 빌드·회귀를 돌린다. "HEAD 단독 빌드"는 이번 단계에서 **실행하지 않은 정적 판단**이다.

| 순서 | 단위 | 포함 hunk·파일 | HEAD 단독 빌드 | 필요한 회귀 시험 | 필요한 검토 |
|---|---|---|---|---|---|
| 0 | 제외 | X, Y(§3 단위 밖) | 해당 없음 | 없음 | 무시 규칙은 리더 결정(§7-7) |
| 1 | A1 MSVC include | A1 파일 5개 | 가능(`Platform.cmake` 가 새 `.cmake` 를 포함하므로 둘이 한 커밋) | CMake 설정(MSVC Ninja), `tools/ci/test_msvc_include_output.py` | 5개 중 3개 수용됨, 시험 2개 재검토 |
| 2 | A2 + A3 preprocess 기반 | A2·A3 파일, §4 의 헤더 hunk 전부, 시험 4개 공유 파일, `CMakeLists` +677..+681 | 가능(두 단위가 `internal.h`·시험 파일을 공유하므로 한 커밋을 제안. 나누려면 헤더 hunk 와 `test_oom_injection`·`test_xpe_preprocess_calibration` hunk 를 쪼개야 하고 쪼갠 각 커밋의 빌드를 따로 확인해야 한다) | `xpe_preprocess_tests` 전체, 특히 ghost·저장 시험, `preprocess.legacy_ghost_fingerprints` (CI 의 preprocess 잡) | 수용됨(해시 일치). 단 `OFFSET-CONFIG-FINAL` 의 압축 offset P1 정정이 `SAVE-STATE-FINAL` 고정본에서 닫혔는지 리더가 확인 |
| 3 | A5 합성 지표 | A5 파일 4개 | 가능해 보이나 확인 필요(시험 4개가 `nlohmann/json.hpp` 를 새로 포함한다. HEAD `CMakeLists.txt` 에 `nlohmann_json` 링크가 3곳(75, 636, 673행) 있으나 `xpe_preprocess_tests` 대상에 닿는지는 확인하지 못했다) | 해당 시험 4개 | 재검토 필요(검토 없음 3개) |
| 4 | A4 PRE E2E | A4 파일, `CMakeLists` +115..+132, +477, +572, +617..+620 | 순서 2 뒤에만 가능 | `xpe_pre_e2e` 빌드, `test_pre_e2e`, `test_pre_test_evidence.py` | 수용됨. 평가 프로토콜 문서는 변경분 재검토 |
| 5 | A6 문서 | `docs/calibration` 4, `docs/help` 2, `api-spec.md`(§4 hunk 를 코드 관련 / 6.22 로 나눔), 평가 프로토콜 | 문서만(빌드 영향 없음; 도움말 `docs/help` 는 CI 문서 점검 대상인지 확인) | 문서 점검(줄 끝 공백 등 `Test-TrackedTextFiles.ps1`) | 재검토 필요(§7-5) |
| 6 | C1a 데이터 루트(GUI) | `GuiDataPaths.cs`, `AppSettingsService.cs`, 뷰모델 hunk 6곳(2563…4025), `App.xaml.cs` +65..+67 | 가능 | `ImageProcTest.SelfCheck`(설정 저장·스키마 시험), E2E | 수용됨(설치 검토 해시 일치) — 단 뷰모델·`App.xaml.cs` 는 파일 해시가 B1 hunk 를 포함한 값이라 hunk 별 수용은 아님 |
| 7 | B1 워크플로 GUI | B1 새 파일 9, 수정 파일 hunk(§4 의 B1 행), `xpe_calib_fixture_gen.cpp` +14, 시험 갱신 2개 | 순서 6 뒤에 가능. 증거 스냅샷 hunk 는 §7-1 결정에 따름 | `ImageProcTest.SelfCheck` 30개, `ImageProcTest.CalibrationChecks`, E2E, 그리고 `clients/ImageProcTest.E2ETests` 의 W-09 계열 | build6/7 독립 수용(해시 일치). 273 핸들러 이동분은 제외 |
| 8 | C1b 설치기 | `tools/packaging/pre-gui/**`(산출물 제외), `quick-start*.html` | GUI 빌드와 무관(별도 프로젝트) | `SettingsChecks`, 설치 후보 점검 | Reviewer 설치 검토 수용(`SettingsChecks` 는 검토 없음) |
| 9 | B6 워크플로 문서 | `PRE-GUI-CALIBRATION-WORKFLOW.md`, `...-DOC-SUPPLEMENT.md` | 문서만 | 문서 점검 | 재검토 필요 |
| 10 | D1 저수량 도구 | `tools/calibration/*` | 독립 | `test_measure_signal_stack.py` | 도구 수용됨, 시험 검토 없음 |
| — | 보류 | `docs/superpowers/plans/...` | 해당 없음 | 없음 | 출처 확인 후 결정(§7-4) |

## 9. 미검증과 한계

- 빌드·회귀를 실행하지 않았다. "HEAD 단독 빌드 가능"은 소스 읽기로 한 정적 판단이다. 실제로 확인하려면 단위마다 `git archive HEAD` 에 해당 hunk 만 덮어 빌드해야 한다(2단계 작업).
- hunk 의 단위 배정은 diff 내용, 파일 수정 시각, 근거 JSON 의 해시로 했다. 작성 세션을 직접 관측한 것이 아니다. `MainWindowViewModel.cs`·`App.xaml.cs` 는 해시 이력상 b7 이후 설치기 작업이 겹쳤고, 위 hunk 배정은 내용 읽기에 의존한다.
- `quick-start.ko.html` 의 B1/C1 혼합 여부는 hunk 별로 읽지 않았다.
- 검토 문서의 "해시 일치"는 해시가 적혀 있음을 뜻한다. 문서별 판정 범위 표(§5.2)는 원문 일부를 읽은 요약이며 전문을 다시 심사하지 않았다.
- `api-spec.md` 의 선언 수 56 이 HEAD 공개 헤더와 맞는지 세어 보지 않았다.
- `clients/` 시험과 증거 스냅샷 hunk 의 충돌은 소스를 읽은 결론이다. 시험을 실행하지 않았다.
- 보호 GUI(PID 1868·17364·19184)는 건드리지 않았고 프로세스를 새로 띄우지 않았다. dirty 파일은 읽기만 했다.

## 10. 재현 방법

```
git status --porcelain -uall                      # 현재 목록
python -X utf8 .moai/reports/lane-pre/QA-A-274/collect.py       # 출처 해시 대조
python -X utf8 .moai/reports/lane-pre/QA-A-274/make_inventory.py  # 단위·출처·검토 상태
python -X utf8 .moai/reports/lane-pre/QA-A-274/render_inventory_md.py        # 이 문서의 표 생성
```

저장소 규칙상 `.py` 는 무시되므로 커밋된 사본은 `*.py.txt`(`render_inventory_md.py.txt` 가 `build_md.py` 의 사본)이다. 같은 이름의 `.py` 로 되돌려 쓰면 위 명령이 그대로 돈다.
