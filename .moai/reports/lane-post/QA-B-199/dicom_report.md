# QA-B-199 DICOM — SPEC-XPE-P1B-DICOM 작업표·요구 실태 대조

범위: `.moai/specs/SPEC-XPE-P1B-DICOM/`(spec.md·plan.md), `modules/dicom/`, `docs/dicom/RTM-DICOM-001_Requirements_Traceability_Matrix.md`, `.github/workflows/ci.yml`. 코드는 바꾸지 않았다. 이미 처분된 #236(부호 화소)·#237(GSPS)·#239(경로 문자)는 다시 세지 않고 참조만 한다.

## 0. 방법과 한계

방법은 ENH 보고서 §0 과 같다. DICOM 에서 다른 점:

- plan.md 는 요구를 작업 행이 아니라 **마일스톤 단위**("REQ Coverage: REQ-DICOM-001 through 012")로 매긴다. 추출기는 각 작업(D-0~D-12)이 속한 절의 coverage 줄을 상속시킨다. 처음 추출기는 "through" 범위를 놓쳤고, 이를 고친 뒤(extract_script.txt) 요구 46개 중 1~40 이 작업에 걸림을 확인했다.
- 검증(`verify_DICOM.txt`): 시험 인용 75건은 전부 인용한 줄 ±2 안에 실재한다. 실패 5건은 시험이 아닌 쪽이다 — 구현 인용 3건(줄 번호 없는 산문 2건, 파일 길이 밖 1건)은 쓰지 않았고, 헤더 약속 2건은 **문장은 실재하나 줄 번호가 틀렸다**: `dicom_api.h` 의 “@return XPE_ERR_DICOM_INVALID if the file is not a valid DICOM Part 10 file.” 는 72 가 아니라 75행이고, `CMakeLists.txt` 의 `src/DicomWriter.cpp # SWU-4.2 REQ-DICOM-012..020` 은 455 가 아니라 20행이다(내가 grep 으로 확인). 아래는 바로잡은 줄 번호를 쓴다.
- 핵심 주장 두 건을 내가 코드에서 확인했다: mAs 는 `DicomWriter.cpp:181` 에서 `DCM_ExposureInmAs`(0018,9332)로 쓰이고 `DicomReader.cpp:776` 에서 같은 태그로 읽힌다. 취소 시험 `CancelCStore_TerminatesOperation` 은 `test_dicom_network_scu.cpp:179` 에서 무조건 `GTEST_SKIP` 한다.
- 실행하지 않았다. 결함 후보는 읽기로 찾은 후보다.

## 1. 작업 행 (plan.md 12행)

| 작업 | 표 상태 | 작업 내용 | 요구 | 실제 | 판정별 개수 |
|---|---|---|---|---|---|
| D-0 | (no status column) | Configure Third-Party Dependencies |  | **(요구 없음)** |  |
| D-1 | (no status column) | Header and Opaque Handle (RED-GREEN) | 001, 002, 003, 004, 005, 006, 007, 008, 009, 010, 011, 012 | **부분** | asserted 5, constant_only 1, partial 5, self_referential 1 |
| D-2 | (no status column) | Pixel Data Extraction (RED-GREEN) | 001, 002, 003, 004, 005, 006, 007, 008, 009, 010, 011, 012 | **부분** | asserted 5, constant_only 1, partial 5, self_referential 1 |
| D-3 | (no status column) | Metadata Extraction (RED-GREEN) | 001, 002, 003, 004, 005, 006, 007, 008, 009, 010, 011, 012 | **부분** | asserted 5, constant_only 1, partial 5, self_referential 1 |
| D-4 | (no status column) | Handle Close and Edge Cases (RED-GREEN-REFACTOR) | 001, 002, 003, 004, 005, 006, 007, 008, 009, 010, 011, 012 | **부분** | asserted 5, constant_only 1, partial 5, self_referential 1 |
| D-5 | (no status column) | Uncompressed Write (RED-GREEN) | 013, 014, 015, 016, 017, 018, 019, 020, 021, 022 | **부분** | asserted 3, no_test 2, partial 4, self_referential 1 |
| D-6 | (no status column) | JPEG 2000 Lossless Write (RED-GREEN) | 013, 014, 015, 016, 017, 018, 019, 020, 021, 022 | **부분** | asserted 3, no_test 2, partial 4, self_referential 1 |
| D-7 | (no status column) | Error Handling and Rescale (RED-GREEN-REFACTOR) | 013, 014, 015, 016, 017, 018, 019, 020, 021, 022 | **부분** | asserted 3, no_test 2, partial 4, self_referential 1 |
| D-8 | (no status column) | Conformance Validation (RED-GREEN-REFACTOR) | 023, 024, 025, 026, 027, 028 | **부분** | asserted 1, partial 5 |
| D-9 | (no status column) | C-STORE Implementation (RED-GREEN) | 029, 030, 031, 032, 033, 034, 035, 036, 037, 038, 039, 040 | **부분** | asserted 2, constant_only 1, no_code 2, no_test 1, partial 6 |
| D-10 | (no status column) | C-FIND MWL Implementation (RED-GREEN) | 029, 030, 031, 032, 033, 034, 035, 036, 037, 038, 039, 040 | **부분** | asserted 2, constant_only 1, no_code 2, no_test 1, partial 6 |
| D-11 | (no status column) | Cancel and Thread Safety (RED-GREEN-REFACTOR) | 029, 030, 031, 032, 033, 034, 035, 036, 037, 038, 039, 040 | **부분** | asserted 2, constant_only 1, no_code 2, no_test 1, partial 6 |

**읽는 법.** plan.md 에는 상태 열이 없어 표 상태와의 어긋남은 계산하지 않았다. "실제" 는 작업이 상속한 요구들의 판정에서 규칙으로 계산했다. D-12(통합·경계 시험)는 plan 이 `test_dicom_integration.cpp`·`test_dicom_boundary.cpp` 를 만들라고 적었지만 이 두 파일은 존재하지 않는다(`modules/dicom/tests/` 는 `mock_scp.cpp`, `test_dicom_network_scu.cpp`, `test_dicom_reader.cpp`, `test_dicom_validator.cpp`, `test_dicom_writer.cpp`, `test_parameter_dependency.cpp`). D-12 는 요구를 하나도 이름으로 대지 않는다.

## 2. 정의된 요구 중 어떤 작업에도 안 걸린 것 — 두 축

| 축 | 정의 | 개수(46개 중) | 해당 |
|---|---|---|---|
| 이름(작업) | 어느 작업(마일스톤 coverage 포함)에도 id 가 없음 | 6 | REQ-DICOM-041, 042, 043, 044, 045, 046 |
| 이름(시험 텍스트) | 시험 소스 어디에도 id 문자열이 없음 | 24 | 007, 014, 015, 016, 019, 020, 025, 027, 028, 030~037, 039, 041, 042, 043 외(`extract_out.txt`) |
| 행위 | 독립 기대값 단언 없음(no_test·constant_only·self_referential)이거나 코드 없음(no_code) | 13 | no_test 6: 014, 021, 039, 041, 042, 046 / no_code 3: 032, 035, 043 / constant_only 2: 010, 040 / self_referential 2: 009, 015 |
| 행위(넓게) | `asserted` 가 아님(partial 22 포함) | 35 | |

차이: 이름(작업) 축 6 대 행위 축 13(넓게 35). 이름 축 6건(041~046)은 plan 의 어느 마일스톤도 비기능 요구(041 C 연결·__cdecl, 042 예외 차단, 043 로깅, 044 누수, 045 리더 재진입, 046 호출자의 네트워크 직렬화)를 대지 않아 생겼다. 046 은 호출자 책임 요구라 코드가 없는 것이 맞고, 판정이 no_test 인 이유는 그 책임을 문서·시험 어디서도 확인하지 않기 때문이다. 그중 045 는 partial, 044 는 partial, 041·042·046 은 시험이 없고 043 은 코드가 요구와 일부만 맞는다. 이름 축이 못 잡는 9건(009, 010, 014, 015, 021, 032, 035, 039, 040)은 마일스톤이 요구를 대고 있는데도 행위가 비어 있다.
파서 대조군: 추출기가 `D-12` 를 요구 없는 작업으로 내놓고, `extract_out.txt` 의 `tasks_named_but_undefined []` 는 작업이 정의되지 않은 id 를 대지 않음을 보인다.

## 3. RTM ✓ 인데 시험이 없는 것

`rtm_out.txt`. DICOM RTM 의 ✓ 행 137개: 시험 id(`TC-`·`STC-`)가 있는 행 59개, 없는 행 78개(§3.1~§7 의 SRS 요구 정의 행이 대부분이라 시험 열이 없음). 시험 id 59종 중 시험 소스에 문자열로 나오는 것은 2종이다(대조군 `TC-109` 가 찾아짐 — `Tc109_*` 시험). 나머지 57행의 id(`TC-101`~`TC-413`, `STC-001`~`STC-008`)는 시험 소스 어디에도 없다. 합성 대조군은 `rtm_control.txt`(`Tc101_Foo` 에서 TC-101 을 찾음). `docs/dicom/` 에는 시험 데이터셋 문서(TDS)가 없고 TC id 정의처는 RTM-DICOM 뿐이다(grep: `TC-101` 은 이 RTM 에만).
QA-B-189 가 이미 RTM 을 재계산해 90.8% 추적, FR-DCM-301~306 ✗, FR-DCM-109 는 실제 시험 이름을 언급하는 상태로 만들었다. 그래서 이 이름 축 결과는 "RTM 이 시험 이름이 아닌 id 를 인용한다" 의 측정이지 "시험이 없다" 의 측정이 아니다. FR-DCM-xxx 가 실제 단언되는지(행위 축)는 이번에 가르지 않았다 → Gap.

## 4. 헤더가 하지 않는 일을 약속하는 문장

verify_DICOM.txt: 12건 중 10건은 줄 ±2 안에서 검증, 2건은 위 §0 의 줄 번호 정정.

1. `dicom_api.h:252-253` “dataSize == 0 means unspecified and is accepted” — 쓰기가 `img->dataSize` 를 픽셀 데이터 길이로 넘기므로 dataSize==0 은 길이 0 PixelData 로 XPE_OK 가 나오고, 읽기가 그 파일을 거부한다(`DicomReader.cpp:716-720`). 시험은 반환값이 INVALID_INPUT 이 아님만 단언.
2. `dicom_api.h:355` “Connection/operation timeout in milliseconds (0 = no timeout)” — `timeoutMs==0` 은 setter 만 건너뛰어 DCMTK 기본값이 남는다(`DicomNetworkSCU.cpp:86`). `timeoutMs` 는 1000 으로 나뉘어 1..999 ms 는 0 s 가 된다(`:87-89`, cfindMwl `:180-184` 도 동일).
3. `dicom_api.h:277-278` “Bit-exact round-trip is guaranteed.” — 선언한 bitsStored 를 넘는 화소는 쓰기가 PROCESSING_FAILED 로 실패하고, `xpe_dicom_write_j2k` 는 `img->format` 을 검사하지 않아 FLOAT32·UINT8 버퍼를 uint16 으로 재해석한다.
4. `dicom_api.h:220` “XPE_ERR_DICOM_INVALID if the file carries no dataset.” — `getDataset()==nullptr` 는 open 성공 뒤에 도달 불가(에이전트가 도달 가능한 입력을 못 찾음).
5. `dicom_api.h:75` “…if the file is not a valid DICOM Part 10 file.” — 헤더가 바로 아래 @note(78-81)에서 meta header 없는 데이터셋이 열린다고 스스로 밝힌다.
6. `DicomNetworkSCU.cpp:316` “Add Scheduled Procedure Step Sequence as required by MWL” — 시퀀스를 추가하지 않는다(318-322 는 최상위 universal-match 키 다섯 개).
7. `DicomNetworkSCU.cpp:25-30` “Well-known SOP classes for DX storage” — `DX_SOP_CLASS_UIDS` 는 어디서도 참조되지 않고 MWL FIND UID 도 섞여 있다.
8. `DicomNetworkSCU.h:5` “Wraps DCMTK storescu and wlmscuscu…” — 구현은 `DcmSCU` 를 쓰고, 취소는 두 점검점에서만 보이는 단일 플래그다.
9. `dicom_api.h:416-420` “Signal cancellation to any in-progress C-STORE or C-FIND operation.” — 전송 중·C-FIND 교환 중 도착한 취소는 보이지 않고(`:425-428` 에 스스로 밝힘) 플래그는 모든 호출 입구에서 지워진다.
10. `CMakeLists.txt:20` — 요구 범위 주석이 낡았다(REQ-DICOM-012 는 Close, 쓰기는 013..022, 검증기는 023..028, 읽기는 001..012; `dicom.cpp` 는 "version entry point" 로 적혔지만 10 개 수출을 모두 담음).
11. `test_dicom_network_scu.cpp:6-8` “storescp/wlmscpfs mock servers are launched on random localhost ports” — 실제는 프로세스 안 `DcmSCP` 하위 클래스이고 포트는 빈 포트를 찾아 해제한 값(무작위 아님).
12. `DicomWriter.cpp:309` 주석 “2x input size” — 버퍼는 `w*h*4+4096` 바이트.

## 5. 빌드·CI 에서 빠진 것

- `dicom-build` 잡(`ci.yml:449`)이 `ci-dicom` 프리셋으로 전체 ctest 를 돌린다(`:528`, 같은 `-E "Performance|Within[0-9]+ms|…"` 제외). DICOM 시험 이름에는 Performance·Within 이 없어 제외되는 시험이 없다. 즉 DICOM 은 CI 에서 시험이 빠지는 문제가 없다.
- 반대 방향: 시험이 있어도 무엇을 단언하는지가 약하다(§2, §6). 모의 SCP 는 항상 성공을 돌려주고 받은 데이터셋을 보지 않고 버리며 어떤 호출 AE 도 받는다. "Timeout" 두 시험은 리스너 없는 포트에 연결해 연결 거부를 보는 것이라 타임아웃이 아니다. 주요 fixture 는 모듈 자신의 `xpe_dicom_write` 로 만들어 쓰기·읽기의 태그 불일치가 상쇄된다. 외부 시스템이 만든 DICOM 파일은 한 개도 없다.
- plan 이 이름 붙인 `test_dicom_integration.cpp`·`test_dicom_boundary.cpp` 가 없다(§1).
- 여러 부정 시험이 DCMTK 가 인코딩할 수 없으면 `GTEST_SKIP` 한다. 누수 시험은 Windows 전용 힙 걷기다(핵심 경로 일부만).
- 시험 이름이 약속하는 것과 단언이 다르다: `CloseValid_NoLeak`, `RescaleDefaults_OneAndZero`(validator valid=true 만), `GetMetadata_MissingTagsDefault`(모든 태그가 있는 파일을 연다), `CStoreTimeout/CFindTimeout`(거부를 시험), `CancelCStore_TerminatesOperation`(무조건 스킵).

## 6. 결함 후보 (고치지 않음, 읽기로 찾은 후보)

| # | 요구 | 위치 | 후보 |
|---|---|---|---|
| C1 | REQ-DICOM-015/009 | `DicomWriter.cpp:181`, `DicomReader.cpp:776` | SPEC 은 Exposure(0018,1152)↔mAs. 코드는 `ExposureInmAs`(0018,9332) 로 읽고 쓴다. 왕복 시험은 같은 선택을 공유해 통과하지만 다른 시스템의 (0018,1152) 파일은 mAs 0 으로 읽힌다. |
| C2 | REQ-DICOM-009/015 | `DicomWriter.cpp:199-215`, `DicomReader.cpp:806-840`, `xpe_types.h:104,114` | SPEC 은 acquisitionTime 이 epoch ms, 타입 헤더와 두 코드 경로는 epoch 초. SPEC 과 타입 헤더가 어긋나고 코드는 헤더를 따른다. |
| C3 | REQ-DICOM-003 | `DicomReader.cpp:131-150`, `:113-116` | 128바이트 서문·DICM 매직도 meta header 도 없는 데이터셋이 XPE_OK 로 열린다(시험 `OpenDatasetWithoutMetaHeader_TreatedAsExplicitLE` 가 이를 못 박음). SPEC 은 INVALID. 헤더는 스스로 밝힘. |
| C4 | REQ-DICOM-026 | `DicomValidator.cpp:70-77` | SPEC 이 정한 보고(`tag ""`, `"Not a valid DICOM file"`) 대신 `tag "0008,0000"`, `"File cannot be parsed as DICOM: …"` 를 낸다. 시험은 errors 가 비어 있지 않음만 확인. |
| C5 | REQ-DICOM-024 | `DicomValidator.cpp:201-211, 24-36` | "present and non-empty" 중 존재만 검사, 빈 Type 1 값이 통과. 화소 표현과 선언 형식의 일관성 검사 코드 없음. 서문·매직은 직접 검사하지 않음. |
| C6 | REQ-DICOM-025 | `DicomValidator.cpp:214-226` | 잘못된 UID 가 warnings 와 errors 양쪽에 들어가고 valid=false. |
| C7 | REQ-DICOM-032/039 | `DicomNetworkSCU.cpp:136-145, 101-103, 119-122` | SPEC 은 C-STORE 실패 시 WARNING 알림과 이유, 취소 시 알림에 취소 표시를 요구. SCU 는 `spdlog::warn` 만 하고 `xpe_alert_push` 를 한 번도 호출하지 않는다(내가 grep: 이 파일에 호출 없음). |
| C8 | REQ-DICOM-035/034 | `DicomNetworkSCU.cpp:312-346` | `ScheduledStationAETitle`·`ScheduledProcedureStepStartDate` 키가 무시되고 Modality 가 시퀀스(0040,0100) 안이 아니라 최상위에 들어간다. 표준 MWL 질의가 아니다. |
| C9 | REQ-DICOM-038 | `DicomNetworkSCU.cpp:247-255` | `sendFINDRequest` 의 `cond.bad()` 만 보고 최종 C-FIND-RSP 상태(0xA700 등 실패)를 읽지 않아 실패한 질의가 `[]` 와 XPE_OK 로 돌아온다. |
| C10 | REQ-DICOM-031 | `DicomNetworkSCU.cpp:86-90, 180-184` | `timeoutMs/1000` — 1000 ms 미만이 0 s. 정체하는 상대로 한 시험이 없다. |
| C11 | REQ-DICOM-029 | `DicomNetworkSCU.cpp:92-98` | 전송 구문을 파일 구문과 무관하게 고정 제안(Explicit LE, J2K Lossless, Implicit LE). JPEG Lossless 파일은 트랜스코딩 없이 보내진다. |
| C12 | REQ-DICOM-016/013 | `DicomWriter.cpp:148-149`, `dicom.cpp:105-134` | Rows/Columns 를 `static_cast<Uint16>` 로 범위 검사 없이 변환하고 화소 형식을 검증하지 않아 FLOAT32·UINT8 버퍼가 uint16 단어로 쓰인다. |
| C13 | REQ-DICOM-013 | `DicomWriter.cpp:49-53`, `dicom.cpp:108-109` | `dataSize==0` 을 가드는 받아들이지만 PixelData 길이로 쓴다(헤더 약속 1). |
| C14 | REQ-DICOM-044 | `dicom.cpp:44-48` | `reader.open()` 이 던지면 `new XpeDicomHandle` 이 해제되지 않는다(h 가 catch 밖). |
| C15 | REQ-DICOM-009 | `DicomReader.cpp:747-845` | Patient ID·Study/Series UID·Modality 를 "핸들로 얻을 수 있다" 고 했으나 `getMetadata` 는 읽지 않고 접근자도 없다. |
| C16 | REQ-DICOM-043 | `dicom.cpp:37-212` | 진입만 DEBUG 로 기록하고 종료는 안 한다. 실패는 대개 `spdlog::warn`(ERROR 아님), 로깅이 xpe_common API 가 아닌 모듈 자체 spdlog. |

보조: 쓰기가 환자명·ID 를 'ANONYMOUS' 로 고정하고 파일마다 새 무작위 StudyInstanceUID 를 만든다(`DicomWriter.cpp:113-120`) — 한 촬영의 두 영상이 같은 검사를 공유하지 못한다. 질문(a)~(f)에 대한 에이전트의 노트는 `agent_notes_DICOM.txt` 에 그대로 있다.

추적 항목: 이 대조를 담을 더 맞는 기존 항목이 없다 — **새 이슈 필요**(리더 판단). C1·C7·C9 가 우선 후보(데이터가 틀린 채 XPE_OK 로 흐르거나 알림이 안 나가는 것). 이번 커밋은 `Refs #130`.

## 7. 수정안 초안 (리더 소유 파일)

`drafts/DICOM_plan_rtm_draft.txt` — plan.md D-12 의 존재하지 않는 시험 파일 정정과 REQ-DICOM-041~046 의 마일스톤 배정안, RTM 의 시험 id 정의 부재 정리안.

## 8. Gap / 잔여 위험

Gap
- 결함 후보 C1~C16 은 실행 재현을 하지 않았다.
- RTM ✓ 137행의 행위 축(FR-DCM-xxx 를 실제 단언하는 시험)을 가르지 않았다. QA-B-189 의 재계산 결과를 전제로 두고 다시 세지 않았다.
- 에이전트의 구현 줄 번호는 쓰지 않았다(DICOM 에서는 46건 중 2건만 파일 밖이나 DISP 의 13건 때문에 같은 기준을 적용). `partial` 사유 문장은 내가 직접 읽은 mAs 태그·취소 시험 스킵·알림 부재 외에는 에이전트 서술이다.
- 이미 처분된 #236·#237·#239 는 참조만 하고 재집계하지 않았다.

잔여 위험
- C2(acquisitionTime 단위)는 SPEC 과 타입 헤더 중 어느 쪽이 맞는지를 이번 대조로 정하지 못했다.
- 모의 SCP 가 실패를 만들 수 없어 REQ-DICOM-029~039(네트워크 오류 경로)는 어떤 시험으로도 관측되지 않는다. 실제 DICOM 장비에서 처음 드러날 수 있다.

## 부록 A. 요구별 판정 전체 표

| 요구 | 판정 | 독립 기대값의 출처 | 시험이 단언하지 않는 문구 | 근거 시험 |
|---|---|---|---|---|
| REQ-DICOM-001 | asserted | valid Part 10 files (module-written, and DCMTK-written JPEG Lossless/Process 14 copies) must open; success/non | the preamble/DICM-magic/meta-header parsing as separate steps; handle is opaque so nothing about the parse result is inspected beyond later reads | `test_dicom_reader.cpp:121` OpenValid_ReturnsOkAndNonNullHandle; `test_dicom_reader.cpp:701` EverySupportedTransferSyntaxActuallyReads |
| REQ-DICOM-002 | partial | nonexistent path and empty path are hard-coded negative inputs | the 'set *outHandle to NULL' half is not discriminated (the test pre-initialises handle to nullptr); 'exists but cannot be opened for reading' (permis | `test_dicom_reader.cpp:132` OpenMissingFile_ReturnsIOFailed; `test_dicom_reader.cpp:443` OpenEmptyPath_ReturnsIoFailed |
| REQ-DICOM-003 | partial | PNG-header bytes as an independent invalid file | missing preamble, invalid magic bytes, corrupted meta-information: none has a control; a valid dataset with no preamble/meta opens successfully, contr | `test_dicom_reader.cpp:142` OpenInvalidDicom_ReturnsDicomInvalid; `test_dicom_reader.cpp:415` OpenDatasetWithoutMetaHeader_TreatedAsExplicitLE |
| REQ-DICOM-004 | asserted | .1 and .4.70/.4.57 fixtures encoded by DCMTK's own encoder and compared byte-for-byte with the uncompressed or | the table-driven loop iterates the module's own kSupportedTransferSyntaxes, so a list shrunk to 3 entries would pass it (the separate .70/.57 tests mi | `test_dicom_reader.cpp:713` EverySupportedTransferSyntaxActuallyReads; `test_dicom_reader.cpp:586` ReadJpegLossless_DecodesPixelExact; `test_dicom_reader.cpp:2129` ReadJpegLosslessProcess14_DecodesPixelExact |
| REQ-DICOM-005 | asserted | Implicit VR LE file produced by DCMTK saveFile; JPEG Baseline (.50) produced by DCMTK encoder | the compressed-syntax test GTEST_SKIPs if DCMTK cannot encode .50; unsupported syntaxes in the meta-less path are covered by KnownDivergence tests | `test_dicom_reader.cpp:182` UnsupportedTS_ReturnsUnsupportedFormat; `test_dicom_reader.cpp:2192` UnsupportedCompressedTS_ReturnsUnsupportedFormat |
| REQ-DICOM-006 | partial | J2K: hand-built codestreams (test OpenJPEG, Ramp formula) with attributes set by test; native: dimensions of m | bitsStored on the uncompressed and JPEG Lossless paths is never asserted against an independent literal (only J2K asserts 8 and 12); bitsAllocated is  | `test_dicom_reader.cpp:195` ReadImage_PixelDimensionsMatch; `test_dicom_reader.cpp:3268` J2kScope_TwelveBitInASixteenBitWordKeepsItsPrecision; `test_dicom_reader.cpp:3252` J2kScope_EightBitUnsignedSingleComponentIsReadAndDescribedAsSixteenBit |
| REQ-DICOM-007 | partial | dataSize literal 256*256*2 | that the buffer really comes from xpe_alloc_image and is released by xpe_free_image without leaking (no heap check on any successful read) | `test_dicom_reader.cpp:197` ReadImage_PixelDimensionsMatch |
| REQ-DICOM-008 | asserted | J2K: pixels from a test-side OpenJPEG encode of a formula; JPEG-LL: DCMTK encoder output compared with the unc |  | `test_dicom_reader.cpp:3256` J2kScope_EightBitUnsignedSingleComponentIsReadAndDescribedAsSixteenBit; `test_dicom_reader.cpp:3270` J2kScope_TwelveBitInASixteenBitWordKeepsItsPrecision; `test_dicom_reader.cpp:586` ReadJpegLossless_DecodesPixelExact |
| REQ-DICOM-009 | self_referential | none: the only fixtures are written by the module's own writer, which chooses the tags; the expected values ar | tag identity (the module reads (0018,9332) ExposureInmAs, not (0018,1152) Exposure as the SPEC says); Patient ID/Study UID/Series UID/Modality 'stored | `test_dicom_reader.cpp:226` GetMetadata_FieldsPopulated; `test_dicom_writer.cpp:95` WriteMetadata_Preserved |
| REQ-DICOM-010 | constant_only | none | any absent-tag case: the fixture has every tag present; defaults (empty string/0.0f/0) are never compared | `test_dicom_reader.cpp:241` GetMetadata_MissingTagsDefault |
| REQ-DICOM-011 | partial | CRT heap block count over 1000 cycles (control leak of 64 B/cycle is detected) | success-path handles (open+read+close of a good file) are not heap-measured; Windows-only (GTEST_SKIP elsewhere); bound is <1 block per 10 cycles and  | `test_dicom_reader.cpp:252` CloseValid_NoLeak; `test_dicom_reader.cpp:1269` FailurePathsDoNotGrowCrtHeap |
| REQ-DICOM-012 | asserted | survival of delete nullptr | 'no operation' cannot be observed beyond not crashing | `test_dicom_reader.cpp:259` CloseNull_NoOp |
| REQ-DICOM-013 | partial | the written file is read back by the module's own reader and validator; 128-byte lower bound on file size | SOP Class UID 1.2.840.10008.5.1.4.1.1.1.1 and Transfer Syntax 1.2.840.10008.1.2.1 of the output are never read by an independent reader or compared wi | `test_dicom_writer.cpp:53` WriteBasic_CreatesFile; `test_dicom_validator.cpp:291` ValidateWriterOutput_IsConformant |
| REQ-DICOM-014 | no_test | none | uniqueness of SOP Instance UID and Series Instance UID across two writes; UID validity beyond the validator's regex |  |
| REQ-DICOM-015 | self_referential | none: values read back with the module's reader that shares the same tag choices | tag identity for Exposure ((0018,1152) per SPEC, code writes (0018,9332)); (0018,0060),(0018,1110),(0028,0030) tag numbers; (0008,0032) acquisitionTim | `test_dicom_writer.cpp:96` WriteMetadata_Preserved; `test_parameter_dependency.cpp:180` MetadataFieldsReachTheFile |
| REQ-DICOM-016 | partial | PhotometricInterpretation read with DCMTK in Tc109 test; Rows/Columns via a non-square 32x96 round trip; the r | HighBit, PixelRepresentation, SamplesPerPixel not read by an independent reader (they are only implied because the module's strict reader would refuse | `test_parameter_dependency.cpp:140` ImageDimensionsReachTheFile; `test_dicom_reader.cpp:4314` Tc109_CurrentBehaviour_WriteDoesNotCarryWindowRescaleOrPresentationShapeFromTheSourceFile |
| REQ-DICOM-017 | partial | unwritable path literal | xpe_dicom_write_j2k IO failure (DicomWriter.cpp:94-98) has no test; leftover partial file after a failed save is not checked | `test_dicom_writer.cpp:106` WriteIOError_ReturnsIOFailed |
| REQ-DICOM-018 | asserted | NULL literals | xpe_dicom_write_j2k with meta == NULL (only img == NULL is tested) | `test_dicom_writer.cpp:119` WriteNullImg_ReturnsInvalidInput; `test_dicom_writer.cpp:125` WriteNullMeta_ReturnsInvalidInput |
| REQ-DICOM-019 | partial | round trip against the input ramp; TS .90 only implied by the reader taking the J2K decode branch | Transfer Syntax UID of the written file never compared with the literal; 'all other attributes identical to xpe_dicom_write' is not compared (no test  | `test_dicom_writer.cpp:133` WriteJ2KRoundTrip_PixelExact |
| REQ-DICOM-020 | asserted | the original input array (a 0..65535 ramp; a 12-bit ramp) compared word by word with the decoded output; test- | decoder is the same OpenJPEG that the encoder uses, so there is no second decoder of the module's own stream; only 256x256 and 64x48 images; no random | `test_dicom_writer.cpp:146` WriteJ2KRoundTrip_PixelExact; `test_dicom_reader.cpp:3380` J2kScope_AnImageWrittenWithTwelveBitsStoredIsReadBack; `test_dicom_reader.cpp:3235` J2kScope_OwnSixteenBitFileReadsAndIsDescribedAsSixteenBit |
| REQ-DICOM-021 | no_test | none | the encoder-failure path (e.g. pixel exceeding bitsStored -> {} -> XPE_ERR_PROCESSING_FAILED) and that no file results from it | `test_dicom_writer.cpp:172` WriteJ2KNullImg_ReturnsInvalidInput |
| REQ-DICOM-022 | asserted | tags re-read with DCMTK in the test | write_j2k output; the 'override via metadata path' clause (XpeImageMetadata has no rescale fields, so the clause is vacuous) | `test_dicom_reader.cpp:4308` Tc109_CurrentBehaviour_WriteDoesNotCarryWindowRescaleOrPresentationShapeFromTheSourceFile; `test_dicom_reader.cpp:4310` Tc109_CurrentBehaviour_WriteDoesNotCarryWindowRescaleOrPresentationShapeFromTheSourceFile; `test_dicom_writer.cpp:162` RescaleDefaults_OneAndZero |
| REQ-DICOM-023 | partial | files derived with DCMTK from the conformant file, one defect each | 'DX IOD conformance' is a fixed list of tags, not the IOD; only a module-written conformant file is tested for valid=true | `test_dicom_validator.cpp:98` ValidateConformant_ReturnsValid; `test_dicom_validator.cpp:111` ValidateMissingPatientID_ReportsError |
| REQ-DICOM-024 | partial | DCMTK-derived negative files | (1) preamble/magic: no direct check (relies on loadFile + meta->card()==0) and no control with a bad magic; (2) Type 1 'present and non-empty': code c | `test_dicom_validator.cpp:114` ValidateMissingPatientID_ReportsError; `test_dicom_validator.cpp:166` ValidateBadUID_ReportsWarning; `test_dicom_validator.cpp:209` ValidateStrippedTags_ReportsAllMissing |
| REQ-DICOM-025 | partial | parsed JSON of the report | the 'message' text is never asserted; warnings are only produced for invalid UIDs and those are ALSO pushed into errors (a non-critical issue is promo | `test_dicom_validator.cpp:114` ValidateMissingPatientID_ReportsError; `test_dicom_validator.cpp:99` ValidateConformant_ReturnsValid; `test_dicom_validator.cpp:166` ValidateBadUID_ReportsWarning |
| REQ-DICOM-026 | partial | garbage file | the exact report text: code emits tag "0008,0000" and message "File cannot be parsed as DICOM: <DCMTK text>", not tag "" / "Not a valid DICOM file"; w | `test_dicom_validator.cpp:123` ValidateNotDicom_ReturnsDicomInvalid; `test_dicom_validator.cpp:128` ValidateNotDicom_ReturnsDicomInvalid |
| REQ-DICOM-027 | partial | none for the number itself | required size equal to the actual report length+1; test only requires it to exceed the buffer (>10 or > sizeof(buf)); that a second call with that siz | `test_dicom_validator.cpp:141` BufferTooSmall_ReturnsBufferTooSmall; `test_dicom_validator.cpp:510` OutputBufferTooSmall_StillReportsRequiredSize |
| REQ-DICOM-028 | asserted | NULL literals |  | `test_dicom_validator.cpp:149` NullFilePath_ReturnsInvalidInput; `test_dicom_validator.cpp:154` NullOutBuf_ReturnsInvalidInput |
| REQ-DICOM-029 | partial | a real DCMTK DcmSCP in the test process receives the association and the C-STORE request (storeRequests counte | the stored dataset is discarded unseen (mock_scp.hpp: 'delete received'), so what was sent (SOP instance, pixels) and the negotiated transfer syntax a | `test_dicom_network_scu.cpp:120` CStoreSuccess_ReturnsOK; `test_dicom_network_scu.cpp:281` CStoreCalledAeInHost_NegotiatesAndStores |
| REQ-DICOM-030 | partial | mock SCP accepts any called AE (setRespondWithCalledAETitle(OFTrue)), and calling AE is only logged | calling AE 'TESTSCU' arrives at the SCP; default called AE 'ANY-SCP'; the CALLED_AE value of the 'CALLED_AE@host' form (the SCP does not require it, s | `test_dicom_network_scu.cpp:279` CStoreCalledAeInHost_NegotiatesAndStores |
| REQ-DICOM-031 | partial | connection to a port with no listener | an association attempt that actually hangs until timeoutMs; the duration is never measured; sub-second timeouts (500) become 0 s via timeoutMs/1000 | `test_dicom_network_scu.cpp:131` CStoreTimeout_ReturnsNetworkFailed |
| REQ-DICOM-032 | no_code | none | the WARNING alert with the failure reason string (no xpe_alert_push anywhere in DicomNetworkSCU.cpp); remote rejection / non-success C-STORE status ar |  |
| REQ-DICOM-033 | asserted | a real DCMTK SCP returns status 0x0000 | behaviour for non-success statuses (never produced by the mock) | `test_dicom_network_scu.cpp:120` CStoreSuccess_ReturnsOK |
| REQ-DICOM-034 | partial | mock SCP returns 3 literal worklist entries unless PatientID is a filter | query keys other than PatientID are not shown to reach the request (the mock matches PatientID only); no Scheduled Procedure Step Sequence is built, s | `test_dicom_network_scu.cpp:147` CFindResults_ReturnsJsonArray; `test_dicom_network_scu.cpp:158` CFindEmpty_ReturnsEmptyArray |
| REQ-DICOM-035 | no_code | mock matches on PatientID only | ScheduledStationAETitle (0040,0001) and ScheduledProcedureStepStartDate (0040,0002) are not implemented (header dicom_api.h:379-384 admits this); Acce | `test_dicom_network_scu.cpp:314` CFindQueryWithNameAndAccession_ReturnsJsonArray; `test_dicom_network_scu.cpp:318` CFindQueryWithNameAndAccession_ReturnsJsonArray |
| REQ-DICOM-036 | partial | mock's literal entry count | contents of each element: only the array shape and size 3 are asserted; code emits a fixed set of 5 tags (PatientID, PatientName, StudyInstanceUID, Ac | `test_dicom_network_scu.cpp:146` CFindResults_ReturnsJsonArray |
| REQ-DICOM-037 | asserted | mock returns no entry for an unknown PatientID; literal '[]' |  | `test_dicom_network_scu.cpp:160` CFindEmpty_ReturnsEmptyArray |
| REQ-DICOM-038 | partial | closed port | a C-FIND that fails after association (non-success final status, DIMSE error) and a real timeout; the code ignores the response status of the final C- | `test_dicom_network_scu.cpp:169` CFindTimeout_ReturnsNetworkFailed |
| REQ-DICOM-039 | no_test | none | everything: the only cancel-in-flight test is an unconditional GTEST_SKIP; no cancel alert exists in code; a cancel issued before the call is erased b | `test_dicom_network_scu.cpp:179` CancelCStore_TerminatesOperation |
| REQ-DICOM-040 | constant_only | none | calling cancel from a second thread while an operation runs | `test_dicom_network_scu.cpp:200` CancelNoOp_WhenIdle |
| REQ-DICOM-041 | no_test | none | export table (10 symbols, undecorated names, calling convention); no dumpbin/PE test in this module |  |
| REQ-DICOM-042 | no_test | none | any exception injection (no throw, no bad_alloc simulation in modules/dicom/tests) |  |
| REQ-DICOM-043 | no_code | none | exit logging at DEBUG; ERROR level for error conditions (most failures use spdlog::warn; only 12 spdlog::error calls exist); logging 'via xpe_common.d |  |
| REQ-DICOM-044 | partial | CRT heap-walk block counts with a 64 B/cycle positive control | success-path read, J2K write, validator, C-STORE and C-FIND have no leak test; Windows-only (skips elsewhere); thresholds allow <1 leaked block per 10 | `test_dicom_writer.cpp:265` ThousandCycles_CrtHeapDoesNotGrow; `test_dicom_reader.cpp:1269` FailurePathsDoNotGrowCrtHeap; `test_dicom_reader.cpp:1286` FailurePaths_ControlLeakIsCaught |
| REQ-DICOM-045 | partial | uncompressed original pixels vs 8 threads x 8 rounds of JPEG Lossless reads | xpe_dicom_get_metadata concurrency; native and J2K reads concurrently; the test's own comment says a pass is not evidence of thread safety | `test_dicom_reader.cpp:1045` ConcurrentOpenOfJpegLossless_NoCorruption |
| REQ-DICOM-046 | no_test | none | no test calls the SCU concurrently |  |
