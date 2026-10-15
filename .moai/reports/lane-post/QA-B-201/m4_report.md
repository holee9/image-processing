# QA-B-201 M4 — `xpe_dicom_write` 가 UINT16 이 아닌 영상을 입구에서 거부한다 (#251)

카드: `.moai/lanes/post/inbox/QA-B-201.md` M4(리더 추가 절). 관측은 M1 §3(별건): `xpe_dicom_write` 가 FLOAT32 영상을 `XPE_OK` 로 받아 32비트 파일을 쓰는데 이 모듈의 `xpe_dicom_read_image` 는 그 파일을 `BitsAllocated 32 (only 16 is supported …)` 로 거부한다. 요구 문구를 인용해 입구 거부와 변환 중 하나를 정하고 보고서에 적는다.

## 0. 결과

| 항목 | 내용 |
|---|---|
| 정한 것 | **입구 거부.** `xpe_dicom_write` 와 `xpe_dicom_write_j2k` 는 `XPE_PIXEL_UINT16` 이 아닌 영상을 `XPE_ERR_INVALID_INPUT` 으로 거부한다. 거부는 데이터셋을 만들기 전, 파일을 열기 전에 일어난다(목적지 파일이 이미 있으면 그대로). |
| 바뀐 코드 | `modules/dicom/src/dicom.cpp`: `pixel_format_is_writable` 추가, 두 쓰기 함수의 입구 검사에 한 줄씩. 그 결과 도달할 수 없게 된 `data_size_is_consistent` 의 FLOAT32·"크기를 모르는 형식" 분기를 제거(UINT16 만 남김). `dicom_api.h` 의 두 함수 설명(`@param img`, `@return`)에 형식 규칙을 적음. |
| 시험 | `test_dicom_writer.cpp`: 새 3개(FLOAT32 두 쓰기 함수 거부+파일 없음 / 거부 시 기존 파일 보존 / 쓰기가 OK 를 낸 영상은 reader 가 읽는다는 성질 시험) + 옛 `WriteUint8Format_NotRejectedBySizeGuard`(QA-B-29)를 새 계약으로 바꾼 1개. |
| 반증 | 5팔 모두 터졌고 소스는 바이트 단위로 복원(`m4_arms_out.txt`). |
| 검증(관측) | ci-dicom ctest 322개 통과·0 실패(이전 319 + 새 3), `DicomWriterTest.*` 26개 통과, doxygen 경고 0, 헤더 점검 20개 0건. |

## 1. 입구 거부와 변환 중 무엇이 요구에 맞는가

인용한 문구와 판단:
- **API 계약**(`dicom_api.h`): 두 쓰기 함수의 `@param img` 는 처음부터 "Source pixel buffer (XPE_PIXEL_UINT16)" 였다 — 계약은 UINT16 만 말했고 코드가 지키지 않았다.
- **REQ-DICOM-006**: reader 는 `outImg` 를 `XPE_PIXEL_UINT16` 으로 채운다. **REQ-DICOM-008**: 압축 데이터는 raw uint16 으로 푼다. 읽기 쪽 어디에도 32비트 화소는 없다 — 쓰기가 만든 32비트 파일은 이 모듈의 reader 가 정의상 받지 못한다(관측: `read rc=-7`).
- **REQ-DICOM-016**: 쓰기는 Pixel Data 속성(Bits Allocated 등)을 "`img` 의 속성에 맞게 올바르게" 정한다. FLOAT32 를 `Bits Allocated 32` 로 적는 것은 이 문구를 글자대로는 만족하지만, DX 영상 모듈의 `Bits Allocated`(0028,0100)에 대한 값 제한을 표준 본문에서 직접 확인하지는 못했다(PS3.3 의 DX Image 모듈 속성 목록에 그 속성이 있다는 것까지만 이번 조회로 확인했다) — 그 제한에 기대지 않았다.
- **REQ-DICOM-018** 은 NULL 만 `XPE_ERR_INVALID_INPUT` 으로 정하고, 형식 오류의 코드는 `xpe_error.h` 가 정의한다: `XPE_ERR_INVALID_INPUT` = "NULL pointer, **wrong pixel format**, or invalid parameter value". (`XPE_ERR_UNSUPPORTED_FORMAT` 은 "Pixel format or image dimensions not supported by this function" 로 읽힐 수도 있어 둘이 겹치지만, 카드가 `INVALID_INPUT` 을 지정했고 이 모듈의 다른 입구 검사(빈 영상, 크기 불일치)가 모두 `INVALID_INPUT` 이므로 맞췄다.)

변환(float → uint16)을 택하지 않은 이유: float 영상을 16비트 계수로 바꾸는 방법(반올림, 0 에서 자름, 창을 씌움)은 쓰는 쪽이 모르는 정책이다. gui `EnhanceBasicStage` 는 half-even 반올림 + `<0 → 0`, `>65535 → 65535` 로 하고, 레거시 clients 는 최솟값 기준 정규화를 한다 — 두 호출자가 이미 서로 다른 변환을 한다. 쓰기가 하나를 고르면 조용히 화소 값을 바꾸는 규칙이 하나 더 생긴다. 요구에 변환을 정한 문구도 없다.

## 2. 같은 결함이 UINT8 에도 있었다 (이번에 확인)
`XPE_PIXEL_UINT8` 은 선언된 형식인데 쓰기가 막지 않아 바이트가 16비트 워드로 읽혀 써졌고(옛 시험 `WriteUint8Format_NotRejectedBySizeGuard` 는 그 입구 통과를 오히려 단언하고 있었다 — "형식 검사가 할 일"이라는 주석과 함께, 형식 검사는 없었다), reader 는 `BitsAllocated 8` 파일을 받지 않는다. 이번 반증이 그것을 보인다: FLOAT32 만 막는 팔(`only_float32_is_refused_uint8_goes_on`)에서 성질 시험(`EveryImageTheWriterAcceptsIsAnImageTheReaderReads`)이 UINT8 행으로 빨개진다. 옛 시험은 새 계약으로 바꿨다(`WriteUint8Format_IsRefusedAtTheDoorByTheFormatCheck`).

## 3. 바뀐 시험과 새 시험 (`test_dicom_writer.cpp`)
- `WriteUint8Format_IsRefusedAtTheDoorByTheFormatCheck`: UINT8 영상(버퍼를 화소당 2바이트로 잡아 크기 가드는 통과시키고 **형식 검사만** 거부하도록)을 두 쓰기 함수 모두 `INVALID_INPUT` 으로 거부, 파일 없음. (처음 판은 버퍼가 화소당 1바이트라 크기 가드가 먼저 거부해 형식 검사를 끈 팔에서도 통과했다 — 반증이 잡아 고쳤다.)
- `AFloat32ImageIsRefusedByBothWritersAndNoFileIsMade`: −500..4000 의 float 영상(크기 일치)을 두 함수 모두 `INVALID_INPUT`, 파일 없음(수정 전: `XPE_OK`).
- `ARefusedFormatLeavesAnExistingDestinationFileUntouched`: 목적지에 이미 있는 파일의 바이트가 거부 뒤에도 그대로.
- `EveryImageTheWriterAcceptsIsAnImageTheReaderReads`: 형식 셋(UINT16·FLOAT32·UINT8)마다 쓰기가 OK 이면 `xpe_dicom_open`·`xpe_dicom_read_image` 가 OK 여야 한다. UINT16 행이 대조군(쓰기 OK·읽기 OK). 결함이 깬 결합(쓰기는 받고 reader 는 거부)을 직접 단언한다.

## 4. 반증 (`m4_arms_driver.py.txt`, `m4_arms_out.txt`)

| 끈 방어 | 빨개진 시험 |
|---|---|
| `xpe_dicom_write` 의 형식 검사 제거 | FLOAT32 거부, 기존 파일 보존, 성질 시험, UINT8 거부 |
| `xpe_dicom_write_j2k` 의 형식 검사 제거 | FLOAT32 거부, UINT8 거부 |
| FLOAT32 만 거부(UINT8 통과) | 성질 시험(UINT8 행), UINT8 거부 |
| UINT8 만 거부(FLOAT32 통과) | FLOAT32 거부, 기존 파일 보존, 성질 시험 |
| 모든 형식 거부(UINT16 까지) | 성질 시험의 UINT16 대조 행과 정상 쓰기를 쓰는 기존 시험 여러 개 |

## 5. 호출자 영향 (코드를 읽은 것, 실행하지 않음)
- gui `GuiDicomNative.cs`: 항상 `Format = UInt16`, `BitsAllocated 16`, `DataSize = 개수 × sizeof(ushort)` 로 호출 — 영향 없음.
- 레거시 clients `NativePresentationExportService.cs`: `RunDicomWriteAndValidate(… ref buffer …)` 가 앞 단계의 버퍼를 받는다. 쓰기 앞의 표시 사슬이 `xpe_apply_presentation_lut` 이고(그 함수는 출력 형식을 `UINT16` 으로 바꾼다, `presentation_lut.cpp`) 소스에 "presentation-lut 없이 DICOM 쓰기를 요구하면 그것을 넣는다"는 보정이 있으므로(`:415`) 쓰기에 닿는 버퍼는 UINT16 이다. 이 줄들은 읽었고 실행하지는 않았다.
- `PipelineOrchestrator.cs` 의 DICOM 쓰기는 TODO 스텁이다(호출하지 않음).

## 6. Gap / 잔여 위험
Gap
- 호출자 영향(§5)은 코드 읽기이고 .NET 시험은 돌리지 않았다.
- DX 영상 모듈의 `Bits Allocated` 값 제한(표준 본문)을 확인하지 못해 인용하지 않았다(§1).
- `xpe_dicom_write` 가 `bitsAllocated`·`bitsStored` 필드를 UINT16 영상에서 어떻게 해석하는지는 바꾸지 않았다(예: UINT16 인데 `bitsAllocated = 8` 인 모순된 구조체는 이번 범위가 아니다).
- 리더가 문서에 반영할 것: `api-spec.md` 의 쓰기 함수 설명(UINT16 만, 그 외 `XPE_ERR_INVALID_INPUT`)과 REQ-DICOM 에 형식 규칙 한 줄.

잔여 위험
- float 영상을 DICOM 으로 쓰던 호출자가 있었다면 이제 실패한다(`XPE_ERR_INVALID_INPUT`). 지금 코드베이스에서는 찾지 못했고, 그런 호출이 낸 파일은 이 모듈의 reader 가 읽지 못했다.
