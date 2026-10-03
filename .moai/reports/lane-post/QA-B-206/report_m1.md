# QA-B-206 M1 — DICOM C10(짧은 timeoutMs)·C13(dataSize 0 쓰기) (#251)

카드: `.moai/lanes/post/inbox/QA-B-206.md` M1. 판정 근거: `.moai/reports/lane-post/QA-B-204/dicom_report.md` §1·§2.

## 0. 결과

| 항목 | 결과 |
|---|---|
| C10 | `timeoutMs` 를 올림 초(최소 1)로 DCMTK 에 넘긴다. 정체 피어에서 300·999 ms → 각 ~1 s, 1400 ms → ~2 s (전엔 100 s, 700·999 도 40 s 넘게, 1500 은 1 s 로 일찍 끝남) |
| C13 | PixelData 길이를 항상 width×height×2 로 쓴다. dataSize 0 은 영상 전체를 쓰고 읽으면 화소가 같다. 더 큰 dataSize 의 남는 바이트는 파일에 들어가지 않는다 |
| 시험 | `DicomTimeout.*` 3개(`test_dicom_failure_paths.cpp`), `DicomWriterTest` 4개(`test_dicom_writer.cpp`) |
| 반증 | C10 6팔 중 5팔 터짐 + 등가 변이 1개 + 관측 1개, C13 4팔 모두 터짐, 소스는 전부 바이트 단위 복원 |
| 검증(관측) | ci-dicom 전체 ctest(성능 시험 제외) 329개 통과·0 실패, doxygen 경고 0, 헤더 점검 20개 0건 |

## 1. C10

### 소스에서 인용한 원인
`DicomNetworkSCU.cpp` 의 C-STORE·C-FIND 두 곳 모두 `scu.setConnectionTimeout / setACSETimeout / setDIMSETimeout(static_cast<Sint32>(timeoutMs / 1000))` 였다. DCMTK 의 세 setter 는 초 단위만 받는다(`scu.h`: "ACSE timeout in seconds", "DIMSE timeout in seconds", "connection timeout in seconds"; 밀리초 형태 없음). 정수 나눗셈이라 1000 미만은 0, 1500 은 1.
- 0 초가 DCMTK 에서 무엇이 되는지(약 100 s 대기의 출처)는 소스까지 내려가 확인하지 않았다. 관측은 §2 의 수치다(Gap).

### 수정
익명 네임스페이스에 `timeoutSeconds(ms) = max(1, ceil(ms / 1000))` (64비트로 계산해 큰 값에서 넘치지 않음)를 두고 두 곳이 같은 함수를 쓴다. 올림이라 타임아웃이 요청보다 일찍 터지지 않고, 작은 값은 작게 남는다. 밀리초 API 는 없으므로 카드의 둘째 선택지(올림, 최소 1초)다.

### `timeoutMs == 0` — 건드리지 않았다
헤더는 "(0 = no timeout)" 이라 약속한다(`dicom_api.h`, C-STORE·C-FIND 둘 다). 코드는 0 이면 setter 를 부르지 않아 DCMTK 기본값이 남고, 정체 피어에서 **30.02 s 뒤 NETWORK_FAILED**(QA-B-204 측정)였다. 즉 헤더의 약속("무제한")과 동작(30 초 뒤 실패)이 다르다. 카드 지시에 따라 동작을 바꾸지 않았다. 선택지는 ① 헤더를 "0 = DCMTK 기본(약 30 s)" 으로 고친다, ② 정말 무제한으로 구현한다(정체 피어에서 영원히 멈추고 `xpe_dicom_cancel` 로도 못 풀기 때문에 권하지 않음 — QA-B-200 M2a 가 "DCMTK 호출은 중단할 수 없다" 고 적었다). 리더 결정.

### 시험 (`DicomTimeout.*`, 수락 후 침묵하는 소켓 피어)
- 피어는 TCP 연결을 받고 읽지도 쓰지도 않는다. 6 s 감시 장치가 연결을 닫아, 타임아웃이 안 먹으면 시험이 매달리지 않고 실패한다.
- 각 호출의 경계: 요청 ms 이상 (`seconds ≥ ms/1000`), 올림 초 + 1 s 이하.
- 값: C-STORE 300·999, 1000·1400, C-FIND 300. 1400 은 내림(1 s)·반올림(1 s)·올림(2 s)을 모두 가려내는 값이다.
- 수정 전 RED(`c10_red_before_fix.txt`): 300·999 와 C-FIND 300 은 감시 장치가 닫을 때까지(6 s) 대기, 1400 은 1.01 s 에 끝나 "요청보다 일찍". 수정 후 GREEN(`c10_green_after_fix.txt`): 2.04 s / 3.02 s / 1.01 s.

### 반증 (`arms_c10.py.txt`, `arms_c10_out.txt`)

| 끈 방어 | 빨개진 시험 |
|---|---|
| 내림, 최소 없음(원래 코드) | 셋 모두 |
| 내림 + 최소 1초 | 반올림 시험(1400 이 1 s 로 일찍) |
| 반올림 + 최소 1초 | 반올림 시험 |
| C-STORE 쪽만 내림으로 | C-STORE 두 시험 |
| C-FIND 쪽만 내림으로 | C-FIND 시험 |
| 올림 + 최소 1초 중 "최소" 제거 | 없음 — **등가 변이**: ms ≥ 1 이면 올림이 이미 ≥ 1 이라 최소는 ms = 0 의 안전판일 뿐이고, 호출부가 ms > 0 일 때만 부른다 |
| ACSE 만 설정(연결·DIMSE setter 제거) | 없음 — 관측: 이 피어(연결 수락 후 침묵)에서는 ACSE 대기가 호출을 끝낸다. 연결 단계·DIMSE 단계의 setter 는 이 시험이 보호하지 않는다(Gap) |

## 2. C13

### 규범과 카드 지시의 충돌, 리더 결정
카드는 "dataSize 0 거부" 였으나 api-spec "XpeImageBuffer.dataSize on input (normative, #123)" 과 `dicom_api.h` 는 "dataSize == 0 은 unspecified, 진입점이 width×height×bpp 를 신뢰하고 받아들인다" 이다. 리더가 확인해 "거부하지 말고 PixelData 길이를 항상 width×height×2" 로 확정했다. 결함의 실체는 쓰기가 PixelData 길이로 `dataSize` 를 그대로 쓴 것이다.

### 수정
`DicomWriter::write` 가 `putAndInsertUint8Array` 의 길이로 `width * height * sizeof(uint16_t)` 를 쓴다. 공개 진입점이 이미 UINT16 이 아닌 형식과 이 크기보다 작은 dataSize 를 거부하므로(`dicom.cpp`) 2 바이트 가정이 안전하다. J2K 쓰기는 `compressJ2K` 가 width×height 로 계산해 영향이 없다. 헤더 `xpe_dicom_write` 의 `@param img` 에 "파일에는 dataSize 와 무관하게 정확히 width×height×2 바이트" 를 적었다.

### 측정(수정 전, QA-B-204 와 이번 시험의 이전 상태)
| 입력 | 결과 |
|---|---|
| dataSize = width×height×2 (대조군) | PixelData 128 바이트(8×8), 읽기 OK |
| dataSize = 0 | 쓰기 OK, PixelData 길이 0, `xpe_dicom_read_image` = `DICOM_INVALID` |
| dataSize = 128 보다 작음(100) | `INVALID_INPUT`(규범대로 — 검사는 쓰기 전에 있다) |
| dataSize 가 더 큼(surplus) | 수정 전에는 surplus 가 PixelData 에 들어가는 코드 경로였다(길이 = dataSize). 수정 전 파일의 길이를 직접 재지는 않았고, 반증 팔 1(원래 코드)에서 시험이 빨개지는 것으로 확인했다 |

dataSize 가 화소 수보다 작을 때(카드의 질문): 현재도 `XPE_ERR_INVALID_INPUT`, 파일 생성 전. 규범(#123: "dataSize != 0 이고 dataSize < width×height×bpp → INVALID_INPUT")대로이고, 새 시험이 1 바이트와 imageBytes − 1 을 확인하며 파일이 생기지 않는 것도 단언한다.

### 시험 (`DicomWriterTest`)
- `ZeroDataSizeWritesTheWholeImageAndTheFileReadsBackExactly`: PixelData 길이 = 256×256×2, `read_image` OK, dataSize·화소 `memcmp` 일치.
- `ZeroDataSizeWritesTheWholeImageForTheJ2kWriterToo`: J2K 도 읽어 화소 일치(통제 — 수정이 J2K 에 닿지 않음을 확인).
- `ASurplusBeyondTheImageInALargerBufferIsNotWrittenIntoPixelData`: 64 바이트 surplus(0xEE) 버퍼 → PixelData 길이 = 영상 크기, 읽은 화소 일치.
- `ADataSizeOfOneByteLessThanTheImageIsStillRefusedAndNoFileIsMade`: 1 과 imageBytes − 1 → `INVALID_INPUT`, 파일 없음.
- 기존 `DataSizeGuard_*_ZeroDataSize_Accepted` 는 `!= INVALID_INPUT` 만 보는 약한 단언이라 이 결함 아래서도 초록이었다. 지우지 않았고, 새 시험이 같은 입력의 파일 내용을 본다.

### 반증 (`arms_c13.py.txt`, `arms_c13_out.txt`)

| 끈 방어 | 빨개진 시험 |
|---|---|
| 길이를 다시 `dataSize` 로(원래 코드) | 0 시험, surplus 시험 |
| 길이 = dataSize, 0 일 때만 계산 | surplus 시험(0 시험은 초록 — 카드의 "0 일 때와 더 클 때 둘 다" 를 따로 가린다) |
| 화소당 1 바이트 | 0·surplus 시험과 기존 `WriteRoundTrip_PixelExact`, `EveryImageTheWriterAcceptsIsAnImageTheReaderReads` |
| 화소당 3 바이트(영상 밖을 읽음) | 0·surplus 시험 |

첫 시도에서 팔 1 이 "미사용 변수" 경고(오류 처리)로 빌드되지 않았고(`arms_c13_out.txt` 첫 블록), 변수를 계속 쓰는 형태로 바꿔 다시 돌렸다(같은 파일 끝 블록).

## 3. 영향 범위
- gui·clients: 이 모듈의 공개 헤더 시그니처 변경 없음. 동작 변화는 (a) 정체 피어에서 짧은 타임아웃이 이제 정말 짧음, (b) dataSize 0·surplus 로 쓴 파일이 올바른 길이가 됨. 클라이언트 쪽이 `timeoutMs` 를 1000 미만으로 보내는지, dataSize 를 0 으로 보내는지는 확인하지 않았다(Gap).
- 영상 출력(화소)은 바뀌지 않아 16비트 소비자 비교는 해당 없다. 쓰기 경로의 정상 입력(dataSize = 정확)은 같은 바이트를 쓴다(기존 왕복·정확 일치 시험이 초록).

## 4. Gap / 잔여 위험
Gap
- 0 초가 DCMTK 에서 약 100 s 대기가 되는 내부 이유를 소스에서 확인하지 않았다(수정은 그것에 의존하지 않는다: 올림 후 실측 1~2 s).
- 연결 단계·DIMSE 단계 타임아웃(침묵하는 피어가 아니라 연결은 되고 응답 도중 멈추는 피어)은 시험하지 않았다. 세 setter 가 모두 올림 초를 받는다는 것만 코드로 확인했다.
- `timeoutMs == 0` 동작(30 s)은 시험으로 고정하지 않았다(30 s 시험이라 부적합).
- Windows 전용 시험(`test_dicom_failure_paths.cpp` 는 이미 `windows.h` 를 쓴다). 다른 OS 에서의 소켓 시험은 관측하지 않았다.

잔여 위험
- 감시 장치 6 s 와 상한 "올림 초 + 1 s" 는 부하가 큰 기계에서 흔들릴 수 있다(관측: 1.01·2.01·3.02 s 로 안정적이었다, 한 번의 측정 묶음).
- 호출자가 1000 ms 미만을 의도적으로 "무제한 근처" 로 쓰고 있었다면 이제 1 s 에 끝난다(그런 호출자가 있는지 확인하지 않았다).
