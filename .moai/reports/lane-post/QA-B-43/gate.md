# QA-B-43 게이트 보고서 — 계약 이전 동작을 기록한 단언 전수 + 4바이트 가드 반증

**카드**: QA-B-43 (#142, B-42 신호·Gaps)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-43/`
**커밋 2건**: `6c5cca4` 이름 정정·계약 수정 · `daa6740` Gaps 해소
**선행**: `git merge origin/main` — Already up to date (B-42 는 로컬 병합 상태)

---

## 1. 판별 기준 (한 줄)

> **이 단언이 깨졌을 때 그것이 결함인가, 아니면 단지 구현이 달라진 것인가.**
> 전자면 **요구**, 후자면 **기록**이다.

"헤더에 문장이 있는가" 를 기준으로 쓰지 않은 이유: B-39 가 구현을 읽어 헤더에
받아 적은 문장이 여러 개 있다. 그것을 근거로 삼으면 **구현이 구현을 정당화하는
순환**이 된다. 위 기준은 그 순환을 피한다 — 문장의 출처가 아니라 **깨졌을 때의
의미**를 묻는다.

---

## 2. 전수 표 — post 소유 모듈의 `PROCESSING_FAILED` / stub 상수 단언

`modules/preprocess` 는 Lane A 소유라 제외했다(같은 grep 에 3파일이 잡히지만
건드리지 않았다).

| # | 위치 | 단언 | 근거 문장 | 판정 | 조치 |
|---|---|---|---|---|---|
| 1 | `test_exposure_index.cpp:128` `ZeroMeanImage_ReturnsProcessingFailed` | mean<=0 → PROCESSING_FAILED | **있음** — REQ-ENH-030 (헤더 `@return` 에도 명시) | **요구** | 유지 |
| 2 | `test_ai_ipc_bridge.cpp:154, :273` 타임아웃 → PROCESSING_FAILED | 타임아웃 | **있음** — `ai_ipc_bridge.h:45, :71` 의 `@return` | **요구**(내부 계약) | 유지 |
| 3 | `test_ai_fallback.cpp:206` `BodypartRecognizeNullConfOutIsAcceptable` | `confidenceOut` NULL 은 INVALID 아님 | **있음** — 헤더 "May be NULL" | **요구** | 유지 |
| 4 | `test_ai_fallback.cpp:505, :511` `DataSizeGuard_*_ZeroAccepted` | `dataSize==0` 수용 | **있음** — api-spec #123 | **요구** | 유지 |
| 5 | `test_dicom_network_scu.cpp:192` `CancelCStore_TerminatesOperation` | 취소 → PROCESSING_FAILED | **있음** — REQ-DICOM-039..040 + 헤더 | **요구** | 유지(Skipped 상태) |
| 6 | `test_ai_fallback.cpp:92,127,139,148` `*StubReturnsProcessingFailed` (4건) | stub 는 PROCESSING_FAILED | 값은 REQ-AI-002, **"무조건" 은 근거 없음** | **기록** | **이름이 이미 `Stub` 로 드러냄 → 유지** |
| 7 | `test_ai_worker_isolation.cpp:71,85,96,105,266` `StubMode*` (5건) | 동일 | 동일 | **기록** | **이름이 이미 `StubMode` → 유지** |
| 8 | `test_dicom_network_scu.cpp:288` `CFindMalformedQueryJson_ReturnsProcessingFailed` | 잘못된 JSON → PROCESSING_FAILED | **없음** — B-39 가 구현을 읽어 헤더에 적은 것이 전부 | **기록** | **`KnownDivergence_` 개명** |
| 9 | `test_ai_fallback.cpp:118` `BodypartRecognizeSetsUnknownLabel` | 라벨 == `"UNKNOWN"` | **없음** — stub 상수 | **기록** | **개명** |
| 10 | `test_ai_worker_isolation.cpp:123` `RepeatedFallbackIsConsistent` | 라벨·신뢰도·반환코드 전부 못박음 | **없음**(일관성만 요구) | **기록** | **개명** |
| 11 | `test_ai_model_card.cpp:102` `GetModelCardContainsModelVersion` | 버전에 `"stub"` 포함 | **없음** — REQ-AI-008 은 "버전이 있을 것" 까지 | **기록** | **개명** |
| 12 | `test_ai_fallback.cpp:566` Endurance 루프 | 사이클마다 PROCESSING_FAILED | **없음** | **기록** | **계약대로 수정(a)** |
| 13 | `test_lifecycle_ext.cpp:92`, `test_header_guards.cpp:53` 버전 `"1.0.0"` | 버전 문자열 | **있음** — `XPE_ENHANCE_ADVANCED_VERSION` 이 프로젝트 사실 | **요구** | 유지 |

**13항목 중 요구 6 · 기록 7.** 기록 7 중 **9건(6·7번)은 이름이 이미 `Stub`/`StubMode` 로
드러내고 있어 바꾸지 않았다** — `KnownDivergence_` 보다 구체적으로 같은 것을 말한다.
남은 4건을 개명하고 1건을 고쳤다.

### 2.1 왜 이름인가

ctest 목록에는 **이름만 보인다.** 주석은 파일을 열어야 읽힌다.
요구가 아닌 것이 요구처럼 읽히는 지점이 바로 목록이므로, 구분은 이름이 져야 한다.

---

## 3. 조치 (`6c5cca4`)

### 3.1 개명 4건 — **동작 변경 0**

| 이전 | 이후 | 한 줄 근거 |
|---|---|---|
| `CFindMalformedQueryJson_ReturnsProcessingFailed` | `KnownDivergence_CFindMalformedQueryJson_ReturnsProcessingFailed` | 그 코드는 `buildFindRequest` 실패가 깊은 곳에서 새어 나온 값이고, `INVALID_INPUT` 도 똑같이 defensible 하다 — 둘을 가르는 문장이 없다 |
| `BodypartRecognizeSetsUnknownLabel` | `KnownDivergence_BodypartRecognizeStubSetsUnknownLabel` | `"UNKNOWN"` 은 stub 상수다. ONNX 빌드는 실제 라벨을 쓴다 |
| `RepeatedFallbackIsConsistent` | `KnownDivergence_RepeatedStubFallbackIsConsistent` | 요구는 "반복 호출이 일관되다" 이고 그것은 남는다. 라벨·신뢰도 값까지 못박은 부분이 기록이다 |
| `GetModelCardContainsModelVersion` | `KnownDivergence_GetModelCardStubVersionSaysStub` | REQ-AI-008 은 버전이 있을 것을 요구한다. `"stub"` 부분 문자열은 빌드 산물이다 |

### 3.2 계약대로 고친 1건 — Endurance 루프

워킹셋을 재는 케이스가 `PROCESSING_FAILED` 를 못박고 있었다.
**ONNX 빌드가 `XPE_OK` 를 내면 메모리와 무관한 이유로 메모리 테스트가 깨진다.**
루프에 필요한 것은 "호출이 받아들여졌다" 이므로 `EXPECT_NE(INVALID_INPUT)` 로 바꿨다.
개명이 아니라 수정을 고른 이유: 이름을 바꿔도 케이스의 목적(메모리)과 단언(반환값)이
어긋난 사실은 남는다.

---

## 4. B-42 Gaps 해소 (`daa6740`)

### 4.1 4바이트 가드 반증 (`_falsify_4byte.log`)

가드를 약화시키자 **4건 중 정확히 1건** —
`OutputBufferTooSmallForSizeReport_DoesNotOverflow` — 만 실패했고, 메시지가
`byte 2 written past a 2-byte buffer` 로 B-42 RED 와 같았다.
**가드가 그 케이스를 실제로 지탱한다는 증거다.**

**첫 시도를 증거로 쓰지 않았다.** 가드를 통째로 지웠더니 `bufLen` 이 미사용이 되어
`/W4 /WX` 로 빌드가 깨졌다(`BUILD=1`). 그 상태에서 ctest 는 **낡은 바이너리로 4/4
통과**를 찍었다 — 빌드 실패 뒤의 통과는 아무것도 증명하지 않는다.
`bufLen` 을 쓰는 형태(`bufLen < 1u`)로 약화시켜 다시 쟀다.
**B-37 의 "반증 뒤 재빌드" 규약이 여기서 한 단계 더 필요해진 사례**다 —
재빌드뿐 아니라 **빌드 성공 여부까지 읽어야 한다.**

### 4.2 stub 임계를 눈에 보이게

`KnownDivergence_LabelBufferThresholdFollowsStubLabelLength` 가 경계 양쪽을 못박는다:

| 버퍼 | 결과 |
|---|---|
| 7바이트 (`sizeof("UNKNOWN") - 1`) | `BUFFER_TOO_SMALL` |
| 8바이트 (정확히) | 통과 + 라벨이 **잘리지 않음** |

라벨이 길어지면 **호출자가 아니라 여기서 먼저 드러난다.**
숫자 8 은 요구가 아니라 stub 의 기록이므로 `KnownDivergence_` 다.
ONNX 배선은 하지 않았다(#130, 사용자 결정 대기).

### 4.3 도달 불가 경로 — 테스트를 만들지 않았다

`stitch_images` / `bone_suppress` 의 **"출력 버퍼가 존재하나 부족"** 경로는
stub 이 추론 전에 `PROCESSING_FAILED` 로 빠져 **도달하지 않는다.**
케이스를 만들면 통과는 하지만 그 분기를 지나지 않는다 —
**통과하지 않는 테스트를 통과처럼 세지 않기 위해** 만들지 않고 여기에 남긴다.
ONNX 경로가 배선된 뒤에 의미가 생긴다.

---

## 5. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 이전 ctest | 465 / 208 / 154 | QA-B-42 `_verify.log` |
| 개명 직후 | 465 / 208 / 154 (**불변**) | `_rename.log` — 이름만 바뀌었다 |
| 최종 | **465 / 209 / 154** | `_verify.log` — 임계 케이스 1건 추가 |
| 4바이트 반증 | 4건 중 1건 실패 | `_falsify_4byte.log` |
| 임계 케이스 | 1/1 | `_threshold.log` |
| 빌드 경고 | 0 | `grep -c "warning C" _verify.log` |

---

## 6. 미검증 (Gaps)

- **전수 범위는 `PROCESSING_FAILED` 와 stub 상수 두 신호로 한정했다.** 카드가 그
  세 후보 신호를 제시했고 그중 "헤더에 근거 없는 반환값" 은 §1 의 이유로 다른
  기준으로 대체했다. **다른 종류의 기록형 단언**(예: 특정 float 허용오차, 타이밍
  임계)은 훑지 않았다.
- **개명 4건이 CI 아티팩트 이름에 미치는 영향을 확인하지 않았다.** ctest 이름이
  바뀌므로 이름으로 필터하는 외부 스크립트가 있으면 영향을 받는다. 저장소 안에는
  그런 필터가 없다(`-R` 사용처는 이 레인의 증거 스크립트뿐).
- **6·7번(9건) 을 유지한 판단은 반증하지 않았다.** "`Stub` 이 `KnownDivergence_` 만큼
  드러낸다" 는 판단이고, leader 가 달리 보면 개명은 기계적이다.
- **`CancelCStore_TerminatesOperation` 은 여전히 Skipped 다.** 요구로 분류했지만
  실행되지 않으므로 그 요구가 지켜지는지는 이 카드도 확인하지 못했다.
- **api-spec 과 대조하지 않았다.** `docs/` 는 main 소유이고, 출력 버퍼 계약 기록은
  leader 가 맡았다.

---

## 7. 잔여 위험 (Residual-risk)

- **`KnownDivergence_` 는 규율이지 강제가 아니다.** 다음 사람이 접두어 없는
  기록형 단언을 새로 넣으면 아무것도 막지 않는다. 이름 규칙을 CI 로 검사하는
  방법은 이 카드 범위 밖이다.
- **개명은 이름만 바꾼다 — 단언은 그대로 stub 값을 못박고 있다.** ONNX 가 배선되면
  이 케이스들은 여전히 실패한다. 다만 **실패했을 때 "결함이 아니라 기록이 낡은 것"
  임을 이름이 말해 준다** — 그것이 이 카드가 산 것이다.
- **임계 케이스가 stub 라벨을 두 번 적는다**(구현과 테스트). 구현이 라벨을 바꾸면
  테스트가 먼저 깨지도록 의도한 것이지만, 공유 상수가 아니므로 **두 곳을 함께
  고쳐야 한다.**
- 커밋 2건은 push 전까지 미푸시다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` | 환경 + 세 프리셋 재빌드·전체 ctest |
| `_rename.log` | 개명 직후 465 / 208 / 154 (개수 불변) |
| `_falsify_4byte.log` | 가드 약화 시 4건 중 1건 실패 |
| `_threshold.log` | stub 임계 케이스 1/1 |
| `_verify.log` | 최종 465 / 209 / 154, 경고 0 |
