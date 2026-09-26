# QA-B-40 게이트 보고서 — B-39 코드 결함 4건 처리 (D2 · D3 · D5 · D1)

**카드**: QA-B-40 (#142 #133)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-40/`
**커밋 4건 (항목당 1)**: `78c51c5` D2 · `723d076` D3 · `7d560c9` D5 · `3a624fa` D1

---

## 1. 주장 (Claim)

| # | 주장 | 상태 |
|---|------|------|
| C1 | **D2** guard 충돌을 없애고, 둘 다 include 하는 TU 를 테스트로 고정했다 | PASS |
| C2 | D2 **반증**: 옛 헤더로 되돌리면 7개 중 **6개**가 컴파일 실패 | PASS |
| C3 | **D3** 미등록 소스 2개를 대조 후 제거했다 — 근거는 CMake 전수 grep + build.ninja 0건 | PASS |
| C4 | **D5** CRT 링크를 **실측**했다 — 공유 UCRT 확인, 코드 변경 없이 테스트로 고정 | PASS |
| C5 | **D1** 빈 이미지 계약을 통일했다 — RED 5건 → GREEN | PASS |
| C6 | 세 프리셋 재빌드 포함 **458 / 198 / 144**, 경고 0 | PASS |

---

## 2. D2 — include guard 충돌 (`78c51c5`)

### 2.1 실측한 관계

| 헤더 | guard | 내용 | include 하는 TU |
|---|---|---|---|
| `enhance_advanced_api.h` | `XPE_ENHANCE_ADVANCED_API_H` | version **1개만** 선언 | `collimation_detect.cpp` · `fractional_process.cpp` · `multiscale_process.cpp` |
| `xpe_enhance_advanced_api.h` | **같은 값** | API **7개** 선언 | `xpe_enhance_advanced.cpp` + 테스트 다수 |

하나가 다른 것을 include 하지 않았다 — **독립된 두 선언 집합**이 같은 guard 를 쓰고 있었다.

### 2.2 guard 만 바꾸지 않은 이유

guard 를 다르게 하면 충돌은 사라지지만 **선언이 두 곳에 갈라진 상태는 남는다.**
얇은 전달 헤더로 바꾸면 선언 집합이 하나가 되고, 어느 이름으로 include 하든 같은 것을 본다.
카드가 허용한 두 안 중 후자를 골랐다.

### 2.3 반증 — 이 항목의 실질 (`_d2_falsify.log`)

옛 헤더(version 만 선언 + 같은 guard)로 되돌리고 빌드했다:

```
test_header_guards.cpp(27): error C2065: 'xpe_enhance_advanced_init'
test_header_guards.cpp(28): error C2065: 'xpe_enhance_advanced_shutdown'
test_header_guards.cpp(30): error C2065: 'xpe_multiscale_process'
test_header_guards.cpp(31): error C2065: 'xpe_fractional_process'
test_header_guards.cpp(32): error C2065: 'xpe_detect_collimation'
test_header_guards.cpp(33): error C2065: 'xpe_calc_exposure_index'
```

**7개 중 6개가 사라졌고, 살아남은 하나가 `xpe_enhance_advanced_version` 이다** —
옛 헤더가 선언하던 유일한 심볼이다. 예측과 정확히 일치한다.
이 결과가 D2 를 "이론적 위험" 이 아니라 **관측된 손실**로 만든다.

### 2.4 왜 지금까지 사고가 없었나

두 헤더를 함께 include 하는 TU 가 **없었기 때문**이다(B-39 §7 이 적은 그대로).
`test_header_guards.cpp` 가 그 TU 를 처음 만든다 — 회귀하면 다운스트림이 아니라
여기서 멈춘다.

---

## 3. D3 — 미등록 중복 소스 (`723d076`)

### 3.1 "빌드에 없다" 의 근거 — B-31 의 오판을 피한다

B-31 에서 "루트가 `add_subdirectory` 하지 않는다 → 고아" 추론이 **틀렸다.**
이번에는 그 추론을 쓰지 않고 두 가지를 직접 봤다:

```
$ grep -rn "xpe_collimation_detect|enhance_advanced\.cpp" --include=CMakeLists.txt --include=*.cmake .
(두 파일명 0건 — 잡히는 두 줄은 살아 있는 xpe_enhance_advanced.cpp 다)

$ build.ninja 오브젝트 규칙:  ci-post 0 · ci-ai-b20 0 · ci-dicom 0
```

### 3.2 정의 대조표

| 파일 | 정의 | 살아 있는 대응물 | 고유한가 | 판정 |
|---|---|---|---|---|
| `enhance_advanced.cpp` (7줄) | `xpe_enhance_advanced_version` → **"0.1.0"** | `xpe_enhance_advanced.cpp:71` → **"1.0.0"** | 아니오 — 낡은 값 | 삭제 |
| `xpe_collimation_detect.cpp` (241줄) | `xpe_detect_collimation` | `collimation_detect.cpp:35` | 아래 4항목이 다르나 **전부 열등 또는 폐기 설계** | 삭제 |

`xpe_collimation_detect.cpp` 의 차이를 하나씩:

| 항목 | 죽은 쪽 | 살아 있는 쪽 | 판정 |
|---|---|---|---|
| 초기화 검사 | `version()` 문자열 길이로 대용 (`:110-113`) | `g_initialized` 플래그 | 죽은 쪽은 **리터럴이라 언제나 참** — NOT_INITIALIZED 가 발생하지 않는다 |
| Eigen Map | `Eigen::Map<const MatrixXf>` = **기본 column-major** (`:133`) | `RowMajor` 명시 | 데이터는 row-major다. 죽은 쪽은 **픽셀을 전치해 읽는다** |
| config 키 | `edge_threshold`/`confidence_threshold`/`theta_step`/`rho_step` | `sensitivity`/`min_area_ratio`/`border_margin` | 죽은 쪽 키를 참조하는 코드·테스트가 저장소에 **없다** |
| 파싱 실패 | 경고만, 기본값 진행 | `CONFIG_INVALID` 반환 | 살아 있는 쪽이 엄격 |

**이식할 것이 없다** — 고유 코드는 있으나 고유 *능력* 은 없다. 셋은 열등하고,
config 키는 대체된 설계다.

### 3.3 부수 발견 (보고만)

죽은 쪽 config 키가 **문서에는 남아 있다** — `docs/enhance-advanced/SAD-ENHANCE-ADV-001:545`
의 `"confidence_threshold": 0.7`. 문서가 삭제된 구현을 기술하고 있다.
`docs/` 는 main 소유라 목록만 낸다.

---

## 4. D5 — cross-DLL 할당/해제 (`7d560c9`)

### 4.1 실측 (`_d5_crt.log`)

B-39 는 이것을 **결함이라 단정하지 않았다** — 링크 설정을 확인하지 않았기 때문이다.
이번에 쟀다:

```
xpe_common.dll  -> MSVCP140.dll, VCRUNTIME140.dll, VCRUNTIME140_1.dll,
                   api-ms-win-crt-heap-l1-1-0.dll
xpe_display.dll -> VCRUNTIME140.dll, api-ms-win-crt-heap-l1-1-0.dll
```

둘 다 **공유 UCRT**(`api-ms-win-crt-heap-l1-1-0.dll`)를 쓴다. 힙이 하나이므로
`xpe_common` 의 `malloc` 을 `xpe_display` 가 `free` 하는 것은 지금 안전하다.
프로젝트 어디에도 `MSVC_RUNTIME_LIBRARY` 설정이 없어 CMake 의 MSVC 기본값
(MultiThreadedDLL, `/MD`)이 적용된 결과다.

### 4.2 그래서 코드를 바꾸지 않았다

**측정 결과가 "안전" 이면 고칠 것이 없다.** 카드도 "공유 CRT 면 사실을 주석에" 라고
갈랐다. 대신 두 가지를 남겼다:

- `presentation_lut.cpp:52` 의 `free` 지점에 실측 근거와 **"정적 CRT 로 바뀌면
  조용히 깨진다"** 는 조건
- `PresentationLutCrossDllTest`: `xpe_common` 할당 → `xpe_display` 가 변환하며
  해제·재할당 → `xpe_common` 해제. **한 케이스가 경계를 양방향으로 건넌다.**

실측은 시점 스냅숏이고 테스트가 상시 검사다.

---

## 5. D1 — 빈 이미지 계약 통일 (`3a624fa`)

### 5.1 RED (`_d1_red.log`)

```
ZeroWidthIsInvalidInputEverywhere  ***Failed
  xpe_log_transform / xpe_log_inverse / xpe_noise_reduce /
  xpe_contrast_enhance / xpe_edge_enhance  accepted width == 0
ZeroHeightIsInvalidInputEverywhere ***Failed  (같은 5개)
NullDataIsInvalidInputEverywhere      Passed
```

**예측한 5개 함수만 실패했다.** NULL data 는 기존 검증기가 이미 잡고 있었다.

### 5.2 여집합 케이스가 픽스처 오류를 잡았다

RED 1회차에서 `ValidImageStillAccepted` 도 실패했다. 원인은 구현이 아니라
**내 픽스처**였다 — 8×8 이 `contrast_enhance` 의 "타일 격자의 2배" 규칙에 걸렸다
(B-39 가 문서화한 동작). 32×32 로 고쳤다.

이 케이스를 넣지 않았다면 **"전부 거부하는 검증기" 도 앞의 세 케이스를 통과했을 것이다.**
여집합이 없는 계약 테스트는 반쪽이다.

### 5.3 구현 — 한 곳에 넣었다

```cpp
// enhance_basic_internal.h, validate_float32_image()
if (!img || !img->data) return XPE_ERR_INVALID_INPUT;
if (img->width == 0 || img->height == 0) return XPE_ERR_INVALID_INPUT;   // 추가
if (img->format != XPE_PIXEL_FLOAT32) return XPE_ERR_UNSUPPORTED_FORMAT;
```

함수마다 가드를 복사하면 다음에 또 갈라진다. **널 검사 뒤·format 검사 앞**에 놓았다 —
치수는 픽셀 타입이 아니라 서술자의 속성이므로, 빈 UINT16 버퍼는 "형식이 틀렸다" 가
아니라 "비었다" 로 보고되는 것이 옳다.

검증기가 잡게 되면서 **죽은 코드가 된 함수별 조기 반환 4개를 함께 지웠다**
(`noise_reduce:278`, `contrast_enhance:133`, `edge_enhance:75`, `noise_reduce:314`).

헤더 Doxygen 도 갱신했다 — B-39 가 적은 "zero-sized 는 XPE_OK" 서술이 이 커밋으로
거짓이 됐다. **하루 전에 내가 쓴 문서를 내가 무효화했다는 사실을 커밋에 적었다.**

---

## 6. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 이전 ctest | 451 / 198 / 144 | QA-B-39 `_verify.log` |
| 현재 ctest | **458 / 198 / 144** | `_verify.log` (= `_d1_green.log`, 세 프리셋 재빌드) |
| 증가 내역 | +2 (D2) +1 (D5) +4 (D1) = **+7** | 각 항목 로그 |
| D2 반증 | 7개 중 6개 컴파일 실패 | `_d2_falsify.log` |
| D3 근거 | CMake grep 0건, build.ninja 0건 × 3프리셋 | §3.1 |
| D5 실측 | 양쪽 모두 공유 UCRT | `_d5_crt.log` |
| 빌드 경고 | 0 | `grep -c "warning C" _verify.log` |

---

## 7. 미검증 (Gaps)

- **D2: `moai` 외 다른 소비자에서 두 헤더가 함께 쓰이는지 보지 않았다.**
  `clients/`·`gui/` 는 Lane C 소유라 grep 하지 않았다. 전달 헤더는 그쪽에도
  안전하지만, 실제로 쓰이는지는 관측 밖이다.
- **D3: 삭제 결과를 실험으로 확인하지 않았다.** 세 프리셋이 통과하는 것이
  간접 증거이지만, "지워도 아무것도 안 깨진다" 를 별도로 시험하지는 않았다
  (되돌리기 어려운 작업을 확인용으로 반복하지 않는다 — B-31 판단 승계).
- **D5: 정적 CRT 빌드를 실제로 만들어 실패를 보지 않았다.** "그러면 깨진다" 는
  링크 모델에서 유도한 것이지 관측이 아니다. 반증하려면 `/MT` 빌드가 필요한데
  프리셋 추가는 `cmake/` 소유 밖이다.
- **D1: 다른 모듈의 빈 이미지 계약은 손대지 않았다.** `display`·`dicom`·`ai`·`gsvg`
  가 같은 입력에 무엇을 돌려주는지 이번에 조사하지 않았다. 카드 범위가
  enhance_basic 이다.
- **D1: 성능 영향을 재지 않았다.** 검증기에 비교 2개가 늘었고 핫 경로이지만,
  루프 밖 1회이므로 무시할 수준으로 판단했다 — 측정은 아니다.

---

## 8. 잔여 위험 (Residual-risk)

- **D1 은 동작 변경이다.** 빈 이미지에 `XPE_OK` 를 기대하던 호출자가 있으면
  이제 `INVALID_INPUT` 을 받는다. 저장소 안에는 그런 호출자가 없다(458 전부 통과)
  — 하지만 **저장소 밖(C# 오케스트레이터·GUI)은 확인하지 않았다.** leader 가
  Lane C 에 알릴 사항이다.
- **D3 삭제는 revert 로 되돌릴 수 있다.** 다만 되살릴 이유가 생긴다면 그것은
  §3.2 의 열등한 구현이 필요해졌다는 뜻이 아니라, 삭제 판단이 틀렸다는 뜻이다.
- **D5 의 안전은 링크 설정에 달려 있다.** 누군가 `MSVC_RUNTIME_LIBRARY` 를 정적으로
  바꾸면 `PresentationLutCrossDllTest` 가 잡겠지만, **힙 손상은 테스트가 통과한
  뒤에도 잠복할 수 있다.** 테스트는 조기 경보이지 보증이 아니다.
- **D2 전달 헤더는 `enhance_advanced_api.h` 를 사실상 폐기 예정으로 만든다.**
  세 소스가 아직 그 이름을 쓴다. 정리는 별건이다.
- 커밋 4건은 push 전까지 미푸시다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` | vcvars + CMake/Ninja PATH |
| `_verify.bat` | 세 프리셋 재빌드 + 전체 ctest |
| `_d2.log` | D2 적용 후 ci-post 453, 새 케이스 2건 통과 |
| `_d2_falsify.log` | 옛 헤더 복원 시 7개 중 6개 컴파일 실패 |
| `_d5_crt.log` | `dumpbin /dependents` — 양쪽 모두 공유 UCRT |
| `_d5.log` | D5 적용 후 ci-post 454 |
| `_d1_red.log` | D1 구현 전 — 예측한 5함수만 실패 |
| `_d1_green.log` / `_verify.log` | 최종 458 / 198 / 144, 경고 0 |
