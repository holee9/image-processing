# QA-B-182f — Codex #54: 필수 PhotometricInterpretation 을 읽지 않고 성공 반환 (#235)

Refs #235

## 0. 먼저 읽을 것

| 항목 | 결과 |
|------|------|
| 지적은 맞았다 | `PhotometricInterpretation (0028,0004)` 는 Type 1 인데 reader 가 한 번도 읽지 않았다. 태그가 없거나 PALETTE COLOR 여도 회색 uint16 으로 성공했다. |
| 고친 것 | 부재·빈 값 → `DICOM_INVALID` + 알림(디코드·할당 전). 값 분류는 §2 표. Rows/Columns 도 부재·0 일 때 알림을 추가했다(코드는 이미 `DICOM_INVALID`). |
| MONOCHROME1 | 거부하지 않는다. 지금처럼 **저장된 그대로** 반환하며, 호출자가 알 방법이 **없다** (반환 메타·알림 모두 없음, §4). |
| 182e 의 Gap 이 하나 메워졌다 | 182e 보고서는 "PS3.3 의 BitsStored/HighBit 정의를 인용하지 못했다"고 적었다. 이번에 PS3.3 표 C.7-11c 에서 찾았다 (`standard_quotes.txt` [1]). 규칙이 PS3.5 8.1.1 과 같은 문장이라 182e 의 분류는 그대로 맞다. |
| 카드 항목 5 (JPEG LL 성분 수·부호) | 182e 범위로 지정된 일이라 182e 에 커밋 `de3a3ecc` 로 했다. 결과만 §6 에 옮긴다. |
| 공개 헤더 문서 | `dicom_api.h` 의 `@return` 설명이 **182e 의 변경도 반영하지 못해** 틀려 있었다("1, 8, 12, 32 bits allocated … 은 UNSUPPORTED" 등). 182e·182f 의 실제 동작으로 다시 썼다. |

## 1. 표준 원문 (전체: `standard_quotes.txt`)

- **PS3.3 표 C.7-11c Image Pixel Description Macro** — https://dicom.nema.org/medical/dicom/current/output/chtml/part03/sect_C.7.6.3.3.html : Type 1 은 Samples per Pixel, **Photometric Interpretation**, Rows, Columns, Bits Allocated, Bits Stored, High Bit, Pixel Representation. 1C 는 Planar Configuration (Samples per Pixel > 1 일 때), Pixel Aspect Ratio, Red/Green/Blue Palette Color Lookup Table Descriptor (PALETTE COLOR 일 때). Smallest/Largest Image Pixel Value 는 Type 3.
- **PS3.3 C.7.6.3.1.2** — https://dicom.nema.org/medical/dicom/current/output/chtml/part03/sect_C.7.6.3.html :
  - MONOCHROME1: "The minimum sample value is intended to be displayed as white after any VOI gray scale transformations have been performed. … This Value may be used only when Samples per Pixel (0028,0002) has a Value of 1."
  - MONOCHROME2: 같은 문장에서 "black".
  - PALETTE COLOR: "The pixel value is used as an index into each of the Red, Blue, and Green Palette Color Lookup Tables … This Value may be used only when Samples per Pixel (0028,0002) has a Value of 1."
  - RGB, YBR_FULL, YBR_PARTIAL_420, YBR_ICT, YBR_RCT: "… may be used only when Samples per Pixel (0028,0002) has a Value of 3." YBR_FULL_422 는 "The same as YBR_FULL except …" 라고만 적혀 YBR_FULL 처럼 3 샘플로 읽었다 (그 페이지가 문장을 반복하지 않는다).
  - 그 밖의 값: "Other Values are permitted if supported by the Transfer Syntax but the meaning is not defined by this Standard." HSV·ARGB·CMYK 는 "Retired."
- **PS3.5 표 6.2-1, VR CS** — https://dicom.nema.org/medical/dicom/current/output/chtml/part05/sect_6.2.html : "Leading or trailing spaces (20H) are not significant."

## 2. 값별 분류 (카드 항목 2) — 182e 의 기준

기준: **표준이 금지하는 조합 → `DICOM_INVALID`**, **표준이 허용하지만 이 API(회색 uint16 한 장)가 충실히 표현할 수 없는 값 → `UNSUPPORTED_FORMAT`**. 이 reader 는 SamplesPerPixel 이 1 이 아니면 이미 `UNSUPPORTED` 로 거절하므로, 아래 표는 SamplesPerPixel=1 에서의 분류다.

| PhotometricInterpretation | 표준의 말 | 결과 | 비고 |
|--------------------------|-----------|------|------|
| 부재, 빈 값 | Type 1 (표 C.7-11c) | **`DICOM_INVALID`** + 알림 | 새 검사 |
| MONOCHROME2 | 한 장의 흑백 평면, 최소값=검정 | 받음 | |
| MONOCHROME1 | 한 장의 흑백 평면, 최소값=**흰색** | 받음, **저장된 그대로** | 반전은 #235 의 별도 설계 결정 (카드 지시) |
| PALETTE COLOR | 값이 팔레트 표의 **색인** | **`UNSUPPORTED_FORMAT`** | 회색 레벨로 읽으면 색인을 밝기로 오해 |
| RGB, YBR_FULL, YBR_FULL_422, YBR_PARTIAL_420, YBR_ICT, YBR_RCT, SamplesPerPixel=1 | 3 샘플에만 사용 | **`DICOM_INVALID`** + 알림 | 표준이 금지한 조합 |
| 같은 값들, SamplesPerPixel=3 | 유효한 컬러 | `UNSUPPORTED_FORMAT` | 샘플 수 검사에서 먼저 걸림(기존) |
| XYB, 그 밖의 정의 없는 값, 폐기된 HSV·ARGB·CMYK | 의미 정의 없음/폐기 | `UNSUPPORTED_FORMAT` | XYB 의 샘플 수 문장은 읽지 않았다 |

알림 문구 (레인 간 계약, §5):
- `PhotometricInterpretation (0028,0004) is absent, empty or not a string (it is a Type 1 attribute and has no default)`
- `PhotometricInterpretation %s with SamplesPerPixel 1 (PS3.3 C.7.6.3.1.2: it may be used only when SamplesPerPixel is 3)`
- `PhotometricInterpretation %s (only MONOCHROME1 and MONOCHROME2 are supported)`
- `Rows (0028,0010) is absent, empty, not a number or zero (it is a Type 1 attribute and has no default)` / `Columns (0028,0011) …`

CS 값의 앞뒤 공백은 reader 가 다듬지 않는다. 내가 손으로 다듬는 코드를 넣었다가 **그 코드가 어떤 입력도 바꾸지 못함**을 반증에서 발견해 지웠다 — DCMTK 가 이미 정규화한다 (`" MONOCHROME2"`, `"MONOCHROME2 "` 모두 MONOCHROME2 로 읽힘, 시험 행 `monochrome2_padded`, `monochrome2_leading_space`).

## 3. Image Pixel Description Macro 의 Type 1 속성 전부 (카드 항목 4)

| 속성 | 타입 | 읽는가 | 부재·빈 값 | 사용 |
|------|------|--------|-----------|------|
| Samples per Pixel (0028,0002) | 1 | 예 (182c) | `DICOM_INVALID` + 알림 | ≠1 → `UNSUPPORTED` |
| **Photometric Interpretation (0028,0004)** | 1 | **아니오 → 이번에 읽음** | **이전: 성공. 이후: `DICOM_INVALID` + 알림** | 위 §2 표 |
| Rows (0028,0010) | 1 | 예 (`readImage`) | `DICOM_INVALID` (이전엔 알림 없음 → 이번에 알림 추가), 0 도 같음 | 출력 크기, J2K·JPEG LL 코드스트림 대조 |
| Columns (0028,0011) | 1 | 예 | 위와 같음 | 〃 |
| Bits Allocated (0028,0100) | 1 | 예 (182c) | `DICOM_INVALID` + 알림 | 182e 분류 |
| Bits Stored (0028,0101) | 1 | 예 (182c) | 〃 | 〃 |
| High Bit (0028,0102) | 1 | 예 (182c) | 〃 | 〃 |
| Pixel Representation (0028,0103) | 1 | 예 (182c) | 〃 | 1(부호 있음) → `UNSUPPORTED` |
| Planar Configuration (0028,0006) | 1C (샘플 > 1) | 아니오 | — | 샘플 > 1 은 읽기 전에 거절하므로 필요 없다 |
| Pixel Aspect Ratio (0028,0034) | 1C | 아니오 | — | 출력에 실리지 않는다 (**1:1 이 아닌 파일의 기하가 호출자에게 가지 않는다**, 이 카드는 건드리지 않음) |
| Smallest / Largest Image Pixel Value | 3 | 아니오 | — | 선택 속성 |
| Palette Color Lookup Table Descriptor 3개 | 1C (PALETTE COLOR) | 아니오 | — | PALETTE COLOR 는 거절하므로 필요 없다 |

빠진 Type 1 은 Photometric Interpretation 하나였다. Rows·Columns 는 코드는 맞았고 알림만 없었다.

## 4. MONOCHROME1 의 현재 상태 (카드 항목 2, 보고만)

- 반환 버퍼: MONOCHROME2 와 **같은 방식으로 저장된 값 그대로** (시험 `Scope_PhotometricInterpretationRefusalsNameTheValueAndMonochrome1IsReturnedAsStored` 가 기준 파일의 읽은 값과 같음을 단언).
- 호출자가 알 수 있는가: **아니오.** `XpeImageMetadata` 에 PhotometricInterpretation 이 없고(`modules/dicom/src` 에서 이 속성을 쓰는 곳은 `DicomWriter.cpp:147` 의 쓰기 하나뿐이었다), 알림도 오르지 않는다 (같은 시험이 `xpe_get_pending_alert_count() == 0` 을 단언). `dicom_api.h` 문서에 "MONOCHROME1 (no inversion, no indication)" 이라고 적혀 있었고 그것이 맞다.
- 결과: MONOCHROME1 파일을 읽은 호출자는 값의 의미가 반전(최소=흰색)임을 이 API 만으로는 알 수 없다. 반전 정책은 #235 의 설계 결정으로 남긴다.

## 5. 바뀌는 반환 코드와 알림 (레인 간 계약)

| 입력 | 이전 | 이후 |
|------|------|------|
| PhotometricInterpretation 부재·빈 값 | `XPE_OK` (회색 uint16) | `DICOM_INVALID` + 알림 |
| PALETTE COLOR, SamplesPerPixel=1 | `XPE_OK` (색인을 밝기로) | `UNSUPPORTED_FORMAT` + 알림 |
| RGB/YBR 계열, SamplesPerPixel=1 | `XPE_OK` | `DICOM_INVALID` + 알림 |
| XYB·정의 없는 값·폐기된 값 | `XPE_OK` | `UNSUPPORTED_FORMAT` + 알림 |
| Rows·Columns 부재 또는 0 | `DICOM_INVALID`, 알림 없음 | `DICOM_INVALID` + 알림 (코드 불변) |

클라이언트 확인 (`clients/`, 이 세션에서 코드 검색): `DICOM_INVALID`·`UNSUPPORTED_FORMAT` 는 열거형 선언에만 있고 분기·문구 고정이 없다. 182e 와 같은 결과다. GUI 는 실행하지 않았다.

## 6. 182e 범위로 지정된 JPEG LL 성분 수·부호 (카드 항목 5)

182e 에서 처리했다 (커밋 `de3a3ecc`, 182e 보고서 §9). 요약: JPEG LL 프레임 헤더에는 **부호 플래그가 없어** 대조할 값이 없다. 성분 수는 SOF 를 성분 3개로 다시 쓴 파일(조각 길이 포함)로 쟀고, reader 의 새 검사를 뺀 상태에서 `rc=-3 (PROCESSING_FAILED)` — DCMTK 가 불일치를 거부하긴 하나 코드가 원인을 가리키지 않아 reader 에 대조를 넣었다.

## 7. 증거

- **시험**: 3건 — `Scope_PhotometricInterpretationIsRequiredAndOnlyAMonochromePlaneIsReturned` (14개 값 × 3개 경로 = 42건, 건수를 단언), `Scope_PhotometricInterpretationRefusalsNameTheValueAndMonochrome1IsReturnedAsStored` (알림 문구와 MONOCHROME1 의 현재 상태), `Scope_AbsentOrZeroRowsAndColumnsAreRefusedWithAnAlertOnEveryPath` (Rows·Columns × 부재·0 × 3경로).
- **빨강 → 초록**: 구현 전 3건 모두 빨강 (`red_before_fix.txt`), 구현 후 리더 시험 89건 통과. 이번에는 시험을 먼저 만들고 빨강을 확인한 뒤 코드를 고쳤다.
- **반증** (`arms_check_removed.txt`): 새 검사를 하나씩 약화시켜 리더 시험 전체를 돌렸다.

  | 약화한 검사 | 빨강이 된 시험 |
  |-------------|----------------|
  | F1 PhotometricInterpretation 부재·빈 값 거부 | 값 표 시험 + 알림 시험 |
  | F2 RGB/YBR + 1 샘플은 위반 | 값 표 시험 + 알림 시험 |
  | F3 MONOCHROME1·2 만 반환 | 값 표 시험 + 알림 시험 |
  | F5 Rows 부재·0 알림 | Rows/Columns 시험 |
  | F6 Columns 부재·0 알림 | Rows/Columns 시험 |

  F4(CS 공백 다듬기)는 빨강이 되지 않았다. 앞 공백(`" MONOCHROME2"`)을 넣은 행을 추가해도 마찬가지였고, DCMTK 가 이미 정규화한다는 뜻이라 그 코드를 지웠다 (위 §2).
- **전체**: `ctest --test-dir build/ci-dicom` (`ctest_ci_dicom.txt`): `100% tests passed, 0 tests failed out of 236`, 건너뜀 1건(`DicomNetworkTest.CancelCStore_TerminatesOperation`, 이 카드와 무관한 기존 skip).

## 8. Gaps

- XYB 의 SamplesPerPixel 제약 문장은 읽지 않았다. 그래서 XYB 를 `UNSUPPORTED` 로 둔 것은 "위반이라고 확인하지 못한 값은 위반으로 단정하지 않는다"는 선택이다.
- YBR_FULL_422 가 3 샘플 전용이라는 것은 "YBR_FULL 과 같다"는 문장에서 읽은 것이다.
- Pixel Aspect Ratio 가 1:1 이 아닌 파일의 기하가 호출자에게 전해지지 않는 점은 확인만 하고 건드리지 않았다.
- 시험 파일은 합성(donor 의 태그 편집)이다. 실제 장비의 MONOCHROME1 파일은 쓰지 않았다.
- 알림 시험은 이 카드의 새 문구를 단언한다. `clients/` 밖의 소비자가 이 문구를 고정했는지는 모른다.

## 9. Residual-risk

- 이전에 `XPE_OK` 로 읽히던 PALETTE COLOR·부재 PI 파일이 이제 거절된다. 그런 파일이 실제로 흘러오는 경로가 있는지 모른다. 거절은 의도된 변화다 (이전 결과는 색인을 밝기로 읽은 틀린 영상이었다).
- MONOCHROME1 이 반전 없이, 표시 없이 나가는 문제는 그대로다 (#235).
