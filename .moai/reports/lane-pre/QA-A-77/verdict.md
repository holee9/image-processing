# QA-A-77 — `mode_selector.cpp` + `mode_selector.h`: 공개 헤더를 가진 고아 (#160)

**카드**: `.moai/lanes/pre/inbox/QA-A-77.md` · **브랜치**: `dev/preprocess` · **기계**: Intel Core i7-12700

**이 카드에서 저장소 파일은 하나도 바꾸지 않았다.** 카드 5번("결정하지 마십시오. 제가 판정합니다")과 6번("만들지는 마십시오")에 따라 조사와 보고만 했다.

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | **(a) 가 맞다 — 다른 것이 아니다.** 살아 있는 `xpe_calib_mode.cpp` 가 같은 개념(교정 모드 선택)을 구현하고, 고아 쪽은 같은 개념의 **다른 이름·다른 접두사** 판본이다 |
| C2 | **컴파일되지 않는다. 그리고 오류는 `.cpp` 가 아니라 공개 헤더에 있다.** `mode_selector.h` 자체가 `XpeCalibrationMode` 를 선언 없이 쓴다 |
| C3 | 그래서 리더의 전제("링크 실패")는 **절반만 맞다.** 소비자는 include 순서에 따라 **컴파일 단계**에서 죽거나, 그 다음 **링크 단계**에서 죽는다. 둘 다 보였다 |
| C4 | `xpe_mode_selector_*` 9개 함수 전부 외부 호출 **0건**. `mode_selector.h` 를 include 하는 파일은 **자기 짝 `.cpp` 하나뿐** |
| C5 | 대조군은 살아 있다: `xpe_calib_set_mode` 16건, `xpe_calib_get_mode` 14건 |
| C6 | 그런데 **SRS FUNC-031 은 고아 쪽 열거자 이름(`XPE_CALIB_MODE_*`)을 쓴다.** 문서가 죽은 쪽 이름을 따라간 자리가 있다 |
| C7 | §5에 CMake↔`src/*.cpp` 대조를 **기계화하는 제안**을 올린다 (제안만, 만들지 않음) |

---

## 2. 증거 (Evidence)

### C2 · C3 — 컴파일과 링크, 둘 다 실행했다

카드 6번은 "지우기 전에 `mode_selector.h` 를 include 하는 테스트를 하나 만들어 링크가 실패하는 것을 보이라"고 했다. **저장소에 테스트 파일을 만드는 대신 스크래치패드에 소비자 TU 둘을 만들어 컴파일했다** — 나중에 제거할 것이 남지 않는다. 이 이탈을 여기 적는다.

두 소비자 다 `build/ci-preprocess/lib/xpe_preprocess.lib` 에 링크했다.

**소비자 1 — `mode_selector.h` 만 include:**

```
modules\preprocess\include\xpe/preprocess/mode_selector.h(65): error C2065: 'XpeCalibrationMode': 정의되지 않은 식별자
modules\preprocess\include\xpe/preprocess/mode_selector.h(65): error C2146: 구문 오류: ')'가 'mode' 식별자 앞에 없습니다
modules\preprocess\include\xpe/preprocess/mode_selector.h(76): error C2065: 'XpeCalibrationMode': 정의되지 않은 식별자
modules\preprocess\include\xpe/preprocess/mode_selector.h(76): error C2065: 'out_mode': 정의되지 않은 식별자
modules\preprocess\include\xpe/preprocess/mode_selector.h(112): error C2065: 'XpeCalibrationMode': 정의되지 않은 식별자
modules\preprocess\include\xpe/preprocess/mode_selector.h(124): error C4430: 형식 지정자가 없습니다. int로 가정합니다
modules\preprocess\include\xpe/preprocess/mode_selector.h(124): error C2146: 구문 오류: ';'가 'xpe_mode_selector_auto_select' 식별자 앞에 없습니다
```

**링크까지 가지도 못한다.** 원인: `mode_selector.h` 는 `xpe/common/xpe_types.h` 와 `xpe/common/xpe_error.h` 만 include 하는데, `XpeCalibrationMode` 는 `preprocess_api.h` 에 있다. **공개 헤더가 자기 의존을 가져오지 않는다.**

**소비자 2 — `preprocess_api.h` 를 먼저 include 한 뒤 `mode_selector.h`:**

```
a77_p2.obj : error LNK2019: xpe_mode_selector_init, main 함수에서 참조되는 확인할 수 없는 외부 기호
a77_p2.exe : fatal error LNK1120: 1개의 확인할 수 없는 외부 참조입니다
```

**이쪽이 리더가 예측한 링크 실패다.** 컴파일은 통과하고 링크에서 죽는다 — 라이브러리에 구현이 없다는 뜻이고, **다른 어딘가에 구현이 있다는 대안 가설은 반증됐다.**

> **주의 — `PROBE1_EXIT=0` / `PROBE2_EXIT=0` 은 컴파일러 종료 코드가 아니다.** 스크립트에서 `cl` 출력을 `findstr` 로 걸렀기 때문에 `%errorlevel%` 은 `findstr` 의 것이다(패턴을 찾았으므로 0). **결론의 근거는 위 오류 원문이지 그 숫자가 아니다.** 숫자를 여기 밝혀 두는 이유는, 안 적으면 "종료 코드를 봤다"는 잘못된 인상이 남기 때문이다.

### C1 — (a) 대 (b): 나란히 놓고 읽었다

| 축 | 고아 `mode_selector.h` / `.cpp` | 살아 있는 `xpe_calib_mode.cpp` |
|---|---|---|
| CMake 등재 | **없음** | `CMakeLists.txt:35` (`SWU-1.12`, FUNC-031~033) |
| 모드 설정 | `int xpe_mode_selector_set(XpeCalibrationMode)` | `XpeErrorCode xpe_calib_set_mode(XpeCalibrationMode)` |
| 모드 조회 | `int xpe_mode_selector_get(XpeCalibrationMode* out)` | `XpeCalibrationMode xpe_calib_get_mode(void)` |
| 최대 차수 | `int xpe_mode_selector_get_max_degree(XpeCalibrationMode)` | `uint32_t xpe_calib_get_poly_degree(void)` |
| 최대 점수 | — | `uint32_t xpe_calib_get_max_points(void)` |
| 수명주기 | `init` / `shutdown` | 없음(정적 상태) |
| 변경 잠금 | `is_change_allowed` / `block_changes` / `allow_changes` | 없음 |
| AUTO 선택 | `XpeCalibrationMode xpe_mode_selector_auto_select(int num_points, float snr_db)` | **없음** — AUTO 는 `{10, 3}` 상수 |
| 품질 메타 | 없음 | `get/record/apply_quality_meta*` |
| 줄 수 | 130 + 176 | 276 |

**같은 개념의 두 판본이다** — 모드 설정/조회/차수라는 뼈대가 겹친다. 반환 규약(`int` 대 `XpeErrorCode`), 조회 형태(출력 인자 대 반환값), 접두사가 다를 뿐이다. 그래서 **(a) 로 읽는다.**

**다만 완전한 부분집합은 아니다.** 고아 쪽에만 있는 것이 둘:

- **`auto_select(num_points, snr_db)`** — SRS FUNC-031(5)의 AUTO 선택 로직. 살아 있는 쪽은 AUTO 를 `{10, 3}` 고정값으로 처리하고 SNR 을 보지 않는다
- **변경 잠금 3종** — FUNC-031 이 요구하지 않는 기능

즉 **지우면 "AUTO 를 SNR 로 고르는 코드"가 저장소에서 사라진다.** 컴파일된 적 없는 코드라 동작을 잃는 것은 아니지만, **의도의 유일한 기록**이긴 하다. 판정은 리더 몫이다.

### C4 · C5 — 센서스

**범위: 이 워크트리 전체. 제외: `build/`, 고아 짝(`mode_selector.h`/`.cpp`) 자신, 제 레인 보고서(`.moai/reports/lane-pre/`). 확장자: `.c .cpp .h .hpp .txt .md`.** 세는 형태는 `이름(` — 정의행이 아니라 호출행을 세기 위해서다(A-73 에서 파일명을 호출로 오독한 것의 교정).

```
xpe_mode_selector_init               = 0
xpe_mode_selector_shutdown           = 0
xpe_mode_selector_set                = 0
xpe_mode_selector_get                = 0
xpe_mode_selector_is_change_allowed  = 0
xpe_mode_selector_block_changes      = 0
xpe_mode_selector_allow_changes      = 0
xpe_mode_selector_get_max_degree     = 0
xpe_mode_selector_auto_select        = 0
--- control ---
xpe_calib_set_mode  = 16
xpe_calib_get_mode  = 14
```

**대조군이 0이 아니다** — 같은 명령·같은 범위에서 살아 있는 API 는 잡힌다. 그러므로 위의 9개 0은 "명령이 아무것도 못 잡는다"가 아니라 **실제로 아무도 부르지 않는다**는 뜻이다.

`mode_selector.h` 를 include 하는 곳:

```
./modules/preprocess/include/xpe/preprocess/mode_selector.h:2:  * @file mode_selector.h   ← Doxygen 태그, include 아님
./modules/preprocess/src/mode_selector.cpp:15:#include "xpe/preprocess/mode_selector.h"
```

**자기 짝 하나뿐이다.**

### C6 — SRS 가 죽은 쪽 이름을 쓴다

`XPE_CALIB_MODE_` 접두사 센서스(같은 범위):

```
19  ./modules/preprocess/src/mode_selector.cpp
 1  ./docs/calibration/SRS-CALIB-001_Software_Requirements_Specification.md
```

살아 있는 코드의 열거자는 `XPE_CALIB_*` 이고, `XPE_CALIB_MODE_*` 는 **고아 파일 안에서만** 산다 — `XPE_EXPORT` 와 같은 형태다. 그런데 **SRS FUNC-031 본문이 `XPE_CALIB_MODE_*` 쪽 표기를 쓴다.** 반대로 FUNC-031 이 지목하는 함수 이름(`xpe_calib_set_mode` / `xpe_calib_get_mode`)은 **살아 있는** 파일 쪽이다.

**한 요구사항 문단이 두 판본에서 절반씩 인용한다.** 고아를 지우면 SRS 의 열거자 표기가 어떤 코드와도 대응하지 않게 된다. `docs/` 는 제 소유가 아니라 고치지 않았다.

### BUILD_EXIT — 카드 8번

이 카드에서 저장소 파일을 바꾸지 않았으므로 빌드는 **변화 없음을 확인하는 용도**다.

```
ninja: no work to do.
PRE_BUILD_EXIT=0
ninja: no work to do.
COMMON_BUILD_EXIT=0
```

`no work to do` 는 **트리가 실제로 손대지지 않았다는 증거**이기도 하다. 반대로 말하면 **이 0은 고아 파일에 대해 아무것도 말해주지 않는다** — 애초에 빌드에 없는 파일이기 때문이다.

---

## 3. baseline 귀속 (Baseline-attribution)

- 컴파일/링크 오류는 **이번에 실행한** `cl` 출력 원문이다. 두 소비자 다 `/I modules\preprocess\include /I modules\common\include`, `/std:c++17`, 링크 대상 `build\ci-preprocess\lib\xpe_preprocess.lib`.
- 센서스 셋(9개 함수 / 대조군 2개 / `XPE_CALIB_MODE_`)은 **같은 턴에 같은 제외 규칙으로** 실행했다.
- 시그니처 표는 두 파일의 원문 grep 결과다.
- 줄 수는 `wc -l`.

**주의:** `xpe_calib_get_mode` 를 이전 턴에 15로 보고한 적이 있다. **이번 측정은 14다.** 차이는 제외 규칙(`.moai/reports/lane-pre/` 제외) 적용 여부로 보이지만 **그 차이를 따로 확인하지는 않았다.** 이 보고서의 수치는 위에 적은 명령의 이번 결과다.

---

## 4. 미검증 (Gaps)

- **삭제하지 않았다.** 카드가 판정을 리더에게 남겼으므로 파일은 그대로 있다. 따라서 "삭제 후 빌드 통과"는 이번에 측정하지 않았다.
- **176줄 구현의 논리를 검사하지 않았다.** 컴파일되지 않는 코드는 검사할 수 없다 — A-75/A-76 과 같다.
- **`auto_select` 의 알고리즘이 FUNC-031(5) 를 만족하는지 확인하지 않았다.** 이름과 인자가 그 요구사항과 대응한다는 것까지만 봤다.
- **git 히스토리를 보지 않았다.** A-76 에서는 `XPE_EXPORT` 때문에 필요했지만, 이번엔 컴파일 실패를 직접 측정했으므로 히스토리가 결론에 필요하지 않았다. "언제 왜 생겼는가"는 따라서 미검증이다.
- **`modules/preprocess/` 의 다른 `src/*.cpp` 가 CMake 에 다 올라 있는지 세지 않았다** — A-76 잔여 위험에 적었던 바로 그 항목이고, §5 의 제안 대상이다.
- **SRS 를 고치지 않았고, 다른 문서에 같은 혼선이 더 있는지 훑지 않았다.**

---

## 5. 제안 — CMake↔`src/*.cpp` 대조의 기계화 (제안만)

두 카드 연속으로 같은 모양이 나왔다(A-76 `simd_dispatch.cpp`, A-77 `mode_selector.cpp`). **눈으로 세면 다음번에도 놓친다.** 다만 `.github/workflows/` 와 루트 CMake 는 리더 소유이므로 만들지 않고 제안만 한다.

**무엇을 재는가.** `modules/<m>/src/` 의 `*.c`/`*.cpp` 집합과, 그 모듈 `CMakeLists.txt` 의 소스 목록에 실제로 오른 집합의 **차집합**:

- 디스크에 있는데 목록에 없음 → **이번 두 건의 형태** (컴파일된 적 없는 코드)
- 목록에 있는데 디스크에 없음 → 빌드가 즉시 깨지므로 CI 가 이미 잡는다

그러니 **한 방향만 보면 된다.**

**어디에 두는가 — 세 후보와 실제 차이:**

| 후보 | 언제 잡는가 | 대가 |
|---|---|---|
| **A. CMake 자기점검** (configure 때 `file(GLOB)` 로 `src/*.cpp` 를 훑어 목록과 대조, 차이가 있으면 `message(WARNING)`) | 개발자가 로컬에서 configure 하는 **즉시** | GLOB 는 configure 시점에만 돌아 파일 추가를 자동 감지하지 못한다 — 하지만 **여기서는 그게 문제가 아니다.** 빌드 대상을 GLOB 로 정하는 게 아니라 **검사에만** 쓰기 때문이다 |
| **B. CI 스텝** (별도 스크립트, PR 마다) | 머지 전 | 로컬에서는 안 잡힌다. 빨간불이 하나 더 늘고, 아무도 안 읽으면 소용없다(#153 형태) |
| **C. 독립 스크립트** (`tools/` 에 두고 사람이 부름) | 부를 때만 | 안 부르면 안 돈다. 사실상 없는 것과 같다 |

**A 를 권한다.** 이유는 속도가 아니라 **읽는 사람이 있다는 것**이다 — configure 출력은 빌드하는 사람이 반드시 지나가는 자리고, CI 빨간불과 달리 무시하려면 의식적으로 무시해야 한다. B 는 A 를 보완하는 자리지 대체하는 자리가 아니다.

**경고로 둘지 오류로 둘지.** 처음엔 **경고**를 권한다. 정당한 예외가 있을 수 있다(플랫폼 조건부 소스, 아직 안 쓰는 참조 구현). 경고가 한동안 0으로 유지되면 그때 `FATAL_ERROR` 로 올리는 게 순서다. 지금 바로 오류로 두면 첫 정당한 예외에서 검사 자체가 꺼진다.

**남는 구멍 — 이 검사가 잡지 못하는 것.** `mode_selector.h` 같은 **헤더**는 이 대조에 걸리지 않는다. 헤더는 CMake 소스 목록에 오르지 않는 게 정상이기 때문이다. 이번 건의 더 날카로운 신호는 오히려 **"공개 헤더인데 아무도 include 하지 않는다"** 쪽이었고, 그건 별개의 검사다. 함께 제안하지만 A 보다 뒤에 둘 것을 권한다 — 오탐(앞으로 쓰일 API 헤더)이 훨씬 많을 것이기 때문이다.

---

## 6. 잔여 위험 (Residual-risk)

- **판정 전까지 저장소에는 컴파일되지 않는 공개 헤더가 남아 있다.** `modules/preprocess/include/` 아래에 있으므로 이 모듈의 공개 표면으로 보인다. 소비자가 실수로 include 하면 §2 의 오류를 그대로 받는다.
- **지우면 `auto_select` 의 SNR 기반 AUTO 선택 의도가 사라진다.** 동작을 잃지는 않지만(애초에 컴파일된 적 없음) 기록은 잃는다. 리더가 그 의도를 살릴 생각이면 지우기 전에 옮겨 적을 자리가 필요하다.
- **SRS 와 코드의 열거자 표기 불일치는 이 카드로 해소되지 않는다.**
- **이 보고서는 제거를 실행하지 않았으므로, 제거가 빌드를 깨지 않는다는 것은 아직 측정되지 않았다.** §C4 의 0건이 강한 근거지만, 측정은 아니다.

---

Refs #160
