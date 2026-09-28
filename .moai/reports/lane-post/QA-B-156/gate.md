# QA-B-156 (`#130`) — 풀 모드가 없다는 것을 적었습니다. 그리고 **제 인용이 밀렸습니다**

## 1. 주장

1. **리더 수치를 독립으로 재현했습니다** — `modules/` 전체에서 ONNX API 6종 전부
   **0건**, 대조군은 살아 있습니다. §2
2. **`ai_onnx_session.cpp` 머리말을 실재에 맞췄습니다** — 지우지 않고, 수치·대조군·
   그리고 *"미구현이지 설계가 아니다"* 를 적었습니다. `@MX:TODO` 로 기계 수확도
   가능하게 했습니다. §3
3. **`CMakeLists.txt:256` 은 `QA-B-155` 에서 이미 고쳤습니다** — 이 카드에서 다시
   고칠 것이 없었고, 남은 일은 `blame` 이었습니다. §4
4. **`blame` 결론: 잔재입니다.** `if(TRUE)` 와 *"temporarily disabled"* 주석이
   **같은 커밋에서 함께 태어났습니다** — 모순이 diff 로 한 번도 보이지 않았습니다.
   **가드는 손대지 않았습니다.** §4
5. **제 자신의 결함 1건**: `QA-B-155` 에서 제가 적은 `:198` 인용이 이 카드의 삽입
   22줄 때문에 밀렸고, 같은 파일 인용 3건도 밀렸습니다. **전부 이름으로 바꿨습니다.** §5
6. 비주석 변경 **0건**, `ctest -R "^Ai"` **156/156**. §6

---

## 2. 전제 — 리더 수치의 독립 재현 (카드 §0)

넘겨받은 사유를 그대로 옮겨 적지 않고 다시 셌습니다.

| 검색어 | `modules/**/*.{cpp,h}` 적중 |
|---|---|
| `Ort::` | **0** |
| `onnxruntime_c_api` | **0** |
| `OrtApi` | **0** |
| `OrtSession` | **0** |
| `OrtGetApiBase` | **0** |
| `OrtEnv` | **0** |
| **대조군** `spdlog` (같은 파일 안) | 4 |
| **대조군** `nlohmann` (같은 파일 안) | 2 |
| **대조군** `XpeErrorCode` (`modules/` 전체) | 841 |

리더가 쓴 `OrtEnv` 를 제가 하나 더 추가했고 결과는 같습니다. **검색이 눈먼 것이
아닙니다** — 같은 도구·같은 범위에서 존재하는 것은 읽힙니다.

---

## 3. `ai_onnx_session.cpp` — 지우지 않고 **적었습니다** (카드 §1)

### 옛 문구 (머리말 `:5-6`)

> *"Implements session management with stub/full mode support."*
> *"Stub mode provides functional API without ONNX Runtime dependency."*

**풀 모드가 없으므로 둘 다 실재와 어긋납니다.** 그냥 지우면 *"풀 모드가 없다"* 는
사실도 같이 사라져, 다음 사람이 파일을 읽고 *"구현돼 있나 보다"* 로 가는 것을 막지
못합니다 — 카드 §1 의 요구 그대로 **침묵 대신 기록**을 택했습니다.

### 새로 적은 것 (요지)

- ONNX API 호출이 **없다**는 것 — §2 의 6종 0건 + 대조군 3개를 주석 안에 인용
- `#if ONNX_RUNTIME_STUB_BUILD` 의 **양쪽 가지가 모두 스텁**이고, `#else` 가지가
  스스로 그렇게 말한다는 것 (`OnnxSession::Create` 의 *"For now, use stub
  implementation even in full build"*)
- 그것이 **설계가 아니라 미구현**이며 **`#130` 의 본체**라는 포인터
- **호출자에게 갖는 뜻**: 진짜 런타임을 링크해도 이 파일은 달라지지 않습니다.
  `GetAvailableExecutionProviders()` 의 "풀" 가지는 `kCuda`·`kTensorRt`·
  `kDirectMl` 를 **무조건 `push_back`** 하는 하드코딩 목록이라, **그 기계에 없는 EP
  를 있다고 답할 수 있습니다**
- 옛 문구를 **그대로 인용해 남겼습니다** — `QA-B-155` 에서 쓴 방식과 같습니다

### `@MX:TODO` 를 붙였습니다

`mx-tag-protocol` 의 *"SPEC requirement is not implemented"* 트리거에 정확히
해당합니다. 산문만으로는 기계가 못 세므로:

```
 * @MX:TODO: implement the ONNX Runtime session path (create session, extract
 *           input/output metadata, query available execution providers)
 * @MX:SPEC: SPEC-XPE-P3-AI REQ-AI-006
```

`code_comments: en` 을 읽고 영어로 적었습니다.

### `:8`/`REQ-AI-006` 도 같은 처리

머리말의 요구 줄에 *"REQUIREMENT, NOT CURRENT STATE"* 를 달았습니다 —
`CMakeLists.txt` 쪽(`QA-B-155`)과 같은 문구로 맞췄습니다.

---

## 4. `CMakeLists.txt:256` — 고칠 것이 없었고, `blame` 이 남았습니다 (카드 §2)

### 이미 고쳐져 있습니다

카드는 이 주석을 실재에 맞추라고 했지만, **`QA-B-155` 에서 이미 했습니다**(커밋
`99fe367`, 리더가 푸시). 현재 상태:

```cmake
endif()  # if(TRUE) above. This said "End of temporarily disabled worker
         # build" while the guard it closes is `if(TRUE)` -- the worker HAS
         # been built all along (xpe_ai_worker.exe is produced; #205 found the
         # single-process step tripping over it). QA-B-155.
```

**중복 작업을 하지 않았습니다.** 이 카드가 더한 것은 아래 `blame` 뿐입니다.

### 반증 (카드 §3) — 주석대로면 워커가 없어야 합니다

```
build\v156\bin\xpe_ai_worker.exe      (존재)
```

**있습니다.** 주석이 틀렸습니다.

### `blame` — 의도인가 잔재인가 (카드 §2, **보고만**)

| 관측 | 값 |
|---|---|
| `if(TRUE)  # Worker build enabled` 도입 | `dd7c8e0` (2026-04-28, drake.lee) |
| `endif()  # End of temporarily disabled worker build` 도입 | **같은 커밋 `dd7c8e0`** |
| 그 커밋 제목 | `fix(post): enhance_basic MSVC warning cleanup (#53)` |
| 그 커밋이 이 파일에 한 일 | **+270 / −23** (워커 블록 최초 도입) |
| `dd7c8e0^` 시점에 이 파일의 워커 블록 | **0건** (`WORKER_NAME`·`if(TRUE)`·`temporarily` 전부 없음) |
| 같은 형태가 다른 모듈에 | **0건** (`modules/*/CMakeLists.txt` + 루트 전수) |

**읽기 (관측이 아니라 해석입니다):** 워커 블록은 처음부터 `if(TRUE)` 로 태어났고,
닫는 주석은 *"임시 비활성"* 으로 태어났습니다. **둘이 한 커밋 안에 함께 들어왔으므로
모순이 diff 로 한 번도 드러나지 않았습니다.** `if(TRUE)` 는 변수를 읽지 않으므로
아무 결정도 담지 않습니다 — 토글 자리에 남은 **잔재**로 읽힙니다. 다만
*"비활성으로 쓰려다 켠 뒤 주석을 안 고쳤다"* 는 저자 의도의 추정이고, 커밋 제목이
이 파일과 무관(`enhance_basic` 경고 정리)해서 그 커밋의 기록만으로는 확정할 수
없습니다.

**가드는 지우지도 되살리지도 않았습니다** (카드 §4). `if(TRUE)` 를 진짜 옵션으로
바꾸는 것은 워커를 안 짓는 구성을 새로 만드는 일이고, `#205` 가 방금 이 워커를 CI
에 노출시킨 직후라 **별건입니다.**

---

## 5. 제 인용이 밀렸습니다 — 같은 카드가 다루는 그 형태입니다

머리말에 줄을 더하자(`git diff --numstat`: `.cpp` **+25/−2**, `CMakeLists`
**+12/−7**) **제가 `QA-B-155` 에서 적은 인용이 조용히 틀렸습니다.**

| 인용 위치 | 적힌 값 | 실제 | 상태 |
|---|---|---|---|
| `CMakeLists.txt` 머리말 → 소스의 "For now…" | `:198` | **222** | 밀림 |
| `CMakeLists.txt` → 모순 가드 | `:57` | **68** | 밀림 |
| `CMakeLists.txt` → `option()` | `:26` | **36** | 밀림 |
| `CMakeLists.txt` → gtest `FetchContent_Declare` | `:304` | **336** | 밀림 |
| `CMakeLists.txt` → `modules/common:19,26` | `:19,26` | 19, 26 | **맞음** (다른 파일이라 안 밀림) |

**전부 이름으로 바꿨습니다** — `OnnxSession::Create`,
`GetAvailableExecutionProviders()`, `if(XPE_AI_USE_ONNXRUNTIME AND
XPE_AI_STUB_BUILD)` 가드, `option(XPE_AI_USE_ONNXRUNTIME)`, *"this same file's
test block"*. 인용한 이름 전부를 `grep` 으로 대조했습니다(§6).

> **줄번호는 같은 파일 안에서도 썩습니다.** 다른 파일을 가리키는 것만 위험하다고
> 생각했는데, 이번엔 **자기 파일 안 인용 3건**이 제 편집 한 번에 밀렸습니다.
> `#180` 에서 시험 이름 인용이 하나 틀리자 셋이었던 것과 같은 형태이고, 이번에는
> **틀린 인용을 만든 쪽이 저였습니다.**

> **이 표의 첫 초안도 틀렸습니다.** 삽입 줄수로 **계산해서** `220`·`333` 을 적었는데,
> `grep -n` 으로 재니 `222`·`336` 이었습니다(중간에 편집을 두 번 더 했기 때문).
> **보고하는 수는 도구가 낸 수여야 한다**는 것이 이 표 안에서 한 번 더 났습니다 —
> 계산으로 채운 인용은 처음부터 인용이 아닙니다.

`modules/common/CMakeLists.txt` 의 `FetchContent_Declare` 는 지금 **4건**
(`:19,26,33,104`)입니다 — `QA-B-155` 에서 "19,26" 만 인용한 것은 대조군으로는
충분하지만 전수는 아니었으므로, 새 주석에서는 **"has four"** 로 바꿨습니다.

---

## 6. 검증

```
비주석 변경:  주석·공백 제외 diff 0줄        (git diff -U0 필터)
구성/빌드:    ===CONFIGURE=0===  ===BUILD=0===
시험:         ===CTEST_AI=0===  156/156 통과   (_verify.bat)
:256 반증:    build\v156\bin\xpe_ai_worker.exe 존재
인용 대조:    OnnxSession::Create 2 · GetAvailableExecutionProviders 3 ·
              ONNX_RUNTIME_STUB_BUILD 4 · kTensorRt 3  (전부 >0)
남은 :숫자:   설명문 안의 ":198" 1건(의도) + modules/common:19 1건(맞음)
```

종료 코드는 파이프 없이 받았습니다(`_verify.bat` 머리말).

## 7. 유지한 것 (카드 §5)

- **철회 기록**: `QA-B-155` §4 의 `ONNX_RUNTIME_STUB_BUILD` 오진 철회를 지우지
  않았습니다. 이 카드가 그 관찰을 보강합니다 — **양쪽 가지가 둘 다 스텁이므로
  어느 쪽이 돌든 동작이 같고, 반증이 구조적으로 침묵합니다.**
- **문제 없던 것**: `QA-B-155` §3 의 "확인했는데 문제 없던 것" 표를 그대로 뒀습니다.

## 8. 미검증 · 잔여 위험

- **런타임이 실제로 있는 경로는 여전히 안 걸어 봤습니다.** 새 주석은 *"링크해도
  달라지지 않는다"* 를 **코드 읽기로** 주장합니다 — ONNX Runtime 바이너리로 확인한
  것이 아닙니다. `Ort::` 0건이 그 주장을 강하게 받치지만, 링크 후 관측은 아닙니다
- **`if(TRUE)` 가 잔재라는 것은 해석입니다.** 관측은 "같은 커밋에서 모순이 함께
  태어났다" 까지이고, 저자 의도는 확정하지 않았습니다
- **주석에는 회귀 가드가 없습니다.** `@MX:TODO` 는 기계로 **세어지지만**, 주석이
  다시 실재와 어긋나는 것을 **막지는** 못합니다
- **인용 썩음을 막는 수단을 만들지 않았습니다.** 이번엔 이름으로 바꿔 피했지만,
  다음 편집에서 다른 인용이 밀리는 것을 기계적으로 잡을 방법은 없습니다
- `ai_onnx_session.h` 의 주석은 보지 않았습니다 — 범위를 `.cpp` 머리말 +
  `CMakeLists` 로 둔 채입니다

---

Refs #130
