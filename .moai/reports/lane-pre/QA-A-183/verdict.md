# QA-A-183 (`#230`) — 잔여 점검 목록 처리: ① 정의 없는 번호 인용 정정, ② `REQ-P1A-066` 조사

①은 주석만 바꿨고, ②는 **보고만** 합니다(시험 파일명·시험 이름은 그대로).

**조건**: 시작 전 로컬 `main`(`6cf18d8f`, 리더가 `QA-A-182` 를 병합한 것)을 `dev/preprocess` 에 fast-forward 병합(이쪽 0 커밋 앞섬·1 커밋 뒤처짐). 병합이 `modules/preprocess`·`modules/common`·`cmake` 를 바꾸지 않았음(`git diff --stat` 빈 출력). 빌드 `build/ci-preprocess` RelWithDebInfo.

---

## ① 정정 — 3파일

### `preprocess_api.h:1067` · `xpe_verify_metrics.cpp:685` — `xpe_verify_pipeline`

| 이전 | 이후 |
|---|---|
| `SRS-CALIB-FUNC-015 / SRS-CALIB-FUNC-021 / REQ-P1A-041..047: Pipeline verification` | `SRS-CALIB-FUNC-015 / SRS-CALIB-FUNC-021: Pipeline verification. No REQ-P1A- requirement covers xpe_verify_* (spec.md does not mention them); the REQ-P1A-041..047 this line used to cite were the pre-bc22093 pipeline-stage requirements, now REQ-P1A-095..101.` |

**근거**

- 옛 `041`~`047` 은 `git show ee2c607:.moai/specs/SPEC-XPE-P1A/spec.md` (`:156-176`)에서 **파이프라인 단계 요구**(순서·재정렬 금지·`uint16 → float32` 전이·비닝 건너뜀·ghost 건너뜀·단계별 가용성·단계별 플래그)임을 확인했습니다. 현재 SPEC 에서 그 내용은 `095`~`101` 이고, 현재 `041`·`042` 는 읽기 검증·파라미터 범위 질의, `043`~`047` 은 정의가 없습니다.
- `xpe_verify_pipeline` 은 원본·최종 영상의 SNR 개선을 계산하는 **검증 지표 함수**이지 파이프라인이 아닙니다. `spec.md` 에서 `xpe_verify`·`verify_pipeline`·`verify_metrics` 를 검색하면 **0건**(`grep`)이라 이 함수를 요구하는 `REQ-P1A-` 가 없습니다. 그래서 번호를 새로 고르지 않고 "요구 없음"을 적었습니다.
- 이미 적혀 있던 두 SRS 번호는 그대로 둡니다: `SRS-CALIB-FUNC-015`(전처리 E2E 지표 보고서 `xpe-pre-e2e-report-v1`, "pass/fail gates" 포함)와 `SRS-CALIB-FUNC-021`(Calibration Effect Score)은 이 함수의 출력(`snr_improvement_db`, `overall_pass`)과 주제가 맞습니다. 다만 **SNR 개선을 계산한다는 문장 자체는 두 SRS 항목에 없습니다** — 이 줄이 "맞는 근거"라고 단정하지 않았습니다.

### `test_golden_reference.cpp` — 헤더 `REQ coverage` 와 절 제목·시험 주석 (같은 파일)

카드가 지목한 `:11-12`(`005..008`·`020..023` 등)를 고치려고 시험 본문을 읽어 보니 **같은 파일 안의 절 제목과 개별 시험 주석에도 같은 종류의 옛 번호**가 있었습니다. 헤더만 고치면 파일 안에서 서로 모순되므로 **이 파일 안의 같은 인용을 함께** 고쳤습니다(파일 밖은 건드리지 않음). 되돌리고 싶으면 이 파일의 변경만 되돌리면 됩니다.

| 줄 | 이전 | 이후 | 시험이 실제로 하는 일 |
|---|---|---|---|
| `:11-12` 헤더 | `010, 021, 016..019, 085..087, 005..008, 020..023, 001..004` | 네 줄: `010 (offset)`, `011 (gain; the zero-gain case is a loader refusal, SRS-CALIB-FUNC-002)`, `080/081 (temperature)`, `087/088 (ghost)`, `090/091 (binning)`, `041 (readout; only partly implemented, see #232)` | 아래 절별 |
| `:117` Offset 절 | `010, 021` | `010` | `SpecificPixelValues`·`FormulaAppliedElementWise` 가 `max(raw − offset, 0)` 만 단언. **치수 불일치(`021`)를 단언하는 시험이 이 파일에 없음**(이 파일이 단언하는 오류 코드는 `INVALID_CALIB_DATA`·`INVALID_INPUT`(온도)·`CONFIG_INVALID`(비닝) 세 가지뿐) |
| `:215` Gain 절 | `016 to 019` | `011` | `FormulaMatchesExactFloat` 가 게인 곱셈, `ZeroGainIsRefusedAtLoad` 는 로더가 0 을 거부(`:294` 가 이미 `SRS-CALIB-001 FUNC-002` 를 인용) |
| `:314` Ghost 절 | `085 to 087` | `087, 088` | `Frame0PassesThroughExactly`·`Frame1MatchesDualExponentialFormula`·`GhostSubtractedAfterFirstFrame` = 실행(`087`), `AfterResetHistoryIsZero` = 리셋(`088`). 핸들 유효성 가드(`086`)를 단언하는 시험은 없음 |
| `:348` | `REQ-P1A-032: Frame 0 …` | `REQ-P1A-087: …` | 현재 `032` 는 *No Uninitialized Output* |
| `:363` | `REQ-P1A-033: Frame 1 … dual-exponential` | `REQ-P1A-087: …` | 현재 `033` 은 *No NaN/Inf in Output* |
| `:438` Temp 절 | `080 to 082` | `080, 081` | `082`(플래그)는 파이프라인이 설정하고 이 함수·시험은 메타데이터를 쓰지 않음(`QA-A-181` §2) |
| `:462` | `REQ-P1A-005: At T_ref=25°C …` | `REQ-P1A-080: …` | 현재 `005` 는 *Input Validation*, 기준 온도에서 배율 1 은 `080` 의 식 |
| `:520` | `REQ-P1A-020: mode=1 is identity` | `REQ-P1A-091: …` | `091` 본문 *"While `binningMode == 1` … return `XPE_OK` without modifying"* |
| `:530` | `REQ-P1A-021: mode=2 → raw / 4` | `REQ-P1A-090: …` | `090` 본문 *"normalize each pixel by `1 / binningMode²`"* |
| `:540` | `REQ-P1A-022: mode=4 → raw / 16` | `REQ-P1A-090: …` | 같음 |
| `:562` Readout 절 | `001 to 004` | `041` | 깨끗한 영상·포화 영상·포화 행 하나·빈 열 — 구현이 실제로 하는 두 검사(`QA-A-182` §1)와 일치, 선 잡음 시험은 없음 |

이미 맞던 줄(`:160` `010`, `:250` `011`, `:386` `088`, `:472` `080`, `:495` `081`, `:505` `090, 091`, `:550` `091`, `:587` `041`)은 그대로 둡니다.

### 주석만 바뀌었다는 증거

| 항목 | 방법 | 결과 |
|---|---|---|
| 소스 | `HEAD` 와 작업본을 **주석 제거 후 비교** (`evidence/02_comment_strip_equivalence.txt`, 스크립트 `02b_comment_only_check.py`) | 세 파일 모두 **IDENTICAL** |
| 대조군 | 같은 도구에 코드 토큰을 바꾼 사본 (`GoldenOffsetTest`, `xpe_verify_pipeline` ×2 — 각각 해당 파일 **코드**에 나오는 이름) | 모두 **바뀜을 감지함** (`control token occurs in code: True`) |
| ABI | `dumpbin /exports build\ci-preprocess\bin\xpe_preprocess.dll` 변경 전·후 (`evidence/01`, `04`, `05`) | 내보내기 **48개 = 48개**, 서수·이름 동일, RVA·`@ILT` 열까지 **줄 전체 동일** (`DIFF_EXIT=0`) |
| 빌드 | `cmake --build build\ci-preprocess --config RelWithDebInfo --target xpe_preprocess_tests` (`evidence/03`) | `BUILD_EXIT=0`, `error C`·`warning C` 0건, 공개 헤더가 바뀌어 **104개 파일 재컴파일** |
| 시험 (전체) | `xpe_preprocess_tests.exe` 필터 없이 (`evidence/06`) | `721 tests from 94 test suites ran`, **`713 PASSED`**, 8 `SKIPPED`, **실패 0** — `QA-A-181`·`182` 실행과 같은 건수 |
| 고친 시험 파일 | `--gtest_filter='Golden*'` | `20 tests from 6 test suites … PASSED` |
| 캐시–프리셋 대조 (규약) | `check_cache_matches_preset.py build/ci-preprocess ci-preprocess` (`evidence/07`) | `OK`, `PRESET_EXIT=0` |
| 세 수 (규약) | `ctest -N` (`evidence/08`, `09`) | **실행 790 / 총계 826 / DISABLED 36** — 변경 전과 같음 |

---

## ② `REQ-P1A-066` — 어디서 왔고, 시험은 무엇을 검증하는가 (보고만)

### 번호의 출처

| 단계 | 내용 | 근거 |
|---|---|---|
| ① 정의 | `REQ-P1A-066`: *"Ghost corrector handles SHALL NOT be shared across threads. WHILE one thread is using a handle for `xpe_ghost_correct`, another thread SHALL NOT call `xpe_ghost_correct`, `xpe_ghost_reset`, or `xpe_ghost_destroy` on the same handle."* — 즉 **핸들 하나를 여러 스레드가 공유하지 말라**는 사용 제약 | `git show ee2c6077:.moai/specs/SPEC-XPE-P1A/spec.md` `:222` (`2.14 Cross-Cutting`, 이웃 `065` 는 "reentrant with independent caller-supplied buffers") |
| ② 부재 | `ee2c6077`(2026-04-16, `feat(preprocess): … 전체 구현 완료`, `spec.md` 435줄, **`REQ-P1A-` 정의 71개**)에는 있고, 같은 날의 `bc22093c`(`구현: SPEC-XPE-P1A Pre-processing Module (Phase 1-5)`, 361줄, **정의 25개**)에는 **없음** — `spec.md:645` 가 적은 "71 → 25" 와 같은 두 수입니다. `bc22093c` 의 부모에는 `spec.md` 자체가 없습니다(그 갈래에서는 이 커밋이 파일의 첫 등장). 두 커밋은 **서로의 조상이 아니며**(둘 다 `HEAD` 의 조상, 서로 다른 갈래) 이 파일의 이력은 `bc22093c` 쪽에서 시작합니다 | `git merge-base --is-ancestor` 양방향 `no`, `spec.md` 의 `066` 개수 `ee2c6077` 1 / `bc22093c^` 0 / `bc22093c` 0. `git log -S"REQ-P1A-066" -- spec.md` 는 이 경로 한정으로는 빈 결과였고(대조군: 같은 방식의 `REQ-P1A-010` 검색은 5개 커밋을 찾음), 경로 한정 없는 `-S` 가 `ee2c6077` 을 `spec.md` 와 함께 보여 주었습니다 |
| ③ 새 인용 | 시험 `test_req_p1a_066.cpp` 와 `CMakeLists.txt` 줄은 **2026-05-09** `d3e78bfb`(`fix(pre): P1A 심각 결함 D1/D4 수정 + REQ-P1A-066 에러 경로 테스트 (#68 #70 #73)`)에서 생겼고, **그 커밋 시점의 `spec.md` 에는 이미 `066` 이 0건**이었음 | `git show d3e78bfb:…/spec.md \| grep -c` = 0 |
| ④ 번호의 의미가 넓어진 경위 | `docs/audit/SESSION-10-BRIEF-PRE.md:101-110`: *"#73 — P1A 심각 결함 4건 + **REQ-P1A-066 (신규 등록)**"*, 수용 기준 *"REQ-P1A-066: **malloc 실패 / fopen 권한 거부 / CRC 불일치 / Ghost race 4종 단위 테스트**"*. 이 네 가지는 `SPEC-XPE-P1A-QUALITY-REPORT` 의 *"7. 오류 경로 테스트 커버리지 부족"*(`quality-report.md:141-147`)의 네 항목이고, 그 중 **"Ghost 핸들 동시 접근 (REQ-P1A-066)"** 한 곳에만 옛 번호가 붙어 있었음 | 두 문서의 해당 줄 |
| ⑤ 현재 | 현재 `spec.md` 에는 정의 없음. 인용처: `CMakeLists.txt:433`, `tests/test_req_p1a_066.cpp`(5곳), VVP `:94`, `quality-report.md:147`, `docs/audit/SESSION-10-*.md`(역사 기록). `tools/docs/check_req_citations.py:32-36` 이 같은 사실을 이미 기록함(*"deleted from spec.md by bc22093 (#211) … whole test file still implements it"*) | `grep` |

**요약**: 옛 `066` 은 *"Ghost 핸들을 스레드 사이에 공유하지 말 것"* 이라는 한 가지 사용 제약이었는데, 그 요구가 `spec.md` 에서 사라진 뒤(`bc22093c` 쪽 갈래) **이슈 `#73` 의 수용 기준이 같은 번호를 "오류 경로 시험 네 가지"의 이름표로 다시 썼고**, 시험 파일이 그 이름을 물려받았습니다. 시험이 생긴 시점부터 `spec.md` 에 정의가 없었습니다.

### 시험이 실제로 검증하는 것

실행: `xpe_preprocess_tests.exe --gtest_filter='P1A066*'` → `4 tests from 2 test suites … PASSED` (`evidence/10`). `CMakeLists.txt:433-434` 에 등록(`DISABLED_` 아님), VVP `:94` 의 "4건 / 등록 Yes" 와 일치합니다.

| 시험 | 단언 | 실제 대응 요구 / 문서 | 판정 |
|---|---|---|---|
| `T1_GainCorrect_HugeDimension_RejectsBeforeAllocation` | `width = height = UINT32_MAX` 인 입력에 `xpe_gain_correct` 가 `XPE_ERR_INVALID_INPUT` (`gain_correct.cpp` 의 `n > SIZE_MAX / sizeof(float)` 가드) | 가장 가까운 것은 `REQ-P1A-005`(*Input Validation*: 널·0 차원 검증, 위반 시 `INVALID_INPUT`). **곱셈 오버플로·할당 전 거부는 본문에 없음** | 대응 요구 없음(부분적으로 `005`) |
| `T2_LoadOffset_DirectoryPath_ReturnsIoFailed` | 디렉터리 경로로 `xpe_calib_load_offset` → `XPE_ERR_IO_FAILED` | `REQ-P1A-014`(오프셋 읽기) 본문은 *"valid XCal file path"* 일 때의 동작만 말함. **잘못된 경로의 오류 코드는 본문에 없음** | 대응 요구 없음(주제는 `014`) |
| `T3_LoadOffset_WrongSha256_ReturnsConfigInvalid` | SHA-256 필드가 0 으로 채워진 XCal 파일 → `XPE_ERR_CONFIG_INVALID` | `REQ-P1A-014` 본문 *"validate SHA-256 integrity"*. **불일치 시 반환 코드는 본문에 없음** | 주제는 `014`, 코드는 본문이 말하지 않음 |
| `T4_GhostCorrect_MultiHandle_ConcurrentNoError` | 스레드 4개가 **각자 자기 핸들**로 50회 `xpe_ghost_correct`, 오류 0 | `REQ-P1A-003`(*Thread Safety*: 독립 버퍼면 재진입 가능), `api-spec.md:586` *"Reentrant per handle (do not share a single handle across threads)"*, `SRS-CALIB-NFR-003` | 아래 |

**T4 는 옛 `066` 을 시험하지 않습니다.** 옛 `066` 은 "공유하지 말 것"이라는 **금지**이고, T4 는 "각자 따로 쓰면 안전하다"는 **반대 방향**(독립 핸들의 동시 사용)을 확인합니다. 핸들을 공유했을 때의 동작을 시험하는 것은 이 파일에 없습니다. T4 가 확인하는 것은 `REQ-P1A-003`(독립 버퍼의 재진입)과 `api-spec.md:586` 의 "핸들마다 재진입 가능" 쪽입니다.

**부수 발견 (SRS ↔ 구현, 이 카드는 판정하지 않음)**: `SRS-CALIB-NFR-003`(`docs/calibration/SRS-CALIB-001_…:415`)은 *"Ghost correction functions (stateful) shall be **protected by mutex per handle**"* 라고 쓰는데, `ghost_correct.cpp` 와 `xpe_preprocess_internal.h` 를 `mutex` 로 검색하면 **핸들별 뮤텍스는 없고** 전역 `g_calib_mutex`(`xpe_preprocess_internal.h:330`) 하나뿐입니다(대조군: 같은 검색이 `gain_correct.cpp` 에서는 `mutex` 2건을 찾아냄). 즉 SRS 의 "핸들별 뮤텍스"와 `api-spec.md` 의 "핸들을 공유하지 말 것"(사용자가 책임짐)이 서로 다른 계약이고 구현은 후자 쪽입니다. 검색 범위는 위 두 파일입니다.

### 정정 방법에 대한 재료 (결정은 리더)

- 시험 파일명·시험 이름·`CMakeLists.txt` 주석은 바꾸지 않았고, VVP `:94` 의 인용도 그대로입니다.
- 선택지의 재료만 적습니다: (a) 번호 `066` 을 "정의 없음"으로 두고 시험 머리말에 사실(위 요약)을 적는다, (b) 네 시험을 각자의 주제(`005`·`014`·`014`·`003`)로 나눠 인용한다 — 단 `T1`~`T3` 은 **본문이 말하지 않는 동작**이라 번호를 붙이면 요구가 없는 동작을 요구가 있는 것처럼 보이게 한다, (c) 오류 경로 요구를 SPEC 에 새로 쓴다(`#211`(2026-09-28)이 `REQ-P1A-080`~`101` 을 `spec.md` 의 *신설* 절로 구현 서술로 적은 방식, `spec.md:645` 의 "왜 신설인가" 참고). `T1`~`T3` 이 말하는 동작은 이미 구현에 있으므로 (c)는 구현을 서술하는 새 요구가 됩니다.

---

## 하지 않은 것

- 코드·단언·시험 이름 변경 없음, **`test_req_p1a_066.cpp` 파일명·시험 이름·`CMakeLists.txt:433` 주석 수정 없음**, push 없음, 새 이슈 없음
- `docs/`·SPEC·VVP·생성된 Doxygen 수정 없음. `test_golden_reference.cpp` 밖의 인용은 건드리지 않음

## 미검증

- `xpe_verify_pipeline` 이 계산하는 지표(SNR 개선)가 `SRS-CALIB-FUNC-015`·`021` 의 어느 문장에 근거하는지는 확인하지 못했습니다(§① 참고).
- `ee2c6077` 과 `bc22093c` 가 서로 다른 갈래인 이유(어느 갈래가 이 날의 최종 결정이었는지)는 이력에서 확인하지 않았습니다. 이 파일의 `HEAD` 쪽 이력이 `bc22093c` 에서 시작한다는 것만 관측했습니다.
- `T1` 의 가드가 `gain_correct.cpp` 의 어느 줄인지(현재 줄 번호)는 이 카드에서 다시 열지 않았습니다. 시험 머리말과 단언으로 동작을 확인했습니다.
- `test_calibration_roundtrip.cpp:302` 의 `// REQ-P1A-040`(SIMD Optimization)이 그 시험의 내용과 맞는지는 점검 대상이 아니어서 보지 않았습니다.

🗿 MoAI
