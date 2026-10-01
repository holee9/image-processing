# QA-B-182b — Codex #39 보류 3건: 필수 태그, 비트 깊이, J2K 코드스트림 검증 (#235)

Refs #235

## 0. 먼저: QA-B-182 증거 파일이 비어 있었다

`ec0759f1`(QA-B-182)에 올린 `.moai/reports/lane-post/QA-B-182/report.md` 와 `arm_dic_*.txt` 6개가 **각 1바이트(빈 파일)** 였다. 커밋 메시지는 거기에 반증 기록이 있다고 적었으므로 틀린 서술이다. 원인은 파일을 쓴 뒤 정리 단계에서 내용이 지워진 것으로 보이며, 커밋 전에 크기를 확인하지 않았다. 원본은 복구할 수 없어 기억으로 다시 쓰지 않았다. `QA-B-182/report.md` 를 이 사실을 적은 정정 기록으로 바꿨고(남은 증거 두 파일은 원본 그대로), 그 카드의 반증 주장은 이 카드에서 **현재 코드를 대상으로 새로 실행한** 반증(5절)으로 대체해 읽는다. 이번 카드의 증거 파일 크기는 커밋 전에 확인했다(아래 8절).

## 1. 주장

| # | 주장 |
|---|------|
| C1 | `SamplesPerPixel`·`PixelRepresentation`·`BitsAllocated` 의 부재·빈 값은 `XPE_ERR_DICOM_INVALID` 다. `NumberOfFrames` 의 부재만 1 프레임이다. |
| C2 | 비압축·JPEG Lossless 는 `BitsAllocated == 16`, `BitsStored ≤ 16`, `HighBit == BitsStored − 1` 만 읽는다. 그 밖(1·8·12·32비트 포함)은 `XPE_ERR_UNSUPPORTED_FORMAT`. |
| C3 | J2K 는 출력 할당·디코드 전에 코드스트림 헤더를 검사한다. 모순이면 거부하고, 성공 시 출력은 `UINT16`, `bitsAllocated = 16`, `bitsStored = 정밀도` 다. |
| C4 | 8비트 J2K 예외를 측정했고 **유지한다**: 단일 컴포넌트 무부호 8비트 코드스트림은 원본 0..255 와 화소가 모두 같다. |
| C5 | 이 규칙이 생기자 모듈 자신의 writer 가 만든 12비트 J2K 파일을 reader 가 거부했다. writer 가 선언과 다른 정밀도(16)로 인코딩하고 있었기 때문이며, writer 를 고쳤다. |

## 2. 규칙과 근거

### 2.1 필수 태그 (리더 결정 + 규격)

PS3.3 C.7.6.3.1.1 의 Image Pixel 매크로에서 세 속성은 Type 1 이다. 기본값이 없다. 부재·빈 값은 파일이 깨졌다는 뜻이라 `DICOM_INVALID`, 값이 읽히지만 지원 밖이면 `UNSUPPORTED_FORMAT`. 검사 순서는 필수 속성 → 프레임 수 → 지원 여부라 둘이 섞여도 코드가 안정적이다.

### 2.2 J2K 의 태그·코드스트림 일치 (규격 확인)

PS3.5 8.2.4 (https://dicom.nema.org/medical/dicom/current/output/chtml/part05/sect_8.2.4.html, 이 세션에서 가져와 확인): 압축 데이터에 쓰는 경우 "Samples per Pixel, … Bits Allocated, Bits Stored, High Bit, Pixel Representation, Rows, Columns 등이 **압축 데이터 스트림의 특성과 일관된 값**을 가져야 하고, JPEG 2000 비트 스트림에 들어 있는 특성이 디코딩에 쓰인다". 표 8.2.4-1 은 MONOCHROME 에서 `BitsAllocated` 1, 8, 16, 24, 32, 40 을 허용한다. 이 카드는 그중 8·16 만 읽는다(출력이 16비트 무부호 버퍼).

코드 결정(리더가 "어느 코드인지 정하고 이유를" 요청):

| 상황 | 코드 | 이유 |
|------|------|------|
| 코드스트림의 컴포넌트 수·부호·정밀도가 태그와 다름 | `DICOM_INVALID` | 한 파일의 두 부분이 서로 모순 — 크기 불일치(`#150`)와 같은 부류로 둔다 |
| 정밀도 > 16 | `UNSUPPORTED_FORMAT` | 코드스트림 자체가 16비트 버퍼에 담길 수 없다 |
| `BitsStored > BitsAllocated` | `DICOM_INVALID` | 태그 자체가 불가능한 값 |
| J2K 의 `BitsAllocated` 가 8·16 이 아님, `SamplesPerPixel ≠ 1`, 부호 있음 | `UNSUPPORTED_FORMAT` | 형식이 올바르고 이 reader 가 못 돌려줌 |

검사는 `opj_read_header` 뒤, `opj_decode` 와 출력 할당 앞이다.

### 2.3 writer 정정 (C5)

`DicomWriter::compressJ2K` 는 정밀도를 항상 16 으로 인코딩하면서 `BitsStored` 는 영상이 선언한 값(예: 12)으로 썼다. 위 규격 문장으로는 비일관 파일이다. 정밀도를 선언한 `BitsStored`(1..16, 아니면 16)로 맞췄고, 값이 그 정밀도를 넘으면 인코딩하지 않고 `XPE_ERR_PROCESSING_FAILED` 로 실패시킨다(선언을 어기는 데이터를 그대로 인코딩하지 않기 위해). 기존에는 12비트 선언 + 16비트 데이터도 쓰였다 — 이 동작이 바뀐다.

### 2.4 문서 표현 정정 (Codex 지적)

"핸들 내부 상태가 비트 단위로 불변" 은 DCMTK 의 지연 적재 때문에 과한 표현이었다. `dicom_api.h` 와 `DicomReader.cpp` 주석을 "`outImg` 는 건드리지 않고, 핸들은 계속 쓸 수 있으며 같은 호출이 같은 답을 낸다(외부 API 상태·재호출 결과가 보존된다). 내부 파싱 상태는 약속하지 않는다" 로 바꿨다.

## 3. 변경

- `modules/dicom/src/DicomReader.cpp`: `checkSupportedImageModule(ds, isJ2K)` 재작성, `checkJ2kCodestreamShape`(신규), `decodeJ2KBitstream` 이 헤더 검사 뒤 출력 기술자를 16/정밀도로 설정
- `modules/dicom/src/DicomWriter.cpp`: J2K 정밀도 = `BitsStored`
- `modules/dicom/include/xpe/dicom/dicom_api.h`: 반환 코드·규칙·표현 정정
- `modules/dicom/tests/test_dicom_reader.cpp`: 시험 재작성·추가(아래), `ReadScope` 가 출력 기술자도 기록
- `modules/dicom/CMakeLists.txt`: 시험 exe 가 `openjp2` 에 링크(변형 코드스트림을 시험 안에서 인코딩)

## 4. 증거

### 4.1 수정 전 빨강 (`red_before_fix_reader.txt`)

새 시험 10개를 먼저 추가하고 옛 reader 로 실행: 6개 빨강 — 필수 태그 부재가 `0`(OK)으로 읽힘, 비트 1·12·32 가 OK, `HighBit` 불일치 5건이 OK, J2K 변형 6건(컴포넌트 2·부호 있음·정밀도 불일치·태그 단독 거부)이 OK, 8비트 J2K 가 `bitsAllocated 8` 로 기술. 나머지(옛 시험 + 12비트 J2K·writer 왕복)는 초록(통제).

### 4.2 수정 후 (`green_after_fix_scope.txt`)

`DicomReaderTest.Scope_*:J2kScope_*` 17/17 초록. dicom 전체 직렬 ctest: `100% tests passed, 0 tests failed out of 214` (`after_full_ci_dicom_ctest.txt`; QA-B-182 의 205 에서 +9: 새 시험 +10, 대체 −1). 빌드 `warning C` 0건.

### 4.3 8비트 J2K 측정 (C4)

`J2kScope_EightBitUnsignedSingleComponentIsReadAndDescribedAsSixteenBit`: 256×256 무부호 8비트 단일 컴포넌트를 시험 안에서 OpenJPEG 로 인코드(값 0..255 램프), 태그 `8/8/7`, 읽은 결과는 `UINT16`, `bitsAllocated 16`, `bitsStored 8`, **65536개 화소 중 다른 것 0개**. 같은 방식의 12비트(`16/12/11`)도 정밀도와 화소를 보존한다. 측정이 통과했으므로 8비트 J2K 는 거부로 바꾸지 않는다.

### 4.4 시험 목록

| 시험 | 단언 |
|------|------|
| `Scope_AbsentOrEmptyRequiredAttributesAreMalformed` | 필수 셋 각각 부재·빈 값 → INVALID, 출력 불변, 같은 핸들 메타데이터 정상 |
| `Scope_AbsentNumberOfFramesIsASingleFrame` | `NumberOfFrames` 부재 → 정상 단일 프레임, 화소 동일 |
| `Scope_OnlySixteenBitAllocationIsReadFromNativePixelData` | `BitsAllocated` {1, 8, 12, 16, 32}: 16 만 OK(통제), 그 밖 UNSUPPORTED. 실제 32비트 크기 데이터도 거부 |
| `Scope_BitsStoredAndHighBitMustDescribeLowBitsOfA16BitWord` | (16,15)(12,11)(1,0) OK / (12,15)(12,12)(16,14)(17,16)(0,0xFFFF) UNSUPPORTED |
| `J2kScope_OwnSixteenBitFileReadsAndIsDescribedAsSixteenBit` | 통제: 기존 16비트 J2K, `UINT16`/16/16, 비압축 파일과 같은 화소 |
| `J2kScope_EightBit…` / `J2kScope_TwelveBit…` | 4.3 |
| `J2kScope_ACodestreamThatContradictsTheTagsIsRefused` | 변형 6종: 컴포넌트 2(INVALID), 부호 있음(INVALID), 정밀도 12·8 인데 태그 16(INVALID), 정밀도 16 인데 태그 12(INVALID), 정밀도 20(UNSUPPORTED). 각각 출력 불변·메타데이터 정상 |
| `J2kScope_TheTagsAloneRejectWhatNoCodestreamCouldFix` | `BitsAllocated` 32·1, RGB, 부호 있음(UNSUPPORTED), `BitsStored > BitsAllocated`(INVALID) |
| `J2kScope_AnImageWrittenWithTwelveBitsStoredIsReadBack` | writer 12비트 왕복(C5) |

기존 `Pinned_Issue235AwaitsDesignDecision_*` 3개와 나머지 `Scope_*` 는 그대로 통과(`BitsStored 12 / HighBit 11` 고정 시험도 새 규칙에 맞는다).

## 5. 반증 (현재 코드, 검사 하나씩 끔 — `arms_summary.txt`, `arm_*.txt`)

검사 조건을 실행 시점에 거짓인 스위치(`&& g_armOn`)와 묶어 경고 없이 끄고, 빌드하고(`build_ok=True` 전부), `Scope_*`·`J2kScope_*` 를 돌려 빨개지는 시험을 기록한 뒤 소스를 되돌렸다(최종 파일이 원본과 바이트 동일함을 비교로 확인):

| 끈 검사 | 빨개진 시험 |
|---------|-------------|
| 필수 속성 부재/빈 값 | `Scope_AbsentOrEmptyRequiredAttributesAreMalformed` |
| 여러 프레임 | `Scope_MultiFrameIsUnsupportedNotTheFirstFrame` |
| 표본 수 | `Scope_RgbIsUnsupportedNotByteSoup`, `J2kScope_TheTagsAlone…` |
| 부호 있는 화소 | `Scope_SignedPixelsAre…`, `Scope_ARefusedHandleRefusesTheSameWayAgain`, `J2kScope_TheTagsAlone…` |
| 비압축 비트 규칙 | `Scope_BitsStoredAndHighBit…`, `Scope_EightBitIsUnsupported…`, `Scope_OnlySixteenBit…` |
| J2K `BitsAllocated` 8/16 | `J2kScope_TheTagsAlone…` |
| 코드스트림 컴포넌트 수 | `J2kScope_ACodestreamThatContradicts…` |
| 코드스트림 부호 | `J2kScope_ACodestreamThatContradicts…` |
| 코드스트림 정밀도 > 16 | `J2kScope_ACodestreamThatContradicts…` |
| 코드스트림 정밀도 = BitsStored | `J2kScope_ACodestreamThatContradicts…`, `J2kScope_TheTagsAlone…` |
| writer 수정 되돌림 (`arm_old_writer_16bit_precision_under_12bit_tag.txt`) | `J2kScope_AnImageWrittenWithTwelveBitsStoredIsReadBack` — `-13`(DICOM_INVALID): 자기 파일을 자기 reader 가 거부 |

각 검사가 자기 시험 하나 이상을 빨갛게 한다. (코드스트림 정밀도 > 16 반증이 같은 시험을 빨갛게 하는 것은 그 시험 안의 정밀도 20 변형 때문이다.)

## 6. Gaps (미검증)

- `HighBit` 은 비압축 규칙에서만 본다. J2K 의 `HighBit` 은 검사하지 않았다.
- `BitsStored`·`HighBit` 부재는 이전 기본값(`BitsStored = BitsAllocated`, `HighBit = BitsStored − 1`)을 유지했다. 둘도 Type 1 이라 규격으로는 거부 대상이다. 리더 결정에 없어 바꾸지 않았다 — **결정이 필요하다.**
- 시험 데이터는 DCMTK 로 만든 변형과 OpenJPEG 로 만든 코드스트림이다. 실제 장비·다른 인코더(컴포넌트 부표본화, 다중 타일, ROI 등)의 코드스트림은 보지 않았다.
- JPEG Lossless 경로에 대한 규칙 적용은 비압축과 같은 함수(태그 규칙)로만 검증했다. JPEG Lossless 파일의 비트 깊이 변형을 별도로 만들어 돌리지 않았다.
- `modules/dicom` 밖에서 이 reader 에 먹이는 DICOM 이 Type 1 속성을 빠뜨린 채 만들어지는 곳이 있는지(예: 다른 레인의 시험·클라이언트 진단)는 찾아보지 않았다. 이 모듈의 writer 는 세 속성을 항상 쓴다.
- 이 세션에서 ci-dicom 트리(Ninja, RelWithDebInfo)로 짓고 돌렸다. CI 는 `coverage-dicom`(Debug)에서 직렬로 돌린다 — 그 구성으로는 실행하지 않았다.

## 7. Residual-risk (잔여 위험)

- 이전에는 읽히던 파일이 이제 거부된다: 필수 속성이 빠진 파일, 8·12비트 J2K 중 태그와 정밀도가 어긋난 파일, writer 가 쓴 "12비트 선언 + 16비트 정밀도" 파일(옛 writer 산출물). 옛 writer 로 이미 만든 J2K 파일이 현장에 있다면 `BitsStored < 16` 인 것은 새 reader 가 `DICOM_INVALID` 로 거부한다. 있는지 확인하지 않았다.
- writer 는 선언한 `BitsStored` 보다 큰 값이 있으면 이제 실패한다(이전에는 인코딩됨). 이 동작에 기대는 호출자가 있다면 깨진다.
- `BitsAllocated` 8 의 J2K 는 읽히고 16 으로 기술되지만, 그 영상을 8비트로 기대하는 소비자는 `bitsStored 8` 로 의미를 읽어야 한다.

## 8. 증거 파일 크기 (커밋 전 확인)

이 디렉터리의 모든 `*.txt`·`report.md` 가 비어 있지 않음을 크기로 확인한 뒤 커밋했다(`ls -la` 의 바이트 수: 가장 작은 것 1095바이트).
