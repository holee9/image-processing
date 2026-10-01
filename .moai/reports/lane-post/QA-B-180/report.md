# QA-B-180 — DICOM 읽기가 부호·리스케일·광도·프레임·샘플 해석을 무시하는지 실측

조사 카드. 제품·시험 코드 변경 없음. 프로브(Python)는 저장소에 넣지 않았다.

## 1. 결론

gui 의 코드 읽기 추론(`DicomReader` 는 `PixelRepresentation`, `RescaleSlope/Intercept`, `PhotometricInterpretation`, `NumberOfFrames`, `SamplesPerPixel` 을 읽지 않고 16비트 단어를 그대로 복사)은 **실측으로 확인됐다.** 코드로도 같다: `DicomReader::readImage` 가 읽는 태그는 `Rows`, `Columns`, `BitsAllocated`, `BitsStored` 뿐이고 `std::memcpy` 한 번으로 끝난다(`DicomReader.cpp:274-275`, `:379`). 그 결과 아래 5개 입력이 **오류 없이 틀린 화소**를 돌려주고, 1개는 틀린 이름의 오류를 돌려준다.

| 입력 | 읽기 결과 | 판정 |
|------|-----------|------|
| (a) 부호 없는 MONOCHROME2 | 원본과 단어 일치 | **이미 맞음** |
| (b) `PixelRepresentation=1` 음수 포함 | 두 보수 단어를 부호 없는 값으로 (-1000 → 64536). 형식은 `UINT16`, 부호 정보 없음 | **결함** (조용한 오류) |
| (c) MONOCHROME1 | 단어 그대로 (반전 없음, 표시 정보 없음) | **결함** (조용한 오류) |
| (d) `RescaleSlope=2`, `Intercept=-1024` | 저장 단어 그대로. 기울기·절편을 알릴 수단이 ABI 에 없음 | **결함** (조용한 오류, 모달리티 의존) |
| (e) 3프레임 | 첫 프레임만, 오류 없음, 프레임 수 알림 없음 | **결함** (조용한 자료 손실) |
| (f) RGB (`SamplesPerPixel=3`, 8비트) | `OK`, 바이트 쌍이 16비트 값으로 합쳐진 쓰레기 | **지원 범위 밖 → 오류 코드로 거부해야** |
| (g) 8비트 회색조 | `XPE_ERR_DICOM_INVALID` (-13, "PixelData is short") | **거부는 맞지만 이름이 틀림**: 손상이 아니라 미지원 |
| (h) `BitsStored=12` 이고 상위 4비트에 값 | 상위 비트까지 그대로 | **결함** (경미) |

## 2. 방법

1. 시험 파일: 프로브가 DICOM Part 10 파일(Explicit VR Little Endian, 파일 메타 + 데이터셋)을 **바이트 단위로 직접 만든다**(임시 디렉터리, 저장소에 `.dcm` 없음). 모듈 쓰기 API(`xpe_dicom_write`) 왕복으로도 대조군을 하나 만들었다. 손으로 만든 파일이 올바른 DICOM 이라는 근거: 모듈의 DCMTK 기반 읽기가 (a) 에서 행·열·비트·화소를 정확히 읽었고, 모듈 자신의 쓰기 출력을 같은 경로로 읽은 결과와 같다.
2. 읽기: `ctypes` 로 `build/ci-dicom/bin/xpe_dicom.dll` 의 `xpe_dicom_open` → `read_image` → `get_metadata` → `close`. 사례당 별도 프로세스. 영상은 4×4, 값 16개(`VALS`).
3. 판정 기준: "올바른 읽기가 뷰어에 넘길 값" 을 사례마다 정해 비교했다.

## 3. 사례별 결과 (`probe_dicom_read_cases.txt`)

`VALS` = `[0, 1, 100, 1000, 4095, 4096, 20000, 32767, 32768, 40000, 50000, 60000, 65000, 65534, 65535, 12345]`. 모든 사례에서 `open`, `get_metadata` 는 0(OK), 메타데이터 `flags` 는 0 이고, `XpeImageMetadata` 에는 광도·부호·기울기·절편·프레임 수를 담을 필드가 없다(`xpe_types.h`: `bodyPart`, `kVp`, `mAs`, `SID_mm`, `pixelPitch_mm`, `acquisitionTime`, `flags`).

| 사례 | 파일 내용 | `read_image` | 영상 필드 | 화소 (앞 16개) | 올바른 값 |
|------|-----------|--------------|-----------|----------------|-----------|
| a | `VALS`, 부호 없음, MONOCHROME2 | 0 | 4×4, 16/16비트, `UINT16` | `VALS` 와 같음 | `VALS` |
| a2 | 모듈 `xpe_dicom_write` 가 쓴 파일 | 0 | 같음 | `VALS` 와 같음 | `VALS` |
| b | int16 `[-32768, -1000, -1, 0, 1, 1000, 32767, -2, 5, -5, 100, -100, 2, -3, 3, -4]`, `PixelRepresentation=1` | 0 | 4×4, 16/16비트, `UINT16` | `[32768, 64536, 65535, 0, 1, 1000, 32767, 65534, 5, 65531, 100, 65436, 2, 65533, 3, 65532]` | 부호 있는 값 (또는 알림) |
| c | `VALS`, MONOCHROME1 | 0 | 같음 | `VALS` 그대로 | `65535 - VALS` (낮은 값이 밝음을 반영) 또는 알림 |
| d | `VALS`, `RescaleSlope=2`, `RescaleIntercept=-1024` | 0 | 같음 | `VALS` 그대로 | `2*VALS - 1024` 또는 알림 |
| e | 3프레임 (프레임 k = `VALS + 1000k`) | 0 | 4×4, `dataSize` 32 | 프레임 0 만 | 오류 또는 프레임 3개 |
| f | 8비트 RGB `0..47`, `SamplesPerPixel=3` | 0 | 4×4, **bitsAllocated 8**, 형식 `UINT16` | `[256, 770, 1284, …]` (바이트 쌍 합침) | 오류 |
| g | 8비트 회색조 `0..15` | **-13** | — | — | 지원하거나 `UNSUPPORTED_FORMAT` |
| h | `BitsStored=12`, `HighBit=11`, 앞 8개에 상위 4비트 `0xF` | 0 | 4×4, bitsAllocated 16, **bitsStored 12** | 상위 비트 그대로 (`61440, 61441, …`) | 하위 12비트만 (`0, 1, 100, …`) |

## 4. 판정과 고칠 방향 (이번 카드에서 고치지 않음)

리더가 GUI 계획(#225 Open DICOM)과 묶어 정한다. 각 방향은 하나의 선택지이고 권고는 마지막 줄.

| 사례 | 분류 | 방향 |
|------|------|------|
| b 부호 | 결함 | 지원 범위 정의 전에는 `PixelRepresentation=1` 을 `XPE_ERR_UNSUPPORTED_FORMAT`(또는 `DICOM_CONFORMANCE`)로 거부. 지원하려면 부호 있는 값을 변환하고 그 사실을 ABI 로 알려야 한다. 지금의 `UINT16` 은 두 보수 단어를 부호 없는 값으로 둔갑시킨다 |
| c MONOCHROME1 | 결함, **가장 현실적** | X선 DX/CR 은 MONOCHROME1 이 흔하다. 읽을 때 `(2^BitsStored - 1) - v` 로 MONOCHROME2 로 정규화하거나(그리고 사실을 알림), 광도를 ABI 로 노출해 표시 단계가 반전. 무엇이든 "아무 신호 없음" 은 금지 |
| d 리스케일 | 결함, 모달리티 의존 | DX 는 항등이거나 태그가 없는 경우가 많아 영향이 작을 수 있으나 CT 류는 물리값이 달라진다. 최소: 기울기≠1 또는 절편≠0 이면 거부하거나 `flags` 에 알림. 완전: `XpeImageMetadata` 에 기울기·절편 필드를 추가해 호출자가 적용 |
| e 다중 프레임 | 결함 | `NumberOfFrames > 1` 은 거부(`UNSUPPORTED_FORMAT`)하거나 프레임 인덱스 인자를 가진 읽기 함수를 추가. 첫 프레임만 조용히 돌려주는 것이 가장 나쁘다 |
| f RGB | 지원 범위 밖 | `SamplesPerPixel != 1` 은 `XPE_ERR_UNSUPPORTED_FORMAT` 으로 거부 |
| g 8비트 | 이름이 틀린 거부 | `BitsAllocated != 16` 이면 `UNSUPPORTED_FORMAT`(또는 `DICOM_CONFORMANCE`)로 분리. 지금은 손상 파일과 구별되지 않고 호출자가 파일이 깨졌다고 판단한다. 8비트를 16비트로 넓혀 지원하는 선택도 있다 |
| h 상위 비트 | 결함(경미) | `BitsStored` 위의 비트는 DICOM 이 정의하지 않은 값이다. `(1 << BitsStored) - 1` 로 마스크. 영상 필드의 `bitsStored` 는 정확히 보고되므로 호출자가 마스크할 수는 있으나 모듈이 책임지는 편이 안전 |

**권고 우선순위**: c(MONOCHROME1, 현실 빈도 높음) > e, f(조용한 자료 손실/쓰레기, 거부만으로 닫힘) > b, d > g(코드만 정정) > h. e, f, g 와 b 의 "거부" 안은 한 곳(`readImage` 앞단의 검사)에서 함께 처리된다. c 와 d 는 ABI 에 정보를 어떻게 싣느냐는 설계 결정(메타데이터 필드 추가 vs 읽을 때 정규화)이 필요하다.

### 4.1 영향 범위의 한계

이 모듈의 읽기는 비압축(Explicit VR LE) 경로를 시험했다. 압축 전송 구문(J2K, JPEG-LL)에서는 DCMTK/OpenJPEG 가 해제하며 광도·부호 해석이 같은 방식으로 빠지는지, 또는 일부를 처리하는지는 **측정하지 않았다**.

## 5. 읽기 함수의 스레드 계약 (코드 + 실측)

**헤더에는 읽기 함수의 스레드 계약이 없다.** `dicom_api.h` 에서 스레드 안전성을 말하는 곳은 `xpe_dicom_cancel`(313행: "Thread-safe. May be called from any thread")뿐이다.

코드로 확인한 모듈의 전역 상태 (`modules/dicom/src`):

| 상태 | 위치 | 비고 |
|------|------|------|
| JPEG 코덱 등록 | `DicomReader.cpp:47-48`, `std::call_once` | DCMTK 의 전역 레지스트리를 한 번만 채움. 안전하게 보호됨 |
| 취소 플래그 | `DicomNetworkSCU::s_cancelRequested`, `std::atomic<bool>` | 네트워크 경로, 문서화된 대로 스레드 안전 |
| 그 밖 | 없음 (읽기·쓰기·검증은 정적 함수이거나 핸들 안의 상태) | 모듈 자신의 가변 전역은 위 둘뿐 |

DCMTK 내부의 전역(데이터 사전, UID 생성기의 상태 등)은 DCMTK 소관이고 **확인하지 않았다**.

실측 (`probe_dicom_concurrency.txt`, 스레드 8개):

| 사용 | 결과 |
|------|------|
| **서로 다른 핸들**: 스레드마다 열고 읽기 (스레드당 60회) × 3개 프로세스 | 불일치 **0 / 1440** |
| **같은 핸들 하나를 공유**해 `read_image` 동시 호출 (스레드당 80회) × 11개 프로세스 (7040회) | 불일치 **2건**(두 프로세스에서 각 1건): 반환 코드 `-13`(`DICOM_INVALID`) 이 간헐적으로 나옴. 화소 값 불일치는 없었음 |

해석: 핸들은 독립적으로 쓰면 안전해 보이고, **같은 핸들의 동시 사용은 안전하지 않다**(`readImage` 가 핸들의 `DcmFileFormat` 데이터셋을 가변 상태로 조회하며, 한 번 측정한 간헐 오류가 그 증거이다). 메커니즘(DCMTK 요소의 지연 로드/바이트 순서 변환 등)은 **확인하지 않았다**.

방향: 헤더에 계약을 적는다 — "서로 다른 핸들은 동시에 사용할 수 있고, 같은 핸들은 호출자가 직렬화한다". 선택적으로 핸들 안에 뮤텍스를 둬서 같은 핸들도 안전하게 할 수 있으나, 현재 호출 패턴(GUI 가 열고 읽고 닫음)에서는 계약 명시로 충분할 가능성이 높다.

## 6. 5절식 보고

**주장**: 1절 표.
**증거**: `probe_dicom_read_cases.txt`(사례 9개), `probe_dicom_concurrency.txt`(동시성), 코드 인용(`DicomReader.cpp:274-275, 379`, `dicom_api.h` 313행).
**귀속**: `build/ci-dicom/bin/xpe_dicom.dll`(x64, DCMTK/OpenJPEG vcpkg). 이 DLL 은 이번 세션에서 다시 빌드하지 않았다. 빌드 시각(2026-09-19)이 `modules/dicom` 을 바꾼 마지막 커밋(`8686d150`, 2026-09-17)보다 늦고 그 뒤 `modules/dicom` 변경이 없으므로 현재 소스(HEAD `e62ad8c2`)와 같은 코드로 본다(빌드 입력을 직접 대조하지는 않았다).
**Gaps (미검증)**:
- 압축 전송 구문의 읽기(4.1).
- 손으로 만든 파일은 DICOM 이 요구하는 모든 필수 태그를 담지 않았다(예: 환자·검사 모듈). 모듈의 읽기는 이를 요구하지 않는 것으로 관찰되었으나 `xpe_dicom_validate` 가 같은 파일을 어떻게 판정하는지는 보지 않았다.
- 실제 장비가 만든 MONOCHROME1 파일(PresentationLUTShape, WindowCenter/Width 와의 상호작용)은 사용하지 않았다.
- `RescaleType`, `PixelPaddingValue`, `SmallestImagePixelValue` 등 다른 해석 태그는 확인하지 않았다.
- 동시성은 Python 스레드(GIL 해제)로 8개까지 시험했다. 같은 핸들의 간헐 오류는 11개 프로세스 중 2번, 불일치 원인 코드 `-13` 만 확인했고 재현 확률이 낮아 더 정밀한 특성화는 하지 않았다.

**잔여 위험**: 4절의 "결함" 판정은 이 모듈이 *읽기만* 책임지는 관점이다. 호출자(GUI/파이프라인)가 DICOM 태그를 따로 읽어 반전·리스케일을 적용하고 있다면 (c)(d) 의 실제 피해는 더 작다. 그런 호출자가 있는지는 확인하지 않았다.
