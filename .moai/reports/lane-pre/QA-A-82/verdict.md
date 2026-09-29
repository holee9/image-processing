# QA-A-82 — `b3dd3b8`(줄 중간 주석) held-in 시험 (#160)

**카드**: `.moai/lanes/pre/inbox/QA-A-82.md` · **브랜치**: `dev/preprocess` · **HEAD**: `0e4b8ab` (`b3dd3b8` 포함) · **기계**: Intel Core i7-12700

**저장소 파일을 바꾸지 않았다.** 되살린 소스, 모듈 CMakeLists 68행 주석, 잠시 되돌린 루트 CMakeLists 전부 작업 트리에서만 쓰고 복원했다.

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | 네 팔 결과: **1 / 1 / 0 / 0** — 카드 기대값과 모두 같다 |
| C2 | **4-old 가 0건이다.** 같은 트리에서 `b3dd3b8` 이전 검사는 줄 중간 주석에 속았다 |
| C3 | **4 가 1건이다.** `b3dd3b8` 이후 검사는 같은 주석에 속지 않았다 |
| C4 | 되살린 파일·주석·루트 되돌림 모두 흔적 없음 |
| C5 | 빌드·테스트 정상: 643/643, 경고 0 |
| C6 | `#160` 에 수치만 담은 코멘트를 달았다 |

"`b3dd3b8` 이 무엇을 고쳤는가"의 판정은 리더 몫이다. 이 보고서는 수치를 적는다.

---

## 2. 증거 (Evidence)

### 사전 확인

```
$ git merge-base --is-ancestor b3dd3b8 HEAD && echo YES
YES
$ git rev-list --count --left-right origin/main...HEAD
0	4
```

두 판본의 필터 부분:

```
HEAD 판      41:  string(REGEX REPLACE "#.*$" "" _line "${_line}")
b3dd3b8^ 판  37:  if(NOT _line MATCHES "^[ <TAB>]*#")      (대괄호 안은 공백·탭 원문)
```

### C1 — 네 팔

명령: `cmake --preset ci-preprocess` (배치 `a82-cfg.bat`, 인자로 로그 이름만 달리함). 모든 `CFG_EXIT_*=0`.

| 팔 | 작업 트리 | 루트 CMake | 출력 | 로그 바이트 |
|---|---|---|---|---|
| 1 | 고아 소스 되살림 | HEAD | `1 source file(s) under modules/*/src/ ...` + `modules/preprocess/src/mode_selector.cpp` | 1240 |
| **4** | 1 + 68행 끝 주석 | HEAD | `1 source file(s) ...` + `modules/preprocess/src/mode_selector.cpp` | 1240 |
| **4-old** | 4 와 동일 | `b3dd3b8^` | `-- Orphan-source check: 0 unlisted sources under modules/*/src/` | 874 |
| 2 | 정리 | HEAD | `-- Orphan-source check: 0 unlisted sources under modules/*/src/` | 874 |

4번 팔에서 바꾼 68행 원문:

```
target_compile_features(xpe_preprocess PUBLIC cxx_std_17)  # mode_selector.cpp
```

`sed -i '68s/$/  # mode_selector.cpp/'` 로 붙였고, 붙인 직후 `sed -n '68p'` 로 위 원문을 확인했다.

4-old 판본 확인:

```
REGEX REPLACE in root = 0      <- b3dd3b8^ 판에 새 필터가 없음
```

### C2 · C3 — 4 와 4-old 의 차이

두 팔의 작업 트리는 **같다**(고아 소스 있음, 68행 주석 있음). 다른 것은 루트 CMakeLists 판본 하나다.

- `b3dd3b8^` 판 → **0건**
- HEAD 판 → **1건**

A-80 의 3 / 3-old 와 같은 구조다. 그때는 **줄 맨 앞** 주석(`2ef4d6f` 대상), 이번에는 **줄 중간** 주석(`b3dd3b8` 대상).

### C4 — 복원 확인

```
4-old 직후   git diff CMakeLists.txt | wc -l      → 0
2번 팔 전    git checkout -- modules/preprocess/CMakeLists.txt
             rm -f modules/preprocess/src/mode_selector.cpp
종료 후      git status --short
             ?? .claude/settings.json.bak-hook
             ?? .claude/settings.json.bak-rmask
```

### C5 — 빌드·테스트

```
PRE_BUILD_EXIT=0        a82-pre-build.log   161 bytes
PRE_CTEST_EXIT=0        a82-pre-ctest.log   126312 bytes
warnings = 0
100% tests passed, 0 tests failed out of 643
```

### C6 — `#160` 코멘트

```
https://github.com/holee9/image-processing/issues/160#issuecomment-5706503899
comments now: 5 state=CLOSED
```

본문은 §C1 표와 판본·복원 확인만 담았다. "성공/실패" 같은 판정 단어는 넣지 않았다.

**게시 절차 한 가지**: 본문 파일 수정(필터 표기를 `\t` → `<TAB>` 원문 설명으로)과 게시를 같은 턴에 병렬로 실행했다. 순서가 보장되지 않으므로 게시 후 본문을 다시 받아 `<TAB>` 문자열이 1건 있는 것을 확인했다 — 수정본이 올라갔다.

---

## 3. baseline 귀속 (Baseline-attribution)

- 네 configure 는 같은 스크립트·같은 프리셋, 로그 이름만 다름.
- 4 와 4-old 의 유일한 차이는 루트 CMakeLists 판본. 1 과 4 의 유일한 차이는 68행 주석.
- 되살린 소스는 A-79·A-80 과 같은 `git show 87ff54d^:...` 원문.
- 빌드·ctest 는 네 팔 뒤 실행분.

---

## 4. 미검증 (Gaps)

- **줄 중간 주석 위치를 68행 한 곳만 시험했다.** 소스 목록 블록(`set(XPE_PREPROCESS_SOURCES ...)` 안쪽) 같은 여러 줄 명령 내부의 줄 중간 주석은 시험하지 않았다.
- **`#` 가 문자열 안에 있는 경우**(예: `"a#b"`)는 시험하지 않았다. 새 필터 `#.*$` 는 따옴표를 구분하지 않으므로 그런 줄의 뒷부분도 지워진다 — 코드를 읽은 것이지 실행한 것이 아니다.
- **`preprocess` 외 모듈에서 돌리지 않았다.**
- **`string(FIND)` 부분 문자열 성질은 이번에도 시험하지 않았다**(A-80 §5).
- **push 하지 않았다.**

---

## 5. 잔여 위험 (Residual-risk)

- **새 필터는 따옴표 안 `#` 도 주석으로 본다.** 소스 목록에 `#` 가 들어간 파일명이나 생성기 표현식이 있으면 그 뒤가 지워져 미등재로 오판(오탐)할 수 있다. 오탐은 WARNING 한 줄로 드러나므로 미탐보다 덜 위험하지만, 구조적으로 가능하다. 현재 저장소에 그런 줄이 있는지는 세지 않았다.
- **WARNING 은 빌드를 세우지 않는다.**
- **이번 병합 커밋들은 미푸시다.**

---

Refs #160
