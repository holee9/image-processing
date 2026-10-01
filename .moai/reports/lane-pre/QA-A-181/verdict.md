# QA-A-181 (`#230`) — `CMakeLists.txt` 소스 목록 다섯 줄의 옛 REQ 번호를 요구 **본문**과 구현으로 정정

주석만 바꿨습니다(`CMakeLists.txt` 다섯 줄). 이번에는 제목 목록이 아니라 **요구 본문**(`spec.md`)과 **구현 파일**을 읽고 대조했습니다. 그 결과 지난 카드에서 제가 제목만 보고 적은 것 하나(`pipeline`)가 불완전했음이 드러나 §3 에서 정정합니다.

**조건**: 시작 전 로컬 `main`(`fe2cae78`, 리더가 `QA-A-180` 을 병합한 것)을 `dev/preprocess` 에 fast-forward 병합(이쪽 0 커밋 앞섬·1 커밋 뒤처짐). 병합이 `modules/preprocess`·`modules/common`·`cmake` 를 바꾸지 않았음(`git diff --stat` 빈 출력). 빌드 `build/ci-preprocess` RelWithDebInfo.

---

## 1. 바꾼 다섯 줄

| 줄 | 이전 | 이후 |
|---|---|---|
| `:20` gain | `# SWU-1.2 REQ-P1A-016..019 (new 3-arg g_calib API)` | `# SWU-1.2 REQ-P1A-011 (new 3-arg g_calib API); guards REQ-P1A-020, REQ-P1A-020a` |
| `:21` readout | `# SWU-1.9 REQ-P1A-001..004` | `# SWU-1.9 REQ-P1A-041 (dropped columns, rows above 0.9 x full scale; no line-noise check)` |
| `:22` temp | `# SWU-1.6 REQ-P1A-005..008` | `# SWU-1.6 REQ-P1A-080, REQ-P1A-081 (the flag, REQ-P1A-082, is set by the pipeline)` |
| `:23` nonlinearity | `# SWU-1.7 REQ-P1A-012..015` | `# SWU-1.7 SRS-CALIB-FUNC-006 / -006-EXT (SPEC-XPE-P1A has no nonlinearity requirement)` |
| `:31` pipeline | `# Full pipeline integration (REQ-P1A-041..047)` | `# Full pipeline integration (REQ-P1A-095..101; xpe_calib_state_load: REQ-P1A-016a; _ex/_batch have no REQ)` |

---

## 2. 줄마다: 요구 본문 ↔ 구현

### `:20` `gain_correct.cpp` → `REQ-P1A-011`, 가드 `020`·`020a`

- **본문** (`spec.md:152`): *"When `xpe_gain_correct(input, output, metadata)` is called on an initialized module with a previously loaded gain map … the module shall multiply each pixel of `input` by the corresponding gain factor from the module-global calibration store and write the FLOAT32 result to `output`."*
- **구현**: 시그니처 `xpe_gain_correct(const XpeImageBuffer* input, XpeImageBuffer* output, const XpeImageMetadata* metadata)`(`gain_correct.cpp:242-245`)가 본문과 같고, 같은 파일이 이미 `@MX:SPEC: REQ-P1A-011, REQ-P1A-020`(`:241`)을 답니다. `:282` `if (!xpe_preprocess_is_initialized()) return XPE_ERR_NOT_INITIALIZED;` 가 `REQ-P1A-020`(`spec.md:614`, *"all processing functions shall return `XPE_ERR_NOT_INITIALIZED`"*)을, `:318` 의 `else if (!g_calib.gain_map)` 분기가 반환하는 `XPE_ERR_CALIB_NOT_LOADED` 가 `REQ-P1A-020a`(`:621`, *"… `xpe_gain_correct` … shall return `XPE_ERR_CALIB_NOT_LOADED`"*)를 구현합니다.
- **옛 `016..019` 가 틀린 이유**: 현재 `016`·`017`·`018`·`019` 는 결함 맵 읽기·오프셋 생성·만료 검사·저장(`spec.md:577`~`:605`)이고 게인 *보정*이 아닙니다.
- **`021`·`022` 는 넣지 않았습니다.** `021` 본문은 *"dimensions differ … return `XPE_ERR_INVALID_INPUT`"* 인데 이 함수는 치수 불일치에 `XPE_ERR_BUFFER_TOO_SMALL` 을 돌려줍니다(`gain_correct.cpp:252-253`, 맵 치수 불일치도 `:309`·`:327` 에서 같은 코드). 본문과 구현이 맞지 않는 번호를 근거처럼 적지 않기 위해서입니다 (§4 참고).

### `:21` `readout_validate.cpp` → `REQ-P1A-041` — **일부만 구현**

- **본문** (`spec.md:858`): *"Where the caller provides a raw frame, the module shall validate it for **line noise, dropped columns, and ADC saturation patterns** via `xpe_validate_readout_artifact()`."*
- **구현** (`readout_validate.cpp:14-58`): 함수 이름은 일치. 하는 일은 (1) 모든 화소가 0 인 열 → `has_dropped_columns`, (2) 행 평균이 `0.9 × 65535` 를 넘는 행 → `has_nonuniform_gain`(상수 이름은 `kLineNoiseFrac` 이지만 **선 잡음 검출이 아닙니다**), (3) 항상 `XPE_OK`. **선 잡음 검출은 없고**, "ADC 포화 패턴" 은 밝은 행 검사가 가장 가깝지만 출력 플래그가 `has_nonuniform_gain` 입니다.
- 번호는 `041` 이 맞습니다(요구가 이 함수를 이름으로 부름). 다만 본문 전부를 충족하지는 않으므로 줄에 *"dropped columns, rows above 0.9 x full scale; no line-noise check"* 로 **구현이 실제로 하는 일**을 적었습니다.
- **옛 `001..004` 가 틀린 이유**: 현재 `001`~`004` 는 모듈 초기화·ABI·스레드 안전·오류 코드(`spec.md:99`~`:122`)입니다. 이 파일의 옛 인용(`:32` 열, `:42` 이득 비균일성, `:56` 항상 `XPE_OK`)은 옛 SPEC 의 읽기 검증 네 줄을 가리킨 것입니다.

### `:22` `temp_compensate.cpp` → `REQ-P1A-080`, `081`

- **`080` 본문** (`spec.md:651`): *"`xpe_temp_compensate(img, detectorTempC, configJsonOrNull)` … **UINT16** buffer … scale each pixel by the inverse of the dark-current factor relative to `T_ref = 25 °C`, computed as `exp(-Eg/2kT) / exp(-Eg/2kT_ref)`, writing the result **in place** and clamping to `65535`."* ↔ 구현 `temp_compensate.cpp` 의 `exp_T/exp_ref`, `px[i] / scale`, `std::min(corrected, 65535.0f)`, UINT16 전용 검사(`:30`), 클램프(`:55`)가 일치.
- **`081` 본문** (`:660`): *"If `detectorTempC` is `NaN`, substitute `25.0 °C`. If … outside `[-20.0, +60.0] °C`, return `XPE_ERR_INVALID_INPUT` without modifying the image."* ↔ `:33` NaN 치환, `:36` 범위 검사 — NaN 치환이 범위 검사보다 앞섬까지 일치.
- **`082`(플래그)를 뺀 이유**: `082` 본문의 *"the pipeline shall set `XPE_FLAG_TEMP_COMPENSATED`"* 이고 이 함수는 메타데이터를 받지 않습니다(시그니처에 없음). 플래그는 `pipeline.cpp` 가 겁니다. 그래서 줄에 그 사실을 적었습니다.
- **옛 `005..008` 이 틀린 이유**: 현재 `005` 는 *Input Validation*(모든 내보내기 함수의 널·0 검사), `006`~`008` 은 정의가 없습니다.

### `:23` `nonlinearity_correct.cpp` → `SRS-CALIB-FUNC-006 / -006-EXT`

- 현재 SPEC 에는 비선형 요구가 없습니다(`spec.md:55` `PRE-08 … 별도 SPEC`, `:956` `SPEC-XPE-P1D` 부재). 요구는 `SRS-CALIB-001` 의 `FUNC-006`(`docs/calibration/SRS-CALIB-001_Software_Requirements_Specification.md:55`: *"apply nonlinearity correction using lookup table (LUT) or monotonic polynomial fitting … `f_nonlin` … stored in calibration profile"*)과 `-006-EXT`(`:57` `6a` LUT, `:88` `6b` 다항식)입니다. 구현 파일 머리말(`nonlinearity_correct.cpp:3-4`)도 같은 인용입니다. `QA-A-176`·`177` 에서 확인한 내용 그대로입니다.

### `:31` `pipeline.cpp` → `REQ-P1A-095..101`, `016a`

본문과 구현을 일곱 개 모두 확인했습니다:

| 요구 | 본문 | 구현 (`pipeline.cpp`) |
|---|---|---|
| `095` 단계 순서 | readout → temp → offset → nonlinearity → gain → binning → defect → ghost | 단계 표지가 `:114` 0.5(readout), `:125` 1(temp), `:142` 2(offset), `:158` 3(nonlinearity), `:184` 4(gain), `:209` 5(binning), `:233` 6(defect), `:276` 7(ghost) — **같은 순서** |
| `096` 단계 플래그 | 성공한 단계의 비트를 켬 | `meta->flags \|= XPE_FLAG_GHOST_CORRECTED` 같은 설정(`:297`)이 단계마다 있음 (플래그 여덟 개를 하나씩 대조하지는 않음) |
| `097` 단계별 bypass | 키가 `"true"` 면 건너뜀, 플래그 없음 | `:55-77` 여덟 개 키, `bypassStr == "true"` 정확 일치 |
| `098` ghost 핸들 의존 | `ghostHandle` 이 NULL 이면 건너뜀 | `:280` `if (!cfg.bypassGhost && ghostHandle)` |
| `099` ghost 버퍼 격리 | stage-6 버퍼가 아닌 복사본을 줌 | `:292-294` `std::memcpy(stage7Data.data(), stage6.data, …)` |
| `100` 데이터 영역 전이 | 게인 단계 안에서 UINT16 → FLOAT32 | `:184`·`:194`·`:199` ("DOMAIN TRANSITION") |
| `101` 결함 단계 가용성 | 결함 맵이 없으면 `XPE_ERR_CALIB_NOT_LOADED` | `:248-253` `defectAvailable`, `if (!defectAvailable) return XPE_ERR_CALIB_NOT_LOADED;` |

같은 파일이 정의하는 다른 함수: `xpe_calib_state_load`(`:366`)는 `REQ-P1A-016a` 본문(*"`xpe_calib_state_load(state, calibPath)` … compose `offset.xcal`, `gain.xcal` and `defect.xcal` …"*)이 이름으로 부릅니다. `xpe_preprocess_pipeline_ex`(`:425`)와 `_batch`(`:451`)는 SPEC 이 *"아직 서술하지 않았습니다"* 라고 적은 것(`spec.md` `4.3c` 의 "이 절의 미검증")이라 줄에 *"_ex/_batch have no REQ"* 로 적었습니다 — `_ex` 는 `016a` 가 이름은 부르지만 그것은 캘리브 상태 계약이지 파이프라인 계약이 아닙니다.

---

## 3. 지난 카드(`QA-A-180`)의 제 표 정정

`QA-A-180` §1 의 목록에서 pipeline 의 현재 번호를 **"`095`~`099`"** 라고 적었는데 **`100`·`101` 이 빠졌습니다.** SPEC 에 `REQ-P1A-100`(데이터 영역 전이)과 `REQ-P1A-101`(결함 단계 가용성)이 있습니다(`spec.md:776`, `:785`). 제목 목록을 뽑는 검색식(`REQ-P1A-0(0[1-5]|1[0-9]|4[0-9]|8[0-9]|9[0-9])`)이 세 자리 번호를 포함하지 않았기 때문입니다 — 이번에는 `^#### REQ-P1A-` 전체를 뽑아 대조했습니다. 그 표의 나머지(`gain`·`readout`·`temp`·`nonlinearity`)의 "틀렸다" 판정은 이번 본문 대조에서도 유지됩니다. `QA-A-180` 의 `:19` offset "맞음" 은 **제목 기준**이었고 이번에 본문과 맞춰 본 결과는 §4 b 입니다.

---

## 4. 본문과 구현이 어긋나는 곳 — 고치지 않고 보고만

**a. `REQ-P1A-041` (readout)**: 위 §2 와 같음. 본문의 "line noise" 검출이 구현에 없고, 플래그 이름(`has_nonuniform_gain`)과 상수 이름(`kLineNoiseFrac`)이 하는 일과 다릅니다. 요구가 맞는지 구현이 맞는지는 이 카드가 판정하지 않았습니다.

**b. `REQ-P1A-021` (치수 불일치) ↔ offset·gain 구현**: `021` 본문은 *"`XPE_ERR_INVALID_INPUT`"* 인데 `offset_correct.cpp:155-156`, `:185-186` 과 `gain_correct.cpp:252-253` 은 **`XPE_ERR_BUFFER_TOO_SMALL`** 을 돌려줍니다. `docs/project/api-spec.md:152` 는 `XPE_ERR_INVALID_INPUT` 을 *"NULL pointer, out-of-range value, wrong dimensions"* 로, `:103`·`:108` 은 출력 버퍼가 필요한 바이트보다 작으면 `XPE_ERR_BUFFER_TOO_SMALL` 로 적습니다 — 즉 SPEC 본문과 api-spec 이 서로 다른 말을 하고 구현은 후자를 따릅니다. `CMakeLists.txt:19` 의 `offset_correct.cpp # … REQ-P1A-021` 은 **주제로는 맞지만** 오류 코드가 본문과 다르므로, 이번 카드에서 그 줄은 건드리지 않았습니다.

**c. 소스 파일 자신의 옛 인용** (이번 카드 범위 밖, 같은 유형 — 목록만):

| 파일:줄 | 인용 | 현재 번호 |
|---|---|---|
| `readout_validate.cpp:4` `:16` `:32` `:42` `:56` | `REQ-P1A-001 to 004`, `@MX:SPEC: REQ-P1A-001`, `002`, `003`, `004` | `041` |
| `temp_compensate.cpp:23`, `:51` | `@MX:SPEC: REQ-P1A-005`, `// REQ-P1A-005: apply correction` | `080` (`005` 는 입력 검증) |
| `temp_compensate.cpp:5` | `REQ-P1A-080 to REQ-P1A-082` | `080`, `081` (`082` 는 파이프라인이 건 플래그) |
| `pipeline.cpp:4`, `:24`, `:325` | `REQ-P1A-095 to REQ-P1A-099` | `095..101` (`100`, `101` 누락) |

맞는 쪽: `gain_correct.cpp:5`·`:241` (`REQ-P1A-011`, `REQ-P1A-020`), `nonlinearity_correct.cpp` 머리말(SRS 인용).

---

## 5. 주석만 바뀌었다는 증거

| 항목 | 방법 | 결과 |
|---|---|---|
| 소스 | `HEAD` 와 작업본을 **주석(`#` 이후) 제거 후 비교** (`evidence/02_comment_strip_equivalence.txt`, 스크립트 `02b_cmake_comment_check.py`) | `CMakeLists.txt` **IDENTICAL** |
| 대조군 | 같은 도구에 코드 토큰(`src/binning_correct.cpp` → `…X.cpp`)을 바꾼 사본 | **감지함** (`detected: True`) |
| ABI | `dumpbin /exports build\ci-preprocess\bin\xpe_preprocess.dll` 변경 전·후 (`evidence/01`, `04`, `05`) | 내보내기 **48개 = 48개**, 서수·이름 동일, RVA·`@ILT` 열까지 **줄 전체 동일** (`DIFF_EXIT=0`) |
| 빌드 | `cmake --build build\ci-preprocess --config RelWithDebInfo --target xpe_preprocess_tests` (`evidence/03`) | CMake 재구성 뒤 `BUILD_EXIT=0`, `error C`·`warning C` 0건 |
| 시험 (전체) | `xpe_preprocess_tests.exe` 필터 없이 (`evidence/06`) | `721 tests from 94 test suites ran`, **`713 PASSED`**, 8 `SKIPPED`, **실패 0**, `TESTS_EXIT=0` |
| 캐시–프리셋 대조 (규약) | `check_cache_matches_preset.py build/ci-preprocess ci-preprocess` (`evidence/07`) | `OK`, `PRESET_EXIT=0` |
| 세 수 (규약) | `ctest -N` (`evidence/08`, `09`) | **실행 790 / 총계 826 / DISABLED 36** — 변경 전과 같음 |

**한계**: `SKIPPED` 8건은 시험 자신의 `GTEST_SKIP`(예: `test_xpe_calib_save.cpp:167`)이고 이번 변경 전의 건수와는 **비교하지 않았습니다**(주석만 바꾼 변경이라 영향이 없다는 것은 위 내보내기 동일에서 따라오지만, 직접 대조한 값은 아닙니다). `ctest -N` 의 "실행 790" 과 이 실행 파일 한 개의 `721` 이 다른 것은 `ctest` 가 시험 전체를 항목별로 세는 방식과 한 프로세스 실행의 차이로 보이며, 이 차이를 이번 카드에서 따로 확인하지 않았습니다. 주석 제거 비교는 `#` 이후를 지우는 단순한 방식이라 CMake 의 인용 문자열 안 `#`(이 파일의 해당 줄에는 없음)은 다루지 않습니다.

---

## 6. 하지 않은 것

- 코드·단언·시험 이름 변경 없음, push 없음, 새 이슈 없음
- 소스 파일 자신의 인용(§4 c), `CMakeLists.txt:19`(§4 b), 생성된 Doxygen·`docs/`·SPEC·VVP 수정 없음

## 7. 미검증

- `REQ-P1A-096` 의 플래그 여덟 개를 하나씩 코드와 대조하지는 않았습니다 (단계 순서와 `GHOST` 플래그 한 줄만 읽음).
- `REQ-P1A-020a` 가 `pipeline` 진입점에도 적용된다는 문장(본문 후반)은 읽었지만 `pipeline.cpp` 에서 오프셋·게인 가용성 반환을 올리는 경로를 이 카드에서 따라가지 않았습니다 (`REQ-P1A-101` 아래 기록 문단이 그것을 다룹니다).
- `readout_validate.cpp` 가 본문 전부가 아닌 일부만 구현한다는 판단은 코드 읽기이며, 시험(`test_readout_validate.cpp`)이 무엇을 단언하는지는 보지 않았습니다.

🗿 MoAI
