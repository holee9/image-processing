# QA-A-160 (`#186`) — **카드 전제가 코드와 다릅니다.** 없던 것은 6b·6c 였습니다

§2(1) 이 카드에서 가장 값이 컸습니다. *"있는 것을 다시 만들지 마십시오"* 라는 지시가 **구현 대상 자체를 바꿨습니다.**

---

## §2(1) 조사 — `:211` 은 스텁이 아닙니다

카드가 인용한 곳:

```cpp
// nonlinearity_correct.cpp:211-213
    (void)img;
    return XPE_OK;
}
```

**이것은 함수의 끝이 아니라 "LUT 가 없을 때" 갈래의 끝입니다.** 그 위에 실제 적용 코드가 있습니다:

```cpp
if (g_calib.nonlin_lut && g_calib.nonlin_entries > 0) {
    ...
    px[i] = lut[raw > last ? last : raw];   // 6a, 실제로 적용 중
    changed = true;
```

`panel.linear` 를 읽는 코드도 **이미 있습니다** (`xpe_json_get_string(configJsonOrNull, "panel.linear")`). 카드 §2(2) 가 인용한 *"주석 서술만 3건, 읽는 코드 없음"* 기록은 **현재 코드와 맞지 않습니다** — 그 사이에 QA-A-111·127·140 이 구현했습니다.

### 6a 는 end-to-end 로 살아 있습니다

| 조각 | 자리 |
|---|---|
| 생성 | `xpe_calib_generate_nonlin_lut.cpp:197` |
| 적재 / 해제 | `xpe_calib_load_nonlin_lut.cpp:32` · `:92` |
| 적용 | `nonlinearity_correct.cpp` LUT 갈래 |
| 파이프라인 배선 | `pipeline.cpp:177` |
| 시험 | `NonlinLutGenerateTest` 6건 + `NonlinApplyTest` 6건 |

### 그럼 없는 것은 — **6b(다항식)와 6c(선택 규칙)**

부재 확인 (범위: `modules/`, `tests/`, `clients/`, `gui/`):

| 토큰 | 건수 |
|---|---|
| `nonlinearity_mode` | **0** |
| `target_platform` | **0** |
| `nonlin_poly` | **0** |
| `panel.linear` | 22 ← **대조군** (같은 검색이 파일을 읽고 있음) |

그리고 파일 자신이 그 공백을 적어 두고 있었습니다:

> *"when EXT 6b (the global polynomial) arrives there will be two, and LUT-vs-polynomial has to be decided then — not inferred from whichever branch happens to come first."*

**그 자리가 이 카드입니다.**

---

## §1 구현 — 6b + 6c

### 6b 다항식 (`xpe_nonlinearity_apply_polynomial`)

- **Horner** 평가, 차수 4 (`FUNC-006` 상한 5)
- 계수는 **검출기 프로파일**에서 — `panel.nonlin_poly_c0`..`_c4`. `FUNC-006` 이 *"stored in calibration profile"* 라 했고, `panel.*` 를 읽는 기존 경로가 이미 있습니다. **숫자 다섯 개를 위해 새 파일 형식을 만들지 않았습니다** — 6b 의 대상이 MCU/FPGA 이고 거기서 피하려는 것이 128 KB 표입니다
- **단조성 강제** (6b 4·5단계): 위반 시 거부 → 6a 로 **폴백**
- 계수 전무·비유한 계수도 같은 거부 경로 — **영다항식으로 프레임을 0 으로 뭉개지 않습니다**

#### 단조성 검사를 "도함수 근" 대신 **정수 전수**로 한 이유

요구는 *"checking derivative root locations"* 라고 적었습니다. 정수 전수는 그것의 **근사가 아니라 같은 성질의 더 엄격한 읽기**입니다 — 입력이 `uint16` 화소이므로 **정수가 곧 정의역**입니다. 두 정수 사이에서 도함수가 잠깐 음이 되더라도 이 함수가 받을 어떤 값의 순서도 뒤집지 못하고, 두 정수의 순서를 실제로 뒤집는 다항식은 여기서 **정확히** 걸립니다. 비용은 호출당 최대 65,536회 Horner — 수백만 화소에 대해 한 번입니다.

### 6c 선택 규칙

```
panel.nonlinearity_mode == "POLY"                      -> 6b
                        == "AUTO" && target 이 MCU/FPGA -> 6b
그 외                                                   -> 6a
```

**이 결정은 우리가 하지 않습니다** — 요구가 이미 했고, 코드는 프로파일을 읽습니다. 파일에 남아 있던 "나중에 정해야 한다" 가 이것으로 닫힙니다. 알 수 없는 모드 문자열은 6a 로 흘려보냅니다(`QA-A-127` 이 하드코딩 목록을 없앤 것과 같은 형태).

### §2(3) 순서 — **이미 맞습니다**

`pipeline.cpp:177` nonlinearity → `:200` gain. *"linearize before normalize"* 대로입니다. 고칠 것이 없습니다.

---

## §3 반증 — 셋 모두

### (1) 보정을 끄면 잔차가 0.3% 를 **넘습니다**

런타임-거짓 가드로 적용 루프만 건너뛰게 했습니다:

```
worst_residual = 140.58144000000016  vs  budget 12.285
[  FAILED  ] NonlinPolyTest.PolynomialCorrectionMeetsTheZeroPointThreePercentResidual
```

**예산의 11.4배**입니다. 시험 입력에 실재하는 비선형성이 있다는 뜻이고, 그래서 통과가 무언가를 증명합니다. (`BUILD_EXIT=0`, DLL 18:02:30 갱신 확인)

같은 것을 시험 안에도 박아 두었습니다 — 모든 잔차 검사 앞에 **보정 전 격차가 같은 예산을 초과하는지** 를 `ASSERT_GT` 로 먼저 겁니다. 반증은 한 번이지만 이 단언은 앞으로도 남습니다.

### (2) `panel.linear` 양방향

프로브로 `panel.linear == "true"` 검사를 무시하게 하자:

```
[  FAILED  ] NonlinPolyTest.PanelLinearGovernsEnableAndDisable
```

`true` 쪽만 빨갛고 `false` 쪽은 통과 — 양방향이 각각 고정돼 있습니다.

### (3) LUT·다항식 **둘 다** 실행

| 경로 | 시험 | 결과 |
|---|---|---|
| 6b 다항식 | `NonlinPolyTest` **5건** (신규) | 통과 |
| 6a LUT | `NonlinApplyTest` 6건 + `NonlinLutGenerateTest` 6건 | 통과 |

같은 빌드에서 **29건 전부 초록**입니다. 그리고 6c 의 `AUTO` + `CPU` 사례가 **6b 를 건너뛰고 6a 로 떨어지는 것**을 반환값으로 고정합니다 — 제가 넣은 선택 블록이 6a 를 가로막지 않았다는 증거입니다.

---

## 신규 시험 5건

| 시험 | 고정하는 것 |
|---|---|
| `PolynomialCorrectionMeetsTheZeroPointThreePercentResidual` | 유일한 수치 합격선 + 입력이 비선형이라는 전제 |
| `NonMonotonePolynomialIsRejectedAndFallsBackToLut` | 6b 5단계 거부 **및 폴백이 실제로 일어남** |
| `AutoSelectsPolynomialOnlyForEmbeddedTargets` | 6c 양방향 (FPGA→6b, CPU→6a) |
| `PanelLinearGovernsEnableAndDisable` | `panel.linear` 양방향 |
| `PolyModeWithoutCoefficientsDoesNotZeroTheFrame` | 계수 없는 POLY 가 프레임을 0 으로 만들지 않음 |

기대값은 **Horner 의 두 번째 사본이 아닙니다** — 시험이 선형화 `f()` 를 먼저 정의하고 거기서 계수와 기대값을 **함께** 유도합니다. 평가 순서가 틀리거나 항이 빠지면 측정값만 움직입니다.

## 검증

| 항목 | 결과 |
|---|---|
| 빌드 | `BUILD_EXIT=0` |
| 전체 ctest | **774 / 774** (직전 769 + 신규 5) |
| 한 프로세스 네 순서 (기본 + seed 1/2/9) | 각각 `ran=705`, `PASSED 697`, 실패 0 |
| 깨진 기존 시험 | **0건** |

## 하지 않은 것 (카드 §4)

- 검증 지표(`xpe_verify_*`) — 손대지 않았습니다
- **새 이슈 만들지 않았습니다** — 아래는 보고입니다
- 범위 밖 정리·리팩터

## 보고 (이슈 아님, 리더 판단 대상)

1. **카드 §2(2) 가 인용한 `RTM-CALIB-001` §5b 기록이 현재 코드와 맞지 않습니다** — `panel.linear` 를 읽는 코드가 있습니다. 판정 보류 상태를 갱신할 자리로 보입니다.
2. **6b 의 "factory step" 은 이 모듈에 없습니다.** 계수를 적합하는 쪽(요구가 numpy.polyfit 을 지목)은 오프라인이고, `xpe_calib_generate_nonlin_lut` 에 대응하는 `generate_nonlin_poly` 가 없습니다. 이 카드는 **런타임(평가·단조성·선택)** 만 구현했습니다.
3. 파일 머리 주석이 여전히 *"Applies … 6a"* 라고만 적혀 있습니다 — 이제 6b·6c 도 합니다. 고치려다 범위 밖으로 두었습니다.

## 미검증 / 잔여 위험

- **계수의 출처가 프로파일 JSON 이라는 것은 제 설계 결정입니다.** `FUNC-006` 은 "calibration profile 에 저장" 이라고만 하고 키 이름을 정하지 않습니다. `.xcal` 파일로 갈 수도 있었고, 그 경우 `XCAL_TYPE_NONLIN_POLY` 와 적재 함수가 필요합니다 — 숫자 다섯 개와 MCU/FPGA 대상을 근거로 JSON 을 골랐습니다.
- **0.3% 잔차 시험은 평가 정확도만 잽니다.** 실제 검출기 곡선에 대한 적합 품질(6b 3단계의 "residual ≤ 0.5% across all measurement points")은 factory step 의 것이고 여기 없습니다.
- **`panel.adc_max` 기본값 65535 는 제 판단입니다.** 6c 표는 12비트를 전제하지만 버퍼가 `uint16` 이라 가장 넓은 값을 기본으로 두고 프로파일이 좁히게 했습니다.
- **실물 검출기 데이터로 재지 않았습니다.** 합성 다항식 픽스처입니다 — 6a 쪽 `NonlinApplyTest` 는 감마 1.35 모사를 쓰는데, 6b 는 계수를 줄 실물 프로파일이 없습니다.
- 단조성 검사는 `adc_max` **이하**만 봅니다. 그 위 값은 6a 와 같은 이유로 클램프되며 정확도를 주장하지 않습니다.

🗿 MoAI
