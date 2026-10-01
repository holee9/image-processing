# QA-B-176 — 성능·시간 판정 시험 전수 점검 (post 소유 모듈, #179)

## 결론

1. **post 소유 모듈에서 시간을 판정하는 시험은 33개**(이 보고서의 표) + 시간을 단언하지 않고 측정만 하는 14개다. 등록된 시험 988개 중 post 소유는 919개.
2. **"CI 에서 제외되지 않고 도는 1회 측정 시험"(가장 위험)은 4개였고 전부 변환했다**: `BenchmarkFreeze.BP06`(gsvg), `CollimationDetectTest.BenchmarkFreeze_BP07`, `ExposureIndex.BenchmarkFreeze_BP08` / `BP09`(enhance_basic). 이름에 `Performance`·`Within…ms` 가 없어서 CI 의 제외 규칙을 비껴간 것들이다 — 이름만 보고 센 점검이 놓친 종류.
3. 나머지 1회 측정 시험 17개는 **CI 에서 제외**되어 있다(이 중 5개는 QA-B-175 에서 이미 변환 — T308·T508·LargeImage·T603b·T608). 남은 12개와, 벤치마크 워크플로가 고르는 E2E 3072 1개는 카드의 규칙대로 **표에만** 올렸다. 예산 값은 어디에서도 바꾸지 않았다.
4. 이 점검에서 **CI 가 시간 시험 때문에 실패한 이력은 없다**(2026-09-27~10-01 `ci.yml` 80번 실행). 위험은 실현된 적 없는 잠재 위험이다.

## 방법 — 두 축으로 세고 대조했다

- **이름 축:** `ctest` 등록 이름에서 성능·시간 어휘(`perf|within|budget|benchmark|latency|timing|deadline|stall|timeout|…`)로 후보를 뽑았다.
- **행위 축:** 시험 소스에서 `TEST`/`TEST_F` 본문을 파싱해 **시계를 읽고 시간 값을 단언**하는 시험을 뽑았다.
- 결과: 후보 108 = 이름만 58 + 행위만 3 + 둘 다 47. 이름만 걸린 58개는 목록을 전부 읽고 분류했다(누수 시험의 `Endurance`, `Slow`·`measure` 같은 단어의 오탐 등). 의심스러운 몇 개(`IpcDeadline.ADeadWorker…`, `…ControlAWorkerThatAnswers…WithinTheBudget`)는 본문까지 읽었다.
- **행위 축의 빈틈을 확인했다:** 시계가 본문이 아니라 헬퍼 안에 있으면 놓친다. `FullPipelineE2E.PostProcess_*_Within*ms`(`run_pipeline` 안의 시계)와 `IpcDeadline.*`(헬퍼 안의 `hung` 판정)가 그런 경우였고, 이름 축이 잡았다. 그래서 둘 중 하나가 아니라 **합집합**을 개별 확인했다.
- **CI 규칙은 기계적으로 적용했다.** 세 규칙 — `ci.yml` post 잡의 `ctest -E "Performance|Within[0-9]+ms|PerformanceBudget|LargeImagePerformance"`, 같은 잡의 "단일 프로세스" 단계(`--gtest_filter=-*Performance*:*Within*ms*`), `benchmark-regression.yml` 의 `-R` 패턴 — 을 `ctest` 가 등록한 **988개 이름 전부**에 적용했다. **`-E` 정규식과 gtest 필터는 988개 모두에서 같은 판정**(불일치 0)이다.

## CI 가 이 시험들을 도는 길 (4개)

| 경로 | 무엇을 돌리나 | 시간 시험에 대해 |
|---|---|---|
| `ci.yml` post-build, `ctest` 단계 | `-E` 정규식으로 제외 후 전부 | 이름에 `Performance`/`Within…ms` 가 있으면 제외. **없으면 실행** |
| `ci.yml` post-build, 단일 프로세스 단계 | 실행 파일마다 gtest 필터로 제외 후 한 프로세스씩 | 위와 같은 판정 (이 단계는 ctest 단계 **뒤에 한 번 더** 돈다) |
| `ci.yml` ai-onnx | `build/ci-ai` 의 `ctest`, **제외 없음** | ai 시험은 전부 실행 |
| `benchmark-regression.yml` | `-R` 패턴(`BenchmarkFreeze`, E2E 3072, BP07~09). **main 푸시(모듈 변경)·수동 실행에서만**, PR 게이트가 아님 | 실패하면 워크플로가 빨개짐. 측정 전용 시험은 시간을 단언하지 않아 통과 |
| (`ci.yml` coverage) | `XpeCoverage.cmake` 의 같은 제외 정규식 | 위와 같은 판정 |

`ci-common`·`ci-preprocess`(Lane A 소유 모듈)는 이 카드의 범위가 아니라 보지 않았다.

## CI 이력 — 실제로 실패한 적이 있나

`ci.yml`, 2026-09-27 ~ 10-01, 최근 80번 (성공 41, 실패 22, 취소 15, 기타 2):
- 실패한 22번의 잡 별 집계: text-lint 12, **post-build 10**, gui-e2e-native 5, gui-automation 5, dotnet-tests 2, doxygen 2, gui-shell-runners 1, **ai-onnx 1**.
- post-build 10번의 실패 단계: **단일 프로세스 단계 9번**, 빌드 1번. ai-onnx 1번은 빌드. **`ctest` 단계의 실패는 0번.**
- 단일 프로세스 단계 9번에서 실패한 시험은 매번 같은 5개 — `OnnxSessionRun.StubBuildRefusesToRunAndDoesNotEcho`, `OnnxSessionRun.EmptyInputIsInvalidInput`, `OnnxSessionFixtures.ModelFilesArePresent`, `BoneSuppressAbi.StubBuildFailsAndLeavesTheOutputAlone`, `BoneSuppressAbi.ModelDirectoriesArePresent`(모두 `0 ms`, 시간 시험이 아님). **시간 시험의 실패는 0번.** (`build/g176-single-process-failed-tests.txt` 의 집계, 시험 이름에 시간 어휘가 든 것 0개.)
- 같은 기간 벤치마크 워크플로 14번 모두 성공. 그 로그의 E2E 3072 소요 시간은 **240~263 ms**(예산 3000 ms 의 11배 여유, 편차 9%).
- 한 번의 실패 기록은 문서에 있다: `ci.yml` 주석 — 단일 프로세스 단계가 첫 실행에서 `EdgeEnhance.Performance_3072x3072_Within20ms`(러너에서 28 ms)로 빨개졌고, 그래서 제외 규칙이 생겼다.

CI 공유 러너에서의 실측 중앙값(벤치마크 로그의 `PERFMEASURE`, 14번): 분수 처리 3072² order 1.0 **417 ms**(요구 400 ms), 콜리메이션 3072² 389 ms, T308 의 1024² order 1.2 **49.2 ms**(이 기계에서는 23.3 ms — 러너가 약 2배 느림, T308 의 100 ms 예산에 2배 여유), bilateral 3072² 122 ms … 전체 14개 계열이 `census` 증거에 있다.

## 전수 표

열: **CI 열은 계산값**(등록 이름에 위 규칙을 적용), 측정 모양·예산·출처는 소스와 스펙을 읽은 결과. "변환 전 위험"은 변환 전의 측정 모양과 CI 노출로 계산했다: 높음 = ci.yml 에서 실행되는 단발, 중간 = ci.yml 은 제외지만 벤치마크 워크플로가 고르는 단발, 낮음 = 어느 쪽에서도 CI 에 안 돌거나 이미 중앙값.

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

**예산 출처의 범례:** *요구 직접* = 요구의 3072² 값이 그 크기에서 그대로(예: REQ-ENH-017 ≤ 50 ms), *요구 숫자를 환산 없이 적용* = 3072² 요구 값을 더 작은 이미지에 그대로, *동결 기준선* = SPEC-BENCH-POST 의 요구 REQ-BPOST-001~004(2026-04-22 동결; 제품 성능 요구 PERF-ADV 에서 환산한 값은 아님), *작성자 선택* = 요구에 해당 숫자가 없거나 환산과 다름(근거 문서 없음). 요구 원문은 `.moai/specs/SPEC-XPE-P2-ADV/acceptance.md`(PERF-ADV), `docs/post-processing/xpe/VVP-P1B-001.md` §6.2 의 표(REQ-ENH-006/012/017/022, REQ-ENH-CC-005, REQ-DISP-008/016/028), PR-PERF-002(E2E)에서 확인했다. 기준 하드웨어 정의는 `spec.md`(#179).

## 변환한 것 (예산 불변)

| 시험 | 변환 전 | 변환 후 | 실측 중앙값 vs 예산 |
|---|---|---|---|
| `BenchmarkFreeze.BP06_GsvgVersionProbeBaseline` | 1024호출 합계 1회 | 합계 5회 중앙값(워밍업 1) | < 1 µs vs 5000 µs |
| `CollimationDetectTest.BenchmarkFreeze_BP07_…` | 1회 | 5회 중앙값 | 7.1 ms vs 500 ms |
| `ExposureIndex.BenchmarkFreeze_BP08_…` / `BP09_…` | 1회 | 5회 중앙값 | 약 16 µs vs 25 ms |
| `IntegrationTest.T602_DiagnosticLogging` | `> 0` 을 ms 로 절삭 | µs 해상도 | (예산 아님) |

- `perf_budget.h` 를 `enhance_basic/tests` 와 `gsvg/tests` 에 복사했다. 세 복사본은 **바이트 동일**(md5 `c3e243d4714a0505f9e0cb20a2d6b8ee`, `cmp` 로 확인). 헤더 주석을 "여기에만 있다" 에서 "세 곳에 같은 복사본, 동일 유지"로 고쳤다.
- **`T602` 는 예산이 아니라 결함이다:** `EXPECT_GT(duration, 0)` 을 `milliseconds` 로 절삭한 값에 단언한다. MFP 가 1 ms 미만이면 0 이 나와 **더 빠른 기계에서 실패**한다(정지가 아니라 빠를 때). 이 기계에서는 중앙값 6.7 ms 라 지금은 통과하는, 잠재 결함이다. perf_budget 이 아니라 해상도만 µs 로 고쳤다(`> 0` 의도 유지).
- 모든 예산 값과 이름은 그대로. 각 시험의 예산 출처를 주석에 사실대로 적었다. **BP06~09 의 값은 SPEC-BENCH-POST 의 요구(REQ-BPOST-001~004)이고, 같은 스펙의 REQ-BPOST-006 은 "BP-06~09 를 CI 가 돌려 회귀 시 병합을 막는다" 라고 요구한다** — 이 시험들은 의도된 CI 게이트이므로, 한 번의 정지로 빨개지지 않게 하는 것이 맞는 변환이다.

## 증거

- **전체 `ci-post`:** cfg 0, build 0, ctest 0, **987 / 987**(skipped 25, DISABLED 1) — 시험 수는 변환 전과 같다 (`after_full_ci_post_ctest.txt`).
- **CI 단일 프로세스 단계의 재현:** `--gtest_filter=-*Performance*:*Within*ms*` 로 실행 파일마다 한 프로세스 — gsvg 162, enhance_basic 105, enhance_advanced 243개, **전부 exit 0** (`single_process_step_simulation.txt`).
- **반증 (변환된 4개 시험에 700 ms 주입, 시험 전용 환경변수):** 첫 표본 하나만 지연 → **4개 전부 통과**(중앙값 정상, 최댓값만 700 ms 대: BP06 714 ms, BP07 719 ms, BP08 702 ms, BP09 712 ms), 모든 표본 지연 → **4개 전부 "baseline exceeded: median ~700 ms of […]" 로 실패** (`arm_first_sample_delayed_700ms.txt`, `arm_every_sample_delayed_700ms.txt`). `T602` 는 `Measure` 를 쓰지 않아 주입의 영향이 없다(기대대로).
- 판별 대상이 맞는지(워밍업·prepare 가 안 재지고, 중앙값이 1~2개 이상치를 견디고 3개 이상은 못 견딘다)는 QA-B-175 의 `PerfBudgetHelper` 10개 시험이 이미 고정하고 있다. 이 카드는 그 헬퍼를 복사해서 쓴다.

## 발견 (이 카드의 범위 밖이거나 부수)

- **`perf_measure.h` 복사본이 어긋나 있다.** 헤더 주석은 "세 모듈 디렉터리에 같은 파일" 이라고 하지만 실제로는 **네 곳**(display, enhance_advanced, enhance_basic, gsvg)에 있고 **gsvg 의 것만 내용이 다르다**(md5 `00a9b779…` vs 나머지 셋 `02ebdfcb…`). 복사본 동일성을 검사하는 장치가 없다. 고치지 않았다. `perf_budget.h` 는 같은 길을 가지 않도록 주석에 "동일 유지, `cmp` 로 확인" 을 적었지만 기계적 검사는 없다.
- **ai 의 데드라인 시험 9개**(표의 마지막 묶음)는 `ai-onnx`·post-build 에서 **제외 없이** 돈다. 절대 시간 상한이 아니라 "예산 이전에 반환하지 않음 + 슬랙(2500 ms / 1500 ms) 안에 반환함(`hung` 플래그)" 의미 시험이고 이력상 실패는 없다. 변환 대상이 아니다.
- **로그 서식 결함(별개, QA-B-177 로 접수):** `ai.cpp` 의 `AI_LOG_*` 가 spdlog 경로에서 printf 서식을 그대로 넘겨 `%s/%d/%u` 가 풀리지 않는다. 이 카드의 재현 과정에서 관측했다.

## Gaps / 한계

- **행위 축의 재현율:** 시계를 헬퍼·픽스처 안에서 읽는 시험은 본문 파싱이 놓친다. 이름 축이 E2E 둘과 `IpcDeadline.*` 를 잡았지만, **이름에도 시간이 안 나오고 시계도 헬퍼 안에 있는** 시험이 있다면 이 전수에 없다. 그런 시험은 `std::chrono`/`GetTickCount` 를 쓰는 헬퍼 함수를 따로 찾아야 한다 — 하지 않았다.
- `ci-common`·`ci-preprocess`(Lane A 소유)와 `test_xpe_common` 의 시간 시험은 보지 않았다.
- "높음" 4개가 실제로 실패한 적이 있는지는 **이력이 없음**(위)이지, **실패하지 않는다**는 증명이 아니다. 80번(5일) 표본이다.
- 변환된 시험의 여유는 이 기계 값이다. BP06 은 합계가 1 µs 안팎이라 예산 5000 µs 의 5000배 여유 — 위험은 "여유 부족"이 아니라 **한 번의 5 ms 이상 정지**(스케줄링 한 틱은 10~15 ms)였고, 중앙값 판정은 바로 그것을 막는다.
- `FullPipelineE2E.PostProcess_3072x3072_Within3000ms` 는 변환하지 않았다: ci.yml 에서는 제외이고 벤치마크 워크플로에서만 돌며, 그 CI 실측이 14번 240~263 ms 로 안정적이다. 변환하려면 `run_pipeline` 이 내부에서 시간을 재므로 리팩터링이 필요하다 — 필요하면 별도 카드.
- 이 시험들은 여전히 CI 의 `ctest` 단계에서 제외되는 것과 아닌 것이 섞여 있다(표의 `ci.yml` 열). 제외 규칙 자체는 바꾸지 않았다.

## 증거 파일 (`.moai/reports/lane-post/QA-B-176/`)

`census_table.md`(표 원본), `normal_run.txt`, `arm_first_sample_delayed_700ms.txt`, `arm_every_sample_delayed_700ms.txt`, `after_full_ci_post_ctest.txt`, `single_process_step_simulation.txt`, `repro_stub_use_worker_script.txt`(PowerShell, 실행하려면 `.ps1` 로 복사)·`repro_stub_use_worker_result.txt`(리더가 따로 물은 GUI-C-184 의 -9 재현).
