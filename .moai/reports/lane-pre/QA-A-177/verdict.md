# QA-A-177 (`#230`) — QA-A-176 §3 의 주석 오기 정정: 제품 헤더·소스·CMake·시험

주석만 바꿨습니다. 비주석 변경 0줄을 **주석 제거 후 비교**와 **내보내기 목록 비교**로 보였습니다 (§3). 공개 헤더가 들어 있어 ABI 증거를 따로 달았습니다.

**조건**: 시작 전 `origin/main` 을 `dev/preprocess` 에 fast-forward 병합(`a7536384` → `84aea8f1`, 이쪽이 0 커밋 앞서고 10 커밋 뒤처져 있었음). 병합이 `modules/preprocess`·`modules/common`·`cmake` 를 바꾸지 않았음(`git diff --stat` 빈 출력). 트리 `84aea8f1` 위에서 작업, 빌드 `build/ci-preprocess` RelWithDebInfo.

---

## 1. 바꾼 곳 — 4파일 7곳

| # | 위치 | 이전 | 이후 | 근거 |
|---|---|---|---|---|
| 1 | `include/xpe/preprocess_api.h` `xpe_nonlinearity_correct` 문서 블록 (이전 `:646-650`) | `REQ-P1A-012/013/014/015` 네 줄 | `SRS-CALIB-FUNC-006 / -006-EXT` 출처 + 동작 서술 (§2) | §2 |
| 2 | 같은 블록 `@return` | `XPE_ERR_CONFIG_INVALID if unknown detector mode` | `XPE_ERR_CALIB_NOT_LOADED if the panel is declared non-linear and no LUT is loaded` | §2 — **목록 밖**, 같은 블록의 거짓 서술 |
| 3 | `src/nonlinearity_correct.cpp:261` | "`REQ-P1A-013` in the CURRENT set is defect correction (the Hampel …" | "… is **runtime defect detection** (the Hampel …" | `spec.md:264` `REQ-P1A-013: Runtime Defect Detection` |
| 4 | `CMakeLists.txt:25` | `src/defect_correct.cpp # SWU-1.3 REQ-P1A-012, REQ-P1A-013 (new 3-arg g_calib API)` | `# SWU-1.3 REQ-P1A-012 (new 3-arg g_calib API); runtime detection REQ-P1A-013 is in runtime_detection.cpp` | 아래 |
| 5 | `tests/test_temp_nonlinearity_binning.cpp:157` | `// REQ-P1A-020: binningMode == 1 is no-op …` | `// REQ-P1A-091: …` | §4 |
| 6 | 같은 파일 `:165` | `// REQ-P1A-021: unknown binning mode -> XPE_ERR_CONFIG_INVALID` | `// REQ-P1A-091: unknown binning mode (not 1, 2 or 4) -> XPE_ERR_CONFIG_INVALID` | §4 |

**#4 의 근거**: `src/defect_correct.cpp` 에서 `xpe_defect_detect_runtime(` 정의는 **0건**이고, 정의는 `src/runtime_detection.cpp:67` 에 있습니다. 같은 파일 `:276` 의 주석("Runtime detection implementation moved to `runtime_detection.cpp` (REQ-P1A-013)")도 같은 말을 합니다. 그래서 `defect_correct.cpp` 의 줄에서 013 을 빼고 위치를 알리는 문구로 바꿨습니다.

---

## 2. 비선형 헤더 블록 — 번호만 바꾸지 않은 이유

**옛 번호의 의미를 `git show ee2c607:.moai/specs/SPEC-XPE-P1A/spec.md` 로 확인**했습니다 (`:86-92`):

| 번호 | 옛 SPEC 원문 | 옛 헤더 블록이 붙인 라벨 |
|---|---|---|
| 012 | 비선형 보정 적용 | "Apply nonlinearity correction" ✓ |
| 013 | 선형 응답 패널은 우회, `XPE_OK`, 이미지 불변 | "No-op when no config supplied" (비슷) |
| **014** | **`XPE_FLAG_NONLINEARITY_CORRECTED` 플래그 설정** | **"Unknown mode -> XPE_ERR_CONFIG_INVALID"** ✗ |
| **015** | **알 수 없는 detector mode → `XPE_ERR_CONFIG_INVALID`** | **"Identity polynomial for baseline"** ✗ |

즉 헤더의 네 줄은 **옛 번호체계 기준으로도 라벨이 어긋나 있었습니다**(014 와 015 의 내용이 뒤섞임). 번호만 `090/091` 식으로 갈아 끼울 수 없는 이유입니다. 그리고 현재 SPEC 에는 맞는 번호가 없습니다: `spec.md:55` 는 `PRE-08: Nonlinearity Correction -- 별도 SPEC`, `:956` 은 `SPEC-XPE-P1D` 가 **없음**(리더가 대조군을 적은 표). 요구는 `SRS-CALIB-001` 의 `SRS-CALIB-FUNC-006` / `-006-EXT` 에 있고, 이는 `QA-A-176` 에서 SRS 원문(`:55`, `:57-118`, `:306`)으로 확인했습니다.

새 블록이 말하는 것과 근거:

| 새 문장 | 근거 |
|---|---|
| 출처는 `SRS-CALIB-FUNC-006 / -006-EXT`, REQ-P1A- 번호 아님 | 위 + `nonlinearity_correct.cpp:3-4` 머리말 |
| LUT(6a) 또는 다항식(6b)을 적용 | SRS `:57`, `:88` |
| LUT·계수가 없으면 알림과 함께 no-op, `XPE_OK` (QA-A-127, QA-A-140, #196) | `nonlinearity_correct.cpp` 의 `panel_linear` 분기 뒤 no-op 보고 부분(`:244` 이후), `test_nonlin_noop_report.cpp` |
| **이 문장을 말하는 요구는 없고**, 가장 가까운 글은 `FUNC-006` 과 `SAFE-001` | `QA-A-176` 과 같은 한계를 주석에 그대로 적음 |
| 알 수 없는 `"mode"` 는 오류가 아님 (QA-A-127, #196) | `src/nonlinearity_correct.cpp:276-` 서술, `UnknownModeIsNoLongerAnError` 시험 |
| 옛 번호 `012..015` 는 `bc22093` 이전 것이고 지금은 다른 요구를 가리킴 | `git show ee2c607` 와 현재 `spec.md` 대조 |
| "identity polynomial for baseline" 줄은 SRS 6b 에도 구현에도 대응이 없음 | SRS `:88-118` 에 해당 문구 없음, `nonlinearity_correct.cpp` 에 `identity` 검색 **0건** |

마지막 줄은 부재 주장이므로 범위를 적습니다: SRS `6b` 절과 구현 파일 한 개만 봤습니다. 대조군 없이 "대응이 없음" 을 단정하지 않으려고 문구를 **"찾지 못함"의 범위**로 한정했습니다.

### `@return` 줄(#2)은 왜 고쳤나

카드 목록에는 없지만 같은 블록의 **거짓 서술**입니다. `src/nonlinearity_correct.cpp` 에서 `XPE_ERR_CONFIG_INVALID` 를 검색하면 **0건**이고, 실제로 돌려주는 코드는 `XPE_ERR_INVALID_INPUT`(널 이미지·비 UINT16, `:144-145`), `XPE_ERR_CALIB_NOT_LOADED`(`panel.linear` 가 `"false"` 인데 LUT 가 없음, `:241`), 그 밖에 `XPE_OK` 입니다. 새 번호는 고치고 이 줄을 거짓인 채로 두면 한 주석 안에서 서로 모순되므로 이 한 줄만 바꿨습니다. 헤더 `@return` 의 나머지(`XPE_ERR_INVALID_INPUT if NULL img`)는 그대로 둡니다 (비 UINT16 도 같은 코드를 돌려주지만 거기까지 넓히지 않았습니다). **리더가 원치 않으면 이 줄은 되돌려도 됩니다.**

---

## 3. 주석만 바뀌었다는 증거

| 항목 | 방법 | 결과 |
|---|---|---|
| 소스 | 변경 4파일을 `HEAD` 와 작업본으로 **주석 제거 후 비교** (`evidence/02_comment_strip_equivalence.txt`, 스크립트 `02b_comment_only_check.py`) | 4파일 모두 **IDENTICAL** |
| 대조군 | 같은 도구에 코드 토큰 하나를 일부러 바꾼 사본(`if (!img)` → `if (img)`) | **바뀜을 감지함** (`control … detected: True`) |
| ABI | `dumpbin /exports build\ci-preprocess\bin\xpe_preprocess.dll` 변경 전·후, 서수+이름 비교 (`evidence/01`, `04`, `05`) | 내보내기 **48개 = 48개, 서수·이름 동일** (`DIFF_EXIT=0`) |
| 빌드 | `cmake --build build\ci-preprocess --config RelWithDebInfo --target xpe_preprocess_tests` (`evidence/03`) | `BUILD_EXIT=0`, 오류·경고 `C` 0건 (107단계, 공개 헤더 변경으로 전체 재컴파일) |
| 영향 시험 | `--gtest_filter='PreprocessDegraded.*:*Nonlin*:BinningCorrect*:*Binning*:RuntimeDetection*'` (`evidence/06`) | `112 tests from 17 test suites … PASSED`, `TESTS_EXIT=0` |
| 캐시–프리셋 대조 (규약) | `check_cache_matches_preset.py build/ci-preprocess ci-preprocess` (`evidence/07`) | `OK`, `PRESET_EXIT=0` |
| 세 수 (규약) | `ctest -N` (`evidence/08`, `09`) | **실행 790 / 총계 826 / DISABLED 36** — 변경 전과 같음 |

**한계 둘**:

- 내보내기 목록의 **RVA·`@ILT` 썽크 주소 열은 변경 전후에 다릅니다**(`evidence/05b` 에 원본 diff). 예: `xpe_binning_correct` `00002A13` → `00002A0E`. 서수·이름은 같고 소스는 주석 제거 후 같으므로 증분 링크(incremental link)의 썽크 배치가 링크 이력에 따라 달라진 것으로 **추정**하지만, **원인을 확인하지 않았습니다.**
- 주석 제거 비교 스크립트는 제가 쓴 것이고 문자열·문자 리터럴을 상태 기계로 건너뛰는 단순한 구현입니다. 대조군이 코드 변경은 잡는다는 것을 보였을 뿐 모든 C++ 구문(원시 문자열 등)을 다룬다고 보증하지 않습니다.

---

## 4. 비닝 `REQ-P1A-020/021` → 정확히 `091` 입니다 (`090/091` 이 아님)

SPEC 원문 (`spec.md:708-720`):

- `REQ-P1A-090: Binning Correction Execution` — `binningMode` 가 `2` 또는 `4` 일 때 화소를 `1/mode²` 로 정규화.
- `REQ-P1A-091: Binning Mode Guard` — *"While `binningMode == 1`, … return `XPE_OK` without modifying the image. If `binningMode` is not `1`, `2`, or `4`, … `XPE_ERR_CONFIG_INVALID`."*

시험 두 곳이 단언하는 것은 둘 다 **091 의 문장**입니다:

| 줄 | 시험 | 단언 | 번호 |
|---|---|---|---|
| `:157` | `Binning1x1IsNoOp` | `xpe_binning_correct(&buf, 1, nullptr)` → `XPE_OK`, 화소 불변 | **091** (`binningMode == 1`) |
| `:165` | `UnknownBinningModeReturnsError` | `xpe_binning_correct(&buf, 3, nullptr)` → `XPE_ERR_CONFIG_INVALID` | **091** (1·2·4 가 아닌 값) |

`090` 은 정규화(`2`·`4` 의 계수) 요구라 이 두 시험의 근거가 아닙니다. 카드의 "→ 090/091" 중 이 두 줄에 해당하는 것은 `091` 입니다. 현재 SPEC 의 `020`·`021` 은 *Not-Initialized Guard*·*Dimension Mismatch Guard*(`spec.md:614`, `:629`)라 옛 인용이 틀렸음은 `QA-A-176` 의 지적대로입니다.

**`tests` 안의 비닝 관련 `020/021/022` 인용은 이 두 줄뿐**입니다 (`grep -n "REQ-P1A-02[0-2]" | grep -i binn`, 대조군으로 같은 검색이 이 두 줄을 찾아냄).

---

## 5. 고치지 않은 것 — 리더 결정 요청

1. **같은 헤더의 비닝 블록** (`preprocess_api.h:674-677`, 위 편집 뒤의 줄 번호): `REQ-P1A-020: No-op for binningMode == 1`, `REQ-P1A-021: XPE_ERR_CONFIG_INVALID for unknown binning mode` 는 시험과 **같은 오기**(→ `091`)입니다. `REQ-P1A-022: Float32 format (post-gain-correct stage)` 는 현재 `022` 가 *Format Mismatch Guard*(일반 가드)라 의미가 겹치는지 확인하지 못했고, `REQ-P1A-090: Per-mode correction profile` 은 현재 `090`(*Binning Correction Execution*)과 가까워 보입니다. 카드 목록에 없어서 건드리지 않았습니다.
2. **생성된 Doxygen 문서**: `docs/help/generated/doxygen/{html,xml}/preprocess__api_8h*` 가 옛 문구("No-op when no config supplied", "Unknown mode")를 그대로 싣고 있습니다 (3개 파일, 일치 3건). 생성물이라 재생성하지 않았고 헤더 주석과 어긋난 상태입니다.
3. `include/xpe/preprocess_api.h` 의 다른 블록들(`:175-228` 의 `REQ-P1A-020/021/022` 가드 인용 등)은 현재 `REQ-P1A-020/021/022` 의 의미(가드)와 맞아 보여 대조하지 않았습니다.

## 6. 하지 않은 것

- 코드·단언·시험 이름 변경 없음, push 없음, 새 이슈 없음
- `docs/`·SPEC·VVP 수정 없음, 생성된 Doxygen 재생성 없음

## 7. 미검증

- 내보내기 RVA·썽크 이동의 원인 (§3).
- 새 헤더 블록의 "no-op with an alert" 가 **LUT 도 계수도 없는 모든 경로**에 맞는지는 `panel.linear` 분기와 `test_nonlin_noop_report.cpp` 의 시험으로 읽었을 뿐 모든 구성 조합을 실행하지 않았습니다.
- `SRS-CALIB-FUNC-006` 이 "설정 없음 → no-op" 을 문장으로 말하지 않는다는 판단은 `QA-A-176` 과 같은 범위(SRS 해당 절)에서 읽은 결과입니다.

🗿 MoAI
