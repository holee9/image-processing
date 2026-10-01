# QA-A-27 검증 보고서 — 죽은 소스 처분 (#112) + `/WX` 잔여 현상 실측 (#104 현상 2)

- 레인: Lane A (pre) / 브랜치 `dev/preprocess`
- 카드: QA-A-27 (`.moai/lanes/pre/inbox/QA-A-27.md`, 2026-09-10 3항 추가 포함)

## 1. 주장 (Claim)

1. **#112**: `modules/preprocess/src/xpe_preprocess.cpp`(287줄) 의 정의 7개를 살아 있는 소스와 전수 대조했다. **이식이 필요한 것은 0건** — 삭제했다.
2. **#104 현상 2 는 이미 해소돼 있었다.** 고의 경고 프로브가 **빌드를 깨뜨렸다**(C2220). `xpe_target_warnings_as_errors()` 는 `xpe_preprocess`(`:79`)와 `xpe_preprocess_tests`(`:251`) 양쪽에 **이미 붙어 있다**. 붙일 것이 없었다.
3. **3항**: 고아 트리 `tests/preprocess/`·`tests/preprocess_smoke/`(3파일 1,040줄, 55케이스)를 대조했다. 고유 단언 1건을 이식하고 삭제했다.
4. 총 **1,424줄 삭제**. 재실측 ci-preprocess 498 → **500/500 PASS**, ci-common **69/69 PASS**.

**단, 3항에서 되찾지 못한 커버리지가 있다 — §5 첫 항목. 새 카드가 필요하다.**

## 2. #112 심볼 대조표

`xpe_preprocess.cpp` 는 `modules/preprocess/CMakeLists.txt` 어디에도 없다(실측: `grep "src/xpe_preprocess.cpp"` → 일치 0). 어떤 TU 로도 컴파일되지 않는다.

| 죽은 파일의 정의 | 살아 있는 정의 | 이식 필요 |
|---|---|---|
| `xpe_preprocess_is_initialized` (`:52`) | `preprocess.cpp:34` | 불필요 — QA-A-20 이 이미 옮김 |
| `xpe_preprocess_version` (`:60`) | `preprocess.cpp:39` | 불필요 |
| `xpe_preprocess_init` (`:75`) | `preprocess.cpp:44` | 불필요 |
| `xpe_preprocess_shutdown` (`:137`) | `preprocess.cpp:71` | 불필요 |
| `xpe_defect_detect_runtime` (`:175`) | `runtime_detection.cpp:119` | 불필요 — 아래 참조 |
| `xpe_preprocess_get_param_range` (`:247`) | `preprocess.cpp:82` | 불필요 |
| `copy_c_string` (`:42`, file-static) | 없음 | 불필요 — 파일 내부 헬퍼, 외부에서 참조 불가 |

**`xpe_defect_detect_runtime` 은 살아 있는 쪽이 엄격히 더 낫다.** 죽은 버전은 전역 평균 ± 3σ·선량 계수라는 단순 문턱을 쓰고, 살아 있는 버전(`runtime_detection.cpp:119-182`)은 `xpe_pixel_count` / `xpe_required_bytes` 경계 검사, 설정 파싱·검증, 국소 창 기반 `DetectDefectivePixel` 을 수행한다. 게다가 죽은 버전은 **자기모순**이다 — 출력 포맷을 `XPE_PIXEL_UINT16` 로 검사해 놓고 `memset(defect_map, 0, width*height)` 로 uint8 크기만큼 지우고 `uint8_t*` 로 쓴다(`:196-204`). 옮길 가치가 없다.

## 3. #104 현상 2 프로브

### 프로브 (`a27-wx-probe-before.log`, exit=1)

`test_sigma_clip_conformance.cpp` 에 미사용 지역변수 1건을 고의로 넣고 빌드:

```
test_sigma_clip_conformance.cpp(87): error C2220: 경고를 오류로 처리합니다.
test_sigma_clip_conformance.cpp(87): warning C4189: 'unusedLocal': 지역 변수가 초기화되었지만 참조되지 않았습니다.
test_sigma_clip_conformance.cpp(87): warning C4505: '`anonymous-namespace'::a27_wx_probe': 참조되지 않은 함수가 제거되었습니다
```

**빌드가 깨졌다.** `/WX` 는 Lane A 범위에서 동작한다. 프로브는 같은 턴에 원복했다(현재 트리에 `a27_wx_probe` 없음).

### 배선 확인 (`a27-wx-wiring.txt`)

```
79:xpe_target_warnings_as_errors(xpe_preprocess)
251:    xpe_target_warnings_as_errors(xpe_preprocess_tests)
```

생성된 `build.ninja` 의 컴파일 플래그에도 `/WX` 가 들어 있다(93개 FLAGS 행 전부):

```
FLAGS = ... /W4 /utf-8 /WX /wd4251 /wd4275
```

`ci-preprocess` 프리셋이 `XPE_WARNINGS_AS_ERRORS: ON` 이므로 헬퍼가 실제로 `/WX` 를 붙인다.

**즉 카드의 "안 붙어 있으면 붙인다" 는 실행할 것이 없었고, "#104 현상 2 가 지금도 참인가" 의 답은 거짓이다.** 언제 붙었는지는 추적하지 않았다(§5).

## 4. 3항 고아 트리 대조표

`tests/preprocess/` 와 `tests/preprocess_smoke/` 는 **컴파일 불가능한 상태**였다. 세 파일 모두 `#include "xpe/preprocess/xpe_preprocess_api.h"` 를 쓰는데 **그 헤더는 존재하지 않는다**(`modules/preprocess/include/xpe/preprocess/` 에는 `mode_selector.h`, `xcal_format.h`, `xpe_preprocess_internal.h` 뿐). 등록되지 않은 정도가 아니라 빌드가 불가능하다.

| 고아 파일 | 케이스 | 판정 | 근거 |
|---|---|---|---|
| `test_gain_correction.cpp` | 12 (AC-GAIN-001~005) | **전부 덮임** | `test_gain_correct_reciprocal_fma.cpp`(22케이스)가 같은 AC 라벨·ULP 헬퍼·NaN/Inf 케이스를 갖는 후속본이다(QA-A-25 에서 현행 ABI 로 이관). `test_gain_correct.cpp`(10) + `test_gain_correct_avx2_parity.cpp`(2)가 나머지를 덮는다 |
| `test_preprocess_smoke.cpp` | 21 | **20 덮임 / 1 이식** | version·init·shutdown·동시 init·재초기화는 `test_xpe_preprocess_init.cpp`(18케이스)가 덮는다. 널 인자는 QA-A-14 의 `test_error_precedence.cpp` 가 덮는다. **고유**: 6개 파라미터 이름을 개별 조회하는 단언 — 살아 있는 쪽(`test_xpe_preprocess.cpp:1577`)은 유효 조회 1건 + 널 인자만 본다 |
| `test_runtime_detection.cpp` | 24 | **덮이지 않음 — 이식하지 못함** | 아래 |

### 이식한 것

`modules/preprocess/tests/test_param_range_names.cpp`(신규 2케이스): `preprocess.cpp:20-25` 의 6개 이름(`integration_time_ms`, `temperature_c`, `kVp`, `mAs`, `SID_mm`, `pixelPitch_mm`)을 각각 조회해 `XPE_OK` 와 `min < max` 를 확인하고, 알 수 없는 이름은 거부되는지 본다. 이름 6개는 실측으로 현재 구현 테이블과 대조했다.

### 이식하지 못한 것 — `test_runtime_detection.cpp` 24케이스

이 파일은 **은퇴한 시그니처**로 작성돼 있다: `xpe_defect_detect_runtime(img, defectMap, configJson)` — 현행은 `(image, metadata, defect_map_output)` 로 **인자 순서와 의미가 다르다**. 게다가 존재하지 않는 헤더를 포함한다. 옮기는 것이 아니라 **다시 쓰는 작업**이고, QA-A-25 한 장 분량이다.

살아 있는 쪽의 runtime detection 커버리지는 `test_runtime_detection_avx2_parity.cpp` **4케이스(전부 AVX2 대 스칼라 동등성)** 뿐이다. 고아 파일이 갖고 있던 **기능 커버리지** — Hampel 필터 창 크기 3종, MAD 계산, 슬라이딩 윈도 중앙값의 outlier 내성, 저노이즈/고노이즈 결함 검출, 가장자리 픽셀, 소·대 이미지 — 는 **어디에도 없다.**

파일을 남겨두는 선택지는 없었다. 컴파일되지 않는 파일은 아무것도 지키지 못하며, 이 카드가 처분하라고 지정한 바로 그 죽은 트리다. 그래서 삭제하되 **잃은 것을 §5에 크게 남긴다.**

## 5. 미검증 (Gaps)

- **[가장 큰 항목] runtime detection 기능 커버리지가 0이 되었다.** 위 24케이스를 현행 API 로 다시 작성하는 새 카드가 필요하다. 지금 남은 것은 AVX2 동등성 4케이스뿐인데, 이것은 "두 경로가 같은 답을 낸다" 만 보장하고 **그 답이 옳은지는 보지 않는다.** 두 경로가 함께 틀려도 통과한다.
- **`/WX` 가 언제 preprocess 타깃에 붙었는지 추적하지 않았다.** 현재 붙어 있다는 것만 실측했다. #104 현상 2 를 닫으려면 어느 커밋이 해소했는지 확인하는 편이 낫지만, 이 카드의 질문("지금도 참인가")에는 답했다.
- **`/WX` 프로브는 테스트 타깃 1개만 검증했다.** `xpe_preprocess` 라이브러리 타깃에 경고를 넣어 보지는 않았다. 배선(`:79`)과 `build.ninja` 플래그로 확인했을 뿐이다.
- **죽은 파일의 동작을 실행해 비교하지 않았다.** `xpe_defect_detect_runtime` 두 구현의 우열은 **소스 판독**으로 판단했다. 죽은 쪽은 컴파일되지 않으므로 실행 비교 자체가 불가능하다.
- **고아 gain 스위트 12케이스를 단언 단위로 일일이 대조하지는 않았다.** AC 라벨(AC-GAIN-001~005)이 살아 있는 후속본과 일치하고 그 후속본이 22케이스로 더 넓다는 것을 근거로 삼았다. 케이스 하나하나를 맞춰 본 것은 아니다.
- **`tests/` 에 남은 트리는 여전히 조사하지 않았다** — `ai_tests`, `e2e_post_pipeline`, `test_data`. A-13 에서와 같은 미검증 항목이 남아 있다.

## 6. 잔여 위험 (Residual risk)

- **runtime detection 이 조용히 틀릴 수 있다.** 기능 커버리지가 사라진 지금, Hampel 창이나 MAD 계산이 깨져도 AVX2 동등성 테스트는 통과한다. 이것이 이 카드가 만든 가장 큰 위험이며 새 카드로 닫아야 한다.
- **죽은 코드 삭제는 되돌릴 수 있지만 아무도 그러지 않을 것이다.** `git rm` 이라 이력에는 남는다. 다만 `xpe_preprocess.cpp` 의 선량 의존 문턱(`mAs` 를 곱한 3σ) 아이디어는 살아 있는 구현에 없다 — 설계 의도였는지 습작이었는지는 확인하지 않았다.
- **`/WX` 가 켜져 있다는 것은 앞으로의 경고가 곧 빌드 실패라는 뜻이다.** 이 카드에서는 이득이지만, 컴파일러 업그레이드가 새 경고를 들고 오면 Lane A 빌드가 먼저 멈춘다.
- **이식한 파라미터 이름 테스트는 이름 문자열에 의존한다.** `preprocess.cpp:20-25` 의 테이블을 바꾸면 이 테스트가 먼저 깨진다 — 의도한 바지만, 이름 변경을 계획하는 쪽에서는 실패 이유를 오해할 수 있다.

## 7. 증거 (Evidence)

### 재실측

```
100% tests passed, 0 tests failed out of 500     (a27-pre.log,    exit=0)
100% tests passed, 0 tests failed out of 69      (a27-common.log, exit=0)
```

### 삭제 규모 (`a27-diffstat.txt`)

```
 modules/preprocess/src/xpe_preprocess.cpp        | 287 --------------
 tests/preprocess/CMakeLists.txt                  |  87 -----
 tests/preprocess/test_gain_correction.cpp        | 407 --------------------
 tests/preprocess/test_runtime_detection.cpp      | 468 -----------------------
 tests/preprocess_smoke/CMakeLists.txt            |  10 -
 tests/preprocess_smoke/test_preprocess_smoke.cpp | 165 --------
 6 files changed, 1424 deletions(-)
 modules/preprocess/CMakeLists.txt | 2 ++
```

## 8. baseline 귀속

| 대상 | 직전 baseline | 이번 실측 | 차이 |
|---|---|---|---|
| ci-preprocess | 498/498 PASS (QA-A-26, `../QA-A-26/a26-green.log`) | **500/500 PASS** (`a27-pre.log`) | +2 = 이식한 파라미터 이름 케이스 |
| ci-common | 69/69 PASS (QA-A-24, `../QA-A-24/a24-common.log`) | **69/69 PASS** (`a27-common.log`) | 변화 없음 |

삭제한 1,424줄은 어떤 빌드에도 들어간 적이 없으므로 테스트 수를 줄이지 않는다 — 실측이 그 예상과 일치한다.

## Card Cross-Check

| 마일스톤 | 카드 |
|---|---|
| #112 심볼 대조표 + 죽은 파일 삭제 | QA-A-27 |
| #104 현상 2 프로브 (깨짐 확인) + 배선 확인 | QA-A-27 |
| 고아 트리 3파일 대조 + 고유 단언 이식 + 삭제 | QA-A-27 |
| ci-preprocess / ci-common 재실측 | QA-A-27 |
| **runtime detection 기능 커버리지 24케이스 재작성** | **신규 카드 필요 (최우선)** |
| `tests/ai_tests`·`e2e_post_pipeline` 처분 | 신규 카드 필요 |

## 인용 로그 (같은 디렉터리)

| 파일 | 내용 |
|---|---|
| `a27-wx-probe-before.log` | 고의 경고 프로브 — C2220 으로 빌드 실패 (exit=1) |
| `a27-wx-wiring.txt` | `xpe_target_warnings_as_errors` 호출 2행 |
| `a27-pre.log` | ci-preprocess 500/500 PASS (exit=0) |
| `a27-common.log` | ci-common 69/69 PASS (exit=0) |
| `a27-diffstat.txt` | 삭제·추가 `git diff --stat` |
