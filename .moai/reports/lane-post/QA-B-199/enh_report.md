# QA-B-199 ENH — SPEC-XPE-P1B-ENH 작업표·요구 실태 대조

범위: `.moai/specs/SPEC-XPE-P1B-ENH/`(spec.md·tasks.md), `modules/enhance_basic/`, `docs/enhance-basic/RTM-ENHANCE-BASIC-001_Requirements_Traceability_Matrix.md`, `.github/workflows/ci.yml`. 코드는 바꾸지 않았다.

## 0. 방법과 한계

1. 기계 추출(`extract_script.txt`): spec.md 에서 `**REQ-ENH-…**` 로 시작하는 정의 줄, tasks.md 의 `| T-` 행과 그 요구 열, 시험 소스의 `TEST(…)` 이름·줄 번호를 뽑는다. 대조군: T-004 가 REQ-ENH-001~006 을 가리키는지, 범위 표기(`001..006`)가 올바르게 펼쳐지는지(스크립트 안 assert).
2. 요구별 판정은 요구 문구와 시험 단언을 에이전트가 읽고 매겼다. 판정 기준: `asserted`(독립 기대값에 대한 단언) / `partial`(일부 문구만 단언, 또는 기대값이 구현 상수의 복사) / `self_referential` / `constant_only`(균일·상수 입력이라 구현이 일을 안 해도 통과) / `no_test` / `no_code`.
3. 검증(`verify_script.txt`, 출력 `verify_ENH.txt`): 인용한 시험 코드 95건이 인용한 줄 ±2 줄 안에 실재한다(실패 0). 구현 인용 36건 중 1건은 줄 번호 없는 산문(CC-005 의 "n/a")이라 쓰지 않았다. 대조군: 진짜 인용은 받아들이고 가짜 인용은 거부함(verify_ENH.txt 첫 줄).
4. 핵심 주장 다섯 건은 내가 코드를 직접 읽어 확인했다(§4). 나머지 `partial` 판정의 사유 문장은 에이전트의 서술이며 줄 인용만 기계 검증했다.
5. **실행하지 않았다.** 아래 결함 후보는 모두 읽기로 찾은 후보이며 재현 시험을 돌리지 않았다.

## 1. 작업 행 (tasks.md 13행)

표 상태 열은 tasks.md 의 `status` 값 그대로다. "실제" 는 그 작업이 가리키는 요구들의 판정에서 규칙으로 계산했다(전부 asserted → 구현됨, 전부 constant_only/no_test/no_code → 없음, 나머지 → 부분).

| 작업 | 표 상태 | 작업 내용 | 요구 | 실제 | 판정별 개수 |
|---|---|---|---|---|---|
| T-001 | pending | Expand enhance_basic_api.h: 7 API decls + 3 param structs + 1 enum | CC-001 | **부분** | partial 1 |
| T-002 | pending | Update CMakeLists.txt: SHARED target + GTest + 6 test sources | CC-001 | **부분** | partial 1 |
| T-003 | pending | Implement exposure_index.cpp (SWU-2.10): EI/DI, EIT lookup, DI alert | 023, 024, 025, 026, 027, 028, 029, 030 | **부분** | asserted 3, partial 5 |
| T-004 | pending | Implement log_transform.cpp (SWU-2.1): forward/inverse log, clamping | 001, 002, 003, 004, 005, 006 | **부분** | asserted 2, partial 4 |
| T-005 | pending | Implement noise_reduce.cpp (SWU-2.2): bilateral + NLM + sigma MAD | 007, 008, 009, 010, 011, 012 | **부분** | constant_only 1, partial 5 |
| T-006 | pending | Implement contrast_enhance.cpp (SWU-2.3): CLAHE tile-based | 013, 014, 015, 016, 017 | **부분** | asserted 2, constant_only 2, partial 1 |
| T-007 | pending | Implement edge_enhance.cpp (SWU-2.4): USM + overshoot clamp | 018, 019, 020, 021, 022 | **부분** | asserted 1, constant_only 1, partial 3 |
| T-008 | pending | Write test_log_transform.cpp: round-trip fidelity, edge cases, perf | 001, 002, 003, 004, 005, 006 | **부분** | asserted 2, partial 4 |
| T-009 | pending | Write test_noise_reduce.cpp: bilateral, NLM, sigma est, param validati | 007, 008, 009, 010, 011, 012 | **부분** | constant_only 1, partial 5 |
| T-010 | pending | Write test_contrast_enhance.cpp: CLAHE correctness, tile blending, par | 013, 014, 015, 016, 017 | **부분** | asserted 2, constant_only 2, partial 1 |
| T-011 | pending | Write test_edge_enhance.cpp: USM correctness, overshoot bounds, params | 018, 019, 020, 021, 022 | **부분** | asserted 1, constant_only 1, partial 3 |
| T-012 | pending | Write test_exposure_index.cpp: EI/DI accuracy, bodyPart lookup, DI ale | 023, 024, 025, 026, 027, 028, 029, 030 | **부분** | asserted 3, partial 5 |
| T-013 | pending | Write test_enhance_integration.cpp: full pipeline, thread safety, P/In | CC-001, CC-002, CC-003, CC-004, CC-005 | **부분** | partial 5 |

**읽는 법.** 13행 전부 `pending` 인데 `enhance_basic` 은 main 에 있고 시험 160개가 돈다. 따라서 표 상태는 전부 틀렸다. 반대로 "구현됨" 으로 올릴 수 있는 행도 0행이다: 모든 작업에 `partial` 이나 `constant_only` 요구가 한 개 이상 걸려 있다. T-001·T-002(헤더·CMake)는 요구가 CC-001 하나이고 판정이 partial 이다(10 함수 vs SPEC 7, §4 헤더 약속).

## 2. 정의된 요구 중 어떤 작업에도 안 걸린 것 — 두 축

| 축 | 정의 | 개수(36개 중) | 해당 |
|---|---|---|---|
| 이름 | 어느 tasks.md 행의 요구 열에도 id 가 없음 | 1 | REQ-ENH-023a |
| 행위 | 가리키는 작업이 있어도 어떤 시험도 독립 기대값으로 단언하지 않거나(constant_only·no_test·self_referential) 코드가 없음(no_code) | 4 | 008, 014, 017, 019 (모두 constant_only) |
| 행위(넓게) | `asserted` 가 아님(partial 포함) | 27 | partial 23 + constant_only 4 |

차이: 이름 축 1 대 행위 축 4(넓게 27). 이름 축이 못 잡는 4건은 시험이 있고 이름도 있으나 균일 입력이라 구현이 아무 일을 안 해도 통과한다(예: 017 은 평탄한 500.0 영상이라 `contrast_enhance.cpp` 가 조기 반환하고 50 ms 를 통과한다).
파서 대조군: 이름 축 1건(023a)은 spec.md 에서 `023a` 접미사 id 를 정의 줄로 인식했고(`defined 36`) tasks.md 어느 행에도 없음을 확인했다. 시험 소스 텍스트에는 36개 요구 id 전부가 한 번 이상 나온다(`in_no_test_text []`) — 이름 축으로는 "시험 있음" 이지만 행위 축으로는 4건이 비어 있다.

## 3. RTM ✓ 인데 시험이 없는 것 (item 3)

`rtm_script.txt` 출력 `rtm_out.txt`. ENH RTM 의 ✓ 행 10개, 각 행의 시험 id 는 `TST-100-001` ~ `TST-200-003` 7종이다. 이 id 는 SRS 시험군 정의(“N test cases”)이며 시험 소스 어디에도 문자열로 나오지 않는다(7종 중 0). 이름 검색기의 합성 대조군(`rtm_control.txt`): `// covers TST-100-002 here` 에서는 찾고, 관련 없는 텍스트에서는 못 찾는다. 즉 "0건" 은 검색기가 눈먼 결과가 아니다.
다만 이것은 이름 축 결과다. 10행이 가리키는 SRS 요구(FR-100-001 등)를 실제로 단언하는 시험이 있는지(행위 축)는 이번에 가르지 않았다. → Gap.
이미 처분된 #237·#236·#239 는 ENH 와 관계없어 다시 세지 않았다.

## 4. 헤더가 하지 않는 일을 약속하는 문장 / 코드와 SPEC 의 어긋남

verify_ENH.txt: 헤더 약속 4건의 인용문이 줄 ±2 안에 실재한다.

1. `enhance_basic_api.h:243` `EI = EIT * (mean_pixel_value / S0_reference)` — 코드(`exposure_index.cpp:132`)는 `K_CAL_DEFAULT * (mean / S0_REFERENCE)` 이고 EIT 는 EI 계산에 안 들어간다(#154). 헤더에 옛 식이 남아 있다.
2. `enhance_basic_api.h:8` “Provides 8 exported C API functions” — 헤더가 선언한 XPE_API 는 10개, SPEC(CC-001)은 7개.
3. `enhance_basic_api.h:99` `set_max_threads` — 바이래터럴(bilateral) 필터만 설정을 읽는다. 로그·NLM·CLAHE·USM·EI 는 읽지 않아 설정이 무효.
4. `enhance_basic_api.h:136` 반환 코드 설명에 비유한 화소·오버플로 거부가 빠짐.

## 5. 빌드·CI 에서 빠진 것

- `ci.yml:318` 의 `ctest -E "Performance|Within[0-9]+ms|PerformanceBudget|LargeImagePerformance"` 와 `ci.yml:380` 의 `--gtest_filter=-*Performance*:*Within*ms*` 가 `post-build` 잡에서 시간 단언 시험을 뺀다. 그래서 CI 는 REQ-ENH-006/012/017/022 와 CC-005 의 시간 예산을 한 번도 단언하지 않는다. 제외되는 시험: `test_contrast_enhance.cpp:193/215`, `test_edge_enhance.cpp:191/213`, `test_enhance_integration.cpp:93`, `test_log_transform.cpp:142/153/189/206`, `test_noise_reduce.cpp:224/249`. 로컬 `ctest` 에서는 환경 게이트 없이 돈다.
- 시험 파일 전부가 `CMakeLists.txt` 에 등록되어 있다(이름 확인). 등록 누락은 없다.
- 시험 안에서도 새는 곳: ENH-012 는 512×512 평탄 영상에서만 단언하고 3072×3072 벤치는 측정만 한다. EI/DI 의 모든 고정 입력이 균일 채움이다. 시험 상수 S0=1000, K_cal=100 과 EIT 표는 코드 상수의 복사본이다(`test_exposure_index.cpp` 23행 “Must match the EIT table”).
- `NLM_EvenWindowSize_HandledGracefully` 는 XPE_OK 와 INVALID_INPUT 둘 다 통과시킨다(`test_noise_reduce.cpp:185`). 왕복 시험들(RoundTrip_…, AC-01)은 같은 모듈의 순방향·역방향끼리 비교라 자기 참조다.

## 6. 결함 후보 (고치지 않음, 읽기로 찾은 후보)

| # | 요구 | 위치 | 후보 |
|---|---|---|---|
| E1 | REQ-ENH-018/021 | `edge_enhance.cpp:164, 211-215` | 선명화가 `|diff| >= threshold` 인 화소에만 적용되고 결과는 항상 `orig ± amount*threshold` 로 잘린다. 변화량이 diff 에 비례하지 않고 상수이며 threshold=0 이면 무동작. SPEC 상한은 `max(orig*2, orig+amount*threshold)`. |
| E2 | REQ-ENH-017 | `contrast_enhance.cpp` 조기 반환(`val_range <= 0`) | 성능 시험이 평탄 500.0 영상이라 CLAHE 가 일을 안 하고 통과(§2). |
| E3 | CC-002 | `enhance_basic_internal.h:109` | 비 FLOAT32 영상에 SPEC 은 INVALID_INPUT, 코드는 UNSUPPORTED_FORMAT. 분기를 건드리는 시험 없음. |
| E4 | REQ-ENH-030/023 | `exposure_index.cpp:87-97` | 비유한 화소 검사 없음. NaN 이면 평균 NaN, `mean <= 0` 이 거짓이라 EI/DI 가 NaN 인 채 XPE_OK. |
| E5 | CC-004 | `enhance_basic.cpp:11` | SPEC 은 전역 가변 상태 금지인데 `std::atomic<int> g_maxThreads` 와 공개 setter 가 있다. |
| E6 | REQ-ENH-013 | `contrast_enhance.cpp:61,113,222-245` | 타일 사이를 보간하지 않고 최근접 타일을 쓴다(주석에 의도라 적힘). 평탄 타일은 전역 범위의 0.5 로 매핑. |
| E7 | REQ-ENH-007 | `noise_reduce.cpp:17-19,211-218` | 분리형 근사에 반경 상한 15 — sigma_space ≳ 7.5 는 SPEC 대로 반영되지 않음. |
| E8 | REQ-ENH-011 | `noise_reduce.cpp:410-448` | sigma 를 중앙 10% ROI 의 MAD 로 추정. 해부학이 든 ROI 는 균일하지 않아 과대 추정(SPEC 은 ROI 를 정의하지 않음). |
| E9 | REQ-ENH-014/016 | `contrast_enhance.cpp:165-168` | 기본값(8×8) 이면 폭 16 미만 영상이 INVALID_INPUT. SPEC 은 NULL = 기본값만 말함. |

추적 항목: 이 대조를 담을 더 맞는 기존 항목을 찾지 못했다. **새 이슈 필요** — E1~E4 가 우선 후보(리더 판단). 이번 커밋은 `Refs #130`.

## 7. 수정안 초안 (리더 소유 파일)

`drafts/ENH_tasks_status_draft.txt` — tasks.md 13행의 상태와 근거 열 수정안.

## 8. Gap / 잔여 위험

Gap(관측하지 않은 것)
- 결함 후보 E1~E9 는 실행 재현을 하지 않았다.
- RTM ✓ 10행이 가리키는 SRS 요구에 대해 행위 축(실제 단언하는 시험이 있는지)을 가르지 않았다.
- `partial` 23건의 사유 문장은 에이전트 서술이며 줄 인용만 기계 검증했다. 사유가 맞는지는 위 §4·§6 에서 내가 직접 읽은 몇 건만 확인했다.
- 에이전트가 제시한 구현 줄 번호는 사용하지 않았다(ENH 는 36건 모두 파일 안에 있었으나 DISP 에서 13건이 파일 밖이라 같은 기준으로 쓰지 않음).

잔여 위험
- `partial` 이 "일부 문구 단언" 이라는 판정은 에이전트의 판단 재량이 들어간다. 경계 사례는 `constant_only`↔`partial` 사이.
- 로컬 읽기만으로 CI 의 동작을 대조했다. ci.yml 필터 문자열은 직접 읽어 인용했다.

## 부록 A. 요구별 판정 전체 표

| 요구 | 판정 | 독립 기대값의 출처 | 시험이 단언하지 않는 문구 | 근거 시험 |
|---|---|---|---|---|
| REQ-ENH-001 | asserted | hand-chosen pixel values with the reference taken from std::log10 of a literal (101, 100); not a call into the |  | `test_log_transform.cpp:48` ForwardTransform_SinglePixel_CorrectFormula; `test_log_transform.cpp:177` MultiplePixels_AllTransformedCorrectly |
| REQ-ENH-002 | asserted | literal 0 (log10(0+1)=0) |  | `test_log_transform.cpp:57` NegativePixel_ClampedToZero; `test_log_transform.cpp:66` ZeroPixel_ProducesZero |
| REQ-ENH-003 | partial | literal return code | "without modifying the image" is not asserted by the requirement tests (images are not compared after the call); only the non-finite refusal tests in  | `test_log_transform.cpp:73` ZeroNormFactor_ReturnsInvalidInput; `test_log_transform.cpp:80` NegativeNormFactor_ReturnsInvalidInput; `test_nonfinite_pixels.cpp:169` LogTransformRefusesANonFiniteNormFactor |
| REQ-ENH-004 | partial | literal pow(10,2)-1 = 99 | the division input/normFactor is not asserted against an independent value: the only value test uses normFactor 1.0 (division is a no-op); other normF | `test_log_transform.cpp:94` InverseTransform_SinglePixel_CorrectFormula; `test_parameter_dependency.cpp:92` LogInverse_NormFactorReachesTheOutput |
| REQ-ENH-005 | partial | literal return code | "without modifying the image" not asserted for 0/negative normFactor | `test_log_transform.cpp:101` InverseZeroNormFactor_ReturnsInvalidInput; `test_log_transform.cpp:108` InverseNegativeNormFactor_ReturnsInvalidInput |
| REQ-ENH-006 | partial | numeric budget 15 ms from the SPEC | CI enforcement: ci.yml:318 runs ctest with -E "Performance/Within[0-9]+ms/..." so these Within15ms tests do not run in CI; one timed sample, no median | `test_log_transform.cpp:148` Performance_3072x3072_Within15ms; `test_log_transform.cpp:159` InversePerformance_3072x3072_Within15ms; `test_log_transform.cpp:198` BenchmarkFreeze_Performance_REQ_ENH_006_LogTransform3072 |
| REQ-ENH-007 | partial | SNR computed against a known clean synthetic signal in the test | that the filter is a bilateral filter (edge preserving, range weighting) rather than any low-pass; no test compares against a reference bilateral or c | `test_enh01_log_noise.cpp:208` Bilateral_OnNoisyImage_SNRImprovement_Above_6dB; `test_noise_reduce.cpp:123` BilateralFilter_NoisyImage_SNRImproves; `test_parameter_dependency.cpp:120` NoiseReduce_EveryParameterReachesTheOutput |
| REQ-ENH-008 | constant_only | none | NLM output never compared to an expected value; no denoising-quality assertion (no SNR test for NLM); the algorithm identity (patch distance / weights | `test_noise_reduce.cpp:171` NLM_ValidSmallImage_ReturnsOk; `test_enh01_log_noise.cpp:305` NLM_Mode_Runs_And_OutputDiffersFromBilateral; `test_parameter_dependency.cpp:149` NoiseReduce_NlmParametersReachTheOutput |
| REQ-ENH-009 | partial | literal return code | "without modifying the image" (image not compared after the call) | `test_noise_reduce.cpp:58` NullParams_ReturnsInvalidInput |
| REQ-ENH-010 | partial | literal return codes | sigma_range == 0 (non-positive includes zero) is not tested; only sigma_space 0 and both negatives | `test_noise_reduce.cpp:135` BilateralFilter_NegativeSigmaSpace_ReturnsInvalidInput; `test_noise_reduce.cpp:147` BilateralFilter_NegativeSigmaRange_ReturnsInvalidInput; `test_noise_reduce.cpp:157` BilateralFilter_ZeroSigmaSpace_ReturnsInvalidInput |
| REQ-ENH-011 | partial | noise generated by std::mt19937 + normal_distribution with known sigma (10, 25), i.e. a value from the generat | the constant 1.4826 and the MAD definition are pinned only to +/-10% (9..11) and +/-20% (20..30); a plain std-dev estimate or constant 1.4/1.6 would p | `test_enh01_log_noise.cpp:147` NoiseSigma_Gaussian10_EstimateWithin10Percent; `test_enh01_log_noise.cpp:148` NoiseSigma_Gaussian10_EstimateWithin10Percent; `test_noise_reduce.cpp:205` SigmaEstimation_KnownNoise |
| REQ-ENH-012 | partial | numeric budget 100 ms from the SPEC | the budget is asserted only at 512x512 on a flat image; at the SPEC size 3072x3072 only a measure-only benchmark (no assertion) plus the 200 ms pipeli | `test_noise_reduce.cpp:235` Performance_Bilateral_Small_Within100ms; `test_noise_reduce.cpp:262` BenchmarkFreeze_Performance_REQ_ENH_012_Bilateral3072 |
| REQ-ENH-013 | partial | properties of CLAHE (stretch, range, per-tile monotonicity) derived without a reference implementation | CLAHE output values are never compared with a reference (no hand-computed equalization of a tiny tile); clip_limit redistribution correctness untested | `test_contrast_enhance.cpp:167` LowContrastRegion_ContrastImproves; `test_exception_guard.cpp:330` ContrastEnhanceKeepsItsImagePropertiesOnEveryTileShape; `test_exception_guard.cpp:353` ContrastEnhanceKeepsItsImagePropertiesOnEveryTileShape |
| REQ-ENH-014 | constant_only | none | the default values clip_limit=3.0, tile 8x8 are not asserted: the only NULL-params calls use a flat image (returns at contrast_enhance.cpp:195 before  | `test_contrast_enhance.cpp:79` NullParams_UsesDefaults_ReturnsOk; `test_empty_image_contract.cpp:148` ValidImageStillAccepted |
| REQ-ENH-015 | asserted | literal return code (0.5 < 1.0) | boundary clip_limit == 1.0 accepted; not-modified | `test_contrast_enhance.cpp:87` ClipLimitBelow1_ReturnsInvalidInput; `test_exception_guard.cpp:591` ContrastEnhanceRefusesANonFiniteClipLimitAndAcceptsLargeFiniteOnes |
| REQ-ENH-016 | asserted | literal return codes |  | `test_contrast_enhance.cpp:99` TileWidthBelow2_ReturnsInvalidInput; `test_contrast_enhance.cpp:111` TileHeightBelow2_ReturnsInvalidInput |
| REQ-ENH-017 | constant_only | numeric budget 50 ms | the budget test runs on a FLAT 500.0 image, which xpe_contrast_enhance answers at contrast_enhance.cpp:195 (val_range <= 0 -> return XPE_OK) before bu | `test_contrast_enhance.cpp:204` Performance_3072x3072_Within50ms; `test_contrast_enhance.cpp:194` Performance_3072x3072_Within50ms; `test_contrast_enhance.cpp:228` BenchmarkFreeze_Performance_REQ_ENH_017_Clahe3072 |
| REQ-ENH-018 | partial | none for the formula; zero/flat cases are trivially independent | the formula output[i] = input + amount*(input - blur) is never compared to an expected value; the "only where /diff/ >= threshold" branch is checked o | `test_edge_enhance.cpp:185` ThresholdGating_FlatRegion_Unchanged; `test_edge_enhance.cpp:134` ZeroAmount_ImageUnchanged; `test_parameter_dependency.cpp:237` EdgeEnhance_EveryParameterReachesTheOutput |
| REQ-ENH-019 | constant_only | none | defaults amount=0.5, radius=2.0, threshold=10.0 not asserted: NULL params call is on a flat 500 image and only checks XPE_OK | `test_edge_enhance.cpp:56` NullParams_UsesDefaults_ReturnsOk; `test_empty_image_contract.cpp:148` ValidImageStillAccepted |
| REQ-ENH-020 | asserted | literal return codes | exact boundary values (0.0, 5.0, 0.5, 10.0 accepted) not asserted | `test_edge_enhance.cpp:64` AmountTooLarge_ReturnsInvalidInput; `test_edge_enhance.cpp:75` AmountNegative_ReturnsInvalidInput; `test_edge_enhance.cpp:87` RadiusTooSmall_ReturnsInvalidInput |
| REQ-ENH-021 | partial | bound formula written in the test from the SPEC text | only the upper bound is asserted and it is looser than what the code enforces; the code clamps to orig +/- amount*threshold (both sides), not to max(o | `test_edge_enhance.cpp:162` OvershootClamp_NeverExceedsBound; `test_edge_enhance.cpp:163` OvershootClamp_NeverExceedsBound |
| REQ-ENH-022 | partial | numeric budget 20 ms | one timed sample on a constant 500.0 image (real work is done, no flat shortcut); not run in CI (ci.yml:318 excludes Within..ms; ci.yml:377 comment re | `test_edge_enhance.cpp:202` Performance_3072x3072_Within20ms; `test_edge_enhance.cpp:226` BenchmarkFreeze_Performance_REQ_ENH_022_Usm3072 |
| REQ-ENH-023 | partial | test-side formula K_CAL*(mean/S0) and a literal EI of 100 at fill 1000; the constants K_cal=100 and S0=1000 ar | "mean_pixel_value" is never exercised on a non-uniform image: every EI fixture is a uniform fill, so mean vs median vs first pixel are indistinguishab | `test_exposure_index.cpp:168` KnownPhantom_EI_MatchesReference; `test_exposure_index_contract.cpp:89` Stage2_EiIsIndependentOfTheTarget_154; `test_exposure_index_contract.cpp:93` Stage2_EiIsIndependentOfTheTarget_154 |
| REQ-ENH-023a | asserted | literal 100.0f at fill == S0 (1000) | the "SHALL NOT claim IEC-conformant absolute magnitude" clause is a documentation obligation, not testable behaviour here | `test_exposure_index_contract.cpp:93` Stage2_EiIsIndependentOfTheTarget_154; `test_exposure_index_contract.cpp:94` Stage2_EiIsIndependentOfTheTarget_154 |
| REQ-ENH-024 | asserted | separate reference expression written in the test with literal EIT (200) and K_cal; plus differences derived f |  | `test_exposure_index_contract.cpp:123` Stage2_DiSeparatesBodyPartsByTheTableRatio_154; `test_exposure_index_contract.cpp:287` Stage2_EveryTableEntryReachesDi_154; `test_exposure_index.cpp:183` KnownPhantom_DI_MatchesReference |
| REQ-ENH-025 | asserted | literal table values copied from the code (the SPEC lists no EIT values, so no independent source exists); rat | case-insensitive matching (the code uppercases both sides) is never exercised: no lowercase/mixed-case body part in any test; table values themselves  | `test_exposure_index_contract.cpp:259` Stage2_EveryTableEntryReachesDi_154; `test_exposure_index_contract.cpp:272` Stage2_EveryTableEntryReachesDi_154; `test_exposure_index.cpp:248` UnknownBodyPart_UsesDefault |
| REQ-ENH-026 | partial | literal threshold 3 dB and a mean chosen to exceed it | the DI < -3 side never asserts an alert (KnownPhantom tests give DI=-6.02 and post an alert nobody checks); exact boundary +/-3.0 untested; alert mess | `test_exposure_index.cpp:205` DIOutOfRange_PostsAlert; `test_exposure_index.cpp:212` DIOutOfRange_PostsAlert; `test_exposure_index.cpp:230` DIInRange_NoAlert |
| REQ-ENH-027 | asserted | literal return codes |  | `test_exposure_index.cpp:86` NullImg_ReturnsInvalidInput; `test_exposure_index.cpp:93` NullMeta_ReturnsInvalidInput; `test_exposure_index.cpp:102` NullOutEI_ReturnsInvalidInput |
| REQ-ENH-028 | partial | literal return codes | "without computing EI/DI": outEI/outDI initialised to -1 in the tests but never compared after the call | `test_exposure_index.cpp:118` EmptyImage_WidthZero_ReturnsInvalidInput; `test_exposure_index.cpp:135` EmptyImage_HeightZero_ReturnsInvalidInput |
| REQ-ENH-029 | partial | test-side K_cal*(750/1000) | the only "ROI sub-buffer" case is a freshly allocated uniform 100x100 image, indistinguishable from a whole image of that size; no non-uniform ROI, no | `test_exposure_index.cpp:264` ROISubBuffer_ComputesOnSubRegion |
| REQ-ENH-030 | partial | literal 0.0 outputs and return code | negative mean ("zero or negative") is never tested; only an all-zero image | `test_exposure_index.cpp:152` ZeroMeanImage_ReturnsProcessingFailed; `test_exposure_index.cpp:153` ZeroMeanImage_ReturnsProcessingFailed; `test_exposure_index.cpp:154` ZeroMeanImage_ReturnsProcessingFailed |
| REQ-ENH-CC-001 | partial | struct-size bounds written in the test | __cdecl is not asserted; "7 API functions" (the header says 8 and the DLL exports 10) not asserted; blittable parameter types and field offsets not as | `test_enhance_integration.cpp:140` StructSizes_PInvokeCompatible; `test_enhance_integration.cpp:314` Version_ReturnsNonNull |
| REQ-ENH-CC-002 | partial | literal return codes | the second condition "format != XPE_PIXEL_FLOAT32 -> XPE_ERR_INVALID_INPUT" is not tested at all (no test passes a non-FLOAT32 image) and the code ans | `test_enhance_integration.cpp:89` AllFunctions_NullImg_ReturnInvalidInput; `test_enhance_integration.cpp:69` AllFunctions_NullImg_ReturnInvalidInput |
| REQ-ENH-CC-003 | partial | CRT heap block/byte counts after N cycles, with a positive control that injects 64 B/cycle | the heap tests run xpe_log_transform and the bilateral/CLAHE/USM triple on a FLAT 64x64 image: CLAHE returns at the flat check before allocating tiles | `test_enhance_integration.cpp:292` NoHeapLeak_1000Iterations_AllocatingPaths; `test_enhance_integration.cpp:307` NoHeapLeak_AllocatingPaths_ControlLeakIsCaught; `test_enhance_integration.cpp:242` NoHeapLeak_1000Iterations |
| REQ-ENH-CC-004 | partial | a serial run of the same code is the reference (not independent) | only xpe_noise_reduce (bilateral) is exercised concurrently, with 2 threads on 128x128; the other six functions are untested for concurrency; the modu | `test_enhance_integration.cpp:205` ThreadSafety_ConcurrentBilateral; `test_thread_determinism.cpp:62` BilateralIsBitIdenticalForEveryThreadCount |
| REQ-ENH-CC-005 | partial | numeric budget 200 ms | run on a constant 500.0 image (after log+bilateral the image may still be flat, in which case the CLAHE stage takes the flat shortcut); single sample; | `test_enhance_integration.cpp:128` FullPipeline_3072x3072_Within200ms; `test_enhance_integration.cpp:94` FullPipeline_3072x3072_Within200ms |
