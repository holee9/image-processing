# QA-B-174 — `T608_PerformanceBudgetVerification` 이 전체 실행에서 121 ms(예산 50 ms)

**보고만, 제품 코드 변경 없음.** 측정을 위해 시험에 임시 계측을 넣었다가 원복했다(`git status` 깨끗, 계측은 `temp_probe_T608.patch.txt` 로만 남김).

## 결론

**"부하 의존이라 측정 방식 문제"** — 실제 성능 문제는 아니다. 단, 이상치의 **직접 원인은 관측하지 못했고** 같은 조건에서 **재현하지 못했다.**

근거 (자세한 내용은 아래):
1. 이 시험은 호출 하나씩을 **벽시계로 딱 한 번** 잰다(워밍업·반복·통계 없음). 한 번의 스케줄링 지연이 그대로 판정이 된다.
2. 같은 코드를 36번 재서 분수 처리는 **6~10 ms(중앙값 7)**, 예산 50 ms 의 7배 여유. 실패한 121 ms 는 36개 표본 최댓값의 **12.1배**, 중앙값의 **17.3배**로 어느 정상 분포에도 속하지 않는 이상치다. 균일하게 느려진 것이 아니다.
3. 50 ms 는 요구에서 나온 숫자가 아니다. 요구는 3072² 용 숫자뿐이고, 시험의 50 ms 는 작성자가 고른 값이다(§①).
4. **이 시험은 CI 에서 한 번도 실행되지 않는다**(§③). 로컬 전체 실행에서만 만난다.

## ① 이 시험이 무엇을 재는가, 예산의 출처

`modules/enhance_advanced/tests/test_integration.cpp` `IntegrationTest.T608_PerformanceBudgetVerification`.

| 항목 | 내용 |
|---|---|
| 입력 | 512 × 512 FLOAT32 (`0.5 + 0.01·sin(i·0.1)`), 3072² 의 약 1/36 |
| 시계 | `std::chrono::high_resolution_clock` **벽시계**, 호출 전후 두 번 읽음. 결과는 `duration_cast<milliseconds>` 로 **ms 로 잘림**(EI 만 µs) |
| 반복 | **호출당 1회**. 워밍업 없음, 반복·중앙값·최솟값 없음 |
| 첫 호출 포함 | 예. 프로세스 첫 호출들이고 모듈 init 직후다 |
| 순서 | init → MFP → (이미지를 0 으로 되돌림) → **Fractional** → Collimation → EI. 분수 처리는 0 으로 채운 이미지에서 잰다 |
| 단언 | MFP `< 100` ms, **Fractional `< 50` ms**, Collimation `< 50` ms, EI `< 10000` µs |

**50 ms 예산의 출처:**
- 요구 원문은 `.moai/specs/SPEC-XPE-P2-ADV/acceptance.md` `PERF-ADV-002`: Fractional, **3072² F32**, `< 400 ms`(scalar) / `< 120 ms`(AVX2). 512² 에 대한 요구는 없다.
- 시험 주석은 "비례 환산 → 3072² 의 400 ms 는 512² 에서 **~11 ms**" 라고 적고, 코드는 **50 ms** 로 단언한다. 즉 50 ms 는 요구에서 환산한 값이 아니라 작성자가 고른 값(환산치의 약 4.5배)이며 **근거 문서가 없다.** AVX2 목표 120 ms 는 같은 스펙이 "이 모듈에 명시적 SIMD 코드가 없어 아직 해당 없음" 이라고 적었다.
- 같은 스펙(`spec.md`, #179)은 이미 두 가지를 적어 두었다: (a) 400 ms 는 **기준 하드웨어에서의 문턱**이다("어느 기계인지 말하지 않는 성능 요구는 어떤 기계에서도 통과시키거나 실패시킬 수 있다"). (b) **GitHub 공유 러너는 이 요구의 적합 판정 주체가 아니다**(개발 PC 실측 302 ms, 공유 러너 최소 417.5 ms).
- 개발 PC 의 3072² 실측 302 ms 를 1/36 하면 8.4 ms 로, 이번에 잰 512² 중앙값 7 ms 와 맞는다. 즉 선형 환산은 이 기계에서 대체로 성립한다.

## ② 전체 스위트 안의 위치, 분포

**위치:** `ctest` 번호 257 / 978. 앞뒤는 `T607_IndependentFunctionCalling`(256), `T609_SIMIDispatchPreparation`(258) — 둘 다 가벼운 같은 파일의 시험. ctest 는 시험마다 **새 프로세스**를 시작한다.

**병렬 실행:** 이 실행들에서 `ctest` 는 `-j` 없이 **직렬**이었다(우리 실행 스크립트). CI 도 `-j` 없이 돈다(`.github/workflows/ci.yml`). 즉 같은 `ctest` 안의 다른 시험이 부하를 만든 것은 아니다. **다른 프로세스**(같은 기계에서 도는 다른 레인의 빌드·시험)의 부하는 알 수 없다 — CPU 부하 표본 추출을 시도했으나 `_env.bat` 의 PATH 에 powershell 이 없어 비었다(Gaps).

**같은 조건 측정 (`stats.txt`, 원자료 CSV, 저장소가 `*.csv`·`*.patch` 를 무시해서 `.txt` 사본으로 보존):** 시험에 임시로 호출별 시간을 CSV 에 덧붙이는 계측을 넣고(`temp_probe_T608.patch.txt`) 측정한 뒤 원복했다.

| 조건 | 표본 | MFP ms | **Fractional ms** | Collimation ms | EI µs |
|---|---|---|---|---|---|
| A 단독, 새 프로세스 30회 | 30 | 7–9 (중 7) | **6–10 (중 7)** | 3–4 | 244–345 |
| B `IntegrationTest.*` 묶음, 같은 순서, 3회 | 3 | 7 | **6, 7, 7** | 3–4 | 246–268 |
| C 전체 스위트(978개), 3회 | 3 | 7 | **6, 6, 7** | 3–4 | 245–284 |
| **합계** | **36** | | **6–10 (중 7, p90 8)** | | |

**실패한 실행(`QA-B-173/first_full_run_T608_failure_ci_post_ctest.txt`):** Fractional **121 ms**, 시험 전체 143 ms.
- 정상 분포 대비: 최댓값(10)의 12.1배, 중앙값(7)의 17.3배, 예산의 2.4배. 정상의 6~10 ms 와는 겹치지 않는다.
- **같은 시험의 나머지는 대체로 정상**: MFP `< 100` 과 Collimation `< 50` 은 통과했고, 시험 전체 143 ms − Fractional 121 ms = 22 ms 는 정상 시험 전체(20~21 ms, 단독 5회) − 정상 Fractional(7 ms) = 약 14 ms 와 같은 크기다(차이 8 ms 는 실패 메시지 출력일 가능성, 재지 않음).
- **다만 완전히 정상은 아니었다**: 같은 실행의 Collimation 모듈 로그가 `total_time_ms 7`(정상 단독 6회 `3, 5, 3, 3, 4, 3`)로 약 2배 느렸다.
- 해석: 단일 스레드 구간(Collimation)이 약 2배, 기본 설정에서 **여러 스레드**를 쓰는 Fractional(스펙: `min(4, 논리 코어/2)`)이 17배 느린 비대칭은 "그 순간 코어를 다른 프로세스가 점유했다"는 설명과 맞는다. 다중 스레드 작업은 가장 늦은 스레드를 기다리므로 코어 경합에 가장 민감하다. **이것은 추정이다** — 스레딩 기전은 모듈 `src` 에서 `std::thread`/`async`/풀 패턴으로 찾지 못했고(병렬 헬퍼가 따로 있을 것) 부하는 직접 관측하지 못했다.

**단일 이상치인가 균일 저하인가:** 단일(또는 짧은 구간) 이상치. 측정한 36회에서 재현되지 않았고, 이 세션의 전체 `ci-post` 실행 9회 중 1회만 실패했다(나머지 8회 통과: QA-B-171C/173 의 `g171c-m·n·p·r·s` 5회는 각 보고서 폴더에 로그가 있고, 실패 1회는 위 파일, 이번 측정의 C 3회는 `C_full_suite_ctest_summaries.txt` 에 요약 줄만 보존). 균일 저하라면 Collimation·MFP 도 같은 비율로 느려야 한다.

## ③ main CI 에서의 최근 실행 시간

**없다 — 이 시험은 CI 에서 실행되지 않는다.**
- `.github/workflows/ci.yml` 의 `ctest` 단계: `-E "Performance|Within[0-9]+ms|PerformanceBudget|LargeImagePerformance"`. `IntegrationTest.T608_PerformanceBudgetVerification` 은 `PerformanceBudget` 에 걸려 **제외**된다. 주석이 이유를 적고 있다: "on a shared runner they measure the machine, not the code. First run failed exactly 3 such cases by 1-16 ms. Timing regressions are the benchmark workflow's gate."
- `benchmark-regression.yml` 의 패턴(`FullPipelineE2E…Within3000ms|BenchmarkFreeze|…`)에도 없다.
- 커버리지 타깃도 같은 정규식으로 제외한다(`cmake/XpeCoverage.cmake` `XPE_COVERAGE_EXCLUDE_TESTS`).
- 따라서 **CI 로그에는 이 시험의 실행 시간 이력이 없다.** 이 시험은 로컬 전체 실행에서만 돈다.

## 선택지 (구현하지 않음)

1. **그대로 둔다.** CI 가 이미 제외하고, 타이밍 회귀의 게이트는 벤치마크 워크플로다. 로컬 전체 실행에서 드물게 빨개질 뿐이다.
2. **시험을 견고하게 한다**(post 레인 소유, 카드로 가능): 워밍업 1회 + N회(예: 5) 중 **최솟값 또는 중앙값**으로 판정. 50 ms 는 그대로 둔다 — 이미 환산치(~11 ms)의 4.5배라 더 조일 이유가 없다. 단일 호출 이상치(이번 121 ms)가 판정에 영향을 주지 않게 된다.
3. **예산의 출처를 적는다**: 시험 주석에 "50 ms 는 작성자가 고른 값이고 요구의 환산치는 ~11 ms" 를 명시(지금은 주석이 "~11 ms" 만 적고 단언은 50 이라 서로 어긋나 보인다).

권장: 2 와 3 을 한 카드로(시험만, 제품 변경 없음). 1 도 정당하다 — 이 시험이 CI 게이트가 아니라는 사실이 중요하다.

## Gaps

- **이상치의 직접 원인은 관측하지 못했다.** 부하를 재현하려고 한 시도는 하지 않았다(범위 밖). "다른 프로세스의 코어 점유" 는 비대칭(Collimation 2배 vs Fractional 17배)과 맞는 추정이다.
- **CPU 부하 표본은 비었다.** 측정 스크립트의 환경 배치 파일이 PATH 를 초기화해서 powershell 을 못 찾았다. 실패한 실행 당시 다른 레인이 무엇을 돌렸는지는 알 수 없다.
- 측정은 이 기계(Windows 11, 개발 PC) 한 대에서만 했다. 느린 기계에서의 분포는 모른다(스펙의 공유 러너 실측은 개발 PC 대비 약 1.4배 느림 — 그 비율이라도 정상 분포는 50 ms 아래로 보인다. 계산: 10 ms × 1.4 = 14 ms, 환산이지 측정은 아니다).
- 계측은 ms 단위로 잘린 값이다(시험 본래의 해상도). 6 ms 와 7 ms 의 차이는 실제로는 0~1 ms 일 수 있다.
- 시험 총 시간 22 ms 의 "나머지" 해석(실패 메시지 출력 가능성)은 재지 않았다.
- 스레딩 기전(어느 헬퍼가 스레드를 만드는가)은 확인하지 못했다.

## 증거 파일 (`.moai/reports/lane-post/QA-B-174/`)

`A_alone_30_cold_processes.csv.txt`, `B_integration_group_3_runs.csv.txt`, `C_full_suite_3_runs.csv.txt`, `C_full_suite_ctest_summaries.txt`, `stats.txt`(계산), `temp_probe_T608.patch.txt`(임시 계측 — 시험에는 남아 있지 않음). 실패한 실행의 로그는 `QA-B-173/first_full_run_T608_failure_ci_post_ctest.txt`.
