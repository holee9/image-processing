# QA-B-99 게이트 — 기준 기계(개발 PC)에서 속도 요구 실측 (#179)

커밋 `59d49cc`(측정 시험 1건 추가), 미푸시. **측정만 했다. 최적화는 하지 않았다.**

## 1. 주장

### 기준 기계와 측정 조건 (`_machine.txt`)

**기기**
- CPU: 12th Gen Intel Core i7-12700, 12 코어 / 20 논리 프로세서
- 메모리: 34,088,599,552 바이트(32 GiB)
- OS: Windows 11 Pro 10.0.26200
- 전원 설정: GUID 381b4222-f694-41f0-9685-ff5bb260df2e(균형 조정)

**빌드**
- 프리셋 `ci-post`: Ninja, `CMAKE_BUILD_TYPE=RelWithDebInfo`
- compile_commands 의 플래그: `/O2 /Ob1 /arch:AVX2`
- enhance_basic 은 추가로 `/arch:AVX2 /fp:fast`(`modules/enhance_basic/CMakeLists.txt:51`)

**스레드**
- 대상 모듈 소스(enhance_basic, enhance_advanced, gsvg, display `src/`)에서 `omp parallel`, `std::thread`, `parallel_for`, `std::execution` 은 0건이다.
- CMake 에 OpenMP 도 없다.
- 따라서 모든 측정은 단일 스레드다.

**방법**
- 기존 `perf_measure::Measure` 를 썼다: 준비 1회, 이후 7회를 재어 최소·중앙·최대를 낸다.
- 실행 파일마다 `--gtest_filter=*BenchmarkFreeze* --gtest_repeat=3` 로 돌렸고, 이 묶음을 3차례(`_bench1/2/3.log`) 반복했다.
- 따라서 항목마다 7회 중앙값이 9개다. 표의 "로컬 중앙값"은 그 9개의 중앙값이다.

**측정 시각과 동시 부하**
- 1차 23:12:03–23:13:56, 2차 23:14:13–23:16:00, 3차 23:16:33–23:18:20.
- `cl.exe`·`ninja.exe` 수: 각 차수 전후 모두 0.
- 다른 프로세스는 돌고 있었다. 2차 직전 CPU 누적 상위(`_procs_before2.txt`): ChatGPT, claude 4개, java, remoting_host 등.
- 3차 중 전체 CPU(`_cpu3.log`, 3초 간격 45 표본): 평균 19.5%, 최대 39.1%. 측정 자체는 20 논리 프로세서 중 1개(약 5%)다.

### 결과

**판정 기준**
- 여유: 요구의 80% 미만
- 경계: 80–100%
- 초과: 100% 초과

"CI 최근 값"은 benchmark-regression 실행 35231522563(3a39206, 2026-09-17T14:07:46Z)의 7회 중앙값이다.

| 항목 | 요구(문서:줄) | 조건 | 로컬 중앙값 (9개 범위) | CI 최근 값 | 판정(로컬) |
|---|---|---|---|---|---|
| REQ-ENH-012 bilateral | 100 ms (`SPEC-XPE-P1B-ENH/spec.md:278`) | 3072² F32 | **113 ms** (109–190) | 196 ms | 초과 |
| REQ-ADV-061 fractional | scalar 400 ms / AVX2 120 ms (`SPEC-XPE-P2-ADV/spec.md:266`) | 3072² F32, 측정은 order 1.2 | **1189 ms** (968–1251) | 1124 ms | 초과 (scalar 기준 297%) |
| REQ-GSVG-019 그리드 억제 | 1.0 s (`SPEC-XPE-GSVG/spec.md:219–222`) | 3072² 16-bit | **1322 ms** (1259–1428) | 1507 ms | 초과 |
| REQ-GSVG-019 가상 그리드 | 같은 요구(가상 그리드 포함 여부는 문서에 없음) | 3072², 합성 표 | **612 ms** (570–667) | 598 ms | 여유 (61%) |
| REQ-ENH-017 CLAHE | 50 ms (`SPEC-XPE-P1B-ENH/spec.md:290`) | 3072² F32 | **50 ms** (46–51) | 49 ms | 경계 (100%) |
| REQ-ENH-022 USM | 20 ms (`SPEC-XPE-P1B-ENH/spec.md:302`) | 3072² F32 | **17 ms** (16–24) | 22 ms | 경계 (85%) |
| PERF-ADV-003 콜리메이션 | scalar 500 ms / AVX2 200 ms (`SPEC-XPE-P2-ADV/acceptance.md:514`) | 3072² F32 | **707 ms** (655–768) | 551 ms | 초과 (141%) |
| REQ-DISP-028 Presentation LUT | 25 ms (`SPEC-XPE-P1B-DISP/spec.md:348`) | 3072² | **21 ms** (21–22) | 28 ms | 경계 (84%) |
| T308 (시험 문턱) | 100 ms (`test_edge_enhancement.cpp:639`) | 1024², order 1.2 | 단독 **89 ms** (86–93, 60회) / 묶음 안 **121 ms** (88–137) | CI 미측정 | 경계 (단독 여유 11%) |

참고로 같이 측정된 항목(대상 외)

| 항목 | 로컬 중앙값 (범위) | CI |
|---|---|---|
| REQ-ENH-006 log | 3 ms (3–4) | 6.8 ms |
| REQ-ENH-006 inverse | 3 ms (3–8) | 4.4 ms |
| PERF-ADV-004 EI | 11 ms (8–32) | 9.0 ms |

### T308 문턱의 여유

- 단독 실행 60회(`--gtest_repeat=20` × 3): 86–93 ms, 중앙 89 ms. 실패 0.
  - 문턱 100 ms 까지의 여유는 중앙 기준 11 ms(11%), 최대 기준 7 ms 다.
- 벤치 묶음 안에서 같은 장면(신규 `BenchmarkFreeze_Performance_T308_Fractional1024`)의 7회 중앙값 9개: 88, 90, 99, 103, 121, 126, 135, 136, 137 ms. 이 중 6개가 100 ms 를 넘었다.
  - 같은 프로세스에서 3072² 측정 직후에 돌 때 느려졌다. 원인은 재지 않았다(§4).
- 이 시험은 이름에 "Performance" 가 있어 ci.yml 의 `-E` 에서 제외된다. CI 로그 35231522563 에서 `T308` 은 0건이다.
- B-98 의 109 ms 실패는 전체 ctest 실행 중이었다. 이 측정과 같은 "다른 부하 뒤" 조건이다.

## 2. 증거

- `_bench1.log`, `_bench2.log`, `_bench3.log`: 원본 PERFMEASURE / ADV061 줄과 T308 20회 결과, 전후 부하 수.
- `_cpu3.log`: 3차 CPU 표본.
- `_machine.txt`, `_procs_before2.txt`: 기기 정보와 프로세스 목록.
- `_ci_35231522563.log`: CI 원본 로그. PERFMEASURE 줄은 `grep -oE "PERFMEASURE …"` 로 뽑았다.
- 추가한 측정: `BenchmarkFreeze_Performance_T308_Fractional1024` 1건.
  - 이름이 "BenchmarkFreeze" 를 포함하므로 benchmark-regression.yml 의 `-R` 패턴에 걸린다.
  - REQ-ADV-061 은 기존 `BenchmarkFreeze_ADV061_FractionalMeasure3072`(자체 출력 `ADV061 …`)를 그대로 썼다. 처음에 중복 시험을 넣었다가 기존 것을 발견해 지웠다.
- `_verify.log`
  - ci-post: BUILD=0, 634 통과(+1 = 신규 측정)
  - ci-ai-b20: BUILD=0, 225 통과
  - ci-dicom: BUILD=0, 194 통과
  - e2e: 28 통과

## 3. 기준선 귀속

- 로컬 값은 이 워크트리 HEAD 122108e 소스로 만든 `build/ci-post` 바이너리다.
  - 측정 시험 추가분은 59d49cc 와 같은 소스다.
  - QA-B-101 병합본(3a39206)과 코드가 같다. 3a39206 은 main 에서 122108e 를 병합한 것이다.
- CI 값은 run 35231522563(3a39206)이다.

## 4. 미검증

- 묶음 안에서 T308 장면이 느려지는 원인(열, 메모리 배치, 캐시)은 재지 않았다.
- REQ-ADV-061 의 요구 조건은 order 1.0(PERF-ADV-002)인데 측정은 order 1.2 다. order 1.0 은 재지 않았다.
- "AVX2 목표"(ADV 120/200 ms)의 적용 여부: 빌드는 `/arch:AVX2` 이지만 명시적 SIMD 코드가 있는지는 조사하지 않았다. 판정은 scalar 목표로 했다.
- REQ-GSVG-019 에 가상 그리드가 포함되는지는 문서에 없다. 가상 그리드 측정은 합성 표(환경 변수 `XPE_VG_TABLE` 미설정), 반복 3회, 피라미드 6단·gain 1.3·denoise 2 설정이다(`test_virtual_grid.cpp:797-800`). 제품용 표로는 재지 않았다.
- 메모리(REQ-GSVG-020) 등 속도 외 항목은 대상이 아니다.
- 1·2차의 전체 CPU 부하는 기록에 실패했다(`_cpu2.log` 비어 있음). 3차만 있다.
- 전원 설정을 "고성능"으로 바꾼 측정은 하지 않았다(설정 변경 없음).

## 5. 잔여 위험

- 같은 기계에서도 중앙값이 흔들린다. 예: bilateral 109–190 ms, T308 장면 88–137 ms. 경계 항목(ENH-017 100%, ENH-022 85%, DISP-028 84%)은 부하에 따라 넘을 수 있다.
- CI 와 로컬의 순서가 항목마다 다르다. bilateral 은 CI 가 1.7배 느리고, 콜리메이션은 CI 가 0.78배로 빠르다. 한쪽 기준의 여유를 다른 쪽에 옮길 수 없다.
- 다른 세션(claude 4개 등)이 동시에 돌고 있었다.

## Card Cross-Check

| 카드 항목 | 결과 |
|---|---|
| 요구 원문·측정 조건 | §1 표 |
| 기기 정보 | §1 |
| 반복·중앙·최대·최소, 측정 시각·부하 | §1, `_bench*.log` |
| 기존 BenchmarkFreeze·PERFMEASURE 재사용, 없는 항목만 추가 | T308 1건 추가(59d49cc) |
| T308 문턱 여유 | §1 |
| 결과 표 (요구/로컬/CI/판정) | §1 |
| 최적화 금지 | 소스 변경 없음 |
