# TASK-GUI-IA-001: Evaluation / Calibration 탭 책임 분리 및 viewer 중심 재구성

**문서 ID**: TASK-GUI-IA-001  
**연관 이슈**: GitHub Issue #47  
**Lane**: C (GUI) — `dev/gui` 브랜치  
**대상 프로젝트**: `clients/ImageProcTest`  
**작성일**: 2026-04-20  
**상태**: 승인 대기

---

## 1. 목적

`ImageProcTest.exe`는 알고리즘 개발 결과를 사용자가 직접 검증평가하는 test GUI app이다.  
현재 Evaluation 탭은 calibration 폴더/파일 선택, raw 선택, algorithm chain 구성, before/after viewer, 사용자 판정이 한 화면에 섞여 있어 평가 화면의 초점이 흐려진다.

본 작업의 목적은 탭 책임을 다음처럼 분리하는 것이다.

- **Evaluation 탭**: 원본/적용 영상 비교, W/L viewer 조작, 핵심 run 결과, 사용자 판정에 집중
- **Calibration 탭**: calibration 폴더/파일 선택, 취득 가이드, 역할 판정, stage/module 사전 선택에 집중
- **Metrics / Reports 탭**: 정량 metric과 evidence artifact 확인에 집중
- **Diagnostics 탭**: native readiness, DLL/export 상태, smoke diagnostics에 한정

---

## 2. 교차검증 문서

| 문서 | 확인한 기준 | GUI 구조 반영 |
|------|-------------|---------------|
| `TASK-GUI-VIEWER-001` | W/L, brightness/contrast, histogram은 픽셀 불변 렌더 파라미터이며 DLL 불필요 | Evaluation viewer 내부 기능으로 유지 |
| `XPE-GUI-COMPARE-001` | 기본 비교는 하나의 동기화된 ComparisonViewport, source/processed swipe 중심 | Evaluation 탭의 주 콘텐츠는 viewer가 되어야 함 |
| `XPE-GUI-ARCH-001` | View / ViewModel / Service 경계, code-behind 최소화 | 탭별 ViewModel과 UserControl로 점진 분리 |
| `XPE-GUI-CALIB-001` | calibration verification은 폴더/파일, fixture, stage switch, BPM/algorithm compare 준비를 포함 | Calibration 탭은 입력 준비, Evaluation workbench는 Apply/Skip 실행 |
| `XPE-EVAL-001` | detector-domain metric과 사용자/관찰자 평가가 알고리즘 promotion 근거 | Evaluation 탭은 결과 해석과 사용자 verdict 중심 |
| `XPE-PRE-E2E-001` | real fixture는 raw/calibration context와 report schema를 기록 | 선택 context는 Calibration 탭에서 만들고 Evaluation은 active context만 소비 |

---

## 3. 현재 구현과의 차이

| 영역 | 현재 상태 | 문제 |
|------|-----------|------|
| Evaluation 탭 | calibration folder, cal files, target raw, algorithm chain, rule findings, viewer, verdict가 모두 포함 | 평가 화면이 사전 설정 화면처럼 보이며 viewer 집중도가 낮음 |
| Calibration 탭 | validation matrix 위주 | calibration 폴더/파일 선택, 역할 판정, 취득 가이드, stage 사전 선택의 실제 작업 위치가 아님 |
| Diagnostics 탭 | 숨겨진 preprocessing fixture 영역과 viewer 원본 XAML host가 남아 있고 runtime에서 Evaluation으로 이동 | XAML 구조와 runtime 구조가 달라 유지보수성이 낮음 |
| Viewer | 기능은 Evaluation으로 이동되지만 panel 자체는 독립 UserControl이 아님 | 문서의 `ViewportControl` / `ViewportViewModel` 방향과 불일치 |
| Help 문구 | Evaluation에서 calibration selection을 하라고 안내 | 탭 책임 분리 후 문구 갱신 필요 |

---

## 4. 목표 정보 구조

### 4.1 Evaluation 탭

Evaluation 탭은 한 화면에서 다음 정보만 강조한다.

1. Active context 요약: 선택된 calibration folder, target raw, selected chain을 작은 상태로 표시
2. Visual Review: 원본/적용 영상 비교 viewer를 탭 최상위 주 콘텐츠로 배치
3. Processing stage switches: Offset/Gain/Defect Apply/Skip, all Off는 bypass output buffer
4. Viewer controls: zoom, swipe, W/L, LUT, histogram range handles, invert
5. Run result strip: latest run status, latency, changed pixels, NaN/Inf, output range
6. User Evaluation: evaluator, verdict, notes, save evidence

Evaluation 탭에서는 calibration 파일 목록, algorithm 후보 전체 목록, fixture 탐색 목록을 노출하지 않는다. 단, 영상 확인 중 즉시 재실행해야 하는 Offset/Gain/Defect Apply/Skip 스위치는 Evaluation workbench에 둔다. 필요한 경우 `Open Calibration Setup` 명령으로 Calibration 탭으로 이동한다.

### 4.2 Calibration 탭

Calibration 탭은 실행 전 context 준비에 집중한다.

1. Calibration folder 선택/refresh
2. Calibration file role audit: Offset/Dark, Gain/Flat, Defect/BPM, Reference, Unknown
3. Target raw 선택
4. Algorithm chain builder: 후보/선택/순서 변경/프리셋/rule findings
5. Acquisition guide: IAP-CALIB-001 기준 dark/flat/BPM 취득 안내

Calibration 탭에서 선택한 상태는 `ActiveEvaluationContext` 형태로 Evaluation 탭이 소비한다.

### 4.3 Metrics / Reports / Diagnostics

- Metrics: detector-domain metrics, stage latency, calibration load latency, dark/flat/defect grids
- Reports: saved JSON/Markdown evidence 목록과 열기 동작
- Diagnostics: native common/preprocess readiness, parameter ranges, smoke 결과

---

## 5. 모듈화 방향

### 5.1 신규/분리 대상

```
clients/ImageProcTest/
├── Controls/
│   ├── EvaluationViewerPanel.xaml
│   ├── EvaluationViewerPanel.xaml.cs
│   ├── CalibrationSetupPanel.xaml
│   └── CalibrationSetupPanel.xaml.cs
├── ViewModels/
│   ├── EvaluationViewModel.cs
│   ├── CalibrationSetupViewModel.cs
│   └── ViewportViewModel.cs
├── Models/
│   └── ActiveEvaluationContext.cs
└── Services/
    └── EvaluationContextService.cs
```

### 5.2 책임 경계

| 구성요소 | 책임 |
|----------|------|
| `CalibrationSetupViewModel` | calibration folder/raw/algorithm chain 선택 상태 |
| `EvaluationContextService` | active context 저장, context readiness 판단 |
| `EvaluationViewModel` | latest run, visual review 상태, user verdict, report trigger |
| `ViewportViewModel` | W/L, LUT, histogram, zoom, swipe 상태 |
| `ViewportRenderService` | 픽셀 불변 렌더링 및 histogram 계산 |

---

## 6. 구현 계획

### Phase A — XAML 정보 구조 정리

- Evaluation 탭에서 calibration folder/file list, algorithm chain builder, rule findings를 제거
- Evaluation 탭에는 compact active context, stage Apply/Skip switches, viewer, run result, user evaluation만 유지
- Calibration 탭에 folder/raw/algorithm chain 준비 영역을 배치
- 숨겨진 Diagnostics fixture 영역에서 viewer host를 제거하고 Evaluation에 정적으로 배치

### Phase B — 상태 공유 경계 도입

- `ActiveEvaluationContext` 모델 추가
- calibration folder, target raw, selected chain, stage switch 상태를 하나의 context로 묶음
- Evaluation 탭 run 버튼은 active context readiness만 참조
- context 변경 시 Evaluation viewer는 원본/적용 결과 상태를 명확히 reset

### Phase C — viewer 모듈화

- `EvaluationViewerPanel` 또는 `ViewportControl`로 viewer XAML 분리
- `ViewportViewModel`로 W/L, LUT, histogram, histogram range, zoom, swipe 상태 이동
- MainWindow code-behind의 viewer event handler를 control 내부 또는 ViewModel command로 축소

### Phase D — 문구/Help/Report 동기화

- Help 탭 문구를 탭 책임 분리 기준으로 갱신
- evidence report에 active context summary와 viewer render params를 기록
- GitHub issue #47에 변경/검증 결과를 `codex:` prefix로 기록

---

## 7. 승인 후 검증 기준

| 검증 | 합격 기준 |
------|-----------|
| Build | `dotnet build clients\ImageProcTest\ImageProcTest.csproj -c Debug --no-restore` 통과 |
| Integration test | `dotnet test clients\ImageProcTest.IntegrationTests\ImageProcTest.IntegrationTests.csproj -c Debug --no-restore` 통과 |
| Viewer focus | Evaluation 탭 첫 화면에서 viewer가 주 콘텐츠이며 calibration 파일 목록이 보이지 않음 |
| Calibration setup | Calibration 탭에서 folder/raw/chain/stage 선택이 가능하고 active context summary가 갱신됨 |
| Run gating | calibration folder + target raw + executable chain 없이는 Evaluation run이 차단됨 |
| Pixel immutability | W/L 조작 후 raw/input buffer는 변경되지 않음 |
| Evidence | report에 active context, selected chain, stage switches, run metrics, user verdict가 기록됨 |

---

## 8. 승인 대기 항목

본 문서는 구현 계획이다. 승인 전에는 `clients/ImageProcTest` 코드 구조 변경을 시작하지 않는다.

승인되면 Phase A부터 작은 단위로 구현하고, 각 단계마다 build/test 후 앱을 실행해 사용자가 확인할 수 있게 한다.
---

## 9. Implementation Update - 2026-04-20

GitHub Issue #47 approval received. All phases were implemented in `clients/ImageProcTest`.

- Phase A: Evaluation is viewer-first; Calibration owns folder/raw/chain/stage setup.
- Phase B: `ActiveEvaluationContext` and `EvaluationContextService` gate Evaluation run readiness.
- Phase C: `EvaluationViewerPanel` and `ViewportViewModel` own zoom, swipe, W/L, LUT, invert, and histogram state.
- Phase D: Help copy and GUI report output include active context and viewer render state.

Verification:

- `dotnet build clients\ImageProcTest\ImageProcTest.csproj -c Debug --no-restore` passed.
- `dotnet test clients\ImageProcTest.IntegrationTests\ImageProcTest.IntegrationTests.csproj -c Debug --no-restore` passed: 80/80.
