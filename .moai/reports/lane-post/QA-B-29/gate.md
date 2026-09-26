# QA-B-29 게이트 보고서 — in-process mock SCP + 오류 핸들러 · J2K 손상 경로

**카드**: QA-B-29 (#120 #124, 사용자 결정 B)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-29/`
**선행 병합**: `git merge origin/main` → `3fa1bfd`
**커밋**: `92db69a` (1/3 도입) · `208447f` (1/3 원인 해소) · `0bc0c05` (2/3) · `be85276` (3/3)

---

## 1. 주장 (Claim)

| # | 주장 | 상태 |
|---|------|------|
| C1 | 픽스처 소유 in-process SCP 를 도입했다 (기동·종료 보장) | PASS |
| C2 | **`CStoreSuccess_ReturnsOK` 가 스킵에서 실행·통과로 바뀌었다** | PASS |
| C3 | 나머지 3건은 실행 전환 실패 — 스킵 유지, 사유는 관측값 | **부분 미달** (§2.4) |
| C4 | B-27 분류표를 **계측 줄 기준**으로 재검산했다 | PASS |
| C5 | J2K 코드스트림 손상 경로 2건을 커버했다 | PASS |
| C6 | 재실측 ci-post 445 / ci-ai 192 / ci-dicom **121** 전부 통과, skip 9 → 8 | PASS |
| C7 | leader 요청 단서(SCU 가 제안하는 MWL TS 목록)를 기록했다 | PASS (§4) |

---

## 2. 증거 (Evidence)

### 2.1 픽스처와 수명 보장 (C1)

`modules/dicom/tests/mock_scp.{hpp,cpp}` — `DcmSCP` 파생. C-STORE 는
`handleSTORERequest`, C-FIND(MWL)는 고정 1건 응답.

| 카드 규약 | 구현 |
|---|---|
| `SetUp` 기동 / `TearDown` 종료 | `SetUpTestSuite` → `start()`, `TearDownTestSuite` → `stop()` |
| join 보장 | `stop()` 이 stop 플래그를 세우고 폴링 후 `join()`. 소멸자에도 최후 방어 |
| 타임아웃 시 실패 보고 | `stop()` 이 `false` 반환 → `EXPECT_TRUE(s_scp.stop())` 로 **테스트 실패**. 조용히 넘기지 않는다 |
| 고정 포트 금지 | OS 에 포트 0 으로 물어 실제 번호를 조회(`probe_free_port`) |
| detached 스레드·별도 exe 금지 | `std::thread` 멤버, joinable 유지. 별도 프로세스 없음 |

DCMTK 는 stop *명령* 을 노출하지 않는다 — `stopAfterCurrentAssociation()` /
`stopAfterConnectionTimeout()` 은 기반 클래스가 **호출하는 protected 술어**다.
그래서 `requestStop()` 이 플래그를 세우고 두 술어가 그 값을 보고하는 형태로 구현했다.

### 2.2 블로커와 그 원인 (C2) — 이 카드의 실질

첫 구현은 `listen()` 이 `NET_EC_InvalidSCPAssociationProfile` 로 실패했다.
헤더 문서는 이 오류를 *"예: 프레젠테이션 컨텍스트가 하나도 추가되지 않음"* 이라고 적는다.
**관측은 정반대였다.**

`dumpPresentationContexts` 출력 (당시):
```
Dumping Profile: DEFAULT
Presentation Context #1, abstract syntax: 1.2.840.10008.1.1        ← Verification
...
Presentation Context #5, abstract syntax: 1.2.840.10008.1.1        ← 또
Presentation Context #6, abstract syntax: 1.2.840.10008.1.1        ← 또
```

**원인은 부재가 아니라 중복이었다.** `DcmStorageSCP` 가 이미 Verification 을 등록하는데
내가 배열로 한 번, `setEnableVerification()` 으로 또 한 번 넣어 같은 abstract syntax 가
프로필에 세 번 들어갔다. 중복을 없애자 곧바로 association 이 성립했다.

```
[       OK ] DicomNetworkTest.CStoreSuccess_ReturnsOK (2 ms)
```

기반 클래스도 `DcmStorageSCP` → `DcmSCP` 로 바꿨다. `DcmStorageSCP` 는 저장 SOP
클래스만 협상해서 C-FIND 가 컨텍스트를 잡지 못한다.

**leader 의 두 지시에 대한 결과:**
- 사전 점검("`DEFAULT_PCKEY` 의 TS 목록이 비었는지") → **반증됐다.** TS 는 채워져 있었다
  (TSKEY_0/1/2 각각 1~3개). 다만 그 점검 때문에 덤프를 다시 본 것이 중복 발견으로 이어졌다.
- `mock_scp.cfg` + `XPE_DICOM_TEST_DATA_DIR` → **만들지 않았다.** 불필요했다.
  저장소에 데이터 파일과 CMake define 을 늘리지 않는 쪽을 택했다.

### 2.3 시도하고 실패한 것 (기록)

블로커 규명 과정에서 시도한 것들. 전부 반환값은 정상이고 `listen()` 만 거부했다:

| 시도 | 결과 |
|---|---|
| `DcmSCP::addPresentationContext(...)` | 성공, listen 거부 |
| `getConfig().addPresentationContext(...)` | 성공, listen 거부 |
| `getConfig().setAndCheckAssociationProfile("DEFAULT")` | **이 호출 자체가 같은 오류**. `checkAssociationProfile` 도 동일, mangled 이름은 `'DEFAULT'` 로 정상 |
| `setEnableVerification()` | 성공, listen 거부 (그리고 이것이 중복의 한 원인이었다) |

같은 지점 5회 실패 시점에 레인 규약대로 멈추고 leader 에 블로커를 보고했고,
그 뒤 점검 지시를 따르다 원인을 찾았다.

### 2.4 남은 3건 — 스킵 유지 (C3, 미달)

빨간 스위트를 남기지 않기 위해 정확한 사유를 붙인 스킵으로 두었다.
`test_dicom_network_scu.cpp` 안에 사유가 상수로 박혀 있다.

| 테스트 | 사유 (관측) |
|---|---|
| `CFindResults_ReturnsJsonArray` | MWL 컨텍스트가 프로필에 **있는데도**(덤프로 `1.2.840.10008.5.1.4.31` + Explicit LE/Implicit LE/J2K 확인) SCU 가 `DIMSE No valid Presentation Context ID` 를 낸다 |
| `CFindEmpty_ReturnsEmptyArray` | 같음 |
| `CancelCStore_TerminatesOperation` | 로컬 SCP 상대로 C-STORE 가 약 1 ms 에 끝나 100 ms 뒤 취소가 도달할 수 없다. `PROCESSING_FAILED` 를 단언하면 동작이 아니라 **경쟁을 단언**하는 것이 된다 |

C-FIND 는 `DcmSCP` 전환, `setAlwaysAcceptDefaultRole(OFTrue)`, 준비 프로브 제거 실험으로도
바뀌지 않았다. **같은 리스너에서 C-STORE 는 성립하므로 리스너 자체는 정상이고
MWL 협상만 남은 문제다.** leader 판정으로 여기서 접었다.

### 2.5 분류표 계측 기준 재검산 (C4)

B-28 의 자기 정정(소스 구간 ≠ 계측 줄)을 B-27 분류표 전체로 확장했다.
`_reclass.txt` — 줄 번호를 빼서 얻은 값이 아니라, 각 구간에서 **아티팩트가 계측한 줄**을 센 값이다.

| 파일 | 구간 | 계측 | 미커버 | B-27 추정 |
|---|---|---:|---:|---:|
| `DicomNetworkSCU.cpp` | 연결 전 (피어 불필요) | 7 | 7 | — |
| | association 이후 (라이브 SCP 필요) | 111 | **90** | ~95 |
| `DicomValidator.cpp` | 조기 반환 (meta/dataset 없음) | 30 | 30 | ~30 |
| | 리포트 버퍼 크기 | 3 | 3 | — |
| | `buildReport` json 분기 | 13 | 8 | — |
| `DicomReader.cpp` | J2K 디코드 실패 | 86 | **33** | ~35 |
| | open / transfer-syntax | 21 | 16 | — |
| `dicom.cpp` | `catch(...)` | 35 | **33** | ~34 |
| | 미지원 bpp 분기 | 1 | 1 | — |
| `DicomWriter.cpp` | openjpeg 실패 | 71 | **23** | ~26 |
| | 일반 입력 분기 | 4 | 4 | — |
| **버킷 합** | | **382** | **248** | |
| **리포트 전체** | | **1156** | **326** | |

추정치들은 크게 틀리지 않았지만 **전부 소폭 과대**였다.

**[중요] 이 재검산의 기준선은 B-27 이전이다.** 아티팩트는 CI run `34447732961` 이고
QA-B-27 커밋 4건보다 앞선다. 그래서 표의 "미커버" 에는 B-27 에서 이미 덮은 것
(writer 일반 입력 4, validator 버퍼 3 등)이 그대로 들어 있다. **이 표를 현재 상태로 읽으면 안 된다.**

### 2.6 추가 커버 (C5 + 커밋 2)

| 테스트 | 표적 |
|---|---|
| `WriteUint8Format_NotRejectedBySizeGuard` | `dicom.cpp:100-104` — `XPE_PIXEL_UINT8` 은 선언된 포맷인데 크기 가드에 bpp 항목이 없다. 가드는 비교 없이 통과시키고 포맷 판정은 뒤 단계에 맡겨야 한다 |
| `ReadJ2K_CorruptHeader_ReturnsProcessingFailed` | `DicomReader.cpp:416-421` — SOC 직후 SIZ 훼손 → `opj_read_header` 실패 |
| `ReadJ2K_CorruptBody_ReturnsProcessingFailed` | `:425-431` — 헤더는 두고 본문 훼손 → 디코드 단계 |

J2K 손상은 **길이를 보존**한다(삽입·삭제 없이 덮어쓰기만). DICOM 컨테이너가 계속
유효하므로 실패 원인이 파싱이 아니라 디코더임이 확정된다.
`CorruptBody` 는 손상 본문에서 픽셀이 나오는 것도 허용 가능한 결과로 보아 `XPE_OK` 를
실패로 단언하지 않는다 — 다만 `XPE_OK` 면 버퍼가 실제로 있어야 한다고 단언한다.

### 2.7 재실측 (C6) — `_verify.log`, 필터 없음

```
===CI_POST===    100% tests passed, 0 tests failed out of 445   ===POST_EXIT=0===
===CI_AI===      100% tests passed, 0 tests failed out of 192   ===AI_EXIT=0===
===CI_DICOM===   100% tests passed, 0 tests failed out of 121   ===DICOM_EXIT=0===
```

| | B-28 | B-29 | 차 |
|---|---:|---:|---:|
| ci-dicom 전체 | 118 | **121** | +3 (uint8 가드 1, J2K 손상 2) |
| ci-dicom skip | 9 | **8** | −1 (`CStoreSuccess` 가 실행으로) |
| ci-post / ci-ai | 445 / 192 | 445 / 192 | 0 |

---

## 3. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 분류 재검산 | §2.5 표 | CI run `34447732961` 아티팩트를 구간별로 재파싱 (`_reclass.txt`) |
| ci-dicom 121/121, skip 8 | §2.7 | `_verify.log`, 이번 실행, `-R` 필터 없음 |
| B-28 기준선 118 / skip 9 | | `.moai/reports/lane-post/QA-B-28/_after.log` |

커버리지 line-rate 는 **재지 않았다** — 로컬 OpenCppCoverage 부재(B-27 §4 와 동일 제약).
카드대로 leader 의 CI dispatch 가 정본이다.

---

## 4. 미검증 (Gaps)

### 4.1 leader 요청 단서 — SCU 가 제안하는 MWL TS 목록

C-FIND 실패를 다음에 파는 사람을 위한 관측이다. **실험은 하지 않았고 소스 판독이다**
(`DicomNetworkSCU.cpp:182-190`):

```cpp
// Add Modality Worklist presentation context
OFList<OFString> transferSyntaxes;
transferSyntaxes.push_back(OFString(UID_LittleEndianExplicitTransferSyntax));  // 1.2.840.10008.1.2.1
transferSyntaxes.push_back(OFString(UID_LittleEndianImplicitTransferSyntax));  // 1.2.840.10008.1.2
scu.addPresentationContext(OFString(UID_FINDModalityWorklistInformationModel), transferSyntaxes);
```

- **SCU 제안**: MWL(`1.2.840.10008.5.1.4.31`) × { Explicit LE, Implicit LE } — **2종**
- **mock 제공**: 같은 abstract syntax × { Explicit LE, Implicit LE, J2K } — 3종 (덤프로 확인)

**교집합이 비어 있지 않다 → 전송 구문 불일치는 원인이 아니다.** 한 가설을 지운 것이
이 단서의 값이다. 비교 대상으로, 같은 리스너에서 성립하는 C-STORE 는 SCU 가
3종(Explicit LE, Implicit LE, J2K)을 제안한다(`:93-96`).

`ASC_dumpParameters` 로 협상 결과 자체를 덤프하지는 않았다 — SCU 는 제품 코드라
이 레인이 진단 출력을 넣을 자리가 아니라고 판단했다. 필요하면 별도 카드로.

### 4.2 그 밖에 관측하지 못한 것

- **C-FIND 실패의 근본 원인 미특정.** 누적 10회 시도, 위 §2.3 + `DcmSCP` 전환 +
  `setAlwaysAcceptDefaultRole` + 프로브 제거 실험 전부 무효.
- **Cancel 의 취소 경로는 여전히 미검증이다.** 스킵 사유가 정확해졌을 뿐,
  `DicomNetworkSCU.cpp:119-121`(association 이후 취소)은 실행된 적이 없다.
- **커버리지 수치 미측정** — §3.
- **§2.5 표는 B-27 이전 기준선이다** — §2.5 말미.
- **mock SCP 는 C-STORE 를 메모리에서 소비하고 버린다.** 수신 객체의 내용이
  보낸 것과 같은지 비교하지 않는다 — `CStoreSuccess` 는 반환 코드만 단언한다.
- **포트 프로브에는 TOCTOU 창이 있다** — §5.

---

## 5. 잔여 위험 (Residual-risk)

- **포트 프로브 경쟁.** `probe_free_port()` 가 포트를 얻어 닫은 뒤 SCP 가 다시 bind 한다.
  그 사이에 다른 프로세스가 채가면 기동이 실패한다 — 고정 포트보다 낫다고 판단했지만
  창은 남아 있다. 실패 시 조용히 넘어가지 않고 `listenError()` 에 사유가 담긴다.
- **준비 확인용 더미 접속.** `wait_until_accepting()` 이 TCP 연결을 열고 즉시 닫는다.
  SCP 는 이것을 association 시도로 받아 실패 처리한다. 현재 동작에 영향은 관측되지
  않았지만(제거 실험에서도 C-FIND 결과 불변) SCP 상태를 건드리는 것은 사실이다.
- **DCMTK 버전 의존.** 중복 abstract syntax 가 프로필을 무효로 만드는 동작은 이 버전에서
  관측한 것이다. 버전이 바뀌면 다르게 굴 수 있고, 그때 증상은 다시 "Invalid or
  non-existing SCP Association Profile" 이라는 방향을 잘못 가리키는 메시지로 나타난다.
- **`CorruptBody` 는 `XPE_OK` 도 허용한다.** openjpeg 가 손상 본문에서 픽셀을 만들어내면
  통과한다. 이 케이스는 "죽지 않는다 + XPE_OK 면 버퍼가 있다" 까지만 보증한다.
- 커밋 4건은 origin/main 병합 전까지 미푸시다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` | vcvars + CMake/Ninja PATH |
| `_dicom.bat` / `_after.log` | ci-dicom 빌드 + 전체 ctest 121/121, skip 8 |
| `_run1.bat` / `_blocker.log` | 블로커 재현 — `listen()` 이 돌려준 사유 |
| `_reclass.txt` | 분류표 계측 줄 기준 재검산 |
| `_verify.bat` / `_verify.log` | 재실측 445 / 192 / 121 |
