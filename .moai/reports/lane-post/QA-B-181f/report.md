# QA-B-181f — 비유한 화소: 조용한 rc=0 을 없앤다 (리더 결정, #233)

Refs #233

원칙(리더 결정): 모듈은 유한한 입력에서 비유한 출력을 내지 않고, 비유한 입력을 성공(rc=0)으로 처리하지 않는다. 0 으로 바꿔 넣지 않는다. 소비자는 진입 검사로 `XPE_ERR_INVALID_INPUT`, 생산자는 결과가 유한하지 않을 요청을 같은 코드로 거부하고, 거부 시 출력은 건드리지 않는다.

## 0. 먼저 읽을 것 (리더가 판단할 것 포함)

| 항목 | 결과 |
|------|------|
| 진입 검사 비용 | contrast 는 이미 있는 전역 min/max 패스에 합쳐서 **비용 0** (47.2 → 46.1 ms, 오차 안). 나머지는 화소 한 번 더 읽는 비용으로 **+1.7~2.1 ms** (3072²), 표는 §3. 이 값은 루프 안 가드 측정(+1.4 ms)보다 약간 크다. 카드의 "진입 검사가 더 비싸면 보고하고 멈춘다" 조건에 걸리는 곳이 있어 **숫자와 함께 올린다.** 지금 커밋은 로컬이며 푸시하지 않았다. 비용이 받아들일 수 없으면 되돌릴 수 있다. |
| 정상 입력 결과 | 변경 전 DLL 과 **비트 동일** — 9개 함수 중 해시를 낸 8개 모두 4회 전부 일치 (§4) |
| **결정이 필요한 기존 동작** | `xpe_fractional_process` 는 SPEC REQ-ADV-032 에 따라 비유한 입력 화소를 **0 으로 바꿔 쓰고 OK 를 반환한다** (`fractional_derivative.cpp:360-369`, 계산 중에도 비유한 값을 0 으로 바꿈 `:279-331`). 이번 원칙과 정면으로 충돌한다. 고치지 않았다 — SPEC 이 정한 것이라서다. |
| 생산자로 새로 찾은 것 | `xpe_apply_voi_lut` (LINEAR_EXACT, width 1e-30, 출력 범위 0 → `inf*0 = NaN`, 시험 빨강→초록), `xpe_contrast_enhance` (유한한 −3e38 / 3e38 의 범위가 ∞), `xpe_log_transform` (enormous normFactor), `xpe_apply_modality_lut` LINEAR (slope 1e38) |

## 1. 동작 변화 — 이제 오류를 받는 입력 (레인 간 계약)

모두 `XPE_ERR_INVALID_INPUT`, 입력 영상은 한 바이트도 바뀌지 않는다 (시험이 바이트 비교로 단언). 오류 코드를 `INVALID_INPUT` 하나로 한 이유: 호출자가 입력이나 파라미터를 고치면 해결되고, 모듈 내부 실패(`PROCESSING_FAILED`)가 아니다.

| 함수 | 이제 거부하는 입력 | 이전 |
|------|-------------------|------|
| `xpe_contrast_enhance` | NaN/±∞ 화소 · 유한하지만 `max−min` 이 float 을 넘는 영상 (−3e38 과 3e38) | rc=0, ∞ 한 화소가 영상 전체를 비유한으로 |
| `xpe_log_inverse` | NaN/±∞ 화소 · NaN/+∞ `normFactor` · 결과가 float 을 넘는 요청 (화소 100, normFactor 1: 약 38.5 초과) | rc=0, 한 화소가 +∞ → 다음 단계가 전체를 오염. `normFactor=+∞` 는 영상 전체를 0 으로 |
| `xpe_log_transform` | NaN/±∞ 화소 · NaN/+∞ `normFactor` · 결과가 float 을 넘는 요청 (normFactor 3e38) | rc=0 |
| `xpe_apply_modality_lut` | NaN/±∞ 화소 (TABLE·LINEAR) · LINEAR 의 NaN/∞ slope·intercept · 결과가 float 을 넘는 LINEAR (slope 1e38) | rc=0, TABLE 은 0 번 칸/끝 칸으로 포화 |
| `xpe_apply_presentation_lut` | NaN/±∞ 화소 | rc=0, NaN→0, +∞→맨 위 칸 |
| `xpe_apply_voi_lut` | NaN/±∞ 화소 · NaN/∞ `center`·`width`·`minOut`·`maxOut` · 출력 범위가 float 을 넘는 경우 (−3e38..3e38) | rc=0, 창 전체가 NaN |
| `xpe_detect_collimation` | NaN/±∞ 화소 | rc=0, +∞ 한 화소로 상자가 전체 영상 |
| `xpe_gsvg_process` (가상 그리드) | 설정 값(`vg_pyramid_gain` 1e308)에서 중간 결과가 비유한 → `XPE_ERR_CONFIG_INVALID`, 기존 거부 경로대로 원본 복원 | rc=0, 0 과 65535 가 섞인 쓰레기 출력 |

바뀐 값 하나: `xpe_apply_voi_lut` 에서 `minOut == maxOut` 이면 모든 화소를 `minOut` 으로 쓴다. 정확한 산술의 `clamp(…, minOut, minOut)` 결과와 같고 유한 입력의 기존 결과도 같다. 달라진 것은 `inf*0 = NaN` 이 나던 경우뿐이다.

**GUI/클라이언트 한 줄 확인** (`clients/ImageProcTest`, 이 세션에서 읽음): 공개 함수 호출은 `Native*Service` 가 한다. 디스플레이 내보내기(`NativePresentationExportService.AddRequiredStage`)는 OK 가 아닌 단계를 `InvalidOperationException("<stage> returned INVALID_INPUT: …")` 으로 던지고, 미리보기(`NativeEnhanceBasicPreviewService.CallStage`)는 단계 결과에 오류 코드 문자열을 기록한다. `CreateAutoVoi` 는 이미 비유한 화소를 건너뛰며 창을 계산한다. 그래서 비유한 화소가 있는 영상은 VOI 단계에서 **오류로 보이게 된다**(전에는 조용히 포화). `xpe_log_inverse` 를 부르는 클라이언트 코드는 찾지 못했다. 이 한 줄은 코드 읽기이며 GUI 를 실행하지 않았다.

## 2. 생산자 표 (카드 항목 1) — 유한 입력·유한 파라미터에서 비유한이 나올 수 있는가

| 공개 함수 | 가능? | 근거 | 조치 |
|-----------|-------|------|------|
| `xpe_log_inverse` | 가능 | 화소 100, normFactor 1 → +∞ (181e 프로브, 시험 빨강) | 거부 |
| `xpe_log_transform` | 가능 (enormous normFactor) | normFactor 3e38 × log10(최대 화소) 시험 빨강 | 거부 |
| `xpe_apply_modality_lut` LINEAR | 가능 | slope 1e38 × 화소 100 | 거부 |
| `xpe_apply_voi_lut` | 가능 | LINEAR_EXACT, width 1e-30, 범위 0, 화소 1e10 → NaN (시험 빨강 → 초록) | `range == 0` 이면 정의된 값을 직접 씀 |
| `xpe_contrast_enhance` | 가능 | 유한한 −3e38/3e38 → 범위 ∞ (시험 빨강) | 거부 |
| `xpe_gsvg_process` 가상 그리드 | 가능 | `vg_pyramid_gain` 1e308 (시험 빨강, rc=0 쓰레기 출력) | 거부 + 원본 복원 |
| `xpe_noise_reduce` (bilateral·NLM) | **측정했고 없음** | `sigma_range`/`h_param` 1, 1e-10, 1e-20, 1e-30, 1e-38 에서 rc=0 이고 출력 전부 유한 (각 10건, 두 모드) | 변경 없음, 회귀 방지 시험만 추가 |
| `xpe_gsvg_process` grid_dwt (`grid_dwt.cpp:511`) | 입력 계약상 도달 불가라고 **판단** | 입력이 uint16(유한), 단계는 유한 계수의 선형 연산과 `[0,1]` 안의 이득이며 `exp` 인자의 크기가 한정됨. 5개 극단 패턴(전부 0/65535/교대/무작위/줄무늬)의 출력이 0..65535 안임을 181e 에서 측정 | 변경 없음 |
| `xpe_edge_enhance` | 파라미터가 범위로 한정됨 (amount 0..5, radius 0.5..10) | 코드 읽기만, 극단 입력 측정 없음 | 변경 없음 |
| `xpe_apply_presentation_lut` | float 출력 없음 (uint16 으로 변환, 화소는 `[0,1]` 로 먼저 clamp) | 코드 읽기 | 소비자 검사만 |

## 3. 진입 검사 비용 (카드 항목 2)

`entry_scan_cost_summary.txt`, 3072² float32, 변경 전/후 DLL 을 **번갈아 4회**, 회마다 9회 중앙값, 그 중앙값들의 중앙값. 검사는 `std::isfinite` 가 아니라 지수부 비트 테스트다. `std::isfinite` 는 이 빌드에서 화소마다 CRT 호출로 컴파일되어 같은 영상에서 6.5 ms 가 나왔고 비트 테스트는 1.6 ms 였다(독립 측정, `/fp:fast`·`/fp:precise` 모두).

| 함수 | 전 (ms) | 후 (ms) | 차 | 비고 |
|------|--------|--------|----|------|
| `contrast_enhance` | 47.20 | 46.08 | −1.12 (오차) | 이미 있는 전역 min/max 패스에 비트 검사를 합침: 추가 메모리 패스 없음 |
| `log_transform` | 3.35 | 5.50 | +2.14 | 최대 화소를 알아야 해서 min/max 포함 스캔 |
| `log_inverse` | 3.17 | 5.11 | +1.94 | 〃 |
| `modality TABLE` | 16.81 | 18.70 | +1.89 | |
| `modality LINEAR` | 1.94 | 3.73 | +1.79 | 처음엔 float min/max 로 짜서 +8 ms 였다. 정수 키로 바꿔 벡터화. |
| `presentation_lut` | 21.46 | 23.12 | +1.66 | |
| `voi LINEAR_EXACT` | 2.62 | 4.47 | +1.85 | |
| `voi SIGMOID` | 35.35 | 38.84 | +3.49 | 오차가 큼(회마다 편차) |
| `detect_collimation` (1024²) | 32.39 | 34.08 | +1.69 | |

해석: 스캔은 화소를 한 번 더 읽는 비용이라 절댓값이 거의 일정(약 +1.7~2 ms)하다. 무거운 함수에서는 5~10% 이고 `log`·`voi LINEAR_EXACT`·`modality LINEAR` 처럼 원래 2~3 ms 인 함수에서는 +60~90% 이다. contrast 를 뺀 나머지는 181e 의 루프 안 가드 측정(+1.4 ms, 4회 모두 가드 쪽이 느림)보다 약간 크다. 루프 안 가드는 +∞ 범위 문제를 막지 못해 이 카드의 원칙(진입 검사)을 택했다. 이 비용이 허용되는지는 리더가 결정한다.

## 4. 증거

**빨강 → 초록** — 새 시험 3벌 + gsvg 1건.
- enhance_basic: `NonFinitePixels.*` 13건. 구현 전 11건(contrast 범위 오버플로 1건과 noise_reduce 회귀 방지 1건을 더하기 전) 중 8건 빨강, 정상 처리 대조군 3건 초록으로 확인했다. contrast 범위 오버플로 1건은 추가 시 빨강 → 수정 후 초록 (`g181f-range-red` 로그).
- display: `DisplayNonFinite.*` 12건. VOI NaN 생산 시험은 구현 전 빨강(`mode 1 width 1e-30 out 5..5 pixel 1e10 -> -nan(ind)`) → 수정 후 초록. 나머지 11건은 소스를 먼저 고친 뒤 시험을 써서 구현 전 빨강 기록이 없다. 그 대신 아래 반증이 빨강 증거다.
- enhance_advanced: `NonFinitePixelsCollimation.*` 3건.
- gsvg: `GsvgExceptionGuard.AVirtualGridWhoseResultIsNotFiniteIsRefusedAndTheOriginalIsRestored` 구현 전 빨강(rc=0, 출력 `65535, 65535, 0, 0, …`) → 초록.

**반증 — 검사를 하나씩 지우면 해당 시험이 빨강** (`arms_single_check_removed.txt`). enhance_basic 은 출하 `/fp:fast` 와 `/fp:precise` 둘 다 돌렸고, 나머지 모듈은 `/fp:fast` 를 쓰지 않으므로 출하 설정 한 번이다. 검사 18개 중 15개는 해당 시험만 빨강이다 (E1·E3~E8, D3~D8, A1, G1). 소스는 매번 원복했고 최종 바이트 일치를 단언했다.

**겹치는 검사 3개는 하나만 지워서는 빨강이 되지 않았다** — 정직하게 적는다.
- E2 (`log_transform` 의 normFactor 유한성), D1 (`modality LINEAR` 의 slope/intercept), D2 (`modality LINEAR` 의 화소 스캔)를 각각 지워도 시험이 초록이었다. 뒤의 오버플로 검사(`top`, `lo*slope+intercept`)가 NaN/∞ 도 잡기 때문이다. 즉 이중 방어다.
- 쌍으로 지우면 빨강이다 (`arms_pair_checks_removed.txt`): P1 (E2+E4) 두 모드 모두 2건 빨강, P2 (D1+D3) · P3 (D2+D3) · P4 (세 검사 모두) 빨강. 시험은 각 검사를 정말 겨누지만 **단독으로는 판별하지 못하는 중복 방어**다.
- 판단: 중복 방어를 지우지 않았다. 하나의 검사는 입력을 명시적으로 이름 붙여 거부하고(코드 읽기 쉬움), 다른 하나는 결과를 지킨다. `/fp:fast` 가 `<= 0` 으로 NaN 을 우연히 걸러내던 181e 의 문제와 달리, 이 이중 방어는 플래그에 의존하지 않는다.

**정상 입력 결과 비트 동일** (`entry_scan_cost_alternating.txt`, 변경 전 DLL 세트 `build/g181f-old-dll` ↔ 변경 후): contrast, log_transform, log_inverse, modality TABLE·LINEAR, presentation_lut, voi LINEAR_EXACT·SIGMOID 의 출력 FNV-1a 해시가 4회 전부 일치. 상자(detect_collimation)도 `(200,200)-(799,799)` 로 같다. 해시는 3072² 의 한 입력 패턴에서만 냈다.

**전체 시험**: `ctest --test-dir build/ci-post` (성능 이름 제외, `ctest_ci_post.txt`): `100% tests passed, 0 tests failed out of 1064`, 25건은 ONNX 의존이라 `Skipped` (ci-post 는 스텁 AI 빌드다 — 이것을 AI 동작 검증으로 읽지 말 것).

## 5. Gaps

- ci-ai(실 ONNX), ci-dicom, ci-preprocess 구성은 이번에 돌리지 않았다. dicom 은 이 카드가 건드리지 않은 모듈이다.
- `xpe_fractional_process` 의 0 대체(REQ-ADV-032), `xpe_multiscale_process`, `xpe_gsdf_calibrate` 이후 경로, `xpe_edge_enhance`·`xpe_exposure_index`·AI 모듈의 비유한 처리는 이 카드의 7곳 밖이라 **조사하지 않았다**. `fractional` 은 코드를 읽어 위 사실만 확인했다.
- `grid_dwt.cpp:511` 의 "도달 불가" 는 코드 읽기와 5개 극단 패턴 측정에 근거한 **판단**이고, 증명도 시험도 아니다.
- 비용은 한 기계의 한 입력(3072², 합성 무작위)이다. 실제 영상의 캐시 거동은 다를 수 있다. `voi SIGMOID` 의 +3.49 ms 는 회마다 편차가 커서 불확실하다.
- `log_inverse` 의 오버플로 판정은 최대 화소로 한다. 최대 화소에서 `exp` 가 float 안이면 나머지도 안이다(단조). 다만 SVML 벡터 `exp` 와 스칼라 `exp` 가 오버플로 경계에서 1 ulp 다를 수 있어, 경계에 정확히 걸린 한 값에서 판정과 루프가 어긋날 수 있다. 측정하지 않았다.
- GUI 는 코드를 읽었을 뿐 실행하지 않았다. 비유한 화소가 있던 미리보기·내보내기 시나리오가 E2E 에 있는지 모른다.
- 새 시험 파일명: `modules/enhance_basic/tests/test_nonfinite_pixels.cpp`, `modules/display/tests/test_nonfinite_pixels.cpp`, `modules/enhance_advanced/tests/test_nonfinite_pixels.cpp`, gsvg 는 기존 `test_exception_guard.cpp` 에 한 건.

## 6. Residual-risk

- 오류 코드가 `INVALID_INPUT` 하나라 호출자는 "화소가 문제인가, 파라미터와 값의 조합이 문제인가" 를 코드로 구별하지 못한다 (알림은 보내지 않았다).
- 비용: 위 §3. 허용 여부는 리더 결정.
- `std::isfinite` 를 쓰는 기존 코드(181d 의 파라미터 검사 등)는 그대로이고 그건 스칼라라 비용이 무시할 만하다. 다만 같은 의미의 검사가 두 방식으로 공존한다.
- 위 판단들은 MSVC 14.44, 한 CPU(AVX2)의 측정이다.
