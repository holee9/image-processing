# QA-B-200 M2a — C9 · C7 · C1 · D9(REQ-DISP-031) 수정 (#251)

> **정정 (M2a2, 2026-10-03):** §3 (C1 mAs)의 서술은 `m2a2_report.md` 로 대체됐다 — (0018,9332) 는 더 이상 쓰지 않고 (0018,1152)+(0018,1153) 을 쓰며 읽기 순서는 1153 → 9332 → 1152 이다. 아래 §3 와 §6 의 C1 관련 문장은 M2a 커밋(f876e420) 시점의 기록이다.

카드: `.moai/lanes/post/inbox/QA-B-200.md` 의 리더 결정(M1 수신 후). 영상 출력이 바뀌지 않는 네 건이다. E1(M2b)은 이 커밋에 없다.

## 0. 결과 요약

| 후보 | 고친 것 | 새·바뀐 시험 | 반증 |
|---|---|---|---|
| C9 C-FIND 실패 상태 | 최종 응답의 상태가 `0x0000` 이 아니면 `XPE_ERR_NETWORK_FAILED` | `DicomFailurePaths` 2 | 터짐 |
| C7 C-STORE 실패·취소 | 실패 모든 지점에 WARNING 알림(이유 포함), 취소는 `XPE_ERR_PROCESSING_FAILED` + "cancelled" 알림 | `DicomFailurePaths` 5, `DicomNetworkTest.CancelCStore_TerminatesOperation`(되살림) | 터짐 |
| C1 mAs 태그 | 읽기: (0018,9332) 있으면 그것, 없으면 (0018,1152). 쓰기: 둘 다 | `DicomExposureTag` 4 | 터짐 |
| D9 REQ-DISP-031 | 표시 함수 5개가 진입·종료를 DEBUG, 오류를 ERROR 로 xpe_common 로거에 기록 | `DisplayLogging` 4 | 터짐 |

검증(관측): ci-post 의 CI 와 같은 `-E "Performance|Within…"` 필터 ctest 1354개 통과·0 실패, ci-dicom 313개 통과·0 실패(기존 302 + 새 11)(`m2a_ctest_summary.txt`). 반증 11팔 모두 터졌고 원본은 팔마다 바이트 단위로 복원됨(`m2a_arms_out.txt`). doxygen 경고 0건, `check_header_docs.py` 20개 헤더 0건. 로컬 성능 예산 시험(CI 가 뺌)은 그대로 통과: 모달리티 선형 11 ms(예산 20), VOI 18 ms(16 — 시험이 자기 측정으로 단언), 프레젠테이션 29 ms(30)(`m2a_display_perf_run.txt`).

## 1. C9 — C-FIND 실패 상태

**바뀐 것.** `DicomNetworkSCU::cfindMwl` 이 `sendFINDRequest` 가 성공한 뒤 마지막 응답(`QRResponse::m_status`)을 본다. `0x0000` 이 아니면(실패 `0xA7xx`·`0xA9xx`·`0xCxxx`, 상대가 스스로 보낸 취소 `0xFE00`, 또는 최종 응답이 아예 없음) 응답을 지우고 알림을 올리고 `XPE_ERR_NETWORK_FAILED` 를 돌려준다. 출력 버퍼는 건드리지 않는다. "일치 없음"(최종 상태 성공, 대기 항목 없음)은 그대로 `XPE_OK` + `[]` 이다.
**오류 코드 매핑.** 새 코드는 필요 없었다. 기존 `XPE_ERR_NETWORK_FAILED` 가 REQ-DICOM-038 의 문구("C-FIND 가 실패하면 `XPE_ERR_NETWORK_FAILED`")와 같다. common 레인 변경 없음.
**시험.** 상태 `A700`·`A900`·`C001`·`FE00` 각각이 `XPE_ERR_NETWORK_FAILED` 이고(출력 버퍼에 쓰지 않고 알림 "C-FIND failed" 가 있음), 같은 상대를 강제 없이 쓰면 다시 정상. 대조군: 강제 없는 상대는 항목 3개, 일치 없는 질의는 `[]`+OK.
**반증.** 최종 상태 검사를 끈 팔: 이 시험이 빨강.

## 2. C7 — C-STORE 실패와 취소

### 알림 문구 (교차 레인 계약 확인)
GUI 가 이 문구를 거르는지 `clients/` 를 검색했다: `xpe_dicom_cstore` 를 가리키는 곳은 `XpeDicomWrapper.cs`(P/Invoke 선언)와 `XpeDicomReadinessProbe.cs`(심볼 존재 확인)뿐이고, 알림 텍스트를 이 문구로 매칭하는 코드는 없다. 따라서 문구 변경은 현재 깨뜨릴 소비자가 없다. 문구(모두 WARNING):

| 상황 | 문구 |
|---|---|
| 네트워크 시작 실패 | `DICOM C-STORE failed: the network could not be started (<DCMTK 문구>)` |
| 연결 실패 | `DICOM C-STORE failed: no association with the peer (<DCMTK 문구>)` |
| 전송 미완료 | `DICOM C-STORE failed: the transfer did not complete (<DCMTK 문구>)` |
| 상대가 거절 | `DICOM C-STORE failed: the peer rejected the data set with status 0xA700` |
| 취소: 연결 전 | `DICOM C-STORE cancelled by the caller before it connected; nothing was sent` |
| 취소: 협상 뒤 | `DICOM C-STORE cancelled by the caller before the transfer started; nothing was sent` |
| 취소: 전송 중 | `DICOM C-STORE cancelled by the caller during the transfer; the transfer could not be interrupted, so the peer may have received the data` |
| C-FIND 최종 실패 | `DICOM C-FIND failed: the peer ended the query with status 0xA700` (또는 `the peer sent no final response`) |
| C-FIND 취소 | `DICOM C-FIND cancelled by the caller …`(연결 전·질의 전·질의 중 세 문구) |

"cancelled" 는 취소 문구에만 있고 실패 문구에는 없다. 취소와 실패를 로그 읽는 사람도 가를 수 있다.

### 바뀐 것과 한계
- 실패의 모든 지점(네트워크 시작, 연결, 전송, 상대 거절)이 WARNING 알림을 올린다(REQ-DICOM-032). 반환 코드는 그대로 `XPE_ERR_NETWORK_FAILED`.
- 취소는 세 번 확인한다: 연결 전, 협상 뒤, 그리고 **전송이 돌아온 뒤**(새로 더한 것). 전송 중 도착한 취소는 DCMTK 의 블로킹 호출을 다른 스레드에서 끊을 방법이 없어 전송이 끝난 뒤에야 확인되고, 그때 `XPE_ERR_PROCESSING_FAILED` 와 위의 "may have received" 알림을 돌려준다(성공으로 보고하지 않는다). **전송을 실제로 중단하지는 못한다.** REQ-DICOM-039 가 "취소가 신호된다"를 요구하는 정도까지는 맞지만 "즉시 중단한다"는 아니다.
- 취소 플래그는 호출 진입에서 지워지므로 진행 중인 작업이 없을 때의 취소는 다음 호출에 영향이 없다(시험으로 못 박음).

### 시험
`DicomFailurePaths`: 실패 알림(대조군: 성공은 알림 0건, 알림은 WARNING 이고 "C-STORE failed"·"0xA700" 포함·"cancel" 미포함), 아무도 서비스하지 않는 포트, 전송 중 취소(상대가 2초 지연, 700 ms 에 취소 → `XPE_ERR_PROCESSING_FAILED`, "cancelled"·"may have received"), 질의 중 취소, 진행 중이 아닌 취소의 무해성. 기존 `DicomNetworkTest.CancelCStore_TerminatesOperation` 은 "로컬 SCP 가 너무 빨라 관측할 수 없다"는 이유로 무조건 건너뛰었는데, 모의 상대를 느리게 만들 수 있게 되어(`MockScp::storeDelayMs`) 실제 시험으로 되살렸다(1508 ms, 통과). M1 재현의 `ms < 2000`("취소가 전송을 즉시 끊는다") 단언은 모듈이 줄 수 없는 약속이라 정식 시험에 넣지 않았다.
**반증.** 거절 알림 삭제 팔 → 실패 시험 빨강. 전송 뒤 취소 확인을 끈 팔 → `ACancelDuringATransfer…` 와 되살린 `CancelCStore_TerminatesOperation` 둘 다 빨강. C-FIND 의 취소 확인을 끈 팔 → `ACancelDuringAQuery…` 빨강.

## 3. C1 — mAs 태그

**바뀐 것.** 읽기: (0018,9332) 가 숫자로 읽히면 그것, 아니면 (0018,1152)(숫자가 아니거나 음수거나 비어 있으면 mAs 는 기본값 0). 쓰기: (0018,9332) 에 정확값(FD)을 쓰고, **(0018,1152) 에 반올림한 정수(IS)를 함께** 쓴다.
**반올림 규칙.** 가장 가까운 정수, 0.5 는 올림(2.5 → 3, 2.4 → 2, 0.5 → 1, 0.4 → 0). IS 가 담을 수 있는 범위(2³¹−1)를 넘는 값은 (0018,1152) 를 쓰지 않고 (0018,9332) 만 쓴다. 모듈 자신의 왕복은 정확값을 돌려준다(9332 우선).
**DX IOD 에서 둘 다 써도 되는가(속성 Type), 한 줄 확인.** PS3.3 Table C.8-33 X-Ray Acquisition Dose 모듈(사용: U)에 (0018,1152) Exposure 가 Type 3(Optional), VR IS 로 있다. 같은 모듈의 속성 목록을 검색했을 때 **(0018,9332) 는 나오지 않았다**(`standards_cited.txt`). DX 영상 IOD 전체에서 (0018,9332) 가 다른 모듈에 있는지는 확인하지 못했다. 따라서 (0018,9332) 를 DX 영상의 최상위 속성으로 쓰는 것은 IOD 에 정의되지 않은 속성일 가능성이 있고, 엄격한 IOD 검증기가 지적할 수 있다. 이 모듈은 변경 전부터 (0018,9332) 를 썼고 리더가 "둘 다 쓴다"고 정했기 때문에 지우지 않았다. **리더 판단 사항**: 9332 를 계속 쓸지(정확값 보존) 1152 만 쓸지(소수 mAs 정밀도 손실).
**시험.**
- 외부 파일(DCMTK 로 직접 만들고 9332 를 지우고 1152 = "100")이 100 으로 읽힘(전에는 0). 대조군: 모듈이 쓴 파일의 왕복.
- 둘 다 있고 값이 다르면 9332 가 우선.
- 쓰기 표: 100→"100", 2.5→"3", 2.4→"2", 0.4→"0", 0.5→"1", 3.0e9→(1152 없음); 9332 는 모두 정확값, 1152 의 VR 은 IS.
- 1152 가 `abc`·`-7`·빈 문자열이면 mAs 는 0.
**반증.** 1152 대체 읽기 삭제, 1152 쓰기 삭제, 반올림을 내림으로, 9332 쓰기 삭제 — 네 팔 모두 터졌다. 9332 쓰기 삭제 팔은 기존 시험 `GetMetadata_FieldsPopulated`·`WriteMetadata_Preserved` 도 빨갛게 만든다(복원되지 않은 변형 바이너리를 우연히 돌려서 확인한 사실이며, 복원 뒤 313개 모두 통과).

## 4. D9 — REQ-DISP-031 로깅

**바뀐 것.** 공개 함수 5개(`xpe_apply_modality_lut`, `xpe_apply_voi_lut`, `xpe_voi_preset_create`, `xpe_apply_presentation_lut`, `xpe_gsdf_calibrate`)의 본문을 `*_impl` 로 옮기고, 바깥 함수가 진입을 DEBUG 로, 종료를 기록한다: 성공이면 DEBUG `exit OK`, 아니면 ERROR `<함수> failed: code <n> (<xpe_error_string>)`. 반환 코드는 바꾸지 않는다. 도우미(`xpe_display_log_enter/exit`)는 `display_helpers.cpp` 에 있고 spdlog 가 던져도 삼킨다(로깅 실패가 결과를 바꾸면 안 되므로). 이미 링크된 spdlog 의 기본 로거를 쓰므로 `xpe_log_set_level`·`xpe_log_set_file` 이 그대로 적용된다.
**수준.** xpe_common 의 기본은 INFO 다. 진입·종료는 DEBUG(INFO 에서는 보이지 않음), 오류는 ERROR(INFO 에서 보임)이므로 기본 설정에서 보이는 것은 오류 줄뿐이다. `xpe_display_version` 은 결과가 없는 상수 반환이라 기록하지 않았다.
**시험(`test_display_logging.cpp`).** DEBUG 수준에서 함수마다 진입 2줄(호출 2회)·종료 1줄(성공한 호출)·ERROR 1줄(실패한 호출), 오류 줄이 코드 이름("Invalid input parameter")을 포함. INFO 에서는 함수마다 ERROR 1줄이고 `[debug]` 0줄. ERROR 수준에서는 ERROR 5줄만. 로거를 DEBUG 와 OFF 로 두고 같은 호출을 해도 반환 코드가 같다. 대조군: 같은 로거에 쓴 줄이 파일에 도착한다.
**반증.** 오류를 DEBUG 로 쓰는 팔 → INFO·ERROR 수준 시험과 DEBUG 시험이 빨강. 한 함수의 진입 기록 삭제 → DEBUG 시험 빨강. 종료 도우미가 항상 `XPE_OK` 를 돌려주는 팔 → 반환 코드 시험 빨강.
**M1 의 D5(`gsdfEnabled`)와 REQ-DISP-030 은 결함이 아니어서 이 커밋에서 건드리지 않았다.** `test_repro_qa_b_200.cpp` 에는 D5 기록용 비활성 시험만 남겼다.

## 5. 이 커밋의 시험 보조 변경
- `tests/mock_scp.hpp`: `forcedFindStatus`·`forcedStoreStatus`·`storeDelayMs`·`findDelayMs`(기본 0 = 기존 동작). 기존 `test_dicom_network_scu` 가 ci-dicom ctest 안에서 통과.
- `test_repro_qa_b_200.cpp`(dicom)를 `test_dicom_failure_paths.cpp` 로 이름 변경(결함 재현이 아니라 실패·취소 경로의 회귀 시험이 되었으므로), CMake 실행 파일도 `test_dicom_failure_paths`.

## 6. Gap / 잔여 위험

Gap(관측하지 않은 것)
- 전송 중 취소는 **중단하지 못한다**(위 §2). 보고만 한다. 실제 PACS 에서의 지연 형태(연결 단계, 데이터 전송 단계)는 구분해 시험하지 않았다.
- (0018,9332) 가 DX IOD 전체에서 허용되는지(다른 모듈에 있는지)는 확인하지 못했다. 엄격한 검증기(dciodvfy 등)로 이 모듈이 쓴 파일을 돌리지 않았다.
- 표시 모듈의 로그 줄이 GUI·기존 시험의 로그 기대와 충돌하는지(예: 오류 입력을 일부러 주는 시험이 로그가 비어 있기를 기대하는지)는 `clients/` 와 ci-post 의 시험 범위에서만 확인했다. ci-post 1354개는 통과.
- `REQ-DISP-029` → `REQ-DISP-036` 재번호(리더 추가 지시, 코드·주석 11곳)는 이 커밋에 없다. 번호가 바뀐 spec 이 main 에만 있어(내 브랜치에 아직 병합되지 않음) 인용 점검(`check_req_citations.py`)을 내 작업 트리에서 의미 있게 돌릴 수 없다. main 병합 뒤 별도의 주석 전용 커밋으로 한다.
- 모든 새 시험은 로컬 한 번의 실행이다. 취소 시험은 시간에 의존한다(2 초 지연 대 700 ms 취소).

잔여 위험
- 호출자가 취소를 보내도 전송은 끝까지 간다. 상대에 데이터가 저장될 수 있으나 호출은 "취소됨"을 돌려준다(알림에 명시).
- 표시 함수마다 DEBUG 두 줄 호출이 늘었다. 기본 INFO 에서는 수준 비교뿐이며 로컬 성능 시험은 통과했다(§0).
- 사용자 환경에서 로그 수준을 DEBUG 로 두면 표시 함수 호출마다 두 줄씩 쌓인다(요구가 정한 대로).
