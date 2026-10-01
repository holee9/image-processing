| 모듈 | 시험 | 측정 모양 | 예산 | 예산 출처 | ci.yml ctest·단일프로세스 | 벤치마크 워크플로 | ai-onnx | 변환 전 위험 | 조치 |
|---|---|---|---|---|---|---|---|---|---|
| enhance_advanced | `EdgeEnhancementTest.T308_PerformanceBudget` | 중앙값(워밍업1+5회); 이전 1회 | 100 ms @1024² | 작성자 선택 (요구 PERF-ADV-002 400 ms @3072², 환산 44 ms) | 제외 | - | - | 낮음 (CI 제외 + 단발) | 변환됨 (QA-B-175) |
| enhance_advanced | `ExposureIndexTest.T508_PerformanceBudget` | 중앙값(워밍업1+5회); 이전 1회 | 100 ms @2048² | 작성자 선택 (PERF-ADV-004 50 ms @3072², 환산 22 ms) | 제외 | - | - | 낮음 (CI 제외 + 단발) | 변환됨 (QA-B-175) |
| enhance_advanced | `CollimationDetectTest.LargeImagePerformance` | 중앙값(워밍업1+5회); 이전 1회 | 500 ms @2048² | 요구 숫자를 환산 없이 적용 (PERF-ADV-003 500 ms @3072²) | 제외 | - | - | 낮음 (CI 제외 + 단발) | 변환됨 (QA-B-175) |
| enhance_advanced | `IntegrationTest.T603b_FullPipeline_PerformanceBudget` | 중앙값(워밍업1+5회); 이전 1회 | 500 ms @512² 전체 | 작성자 선택 (PERF-ADV-005 2500 ms @3072², 환산 약 70 ms) | 제외 | - | - | 낮음 (CI 제외 + 단발) | 변환됨 (QA-B-175) |
| enhance_advanced | `IntegrationTest.T608_PerformanceBudgetVerification` | 중앙값(워밍업1+5회) ×4함수; 이전 1회 | 100/50/50 ms, 10 ms @512² | 작성자 선택 (PERF-ADV-001~004 환산의 수 배) | 제외 | - | - | 낮음 (CI 제외 + 단발) | 변환됨 (QA-B-175) |
| enhance_advanced | `CollimationDetectTest.BenchmarkFreeze_BP07_CollimationDetectionBaseline` | 중앙값(워밍업1+5회); 이전 1회 | 500 ms @512² | 요구 (SPEC-BENCH-POST REQ-BPOST-002, 동결: 512² 콜리메이션 < 500 ms; PERF-ADV-003 의 500 ms 와 같은 수) | **실행** | 선택 | - | 높음 (CI 실행 + 단발) | 변환됨 (QA-B-176) |
| enhance_advanced | `EdgeEnhancementTest.BenchmarkFreeze_ADV061_FractionalPerformanceRegressionGate3072` | 중앙값(워밍업1+7회) | 700 ms @3072² order 1.2 | 도출 (CI 회귀 예산, 실측 중앙값 428~586 ms 를 근거로 주석에 기록) | 제외 | 선택 | - | 낮음 (이미 중앙값) | 변경 없음 |
| enhance_advanced | `IntegrationTest.T602_DiagnosticLogging` | 1회, 하한 `> 0` 을 ms 로 절삭해 단언 | 예산 아님 (하한) | 해당 없음 — 빠른 기계에서 0 이 나와 실패할 수 있음 | **실행** | - | - | 중간 (CI 실행, 빠른 기계에서 실패 가능) | 수정 (µs 해상도) |
| enhance_basic | `LogTransform.Performance_3072x3072_Within15ms` | 1회 (ms 절삭) | 15 ms @3072² | 요구 직접 (REQ-ENH-006 ≤ 15 ms) | 제외 | - | - | 낮음 (CI 제외 + 단발) | 표에만 |
| enhance_basic | `LogTransform.InversePerformance_3072x3072_Within15ms` | 1회 (ms 절삭) | 15 ms @3072² | 요구 값을 역변환에 확장 적용 (REQ-ENH-006) | 제외 | - | - | 낮음 (CI 제외 + 단발) | 표에만 |
| enhance_basic | `NoiseReduce.Performance_Bilateral_Small_Within100ms` | 1회 (ms 절삭) | 100 ms @512² | 요구 숫자를 환산 없이 적용 (REQ-ENH-012 ≤ 100 ms @3072²) | 제외 | - | - | 낮음 (CI 제외 + 단발) | 표에만 |
| enhance_basic | `ContrastEnhance.Performance_3072x3072_Within50ms` | 1회 (ms 절삭) | 50 ms @3072² | 요구 직접 (REQ-ENH-017 ≤ 50 ms) | 제외 | - | - | 낮음 (CI 제외 + 단발) | 표에만 |
| enhance_basic | `EdgeEnhance.Performance_3072x3072_Within20ms` | 1회 (ms 절삭) | 20 ms @3072² | 요구 직접 (REQ-ENH-022 ≤ 20 ms); CI 러너에서 28 ms 로 실패한 적 있음(ci.yml 주석) — 제외 사유 | 제외 | - | - | 낮음 (CI 제외 + 단발) | 표에만 |
| enhance_basic | `EnhanceIntegration.FullPipeline_3072x3072_Within200ms` | 1회 전체 (ms 절삭) | 200 ms @3072² | 요구 직접 (REQ-ENH-CC-005 ≤ 200 ms) | 제외 | - | - | 낮음 (CI 제외 + 단발) | 표에만 |
| enhance_basic | `ExposureIndex.BenchmarkFreeze_BP08_EICalcTimeBaseline` | 중앙값(워밍업1+5회); 이전 1회 | 25 ms @512² | 요구 (SPEC-BENCH-POST REQ-BPOST-003, 동결: 512² EI < 25 ms) | **실행** | 선택 | - | 높음 (CI 실행 + 단발) | 변환됨 (QA-B-176) |
| enhance_basic | `ExposureIndex.BenchmarkFreeze_BP09_DICalcTimeBaseline` | 중앙값(워밍업1+5회); 이전 1회 | 25 ms @512² | 요구 (SPEC-BENCH-POST REQ-BPOST-004, 동결: 512² DI < 25 ms) | **실행** | 선택 | - | 높음 (CI 실행 + 단발) | 변환됨 (QA-B-176) |
| display | `ModalityLut.Performance_3072x3072_Linear` | 1회 (ms 절삭) | 20 ms @3072² | 요구 직접 (REQ-DISP-008 ≤ 20 ms) | 제외 | - | - | 낮음 (CI 제외 + 단발) | 표에만 |
| display | `VoiLut.Performance_3072x3072` | 1회 (ms 절삭) | 16 ms @3072² | 요구 직접 (REQ-DISP-016 ≤ 16 ms) | 제외 | - | - | 낮음 (CI 제외 + 단발) | 표에만 |
| display | `PresentationLut.Performance_3072x3072` | 1회 (ms 절삭) | 30 ms @3072² | 요구보다 완화 (REQ-DISP-028 ≤ 25 ms; 시험 주석 "limit 30 ms, Release target 25 ms") | 제외 | - | - | 낮음 (CI 제외 + 단발) | 표에만 |
| gsvg | `BenchmarkFreeze.BP06_GsvgVersionProbeBaseline` | 중앙값(워밍업1+5회) of 1024호출 합계; 이전 1회 합계 | 5000 µs / 1024호출 | 요구 (SPEC-BENCH-POST REQ-BPOST-001, 동결: 1024호출 < 5000 µs) | **실행** | 선택 | - | 높음 (CI 실행 + 단발) | 변환됨 (QA-B-176) |
| gsvg | `GsvgAbiSmoke.Lifecycle3072_PerformanceBudget` | 1회 ×2 (호출마다, ms 절삭) | 8000 ms @3072² | 작성자 선택 (요구 숫자를 찾지 못함) | 제외 | - | - | 낮음 (CI 제외 + 단발) | 표에만 |
| gsvg | `GsvgDegradedMode.DegradedMode_PerformanceBudget` | 1회 ×3 (모드마다, ms 절삭) | 50 ms @64² | 작성자 선택 (요구 숫자를 찾지 못함) | 제외 | - | - | 낮음 (CI 제외 + 단발) | 표에만 |
| e2e_post_pipeline | `FullPipelineE2E.PostProcess_3072x3072_Within3000ms` | 1회 전체 (run_pipeline, ms 절삭) | 3000 ms @3072² | 요구 직접 (PR-PERF-002 / PERF-P1-001 ≤ 3000 ms) | 제외 | 선택 | - | 중간 (벤치마크 CI 선택 + 단발) | 표에만 (벤치마크 CI 실측 14회 240~263 ms) |
| e2e_post_pipeline | `FullPipelineE2E.PostProcess_512x512_Within200ms` | 1회 전체 (ms 절삭) | 200 ms @512² | 작성자 선택 | 제외 | - | - | 낮음 (CI 제외 + 단발) | 표에만 |
| ai | `WorkerSupervisor.AStalledWorkerFailsThatCallIsKilledAndTheNextCallStartsAFreshOne` | 1회; 하한 예산−100 ms, 상한 예산+2500 ms | 예산 2000 ms (시험 인자) | 작성자 선택 (REQ-AI-092 는 숫자를 주지 않음); 슬랙 2500 ms | **실행** | - | 실행 | 데드라인 시험 (CI 실행, 슬랙 큼) | 표에만 |
| ai | `WorkerSupervisor.MeasureColdStartAgainstTheDefaultBudget` | 1회 기동 시간 + 예산 경계 | 예산 3000 ms | 작성자 선택; 슬랙 2500 ms | **실행** | - | 실행 | 데드라인 시험 (CI 실행, 슬랙 큼) | 표에만 |
| ai | `WorkerSupervisor.MeasureTheCostOfRepeatedFailuresWhenAWorkerHangsOrDiesOnStart` | 반복 실패 N회; 하한 예산−100 ms | 예산 800 ms | 작성자 선택 | **실행** | - | 실행 | 데드라인 시험 (CI 실행, 슬랙 큼) | 표에만 |
| ai | `IpcDeadline.AWorkerThatNeverAnswersReturnsControlAfterTheBudget` | 1회; 하한·`hung` 플래그 | 예산 400 ms, 슬랙 1500 ms | 작성자 선택 | **실행** | - | 실행 | 데드라인 시험 (CI 실행, 슬랙 큼) | 표에만 |
| ai | `IpcDeadline.BoneSuppressWithAStalledWorkerFailsAfterTheBudgetAndLeavesTheOutputAlone` | 1회; 하한·`hung` 플래그 | 예산 400 ms, 슬랙 1500 ms | 작성자 선택 | **실행** | - | 실행 | 데드라인 시험 (CI 실행, 슬랙 큼) | 표에만 |
| ai | `IpcDeadline.AReplyThatStopsHalfWayAndStallsIsNotCountedAsSuccess` | 1회; 하한·`hung` 플래그 | 예산 400 ms, 슬랙 1500 ms | 작성자 선택 | **실행** | - | 실행 | 데드라인 시험 (CI 실행, 슬랙 큼) | 표에만 |
| ai | `IpcDeadline.TheBudgetIsTheConfiguredValueNotAConstant` | 2회(300 ms vs 1200 ms 예산)의 차이 ≥ 500 ms | 300/1200 ms | 작성자 선택 | **실행** | - | 실행 | 데드라인 시험 (CI 실행, 슬랙 큼) | 표에만 |
| ai | `WorkerPathFixture.ASilentWorkerIsReportedTheInputIsReturnedAndTheNextCallRecovers` | 1회; 시간 단언 포함 | 예산 2000 ms (시험 인자) | 작성자 선택 | **실행** | - | 실행 | 데드라인 시험 (CI 실행, 슬랙 큼) | 표에만 |
| ai | `WorkerPathFixture.WorkerStateAnswersPromptlyWhileACallIsStuckOnASilentWorker` | 1회 조회; 상한 1000 ms (측정 근거 주석) | 1000 ms / 예산 5000 ms | 이 기계 실측 기반 (조회 0 ms, 락 잡는 반증 4985 ms) | **실행** | - | 실행 | 데드라인 시험 (CI 실행, 슬랙 큼) | 변경 없음 |

측정만 하고 시간을 단언하지 않는 시험 (`perf_measure.h`, 워밍업 1 + 7회, 예산 없음). 모두 ci.yml 에서 제외되고 벤치마크 워크플로가 선택한다:

- **enhance_advanced** (4): `CollimationDetectTest.BenchmarkFreeze_Performance_PERF_ADV_003_Collimation3072`, `EdgeEnhancementTest.BenchmarkFreeze_Performance_PERF_ADV_002_Fractional3072_Order1`, `EdgeEnhancementTest.BenchmarkFreeze_Performance_T308_Fractional1024`, `ExposureIndexTest.BenchmarkFreeze_Performance_PERF_ADV_004_ExposureIndex3072`
- **enhance_basic** (5): `LogTransform.BenchmarkFreeze_Performance_REQ_ENH_006_LogTransform3072`, `LogTransform.BenchmarkFreeze_Performance_REQ_ENH_006_LogInverse3072`, `NoiseReduce.BenchmarkFreeze_Performance_REQ_ENH_012_Bilateral3072`, `ContrastEnhance.BenchmarkFreeze_Performance_REQ_ENH_017_Clahe3072`, `EdgeEnhance.BenchmarkFreeze_Performance_REQ_ENH_022_Usm3072`
- **display** (3): `ModalityLut.BenchmarkFreeze_Performance_REQ_DISP_008_Linear3072`, `VoiLut.BenchmarkFreeze_Performance_REQ_DISP_016_Linear3072`, `PresentationLut.BenchmarkFreeze_Performance_REQ_DISP_028_Lut3072`
- **gsvg** (2): `GsvgGridSuppression.BenchmarkFreeze_Performance_REQ_GSVG_019_GridDwt3072`, `GsvgVirtualGridBench.BenchmarkFreeze_Performance_REQ_GSVG_019_VirtualGrid3072`
