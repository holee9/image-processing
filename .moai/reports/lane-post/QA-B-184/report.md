# QA-B-184 — MONOCHROME1 극성: 결정을 위한 조사 (#235)

코드 변경은 없다. 이 보고서는 결정에 필요한 사실과 선택지별 영향을 모은 것이고, 결정은 리더(필요하면 사용자)가 한다.

## 결론 먼저

1. **저장소에는 이미 이 결정에 대한 문서가 있고, 두 곳이 서로 반대로 쓰여 있다.**
   - DICOM 모듈 문서 5곳(SRS FR-DCM-109, SAD 10단계, README, PRD REQ-4.1.9, 추적 행렬)은 **읽을 때 반전한다**(`MAX − pixel`, 출력은 항상 MONOCHROME2 의미)고 쓴다. 쓰기 쪽 FR-DCM-205 는 "항상 MONOCHROME2".
   - 후처리 문서(`XPE-SRS-001` SRS-FUNC-023, `XPE-SDD-002` Presentation LUT 단계, `XPE-STP-001` UT-3.3-003)는 **표시 단계가 PI 를 받아 반전한다**고 쓴다.
   - 코드는 둘 다 하지 않는다. 읽기는 저장된 그대로, 표시 모듈의 Presentation LUT 함수에는 PI 인자가 없다. 두 문서의 방식을 **둘 다** 구현하면 이중 반전이 된다.
2. **수정 없이도 지금 벌어지는 결함을 실행으로 확인했다**: MONOCHROME1 파일을 읽어 `xpe_dicom_write` 로 다시 쓰면 같은 화소값에 `MONOCHROME2`+`IDENTITY` 가 붙어 나온다. 원본과 화면에서 극성이 반대다 (§4).
3. **저장소 안의 소비자 중 실제로 이 값을 쓰는 곳은 개발 도구 하나뿐이다.** GUI 는 이 함수를 부르지 않는다 (§2).
4. **현장에 MONOCHROME1 을 내보내는 장비가 있다**: Carestream DRX-Evolution 적합성 선언문은 "표시 공간이 density 로 설정되면 MONOCHROME1 을 쓴다" 고 밝힌다 (§1). 우리 고객 장비가 어떤지는 저장소로는 알 수 없다.
5. **권고(리더 결정 사항): (b) 읽을 때 반전하고 항상 MONOCHROME2 의미로 돌려준다.** 이유와 조건은 §6.

## 1. 표준 (원문 인용, 전체는 `standard_quotes.txt`)

| 질문 | 표준 | 출처 |
|------|------|------|
| MONOCHROME1 의 뜻 | "The minimum sample value is intended to be displayed as white after any VOI gray scale transformations have been performed." (MONOCHROME2 는 black) | PS3.3 C.7.6.3.1.2 — https://dicom.nema.org/medical/dicom/current/output/chtml/part03/sect_C.7.6.3.html |
| 극성이 적용되는 위치 | 파이프라인은 Modality LUT → VOI LUT → Presentation LUT. "the input range is implicitly specified to be the output range of the preceding transformation (VOI LUT, or if the VOI LUT is identity or absent, the Modality LUT, or ... the stored pixel values)" | PS3.3 C.11.6 — https://dicom.nema.org/medical/dicom/current/output/chtml/part03/sect_C.11.6.html |
| 반전의 표현 | Presentation LUT Shape `INVERSE`: "the minimum output value shall convey the meaning of the maximum available luminance". 즉 **INVERSE 는 출력 쪽에서 뒤집는다** | 같은 곳 |
| DX 이미지 규칙 | `IDENTITY` "shall be used if Photometric Interpretation is MONOCHROME2", `INVERSE` "shall be used if Photometric Interpretation is MONOCHROME1". PI 는 MONOCHROME1·MONOCHROME2 둘 다 허용 | PS3.3 C.8.11.3 — https://dicom.nema.org/medical/dicom/current/output/chtml/part03/sect_C.8.11.3.html |
| Window 와의 관계 | Window Center·Width 는 MONOCHROME1·2 에서만 쓰고, 극성은 VOI 뒤에서 정해진다 | PS3.3 C.11.2 — https://dicom.nema.org/medical/dicom/current/output/chtml/part03/sect_C.11.2.html |
| 프레젠테이션 상태 | 참조 영상의 PI 는 무시되고 "inversion of the polarity ... by choosing Presentation LUT Shape of IDENTITY or INVERSE" | PS3.4 N.2 — https://dicom.nema.org/medical/dicom/current/output/chtml/part04/sect_N.2.html |
| 극성과 X선 세기 | 별개 속성이다: `Pixel Intensity Relationship Sign (0028,1041)` 은 저장값과 X선 세기의 관계 (PI 와 무관) | PS3.3 C.8.11.3 |
| 현장 | Carestream DRX-Evolution v5.7(86쪽, 쪽 15·24·25·36): "If the image space configured on the destination is set to density, MONOCHROME1 is set. If ... p-values or luminance, MONOCHROME2 is set." 71쪽: "With a photometric interpretation of MONOCHROME1, a value of 0 represents minimum density and a value of 4095 represents maximum density" (Bits Stored 12) | https://www.carestream.com/en/us/-/media/publicsite/resources/radiography-and-health-it/regulatory-information/dicom-statements/image-capture/cr-systems/pdf/drx-evolution-v5_7.pdf?la=en |

**읽는 법**: 표준의 극성은 "저장값 → 표시 밝기" 사이의 **표시 규칙**이다. 저장값을 바꾸라는 규칙은 없다. 같은 이유로 "읽을 때 값을 뒤집는다"는 표준이 요구하는 동작이 아니라, 이 제품이 **표시 규칙을 데이터 쪽으로 옮겨 단순화하려는 선택**이다 (§6).

## 2. 이 저장소의 소비자

방법: `git grep xpe_dicom_read_image` (추적 파일, `.moai/reports` 제외) → 15개 파일. 대조군은 `consumers.txt` 에 있다: (A) 정의가 `dicom_api.h`·`dicom.cpp` 에서 잡힘, (B) 독립 토큰 `xpe_dicom_open` 으로 만든 집합과 비교해 읽기 호출 파일이 전부 `open` 집합에 있음(예외 1: `VVP-P1B-001.md` 는 문서 파일이고 `xpe_dicom_open` 토큰이 없다. 이유는 확인하지 않았다). 병행한 전체 트리 `grep -rl` 도 같은 15개 파일을 냈다.

| 소비자 | 위치 | 읽은 영상으로 하는 일 | 극성에 민감한가 |
|--------|------|------------------------|------------------|
| 구현 | `modules/dicom/src/dicom.cpp`, `DicomReader.*` | 읽기 자체 | — |
| **개발 도구** | `modules/preprocess/tools/xpe_real_frames.cpp` (`loadDicomApi`·`loadDicom`, 460–487행) | 실제 프레임 파일을 float 로 바꿔 전처리 측정에 입력으로 쓴다. 극성 처리 없음 | 예: MONOCHROME1 파일은 뒤집힌 채 들어간다. 단 이 도구는 측정용이고 제품 경로가 아니다 |
| 시험 | `test_dicom_reader.cpp`(35건), `test_dicom_writer.cpp`(2), `test_parameter_dependency.cpp`(2) | 쓰기 후 다시 읽어 비교 | 이 시험들의 파일은 모두 우리 쓰기기가 만든 MONOCHROME2 |
| 진단 | `clients/.../XpeDicomReadinessProbe.cs` | 심볼이 **내보내졌는지**만 확인 (`TryGetExport`), 호출하지 않음 | 아니오 |
| 선언만 있음 | `clients/.../XpeDicomWrapper.cs` 의 `ReadImageDelegate` | 선언만 있고 `GetRequiredDelegate<ReadImageDelegate>` 호출이 없다 (`consumers.txt` §7) | 아니오 |
| **GUI** | `gui/.../PipelineOrchestrator.cs` | 호출하지 않는다. `ReadDicomAsync` 는 `Task.FromResult(true)` 를 돌려주는 스텁(322–326행)이고, 필수 export 이름은 `xpe_dicom_read`·`xpe_dicom_write` 로 **존재하지 않는 이름**이다(실제 export 는 `xpe_dicom_read_image` 등, `QA-B-179/dll_exports_dicom.txt`) | — (호출 자체가 없다) |
| clients 쓰기 | `NativePresentationExportService.cs` | `xpe_dicom_write`+`validate` 만 사용 (표시 처리된 영상 쓰기) | 쓰기기가 항상 MONOCHROME2 라 일관 |
| 문서·SPEC | `.moai/specs/SPEC-XPE-P1B-DICOM/*`, api-spec, VVP | 언급 | — |

**다른 모듈이 극성에 기대는 곳** (읽기를 부르지 않아도, 읽은 영상이 흘러갈 수 있는 곳):

- `enhance_basic/src/exposure_index.cpp`: `ei = K_CAL * (mean / S0_REFERENCE)` — 화소 평균이 선량에 비례한다고 가정한다. 뒤집힌 영상이면 방향이 반대다.
- `xpe_bone_suppress`(AI): 모델은 학습 때의 극성을 가정한다. 이 저장소의 시험은 `Y=2X` 장난감 모델이라 극성을 보지 않는다.
- `display` 모듈: **극성이나 Presentation LUT Shape 를 다루는 함수가 없다.** `xpe_apply_presentation_lut` 는 PS3.14 GSDF 에서 만든 1024 항목 구동 수준 표(`XpePresentationLutParams.lutData`)로 화소를 옮길 뿐이고 PI 인자가 없다(`display_api.h` 255–290행). `MONOCHROME|INVERSE|IDENTITY|PresentationLUTShape|Photometric` 을 `modules/display` 의 소스·헤더에서 찾으면 4건이 나오는데 전부 무관한 주석이다(Modality LUT 항등 2곳, GSDF 식의 수학적 역함수 2곳). DICOM PI·Presentation LUT Shape 를 다루는 코드는 없다 (`documents_and_code.txt` 끝에 4건의 줄과 대조군 — 같은 검색이 `modules/dicom/src` 에서는 PI 처리로 13건 — 이 있다). 처음에는 "0건"이라 적었다가 검색을 다시 돌려 4건임을 확인하고 고쳤다. 내림차순 표를 주면 뒤집힐 수 있어 보이지만 그렇게 시험하지는 않았다.
- `xpe_dicom_get_metadata` 가 채우는 `XpeImageMetadata` 에는 PI 필드가 없다 (`bodyPart, kVp, mAs, SID, pixelPitch, acquisitionTime, flags`).

## 3. 저장소 문서가 이미 정한 것 (결정의 가장 큰 변수)

| 문서 | 내용 |
|------|------|
| `docs/dicom/SRS-DICOM-001` 117–123행 **FR-DCM-109** (Must) | "MONOCHROME1 (작은 값 = 밝음): 자동 반전 (MAX - pixel) / MONOCHROME2: 그대로 / 다른 값 → `XPE_ERR_DICOM_UNSUPPORTED_PHOTOMETRIC`" |
| `docs/dicom/SAD-DICOM-001` 187–194행, 892–894행 | 읽기 10단계 "IF MONOCHROME1: invert pixels: p_out = MAX_VALUE - p_in" |
| `docs/dicom/xpe-dicom-prd.md` 45, 136행, `docs/dicom/README.md` 130–152, 397–399행 | "MONOCHROME1 (자동 반전)" |
| `docs/dicom/SRS-DICOM-001` 266–271행 **FR-DCM-205** | 쓰기: "모든 출력 이미지 MONOCHROME2로 표준화 ... 자동 정규화 (MONOCHROME1 입력일 경우)" |
| `docs/dicom/RTM-DICOM-001` 206행 | FR-DCM-109 → TC-109 "MONOCHROME1 반전, MONOCHROME2 유지" 에 **✓** |
| `docs/post-processing/xpe/XPE-SRS-001` 63행 **SRS-FUNC-023** (Must) | "Photometric Interpretation MONOCHROME1/MONOCHROME2를 올바르게 처리해야 한다" |
| `docs/post-processing/xpe/XPE-SDD-002` 1044–1082행 | Presentation LUT 설계: `xpe_apply_presentation_lut(input, output, int32_t photometricInterpretation /*0=MONO1*/, gsdfEnabled)`, "IF MONOCHROME1: output = maxVal − pValue", 에지 케이스 "MONOCHROME1 / inversion needed / Invert output" |
| `XPE-STP-001` 215·217행 UT-3.3-003/005 | "MONOCHROME1 → inverted output", "MONOCHROME2 → direct output" |

세 가지가 걸린다.

1. **두 설계가 서로 다른 곳에서 반전한다**: DICOM 문서는 읽을 때(데이터가 MONOCHROME2 의미가 되므로 표시는 반전하지 않는다), 후처리 SDD 는 표시 때(PI 를 알아야 한다). 하나만 해야 한다. 하나를 고르면 다른 문서를 고쳐야 한다.
2. **SDD-002 의 시그니처는 실제 코드와 다르다**: 구현된 `xpe_apply_presentation_lut(XpeImageBuffer*, const XpePresentationLutParams*)` 에는 PI 인자가 없다. SDD 의 PI 기반 설계는 구현된 적이 없다.
3. **추적 행렬의 ✓ 는 내용과 일치하지 않는다**: RTM-DICOM 은 FR-DCM-109 를 검증된 것으로 표시하지만, 코드는 반전하지 않고 시험은 "반전하지 않음"을 고정(`Pinned_Issue235…Monochrome1IsReturnedAsStored`)한다. `TC-109`, `UT-3.3-003`, `FR-DCM-109` 를 시험 코드에서 검색하면 0건이다 (`git grep`). 기록이 있다는 것이 기록이 맞다는 뜻은 아니다.

## 4. 실행 증거: 읽고 쓰면 극성이 뒤집힌다 (`roundtrip_probe.py`, 결과는 `roundtrip_probe_output.txt`)

DX 규칙을 따르는 MONOCHROME1 파일을 만들어 모듈로 끝까지 돌렸다.

1. `xpe_dicom_write` 로 12비트 램프 A(`MONOCHROME2`+`IDENTITY`)를 쓴다.
2. 길이를 바꾸지 않고 두 속성만 고쳐 B 를 만든다: `MONOCHROME2→MONOCHROME1`, `IDENTITY→INVERSE ` (표준이 MONOCHROME1 에 INVERSE 를 요구하므로 적합한 파일이다).
3. B 를 `xpe_dicom_read_image` 로 읽는다.
4. 읽은 화소를 `xpe_dicom_write` 로 C 에 쓴다.

관측:

```
A attrs = MONOCHROME2 / IDENTITY      B attrs = MONOCHROME1 / INVERSE
read A rc=0 alerts=0   read B rc=0 meta rc=0 alerts=0
words of B == words of A (no inversion): True | first/last of B: 0 4095
metadata struct of B == metadata struct of A (nothing tells the caller about the polarity): True
C attrs = MONOCHROME2 / IDENTITY      words of C == words of B: True
```

- 반환 코드·알림·메타데이터 어디에도 B 가 A 와 반대 극성이라는 신호가 없다. 읽은 값은 저장된 그대로다.
- 쓰기기는 PI 를 `MONOCHROME2`, 프레젠테이션 LUT 모양을 `IDENTITY` 로 **고정**해 쓴다(`DicomWriter.cpp` 147행과 그 아래 `DCM_PresentationLUTShape`). 그래서 C 는 B 와 **같은 화소 값**에 **반대 극성 표지**가 붙는다. B 에서 0 은 흰색, C 에서 0 은 검정이다. 즉 이 결함은 "읽은 뒤 호출자가 모른다"에서 끝나지 않고 파일로 나간다.

## 5. 선택지

위 표의 "소비자 영향"은 §2 의 조사 결과(실제 호출자는 개발 도구 하나)에 근거한다.

### (a) 메타데이터로 알린다

변형이 셋이다.

| | API·ABI 변화 | 새 헤더 + 옛 DLL | 옛 헤더 + 새 DLL | 비고 |
|---|---|---|---|---|
| a1. `XpeImageMetadata::flags` 에 비트 하나 (예: `XPE_FLAG_PI_MONOCHROME1`) | **구조체 변경 없음**(96바이트 그대로), 상수 하나 추가. `getMetadata` 는 구조체 전체를 `memset 0` 한 뒤 채우므로(`DicomReader.cpp` 588행) 옛 DLL 은 비트가 0 | 안전: 비트가 서지 않아 "MONOCHROME2 처럼 읽힘" = 오늘의 동작. 위험은 새 기능을 못 쓰는 것뿐 | 안전: 옛 호출자는 모르는 비트를 무시 | 쓰는 비트: 0x1–0x80, 0x200–0x1000, 0x2000. **0x100 은 정의가 없다**(예약인지 미사용인지는 문서에서 확인하지 못했다). `flags` 의 정의는 "처리 단계 완료 플래그"라 의미가 섞인다 |
| a2. 구조체 꼬리 패딩 4바이트에 필드 | 크기 96 유지. **208e 와 같은 종류**: 옛 DLL 은 패딩을 쓰지 않으므로 새 헤더 호출자가 읽으면 **값이 정해지지 않는다** (`BUILD-MATCHED USE`) | 위험 | 안전 | 한 묶음으로 배포한다는 문서 규칙을 따라야 한다 |
| a3. 새 export `xpe_dicom_get_photometric(handle, …)` | 구조체 변경 없음, 함수 추가 | 새 호출자가 `GetProcAddress` 로 **없음을 발견**한다(소비자 대부분이 이미 export 존재를 검사하는 습관이 있다) | 안전 | 가장 명시적이고 판별 가능. 함수가 하나 늘고 핸들당 호출이 하나 더 든다 |

소비자: 알림만으로는 아무것도 바뀌지 않는다. **각 소비자가 반전을 직접 해야 하고**(현재 실제 호출자는 개발 도구 하나), 쓰기기는 지금 PI 를 고정해 쓰므로 §4 의 왕복 결함은 쓰기기까지 고쳐야 해결된다. 위험: 알려도 소비자가 무시하면 결함이 그대로이고, 반전 구현이 소비자마다 흩어진다.

### (b) 읽을 때 반전해 항상 MONOCHROME2 의미로 돌려준다

| 항목 | 내용 |
|------|------|
| 식 | 부호 없는 `BitsStored = B` 에서 `v' = (2^B − 1) − (v & (2^B − 1))`. 부호 있는 화소는 이미 `UNSUPPORTED_FORMAT` 이라 범위 밖이다. B=12 → `4095 − v` (Carestream 12비트 예와 일치), B=16 → `65535 − v` |
| 마스크가 먼저 | 지금 리더는 BitsStored 위 비트를 마스크하지 않는다(`Pinned_…BitsAboveBitsStoredAreNotMasked`). 반전하려면 **먼저 마스크해야** 한다(안 하면 `2^B−1−v` 가 음수 쪽으로 감겨 위 비트가 어긋난다). 이 경로에서 MONOCHROME1 만 마스크가 생기는 셈이라 고정 시험 하나를 갈라야 한다 |
| API·ABI | 변화 없음. 반환 문서만 바뀐다 |
| 소비자 | 영향 없음: 모든 소비자가 MONOCHROME2 의미를 받는다. 표시 단계는 반전하지 않는다. §4 의 왕복은 쓰기기(항상 MONOCHROME2)와 일관해 **그대로 해결**된다 |
| 문서 | DICOM 문서 5곳과 일치한다. 후처리 SDD-002·STP 의 "표시가 PI 로 반전" 문장과 `XPE-SRS-001` SRS-FUNC-023 의 해석을 고쳐야 한다(이중 반전 방지). SDD 의 PI 인자는 어차피 구현된 적이 없다 |
| 정합성 (Window·Rescale) | 이 API 는 `Window Center/Width`, `RescaleSlope/Intercept`, `Pixel Padding Value`, `Smallest/Largest Image Pixel Value` 를 돌려주지 않는다(Rescale 은 적용도 보고도 안 한다 — 고정 시험 `…RescaleIsNotAppliedNorReported`). 그래서 호출자가 이 값들을 **파일에서 따로** 읽는다면 반전된 화소와 맞지 않게 된다(Window 중심은 `max − C` 로 거울상). 이 저장소에는 그렇게 쓰는 호출자가 없다. Rescale 을 나중에 보고하게 되면 반전 뒤의 의미를 보존하려고 기울기 부호를 뒤집어야 한다(`m = s·v + b` → `m = −s·v' + (s·M + b)`, 음의 기울기는 PS3.4 N.2 가 "negative slope" 를 따로 다루는 영역이다) |
| 정보 손실 | 원본 극성을 호출자가 알 수 없다(알림으로 보충 가능, `Info` 알림 "MONOCHROME1 normalised"). 원본 값은 `2^B−1−v'` 로 복원된다 |
| 표준과의 관계 | 표준은 값을 바꾸라고 하지 않는다. 이것은 제품의 정규화 선택이다. 쓰기기가 항상 MONOCHROME2 로 쓰므로 정규화된 영상을 쓸 때 라벨이 참이 된다 |
| 위험 | 이미 MONOCHROME2 인 영상에 호출자가 또 반전하면 이중 반전. JPEG 2000·JPEG LL 경로의 `bitsStored`(코드스트림 정밀도)와 일치해야 한다 |

### (c) 거부 (`UNSUPPORTED_FORMAT`)

| 항목 | 내용 |
|------|------|
| API·ABI | 변화 없음. `UNSUPPORTED_FORMAT` 목록에 한 줄 추가 |
| 소비자 | MONOCHROME1 파일을 못 읽는다 |
| 현장 | **실제로 MONOCHROME1 을 내보내는 장비가 있다**(Carestream DRX 계열, density 공간 설정). DX IOD 도 둘 다 허용한다(§1). 이 제품이 그런 장비의 파일을 받을 수 있는지는 저장소로는 알 수 없고 제품 요구에 달렸다 |
| 위험 | 틀린 화면은 없다. 대신 정상 파일이 처리되지 않는다 |

### (d) 현 상태 유지 + 알림만

| 항목 | 내용 |
|------|------|
| API·ABI | 변화 없음 (알림 문구가 클라이언트와의 계약이 된다) |
| 소비자 | MONOCHROME1 읽기마다 알림이 하나 뜬다. 화소는 그대로 |
| 한계 | §4 의 왕복 결함은 **그대로**다 (알림은 쓰기기의 고정 라벨을 못 고친다). 쓰기기를 같이 고쳐야 한다 |
| 위험 | 알림을 보지 않는 호출자(개발 도구)는 계속 뒤집힌 영상을 쓴다 |

### 한눈에

| | 호출자가 알 수 있다 | 호출자 코드 변경 | ABI | 왕복(읽기→쓰기) 결함 | 문서 정합 |
|---|---|---|---|---|---|
| a1/a3 | 예 | **필요** | 없음 / 함수 추가 | 쓰기기도 수정해야 | 두 문서 모두 고침 |
| a2 | 예 | 필요 | 208e 형 위험 | 〃 | 〃 |
| **b** | 알림으로 | **없음** | 없음 | **해결** | DICOM 문서와 일치, 후처리 문서 수정 |
| c | 거부로 | 없음 | 없음 | 해당 없음 | 두 문서 모두 고침 |
| d | 알림으로 | 필요(알림을 읽어야) | 없음 | 쓰기기도 수정해야 | 두 문서 모두 고침 |

## 6. 권고와 조건

**(b) 를 권고한다.** 근거는 사실 네 가지다.

1. DICOM 모듈의 문서 5곳이 이미 이 동작(`MAX − pixel`, 출력은 항상 MONOCHROME2)을 요구한다. 구현이 요구에 뒤처져 있는 것이지, 새 설계를 만드는 것이 아니다.
2. §4 에서 실행으로 확인한 읽기→쓰기 극성 반전이 **쓰기기를 건드리지 않고** 닫힌다 (쓰기기는 이미 MONOCHROME2 고정).
3. 호출자·ABI 영향이 없다. 현재 실제 호출자는 개발 도구 하나다.
4. 후처리 SDD 의 PI 기반 설계는 구현된 적이 없어, 고쳐야 할 쪽의 비용이 적다.

권고에는 조건이 붙는다.

- **먼저 문서 충돌을 정리한다**: SDD-002 §Presentation LUT·STP UT-3.3-003·SRS-FUNC-023 해석에서 "표시 단계가 PI 로 반전"을 "리더가 이미 정규화했으므로 표시는 반전하지 않는다"로 바꾼다. 안 그러면 이중 반전이 구현 단계에서 생길 수 있다.
- 식은 §5(b) 대로 마스크가 먼저다. 고정 시험 `…Monochrome1IsReturnedAsStored` 와 `…BitsAboveBitsStoredAreNotMasked` 는 이 결정과 함께 바뀐다(두 시험 모두 "결정이 나는 날 이 시험을 바꾸라"고 적혀 있다).
- 정규화 사실이 사라지지 않도록 `Info` 알림 하나를 권한다 (알림 문구는 레인 간 계약이므로 clients 와 합의).
- `RescaleSlope/Intercept`·Window 를 나중에 돌려주기로 하면 반전과 같이 설계해야 한다 (§5 정합성).
- RTM-DICOM 의 FR-DCM-109 ✓ 는 구현 전까지 사실이 아니다 (§3). 결정과 별개로 고칠 대상이다.

**리더가 정해야 할 것**: (1) 반전 위치 — 읽을 때(b) 대 표시 때(문서 둘 중 하나). (2) 고객 장비에 MONOCHROME1 이 있는지(없다면 (c) 도 선택지). (3) 정규화 알림의 유무.

## 7. 시험·픽스처

**저장소에는 `.dcm` 픽스처 파일이 없다** (`find`, 추적 파일 0건). 시험은 쓰기기로 영상을 만들고 DCMTK 로 속성을 바꾼다(`MakeScopeVariant`). MONOCHROME1 을 다루는 기존 시험은 셋이다.

| 시험 (`test_dicom_reader.cpp`) | 내용 | 결정이 나면 |
|--------------------------------|------|-------------|
| `Scope_PhotometricInterpretationIsRequiredAndOnlyAMonochromePlaneIsReturned` (3652행) | 표 행 `{"monochrome1", "MONOCHROME1", 1, OK}` — "읽는다" | (b)(d) 유지 / (c) 거부로 바꿈 |
| `Scope_PhotometricInterpretationRefusalsNameTheValueAndMonochrome1IsReturnedAsStored` (3705행) | 같은 화소 + **알림 0건** | (b) 반전 단언 / (d) 알림 1건 단언 |
| `Pinned_Issue235AwaitsDesignDecision_Monochrome1IsReturnedAsStored` (3961행) | 같은 화소 | 같이 바뀐다 |

선택지별 시험 방법 (모두 `MakeScopeVariant` 로 MONOCHROME1 파일을 만들 수 있다 — 이 조사의 `roundtrip_probe.py` 는 파일 바이트를 같은 길이로 고쳐 PI 와 Presentation LUT Shape 를 함께 바꾸는 방식을 쓴다):

| 선택지 | 시험 |
|--------|------|
| (a) | 새 플래그/함수가 MONOCHROME1 에서만 서고 MONOCHROME2 에서는 안 선다. 옛 DLL 대조: 비트 0. 쓰기기가 플래그를 보고 `MONOCHROME1`+`INVERSE` 로 쓰는 왕복 시험 |
| (b) | **표시 동치 시험**: 같은 화소 w 의 A(MONOCHROME2)와 B(MONOCHROME1, INVERSE)에 대해 `read(B) == (2^B − 1) − read(A)`. B=8·12·16 (비압축, JPEG LL, JPEG 2000 세 경로), 위 비트가 섞인 입력(마스크), 이미 MONOCHROME2 인 파일은 비트 동일, 왕복 `write(read(B))` 의 표시 의미가 B 와 같음 |
| (c) | MONOCHROME1 → `UNSUPPORTED_FORMAT` + 알림, 핸들 재사용 가능 |
| (d) | MONOCHROME1 → `OK` + 문구 전체 일치하는 알림 1건, MONOCHROME2 → 알림 0건. 왕복 결함은 쓰기기 수정 전까지 시험이 "뒤집힌다"를 단언하게 둘지 정해야 한다 |

## Gaps (미검증)

- **실제 MONOCHROME1 장비 파일로는 돌리지 않았다.** §4 의 B 는 우리 쓰기기가 만든 파일의 속성 두 개를 바꾼 것이다(표준을 따르는 DX MONOCHROME1 파일의 PI·Presentation LUT Shape 와 같은 모양이지만 실제 장비의 다른 속성, 예를 들어 `Pixel Intensity Relationship`, 은 없다).
- **고객 장비가 MONOCHROME1 을 쓰는지는 모른다.** 현장 근거는 Carestream 한 제품군의 적합성 선언문뿐이다(쪽수와 문장은 `standard_quotes.txt`, PDF 를 `pypdf` 로 추출했고 표·그림 안의 문장은 추출 순서가 어긋날 수 있다). 다른 제조사는 조사하지 않았다.
- 소비자 검색은 **추적된 파일**만 본다(`git grep`). 저장소 밖(고객사 소프트웨어, 외부 스크립트)의 호출자는 알 수 없다. 병행한 전체 트리 `grep` 도 같은 15개 파일이었다.
- 문서 인용은 줄 번호로 되어 있고 이 문서를 고치는 편집에 밀릴 수 있다. 인용한 문장 자체는 `documents_and_code.txt` 에 복사해 두었다.
- (b) 의 식은 부호 없는 한 평면에 대한 것이다. 부호 있는 화소·다중 프레임은 지금 `UNSUPPORTED_FORMAT` 이라 다루지 않았다.
- `0x100` 플래그 비트가 예약인지 비어 있는지는 확인하지 못했다.
- 내림차순 `lutData` 로 표시 모듈이 극성을 뒤집을 수 있는지는 시험하지 않았다.
- AI 모델이 가정하는 극성은 모른다(시험 모델은 장난감).

## Residual-risk (잔여 위험)

- (b) 를 고르면 "읽은 영상의 값은 파일의 저장값이 아니다"가 호출자에게 보이지 않는 사실이 된다. 알림으로 보완하지 않으면 외부에서 같은 파일을 DCMTK 로 직접 읽은 값과 비교하는 순간 어긋난다.
- 어느 쪽을 고르든 문서(SDD-002, STP, RTM-DICOM)가 고쳐지기 전까지 두 설계가 서로 다른 말을 한다.
- 지금 상태가 유지되는 동안(어느 선택지도 구현되기 전까지)에는 MONOCHROME1 파일을 읽어 쓰는 호출자가 §4 의 결함을 그대로 겪는다.
