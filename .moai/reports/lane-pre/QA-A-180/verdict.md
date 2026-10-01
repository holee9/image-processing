# QA-A-180 (`#230`) — `CMakeLists.txt:24` 정정, 그리고 `@MX:SPEC` 값이 도구에 읽히는가

두 가지입니다. ①은 주석 한 줄 정정, ②는 판정입니다.

**판정 (②)**: **`@MX:SPEC: REQ-P1A-090, REQ-P1A-091` 는 어떤 형태로도 도구에 읽히지 않습니다.** 읽히는 것은 `//` 줄 주석 안의, 값이 `SPEC-` 로 시작하는 태그뿐이고, 쉼표로 이어도 **첫 ID 하나만** 읽힙니다. `REQ-…`·`SRS-…` 값은 쉼표 유무와 상관없이 **읽히지 않습니다.** 근거는 §2.

**조건**: 시작 전 로컬 `main`(`8ddea4d8`, 리더가 `QA-A-179` 를 병합한 것)을 `dev/preprocess` 에 fast-forward 병합(이쪽 0 커밋 앞섬·3 커밋 뒤처짐). 병합이 `modules/preprocess`·`modules/common`·`cmake` 를 바꾸지 않았음(`git diff --stat` 빈 출력). 빌드 `build/ci-preprocess` RelWithDebInfo.

---

## 1. ① `modules/preprocess/CMakeLists.txt:24`

| 이전 | 이후 |
|---|---|
| `src/binning_correct.cpp      # SWU-1.8 REQ-P1A-020..023` | `src/binning_correct.cpp      # SWU-1.8 REQ-P1A-090, REQ-P1A-091` |

근거는 `QA-A-178`·`QA-A-179` 와 같습니다 (`spec.md:708` `090` 정규화, `:715` `091` 모드 가드; 현재 `020`·`021`·`022` 는 가드, `023` 은 정의 없음).

### 주석만 바뀌었다는 증거

| 항목 | 방법 | 결과 |
|---|---|---|
| 소스 | `HEAD` 와 작업본을 **주석(`#` 이후) 제거 후 비교** (`evidence/02_comment_strip_equivalence.txt`, 스크립트 `02b_cmake_comment_check.py`) | **IDENTICAL** |
| 대조군 | 같은 도구에 코드 토큰(`src/binning_correct.cpp` → `…X.cpp`)을 바꾼 사본 | **감지함** (`detected: True`) |
| ABI | `dumpbin /exports build\ci-preprocess\bin\xpe_preprocess.dll` 변경 전·후 (`evidence/01`, `04`, `05`) | 내보내기 **48개 = 48개**, 서수·이름 동일, RVA·`@ILT` 열까지 **줄 전체 동일** (`DIFF_EXIT=0`) |
| 빌드 | `cmake --build build\ci-preprocess --config RelWithDebInfo --target xpe_preprocess_tests` (`evidence/03`) | CMake 재구성(`Re-running CMake … Configuring done`) 뒤 `BUILD_EXIT=0`, `error C`·`warning C` 0건. 컴파일된 파일은 없음(재구성만) |
| 영향 시험 | `--gtest_filter='*Binning*:BinningCorrect*:PreprocessDegraded.*:*Pipeline*'` (`evidence/06`) | `63 tests from 14 test suites … PASSED`, `TESTS_EXIT=0` |
| 캐시–프리셋 대조 (규약) | `check_cache_matches_preset.py build/ci-preprocess ci-preprocess` (`evidence/07`) | `OK`, `PRESET_EXIT=0` |
| 세 수 (규약) | `ctest -N` (`evidence/08`, `09`) | **실행 790 / 총계 826 / DISABLED 36** — 변경 전과 같음 |

### 같은 블록의 같은 유형 — 고치지 않고 목록만

`CMakeLists.txt` 의 `XPE_PREPROCESS_SOURCES` 블록에는 같은 오기가 더 있습니다. 현재 SPEC 의 `#### REQ-P1A-` 제목 목록(`spec.md`)과 대조했습니다:

| 줄 | 인용 | 판정 |
|---|---|---|
| `:19` offset_correct | `REQ-P1A-010, REQ-P1A-021` | 맞음 (`010` 오프셋 보정, `021` 치수 불일치 가드) |
| `:20` gain_correct | `REQ-P1A-016..019` | **오기**: 현재 `016`~`019` 는 교정 파일 읽기·오프셋 생성·만료 검사·저장. 게인은 `011`(실행)·`015`(읽기) |
| `:21` readout_validate | `REQ-P1A-001..004` | **오기**: `001`~`004` 는 모듈 초기화·ABI·스레드 안전·오류 코드. 읽기 검증은 `041` |
| `:22` temp_compensate | `REQ-P1A-005..008` | **오기**: `005` 는 입력 검증, `006`~`008` 은 정의 없음. 온도는 `080`~`082` |
| `:23` nonlinearity_correct | `REQ-P1A-012..015` | **오기**: SPEC 에 비선형 요구 없음, `SRS-CALIB-FUNC-006` (`QA-A-176`~`177` 참조) |
| `:25` defect_correct | `REQ-P1A-012 …; runtime detection REQ-P1A-013 is in runtime_detection.cpp` | 맞음 (`QA-A-179` 에서 정정한 줄) |
| `:26` ghost_correct | `REQ-P1A-085..087` | 맞음 (`085` 핸들 수명, `086` 유효성 가드, `087` 실행) |
| `:31` pipeline | `REQ-P1A-041..047` | **오기**: `041` 은 읽기 검증, `042` 는 파라미터 범위 질의, `043`~`047` 은 정의 없음. 파이프라인은 `095`~`099` |
| `:33` xpe_calib_check_expiry | `REQ-P1A-018` | 맞음 |

이 표는 `#230` 의 "요구 번호 오기 정정"을 닫기 전에 **남은 것의 전수 목록**입니다. 이 카드의 범위는 `:24` 한 줄이라 나머지는 고치지 않았습니다. 대조는 SPEC **제목 목록**과의 비교이고, 각 요구의 본문까지 읽지는 않았습니다 (`:20`·`:21`·`:22`·`:31` 의 "현재 번호가 무엇인가"는 제목으로 판단).

---

## 2. ② `@MX:SPEC` 값을 읽는 도구

### 찾은 것

- **저장소 안에는 이 태그를 파싱하는 코드가 없습니다.** `@MX`·`MX:SPEC` 를 담은 파일은 문서 84(`.md`)·C/C++/C# 소스·설정 `.yaml` 4(`.moai/config/sections/mx.yaml` 등)·생성물이고, **스크립트·코드 파일 중에는 `.moai/reports/lane-pre/QA-A-105/a105_docs.py` 한 개**뿐인데 그것은 소스에 태그를 *써 넣는* 일회성 스크립트라 파싱이 아닙니다 (`evidence/21`). 대조군: 같은 방식의 스캔이 `tools/ci/check_cache_matches_preset.py` 는 찾아냅니다.
- **파서는 설치된 `moai` 바이너리**입니다: `moai mx scan`(색인 생성)과 `moai mx query --spec <ID>`(조회). 버전 `moai-adk 3.1.2`, 경로 `C:\Users\drake\AppData\Local\Programs\moai\moai.exe`. 색인은 `.moai/state/mx-index.json`(`.gitignore:203` 로 무시되는 디렉터리)에 쓰입니다.

### 실제로 돌려 본 결과

1. **이 저장소**(`modules/preprocess/src`, `evidence/11`, `18`): 색인 29건, 레코드 키는 `body, createdBy, file, kind, lastSeenAt, line, reason` — **`specRef` 키가 한 건에도 없습니다.** 모듈 소스의 `@MX:SPEC` 줄 19개 중 `SPEC-` 로 시작하는 값은 2개(`rle_codec.cpp:6`, `rle_codec.hpp:14`)뿐이고 둘 다 `/** … */` 블록 주석 안입니다. `binning_correct.cpp:14` 의 `NOTE` 는 색인되었지만 `specRef` 가 없습니다.
2. **`--spec` 조회**: 실제 값(`SRS-CALIB-FUNC-006-EXT`, `REQ-P1A-013`, `REQ-P1A-090`, `REQ-P1A-091`, 쉼표 두 값) 전부 **0건**. 이것만으로는 "도구가 REQ 값을 못 읽는다"와 "질의가 아무것도 못 찾는다"를 가르지 못하므로 **양성 대조군**을 만들었습니다:
3. **프로브 파일**(스크래치 디렉터리의 `.cpp`, 태그 모양만 바꿔 같은 도구로 스캔, `evidence/13`~`16`, `19`~`20`):

| `// @MX:SPEC:` 값 | 도구가 잡은 `specRef` |
|---|---|
| `SPEC-AUTH-001` (문서의 예시 모양) | `SPEC-AUTH-001` |
| `SPEC-AUTH-001, SPEC-AUTH-002` | `SPEC-AUTH-001` (**첫 ID 만**) |
| `SPEC-XPE-P1A` | `SPEC-XPE-P1A` |
| `SPEC-XPE-P1A-REQ-P1A-013` | `SPEC-XPE-P1A-REQ-P1A-013` |
| `SPEC-SIMD-001 REQ-SIMD-004` | `SPEC-SIMD-001` (첫 토큰) |
| `SRS-CALIB-FUNC-006-EXT 6a` | **없음** |
| `REQ-P1A-091` (값 하나) | **없음** |
| `REQ-P1A-090, REQ-P1A-091` (쉼표 두 값) | **없음** |
| `api-spec.md 6 "Cached loaders"` | **없음** |

   그리고 `--spec SPEC-AUTH-001` 은 프로브에서 **1건**을 찾아내고 `--spec REQ-P1A-091` 은 0건입니다 — 질의 자체는 동작하고, 값이 `SPEC-` 로 시작해야만 색인됩니다.
4. **주석 모양**(`evidence/20`): 같은 `SPEC-XPE-P1A` 값이라도 `/** … */` 안의 ` * @MX:NOTE` / ` * @MX:SPEC` 은 **스캐너가 태그로 인식하지 않았고**(레코드 0건), `//` 줄 주석 모양만 인식되었습니다. 저장소 `rle_codec.cpp` 의 `SPEC-XPE-P1A` 두 곳이 색인에서 빠진 이유와 일치합니다.

### 판정

| 질문 | 답 |
|---|---|
| `@MX:SPEC: REQ-P1A-090, REQ-P1A-091` (쉼표 두 값)이 읽히는가 | **아니오.** `REQ-…` 로 시작하는 값은 도구가 `specRef` 로 잡지 않습니다. 단일 `REQ-P1A-091` 도 마찬가지입니다 |
| 쉼표 형식이 문제인가 | 이 도구 버전에서는 **무관**합니다. `SPEC-` 값이었어도 쉼표 목록은 첫 ID 하나만 읽힙니다 |
| 이 저장소에서 `moai mx query --spec` 로 추적되는 `@MX:SPEC` 가 있는가 | **0건**(`//` 줄 주석 + `SPEC-` 값 조건을 만족하는 태그가 없음) |

**`QA-A-179` 의 논거에 대한 함의**: "MX 태그가 잘못된 요구를 가리키면 추적이 틀어진다"는 이 도구 버전에서는 **기계 추적이 아니라 사람이 읽는 추적**에 해당합니다. 정정 자체는 맞지만(옛 `REQ-P1A-020` 은 틀린 요구를 가리켰고 지금 `090, 091` 은 맞음), 그 값을 `moai mx query` 가 소비하지는 않습니다. 기계가 읽게 하려면 `REQ-…` 가 아니라 `SPEC-XPE-P1A` 같은 `SPEC-` 형태의 ID 가 필요한데, 그러면 어느 요구인지가 사라집니다 — 이 선택은 리더 몫으로 남깁니다.

`.agents/skills/moai/references/mx-tag.md:116` 의 규칙("`@MX:SPEC: SPEC-XXX-000` 가 가리키는 SPEC 파일이 없으면 `@MX:LEGACY` 로 바꾸고 TODO 를 단다")도 `SPEC-…` 모양의 ID 에 대한 것이고, `REQ-P1A-…` 값에는 걸리지 않습니다.

### 한계

- **`moai-adk 3.1.2` 한 버전**의 동작입니다. 다른 버전·`moai` 의 다른 명령(예: `moai mx` 의 다른 부속 기능, 훅, `moai review`)이 `@MX:SPEC` 를 다르게 읽는지는 확인하지 않았습니다. 바이너리 소스는 읽지 못했고, 판정은 **블랙박스 프로브**(입력 모양 → 출력)에서 나온 것입니다.
- 프로브 모양은 위 표의 9개와 주석 모양 2개입니다. 모든 접두어(`REQ-`·`SRS-`·`SPEC-` 외의 것)를 시험하지는 않았습니다. "`SPEC-` 로 시작해야 한다"는 표본에서 관측한 **규칙의 추정**이며, 정규식을 직접 본 것이 아닙니다.
- 스캔은 `.moai/state/mx-index.json` 을 썼고, 이 파일은 **이 카드 전에는 없던 것**이라 확인 뒤 삭제했습니다(`.moai/state/` 에는 원래대로 `config-cache.json` 만 남음). 스캔은 `modules/preprocess/src` 에 한정했습니다(저장소 전체가 아님).

---

## 3. 하지 않은 것

- 코드·단언·시험 이름 변경 없음, push 없음, 새 이슈 없음
- `CMakeLists.txt:24` 외의 줄(§1 목록), `@MX:SPEC` 태그 값(`binning_correct.cpp:15`)은 이번 카드에서 바꾸지 않았습니다 — 판정만 보고합니다
- `docs/`·SPEC·VVP·생성된 Doxygen 수정 없음

## 4. 미검증

- 위 한계의 `moai` 버전·다른 명령.
- `CMakeLists.txt` 목록(§1)은 SPEC 제목과의 대조이고 요구 본문과 구현을 한 줄씩 비교하지는 않았습니다.
- `.moai/reports/` 의 과거 카드 보고서가 옛 번호를 인용하는지는 보지 않았습니다 (역사 기록).

🗿 MoAI
