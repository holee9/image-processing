# QA-B-182c — BitsStored/HighBit 필수화, 옛 writer 산출물, 182 빈 파일 정정 (#235)

Refs #235

## 1. 주장

| # | 주장 |
|---|------|
| C1 | `BitsStored`·`HighBit` 의 부재·빈 값·해독 실패는 `XPE_ERR_DICOM_INVALID` 다. 비압축·JPEG Lossless·J2K 세 경로 모두. 기본값을 채우지 않는다. |
| C2 | 옛 writer 산출물(16비트 정밀도 코드스트림 + `BitsStored 12`)은 새 reader 가 `-13` 으로 거부한다. 시험 한 건으로 고정했다. |
| C3 | 거부 이유가 사용자(운영자)에게 나온다. 이전에는 로그에만 있었고, 이제 `XPE_ALERT_ERROR` 알림이 어떤 속성이, 또는 어떤 두 값이 어긋났는지 적는다. **알림 문구는 레인 간 계약이라 아래 4절에 문구를 전부 적었다.** |
| C4 | 저장소에는 `.dcm` 등 DICOM 파일이 없다(0건, 대조군 포함). DICOM 을 만드는 소스는 모듈의 writer 를 쓰거나 reader 시험의 DCMTK 변형뿐이다. |
| C5 | 182 의 빈 증거 파일을 찾은 검색 명령과 대조군을 남겼고, 같은 검색이 **내가 놓친 것 하나를 더 보여 줬다**(6절). |

## 2. 결정과 코드 — 리더 카드와 어긋난 한 곳

카드 문구는 "`XPE_ERR_INVALID_INPUT`, 182b 의 필수 태그 부재와 같은 코드" 였다. 182b 의 필수 태그 부재 코드는 `XPE_ERR_DICOM_INVALID`(-13)다(`INVALID_INPUT` 은 -1, 호출자의 인자 오류). 카드의 원칙("182b 와 같은 코드")을 따라 **`XPE_ERR_DICOM_INVALID`** 로 했다. 카드 문구의 코드명이 의도한 것과 다르면 알려 주기 바란다 — 바꾸는 데는 한 줄이다(`DicomReader.cpp` 의 `required` 표 아래 `refuse(XPE_ERR_DICOM_INVALID, …)`).

수정(`DicomReader.cpp`):
- Type 1 속성 다섯(`SamplesPerPixel`, `PixelRepresentation`, `BitsAllocated`, `BitsStored`, `HighBit`)을 한 표(`required`)로 읽고, 하나라도 없거나 비었거나 숫자가 아니면 `DICOM_INVALID`. 이전의 `BitsStored = BitsAllocated`, `HighBit = BitsStored − 1` 기본값은 `readImage` 와 `decompressPixelData` 에서 모두 없앴다.
- 거부는 `refuse(code, fmt, …)` 한 곳을 지난다: 로그와 알림을 함께 남긴다.
- `dicom_api.h` 반환 코드·`@note` 를 갱신(필수 속성 다섯, 알림 문구가 계약이라는 점, 옛 writer 산출물).

## 3. 증거

### 3.1 수정 전 빨강 (`red_before_fix.txt`)

시험 3개를 먼저 추가하고 옛 reader 로 실행: 3개 빨강, 나머지 17개 초록(통제).
- `Scope_AbsentOrEmptyBitsStoredAndHighBitAreMalformedOnEveryPath`: 경로 3개 × 속성 2개의 **부재** 6건이 `OK(0)` 로 읽힘(빈 값은 182b 의 읽기 실패 처리로 이미 `INVALID`). 같은 시험의 통제(속성이 있는 같은 경로 파일)는 초록이었다.
- `Scope_ARefusalPostsAnAlertThatNamesTheCause`, `J2kScope_AFileFromTheOldWriterIsRefusedAndTheAlertSaysWhy`: 알림이 없다.

### 3.2 수정 후 (`green_after_fix.txt`, `after_full_ci_dicom_ctest.txt`)

`Scope_*`·`J2kScope_*` 20/20. dicom 전체 직렬 ctest `100% tests passed, 0 tests failed out of 217`(214 + 신규 3). 빌드 `warning C` 0건. (ci-dicom 트리, RelWithDebInfo; CI 의 `coverage-dicom` Debug 구성으로는 돌리지 않았다.)

### 3.3 반증 (`arms_summary.txt`, `arm_*.txt`, 소스는 되돌려 바이트 동일함을 비교로 확인)

| 약화 | 빨개진 시험 |
|------|-------------|
| `BitsStored` 를 필수 표에서 뺌 | 8개(`Scope_AbsentOrEmptyBitsStoredAndHighBit…` 포함) |
| `HighBit` 를 필수 표에서 뺌 | 8개(같은 시험 포함) |
| 알림 `xpe_alert_push` 를 지움 | `Scope_ARefusalPostsAnAlert…`, `J2kScope_AFileFromTheOldWriter…` |
| 정밀도 불일치 알림 문구를 바꿈 | `J2kScope_AFileFromTheOldWriter…` |

필수 표에서 항목을 빼는 두 반증은 대상 시험 외 다른 시험도 빨갛게 만든다(그 변수가 0 으로 남아 모든 파일이 "지원 안 함"이 된다). 그래서 "각 검사가 자기 시험만 빨갛게 한다"는 증명은 아니고, 대상 시험이 그 빨강에 포함됨을 보인 것이다.

## 4. 알림 문구 (레인 간 계약 — 바뀐 것을 전부 적는다)

모두 `XPE_ALERT_ERROR`, 접두사 `dicom read refused: `. 이전에는 이 reader 가 알림을 쓰지 않았다(로그만). 새로 생긴 문구:

| 상황 | 문구 |
|------|------|
| 필수 속성 부재 | `<이름 (gggg,eeee)> is absent, empty or not a number (it is a Type 1 attribute and has no default)` — 이름은 SamplesPerPixel (0028,0002), PixelRepresentation (0028,0103), BitsAllocated (0028,0100), BitsStored (0028,0101), HighBit (0028,0102) |
| 프레임 수 | `NumberOfFrames (0028,0008) is present but is not a number >= 1` / `NumberOfFrames <n> (only single-frame images are supported)` |
| 표본 수 | `SamplesPerPixel <n> (only one sample per pixel is supported)` |
| 부호 | `PixelRepresentation <n> (only unsigned pixels are supported)` |
| 비트 | `BitsAllocated <n> (only 16 is supported for uncompressed and JPEG Lossless data)` / `BitsStored <s> with HighBit <h> (the significant bits must be the low ones: HighBit = BitsStored - 1, BitsStored 1..16)` / `BitsAllocated <n> for JPEG 2000 (8 or 16 are supported)` |
| J2K 코드스트림 | `JPEG 2000 codestream carries <n> components, the dataset says 1 (SamplesPerPixel)` / `… is signed, the dataset says unsigned pixels (PixelRepresentation 0)` / `… precision <p> exceeds the 16 bits this reader returns` / **`JPEG 2000 codestream precision <p> does not match the dataset (BitsStored <s>, BitsAllocated <a>)`** / `JPEG 2000 codestream size <w>x<h> does not match the declared Columns x Rows <w>x<h>` |

옛 writer 산출물을 읽으면 운영자가 보는 알림은 `dicom read refused: JPEG 2000 codestream precision 16 does not match the dataset (BitsStored 12, BitsAllocated 16)` 이고 반환은 `-13` 이다(시험이 두 부분을 단언).

clients/gui 쪽에서 `dicom read` 알림을 정규식·접두사로 고정한 시험이 있는지는 확인하지 않았다(이 레인의 범위 밖; 리더 확인 요청).

## 5. 저장소 안의 DICOM 픽스처·샘플과 생성원 (`search_dicom_fixtures_and_empty_files.txt` 1·2절)

- 추적 파일 3686개를 확장자(`.dcm .dicom .dic .ima`)와 오프셋 128 의 `DICM` 마법으로 훑었다: **0건**. 대조군: 같은 함수가 `DICM` 마법만 있는 임시 파일(확장자 없음)과 확장자만 있는 임시 파일을 각각 찾아냈다(`['DICM magic']`, `['extension']`); 평문 `docs/dicom/README.md` 는 `[]`.
- 따라서 "이 태그가 없는 .dcm 파일"은 저장소에 없다. DICOM 을 만들거나 쓰는 **소스**(주석 제외하지 않은 텍스트 검색, `.moai/reports` 제외): `modules/dicom/{src,tests}` 와 `clients/…XpeDicomReadinessProbe.cs`, `clients/…NativePresentationExportService.cs`, `gui/…PipelineOrchestrator.cs`. 클라이언트·GUI 셋은 `xpe_dicom_write` 만 부른다. 모듈의 writer(`populateDataset`)는 다섯 속성을 항상 쓴다(`SamplesPerPixel`, `BitsAllocated`, `BitsStored`, `HighBit`, `PixelRepresentation`: `DicomWriter.cpp` 148~153행 부근). reader 시험의 데이터는 그 writer 가 쓴 파일을 DCMTK 로 변형한 것이다.
- 한계: **저장소 밖의 파일**은 볼 수 없다. `modules/preprocess/tools/xpe_real_frames.cpp` 는 실제 장비 프레임 DICOM 을 이 reader 로 읽는 도구인데, 그 파일들(저장소에 없음)이 다섯 속성을 모두 갖는지는 모른다. 갖지 않은 실제 파일은 이제 거부된다.

## 6. 182 빈 파일 정정 (`search_dicom_fixtures_and_empty_files.txt` 3절)

검색: `git ls-tree -r -l <commit>` 로 크기 2바이트 이하인 추적 파일을 센다.
- **대조군** (`ec0759f1`, QA-B-182 가 커밋된 시점): 6개의 `arm_dic_*.txt` 와 `report.md` 를 모두 잡는다(1바이트 7개).
- `HEAD`(QA-B-182b 커밋 뒤): `report.md` 는 채워졌지만 **`arm_dic_*.txt` 6개는 그대로 비어 있었다** — 182b 에서 `report.md` 만 정정하고 이 6개를 남겨 둔 것은 내 누락이다. 이 카드에서 `git rm` 했고 `QA-B-182/report.md` 에 적었다.
- 같은 검색이 **저장소 전체**에서 다른 2바이트 이하 파일도 보여 준다(33개): `.gitkeep` 디렉터리 표시 파일들, `.dotnet-cli/.dotnet/` 의 sentinel 파일 4개, `lane-pre` 의 `*-export-diff.txt`·`19_export_list_diff_empty.txt` 7개(이름이 "변경 없음 diff"를 가리키며 내용은 비어 있음), `gui/…/.gitkeep` 4개(1바이트). 182b 에서 "이 6개 외에 없다"고 보고했을 때 내 확인 범위는 `lane-post` 보고서뿐이었다. **전체 범위로는 이 6개 외에도 작은 파일이 있고**, `lane-pre` 의 빈 diff 7개가 정당한 "차이 없음" 기록인지는 열어 보지 않았다(다른 레인 소유라 손대지 않았다).

## 7. Gaps (미검증)

- 알림이 실제로 GUI 에 어떻게 보이는지(길이 한도, 표시 방식)는 보지 않았다. 문구는 모두 reader 의 400바이트 버퍼 안에 들어간다(가장 긴 것도 200자 미만).
- 알림 큐가 가득 찬 상태의 동작은 이 카드에서 보지 않았다.
- `JPEG Lossless` 경로의 거부 시험은 속성 부재 2종에 한정했다(경로별 모든 거부 사유를 JPEG LL 로 반복하지 않았다).
- 리더 카드의 "빈 값" 시험은 `BitsStored`·`HighBit` 에 대해 `putAndInsertString(tag, "")` 로 만든 빈 US 값이다. 그 밖의 "해독 실패" 형태(VR 이 다른 값 등)는 만들지 않았다.
- 5절의 검색은 이름·확장자·마법 한 축이다. 다른 인코딩으로 DICOM 을 담은 파일(압축·base64 등)은 못 찾는다.

## 8. Residual-risk (잔여 위험)

- 필수 속성 다섯을 모두 갖지 않는 실제 장비 파일이 있으면 이전에는 읽히던 것이 거부된다(5절 한계).
- 옛 writer 로 이미 만든 `BitsStored < 16` J2K 파일은 거부된다. 현장에 있는지는 레인이 알 수 없다(리더 기록 대상, 카드대로).
- 알림이 새로 생겼으므로 호출 순서에 따라 알림 큐에 읽기 실패 알림이 섞일 수 있다(`xpe_clear_alerts` 를 읽기 사이에 부르는 호출자가 있다면 영향 없음).
