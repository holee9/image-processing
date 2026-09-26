# QA-B-86 (#179) 게이트 보고서 — 3072² 모듈 성능 요구를 SPEC 크기로 재기만 하는 벤치마크

**카드**: QA-B-86 · **레인**: Lane B (`xpe-post`, `dev/postprocess`)
**커밋**: `6e9fa6a` (`Refs #179`) — 시험 9파일 + 도우미 헤더 3개, 252줄 추가 · push 없음
**BUILD_EXIT**: 세 프리셋 모두 `BUILD=0` / `EXIT=0`, ctest **545 / 224 / 192**, 실패 0 (`_verify.log`)
워크플로 패턴은 `origin/main` 의 파일에서 옮겼다(`_wf_benchmark.yml`, `_wf_ci.yml`).

---

## 1. 주장

| 요구 | 추가한 시험 | 로컬 min / med / max (µs) — **판정에 쓰지 않음** |
|---|---|---|
| REQ-ENH-006 (log) | `LogTransform.BenchmarkFreeze_Performance_REQ_ENH_006_LogTransform3072` | 3350 / 3520 / 4196 |
| REQ-ENH-006 (inverse) | `LogTransform.BenchmarkFreeze_Performance_REQ_ENH_006_LogInverse3072` | 3176 / 3336 / 4082 |
| REQ-ENH-012 | `NoiseReduce.BenchmarkFreeze_Performance_REQ_ENH_012_Bilateral3072` | 113868 / 121426 / 123574 |
| REQ-ENH-017 | `ContrastEnhance.BenchmarkFreeze_Performance_REQ_ENH_017_Clahe3072` | 45660 / 45898 / 48095 |
| REQ-ENH-022 | `EdgeEnhance.BenchmarkFreeze_Performance_REQ_ENH_022_Usm3072` | 15953 / 17452 / 20052 |
| PERF-ADV-003 | `CollimationDetectTest.BenchmarkFreeze_Performance_PERF_ADV_003_Collimation3072` | 599705 / 608784 / 613909 |
| PERF-ADV-004 | `ExposureIndexTest.BenchmarkFreeze_Performance_PERF_ADV_004_ExposureIndex3072` | 7475 / 7652 / 7877 |
| REQ-DISP-008 | `ModalityLut.BenchmarkFreeze_Performance_REQ_DISP_008_Linear3072` | 2026 / 2173 / 3487 |
| REQ-DISP-016 | `VoiLut.BenchmarkFreeze_Performance_REQ_DISP_016_Linear3072` | 2445 / 2488 / 3016 |
| REQ-DISP-028 | `PresentationLut.BenchmarkFreeze_Performance_REQ_DISP_028_Lut3072` | 21155 / 21328 / 23761 |
| **REQ-GSVG-019** | **건너뜀** — §4 | — |
| REQ-DICOM-023 | 대상에서 뺌(카드 지시: 벤치마크 워크플로가 dicom 을 빌드하지 않음, `BUILD_DICOM=OFF`) | — |

- 요구 9개(카드 표의 10개에서 GSVG-019 제외)에 시험 10개를 붙였다. REQ-ENH-006 은 SPEC 이 `xpe_log_transform` **or** `xpe_log_inverse` 로 두 함수를 명시해서 두 시험으로 나눴다.
- **벤치마크 한 번에 늘어나는 시간(로컬 합계)**: 0.09 + 0.09 + 1.01 + 0.73 + 0.31 + 5.23 + 0.09 + 0.08 + 0.08 + 0.29 ≈ **8.0 s**. 가장 긴 것은 PERF-ADV-003 의 5.23 s 다.
- 로컬 수치는 부하가 있는 개발 기계의 값이다. SPEC 문턱과 비교하지 않는다.

## 2. 형식

`perf_measure.h` — 같은 내용을 `modules/{enhance_basic,enhance_advanced,display}/tests/` 에 하나씩 두었다. 모듈마다 시험 실행 파일이 따로이고 공용 시험 라이브러리가 없어서다.

- `perf_measure::Measure(id, size, prepare, run)` 의 순서:
  1. `prepare()` 후 워밍업 1회를 돌린다(기록 안 함, `XPE_OK` 단언).
  2. `kRuns=7` 회 반복한다. 매 회 `prepare()` 로 입력을 복원하고(시간 밖), `run()` 만 `steady_clock` 으로 잰다.
  3. 결과를 µs 단위로 정렬해 출력한다.
- 출력: `PERFMEASURE <요구ID>/<함수> 3072x3072 min=… med=… max=… us (runs=7)` 와 `RecordProperty(perf_min_us / perf_med_us / perf_max_us)`
  - 요구 ID 필드 뒤에 `/함수` 를 붙였다. REQ-ENH-006 이 두 함수를 가리키기 때문이다. 공백이 없어서 필드 하나로 읽힌다.
- **시간 단언은 없다.** 단언은 다음뿐이다.
  - 매 호출 `XPE_OK`
  - 출력 유효성:
    - enhance_basic·display LUT 시험: 마지막 출력 전 화소가 유한
    - collimation: 좌표가 영상 안이고 x0≤x1, y0≤y1
    - EI: ei·di 가 유한
    - presentation LUT: 형식이 `XPE_PIXEL_UINT16`, 데이터가 NULL 아님
- 입력: `1 + (i·2654435761 mod 4096)` 같은 결정적이고 일정하지 않은 값을 썼다. log inverse 는 (0, 4], display 는 0..65535, presentation LUT 는 [0, 1] 범위다.
- **REQ-DISP-028** 은 "including format conversion" 이다. 호출이 float32 버퍼를 해제하고 uint16 버퍼를 새로 달기 때문에, 매 회 새 `std::malloc` float32 영상을 만들고 이전 uint16 결과를 해제한다(시간 밖).
- 파라미터는 기존 대리 시험과 같게 했다.
  - bilateral 3/50
  - CLAHE clip 3, tile 8
  - USM 0.5/2/10
  - modality slope 1, intercept −1024
  - VOI 는 BONE 프리셋(B-82 이후 32768/65535)
  - presentation LUT 는 identity
  - collimation 은 합성 사각형(200,240)-(2880,2840), 에지 900
  - EI 는 CHEST, 80 kVp, 10 mAs
- **기존 시험은 바꾸지 않았다**(추가만).

## 3. 선택 확인 (`_run.log` → `_select_bench.txt`, `_select_ci.txt`)

| 패턴 (워크플로 파일 그대로) | 전체 | 새 시험 10개 중 |
|---|---|---|
| `benchmark-regression.yml:68` `-R` | 16 (기존 6 + 10) | **10 선택** |
| `ci.yml:263` `-E` | 518 | **0** (모두 제외) |
| 대조군 — `EdgeEnhancementTest.BenchmarkFreeze_ADV061_…`(이름에 `Performance` 없음) | | ci 목록에 **1** |

`ctest -V -R BenchmarkFreeze_Performance` 실행에서 `PERFMEASURE` 줄 **10**, 10/10 Passed.

## 4. 건너뛴 것 — SPEC 문언과 구현이 다름

### REQ-GSVG-019

`.moai/specs/SPEC-XPE-GSVG/spec.md:215-218`:

> **When** a 3072x3072 16-bit image is processed, **the system shall** complete processing within 1.0 seconds (**Tier 1 DWT**, Intel i7 or equivalent).

- 같은 SPEC 의 `REQ-GSVG-002`(:54-57)는 "decompose the image into multi-scale sub-bands using 2D Discrete Wavelet Transform" 이다.
- 구현(`modules/gsvg/src/gsvg.cpp`, 350줄)에서 `tier|dwt|wavelet|haar` grep 은 **0건**이다.
  - 대조: 같은 파일에서 `vignette` 는 13건이 잡힌다.
- 파일 주석:
  - `:10` "Grid shadow suppression: per-row mean-deviation subtraction"
  - `:14-15` "A proper FFT-based notch filter is out of scope here (no FFT dependency available)"
- **"Tier 1 DWT" 조건으로 처리하는 경로가 코드에 없다.** 그래서 카드 지시대로 건너뛰었다. 지금의 행 평균 경로를 3072² 로 재는 것은 이 요구의 조건과 다른 측정이다.

### REQ-ENH-012 의 NLM (부분)

`.moai/specs/SPEC-XPE-P1B-ENH/spec.md:278`:

> **REQ-ENH-012**: WHILE processing a 3072x3072 float32 image, the system SHALL complete `xpe_noise_reduce` within 100 milliseconds.

- **모드를 지정하지 않는다.** `xpe_noise_reduce` 는 `XPE_NOISE_BILATERAL` 과 `XPE_NOISE_NLM` 을 받는다(`enhance_basic_api.h:46-47`).
- 기존 대리 시험과 같은 bilateral 만 쟀다.
- NLM 은 재지 않았다. 구현(`noise_reduce.cpp:202-240`)은 탐색창 × 패치의 단순 중첩 루프이고, 기본 21/7(`enhance_basic_api.h:62-63`)이면 3072² 에서 내부 반복이 약 3072²·21²·7² ≈ 2×10¹¹ 회다(**계산, 미측정**). 벤치마크 한 번에 넣을 수 없는 규모로 판단했다.

## 5. 부수 확인

- `#160` orphan 검사(루트 `CMakeLists.txt`)는 `modules/<m>/src/` 의 `.cpp` 만 본다(주석: "this compares SOURCES only"). 시험 폴더의 새 헤더는 대상이 아니다. 전체 configure·빌드는 `/WX` 로 `BUILD=0` 이었다.

## 6. 미검증

- CI 러너 수치 — 병합 후 리더의 벤치마크 실행으로 얻는다. 요약 스텝이 `PERFMEASURE` 를 옮기도록 바꾸는 일은 리더 쪽이다.
- `-V` 로 찍힌 줄이 워크플로 요약에 실제로 옮겨지는지(이번에는 워크플로를 실행하지 않음).
- 워밍업 1회로 충분한지, 7회가 충분한지.
- 입력 분포가 실제 영상과 다를 때의 차이(`#148`).
- PERF-ADV-003 은 합성 사각형 하나로만 쟀다. 검출 경로(폴백 여부)에 따라 시간이 달라질 수 있다.

## 7. 잔여 위험

- 벤치마크 워크플로 시간이 약 8 s 늘고, 3072² 입력 복사본으로 메모리 사용도 는다(약 36~72 MB, 미측정).
- REQ-GSVG-019 는 여전히 CI 에서 확인되지 않는다. SPEC 조건과 구현이 맞지 않는 것이 먼저 풀려야 한다.

## 부록 — 증거

`_env.bat`, `_b86.bat`, `_verify.bat`, `_run.log`, `_select_bench.txt`, `_select_ci.txt`, `_verify.log`, `_wf_benchmark.yml`, `_wf_ci.yml`
