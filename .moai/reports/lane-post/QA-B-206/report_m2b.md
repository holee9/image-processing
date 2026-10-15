# QA-B-206 M2b — Patient's Name·Patient ID 는 Type 2 (Codex #111, #251)

회신 전문: 메인 저장소 `.moai/state/codex-archive/111.md`. 앞선 커밋: M2 `a7b50631`, M1c `6313477f`. **사용자 결정(2026-10-03): 표준대로 허용.**

## 0. 결과

| 항목 | 결과 |
|---|---|
| 수정 | 검증기의 필수 목록 항목마다 "값이 있어야 하는가"(Type 1)와 "존재만"(Type 2)을 나눴다. Patient's Name `(0010,0010)`·Patient ID `(0010,0020)` 는 존재만 검사: 없으면 오류(`Missing required Type 2 tag`), 비어 있으면(길이 0·공백뿐) 통과. 나머지 항목의 빈 값 거부는 그대로 |
| 대조 | 목록 11개 전체를 DX IOD 각 모듈의 Type 표와 원문 대조(§1) — **Patient 둘만 틀렸고** 나머지는 Type 1 이 맞다. Pixel Data 는 Type 1C |
| 시험 | 새 3개, 고친 기존 2개(§2) |
| 반증 | 7팔 모두 터짐(§3), 소스 바이트 복원 |
| 검증(관측) | ci-dicom 전체 ctest(성능 시험 제외) 348 통과·0 실패(최종 재빌드 뒤), doxygen 경고 0, 헤더 점검 20개 0건 |

## 1. 목록 전체의 Type 대조 (카드 지시 2)

원문: DICOM PS3.3 현행판(`dicom.nema.org/medical/dicom/current/output/chtml/part03/`, 페이지 머리말 "PS3.3 2026d")을 직접 읽었다. DX IOD 의 모듈 구성은 A.26.3 "Digital X-Ray Image IOD Modules" 의 표: Patient C.7.1.1 **M**, General Study C.7.2.1 **M**, General Series C.7.3.1 **M**, DX Series C.8.11.1 **M**, General Image C.7.6.1 **M**, Image Pixel C.7.6.3 **M**, DX Image C.8.11.3 **M**, SOP Common C.12.1 **M** (모두 필수 모듈).

| 태그 | 속성 | 모듈 (표준 절) | 표준 Type | 코드 처리 | 원문 인용 |
|---|---|---|---|---|---|
| (0010,0010) | Patient's Name | Patient C.7.1.1, `sect_C.7.html` Table C.7-1 | **2** | **존재만**(수정됨: 전에는 Type 1 처럼 값까지) | "Patient's Name (0010,0010) 2 Patient's full name." |
| (0010,0020) | Patient ID | Patient C.7.1.1, Table C.7-1 | **2** | **존재만**(수정됨) | "Patient ID (0010,0020) 2 Primary identifier for the Patient." |
| (0020,000D) | Study Instance UID | General Study C.7.2.1 | 1 | 존재 + 값 | "Study Instance UID (0020,000D) 1 Unique identifier for the Study." |
| (0020,000E) | Series Instance UID | General Series C.7.3.1 | 1 | 존재 + 값 | "Series Instance UID (0020,000E) 1 Unique identifier of the Series." |
| (0008,0018) | SOP Instance UID | SOP Common C.12.1 | 1 | 존재 + 값 | "SOP Instance UID (0008,0018) 1 Uniquely identifies the SOP Instance." |
| (0008,0060) | Modality | DX Series C.8.11.1 | 1 | 존재 + 값 | "Modality (0008,0060) 1 Type of device, process or method that originally acquired the data used to create this Series." |
| (0028,0010) | Rows | Image Pixel Description Macro, Table C.7-11c(`sect_C.7.6.3.3.html`), Image Pixel C.7.6.3 가 포함 | 1 | 존재 + 값 | "Rows (0028,0010) 1 Number of rows in the image." |
| (0028,0011) | Columns | 같은 매크로 | 1 | 존재 + 값 | "Columns (0028,0011) 1 Number of columns in the image." |
| (0028,0100) | Bits Allocated | 같은 매크로 | 1 | 존재 + 값 | "Bits Allocated (0028,0100) 1 Number of bits allocated for each pixel sample." |
| (0028,0101) | Bits Stored | 같은 매크로 | 1 | 존재 + 값 | "Bits Stored (0028,0101) 1 Number of bits stored for each pixel sample." |
| (7FE0,0010) | Pixel Data | Image Pixel C.7.6.3 | **1C** | 존재 + 값(URL 경우 없음) | "Pixel Data (7FE0,0010) 1C A data stream of the pixel samples that comprise the Image. … Required if Pixel Data Provider URL (0028,7FE0) is not present." |

결론: 같은 잘못은 다른 항목에 없다. Type 2 는 Patient 둘뿐이다. Pixel Data 는 Type 1C 인데 이 검증기에는 Pixel Data Provider URL 경우(`(0028,7FE0)`)가 없어 Type 1 로 다룬다(URL 로 가는 파일은 이 모듈이 만들지 않는다; 외부 파일이 URL 만 가지면 거부될 수 있다 — Gap).

참고(표준과 맞는 현재 동작): 값이 비어 있는 UID 는 "값 없음" 과 "UID 형식 오류" 두 오류로 보고된다(빈 문자열은 UID 형식도 아니다). 중복이지만 틀린 보고는 아니라 건드리지 않았다.

## 2. 수정과 시험

**수정.** `DicomValidator.cpp`: 필수 목록을 `struct RequiredTag { key, tag, needsValue }` 로 바꿔 항목마다 Type 을 적었다(주석에 위 표의 모듈·절). 루프는 존재를 먼저 검사(없으면 Type 에 따라 `Missing required Type 1/2 tag`), `needsValue == false` 이면 거기서 끝, 아니면 M2 의 값 검사.

**시험** (`test_dicom_validator.cpp`):
- 새 `PatientNameAndPatientIdWithNoValueAreConformantTypeTwoAttributes`: PatientID·PatientName 가 각각 빈 문자열, 공백뿐, 둘 다 빈 익명화 파일 → `valid:true`, errors 비어 있음.
- 새 `PatientNameAndPatientIdThatAreAbsentAreStillAnErrorBecauseTypeTwoNeedsPresence`: 각각 없음 → 해당 태그 오류, 둘 다 없음 → 오류 2개이고 메시지가 "Type 2" 를 말함.
- 새 `EveryRequiredAttributeIsJudgedByTheTypeTheStandardGivesIt`: 값이 있는 6개 속성(Patient 둘, Study·Series·SOP UID, Modality)에 대해 "없음 → 오류(두 Type 모두)", "빈 값 → 'no value' 오류는 Type 1 만, Type 2 는 오류 전혀 없음". 메시지까지 보는 이유는 빈 UID 가 형식 오류로도 보고돼 같은 태그의 오류만으로는 Type 1 이 Type 2 로 내려가도 숨겨지기 때문이다(반증 §3 이 보여 줌). Rows 등 US 속성과 Pixel Data 는 M2 의 `ARequiredTypeOneTagWithNoValueIsAnErrorWhateverTheKindOfTag` 가 계속 본다.
- 고친 기존 시험 2개: M2 의 `ARequiredTypeOneTagWithNoValueIsAnErrorWhateverTheKindOfTag` 에서 Patient 네 경우를 뺐다(이유 주석), `AMissingTagAndAnEmptyTagAreReportedWithDifferentMessages` 의 대상을 Patient ID → Study Instance UID 로 바꿨다(Type 1 이어야 "없음/값 없음" 이 갈린다). `ValidateMissingPatientID_ReportsError` 는 그대로 통과(존재 필수).

## 3. 반증 (`arms_m2b.py.txt`, `arms_m2b_out.txt`)

| 끈 방어 | 결과 |
|---|---|
| Type 2 건너뛰기 제거(값을 다시 모두 판정) | 빈 값 통과 시험, 표 시험 빨강 |
| Patient Name 을 Type 1 로(M2 상태) | 같은 두 시험 + 존재 시험 빨강 |
| Patient ID 를 Type 1 로 | 같음 |
| Study UID 를 Type 2 로(실수) | 표 시험 빨강(첫 실행에서는 빨간 시험이 기대와 달랐다: 빈 UID 는 UID 형식 오류도 내서 태그 단위 단언이 가려졌다 — 시험을 "값 없음" 메시지 기준으로 고쳐 다시 돌렸다) |
| Modality 를 Type 2 로(실수) | M2 의 Type 1 시험, 표 시험 빨강 |
| Type 2 의 존재 검사 제거(없어도 통과) | 존재 시험, 표 시험, 기존 `ValidateMissingPatientID_ReportsError` 빨강 |
| Type 2 누락 메시지가 "Type 1" 이라고 말함 | 존재 시험 빨강 |

## 4. 영향·비고
- M2 보고서(`report_m2.md`)의 C5 서술 중 "Patient Name·ID 가 빈 값이면 오류" 는 이 커밋으로 대체된다. 앱이 만든 파일은 작성기가 두 값을 `ANONYMOUS` 로 채워 영향이 없다고 리더가 확인했다.
- 요구 문구(REQ-DICOM-024 의 Type 1 목록)는 리더가 병합 때 고친다(카드).

## 5. Gap / 잔여 위험
Gap
- 검증기가 확인하지 않는 DX IOD 의 다른 필수 속성(Photometric Interpretation, DX Image 모듈의 Type 1 들 등)은 이번에 대조하지 않았다. 대조 범위는 이 검증기의 필수 목록 11개다.
- Pixel Data 가 URL 로만 제공되는 외부 파일(Type 1C 의 면제 경우)은 이 검증기가 거부한다. 이 모듈이 그런 파일을 만들거나 읽지는 않는다.
- 표준 원문은 현행판(PS3.3 2026d) 한 판만 읽었다. Codex 가 인용한 2024e·2025b 와 항목의 Type 이 같은지는 읽지 않았다.

잔여 위험
- 이전에 거부되던 빈 Patient Name/ID 의 파일이 이제 통과한다(표준대로, 사용자 결정).
