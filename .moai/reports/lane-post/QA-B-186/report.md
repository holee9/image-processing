# QA-B-186 — #235 마감 대조: 제목의 다섯 항목이 지금 각각 어떻게 처리되는지

코드 변경은 없다. 이 보고서는 §1 의 표를 **실행으로** 채웠다: 다섯 항목 × 세 압축 경로(비압축·JPEG LL·JPEG 2000)를 임시 시험(`matrix_probe_source.txt`, 커밋하지 않음)으로 읽어 27칸의 결과를 얻었다 (`matrix_output.txt`, 칸마다 반환 코드·출력 불변 여부·알림 수와 첫 알림 문구·정상 반환이면 화소가 저장값/반전값/그 외 중 무엇인지).

## 결론 먼저

**#235 는 지금 닫을 수 없다. 남은 것은 리스케일 하나다.**

| 항목 | 지금 | 판정 |
|------|------|------|
| 부호 | 세 경로 모두 `UNSUPPORTED_FORMAT` + Error 알림 | 거부 (가용성 항목, §4) |
| MONOCHROME1 | 세 경로 모두 반전해 돌려주고 Info 알림 | 올바른 처리 (185·185b) |
| 다중 프레임 | 세 경로 모두 `UNSUPPORTED_FORMAT` + Error 알림 | 거부 (가용성 항목) |
| RGB | 세 경로 모두 `UNSUPPORTED_FORMAT`(3샘플) 또는 `DICOM_INVALID`(1샘플에 RGB 라벨) + Error 알림 | 거부 (가용성 항목) |
| **리스케일** | 세 경로 모두 `OK`, 화소는 **저장값 그대로**, 알림 0건, 반환 메타에도 없음. 기울기 0 이나 숫자가 아닌 값도 같은 결과 | **조용함** — 제목의 "오류 없이 틀린 화소를 돌려준다"에 아직 해당 |

덧붙여 **시험이 비어 있는 칸**이 있다 (§2): 부호·리스케일·다중 프레임은 시험이 비압축 경로에만 있고 JPEG LL·JPEG 2000 칸은 이 조사의 임시 시험으로만 확인했다.

## 1. 표: 다섯 항목 × 세 경로

「시험」 열은 `git grep` 으로 이름을 확인했고(`introducing_commits.txt`), 표의 시험 중 13개를 지금 골라 실행해 모두 통과했다 (`cited_tests_run.txt`; 나머지 `Tc109_` 시험은 185b 때의 리더 시험 전체 96/96 에 들어 있다). 「근거 커밋」은 그 시험(또는 거부 코드)을 처음 넣은 커밋이다. 「임시」는 이 보고서의 임시 시험으로만 확인한 칸이다.

### 1) 부호 (PixelRepresentation 1)

| 경로 | 지금 동작 | 시험 | 근거 커밋 |
|------|-----------|------|-----------|
| 비압축 | `UNSUPPORTED_FORMAT`, 출력 불변, 메타 조회 가능, Error 알림 "PixelRepresentation 1 (only unsigned pixels are supported)" | `Scope_SignedPixelsAreUnsupportedNotReinterpreted` | a0db88d6 (거부), e0827103 (알림 문구) |
| JPEG LL | 같음 | **없음** (임시로만 확인) | — |
| JPEG 2000 | 태그가 부호 있음 → 같음 | **없음** (임시로만 확인). 코드스트림이 실제로 부호 있는데 태그가 부호 없음 → `DICOM_INVALID`: `J2kScope_ACodestreamThatContradictsTheTagsIsRefused` (`signed_codestream_tags_say_unsigned` 칸) | adf21476 |
| MONOCHROME1 과의 조합 | 같음 (`−1 − v` 반전을 새로 만들지 않고 거부) | `Tc109_Monochrome1WithSignedPixelsIsRefusedLikeEverySignedImage` (비압축만) | d691296e |

### 2) MONOCHROME1

| 경로 | 지금 동작 | 시험 | 근거 커밋 |
|------|-----------|------|-----------|
| 비압축 / JPEG LL / JPEG 2000 | `OK`, 모든 워드가 `(2^B − 1) − (v & (2^B − 1))`(반전), Info 알림 1건 | `Tc109_FrDcm109_Monochrome1IsInvertedAndMonochrome2IsKept_OnEveryPath` (세 경로 + 12비트), `Scope_PhotometricInterpretationIsRequiredAndOnlyAMonochromePlaneIsReturned` (세 경로가 MONOCHROME1 을 `OK` 로 받음), `Tc109_Monochrome1_Masks…`, `…ReadingTwiceOnOneHandle…`, `…RoundTripKeepsThePolarity` | d691296e, 5ee4a336(185b), b69d22df (PI 표) |

### 3) 리스케일 (RescaleSlope / RescaleIntercept)

| 경로 | 입력 | 지금 동작 | 시험 |
|------|------|-----------|------|
| 비압축 | 기울기 2, 절편 −1024 | `OK`, 화소 = 저장값, 알림 0건 | `Pinned_Issue235AwaitsDesignDecision_RescaleIsNotAppliedNorReported` (a0db88d6) |
| JPEG LL | 같음 | 같음 | **없음** (임시로만) |
| JPEG 2000 | 같음 | 같음 | **없음** (임시로만) |
| 세 경로 | 기울기 1·절편 −1024 / **기울기 0·절편 0** / **숫자가 아닌 값**("abc","xyz") | 모두 `OK`, 화소 = 저장값, 알림 0건 | **없음** (임시로만) |

### 4) 다중 프레임 (NumberOfFrames > 1)

| 경로 | 지금 동작 | 시험 | 근거 커밋 |
|------|-----------|------|-----------|
| 비압축 | `UNSUPPORTED_FORMAT`, 출력 불변, Error 알림 "NumberOfFrames 3 (only single-frame images are supported)" | `Scope_MultiFrameIsUnsupportedNotTheFirstFrame` | a0db88d6, e0827103 |
| JPEG LL / JPEG 2000 | 같음 (속성만 3 이고 코드스트림은 한 프레임이어도 디코드 전에 거부) | **없음** (임시로만) | — |
| 속성 자체 | 없으면 한 프레임, 숫자가 아니거나 0 이거나 빈 값이면 `DICOM_INVALID` | `Scope_AbsentNumberOfFramesIsASingleFrame` (adf21476), `Scope_PresentButUnreadableAttributesAreMalformedNotDefaulted` (a0db88d6) — 비압축 | |
| 문서화된 관용 | `NumberOfFrames` 가 없는 파일의 PixelData 가 선언보다 길면 **남는 부분을 무시하고 `OK`** (말미 패딩은 합법). 즉 속성이 빠진 다중 프레임 파일은 첫 프레임만 조용히 읽힌다 | `SurplusPixelData_IsIgnoredAndReadSucceeds` (6c9356b4) | 속성이 없으면 한 프레임이라는 정의 그대로이며 이 파일은 표준 위반이다 |

### 5) RGB

| 경로 | 입력 | 지금 동작 | 시험 | 근거 커밋 |
|------|------|-----------|------|-----------|
| 세 경로 | SamplesPerPixel 3 + PI RGB | `UNSUPPORTED_FORMAT`, 출력 불변, Error 알림 "SamplesPerPixel 3 (only one sample per pixel is supported)" | `Scope_PhotometricInterpretationIsRequiredAndOnlyAMonochromePlaneIsReturned` (행 `rgb_three_samples`, **세 경로**), 비압축 16·8비트 `Scope_RgbIsUnsupportedNotByteSoup`, 알림 `Scope_ARefusalPostsAnAlertThatNamesTheCause` | a0db88d6, e0827103, b69d22df |
| 세 경로 | SamplesPerPixel 1 + PI RGB (라벨만) | `DICOM_INVALID` (표준 위반), Error 알림 "…RGB with SamplesPerPixel 1 (PS3.3 C.7.6.3.1.2: it may be used only when SamplesPerPixel is 3)" | 같은 표 시험의 `rgb_one_sample` 행 (세 경로) | b69d22df |

## 2. 조용하거나 시험이 없는 칸 — 재현 입력과 함께

| 칸 | 재현 입력 | 지금 | 의미 |
|----|-----------|------|------|
| **리스케일, 세 경로** | 모듈의 쓰기기가 만든 256×256 16비트 파일에 `RescaleSlope "2"`, `RescaleIntercept "-1024"` 를 넣어(비압축은 DCMTK 로, JPEG LL·JPEG 2000 은 같은 구문 변형) `xpe_dicom_read_image` | `OK`, 모든 화소가 저장값, 알림 0건 | 호출자가 모달리티 단위(예: HU)를 기대하면 틀린 값이다. API 는 저장값을 돌려준다고 문서화했지만 파일이 그것과 다르다고 알리지 않는다 — **조용함** |
| 리스케일, 기울기 0 / 숫자 아님 | 같은 파일에 `RescaleSlope "0"`, `"abc"` | 같음 | 값이 읽히지도 않고(검사 없음) 알림도 없다. 기울기 0 은 의미 없는 값이다 |
| 부호, JPEG LL·JPEG 2000 칸 | 두 경로의 파일에 `PixelRepresentation 1` | 거부 | **시험이 없다** (동작은 올바름) |
| 리스케일, JPEG LL·JPEG 2000 칸 | 위 | 조용함 | 시험 없음 |
| 다중 프레임, JPEG LL·JPEG 2000 칸 | 두 경로의 파일에 `NumberOfFrames "3"` | 거부 | 시험 없음 (동작은 올바름) |

**JPEG LL 에서 데이터는 부호 있는데 `PixelRepresentation` 이 0 인 파일**은 이 조사로 판별하지 못했다: JPEG LL 의 프레임 헤더에는 부호 정보가 없어 검사할 수단이 없다고 읽었고(읽기 코드에서 SOF 의 정밀도와 성분 수만 본다), 실행으로 확인하지는 않았다. JPEG 2000 은 코드스트림의 부호를 보고 거부한다(위 시험).

## 3. 리스케일 (카드 3): 비항등 Rescale 파일을 읽으면?

- **거부하지 않는다. 그대로 돌려준다.** 반환 코드 `OK`, 화소는 저장된 워드, 알림 없음, `XpeImageMetadata` 에도 Rescale 이 없다 (`bodyPart, kVp, mAs, SID_mm, pixelPitch_mm, acquisitionTime, flags` 뿐).
- **조용한 오답인가?** 호출자가 이 워드를 모달리티 단위로 해석한다면 그렇다. 호출자가 "저장값"으로 받는다면 아니다. `dicom_api.h` 는 "returned as stored … not applied, not reported" 라고 쓰지만 이는 **문서에만** 있고 런타임 신호가 없다 — #235 제목의 "오류 없이"에 해당한다. 이 제품의 호출자(X선 DX/CR 처리)는 대개 Rescale 이 항등이거나 없어 영향이 작겠지만, 그것을 이 저장소에서 측정하지는 않았다(실제 장비 파일이 없다).
- **읽기→쓰기에서 손실**: 쓰기기는 Rescale 을 1/0 으로 새로 쓰므로(`Tc109_CurrentBehaviour_Write…`) 비항등 Rescale 파일을 읽어 쓰면 모달리티 의미가 사라진다.
- **MONOCHROME1 과의 관계**: 185 의 거울 공식은 Rescale 을 알아야 한다(`K = s·M + 2b`). 이 API 가 Rescale 을 돌려주지 않으므로 호출자는 파일에서 따로 읽어야 한다.

## 4. 부호 있는 MONOCHROME1 — 가용성 항목

표준상 합법인 조합(PS3.3 은 PI 를 Pixel Representation 에 묶지 않는다)이며 지금 `UNSUPPORTED_FORMAT` 이다. **정확성이 아니라 가용성의 문제**다: 틀린 화소를 돌려주지 않고, 읽히지 않을 뿐이다. 부호 있는 화소 전체(MONOCHROME2 포함)가 같은 사유로 거부되고, 1)·4)·5) 항목의 다른 거부(부호, 다중 프레임, RGB)도 같은 부류이므로 한 묶음의 가용성 항목으로 분류한다. 지원하려면 부호 있는 워드를 담을 반환 형식이 필요하다(`XpePixelFormat` 은 `UINT16`·`FLOAT32`·`UINT8` 뿐). 구현 범위는 §5 의 항목 B.

## 5. 결론: 닫을 수 있는가, 남은 것

**닫을 수 없다.** 제목의 다섯 항목 중 네 항목은 거부 또는 올바른 처리다(조용한 오답 없음, 위 표). 리스케일이 남았다. 남은 것마다 한 줄 범위 (리더가 다음 카드나 별도 이슈로 정한다):

| | 남은 것 | 구현 범위 (한 줄) |
|---|---------|---------------------|
| **A** | 리스케일이 조용하다 (§3) | 가장 작은 처방: 비항등 Rescale(기울기 ≠ 1 또는 절편 ≠ 0; 값을 못 읽거나 기울기 0 이면 `DICOM_INVALID`)을 `readImage` 에서 판정하고 Warning 알림 1건("Rescale not applied: slope s, intercept b; returned words are stored values") — 읽기 두 속성과 알림 문구, 세 경로 시험. 더 큰 처방: 메타에 보고(구조체/`flags`/새 export — QA-B-184 §5(a) 와 같은 변형) 또는 적용(반환 형식 변경, 항목 B 와 같이) |
| **B** | 부호 있는 화소 가용성 (§4) | 반환 형식(부호 있는 정수 또는 float 변환)과 그에 맞는 반전 식(`−1 − v`)을 정하고 구현. 이 제품의 입력(X선)에서 필요한지는 제품 요구에 달렸다 |
| **C** | 시험의 빈 칸 (§2) | 임시 시험(`matrix_probe_source.txt`)을 영구 시험으로 바꾼다: 5개 항목 × 3개 경로 27칸의 반환 코드와 알림을 단언하는 시험 하나 — 시험 이름이 모든 칸을 가리키게 된다 |
| **D** | MONOCHROME2 의 BitsStored 위 비트 (고정 시험 `…BitsAboveBitsStoredAreNotMasked`) | 이슈 본문에 있는 항목이면: 마스크를 MONOCHROME2 에도 적용(MONOCHROME1 은 이미 마스크) + 고정 시험 갱신. 제목의 다섯 항목에는 없다 |
| **E** | 리스케일 값 검증 (기울기 0·숫자 아님이 조용히 통과) | A 와 함께: 값 읽기와 검증을 같은 자리에서 |
| **F** | 문서 기록 (RTM-DICOM, SRS 오류 코드 이름, FR-DCM-111) | 185 보고서 §7 에서 이미 리더에게 넘겼다 |

A 한 가지만 정하면 제목의 다섯 항목이 모두 "거부 / 올바른 처리 / 알림이 있는 처리" 중 하나가 되어 #235 를 닫을 수 있다. B·D·C 는 닫는 조건이 아니라 별도 이슈로 분리할 수 있는 범위로 본다(판단은 리더).

## Gaps (미검증)

- **27칸은 우리 쓰기기가 만든 파일을 바꾼 변형**으로만 읽었다. 실제 장비의 JPEG LL·JPEG 2000 파일, 실제 다중 프레임(여러 프래그먼트) 인캡슐된 파일, 진짜 RGB JPEG LL(성분 3)로는 돌리지 않았다. 성분 수가 다른 JPEG LL 은 기존 시험(`Scope_JpegLosslessComponentCount…`)이 따로 다룬다.
- 부호 있는 데이터에 `PixelRepresentation 0` 이 붙은 JPEG LL 파일은 판별 수단이 없다고 **읽어서** 판단했고 실행으로 확인하지 않았다 (§2).
- 이 제품의 실제 입력(장비 파일)에서 비항등 Rescale 이 얼마나 흔한지는 모른다.
- 이슈 #235 의 본문(제목 외)은 읽지 못했다. 카드가 준 제목 다섯 항목과 저장소의 고정 시험 이름(`Pinned_Issue235…`)에서 범위를 따랐다. D 가 이슈 본문 항목인지는 확인하지 못했다.
- 임시 시험은 커밋하지 않았고, 이 보고서의 27칸은 한 번 실행한 결과다 (반복 실행하지 않았다).

## Residual-risk (잔여 위험)

- 리스케일이 정해지기 전까지 모달리티 단위를 기대하는 호출자는 조용히 틀린 값을 받는다. 읽기→쓰기에서는 Rescale 이 1/0 으로 바뀌어 의미가 사라진다.
- 영구 시험이 생기기 전까지 JPEG LL·JPEG 2000 의 거부는 회귀해도 시험이 잡지 못한다 (동작은 지금 올바르다).
- 임시 시험이 쓴 도우미(`MakeSameSyntaxVariant`, `MakeJ2kVariant`)가 바뀌면 `matrix_probe_source.txt` 는 그대로는 컴파일되지 않을 수 있다.
