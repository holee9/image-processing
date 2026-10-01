# QA-A-176 (`#230`) — 시험 주석의 `REQ-P1A-013` 오기 정정과 인용 전수 대조

주석만 바꿨습니다. 코드·단언·시험 이름은 그대로입니다 (`git diff -U0` 의 변경 줄이 전부 주석 줄임을 확인, §1). 제품 소스·공개 헤더·CMake 의 주석은 **고치지 않고** §3 에 목록으로 보고합니다.

**조건**: 트리 `7f4c81d1`(`dev/preprocess`), 빌드 `build/ci-preprocess` RelWithDebInfo. 캐시–프리셋 대조 `OK`, `PRESET_EXIT=0` (`evidence/04`).

---

## 1. 바꾼 주석 — 3파일 5곳

각 주석이 실제로 검증하는 동작을 시험 본문에서 읽었고, 맞는 번호는 SPEC 과 SRS 원문에서 찾았습니다.

| # | 위치 | 이전 | 시험이 실제로 하는 일 | 이후 |
|---|---|---|---|---|
| 1 | `test_preprocess_degraded.cpp:289` (BP-05 머리말) | `(REQ-P1A-013)` | `xpe_nonlinearity_correct(&buf, nullptr)` 이 `XPE_OK` 이고 화소 불변 | `SRS-CALIB-FUNC-006` 로 교체, "null 설정 문장 자체는 SRS 에 없음", "`REQ-P1A-013` 아님: 현재 SPEC 에서 그 번호는 런타임 결함 검출" |
| 2 | 같은 파일 `:285-286` (온도 항목) | 요구 번호 없음 | `xpe_temp_compensate(&buf, 25.0f, nullptr)` 이 UINT16 화소 불변 | `REQ-P1A-080` 추가 (`T_ref = 25 °C` 에서 배율 1, SPEC `spec.md:651`) |
| 3 | 같은 파일 `:331` | `// REQ-P1A-013: null-config … no-op` | 위 #1 의 단언 | `// SRS-CALIB-FUNC-006 (not REQ-P1A-013; see the BP-05-DEG header)` |
| 4 | `test_temp_nonlinearity_binning.cpp:108` | `// REQ-P1A-013: no-op if no coefficients …` | `NullConfigIsNoOp`: null 설정이 `XPE_OK`, 화소 불변 | `// SRS-CALIB-FUNC-006 (not REQ-P1A-013; see the file header)` |
| 5 | `test_nonlin_noop_report.cpp:15-16` | "그 번호는 현재 **defect correction**" | (머리말 서술) | "현재 **runtime defect detection** (`REQ-P1A-012` 가 defect correction)" + 비선형 요구는 `SRS-CALIB-FUNC-006` 이라는 포인터 |

### 맞는 번호를 어디서 찾았나 — "SPEC 에 없으면 지어내지 말 것"

- **`SPEC-XPE-P1A` 에는 비선형 보정 요구가 없습니다.** `spec.md` 의 `REQ-P1A-` 정의(`:99` ~ `:735`)에 비선형이 없고, `:55` 는 `PRE-08: Nonlinearity Correction -- 별도 SPEC` 이며, 이관 대상 `SPEC-XPE-P1D` 는 실재하지 않습니다(`:956`, 리더가 대조군까지 적은 표). `REQ-P1A-095/096` 은 단계 순서·플래그만 다룹니다.
- **그러나 "요구 없음" 이 아닙니다.** `test_temp_nonlinearity_binning.cpp:7-17` 머리말이 이미 정정해 두었듯(`QA-A-150`, *"한 표만 보고 부재를 주장한 오류"*), 비선형의 요구는 **다른 계열**인 `SRS-CALIB-001` 의 `SRS-CALIB-FUNC-006` / `-006-EXT` 에 있습니다. 구현 파일 머리말(`nonlinearity_correct.cpp:3-4`)이 그것을 인용하는 것과 별개로, **SRS 원문을 직접 열어** 확인했습니다 (`docs/calibration/SRS-CALIB-001_Software_Requirements_Specification.md:55`):
  - `FUNC-006`: *"apply nonlinearity correction using lookup table (LUT) or monotonic polynomial fitting … where `f_nonlin` is detector-specific and stored in calibration profile … Detector profile governs enable/disable via field `panel.linear`"*
  - `SAFE-001` (`:306`): *"Defect, ghost, nonlinearity, binning, and temperature corrections are optional and conditional."*
- **정확히 무엇이 근거인가**: SRS 는 **"설정이 없으면 no-op"이라는 문장을 적고 있지 않습니다.** 가장 가까운 것이 위 두 문장(보정 함수는 프로파일에서 오고, 선택적·조건부)이고, 주석에도 "null 설정 문장 자체는 SRS 에 없음" 이라 적었습니다. 그래서 #1·#3·#4 는 "`SRS-CALIB-FUNC-006`" 을 **요구의 출처로** 인용하되 null 설정 동작을 SRS 가 명시한다고 쓰지 않았습니다.
- 대조군: `SRS-CALIB-FUNC-006` 검색은 구현·문서·클라이언트 카탈로그에서 여러 곳을 찾아내고, 이웃 번호 `FUNC-005` 검색도 29곳을 찾아냅니다 (검색이 읽고 있음).
- 온도(#2): `REQ-P1A-080` 은 `spec.md:651` 에 있고 `T_ref = 25 °C`, UINT16, 제자리 쓰기를 적습니다 — 시험의 입력(`25.0f`, UINT16 버퍼)과 일치합니다. 같은 요구를 SRS 쪽에서는 `SRS-CALIB-FUNC-008`(`:120`, `T_ref = 25°C`)이 말합니다.

### 검증 (주석만 바뀌었다는 증거와 시험 통과)

| 항목 | 명령 | 결과 |
|---|---|---|
| 변경이 주석뿐 | `git diff -U0 -- modules/preprocess/tests` 에서 주석 줄(`//`, `*`, `/*`)이 아닌 변경 줄 | **0줄** |
| 빌드 | `cmake --build build\ci-preprocess --config RelWithDebInfo --target xpe_preprocess_tests` (`evidence/02`) | `[4/4] Linking …`, `BUILD_EXIT=0` |
| 영향받는 시험 | `xpe_preprocess_tests.exe --gtest_filter=PreprocessDegraded.*:*Nonlin*` (`evidence/03`) | `62 tests from 10 test suites … PASSED`, `TESTS_EXIT=0` |
| 세 수 (규약) | `ctest -N` (`evidence/05, 06`) | **실행 790 / 총계 826 / DISABLED 36** — 변경 전과 같음 |

---

## 2. 시험 전체의 `REQ-P1A-013` 인용 — 런타임 검출기를 실제로 부르는지 대조

`modules/preprocess/tests` 에서 `P1A-013` 을 찾은 **26곳**(바꾼 4곳 포함)입니다. "부름" = 파일이 공개 진입점 `xpe_defect_detect_runtime` 을 호출함, "내부" = 진입점은 부르지 않지만 `runtime_detection.h` 의 검출기 구성요소를 시험함.

| 파일:줄 | 호출 | 판정 |
|---|---|---|
| `test_defect_correct.cpp:4` | 부름 (`:337`, `:342`, `:356`) | 맞음 |
| `test_runtime_detection_rates.cpp` `:3` `:6` `:212` `:233` `:238` `:278` `:287` `:330` `:356` `:370` `:379` | 부름 | 맞음 (11곳) |
| `test_runtime_detection_functional.cpp:3` `:129` `:176` | 부름 | 맞음 |
| `test_runtime_detection_avx2_parity.cpp:28` | 부름 (`ShippedEntryPointAgreesWithTheScalarRule`) | 맞음 |
| `test_runtime_detection_performance_gate.cpp:64` | 부름 | 맞음 |
| `test_runtime_detection_radix_select_parity.cpp:24` | **내부**: `SelectKthSmallest`, `ComputeGlobalSigma` | 맞음 (검출기의 선택·시그마 단계 구현) |
| `test_runtime_detection_median8_parity.cpp:36` | **내부**: `ComputeMedian` (8값 중앙값 경로) | 맞음 (검출 망) |
| `test_runtime_detection_sigma_buffer_parity.cpp:30` | **내부**: `ComputeGlobalSigma`, `SelectKthSmallest` | 맞음 |
| `test_runtime_detection_thread_parity.cpp:32` | **내부**: `DetectFrame`, `ComputeGlobalSigmaThreaded` | 맞음 |
| `test_zz_a172_path.cpp:12` | 부름 (프로브) | 맞음 (산문 인용) |
| `test_preprocess_degraded.cpp:289`, `:331` | **안 부름** (`xpe_defect_detect_runtime` 0건) | **오기 → 정정 (#1, #3)** |
| `test_temp_nonlinearity_binning.cpp:108` | 이 시험은 비선형 | **오기 → 정정 (#4)** |
| `test_nonlin_noop_report.cpp:15` | 인용이 *고아*라는 서술 | 번호의 현재 의미를 **잘못 적음 → 정정 (#5)** |

"내부" 네 건은 공개 진입점을 부르지는 않지만 `SelectKthSmallest`·`ComputeMedian`·`ComputeGlobalSigma`·`DetectFrame` 은 `REQ-P1A-013` 알고리즘 자체의 구현이라 인용이 맞다고 봤습니다. 이는 카드의 "부르지 않는데 013 을 인용하면 같은 오기" 의 엄격한 읽기에서는 경계 사례이므로 구분해 적습니다.

**참고 (같은 파일의 검출 시험)**: `test_temp_nonlinearity_binning.cpp:179-255` 의 `RuntimeDetection.*` 5건은 탐지기를 부르지만 **요구 번호 인용이 없습니다.** 오기는 아니며, 가질 수 있는 번호는 `REQ-P1A-013` 입니다 (바꾸지 않았습니다).

**한계**: 검색식은 `REQ-P1A-013|P1A-013` 입니다. `013` 만 따로 적었거나 다른 철자("P1A 013" 등)는 찾지 않았습니다. 대조군으로 같은 도구가 `REQ-P1A-012`·`-080`·`-081` 인용은 찾아냅니다.

---

## 3. 모듈 안 제품 소스·헤더·CMake 의 인용 (**고치지 않음**, 목록만)

카드의 범위는 시험 주석이므로 제품 소스·공개 헤더·CMake 는 건드리지 않았습니다. 리더가 정하십시오.

| 위치 | 인용 | 판정 |
|---|---|---|
| `include/xpe/preprocess_api.h:648` | `REQ-P1A-013: No-op when no config supplied` (비선형 API 문서) | **같은 오기** (공개 헤더) |
| 같은 블록 `:646-650` | `REQ-P1A-012` (비선형 적용), `-013`, `-014` (알 수 없는 mode), `-015` (항등 다항식) | **블록 전체가 옛 번호** — 현재 012 는 결함 보정, 014 는 교정 파일 읽기, 015 는 게인 읽기 (`spec.md:563`, `:570`). `-014` 는 `test_nonlin_no_mode_rejection.cpp:16` 이 이미 같은 지적 |
| `src/nonlinearity_correct.cpp:261` | "`REQ-P1A-013` in the CURRENT set is **defect correction** (the Hampel recipe …)" | **#5 와 같은 부정확**: Hampel 은 *검출*이고 현재 013 은 런타임 결함 **검출** |
| `src/nonlinearity_correct.cpp:248`, `:257` | 옛 인용을 *인용*하는 서술 | 맞음 (역사 서술) |
| `CMakeLists.txt:25` | `src/defect_correct.cpp # SWU-1.3 REQ-P1A-012, REQ-P1A-013` | 불명확: 같은 파일 `:276` 이 "Runtime detection implementation moved to `runtime_detection.cpp`" 라 적어, 이 파일은 더는 013 을 구현하지 않음 |
| `src/defect_correct.cpp:276` | `moved to runtime_detection.cpp (REQ-P1A-013)` | 맞음 |
| `src/runtime_detection.cpp:5,8,47`, `include/runtime_detection.h:5,87,102,307,783,1020,1048,1051`, `include/xpe/preprocess_api.h:521,527`, `tools/xpe_detect_experiment.cpp:19,296`, `tools/xpe_real_frames.cpp:48,99,138`, `CMakeLists.txt:29,443` | 검출기 | 맞음 |

### 같은 계열의 인접 오기 (013 밖, 읽기만)

`test_temp_nonlinearity_binning.cpp:157` `// REQ-P1A-020: binningMode == 1 is no-op` 과 `:165` `// REQ-P1A-021: unknown binning mode -> XPE_ERR_CONFIG_INVALID`. 현재 SPEC 에서 `REQ-P1A-020` 은 *Not-Initialized Guard*, `-021` 은 *Dimension Mismatch Guard* (`spec.md:614`, `:629`)이고 비닝은 `REQ-P1A-090/091` 입니다. 이 카드의 범위(013)가 아니라 바꾸지 않았고, 같은 파일의 `:74`·`:82` 의 `REQ-P1A-081`(온도 입력 가드)은 맞습니다.

---

## 4. `Avx2Parity` / `AVX2Parity` 철자 — 인용처 목록 (**고치지 않음**)

**시험 이름의 실제 철자** (`ctest -N`, `evidence/01`):

| ctest `-R` 패턴 | 잡히는 시험 수 | 무엇이 잡히나 |
|---|---|---|
| `Avx2Parity` | **10** | `Avx2ParityTest.*` (런타임 검출, `test_runtime_detection_avx2_parity.cpp`) |
| `AVX2Parity` | **6** | `OffsetCorrectAVX2ParityTest` 3 + `GainCorrectAVX2ParityTest` 3 |
| `OffsetAVX2Parity` | **0** | — |
| `GainAVX2Parity` | **0** | — |
| `DefectAVX2Parity` | **0** | — |
| `RuntimeDetectAVX2Parity` | **0** | — |

즉 두 철자의 시험 이름(`Avx2ParityTest` 와 `…AVX2ParityTest`)이 공존하고, **문서가 인용하는 철자 중 네 개는 어느 시험도 잡지 못합니다.** 대조군: 앞의 두 패턴은 실제로 시험을 잡으므로 `ctest -N -R` 은 동작합니다.

**인용처 전수** (검색: 대소문자 무시 `avx2parity`, `build`·`_deps`·`.git` 제외, **`.moai/reports/` 제외** — 과거 카드 보고서는 역사 기록이라 별도), 이 트리(`7f4c81d1`) 기준:

| 인용처 | 줄 | 인용한 철자 | 이 철자의 현재 일치 |
|---|---|---|---|
| `docs/post-processing/xpe/preprocess/VVP-PREPROCESS-001.md` | `:21`, `:236`, `:259` | `ctest -R AVX2Parity` | 6건만 잡힘 (리더가 `ed890f19` 에서 `:236` 을 반영했다면 이 줄 번호는 이미 달라졌을 수 있음 — 이 트리에는 그 커밋이 없습니다) |
| `.moai/specs/SPEC-SIMD-001/spec.md` | `:123` `:124` `:125` `:126` | `OffsetAVX2Parity`, `GainAVX2Parity`, `DefectAVX2Parity`, `RuntimeDetectAVX2Parity` | **0건씩** |
| `.moai/specs/SPEC-SIMD-001/progress.md` | `:122` `:123` `:124` `:125` | 같은 네 개 | **0건씩** |
| `modules/preprocess/include/runtime_detection.h` | `:49` | `Avx2ParityTest.NoAvx2PathCompiledIn` | 일치 (시험 이름 그대로) |
| `docs/help/generated/doxygen/html/runtime__detection_8h.html:453`, `…_8h_source.html:152`, `xml/runtime__detection_8h.xml:104,347` | | 위 `:49` 에서 생성된 문서 | 일치 (생성물) |
| `modules/preprocess/tests/test_gain_correct_avx2_parity.cpp`, `test_offset_correct_avx2_parity.cpp`, `test_runtime_detection_avx2_parity.cpp` | 클래스·`TEST` 정의 | 정의 자체 | — |

- **`.github/`, `tools/`, `scripts/`, `cmake/`, 루트 `CMakeLists.txt`, `CMakePresets.json` 에는 인용이 없습니다.** 대조군: 같은 방식으로 `.github/workflows/ci.yml` 에서 `ctest` 는 찾아집니다(`:133`, `:185`). 즉 **CI 와 스크립트가 이 이름에 의존하지 않으므로 이름을 바꿔도 자동화는 끊기지 않고, 끊기는 것은 문서의 `ctest -R` 명령뿐입니다.**
- `SPEC-SIMD-001` 의 `ctest -R DefectAVX2Parity` 는 철자 문제 이전에 **그 시험 파일이 없습니다**(`QA-A-145` 가 `test_defect_correct_determinism.cpp` 로 개명, `CMakeLists.txt:369`). 이름 통일의 범위를 정할 때 함께 보셔야 합니다.
- 이 목록의 한계: 문서 형식 파일만 찾았습니다(`-I` 로 이진 제외). 이슈 본문·외부 위키는 보지 않았습니다.

---

## 5. 하지 않은 것

- 코드·단언·시험 이름 변경 없음, `Avx2Parity` 철자 수정 없음
- 제품 소스·공개 헤더·CMake 주석 수정 없음 (§3 목록으로 보고)
- `docs/`·SPEC·VVP 수정 없음, push 없음, 새 이슈 없음

## 6. 미검증

- SRS 가 null 설정 no-op 을 **문장으로** 요구하는지는 못 찾았습니다(§1). 다른 SRS 절·다른 문서에 있을 가능성을 배제하지 않았습니다.
- `ed890f19`(리더의 VVP 수정)와 `3a4855ff` 는 이 트리에 없습니다. VVP 인용 줄 번호는 `7f4c81d1` 기준입니다.

🗿 MoAI
