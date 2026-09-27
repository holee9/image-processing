# QA-A-83 — AUTO 모드의 "Adaptive" 주석 정정 (#169) + #143 출처 확인

**카드**: `.moai/lanes/pre/inbox/QA-A-83.md` · **브랜치**: `dev/preprocess` · **커밋**: `5747c62` (미푸시)

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | **카드가 지목한 두 곳 외에 두 곳이 더 있었다.** AUTO 를 "Adaptive" 라고 적은 곳은 4개 — 공개 헤더 2, 구현 1, 테스트 1. 넷 다 고쳤다 |
| C2 | 주석만 바뀌었다. 주석을 걷어낸 세 파일의 해시가 전후 동일하다 |
| C3 | 남은 "adaptive" 2건은 결함 검출 RMM 의 "adaptive local statistics" 로, 뜻이 달라 두었다 |
| C4 | 공개 헤더의 이슈 번호는 `\#169` 로 적었다 — Doxyfile 이 경고를 실패로 취급한다 |
| C5 | 빌드·테스트 정상: 헤더 변경으로 재컴파일, 경고 0, 643/643 |
| C6 | **Doxygen 은 로컬에서 돌리지 못했다** — 실행 파일이 없다(#159 이력과 같음) |
| C7 | `#169` 에 사실 코멘트를 달았다 |
| C8 | **#143 의 수치는 합성 가우시안 프레임에서 나왔다** — QA-A-40 보고서에 명시 |

---

## 2. 증거 (Evidence)

### C1 — 범위를 넓힌 계수

**범위 A**: `modules/preprocess/`, `modules/common/` 전체, `build/` 제외, `.h .hpp .cpp .c .txt .md`, 대소문자 무시.

수정 전:

```
modules/preprocess/include/xpe/preprocess_api.h:872: *   Uses RMM (Robust Mask Maker) with adaptive local statistics:
modules/preprocess/include/xpe/preprocess_api.h:937: * - AUTO: Adaptive selection (max 10 points, degree 3)
modules/preprocess/include/xpe/preprocess_api.h:945:    XPE_CALIB_AUTO           = 5   ///< Adaptive mode (max 10 points)
modules/preprocess/src/xpe_calib_mode.cpp:66:    /* XPE_CALIB_AUTO           */ {10, 3 }   // Adaptive (max 10, cubic)
modules/preprocess/src/xpe_defect_gen.cpp:230: * FUNC-022: Dark BPM generation with adaptive local statistics
modules/preprocess/tests/test_calib_mode.cpp:134:        {XPE_CALIB_AUTO,           3}   // Adaptive (cubic)
count = 6
```

카드는 `:945` 와 `:66` 을 지목했다. **`:937`(같은 헤더의 모드 표 설명)과 `test_calib_mode.cpp:134` 가 더 있었다.** 카드가 "한 곳만 고치고 정리됐다고 읽는 실수"를 경고한 그대로다.

수정 후:

```
modules/preprocess/include/xpe/preprocess_api.h:872: *   Uses RMM (Robust Mask Maker) with adaptive local statistics:
modules/preprocess/src/xpe_defect_gen.cpp:230: * FUNC-022: Dark BPM generation with adaptive local statistics
```

**범위 B** (Doxygen 공개 문서 범위): `docs/help/doxygen/Doxyfile` 의 INPUT —

```
INPUT = ../../../modules/common/include \
        ../../../modules/preprocess/include \
        ../../../modules/enhance_basic/include \
        ../../../modules/display/include \
        ../../../modules/dicom/include \
        ../../../modules/enhance_advanced/include \
        ../../../modules/gsvg/include
FILE_PATTERNS = *.h
RECURSIVE     = YES
EXCLUDE_PATTERNS = *internal* *_impl* *_detail* *_private*
```

`*.h`, 재귀, `*internal*` 만 제외 → 헤더 13개. 수정 전 3건(`preprocess_api.h` 872·937·945), 수정 후 1건(872).
대조군(같은 범위·같은 실행): `XPE_CALIB_AUTO` 가 `preprocess_api.h` 에서 1건 — 계수 명령이 이 범위를 실제로 읽는다.

`*_impl*` `*_detail*` `*_private*` 는 이 계수에 적용하지 않았다. 적용하면 범위가 같거나 좁아지므로, 위 3건/1건은 상한이다.

### 바꾼 문구

```diff
- * - AUTO: Adaptive selection (max 10 points, degree 3)
+ * - AUTO: currently the same fixed parameters as MULTI_POINT_10 (max 10
+ *   points, degree 3). It does NOT select a mode from the input: the
+ *   automatic selection required by SRS-CALIB-FUNC-031(5) is not
+ *   implemented (tracked in issue \#169).

-    XPE_CALIB_AUTO           = 5   ///< Adaptive mode (max 10 points)
+    XPE_CALIB_AUTO           = 5   ///< Same as MULTI_POINT_10 today; no automatic selection (issue \#169)

-    /* XPE_CALIB_AUTO           */ {10, 3 }   // Adaptive (max 10, cubic)
+    /* XPE_CALIB_AUTO           */ {10, 3 }   // Same as MULTI_POINT_10; FUNC-031(5) auto-select not implemented (#169)

-        {XPE_CALIB_AUTO,           3}   // Adaptive (cubic)
+        {XPE_CALIB_AUTO,           3}   // Cubic, same as MULTI_POINT_10 (no auto-select, #169)
```

`\#169` 가 백슬래시 하나로 들어갔는지 바이트로 확인했다(`cat -A`: `issue \#169).$`). 수정 스크립트가 파이썬 `SyntaxWarning: invalid escape sequence '\#'` 를 냈기 때문에, 경고를 믿지 않고 결과 파일을 봤다.

### C2 — 동작 불변

주석(`//` 이후, `*` 로 시작하는 줄)을 걷어낸 뒤 해시:

```
xpe_calib_mode.cpp   HEAD=a8ef8582  작업트리=a8ef8582
test_calib_mode.cpp  HEAD=5c7ed1c8  작업트리=5c7ed1c8
preprocess_api.h     HEAD=ea7d6fcc  작업트리=ea7d6fcc
```

`kModeParams` AUTO 행은 diff 에서 `{10, 3 }` 이 전후 동일하다.

### C4 — Doxygen 설정

```
docs/help/doxygen/Doxyfile:100: WARN_AS_ERROR = FAIL_ON_WARNINGS
docs/help/doxygen/Doxyfile:108: EXTRACT_ALL   = NO
```

`\#` 는 A-68/A-71 에서 `\#if` 로 쓴 것과 같은 이스케이프다.

### C5 — 빌드·테스트

```
PRE_BUILD_EXIT=0      a83-pre-build.log   11068 bytes   (헤더 변경 → [92/93] 까지 재컴파일)
PRE_CTEST_EXIT=0      a83-pre-ctest.log   126312 bytes
warnings = 0
100% tests passed, 0 tests failed out of 643
```

`CalibModeTest.DefaultModeIsMultiPoint8`, `CalibModeTest.SetGetRoundtrip_AllModes` 등 해당 테스트가 로그에 Passed 로 있다.

### C6 — Doxygen 미실행

```
$ which doxygen
which: no doxygen in (...)
$ ls "/c/Program Files/doxygen/bin/doxygen.exe"
No such file or directory
```

CI `docs-generate.yml` 은 `ssciwr/doxygen-install@v2` 로 1.12.0 을 설치해 `doxygen Doxyfile` 을 돌린다. **푸시 후 그 결과가 이 변경의 유일한 Doxygen 증거가 된다.**

### C7 — `#169` 코멘트

```
https://github.com/holee9/image-processing/issues/169#issuecomment-5706547646
check: n=3 impl=true      <- 게시 후 재조회, 마지막 수정(_impl 문구)이 본문에 있음
```

바꾼 네 위치와 전후 문구, 두 범위의 계수, 해시, 빌드·테스트만 담았다. 판정 문장 없음.

### C8 — #143 의 출처

`#143` 본문: `실측 (QA-A-40, 2026-09-11, 1024² 합성 프레임, 961 주입 5σ 트랜지언트, RelWithDebInfo)`

`QA-A-40/verdict.md` 의 실험 조건 표:

```
| 프레임 크기 | 1024 × 1024 | 카드 지정 |
| 잡음 준위   | σ = 10 ADU / 50 ADU (평균 3000) | ...
| 주입 개수   | 961 (32픽셀 격자, 테두리 16 여유) | ...
| 시드        | std::mt19937(20260911) | 결정성 |
```

같은 보고서 §4 Gap 5:

```
5. **실제 임상 프레임이 아니라 합성 가우시안이다.** SPEC 은 "clean clinical
   frames" 라고 쓴다.
```

원시 출력(보고서 73·74행):

```
[rates] low noise, 5 sigma    injected=961 TP=481 FN=480  TPR=0.500520 | FP=496/1048576 FPR=0.000473022
[rates] high noise, 5 sigma   injected=961 TP=481 FN=480  TPR=0.500520 | FP=496/1048576 FPR=0.000473022
```

**합성 가우시안이다.**

덧붙여 관측한 것 — 같은 이름의 행이 뒤 보고서에서 다른 값을 적는다:

```
QA-A-42/verdict.md:82   | TPR @ 5σ | 0.500520 | 0.610822 | 개선 +22% |
QA-A-43/verdict.md:24   | TPR @ 5σ | 0.500520 | 0.610822 | 0.553590 |
```

**`#143` 본문은 A-40 시점의 값이다.** A-42·A-43 의 프레임 출처는 이번에 확인하지 않았다.

---

## 3. baseline 귀속 (Baseline-attribution)

- 계수 두 범위는 같은 턴에 수정 전·후로 각각 실행했다.
- 해시 비교는 `git show HEAD:<file>` 과 작업 트리 파일을 같은 필터로 처리했다.
- 빌드·ctest 는 수정 후, 커밋 전 실행분.
- #143 확인은 보고서·이슈 본문 원문이며 재측정하지 않았다.
- 병합 기준: 착수 전 `origin/main` 병합(`CMakeLists.txt` 8줄 — 리더의 `5dec3c5` 한계 주석).

---

## 4. 미검증 (Gaps)

- **Doxygen 을 돌리지 않았다.** `\#169` 가 경고 없이 처리되는지는 CI 결과를 봐야 안다.
- **범위 B 에 Doxyfile 의 제외 패턴 셋을 적용하지 않았다**(§C1).
- **`modules/preprocess`·`common` 밖의 소스 파일(`.cpp`)은 세지 않았다.** 범위 B 는 헤더만이다. 다른 레인 모듈의 구현에 AUTO 관련 서술이 있는지는 모른다.
- **"adaptive" 외의 다른 표현**(예: "auto-select", "automatic", "자동 선택")으로 같은 거짓을 적은 곳이 있는지는 세지 않았다.
- **`docs/` 와 SRS 는 제 소유가 아니라 세지 않았다.**
- **A-42·A-43 의 TPR 이 어떤 프레임에서 나왔는지 확인하지 않았다.**
- **push 하지 않았다.**

---

## 5. 잔여 위험 (Residual-risk)

- **Doxygen CI 가 깨질 수 있다.** `\#` 는 이 저장소에서 통과한 이력이 있는 이스케이프지만, 이번 문맥(`issue \#169)`)에서 실행으로 확인한 적은 없다.
- **#169 의 결정이 나면 네 주석을 다시 바꿔야 한다.** 이번 문구는 "현재 동작"을 적었으므로, 구현이 들어오든 요구가 바뀌든 둘 다 이 주석을 무효로 만든다. 네 위치는 커밋 메시지와 `#169` 코멘트에 적어 두었다.
- **다른 표현의 같은 거짓은 남아 있을 수 있다**(§4).

---

Refs #169
Refs #143
