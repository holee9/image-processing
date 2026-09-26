# QA-B-32 게이트 보고서 — MWL C-FIND 결함 수정 (#137)

**카드**: QA-B-32 (#137 #124)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-32/`
**커밋**: `e71d459`

---

## 1. 주장 (Claim)

| # | 주장 | 상태 |
|---|------|------|
| C1 | RED: 스킵 제거 후 C-FIND 2건이 현재 코드에서 실패함을 관측했다 | PASS |
| C2 | `presID` 를 협상된 컨텍스트 ID 로 조회해 전달하도록 고쳤다 | PASS |
| C3 | GREEN: C-FIND 2건 실행·통과 | PASS |
| C4 | `send*Request` 전수 대조표를 계약 문서 인용으로 제시했다 | PASS |
| C5 | ci-dicom 131/131, skip 8 → **6** | PASS |
| C6 | 무회귀: ci-post 445 / ci-ai 192 | PASS |
| C7 | B-30 진단 훅은 기본 꺼짐 유지 | PASS |

---

## 2. 증거 (Evidence)

### 2.1 RED (C1) — `_red.log`

`kFindNotNegotiated` 스킵 상수를 제거해 두 케이스를 실제로 실행시켰다.

```
===BUILD=0===
98% tests passed, 2 tests failed out of 131
123/131 Test #123: DicomNetworkTest.CFindResults_ReturnsJsonArray ...***Failed    1.62 sec
124/131 Test #124: DicomNetworkTest.CFindEmpty_ReturnsEmptyArray ....***Failed    1.93 sec
```

B-30 의 진단이 실행으로 재현됐다 — 추론이 아니라 실패하는 테스트가 됐다.

### 2.2 수정 (C2)

`modules/dicom/src/DicomNetworkSCU.cpp`

```diff
-    cond = scu.sendFINDRequest(0, &requestDS, &responses);
+    const T_ASC_PresentationContextID findPresID =
+        scu.findPresentationContextID(OFString(UID_FINDModalityWorklistInformationModel),
+                                      OFString(""));
+    if (findPresID == 0) {
+        spdlog::warn("[DicomNetworkSCU] cfind: no accepted presentation context for MWL");
+        scu.releaseAssociation();
+        return XPE_ERR_NETWORK_FAILED;
+    }
+
+    cond = scu.sendFINDRequest(findPresID, &requestDS, &responses);
```

`findPresentationContextID` 는 `scu.h:238` 이 *"Adequate Presentation context ID that can be
used. **0 if none found**"* 라고 적는다. 그래서 0 을 실패로 처리하고 사유를 남긴다 —
카드가 요구한 "ID 를 못 찾으면 `XPE_ERR_NETWORK_FAILED` + 사유".

`releaseAssociation()` 을 이 경로에서도 호출한다. 협상은 이미 성립한 뒤라 그냥 반환하면
association 이 남는다.

### 2.3 `send*Request` 전수 대조표 (C4)

파일 내 호출은 **둘뿐이다**:

```
$ grep -n "send[A-Za-z]*Request(" modules/dicom/src/DicomNetworkSCU.cpp
127:    cond = scu.sendSTORERequest(
226:    cond = scu.sendFINDRequest(...)
```

| 호출부 | API | DCMTK 계약 (헤더 인용) | `0` 허용? | 조치 |
|---|---|---|---|---|
| `:127` C-STORE | `DcmSCU::sendSTORERequest` | `scu.h:273-277` — *"@param presID … **If 0 is given, the function tries to find an appropriate presentation context itself** (based on SOP class and original transfer syntax of the 'dicomFile' or 'dataset')."* | **예** | **수정 없음** — 계약상 정당하고 실제로 동작한다 |
| `:226` C-FIND | `DcmSCU::sendFINDRequest` | `scu.h:461-462` — *"@param presID [in] The presentation context ID that should be used. **Must be an odd number.**"* | **아니오** | 조회 후 전달로 수정 |

**결함의 형태는 "두 DCMTK API 의 계약이 다른데 호출부가 같은 값을 썼다" 이다.**
C-STORE 가 `0` 으로 동작하는 것을 보고 C-FIND 도 되리라 가정하면 정확히 이 결함이 나온다.

### 2.4 GREEN (C3, C5) — `_green.log`

```
===BUILD=0===
100% tests passed, 0 tests failed out of 131
123/131 Test #123: DicomNetworkTest.CFindResults_ReturnsJsonArray ...   Passed    1.58 sec
124/131 Test #124: DicomNetworkTest.CFindEmpty_ReturnsEmptyArray ....   Passed    0.57 sec
```

skip **8 → 6** (남은 6 = DegradedMode 5 + Cancel 1).

수정 직후 첫 실행에서 SCU 가 실제 결과를 돌려주는 것을 확인했다:
```
outJson = "[{\"AccessionNumber\":\"ACC-0001\",\"Modality\":\"DX\",
             \"PatientID\":\"MOCK-0001\",\"PatientName\":\"MOCK^WORKLIST\"}]"
```
**MWL 조회가 처음으로 성립한 순간이다** — 요청 전송 → 응답 수신 → JSON 변환까지.

### 2.5 mock 보강 — 테스트 단언은 바꾸지 않았다

첫 GREEN 시도에서 2건이 여전히 실패했는데, 이번엔 **제품이 아니라 mock 이 원인**이었다:

| 테스트 | 단언 | 첫 결과 |
|---|---|---|
| `CFindResults_ReturnsJsonArray` | 항목 3건 | 1건 (mock 이 고정 1건만 응답) |
| `CFindEmpty_ReturnsEmptyArray` | `"[]"` | 1건 (mock 이 질의를 무시) |

mock 을 기대에 맞췄다: DX 조회에 3건, `PatientID` 가 지정되면 그 항목만, 워크리스트에 없는
ID 면 0건. 매칭은 `PatientID` 하나만 본다 — 빈 결과 케이스에 필요한 최소한이다.

**기존 테스트의 단언은 한 글자도 바꾸지 않았다.** 3건과 `"[]"` 는 원래 테스트가 요구하던
값이고, mock 이 그 요구를 충족하지 못하고 있었다.

### 2.6 무회귀 (C6) — `_verify.log`, 필터 없음

```
===CI_POST===    100% tests passed, 0 tests failed out of 445   ===POST_EXIT=0===
===CI_AI===      100% tests passed, 0 tests failed out of 192   ===AI_EXIT=0===
===CI_DICOM===   100% tests passed, 0 tests failed out of 131   ===DICOM_EXIT=0===
```

---

## 3. Baseline 귀속 (Baseline-attribution)

| 항목 | B-30 | B-32 | 차 |
|---|---:|---:|---:|
| ci-dicom 전체 | 131 | 131 | 0 (케이스 추가 없음 — 스킵을 실행으로 바꾼 것) |
| ci-dicom skip | 8 | **6** | **−2** |
| ci-post / ci-ai | 445 / 192 | 445 / 192 | 0 |

B-30 수치는 `.moai/reports/lane-post/QA-B-30/_verify.log`, 이번 수치는 §2.6 의 이번 실행이다.

---

## 4. 미검증 (Gaps)

- **실제 PACS 상대로는 검증하지 않았다.** GREEN 은 이 저장소의 in-process mock 상대다.
  상용 MWL 서버가 컨텍스트를 다르게 협상하면 `findPresentationContextID` 가 0 을 돌려줄 수
  있고, 그 경로(새로 추가한 `NETWORK_FAILED` 반환)는 **테스트로 실행된 적이 없다.**
- **`findPresentationContextID` 의 세 번째 인자(role)를 기본값으로 두었다.** 역할을 명시해야
  하는 상황이 있는지 확인하지 않았다. 현재 협상은 양쪽 DEFAULT 라 문제가 없다(B-30 §2.4).
- **전송 구문을 빈 문자열로 조회한다** — "아무거나 수락된 것" 을 뜻한다. 특정 TS 를 강제해야
  하는 요구가 있는지 api-spec 에서 확인하지 않았다.
- **`cfindMwl` 의 질의 변환(`buildFindRequest`)은 이 카드에서 손대지 않았다.** mock 이
  `PatientID` 만 보고 매칭하므로, `Modality` 등 다른 키가 SCU→SCP 로 제대로 전달되는지는
  **이 테스트가 검증하지 않는다.**
- **`xpe_dicom_cfind_mwl` 의 버퍼 부족 경로**(`outJson` 이 작을 때)는 여전히 미검증이다.

---

## 5. 잔여 위험 (Residual-risk)

- **mock 의 워크리스트가 이제 테스트 기대에 맞춰져 있다.** 테스트가 mock 을 규정하고 mock 이
  테스트를 통과시키는 구조라, 두 쪽이 함께 틀리면 드러나지 않는다. 실제 PACS 상호운용은
  이 스위트가 보증하지 않는다 — §4 첫 항목.
- **`PatientID` 만으로 매칭한다.** `CFindEmpty` 가 통과하는 것은 그 키를 보기 때문이며,
  다른 키로 빈 결과를 기대하는 테스트가 생기면 mock 을 다시 손봐야 한다.
- **새 실패 경로(`findPresID == 0`)가 미실행이다.** 컴파일만 됐다 — B-20 판정문의
  "발화한 적 없는 가드는 판정 근거가 아니다" 가 이 줄에 그대로 적용된다.
- **Cancel 스킵 1건은 그대로다.** `DicomNetworkSCU.cpp:119-121`(association 이후 취소)은
  여전히 미실행이며, B-29 §2.4 의 사유(로컬 전송이 너무 빨라 경쟁 단언이 된다)가 유효하다.
- 커밋은 origin/main 병합 전까지 미푸시다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` | vcvars + CMake/Ninja PATH |
| `_dicom.bat` | ci-dicom 빌드 + 전체 ctest |
| `_red.log` | 스킵 제거 직후 — 2건 실패 |
| `_green.log` | 수정 후 — 131/131, skip 6 |
| `_verify.log` | 재실측 445 / 192 / 131 |
