# QA-A-210c — 다항식 게인: 방향 무관 단조 + 최종 대체는 1차 최소제곱, 끝점 직선 폐기 (리더 결정 A, #233)

기준 커밋 `642503e8`. 증거: `evidence/` (빨강·초록 실행 전문, 반증, 실데이터 전/후 측정, 검증 배치). 판정 근거: QA-A-210(합성), QA-A-210b(실데이터 `cf03f5b9`).

## 1. 무엇을 바꿨나

`modules/preprocess/src/xpe_calib_generate_gain.cpp`

- `validate_monotonicity`: 곡선이 **비감소 또는 비증가**이면 통과(방향 무관). 표본 100점 위에서 위로 간 적도 아래로 간 적도 있으면 거부.
- 차수는 요청 차수에서 1차까지 내려가며 시도하고, **1차는 최소제곱 직선**이다. 직선은 방향과 무관하게 단조이므로 항상 통과 — 내려가기는 반드시 1차 최소제곱에서 끝난다.
- 끝점 직선 갈래 삭제. 도달할 수 없는 자리(정규방정식이 풀리지 않을 만큼 선량 간격이 붙은 경우)에는 `XPE_ERR_PROCESSING_FAILED` 를 돌려준다(공개 헤더에 이미 있는 반환 코드). 거짓 모델을 저장하지 않는다.
- 주석(`xpe_calib_generate_gain.cpp` 머리, `preprocess_api.h` 의 `xpe_calib_generate_gain_polynomial` 알고리즘 항목)을 새 정책에 맞췄다.

게이트(`calibration_pass`: R² ≥ 0.999), 알림 문구, 저장 형식은 바꾸지 않았다.

## 2. 시험 (`tests/test_gain_poly_policy.cpp`, 새 파일 + `gain_poly_real_crop.h` + `gain_poly_policy_golden.inc`)

- **실데이터 축소본**: cyan_test 의 16×16 크롭 5준위(게인 맵은 제품이 만든 값)를 헤더에 숫자로 넣었다. 원본 `.raw` 는 저장소에서 무시되므로(gitignore) 시험이 필요로 하는 값만 들어 있다. 출처와 선량은 헤더 머리말에 있다.
- 7개 시험(+ 골든 캡처용 `DISABLED_PrintGolden`):
  1. 실데이터·잡음 데이터에서 화소별 R² ≥ 0, 파일 R² ≥ 0
  2. 저장된 모든 다항식이 측정 선량 범위에서 단조(방향 무관)
  3. 화소별 저장 차수 == **이 파일 안에 독립적으로 쓴** 최소제곱 + 단조 탐색(long double 가우스-조던)의 결과(실데이터 크롭 전체, 최대 차수 2·3). 최대 차수 화소와 1차 화소가 모두 존재함을 대조군으로 단언
  3'. 직접 만든 화소 4개: 하강하는 볼록 곡선은 2차로 유지되고 R² > 0.999999, 오르다 내리는 값은 1차 **최소제곱** 직선(끝점 기울기와 다름을 대조군으로 단언), 직선 하강은 직선으로 남음
  3b. 옛 정책이 이미 2차로 채택하던 화소 20개의 저장값이 **비트 단위로 같다** (골든은 옛 생성기에서 `DISABLED_PrintGolden` 으로 캡처: `evidence/02_golden_capture_old_generator.txt`)
  4. 합성 사다리(`xpe_calib_fixture_gen --gain-poly --seed 7 32×32`)의 파일 R² ≥ 0 (QA-A-210 에서 -0.0766)

## 3. 증거

- **빨강 먼저**(`04_red.txt`, 옛 생성기): 7개 중 5개 실패(R² 시험 둘, 독립 탐색 일치, 하강 곡선 유지, 합성 사다리), 단조 시험과 비트 일치 시험은 옛 정책에서도 통과(끝점 직선은 방향이 하강일 뿐 단조이고, 골든은 옛 값이므로) — 이 둘은 변화가 **없어야 하는 것**을 고정하는 시험이다.
- **초록**(`06_green_policy.txt`, `60_verify_summary.txt`): 새 시험 7/7, DLL 810 실행 / 802 통과 / 8 건너뜀(기존과 같은 8), 셔플 동일, OOM 55, common 69 + 12, `ctest -N` 987(기준 979, +7 시험 +1 비활성 캡처 시험), 내보내기 차이 0, 헤더 문서 0건, 프리셋 12/12.
- **반증 3건**(한 번에 하나, 전체 빌드, 복원 후 `cmp`; `50_arms.txt`):

| 손상 | 빨개진 시험 |
|------|-------------|
| 끝점 직선 되살림(1차도 하강이면 거부 + 끝점 대체) | 화소별/파일 R² ≥ 0 시험 둘 |
| 방향 검사를 비감소로 되돌림(+ 끝점 대체) | 하강 곡선 유지, 두 R² 시험, 독립 탐색 일치, 합성 사다리 (5건) |
| 2차 이상의 하강 곡선만 다시 거부(1차는 방향 무관) | 하강 곡선 유지, 독립 탐색 일치 — "1차 채택 비율" 시험이 이 자리 |

## 4. 실데이터 전/후 (cyan_test 5준위, 3072×3072 = 9,437,184 화소, 선량 = 암전류 보정 평균 ADU)

전: `QA-A-210b/evidence/02_measure.txt`, 후: `evidence/11_measure_after.txt`(같은 스크립트 + 새 정책의 복제; **대조군**: 복제의 R² 가 제품 파일의 `fit_r_squared` 와 4.6e-10 이내로 일치).

| | 전 (2차) | 후 (2차) | 전 (3차) | 후 (3차) |
|---|---|---|---|---|
| 최대 차수 최소제곱 | 6.87% | **13.37%** | 1.65% (3차) + 5.78% (2차) | **3.17%** + **11.28%** |
| 1차 최소제곱 | 44.19% | **86.63%** | 43.63% | **85.56%** |
| 끝점 직선 | **48.94%** | **0%** | 48.94% | **0%** |
| 제품 파일 R² (`fit_r_squared`) | -0.0352 | **+0.2522** | -0.0347 | **+0.2534** |
| 화소별 R² < 0 | 33.2% | **1 화소 (0.00001%)** | 33.2% | 1 화소 |
| 화소별 R² 중앙값 | — | 0.188 | — | 0.189 |

- 화소별 R² < 0 인 1 화소는 -3.1e-15 (그 화소의 SS_tot 4.0e-5, SS_res 4.0e-5 — 기울기가 사실상 0 인 화소의 float32 반올림)다. 의미 있는 음수가 아니라 0 의 수치 잡음이다. 카드의 "0 이어야 함" 을 이 한 화소 때문에 문자 그대로 말하지 못한다. 시험은 `-1e-6` 허용으로 단언한다.
- **`calibration_pass`(R² ≥ 0.999)는 여전히 0 이다** — 후의 R² 가 +0.25 이고 게이트는 0.999. 이 데이터로 다항식 게인이 품질 게이트를 통과하지 못하는 것은 별개 문제이며 **게이트는 바꾸지 않았다**(리더가 #233 에 기록).
- 파일 번호를 선량으로 쓴 축: R² +0.2525, 끝점 0%, 화소별 R² < 0 은 0 화소.

## 5. 알림 문구 제안 (바꾸지 않음 — 레인 간 계약)

현재(`xpe_calib_generate_gain.cpp`): `XPE_WARN_CALIB_POOR_FIT: fit_r_squared=%.6f below %.3f (max_residual=%.3f%%, mean_residual=%.3f%%); increase dose levels or check detector stability`.

210 의 지적(입력이 평평하면 "선량 단계를 늘려라" 가 원인에 맞지 않는다)에 대한 제안 — **접두사와 숫자 필드는 그대로**(매칭하는 쪽이 깨지지 않게), 끝의 권고만 원인 중립적으로:

`XPE_WARN_CALIB_POOR_FIT: fit_r_squared=%.6f below %.3f (max_residual=%.3f%%, mean_residual=%.3f%%); the fitted curves explain little of the per-level variation -- if the per-level gain maps differ only by noise the polynomial adds nothing over one map; otherwise check the dose spacing, the number of levels and detector stability`

## 6. SRS-CALIB-FUNC-027 / RTM

`docs/calibration/SRS-CALIB-001_Software_Requirements_Specification.md:182` 에 이미 쓰여 있는 것: "Use least-squares regression", "polynomial must be monotone in [D_min, D_max]. If non-monotone, reduce degree until monotonicity achieved (minimum degree 1 = linear)". **방향 지정이 없고**("monotone") 최소 차수는 1차 선형이다 — 새 정책과 모순되지 않는다. 문서에 없는 것은 두 가지다:

1. "monotone" 이 비감소·비증가 **둘 다**를 뜻한다는 명시
2. 1차(선형)는 **최소제곱 직선**이며 내려가기가 거기서 끝난다는 명시(그래서 모든 화소가 자기 평균보다 나쁘지 않은 모델을 갖는다)

제안 문안(문서 수정은 리더): "…polynomial must be monotone in [D_min, D_max], in either direction (non-decreasing or non-increasing). If non-monotone, reduce degree until monotonicity is achieved; degree 1 is the least-squares straight line, which is monotone whichever way it points, so the reduction always ends there and no pixel is stored with a model worse than its own mean."

RTM-CALIB-001 의 FUNC-027 행(UT-GAIN-GEN-002~003)은 시험 ID 만 있고 내용이 없어 수정이 필요 없다. 새 시험(`GainPolyPolicyTest.*`)을 UT-GAIN-GEN 에 매핑할지는 리더 결정.

## 7. 미검증 (Gaps)

- 실데이터는 cyan_test 한 세트(5준위)뿐이다. CalData_6 은 선량이 없고 이 워크트리에 원본이 없어 쓰지 못했다. 준위 수·검출기가 다르면 비율은 달라질 수 있다.
- 시험의 실데이터는 16×16 크롭(256 화소)이다. 전체 9.4M 화소의 비율은 위 측정(스크립트)이 보여 주며, 시험은 그 비율이 아니라 "독립 탐색과 화소별로 일치" 를 단언한다.
- 게인 **적용**(`xpe_gain_correct`)이 이제 하강하는 다항식을 받는 경우의 동작은 이 카드에서 새로 측정하지 않았다. 기존 시험(`GainPolyDoseRange*`, `GainPolyNotApplied*` 등)은 모두 통과하고, 하강 폭은 데이터상 3e-3 수준이다.
- 단조 검사의 표본은 여전히 100점이다 — 그 사이의 국소 진동은 보지 못한다(옛 정책과 같은 한계).

## 8. 잔여 위험

- 이전에 끝점 직선으로 저장되던 화소가 이제 최소제곱 직선/곡선으로 저장되므로, 같은 입력으로 다시 만든 다항식 파일은 값이 달라진다. 이미 만들어 둔 파일은 그대로 읽히고 적용된다(읽기 경로는 바꾸지 않았다).
- 최소제곱 직선의 기울기가 데이터상 잡음 크기이므로 적용 결과가 "0 에 가까운 기울기" 가 된다. 이는 의도한 결과이지만, 그 결과가 화질에 미치는 영향은 측정하지 않았다.
