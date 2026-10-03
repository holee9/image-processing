# QA-A-230 M1 — 꺼진 성능 시험이 요구의 유일한 단언인 문제 (228 D12, #245)

보고서만이다. 판정선을 새로 정하지 않았다. 로컬 측정값은 CI 와 다른 기계의 값이므로 "상한 추정"으로만 적는다(`feedback_gate_calibrated_on_wrong_machine`).

## 1. 한 줄 결과

SRS-CALIB-PERF-001(전체 500 ms)의 유일한 전체 단언 `Integration.DISABLED_PipelinePerformance3072x3072` 는 꺼져 있고, **켜도 요구를 재지 않는다** — 보정 맵을 적재하지 않아 offset·gain·defect 가 즉시 거부되고 ghost 도 입력 형식이 틀려 거부되는데 시험은 반환 코드를 보지 않는다. 같은 요구를 재는 다른 곳은 없다. 벤치마크 워크플로는 전처리 모듈을 빌드조차 하지 않는다(`BUILD_PREPROCESS=OFF`). 단계 예산(55/20/55/10/95/140 ms)은 런타임 검출기 하나를 빼고 단언이 없다.

## 2. 요구 문구 (인용)

`SRS-CALIB-001_Software_Requirements_Specification.md:327`, SRS-CALIB-PERF-001:

> Total preprocessing pipeline execution time shall not exceed 500 ms per 3072×3072 float32 frame on Intel Core i7 or equivalent processor. Breakdown: (0) CalibManager load ≤200 ms (one-time startup), (1) Offset ≤55 ms, (1.5) Nonlinearity ≤20 ms, (2) Gain ≤55 ms, (2.5) Binning ≤10 ms, (3) Defect ≤95 ms, (4) Ghost Tier 1 ≤140 ms, (4) Ghost Tiers 2-3 ≤+130 ms. … Timer instrumentation shall log per-phase duration. | 검증 열: Test: Profiling on reference hardware

PERF-003(`:339`)는 교정 파일 3개 적재 ≤200 ms(SSD). 같은 문서 47행은 "PERF-003 의 200 ms 는 CNG 전환 뒤 3파일 합계 130–137 ms 로 충족한다(#179)"고 적는다.

## 3. 결론 표

| 요구 | 지금 재는 곳 | 기계 | 판정선 | 빈칸 |
|---|---|---|---|---|
| PERF-001 전체 ≤500 ms @3072² | `Integration.DISABLED_PipelinePerformance3072x3072`(`test_integration.cpp:161`): 꺼짐, CI 에서 실행 안 됨 | 없음(CI), 로컬은 `--gtest_also_run_disabled_tests` 로만 | `EXPECT_LE(ms, 500)` — **켜도 무의미**(§4) | 전체 경로를 보정 맵을 적재한 채 재는 곳이 없다 |
| 단계 Offset ≤55 | 없음 | — | — | 단언 없음 |
| 단계 Nonlinearity ≤20 | 없음 | — | — | 단언 없음 |
| 단계 Gain ≤55 | 없음 | — | — | 단언 없음 |
| 단계 Binning ≤10 | 없음 | — | — | 단언 없음 |
| 단계 Defect ≤95 | 일부: 런타임 검출 `RuntimeDetectionPerformanceGateTest.Frame3072SquaredWithinMachineRatio`(`test_runtime_detection_performance_gate.cpp:514`) — 검출만 | CI: AMD EPYC 9V45 4 vCPU, `perf-gate-abs 3072 best=74.6 ms`. 로컬 i7-12700: 반복 최소 92.5~107.1 ms | 절대 상한 `kAbsoluteBudget3072Ms = 400.0 ms`(활성), 비율 한계 1.45(**정지**: CI 비율 1.738) | 400 ms 는 SRS 의 95 ms 의 4.2배다. 보간 보정(`xpe_defect_correct`)은 어디에도 시간 단언이 없다 |
| 단계 Ghost Tier 1 ≤140, Tier 2~3 ≤+130 | 없음(꺼진 측정 `A184Probes.DISABLED_GhostSingleThreadTime` 만) | — | — | 단언 없음 |
| PERF-003 적재 ≤200 | 이 트리의 시험 어느 것도 `PERF-003` 을 인용하지 않는다. SRS 가 인용하는 "130–137 ms(#179)"의 단언 위치는 찾지 못했다 | 미상 | 미상 | 단언 위치 미확인 |
| 저성능 퇴행의 다른 감시 | `PreprocessDegraded` 6건(`test_preprocess_degraded.cpp`): 64×64 영상, `kDegradedBudgetMs = 100` | CI/로컬 | 100 ms | 3072² 가 아니고 요구의 예산과 무관 |
| 벤치마크 워크플로 `XPE Benchmark Regression (BP-06~09 + 3000ms budget)` | `ci-post` 프리셋의 후처리·표시·GSVG 20개 시험(`PostProcess_3072x3072_Within3000ms` 등) | GitHub 호스트 `windows-2025-vs2026`(CPU 는 로그에 안 찍힘) | 3000 ms(후처리) | **전처리 0건**: `ci-post` 는 `BUILD_PREPROCESS=OFF`(`CMakePresets.json`). 실행한 20개 시험 중 이름에 Preprocess/Integration 이 든 것 0 |

## 4. 꺼진 시험이 켜져도 요구를 재지 않는 이유

`Integration.DISABLED_PipelinePerformance3072x3072`(`test_integration.cpp:161-182`)는 아홉 함수를 부르고 전체를 한 번에 시간 잰다. 반환 코드는 어디서도 읽지 않는다.

- `xpe_offset_correct`·`xpe_gain_correct`·`xpe_defect_correct`: 이 시험은 보정 맵을 적재하지 않는다. 같은 파일의 살아 있는 `Integration` 시험이 같은 호출에서 `XPE_ERR_NOT_INITIALIZED`(-6)를 단언한다(`test_integration.cpp:118-132`, 주석 87행 "got -6 … in all"). 즉 이 세 단계는 시간이 거의 들지 않는 즉시 거부다.
- `xpe_ghost_correct`: 이 시험은 `rawBuf`(UINT16)를 넘기는데 ghost 는 FLOAT32 만 받는다(`ghost_correct.cpp` 의 `xpe_buffer_has_format(img, XPE_PIXEL_FLOAT32, …)` → `XPE_ERR_INVALID_INPUT`). 핸들 생성은 3072² 이력 버퍼(약 189 MB) 할당과 0 채우기라 시간이 들지만 보정 자체는 하지 않는다.
- 실제로 도는 일: 판독 검증, 온도 보상, 비선형 보정(LUT 없음), 핸들 생성, 비닝.

측정(로컬 i7-12700, 3회): 시험 전체가 **66~67 ms**(`evidence/disabled_test_local_runs.txt`; 시간 재는 구간은 이보다 짧다). 500 ms 의 약 1/7 이다. offset·gain·defect·ghost 가 지금보다 몇 배 느려져도 이 시험은 통과한다. 그래서 켜는 것만으로는 요구를 지키지 못한다.

## 5. CI 에서 관측한 것 (실제 로그)

- `ci.yml` main push 실행 `37090327999`(`f1f3e54d9fd02331b6388bbd58b1dd1b65da3c51`, success), 잡 `preprocess-tests` 단계 "Run preprocess test binary as one process": 위 표의 EPYC 줄, 라운드별 검출 71.3/74.6/71.5 ms, 기준 커널 40.5/43.0/40.5 ms, 비율 1.761/1.738/1.768(한계 1.450 → `perf-gate-OVER … ASSERTION SUSPENDED`), `perf-gate-abs 3072 best=74.6 ms budget=400.0 ms`. 발췌: `evidence/ci_run_perf_gate_lines.txt`.
- `benchmark-regression.yml` push 실행 `37090328006`(같은 SHA, success): ctest 20개 모두 통과, 후처리/표시/GSVG 만. 목록: `evidence/benchmark_run_tests.txt`. 전처리 시험 0.
- 로컬 같은 시험(i7-12700, 20 논리 코어, 대역폭 27.3 GB/s): 반복 최소 107.1/93.8/92.5 ms, 비율 2.175/1.860/1.887(`evidence/local_perf_gate_3072.txt`). CI 와 비율 방향이 같다(1.45 초과). 로컬이 CI 보다 느린 것은 이 시점에 다른 빌드가 같은 기계에서 돌고 있었을 수 있어 기계 차이로 읽지 않는다 — **상한 추정**.

## 6. 빈칸 정리 (결정은 리더)

1. 전체 500 ms 를 재는 단언이 없다 — 꺼진 시험은 켜도 재지 못하므로 "켜기"가 해법이 아니고, 보정을 적재한 전체 경로를 재는 시험이 새로 필요하다. 판정선은 새로 정하지 않았다.
2. 단계별 예산 6개 중 단언이 있는 것은 0개다(런타임 검출기는 Defect 단계의 일부이며 한도가 SRS 값의 4.2배).
3. 벤치마크 워크플로는 전처리를 포함하지 않는다. 이름의 "BP-06~09"는 후처리 팩이다(BP-01~05 전처리 팩은 `benchmark/BP-01-05-preprocess-manifest.md` 에 있고 DegradedMode 64×64 6건이 그 증거).
4. RTM 의 PERF-001 행은 "Load all calibration maps within 200 ms (startup)" 로 SRS 의 PERF-001 과 다른 문장을 적고 있다(`RTM-CALIB-001…md:126`) — #195 의 번호 어긋남과 같은 부류일 수 있다. 이번에 확인하지 않았다.

## 7. 미검증 (Gaps)

- 전체 파이프라인을 보정 맵을 적재한 채 3072² 에서 재지 않았다. 그래서 "500 ms 를 지키는가"는 모른다. 로컬/CI 어느 쪽 값도 없다.
- 단계별 시간(offset·gain 등)도 재지 않았다. 꺼진 `A161/A166/A172/A184` 프로브가 일부를 재지만 실행하지 않았다.
- 시험 목록에서 시간 단언을 찾은 방법은 `grep` 이다(`EXPECT_(LT|LE)(… ms|Budget …)` 형). 다른 형태의 시간 단언(예: 변수에 담았다 비교)이 있다면 놓쳤을 수 있다. 대조군: 같은 grep 이 `test_integration.cpp:181`, `test_preprocess_degraded.cpp`(6건), `test_runtime_detection_performance_gate.cpp:621` 을 찾아냈다.
- PERF-003 의 단언 위치는 찾지 못했다(`PERF-003`·`200 ms` 인용 grep 0건) — "없다"가 아니라 "찾지 못했다".
- 기계: 벤치마크 워크플로의 러너 CPU 는 로그에 없다. CI preprocess 잡의 EPYC 9V45(4 vCPU)는 SRS 의 "Intel Core i7 or equivalent" 와 다른 기계이며, 요구가 말하는 기준 하드웨어(i7)에서의 측정은 이 보고서에 로컬 i7-12700 반복 최소값(위)뿐이다.
