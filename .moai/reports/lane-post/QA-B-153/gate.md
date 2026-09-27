# QA-B-153 (`#130`) — 조용한 강등을 없앴습니다. **경로가 하나가 아니라 둘이었습니다**

## 1. 주장

1. **요청했는데 못 주면 멈춥니다.** `USE_ONNXRUNTIME=ON` 인데 런타임이 없으면
   `FATAL_ERROR` — 강등하지 않습니다.
2. **두 번째 조용한 경로를 찾아 같이 막았습니다.** 카드가 지목한 것(`:73-74` 의
   `CACHE FORCE`) 말고도, **`USE=ON` + `STUB` 기본값 `ON`** 이면 획득 블록에 아예
   **안 들어가서 경고조차 없었고, 캐시는 `ON` 이라고 말했습니다.** §3
3. **대조군이 살아 있습니다.** 아무것도 요청하지 않으면 여전히 스텁으로 섭니다
   (`CONFIGURE=0`, `BUILD=0`).
4. **기본값/사용자 구별은 필요하지 않았습니다** — 이유는 §4.
5. 기존 빌드 무영향: ctest **703/703**.

## 2. 측정 — 세 갈래 (카드 §2)

```
---A CONTROL: nothing requested, must still build a stub---
===A_CONFIGURE=0===
-- xpe_ai: STUB build (no ONNX Runtime)
-- xpe_ai_worker: STUB build (no ONNX Runtime)
===A_BUILD=0===
---B: USE=ON only, STUB at default ON, must FAIL---
===B_CONFIGURE=1===
  xpe_ai: XPE_AI_USE_ONNXRUNTIME=ON and XPE_AI_STUB_BUILD=ON contradict each …
---C: USE=ON + STUB=OFF, runtime absent, must FAIL---
===C_CONFIGURE=1===
  found, so the build cannot honour it (#130).  It is NOT downgraded to a …
    Point at a pre-built runtime:  -DONNXRUNTIME_ROOT=<dir containing include/ and lib/>
```

| 갈래 | 요구 | 관측 |
|---|---|---|
| **A** 요청 안 함 | 스텁으로 선다 | `CONFIGURE=0`·`BUILD=0`, STUB 메시지 2줄 |
| **B** `USE=ON` 만 | 멈춘다 | **`CONFIGURE=1`**, 모순을 이름으로 지목 |
| **C** `USE=ON`+`STUB=OFF`, 런타임 없음 | 멈춘다 | **`CONFIGURE=1`**, `ONNXRUNTIME_ROOT` 안내 |

**A 가 대조군입니다.** 없으면 "다 죽이는 구현" 이 B·C 만으로 통과합니다.

### 메시지가 무엇을 하라고 말하는가 (카드 §2 셋째)

B: `-DXPE_AI_USE_ONNXRUNTIME=ON -DXPE_AI_STUB_BUILD=OFF` 또는
`-DXPE_AI_USE_ONNXRUNTIME=OFF`.
C: `-DONNXRUNTIME_ROOT=<include/ 와 lib/ 를 가진 디렉터리>` 또는
`find_package(onnxruntime CONFIG)` 가 볼 수 있는 설치, 또는 `OFF` 로 스텁.

## 3. 두 번째 경로 — 카드가 몰랐던 쪽이 **더 나쁩니다**

획득 블록의 조건은 `if(XPE_AI_USE_ONNXRUNTIME AND NOT XPE_AI_STUB_BUILD)` 입니다.
`STUB_BUILD` 기본값이 `ON` 이므로 **`USE=ON` 만 주면 블록에 안 들어갑니다.**

고치기 전 관측(`_probe.log`):

```
---B USE=ON only, STUB left at default ON---
-- xpe_ai: STUB build (no ONNX Runtime)
===B_CONFIGURE=0===
XPE_AI_STUB_BUILD:BOOL=ON
XPE_AI_USE_ONNXRUNTIME:BOOL=ON        ← 캐시는 ON, 빌드는 스텁
```

> **경고 한 줄조차 없습니다.** 그리고 `:73-74` 경로와 달리 **캐시가 `ON` 으로
> 남습니다** — 확인하려고 캐시를 들여다본 사람이 정반대 결론을 얻습니다.
> `:73-74` 는 최소한 자기 상태를 정직하게 `OFF` 로 적었습니다.

`CACHE FORCE` 만 고쳤으면 이쪽은 그대로 남았습니다.

## 4. 기본값/사용자 구별 — 필요 없었습니다 (카드 §1 질문)

카드가 *"`CACHE` 가 기본값에서 온 것인지 사용자가 준 것인지 구별하는 방법을
확인해 보십시오"* 라고 했는데, **이 자리에서는 불필요합니다**:

- `option(XPE_AI_USE_ONNXRUNTIME … OFF)` 가 기본을 `OFF` 로 둡니다(`:26`)
- **트리 안에서 `ON` 으로 켜는 곳이 없습니다** — `CMakeLists.txt`·`*.cmake`·
  `CMakePresets.json` 전수 검색, 나오는 것은 `option()` 선언과 사용처뿐

따라서 **`ON` 이라는 값 자체가 요청입니다.** 출처를 물을 필요가 없습니다.

### 검토하고 버린 두 가지 — 근거를 적습니다

**(a) `if(DEFINED VAR)` 을 `option()` 앞에 두는 기법.** CMake 에 실제로 있는 수단
이지만, **재구성(re-configure)에서는 캐시에 이미 항목이 있어 항상 `TRUE`** 가 됩니다
— 둘째 실행부터 기본값과 사용자 값을 구별하지 못합니다.
**이것은 CMake 캐시 의미론에서 추론한 것이고 측정하지 않았습니다**(미검증).
쓰지 않았으므로 판정에 영향은 없습니다.

**(b) `USE=ON` 일 때 `STUB_BUILD` 를 자동으로 `OFF` 로 바꾸기.** 더 친절하지만,
**지금 없애는 것과 같은 동작**입니다 — 호출자가 준 값을 조용히 바꾸는 것. 방향만
호의적입니다. 게다가 (a) 없이는 *"기본값 `ON`"* 과 *"사용자가 명시한 `ON`"* 을
구별할 수 없어, 명시적으로 준 `STUB=ON` 을 지워 버릴 수 있습니다.
**이름을 지목하는 오류 한 줄이 마찰은 적고 오독은 불가능합니다.**

## 5. 계측 자체가 틀렸던 것 — 기록합니다

첫 검증 하네스가 이렇게 찍었습니다:

```
===B_CONFIGURE=0===      ← 실제로는 실패했는데 0
===C_CONFIGURE=0===
```

원인: `cmake … | findstr …` 로 파이프를 걸어서 **`%errorlevel%` 이 `findstr` 의
결과**를 담았습니다. 오류 본문이 로그에 있으니 가드가 동작한 것은 보였지만,
**제가 보고할 숫자는 무의미했습니다.** 파이프를 없애고 파일로 받아 다시 쟀습니다
(`_verify3.bat` 머리말에 이유를 적었습니다).

> **가드는 처음부터 옳았고 틀린 것은 측정이었습니다.** `CONFIGURE=0` 을 그대로
> 보고했으면 *"FATAL 을 넣었는데 여전히 0 이 난다"* 는 없는 결함을 만들었을
> 자리입니다.

## 6. 범위 밖 (손대지 않음)

조달 경로 · `SOUP-003` 버전 · 모델 조달 · CI 잡(`#205`). 카드 §3 대로입니다.

## 7. 이 카드가 닫는 것

`#130` 의 차단 사유 중 **"옵션을 켜도 조용히 되돌아간다" 하나**입니다. 실제로는
**그 하나가 둘이었고 둘 다 닫았습니다.** 나머지 셋(모델·조달·SOUP 버전)은 남습니다
— **`#130` 은 닫히지 않습니다.**

## 8. 미검증 · 잔여 위험

- **런타임이 있는 경우의 성공 경로를 시험하지 않았습니다.** ONNX Runtime 바이너리가
  없어 `ONNXRUNTIME_ROOT` 분기(`:45-63`)가 실제로 서는지는 여전히 미확인입니다.
  이 카드는 **실패 경로만** 고쳤습니다
- **`:45-63` 의 파일명 가정**(`onnxruntime.lib`/`onnxruntime.dll`)이 실제 배포본
  레이아웃과 맞는지 확인 안 됨 — QA-B-152 에서와 같은 미검증
- **(a) 기법의 재구성 한계는 추론이고 측정이 아닙니다** (§4)
- **`xpe_ai_worker` 쪽 분기(`:179-182`)는 건드리지 않았습니다.** 같은 두 변수를
  읽지만 새 가드가 구성 초반에 멈추므로 도달하지 않습니다 — 다만 **그 경로를 따로
  시험하지는 않았습니다**
- CI 에서 `BUILD_AI=OFF` 이므로 **이 가드도 어떤 CI 잡에서도 실행되지 않습니다**
  (`#205`). 로컬 관측뿐입니다

## 9. 검증

```
===BUILD=0===                    (_build.log)
ctest ci-post: 703/703 통과       (_verify.log, ===CTEST=0===)
한 프로세스 전체: 12 바이너리 0 실패
세 갈래 반증: A 0/0 · B 1 · C 1  (_verify3.log)
고치기 전 관측                     (_probe.log — 두 번째 조용한 경로)
```

---

Refs #130
