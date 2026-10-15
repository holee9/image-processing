# QA-B-185 — MONOCHROME1 은 읽을 때 반전해 항상 MONOCHROME2 의미로 (리더 결정, #235)

> **정정 (QA-B-185b, Codex #70)**: 이 보고서가 왕복을 "표시가 보존된다"고 쓴 곳은 과한 표현이다. 보존되는 것은 극성이고, `xpe_dicom_write` 는 원본의 Window·Rescale·Presentation LUT Shape 를 보존하지 않는다. 알림 문구의 식(§4)도 구현과 달랐다 — 현재 문구와 문서 초안(§7)은 QA-B-185b 보고서가 대체한다.

## 요약

| 항목 | 결과 |
|------|------|
| `xpe_dicom_read_image` 가 MONOCHROME1 을 반전 | 구현했다. 세 경로(비압축·JPEG LL·JPEG 2000) 모두, 디코드 뒤 호출자 버퍼에서, 식 `(2^B − 1) − (v & (2^B − 1))` (B = 반환 버퍼의 `bitsStored`, 마스크가 먼저) |
| 메타데이터의 Window·Rescale 정합성 | 반환 메타(`XpeImageMetadata`)에 Window·Rescale 필드가 없어 **맞출 값이 없다**. 파일에서 따로 읽는 호출자를 위한 반전 공식을 표준 근거와 함께 헤더에 적었고 수치로 검증했다 (§3) |
| 알림 | 반전한 읽기마다 Info 1건. 문구는 레인 간 계약 (§4) |
| 왕복 | MONOCHROME1+INVERSE → 읽기 → `xpe_dicom_write` → 읽기: **극성**이 보존된다(VOI 항등·Rescale 1/0 인 픽스처에서 화소 단위 단언). 표시 전체의 보존은 아니다 — QA-B-185b 정정 |
| 고정 시험 2개 | 하나는 반대 단언으로 바뀌고(옛 근거 §5), 다른 하나는 MONOCHROME2 부분이 그대로 유효해 유지하고 MONOCHROME1 부분을 새 시험으로 분리했다 |
| 반증 | 8가지 약화 각각이 해당 시험만 빨강으로 만든다. 마스크를 지우면 마스크 시험만 빨강 (§6) |
| 회귀 | 리더 시험 95/95, `ci-dicom` ctest 242개 중 실패 0 |
| 리더가 고칠 문서 | 줄 번호와 문장 초안 §7 |

## 1. 변경

| 위치 | 내용 |
|------|------|
| `modules/dicom/src/DicomReader.cpp` | `isMonochrome1(ds)` 와 `normaliseMonochrome1(img)` 추가. `readImage` 는 범위 검사 뒤 PI 를 한 번 더 읽어(범위 검사가 MONOCHROME1·2 만 통과시킨 뒤이므로 값은 확정) JPEG 2000 은 `decompressPixelData` 성공 직후, 비압축·JPEG LL 은 복사 직후에 반전한다. 반전은 호출자 버퍼에만 일어나고 핸들의 데이터셋은 건드리지 않는다 |
| `modules/dicom/include/xpe/dicom/dicom_api.h` | `xpe_dicom_read_image` 문서: 이전의 "MONOCHROME1 (no inversion, no indication)" 문장을 새 동작·알림·Window 공식으로 교체 |
| `modules/dicom/tests/test_dicom_reader.cpp` | 새 시험 5개, 기존 3개 수정 (§2, §5) |

식의 근거: PS3.3 C.7.6.3.1.2 — MONOCHROME1 은 "The minimum sample value is intended to be displayed as white", MONOCHROME2 는 "... as black" (QA-B-184 `standard_quotes.txt` [1]). 한 표본의 값은 워드의 하위 BitsStored 비트이므로 위 비트를 먼저 마스크하고 최댓값은 `2^B − 1` 이다. 반환 버퍼의 `bitsStored` 는 비압축·JPEG LL 에서는 데이터셋의 BitsStored, JPEG 2000 에서는 코드스트림 정밀도이며 두 값은 읽기 전에 서로 일치함이 검사된다.

### 부호 있는 화소 × MONOCHROME1 (카드 1)

표준은 PI 를 Pixel Representation 에 묶지 않으므로 이 조합은 합법이다. 그 경우 반전은 `−1 − v` (두 보수에서 최소↔최대) 이다. 이 함수는 부호 있는 화소를 **모든 PI 에서** `UNSUPPORTED_FORMAT` 으로 거부한다(반환 버퍼가 부호 없는 워드이기 때문, QA-B-182). 그래서 조합은 같은 사유로 거부된 채 남고, 반전 식을 하나 더 도입하지 않았다. 시험: `Tc109_Monochrome1WithSignedPixelsIsRefusedLikeEverySignedImage` (거부 코드, 출력 불변, 메타 조회 가능, "inverted" 알림 없음).

## 2. 시험 (`test_dicom_reader.cpp`)

TC-109 / FR-DCM-109 가 시험 이름과 주석으로 추적된다(`git grep FR-DCM-109 -- modules` 가 시험 파일을 가리킨다).

| 시험 | 단언 |
|------|------|
| `Tc109_FrDcm109_Monochrome1IsInvertedAndMonochrome2IsKept_OnEveryPath` | 비압축 16비트·비압축 12비트·JPEG LL·JPEG 2000 각각: MONOCHROME2 파일은 알림 0건, MONOCHROME1 사본은 모든 워드가 `M − (a & M)`, `bitsStored`·`bitsAllocated`·형식 불변, 알림 정확히 1건 + 심각도 Info |
| `Tc109_Monochrome1_MasksTheBitsAboveBitsStoredBeforeInverting` | BitsStored 12 + 위 4비트가 켜진 워드: MONOCHROME2 는 그대로(마스크 안 함), MONOCHROME1 은 `4095 − (v & 4095)` 이고 4095 를 넘는 워드가 0개 |
| `Tc109_Monochrome1_ReadingTwiceOnOneHandleGivesTheSameWords` | 같은 핸들에서 두 번 읽어도 같은 워드(세 경로) — 이중 반전이 없다 |
| `Tc109_Monochrome1WithSignedPixelsIsRefusedLikeEverySignedImage` | §1 |
| `Tc109_Monochrome1PlusInverse_RoundTripKeepsHowTheImageDisplays` | 원본(MONOCHROME1+INVERSE 의 표시값 = `M − 저장값`)과 복사본(MONOCHROME2+IDENTITY 의 표시값 = 워드)이 65536 화소 모두에서 같다. 복사본의 PI 는 `MONOCHROME2`, `MONOCHROME1` 문자열은 없다 |

MONOCHROME1 파일은 `PatchOnce` 로 같은 길이의 바이트를 고쳐 만든다: PI `MONOCHROME2→MONOCHROME1`, 그리고 파일에 `IDENTITY` 가 있으면 `INVERSE ` (CS 는 짝수 길이로 공백 패딩; 표준이 MONOCHROME1 DX 에 INVERSE 를 요구하므로 적합한 파일이 된다, PS3.3 C.8.11.3). 치환 대상이 정확히 한 번 나와야 하며 아니면 시험이 실패한다.

빨강 → 초록: 수정 전 5개 빨강(`red_before_fix.txt`: 네 경로 모두 "65536 words differ", Info 알림 없음), 수정 후 전부 초록 (`green_after_fix.txt`). 수정 전에도 초록인 두 개(부호 있는 조합, 두 번 읽기)는 "그대로 유지" 대조이고, 두 번 읽기는 S6 반증이 의미를 증명한다.

## 3. 메타데이터의 Window·Rescale (카드 2)

**조사**: `xpe_dicom_get_metadata` 가 채우는 `XpeImageMetadata` 는 `bodyPart, kVp, mAs, SID_mm, pixelPitch_mm, acquisitionTime, flags` 뿐이고, `modules/` 에서 `WindowCenter|WindowWidth|DCM_Window` 를 `git grep` 하면 0건이다(대조: PI 처리는 `DicomReader.cpp` 에서 잡힌다). 반환하는 Window 가 없으므로 반전된 화소와 맞출 반환값이 없다.

**그래도 호출자가 파일에서 Window 를 따로 읽는 경우**를 위해 공식을 정했다. 표준: Window 는 Modality LUT 출력에 적용되고 극성은 그 뒤에 정해진다(PS3.3 C.11.2: "Whether the minimum output value is rendered as black or white may depend on the Value of Photometric Interpretation"). 반전 뒤 워드 `v' = M − v` 의 모달리티 값은 `s·v' + b = K − (s·v + b)`, `K = s·M + 2b`. 그러므로 폭은 그대로이고 중심은 VOI LUT Function 에 따라 다르다:

| VOI LUT Function | 표준의 창 | 반전 후 중심 |
|-------------------|-----------|---------------|
| `LINEAR_EXACT`, `SIGMOID` | `c` 를 중심으로 대칭 | `K − c` |
| `LINEAR` (기본값) | 중심이 `c − 0.5` (PS3.3 C.11.2.1.2: `x <= c − 0.5 − (w−1)/2 …`) | `K − c + 1` |

기울기 1·절편 0 이면 `M − c` (LINEAR_EXACT, SIGMOID) 와 `M − c + 1` (LINEAR). **LINEAR 의 `+1` 은 흔히 빠뜨리는 항이라 수치로 확인했다**: `window_mirror_check.py` 가 표준 의사코드를 옮겨(LINEAR, LINEAR_EXACT, SIGMOID) 8~16비트, 기울기 {1, 2, 0.5, 1/65535}, 절편 {0, −1024, 5} 무작위 조합에서 `원본 밝기 = ymax+ymin − voi(v; c, w)` 와 `반환 밝기 = voi(M−v; c', w)` 를 비교했다: 최대 차이 LINEAR 2.9e-11, LINEAR_EXACT 1.5e-11, SIGMOID 1.7e-13 (부동소수점 오차 수준). 다른 계열의 이동량을 쓰면 LINEAR 에서 61932/108768, LINEAR_EXACT 에서 63813/110784 표본이 어긋나, 검사가 틀릴 수 있음이 대조된다(`window_mirror_check_output.txt`). 공식은 `dicom_api.h` 의 `xpe_dicom_read_image` 문서에 적었다. 명시적 VOI LUT 표는 표를 뒤집어야 하며 다루지 않았다.

## 4. 알림 — 레인 간 계약

반전한 읽기마다 `XPE_ALERT_INFO` 한 건:

> `MONOCHROME1 pixel values were inverted to MONOCHROME2 sense: value = (2^BitsStored - 1) - value, BitsStored <B>`

MONOCHROME2 읽기와 거부된 읽기는 이 알림을 내지 않는다(시험이 둘 다 단언). 문구는 `clients/`·`gui/` 가 표시하는 알림 계약이므로 바꿀 때 함께 합의한다. 이 카드에서 `clients/`·`gui/` 가 이 문구를 매칭하는지 확인하지는 않았고, QA-B-184 에서 읽기를 부르는 곳이 개발 도구 하나뿐임을 확인했다.

## 5. 고정 시험 2개의 옛 근거와 새 상태

| 시험 | 옛 근거 (QA-B-182f) | 새 상태 |
|------|----------------------|---------|
| `Pinned_Issue235AwaitsDesignDecision_Monochrome1IsReturnedAsStored` | "#235 가 결정하면 이 시험이 바뀐다: no inversion, and no signal that the data is inverted" — 결정 전의 현상을 고정 | **`Issue235Decided_Monochrome1IsInvertedToMonochrome2Sense`** 로 이름·단언 반대. 주석에 옛 근거를 남겼다 |
| `Pinned_Issue235AwaitsDesignDecision_BitsAboveBitsStoredAreNotMasked` | "위 비트를 마스크하지 않는다 — 결정 대기" | **MONOCHROME2 에 대해서는 그대로 유효**(이 결정이 바꾸지 않았다)라 이름을 유지하고 단언도 그대로 둔다. MONOCHROME1 은 마스크가 되므로 새 시험 `Tc109_Monochrome1_MasksTheBitsAboveBitsStoredBeforeInverting` 이 맡는다. #235 에는 MONOCHROME2 쪽 마스크·Rescale 이 여전히 열려 있다 |
| `Scope_PhotometricInterpretationRefusalsNameTheValueAndMonochrome1IsReturnedAsStored` | 같은 화소 + 알림 0건 | `…Monochrome1IsInverted` 로 개명, 반전 + 알림 1건 단언 |
| 표 행 `{"monochrome1", …, OK}` | 주석 "read as stored (#235 decides)" | 주석만 갱신 (여전히 OK) |

## 6. 반증 (`arms.txt`)

| 약화 | 빨강이 된 것 |
|------|--------------|
| S1 반전 루프가 돌지 않음 | 반전을 단언하는 시험 전부 (5개) |
| **S2 마스크 제거** | `Tc109_Monochrome1_MasksTheBitsAboveBitsStoredBeforeInverting` **만** |
| S3 JPEG 2000 만 반전 안 함 | 전 경로 시험의 "JPEG 2000" 항목만 (워드 65536개 불일치) |
| S4 JPEG LL 만 반전 안 함 | "JPEG Lossless" 항목만 |
| S5 비압축만 반전 안 함 | "native 16-bit", "native 12-bit" 항목과 단일 경로 시험들 |
| S6 반전을 핸들의 데이터셋 버퍼에서 함 | `…ReadingTwiceOnOneHandle…` 만 (두 번째 읽기가 다시 뒤집힘) |
| S7 알림 제거 | 알림을 세는 시험 둘 |
| S8 MONOCHROME2 도 반전 | 반전 보존(MONOCHROME2 유지) 단언 시험들, `…BitsAbove…NotMasked` 포함 |
| 복원 후 | 소스 바이트 동일, 대조군 9개 초록 |

## 7. 리더가 고칠 문서 — 정확한 줄과 문장 초안

문장은 초안이다. 검색은 `git grep -n` 이며 줄 번호는 편집에 밀릴 수 있으니 고칠 때 문장으로 다시 찾는다.

| 파일:줄 | 현재 | 제안 |
|---------|------|------|
| `docs/post-processing/xpe/XPE-SDD-002_Software_Detailed_Design.md:1051` | `int32_t photometricInterpretation, // 0=MONO1, 1=MONO2` | 인자 삭제. 구현된 시그니처는 `xpe_apply_presentation_lut(XpeImageBuffer* img, const XpePresentationLutParams* params)` |
| 〃 `:1062` | `3. IF photometricInterpretation == MONOCHROME1:` 와 그 아래 `output = maxVal - pValue` | 삭제. 대신: "극성은 `xpe_dicom_read_image` 가 읽을 때 정규화한다(FR-DCM-109). 이 단계는 항상 MONOCHROME2 의미의 입력을 받고 반전하지 않는다." |
| 〃 `:1082` | `\| MONOCHROME1 \| inversion needed \| Invert output \| SRS-FUNC-023 \|` | `\| MONOCHROME1 \| 이 단계에 도달하기 전에 리더가 반전해 MONOCHROME2 의미가 됨 \| 이 단계는 반전하지 않음 \| SRS-FUNC-023 \|` |
| 〃 `:1043` | `Trace: SRS-FUNC-022, SRS-FUNC-023` | `SRS-FUNC-023` 은 DICOM 리더(FR-DCM-109) 로 옮기고 이 줄은 `SRS-FUNC-022` 만 |
| `docs/post-processing/xpe/XPE-STP-001_Software_Test_Plan_and_Cases.md:215, 217` | UT-3.3-003 "MONOCHROME1 / inversion needed / inverted output", UT-3.3-005 "MONOCHROME2 / no inversion" | 표시 단위(SWU-3.3)에서 삭제하고 DICOM 리더 시험으로 이동: UT-3.3-003 → `DicomReaderTest.Tc109_FrDcm109_Monochrome1IsInvertedAndMonochrome2IsKept_OnEveryPath` 의 MONOCHROME1 항목, UT-3.3-005 → 같은 시험의 MONOCHROME2 항목 |
| `docs/post-processing/xpe/XPE-SRS-001_Software_Requirements_Specification.md:63`, `xpe-iec62304-class-b-package.md:314` (SRS-FUNC-023) | "시스템은 Photometric Interpretation MONOCHROME1/MONOCHROME2를 올바르게 처리해야 한다." | 본문은 유지하고 해석을 덧붙인다: "MONOCHROME1 은 `xpe_dicom_read_image` 가 읽을 때 `(2^BitsStored − 1) − 값` 으로 정규화하여 MONOCHROME2 의미로 돌려준다(SRS-DICOM-001 FR-DCM-109). 이후 파이프라인은 항상 MONOCHROME2 의미의 화소를 받으며 표시 단계는 극성을 바꾸지 않는다." |
| `docs/post-processing/xpe/XPE-RTM-001_Requirements_Traceability_Matrix.md:41` | `SRS-FUNC-023 … SWU-3.3 … UT-3.3-003,005` | SWU 를 DICOM 리더 단위로, 시험을 위 `Tc109_` 시험으로 |
| `docs/post-processing/xpe/XPE-SDD-001_Software_Unit_Identification.md:51` | `SWU-3.3 \| PresentationLUT \| GSDF P-Value conversion, MONOCHROME1/2 handling \| SRS-FUNC-022, 023` | `MONOCHROME1/2 handling` 과 `023` 삭제 |
| `docs/post-processing/xpe/xray-postprocessing-prd.md:250` | `MONOCHROME1 (bone=dark) / MONOCHROME2 (bone=bright) 자동 처리` | "읽기 단계에서 MONOCHROME2 의미로 정규화" 로 |
| `docs/dicom/RTM-DICOM-001_Requirements_Traceability_Matrix.md:61, 206` | `FR-DCM-109 … TC-109 … MONOCHROME1 반전, MONOCHROME2 유지 \| ✓` | ✓ 가 이제 사실이다. `TC-109` 가 가리킬 시험 이름: `Tc109_FrDcm109_Monochrome1IsInvertedAndMonochrome2IsKept_OnEveryPath`, `Tc109_Monochrome1_MasksTheBitsAboveBitsStoredBeforeInverting`, `Tc109_Monochrome1_ReadingTwiceOnOneHandleGivesTheSameWords`, `Tc109_Monochrome1WithSignedPixelsIsRefusedLikeEverySignedImage`, `Tc109_Monochrome1PlusInverse_RoundTripKeepsHowTheImageDisplays` |
| `docs/project/api-spec.md:1360` | "**MONOCHROME1 은 저장된 그대로 돌려주며 호출자가 이를 구별할 수단이 없다** — 극성 정책은 #235 의 별도 결정이다" | "**MONOCHROME1 은 읽을 때 `(2^BitsStored − 1) − (값 & (2^BitsStored − 1))` 로 반전해 MONOCHROME2 의미로 돌려주고(FR-DCM-109), 반전한 읽기마다 Info 알림 1건을 낸다.** 부호 있는 화소는 PI 와 무관하게 거부한다. 파일의 Window/Rescale 은 저장된 표본 기준이므로 이 함수의 반환값과 같이 쓰려면 `dicom_api.h` 의 거울 공식을 따른다" |
| `docs/dicom/SRS-DICOM-001…:121` (FR-DCM-109), `SAD-DICOM-001…:187-194`, `README.md:130` | "MAX - pixel" / "MAX_VALUE" | MAX 가 `2^BitsStored − 1` 이고 위 비트를 먼저 마스크한다는 한 줄 보강 (정의가 빠져 있었다) |

**함께 발견한 불일치 (이 카드 범위 밖, 판정하지 않음)**:

- `SRS-DICOM-001:123`, `SAD-DICOM-001:194` 가 부르는 오류 코드 `XPE_ERR_DICOM_UNSUPPORTED_PHOTOMETRIC` 는 코드에 없다(실제 반환은 `XPE_ERR_UNSUPPORTED_FORMAT`; `git grep UNSUPPORTED_PHOTOMETRIC -- modules` 0건).
- `RTM-DICOM-001:63, 208` 은 `FR-DCM-111` (Window/Level 읽기) 를 ✓ 로 표시하지만 `modules/` 에는 Window 를 읽는 코드가 없다(위 `git grep` 0건). FR-DCM-109 와 같은 종류의 "기록은 있으나 일치하지 않음" 이다.

## 8. 재실행

- `roundtrip_probe.py` (QA-B-184 의 것): 현재 DLL 에 대해 돌린 결과가 `roundtrip_probe_after_output.txt`. **스크립트의 마지막 줄("SAME words … opposite polarity")은 184 의 문구라 이제 맞지 않는다**: 이제 B 를 읽은 값은 A 의 반전(첫/마지막 `4095 / 0`)이고, C 는 그 반전된 값에 MONOCHROME2 라벨이 붙어 원본과 같게 표시된다. 판정은 시험 `Tc109_Monochrome1PlusInverse_RoundTripKeepsHowTheImageDisplays` 가 한다.
- `window_mirror_check.py`: 순수 파이썬, 입력 없음.

## Gaps (미검증)

- 실제 장비의 MONOCHROME1 파일로는 돌리지 않았다. 시험 파일은 우리 쓰기기가 만든 파일의 두 속성을 바꾼 것이다(JPEG LL·JPEG 2000 사본은 쓰기기+DCMTK/OpenJPEG 가 만든 것).
- 반환 Window·Rescale 이 없어 §3 의 거울 공식은 **이 API 의 반환값으로는 시험되지 않는다**. 표준 의사코드를 옮긴 수치 검사(`window_mirror_check.py`)와 헤더 문서뿐이다. 의사코드는 PS3.3 에서 옮겼지만 이 저장소의 VOI 구현(`display/src/voi_lut.cpp`)과 대조하지는 않았다.
- 8비트(BitsAllocated 8)와 부호 있는 화소는 이 함수가 원래 거부한다(JPEG 2000 의 8비트는 16 워드 + `bitsStored 8` 로 돌려주며 반전 식이 B=8 에 대해서도 성립하나, 이 경로의 MONOCHROME1 8비트 J2K 는 시험하지 않았다).
- `Pixel Padding Value`, `Smallest/Largest Image Pixel Value` 는 이 API 가 돌려주지 않는다. 반전 뒤 그 값들의 의미는 다루지 않았다.
- `ci-dicom` 외의 CI 구성, CI 러너 실행은 푸시 뒤에야 본다. `clients/`·`gui/` 쪽 알림 문구 매칭은 확인하지 않았다.

## Residual-risk (잔여 위험)

- 이 함수로 읽은 영상을 DCMTK 로 같은 파일을 직접 읽은 값과 비교하는 외부 도구는 MONOCHROME1 파일에서 어긋난다. Info 알림과 `dicom_api.h` 문서로 알린다.
- 리더가 고칠 문서가 고쳐지기 전까지 후처리 SDD-002·STP 는 "표시가 PI 로 반전한다"고 말한다. 코드는 이중 반전을 하지 않는다(표시 모듈에는 PI 인자가 없다).
- MONOCHROME2 쪽 위 비트 마스크와 Rescale 은 #235 에 열려 있다.
