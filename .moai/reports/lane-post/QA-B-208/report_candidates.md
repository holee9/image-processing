# QA-B-208 — #251 에서 카드가 아직 없는 확정 후보 정리 (C8, C11, C15, C16, E2, E7)

요청: xpe-leader. **코드 변경 없음, 보고만.** 근거는 QA-B-204 의 재현 판정(`QA-B-204/dicom_report.md`, `enh_report.md`)과, 이번에 현재 트리(origin/main 병합 `2e296737` 위)에서 다시 읽은 코드·SPEC 이다. 이번에 새로 실행한 측정은 없고, 아래 "관측"은 모두 QA-B-204 의 것이거나 코드 읽기(grep·읽기)라고 표시했다.

## 0. 한눈에

| 후보 | 고칠 대상 | 요구 vs 코드 | 고칠 크기 | 결정 필요 |
|---|---|---|---|---|
| C8 MWL 질의 모양 | 코드 | 표준 MWL 질의가 아님 | 중(SCU 한 함수 + 모의 SCP + 시험) | 거의 없음(형태는 표준이 정함). 소소한 것 둘만 |
| C11 전송 구문 제안 | 코드(+요구 문구 보강) | "적절한 전송 구문 협상"인데 고정 제안 | 작음~중(선택지에 따라) | **예**(실패 시 동작) |
| C15 메타 접근자 | 코드(API 추가) 또는 요구 문구 | "핸들로 얻을 수 있다"인데 접근자 없음 | 중(공개 API·P/Invoke·REQ-041 개수 변경) | **예**(만들지, 문구를 고칠지) |
| C16 로그 | 코드 + 요구 문구 | 종료 로그 없음, 실패가 ERROR 아님 | 작음 | **예**(작음: "xpe_common 로깅 서브시스템"의 뜻) |
| E2 성능 시험 입력 | 시험만 | 평탄 영상을 재는 시험 | 작음 | 아니오 |
| E7 이중측 반경 상한 | 코드 또는 헤더 | σ 가 커도 효과가 포화 | 작음(코드)~중(성능) | **예**(올릴지, 한계로 문서화할지) |

## 1. C8 — MWL 질의 모양 (REQ-DICOM-034·035)
- **요구 인용**: REQ-DICOM-034 "execute a C-FIND request on the Modality Worklist Information Model using the query keys provided in `queryJson`". REQ-DICOM-035 "Supported keys: PatientID, PatientName, ScheduledStationAETitle (0040,0001), Modality, ScheduledProcedureStepStartDate (0040,0002)".
- **확정 근거**: QA-B-204 C8(코드 읽기): 요청 데이터셋을 만드는 `buildFindRequest`(`DicomNetworkSCU.cpp:391~`)가 Modality 를 최상위에 넣고 Scheduled Procedure Step Sequence `(0040,0100)` 를 만들지 않는다. `ScheduledStationAETitle`·`ScheduledProcedureStepStartDate` 키는 읽지 않는다(같은 함수에서 읽는 키는 PatientID·PatientName·Modality·AccessionNumber). 함수 안 주석은 "Add Scheduled Procedure Step Sequence as required by MWL" 이라 적었는데 그 코드는 없다. **선 위 캡처로 확인하지는 않았다**(코드 읽기만). 반면 코드는 요구 목록에 없는 `AccessionNumber` 키는 받는다.
- **고칠 범위**: 질의를 표준 MWL 모양으로 — 최상위에 PatientID·PatientName, `(0040,0100)` 시퀀스 항목 하나 안에 Modality `(0008,0060)`·ScheduledStationAETitle `(0040,0001)`·ScheduledProcedureStepStartDate `(0040,0002)`. 세 키를 JSON 에서 읽어 넣는다. 모의 SCP(지금은 PatientID 로만 맞춤)와 시험을 시퀀스 모양을 보게 고친다. 응답 쪽(어떤 태그를 JSON 으로 돌려주는지, REQ-036)은 이번에 읽지 않았다.
- **결정 필요**: 모양 자체는 표준(PS3.4)이 정하므로 코드를 요구에 맞추는 쪽이 자명하다 → 결정 불필요. 소소한 둘: (a) 요구에 없는 `AccessionNumber` 키를 요구 목록에 올릴지 지울지, (b) `ScheduledProcedureStepStartDate` 를 날짜 범위 형식(`YYYYMMDD-YYYYMMDD`)까지 받을지(받는다면 요구에 적어야 함).
- **Gap**: 실제 MWL SCP 로는 본 적이 없다.

## 2. C11 — 전송 구문 제안 (REQ-DICOM-029)
- **요구 인용**: REQ-DICOM-029 "negotiate the appropriate Transfer Syntax, and send the DICOM file at `filePath` via C-STORE DIMSE message".
- **확정 근거**: QA-B-204 C11(관측): `xpe_dicom_write` 파일(Explicit LE)과 `xpe_dicom_write_j2k` 파일(J2K Lossless)에서 제안 목록이 **같다** — Explicit LE, J2K Lossless, Implicit LE 순서(코드 `DicomNetworkSCU.cpp:129~134`로 지금도 확인). 파일의 구문과 무관한 고정 제안이다. 다른 구문의 파일(예: JPEG Lossless)은 트랜스코딩 없이 보내진다(SPEC 상태 메모). C-FIND 쪽 제안은 Explicit LE·Implicit LE 둘(`:239~241`).
- **무엇이 "적절한"가**: 파일 자신의 전송 구문(파일 메타 `(0002,0010)`)을 먼저 제안하는 것이 가장 단순한 읽기다. 피어가 그것을 거절했을 때 어떻게 할지가 열려 있다.
- **고칠 범위**: (a) 파일의 구문만 제안하고 거절되면 `XPE_ERR_NETWORK_FAILED`(+경고 알림): `DicomNetworkSCU.cpp` 십수 줄. (b) 파일의 구문 + Explicit LE 대체(비압축 파일에 한해 필요 시 변환): 변환 코드 필요. (c) 현 동작 유지, 요구 문구를 "고정 목록을 제안한다"로 고침. 선택과 무관하게 시험은 모의 SCP 가 받은 제안 목록·받은 데이터셋을 보게 해야 한다(지금 모의 SCP 는 받은 데이터셋을 보지 않고 버린다).
- **결정 필요**: **예** — (a)/(b)/(c). 제 의견은 (a): 가장 작고 계약이 분명하다. 압축 구문 파일을 조용히 다른 구문으로 바꿔 보내는 일을 만들지 않는다.
- **Gap**: 피어가 파일 구문과 다른 구문만 수락했을 때 DCMTK 의 `sendSTORERequest`(presID 0 자동 선택)가 실제로 무엇을 하는지는 시험하지 않았다.

## 3. C15 — 메타 접근자 (REQ-DICOM-009)
- **요구 인용**: REQ-DICOM-009 "(0010,0020) Patient ID → stored internally (not in XpeImageMetadata; **available via handle**)", Study/Series Instance UID·Modality 도 "stored internally".
- **확정 근거**: QA-B-204 C15(기능 없음): 공개 함수 11개 중 Patient ID·Study/Series UID·Modality 를 돌려주는 것이 없고 `getMetadata` 도 이 태그들을 읽지 않는다. 즉 "핸들로 얻을 수 있다"가 지켜지지 않았다.
- **소비처(코드 읽기)**: `xpe_dicom_get_metadata` 를 쓰는 곳은 gui `GuiDicomNative`/`XpeDicomInterop` 와 clients `XpeDicomReadinessProbe`. 이 네 값을 필요로 하는 호출자가 있는지는 이번에 확인하지 못했다.
- **고칠 범위**: (a) 접근자 추가: 일반형 `xpe_dicom_get_attribute(handle, tag, buf, len)` 하나 또는 전용 getter 4개. 공개 헤더, P/Invoke 래퍼(gui·clients — 다른 레인 소유), REQ-DICOM-041 의 "11개 함수" 개수가 바뀐다. (b) 요구 문구에서 "available via handle" 을 지우고 "읽지 않는다"로 정직하게 고침(코드 변경 0).
- **결정 필요**: **예** — 필요한 호출자가 있어야 (a)가 정당하다. 없다면 (b)가 가장 작다. 레인 간 계약(API 추가)이라 리더·gui 레인 합의가 필요하다.

## 4. C16 — 로그 (REQ-DICOM-043)
- **요구 인용**: REQ-DICOM-043 "Each function SHALL log entry/exit at DEBUG level and error conditions at ERROR level via the logging subsystem in xpe_common.dll".
- **확정 근거**: QA-B-204 C16(낮음, 관측): 진입만 DEBUG(`dicom.cpp` 10곳), 종료 로그 없음, 실패는 `[warning]`(관측: `[DicomReader] loadFile failed`, `[DicomNetworkSCU] negotiateAssociation failed`). 이번 grep 호출 수(모듈 `src/*.cpp`, 텍스트 패턴이라 호출 모양만 센 값): debug 19, info 2, warn 36, error 12. 로깅은 모듈 자체 spdlog 이다.
- **"xpe_common 로깅 서브시스템"의 뜻**: `xpe_common_api.h` 가 내보내는 로깅 함수는 설정용 셋뿐이다(`xpe_log_set_level`, `xpe_log_set_file`, `xpe_log_flush`). 로그를 **쓰는** 공개 API 는 없다. 그래서 요구가 말하는 "서브시스템을 통해"는 (i) common 이 설정하는 spdlog 인스턴스를 모듈이 공유한다는 뜻일 수 있는데 DLL 경계를 넘어 같은 로거를 쓰는지는 확인하지 못했다, 또는 (ii) 문구 오류다.
- **고칠 범위**: 11개 공개 함수 종료에 DEBUG 한 줄(RAII 한 개로 해결), 실패 경로(36개 warn 중 오류에 해당하는 것)를 ERROR 로. 로거 공유가 사실이 아니면 요구 문구 정정. 코드 수십 줄 이내, 동작 변화 없음.
- **결정 필요**: **예, 작음** — "xpe_common 로깅 서브시스템"의 정확한 뜻(로거 공유 확인, 아니면 문구 정정). 어느 쪽이든 동작 영향 없음.

## 5. E2 — 성능 시험 입력 (REQ-ENH-017 외)
- **요구 인용**: REQ-ENH-017 "WHILE processing a 3072x3072 float32 image, … `xpe_contrast_enhance` within 50 milliseconds".
- **확정 근거**: QA-B-204 E2(관측): 평탄 입력 1.9 ms 대 구조 있는 영상 47.6 ms(5회 중앙값). **CLAHE 시험은 QA-B-207 M2(`f6ab5828`)에서 이미 고쳤다**(벤치마크 데이터로 교체). 그 보고에서 새 CLAHE 는 번갈아 6회 중앙값 36.7~39.1 ms(옛 45~48 ms)라 "여유 5 %"라던 QA-B-204 의 우려는 해소됐다.
- **남은 것(코드 읽기)**: 다른 `Performance_*` 시험도 평탄 입력이다 — `test_edge_enhance.cpp:194`(USM, 500.0), `test_enhance_integration.cpp:94`(500.0), `test_log_transform.cpp:143·154`(1000.0, 2.5). 로그 변환은 화소당 계산이 값에 무관해 평탄이어도 의미가 있을 가능성이 크지만 **측정하지 않았다**. USM·통합은 평탄 입력에서 조기 반환하거나 일을 건너뛰는지 **모른다**(측정하지 않음).
- **고칠 범위**: 시험 입력을 CLAHE 와 같은 구조 있는 데이터로 바꾸고, 바꾸기 전에 평탄/구조 시간을 각각 재서 "일을 건너뛰는가"를 가린다. 제품 코드·영상 출력 영향 없음. CI 는 `*Performance*` 를 걸러(벤치마크 워크플로가 시간을 봄) 로컬 시험이라 신호 영향도 없다.
- **결정 필요**: 아니오.

## 6. E7 — 이중측 필터 반경 상한 (REQ-ENH-007)
- **요구 인용**: REQ-ENH-007 "apply bilateral filtering with the specified `sigma_space` and `sigma_range`". `sigma_space` 의 상한은 SPEC·헤더 어디에도 없다.
- **확정 근거**: QA-B-204 E7(관측, 델타 영상·`sigma_range` 1e9): σ=2 → 실효 σ 1.85; σ=5 → 4.50; σ=7.5 → 6.70; σ=10 → 7.59; σ=15 → 8.32; σ=20 → 8.59(자르지 않은 가우시안은 10·15·20). 반경이 `min(15, min(w,h)/2−1)`(`noise_reduce.cpp:211`)로 막히고, 커널이 2σ 에서 잘려 모든 σ 에서 7~10 % 작다. 큰 σ 호출자에게만 조용히 약해진다(기본 3.0, σ ≤ 5 에서는 작음).
- **고칠 범위**: (a) 반경 상한 15 를 σ 에 비례(예: 4σ)로 올림 — 비용: 커널 화소 수가 (2r+1)² 이라 σ=15 에서 r 15 → 60 이면 961 → 14641(약 15배, 산술값이고 실측 아님). REQ-ENH-012 의 100 ms 예산은 큰 σ 에서 지킬 수 없을 수 있다. (b) 상한을 문서화하고 초과를 거부하거나 경고: 헤더 한 줄 + 검증 몇 줄, 영상 출력은 σ ≳ 5 에서만 달라진다.
- **결정 필요**: **예** — (a) 성능을 내주고 요구에 충실, (b) 지원 σ 범위를 못 박기. 제 의견은 (b)(요구가 σ 상한을 말하지 않았으니 한계를 적는 것이 먼저고, 큰 σ 가 실제로 쓰이는지부터 확인).

## 7. 제안 카드 순서
1. E2(시험만, 결정 없음, 부수 효과 없음) 2. C8(결정 거의 없음, 모양이 표준으로 정해짐) 3. C16(작은 결정 하나) 4. C11 5. C15 6. E7. C11·C15·E7 은 위 결정이 먼저 필요하다.

## 8. Gap / 잔여 위험
Gap
- 이번에 새로 실행한 측정은 없다. 관측은 QA-B-204 의 것(기계 한 대)이고 나머지는 코드·SPEC 읽기다.
- C8·C11 은 선 위 캡처로 확인하지 않았다.
- C15 의 실제 호출자 수요, C16 의 로거 공유 여부, E2 의 USM·통합 시험이 평탄 입력에서 일을 건너뛰는지는 모른다.
- 나열한 6개 외의 #251 후보(C14 등)는 이번 범위가 아니다.

잔여 위험
- C11 을 (c) 로 두면 압축 구문 파일의 전송이 피어 설정에 따라 조용히 실패하거나 다른 구문으로 보내질 수 있다.
