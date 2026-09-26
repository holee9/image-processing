# QA-B-85 (#179) 게이트 보고서 — post 모듈의 벽시계 시험 전수

**카드**: QA-B-85 · **레인**: Lane B (`xpe-post`, `dev/postprocess`, HEAD `0dbfd36`)
**코드 변경 없음, 커밋 없음, 이슈 코멘트 없음**(카드 지시). 빌드 없음(`ctest -N` 목록만).
워크플로 파일은 `origin/main` `caa1a14` 의 것을 그대로 썼다(`_wf_ci.yml`, `_wf_benchmark.yml`).

---

## 1. 주장 — 결과

| 항목 | 값 |
|---|---|
| 본문에 시간 측정이 있는 시험 | **33** (6개 모듈 + 루트 `tests/e2e_post_pipeline`) |
| 그중 **시간 문턱을 단언**하는 시험 (T) | **22** |
| 0보다 크다만 단언 (T0) | 1 |
| 재기만 하거나 대기만 함 (N) | 10 |
| 분류 안 된 시험 | **0** (스크립트 단언) |
| T 중 `ci.yml` 에서 도는 것 | **4** — BP-06·07·08·09 |
| T 중 `benchmark-regression.yml` 에서 도는 것 | **5** — 위 4 + FullPipeline 3000 ms |
| **T 중 어느 워크플로에서도 안 도는 것** | **17** — 그중 **15개는 SPEC 요구가 있다**(§3) |
| 대조군 | `EdgeEnhancementTest.T308_PerformanceBudget` 포함 확인. 본문만 보던 1차 스캔이 **`FullPipelineE2E.PostProcess_3072x3072_Within3000ms`** 를 놓쳐 스캐너를 넓혔다(§2) |

**ai·dicom 모듈에는 시간을 단언하는 시험이 없다.**
- dicom 의 후보 1건은 `sleep_for` 대기이고, ai 는 후보가 0건이다.
- 두 모듈은 push 로 도는 `ci.yml` 잡이 빌드하지 않는다(`ci-post` 프리셋이 `BUILD_AI=OFF`, `BUILD_DICOM=OFF`). dicom 은 수동 실행(`workflow_dispatch`) 전용 `coverage-dicom` 잡에서만 빌드된다.

## 2. 방법

1. **본문 스캔** (`_scan.py` → `_candidates.tsv`)
   - 대상: `modules/{enhance_basic,enhance_advanced,ai,display,dicom,gsvg}/tests/*.cpp` 와 `tests/e2e_post_pipeline/*.cpp`, 65파일
   - `TEST/TEST_F/TEST_P` 본문(주석 제거)에서 `steady_clock|high_resolution_clock|system_clock|chrono::|QueryPerformanceCounter|GetTickCount|timeGetTime|clock(` 를 찾았다.
   - **1차 스캔은 본문만 봐서 `FullPipelineE2E.*` 를 놓쳤다.** 시간을 `run_pipeline` 도우미 안에서 재기 때문이다. 대조군(시간을 단언한다고 이미 아는 3000 ms 시험)이 빠진 것으로 알아챘다.
   - 그래서 **같은 파일에서 TEST 밖에 정의된 함수 중 시간 표지가 있는 것**을 모으고, 그 함수를 부르는 시험도 후보에 넣었다. 대조군 둘(T308, FullPipeline 3000)이 모두 들어오는 것을 스크립트가 출력한다. 결과: 30 → 33건.
2. **분류** (`_classify.py` → `_table.md`)
   - 33건마다 종류·입력·문턱·측정 방식·SPEC 근거·비고를 한 행으로 적었다.
   - 스크립트는 다음 경우에 실패한다: 후보인데 행이 없음, 행인데 후보가 없음, `_lists.bat` 의 패턴이 워크플로 파일의 패턴과 다름, T308 대조군이 없음.
3. **CI 에서 도는지** (`_lists.bat` → `_n_*.txt`)
   - `ctest -N` 에 워크플로의 패턴을 **그대로** 걸었다. 패턴 문자열은 스크립트가 워크플로 파일에서 정규식으로 뽑아 `_lists.bat` 과 같은지 단언한다.
   - `ci.yml:263`: `ctest --test-dir build/ci-post … -E "Performance|Within[0-9]+ms|PerformanceBudget|LargeImagePerformance"` → 518건
   - `benchmark-regression.yml:68-70`: `-R '<pattern>'` → 6건
   - 전체: ci-post 536건, ci-ai-b20 224건, ci-dicom 192건
   - 판정: 선택 목록에 있으면 "예", 전체 목록에만 있으면 "아니오", ci-post 에서 빌드되지 않으면 "빌드 안 됨"
4. **SPEC 근거**: 시험 주석의 ID 를 SPEC 파일(`spec.md`, `acceptance.md`)에서 찾아 크기·값을 대조했다. 주석과 SPEC 이 다르면 비고에 적었다.

## 3. 결과 — 요구가 있는데 어느 워크플로에서도 안 도는 시간 단언 (15)

모두 `ci.yml` 의 `-E` 에 이름이 걸리고(`Performance`, `Within…ms`, `PerformanceBudget`, `LargeImagePerformance`), `benchmark-regression.yml` 의 `-R` 에는 걸리지 않는다.

| 시험 | 입력 / 문턱 | SPEC 요구 (SPEC 에서 확인) | 시험과 SPEC 의 차이 |
|---|---|---|---|
| `LogTransform.Performance_3072x3072_Within15ms` | 3072² / ≤15 ms | REQ-ENH-006 3072² 15 ms | — |
| `LogTransform.InversePerformance_3072x3072_Within15ms` | 3072² / ≤15 ms | REQ-ENH-006 | — |
| `NoiseReduce.Performance_Bilateral_Small_Within100ms` | **512²** / ≤100 ms | REQ-ENH-012 **3072²** 100 ms | 크기가 다름 |
| `ContrastEnhance.Performance_3072x3072_Within50ms` | 3072² / ≤50 ms | REQ-ENH-017 3072² 50 ms | — |
| `EdgeEnhance.Performance_3072x3072_Within20ms` | 3072² / ≤20 ms | REQ-ENH-022 3072² 20 ms | — |
| `EnhanceIntegration.FullPipeline_3072x3072_Within200ms` | 3072² / ≤200 ms | REQ-ENH-CC-005 **5단계** 200 ms | 시험은 noise·CLAHE·USM **3단계**만 잰다 |
| `CollimationDetectTest.LargeImagePerformance` | **2048²** / <500 ms | PERF-ADV-003 3072² 500 ms | 주석이 인용한 REQ-ADV-062 는 전체 파이프라인 2500 ms |
| `EdgeEnhancementTest.T308_PerformanceBudget` | **1024²** / <100 ms | REQ-ADV-061 3072² 400/120 ms | 100 은 SPEC 값 아님(QA-B-83) |
| `ExposureIndexTest.T508_PerformanceBudget` | **2048²** / <100 ms | PERF-ADV-004 3072² 50/20 ms | 주석이 REQ-ADV-062 를 인용. 100 은 환산값 22 ms 에 대한 "generous" |
| `IntegrationTest.T603b_FullPipeline_PerformanceBudget` | **512²** / <500 ms | REQ-ADV-062 / AC-PIPE-001 3072² 2500 ms | 크기·문턱 모두 환산값 |
| `IntegrationTest.T608_PerformanceBudgetVerification` | **512²** / MFP<100, frac<50, coll<50 ms, EI<10000 µs | PERF-ADV-001~004 3072² | 주석은 AC-PIPE-001 을 인용 |
| `ModalityLut.Performance_3072x3072_Linear` | 3072² / ≤20 ms | REQ-DISP-008 3072² 20 ms | — |
| `PresentationLut.Performance_3072x3072` | 3072² / **≤30** ms | REQ-DISP-028 3072² **25** ms | **주석이 인용한 REQ-DISP-025 는 GSDF 보정 요구이고 성능 요구가 아님.** 문턱 30 > SPEC 25 |
| `VoiLut.Performance_3072x3072` | 3072² / ≤16 ms | REQ-DISP-016 3072² 16 ms | — |
| `GsvgAbiSmoke.Lifecycle3072_PerformanceBudget` | 3072² / **<8000** ms | REQ-GSVG-019 3072² **1000** ms | 주석: "generous ceiling … budget asserted by the dedicated benchmark suite". 그 스위트(BP-06)는 버전 문자열 조회를 잰다 — 영상 처리가 아님 |

**요구를 찾지 못한 시간 단언 (2)** — 역시 어느 워크플로에서도 돌지 않는다
- `GsvgDegradedMode.DegradedMode_PerformanceBudget` (64² / <50 ms)
  - "BP-07~09-DEG" 라벨을 쓰지만, SPEC-BENCH-POST 의 BP-07~09 는 다른 시험이다.
  - 검색 범위: `SPEC-XPE-GSVG/spec.md` §4, `SPEC-BENCH-POST/spec.md`
- `FullPipelineE2E.PostProcess_512x512_Within200ms` (512² / ≤200 ms)
  - 검색 범위: `.moai/specs/**/spec.md` 에서 `200 ?ms`. 걸린 것은 SPEC-DOC-001(Ghost Tier2), P1B-DICOM(검증 200 ms), P1B-ENH(REQ-ENH-CC-005)이고, 512² e2e 요구는 없다.

## 4. 워크플로에서 도는 시간 단언 (5)

| 시험 | 입력 / 문턱 | 근거 | ci.yml | benchmark |
|---|---|---|---|---|
| `BenchmarkFreeze.BP06_GsvgVersionProbeBaseline` | 버전 조회 1024회 / <5000 µs | SPEC-BENCH-POST BP-06 | 예 | 예 |
| `CollimationDetectTest.BenchmarkFreeze_BP07_…` | 512² / <500 ms | BP-07 | 예 | 예 |
| `ExposureIndex.BenchmarkFreeze_BP08_…` | 512² / <25 ms | BP-08 | 예 | 예 |
| `ExposureIndex.BenchmarkFreeze_BP09_…` | 512² / <25 ms | BP-09 | 예 | 예 |
| `FullPipelineE2E.PostProcess_3072x3072_Within3000ms` | 3072² / ≤3000 ms | SPEC-XPE-MASTER Phase 1 < 3000 ms | 아니오 | 예 |

- 이 5개 중 **3072² 모듈 성능 요구(REQ-ENH-*, REQ-ADV-06x, REQ-DISP-*, REQ-GSVG-019)를 직접 받치는 것은 없다.** BP-06~09 는 512² 또는 버전 조회이고, 3000 ms 는 파이프라인 전체다.
- e2e 파이프라인은 enhance_basic·gsvg·display 를 거치고, enhance_advanced 는 거치지 않는다(QA-B-83).

## 5. 측정 방식 — 공통 사실

- T 22건 중 **워밍업이 있는 것은 0건**이고, 반복하는 것은 BP-06(1024회 합계)과 함수·경로별 1회씩인 T608·gsvg 2건뿐이다. 나머지는 단일 호출 1회다.
- 대부분 `duration_cast<milliseconds>` 로 **절사**한다. µs 로 재는 것은 BP-06, T608 의 EI 뿐이다.
- 문턱이 SPEC 값과 같은 것: REQ-ENH-006·017·022, REQ-DISP-008·016 (3072² 그대로).
- 환산값이거나 "generous" 인 것: T308, T508, T603b, T608, LargeImagePerformance, gsvg 8000, PresentationLut 30.

## 6. 부수 사실

- `REQ-DICOM-023` 은 SPEC 표(`SPEC-XPE-P1B-DICOM/spec.md:311`)에 "DICOM validation ≤ 200ms, 3072x3072" 가 있다. dicom 시험에는 시간 측정이 0건이다(본문 스캔). `test_dicom_validator.cpp:4` 는 REQ-DICOM-023 을 인용하지만 시간을 재지 않는다.
- `IntegrationTest.T602_DiagnosticLogging` 은 `EXPECT_GT(duration, 0)` 만 한다(T0). REQ-ADV-091 은 로그 요구다.
- `EdgeEnhancementTest.BenchmarkFreeze_ADV061_…`(QA-B-84)는 N 이고, ci.yml·benchmark 양쪽에서 돈다.

## 7. 미검증

- 도우미 탐지는 **같은 파일 안**의 함수만 본다. 다른 파일·헤더의 도우미에서 시간을 재는 시험은 놓칠 수 있다. 픽스처 메서드는 들여쓴 정의 줄로 잡히게 했지만 개별 확인은 하지 않았다.
- `ci-ai-b20`·`ci-dicom` 은 로컬 빌드 디렉터리 이름이고 `CMakePresets.json` 의 프리셋이 아니다. "빌드 안 됨" 판정은 ci-post 목록에 없다는 사실과 프리셋의 `BUILD_AI/BUILD_DICOM` 값에 근거한다.
- `coverage-post`(수동 실행)는 `XPE_COVERAGE_EXCLUDE_TESTS`(ci.yml 과 같은 정규식)로 같은 시험을 뺀다고 판독했다. 이 잡은 실행하지 않았다.
- AI SPEC(SPEC-XPE-P3-AI)의 성능 요구 목록은 보지 않았다(ai 에 시간 단언 시험이 0건이라 표에 행이 없음).
- 실제 CI 로그에서 이번 목록을 다시 대조하지는 않았다(B-83 에서 ci 5회·benchmark 1회로 한 번 확인함).

## 8. 잔여 위험

- §3 표의 요구들이 **CI 에서 한 번도 확인되지 않는다**: REQ-ENH-006·012·017·022·CC-005, PERF-ADV-001~004 와 REQ-ADV-061·062, REQ-DISP-008·016·028, REQ-GSVG-019. 로컬 시험은 있지만, 문턱이나 크기가 SPEC 과 다른 것이 섞여 있다.
- 시험 주석의 요구 ID 가 틀린 것이 3건 있다(PresentationLut → 025, LargeImagePerformance·T508 → 062). 추적성 표가 주석에서 만들어졌다면 그 표도 틀렸을 수 있다(확인 안 함).

## 부록 — 증거

| 파일 | 내용 |
|---|---|
| `_scan.py` → `_candidates.tsv` | 본문 스캔(도우미 포함), 33건 |
| `_classify.py` → `_table.md` | 33행 전체 표, 분류·합계·미분류 0 단언, 워크플로 패턴 일치 단언 |
| `_lists.bat` → `_n_post_all.txt`, `_n_post_ci.txt`, `_n_post_bench.txt`, `_n_ai_all.txt`, `_n_dicom_all.txt` | `ctest -N` 목록 |
| `_wf_ci.yml`, `_wf_benchmark.yml` | `origin/main caa1a14` 의 워크플로 사본 |
| `_bodies.txt` | 33건의 주석·본문 발췌 |
