# QA-A-224 — 완벽한 보정을 실패로 판정하는 결함 수정 + 근거 없는 문턱 "잠정" 표시 + 낡은 헤더 정정 (Refs #242 #216)

기준: dev/preprocess `c59263db`(QA-A-223) 위. 코드 `xpe_verify_metrics.cpp`, 헤더 `preprocess_api.h`, 주석 `xpe_defect_gen.cpp`, 시험 `test_verify_metrics.cpp`. 증거: `evidence/`. 푸시하지 않음.

## 결론

1. **결함 수정**: 보정 후 PRNU 가 정확히 0 인 완벽한 보정이 `overall_pass = false` 였다. 이제 통과한다(실제 DLL: `snr_improvement_db` 0.0 → 200.0, `overall_pass` False → True).
2. **`xpe_verify_pipeline` 의 같은 경로**: 판정은 이미 맞았지만 필드에 `inf`·`NaN`·`-inf` 가 새고 있었다. 이제 유한한 수다. **판정은 하나도 바뀌지 않았다**(프로브 12행 중 값이 바뀐 4행 외 8행 동일, 바뀐 4행도 판정은 G1 만 달라졌다).
3. **#242 문턱 셋은 값을 그대로** 두고 "잠정 — 요구 근거 없음" 을 코드 상수 옆과 헤더에 적었다. 같은 양에 문턱이 둘(3.0 / 2.0)이라는 사실도 거기에 있다.
4. **낡은 헤더 2곳**(`generate_nonlin_lut` 상단 항등 문구, `bpm_generate` 의 FUNC-024/025 라벨)을 고쳤다. 같은 낡은 라벨이 `xpe_defect_gen.cpp` 주석 5곳에도 있어 함께 고쳤다.

## 1. 결함과 수정

### 1.1 무엇이 틀렸나 (`evidence/10`)

`xpe_verify_gain` 은 `prnu_before > 0 && prnu_after > 0` 일 때만 개선량을 dB 로 계산하고 아니면 `0.0` 이었다. 보정 후 영상이 정확히 평평하면 `prnu_after = 0` 이라 개선량이 0.0 이 되어 3 dB 선(`snr_improved`)을 못 넘고 불합격했다. 같은 패널에 1e-6 잡음이 남아 있으면 94 dB 로 통과했다.

수정 전 실제 DLL(64×64, 게인 맵 1.0):

| 사례 | 수정 전 | 수정 후 |
|---|---|---|
| G1 5% → **정확히 평탄** | `snr_improvement_db` 0.0, **`overall_pass` False** | **200.0, True** |
| G2 5% → 0.0001% 잡음 | 94.05, True | 94.05, True |
| G3 5% → 0.5% 잡음 | 19.74, True | 19.74, True |
| G4 5% → 5% (개선 없음) | 0.03, False | 0.03, False |
| G5 균일 → 균일 | 0.0, True | 0.0, True |
| G6 균일 → 5% 잡음 (악화) | 0.0, False | 0.0, False |
| P1 pipeline 5% → 평탄 | **inf**, True | **173.91**, True |
| P2~P4 pipeline 통제 | 94.06 / 19.75 / 0.03 | 같음 |
| P5 pipeline 균일 → 균일 | **nan**, False | **0.0**, False |
| P6 pipeline 균일 → 5% 잡음 | **-inf**, False | **-173.91**, False |

`evidence/10`(수정 전)과 `evidence/13`(수정 후)을 줄 단위로 비교하면 달라진 행은 G1·P1·P5·P6 넷뿐이고, **`overall_pass` 가 달라진 행은 G1 하나**다.

### 1.2 수정 내용 (`xpe_verify_metrics.cpp`)

- `xpe_verify_gain`: `prnu_before > 0` 이고 `prnu_after == 0.0` 이면 `snr_improvement_db = ZERO_SPREAD_DB(200.0)`. 판정식은 손대지 않았다(`snr_improved`·`prnu_improved`·`flat_residual_ok` 모두 이 값으로 참).
- `xpe_verify_pipeline`: 새 도우미 `snr_db(mean, std)` — 평균이 0 이하(또는 NaN)면 0.0(기존과 같음), **표준편차가 정확히 0 이면 200.0**, 아니면 `20·log10(mean/std)`(기존과 같음). 나눗셈이 0 으로 나뉘는 경로가 사라졌다.

### 1.3 필드에 무엇을 넣나 (리더가 정하라 한 것)

**무한대 대신 200 dB 를 "보고값"으로 넣는다.** 상한(cap)이라 부르지 않는다. 이유와 한계:

- 200 은 판정과 무관한 **보고 관례**다. 어느 문턱도 이 값을 읽지 않는다(문턱은 3.0·2.0). 완벽한 보정은 개선이 무한대이므로 어느 문턱이든 넘는다.
- **상한이 아니다.** 0 이 아닌 잔차가 이론상 더 큰 값을 낼 수 있다: 3072² 영상에서 한 화소가 float32 한 단위 어긋난 경우 약 214 dB(계산이며 측정하지 않았다). 이 경우 완벽한 보정(200)보다 큰 값이 나와 **순서가 뒤집힌다**. 둘 다 문턱을 넘으므로 판정은 같고, 이 뒤집힘은 이론상의 경계다.
- 처음에는 `±200` 클램프도 넣었으나 **뺐다.** 유한한 비율의 로그는 항상 유한하므로 `inf` 는 표준편차 0 에서만 나오고(그건 위에서 처리), 클램프는 어떤 입력으로도 시험할 수 없는 죽은 코드였다. 반증이 터지지 않을 코드를 두고 "방어"라 부르지 않기로 했다.
- `xpe_verify_pipeline` 에서는 필드가 `SNR_final − SNR_raw` 라 평탄 프레임이 SNR 200 으로 세어져 **필드 값 자체는 200 이 아니다**: 5% 잡음 → 평탄이 173.91 = 200 − 26.09(raw SNR). 헤더 필드 문서에 그렇게 적었다.
- `XpeCalibrationMetrics` 의 레이아웃은 그대로다(ABI 잠금 정적 단언 통과, 필드 이름도 그대로).

### 1.4 `xpe_verify_pipeline` 의 같은 경로를 확인한 결과

| 입력 | 판정(전 → 후) | 필드(전 → 후) |
|---|---|---|
| 5% 잡음 → 평탄 | 통과 → 통과 | inf → 173.9 |
| 평탄 → 평탄 | 실패 → 실패 | nan → 0.0 |
| 평탄 → 5% 잡음 | 실패 → 실패 | -inf → -173.9 |

판정은 그대로 두었다. **평탄 → 평탄이 "통과가 아닌" 것은 지금까지의 판정이고 이번에 고정한 것이다**(개선된 것이 없다). 다만 `measured_mask` 는 이 경우에도 SNR 비트가 켜져 있다(측정됐다고 표시): FUNC-036 의 "측정 불가" 에 해당하는지는 이 카드에서 판단하지 않았다(§5).

## 2. 시험 (신규 8건, `test_verify_metrics.cpp`)

| 시험 | 역할 | 수정 전 |
|---|---|---|
| `VerifyGain_PerfectCorrectionPasses` | 보정 후 정확히 평탄 → 통과, 필드 유한·≥ 3 dB (전제 대조: `prnu_before > 4`, `prnu_after == 0`) | **빨강** |
| `VerifyGain_TinyResidualNoiseStillPasses` | 통제: 1e-6 잡음 → 통과(94 dB, 지금처럼) | 초록 |
| `VerifyGain_NoImprovementStillFails` | 통제: 개선 없음 → 실패 | 초록 |
| `VerifyPipeline_PerfectFinalPassesWithAFiniteField` | 평탄 최종 → 통과, 필드 유한 | **빨강**(`got inf`) |
| `VerifyPipeline_TwoFlatFramesReportAFiniteValueAndDoNotPass` | 평탄 → 평탄: 유한, 통과 아님 | **빨강**(`got -nan(ind)`) |
| `VerifyPipeline_FlatRawMadeNoisyReportsAFiniteValueAndFails` | 평탄 → 잡음: 유한·음수, 실패 | **빨강**(`got -inf`) |
| `VerifyPipeline_NoImprovementStillFails` | 통제: 개선 없음 → 실패 | 초록 |
| `VerifyPipeline_ANanPixelIsNotReadAsAPerfectFrame` | 평탄 최종에 NaN 화소 하나 → 통과 아님 | (아래 반증 r3) |

수정 전 실행(`evidence/11`, 시험 7건 시점): 새 시험 중 **정확히 4건 빨강**(위 표), 통제 3건 초록. 수정 후 `*Verify*` 40건 전부 통과(`evidence/12`).

### 반증 (한 번에 하나, 전체 빌드, `*Verify*` 실행; `evidence/30_arm_*`; 마지막에 복원 + 다시 빌드)

| 손상 | 빨강이 된 시험 |
|---|---|
| r1 gain 의 0 경로가 다시 0.0 을 보고 | `VerifyGain_PerfectCorrectionPasses` **하나만** |
| r2 pipeline 의 "표준편차 정확히 0" 규칙 제거 | pipeline 셋(`PerfectFinal…`·`TwoFlatFrames…`·`FlatRawMadeNoisy…`) |
| r3 pipeline 의 `== 0.0` 을 `!(std > 0)` 로(NaN 을 완벽으로 읽음) | `VerifyPipeline_ANanPixelIsNotReadAsAPerfectFrame` **하나만** |

**r3 는 처음에 터지지 않았다.** 내가 처음 고른 시험 입력은 최종 영상에 `+inf` 화소 하나였는데, 이 함수는 중심을 중앙값으로 잡아 `+inf` 가 표준편차를 NaN 이 아니라 `+inf` 로 만들고(→ `mean/std = 0` → `-inf`), 두 규칙이 같은 결과(실패)를 냈다(`evidence/14` 로 측정: `+inf` 화소 → 필드 `-inf`·실패; NaN 화소 → 필드 `nan`·실패). 반증이 안 터진 것을 "규칙이 중복" 이라 적지 않고 입력을 의심해 `evidence/14` 의 프로브로 "중심은 유한, 퍼짐은 NaN" 이 되는 입력(평탄 영상 속 NaN 화소 하나)을 찾아 시험 입력을 NaN 으로 바꿨고, 그러자 r3 가 그 시험 하나만 정확히 빨갛게 만들었다. (`evidence/30_arm_r3…` 는 바꾼 시험의 실행이다. 처음 실행의 "40건 통과" 는 위 설명으로 대체한다.)

**반증이 없는 것**: gain 의 `prnu_after == 0.0` 을 `<= 0.0` 이나 `!(> 0)` 로 바꿔도 시험이 안 빨개진다 — NaN 잔차에서는 `prnu_improved`(`after < before`)가 거짓이라 어차피 실패하기 때문이다. 그래서 gain 쪽의 `== 0.0` 선택은 **다른 검사가 가려 주는 것**이지 이 시험이 지키는 것이 아니다. pipeline 쪽은 가려 주는 것이 없어 r3 와 시험이 지킨다.

## 3. "잠정 — 요구 근거 없음" 표시 (리더 결정)

문턱 **값은 하나도 바꾸지 않았다**(3.0·0.99·2.0, 게이트 제거·병합 없음).

- `xpe_verify_metrics.cpp` 상수 블록: `PROVISIONAL -- NO REQUIREMENT BASIS (QA-A-223, #242)` 절 신설 — 도입 커밋 `b6c19b8a`(문서 변경 0), 값은 한 번도 안 바뀜, `docs/`·`.moai/specs`·`.moai/project` 를 값과 개념으로 검색해 요구 없음, **일부러 유지**(게이트를 빼면 판정이 느슨해지고 되돌리기 어렵다)한다고 적었다. `PRNU_IMPROVE_MIN_DB`·`GAIN_COVERAGE_MIN`·`SNR_IMPROVE_MIN_DB` 각각에 "PROVISIONAL" 을 달았다.
- 같은 양에 문턱이 둘: `PRNU_IMPROVE_MIN_DB` 옆 주석에 "gain 의 `snr_improvement_db` 는 PRNU 개선(dB)이고 `xpe_verify_pipeline` 의 `SNR_final − SNR_raw` 와 같은 양(둘 다 CV 의 dB), 선이 3.0 과 2.0 으로 다르며 간격의 이유는 기록 없음".
- 낡았던 주석 정정: 옛 "Also misnamed: it is compared against snr_improvement_db (:348), not PRNU" 를 "이름이 맞다: gain 안에서 그 값은 PRNU 개선" 으로 바꿨다(QA-A-223 §1.3).
- `GAIN_COVERAGE_MIN`: "유효" = 유한하고 0 초과, 제품의 `[0.1, 10.0]`(SRS-CALIB-FUNC-002, REQ-P1A-011)보다 넓다고 주석에.
- 헤더 `xpe_verify_gain`·`xpe_verify_pipeline` 문서에 같은 취지의 `@note` 를, 구조체 필드 `snr_improvement_db` 문서에 두 함수가 같은 양을 채우고 200 이 보고 관례임을 적었다.
- 코드 안 `xpe_verify_pipeline` 의 판정 줄 위 기존 "NO REQUIREMENT BASIS" 주석 끝에 "PROVISIONAL … 같은 양이 gain 에서 3.0" 을 더했다.
- **`docs/`(SRS·api-spec 등)에는 표시하지 않았다** — 리더 소유다. 표시할 문장은 위 코드 주석을 그대로 쓸 수 있다.

## 4. 낡은 헤더 정정 (주석만)

- **`xpe_calib_generate_nonlin_lut`**: 헤더가 아직 "boundary conditions `LUT[0]=0` and `LUT[ADC_max]=ADC_max`" 와 "cannot reach the identity endpoint" 를 적었으나, SRS 정정(2026-09-18, #186)이 상단 항등을 폐기했고 코드(`xpe_calib_generate_nonlin_lut.cpp:263`, `:238`)도 상단 매듭을 만들지 않는다. 헤더를 코드의 실제 동작으로 바꿨다: `LUT[0]=0` 만, 상단 매듭 없음, 측정 최댓값 위는 마지막 측정 구간의 할선으로 이어 65535 에서 포화, 연장 시작은 파일에 `xcal_nonlin_extension_start` 로 기록. 오류 사유도 코드의 실제 조건(측정이 엄밀 증가가 아님, 최솟값 0 이하, 최댓값이 `lut_entries - 1` 이상, 적합이 유한하지 않거나 비감소가 아닌 표)으로 바꿨다.
- **`xpe_bpm_generate`**: 헤더가 "FUNC-024: BPM Merging", "FUNC-025: Reflect Padding" 이라 적었으나 SRS·RTM(`:131-132`)에서 FUNC-024 는 게인 보정의 프레임 수 등급, FUNC-025 는 BPM 보정 뒤 `LineArtifactScore` 한계이고 SAD(`:285-286`)도 병합·반사로 쓰지 않는다. 라벨을 "구현 동작, SRS 문장 없음" 으로 바꾸고 `SRS-CALIB-FUNC-022 and -023` 만 검출 매개변수의 근거로 남겼다. 헤더의 두 곳(Phase 12 머리말 블록, 함수 문서)을 고쳤다.
- **같은 라벨이 `xpe_defect_gen.cpp` 주석 5곳에도 있어 함께 고쳤다**(파일 머리말 `SPEC:` 줄과 알고리즘 목록, 반사 패딩 함수, 병합 함수, API 줄). 주석만이다.
- 병합 방식 서술은 건드리지 않았다: 헤더는 `max(dark, bright)` 로, 소스 함수 주석은 "logical OR" 로 적고 있어 **서로 다른 표현이다**. 같은 동작인지 이 카드에서 확인하지 않았다(§5).
- `tests/test_defect_gen.cpp:5` 의 머리말 주석("FUNC-022, FUNC-023, FUNC-024, FUNC-025")은 같은 낡은 번호인데 이 카드가 시험 주석까지는 손대지 않았다.

## 5. 미검증 (Gaps)

- **병합 표현**: `bpm_generate` 의 `max` 와 `logical OR` 가 같은 결과인지(값 0/1/2/3 규약 포함) 확인하지 않았다.
- **FUNC-036 과 `xpe_verify_pipeline`**: 평탄 → 평탄에서 `measured_mask` 의 SNR 비트가 켜져 있다. "측정 불가" 로 보고 비트를 끌지는 판단하지 않았다(판정은 그대로).
- **비유한 화소**: 최종 영상의 `+inf` 화소는 필드를 `-inf`, NaN 화소는 `nan` 으로 만든다(`evidence/14`; 판정은 둘 다 실패). 이번 보장("유한 입력에 대해 유한")의 밖이라 그대로 두었다. `verify_*` 가 비유한 입력을 입구에서 거절할지는 별도 결정(다른 단계는 거절 원칙을 따른다).
- 214 dB 는 계산이고 측정하지 않았다. 시험 영상은 32×32 뿐이다(프로브는 64×64).
- gain 쪽 `== 0.0` 선택은 `prnu_improved` 가 가려 주는 것이라 시험으로 고정되지 않았다(§2).
- `docs/` 표시는 하지 않았다(리더).

## 6. 검증 (`evidence/verify/`, 반증 뒤 다시 빌드)

빌드 `BUILD_EXIT=0`, preprocess 시험 **910**(기존 902 + 신규 8) 중 902 통과·8 건너뜀(기존), 셔플 902 동일, 할당 실패 시험 63(변화 없음), common 69·12, ctest 총 **1095**(1087 + 8), 공개 export 변화 0, 헤더 문서 0건, 프리셋 일치 12/12.

## 7. 잔여 위험

- 200 은 관례이므로 이 필드를 소비하는 쪽(GUI·보고서)이 "200 = 무한대" 를 모르고 숫자로 그린다. 소비자 쪽 확인은 이 카드 밖이다.
- 문턱 셋이 "잠정" 으로 남아 있어 판정 근거는 여전히 없다(#242 가 열려 있다).
