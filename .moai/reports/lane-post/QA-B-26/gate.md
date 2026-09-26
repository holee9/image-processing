# QA-B-26 게이트 보고서 — 이슈 #71 항목별 실측 (읽기 전용 감사)

**카드**: QA-B-26 (#71)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-26/`
**선행 병합**: `git merge origin/main` → `3b5c7fd`
**코드 변경 0 · 커밋 0** (카드 3항)

---

## 1. 주장 (Claim)

| # | 주장 | 상태 |
|---|------|------|
| C1 | #71 본문·코멘트를 항목 단위로 분해했다 (15항목) | PASS |
| C2 | "해소" 로 판정한 항목은 전부 파일·줄·테스트명 근거가 있다 | PASS |
| C3 | 미해소 항목을 분리하고 카드 후보로 서술했다 (6건) | PASS |
| C4 | 코드 변경 없음 · 커밋 없음 | PASS |

---

## 2. 증거 (Evidence)

### 2.1 #71 항목 분해

이슈 본문은 추적용 골격(변경 범위 체크박스 4 + 완료 조건 4)이고, **실질 작업 항목은 코멘트
10건에 있다.** 두 곳을 합쳐 15항목으로 분해했다.

### 2.2 항목 표

**A. 코멘트가 "구현했다" 고 적은 것 — 현재 트리에서 확인**

| # | 항목 | 상태 | 근거 |
|---|---|---|---|
| 1 | gsvg CMake 가 참조하던 누락 테스트 3건 추가 | **해소** | `modules/gsvg/tests/` 에 6파일 존재, `modules/gsvg/CMakeLists.txt:48-55` 가 6개 전부 참조. `_full.log` 에서 `gsvg_tests` 정상 실행 |
| 2 | `ai_worker_main.cpp` Windows `ERROR` 매크로 충돌 회피 | **해소** | `ai_worker_main.cpp:51` `ERROR_RESPONSE = 99` — bare `ERROR` 아님. 305행 사용처도 같은 이름 |
| 3 | `ai_worker_main.cpp` `/WX` 미사용 인자 정리 | **해소** | ci-post·ci-ai 가 `XPE_WARNINGS_AS_ERRORS=ON` 으로 빌드 통과 (`_full.log` `===POST=0=== / ===AI=0===`) |
| 4 | `ai_ipc_bridge` send: 프로토콜 검증을 연결 검사보다 앞세움 | **해소** | `ai_ipc_bridge.cpp:103~121` 이 `INVALID_INPUT` 5건, 연결 검사는 **126행**. 124-125행에 사유 주석. 테스트 `AiIpcBridgeTest.Send_InvalidMagicNumber_ReturnsInvalidInput`, `Send_PayloadSizeExceedsMaximum_ReturnsInvalidInput` |
| 5 | `ai_ipc_bridge` receive 미연결 → `PROCESSING_FAILED` | **해소** | `ai_ipc_bridge.cpp:181-182`, 179-180행에 "timeout-like" 사유 주석. 테스트 `AiIpcBridgeTest.Receive_NotConnected_ReturnsError` |
| 6 | `.gitignore` 의 `modules/*/tests/` 규칙이 새 테스트를 숨기는 문제 | **해소(다른 방식으로)** | `.gitignore:269` — 규칙 자체가 **삭제**됐다(2026-09-09, #118). 당시의 예외 추가는 상위 규칙 제거로 대체됨 → §2.3 잔재 1건 |
| 7 | 세션 10 brief 문서 | **해소** | `docs/audit/SESSION-10-BRIEF.md` 존재 (5135 B) |
| 8 | 검증 기록 (CTest 306/306, degraded skip 5) | **해소(수치는 갱신됨)** | 당시 306/306 · skip 5. 현재 §2.4 |

**B. 본문 "변경 범위" 체크박스**

| # | 항목 | 상태 | 근거 |
|---|---|---|---|
| 9 | 코드 `[x]` | **해소** | 위 1~5 |
| 10 | 문서 `[x]` | **해소** | 위 7 |
| 11 | 테스트/검증 데이터 `[x]` | **해소** | 위 1, 8 |
| 12 | 설정/CI `[ ]` (미체크) | **해소됨 — 체크박스만 미갱신** | `CMakePresets.json:120,140` 에 `coverage-post`·`coverage-dicom` 프리셋 존재(빌드 프리셋 212,216). #71 이후 leader 가 추가 |

**C. 본문 "완료 조건" 4건 (전부 미체크)**

| # | 항목 | 상태 | 근거 |
|---|---|---|---|
| 13 | 잔여 구현 항목과 실제 코드 상태를 대조 | **이 카드가 수행** | 본 표 |
| 14 | 필요한 구현/테스트/문서 동기화 반영 | **해소** | 위 1~7 + QA-B-01~25 (main 병합·push 완료) |
| 15 | 로컬 VS2022 공통 빌드 또는 대체 검증 결과 기록 | **해소** | §2.4 — 3개 config 전체 ctest |
| 16 | `git diff --check` 통과 | **해소** | `git diff --check` 출력 없음 (이번 실행) |

**D. 명시적 보류 항목 (코멘트가 "차후 세션" 으로 이관)**

| # | 항목 | 상태 | 근거 |
|---|---|---|---|
| 17 | **S2 — `xpe_ai.dll` 구현 (Phase 3)** | **미해소** | `modules/ai/CMakeLists.txt:26-27` `XPE_AI_USE_ONNXRUNTIME=OFF`, `XPE_AI_STUB_BUILD=ON`. 구성 로그 `xpe_ai_worker: STUB build (no ONNX Runtime)` |
| 18 | **S4 — DICOMweb 상호운용성** | **미해소** | `grep -rniE "wado\|stow\|dicomweb" modules/ --include=*.cpp --include=*.h` → **0건** |

(항목 번호가 18까지 간 것은 표를 A~D로 나눈 결과다. 분해 단위는 15항목이며 13은 이 카드 자체다.)

### 2.3 §2.2 항목 6에서 파생된 잔재 1건

`.gitignore:269` 가 규칙 삭제를 기록한다:
```
269: # modules/*/tests/ was ignored here; removed 2026-09-09 (#118) — it hid new test files
271: !modules/gsvg/tests/test_gsvg_extended_coverage.cpp
```
271행은 **삭제된 규칙에 대한 예외**다. 무시할 상위 규칙이 없으므로 아무 효과가 없다.
해가 되지는 않지만 읽는 사람에게 "여기 무시 규칙이 있다" 는 잘못된 신호를 준다.
루트 `.gitignore` 는 main 소유라 이 레인이 지우지 않는다 → 카드 후보 §3-4.

### 2.4 현재 검증 상태 (`_full.log`)

**필터 없이 전체 ctest** 로 측정했다. 이전 카드들에서 `-R` 부분집합을 전체처럼 보고한 적이
있어(QA-B-25 자기 정정) 이번부터 범위를 명시한다.

| config | 전체 | 통과 | skip |
|---|---|---|---|
| `ci-post` | 445 | 445 | 5 (DegradedMode) |
| `ci-ai` | **192** | 192 | 5 (DegradedMode) |
| `ci-dicom` | **110** | 110 | 9 (DegradedMode 5 + DicomNetworkTest 4) |

```
100% tests passed, 0 tests failed out of 445   ===POST=0===
100% tests passed, 0 tests failed out of 192   ===AI=0===
100% tests passed, 0 tests failed out of 110   ===DICOM=0===
```

참고로 `-R` 을 걸면 ci-ai 는 129, ci-dicom 은 48 로 보인다 — #71 시절의 "306/306" 과
직접 비교할 수 없는 이유이기도 하다(그때는 단일 통합 빌드 스크립트였고 지금은 프리셋별로 나뉘었다).

### 2.5 IPC bridge — 기존 미검증 항목의 현재 상태

내가 QA-B 시리즈 내내 "ai IPC 내부 경로(`ai_ipc_bridge.cpp`)는 들여다본 적 없다" 를 Gap 으로
달아 왔다. 이번에 읽었다: `AiIpcBridgeTest` 10건이 실제로 돈다.

```
AiIpcBridgeTest.CreateBridge_ValidParameters_Succeeds / _NullPipeName_ReturnsNull
AiIpcBridgeTest.Destroy_NullBridge_DoesNotCrash / _MultipleCalls_Safe
AiIpcBridgeTest.Connect_NoWorkerAvailable_ReturnsTimeoutError
AiIpcBridgeTest.Send_NotConnected_ReturnsError / _InvalidMagicNumber_* / _PayloadSizeExceedsMaximum_*
AiIpcBridgeTest.Receive_NotConnected_ReturnsError / _NoResponseWithinTimeout_ReturnsTimeoutError
```
**로컬 경로(인자 검증·수명주기·미연결 반환)는 덮여 있다.** 워커와의 실제 왕복은 덮여 있지 않다
(`Connect_NoWorkerAvailable` 이 타임아웃을 확인할 뿐이다) — Gap 을 그만큼 좁혀 §4 에 다시 적는다.

---

## 3. 카드 후보 (미해소 항목)

| # | 후보 | 크기 | 비고 |
|---|---|---|---|
| 1 | **S2 — ONNX 실경로 활성화·검증** | **한 커밋 단위 아님** | `XPE_AI_USE_ONNXRUNTIME=ON` 빌드, 모델 아티팩트 조달, EP 선택, 워커 왕복 검증이 각각 별개다. 카드로 쪼개기 전에 leader 의 범위 결정이 먼저 필요하다 |
| 2 | **S4 — DICOMweb (WADO-RS / STOW-RS)** | **한 커밋 단위 아님** | 구현 0건. SPEC-XPE-P1B-DICOM 확장이 선행 |
| 3 | `DicomNetworkTest` 4건 활성화 | 1커밋 + 의존성 결정 | QA-B-25 에서 규명 완료: storescp/wlmscpfs 바이너리가 의존성에 없다. 매니페스트 변경이 선행이라 Lane B 단독으로는 닫히지 않는다 |
| 4 | `.gitignore:271` 무효 예외 제거 | **1커밋, Class A** | §2.3. 루트 `.gitignore` = main 소유 |
| 5 | `DegradedMode` 5건 구동 잡 | 1커밋(CI) | `XPE_DEGRADED_ABSENT_DLL` 을 세팅하는 곳이 저장소에 없다. Lane A/main 후속으로 이미 배정됨 |
| 6 | #71 본문 체크박스 갱신·종결 판단 | 0커밋 | 완료 조건 4건과 "설정/CI" 는 실질 해소됐다(§2.2). 이슈 편집은 leader 권한 |

**Lane B 소유 경로에서 한 커밋으로 닫을 수 있는 미해소 항목은 0건이다.**
후보 3·4·5 는 다른 소유자의 선행 결정이 필요하고, 1·2 는 카드 한 장 크기가 아니다.

---

## 4. 미검증 (Gaps)

- **본문이 모호해 판정하지 못한 항목**: "참조 문서/요구사항" 에 나열된 6개 경로
  (`docs/post-processing/xpe/`, `docs/enhance-advanced/`, `docs/display/`, `docs/dicom/`,
  `docs/ai-module/`, `.moai/specs/SPEC-XPE-P2-ADV/`, `.moai/specs/SPEC-XPE-P3-AI/`).
  **참조 목록일 뿐 "무엇을 해야 한다" 가 없다.** 각 문서가 현재 코드와 동기화됐는지는
  판정 기준이 없어 표에 넣지 않았다. 기준을 주면 별도 카드로 잴 수 있다.
- **"제외 범위" 항목은 검증하지 않았다** — 정의상 이 이슈의 작업 대상이 아니다.
- **#71 당시의 커밋(6987b9d, 110ec1c, 2f41b5e, a67689f)이 현재 트리에 있는지 SHA 로
  확인하지 않았다.** 현재 트리의 **상태**로 판정했다. 브랜치 재작성이 있었다면 SHA 는
  안 맞아도 상태는 맞을 수 있어 상태 판정을 택했지만, "그 커밋이 병합됐다" 는 주장은 하지 않는다.
- **306/306 → 445/192/110 의 증감을 항목별로 귀속하지 않았다.** 빌드 구성이 단일
  스크립트에서 프리셋 3종으로 바뀌어 같은 축의 수치가 아니다.
- **ONNX 실경로는 여전히 미검증이다.** stub GREEN 을 "AI 동작 검증" 으로 읽으면 안 된다 (§2.2-17).
- **IPC 워커 왕복 미검증** — §2.5. 로컬 경로만 덮여 있다.
- `ci-post` 가 444 → 445 로 1건 늘었는데 **이번 병합(3b5c7fd, GUI-C-17)에서 온 것으로
  보이나 이름을 대조하지 않았다.** 내 변경이 아닌 것만 확인했다(이 카드는 코드 변경 0).

---

## 5. 잔여 위험 (Residual-risk)

- **"해소" 판정은 현재 트리 상태에 대한 것이다.** 항목 2·3처럼 빌드 통과로 간접 확인한 것은
  구성이 바뀌면(예: `/WX` 옵션 OFF) 다시 드러날 수 있다.
- **항목 12(설정/CI)를 "해소" 로 적었지만 체크박스는 여전히 비어 있다.** 이슈를 읽는 사람은
  미완으로 볼 것이다. 이슈 편집 권한이 이 레인에 없어 보고로만 남긴다.
- **S2/S4 를 "미해소" 로 두는 것이 곧 "해야 한다" 는 뜻은 아니다.** 두 항목 모두 원 코멘트가
  명시적으로 차후 세션 이관을 선택했다. 우선순위 판단은 leader 몫이다.
- **이 감사는 #71 의 종결을 주장하지 않는다.** 항목별 근거를 제시할 뿐이며,
  닫을지 여부는 카드 후보 6번대로 leader 결정이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` | vcvars + CMake/Ninja PATH |
| `_full.bat` / `_full.log` | 필터 없는 전체 ctest 3종 — 445 / 192 / 110, skip 19줄 |
