# QA-B-155 (`#130`) — 없는 경로를 약속하는 주석 **5건**. 한 건이 아니었습니다

## 1. 주장

1. **전제는 참입니다.** ONNX 획득 블록의 `FetchContent_Declare` 는 **0건**이고,
   주석이 약속한 다운로드는 일어나지 않습니다. 측정으로 확인했습니다. §2
2. **전수에서 5건 나왔습니다** — 카드가 지목한 3줄(`:67`·`:72`·`:80`) 외에
   **`pkg-config` 약속**과 **"임시로 비활성화된 워커" 라는 주석이 붙은 `if(TRUE)`**.
   그중 둘은 카드가 몰랐던 것입니다. §3
3. **가장 큰 것은 헤더 `:8` 이었습니다.** *"REQ-AI-006: ONNX Runtime 1.20+
   integration with multi-EP support"* 를 모듈의 **현재 속성**으로 적어 뒀는데,
   ONNX 를 켜고 런타임을 줘도 세션 코드는 스텁과 같은 길을 갑니다. §3-F5
4. **의심했다가 철회한 것 1건을 적습니다** — `ONNX_RUNTIME_STUB_BUILD` 이름
   불일치는 **결함이 아니었습니다**(헤더가 올바로 잇고 있음). §4
5. 대조군 초록: 스텁 구성·빌드·`ctest -R "^Ai"` **156/156**. §6

---

## 2. 전제 확인 + 반증 (카드 §0·§4)

### 세어 본 것

| 대상 | `FetchContent_Declare` |
|---|---|
| `modules/ai/CMakeLists.txt` ONNX 획득 블록 | **0** |
| **대조군** 같은 파일 gtest 블록(`:304`) | 1 |
| **대조군** `modules/common/CMakeLists.txt` | 4 |

같은 검색이 실재하는 곳은 읽습니다 — **0 이 검색 실패가 아닙니다.**

### 서술이 가리키는 동작을 실제로 걸었습니다

`ONNXRUNTIME_ROOT` 없이, 주석대로면 다운로드가 일어나야 하는 구성:

```
cmake -S . -B build\f-doc ... -DXPE_AI_USE_ONNXRUNTIME=ON -DXPE_AI_STUB_BUILD=OFF
===DOC_CONFIGURE=1===
  found, so the build cannot honour it (#130).  It is NOT downgraded to a …
```

**다운로드가 아니라 `FATAL_ERROR` 입니다.** 종료 코드는 파이프 없이 받았습니다
(`_falsify.bat` 머리말에 이유를 적었습니다).

> **로그에 보인 `FetchContent` 경고는 이것이 아닙니다.** `CMakeLists.txt:101
> (FetchContent_Populate)` — **루트의 eigen** 입니다. 파일을 안 보고 문자열만
> 읽었으면 *"FetchContent 가 돌긴 하네"* 로 오독할 자리였습니다.

---

## 3. 전수 (카드 §3) — **이 카드의 값**

`modules/ai/CMakeLists.txt` 372줄 전체에서 **코드가 하지 않는 것을 서술하는 주석**.

| # | 위치 | 주석이 말한 것 | 실재 | 근거 |
|---|---|---|---|---|
| **F1** | `:67` 제목 | "ONNX Runtime **via FetchContent**" | 그 블록에 `FetchContent_Declare` 0건 | 계수 + 대조군 2 |
| **F2** | `:72-73` | "FetchContent **downloads** pre-built NuGet package … or builds from source" | 다운로드 0. 구성이 `FATAL_ERROR` 로 끝남 | `DOC_CONFIGURE=1` |
| **F3** | `:80` | "or let this script **attempt FetchContent**" | 시도하는 코드 없음 | 같음 |
| **F4** | `:102` | "find_package **or pkg-config**" | `pkg_check_modules`·`PkgConfig` **0건** | 파일 전수 |
| **F5** | `:8` 헤더 | "REQ-AI-006: ONNX Runtime 1.20+ **integration with multi-EP support**" | 어떤 빌드도 ONNX 를 링크하지 않음 | 아래 |
| **F6** | `:256` | "End of **temporarily disabled** worker build" | 닫는 가드가 `if(TRUE)`, 워커는 계속 빌드됨 | `xpe_ai_worker.exe` 실재 |

**F5 세부** — `src/ai_onnx_session.cpp` 의 `#else`(풀 빌드) 가지가 스텁과 같습니다:

```cpp
#else
    // Full build mode: Create actual ONNX Runtime session
    // TODO: Implement actual ONNX Runtime session creation
    // For now, use stub implementation even in full build
```

EP 목록도 조회가 아니라 하드코딩입니다(`// TODO: Query ONNX Runtime for
available EPs` 아래에 `kCuda`·`kTensorRt`·`kDirectMl` 를 무조건 `push_back`).
파일 전체에 `Ort::` 0건. 즉 **`:8` 은 요구이지 현재 상태가 아닌데 현재 상태처럼
적혀 있었습니다.**

**F6 세부** — `:216 if(TRUE)  # Worker build enabled` 와 `:256 endif()  # End of
temporarily disabled worker build` 가 **서로 모순**입니다. 실재는 전자입니다:
`build/v155/bin/xpe_ai_worker.exe` 가 나옵니다. `#205` 에서 단일 프로세스 스텝이
이 워커를 gtest 로 잘못 돌린 것이, "비활성" 이라고 적힌 것이 실제로는 돌고 있었기
때문입니다.

### 더해서 — 카드 §3 이 물은 "옵션 기본값 서술"

`:11-12` 의 기본값 서술(`USE` 기본 OFF, `STUB` 기본 ON)은 `option()`(`:26`·`:27`)과
**일치합니다.** 다만 `QA-B-153` 이후 **문서화된 기본값 그대로 `USE` 만 켜면
`FATAL_ERROR`** 입니다 — 두 스위치를 독립인 것처럼 나열한 것이 오해를 부릅니다.
틀린 것은 아니지만 불완전해서 같이 고쳤습니다.

### 확인했는데 **문제 없던** 것 — 전수가 초록도 낸다는 증거

| 대상 | 결과 |
|---|---|
| `:168-169` "소스가 `#ifdef` 가드를 쓴다" | **참** — `XPE_AI_USE_SPDLOG`·`XPE_AI_USE_NLOHMANN_JSON` 둘 다 `src` 에서 실제 사용 |
| `:273-286` 시험 소스 목록 | **일치** — `tests/*.cpp` 9개와 `diff` 0 |
| `:298` "prefer already-available target, then FetchContent" | **참** — 이쪽 FetchContent 는 실재 |
| `:188` `/wd4996 /wd4150` 설명 | 맞음 |

---

## 4. 의심했다가 철회한 것 — 기록합니다

`ai_onnx_session.cpp` 가 `#if ONNX_RUNTIME_STUB_BUILD` 를 쓰는데 CMake 가 정의하는
것은 `XPE_AI_STUB_BUILD` 입니다. **이름이 다르므로 정의되지 않은 매크로가 `0` 으로
평가되어 항상 `#else` 가지가 돈다** 고 의심했습니다.

**틀렸습니다.** `include/xpe/ai/ai_onnx_session.h:24-28` 이 잇고 있습니다:

```cpp
#ifdef XPE_AI_STUB_BUILD
    #define ONNX_RUNTIME_STUB_BUILD 1
#else
    #define ONNX_RUNTIME_STUB_BUILD 0
#endif
```

> 정의처를 전수로 찾기 전에 보고했으면 **없는 결함을 만들 자리**였습니다. 두
> 가지가 둘 다 스텁이라(§3-F5) 어느 쪽이 돌든 동작이 같아서, **시험으로는 이
> 오진을 잡을 수 없었습니다** — 읽는 것 말고는 방법이 없었습니다.

---

## 5. 고친 것

`modules/ai/CMakeLists.txt` 한 파일. **주석과 `include(FetchContent)` 한 줄만**
바뀌었고 빌드 논리는 그대로입니다.

- `:8` — REQ-AI-006 을 **요구이지 현재 상태가 아님**으로 명시, 근거 줄(`:198`) 인용
- `:11-12` — 두 스위치가 독립이 아님과 `FATAL_ERROR` 조건을 적음
- `:67-80` → 새 블록 — 옛 문구를 **그대로 인용해 남기고**, `#130` 결정 표
  (사전 빌드 · vcpkg 안 씀 · 자동 다운로드 안 함)와 실제 3단계 경로를 적음
- `:70` `include(FetchContent)` **제거** — 이 블록에서 안 쓰입니다.
  gtest 쪽(`:330`)은 자기 `if(NOT TARGET GTest::gtest_main)` 안에서 다시
  `include` 하므로 무관합니다. **확인하고 뺐고, 빌드로 재확인했습니다**
- `:102` — "or pkg-config" 제거, 계수(0건)를 그 자리에 적음
- `:256` — "temporarily disabled" 를 실재로 교체

카드 §2 대로 `#130` 결정 표를 주석에 인용했습니다 — 다음 사람이 이슈를 안 열어도
됩니다.

---

## 6. 검증

```
대조군(스텁 빌드가 여전히 선다):
  ===CTRL_CONFIGURE=0===  ===CTRL_BUILD=0===  ===CTRL_CTEST=0===
  100% tests passed, 0 tests failed out of 156      (_verify.bat)
반증(주석이 약속한 다운로드):
  ===DOC_CONFIGURE=1===  "cannot honour it (#130)"  (_verify.bat, _falsify.bat)
F6 의 실재:
  build\v155\bin\xpe_ai_worker.exe                  (존재)
전체:
  ===FULL_CTEST=8===  730/731                       (_verify_full.bat)
```

전체 ctest 의 **빨강 1건은 선재(先在)** 입니다:
`DuplicateExportTest.KnownDivergence_RenamedExportsStillDisagree`
(`enhance_advanced`). **QA-B-154 에서 이 수정 이전에 두 트리(`c-off`·`c-on`)
모두에서 똑같이 났습니다** — `BUILD_AI` 와도 이 카드와도 무관하고, 이 구성이
`ci-post` 프리셋이 아니기 때문입니다.

## 7. 미검증 · 잔여 위험

- **런타임이 실제로 있는 경로는 여전히 안 걸어 봤습니다.** ONNX Runtime 바이너리가
  없어 `ONNXRUNTIME_ROOT` 분기(`:99-115`)가 서는지는 미확인 — `QA-B-153` 과 같은
  미검증이 그대로 남습니다. 새 주석은 그 분기를 **서술만** 하고 증명하지 않습니다
- **`:90-95` 의 파일명 가정**(`onnxruntime.lib`/`.dll`/`libonnxruntime.so`)이 실제
  배포본 레이아웃과 맞는지 확인 안 됨
- **전수 범위는 `modules/ai/CMakeLists.txt` 한 파일입니다.** 같은 형태가 다른
  `CMakeLists` 에 있는지는 안 봤습니다
- **`src/` 주석은 고치지 않았습니다** — `ai_onnx_session.cpp:5-6` 의 *"Stub mode
  provides functional API"* / *"stub/full mode support"* 도 같은 형태입니다(풀
  모드가 없으므로). 카드 범위가 `CMakeLists` 라 손대지 않았고, **별도 카드
  대상으로 보고합니다**
- 주석은 실행되지 않으므로 **회귀를 막는 시험이 없습니다.** 같은 어긋남이 다시
  생기는 것을 기계적으로 잡을 수단은 이 카드가 만들지 않았습니다

---

Refs #130
