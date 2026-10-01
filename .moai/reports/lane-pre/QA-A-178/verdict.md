# QA-A-178 (`#230`) — `preprocess_api.h` 비닝 블록의 옛 번호 정정

주석만 바꿨습니다. 공개 헤더의 한 블록(`xpe_binning_correct` 문서)과 그 `@param` 한 줄입니다. 비주석 변경 0줄과 내보내기 동일을 증거로 달았습니다 (§3).

**조건**: 시작 전 `origin/main` 을 `dev/preprocess` 에 fast-forward 병합(`d7e3b658` → `52501cb1`, 이쪽 0 커밋 앞섬·6 커밋 뒤처짐). 병합이 `modules/preprocess`·`modules/common`·`cmake` 를 바꾸지 않았음(`git diff --stat` 빈 출력). 빌드 `build/ci-preprocess` RelWithDebInfo.

---

## 1. 한 줄씩 대조

SPEC 원문(`spec.md:708-720`, `:614-640`)과 구현(`src/binning_correct.cpp` 전체)을 읽었습니다.

| 옛 줄 | SPEC 의 그 번호 (현재) | 이 함수가 실제로 하는 일 | 판정 → 새 줄 |
|---|---|---|---|
| `REQ-P1A-020: No-op for binningMode == 1` | `020` = *Not-Initialized Guard* (`:614`) | `binningMode == 1` → `XPE_OK`, 이미지 불변 (`:24-25`). 초기화 검사 **없음** | **오기** → `REQ-P1A-091` |
| `REQ-P1A-021: XPE_ERR_CONFIG_INVALID for unknown binning mode` | `021` = *Dimension Mismatch Guard* (`:629`) | `1`·`2`·`4` 외 → `XPE_ERR_CONFIG_INVALID` (`:30-31`) | **오기** → `REQ-P1A-091` |
| `REQ-P1A-022: Float32 format (post-gain-correct stage)` | `022` = *Format Mismatch Guard* (`:636`): **입력과 맵 버퍼**의 화소 형식이 다르면 `XPE_ERR_UNSUPPORTED_FORMAT` | **맵 버퍼를 받지 않음**. FLOAT32 가 아니면 `XPE_ERR_INVALID_INPUT` (`:22`) | **오기** — 이 함수는 022 를 구현하지 않음. FLOAT32 전용은 `REQ-P1A-090`, "gain 뒤 단계" 는 `REQ-P1A-095`(`spec.md:735`, `readout → temp → offset → nonlinearity → gain → binning → …`) |
| `REQ-P1A-090: Per-mode correction profile` | `090` = *Binning Correction Execution*: `2`/`4` 이면 `1/mode²` 로 정규화 | `px[i] *= 1/(mode*mode)` (`:36-38`) | 번호는 **맞지만** "per-mode correction profile" 이라는 서술이 틀림 → 정규화로 바꿈 |

`REQ-P1A-091` 원문(`spec.md:719`): *"While `binningMode == 1`, … return `XPE_OK` without modifying the image. If `binningMode` is not `1`, `2`, or `4`, … `XPE_ERR_CONFIG_INVALID`. If any pixel is non-finite during normalization, … `XPE_ERR_PROCESSING_FAILED`."* — 구현의 세 갈래와 일치합니다(`:25`, `:30-31`, `:39`).

**맞는 번호가 없는 경우**: 옛 `022` 줄이 말한 "FLOAT32 전용" 은 `090` 이, "gain 뒤" 는 `095` 가 맡아 번호가 모자라지 않았습니다. 다만 **"FLOAT32 가 아니면 `XPE_ERR_INVALID_INPUT`"** 이라는 오류 코드는 SPEC 문장에 없고(`090` 은 "FLOAT32 buffer" 라고만, "측정된 계약" 도 "FLOAT32 전용" 이라고만 적음) 구현의 동작입니다. 그래서 새 문구에 *"implementation behaviour; the SPEC text says only 'FLOAT32'"* 라고 적었습니다.

### 새 블록 (헤더)

```
 * REQ-P1A-090: binningMode 2 or 4 -> normalise each pixel by 1/binningMode^2,
 *               in place, FLOAT32 buffers only (a non-FLOAT32 buffer returns
 *               XPE_ERR_INVALID_INPUT -- implementation behaviour; the SPEC
 *               text says only "FLOAT32")
 * REQ-P1A-091: binningMode == 1 -> XPE_OK, image untouched; a mode other than
 *               1, 2 or 4 -> XPE_ERR_CONFIG_INVALID; a non-finite pixel ->
 *               XPE_ERR_PROCESSING_FAILED
 * REQ-P1A-095: this stage runs after gain correction in the pipeline
 * The numbers REQ-P1A-020/021/022 this block used to cite are the
 * pre-bc22093 ones. They now name the not-initialized, dimension-mismatch
 * and format-mismatch guards, which this function does not implement (it
 * has no initialisation check and takes no map buffer).
```

### `@param configJsonOrNull` (목록 밖, 같은 블록의 거짓 서술)

이전: `Optional correction profile JSON`. 구현은 이 인자를 **전혀 읽지 않습니다** — `src/binning_correct.cpp:42` 가 `(void)configJsonOrNull;` 입니다. 옛 `090` 줄의 "Per-mode correction profile" 이 이 서술과 한 쌍이어서 함께 바꿨습니다.

이후: `Unused: accepted for ABI compatibility, no correction profile is read from it`.

"ABI 호환을 위해 받는다" 는 **추정**입니다 — 인자를 쓰지 않는 이유를 구현이나 SPEC 에서 확인하지 못했습니다. 사실로 확인한 것은 "읽지 않는다" 뿐이므로 이 문구가 과하다고 보면 `Unused: no correction profile is read from it` 로 줄이십시오.

---

## 2. 건드리지 않은 것 — 리더 결정 요청

**같은 오기가 구현 파일 자신의 주석에도 있습니다** (`src/binning_correct.cpp`, 제품 소스):

| 줄 | 인용 | 판정 |
|---|---|---|
| `:5` | `REQ-P1A-090, REQ-P1A-091` | **맞음** |
| `:15` | `// @MX:SPEC: REQ-P1A-020` | **오기** → `091` (이 태그는 `moai mx` 도구가 읽는 값일 수 있어 건드리지 않음) |
| `:24` | `// REQ-P1A-020: no-op for binningMode == 1` | **오기** → `091` |
| `:27` | `// REQ-P1A-021: XPE_ERR_CONFIG_INVALID for unknown binning mode` | **오기** → `091` |
| `:28` | `// REQ-P1A-022: float32 format (post-gain-correct stage)` | **오기** → `090`/`095` |
| `:29` | `// REQ-P1A-090: per-mode correction profile` | 서술 부정확 |
| `:33` | `// REQ-P1A-022/023: normalize by binningMode^2 …` | **오기** → `090` (현재 `023` 은 SPEC 에 정의가 없음; 확인: `spec.md` 의 `#### REQ-P1A-` 제목 목록) |

카드 범위가 헤더라서 고치지 않았습니다. 같은 방식으로 정정할 수 있습니다.

`docs/help/generated/doxygen/…` 은 리더 지시대로 **손대지 않았습니다**(생성물).

---

## 3. 주석만 바뀌었다는 증거

| 항목 | 방법 | 결과 |
|---|---|---|
| 소스 | `HEAD` 와 작업본을 **주석 제거 후 비교** (`evidence/02_comment_strip_equivalence.txt`, 스크립트 `02b_comment_only_check.py`) | `preprocess_api.h` **IDENTICAL** |
| 대조군 | 같은 도구에 식별자 하나(`xpe_binning_correct` → `…X`)를 바꾼 사본 | **바뀜을 감지함** (`detected: True`) |
| ABI | `dumpbin /exports build\ci-preprocess\bin\xpe_preprocess.dll` 변경 전·후 (`evidence/01`, `04`, `05`) | 내보내기 **48개 = 48개**, 서수·이름 동일 (`DIFF_EXIT=0`). 이번에는 **RVA·`@ILT` 열까지 줄 전체가 동일**합니다 |
| 빌드 | `cmake --build build\ci-preprocess --config RelWithDebInfo --target xpe_preprocess_tests` (`evidence/03`) | `BUILD_EXIT=0`, `error C`·`warning C` 0건 |
| 영향 시험 | `--gtest_filter='*Binning*:BinningCorrect*:PreprocessDegraded.*:*Pipeline*'` (`evidence/06`) | `63 tests from 14 test suites … PASSED`, `TESTS_EXIT=0` |
| 캐시–프리셋 대조 (규약) | `check_cache_matches_preset.py build/ci-preprocess ci-preprocess` (`evidence/07`) | `OK`, `PRESET_EXIT=0` |
| 세 수 (규약) | `ctest -N` (`evidence/08`, `09`) | **실행 790 / 총계 826 / DISABLED 36** — 변경 전과 같음 |

**관측 하나 (결론 아님)**: `QA-A-177` 에서는 주석만 바꿨는데도 내보내기의 RVA·썽크 열이 달랐고, 이번에는 줄 전체가 같습니다. `QA-A-177` 직전에 링크된 DLL 이 오래된 증분 링크 이력을 가졌다는 가설과 맞지만, **그 원인을 이번에도 확인하지 않았습니다.** 이 DLL 은 변경 전 사본이 남아 있지 않아 이진 해시 비교는 못 했습니다.

**한계**: 주석 제거 비교 스크립트는 제가 쓴 단순한 상태 기계입니다(대조군으로 식별자 변경 감지를 보였을 뿐, 모든 C++ 구문을 다룬다는 보증은 아님).

---

## 4. 하지 않은 것

- 코드·단언·시험 이름 변경 없음, push 없음, 새 이슈 없음
- 구현 파일(`binning_correct.cpp`)·생성된 Doxygen·`docs/`·SPEC·VVP 수정 없음

## 5. 미검증

- `configJsonOrNull` 을 쓰지 않는 **이유**(§1).
- 헤더의 `@return` 블록은 그대로 둡니다. `XPE_ERR_PROCESSING_FAILED`(비유한 화소) 갈래가 `@return` 에 없는데, 거짓이 아니라 **누락**이라 카드 범위 밖으로 보고 건드리지 않았습니다.
- `REQ-P1A-095` 인용은 `spec.md:735-` 의 순서 문장을 읽은 것이고, 실제 `pipeline.cpp` 에서 비닝이 게인 뒤에 도는지는 이 카드에서 다시 열어 보지 않았습니다(SPEC 의 "측정된 계약" 줄이 `pipeline.cpp` 를 인용합니다).

🗿 MoAI
