# QA-B-206 M2 — E4·D2·C14·C5·C6 와 timeoutMs 헤더 문구 (#251)

카드: `.moai/lanes/post/inbox/QA-B-206.md` M2. 판정 근거: `QA-B-204/{enh,disp,dicom}_report.md`. 앞선 커밋: M1 `df67fe3a`, M1b `cc19ff35`.

## 0. 결과

| 항목 | 결과 |
|---|---|
| E4 | `xpe_calc_exposure_index` 가 `+inf`·`−inf`·NaN 을 같은 판정(`PROCESSING_FAILED`, EI=DI=0)으로 거부. 전에는 `+inf` 만 `XPE_OK, EI=DI=inf` |
| D2 | Modality LUT 표 모드의 클램프를 정수 변환 **앞**에서(double) 한다. 입력 ≥ 2³¹ 이 첫 항목이 아니라 마지막 항목으로 간다. 독립 기준(numpy)과 2,564,448 화소 대조에서 차이 0 |
| C14 | `xpe_dicom_open` 의 핸들을 `std::unique_ptr` 로 소유. 실행으로는 재현할 수 없어 **throw 를 주입**해 증명 |
| C5 | 검증기가 Type 1 태그의 빈 값(길이 0 또는 공백뿐)을 오류로 보고 |
| C6 | 잘못된 UID 가 `errors` 에만 들어간다(`warnings` 에서 빠짐), `valid:false` 유지 |
| timeoutMs 헤더 | 리더 결정 ①: "0 = DCMTK 기본(약 30초), 무제한 아님" (C-STORE·C-FIND 두 곳) |
| 검증(관측) | ci-post ctest 1377 통과·0 실패(AI 시험 261건은 `XPE_AI_EXPECT_ONNX` 없이 건너뜀), ci-dicom ctest 337 통과·0 실패(최종 재빌드 뒤), doxygen 경고 0, 헤더 점검 20개 0건 |

## 1. E4 — 비유한 화소

**소스.** `exposure_index.cpp`: 평균을 만드는 합(double) 뒤에 합의 지수 비트를 읽어 비유한 여부를 판정하고, `if (!sumIsFinite || mean <= 0.0f)` 로 기존 거부 분기(`*outEI = *outDI = 0`, `PROCESSING_FAILED`)에 합류시킨다. 비트로 읽는 이유: 이 모듈은 `/fp:fast` 로 컴파일되어 NaN 비교와 `std::isfinite` 가 접힐 수 있다(`xpe_scan_finite` 가 비트를 읽는 이유와 같다). 유한 float 영상의 double 합은 넘치지 않으므로 합이 비유한이면 화소 중 비유한이 있다(`+inf` + `−inf` 는 NaN).
- 카드의 "같은 반환 코드" 대로 NaN·`−inf`(이미 `PROCESSING_FAILED`)에 `+inf` 를 맞췄다. QA-B-204 보고서의 `INVALID_INPUT` 제안 대신 이쪽이다.

**시험** (`ExposureIndexNonFinite.*`, `test_exposure_index_contract.cpp`): NaN·`+inf`·`−inf` 가 같은 코드·EI=DI=0(센티널 123 을 덮어씀), `+inf` 의 위치(첫째·둘째·행 끝·중간·마지막), `+inf` 와 `−inf` 가 한 영상에 함께 있는 경우(합이 NaN).

**반증** (`arms_e4.py.txt`, `arms_e4_out.txt`):

| 끈 방어 | 결과 |
|---|---|
| 조건을 `mean <= 0` 만으로(원래 코드) | 값·위치 시험 빨강 |
| 지수 마스크를 `0x7FE` 로 | **등가 변이** — `+inf` 의 지수 `0x7FF` 도 마스크 뒤 `0x7FE` 라 그대로 잡힌다. 다르게 읽히는 것은 지수 `0x7FE` 의 double(≥ 약 9e307)뿐인데 float 4096 개의 합으로는 닿지 않는다 |
| 검사가 `+inf` 비트 패턴만 봄 | 빨간 시험 없음 — 등가: NaN·`−inf` 는 새 검사 없이도 기존 `mean <= 0` 분기로 거부되므로(QA-B-204 관측) 새 검사가 실제로 필요한 입력은 `+inf` 뿐이고, 그것을 이 팔이 그대로 잡는다. `+inf` 를 놓치는 팔은 첫 팔(원래 코드)이다 |
| 거부 시 EI·DI 를 0 으로 쓰지 않음 | 값 시험 + 기존 `ZeroMeanImage_ReturnsProcessingFailed` 빨강 |
| 거부 시 `XPE_OK` 를 돌려줌 | 세 시험 + 기존 시험 빨강 |


## 2. D2 — 표 모드의 큰 입력

**소스.** `modality_lut.cpp`: `xpe_round_to_int(px) - firstMapped`(float→int32 변환이 2³¹ 이상에서 정의되지 않음, 이 환경에서 INT_MIN 이 되어 첫 항목으로 감김. 같은 줄의 int32 뺄셈도 `lutFirstMapped` 가 INT_MIN/INT_MAX 근처이면 넘칠 수 있음)을 double 로 옮겼다: `d = round(double(px)) - double(firstMapped)`, `d <= 0 → 0`, `d >= len−1 → len−1`, 아니면 `int32_t(d)`. double 은 모든 float 과 int32 를 정확히 담는다. 반올림은 전과 같다(0.5 에서 0 으로부터 멀어지는 쪽, `roundf` 와 동일).

**시험** (`ModalityLutClamp.*`, `test_modality_lut.cpp`): +2³¹ 직전의 가장 큰 float(2147483520)·2³¹·2147483904·3e9·2³²·1e12·3.4e38·float 최댓값 → 마지막 항목(전: 첫 항목), 음의 쪽 대칭 → 첫 항목, 범위 안 반올림과 경계(−0.4, 0, 0.5, 7, 14.6, 15, 15.4, 16, `firstMapped` 100 에서 108), `lutFirstMapped` 가 INT_MIN·INT_MAX 일 때.

**독립 기준 대조** (`d2_reference_diff.py.txt`, `d2_reference_diff_out.txt`): 표 길이 2·16·4096·65536 × `firstMapped` 0·7·−100·30000·INT_MIN·INT_MAX × 입력(표 둘레 균일, 반 정수 전부, float 전 범위 균일, 경계 값 7개, `firstMapped` 둘레) 합계 **2,564,448 화소, 기준과 다른 화소 0**. 기준은 numpy 로 `clip(round_half_away(x) − first, 0, len−1)` 를 따로 계산했다. 영상 출력이 바뀌는 항목이라 16비트 소비자 비교 요구를 이 대조로 대신했다: 출력은 표의 uint16 값이므로 표 값 비교가 곧 16비트 비교다. 수정 전 바이너리와의 비교는 하지 않았다(독립 기준과 일치하므로).

**반증** (`arms_d2.py.txt`, `arms_d2_out.txt`):

| 끈 방어 | 빨개진 시험 |
|---|---|
| 클램프 앞의 변환을 원래대로 | 큰 입력 시험, INT 끝 시험 |
| 위쪽 경계가 첫 항목으로 | 큰 입력 시험, 범위·경계 시험, INT 끝 시험, 기존 `TableMode_BasicLookup`·`ClampingBounds` |
| 아래쪽 경계가 마지막 항목으로 | 음의 큰 입력, 범위·경계, INT 끝, 기존 두 시험 |
| 반올림을 버림(`trunc`) 으로 | 범위·경계 시험 |
| 반올림을 `floor` 로 | 범위·경계 시험 |
| 색인 계산을 int64 로 바꾸되 값만 클램프 | 빨간 시험 없음 — 등가 변이(같은 결과를 다른 산술로 내는 구현) |

## 3. C14 — `xpe_dicom_open` 의 소유권

**소스.** `h` 를 `std::make_unique<XpeDicomHandle>` 로 만들고 `open()` 이 성공했을 때만 `release()` 로 넘긴다. 이전 형태는 `try` 안의 `auto* h = new …` 라서 `open()` 이 던지면 `catch(...)` 가 `h` 를 볼 수 없었다.

**재현 불가, 주입으로 증명.** 입력으로 `open()` 을 던지게 하는 방법이 없다(DCMTK 는 실패를 `OFCondition` 으로 돌려준다). 시험(`ThousandOpenCycles_CrtHeapDoesNotGrowWhetherOrNotOpenSucceeds`)은 열기·닫기(성공 경로)와 없는 파일 열기(실패 경로)를 1000 번 반복해 CRT 힙 증가를 잰다. 기본 빌드에서 이 시험은 회귀 방지용이고, 누수 자체는 소스에 `DicomReader::open` 시작에서 `std::bad_alloc` 을 던지는 한 줄을 주입해 확인했다:

| 구성 | 힙 시험 |
|---|---|
| 던짐 주입 + `unique_ptr`(수정) | 초록(증가 없음) |
| 던짐 주입 + 원래 `new`/`delete h` | **빨강**(호출당 핸들 하나씩 누수) |

(주입한 두 구성에서는 열기가 항상 실패하므로 다른 많은 시험도 빨개진다 — 그것은 주입의 부작용이고 판정은 위 힙 시험 한 줄이다. `arms_m2dicom_out.txt`.)

## 4. C5·C6 — 검증기

**근거 문구.** REQ-DICOM-024 "Required Type 1 tags for DX IOD present **and non-empty**" · "UID format correct". REQ-DICOM-025 "`errors`: failed checks, `warnings`: non-critical issues". 잘못된 UID 는 실패한 검사이므로 `errors`, 비치명이 아니므로 `warnings` 가 아니다.

**C5.** `required Type 1` 루프에서 존재 뒤에 값을 본다: 길이 0 이거나(숫자 속성·Pixel Data 포함) 문자열이 공백·NUL 뿐이면 `"Required Type 1 tag has no value: <tag>"` 오류, `valid:false`. 없는 태그는 전과 같은 `Missing required Type 1 tag`(다른 메시지, 시험이 구별). `ds->getOriginalXfer()` 로 길이를 읽는다.

**C6.** `checkUID` 가 `errors` 에만 넣고 `valid = false`. 기존 시험 `ValidateBadUID_ReportsWarning` 은 "warnings 가 비어 있지 않다" 를 단언했는데 새 계약과 정반대라 `ValidateBadUID_IsAnErrorAndNotAlsoAWarning` 으로 이름과 단언을 바꿨다(인용하는 소스·문서 없음, git grep).

**시험.** 빈 값 9경우(PatientID 빈 문자열·공백, PatientName 빈 문자열·공백, Modality, StudyInstanceUID, Rows·BitsStored(US 값 없음), PixelData 길이 0), 통제 3경우(한 글자 `X`, 앞뒤 공백 `  A1  `, `^` 는 값), 없음/비어 있음의 메시지 구별, 잘못된 UID 가 `errors` 하나·`warnings` 비어 있음.
- 시험 설계 함정: `insertEmptyElement(DCM_PixelData)` 만으로는 파일에 원래 32768 바이트가 그대로 남아 시험이 거짓 빨강이었다(DCMTK 의 `DcmPixelData` 는 기존 표현을 유지). 지우고 `putAndInsertUint8Array(DCM_PixelData, nullptr, 0)` 으로 넣었다. 디버그 출력으로 확인한 뒤 고쳤고 출력 코드는 남기지 않았다.

**반증** (`arms_m2dicom.py.txt`, `arms_m2dicom_out.txt`):

| 끈 방어 | 결과 |
|---|---|
| 빈 값 검사 전체 제거 | 빈 값 시험, 메시지 시험 빨강 |
| 길이만 판정(공백 판정 제거) | **빨간 시험 없음** — 아래 참조 |
| 공백만 판정(길이 판정 제거) | 빈 값 시험 빨강(US 값 없음·PixelData) |
| `^` 도 공백으로 취급 | 통제 시험 빨강 |
| 잘못된 UID 를 `warnings` 에도 | UID 시험 빨강 |
| 잘못된 UID 가 `valid` 를 내리지 않음 | UID 시험 빨강 |

"길이만 판정" 이 안 터진 이유: DCMTK 는 문자열 값의 끝 공백을 읽을 때 잘라 내어(공백뿐인 값의 길이가 0 이 된다) 길이 판정만으로 공백 시험 두 건도 잡힌다. 따라서 이 코드에서 공백·NUL 판정은 DCMTK 가 자르지 않는 값을 위한 이중 방어다. 그 방어만 걸리는 입력은 구성하지 못했다(값 속에 NUL 을 넣는 수단이 없었다). 그래서 지우지 않고 남겨 두고 "필요하다고 입증되지 않음" 으로 적는다.

## 5. 검증과 영향
- ci-dicom 전체 ctest 337 통과·0 실패는 **마지막 재빌드 뒤**의 값이다. 반증 팔이 끝난 직후 같은 스크립트로 먼저 돌렸을 때는 160 개가 빨갰는데 원인은 아직 재빌드하지 않은 마지막 팔(던짐 주입 바이너리)이었고(소스는 복원됨), 재빌드하자 사라졌다. 초록 수치로 인용하는 것은 재빌드 뒤의 값뿐이다.
- 호출자 영향: ① `xpe_calc_exposure_index` 가 `+inf` 를 이제 거부, ② 모달리티 표 모드가 아주 큰 값에서 올바른 항목, ③ 검증기가 빈 Type 1 값을 오류로, 잘못된 UID 를 `errors` 에만 — gui/clients 가 `warnings` 로 UID 문제를 읽는지는 확인하지 않았다(Gap).
- 헤더: `enhance_basic_api.h`(`xpe_calc_exposure_index` 의 `@return`), `dicom_api.h`(`timeoutMs` 두 곳).

## 6. Gap / 잔여 위험
Gap
- gui/clients 가 검증 보고의 `warnings` 로 UID 를 읽는지, E4 의 `+inf` 가 실제 파이프라인에서 생길 수 있는지 확인하지 않았다.
- 검증기의 공백·NUL 판정이 필요한 입력을 구성하지 못했다(§4).
- C14 는 주입으로만 증명했다. 실제로 `open()` 이 던지는 입력은 알려지지 않았다.
- 화소 출력 비교는 표 모드 한 경로의 독립 기준이다. 선형 모드는 이번에 건드리지 않았다.

잔여 위험
- 검증기가 이전에 통과시키던 빈 값 파일을 이제 거부하므로 그런 파일을 만들던 외부 도구는 `valid:false` 를 받는다(요구 문구대로의 변화).
- 같은 UID 문제가 `errors` 로만 보이게 되어 `warnings` 개수에 의존한 소비자가 있다면 값이 줄어든다.
