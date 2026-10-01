# QA-A-175 (`#230`) — VVP 의 `REQ-P1A-013` 행을 실제 시험과 대조: 149행의 "1000-frame synthetic injection test" 는 없습니다

조사 카드입니다. 제품 코드·시험 코드·`docs/` 는 건드리지 않았습니다. VVP·이력표는 리더가 고칩니다.

**조건**: 트리 `2d1d8615`(`main` 을 `dev/preprocess` 에 병합한 뒤), 빌드 `build/ci-preprocess` RelWithDebInfo. 캐시–프리셋 대조 `12 declared, 12 compared, 0 not comparable — OK`, `PRESET_EXIT=0`(`evidence/02`). 병합 전 `main` 과의 차이는 `origin/main` 이 36 커밋 앞서고 이쪽은 0 이라 fast-forward 였고, 병합 뒤 `Test-TrackedTextFiles.ps1` 은 **`LINT_EXIT=0`, "Tracked text file validation passed"** — 이전의 1197건이 사라졌습니다(원인이 브랜치 시차였다는 리더의 설명대로입니다). 병합이 `modules/preprocess`·`modules/common`·`cmake` 를 바꾸지 않았음은 `git diff --stat 846751b..HEAD` 가 빈 출력인 것으로 확인했고, 그래서 병합 전에 지은 시험 실행 파일을 그대로 썼습니다.

---

## 1. 결론 — VVP 행마다

| # | VVP 위치 | VVP 가 적은 것 | 판정 | 한 줄 근거 |
|---|---|---|---|---|
| 1 | 149행 검증 방법 | "1000-frame synthetic injection test" | **시험 없음** (§2) | 있는 것은 **한 프레임** 1024×1024 에 **961곳** 주입하는 시험입니다. 1000프레임을 도는 시험은 어디에도 없습니다 |
| 2 | 149행 TPR | TPR ≥ 99.9% @ **10σ** | **일치** (임계값) | `kTprFloor = 0.999`, `TprReachesTheFloorAtTenSigma` 가 `EXPECT_GE`. SPEC `spec.md:278` 과 같은 수 |
| 3 | 149행 TPR 현황 | "not met on striped frames (0.9865)" | **일치** (수치), **빠진 것 있음** | 줄무늬 프레임 `0.986472` 가 `KnownDivergence_StripedFrameMissesTheTprFloor` 에 고정돼 있고 실행 출력도 같습니다. 그러나 균일 프레임에서는 **`1.000000` (961/961) 로 충족**이고 VVP 는 이를 적지 않았습니다 |
| 4 | 149행 FPR 기준 | FPR < 0.001% | **일치** (임계값) | `kFprCap = 1e-5`, SPEC 과 같은 수 |
| 5 | 149행 FPR 현황 | "FPR met (7.82e-06)" | **불일치** (출처) | 충족은 맞지만 **7.82e-06 은 이 시험이 내는 수가 아닙니다.** 시험은 단일 시드 한 프레임에서 `5/1,048,576 = 4.768e-06` 을 냅니다. 7.82e-06 = `41 ÷ (5 × 1024²)`, 곧 QA-A-162/163 의 **다섯 시드를 합친 프로브 값**이고 시험 안에는 주석(`rates.cpp:283-284`)으로만 있습니다 |
| 6 | 150행 성능 | "≤ 1.3x 측정 하한 … **Cannot be judged**" | **일치** ("판정 불가") / **시험 없음** (기준 자체) | 판정 불가는 `QA-A-174` 와 맞습니다. 그리고 "1.3배 하한" 을 단언하는 시험은 **없습니다** (§4). 성능 시험은 있으나 다른 것을 단언합니다 |
| 7 | 83행 L1 단위 | `test_runtime_detection_avx2_parity.cpp` — **4**건 | **불일치** (개수) | 파일에는 `TEST` 12개(`Avx2ParityTest` 11 + `DetectBuildParityTest` 1), 이 빌드는 **11개** 등록·실행해 `11 tests … PASSED` (§5) |
| 8 | 91행 `REQ-SIMD-004` | 같은 파일 **4** (bit-identical), VERIFIED 2026-05-09 | **불일치** (개수) | 위와 같음. 통과 자체는 오늘 확인(11/11) |
| 9 | 92행 DegradedMode | `test_preprocess_degraded.cpp` **6** — 6/6 PASS | **일치** (개수·통과) | 6개, 실행 `6 tests … PASSED`. **단 `REQ-P1A-013` 의 증거는 아닙니다** (#10) |
| 10 | 235행 BP-05 | `REQ-P1A-013` ← "DegradedMode 6/6 PASS" | **불일치** (근거 부적합) | 6개 시험은 `xpe_defect_detect_runtime` 을 한 번도 부르지 않습니다 (§6) |
| 11 | 230·234행 BP-04 | `REQ-P1A-012, 013` — "See manifest Section 5.4" | **시험 없음** | 매니페스트 §5.4 는 기준을 정의하지만 데이터셋 디렉터리·실행 스크립트가 **없습니다** (§7) |
| 12 | 53행 매트릭스 | L1 ✓ · L2 비움 · L3 ✓ · L4 "✓ (BP-04 runtime)" | L1 **일치**, L2 **일치**(비어 있음이 맞음), L3·L4 **시험 없음** | 탐지기를 부르는 통합·시스템·벤치마크 시험을 찾지 못했습니다 (§8) |
| 13 | 236행 SIMD 패리티 | "10 cases across 4 files (`ctest -R AVX2Parity`)" | **불일치** | 그 패턴은 **6건**(Offset 3 + Gain 3)만 잡습니다. 런타임 검출 패리티는 철자가 `Avx2Parity` 라서 **그 패턴에 안 걸립니다** (§5) |

**한 문장**: 149행의 검증 방법은 존재하지 않는 시험을 가리킵니다. 실제 시험은 `test_runtime_detection_rates.cpp` 이고 VVP 어디에도 이름이 나오지 않습니다(§2의 대조군 포함).

---

## 2. "1000-frame synthetic injection test" — 이름이 아니라 단언을 읽었습니다

### 실제로 있는 시험

`modules/preprocess/tests/test_runtime_detection_rates.cpp` (`QA-A-40`, `RuntimeDetectionRatesTest` 6건, `CMakeLists.txt:449` 등록, `DISABLED_` 아님). 읽은 단언과 실행 출력(`evidence/03_rates_run.txt`, `RATES_EXIT=0`, `6 tests … PASSED`):

| 항목 | 값 | 근거 |
|---|---|---|
| 프레임 수 | **1프레임** (측정마다 한 번) | `measure()` 가 `cleanFrame(sigma, seed)` 한 번, 반복 루프 없음. `rates.cpp:143-170` |
| 프레임 크기 | **1024 × 1024** | `kW = kH = 1024` (`:60-61`). VVP·SPEC 의 3072² 가 아님 |
| 주입 | **961곳** (31×31 격자, 간격 32, 가장자리 여백 16) | 출력 `injected=961`, `defectSites()` (`:91-99`) |
| 진폭 | **+10σ** (`TprReachesTheFloorAtTenSigma`), 5σ·6σ·8σ 는 보고용 | 출력 `10 sigma … TP=961 TPR=1.000000` |
| 노이즈 | 균일 가우시안, σ = 10 ADU (5σ·FPR 은 σ = 50 도 같이) | `measure(10.0f …)`, 시드 `20260911` 고정 |
| 부호 | 양의 방향만 | `withDefects[s] += amplitudeSigma * noiseSigma` (`:151`) |
| FPR 프레임 | 같은 생성기, 주입 없음, 1,048,576 화소 | 출력 `FP=5/1048576 FPR=0.000004768` |
| CI 에서 도는가 | **워크플로를 읽어 확인**: `ci.yml:185` 가 `ctest --test-dir build/ci-preprocess` 를 제외 규칙 없이 실행 | **실제 CI 로그에서 관측하지는 않았습니다** (§9) |

### 부재 주장 — 근거와 대조군

"1000프레임 주입 시험이 없다" 는 부재 주장이므로 검색 범위와 대조군을 적습니다.

1. **문구 검색**: `1000-frame | 1000 frame(s)` 를 `*.md *.cpp *.h *.txt *.yml *.yaml` 에서 찾으면 `memory leak`·`stress` 문맥의 문서 20여 줄과 VVP 149행 **하나**가 나옵니다. **대조군**: 같은 검색이 실제로 존재하는 `Memory Leak (1000 frames)`, `1000-frame stress`, `1000 frame 연속 처리` 줄들을 찾아냅니다. 따라서 검색은 이 범위를 읽는 것이 맞고, 주입 시험 문맥의 "1000-frame" 은 VVP 149행 밖에 없습니다. `synthetic injection` 도 같은 방식으로 찾으면 VVP 149행 한 줄뿐입니다 (같은 도구가 다른 문서의 `injection test` 줄들은 찾아냅니다).
2. **탐지기를 부르는 시험 전부를 열어 반복 구조를 확인**했습니다(`evidence/09_detect_callers_scan.txt`): `xpe_defect_detect_runtime` 호출 시험 파일 14개 중 프레임 수를 반복하는 루프는 **0개**입니다. 루프로 잡힌 곳(성능 게이트의 `rep`, `test_zz_a172_path.cpp` 의 라운드, 그리고 호출 목록 밖이지만 함께 읽은 `thread_parity` 의 `rep < 4`·`rep < 8`)은 모두 **같은 프레임을 되풀이하는** 타이밍·결정성 반복입니다. **대조군**: 같은 검색식이 주입 문구가 실제로 있는 `rates.cpp` 에서는 14줄을 찾아냅니다.
3. **VVP 가 그 문장을 적은 시점**: 149행은 `9835c9e9` (2026-04-19, `docs(sync)`) 에서 들어왔습니다. 그 커밋의 트리에서 탐지기를 부르는 시험은 `test_defect_correct.cpp`(널 검사·인자 가드)와 `test_xpe_preprocess.cpp`(**한 화소 이상치** `data[…] = 10000.0f`) 둘뿐이었습니다. **대조군**: 같은 `git grep` 이 그 두 파일을 찾아냅니다. 즉 **VVP 가 쓰인 때에는 TPR/FPR 주입 시험이 아직 없었습니다.** `rates.cpp` 는 `5e9fe848` (2026-09-11) 에서 생겼습니다.

### "1000" 은 어디서 왔는가 — **가설이며 확인하지 않았습니다**

저장소의 "1000-frame" 은 전부 메모리 누수·스트레스 시험 문구입니다. 그 문구가 검증 방법 칸에 옮겨졌을 수 있고, 실제 주입 수가 **961**(≈1000)인 것도 겹칩니다. 어느 쪽인지는 커밋 메시지·이슈로 확인하지 못했습니다.

### 시험이 무엇을 못 보는가 (VVP 문구와의 차이)

- **"임상 프레임"이 아닙니다.** SPEC 의 FPR 문구는 *clean clinical frames*, *< 9 false pixels per 3072²* 인데, 시험은 합성 가우시안 1024² 에서 `< 1e-5`(=10.5 화소) 로 단언합니다. 임상 데이터는 쓰지 않습니다.
- TPR 단언은 **σ = 10, 시드 하나**에서만 걸립니다. σ = 50 의 10σ 는 단언하지 않습니다(5σ 에서 두 노이즈 수준의 TP 가 같다는 불변성만 `RatesAreInvariantUnderNoiseScaling` 이 단언).
- 6σ·8σ 는 보고만 하고 게이트하지 않습니다(`rates.cpp:327-328`).

---

## 3. TPR·FPR 기대값이 SPEC 현재 기준과 같은가

| 항목 | SPEC `spec.md` (현재) | 시험 | 판정 |
|---|---|---|---|
| TPR 진폭 | 10σ (2026-09-29 개정) | `TprReachesTheFloorAtTenSigma` 가 10σ | 일치 |
| TPR 하한 | ≥ 99.9% | `kTprFloor = 0.999`, `EXPECT_GE` | 일치 |
| FPR | < 0.001% | `kFprCap = 1e-5`, `EXPECT_LT` (두 노이즈) + 괄호 `< 5e-5` | 일치 |
| 맵 상한 | 합계 ≤ 1% | `CleanFrameStaysUnderTheOnePercentCeiling` | 일치 |
| 5σ | 임계와 같은 수라 ≈ 0.5 | `KnownDivergence_TprAtFiveSigmaIsBelowTheRequirement` 가 `< 0.999` 와 `0.40 < TPR < 0.90` 를 **고정**, 출력 `0.507804` | 일치 (미달을 고정) |
| 줄무늬 | — | `KnownDivergence_StripedFrameMissesTheTprFloor` 가 `< 0.999` 와 `> 0.95`, 출력 `0.986472` | VVP 149행의 `0.9865` 와 일치 |

**부수 발견 (VVP 밖, 읽기만)**: SPEC `spec.md` 의 *Informative* 문단은 "shipping algorithm … **0.9865 @10-sigma** … does NOT meet" 라고 적는데, 시험은 균일 프레임에서 `1.000000`, **줄무늬에서만** `0.986472` 입니다. 두 수는 서로 다른 프레임의 값이라 문단이 `QA-A-164` 이전 상태로 남았을 가능성이 있습니다. SPEC 은 리더 소유이고 이 카드는 VVP 만 범위라 문단 전체를 대조하지는 않았습니다.

---

## 4. 성능 행 (150행)

`RuntimeDetectionPerformanceGateTest` 2건이 있고 실행했습니다 (`evidence/08_perfgate_run.txt`, `PERFGATE_EXIT=0`, `2 tests … PASSED`):

| 시험 | 단언 | 이번 실행 |
|---|---|---|
| `Frame3072SquaredWithinMachineRatio` | 비율(검출 ÷ **시험 안에 고정된 참조 커널**) 단언은 `QA-A-119` 로 **중단**(`test_runtime_detection_performance_gate.cpp:556-612`). **살아 있는 단언은 절대 상한 `≤ 400 ms`** (`kAbsoluteBudget3072Ms`, `:411`) | 비율 `1.856` (한도 `1.450` 초과, 출력 `perf-gate-OVER … ASSERTION SUSPENDED`), 절대 `92.4 ms ≤ 400 ms` 통과 |
| `Frame1024SquaredMachineRatioDiagnostic` | 단언 없음(진단) | 비율 `0.208` |

- `kImprovementTargetMs = 60` 은 **출력만** 합니다 (`현재 1.5배`).
- **"측정된 하한의 1.3배" 를 단언하는 시험은 없습니다.** 하한 `76 ms` 는 `DISABLED_` 프로브(`test_zz_a166_bounds.cpp`)와 보고서에만 있고, 게이트의 상수는 `1.45`(참조 커널 대비, 중단)와 `400 ms`(절대)입니다. 그리고 `QA-A-174` 가 보였듯 그 하한에는 독립 항이 없어 기준 자체가 판정되지 않습니다.
- 그러므로 150행의 "Cannot be judged" 는 맞고, 다만 **"성능 시험이 없다" 가 아니라 "기준을 단언하는 시험이 없다 + 회귀를 잡는 절대 상한만 산다"** 가 정확한 서술입니다. 시험 이름이 맞아 보이는 (`…WithinMachineRatio`) 점이 오독의 소지입니다.

---

## 5. 패리티 (83·91·236행)

실행: `Avx2ParityTest.*:DetectBuildParityTest.*` → `11 tests from 2 test suites … PASSED` (`evidence/10_parity_run.txt`).

| | VVP | 실제 |
|---|---|---|
| `test_runtime_detection_avx2_parity.cpp` | 4 | 소스 `TEST` 12개 (`Avx2ParityTest` 11, `DetectBuildParityTest` 1), 이 빌드 **11개**(`NoAvx2PathCompiledIn` 은 AVX2 빌드에서 제외) |
| 같은 표의 다른 `REQ-P1A-013` 시험 | 없음 | 런타임 검출 이름 시험 **52건**: `RuntimeDetectionFunctionalTest` 18, `…ErrorTest` 10, `…RatesTest` 6, `…PerformanceGateTest` 2, `RuntimeDetection` 5, 패리티 11 등 (`evidence/05`) |
| `ctest -R AVX2Parity` | "10 cases across 4 files" | **6건** (`OffsetCorrectAVX2ParityTest` 3 + `GainCorrectAVX2ParityTest` 3). 대조군 `-R Avx2Parity` 는 **10건** (런타임 검출). **대소문자 때문에 VVP 의 명령은 런타임 검출 시험을 잡지 못합니다** |

**부수 발견 (같은 표, `REQ-P1A-013` 밖)**: 88·89행은 `test_offset_correct_avx2_parity.cpp`·`test_gain_correct_avx2_parity.cpp` 를 각각 "2" 로 적는데 `ctest -N` 은 각 3건입니다. 90행의 `test_defect_correct_avx2_parity.cpp` 는 **파일이 없습니다**(`CMakeLists.txt:369` 주석: `QA-A-145` 가 `test_defect_correct_determinism.cpp` 로 개명, 보정에는 AVX2 경로가 없음).

---

## 6. DegradedMode (92·235행)

6건 모두 실행해 통과했습니다 (`evidence/06_degraded_run.txt`, `6 tests … PASSED`). 그러나 **탐지기를 한 번도 부르지 않습니다**: 파일에서 `xpe_defect_detect_runtime` 호출 0건, 부르는 것은 `xpe_offset_correct`, `xpe_gain_correct`, `xpe_defect_correct`(보정), `xpe_ghost_*`, `xpe_temp_compensate`, `xpe_nonlinearity_correct`. **대조군**: 같은 검색식이 이 함수들은 찾아냅니다.

- 235행이 `REQ-P1A-013` 의 근거로 든 "BP-05 DegradedMode" 시험은 온도 보상 기준값 항등과 비선형 널 설정(`:15`, `:285-289`)입니다. 시험 주석이 이 둘을 `REQ-P1A-013` 이라 적는데(`test_preprocess_degraded.cpp:289, 331`), SPEC 의 `REQ-P1A-013` 은 **런타임 결함 검출**(`spec.md:264`)이고 온도 보상은 `REQ-P1A-080` 입니다. 시험 주석의 요구사항 번호가 틀렸고 VVP 가 그것을 물려받은 것으로 보입니다 (**유래는 확인하지 않았습니다**).
- 매니페스트는 BP-05 를 "lag history, **Out of Pre Lane scope**" 로 적고(`manifest:301`), VVP 235행은 BP-05 를 "DegradedMode stress" 라 부릅니다. 이름이 서로 다릅니다.
- 시험 시간이 각 `0 ms` 라 "stress" 는 아닙니다.

**부수 발견 (VVP 문서 구조)**: 227~230행과 231~234행이 **같은 표 행의 중복**이고, 231행(BP-01)의 결과 칸이 `DegradedMode 6/6 PASS (Frozen 2026-04-22)` 로 235행(BP-05)의 것과 같습니다. 복사 오류로 보입니다.

---

## 7. BP-04 매니페스트 (230·234행)

`benchmark/BP-01-05-preprocess-manifest.md` §5:

- 데이터셋: **10장**의 임상급 3072² UINT16 기반 프레임, 프레임당 100 고립 + 20 선 + 10 클러스터(3×3) + 5 클러스터(5×5), 런타임 도전으로 "추가 50개 무작위 과도 결함"(`:187-191`). **"1000" 이 아닙니다.**
- 기준(`:193-202`): TPR ≥ 99.9%, FPR < 0.001% — VVP·SPEC 과 같은 수.
- 재현성(`:206-208`): 데이터셋 `benchmark/datasets/BP-04-defect/`, 실행 `tools/benchmark/run_bp04.py`, 시드는 `0xBP04C0DE` **(placeholder, compute at freeze)** — 유효한 16진수도 아닙니다.
- **둘 다 없습니다** (`evidence/07_bp04_artifacts.txt`): `benchmark/datasets` 디렉터리 없음, `run_bp04*` 파일 0개, `BP-04*` 이름 디렉터리 없음. **대조군**: 같은 `find` 가 실재하는 `BP-01-05-preprocess-manifest.md` 를 찾아냅니다(참고로 `tools/benchmark` 디렉터리도 없습니다).
- 매니페스트 §`:301` 은 BP-04 를 "DegradedMode PASS ✅ … full dataset freeze pending" 으로 적는데, 그 DegradedMode 의 BP-04 시험은 **고스트 제로 래그 항등**(`test_preprocess_degraded.cpp:14`)이라 결함 검출과 무관합니다.

---

## 8. 매트릭스 53행의 L2·L3·L4

탐지기를 부르는 시험 파일은 저장소 전체에서 14개(`evidence/09`)이고 전부 모듈 단위 시험·프로브입니다. `test_integration.cpp` 에는 호출이 **0건**(L2 칸이 비어 있음과 일치). C# 쪽 `clients/ImageProcTest.IntegrationTests/PInvoke/XpePreprocessNative.cs:82` 와 `XpePreprocessReadinessProbe.cs:26` 은 **내보내기 이름 문자열**일 뿐 호출이 아닙니다. 모듈 안의 파이프라인 시험도 탐지기를 부르지 않는다는 점은 `performance_gate.cpp` 의 `QA-A-119` 주석이 이미 같은 말을 적었습니다(*"Integration.PipelinePerformance3072x3072 is DISABLED and does not call the detector"*). 따라서 L3·L4 에 해당하는 시험은 **찾지 못했습니다** (검색 범위: 위 호출 목록; "없다" 가 아니라 "이 범위에서 못 찾음"입니다).

---

## 9. 미검증 / 잔여 위험

- **CI 에서 실제로 도는지는 워크플로를 읽어서만 확인**했습니다(`ci.yml:177-185`). 실제 CI 로그에서 `RuntimeDetectionRatesTest` 6건의 통과를 관측하지 않았습니다. 마지막 CI 실행의 해당 줄을 읽기 전에는 "CI 에서 돈다"를 관측 사실로 쓰지 마십시오.
- **"1000" 의 유래는 가설입니다**(§2). 그 문구가 어떤 의도로 쓰였는지, 당시 계획된 시험이 따로 있었는지는 확인하지 못했습니다.
- `7.82e-06` 의 산식(`41 ÷ (5 × 1024²) = 7.82013e-06`)은 계산으로 맞췄고, 41건과 다섯 시드는 시험 주석(`rates.cpp:283`)과 `QA-A-163` 보고서 표에서 읽었습니다. **그 프로브를 이 카드에서 다시 돌리지 않았습니다.**
- 균일 프레임 `TPR 1.0000` 외에 `QA-A-164` 가 말한 "램프·단계 프레임에서도 충족" 은 이 카드에서 실행하지 않았습니다.
- `REQ-SIMD-004` 의 "VERIFIED 2026-05-09" 는 **오늘 11/11 통과**로 현재 상태만 확인했고, 그 날짜의 상태는 재현하지 않았습니다.
- SPEC·매니페스트의 다른 행은 `REQ-P1A-013` 관련만 읽었습니다.
- 위 검색의 범위는 `*.md *.cpp *.h *.txt *.yml *.yaml *.cs *.cmake CMakeLists.txt` 입니다. 이 밖의 형식(이슈 본문, 외부 문서)은 보지 않았습니다.

## 10. 하지 않은 것

제품·시험 코드 변경 없음, `docs/`·SPEC·VVP·이력표 수정 없음, push 없음, 새 이슈 없음.

🗿 MoAI
