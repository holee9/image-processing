# QA-B-206 M2c — Pixel Data 는 Type 1C, 빈 UID 는 한 번만 (Codex #113, #251)

회신 전문: 메인 저장소 `.moai/state/codex-archive/113.md`. 앞선 커밋: M2b `5ddc25e9`. 리더 결정: 사용자의 "표준대로" 원칙을 같은 요구 줄에 적용.

## 0. 결과

| 항목 | 결과 |
|---|---|
| 1. Pixel Data (Type 1C) | `(7FE0,0010)` 이 없고 Pixel Data Provider URL `(0028,7FE0)` 이 **값과 함께** 있으면 오류가 아니라 경고 하나: "Pixel data by reference (Pixel Data Provider URL) is not supported by this module". `valid` 는 바뀌지 않는다. URL 도 Pixel Data 도 없으면 지금처럼 `Missing required Type 1 tag` 오류 |
| 2. 빈 UID | 값이 없는 UID 는 "has no value" 한 건만 보고하고 형식 검사는 건너뛴다. 값이 있는 UID 는 형식을 그대로 검사한다 |
| 시험 | 새 2개(`PixelDataIsTypeOneC…`, `ABlankUidIsReportedOnce…`), 고친 기존 2개(§2) |
| 반증 | 8팔 모두 터짐(§3), 소스 바이트 복원 |
| 검증(관측) | ci-dicom 전체 ctest 350 통과·0 실패(최종 트리 재빌드 뒤: `ctest_summary_m2c.txt`), validator 30 통과, doxygen 경고 0, 헤더 점검 20개 0건 |

## 1. 수정

`DicomValidator.cpp`
- 도우미 둘: `isAllPadding(text)`(공백·NUL 뿐), `elementHasValue(ds, key)`(요소가 있고 길이가 0이 아니며 패딩만이 아님).
- 필수 태그 루프의 "없음" 갈래: `req.key == DCM_PixelData && elementHasValue(ds, DCM_PixelDataProviderURL)` 이면 경고를 `warnings` 에 넣고 `continue`(오류 없음, `valid` 유지).
- `checkUID`: 값이 패딩뿐이면 반환(값 없음은 필수 루프가 이미 보고했다).

세 가지 판단(보고서에 남긴다):
1. **빈 URL 은 제공자가 아니다.** URL 요소가 있어도 값이 없으면 Pixel Data 부재의 면제 사유가 되지 않는다(면제를 비어 있는 요소가 열어 주면 "있기만 하면 통과"가 된다). 시험 4번째 경우와 반증 `empty_url_counts_as_a_provider` 가 이를 지킨다.
2. **Pixel Data 와 URL 이 둘 다 있으면 경고를 내지 않는다.** 화소가 있으니 참조 화소 지원 여부는 이 파일에 대해 의미가 없다(5번째 경우).
3. **Pixel Data 가 있으나 값이 비어 있는 경우는 그대로 오류**(요소가 "있는" 것이라 면제가 아니다; M2 의 시험이 계속 본다).

`dicom_api.h`: `xpe_dicom_validate` 문서에 Type 규칙(Type 1 / Type 2 / Type 1C 의 URL 경우 / 빈 UID 한 번)을 한 단락 추가했다. 이전에는 헤더가 Type 규칙을 전혀 말하지 않았다.

## 2. 시험 (`test_dicom_validator.cpp`)

- 새 `PixelDataIsTypeOneCSoAFileWithOnlyAProviderUrlIsAWarningNotAnError`: 다섯 경우 — Pixel Data 있음 / URL 만 / 둘 다 없음 / 빈 URL 만 / 둘 다 있음. 각각 `valid`, "Missing required Type 1 tag" 건수, "by reference" 경고 건수, 경고 총수(다른 경고 없음), rc(OK).
- 새 `ABlankUidIsReportedOnceAsNoValueAndItsFormatIsNotJudgedAsWell`: Study·Series·SOP Instance UID 각각 — 빈 값은 "no value" 정확히 1건·"Invalid UID format" 0건, `not.a.uid` 는 형식 오류 정확히 1건·"no value" 0건(건너뛰기가 빈 값에만 걸린다는 것).
- 고친 기존 시험 2개(M2b 가 중복 보고를 전제로 느슨하게 바꾼 단언을 되돌림): `AMissingTagAndAnEmptyTagAreReportedWithDifferentMessages` 는 빈 Study UID 가 오류 정확히 1건("no value", "Missing" 아님), `EveryRequiredAttributeIsJudgedByTheTypeTheStandardGivesIt` 는 태그별 보고 건수를 Type 1 은 1, Type 2 는 0 으로 센다(예전에는 같은 태그 오류의 존재만 봐서 중복이 Type 강등을 가렸다).
- 시험 작성 중 틀린 것 하나: 새 시험이 처음에 `rc == XPE_ERR_DICOM_INVALID`(유효하지 않은 파일) 를 기대했다. 모듈 계약은 "XPE_OK = 보고서가 만들어졌다, 적합하다는 뜻이 아니다"이고 DICOM_INVALID 는 파싱이 안 되는 파일 전용이라 시험 쪽이 틀렸다. 검증기를 바꾸지 않고 기대값을 OK 로 고쳤다.

## 3. 반증 (`arms_m2c.py.txt`, `arms_m2c_out.txt`, `arms_m2c_out2.txt`)

| 끈 방어 | 빨개진 시험 |
|---|---|
| URL 면제를 절대 타지 않음(Pixel Data 항상 필수) | Type 1C 시험 |
| URL 면제 조건 반전(URL 이 없을 때 경고) | Type 1C 시험 |
| 빈 URL 도 제공자로 셈 | Type 1C 시험 |
| 참조 경고가 `valid` 도 false 로 만듦 | Type 1C 시험 |
| 참조 경우에 경고와 함께 오류도 보고 | Type 1C 시험 |
| 참조 경우에 경고를 내지 않음 | Type 1C 시험 |
| 빈 UID 의 형식 검사 복원(중복 보고) | 빈 UID 시험, "Missing/Empty" 시험, Type 표 시험 |
| 값이 있는 UID 도 형식 검사를 건너뜀 | 빈 UID 시험, 기존 `ValidateBadUID_IsAnErrorAndNotAlsoAWarning` |

측정이 한 번 어긋났다: 첫 팔을 `false && …` 로 만들었더니 상수 조건 경고가 오류로 취급돼 **빌드가 실패**했다(`arms_m2c_out.txt` 첫 항목 "BUILD FAILED"). 그것은 "터졌다"가 아니라 "측정 안 됐다"라서 경고를 내지 않는 `… && ds == nullptr` 로 바꿔 그 팔만 다시 돌렸고 터졌다(`arms_m2c_out2.txt`). 8팔 전부 터짐은 두 파일을 합친 결과다.

## 4. 영향·비고
- M2b 보고서 §1 의 "Pixel Data … URL 경우 없음(Type 1 로 다룸)" 과 §5 Gap 의 "URL 면제 미지원" 은 이 커밋으로 해소된다.
- 이 모듈이 쓰는 파일은 Pixel Data 를 항상 직접 담으므로 앱이 만든 파일의 보고서는 달라지지 않는다. 바뀌는 것은 외부 파일: (a) 참조 화소만 가진 파일이 오류에서 경고(valid:true)로, (b) 값이 빈 UID 를 가진 파일의 오류가 2건에서 1건으로.
- `valid:true` 인데 열 수 없는 파일이 생긴다: 참조 화소 파일은 검증은 통과하지만 `xpe_dicom_open` 이 화소를 읽지 못한다. 경고가 그 사실을 알린다(리더 결정). 호출하는 쪽이 `warnings` 를 읽는지는 확인하지 못했다(Gap).

## 5. Gap / 잔여 위험
Gap
- clients·gui 가 `warnings` 항목을 어떻게 소비하는지는 보지 않았다(새 경고가 화면에 나타날 수 있다).
- 실제 JPIP 참조 화소 파일로는 시험하지 않았다. 합성 파일(Pixel Data 삭제 + URL 요소 삽입)이다.
- 패딩 검사의 NUL 갈래는 DCMTK 가 문자열 뒤 공백을 자르고 NUL 만 있는 UID 를 만들기 어려워 시험하지 못했다(M2 때부터 알려진 중복 방어, 그대로).
- 표준 원문은 이번에 다시 읽지 않았다. Type 1C 문구는 M2b §1 에서 읽은 PS3.3 2026d 의 인용을 그대로 쓴다.

잔여 위험
- Provider URL 의 값이 유효한 URL 인지는 검사하지 않는다(값이 있기만 하면 제공자로 본다).
