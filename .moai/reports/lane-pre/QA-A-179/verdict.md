# QA-A-179 (`#230`) — `binning_correct.cpp` 주석의 옛 REQ 번호 정정 (+ `configJsonOrNull` 문구)

주석만 바꿨습니다. 범위는 `src/binning_correct.cpp` 와, 리더 지시에 따른 헤더 `@param` 한 줄입니다. 비주석 변경 0줄·내보내기 동일을 증거로 달았습니다 (§3).

**조건**: 시작 전 로컬 `main`(`b38b3e03`, 리더가 `QA-A-178` 을 병합한 것)을 `dev/preprocess` 에 fast-forward 병합(이쪽 0 커밋 앞섬·1 커밋 뒤처짐). `origin/main` 에는 새 커밋이 없었습니다(리더가 CI 뒤 푸시 예정). 병합이 `modules/preprocess`·`modules/common`·`cmake` 를 바꾸지 않았음(`git diff --stat` 빈 출력). 빌드 `build/ci-preprocess` RelWithDebInfo.

---

## 1. 바꾼 곳 — 2파일

SPEC 번호는 `QA-A-178` 에서 `spec.md:708-720`·`:614-640`·`:735` 으로 대조한 것을 그대로 씁니다 (`090` 정규화 · `091` 모드 가드 · `095` 단계 순서; 현재 `020`/`021`/`022` 는 초기화·치수·형식 불일치 가드).

| # | 위치 | 이전 | 이후 |
|---|---|---|---|
| 1 | `src/binning_correct.cpp:15` | `// @MX:SPEC: REQ-P1A-020` | `// @MX:SPEC: REQ-P1A-090, REQ-P1A-091` |
| 2 | `:24` | `// REQ-P1A-020: no-op for binningMode == 1` | `// REQ-P1A-091: no-op for binningMode == 1` |
| 3 | `:27-29` | `// REQ-P1A-021: XPE_ERR_CONFIG_INVALID for unknown binning mode`<br>`// REQ-P1A-022: float32 format (post-gain-correct stage)`<br>`// REQ-P1A-090: per-mode correction profile` | `// REQ-P1A-091: XPE_ERR_CONFIG_INVALID for a mode other than 1, 2 or 4`<br>`// (the FLOAT32-only check is above: REQ-P1A-090; the stage runs after`<br>`// gain correction: REQ-P1A-095)` |
| 4 | `:33-34` | `// REQ-P1A-022/023: normalize by binningMode^2 to compensate for summed charge`<br>`// float32 format (post-gain-correct stage)` | `// REQ-P1A-090: normalize by 1/binningMode^2 to compensate for summed charge`<br>`// (FLOAT32 only; the non-finite check below is REQ-P1A-091)` |
| 5 | `include/xpe/preprocess_api.h` `@param configJsonOrNull` (`xpe_binning_correct`) | `Unused: accepted for ABI compatibility, no correction profile is read from it` (2줄) | `Unused: no correction profile is read from it` (1줄) |

근거 줄:

- **#1**: 이 함수는 `090`(정규화)과 `091`(모드 가드)을 구현하므로 태그가 그 둘을 가리키게 했습니다. `moai mx` 도구가 이 값을 읽는지는 **확인하지 않았습니다** (리더가 MX 태그 수정은 자율 범위라고 확인).
- **#2·#3**: `binningMode == 1` 의 no-op 과 `1`·`2`·`4` 외 `XPE_ERR_CONFIG_INVALID` 는 둘 다 `REQ-P1A-091`(`spec.md:719`). FLOAT32 검사는 같은 파일 `:22` 에 있고 FLOAT32 전용은 `090`, "게인 뒤 단계" 는 `095`(`spec.md:735` 순서 문장).
- **#4**: `REQ-P1A-023` 은 `spec.md` 에 **정의가 없습니다**(`#### REQ-P1A-` 제목 목록과 `023` 전체 검색 모두 0건). 정규화는 `090`, 같은 루프의 `isfinite` 검사(`:39`)는 `091` 의 세 번째 문장(`XPE_ERR_PROCESSING_FAILED`)입니다.
- **#5**: `QA-A-178` 에서 "ABI 호환을 위해 받는다" 가 **추정**이라고 스스로 적었고, 확인한 사실은 "읽지 않는다"(`binning_correct.cpp` 의 `(void)configJsonOrNull;`)뿐입니다. 리더 지시대로 확인된 사실만 남겼습니다.

`:5` 의 `REQ-P1A-090, REQ-P1A-091`, `:14` 의 `@MX:NOTE`(모드 `1`/`2`/`4`, `1/mode²`)는 맞아서 그대로 둡니다.

---

## 2. 이 파일 밖의 같은 유형 — 고치지 않고 목록만

| 위치 | 인용 | 판정 |
|---|---|---|
| `modules/preprocess/CMakeLists.txt:24` | `src/binning_correct.cpp      # SWU-1.8 REQ-P1A-020..023` | **같은 오기**: `020`~`022` 는 가드이고 `023` 은 SPEC 에 정의 없음. 현재 번호는 `090`/`091` |

**검색 범위와 한계**: 줄에 `binn` 과 `REQ-P1A-020..023` 이 함께 있는 곳을 저장소 전체에서 찾았습니다(`build`·`_deps`·`.git`·`generated`(Doxygen)·`.moai/reports` 제외). 대조군으로 같은 방식의 `binning` + `REQ-P1A-091` 검색은 16줄을 찾아냅니다.

**검색을 한 번 잘못 짰던 기록**: 첫 스캔은 `grep -v "binning_correct.cpp"` 로 이 파일을 걸러냈는데, 위 `CMakeLists.txt:24` 줄 자체에 `binning_correct.cpp` 가 들어 있어 **같이 지워져 0건으로 나왔습니다.** 파일 이름이 아니라 경로로 걸러야 했습니다. 두 번째 스캔(필터 없이 경로만 제외)에서 잡혔습니다.

생성된 Doxygen(`docs/help/generated/…`)은 리더 지시대로 건드리지 않았고 스캔에서 제외했습니다.

---

## 3. 주석만 바뀌었다는 증거

| 항목 | 방법 | 결과 |
|---|---|---|
| 소스 | `HEAD` 와 작업본을 **주석 제거 후 비교** (`evidence/02_comment_strip_equivalence.txt`, 스크립트 `02b_comment_only_check.py`) | `binning_correct.cpp`, `preprocess_api.h` 둘 다 **IDENTICAL** |
| 대조군 | 같은 도구에 식별자 하나(`xpe_binning_correct` → `…X`)를 바꾼 사본 | **바뀜을 감지함** (`detected: True`) |
| ABI | `dumpbin /exports build\ci-preprocess\bin\xpe_preprocess.dll` 변경 전·후 (`evidence/01`, `04`, `05`) | 내보내기 **48개 = 48개**, 서수·이름 동일 (`DIFF_EXIT=0`), RVA·`@ILT` 열까지 **줄 전체 동일** |
| 빌드 | `cmake --build build\ci-preprocess --config RelWithDebInfo --target xpe_preprocess_tests` (`evidence/03`) | `BUILD_EXIT=0`, `error C`·`warning C` 0건 |
| 영향 시험 | `--gtest_filter='*Binning*:BinningCorrect*:PreprocessDegraded.*:*Pipeline*'` (`evidence/06`) | `63 tests from 14 test suites … PASSED`, `TESTS_EXIT=0` |
| 캐시–프리셋 대조 (규약) | `check_cache_matches_preset.py build/ci-preprocess ci-preprocess` (`evidence/07`) | `OK`, `PRESET_EXIT=0` |
| 세 수 (규약) | `ctest -N` (`evidence/08`, `09`) | **실행 790 / 총계 826 / DISABLED 36** — 변경 전과 같음 |

**한계**: 주석 제거 비교 스크립트는 제가 쓴 단순한 상태 기계입니다(대조군으로 식별자 변경 감지를 보였을 뿐, 모든 C++ 구문을 다룬다는 보증은 아님). 변경 전 DLL 사본이 없어 이진 해시 비교는 못 했고, 내보내기 목록의 줄 전체 동일로 대신했습니다.

---

## 4. 하지 않은 것

- 코드·단언·시험 이름 변경 없음, push 없음, 새 이슈 없음
- `CMakeLists.txt:24`(§2)·생성된 Doxygen·`docs/`·SPEC·VVP 수정 없음

## 5. 미검증

- `@MX:SPEC` 값을 읽는 도구가 있는지, 있다면 `090, 091` 두 값 형식(쉼표 구분)을 받아들이는지는 확인하지 않았습니다. 같은 파일 `:5` 가 이미 `REQ-P1A-090, REQ-P1A-091` 형식을 쓰고 있어 그 모양을 따랐습니다.
- `REQ-P1A-095` 가 가리키는 "게인 뒤 단계" 는 `spec.md` 의 순서 문장을 읽은 것이고 `pipeline.cpp` 를 이 카드에서 다시 열지 않았습니다 (`QA-A-178` 과 같은 한계).

🗿 MoAI
