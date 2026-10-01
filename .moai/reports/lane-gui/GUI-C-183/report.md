# GUI-C-183 — 옛 검증 앱(`clients/ImageProcTest`)의 결함 지표 그리드에 새 문구가 실제로 그려지는가

레인: gui · 이슈: `#218`, `#225` · 코드 변경 없음(보고서·증거만). 푸시 없음.

## 결론

**관측했다 — 단, 경로가 둘로 갈린다.**

1. **앱이 스스로 채운 그리드(시작 직후)**: `not computed` 가 실제로 그려진다. 관측.
2. **새 계산이 만든 행(`REPORTED`, `reported, no gate`, `not computed / defect stage not executed`)**: 앱의 **진짜 그리드**가 그것을 그리는 것을 관측했다. 단 그 행들은 **앱의 계산 경로가 만든 것이 아니라**, 앱 자신의 `DefectMetricRows.Build` 에 작은 합성 배열(5×5)을 넣어 만든 행을 그리드의 `ItemsSource` 에 직접 넣은 것이다.
3. **앱의 계산 경로(고정 영상 → 네이티브 보정 → `MetricsComputationService` → 그리드)로 이 행들이 나오는 것은 관측하지 못했다.** 이유: 그 경로는 실제 입력 영상 + 네이티브 DLL 이 필요하고(로컬 금지), 저장소에 이 앱을 구동하는 자동화도 없다(§3).

## 1. 무엇을 어떻게 봤나

저장소 밖의 임시 도구(소스를 `harness_Program.cs.txt`, `harness_csproj.txt` 로 보관)가 **진짜 앱** `clients/ImageProcTest`(Debug 빌드, 이번 카드 변경 반영본)를 같은 프로세스에서 띄운다: `App` 을 만들어 `Run()` → 앱의 `Window_Loaded` 가 끝나길 기다림 → 그리드가 든 탭('Metrics')을 선택 → 그리드의 **시각 트리에서 `DataGridCell → TextBlock.Text`** 를 읽는다(화면에 그려진 글자). 키보드·마우스 입력은 쓰지 않았다(터미널로 입력이 새는 사고 기록 때문, UI 요소를 코드로 선택). 실행 후 남은 프로세스 0개를 확인했다.

| 단계 | 그리드에 들어간 것 | 그려진 글자(`observation.txt` 그대로) |
|---|---|---|
| A 시작 직후, 아무것도 넣지 않음 | 앱이 스스로 `MetricsComputationService.Empty("native calibration preview not run")` | 네 행 모두 `not computed \| native calibration preview not run \| N/A` |
| B 합성 5×5, 결함 단계 실행 | `DefectMetricRows.Build(..)` 의 행 | `DefectRecall 100 % >= 100% PASS` · `DefectFPR 0 % < 0.001% PASS` · **`DefectResidualADU 300 ADU reported, no gate REPORTED`** · `GoodPixelDeltaP99 0 ADU <= 1 ADU PASS` |
| C 합성, 예측 지도 없음 + 결함 단계 미실행 | 같은 함수 | `DefectRecall/FPR: not computed \| predicted BPM not selected \| N/A` · `DefectResidualADU 300 ADU reported, no gate REPORTED` · **`GoodPixelDeltaP99 not computed \| defect stage not executed \| N/A`** |

네 열 머리글은 `Metric | Value | Gate | Status`, 각 행 셀 4개, 행은 모두 보이는 상태였다. 잔여 300 ADU 는 입력한 합성 어긋남(400 − 이웃 100)과 같다.

## 2. 이 관측이 증명하는 것과 하지 못하는 것

- 증명: 그리드의 **열 바인딩과 템플릿**이 새 문자열(`REPORTED`, `reported, no gate`, `not computed`, 새 게이트 문구 `>= 100%`, `< 0.001%`, `<= 1 ADU`)을 잘림·빈칸 없이 그린다. 시작 직후의 `not computed` 는 앱 자체 경로로 관측했다.
- **증명하지 못함**: (a) 실제 계산이 이 행들을 그리드까지 가져가는 연결(`NativePreprocessPreviewService` → `Compute` → 그리드의 `ItemsSource`)과, 거기서 결함 단계 앞·뒤 영상을 복사해 넘기는 부분(GUI-C-182 의 미검증 1번이 **그대로 남는다**). (b) 보고서(`GuiE2eReportService.AppendMetricRows`)에 같은 문구가 찍히는 것 — 이번에 열지 않았다.

## 3. 자동화 경로가 있는가

- 이 앱을 구동하는 시험·CI 는 **없다**: 시험 프로젝트는 `gui/ImageProcTest` 만 구동하고, 워크플로·스크립트에서 이 앱은 `tools/ci/Build-DicomModule.ps1` 의 안내 문구 한 줄(`--run-preprocess-fixture-e2e` 실행 예)에만 나온다. 이 앱에는 UI 자동화용 명령줄 스위치도 없다.
- 앱 자체에 `--run-preprocess-fixture-e2e`, `--run-phase1b-fixture-e2e` 모드가 있으나 **실제 고정 영상을 읽는 실행**이라 로컬 금지에 해당해 돌리지 않았다.
- 따라서 "앱의 계산 경로로 REPORTED 가 나오는 것"을 관측하려면 CI 에 이 앱의 고정 영상 실행을 올려야 한다. 그건 이 카드의 범위 밖이고 지시하지 않은 일이라 제안만 한다(결정은 리더).

## 4. 미검증 · 한계

1. 계산 경로 전체(위 §2 (a))와 보고서 출력(§2 (b))은 관측하지 않았다.
2. A 단계의 덤프에서 그리드가 `loaded=False` 로 찍혔다(탭을 막 선택한 직후라 로드 이벤트 전). 그럼에도 행 4개·셀 4개·글자가 읽혔으나, **창을 사람이 보는 화면 그대로(스크린샷)** 확인한 것은 아니다.
3. 합성 5×5 한 가지 입력만 넣었다. 읽은 것은 `TextBlock.Text`(셀에 들어간 전체 글자)이므로 **칸 너비(Gate 열 180)에서 글자가 잘려 보이는지(말줄임)는 관측하지 못했다.** 가장 긴 Gate 문구(`native calibration preview not run`, 새 문구 중에는 `reported, no gate`)가 화면에서 온전히 보이는지는 스크린샷으로 확인해야 한다.

## 증거 파일

`observation.txt` · `harness_Program.cs.txt` · `harness_csproj.txt` · `text_lint.txt`

🗿 MoAI
