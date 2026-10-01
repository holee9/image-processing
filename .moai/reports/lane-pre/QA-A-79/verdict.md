# QA-A-79 — `Orphan-source check` 대조 쌍 (#160, #169)

**카드**: `.moai/lanes/pre/inbox/QA-A-79.md` · **브랜치**: `dev/preprocess` · **기계**: Intel Core i7-12700

**저장소 파일을 바꾸지 않았다.** 되살린 파일은 작업 트리에만 두었다가 제거했고, `git status` 로 확인했다.

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | **검사가 작동한다. 1건이 나왔다.** "삭제 전" 팔에서 `mode_selector.cpp` 를 정확히 지목했다 |
| C2 | "삭제 후" 팔은 0건 + `message(STATUS)` 로 넘어갔다 |
| C3 | 되살린 파일은 커밋에 들어가지 않았다 |
| C4 | **`XPE_CALIB_MODE_` 가 기대값과 다르다: 규범 1 / 주석 1. 기대는 규범 0 / 주석 2 였다.** (7) Default mode 조항이 제 트리에서 아직 안 고쳐져 있다 — **두 번째 정정이 `origin/main` 에 없다** |
| C5 | 재구성 후 빌드·테스트 정상: 643/643, 경고 0 |
| C6 | 카드 4번이 요구한 규칙을 적는다: **측정 스크립트는 종료 코드만이 아니라 산출물 실재도 확인한다** |

---

## 2. 증거 (Evidence)

### C1 — "삭제 전" 팔: 1건

`git show 87ff54d^:modules/preprocess/src/mode_selector.cpp` 로 **작업 트리에만** 되살린 뒤 `cmake --preset ci-preprocess`:

```
CFG_EXIT_before=0
-rw-r--r-- 1240 a79-cfg-before.log      <- 산출물 실재 확인

CMake Warning at CMakeLists.txt:159 (message):
  1 source file(s) under modules/*/src/ are named in no module
  CMakeLists.txt.  They are not built, so nothing they contain is checked by
  any build or test:

    modules/preprocess/src/mode_selector.cpp

  Either add each to its module's source list or delete it.  Do not leave it
  ...
```

**이것이 이 카드의 요점이다.** 1건이 나왔으므로 검사는 눈멀지 않았고, 따라서 다음 항목의 0건이 "없다"로 읽힌다.

파일 실재도 확인했다: `4886 bytes`, 되살리기 성공.

### C2 — "삭제 후" 팔: 0건

같은 명령, 되살린 파일만 제거:

```
CFG_EXIT_after=0
-rw-r--r--  874 a79-cfg-after.log       <- 산출물 실재 확인

-- Orphan-source check: 0 unlisted sources under modules/*/src/
```

`CMake Warning` 은 이 로그에 없다(같은 grep 에 `CMake Warning` 을 함께 넣어 확인했고 23행의 STATUS 한 줄만 잡혔다).

**전/후가 갈렸다.** 1 → 0, WARNING → STATUS. 대조 쌍 성립.

### C3 — 되살린 파일이 남지 않았다

```
$ git status --short
?? .claude/settings.json.bak-hook
?? .claude/settings.json.bak-rmask
```

`mode_selector.cpp` 항목 없음. 두 `??` 는 lead 의 권한 설정 파일로, 규약대로 손대지 않았다.

### C4 — `XPE_CALIB_MODE_` 가 기대값과 다르다

리더가 미리 준 기대값: **규범 문장 0건, 설명 주석 2건.**

**같은 범위**(워크트리 전체, `build/`·`.moai/reports/lane-pre/` 제외, `.c .cpp .h .hpp .txt .md`)로 다시 셌다:

```
총 2건
./docs/.../SRS-CALIB-001_...md:168   ← 요구 본문 (규범)
./docs/.../SRS-CALIB-001_...md:170   ← 정정 주석 (의도적 인용)
```

**총계는 2로 맞지만 분포가 다르다.** 168행의 내용:

```
... (7) Default mode shall be `XPE_CALIB_MODE_MULTI_POINT_8`
    (industry standard per Rayence, Schmidgunst 2007). ...
```

**이것이 리더가 "지나쳤다"고 한 바로 그 (7) 조항이고, 제 트리에서 아직 고쳐져 있지 않다.** 즉 관측값은 **규범 1 / 주석 1** 이다.

원인은 A-78 때와 같은 형태다 — **두 번째 정정이 `origin/main` 에 없다:**

```
$ git merge origin/main --no-edit
 .github/workflows/ci.yml       |  69 ++++-
 CMakeLists.txt                 |  52 +++++
 ...SRS-CALIB-001...md          |  13 +-
 modules/dicom/tests/...        | 243 +++++

$ git log --oneline -3 -- docs/calibration/SRS-CALIB-001_...md
efc88a0  docs(srs): FUNC-031 열거형 표기를 출하 코드에 맞춘다 ... (#169)
f25e2c1  docs(calib): SRS 표 열 뒤바뀜을 고치고 FUNC-033 현황을 적는다 (#140)
9674c35  docs(srs-calib): FUNC-033 ... (QA-A-35)

$ git fetch -q origin main && git log --oneline HEAD..origin/main
(비어 있음)
```

**SRS 커밋은 `efc88a0` 하나뿐이다** — 리더가 "열거형 목록만 고쳤다"고 한 **첫 번째** 정정이다. (7) 조항을 고친 두 번째 커밋은 `origin/main` 에 도달하지 않았다.

**살아 있는 표기 대조군도 같이 쟀다:**

```
XPE_CALIB_AUTO | XPE_CALIB_MULTI_POINT | XPE_CALIB_SINGLE | XPE_CALIB_DUAL  in SRS = 2
```

A-78 때 0건이었던 것이 2건으로 올라왔다 — `efc88a0` 이 실제로 도착했다는 뜻이다. **첫 정정은 있고 두 번째 정정만 없다.**

**합격 기준(규범 0건)은 현재 제 트리에서 충족되지 않는다.** 리더 트리에서는 충족될 수 있으나 그것은 제가 관측할 수 없는 사실이다.

덤 확인(카드 2절): 기본값 자체는 요구와 일치한다.

```
modules/preprocess/src/xpe_calib_mode.cpp:32:XpeCalibrationMode g_calib_mode = XPE_CALIB_MULTI_POINT_8;
```

### C5 — 재구성 후 빌드·테스트

configure 를 두 번 돌렸으므로 트리가 성한지 확인했다.

```
PRE_BUILD_EXIT=0
PRE_CTEST_EXIT=0
a79-pre-build.log   161 bytes     <- 산출물 실재 확인
a79-pre-ctest.log   126312 bytes  <- 산출물 실재 확인
warning 계수 = 0
100% tests passed, 0 tests failed out of 643
```

### C6 — 카드 4번이 요구한 한 줄

> **측정 스크립트는 종료 코드만이 아니라 산출물이 실재하는지도 확인한다. 로그 파일이 없으면 그 실행은 측정이 아니다.**

A-78 에서 `%SP%` 미정의로 리다이렉트가 깨졌는데 `%errorlevel%` 이 0을 찍었다. 이 저장소가 겪은 낡은 바이너리(빌드 결과를 안 읽어 통과로 읽힘)·도구 부재(0건을 위장)와 같은 계열이고, 셋 다 **"출력이 없는 것"과 "결과가 0인 것"을 가르지 못해** 생긴다.

**이 카드에서는 그 규칙을 실제로 적용했다** — 위 §C1·C2·C5 의 모든 측정에 로그 파일 크기를 함께 찍었다. 크기가 0이거나 파일이 없으면 그 실행은 버린다.

---

## 3. baseline 귀속 (Baseline-attribution)

- 두 configure 는 **같은 스크립트·같은 프리셋**(`ci-preprocess`)이고, 인자로 로그 이름만 달리했다. 차이는 `mode_selector.cpp` 의 유무 하나뿐이다.
- 계수는 A-77/A-78 과 **같은 범위·같은 제외 규칙**이다.
- 병합 기준: `origin/main` merge(ort), 이후 재fetch 시 `HEAD..origin/main` 비어 있음.
- 빌드·ctest 는 재구성 **후** 실행분이다.

---

## 4. 미검증 (Gaps)

- **리더 트리의 (7) 조항 상태를 보지 못했다.** 제가 관측한 것은 `origin/main` 에 두 번째 SRS 커밋이 없다는 것뿐이다. 리더 로컬에서 고쳐졌는지는 리더 쪽 사실이다.
- **`ci.yml` 에 들어온 69줄 변경을 읽지 않았다.** 같은 병합에 포함됐지만 이 카드 범위가 아니다.
- **검사의 다른 모듈 동작을 확인하지 않았다.** `modules/preprocess/` 에 고아를 하나 되살려 1건을 봤을 뿐, 다른 모듈에서도 같은 형태로 잡는지는 시험하지 않았다. GLOB 가 `modules/*` 전체를 도는 코드라는 것은 읽었지만 **읽은 것은 실행이 아니다.**
- **`string(FIND)` 방식의 오탐/미탐을 시험하지 않았다.** 예컨대 파일명이 주석 안에만 적혀 있어도 "목록에 있다"로 읽힐 수 있다. 이번 대조는 "있음/없음" 두 점만 확인했다.
- **헤더 쪽 구멍은 여전히 열려 있다** — 카드 5절대로 별건으로 남겼다.
- **push 하지 않았다.**

---

## 5. 잔여 위험 (Residual-risk)

- **`string(FIND)` 는 부분 문자열 일치다.** 모듈 CMakeLists 어디든 파일명 문자열이 있으면(주석, TODO, 정정 기록) 그 파일은 "목록에 있다"로 읽힌다. **A-76 에서 제가 CMakeLists 정정 주석에 옛 TODO 를 인용으로 남긴 전례가 있다** — 같은 형태로 인용된 파일명이 검사를 통과시킬 수 있다. 지금은 해당 없지만 구조적으로 가능하다.
- **WARNING 은 빌드를 세우지 않는다.** 승격(FATAL_ERROR) 전까지는 읽는 사람에게 달려 있고, "읽는 자리에 있다"가 이 설계의 근거였으므로 그 근거가 유지되는지는 시간이 지나야 안다.
- **규범 문장 1건이 남아 있다.** 두 번째 정정이 푸시되면 사라질 것으로 보이지만, **그때 다시 세는 사람이 없으면 확인되지 않는다.**
- **`87ff54d`(A-78 삭제)와 이번 병합 커밋은 미푸시다.**

---

Refs #160
Refs #169
