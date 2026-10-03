# QA-B-207 M1 (D1·D3·D8) — display 입구 검사 (#251, 사용자 결정)

카드: `.moai/lanes/post/inbox/QA-B-207.md` M1 항목 2~4. C3 는 `report_m1_c3.md`(0144b98b).

## 0. 결과

| 항목 | 결과 |
|---|---|
| D1 | `xpe_gsdf_calibrate`: 검은 수준(첫 표본) < 0.05 cd/m² 이면 `XPE_ERR_INVALID_INPUT`, 출력 불변. 0.05 정확히는 받는다. 0 이하를 0.01 로 바꾸던 암묵 대체는 없앴다 |
| D3 | `xpe_apply_voi_lut` `LINEAR`: 창 폭 < 1 이면 `INVALID_INPUT`. 폭 = 1 은 받고 표준의 문턱(`center − 0.5`)을 단언. `LINEAR_EXACT`·`SIGMOID` 는 폭 > 0 그대로 |
| D8 | 세 모드 모두 `minOut >= maxOut` 이면 `INVALID_INPUT`, 영상 불변 |
| 시험 | `VoiWindowLimits` 6개, `GsdfBlackLevel` 4개, 기존 `GsdfCharacterization` 두 시험의 입력 갱신(§3) |
| 반증 | 14팔 중 13팔 터짐(그중 2팔은 첫 시도에 빌드되지 않아 다시 돌려 터짐) + 등가 변이 1개, 소스 복원 |
| 검증(관측) | ci-post ctest 1387 통과·0 실패(루트 `schemas/` 를 임시로 놓은 상태, AI 시험은 `XPE_AI_EXPECT_ONNX` 없이 건너뜀), doxygen 경고 0, 헤더 점검 20개 0건 |

## 1. DICOM 원문 인용 (카드의 Gap 처리)

PS3.3 C.11.2 VOI LUT Module(현행판, `dicom.nema.org/medical/dicom/current/output/chtml/part03/sect_C.11.2.html` 을 직접 읽음):
- **C.11.2.1.2** (기본 LINEAR): "Window Width (0028,1051) shall always be greater than or equal to 1."
  - 식: `if (x <= c − 0.5 − (w−1)/2) y = ymin; else if (x > c − 0.5 + (w−1)/2) y = ymax; else y = ((x − (c − 0.5)) / (w−1) + 0.5) * (ymax − ymin) + ymin`.
  - 주 2: "The value of 0 for w is expressly forbidden, and the value of 1 for w does not cause division by zero, since the continuous segment of the function will never be reached for that case."
  - 예: `c=2048, w=1` → "if (x <= 2047.5) then y = 0; else if (x > 2047.5) then y = 255; else /* not reached */". `c=0, w=1` → `x <= −0.5 → 0`, `x > −0.5 → 255`.
- **C.11.2.1.3.2** (LINEAR_EXACT): "Window Width (0028,1051) shall always be greater than 0."
- **C.11.2.1.3.1** (SIGMOID): "Window Width (0028,1051) shall always be greater than 0."
- 출력 범위(ymin, ymax)의 순서에 관한 문구는 이 절에 없다. D8 은 모듈 자체의 계약이다(사용자 결정).

구현이 `width == 1` 에서 내는 값은 이 예와 같다: 시험 `LinearAcceptsWidthOneAndGivesTheStandardsThresholdAtCenterMinusHalf` 가 2047.0·2047.4·2047.5(→ 0), 2047.6·2048·5000(→ 255), `c=0` 에서 −3·−0.5(→ 0), −0.4·0·3(→ 255) 를 단언한다.

GSDF 의 범위는 PS3.14 식 7-2(`j(L)`, `L = 0.05 … 4000 cd/m²`, JND 지수 1…1023)에서 왔다. 이번에 원문을 다시 열어 인용하지는 않았고 앞선 QA-B-145 의 확인과 이번 계산(`j(0.05)=1.03`, `j(4000)=1023.16`)에 근거한다(Gap).

## 2. 수정

- **D1** `presentation_lut.cpp`: 유한성·단조성 검사 뒤, 아무것도 쓰기 전에 `luminanceValues[0] < 0.05f` 이면 거부. `lum_min` 의 `<= 0 → 0.01` 대체는 삭제(`lum_max <= lum_min → lum_min + 1` 은 그대로, 카드 범위 밖). 표본은 오름차순이어야 하므로 첫 표본이 가장 낮은 값이다.
- **D3** `voi_lut.cpp`: 기존 `width <= 0` 거부 뒤에 `mode == LINEAR && width < 1` 거부.
- **D8** `voi_lut.cpp`: `!(minOut < maxOut)` 거부(같음 포함, NaN 은 앞의 유한 검사가 먼저). QA-B-181f 가 `minOut == maxOut` 에서 `inf * 0 = NaN` 을 피하려고 둔 특수 처리 블록은 이제 도달할 수 없어 지웠다(그 자리에 설명 주석).
- 헤더 `display_api.h`: `xpe_apply_voi_lut`(폭 한계, `minOut < maxOut`, 영상 불변)와 `xpe_gsdf_calibrate`(검은 수준 한계, 대체 문구 삭제, 상한 4000 미적용) 문서를 갱신.

## 3. 시험

새 시험(모두 수정 전 코드에서는 빨강일 입력, 반증 팔로 확인):
- `VoiWindowLimits`: `LINEAR` 폭 0.25·0.5·0.999·**1보다 작은 가장 큰 float(0.99999994)**·1e-6 거부 + 영상 불변; 폭 1 의 문턱 단언; 폭 2 의 표준 램프(0.25 단위 값); `LINEAR_EXACT`·`SIGMOID` 는 0.25·0.5·0.999 를 계속 받고 0·음수는 거부(제한이 새지 않음); 출력 창 (1,0)·(5,5)·(255,0)·(−1,−2) 를 세 함수가 거부하고 영상 불변 + 같은 호출의 오름차순 창은 통과(통제); 아주 작은 양의 범위(0..1e-3)는 여전히 창.
- `GsdfBlackLevel`: 0·−1·−0.001·0.001·0.01·0.03·0.049·0.05 바로 아래 float 거부 + 출력 파라미터 불변(0xABCD·`gsdfEnabled=7` 센티널); 0.05·0.0500001·0.1·1·5 허용; 첫 표본만 검은 수준(평평한 시작 0.05 는 허용, 0.04 로 시작하는 곡선 거부); 상한 6000 cd/m² 는 막히지 않음(기록).
- 기존 시험의 입력 갱신 2건: `GsdfCharacterization.Stage2_LuminanceSweepMovesTheCurve_155` 의 "6 decades 0.01..10000" → "5 decades 0.05..10000", `Stage2_LutIsNoLongerTheStraightRamp_155` 의 "0.01..10000" → "0.05..10000". 검은 수준 0.01 은 이제 거부되기 때문이고, 두 시험이 지키는 성질(LUT 이 직선 램프가 아님)은 그대로다.
- 기존 시험 중 `minOut == maxOut` 이 `XPE_OK` 라고 단언하던 것은 없었다(전체 ctest 초록).

## 4. 반증 (`arms_d.py.txt`, `arms_d_out.txt`)

| 끈 방어 | 결과 |
|---|---|
| D3 검사 제거 | 폭 < 1 거부 시험 빨강 |
| D3 가 폭 1 도 거부(`<=`) | 폭 1 문턱 시험 빨강 |
| D3 를 모든 모드에 적용 | `LINEAR_EXACT`·`SIGMOID` 시험 빨강 |
| D3 한계를 0.5 로 | 거부 시험 빨강(0.999 와 0.99999994 가 받아짐) |
| D8 검사 제거 | D8 시험 빨강 |
| D8 가 같은 창을 허용(`>`) | D8 시험 빨강 |
| D8 을 `LINEAR` 에 적용하지 않음 | D8 시험 빨강 |
| D8 이 1 미만의 범위도 거부 | 작은 양의 범위 시험 빨강 |
| D1 검사 제거 | 검은 수준 시험 빨강(재실행) |
| D1 한계를 0.06 으로 | 0.05 허용 시험 빨강 |
| D1 이 정확히 0.05 도 거부(`<=`) | 0.05 허용 시험과 기존 2점 곡선 시험 빨강 |
| D1 이 음수만 거부 | 검은 수준 시험 빨강(재실행) |
| D1 이 마지막 표본을 판정 | 검은 수준 시험·첫 표본 시험 빨강 |
| D1 이 둘째 표본도 판정 | 빨간 시험 없음 — **등가 변이**: 둘째 표본은 첫 표본 이상이어야(비감소 검사가 앞서 거부) 하므로 첫 표본이 0.05 이상이면 둘째도 0.05 이상이다 |

D1 의 두 팔(검사 제거, 음수만 거부)은 첫 실행에서 `kMinBlackLevel` 이 쓰이지 않아 빌드되지 않았다(경고=오류). 상수를 계속 참조하게 바꿔 다시 돌렸다(`arms_d_out.txt` 끝 블록).

## 5. D1 상한 측정 (보고서 전용, 카드 지시)

`d1_upper_end_measurement.txt`: 검은 수준 0.1 cd/m², 감마 2.2 곡선 64점, 기준은 PS3.14 식 7-2 를 이분법으로 뒤집은 LUT(표준 다항식을 범위 밖으로 외삽).

| 흰색 cd/m² | rc | 모듈과 기준의 최대 차 | 300 초과 항목 |
|---|---|---|---|
| 500 | OK | 5 | 0 |
| 1000 | OK | 7 | 0 |
| 4000 | OK | 18 | 0 |
| 4500 | OK | 41 | 0 |
| 6000 | OK | 155 | 0 |
| 10000 | OK | 744 | 36 |

`j(4000)=1023.16`, `j(6000)=1084.84`, `j(10000)=1160.20`(표준 범위는 j = 1023 까지). 상한 쪽은 하한보다 훨씬 완만하게 어긋난다(6000 에서 155 카운트, 10000 에서 744 카운트에 36개 항목이 300 초과). 지금은 막지 않는다. 막는다면 같은 이유(표준 범위 밖)이고 기준은 4000 이다. 이 측정은 "모듈의 값이 표준 다항식의 외삽과 다른 정도"이며 외삽 자체가 표준이 정한 값은 아니다.

## 6. Gap / 잔여 위험
Gap
- PS3.14 의 범위(0.05–4000 cd/m²)를 이번에 원문에서 다시 인용하지 않았다. PS3.3 은 직접 인용했다.
- gui·clients 가 `minOut >= maxOut`, `LINEAR` 폭 < 1, 검은 수준 < 0.05 를 보내는지 확인하지 않았다(gui 가 `LINEAR` 폭을 어디서 정하는지, 보정 입력이 어디서 오는지는 읽지 않았다).
- 영상 출력 비교(16비트 소비자)는 하지 않았다: 이 항목들은 영상 값을 바꾸지 않고 입력을 거부하는 쪽이다.

잔여 위험
- 이전에 받아들이던 `minOut == maxOut`(영상 전체가 상수)이나 `LINEAR` 폭 0.x 가 이제 `INVALID_INPUT` 이다(사용자 결정, 의도한 변경).
- 어두운 방의 디스플레이 측정이 0.05 cd/m² 아래로 나오면 보정이 거부된다. 그 입력을 올려 쓸지(0.05 로 고정)는 사용자 결정이 "거부"였다.
