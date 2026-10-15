# QA-B-206 M1b — Codex #108 보류 2건 (DICOM 쓰기 입구) (#251)

회신 전문: 메인 저장소 `.moai/state/codex-archive/108.md`. 앞선 M1: `report_m1.md`(df67fe3a).

## 0. 결과

| 항목 | 결과 |
|---|---|
| (1) 높음: PixelData 길이 32비트 곱 | 길이를 64비트로 계산(`pixel_data_bytes`)하고, 파일이 표현할 수 없는 크기(Rows/Columns > 65535, 또는 PixelData > 0xFFFFFFFE 바이트)는 공개 쓰기 두 함수의 입구와 `DicomWriter::write`/`writeJ2K` 안쪽에서 파일 생성 전에 `XPE_ERR_INVALID_INPUT`. 좁히는 것은 검사 뒤에만 |
| (2) 보통: bitsAllocated 상충 | 공개 쓰기 두 함수가 `bitsAllocated == 16` 과 `bitsStored 1..16` 만 받는다. **0 은 거부**(아래 근거) |
| 시험 | `DicomImageLimits` 1개 + `DicomWriterTest` 3개 |
| 반증 | 10팔 중 8팔 터짐, 2팔은 관측(안쪽 검사가 문 하나를 대신함), 소스 전부 바이트 단위 복원 |
| 검증(관측) | ci-dicom 전체 ctest(성능 시험 제외) 333개 통과·0 실패, doxygen 경고 0, 헤더 점검 20개 0건 |

## 1. 높음 — 길이 오버플로

### 재현(수정 전 코드 모양)
M1 의 `pixelBytes` 는 `unsigned long`(MSVC 에서 32비트)의 곱이었다. Codex 의 입력 65535×32769(UINT16): 필요한 길이 4,295,032,830 바이트, 32비트 곱은 65,534. 이 계산은 시험으로 고정했다: `pixel_data_bytes(65535, 32769) == 4295032830` 이고 같은 값의 하위 32비트가 65,534(`DicomImageLimits.*` 의 첫 두 단언 — 후자는 "32비트로 하면 이렇게 보인다" 는 대조군).

### 수정
- 새 헤더 `modules/dicom/src/DicomImageLimits.h`(헤더 인라인, 한 곳의 정의): `kMaxRowsOrColumns = 0xFFFF`, `kMaxPixelDataBytes = 0xFFFFFFFE`, `pixel_data_bytes`, `image_size_is_representable`, `image_bits_are_writable`.
- 한계의 근거: Rows `(0028,0010)`·Columns `(0028,0011)` 은 US(16비트). OB/OW 길이 필드는 32비트이고 `0xFFFFFFFF` 는 "정의되지 않은 길이"라 가장 긴 값은 짝수 `0xFFFFFFFE`. DCMTK `putAndInsertUint8Array` 의 길이 인자는 `unsigned long`(Windows 32비트)이라 그보다 길면 표현할 수 없다. 이 한계의 이름 있는 정확한 DCMTK 상수는 이번에 소스에서 인용하지 않았다(Gap): 기준은 DICOM 길이 필드의 폭이다.
- `dicom.cpp`: 공개 쓰기 두 함수에 `size_is_representable`·`bits_are_writable` 문을 추가(순서: 널 → 비어 있음 → 형식 → 비트 서술자 → 표현 가능 크기 → dataSize 일관성).
- `DicomWriter.cpp`: `write()` 와 `writeJ2K()` 시작에서도 같은 술어를 확인(공개 함수 밖의 호출자가 생겨도 안전), `pixelBytes` 는 검사 뒤에만 `unsigned long` 으로 좁힌다. 정상 크기의 쓰기는 전과 같은 바이트를 쓴다.
- 폭·높이의 한계는 카드의 `DCMTK 가 표현할 수 없는 크기` 에 포함해 `Rows`/`Columns` > 65535 도 같은 문에서 거부한다(QA-B-199 C12 의 "Rows/Columns 를 범위 검사 없이 Uint16 으로 변환" 의 남은 조각이기도 하다).

### 시험 — 큰 버퍼를 할당하지 않는다
- 거부 경로(`AnImageTheFileCannotDescribeIsRefusedBeforeAnyFileExistsByBothWriters`): 32바이트 버퍼와 `dataSize = 0`(미지정) 으로 65535×32769, 46341×46341, 65536×1, 1×65536, 0x7FFFFFFF×2 를 두 함수에 준다. 모두 `INVALID_INPUT`, 파일 없음. 거부가 앞서지 않으면 쓰기는 버퍼를 한참 넘어 읽는다 — 그래서 이 시험은 거부가 맨 앞에 있다는 것까지 보호한다.
- 경계(`DicomImageLimits.*`): 헤더 인라인 술어를 직접 호출해 경계 직전·직후를 확인(실제로 쓸 수 없는 크기를 할당하지 않고). 46340×46340(통과, 4,294,791,200 바이트) / 46341×46341(거부), 65535×32768(통과, 4,294,901,760 바이트) / 65535×32769(거부), 65536×1·1×65536(거부), 0xFFFFFFFF×0xFFFFFFFF(거부), 0 인 쪽(거부).
- 정확히 0xFFFFFFFE 바이트인 이미지는 만들 수 없다(0xFFFFFFFE / 2 = 2,147,483,647 은 소수라 폭×높이로 쪼갤 수 없다). 한계 바로 아래의 가장 큰 곱을 위 두 쌍으로 대신했다.

## 2. 보통 — 비트 서술자

### 약속이 있는가 (카드의 조건)
- `docs/project/api-spec.md` 88·89행: `bitsAllocated` "Storage bit depth (e.g., 16)", `bitsStored` "Valid bit depth (e.g., 14)" — 0 에 대한 말이 없다.
- `xpe_types.h` 86·87행: "Bits allocated per pixel (16 for UINT16, 32 for FLOAT32)", "Bits stored (… may be <= bitsAllocated)" — 0 이 기본값이라는 말이 없다.
- `dicom_api.h` 의 `xpe_dicom_write` 문서와 SPEC REQ-DICOM-016("`img` 의 속성에 맞게 올바르게 설정") — 0 의 뜻 없음.
- 코드의 `bitsAllocated > 0 ? bitsAllocated : 16` 은 약속이 아니라 구현의 암묵 기본값이었다.
→ 카드 규칙대로 **0 은 거부**.

### 호출자 영향(읽어서 확인한 것)
`gui/.../GuiDicomNative.cs` 는 `BitsAllocated = 16`, `BitsStored = 16` 을 보낸다. `clients/` 의 쓰기 경로(`NativePresentationExportService.cs`)에서 이 필드를 0 으로 두는지는 확인하지 못했다(Gap). 이 저장소의 C++ 시험은 `xpe_alloc_image` 가 16/16 을 채우므로 전체 ctest 가 그대로 초록이다(333/333).

### 시험
- `ADescriptorThatContradictsTheSixteenBitWordsIsRefusedByBothWritersAndNoFileIsMade`: (allocated, stored) = (8,8)(Codex 의 재현), (8,16), (32,16), (0,16), (16,0), (16,17), (16,32) 를 두 함수에 → `INVALID_INPUT`, 파일 없음.
- `EveryBitsStoredFromOneToSixteenIsWrittenAndReadsBack`(통제): bitsStored 1..16 모두 쓰고 모듈 reader 로 읽어 화소 `memcmp` 일치.

## 3. 반증 (`arms_m1b.py.txt`, `arms_m1b_out.txt`)

| 끈 방어 | 결과 |
|---|---|
| 비트 문을 두 공개 쓰기 함수에서 제거 | 서술자 시험 빨강 |
| bitsAllocated 8 도 허용 | 서술자 시험 빨강 |
| bitsStored 0 허용 | 서술자 시험 빨강 |
| bitsStored 32 까지 허용 | 서술자 시험 빨강 |
| bitsAllocated 0 을 16 으로(옛 암묵 기본값) | 서술자 시험 빨강 |
| bitsStored 1..15 만 허용(허용 범위를 좁힘) | 통제 시험과 16/16 을 쓰는 기존 시험 다수 빨강 |
| 곱을 다시 32비트로 | 경계 시험·거부 시험 빨강 |
| Rows/Columns 65535 한계 제거 | 경계 시험·거부 시험 빨강 |
| 크기 문을 공개 쓰기 두 함수에서만 제거(안쪽 검사 유지) | 빨간 시험 없음 — 관측: `write()`/`writeJ2K()` 안쪽 검사가 문 하나를 대신해 같은 거부가 나온다(깊이 방어) |
| 크기 문 + `write()` 안쪽 검사 제거(J2K 안쪽만 남음) | 거부 시험 빨강(비압축 쪽이 막히지 않음) — 이 팔은 거부가 앞서지 않으면 작은 버퍼를 넘어 읽으므로 크래시 가능성이 있었지만 이번 실행은 끝까지 돌았다 |

## 4. 영향·비고
- 공개 헤더 시그니처 불변. 동작 변화: ① 4 GiB 이상·Rows/Columns > 65535 입력과 ② bitsAllocated ≠ 16, bitsStored ∉ 1..16 입력이 쓰기 전에 `INVALID_INPUT`.
- `timeoutMs == 0` 헤더 문구와 M2 항목(E4·D2·C14·C5·C6)은 이 커밋에 없다. 작업 트리에 E4·D2·헤더 문구가 미커밋으로 있어 `dicom_api.h` 는 M1b 부분만 부분 스테이징했다.

## 5. Gap / 잔여 위험
Gap
- DCMTK 의 PixelData 길이 한계를 소스의 상수로 인용하지 않았다(DICOM 길이 필드 폭 0xFFFFFFFE 로 판단).
- 한계 크기의 이미지를 실제로 쓰는 경로는 시험하지 않았다(할당 불가) — 통과 쪽 경계는 술어로만 확인.
- `clients/` 가 bitsAllocated 0 을 보내는지 미확인.
- 정확히 한계 바이트인 이미지는 구성할 수 없어(소수) 마지막 짝수 경계는 두 쌍으로 근사.

잔여 위험
- 이전에 bitsAllocated 0 으로 쓰던 호출자가 있다면 이제 `INVALID_INPUT` 을 받는다(의도한 변경, 확인한 호출자는 없음).
- 크래시 가능성이 있는 반증 팔(문 + 안쪽 검사 둘 다 제거)은 이번에 크래시하지 않았지만 보장은 아니다.
