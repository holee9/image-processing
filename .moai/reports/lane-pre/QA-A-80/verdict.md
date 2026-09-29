# QA-A-80 — 주석 미탐 수정의 held-in 시험 (#160)

**카드**: `.moai/lanes/pre/inbox/QA-A-80.md` · **브랜치**: `dev/preprocess` · **기계**: Intel Core i7-12700

**저장소 파일을 바꾸지 않았다.** 되살린 소스와 주석 줄, 그리고 아래 §C4 에서 잠시 되돌린 루트 `CMakeLists.txt` 까지 전부 작업 트리에만 두었다가 복원했고 `git status` 로 확인했다.

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | **3번 팔 통과 — 주석이 있어도 여전히 1건**이다. 수정이 실제로 먹었다 |
| C2 | 1번 팔 1건, 2번 팔 0건 — A-79 의 대조 쌍이 수정 후에도 그대로 성립 |
| C3 | **덤으로 한 팔 더 돌렸다: 같은 트리에서 수정 *전* 검사는 0건을 찍는다.** "수정 전이었으면 0건이 나왔을 자리"가 추정이 아니라 관측이 됐다 |
| C4 | `XPE_CALIB_MODE_` 규범 0 / 주석 2 — 기대값과 일치 |
| C5 | 되살린 파일·주석 줄·되돌린 루트 CMake 전부 커밋에 안 들어갔다 |
| C6 | 빌드·테스트 정상: 643/643, 경고 0 |
| C7 | **`2ef4d6f` 는 `origin/main` 에 없었다.** 로컬 `main` 에서 가져왔다 — 세 번째 같은 형태 |

---

## 2. 증거 (Evidence)

### 네 팔 — 같은 스크립트, 같은 프리셋, 로그 이름만 다름

| 팔 | 조작 | 기대 | **관측** | 로그 바이트 |
|---|---|---|---|---|
| 1 | 고아 소스 되살림 | 1건 | **1건** | 1240 |
| 3 | 되살린 채 + 모듈 CMakeLists **주석 줄**에 파일명 1회 | 1건 | **1건** | 1240 |
| **3-old** | **3번과 같은 트리, 루트 CMake 만 `2ef4d6f^`(수정 전)** | — | **0건** | 874 |
| 2 | 주석 줄·되살린 파일 제거 | 0건 | **0건** | 874 |

모든 팔 `CFG_EXIT_*=0`, 모든 로그 파일 실재(크기 함께 기록 — A-79 §C6 의 장치).

**1번 팔:**

```
CMake Warning at CMakeLists.txt:159 (message):
  1 source file(s) under modules/*/src/ are named in no module
    modules/preprocess/src/mode_selector.cpp
```

**3번 팔** — `modules/preprocess/CMakeLists.txt` 끝에 붙인 줄:

```
# QA-A-80 temporary probe line: mode_selector.cpp mentioned in a comment only
```

같은 경고, 같은 파일명, 같은 로그 크기(1240). **주석이 검사를 침묵시키지 못했다.**

**2번 팔:**

```
-- Orphan-source check: 0 unlisted sources under modules/*/src/
```

### C3 — 3-old: 수정 전 검사는 침묵한다

카드는 *"수정 전이었다면 3번에서 0건이 나왔을 겁니다"* 라고 썼다. **그것을 추정으로 두지 않고 실제로 돌렸다.**

작업 트리는 3번 팔과 **완전히 동일**하게 두고(고아 소스 되살려진 상태 + 주석 줄 있는 상태), 루트 `CMakeLists.txt` 만 수정 전 판으로 바꿨다:

```
$ git show 2ef4d6f^:CMakeLists.txt > CMakeLists.txt
$ grep -c 'file(STRINGS' CMakeLists.txt
0                                  <- 수정 전 판임을 확인
$ cmake --preset ci-preprocess
CFG_EXIT_arm3old=0
log bytes: 874
-- Orphan-source check: 0 unlisted sources under modules/*/src/
```

**0건이다.** 같은 트리, 같은 고아 파일, 같은 주석 — **검사만 바꿨는데 1 → 0 으로 뒤집힌다.**

이것이 held-in 증명이다: 수정이 없었다면 이 상황에서 검사가 침묵했고, 수정이 있으니 잡는다. 3번 팔의 1건이 "원래도 잡혔을 것"이 아니라 **수정이 만든 결과**임이 확정된다.

복원 확인:

```
$ cp <새 판 사본> CMakeLists.txt
$ git diff --stat CMakeLists.txt
(출력 없음)
```

### C4 — SRS 계수

```
$ grep -n "XPE_CALIB_MODE_" docs/calibration/SRS-CALIB-001_...md
170:> **열거형 표기 정정 2026-09-16 (QA-A-77).** ...
179:> **(7) 기본 모드** 조항에 남은 `XPE_CALIB_MODE_MULTI_POINT_8` 을 놓쳤습니다 ...
```

**규범 문장 0건 / 정정 주석 2건.** 기대값과 일치한다. 168행(요구 본문)에서 사라졌고, 179행은 이번 두 번째 정정이 스스로를 기록한 줄이다.

### C5 — 작업 트리 정리 확인

```
$ git status --short
?? .claude/settings.json.bak-hook
?? .claude/settings.json.bak-rmask
```

되살린 `mode_selector.cpp`, 모듈 CMakeLists 주석 줄, 루트 CMakeLists 되돌림 — **셋 다 흔적 없음.** 두 `??` 는 lead 의 권한 설정 파일로 규약대로 손대지 않았다.

### C6 — 빌드·테스트

configure 를 네 번 돌렸으므로 트리 건전성을 확인했다.

```
PRE_BUILD_EXIT=0        a80-pre-build.log   161 bytes
PRE_CTEST_EXIT=0        a80-pre-ctest.log   126312 bytes
warnings = 0
100% tests passed, 0 tests failed out of 643
```

### C7 — `2ef4d6f` 는 `origin/main` 에 없었다

카드는 "`2ef4d6f` 로 막았습니다" 라고 했는데, `origin/main` 병합 후에도 검사 본문이 `file(READ)` + `string(FIND)` 그대로였다.

```
$ git fetch -q origin main && git merge origin/main --no-edit
 .../SRS-CALIB-001_...md | 8 +++++++-          <- SRS 정정만 왔다

$ git log --oneline -1 -- CMakeLists.txt
4306640  ci+build: ... (#160)                   <- 수정 전 판

$ git branch -a --contains 2ef4d6f
+ main                                          <- origin/main 없음 = 미푸시

$ git log --oneline origin/main..main
2ef4d6f  ci+build: 주석 안의 파일명이 검사를 침묵시키던 미탐을 막고 ... (#160)
e1070f5  merge(gui): GUI-C-75 ... (#149)
4a741a5  test(gui): 재획득 건너뛰기에 XPE-SKIP-ALLOWED:30011 토큰을 단다 (GUI-C-75)
```

**로컬 `main` 을 병합해 가져왔다.** 같이 딸려온 GUI 테스트 변경 1건(`WindowReacquireTests.cs`)은 제 소유가 아니라 읽지 않았고 건드리지 않았다.

**세 번 연속 같은 형태다**(A-78 §7 검사, A-79 SRS 두 번째 정정, 이번 `2ef4d6f`). 매번 "푸시했다"와 제 fetch 사이가 어긋났다. 이번에는 로컬 `main` 이 같은 저장소에 있어 복구할 수 있었지만, **그건 이 워크트리 구조가 준 우연이지 절차가 보장한 것이 아니다.**

---

## 3. baseline 귀속 (Baseline-attribution)

- 네 configure 는 **같은 배치 스크립트**(`a80-cfg.bat`), 같은 프리셋(`ci-preprocess`), 인자로 로그 이름만 달리했다.
- 1번과 3번의 유일한 차이는 모듈 CMakeLists 주석 줄 하나. 3번과 3-old 의 유일한 차이는 루트 CMakeLists 판본 하나.
- SRS 계수는 A-77~A-79 와 같은 파일·같은 패턴.
- 빌드·ctest 는 네 팔이 모두 끝난 **뒤** 실행분.
- 병합 기준: `origin/main` 병합 → 이후 로컬 `main` 병합(ort).

---

## 4. 미검증 (Gaps)

- **다른 모듈에서 돌려보지 않았다.** 세 입력 모두 `modules/preprocess/` 에서만 시험했다. 카드 4절이 전수는 별건으로 둔다고 했고 동의한다.
- **주석 형태를 한 가지만 시험했다.** 줄 맨 앞 `#` 이다. **줄 중간에서 시작하는 주석**(`set(X y)  # mode_selector.cpp`)은 `^[ \t]*#` 에 걸리지 않으므로 여전히 코드로 읽힌다 — 시험하지 않았고, §5 에 위험으로 적는다.
- **인용부호 안 문자열, 여러 줄 주석은 고려하지 않았다.**
- **`2ef4d6f` 가 함께 바꾼 `.github/workflows/ci.yml` 4줄(로그 인코딩)을 읽지 않았다.** 제 소유가 아니고 이 카드 범위 밖이다.
- **GUI 테스트 변경을 읽지 않았다** — 병합에 딸려온 것.
- **push 하지 않았다.**

---

## 5. 잔여 위험 (Residual-risk)

- **줄 중간 주석은 여전히 검사를 침묵시킬 수 있다.** 필터가 `^[ \t]*#` 로 **줄 전체가 주석인 경우**만 제외한다. `target_sources(... )  # 참고: mode_selector.cpp 는 지웠다` 같은 줄은 코드 줄로 남아 파일명이 매칭된다. A-76 때 제가 남긴 인용은 줄 맨 앞 `#` 이었으므로 이번 수정으로 막히지만, **같은 의도의 다른 표기는 안 막힌다.** 이것이 지금 남은 같은 계열의 구멍이다.
- **`string(FIND)` 는 여전히 부분 문자열 일치다.** 주석을 걷어냈을 뿐 매칭 방식은 그대로라, 예컨대 `x_mode_selector.cpp` 를 목록에 넣으면 `mode_selector.cpp` 도 "있다"로 읽힌다. 실사례는 없지만 구조는 남아 있다.
- **WARNING 은 빌드를 세우지 않는다.** 승격 전까지 읽는 사람에게 달려 있다.
- **리더의 커밋이 세 번 연속 제 fetch 보다 늦었다.** 이번엔 로컬 `main` 으로 우회됐지만, 다음에 같은 일이 나고 우회로가 없으면 카드가 그냥 막힌다.
- **`87ff54d` 및 이번 병합 커밋들은 미푸시다.**

---

Refs #160
