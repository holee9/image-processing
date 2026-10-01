# QA-A-78 — `mode_selector.cpp` / `.h` 삭제 (#169, #160)

**카드**: `.moai/lanes/pre/inbox/QA-A-78.md` · **브랜치**: `dev/preprocess` · **커밋**: `87ff54d` · **기계**: Intel Core i7-12700

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | 두 파일 다 삭제했다. 커밋 `87ff54d`, `Refs #169` 포함 |
| C2 | **반증이 요구한 대로 오류 코드가 바뀌었다**: 삭제 전 `C2065` → 삭제 후 `C1083 No such file`. 다른 경로에 사본 없음 |
| C3 | `xpe_mode_selector_` **0건**. 대조군은 그대로 살아 있다(16 / 14) |
| C4 | **`XPE_CALIB_MODE_` 는 0이 아니라 1이다 — 그런데 원인은 리더가 놓친 곳이 아니다.** 리더의 SRS 수정이 **아직 `origin/main` 에 없다** |
| C5 | 전체 통과: preprocess 643/643, common 69/69, 경고 0, 모든 `*_EXIT=0` |
| C6 | **§7 의 configure-time 검사를 검증하지 못했다 — 코드가 제 트리에 없다.** 전/후 대조 쌍을 만들 방법은 §2에 적었다 |

---

## 2. 증거 (Evidence)

### C2 — 반증: 오류 코드가 바뀌었다

카드 6번이 지정한 그대로, 같은 스크래치패드 TU(`mode_selector.h` 만 include)를 **삭제 전후에 각각** 컴파일했다. 저장소에 임시 파일은 만들지 않았다.

**삭제 전:**

```
modules\preprocess\include\xpe/preprocess/mode_selector.h(65): error C2065: 'XpeCalibrationMode': 정의되지 않은 식별자
modules\preprocess\include\xpe/preprocess/mode_selector.h(65): error C2146: 구문 오류: ')'가 'mode' 식별자 앞에 없습니다
modules\preprocess\include\xpe/preprocess/mode_selector.h(76): error C2065: 'XpeCalibrationMode': 정의되지 않은 식별자
modules\preprocess\include\xpe/preprocess/mode_selector.h(76): error C2065: 'out_mode': 정의되지 않은 식별자
modules\preprocess\include\xpe/preprocess/mode_selector.h(112): error C2065: 'XpeCalibrationMode': 정의되지 않은 식별자
modules\preprocess\include\xpe/preprocess/mode_selector.h(112): error C2146: 구문 오류: ')'가 'mode' 식별자 앞에 없습니다
modules\preprocess\include\xpe/preprocess/mode_selector.h(124): error C4430: 형식 지정자가 없습니다. int로 가정합니다
modules\preprocess\include\xpe/preprocess/mode_selector.h(124): error C2146: 구문 오류: ';'가 'xpe_mode_selector_auto_select' 식별자 앞에 없습니다
```

**삭제 후 (같은 TU, 같은 명령):**

```
a77_probe_alone.cpp(2): fatal error C1083: 포함 파일을 열 수 없습니다. 'xpe/preprocess/mode_selector.h': No such file or directory
```

**오류가 헤더 안쪽(`C2065`)에서 헤더를 찾는 단계(`C1083`)로 옮겨갔다.** 카드가 요구한 판별이 그대로 성립한다 — 같은 `C2065` 가 다시 났다면 include 경로 어딘가에 사본이 있다는 뜻이었을 것이다.

### C3 · C4 — 삭제 후 계수

**범위: 이 워크트리 전체. 제외: `build/`, 제 레인 보고서(`.moai/reports/lane-pre/`). 확장자: `.c .cpp .h .hpp .txt .md`.** A-77 과 같은 범위·같은 제외다.

```
xpe_mode_selector_   = 0   (접두사 전체, 함수별 아님)
--- control ---
xpe_calib_set_mode(  = 16
xpe_calib_get_mode(  = 14
```

**대조군이 그대로 16 / 14 다.** 0이 "명령이 눈멀었다"가 아니라 "실제로 없다"임을 같은 실행이 보증한다.

```
XPE_CALIB_MODE_      = 1
```

**카드는 0을 기대했다. 1이다. 그런데 남은 1건은 리더가 놓친 곳이 아니다:**

```
./docs/calibration/SRS-CALIB-001_Software_Requirements_Specification.md:168
  | **SRS-CALIB-FUNC-031** | ... `XPE_CALIB_MODE_SINGLE_POINT` (0, ...), ...
```

**리더의 SRS 수정이 제 트리에 도달하지 않았다.** 확인한 것:

```
$ git fetch origin main
$ git merge origin/main --no-edit
Updating 6f4c1d1..742a066
Fast-forward
 modules/dicom/src/DicomReader.cpp         |  17 +-
 modules/dicom/src/DicomReader.h           |   6 +
 modules/dicom/tests/test_dicom_reader.cpp | 336 +++++++++++++++++++++++++++---
```

```
$ git log --oneline -1 -- docs/calibration/SRS-CALIB-001_Software_Requirements_Specification.md
f25e2c1  docs(calib): SRS 표 열 뒤바뀜을 고치고 FUNC-033 현황을 적는다 (#140)
```

SRS 를 마지막으로 건드린 커밋은 **A-70 때의 `f25e2c1`** 이고, 이후 `origin/main` 에는 dicom 커밋 하나뿐이다. 그리고 삭제 직후 다시 `git fetch` 했을 때 `HEAD..origin/main` 은 **비어 있었다.**

**그러니 "제가 놓친 곳이 있습니다" 분기가 아니라, 수정이 아직 푸시되지 않은 상태다.** 살아 있는 표기 대조군도 같이 쟀는데 정정 전 상태와 일치한다:

```
XPE_CALIB_AUTO | XPE_CALIB_MULTI_POINT | XPE_CALIB_SINGLE  in SRS = 0
```

리더가 말한 "정정 전 SRS 에 살아 있는 표기 0건"이 제 트리에서 그대로 관측된다. **제 트리가 정정 전 상태라는 뜻이고, 이것이 1건의 원인이다.**

### C5 — 빌드와 전체 테스트

```
PRE_BUILD_EXIT=0
COMMON_BUILD_EXIT=0
PRE_CTEST_EXIT=0
COMMON_CTEST_EXIT=0

100% tests passed, 0 tests failed out of 643
100% tests passed, 0 tests failed out of 69
```

빌드 로그는 양쪽 다 한 줄(`ninja: no work to do.`), 경고 `grep -ci warning` = 0 / 0.

**카드가 경고한 대로 이 통과는 증거가 아니다** — 빌드에 없던 파일을 지웠으니 당연하다. 증거는 §C2 의 오류 코드 전환과 §C3 의 계수다. `no work to do` 가 말해 주는 것은 오히려 **삭제가 빌드 그래프를 전혀 건드리지 않았다**는 것이고, 그것이 "빌드에 없었다"의 재확인이다.

> **이 측정은 한 번 버렸다.** 처음 돌린 배치 스크립트에서 `%SP%` 를 정의하지 않아 로그 리다이렉트가 깨졌는데, `echo %errorlevel%` 은 **그래도 0을 찍었다.** 로그 파일이 없는 것을 보고 알아채 재실행했다. 위 수치는 재실행 결과다. (이 세션에서 여섯 번째로 "종료 코드 0이 실행을 뜻하지 않는" 경우다.)

### C6 — §7 의 검사는 검증할 수 없었다

카드 §7 은 configure-time `Orphan-source check` 를 루트 `CMakeLists.txt` 에 넣었다고 하고, 삭제 전 1건 / 삭제 후 0건의 대조 쌍을 요청했다. **그 코드가 제 트리에 없다:**

```
$ grep -n "Orphan-source\|orphan" CMakeLists.txt
(0건)

$ git fetch -q origin main && git log --oneline HEAD..origin/main
(비어 있음)
```

**`origin/main` 에 아직 없다.** 없는 코드를 돌렸다고 적을 수 없으므로 수행하지 않았다.

**대조 쌍을 만들 방법은 남아 있다.** 푸시되면 아래로 그대로 재현 가능하다 — 삭제가 이미 커밋됐어도 "삭제 전" 상태를 작업 트리에 되살릴 수 있기 때문이다:

1. main 을 병합해 검사를 받는다
2. `git show 87ff54d^:modules/preprocess/src/mode_selector.cpp > modules/preprocess/src/mode_selector.cpp` 로 **작업 트리에만** 되살린다(커밋하지 않음)
3. configure → 경고 **1건**이 나와야 한다. **안 나오면 검사가 눈이 먼 것이다**
4. 그 파일을 다시 지우고 configure → **0건**

**3번이 이 대조의 전부다.** 4번만 보면 "0건 = 통과"로 읽히는데, 그 0이 "없다"인지 "못 본다"인지 구분하지 못한다 — 리더가 §7 에 적은 우려가 정확히 그것이고, 이 저장소가 반복해 만난 형태다(부재 단언에는 대조군이 필요하다).

---

## 3. baseline 귀속 (Baseline-attribution)

- 삭제 전/후 컴파일은 **같은 TU·같은 명령**(`cl /c /EHsc /std:c++17 /I modules\preprocess\include /I modules\common\include`)으로, 삭제 직전과 직후에 각각 실행했다.
- 계수 셋은 A-77 과 **같은 범위·같은 제외 규칙**으로 삭제 후 한 번에 실행했다.
- 빌드·ctest 는 `ci-preprocess` / `ci-common` 프리셋, 로그 파일로 남겼고 종료 코드를 `echo` 로 찍었다(재실행분).
- 병합 기준: `6f4c1d1 → 742a066` fast-forward.

---

## 4. 미검증 (Gaps)

- **§7 의 configure-time 검사를 한 번도 돌리지 않았다** — 코드가 트리에 없다. 문법이 맞는지, 경고가 뜨는지 아무것도 확인하지 못했다.
- **삭제 전 계수를 이번에 다시 재지 않았다.** A-77 에서 잰 것(9개 각각 0, 대조군 16/14)을 그대로 쓰고, 이번에는 삭제 후만 쟀다. 삭제 전 값은 A-77 보고서에 귀속된다.
- **`XPE_CALIB_MODE_` 1건의 원인을 "미푸시"로 특정했지만, 리더 트리를 직접 보지는 못했다.** 제 트리에서 관측한 것은 (a) `origin/main` 에 SRS 커밋이 없고 (b) 살아 있는 표기가 SRS 에 0건이라는 것 둘이다. 리더가 실제로 고쳤는지는 리더 쪽 사실이다.
- **지운 176+130줄의 논리는 여전히 검사하지 않았다.** 컴파일되지 않는 코드는 검사할 수 없다.
- **`#169` 의 내용을 읽지 않았다.** 카드에 인용된 범위까지만 안다.
- **push 하지 않았다** — 레인 규약.

---

## 5. 잔여 위험 (Residual-risk)

- **`XPE_CALIB_MODE_` 1건은 리더가 푸시하면 사라지지만, 그때 다시 세는 사람이 없으면 확인되지 않는다.** 이 보고서의 1은 "정정 전 트리"의 값이고, 정정 후 값은 아직 아무도 측정하지 않았다.
- **§7 검사가 미검증인 채 루트 CMake 에 있다.** 문법 오류면 configure 가 깨지고, 논리 오류면 조용히 0건을 찍는다. **후자가 더 위험하다** — 검사가 있다는 사실이 안심을 주는데 실제로는 아무것도 안 보기 때문이다.
- **헤더 쪽 구멍은 여전히 열려 있다**(별건으로 합의됨): "공개 헤더인데 아무도 include 하지 않음"은 이번 소스 대조로 잡히지 않는다. 이번 건의 더 날카로운 신호가 그쪽이었다.
- **`87ff54d` 는 미푸시 유일본이다.**

---

Refs #169
Refs #160
