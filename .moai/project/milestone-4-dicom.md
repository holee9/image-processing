# 4단계 — 앱이 저장한 DICOM 이 표준에 맞는다 (통과 조건, 2026-10-14)

순서: pre 기본(1단계, 닫힘) → post 기본(2단계) → 운영자 앱(3단계, PR #266) → **DICOM(이 문서)**. 통과 조건은 실행 전에 적는다. 근거 실측: post `QA-B-219`(dev/postprocess `83b0d534`, `.moai/reports/lane-post/QA-B-219/report.md`) — 앱 경로 값(`CreateMetadata`)과 2단계 체인 출력 3072² uint16 으로 쓴 DX 파일.

**왜 모듈 검증기만으로는 안 되는가.** `xpe_dicom_validate` 의 "DX IOD 적합"(REQ-DICOM-023)은 REQ-DICOM-024 의 부분집합이라 표준 적합을 뜻하지 않는다. QA-B-219 에서 모듈 검증기는 오류 0 인데 표준 IOD 검증기는 필수 누락 10건을 냈다. 그래서 판정은 독립 검증기로 한다.

| # | 통과 조건 | 판정 방법 | 2026-10-14 실측 |
|---|---|---|---|
| D1 | 앱이 저장한 DX 파일이 표준 IOD 검증기(`dicom-validator` 0.9.0, 표준판 2026d)에서 오류 0. 단 C.8.7.8 X-Ray Acquisition Dose(U 모듈, 속성 전부 Type 3)의 "unexpected" 4건(KVP·SID·Exposure·µAs)은 탐침 증거로 검증기 한계 처리, 그 외 예외 없음 | 검증기 출력 전문 | 실패(14건 = 필수 10 + 한계 4) |
| D2 | 모듈 검증기(`xpe_dicom_validate`) 오류 0 | 모듈 결과 JSON | 통과 |
| D3 | DICOM 화소 = 같은 DLL·같은 입력의 모듈 체인 출력, 바이트 동일(비압축·J2K 무손실 둘 다) | 바이트 비교 | 통과 |
| D4 | 취득 값(부위·kVp·mAs·SID·화소 간격·시각) 왕복 | 쓰기 값 대 읽기 값 | 통과 |
| D5 | 환자·검사 값(Patient Name/ID, Study UID/ID/날짜·시각, Accession)을 사용자가 앱에서 넣고, 파일에 그대로 남는다. 빈 값이면 저장 전에 알린다 | E2E + 파일 덤프 | 실패(입력 경로 없음, ANONYMOUS 고정) |
| D6 | 쓰기 ≤ 100 ms, 검증 ≤ 200 ms, J2K 쓰기 ≤ 1000 ms (3072², CI 5회 중앙값) | 모듈 시간 | 로컬 통과(10.3 / 13.0 / 653 ms) — CI 미측정 |
| D7 | 앱 File 메뉴로 저장한 파일이 D1~D5 를 만족(모듈 하네스가 아니라 앱 경로) | Native E2E | 미측정 |
| D8 | 외부 SCP 상호운용(C-STORE) | 미정 — 범위 결정 필요(아래) | 측정 안 함(storescp 없음) |

## 값 결정 (리더, 2026-10-14)

| 항목 | 결정 | 근거 |
|---|---|---|
| Acquisition Context Seq (0040,0555) | 빈 시퀀스 | Type 2, 값 없음이 사실 |
| Imager Pixel Spacing (0018,1164) | 이미 쓰는 화소 간격 값 | 같은 물리량 |
| Lossy Image Compression (0028,2110) | `00` | 비압축·무손실 J2K 만 씀 |
| Presentation Intent Type (0008,0068) | `FOR PRESENTATION` | SOP 클래스가 For Presentation(REQ-DICOM-013) |
| Window Center/Width | 앱이 그 영상에 쓴 자동 창(해부 영역 창) 값 | FOR PRESENTATION 이면 1C 필수. REQ-DICOM-022·`dicom_api.h` 의 "쓰지 않는다" 문구는 변경 대상 |
| Image Laterality (0020,0062) | 입력 경로가 생기기 전까지 `U` | 모르는 값을 R/L 로 지어내지 않는다 |
| Detector Type (0018,7004) | `SCINTILLATOR` | 제품 표가 CsI 섬광체 기준(`vg_table_water_csi600_victre.csv`). 제품 사양 확인 시 재검토 |
| Anatomic Region Seq (0008,2218) | 앱 부위 목록 → DICOM CID 4031 코드 매핑 표 | 표를 레인이 만들어 리더가 검토 |
| Pixel Intensity Relationship (+Sign) | **추정으로 정하지 않는다** — 저장 화소에서 뼈/공기 값을 실측해 LIN/LOG 와 부호를 정한다(반전이 화소인지 Presentation LUT 인지에 따라 부호가 바뀜) | 3단계 §13: 극성은 Presentation LUT 에서 반전 |
| Patient Orientation (0020,0020) | D5 와 함께 입력 경로를 만든다. 그 전까지는 D1 실패로 남는다 | 값을 지어내지 않는다 |

## 범위

- D8(외부 SCP)은 실사용 범위(PACS 전송을 1차 출하에 넣는가)를 사용자 결정 전까지 **보류**. 넣는다면 DCMTK `storescp` 로 왕복.
- 고스트·래그, MWL·GSPS(#237), 부호 있는 화소(#236), 949 밖 경로(#239)는 이 단계 밖.

이 문서의 D1~D7 이 통과하면 "앱이 저장한 DICOM 사용 가능" 으로 보고한다.
