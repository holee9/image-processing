| 시험 | 위치 | 종류 | 입력 | 문턱 | 측정 방식 | SPEC 근거 | ci.yml | benchmark | 비고 |
|---|---|---|---|---|---|---|---|---|---|
| `ContrastEnhance.Performance_3072x3072_Within50ms` | `modules/enhance_basic/tests/test_contrast_enhance.cpp:192` | T | 3072² | <= 50 ms | 1회, 워밍업 없음, ms 절사 | REQ-ENH-017 (3072², 50 ms) | 아니오 | 아니오 |  |
| `EdgeEnhance.Performance_3072x3072_Within20ms` | `modules/enhance_basic/tests/test_edge_enhance.cpp:190` | T | 3072² | <= 20 ms | 1회, 워밍업 없음, ms 절사 | REQ-ENH-022 (3072², 20 ms) | 아니오 | 아니오 |  |
| `Enh01LogNoise.Bilateral_OnNoisyImage_SNRImprovement_Above_6dB` | `modules/enhance_basic/tests/test_enh01_log_noise.cpp:158` | N | 256² | — | 1회, 시간은 메시지에만 | — | 예 | 아니오 | 시간 단언 없음 (SNR 단언) |
| `EnhanceIntegration.FullPipeline_3072x3072_Within200ms` | `modules/enhance_basic/tests/test_enhance_integration.cpp:106` | T | 3072² | <= 200 ms | 1회, 워밍업 없음, ms 절사 | REQ-ENH-CC-005 (3072², 5단계, 200 ms) | 아니오 | 아니오 | 시험은 3단계(noise+CLAHE+USM)만 잰다 — SPEC 은 log·EI 포함 5단계 |
| `ExposureIndex.BenchmarkFreeze_BP08_EICalcTimeBaseline` | `modules/enhance_basic/tests/test_exposure_index.cpp:247` | T | 512² | < 25 ms | 1회, ms 절사 | SPEC-BENCH-POST BP-08 | 예 | 예 |  |
| `ExposureIndex.BenchmarkFreeze_BP09_DICalcTimeBaseline` | `modules/enhance_basic/tests/test_exposure_index.cpp:272` | T | 512² | < 25 ms | 1회, ms 절사 | SPEC-BENCH-POST BP-09 | 예 | 예 |  |
| `LogTransform.Performance_3072x3072_Within15ms` | `modules/enhance_basic/tests/test_log_transform.cpp:141` | T | 3072² | <= 15 ms | 1회, 워밍업 없음, ms 절사 | REQ-ENH-006 (3072², 15 ms) | 아니오 | 아니오 |  |
| `LogTransform.InversePerformance_3072x3072_Within15ms` | `modules/enhance_basic/tests/test_log_transform.cpp:152` | T | 3072² | <= 15 ms | 1회, 워밍업 없음, ms 절사 | REQ-ENH-006 (3072², 15 ms) | 아니오 | 아니오 |  |
| `NoiseReduce.Performance_Bilateral_Small_Within100ms` | `modules/enhance_basic/tests/test_noise_reduce.cpp:223` | T | 512² | <= 100 ms | 1회, 워밍업 없음, ms 절사 | REQ-ENH-012 (3072², 100 ms) | 아니오 | 아니오 | 크기가 SPEC 과 다름 — SPEC 3072² 에 대한 문턱을 512² 에 적용 |
| `CollimationDetectTest.LargeImagePerformance` | `modules/enhance_advanced/tests/test_collimation_detect.cpp:377` | T | 2048² | < 500 ms | 1회, 워밍업 없음, ms 절사 | PERF-ADV-003 (3072², 500 ms scalar) | 아니오 | 아니오 | 주석은 REQ-ADV-062 를 인용 — 062 는 전체 파이프라인 2500 ms |
| `CollimationDetectTest.BenchmarkFreeze_BP07_CollimationDetectionBaseline` | `modules/enhance_advanced/tests/test_collimation_detect.cpp:408` | T | 512² | < 500 ms | 1회, ms 절사 | SPEC-BENCH-POST BP-07 | 예 | 예 |  |
| `EdgeEnhancementTest.T308_PerformanceBudget` | `modules/enhance_advanced/tests/test_edge_enhancement.cpp:610` | T | 1024² | < 100 ms | 1회, 워밍업 없음, ms 절사 | REQ-ADV-061 (3072², 400/120 ms) | 아니오 | 아니오 | 문턱 100 은 SPEC 에 없음(환산 44 ms, "generous") — QA-B-83 |
| `EdgeEnhancementTest.BenchmarkFreeze_ADV061_FractionalMeasure3072` | `modules/enhance_advanced/tests/test_edge_enhancement.cpp:771` | N | 3072² | — (측정만) | 워밍업 1 + 7회, min/med/max µs | REQ-ADV-061 (3072², 400/120 ms) | 예 | 예 | QA-B-84, 시간 단언 없음 |
| `ExposureIndexTest.T508_PerformanceBudget` | `modules/enhance_advanced/tests/test_exposure_index.cpp:533` | T | 2048² | < 100 ms | 1회, 워밍업 없음, ms 절사 | PERF-ADV-004 (3072², 50/20 ms) | 아니오 | 아니오 | 주석은 REQ-ADV-062 를 인용 — 062 는 전체 파이프라인. 문턱 100 은 환산 22 ms 의 "generous" |
| `IntegrationTest.T602_DiagnosticLogging` | `modules/enhance_advanced/tests/test_integration.cpp:113` | T0 | 512² | > 0 | 1회 | REQ-ADV-091 (로그) | 예 | 아니오 | 시간 문턱 아님 — 0보다 크다만 단언 |
| `IntegrationTest.T603_FullPipelineIntegration` | `modules/enhance_advanced/tests/test_integration.cpp:163` | N | 512² | — | 1회 | — | 예 | 아니오 | 시간 단언은 T603b 로 분리(#120) |
| `IntegrationTest.T603b_FullPipeline_PerformanceBudget` | `modules/enhance_advanced/tests/test_integration.cpp:255` | T | 512² | < 500 ms | 1회, 워밍업 없음, ms 절사 | REQ-ADV-062 / AC-PIPE-001 (3072², 2500 ms) | 아니오 | 아니오 | 크기·문턱 모두 환산값 |
| `IntegrationTest.T608_PerformanceBudgetVerification` | `modules/enhance_advanced/tests/test_integration.cpp:605` | T | 512² | MFP<100, frac<50, coll<50 ms, EI<10000 µs | 함수별 1회, 워밍업 없음 | PERF-ADV-001~004 (3072²) | 아니오 | 아니오 | 주석은 AC-PIPE-001(전체 2500 ms)을 인용. 문턱은 환산값보다 큼 |
| `ModalityLut.Performance_3072x3072_Linear` | `modules/display/tests/test_modality_lut.cpp:226` | T | 3072² | <= 20 ms | 1회, 워밍업 없음, ms 절사 | REQ-DISP-008 (3072², 20 ms) | 아니오 | 아니오 |  |
| `PresentationLut.Performance_3072x3072` | `modules/display/tests/test_presentation_lut.cpp:240` | T | 3072² | <= 30 ms | 1회, 워밍업 없음, ms 절사 | REQ-DISP-028 (3072², 25 ms) | 아니오 | 아니오 | 주석은 REQ-DISP-025 를 인용 — 025 는 GSDF 보정, 성능 아님. 문턱 30 > SPEC 25 |
| `VoiLut.Performance_3072x3072` | `modules/display/tests/test_voi_lut.cpp:336` | T | 3072² | <= 16 ms | 1회, 워밍업 없음, ms 절사 | REQ-DISP-016 (3072², 16 ms) | 아니오 | 아니오 |  |
| `DicomNetworkTest.CancelCStore_TerminatesOperation` | `modules/dicom/tests/test_dicom_network_scu.cpp:177` | N | — | — | sleep_for 100 ms | — | 빌드 안 됨 | 빌드 안 됨 | 측정 아님(대기) |
| `GsvgAbiSmoke.Lifecycle3072_PassThroughIsByteEqual` | `modules/gsvg/tests/test_gsvg_abi_smoke.cpp:100` | N | 3072² | — | 1회, (void)elapsed | — | 예 | 아니오 | 시간 단언은 Lifecycle3072_PerformanceBudget 로 분리 |
| `GsvgAbiSmoke.Lifecycle3072_VignetteAndGrid_OutputClampedAndSourceIntact` | `modules/gsvg/tests/test_gsvg_abi_smoke.cpp:132` | N | 3072² | — | 1회, (void)elapsed | — | 예 | 아니오 | 같음 |
| `GsvgAbiSmoke.Lifecycle3072_PerformanceBudget` | `modules/gsvg/tests/test_gsvg_abi_smoke.cpp:258` | T | 3072² | < 8000 ms (2경로) | 경로별 1회, ms 절사 | REQ-GSVG-019 (3072², 1000 ms) | 아니오 | 아니오 | 주석: "generous ceiling … budget asserted by the dedicated benchmark suite" — 문턱 8000 은 SPEC 값 아님 |
| `BenchmarkFreeze.BP06_GsvgVersionProbeBaseline` | `modules/gsvg/tests/test_gsvg_benchmark.cpp:7` | T | — (버전 문자열) | < 5000 µs / 1024회 | 1024회 합계 | SPEC-BENCH-POST BP-06 | 예 | 예 | 영상 처리 시간이 아님 |
| `GsvgDegradedMode.BP07_NullVignetteMap_IdentityOutput` | `modules/gsvg/tests/test_gsvg_degraded.cpp:59` | N | 64² | — | 1회, (void)elapsed | — | 예 | 아니오 | 시간 단언은 DegradedMode_PerformanceBudget 로 분리 |
| `GsvgDegradedMode.BP08_AllOnesVignetteMap_OutputEqualsInput` | `modules/gsvg/tests/test_gsvg_degraded.cpp:95` | N | 64² | — | 1회, (void)elapsed | — | 예 | 아니오 | 같음 |
| `GsvgDegradedMode.BP09_GridDisabled_VignetteOnly` | `modules/gsvg/tests/test_gsvg_degraded.cpp:133` | N | 64² | — | 1회, (void)elapsed | — | 예 | 아니오 | 같음 |
| `GsvgDegradedMode.DegradedMode_PerformanceBudget` | `modules/gsvg/tests/test_gsvg_degraded.cpp:206` | T | 64² | < 50 ms (3경로) | 경로별 1회, ms 절사 | 찾지 못함 | 아니오 | 아니오 | "BP-07~09-DEG" 라벨이지만 SPEC-BENCH-POST 의 BP-07~09 는 다른 시험 |
| `FullPipelineE2E.PostProcess_3072x3072_Within3000ms` | `tests/e2e_post_pipeline/test_e2e_full_pipeline.cpp:270` | T | 3072² | <= 3000 ms | 1회(도우미 run_pipeline), ms | SPEC-XPE-MASTER Phase 1 < 3000 ms (Pipeline Spec §5.1) | 아니오 | 예 | 루트 tests/ |
| `FullPipelineE2E.PostProcess_512x512_Within200ms` | `tests/e2e_post_pipeline/test_e2e_full_pipeline.cpp:288` | T | 512² | <= 200 ms | 1회(도우미), ms | 찾지 못함 | 아니오 | 아니오 | 루트 tests/ |
| `FullPipelineE2E.PostProcess_3072x3072_PeakMemory190MB` | `tests/e2e_post_pipeline/test_e2e_full_pipeline.cpp:309` | N | 3072² | — (메모리 <= 190 MB) | 1회(도우미) | SPEC-XPE-MASTER Phase 1 peak <= 190MB | 예 | 아니오 | 시간은 재지만 단언은 메모리. 루트 tests/ |

candidates=33 kinds={'N': 10, 'T': 22, 'T0': 1} unclassified=0
time-asserting (T) run by neither workflow: 17
- `ContrastEnhance.Performance_3072x3072_Within50ms` — REQ-ENH-017 (3072², 50 ms) — ci:아니오 bench:아니오
- `EdgeEnhance.Performance_3072x3072_Within20ms` — REQ-ENH-022 (3072², 20 ms) — ci:아니오 bench:아니오
- `EnhanceIntegration.FullPipeline_3072x3072_Within200ms` — REQ-ENH-CC-005 (3072², 5단계, 200 ms) — ci:아니오 bench:아니오
- `LogTransform.Performance_3072x3072_Within15ms` — REQ-ENH-006 (3072², 15 ms) — ci:아니오 bench:아니오
- `LogTransform.InversePerformance_3072x3072_Within15ms` — REQ-ENH-006 (3072², 15 ms) — ci:아니오 bench:아니오
- `NoiseReduce.Performance_Bilateral_Small_Within100ms` — REQ-ENH-012 (3072², 100 ms) — ci:아니오 bench:아니오
- `CollimationDetectTest.LargeImagePerformance` — PERF-ADV-003 (3072², 500 ms scalar) — ci:아니오 bench:아니오
- `EdgeEnhancementTest.T308_PerformanceBudget` — REQ-ADV-061 (3072², 400/120 ms) — ci:아니오 bench:아니오
- `ExposureIndexTest.T508_PerformanceBudget` — PERF-ADV-004 (3072², 50/20 ms) — ci:아니오 bench:아니오
- `IntegrationTest.T603b_FullPipeline_PerformanceBudget` — REQ-ADV-062 / AC-PIPE-001 (3072², 2500 ms) — ci:아니오 bench:아니오
- `IntegrationTest.T608_PerformanceBudgetVerification` — PERF-ADV-001~004 (3072²) — ci:아니오 bench:아니오
- `ModalityLut.Performance_3072x3072_Linear` — REQ-DISP-008 (3072², 20 ms) — ci:아니오 bench:아니오
- `PresentationLut.Performance_3072x3072` — REQ-DISP-028 (3072², 25 ms) — ci:아니오 bench:아니오
- `VoiLut.Performance_3072x3072` — REQ-DISP-016 (3072², 16 ms) — ci:아니오 bench:아니오
- `GsvgAbiSmoke.Lifecycle3072_PerformanceBudget` — REQ-GSVG-019 (3072², 1000 ms) — ci:아니오 bench:아니오
- `GsvgDegradedMode.DegradedMode_PerformanceBudget` — 찾지 못함 — ci:아니오 bench:아니오
- `FullPipelineE2E.PostProcess_512x512_Within200ms` — 찾지 못함 — ci:아니오 bench:아니오
