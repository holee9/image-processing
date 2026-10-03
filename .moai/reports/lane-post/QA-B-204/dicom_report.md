# QA-B-204 DICOM — #251 남은 결함 후보 C2~C6, C8, C10~C16 재현 판정

카드: QA-B-204(보고만, 코드 변경 없음). 후보 출처: `.moai/reports/lane-post/QA-B-199/dicom_report.md` §6. C1·C7·C9 는 이번 카드 범위 밖(QA-B-200/201 에서 처리됨). C12 도 QA-B-201 M4 에서 처리됐다(아래 §9).

방법: 빌드된 `build/ci-dicom/bin/xpe_dicom.dll` (+ vcpkg DCMTK DLL)을 ctypes 로 직접 호출(`dicom_probe.py.txt`, 출력 `dicom_probe_out.txt`). 네트워크 항목은 파이썬 소켓으로 만든 피어(수락 후 침묵 / 첫 PDU 캡처)로 확인했다. 소스는 바꾸지 않았다.

## 0. 판정 한눈에

| 후보 | 판정 | 실제로 관측한 것 | 성격 |
|---|---|---|---|
| C10 | **확정, 심각** | 정체하는 피어에 `timeoutMs` 300·700·999 → **100.02초** 뒤에야 반환(300 의 측정; 700·999 는 40초 상한 안에 반환 안 됨). 0 → 30.02초. 1000·1500·2500 → 1.01·1.01·2.01초. `xpe_dicom_cancel()` 도 정체를 못 푼다(호출 20초 뒤에도 그대로). | 무한에 가까운 멈춤 |
| C13 | **확정** | `dataSize == 0` + 비NULL data → 쓰기는 `XPE_OK`, 그러나 PixelData 길이 0. 같은 파일을 `read_image` 하면 `DICOM_INVALID`. 화소가 없는 파일을 성공으로 보고. | 데이터 손실 |
| C3 | **확정(요구 결정)** | 서문·DICM·meta 가 전부 없는 데이터셋은 `XPE_OK` 로 열린다. 서문 + 가짜 매직, 서문 + DICM + 쓰레기, 빈 파일, 텍스트는 `DICOM_INVALID`. 시험이 이 거동을 고정한다. | 계약 불일치 |
| C5 | **확정** | PatientID 또는 PatientName 값을 공백으로 지워도 `valid:true`, errors 비어 있음. | 검증이 느슨함 |
| C6 | **확정** | 잘못된 UID 하나가 `errors` 와 `warnings` 양쪽에 같은 항목으로 들어가고 `valid:false`. | 계약 불일치 |
| C4 | **확정(문구 불일치)** | 파싱 불가 파일: rc `DICOM_INVALID`(SPEC 일치), 보고는 `tag "0008,0000"`, `"File cannot be parsed as DICOM: …"`. SPEC 은 `tag ""`, `"Not a valid DICOM file"`. | 문서·코드 한쪽 |
| C2 | **확정(SPEC 문구가 틀림)** | 초 단위로 쓰고 읽는다(1700000000 → 파일에 `20231114`/`221320.000000`, 읽으면 1700000000). 밀리초 값 1700000000000 을 넘기면 파일에 날짜가 없고 읽으면 **0**, rc 는 둘 다 OK. 타입 헤더·코드 일치, SPEC 만 ms. | 문서(+ 조용한 손실) |
| C8 | **확정(코드 읽기)** | 요청 데이터셋을 만드는 함수 전체(`buildFindRequest`, 25줄)에서 Modality 가 최상위에 들어가고 (0040,0100) 시퀀스를 만들지 않는다. `ScheduledStationAETitle`·`ScheduledProcedureStepStartDate` 는 읽지도 않는다. 선 위 캡처로 확인하지는 않았다. | 표준 질의 아님 |
| C11 | **확정** | `xpe_dicom_write` 파일(Explicit LE)과 `xpe_dicom_write_j2k` 파일(J2K Lossless)에서 제안 목록이 **같다**: Explicit LE, J2K Lossless, Implicit LE(순서 그대로). 파일 구문과 무관한 고정 제안. | 계약 불일치 |
| C14 | **코드 형태 확인, 실행 재현 못 함** | `xpe_dicom_open` 의 `h` 가 `try` 안에서 `new` 로 만들어지고 `catch(...)` 에는 해제가 없다. `open()` 에도 `try`/`catch` 가 없다. 던지는 경로를 만들 수단이 없었다(예: bad_alloc 주입). | 누수(조건부) |
| C15 | **확정(기능 없음)** | API 는 함수 11개이고 Patient ID·Study/Series UID·Modality 를 돌려주는 것이 없다. SPEC 은 "available via handle". `getMetadata` 는 읽지 않는다. | 기능 공백 |
| C16 | **확정(낮음)** | 진입만 DEBUG(10곳)이고 종료 로그는 없다(`dicom.cpp`). 실패는 `[warning]` 으로 남는다(관측: `[DicomReader] loadFile failed`, `[DicomNetworkSCU] negotiateAssociation failed`). 로깅은 모듈 자체 spdlog. | 로그 |

## 1. C10 — 짧은 timeoutMs 가 100초 멈춤을 만든다

REQ-DICOM-031: 연결을 `timeoutMs` 안에 못 맺으면 `NETWORK_FAILED`. 관측(수락 후 아무것도 보내지 않는 피어, 호출마다 새 프로세스, 상한 40초):

| timeoutMs | 반환까지 | rc |
|---|---|---|
| 0 | 30.02 s | NETWORK_FAILED |
| 300, 700, 999 | **상한 40 s 안에 반환 안 됨**; 300 을 130 s 상한으로 다시 재니 **100.02 s** | NETWORK_FAILED |
| 1000 | 1.01 s | NETWORK_FAILED |
| 1500 | 1.01 s | NETWORK_FAILED |
| 2500 | 2.01 s | NETWORK_FAILED |

- 1000 ms 미만은 초로 바꾸면 0 이고(`timeoutMs / 1000`), DCMTK 에서 0 초는 "기본 대기" 로 읽혀 100 초가 된다고 보인다. 1000 이상은 정수 초로 내림되어 정확히 동작(1500 → 1 s)한다. 원인 설명은 관측한 숫자에 맞춘 추정이다(소스의 설정 지점을 이번에 다시 읽지 않았다).
- `timeoutMs == 0` 은 30 초가 걸린다. 이 값이 "무제한" 이나 "기본" 중 어느 쪽 약속인지 헤더에 확인하지 않았다(Gap).
- `xpe_dicom_cancel()` 은 정체를 풀지 못했다(호출 후 20 초 동안 그대로). 이것은 이미 문서화된 한계다 — QA-B-200 M2a 가 "DCMTK 호출은 중단할 수 없고 교환이 끝난 뒤 취소를 보고한다" 고 적었다. 그래서 짧은 타임아웃이 유일한 방어인데 그것이 바로 이 결함으로 깨져 있다.
- 판정: 확정, 우선순위 가장 높음. 수정: 올림(`(ms + 999) / 1000`, 최소 1)이나 DCMTK 의 밀리초 API 로 바꾸고, 시험은 정체 피어(소켓)로. 시험에는 타임아웃 3초 이하로 해야 100초를 안 기다린다.

## 2. C13 — dataSize == 0

REQ-DICOM-013. 8×8 UINT16 영상, data 비NULL:

| 호출 | 쓰기 | PixelData 길이 | 읽기 |
|---|---|---|---|
| dataSize = 128 (대조군) | OK | 128 | open OK, read_image OK → 8×8, 128 바이트 |
| **dataSize = 0** | **OK** | **0** | open OK, read_image **DICOM_INVALID** |
| dataSize = 100 | INVALID_INPUT | — | — |

- 쓰기가 화소 없는 파일을 성공으로 돌려준다. 100 바이트는 거부되는데 0 바이트는 통과하는 불일치도 보인다.
- 판정: 확정. 수정은 쓰기 입구에서 `dataSize == 0`(또는 `< width*height*bpp`)을 거부.

## 3. C3 — 서문·매직·meta 없는 파일

| 입력 | 결과 |
|---|---|
| 대조군: `xpe_dicom_write` 가 쓴 파일 | OK, 핸들 있음 |
| 서문·DICM·meta 없는 Explicit LE 데이터셋(PatientName, Rows) | **OK, 핸들 있음** |
| 서문 128 바이트 + `XXXX` + 데이터셋 | DICOM_INVALID, 핸들 없음 |
| 서문 + DICM + 쓰레기 | DICOM_INVALID, 핸들 없음 |
| 빈 파일 / 일반 텍스트 | DICOM_INVALID, 핸들 없음 |

- 요구(REQ-DICOM-003)는 서문·매직·meta 가 없으면 `DICOM_INVALID`. 완전히 없는 경우만 통과한다(DCMTK 의 파일 형식 자동 감지).
- 이 동작은 `OpenDatasetWithoutMetaHeader_TreatedAsExplicitLE` 시험이 명시적으로 고정하고 헤더에 밝혀 있다. 그래서 결함이라기보다 요구 결정이다: ① 요구 문구를 코드에 맞춘다(데이터셋만 있는 파일도 받는다), ② 코드를 요구에 맞춘다(엄격 모드). 임상 용도에서는 Part 10 이 아닌 파일을 조용히 열지 않는 쪽이 안전하다.

## 4. C4·C5·C6 — 검증기 보고

- **C4**: 파싱 불가 파일 → rc `-13`, 보고 `{"errors":[{"message":"File cannot be parsed as DICOM: I/O suspension or premature end of stream","tag":"0008,0000"}],"valid":false,"warnings":[]}`. SPEC 보고(`tag ""`, `"Not a valid DICOM file"`)와 두 필드가 다르다. 메시지는 DCMTK 의 상태 문구라 버전에 따라 바뀔 수 있다. 수정 방향: SPEC 문구에 맞추거나, SPEC 을 코드에 맞춘다.
- **C5**: 쓴 파일을 대조군으로 두면(`rc 0`, `valid:true`, errors `[]`), PatientID 값(9자 + 패딩)을 같은 길이의 공백으로 지워도 PatientName 을 지워도 `rc 0`, `valid:true`, errors `[]`. 요구는 "present and non-empty". 화소 표현과 선언된 형식의 일관성 검사는 이번에 관측하지 않았다(Gap).
- **C6**: 데이터셋의 StudyInstanceUID 마지막 숫자를 `x` 로 바꾸면 `valid:false`, errors = `[{tag 0020,000D, "Invalid UID format: …6x"}]`, warnings 에 **같은 항목**. 한 문제가 두 배열에 들어가 비치명 경고와 치명 오류를 구별하지 못한다.

## 5. C2 — acquisitionTime 의 단위

| 넘긴 값 | 파일에 들어간 값 | 읽어 온 값 |
|---|---|---|
| 1700000000 (초) | `20231114`, `221320.000000` | 1700000000 |
| 1700000000000 (같은 시각의 ms) | 날짜·시간 문자열 없음 | **0** (rc OK) |

- 코드와 `xpe_types.h`("seconds since Unix epoch, UTC. 0 = unknown")가 같고 SPEC(`epoch ms`)만 다르다. 문구를 코드에 맞추는 쪽이 자연스럽다.
- 별도 위험: ms 로 오해한 호출자의 값이 오류 없이 사라진다(범위 밖 값에 대한 거부가 없음). 상한 검사를 둘지는 선택.

## 6. C8 — MWL 질의의 모양

`buildFindRequest` 는 이 파일에서 요청 데이터셋을 만드는 유일한 함수다(25줄). 읽은 사실: ① 시작할 때 PatientID·PatientName·StudyInstanceUID·AccessionNumber·Modality 를 빈 값으로 넣고, ② JSON 의 PatientID·PatientName·Modality·AccessionNumber 만 읽는다, ③ `ScheduledProcedureStepSequence (0040,0100)` 을 만들지 않는다(주석은 "Add Scheduled Procedure Step Sequence as required by MWL" 이라 적었지만 코드가 없다). 따라서 Modality 가 최상위에 들어가고, 표준 MWL SCP 는 시퀀스 안에서 키를 찾는다.
- 이 판정은 코드 읽기다. MockScp 가 질의 전체를 저장하지 않아(PatientID 만 읽음) 선 위에서 캡처하지는 못했다. 수정 시에는 MockScp 에 질의 사본을 담는 필드를 먼저 추가해 시험으로 고정해야 한다(헤더는 이미 두 키가 무시된다고 밝힌다).

## 7. C11 — 제안되는 전송 구문

A-ASSOCIATE-RQ(270 바이트)를 소켓으로 받아 UID 를 순서대로 읽었다. Explicit LE 파일과 J2K Lossless 파일 둘 다 제안 순서 `Explicit VR Little Endian`, `JPEG 2000 Lossless`, `Implicit VR Little Endian`. 파일 구문에 따라 달라지지 않는다.
- 피어가 J2K 를 받지 않고 Explicit LE 만 받으면 보내는 쪽이 파일을 트랜스코딩하는지는 관측하지 않았다(Gap). JPEG Lossless 파일은 만들 수단이 없어 확인하지 못했다.

## 8. C14·C15·C16

- **C14** `xpe_dicom_open`: `auto* h = new XpeDicomHandle(filePath);` 가 `try` 안, `catch (...)` 는 `h` 를 못 본다. `h->reader.open()` 이 던지면 누수. `open()` 안에는 `try` 가 없다. `open()` 이 던지는 입력을 만들지 못해(DCMTK 는 오류 코드를 돌려줌) 실행으로는 재현하지 못했다. 수정은 `std::unique_ptr` 한 줄이라 비용이 거의 없다.
- **C15**: 헤더의 함수 11개에 Patient ID·UID·Modality 접근자가 없다. SPEC 이 "stored internally… available via handle" 이라 쓴 기능이 공개 API 에 없다. gui 가 쓰는지는 확인하지 않았다(Gap).
- **C16**: `dicom.cpp` 에 `spdlog::debug` 가 10곳(함수 진입마다), 종료 로그 없음(`grep` 으로 0 건). 이번 프로브가 낸 실패 로그는 `[warning]` 이다. REQ-DICOM-043 은 ERROR 수준·xpe_common 로깅 서브시스템을 요구.

## 9. 이번 카드 밖에서 이미 닫힌 것
- C12(FLOAT32·UINT8 버퍼를 uint16 단어로 씀): QA-B-201 M4 에서 `xpe_dicom_write`·`write_j2k` 가 UINT16 이 아니면 입구에서 거부(commit decdd092).
- C1(mAs 태그): QA-B-200 M2a 계열에서 1152+1153 쓰기·읽기로 처리.

## 10. 처리 우선순위 제안(DICOM)

1. **C10** — 짧은 타임아웃이 100초 멈춤(그리고 취소 불가). 임상 워크플로를 멈출 수 있다. 수정 작고 시험은 소켓 피어.
2. **C13** — 성공으로 보고된 빈 파일(데이터 손실). 입구 거부 한 줄.
3. **C14** — 조건부 누수, `unique_ptr` 한 줄.
4. **C3** — 요구 결정(엄격하게 거부할지 문구를 바꿀지).
5. **C5·C6** — 검증기 정확도(빈 Type 1 값 거부, 경고·오류 중복 해소).
6. **C8** — MWL 질의를 표준 모양으로(MockScp 캡처 선행).
7. **C11·C15** — 기능 공백(전송 구문 협상, 접근자).
8. **C2·C4·C16** — 문서 또는 로그 수준.

## 11. Gap / 잔여 위험

Gap
- C8·C14·C15·C16 은 코드 읽기와 API 목록으로 판정했고, C8·C14 는 선 위·실행 재현을 하지 못했다.
- C10 의 100초가 DCMTK 의 어떤 기본값에서 오는지, `timeoutMs == 0` 이 약속하는 의미를 소스에서 확인하지 않았다.
- C11: 피어가 협상 결과로 다른 구문을 고르는 경우의 전송을 관측하지 않았다.
- C5: 화소 표현·선언 형식 일관성 검사의 유무는 관측하지 않았다.
- 측정은 이 체크아웃의 `ci-dicom` 빌드(MSVC, vcpkg DCMTK) 하나이다.

잔여 위험
- C10 의 시간은 이 기계 부하에 약간 흔들린다(1.01·2.01 s 처럼 내림 규칙이 그대로 보여 안정적이지만, 100.02 s 는 한 번의 측정이다). "300 ms → 약 100 s" 는 한 번의 측정, "700·999 ms → 40 s 넘게" 는 한 번의 측정이다.
