# QA-B-182e — 182d 의 빈 칸 2건: 표준으로 먼저 확인 (#235)

Refs #235

## 0. 먼저 읽을 것

**리더의 짐작은 현행 표준과 다르다.** "비압축에서 HighBit≠BitsStored−1 은 유효한 DICOM 이지만 우리가 지원하지 않는 것"이 아니다. PS3.5 8.1.1 은 **압축 여부와 무관하게 모든 Pixel Data** 에 대해 `High Bit (0028,0102) shall be one less than Bits Stored (0028,0101)` 이라고 적는다 (PS3.5 2014c 부터. 그 전에는 제한이 없었다). 그래서 이 불일치는 **어느 경로에서든 표준 위반 → `DICOM_INVALID`** 이고, 비압축·JPEG LL 이 `UNSUPPORTED_FORMAT` 을 내던 쪽이 틀렸다. 182d 가 J2K 에서 `DICOM_INVALID` 로 정한 것은 맞았다.

| 질문 | 답 |
|------|----|
| 비압축/JPEG LL 의 `UNSUPPORTED` vs J2K 의 `DICOM_INVALID` 는 정당한 구분인가 | **아니다.** 표준 위반은 모든 경로에서 `DICOM_INVALID`. 비압축·JPEG LL 의 해당 칸을 고쳤다. |
| JPEG LL 비트스트림 정밀도 P 를 BitsStored 와 대조해야 하는가 | 표준이 "consistent" 를 요구한다 (PS3.5 8.2). 그러나 **무엇이 일치인지는 적지 않았다.** 확실히 불일치인 경우(P < BitsStored, P > BitsAllocated)만 디코드 전에 거부했다. P > BitsStored 는 거부하지 않는다 (아래 §3). |
| 리더 결정이 필요한 것 | JPEG LL 에서 P ≠ BitsStored 를 J2K 처럼 **정확히 같아야 하는 것으로** 볼지(§3). 지금은 더 느슨하다. |

## 1. 표준 원문 (출처 URL, 전체 인용은 `standard_quotes.txt`)

현행 판(페이지 머리말 "2026d")에서 가져왔다.

- **PS3.5 8.1.1** — https://dicom.nema.org/medical/dicom/current/output/chtml/part05/chapter_8.html
  - "Bits Stored (0028,0101) shall never be larger than Bits Allocated (0028,0100)."
  - "Bits Allocated (0028,0100) shall either be 1, or a multiple of 8. High Bit (0028,0102) shall be one less than Bits Stored (0028,0101)."
  - Note 3: "Formerly, High Bit (0028,0102) was not restricted to be one less than Bits Stored (0028,0101) in this Part, or in the general case, though almost all Information Object Definitions in PS3.3 imposed such a restriction. See PS3.5 2014c."
- **PS3.5 8.2** (JPEG, 표 8.2.1-2) — https://dicom.nema.org/medical/dicom/current/output/chtml/part05/sect_8.2.html
  - "…requires that the Data Elements that are related to the Pixel Data encoding (e.g., … Bits Allocated, Bits Stored, High Bit, Pixel Representation, Rows, Columns, etc.) shall contain Values that are consistent with the characteristics of the compressed data stream."
  - 표 8.2.1-2, JPEG Lossless (.4.57, .4.70), MONOCHROME: Bits Allocated **8 or 16**, Bits Stored **1-16**, High Bit **0-15**.
  - "The Pixel Data characteristics included in the JPEG Interchange Format shall be used to decode the compressed data stream."
- **PS3.5 8.2.4** (JPEG 2000, 표 8.2.4-1) — https://dicom.nema.org/medical/dicom/current/output/chtml/part05/sect_8.2.4.html
  - 같은 "consistent" 문장. MONOCHROME: Bits Allocated **1, 8, 16, 24, 32 or 40**, Bits Stored **1-38**, High Bit **0-37**.

## 2. 분류 — 경로 × 경우 (표준에 근거해 다시 쓴 표)

규칙: **표준이 금지하는 값 → `DICOM_INVALID`**. **표준이 허용하지만 이 reader 가 돌려줄 수 없는 값 → `UNSUPPORTED_FORMAT`**.

| 경우 | 근거 | 비압축 | JPEG Lossless | JPEG 2000 |
|------|------|--------|---------------|-----------|
| High Bit ≠ Bits Stored − 1 | PS3.5 8.1.1 | **INVALID** (이전: UNSUPPORTED) | **INVALID** (이전: UNSUPPORTED) | INVALID (182d 에서 이미) |
| Bits Stored > Bits Allocated 또는 0 | 8.1.1 | **INVALID** (이전: UNSUPPORTED) | **INVALID** (이전: UNSUPPORTED) | INVALID |
| Bits Allocated 가 1 도 8의 배수도 아님 (12, 15…) | 8.1.1 | **INVALID** (이전: UNSUPPORTED) | **INVALID** (이전: UNSUPPORTED) | **INVALID** (이전: UNSUPPORTED) |
| Bits Allocated 가 그 전송 구문의 표에 없음 (LL: 8·16 밖, J2K: 1·8·16·24·32·40 밖) | 표 8.2.1-2 / 8.2.4-1 | — (표 없음) | **INVALID** (이전: UNSUPPORTED) | **INVALID** (이전: UNSUPPORTED) |
| Bits Allocated 가 허용되지만 돌려줄 수 없음 (비압축: 1·8·24·32…, LL: 8, J2K: 1·24·32·40) | 8.1.1 / 표 | UNSUPPORTED (그대로) | UNSUPPORTED (그대로) | UNSUPPORTED (그대로) |
| 스트림 정밀도 P < Bits Stored 또는 P > Bits Allocated | 8.2 "consistent" | — | **INVALID (새 검사)** | INVALID (182b 의 코드스트림 대조, 정확히 같아야 함) |

이 표 밖은 건드리지 않았다. SamplesPerPixel≠1, PixelRepresentation=1 은 표준이 허용하고 이 reader 가 지원하지 않는 값이라 `UNSUPPORTED` 그대로다. PhotometricInterpretation 은 QA-B-182f 의 범위다.

**순서 변화**: 위반 검사(INVALID)가 미지원 검사(UNSUPPORTED)보다 먼저 온다. 그래서 `BitsAllocated=32` 인데 High Bit 이 틀린 파일은 이전엔 `UNSUPPORTED`, 이제는 `INVALID` 다 (위반이 표준 문장에 먼저 걸린다).

## 3. JPEG Lossless 정밀도 (카드 항목 3) — 무엇을 근거로 무엇을 읽는가

- **읽는 것**: 첫 조각(fragment)의 SOF 프레임 헤더 `length(2) precision(1) height(2) width(2)` 의 precision P. 치수는 이미 같은 헤더에서 읽고 있었다 (`jpeg_frame_dimensions`, #150).
- **근거**: PS3.5 8.2 의 "consistent with the characteristics of the compressed data stream" 과 표 8.2.1-2 아래의 "The Pixel Data characteristics included in the JPEG Interchange Format shall be used to decode…".
- **하는 것**: 디코드·할당 전에 P < BitsStored (선언한 값을 담을 수 없음) 또는 P > BitsAllocated (선언한 컨테이너에 안 맞음)이면 `DICOM_INVALID` + 알림 `JPEG Lossless stream sample precision %u does not fit the dataset (BitsStored %u, BitsAllocated %u)`.
- **하지 않는 것**: P > BitsStored 는 거부하지 않는다. 표준은 "consistent" 가 정확한 일치(P == BitsStored)인지 포괄(P ≥ BitsStored)인지 적지 않았다. J2K 는 182b 에서 `prec == BitsStored` 로 정했는데, JPEG LL 에도 그렇게 하면 더 넓은 P 로 인코딩된 파일(예: P=16 에 BitsStored=12)을 거부하게 된다. **그런 파일이 실제로 있다는 것은 이 세션에서 증명하지 못했다** — 시험은 P=16 / BitsStored=12 / HighBit=11 이 읽힘을 단언해 현재 동작을 고정할 뿐이다. 엄격하게(J2K 와 같게) 할지는 리더 결정이다.

## 4. 바뀌는 반환 코드와 알림 문구 (레인 간 계약)

| 입력 | 경로 | 이전 | 이후 |
|------|------|------|------|
| HighBit≠BitsStored−1, BitsStored 0 또는 > BitsAllocated | 비압축, JPEG LL | `UNSUPPORTED_FORMAT` | `DICOM_INVALID` |
| BitsAllocated 가 1 도 8의 배수도 아님 | 모든 경로 | `UNSUPPORTED_FORMAT` | `DICOM_INVALID` |
| BitsAllocated 가 LL 의 {8,16} 밖 (1, 24, 32, 48…) | JPEG LL | `UNSUPPORTED_FORMAT` | `DICOM_INVALID` |
| BitsAllocated 가 J2K 표 밖 (예: 48) | JPEG 2000 | `UNSUPPORTED_FORMAT` | `DICOM_INVALID` |
| 스트림 정밀도 P 가 BitsStored 보다 작거나 BitsAllocated 보다 큼 | JPEG LL | 읽힘 (DCMTK 가 디코드) | `DICOM_INVALID` |

알림 문구:

| 이전 | 이후 |
|------|------|
| `BitsStored %u with HighBit %u (the significant bits must be the low ones: HighBit = BitsStored - 1, BitsStored 1..16)` (비압축·LL) | `BitsStored %u with HighBit %u and BitsAllocated %u (PS3.5 8.1.1: HighBit shall be BitsStored - 1, and BitsStored 1..BitsAllocated)` (모든 경로 하나) |
| `BitsStored %u with HighBit %u for JPEG 2000 (HighBit must be BitsStored - 1, BitsStored 1..BitsAllocated %u)` (182d) | 위와 같은 한 문구 |
| (없음) | `BitsAllocated %u (PS3.5 8.1.1: it shall be 1 or a multiple of 8)` |
| (없음) | `BitsAllocated %u for JPEG Lossless (PS3.5 Table 8.2.1-2 lists 8 and 16)` |
| (없음) | `BitsAllocated %u for JPEG 2000 (PS3.5 Table 8.2.4-1 lists 1, 8, 16, 24, 32 and 40)` |
| (없음) | `JPEG Lossless stream sample precision %u does not fit the dataset (BitsStored %u, BitsAllocated %u)` |

변하지 않은 문구: `BitsAllocated %u for JPEG 2000 (8 or 16 are supported)`, `BitsAllocated %u (only 16 is supported for uncompressed and JPEG Lossless data)`.

클라이언트 확인 (`clients/`, 이 세션에서 읽음): `DICOM_INVALID`·`UNSUPPORTED_FORMAT` 는 `PInvokeWrapper.cs` 의 열거형 선언에만 있고 이 코드들로 분기하거나 알림 문구를 고정한 곳이 없다 (`HighBit`·`dicom read refused` 도 시험 포함 0건). DICOM 읽기 호출은 `XpeDicomReadinessProbe.cs` 의 준비 상태 점검뿐이다. 이 확인은 코드 검색이며 GUI 를 실행하지 않았다.

## 5. 기존 시험이 고정한 코드를 바꾼 곳 (카드 항목 2의 보고 의무)

| 시험 | 이전 기대 | 이후 | 이전 근거 |
|------|-----------|------|-----------|
| `Scope_OnlySixteenBitAllocationIsReadFromNativePixelData`, BitsAllocated=12 | `UNSUPPORTED_FORMAT` | `DICOM_INVALID` | 주석이 12 를 "well-formed file this reader cannot return" 로 적었다. 8.1.1 이 "1 또는 8의 배수" 만 허용하므로 12 는 well-formed 가 아니다. 1·8·32 는 `UNSUPPORTED` 그대로. |
| `Scope_BitsStoredAndHighBitMustDescribeLowBitsOfA16BitWord`, 불일치 5건 | `UNSUPPORTED_FORMAT` | `DICOM_INVALID` | 182b 에서 "표준 위반인지 미지원인지"를 가르지 않고 비압축을 미지원으로 둔 것. 8.1.1 이 위반으로 정한다. |

나머지 기존 시험(J2K 의 `tags_alloc_32`/`tags_alloc_1` 등 UNSUPPORTED 기대)은 바꾸지 않았고 그대로 통과한다.

## 6. 증거

- **시험**: 새 시험 2건 — `Scope_BitsAttributesAreViolationOrUnsupportedAsThePixelDataEncodingRulesSay` (13개 비트 조합 × 3개 경로에서 판정 불가능한 2건(J2K 의 16/12/11, 8/8/7)을 뺀 37건을 실행하고, 실행 수가 30 이상임을 단언), `Scope_JpegLosslessPrecisionIsComparedWithBitsStoredBeforeDecoding` (P=16 대조군, P=12·8·20 거부 + 알림에 `precision` 명시 + 출력 버퍼 미변경, P=16/BitsStored=12 는 읽힘).
- **빨강 → 초록**: 정밀도 시험은 구현 전 빨강 (`red_precision_before_fix.txt`). **분류 시험은 코드를 먼저 고친 뒤에 써서 구현 전 빨강 기록이 없다** — 대신 아래 반증이 빨강 증거다.
- **반증** (`arms_check_removed.txt`): 새 검사를 하나씩 약화시켜 리더 시험 85건 전체를 돌렸다.

  | 약화한 검사 | 빨강이 된 시험 |
  |-------------|----------------|
  | R1 BitsAllocated 1 또는 8의 배수 | 분류 시험 + `Scope_OnlySixteenBit…` |
  | R2 BitsStored 범위 · HighBit=BitsStored−1 | J2K HighBit 시험 + 분류 시험 + `Scope_BitsStoredAndHighBit…` |
  | R3 J2K Bits Allocated 표 | 분류 시험 |
  | R4 JPEG LL Bits Allocated 표 | 분류 시험 |
  | R5a 정밀도 < BitsStored | 정밀도 시험 |
  | R5b 정밀도 > BitsAllocated | 정밀도 시험 |

  R3 은 첫 실행에서 약화 코드가 컴파일되지 않아(`/WX` 미사용 변수) 옛 바이너리가 돌았다. 그 결과는 버리고 컴파일되는 약화로 다시 돌렸다. 소스는 매번 원복했고 바이트 일치를 단언했다. 원복 후 85건 통과.
- **전체**: `ctest --test-dir build/ci-dicom` (`ctest_ci_dicom.txt`): `100% tests passed, 0 tests failed out of 232`, 건너뜀 1건(`DicomNetworkTest.CancelCStore_TerminatesOperation`, 이 카드와 무관한 기존 skip).

## 7. Gaps

- **PS3.3 C.7.6.3 의 BitsStored/HighBit 속성 정의는 인용하지 못했다.** 카드가 PS3.3 C.7.6.3.1 을 요구했으나 그 페이지는 PS3.5 를 가리키고, 내가 쓴 검색에서 해당 문장이 나오지 않았다. 규범 문장은 PS3.5 8.1.1 이다. PS3.3 의 속성 표에 8.1.1 보다 좁은 제한이 따로 있을 수 있으나 확인하지 않았다.
- 인용은 현행 판(2026d)이다. High Bit 규칙이 PS3.5 2014c 이전 판에서는 없었다는 것은 Note 3 의 문장에서 읽은 것이고 옛 판 본문을 보지는 않았다. **2014c 이전에 기록된 파일이 이제 `DICOM_INVALID` 가 될 수 있다.** 그런 실제 파일이 있는지는 모른다.
- 정밀도 대조는 SOF3(JPEG Lossless) 의 첫 프레임 헤더 한 곳만 본다. 헤더를 읽지 못하면(`false`) 거부하지 않는다 (치수 검사와 같은 규칙).
- 정밀도 시험은 donor 파일의 SOF 바이트를 직접 고쳐 만든 합성 파일이다. 실제 장비 파일은 없다.
- JPEG LL 에서 `Samples per Pixel`, `Pixel Representation` 의 표 대조(표 8.2.1-2 의 다른 열)는 하지 않았다.
- 표준 인용은 HTML→텍스트 변환 결과라 표 셀의 구두점이 평탄화되어 있다. 표의 숫자는 변환본에서 읽었다.

## 8. Residual-risk

- P > BitsStored 허용 (§3): 이 reader 가 받아들이는 파일 중에 표준이 "consistent" 로 보지 않는 것이 있을 수 있다. 반대로 엄격히 하면 실제 파일을 거부할 수 있다. 양쪽 모두 이 세션에서 실제 파일로 확인하지 못했다.
- 반환 코드가 `UNSUPPORTED` → `INVALID` 로 바뀌는 입력 범위는 §4 표가 전부이나, 이 표는 이 reader 의 검사 순서에서 도출한 것이고 모든 조합을 실행한 것이 아니다 (실행한 것은 시험의 13개 조합).
- 바뀐 알림 문구는 레인 간 계약이다. 이 저장소의 `clients/` 에는 고정한 곳이 없으나 다른 소비자는 모른다.

## 9. 추가 (182f 카드 항목 5, 182e 범위에 포함시킨 일): JPEG LL 성분 수·부호

182f 카드가 "182e 에서 코드스트림을 조작해 DCMTK 가 불일치를 거부하는지 실제로 확인"하라고 했는데 위 커밋에서 빠뜨렸다. 이 절이 그 확인이다.

- **부호**: JPEG Lossless 프레임 헤더(SOF3)에는 부호 플래그가 없다 (PixelRepresentation 은 데이터셋에만 있다). 스트림과 대조할 값 자체가 없다. J2K 는 코드스트림이 부호를 담아 182b 에서 대조했다.
- **성분 수 (Nf)**: 시험 파일은 donor JPEG LL 의 SOF 를 성분 3개를 선언하도록 다시 쓴다 (헤더 길이와 성분 명세 추가, 조각 item 의 길이도 같이 증가 — 처음에는 item 길이를 고치지 않아 파일이 깨졌고 `DcmSequenceOfItems: Parse error in sequence (7fe0,0010)` 가 나왔다. 그 상태의 결과는 버렸다). 데이터셋은 SamplesPerPixel=1 그대로다.
- **측정**: reader 의 새 검사를 뺀 상태에서 이 파일을 읽으면 **`rc=-3` (`XPE_ERR_PROCESSING_FAILED`)**. 즉 DCMTK 는 불일치를 거부하지만 코드가 "내부 알고리즘 실패"라 원인을 가리키지 않고, 알림도 없고, 디코드를 이미 시작한 뒤다.
- **조치**: reader 가 SOF 의 Nf 를 읽어 1 이 아니면 디코드·할당 전에 `XPE_ERR_DICOM_INVALID` + 알림 `JPEG Lossless stream carries %u components, the dataset says 1 (SamplesPerPixel)`. 카드의 "거부하지 않으면 대조를 넣는다" 조건과는 다르게 DCMTK 가 거부는 하지만, 코드와 시점을 바로잡는 쪽이 낫다고 판단해 넣었다. 코드가 `PROCESSING_FAILED` → `DICOM_INVALID` 로 바뀌는 입력이다 (레인 간 계약: 성분 수가 SamplesPerPixel 과 다른 JPEG LL, 이전 `PROCESSING_FAILED`, 이후 `DICOM_INVALID`. §4 표에는 옮기지 않고 이 절에만 적었다).
- **시험**: `Scope_JpegLosslessComponentCountIsComparedWithSamplesPerPixelBeforeDecoding` — 성분 1 대조군은 읽힘, 성분 3 은 `DICOM_INVALID` + 출력 버퍼 미변경 + 알림에 `component`. 검사를 지우면 `rc=-3` 으로 빨강. 리더 시험 86건 통과, `ctest ci-dicom` `100% tests passed, 0 tests failed out of 233`.
- **Gaps**: 성분 수 불일치의 다른 방향(스트림 1개, 데이터셋 3 → 이미 SamplesPerPixel≠1 로 UNSUPPORTED)은 조작하지 않았다. 처음 깨진 파일은 `rc=0` 으로 읽혔는데(조각 길이가 틀린 상태) 그것이 reader 의 결함인지는 조사하지 않았다 — 한 번 관측했고 재현 파일을 남기지 않았다.

## 10. 정정 (QA-B-182f 에서 추가)

§7 의 첫 Gap("PS3.3 C.7.6.3 의 BitsStored/HighBit 속성 정의는 인용하지 못했다")은 **해소됐다.** PS3.3 표 C.7-11c Image Pixel Description Macro (https://dicom.nema.org/medical/dicom/current/output/chtml/part03/sect_C.7.6.3.3.html) 가 같은 규칙을 직접 적는다: "Bits Allocated (0028,0100) shall be either 1, or a multiple of 8." / "High Bit (0028,0102) shall be one less than Bits Stored (0028,0101)." PS3.5 8.1.1 과 같은 문장이라 위 분류는 바뀌지 않는다. 원문은 `.moai/reports/lane-post/QA-B-182f/standard_quotes.txt` [1].
