# QA-B-175 — 성능 예산 시험을 "워밍업 1회 + 5회 중앙값" 판정으로 (#179)

## 결과

`enhance_advanced` 의 PerformanceBudget 묶음 시험 5개를 **호출 1회 벽시계 단언**에서 **워밍업 1회(비계측) + 5회 계측의 중앙값 판정**으로 바꿨다. **예산 값은 하나도 바꾸지 않았다.** 각 시험의 예산 출처 주석을 사실대로 고쳤다.

| 시험 | 예산(불변) | 이번 실측 중앙값 | 여유 |
|---|---|---|---|
| `EdgeEnhancementTest.T308_PerformanceBudget` (1024², 분수) | 100 ms | 23.3 ms | 4.3배 |
| `ExposureIndexTest.T508_PerformanceBudget` (2048², EI) | 100 ms | 3.4 ms | 29배 |
| `CollimationDetectTest.LargeImagePerformance` (2048²) | 500 ms | 173.7 ms | 2.9배 |
| `IntegrationTest.T603b_FullPipeline_PerformanceBudget` (512², 전체) | 500 ms | 16.8 ms | 30배 |
| `IntegrationTest.T608_PerformanceBudgetVerification` (512²) MFP / 분수 / 콜리메이션 / EI | 100 / 50 / 50 ms / 10 ms | 6.7 / 6.3 / 3.0 ms / 238 µs | 15 / 7.9 / 17 / 42배 |

(`PERFBUDGET …` 줄, `after_full_ci_post_ctest.txt` 와 아래 반증 로그에 원자료.)

## 무엇을 바꿨나

- **`tests/perf_budget.h`** (새 파일): `perf_budget::Measure(what, prepare, run)` — 워밍업 1회 후 5회를 재서 중앙값·최솟값·최댓값·표본을 돌려준다. `prepare()` 는 계측 밖(제자리에서 바꾸는 함수를 위해 입력 복원), `run()` 은 `XPE_OK` 여야 한다. `Describe()` 는 단언 메시지에 표본을 그대로 보인다. 기존 `perf_measure.h` 는 **측정만 하고 판정하지 않는** 헬퍼이고 세 모듈에 같은 복사본이 있어서 건드리지 않았다.
- **시험 5개 변환**: 제자리에서 바뀌는 호출(분수, MFP, 전체 파이프라인)은 매번 입력을 복원하고, 읽기만 하는 호출(EI, 콜리메이션)은 복원하지 않는다. T608 은 원래 순서와 입력 흐름(분수는 0 으로 채운 이미지, 콜리메이션·EI 는 그 뒤 이미지)을 유지했다.
- **예산 출처 주석(사실만)**: T608 의 50 ms 는 "요구에서 환산한 값이 아니라 작성자가 고른 값"이다. 요구(acceptance.md PERF-ADV-001~005)는 3072² FLOAT32, 기준 하드웨어, scalar / AVX2 숫자뿐이고 512² 요구는 없다. 선형 환산(면적 1/36)은 MFP 22, 분수 11, 콜리메이션 14, EI 1.4 ms 이고 시험이 단언하는 값은 그 수 배다. T308(환산 44 ms, 단언 100), T508(환산 22 ms, 단언 100), LargeImage(500 ms 는 3072² 요구를 **환산 없이** 더 작은 이미지에 그대로 적용), T603b(환산 약 70 ms, 단언 500)도 각각 적었다.
- **`tests/test_perf_budget_helper.cpp`** (새 파일, 10개): 가짜 작업으로 중앙값이 무엇을 가려내는지 고정한다. 표본 1개가 느려도·5개 중 2개가 느려도 통과, 5개 전부 느리면 실패, 5개 중 3개가 느리면 실패, 워밍업은 안 잼, `prepare()` 는 안 잼, 환경변수 주입 경로.
- **시험 전용 주입** (`XPE_PERF_INJECT_DELAY_MS`, `XPE_PERF_INJECT_MODE=first|all`): 설정하지 않으면 아무 일도 하지 않는다. 실제 시험을 수정하지 않고 반증을 돌리기 위한 것이다.

## 증거

- **red(TDD):** 헬퍼 시험과 CMake 등록만 먼저 넣고 빌드 — `fatal error C1083: 'perf_budget.h': No such file` (`before_helper_build_red.txt`; 같은 줄의 `RUN=0` 은 낡은 바이너리 결과라 증거 아님).
- **green:** 헬퍼 10개 + 변환된 시험 5개 = 15개 통과. 전체 `ci-post`: cfg 0, build 0, ctest 0, **987 / 987** (skipped 25, DISABLED 1; 이전 977 → 헬퍼 시험 +10) (`after_full_ci_post_ctest.txt`).
- **반증 — 실제 시험 5개에 주입** (예산을 모두 넘는 700 ms 지연):
  - **첫 표본 하나만 지연** (`arm_first_sample_delayed_700ms.txt`): **5개 전부 통과.** 중앙값은 정상(예: T308 23.1 ms, LargeImage 173.2 ms)이고 최댓값만 708~895 ms.
  - **모든 표본 지연** (`arm_every_sample_delayed_700ms.txt`): **5개 전부 실패**, 실패 사유는 모두 예산 초과(예: "Fractional exceeded budget: median 712.23 ms of [711.66 719.19 711.21 712.23 712.39] ms"). 판정 대상이 맞다: 단발 이상치는 무시하고 진짜 느림은 잡는다.
  - 옛 단발 측정 모양이 "첫 표본 지연" 아래서 5개 모두 실패했을 것이라는 것은 **구성으로부터의 추론**이다(옛 시험은 정확히 한 표본을 판정했고 그 표본이 700 ms 였을 것이므로). 옛 코드를 주입으로 돌려 확인하지는 않았다(주입 경로가 헬퍼에 있어 옛 코드에는 닿지 않는다).
- **시험이 스스로 가진 결함을 찾아 고쳤다:** 헬퍼 시험 중 3개가 "정상 호출의 최댓값 < 30 ms" 같은 **단일 표본 절대 상한**을 단언했다. `PerfBudgetHelper` 는 CI 의 제외 정규식(`PerformanceBudget`)에 걸리지 않아 공유 러너에서 실제로 도는데, 거기서는 한 번의 스케줄링 지연으로 실패한다 — 이 카드가 없애려는 바로 그 결함이다. 큰 주입 지연(300 ms)과 그보다 한참 아래의 문턱, 또는 중앙값 기준으로 바꿨다(`PerfBudgetHelper` 10개 재통과).
- **ci-ai 341 확인 (리더 요청):** main 푸시 `a7dfd0c6` 의 XPE CI Pipeline(run 36870828287) `ai-onnx` 잡(110398022780) completed/success, 로그 원문 `100% tests passed out of 341`, Total Test time 42.49 s (`main_ci_ai_onnx_job_summary.txt`). 워커 시험(`WorkerPathFixture…`, `WorkerState…`)의 이름이 그 로그에 있어 시험이 실제로 선택되었음을 대조했다.

## Gaps / 한계

- **같은 모양일 수 있는 다른 모듈의 성능 시험은 열어 보지 않았다.** 카드가 지정한 것은 `enhance_advanced` 의 묶음이다. `ctest -N` 에는 이름에 `Within…ms`·`Performance` 가 들어간 시험이 더 있다: `enhance_basic`(LogTransform·NoiseReduce·ContrastEnhance·EdgeEnhance·EnhanceIntegration), `display`(ModalityLut·VoiLut·PresentationLut), `gsvg`(GsvgDegradedMode·GsvgAbiSmoke), `FullPipelineE2E` 등. 이 중 어느 것이 호출 1회 단언인지는 확인하지 않았다 — 같은 위험이 있을 수 있다. `perf_budget.h` 는 `enhance_advanced/tests` 에만 있다(다른 모듈에 쓰려면 복사 또는 공유 위치가 필요하며 그 결정은 이 카드 밖).
- 중앙값 5회는 **5개 중 2개까지의 이상치**를 견딘다. 3개 이상이 느린 구간(예: 부하가 몇 초 지속)은 여전히 실패할 수 있다 — 그것은 진짜 느림과 구분할 수 없으므로 의도된 동작이다.
- 시험 시간이 늘었다: `LargeImagePerformance` 약 1.05 s(6회 호출), T608 약 0.2 s(이전 약 0.04 s). 전체 스위트 영향은 작다.
- 이 시험들은 여전히 **CI 에서 제외된다**(`ci.yml` `-E` 정규식, QA-B-174). 이번 변경은 로컬 전체 실행에서의 거짓 실패를 줄이는 것이고 CI 게이트를 늘리지 않았다. 예산 값이 요구와 맞는지(작성자 선택 값)는 건드리지 않았다.
- 실측은 이 기계 한 대의 값이다. 느린 기계의 여유는 모른다(스펙의 공유 러너 실측은 개발 PC 대비 약 1.4배 느림 — T308 의 4.3배, LargeImage 의 2.9배 여유는 이 비율을 견딘다. 계산이지 측정은 아니다).
- 주입 환경변수는 시험 전용이며, 설정된 채로 남아 있으면 모든 예산 시험이 영향을 받는다(헬퍼 시험은 `ScopedEnv` 로 정리한다).
