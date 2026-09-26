# QA-B-84 (#179) 게이트 보고서 — fractional 3072² 측정 전용 시험 · AVX2 경로 판독

**카드**: QA-B-84 · **레인**: Lane B (`xpe-post`, `dev/postprocess`)
**커밋**: `0dbfd36` (`Refs #179`) — `modules/enhance_advanced/tests/test_edge_enhancement.cpp` 1파일 · push 없음
**BUILD_EXIT**: 세 프리셋 모두 `BUILD=0` / `EXIT=0`, ctest **535 / 224 / 192**, 실패 0 (`_verify.log`)

---

## 1. 주장

| # | 항목 | 결과 |
|---|---|---|
| 1 | 측정 전용 시험 | `EdgeEnhancementTest.BenchmarkFreeze_ADV061_FractionalMeasure3072` — 3072² 계단 에지, order 1.2, 워밍업 1 + 7회, µs 한 줄 출력, 시간 단언 없음 |
| 2a | 벤치마크 패턴으로 선택되는가 | **예** — `ctest -N -R '<benchmark-regression.yml 패턴 그대로>'` 6건 중 하나 |
| 2b | `ci.yml` 의 `-E` 에 걸리는가 | **아니다** — 제외 후 518건에 포함. ci.yml post 잡에서도 돈다(로컬 7.6 s 추가) |
| **2c** | **워크플로 형식에서 수치가 로그에 나오는가** | **나오지 않는다 (0줄).** `ctest --output-on-failure` 는 통과한 시험의 출력을 찍지 않는다. 직접 실행하거나 `ctest -V` 로 돌리면 1줄 나온다. **병합 후 워크플로를 그대로 돌려서는 #179 에 옮길 수치가 로그에 남지 않는다** — §3 |
| 3a | AVX2 경로가 지금 있는가 | **별도 AVX2 코드 경로는 없고, 있었던 기록도 없다.** 다만 전역 `/arch:AVX2` 가 이 파일에도 적용된다 — §4 |
| 3b | T603b·T608 이 fractional 을 포함하는가 | **둘 다 포함** — 512² 에서, 시간 단언과 함께. CI 에서는 제외된다(이름 패턴) |
| 4 | 로컬 수치 | min 889296 / med 901251 / max 940262 µs — **판정에 쓰지 않는다** |

## 2. 시험

`test_edge_enhancement.cpp` 끝에 추가했다(64줄). `<algorithm> <cstdio> <string>` include 를 더했다.

- 입력: T308 과 같은 모양(왼쪽 0.5, 오른쪽 반 1.0)을 3072×3072 로 만들었다.
- 호출은 in-place 이므로 매번 원본 복사본(`pristine`)을 다시 채운다.
- 시간: `steady_clock`, `duration_cast<microseconds>`.
- 워밍업 1회는 기록하지 않고, 7회를 정렬해 min / med(4번째) / max 를 낸다.
- 출력: `ADV061 fractional 3072x3072 min=%lld med=%lld max=%lld us (runs=7, order=1.2)` 와 `RecordProperty(ADV061_min_us / med_us / max_us)`
- 단언: 매 호출 `XPE_OK`, 마지막 출력에 NaN/Inf 없음. **시간 단언은 없다.**
- 이름: `BenchmarkFreeze` 를 넣고, `Performance`·`Within…ms` 는 넣지 않았다.

## 3. 선택과 로그 노출 (`_run.log`, `_bench_as_workflow.txt`, `_ctest_verbose.log`)

```
===SELECT_BENCH===   (benchmark-regression.yml:64 의 패턴 그대로, -N)
  Test #131: ExposureIndex.BenchmarkFreeze_BP08_EICalcTimeBaseline
  Test #132: ExposureIndex.BenchmarkFreeze_BP09_DICalcTimeBaseline
  Test #210: CollimationDetectTest.BenchmarkFreeze_BP07_CollimationDetectionBaseline
  Test #222: EdgeEnhancementTest.BenchmarkFreeze_ADV061_FractionalMeasure3072
  Test #481: BenchmarkFreeze.BP06_GsvgVersionProbeBaseline
  Test #534: FullPipelineE2E.PostProcess_3072x3072_Within3000ms
Total Tests: 6
===SELECT_CI_EXCLUDE===   (ci.yml:263 의 -E 패턴 그대로, -N)
  Test #214: EdgeEnhancementTest.BenchmarkFreeze_ADV061_FractionalMeasure3072
Total Tests: 518
```

**로그 노출** — 같은 시험을 세 방식으로 돌리고 `ADV061 fractional` 줄 수를 셌다.

| 방식 | ADV061 줄 |
|---|---|
| **워크플로와 같은 명령** `ctest --output-on-failure -R '<패턴>'` (6/6 Passed) | **0** |
| 시험 실행 파일 직접 실행 `--gtest_filter=*BenchmarkFreeze_ADV061*` | 1 |
| `ctest -V -R BenchmarkFreeze_ADV061` | 1 |

- 벤치마크 워크플로(`benchmark-regression.yml:65`)는 `--output-on-failure` 를 쓴다. 그 뒤 업로드하는 것은 `build/ci-post/bin/*.dll`·`*.exe` 뿐이고(`:70-74`), `Testing/` 은 올리지 않는다.
- `RecordProperty` 값은 gtest XML 에만 남는데, 그 XML 을 만드는 옵션도 워크플로에 없다.
- 따라서 **병합한 뒤 워크플로를 그대로 돌리면 이 시험은 통과만 하고 수치는 어디에도 남지 않는다.**
- 수치를 남기려면 워크플로 쪽 변경이 필요하다. 예: 그 스텝에 `-V`, 또는 시험 실행 파일을 직접 실행, 또는 `--output-junit` + 업로드. **리더 소유 파일이라 바꾸지 않았다.** 어느 방식이 맞는지는 판정하지 않는다.

## 4. AVX2 경로 판독

- **코드**: `modules/enhance_advanced/src/**`·`CMakeLists.txt` 에서 `avx|simd|__m256|immintrin` grep **0건**이다.
  - 대조군: 같은 grep 을 `modules/` 전체에 돌리면 `enhance_basic`, `preprocess` 파일이 걸린다.
  - enhance_advanced 에서 걸린 것은 `tests/test_integration.cpp` 하나다. `:597-600` 예산 주석과, T609 주석 "AVX2 implementation will be added in Phase 5." 이다.
- **이력**:
  - `git log -G '_mm256|immintrin|__m256' -- modules/enhance_advanced include` → **0건**
  - 대조군: 같은 명령을 `modules/preprocess` 에 돌리면 `1c4e509`·`6c31996`·`ec0bb7e` 가 나온다.
  - 처음 쓴 `-S '__m256'` 은 enhance_basic 대조군에서도 0건이었는데, enhance_basic 은 인트린식이 아니라 자동 벡터화 방식이라 대조군으로 부적절했다. 그래서 preprocess 로 바꿨다.
  - **#160 계열 삭제(`1c4e509` simd_dispatch 등)는 preprocess 의 것이다.** enhance_advanced 에는 삭제된 AVX2 커널도 없다 — 이력상 **처음부터 없었다.**
- **컴파일 플래그**:
  - `cmake/Platform.cmake:6-10` 이 컴파일러 지원 시 **전역으로** `/arch:AVX2`(MSVC)·`-mavx2` 를 추가한다.
  - `build/ci-post/build.ninja` 의 `fractional_derivative.cpp.obj` FLAGS 에 `/arch:AVX2` 가 있다.
  - 즉 fractional 은 **하나의 스칼라 코드가 AVX2 명령어 생성이 허용된 채로** 컴파일된다. 런타임 분기(CPUID)나 별도 AVX2 구현은 없다.
- **REQ-ADV-061 과의 관계(사실)**: 요구는 "400ms (scalar) **or** 120ms (AVX2)" 로 두 경로를 전제한다. 저장소의 빌드는 두 경로를 구분하지 않고 한 가지 산출물만 만든다. 이 빌드를 scalar 로 볼지 AVX2 로 볼지는 **판정하지 않는다.**
- T609(`T609_SIMIDispatchPreparation`)는 이름과 달리 multiscale·fractional 이 `XPE_OK` 를 내는지만 본다.

## 5. T603b · T608 (`modules/enhance_advanced/tests/test_integration.cpp`)

| 시험 | 크기 | fractional | 시간 단언 | CI |
|---|---|---|---|---|
| `T603b_FullPipeline_PerformanceBudget` (:255) | 512² | `:284` order 1.2 | 파이프라인 전체 `< 500` ms (:293) | `PerformanceBudget` 에 걸려 제외 |
| `T608_PerformanceBudgetVerification` (:605) | 512² | `:644` order 1.0 | fractional `< 50` ms (:648), MFP `< 100` ms | 같음 |

## 6. 로컬 수치 — 판정에 쓰지 않음

| 실행 | min | med | max (µs) |
|---|---|---|---|
| 직접 실행 | 889296 | 901251 | 940262 |
| `ctest -V` | 848907 | 911760 | 962786 |
| ctest 전체 중 시험 소요 | 7.63 s (9회 호출 + 입력 생성) | | |

부하가 걸린 개발 기계의 값이다. REQ-ADV-061 과 비교하지 않는다.

## 7. 미검증

- CI 러너에서의 수치 — §3 때문에 워크플로 변경 없이는 얻을 수 없다.
- 자동 벡터화가 fractional 루프에 실제로 적용됐는지(`/Qvec-report` 등)는 보지 않았다.
- 워밍업 1회로 충분한지.
- 3072² 입력 2벌(원본 + 작업본, 약 72 MB)의 메모리 영향은 재지 않았다.

## 8. 잔여 위험

- ci.yml post 잡이 이 시험을 돈다. 로컬에서 약 7.6 s 가 늘었다. 단언이 없으니 실패하지 않지만, 잡 시간은 늘어난다.
- 이 시험 자체는 게이트가 아니다. 문턱이 정해지기 전까지 REQ-ADV-061 은 여전히 게이트가 없다.

## 부록 — 증거

`_env.bat`, `_b84.bat`, `_verify.bat`, `_run.log`, `_bench_as_workflow.txt`, `_ctest_verbose.log`, `_verify.log`
