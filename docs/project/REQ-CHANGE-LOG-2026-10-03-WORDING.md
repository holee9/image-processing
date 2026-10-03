# 요구 문구 변경 이력 — 2026-10-03 문구 결정 반영

**작성**: 2026-10-03, xpe-leader · **범위**: SPEC-XPE-P0, SPEC-XPE-P1A, SRS-CALIB-001, PRD-FPD-CAL-001(REQ-OFF), SPEC-XPE-P1B-DISP, SPEC-XPE-P1B-ENH, SPEC-XPE-GUI-IT
**목적**: 2026-10-03 사용자가 승인한 요구 문구 결정을 요구 ID 별로 한곳에 모은다. 옛 문구와 새 문구, 바꾼 이유, 승인 근거를 함께 적는다.

**승인 근거**: 이슈 #245 의 마지막 코멘트 "요구 문구 결정 — 사용자 승인 기록 (2026-10-03)". 결정 번호(1~14)는 pre 레인 `QA-A-233` 보고서(워크트리 `xpe-pre` `db571b9e`, `.moai/reports/lane-pre/QA-A-233/report.md`)의 결정 표 번호다. 사용자는 네 묶음으로 답했다.

| 묶음 | 결정 번호 | 사용자 선택 |
|---|---|---|
| ① 이름·번호·개수, 시험으로 굳은 동작 | 1~5 | 문서를 실제에 맞게 |
| ② CI·의존성·함수 모양 | 6 | CI 에 실제로 도입 |
| ② (같은 묶음) | 7·8·9 | 권장안 — 003 문서 정정, 033 은 dicom 버전 함수 추가(post), 8·9 문서 정정 |
| ③ 없는 기능을 약속하는 문구 | 10~14 | "미구현(요구 유지)" 표시 |
| ④ post·gui 문구 | GSDF 플래그, 부정 입력 "20+", ENH·DISP 함수 개수 | 문서를 실제에 맞게 |

**원칙**: 요구는 하나도 지우지 않았다. 묶음 ③ 은 문구를 바꾸지 않고 상태 표시만 달았으므로 §3 에 따로 적는다.

**반영 커밋**: 결정 1~5 는 `209be228`(SPEC-XPE-P0 v1.3.0)에서 대부분 반영됐다. 다만 REQ-P0-023(결정 1)은 그 커밋에서 "결정 대기" 메모만 달렸고 문구는 그대로였으므로 이번 편집(SPEC-XPE-P0 v1.3.1)에서 고쳤다. REQ-P1A-018·013(결정 8·9)의 문구 정정은 그보다 앞선 `18dab3fc`(SPEC-XPE-P1A v1.3.5)에서 이미 들어갔고, 이번 편집은 승인 기록과 실제 선언 인용만 더했다. 나머지는 이번 편집(커밋 전)이다.

---

## 1. 문구가 바뀐 요구 — `209be228` (SPEC-XPE-P0 v1.3.0, 결정 1~5·7)

| 요구 ID | 옛 문구 (줄임) | 새 문구 (줄임) | 이유 | 승인 |
|---|---|---|---|---|
| REQ-P0-003 | "minimum dependencies: spdlog, nlohmann-json, fmt, opencv4, eigen3, dcmtk, gtest" | "spdlog, nlohmann-json, fmt, gtest, dcmtk, openjpeg. Eigen3 is resolved by the root CMakeLists.txt (`find_package`, falling back to `FetchContent`); OpenCV is not used." | `vcpkg.json` 에 opencv4·eigen3 가 없다. OpenCV 는 어느 빌드 파일에도 없다 (결정 7) | 사용자 2026-10-03 (#245 코멘트) |
| REQ-P0-009 | "Enum types: XpePixelFormat, XpeAlertSeverity, XpeErrorCode." | "Enum types: XpePixelFormat, XpeAlertSeverity. Error codes: `XpeErrorCode` is an `int32_t` alias …" | `xpe_error.h` 는 `typedef int32_t` 와 `#define` 상수로 정의한다 (결정 2) | 사용자 2026-10-03 (#245 코멘트) |
| REQ-P0-011 | "… return XPE_ERR_OK on success." | "… return XPE_OK on success." | `XPE_ERR_OK` 는 없다 (결정 2) | 사용자 2026-10-03 (#245 코멘트) |
| REQ-P0-012 | "… flush logs, and return XPE_ERR_OK. Calling shutdown without init SHALL return XPE_ERR_NOT_INITIALIZED." | "… flush logs. It returns `void` and SHALL be safe to call without a prior `xpe_init` (no crash)." | 선언이 `void xpe_shutdown(void)` 다 (결정 2) | 사용자 2026-10-03 (#245 코멘트) |
| REQ-P0-014 | "Invalid JSON SHALL return XPE_ERR_INVALID_PARAM." | "Invalid JSON SHALL return XPE_ERR_CONFIG_INVALID." | `XPE_ERR_INVALID_PARAM` 은 없다. 시험 `ConfigureWithInvalidJsonReturnsInvalid` 가 `XPE_ERR_CONFIG_INVALID` 를 단언한다 (결정 2) | 사용자 2026-10-03 (#245 코멘트) |
| REQ-P0-016 | "Double-free SHALL return XPE_ERR_INVALID_PARAM without crashing." | "Calling it again on an already-freed buffer (`data == NULL`) SHALL return XPE_OK without crashing." | 코드가 `XPE_OK` 를 돌려주고 시험 `FreeImageNullDataDoesNotCrash` 가 고정한다. 이중 해제 버그는 잡지 못한다 (결정 4) | 사용자 2026-10-03 (#245 코멘트) |
| REQ-P0-019 | "… the number of unread alerts in the alert queue." | "… the number of alerts in the alert queue. Reading an alert does not reduce the count." | 큐는 읽어도 줄지 않는다 (결정 5) | 사용자 2026-10-03 (#245 코멘트) |
| REQ-P0-020 | "… copy the oldest unread alert … and remove it from the queue." | "… copy the alert at the given index … The queue is not modified; `xpe_clear_alerts` empties it." | 선언이 `int32_t index` 를 받고 큐를 바꾸지 않는다. 시험 `AlertQueueFifoOrder` 가 인덱스 0·1 로 읽는다 (결정 5) | 사용자 2026-10-03 (#245 코멘트) |
| REQ-P0-022 | "… valid parameter ranges for the specified parameter ID." | "… the valid range and default value for the parameter identified by `(bodyPart, paramName)`." | 시그니처가 `(bodyPart, paramName, …)` 다 (결정 5) | 사용자 2026-10-03 (#245 코멘트) |
| REQ-P0-024 | "… SHALL return XPE_ERR_FILE_IO and retain the previous output destination." | "… SHALL return XPE_ERR_IO_FAILED and retain the previous output destination." | `XPE_ERR_FILE_IO` 는 없다 (결정 2) | 사용자 2026-10-03 (#245 코멘트) |
| REQ-P0-030 | "… declare all 15 xpe_common.dll functions …" | "… declare all 16 xpe_common.dll functions …" | 새로 만든 DLL 의 수출 표가 16개(`QA-A-231`) (결정 3) | 사용자 2026-10-03 (#245 코멘트) |
| REQ-P0-032 | "Each of the 8 module directories (…7개 나열…)" | "Each of the 7 module directories other than modules/common (…)" | 8 이라 적고 7개를 나열했다. 여덟 번째 DLL 은 §2.3 의 xpe_common (결정 3) | 사용자 2026-10-03 (#245 코멘트) |

같은 커밋은 SPEC-XPE-P0 §1·§4·§8 의 함수 수(15 → 16)와 산출물 수(12 → 11)도 고쳤다. 요구 문장이 아니라 이 표에서는 뺐다.

## 2. 문구가 바뀐 요구 — 이번 편집 (2026-10-03, 커밋 전)

| 요구 ID | 문서 | 옛 문구 (줄임) | 새 문구 (줄임) | 이유 | 승인 |
|---|---|---|---|---|---|
| REQ-P0-023 | SPEC-XPE-P0 v1.3.1 | "(TRACE=0, DEBUG=1, INFO=2, WARN=3, ERROR=4, CRITICAL=5)" | "(TRACE=0, DEBUG=1, INFO=2, WARN=3, ERROR=4, OFF=5) … level 5 (OFF) discards every message." | SRS-FUNC-040, 헤더 `xpe_common_api.h`(`5=OFF`), `api-spec.md`, 코드(`QA-A-232` M1)가 모두 OFF 다 (결정 1) | 사용자 2026-10-03 (#245 코멘트) |
| REQ-P1A-018 | SPEC-XPE-P1A v1.3.6 | (`18dab3fc` 이전) `(filePath, expiryEpochMsOut)` · "return `XPE_ERR_CALIBRATION_EXPIRED` if the timestamp is in the past" | `xpe_calib_check_expiry(filepath, is_expired, remaining_days)` … report through `is_expired`·`remaining_days`, return `XPE_OK`. 이번 편집은 실제 선언 인용과 승인 기록을 더함 | 만료는 검사 결과이지 호출 실패가 아니다. C# 호출처 세 곳과 시험이 이 모양에 의존한다 (결정 8) | 사용자 2026-10-03 (#245 코멘트) |
| REQ-P1A-013 | SPEC-XPE-P1A v1.3.6 | (`18dab3fc` 이전) `xpe_defect_detect_runtime(img, defectMapOut, configJsonOrNull)` | `xpe_defect_detect_runtime(image, metadata, defect_map_output)` — 설정 인자 없음, 임계는 프레임 통계에서. 이번 편집은 실제 선언 인용과 승인 기록을 더함 | 설정을 바깥에서 주는 요구가 없다(코드 주석 QA-A-164), `metadata` 는 읽지 않는다(`QA-A-232` M2) (결정 9) | 사용자 2026-10-03 (#245 코멘트) |
| REQ-DISP-024 | SPEC-XPE-P1B-DISP v1.2.0 | "WHEN `gsdfEnabled` is non-zero in the params, the system SHALL apply the GSDF-calibrated LUT entries …" | "The `gsdfEnabled` field … SHALL record whether the LUT entries were generated by `xpe_gsdf_calibrate` … `xpe_apply_presentation_lut` SHALL apply the LUT entries held in the params regardless of the flag value …" | 플래그는 `xpe_gsdf_calibrate` 가 쓰기만 하고 `xpe_apply_presentation_lut` 은 읽지 않는다 (`presentation_lut.cpp`). QA-B-199 후보 D5 | 사용자 2026-10-03 (#245 코멘트, 묶음 ④) |
| REQ-DISP-036 | SPEC-XPE-P1B-DISP v1.2.0 | "All 5 exported functions SHALL use C linkage …" | "All 6 exported functions SHALL use C linkage …" | 헤더 `display_api.h` 의 `XPE_API` 선언 6개(`xpe_display_version` 포함). QA-B-199 후보 D7. §2.3 "exactly 5 functions" → 6, acceptance·plan M5-03 도 같이 | 사용자 2026-10-03 (#245 코멘트, 묶음 ④) |
| REQ-ENH-CC-001 | SPEC-XPE-P1B-ENH v1.3.0 | "The system SHALL export all 7 API functions …" | "The system SHALL export all 10 API functions …" | 헤더 `enhance_basic_api.h` 의 `XPE_API` 선언 10개(처리 7 + 버전·스레드 3). 머리말 API Count 도 7 → 10 | 사용자 2026-10-03 (#245 코멘트, 묶음 ④) |
| AC-9 (REQ-GUI-IT-050·052·006) | SPEC-XPE-GUI-IT v1.3.1 | "… 등 > 20개 negative 시나리오에서 managed exception 없음" | "… 등 서로 다른 거부 경로 18개(GUI-C-209 M2) 각각에서 managed exception 없음" | `GUI-C-209` 결론: xpe_common 의 서로 다른 거부 경로는 18개이고 "20+" 는 한 검사를 두 번 세어야 채워진다. 시험은 `NegativeInputPathTests`(main `b39b34a9`) | 사용자 2026-10-03 (#245 코멘트, 묶음 ④) |

함수 수는 이 체크아웃의 헤더에서 `XPE_API` 선언을 세어 얻었다. 새로 만든 DLL 의 수출 표는 이번에 세지 않았다.

## 3. 문구를 바꾸지 않고 상태만 표시한 요구 — 이번 편집

아래 요구는 문장을 그대로 두고 "미구현(요구 유지) — 사용자 결정 2026-10-03, #245" 표시를 달았다. RTM 에서는 ✗ 로 적었다. 구현 여부는 기능별로 나중에 정한다.

| 요구 ID | 문서 | 상태 | 결정 |
|---|---|---|---|
| REQ-P1A-015 (kVp 보간 표) | SPEC-XPE-P1A v1.3.6 | 게인 적재는 구현, kVp 보간 표는 없음 | 10 |
| PRE-02 (온도별 오프셋 맵·PREP 모델) | SPEC-XPE-P1A v1.3.6 §1.2 | 오프셋 맵 한 장만 씀. 온도 보상 자체(`xpe_temp_compensate`, REQ-P1A-080)는 구현됨 | 11 |
| REQ-OFF-003·004·005 | PRD-FPD-CAL-001 §5.1.6 | 온도별 측정·선택·보간, PREP-time dark map 없음 | 11 |
| SRS-CALIB-FUNC-011 (`xpe_calib_session_create`) | SRS-CALIB-001 v1.3, RTM-CALIB-001 v1.5 | 함수 없음. 파일 간 `session_id` 일치 검사(`QA-A-229` M4)는 별개 | 12 |
| REQ-P1A-042 (부위별 한계) | SPEC-XPE-P1A v1.3.6 | `xpe_preprocess_get_param_range(param_name, min, max)` — 부위 인자 없음 | 13 |
| SRS-CALIB-FUNC-014 (프레임 링 버퍼) | SRS-CALIB-001 v1.3, RTM-CALIB-001 v1.5 (△ → ✗), CALIB-VER-001 표 | 고스트는 화소별 누산기 둘, 링 버퍼 없음 | 14 |
| SRS-CALIB-FUNC-015 (`xpe-pre-e2e-report-v1`) | 같은 문서들 | 구현 없음 | 14 |
| SRS-CALIB-FUNC-021 (CES) | 같은 문서들 | 구현 없음 | 14 |
| SRS-CALIB-FUNC-028 (`xpe_calib_field_generate`) | SRS-CALIB-001 v1.3, RTM-CALIB-001 v1.5 | 구현 없음 | 14 |
| SRS-CALIB-FUNC-029 (`xpe_calib_check_drift`) | 같은 문서들 | 구현 없음 | 14 |
| SRS-CALIB-FUNC-030 (실시간 오프셋 적응) | 같은 문서들 | 구현 없음 | 11·14 |

## 4. 문구를 바꾸지 않고 진행 계획만 적은 요구 — 이번 편집

| 요구 ID | 문서 | 기록 | 결정 |
|---|---|---|---|
| REQ-P0-006 (커버리지 85%) 및 §3.2·§3.4·§3.5 의 cppcheck·clang-tidy·MISRA·ASan | SPEC-XPE-P0 v1.3.1 | "planned — not run by any CI workflow" → "CI 도입 예정 — pre QA-A-234, 사용자 결정 2026-10-03". 문서를 낮추지 않고 CI 를 요구에 맞춘다 | 6 |
| REQ-P0-033 (모듈 버전 함수) | SPEC-XPE-P0 v1.3.1 | "dicom 버전 함수 추가 예정 — post QA-B-200 M2a, 사용자 결정 2026-10-03". 요구는 그대로 | 7 |

---

## 5. 그 뒤 반영 — 결정의 후속 (2026-10-03, post 병합 `79c83e20`)

| 요구 ID | 문서 | 옛 문구 → 새 문구 | 근거 |
|---|---|---|---|
| REQ-ENH-021 (USM 오버슈트) | SPEC-XPE-P1B-ENH v1.3.1 | 상한 `max(original*2.0, original+amount*threshold)` 만 → 같은 상한 + "No output pixel SHALL be below 0, whether or not the pixel was sharpened and including `amount = 0`" | 2026-10-03 사용자 답변 "0에서 자르기 (권장)"(리더 세션 AskUserQuestion, #251). 구현 QA-B-201 M3·M3b·QA-B-205 |
| REQ-DICOM-041 (ABI) | SPEC-XPE-P1B-DICOM | "All 10 exported functions" → "All 11 exported functions" | 결정 7 의 결과(`xpe_dicom_version` 추가, QA-B-200 M2a). api-spec §11 인벤토리 10 → 11, §11.11 추가 |
| (문구 아님) api-spec §11.5·11.6 | api-spec | DICOM 쓰기는 `XPE_PIXEL_UINT16` 만 받고 다른 형식은 입구에서 `XPE_ERR_INVALID_INPUT` | 구현 사실 기록(QA-B-201 M4, Codex #107). 리더 결정: 변환하지 않고 거부 |

---

## 6. QA-B-204 결과에 대한 사용자 결정 — 문서를 실제에 맞게 (2026-10-03)

근거: 2026-10-03 사용자 답변(리더 세션 AskUserQuestion), #251 코멘트 "QA-B-204 판정에 대한 사용자 결정". 재현: post `QA-B-204` 보고서(dev/postprocess `9a9247b0`).

| 요구 ID | 문서 | 옛 문구 → 새 문구 | 재현 |
|---|---|---|---|
| REQ-ENH-CC-002 | SPEC-XPE-P1B-ENH v1.3.2 | 비 FLOAT32 → `INVALID_INPUT` → `UNSUPPORTED_FORMAT` (NULL 은 그대로 `INVALID_INPUT`) | E3: 7개 함수 모두 UINT16 에 −7 |
| REQ-DICOM-009 의 Acquisition Time 매핑 | SPEC-XPE-P1B-DICOM v1.3.0 | epoch ms → epoch 초(UTC, 0 = 모름) | C2: 초로 쓰고 읽음, ms 값은 0 으로 읽힘 |
| REQ-DICOM-026 | SPEC-XPE-P1B-DICOM v1.3.0 | `tag ""`, `"Not a valid DICOM file"` → `tag "0008,0000"`, 메시지 접두 `"File cannot be parsed as DICOM: "` | C4 |
| REQ-DISP-021 | SPEC-XPE-P1B-DISP v1.3.0 | "[0,1] 밖은 클램프" → 유한한 값은 클램프, NaN·±inf 는 `INVALID_INPUT`(버퍼 불변) | D4 |
| REQ-DISP-029 주석 | SPEC-XPE-P1B-DISP v1.3.0 | "NaN 광도는 가드를 통과" 서술 삭제 → 거부됨을 기록 | D6: 7경우 모두 거부 |

같은 결정 묶음에서 **코드를 요구에 맞추기로 한 것**(C3 요구대로 거부, E6 표준 CLAHE, D1·D3·D8 입구 거부)은 post `QA-B-207` 로 구현한다. D1·D3·D8 의 거부 조건과 E6 의 보간 명시는 그 병합 때 요구 문구에 더한다.

---

## 7. 표준과 다른 요구 정정 — DICOM 검증의 환자 속성 (2026-10-03)

근거: Codex #111(QA-B-206 M2 검토)이 찾음, 사용자 답변 "표준대로 허용"(리더 세션 AskUserQuestion), #251 코멘트 "사용자 결정 추가 — DICOM 검증의 Patient Name·Patient ID".

| 요구 ID | 문서 | 옛 문구 → 새 문구 | 근거 |
|---|---|---|---|
| REQ-DICOM-024 | SPEC-XPE-P1B-DICOM v1.3.1 | Type 1 목록(값 필수)에 Patient Name·Patient ID 포함 → 두 항목을 "Type 2: 존재 필수, 빈 값 적합" 줄로 분리 | DICOM PS3.3 Table C.7-1 이 두 속성을 Type 2 로 규정. 옛 문구대로면 표준에 맞는 익명화 DX 파일이 `DICOM_INVALID`. 구현 post QA-B-206 M2b, 나머지 9개 항목의 표준 Type 대조표는 그 보고서 §1 |
| REQ-DICOM-024 (Pixel Data) | SPEC-XPE-P1B-DICOM v1.3.1 | Type 1 목록의 Pixel Data → "Provider URL 이 없을 때 필수(Type 1C)". URL 은 JPIP 참조 전송 구문에서만 화소를 대신하고(그때 비치명 경고), 다른 구문의 URL, JPIP 구문의 Pixel Data, Pixel Data·URL 동시 존재는 오류(PS3.5 §8.2·A.6, Codex #114·#115 로 정정). JPIP 참조 집합에 HTJ2K 참조 `.204`·`.205`(PS3.5 A.11·A.12, Codex #117·#119) 포함 — PS3.3 C.7.6.3 은 `.94/.95` 만 나열하나 PS3.5 를 따름. `.95`·`.205`(Deflate)는 이 빌드에서 읽지 못해 미검증 | Image Pixel Module 의 Type 1C. Codex #113 이 찾음. 같은 요구 줄의 표준 정렬이라 위 사용자 결정("표준대로 허용")을 적용 — 리더 판단, 사용자가 다르게 원하면 되돌린다. 구현 QA-B-206 M2c |
| (설계 예제) XPE-ALG-001 §17.3.1 | XPE-ALG-001 | Type 1 목록에서 PatientID·StudyDate 제거(둘 다 Type 2), PixelData 를 1C 로 | 같은 근거. Codex #113 발견 2 |

---

## 8. SPEC-XPE-GUI-IT 실태 보고에 대한 사용자 결정 (2026-10-03)

근거: gui `GUI-C-224` 보고서(dev/gui `940ff212`, `.moai/reports/lane-gui/GUI-C-224/report.md`), 사용자 답변(리더 세션 AskUserQuestion), #249 코멘트.

| 요구 ID | 문서 | 옛 문구 → 새 문구 | 사용자 선택 |
|---|---|---|---|
| REQ-GUI-IT-008 | SPEC-XPE-GUI-IT v1.3.2 | 허용 폴더 `<repo>/build/`·시험 출력 → 로케이터 후보 다섯(`XPE_NATIVE_DIR`·`build/**`·`modules/common/build_test`·시험 출력·`clients/ImageProcTest/bin`) | 요구를 코드 후보에 맞춤 |
| REQ-GUI-IT-050 | SPEC-XPE-GUI-IT v1.3.2 | AV·SEH 를 관측해 기록 → 모든 시험이 호스트 생존 상태로 끝까지 돌고, SEHException 은 기록·실패. AV 는 .NET 이 잡지 못해 호스트 종료 = 실행 실패로 검출 | 문구를 가능한 형태로 |
| REQ-GUI-IT-063·064·065 | SPEC-XPE-GUI-IT v1.3.2 | 문구 불변, 상태를 "미구현, 계획 없음" → "보류(소비자·러너 없음, 생기면 재개)" | 보류로 표시 |

---

*끝*
