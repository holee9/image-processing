# QA-B-50 게이트 보고서 — 압축 경로의 짧은 픽셀 데이터

**카드**: QA-B-50 (#150 #120)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-50/`
**커밋 1건**: `7de6e16`
**선행**: `git merge origin/main` — 이미 최신

---

## 1. 관측 — 두 경로 모두, 각각 다른 방식으로 계약을 벗어나 있었다

변경 전 측정(`_observation.log`, **BUILD=0**). 선언 `256×256`, 실제 코드스트림 `256×128`.
`outImg` 에는 호출자가 만들 수 없는 센티널(`4242/2424/777/99`)을 찍어 넣어, 리더가 건드린
필드가 드러나게 했다.

| 케이스 | 반환 코드 | 선언 | **반환된 크기** | `outImg.data` | `dataSize` | 0 아닌 픽셀 |
|---|---|---|---|---|---|---|
| `j2k-undersized` | **0 (XPE_OK)** | 256×256 | **256×128** | NON-NULL | 65536 | 32768 |
| `jpegll-undersized` | **0 (XPE_OK)** | 256×256 | 256×256 | NON-NULL | 131072 | **32768 / 65536** |
| `j2k-control-matched` | 0 | 256×128 | 256×128 | NON-NULL | 65536 | 32768 |
| `jpegll-control-matched` | 0 | 256×128 | 256×128 | NON-NULL | 65536 | 32768 |

대조군 두 줄이 **같은 제작 방식으로 선언 == 실제**인 파일이고 정상 판독된다.
따라서 위 두 줄의 이상은 **픽스처가 아니라 불일치 때문**이다.

**판정: 둘 다 B-49 계약(짧으면 `XPE_ERR_DICOM_INVALID`)과 다르다 → RED→GREEN.**

### 1.1 두 경로가 다르게 틀려 있었다 — 검출 지점이 갈린 이유

- **J2K**: 선언 크기를 **아예 보지 않는다.** 코드스트림 크기(`imgW`/`imgH`)로 버퍼를 만들고
  `XPE_OK` 를 돌려준다. 메타데이터의 `Rows`/`Columns` 를 믿는 호출자는 **받은 버퍼 밖을 읽는다.**
  `(void)rows; (void)cols;` 위에 있던 주석 — *"rows/cols are validated via OpenJPEG
  codestream header"* — 은 **사실이 아니었다.** 비교하는 코드가 없었다.
  (B-44 의 교훈과 같은 형태다: 물려받은 사유는 다음 실행이 반박해 주지 않는다.)
- **JPEG-LL**: B-49 가 비압축에서 막은 바로 그 **"아래 절반 검정" 영상**을 만든다.
  **B-49 가드가 못 본 이유**는 DCMTK 가 `Rows × Columns` 크기 버퍼에 압축을 풀어 넣기
  때문이다 — 가드가 볼 시점에는 부족분이 **이미 패딩으로 메워져 있어** `availBytes ==
  expectedBytes` 다. 실제 높이가 남아 있는 유일한 곳은 **JPEG 프레임 헤더(SOF 마커)** 다.

즉 **같은 해저드가 경로마다 다른 지점에서만 보인다.** 하나의 가드로 덮으려는 시도는
JPEG-LL 에서 이미 실패하고 있었다.

### 1.2 못 만든 것 — 없음

카드가 든 두 형태(압축 스트림이 적은 행을 담은 경우 / 디코더는 성공하나 산출이 부족한 경우)는
이 저장소에서 **같은 파일 하나로 동시에 성립한다** — 작은 영상을 정상 인코딩한 뒤 데이터셋의
`Rows` 만 키우면, 디코더는 성공하고 산출은 선언에 못 미친다. 따라서 별도 제작이 필요 없었고,
**만들지 못해 넘긴 형태는 없다.**

---

## 2. 변경 (`7de6e16`)

| 지점 | 판정 | 코드 |
|---|---|---|
| `decodeJ2KBitstream` (디코드 성공 직후) | `imgW < cols \|\| imgH < rows` | `DICOM_INVALID` |
| `readImage` JPEG-LL 분기 (**`chooseRepresentation` 이전**) | SOF 마커의 `frameW < cols \|\| frameH < rows` | `DICOM_INVALID` |

**디코더 내부 실패와 산출 부족을 같은 코드로 뭉개지 않았다**(카드 2항).
OpenJPEG 의 `opj_read_header` / `opj_decode` 실패는 그대로 `XPE_ERR_PROCESSING_FAILED`
— "디코드하지 못했다" 이다. 이번에 추가한 판정은 **"디코드는 됐는데 약속과 다르다"** 이므로
DICOM 일관성 결함, 즉 `DICOM_INVALID` 이다. 두 사건이 코드로 구분된다.

**출력 버퍼**: 두 판정 모두 `xpe_alloc_image` **이전**에 일어난다. 따라서 B-48 의
**"실패하면 outImg 를 전혀 쓰지 않는다"** 계약이 여기서는 **온전히 성립**하고, 단언도
그렇게 썼다(센티널 생존). B-49 의 비압축 경로가 약한 보장(`data=NULL, dataSize=0`)에
머물렀던 것과 다른 점이다 — 부족을 할당 전에 알 수 있느냐가 갈랐다.

**SOF 파서에 대해**: `jpeg_frame_dimensions()` 는 SOF 를 못 찾으면 `false` 를 돌려주고,
**호출자는 그 `false` 로 거절하지 않는다.** "프레임 헤더를 읽지 못했다" 와 "프레임이 작다" 는
다른 진술이기 때문이다 — 읽지 못한 것을 결함으로 세면 B-44 가 정정한 실수를 되풀이한다.

헤더(`dicom_api.h`)에도 압축 경로 계약을 적었다.

---

## 3. §3 판독 — 길이는 맞고 내용이 손상된 경우, 현재 어디까지 검출되는가

**시험하지 않았다. 코드 판독이다**(카드 3항: 고치지 말고 판독만).
다음 dispatch 나 별도 카드가 이 판독을 확인하거나 반박한다.

| 손상 위치 | 현재 검출 | 근거 |
|---|---|---|
| 파일 구조 (preamble, VR/길이 필드) | **검출됨** → `open()` 이 `DICOM_INVALID` | `loadFile` 이 DCMTK 파서를 거친다(`DicomReader.cpp:119`) |
| 메타 그룹 부재·불일치 | **검출됨** | B-36 이 넣은 meta 그룹 계약(`DicomValidator.cpp`) |
| 전송 구문 불일치 | **검출됨** | 허용 목록 대조(B-45 의 `kSupportedTransferSyntaxes`) |
| **비압축 픽셀 바이트의 값 손상**(길이 정상) | **검출 안 됨** | 무결성 검증 코드가 없다 — `modules/dicom/src/` 에 CRC·체크섬·해시 계산이 **한 건도 없다**(grep 결과 0) |
| **압축 스트림의 값 손상**(길이 정상) | **부분 검출** | 코드스트림이 깨지면 디코더가 실패해 `PROCESSING_FAILED`(B-47 이 실측). 다만 **디코드에 성공하는 손상**(엔트로피 코딩 안쪽의 비트 반전 등)은 통과한다 |
| 크기 불일치 | **검출됨** | 비압축 B-49, 압축 이번 카드 |

**요약**: HAZ-DCM-002 의 통제 세 가지 중 1·2(preamble·VR 길이)는 DCMTK 가, 3(손상 감지 시
즉시 에러)은 **"크기가 맞지 않는 경우" 까지만** 구현돼 있다. **값이 손상됐으나 길이가 맞는
파일은 어느 경로에서도 능동적으로 검출되지 않으며, 압축 경로에서 걸리는 것은 디코더가
우연히 실패해 주는 경우뿐이다.** DICOM 표준 자체가 픽셀 데이터 체크섬을 요구하지 않으므로
이것을 막으려면 저장·전송 계층(파일 해시, PACS 전송 무결성)의 통제가 필요하다 —
리더가 SHA 갱신 시 참고할 근거로 남긴다.

---

## 4. 반증 — 두 가드 모두 민감하다 (`_falsify.log`)

두 조건을 **지우지 않고 약화**시켰다(`* 2 <` — 절반 미만일 때만 거절. 픽스처가 정확히
절반이라 빠져나간다):

```
===BUILD=0===                    <- 빌드 성공. 낡은 바이너리가 아니다
[  FAILED  ] DicomReaderTest.J2kCodestreamSmallerThanDeclared_ReturnsDicomInvalid
[  FAILED  ] DicomReaderTest.JpegLosslessFrameSmallerThanDeclared_ReturnsDicomInvalid
[       OK ] DicomReaderTest.CompressedMatchedSize_StillReadsNormally
[       OK ] DicomReaderTest.KnownDivergence_CompressedLargerThanDeclared
===EXIT=1===
```

두 케이스가 **각각** 실패했고 대조군 2건은 통과했다 — 가드 하나가 둘을 다 덮는 것이
아니라 **둘이 따로 동작한다**는 것까지 보인다. 약화 형태를 고른 것은 B-49 와 같은 이유다
(조건 삭제 → 변수 미사용 → `/WX` 빌드 파손 → 낡은 바이너리가 통과를 찍음).
반증 뒤 제품 코드는 원복했다.

---

## 5. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 변경 전 관측 | 두 경로 모두 `rc=0`, 대조군 정상 | `_observation.log` (BUILD=0) |
| 변경 후 관측 | 두 경로 모두 `rc=-13`, **센티널 생존**, 대조군 정상 | `_observation_after.log` (BUILD=0) |
| 반대 방향 관측 | J2K `rc=0`(선언보다 큰 버퍼) / JPEG-LL `rc=-3` | `_observation_oversize.log` |
| GREEN | BUILD=0, 4/4 | `_green.log` / `_green2.log` |
| 반증 | BUILD=0, 대상 2건 재실패·대조군 2건 통과 = **재현됨** | `_falsify.log` |
| 이전 ctest | 465 / 209 / 168 | QA-B-49 `_verify.log` |
| 현재 ctest | **465 / 209 / 172** (신규 4건) | `_verify.log` |
| 빌드 경고 | 0 (`grep -c "warning C"`) | `_verify.log` |
| 변경 파일 | 3 (`DicomReader.cpp`, `dicom_api.h`, `test_dicom_reader.cpp`) | `git diff --stat` |

**이 변경으로 깨진 기존 단언은 없다** — 846건 중 실패 0.

---

## 6. 미검증 (Gaps)

- **반대 방향(선언보다 큰 경우)의 계약이 정해져 있지 않다.** 관측: J2K 는 `rc=0` 으로
  **선언보다 큰 버퍼**를 돌려주고, JPEG-LL 은 DCMTK 가 `rc=-3` 으로 거절한다.
  **같은 형태가 한 경로에서는 성공, 다른 경로에서는 실패다.** HAZ-DCM-002(빠진 데이터)가
  아니라 카드 범위 밖이므로 `KnownDivergence_CompressedLargerThanDeclared` 로 현행만
  고정했다. **계약 결정이 필요하다 — 리더 판정 요청.**
- **SOF 파서는 단일 프레임 JPEG 만 전제한다.** 다중 프레임(프래그먼트 여러 개)에서는
  item 1 만 본다. 이 모듈이 다중 프레임을 다루는지 자체를 이 카드가 확인하지 않았다.
- **SOF 를 못 읽는 JPEG 에 대해서는 아무 판정도 하지 않는다**(의도된 설계, §2). 그런 파일이
  실제로 존재하는지, 존재한다면 어떤 형태인지 시험하지 않았다.
- **§3 은 판독이다.** 값 손상 검출 범위를 실측하지 않았다.
- **다른 컴포넌트 수(RGB 등) 는 보지 않았다.** `comps[0]` 만 쓰며, 컬러 입력이 요구인지
  확인하지 않았다(B-46 이 남긴 같은 Gap).
- **커버리지는 측정하지 않았다** — x64 OpenCppCoverage 미설치 결정 유효.

---

## 7. 잔여 위험 (Residual-risk)

- **SOF 파싱은 직접 짠 마커 스캐너다.** DCMTK·libjpeg 에 노출된 대체 수단을 찾지 못해
  손으로 썼고, 표준 JPEG 마커 배열을 전제한다. 비표준 배열·fill 바이트가 많은 스트림에서
  SOF 를 놓칠 수 있는데, 그 경우 **거절이 아니라 무판정**이므로 **이전 동작으로 되돌아갈 뿐**
  새 실패를 만들지는 않는다. 위험의 방향이 안전 쪽인 것은 의도다.
- **동작 변경이다.** 지금까지 통과하던 불일치 파일이 거절된다. 현장에 그런 파일을 정상으로
  취급하던 흐름이 있었다면 배포 후에 드러난다.
- **"압축도 이제 안전하다" 로 읽으면 안 된다.** 막은 것은 **크기 불일치** 하나이고,
  §3 이 보인 대로 값 손상은 여전히 대부분 통과한다.
- 커밋은 push 전까지 미푸시 유일본이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` | 환경 + 세 프리셋 재빌드·전체 ctest |
| `_obs.bat` | 관측 빌드·실행 |
| `_observation.log` | **변경 전** — 두 경로 `rc=0`, 대조군 정상 |
| `_observation_after.log` | **변경 후** — 두 경로 `rc=-13`, 센티널 생존 |
| `_observation_oversize.log` | 반대 방향 — J2K `rc=0` / JPEG-LL `rc=-3` |
| `_b50.bat` / `_green.log` / `_green2.log` | 신규 4건 BUILD=0, 4/4 |
| `_falsify.log` | 두 가드 약화 — BUILD=0, 대상 2건 재실패(**재현됨**) |
| `_verify.log` | 최종 465 / 209 / 172, 경고 0 |
