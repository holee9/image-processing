# QA-B-185b — 185 의 Codex 보류 2건: 헤더의 과잉 보장과 Info 문구의 식 (#235)

## 요약

| 항목 | 결과 |
|------|------|
| 1 (중간) 헤더가 읽기→쓰기 뒤 "표시 보존"을 보장 | Codex 지적이 맞았다. 헤더 문구를 "극성이 보존된다"로 좁히고 Writer 가 Window·Rescale·Presentation LUT Shape 를 **보존하지 않는다**고 읽기 문서와 쓰기 문서 양쪽에 명시했다. 그 손실을 고정하는 시험을 추가했다 (현재 동작의 기록이며 요구가 아니라고 이름·주석에 적었다) |
| 2 (낮음) Info 알림의 식이 실제 계산과 다름 | 맞았다. 구현은 `mask − (px & mask)` 인데 문구는 `(2^BitsStored − 1) − value` 였다. 정확한 식으로 문구를 바꿨고 시험이 문구 **전체**를 고정한다 |
| 반증 | 6가지 약화 각각 해당 시험만 빨강 (§3) |
| 회귀 | 리더 시험 96/96, `ci-dicom` ctest 243개 중 실패 0 |
| 문서 초안 | 제한 한 문장을 넣어 다시 제출 (§4) |

## 정정 (QA-B-185 보고서·시험)

- 185 보고서 요약표의 "왕복 … 복사본이 원본과 **같게 표시된다**" 는 과한 표현이었다. 시험의 대조는 **VOI 가 항등이고 Rescale 이 1/0 인 픽스처**에서 극성이 같다는 것이었다. 시험 이름을 `…RoundTripKeepsHowTheImageDisplays` → `…RoundTripKeepsThePolarity` 로 고치고 주석·실패 메시지에 "항등 VOI, Rescale 1/0" 조건을 적었다.
- 185 헤더 문장 "reading a MONOCHROME1 file and writing the result keeps how the image displays" 는 같은 오류였고 이 카드에서 바꿨다.

## 1. 헤더 (`dicom_api.h`)

읽기 문서(`xpe_dicom_read_image`)의 MONOCHROME1 항목이 이제 이렇게 말한다.

- **살아남는 것은 극성이다**: MONOCHROME1 파일을 읽어 쓰면 반전된 워드를 가진 MONOCHROME2 파일이 되고, 원본의 극성과 같다.
- **살아남지 않는 것**: `xpe_dicom_write` 는 소스 파일의 Window Center/Width(쓰지 않음), Rescale Slope/Intercept(1 과 0 을 씀), Presentation LUT Shape(IDENTITY 를 씀)를 **보존하지 않는다**. 아무것도 소스 파일에서 복사하지 않는다. 원본에 비항등 VOI 나 Rescale 이 있으면 읽기→쓰기 뒤 밝기·대비가 달라진다 (PS3.3 C.11.2).
- "현재 동작의 기록이며 요구가 아니다."

쓰기 문서(`xpe_dicom_write`)에도 한 항목을 더했다: 데이터셋은 `img` 와 `meta` 만으로 만들어지며 PI 는 항상 MONOCHROME2, 모양은 IDENTITY, Rescale 은 1/0, Window 는 쓰지 않는다. 읽기 문서를 보지 않고 쓰기 문서만 읽는 호출자도 알 수 있게 하려는 것이다.

### 고정 시험

`Tc109_CurrentBehaviour_WriteDoesNotCarryWindowRescaleOrPresentationShapeFromTheSourceFile`

1. 소스 파일에 MONOCHROME1 + Presentation LUT Shape `INVERSE` + Window Center `1500` / Width `800` + Rescale Slope `2` / Intercept `-1024` 를 넣고 **대조**로 실제로 들어 있음을 먼저 확인한다.
2. 읽고(반전) `xpe_dicom_write` 로 쓴다.
3. 새 파일: Window Center·Width **없음**, Rescale Slope `1` / Intercept `0`, Presentation LUT Shape `IDENTITY`, PI `MONOCHROME2`.

이름과 주석에 "RECORDS CURRENT BEHAVIOUR, IS NOT A REQUIREMENT — 나중에 Writer 가 이것을 보존하기로 하면 이 시험을 바꾼다" 를 적었다.

## 2. Info 알림 문구 — 레인 간 계약 변경

| | 문구 |
|---|------|
| 185 | `MONOCHROME1 pixel values were inverted to MONOCHROME2 sense: value = (2^BitsStored - 1) - value, BitsStored <B>` |
| **185b (현재)** | `MONOCHROME1 pixel values were inverted to MONOCHROME2 sense: value = (2^BitsStored - 1) - (stored & (2^BitsStored - 1)), BitsStored <B>` |

위 비트가 켜진 워드에서 옛 식은 틀렸다(예: BitsStored 12 의 워드 `0xF123` 은 옛 식이면 `4095 − 0xF123` 이지만 실제 결과는 `4095 − 0x123`). 새 식은 구현과 같다. 식을 빼는 대안도 있었으나 호출자가 계산 방식을 문구에서 바로 읽을 수 있도록 식을 남겼다.

시험: `Mono1AlertText(B)` 로 문구 전체를 만들어 네 경로(비압축 16·12비트, JPEG LL, JPEG 2000) 모두에서 알림 텍스트와 **문자열 전체가 같음**을 단언한다. 헤더에도 같은 문구를 적었다(`<B>` 는 버퍼의 `bitsStored`). `clients/`·`gui/` 가 이 문구를 매칭하는지는 확인하지 않았다 (185 와 동일).

## 3. 반증 (`arms.txt`)

| 약화 | 빨강이 된 것 |
|------|--------------|
| T1 알림 문구를 185 의 옛 문구로 되돌림 | 알림 문구 전체를 단언하는 `Tc109_FrDcm109_…OnEveryPath` 만 |
| T2 Writer 가 Window Center 를 씀 | 새 고정 시험만 |
| T3 Writer 가 Window Width 를 씀 | 〃 |
| T4 Writer 가 Rescale Slope 를 `2` 로 씀 | 〃 |
| T5 Writer 가 Rescale Intercept 를 `-1024` 로 씀 | 〃 |
| T6 Writer 가 Presentation LUT Shape 를 `INVERSE` 로 씀 | 〃 |
| 복원 후 | 소스 바이트 동일, 대조군 10개 초록 |

카드가 말한 "Writer 가 원본 Window 를 복사하도록 임시로 바꾸면"은 실제로 Writer 가 원본 파일에 접근할 방법이 없어(입력은 `img` 와 `meta` 뿐) 상수를 써 넣는 방식으로 대신했다. 그 변경도 같은 시험을 빨갛게 만든다.

## 4. 리더가 고칠 문서 — 제한 한 문장을 넣은 초안 (185 §7 대체)

공통 제한 문장(문서마다 알맞게 옮겨 쓴다):

> 읽기 정규화가 보존하는 것은 극성이다. `xpe_dicom_write` 는 원본 파일의 Window Center/Width, Rescale, Presentation LUT Shape 를 보존하지 않고(Window 없음, Rescale 1/0, IDENTITY) 소스 파일에서 아무것도 복사하지 않으므로, 원본에 비항등 VOI 나 Rescale 이 있었다면 읽기→쓰기 뒤 밝기·대비가 달라질 수 있다.

| 파일 | 현재 | 제안 |
|------|------|------|
| `docs/post-processing/xpe/XPE-SDD-002_Software_Detailed_Design.md` 1051 | `int32_t photometricInterpretation, // 0=MONO1, 1=MONO2` | 인자 삭제 (구현된 시그니처는 `xpe_apply_presentation_lut(XpeImageBuffer*, const XpePresentationLutParams*)`) |
| 〃 1062 | `3. IF photometricInterpretation == MONOCHROME1:` + `output = maxVal - pValue` | 삭제. "극성은 `xpe_dicom_read_image` 가 읽을 때 정규화한다(FR-DCM-109). 이 단계는 항상 MONOCHROME2 의미의 입력을 받고 반전하지 않는다." **+ 공통 제한 문장** |
| 〃 1082 | `\| MONOCHROME1 \| inversion needed \| Invert output \| SRS-FUNC-023 \|` | `\| MONOCHROME1 \| 이 단계 전에 리더가 반전해 MONOCHROME2 의미가 됨 \| 이 단계는 반전하지 않음 (원본의 Window·Rescale 은 쓰기에서 보존되지 않는다) \| SRS-FUNC-023 \|` |
| 〃 1043 | `Trace: SRS-FUNC-022, SRS-FUNC-023` | `SRS-FUNC-023` 은 DICOM 리더(FR-DCM-109)로 옮기고 이 줄은 `SRS-FUNC-022` 만 |
| `XPE-STP-001…` 215, 217 (UT-3.3-003/005) | "MONOCHROME1 / inversion needed", "MONOCHROME2 / no inversion" | 표시 단위(SWU-3.3)에서 삭제하고 DICOM 리더 시험으로 이동: UT-3.3-003 → `Tc109_FrDcm109_Monochrome1IsInvertedAndMonochrome2IsKept_OnEveryPath` 의 MONOCHROME1 항목, UT-3.3-005 → 같은 시험의 MONOCHROME2 항목. 왕복은 `Tc109_Monochrome1PlusInverse_RoundTripKeepsThePolarity` (극성만) 이고 표시 보존을 요구하지 않는다고 명시 |
| `XPE-SRS-001…` 63, `xpe-iec62304-class-b-package.md` 314 (SRS-FUNC-023) | "…MONOCHROME1/MONOCHROME2를 올바르게 처리해야 한다." | 본문 유지 + 해석: "MONOCHROME1 은 `xpe_dicom_read_image` 가 읽을 때 `(2^BitsStored − 1) − (값 & (2^BitsStored − 1))` 로 정규화하여 MONOCHROME2 의미로 돌려준다(SRS-DICOM-001 FR-DCM-109). 이후 파이프라인은 항상 MONOCHROME2 의미의 화소를 받으며 표시 단계는 극성을 바꾸지 않는다." **+ 공통 제한 문장** |
| `XPE-RTM-001…` 41 | `SRS-FUNC-023 … SWU-3.3 … UT-3.3-003,005` | SWU 를 DICOM 리더 단위로, 시험을 `Tc109_` 시험으로 |
| `XPE-SDD-001…` 51 | `SWU-3.3 \| PresentationLUT \| GSDF P-Value conversion, MONOCHROME1/2 handling \| SRS-FUNC-022, 023` | `MONOCHROME1/2 handling` 과 `023` 삭제 |
| `xray-postprocessing-prd.md` 250 | `MONOCHROME1 (bone=dark) / MONOCHROME2 (bone=bright) 자동 처리` | "읽기 단계에서 MONOCHROME2 의미로 정규화(극성만; 원본 Window·Rescale 은 쓰기에서 보존되지 않음)" |
| `RTM-DICOM-001…` 61, 206 | `FR-DCM-109 … TC-109 … ✓` | ✓ 가 이제 사실이다. TC-109 시험: `Tc109_FrDcm109_Monochrome1IsInvertedAndMonochrome2IsKept_OnEveryPath`, `Tc109_Monochrome1_MasksTheBitsAboveBitsStoredBeforeInverting`, `Tc109_Monochrome1_ReadingTwiceOnOneHandleGivesTheSameWords`, `Tc109_Monochrome1WithSignedPixelsIsRefusedLikeEverySignedImage`, `Tc109_Monochrome1PlusInverse_RoundTripKeepsThePolarity`. 새 현재 동작 기록: `Tc109_CurrentBehaviour_WriteDoesNotCarryWindowRescaleOrPresentationShapeFromTheSourceFile` (요구가 아님) |
| `docs/project/api-spec.md` 1360 | "**MONOCHROME1 은 저장된 그대로 돌려주며 호출자가 이를 구별할 수단이 없다** — 극성 정책은 #235 의 별도 결정이다" | "**MONOCHROME1 은 읽을 때 `(2^BitsStored − 1) − (값 & (2^BitsStored − 1))` 로 반전해 MONOCHROME2 의미로 돌려주고(FR-DCM-109), 반전한 읽기마다 Info 알림 1건(문구 전체는 `dicom_api.h`)을 낸다.** 부호 있는 화소는 PI 와 무관하게 거부한다. 파일의 Window/Rescale 은 저장된 표본 기준이라 반환값과 함께 쓰려면 `dicom_api.h` 의 거울 공식을 따른다. **보존되는 것은 극성이다: `xpe_dicom_write` 는 원본의 Window·Rescale·Presentation LUT Shape 를 보존하지 않는다(Window 없음, Rescale 1/0, IDENTITY).**" |
| `SRS-DICOM-001…` 121 (FR-DCM-109), `SAD-DICOM-001…` 187–194, `README.md` 130 | "MAX - pixel" / "MAX_VALUE" | MAX 가 `2^BitsStored − 1` 이고 위 비트를 먼저 마스크한다는 한 줄 보강 |
| 〃 `FR-DCM-205` (쓰기: "자동 정규화 (MONOCHROME1 입력일 경우)") | 쓰기가 MONOCHROME1 입력을 정규화한다는 인상 | 쓰기기는 입력 화소의 극성을 해석하지 않고 항상 MONOCHROME2 로 쓴다는 한 줄 (정규화는 읽기에서 이미 일어난다) |

## Gaps (미검증)

- 쓰기기가 Window·Rescale 을 **보존해야 하는지**는 판단하지 않았다. 이 카드는 현재 동작을 문서·시험으로 고정하고 과잉 보장을 걷어낸 것뿐이다.
- 알림 문구를 `clients/`·`gui/` 가 매칭하는지는 확인하지 않았다.
- 반환 Window 가 없어 거울 공식은 185 에서 수치 검사(표준 의사코드 옮김)로만 확인했고 이 카드에서 바뀐 것은 없다. Codex 가 3종 30만 표본으로 재실행해 확인했다(카드 전달 내용).
- 쓰기 문서의 새 항목은 이번 시험이 확인한 필드(Window Center/Width, Rescale Slope/Intercept, Presentation LUT Shape, PI)에 대한 것이다. 다른 속성(예: Pixel Padding Value, 명시적 VOI LUT Sequence)의 보존 여부는 확인하지 않았다.

## Residual-risk (잔여 위험)

- 고정 시험은 현재 동작을 기록한다. Writer 가 소스를 보존하도록 바뀌면 시험이 빨개지고 이 보고서·헤더·문서 초안의 제한 문장도 함께 고쳐야 한다.
- 리더가 문서를 고치기 전까지 `FR-DCM-205` 의 "자동 정규화" 문장은 쓰기기가 극성을 해석하는 것처럼 읽힌다.
