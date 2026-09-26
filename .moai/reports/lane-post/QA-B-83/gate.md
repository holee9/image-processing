# QA-B-83 게이트 보고서 — T308 판독 (고치지 않음)

**카드**: QA-B-83 · **레인**: Lane B (`xpe-post`, `dev/postprocess`, HEAD `a4253be`)
**코드 변경 없음, 커밋 없음, 이슈 코멘트 없음**(보고서만). 빌드 없음(`ctest -N` 목록 조회만).

---

## 1. 주장

| # | 질문 | 결과 |
|---|---|---|
| 1 | T308 이 CI 에서 실행되는가 | **실행되지 않는다.** `ci.yml` 의 post 잡이 `-E "…|PerformanceBudget|…"` 로 제외하고, `benchmark-regression.yml` 의 `-R` 패턴에도 들어 있지 않다. 두 워크플로의 **실제 로그**에서 확인했다(대조군 포함) |
| 2 | CI 로그의 T308 측정값 | **없다** — CI 가 이 시험을 돌리지 않으므로 측정값도 없다. main `ci.yml` 성공 5회 로그에서 `T308` **0건**, `Performance: …ms for 1024x1024` **0건** |
| 2' | fractional 성능을 재는 CI 게이트가 따로 있는가 | **찾지 못했다.** benchmark 워크플로의 5개 시험 중 `FullPipelineE2E.PostProcess_3072x3072_Within3000ms` 는 `xpe_fractional_process` 를 호출하지 않는다(파일 판독) |
| 3 | T308 이 재는 것 | 1024×1024 FLOAT32(왼쪽 반 0.5 / 오른쪽 반 1.0 계단), `xpe_fractional_process(&img, 1.2f, nullptr)` **1회**, 워밍업 없음, `high_resolution_clock` 벽시계, **밀리초로 절사**(`duration_cast<milliseconds>`), 단일 측정값이 문턱 `< 100` 인지 |
| 3' | 100 ms 의 근거 | **SPEC 에 없다.** REQ-ADV-061 은 **3072×3072 에서 400 ms(scalar) / 120 ms(AVX2)** 다. 시험 주석은 그것을 1024² 로 환산해 **≈44 ms** 라고 적고, 이어 "Use generous threshold for test environment variability" 로 **100** 을 쓴다. 100 은 주석의 판단이고 요구의 값이 아니다. 두 줄 모두 `10b5551`(2026-04-18 스켈레톤)이 마지막으로 고쳤다 |
| 4 | B-76 이후 enhance_advanced 경로를 바꾼 커밋 | `1b752e3`(QA-B-79, `parse_mfp_config` 의 키 읽는 순서) · `6221931`(QA-B-80, 호출처 없는 `fromJson` 삭제). **둘 다 `xpe_fractional_process` 실행 경로의 코드가 아니다**(판독) |

## 2. CI 실행 여부 — 판독과 로그

### 2.1 워크플로 판독

- `.github/workflows/ci.yml:261-263` (post 잡):
  ```
  - name: Run tests (timing-budget cases excluded)
    run: ctest --test-dir build/ci-post ... -E "Performance|Within[0-9]+ms|PerformanceBudget|LargeImagePerformance"
  ```
  - 주석(`:256-260`): "on a shared runner they measure the machine, not the code … Timing regressions are the benchmark workflow's gate."
  - 이 정규식은 `cmake/XpeCoverage.cmake:29` 의 `XPE_COVERAGE_EXCLUDE_TESTS` 와 같다.
- `.github/workflows/benchmark-regression.yml:63-65`:
  ```
  $pattern = 'FullPipelineE2E\.PostProcess_3072x3072_Within3000ms|BenchmarkFreeze|CollimationDetectTest\.BenchmarkFreeze_BP07_…|ExposureIndex\.BenchmarkFreeze_BP0[89]_(EI|DI)CalcTimeBaseline'
  ctest ... -R $pattern
  ```
  → `EdgeEnhancementTest.T308_PerformanceBudget` 는 이 패턴에 걸리지 않는다.
- `ctest` 를 돌리는 다른 워크플로: `windows-common-build.yml:85` 뿐이고, 대상은 `build/ci-common` 이다. 나머지 워크플로(`release-bundle`, `delivery-bundle`, `codeql`, `docs-generate`, `repository-guard`, `label-sync`)에는 grep 결과 ctest·ci-post 가 없다.

### 2.2 로컬 목록 대조 (`_ctest_all.txt`, `_ctest_ci_filter.txt`)

```
ctest --test-dir build\ci-post -N                                → Total Tests: 535, T308 1건
ctest --test-dir build\ci-post -N -E "<ci.yml 의 정규식>"          → Total Tests: 521, T308 0건
```

### 2.3 CI 로그 (`_ci_scan.txt`, `ci_logs/`)

`gh run view <id> --log` 로 받았다. main `ci.yml`, 결론 success, 최신 5회. 모두 `a4253be` 이전이다(`a4253be` 는 아직 main 에 없음).

| run | head | T308 | **대조군** `T306_OrderValidation` | `Performance: …1024x1024` | ctest "out of" |
|---|---|---|---|---|---|
| 35176868070 | ead6c64 | **0** | 2 | 0 | 515 / 647 / 69 |
| 35176092896 | 8976f73 | **0** | 2 | 0 | 647 / 69 / 515 |
| 35174939593 | d0737d8 | **0** | 2 | 0 | 515 / 69 / 647 |
| 35174139778 | 15f415b | **0** | 2 | 0 | 647 / 515 / 69 |
| 35172821436 | 7d42ba7 | **0** | 2 | 0 | 647 / 69 / 515 |

- 대조군 T306(같은 파일, 같은 픽스처)은 매 실행 `post-build / Run tests (timing-budget cases excluded)` 단계에 `Start 209: EdgeEnhancementTest.T306…` 와 결과 줄, 2줄로 나온다. **이 로그에서 같은 스위트의 시험 이름을 찾을 수 있다**는 뜻이다.
- benchmark 워크플로 최신 성공(`35177717635`, 83a7c2d): 485줄, `T308` **0건**, `BenchmarkFreeze` 10건, "100% tests passed out of 5".

## 3. T308 이 재는 것 (`modules/enhance_advanced/tests/test_edge_enhancement.cpp:607-640`)

```cpp
int width = 1024;  // Smaller than full 3072 for faster test
int height = 1024;
XpeImageBuffer img = createFloatImage(width, height, 0.5f);
for (y) for (x = width/2 .. width) data[...] = 1.0f;          // 계단 에지
auto startTime = std::chrono::high_resolution_clock::now();
XpeErrorCode result = xpe_fractional_process(&img, 1.2f, nullptr);
auto endTime = ...;
auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
// Original budget: 400ms for 3072x3072
// Scaled budget: 400ms * (1024/3072)^2 ≈ 44ms
// Use generous threshold for test environment variability
EXPECT_LT(duration.count(), 100)
```

- 반복 1회, 통계 없음(평균·최솟값·최댓값 아님), 워밍업 없음이다.
- config 가 `nullptr` 이라 `iterations` 는 기본값 1 이다(`XPE_FRAC_DEFAULT_ITER`).
- `duration_cast<milliseconds>` 는 절사한다. 실제 99.9 ms 는 `99` 로 통과하고, 100.0 ms 는 `100` 으로 실패한다.
- 픽스처 생성, 첫 호출에서 할 수 있는 초기화 비용(있다면)이 측정 구간 안에 들어가는지는 확인하지 않았다.

## 4. 문턱의 근거

- **REQ-ADV-061** (`.moai/specs/SPEC-XPE-P2-ADV/spec.md:264-269`): "When `xpe_fractional_process` is called on a **3072x3072** FLOAT32 image, execution shall complete within **400ms (scalar) or 120ms (AVX2)**." — SRS PERF-100
- 1024² 에 대한 문턱은 SPEC 에 없다. 시험 주석의 환산값은 44 ms 이고, 실제 문턱 100 은 주석의 "generous" 판단이다.
- `git blame -L 636,637` → 두 줄 모두 `10b5551` (2026-04-18). 시험 본문의 `git log -L` 이력도 `10b5551` 하나뿐이다.
- 3072² 를 직접 재는 fractional 시험은 찾지 못했다. 이름 패턴 `Perf|Within…ms|BenchmarkFreeze` 로 `modules/`·`tests/` 의 `TEST(_F)` 를 grep 했고, enhance_advanced 에서 걸린 것은 T308, T508, T603b, T608, `LargeImagePerformance`, `BenchmarkFreeze_BP07` 이다. T603b·T608 이 fractional 을 포함하는지는 이번에 읽지 않았다.

## 5. B-76 이후 커밋 (`git log fbd20cc..HEAD -- modules/enhance_advanced`)

```
6221931 2026-09-17 refactor(enhance_advanced): 호출처 없는 MfpConfig/FractionalConfig::fromJson 을 선언까지 삭제
1b752e3 2026-09-17 fix(enhance_advanced): num_levels 와 levels 가 함께 오면 새 이름 num_levels 가 이긴다
```

- `1b752e3`: `parse_mfp_config`(multiscale 경로)의 두 `if` 순서만 바꿨다.
- `6221931`: 호출처 없는 두 함수를 삭제했다(QA-B-79 링커 확인으로 호출처 0). `fractional_derivative.cpp` 에서는 `FractionalConfig::fromJson` 정의와 `nlohmann/json` include 가 빠졌다.
- 로컬 측정: B-76 때 92 / 92 / 93 ms, B-82 때 96~101 ms. 두 커밋이 이 차이를 만들었는지는 **재지 않았다**(카드 지시: 로컬 수치로 판정하지 않음).

## 6. 미검증

- T308 의 CI 측정값 — CI 가 돌리지 않으므로 존재하지 않는다.
- 로컬 92 → 96~101 ms 차이의 원인.
- T603b·T608 이 fractional 을 포함하는지, 그리고 그 시험의 문턱.
- CI 로그는 `ci.yml` 5회, benchmark 1회만 봤다.
- 로컬 전체 목록(535)과 CI post 목록(516, "out of 515" + 비활성 1) 차이의 내역은 맞춰 보지 않았다(브랜치 차이와 제외 패턴이 섞여 있다).

## 7. 잔여 위험

- **REQ-ADV-061(3072² 에서 400 ms)을 CI 에서 확인하는 시험이 없다.** 로컬 T308 은 크기를 줄인 대리 측정이고, 제외 목록에 들어 있다.
- 로컬 T308 은 문턱 경계에 있어, 로컬 전체 ctest 의 결과를 흔든다.

## 부록 — 증거

| 파일 | 내용 |
|---|---|
| `_ci_scan.txt` | CI 로그 5회 스캔 결과 |
| `ci_logs/ci_*.log` | `ci.yml` 실행 로그 5개 |
| `ci_logs/bench_35177717635.log` | benchmark 실행 로그 |
| `_ctest_all.txt` / `_ctest_ci_filter.txt` | 로컬 `ctest -N` 목록 (제외 전/후) |
