# GUI-C-233e 보고 — 보고서 필드가 읽는 값 전수 (Codex #175 마지막 1건, Refs #251)

dev/gui 에서 작업. 푸시 안 함.

## 고친 것
- `calibrationEvaluation.summary` 를 렌더가 쓴 값(`_renderedInputs`)에서 만든다. 렌더가 없으면 `null`. 현재 요청의 요약은 `calibrationEvaluation.requested.summary`. 두 문자열은 한 포매터(`FormatCalibrationModes`)를 거치므로 표현이 갈라지지 않는다. 화면용 `CalibrationEvaluationSummary` 프로퍼티는 그대로 "지금 요청" 을 읽는다(패널·자동화 보고가 쓴다).

## 시험 (SelfCheck 11 확장)
- (a) A 적용 → 교정 모드 하나(Offset)를 바꿈 → 재적용 전(같은 동기 단계)에 보고서: `summary`·`offset` 이 A 의 적용값 그대로, `requested.offset`/`requested.summary` 는 새 값, 적용 summary 의 Offset 이 적용 offset 과 일치.
- (b) A 성공 → B 표시 실패: `summary` 와 개별 모드 모두 `null`.
- 반증: `summary` 를 다시 `CalibrationEvaluationSummary` 로 읽게 하면 (a)(b) 둘 다 빨강 (`falsification_summary_reads_settings.txt`: "followed the settings instead of the applied render", "B's report carries a calibration summary no render of B produced: Offset=Off, …"). 복구 후 통과(`selfcheck_fixed.txt`).

## 전수 표 — `ExportAutomationReport` (menu-command-report.json)
| 필드 | 읽는 곳 | 구분 | 조치 |
|---|---|---|---|
| generatedAt, backend, mockBackend, actualBackendMode | 런타임 | 사실 | — |
| requestedBackendMode | Settings.BackendMode | 요청 (이름이 requested) | — |
| activeImageSummary | 불러온 프레임 | 적용(프레임) | — |
| status | StatusText | 마지막 메시지 | — |
| settings | Settings 객체 전체 | **현재 설정 = 요청** (이름이 settings) | 키 이름은 소비자가 있을 수 있어 바꾸지 않음 — 리더 판단 요청 |
| visiblePanels.calibration / display | Settings.Show*Panel | 현재 화면 상태(즉시 반영) | — |
| visiblePanels.logs | ShowLogsPanel | 현재 화면 상태 | — |
| displayPipeline.applied / summary / version | 프레임 / DisplayPipelineSummary(새 프레임에서 비움) / 런타임 | 적용 | — |
| displayPipeline.windowSource / mode / center / width | Rendered* (233c) | 적용 | — |
| displayPipeline.bodyPart / gsdf | _renderedInputs (233d) | 적용 | — |
| displayPipeline.requested.* | Settings | 요청 | — |
| **calibrationEvaluation.summary** | **Settings(수정 전)** → _renderedInputs | 요청 → **적용** | **고침** |
| calibrationEvaluation.offset … binning (7) | _renderedInputs (233d) | 적용 | — |
| calibrationEvaluation.requested.* (7 + summary) | Settings | 요청 | summary 추가 |
| processingChain.* (status, pipelineTimings, stages, displayInput) | LastChain / ChainStatus (233d 에서 새 프레임 때 비움) | 적용 | — |
| processingChain.exposureKvp / pixelPitchMm / gsvg* / preprocessRequested | _renderedInputs | 적용 | — |
| comparison.mode / zoomScale / panX / panY / swipePosition / overlayOpacity | Settings | 뷰 상태(바꾸면 비교 화면이 즉시 따라감 — 렌더 입력이 아님) | — |
| comparison.sourceLayerPresent / processedLayerPresent / sourcePreserved | 영상 | 현재 상태 | — |
| logCount, alertCount | 컬렉션 | 현재 | — |

검색 범위: `MainWindowViewModel.ExportAutomationReport()` 전체 직렬화 코드와 그 안의 `DescribeChain()`. 같은 종류의 필드는 `calibrationEvaluation.summary` 하나였다.

## 열린 것 (고치지 않았고 이유)
- **두 번째 보고(`--automation-run` 자체 점검, `GuiAutomationReport`, MainWindow.xaml.cs 약 327–345행)** 도 `CalibrationEvaluationSummary`, `OffsetCorrectionMode`, `DefectCorrectionMode`, `ComparisonMode/Zoom/Swipe` 를 **현재 Settings** 에서 읽는다. 이 보고는 "점검이 설정한 값" 을 증거로 남기고, 합격 판정이 `Offset=Off`·`Defect=On`(점검이 직접 세팅한 값)을 요구한다(727–728행). 적용값으로 바꾸면 판정 항이 바뀌고(적용 시점 보장이 없다) 이 카드 범위를 넘는다. 리더 결정 요청: ① 그대로 두고 필드명에 requested 를 붙인다 ② 적용값으로 바꾸고 판정 항을 새로 정한다.
- `settings` 키(위 표)도 같은 질문: 이름만으로 "요청" 임이 읽히는지.

## 확인한 범위·못 한 것
- SelfCheck 는 스크립트 백엔드에서 뷰모델을 실행. Native 백엔드의 실제 보고서 JSON 은 이 카드에서 읽지 않았다.
- (a) 의 "재적용 전" 은 같은 동기 단계에서 읽는 것으로 보장(UI 스레드, 비동기 재렌더 시작 전).

- 통합 시험: 첫 전체 실행에서 PreprocessHandshakeTests 1건이 간헐 실패(원인 미규명), 단독·전체 2회 재실행은 945/0/2 (integration_suite.txt).
