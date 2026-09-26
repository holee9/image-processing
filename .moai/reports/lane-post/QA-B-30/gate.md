# QA-B-30 게이트 보고서 — C-FIND(MWL) 협상 실패 원인 특정

**카드**: QA-B-30 (#124 #120)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-30/`
**선행 병합**: `git merge origin/main` → `dd9cc90`
**시도 상한**: 관측 3종 — **3종 안에서 원인이 특정됐다.**

---

## 1. 주장 (Claim)

| # | 주장 | 상태 |
|---|------|------|
| C1 | 원인을 특정했다 — **mock 이 아니라 SCU 제품 코드에 있다** | PASS |
| C2 | 근거는 DCMTK 계약 문서 + 관측 로그다 (추론 아님) | PASS |
| C3 | mock 은 MWL 컨텍스트를 **정상 수락**한다 | PASS |
| C4 | 제품 코드는 **고치지 않았다** — 카드 지시대로 보고만 | PASS |
| C5 | 관측 3종 안에서 끝냈다. 실험 누적 없음 | PASS |
| C6 | 재실측 ci-post 445 / ci-ai 192 / ci-dicom 131 전부 통과 | PASS |

---

## 2. 증거 (Evidence)

### 2.1 결론 — 원인

**`modules/dicom/src/DicomNetworkSCU.cpp:226`**

```cpp
cond = scu.sendFINDRequest(0, &requestDS, &responses);
```

DCMTK 의 계약 (`dcmtk/dcmnet/scu.h:461-462`, `DcmSCU::sendFINDRequest` 문서):

```
 *  @param presID    [in]  The presentation context ID that should be used. Must be an odd
 *                         number.
```

**`presID` 는 홀수여야 한다.** `0` 은 홀수가 아니고, 유효한 컨텍스트 ID 도 아니다
(DICOM 의 presentation context ID 는 1 부터 시작하는 홀수다). 그래서 SCU 가
`DIMSE No valid Presentation Context ID` 를 낸다.

**대조가 이 진단을 확정한다.** 같은 파일 `:127-132` 의 C-STORE 는 같은 `0` 을 넘기는데
성공한다 — `DcmSCU::sendSTORERequest` 는 계약이 다르기 때문이다(소스 주석도
`0 = auto-select` 라고 적고 있고, 실제로 동작한다). **두 API 의 계약이 다른데 호출 쪽은
같은 값을 쓴다.** C-STORE 가 되니 C-FIND 도 될 것이라는 가정이 성립하지 않는 지점이다.

### 2.2 관측 1 — mock 쪽 협상 결과

`mock_scp.cpp` 에서 `getConfig().setVerbosePCMode(OFTrue)` 를 켜 DCMTK 가 스스로
컨텍스트별 수락/거절을 찍게 했다 (`_negotiation.log`):

```
I: Presentation Contexts:
I:   Context ID:        1 (Accepted)
I:     Abstract Syntax: =FINDModalityWorklistInformationModel
I:     Accepted SCP/SCU Role: Default
I:     Accepted Transfer Syntax: =LittleEndianExplicit
I: Accepted Extended Negotiation:  none
I: Association Accepted (Max Send PDV: 16372)
[warning] [DicomNetworkSCU] sendFINDRequest failed: DIMSE No valid Presentation Context ID
```

**MWL 컨텍스트는 ID 1 로 수락됐고 association 도 수락됐다.** 그런데 SCU 가 "유효한
컨텍스트 ID 가 없다" 고 한다 — 수락된 ID 가 1(홀수)인데도. SCP 쪽에는 문제가 없다는 것이
이 로그의 결론이며, 시선을 SCU 호출부로 돌린 근거다.

C-STORE 접속에서는 `handleIncomingCommand` 가 실제로 불린다:
```
handleIncomingCommand: CommandField=0x0001 presID=1 as=1.2.840.10008.5.1.4.1.1.1.1 ts=1.2.840.10008.1.2.1
```
C-FIND 접속에서는 이 줄이 **한 번도 나오지 않는다** — SCU 가 아예 보내지 않았다는 뜻이다.

### 2.3 관측 2 — SCU 가 실제로 제안한 abstract syntax

`notifyAssociationRequest` 오버라이드로 요청 자체를 기록했다(소스 판독이 아니라 전선 위의 값):

```
--- association request ---
proposed presentation contexts: 1
  PC id=1 as=1.2.840.10008.5.1.4.31 role=1 ts=1.2.840.10008.1.2.1,1.2.840.10008.1.2
negotiateAssociation -> OK
```

**제안된 abstract syntax 는 정확히 MWL** (`1.2.840.10008.5.1.4.31`) 이다. 카드가 의심한
"다른 UID(예: Study Root Q/R)를 제안하고 있을 가능성" 은 **반증됐다.**

### 2.4 관측 3 — 역할(role) 협상

같은 로그의 `role=` 필드를 두 서비스에서 비교했다:

| 서비스 | abstract syntax | proposedRole | 결과 |
|---|---|---|---|
| C-FIND | `1.2.840.10008.5.1.4.31` | **1** | 수락(Default), 그러나 요청 미도착 |
| C-STORE | `1.2.840.10008.5.1.4.1.1.1.1` | **1** | 수락, 요청 도착·처리 |

`T_ASC_SC_ROLE` 은 `NONE=0, DEFAULT=1, SCU=2, SCP=3` (`assoc.h:167-171`).
**둘 다 `1` = DEFAULT 로 동일하다 → 역할은 원인이 아니다.** 가설이 하나 더 지워졌다.

### 2.5 지워진 가설 정리

| 가설 | 출처 | 판정 |
|---|---|---|
| 전송 구문 불일치 | B-29 §4.1 | **반증** (교집합 비어 있지 않음) |
| 다른 abstract syntax 제안 | 카드 관측 2 | **반증** (§2.3, 정확히 MWL) |
| 역할 협상 불일치 | 카드 관측 3 | **반증** (§2.4, 양쪽 DEFAULT) |
| mock 이 컨텍스트를 거절 | 카드 관측 1 | **반증** (§2.2, Accepted) |
| **SCU 호출부의 presID 계약 위반** | §2.1 | **확정** |

### 2.6 남긴 진단 훅 (카드가 허용한 1커밋)

| 위치 | 내용 |
|---|---|
| `mock_scp.hpp/cpp` `notifyAssociationRequest` | 제안된 PC 목록(id / abstract syntax / role / TS) 기록 |
| 같은 파일 `negotiateAssociation` | 협상 반환 코드 기록 |
| `MockScp::handleIncomingCommand` | 도달한 명령과 그 컨텍스트 기록 — **부재가 신호다** |
| `MockScp::negotiationLog` | 위 셋이 누적되는 공개 문자열. 테스트가 읽을 수 있다 |

`setVerbosePCMode` 는 **기본 꺼둠**으로 되돌렸다(모든 association 마다 출력되어 로그가
시끄럽다). 다시 진단할 때 켜라는 주석을 그 자리에 남겼다.

`DcmSCP::m_assoc` 는 private 이라 협상 **후** 컨텍스트 목록을 서브클래스에서 직접 읽을 수
없다. 그래서 관측 1 은 DCMTK 자체 로깅으로 대체했다 — 결과적으로 더 신뢰할 만한 출처다.

### 2.7 스킵 사유 갱신

`CFindResults_ReturnsJsonArray` / `CFindEmpty_ReturnsEmptyArray` 의 스킵 사유를
"mock 이 협상하지 못한다" 에서 사실로 바꿨다:

```
blocked in product code, not in this mock: DicomNetworkSCU.cpp:226 passes
presID 0 to DcmSCU::sendFINDRequest, whose contract requires an odd
presentation context ID. The mock accepts the MWL context (DCMTK logs
'Context ID: 1 (Accepted)'). See the QA-B-30 report
```

**B-29 의 사유는 mock 을 탓하고 있었고, 그것이 틀렸다.** 이 카드가 바로잡는다.

### 2.8 재실측 (C6) — `_verify.log`, 필터 없음

```
===CI_POST===    100% tests passed, 0 tests failed out of 445   ===POST_EXIT=0===
===CI_AI===      100% tests passed, 0 tests failed out of 192   ===AI_EXIT=0===
===CI_DICOM===   100% tests passed, 0 tests failed out of 131   ===DICOM_EXIT=0===
```

---

## 3. Baseline 귀속 (Baseline-attribution)

| 항목 | B-29 | B-30 | 차 |
|---|---:|---:|---:|
| ci-dicom 전체 | 121 | 131 | **+10 — 전부 내 몫이 아니다** |
| ci-dicom skip | 8 | 8 | 0 |
| ci-post / ci-ai | 445 / 192 | 445 / 192 | 0 |

**+10 은 이번 병합(`dd9cc90`)이 들여온 Lane A 테스트다.** 두 `_verify.log` 의 테스트 이름
집합을 비교해 확인했다:

```
$ comm -13 b29.txt b30.txt
AlertQueueOverflowTest.* (6)     ← Lane A
XpeCommonTest.CopyImageReproducesEveryByte
XpeCommonTest.ErrorStringForInvalidInputHasExactText
XpeErrorSafetyViolationTest.* (2)
```

**이 카드가 추가한 테스트는 0건이다** — 진단 카드이므로 정상이다.

---

## 4. 미검증 (Gaps)

- **수정안을 검증하지 않았다.** `sendFINDRequest` 에 홀수 presID(예: 협상된 컨텍스트 ID)를
  넘기면 통과하는지 **시험하지 않았다.** 제품 코드를 건드리지 말라는 카드 지시 때문이다.
  따라서 "presID 0 이 원인" 은 계약 문서 + 관측에 근거한 진단이지, **고쳐서 재현한 것은 아니다.**
- **올바른 presID 를 어떻게 얻는지는 조사하지 않았다.** DcmSCU 에 협상된 컨텍스트 ID 를
  돌려주는 접근자가 있는지(예: `findPresentationContextID`) 확인하지 않았다 — 수정 카드의 몫.
- **C-STORE 가 `0` 으로 되는 이유를 DCMTK 소스로 확인하지 않았다.** 헤더 문서와 실제 동작이
  일치한다는 것까지만 관측했다.
- **Cancel 스킵은 이 카드 대상이 아니다.** `DicomNetworkSCU.cpp:119-121` 은 여전히 미실행이다.
- **`m_assoc` 비공개로 인해 협상 후 상태를 코드로 단언하지 못한다** — DCMTK 로그를 읽는
  형태이므로, 로그 포맷이 바뀌면 이 관측 방법은 깨진다.

---

## 5. 잔여 위험 (Residual-risk)

- **진단 훅이 상시 켜져 있다.** `negotiationLog` 는 association 마다 문자열이 누적된다.
  테스트 프로세스 수명 동안이라 실질 위험은 낮지만, 장기 실행 SCP 였다면 무한 증가다.
  테스트 전용 코드임을 주석으로 못 박아 두었다.
- **`setVerbosePCMode` 를 다시 켜면 CI 로그가 크게 늘어난다.** 기본 꺼둠으로 되돌렸다.
- **이 진단은 이 DCMTK 버전의 계약에 근거한다.** `sendFINDRequest` 의 presID 요구가
  버전에 따라 달라지면 결론도 재확인이 필요하다.
- **제품 코드 수정은 계약 확인이 선행이다.** `xpe_dicom_cfind_mwl` 이 "MWL 조회가 된다" 고
  약속하는 문서가 있다면 이것은 기능 결함이고, 없다면 미구현이다 — 그 판단은 leader 몫이며
  이 레인은 근거만 제출한다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` | vcvars + CMake/Ninja PATH |
| `_dicom.bat` / `_after.log` | ci-dicom 빌드 + 전체 ctest 131/131, skip 8 |
| `_negotiation.log` | 관측 3종 원본 — 제안 PC 덤프, 협상 결과, DCMTK 수락 로그 |
| `_verify.bat` / `_verify.log` | 재실측 445 / 192 / 131 |
