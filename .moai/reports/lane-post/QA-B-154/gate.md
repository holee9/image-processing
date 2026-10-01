# QA-B-154 (`#205`) — 156건 중 **19건만** 스텁 계약을 단언합니다

## 1. 주장

1. **전제 3건 모두 아직 참입니다.** `ci-ai` 프리셋 없음 · 워크플로가 쓰는 프리셋 전부
   `BUILD_AI=OFF` · 프리셋을 안 거치는 `ctest` 스텝에도 `ai` 는 없습니다. §2
2. **세 갈래는 (a) 19 · (b) 0 · (c) 137 입니다.** §3
3. **카드가 지목한 세 요구 중 둘은 단언하는 시험이 0건이고, 기능 자체가 없습니다.**
   `REQ-AI-061`(신뢰도 → Hough)·`REQ-AI-092`(시간 예산 → fallback + 알림). §4
4. **비용: 빌드 +11.3 s, 시험 +6.1~10.4 s.** 전체 스위트 잡음대(±11 s)와 같은 크기입니다. §5
5. **측정이 한 번 틀렸습니다** — 플래그를 `BUILD_TESTS` 아닌 `BUILD_TESTING` 으로
   줘서 첫 두 빌드는 시험을 아예 안 지었습니다. §7

---

## 2. 전제 확인 (카드 §1) — 부재 주장이므로 대조군을 짝지었습니다

### 프리셋 전수

| 프리셋 | `BUILD_AI` |
|---|---|
| `ci-common`·`ci-post`·`ci-preprocess`·`coverage`·`coverage-post`·`coverage-dicom`·`ci-fullstack` | **OFF** (명시) |
| `default`·`release`·`ci` | 미지정 → 루트 `option(BUILD_AI … ON)` |

`ci-ai` 는 **없습니다.** 미지정 3건 중 워크플로가 쓰는 것은 **하나도 없습니다.**

### 프리셋 밖 경로 (카드 §1-3)

`.github/workflows/` 의 `ctest` 호출 전수: 대상 디렉터리가 `build/ci-common` ·
`build/ci-preprocess` · `build/ci-post` 뿐입니다. 커버리지 잡은
`cmake --preset ${{ matrix.preset }}` 로 `[coverage, coverage-post, coverage-dicom]`
— 셋 다 `BUILD_AI=OFF`.

`ci.yml` 에 `paths:` 필터가 없으므로 **`modules/ai` 를 고치면 CI 는 돌지만 그 코드는
아무 잡에서도 안 지어집니다.**

### 네 이름으로 각각 검색 + 대조군 (카드 §4)

| 검색어 | `.github/workflows/` 적중 |
|---|---|
| `ci-ai` | **0** |
| `xpe_ai` | **0** |
| `BUILD_AI` | **0** |
| `modules/ai` | **0** |
| **대조군** `preprocess-tests` | 3 |
| **대조군** `post-build` | 3 |
| **대조군** `xpe_common` | 3 |

> **`modules/ai` 의 0 은 단독으로는 아무것도 증명하지 못합니다** — `modules/preprocess`
> 도 0 입니다(워크플로는 경로형 이름을 안 씁니다). 결론을 지탱하는 것은 앞의 세 이름과
> 프리셋 전수입니다.

---

## 3. 세 갈래 (카드 §2) — **판단의 핵심**

| 갈래 | 건수 | 비율 |
|---|---|---|
| **(a)** 스텁 계약을 단언 | **19** | 12 % |
| **(b)** 스텁이라 검증 불가능 | **0** | 0 % |
| **(c)** 둘 다 아님 | **137** | **88 %** |

합계 156. 분류는 **이름이 아니라 본문의 단언**으로 했고, 19개 이름을 시험 목록과
기계 대조했습니다(불일치 0 — `_classify.py`).

### (a) 19건 — 무엇을 단언하는가

전부 **`REQ-AI-002`(결정론적 fallback)** 한 요구에 붙습니다.

| 시험군 | 건수 | 단언 내용 |
|---|---|---|
| `AiFallbackTest` 스텁 4 경로 + 라벨/신뢰도 2 | 6 | 추론 진입점 4개가 `XPE_ERR_PROCESSING_FAILED`, 라벨 `"UNKNOWN"`, 신뢰도 `0.0` |
| `AiFallbackTest` `SetFallbackMode*` | 4 | 설정자가 양방향으로 `XPE_OK` 를 돌려준다 |
| `AiFallbackTest` `KnownDivergence_FallbackMode*` | 2 | **그 설정자가 되읽히지 않고 출력도 안 바꾼다** |
| `AiWorkerIsolationTest` `StubMode*FallsBackGracefully` | 4 | 워커 없이 죽지 않고 같은 코드로 실패 |
| `AiWorkerIsolationTest` `RepeatedStubFallbackIsConsistent` | 1 | 50회 반복해도 동일 (결정성) |
| `AiParamDependency` 2건 | 2 | 진입점이 모델 앞에서 멈춘다 · fallback 설정의 관측 가능한 효과를 기록 |

즉 **(a) 19건이 보장하는 것은 "AI 가 없을 때 정해진 실패를 정해진 값으로 돌려준다"
뿐입니다.** 임상적 유용성을 유지하는 *대체 경로*가 실제로 동작하는지는 이 안에
없습니다 — 그 대체는 호출자 몫이고, `ai` 모듈 바깥입니다.

### (b) 0건 — 스텁이라 못 재는 것 (`#130` 의 입력)

목록만 적습니다. **시험이 0건인 이유는 누락이 아니라 스텁이기 때문입니다.**

- 추론 정확도 (`bodypart` 라벨의 실제 정답률, bone suppression·denoise 출력 품질)
- 모델 적재 (`.onnx` 파싱, 입출력 텐서 형상 검증)
- EP(실행 공급자) 선택 — CPU/CUDA/DirectML 분기
- `REQ-AI-091` 서명 검증 (Ed25519/ECDSA)
- `REQ-AI-092` 추론 시간 예산 초과 → fallback + 알림
- `REQ-AI-061` 신뢰도 문턱 → Hough 결정론 경로 전환
- 실제 워커 프로세스와의 IPC 왕복 (지금 시험은 **워커가 없는** 경우만)

### (c) 137건 — 무엇인가

| 시험군 | 건수 | 성격 |
|---|---|---|
| `AiFallbackTest` 나머지 | 37 | null 인자 · 버퍼 크기 · `DataSizeGuard` · 빈 영상 계약 |
| `AiAbi` | 25 | 버전 문자열 4 · init/shutdown 수명주기 8 · **기하 헬퍼 `stitch_estimate_size` 7** · 미초기화 가드 6 |
| `AiModelCardTest` | 20 | **하드코딩된 스텁 JSON 문자열**에 필드가 있는지 |
| `AiModelVersioningTest` | 11 | 같은 스텁 JSON 을 다른 각도에서 |
| `AiWorkerIsolationTest` 나머지 | 11 | 프로토콜 **상수** 7 · 스레드 안전 2 · init/shutdown 2 |
| `AiErrorPrecedenceTest` | 10 | null 인자가 미초기화보다 우선하는가 |
| `AiIpcBridgeTest` | 10 | 브리지 생성/파괴 · **워커 없을 때의** 타임아웃 |
| `AiConfigWarning` | 8 | 모르는 config 키를 이름으로 알리는가 |
| `AiEndurance` | 2 | 1000 주기 힙 비증가 (+ 가짜 누수 대조군 1) |
| `AiDataSizeProbe` | 2 | 짧은 `dataSize` 를 과독 없이 거부 |
| `AiParamDependency` 나머지 | 1 | 부분 영상 치수가 출력에 도달 |

> **이름이 오해를 부릅니다.** "AI 시험 156건" 중 **88 %는 AI 와 무관한 C ABI 경계
> 시험**입니다 — null 인자, 버퍼 길이, 오류 우선순위, 상수값, 그리고 AI 가 전혀
> 관여하지 않는 기하 헬퍼. 이 156건이 초록이라는 사실은 **AI 에 대해 아무것도
> 말하지 않습니다.** 정확한 이름은 *"`xpe_ai` C ABI 경계 시험 137건 + 스텁 fallback
> 계약 19건"* 입니다.

---

## 4. 세 요구 중 둘은 **시험도 기능도 없습니다**

인용이 아니라 **거동으로** 뒤졌습니다.

| 요구 | 시험 인용 | 거동 단언 | 구현 |
|---|---|---|---|
| `REQ-AI-002` 결정론적 fallback | 파일 머리말 3곳(주석) | **19건** | 스텁 경로 존재 |
| `REQ-AI-061` 신뢰도 → Hough | **0** | **0** | **`modules/ai` 전체에 `hough` 0건** |
| `REQ-AI-092` 시간 예산 → fallback + 알림 | **0** | **0** | `time_budget`·`deadline`·`elapsed` 0건 |

세부:

- **`REQ-AI-061`.** `confidence_threshold` 는 `ai.cpp:206` 에서 **파싱되어 저장되지만**,
  `ai.cpp:418` 에서 `*confidenceOut = 0.0f` 로 무조건 덮입니다 — 문턱 비교가 한 번도
  실행되지 않습니다. `ConfidenceThresholdDefaultIs06`·`ConfidenceThresholdInRange` 두
  시험은 **헤더 상수가 `0.6` 인지, `[0,1]` 안인지**만 봅니다. 이름에 "Threshold" 가
  있어도 라우팅을 단언하지 않으므로 (c) 로 셌습니다.
  Hough 는 `modules/enhance_advanced/src/detail/hough_transform.cpp` 에 있고, `ai`
  모듈은 그것을 **부르지 않습니다** — 요구가 서술하는 경로는 모듈 경계를 건너는데
  그 자리에 코드가 없습니다.
- **`REQ-AI-092`.** `ai_ipc_bridge.cpp:9` 의 주석은 시간 예산을 **`REQ-AI-009`** 로
  적고 있습니다(`092` 아님). 그리고 `AiIpcBridgeTest` 의 타임아웃 2건은 **파이프
  연결 타임아웃**이지 추론 시간 예산이 아닙니다. `AiConfigWarning` 의 `alert` 36곳은
  **모르는 config 키 경고**이지 예산 초과 알림이 아닙니다. 요구가 말하는 "알림"은
  어디에도 없습니다.

> **리더께**: `REQ-AI-092` ↔ `REQ-AI-009` 번호 불일치는 SPEC 소유자 판단 사항이라
> 고치지 않았습니다. 둘 중 하나가 유령일 수 있습니다.

---

## 5. 비용 (카드 §3)

### (d) `BUILD_AI=ON` 이 더하는 것 — **측정값**

깨끗한 트리 2개(`build/c-off`, `build/c-on`), 같은 옵션, `BUILD_AI` 만 다름.

| 항목 | OFF | ON | 차이 |
|---|---|---|---|
| configure | 59.3 s | 56.8 s | **−2.5 s (잡음)** |
| 전체 빌드 | 36.4 s | 35.2 s | **−1.2 s (잡음)** |
| ninja 엣지 | 116 | 133 | **+17** |
| 시험 실행 파일 | 10 | 12 | **+2** |
| ctest 건수 | 575 | 731 | **+156** |
| ctest 전체 시간 | 130.3 s | 156.3 s | +26.0 s |

전체 빌드 차이가 **음수**로 나온 것이 잡음의 크기를 알려 줍니다. 그래서 **같은 트리
안에서** 다시 갈랐습니다(`build/c-on`):

| 측정 | 건수 | 시간 |
|---|---|---|
| `ctest -E "^Ai"` | 575 | 141.7 s |
| `ctest -R "^Ai"` | 156 | **10.4 s** (앞선 단독 측정 6.1 s) |
| 전체 | 731 | 156.3 s |

`141.7 + 10.4 = 152.1` ≈ `156.3` — 정합합니다. **같은 575건이 두 트리에서
130.3 s ↔ 141.7 s** 로 나온 것이 잡음대 **±11 s** 입니다.

`ai` 타깃만 지워 다시 지은 시간: **11.3 s** (`xpe_ai` + `xpe_ai_tests`).

**결론: 빌드 +11.3 s, 시험 +6.1~10.4 s. 합쳐 20 s 안쪽이고, 전체 스위트 잡음과 같은
크기입니다.** 비용은 판단의 제약이 아닙니다.

> **이 기계는 CI 러너가 아닙니다.** `#179` 에서 러너가 일주일 만에 1.5배 빨라진 것을
> 관측했습니다. 위 수치는 **상대 크기**(전체의 몇 %인가)로만 읽으십시오.

### (e) 새 잡 대 기존 잡 확장 — 자료만

| 선택지 | 더해지는 것 | 걸리는 것 |
|---|---|---|
| 기존 `post-build` 잡에 `BUILD_AI=ON` | 그 잡 +20 s 안쪽 | `ci-post` 는 리더 소유 프리셋; `ai` 가 post 잡의 빨강 원인이 될 수 있음 |
| 새 `ai-tests` 잡 | 러너 슬롯 1개 + 러너 기동·체크아웃 고정비 | `#202` 이후 `gui-e2e-native` 가 이미 최장 — 병렬이면 임계경로는 안 늘어남 |

**측정된 20 s 는 새 잡의 고정비(체크아웃 + 툴체인)보다 작을 가능성이 큽니다** —
다만 그 고정비를 저는 재지 않았습니다(§8).

### 바꿔야 할 것 (수정은 안 했습니다 — 카드 §5)

1. `CMakePresets.json` `ci-post` 의 `"BUILD_AI": "OFF"` → `"ON"` **한 줄**이면
   기존 잡이 156건을 함께 돌립니다. 새 프리셋은 필요 없습니다.
2. `QA-B-153` 의 `FATAL_ERROR` 가드는 **그래도 안 돕니다** — 그 가드는
   `XPE_AI_USE_ONNXRUNTIME=ON` 일 때만 발화하고, 위 변경은 `BUILD_AI` 만 켭니다.
   가드까지 CI 에 태우려면 `-DXPE_AI_USE_ONNXRUNTIME=ON` 으로 **실패를 기대하는**
   별도 구성 스텝이 필요합니다.
3. `ci.yml` 에 `paths:` 필터를 넣을 계획이라면, `modules/ai` 를 **빠뜨리지 않는 것**이
   지금 상태를 고착시키지 않는 조건입니다.

---

## 6. 검증

```
빌드:   ===OFF_BUILD=0===  ===ON_BUILD=0===          (_cost.bat)
ctest:  c-on -R "^Ai"  156/156 통과, 0 실패           (_ctest.bat, _ctest2.bat)
분류:   total 156  (a) 19  (b) 0  (c) 137
        (a) 19개 이름 전부 시험 목록과 일치, 불일치 0  (_classify.py)
```

`c-off`/`c-on` 전체 ctest 에서 **동일한 1건**이 빨강입니다:
`DuplicateExportTest.KnownDivergence_RenamedExportsStillDisagree`
(`enhance_advanced`). **`BUILD_AI` 와 무관**하며 두 트리에서 똑같이 납니다 — 이
구성이 `ci-post` 프리셋이 아니기 때문입니다. 이 카드의 범위 밖입니다.

## 7. 측정이 틀렸던 것 — 기록합니다

첫 비용 측정에서 `-DBUILD_TESTING=ON` 을 줬습니다. 이 트리의 플래그는
**`BUILD_TESTS`** 입니다. 두 구성 모두 `CONFIGURE=0`·`BUILD=0` 으로 성공했고,
`ctest` 는 **종료코드 0** 을 돌려줬습니다 — 로그를 열기 전까지는 통과로 보였습니다.

```
Test project .../build/c-off
No tests were found!!!          ← 종료코드는 0
```

> **아무것도 안 돈 것이 초록으로 보였습니다.** 건수(575/731)를 같이 읽지 않았으면
> "시험 추가 비용 0 s" 라는 틀린 수치를 보고할 자리였습니다. `QA-B-153` 의 파이프
> 오류와 같은 형태 — **가드도 빌드도 옳았고 틀린 것은 계측이었습니다.**

## 8. 미검증 · 잔여 위험

- **CI 러너에서 재지 않았습니다.** §5 는 전부 로컬 관측입니다
- **새 잡의 고정비(러너 기동 + 체크아웃 + 툴체인 설치)를 재지 않았습니다** — (e)의
  비교가 한쪽만 측정된 상태입니다
- `ci-post` 프리셋 구성으로는 재지 않았습니다(`BUILD_PREPROCESS=OFF` 등). 제 두 트리는
  기본 옵션 + `BUILD_TESTS=ON` 이라 **575건**이고 `ci-post` 의 703건과 다릅니다.
  `+156` 이라는 **증분**은 유효하지만 분모는 다릅니다
- **(c) 137건이 잘못 도는지는 안 봤습니다** — 전부 초록이고, 이 카드는 *무엇을
  보장하는가*만 셌습니다
- `REQ-AI-092` ↔ `REQ-AI-009` 번호 불일치의 어느 쪽이 옳은지 판정하지 않았습니다

---

Refs #205
