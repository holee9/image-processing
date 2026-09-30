# QA-B-169a — 한 줄 고쳤습니다. **빠진 구성은 제 `ci-post` 빌드 디렉터리였습니다**

## 1. 주장

1. **고쳤습니다.** `getenv_s`/`std::getenv` 갈래 — `test_onnx_session.cpp:234-240`
   의 관례 그대로. `_CRT_SECURE_NO_WARNINGS`·`/wd4996` 는 쓰지 않았습니다. §2
2. **제 초록이 빠뜨린 것은 "구성" 이 아니라 "타깃" 입니다.** 제 `ci-post`
   빌드 디렉터리는 **`BUILD_AI=OFF` 로 굳어 있었고 `xpe_ai_tests.exe` 자체가
   없었습니다.** 705 안에 AI 시험이 **0건**이었습니다. §3
3. **가설 셋 중 첫째입니다** — `/WX` 도 붙고 컴파일러 버전도 원인이 아닙니다.
   프리셋은 처음부터 옳았고, **제 디스크의 캐시가 프리셋보다 오래됐습니다.** §3
4. **반증 양방향.** 고친 뒤 `ci-post` 초록, `getenv` 를 도로 넣으니 **같은
   `C2220` 이 로컬에서 재현**됩니다. §4
5. **지금 `ci-post` 는 705 가 아니라 895 입니다** — 190건이 그동안 빠져
   있었습니다. §5

---

## 2. 고친 것 — 관례를 따랐습니다

`modules/ai/tests/test_alert_ai_processed.cpp`, `CallerExpectsOnnx()`:

```cpp
#ifdef _MSC_VER
    size_t len = 0;
    char buf[8] = {0};
    const bool present =
        (getenv_s(&len, buf, sizeof(buf), "XPE_AI_EXPECT_ONNX") == 0) && len > 1;
    return present && std::string(buf) == "1";
#else
    const char* v = std::getenv("XPE_AI_EXPECT_ONNX");
    return v && std::string(v) == "1";
#endif
```

`test_onnx_session.cpp:234-240` 과 **같은 형태**입니다. 왜 그렇게 생겼는지를
주석으로 남겼습니다 — `/wd4996` 은 제품 타깃에만 붙고 `xpe_ai_tests` 는 `/WX`
를 받는 **유일한** 시험 타깃(`QA-B-164` 실측)이라, 시험에서 `std::getenv` 를
쓰면 `C4996 → C2220` 입니다.

---

## 3. 왜 제 `ci-post 705/705` 는 통과했나 — **AI 를 하나도 안 지었습니다**

카드의 가설 셋을 순서대로 쟀습니다.

### 가설 1 — `ci-post` 가 `xpe_ai_tests` 를 안 짓는다 → **이것입니다**

```
$ ls build/ci-post/bin/xpe_ai_tests.exe
  xpe_ai_tests.exe 없음

$ grep -o "xpe_ai[a-z_]*" build/ci-post/build.ninja | sort -u
  (출력 없음 — xpe_ai 관련 타깃이 하나도 없음)

$ grep -c "AiProcessedAlert\|BoneSuppressAbi" build/g168-v-post-ctest.txt
  0
```

**제가 보고한 705 안에 AI 시험이 0건이었습니다.** DLL 도 없습니다.

원인은 프리셋이 아니라 **제 디스크의 캐시**입니다:

```
$ grep BUILD_AI build/ci-post/CMakeCache.txt
  BUILD_AI:BOOL=OFF                     <- 캐시
$ (CMakePresets.json 의 ci-post)
  "BUILD_AI": "ON"                      <- 프리셋

$ ls -la build/ci-post/CMakeCache.txt        Sep 11 05:46
$ git log -1 -S'"BUILD_AI": "ON"' -- CMakePresets.json
  5215e0f ci(ai): ONNX 추론을 실제로 돌리는 ai-onnx 잡과 ci-ai 프리셋
```

**캐시가 프리셋보다 오래됐습니다.** `cmake --build build\ci-post` 는 이미 있는
캐시를 그대로 쓰므로 `BUILD_AI=OFF` 가 계속 살아 있었고, CMake 는 그것을
오류로 보지 않습니다 — **요청한 대로 지었고, 그냥 AI 가 그 안에 없었습니다.**

### 가설 2 — 짓는데 `/WX` 가 안 붙는다 → **아닙니다** (다만 제 다른 빌드는 그랬습니다)

프리셋으로 새로 짓고 컴파일 플래그를 봤습니다:

```
build/ci-post  (프리셋 재생성 후)   /W4 /WX /wd4251 /wd4275      <- /WX 있음
```

**`ci-post` 는 `/WX` 를 정상적으로 붙입니다.**

다만 **제가 `QA-B-168` 에서 쓴 `g168-stub`/`g168-full` 은 붙이지 않았습니다**:

```
build/g168-stub   XPE_WARNINGS_AS_ERRORS:BOOL=OFF   /W4 /wd4251 /wd4275
build/g168-full   XPE_WARNINGS_AS_ERRORS:BOOL=OFF   /W4 /wd4251 /wd4275
```

제가 프리셋 대신 `-D` 플래그를 직접 줘서 만든 디렉터리라 `XPE_WARNINGS_AS_ERRORS`
가 기본값 `OFF` 였습니다. **그래서 `getenv` 가 경고로만 났고 빌드는 통과했습니다.**

> **두 구멍이 겹쳤습니다.** 제가 "두 구성에서 돌렸다" 고 한 두 구성은 **둘 다
> `/WX` 가 없었고**, `/WX` 가 있는 구성(`ci-post`)은 **그 파일을 짓지 않았습니다.**
> 어느 한쪽만 있었어도 제 쪽에서 빨강이 났을 것입니다.

### 가설 3 — 컴파일러 버전 차이 → **아닙니다**

```
로컬   MSVC 14.44.35207 (VS 2022 Professional)
CI     MSVC 14.51.36231 (VS 18)
```

버전은 다릅니다만 **원인이 아닙니다** — 로컬 14.44 에서도 `/WX` 를 붙이자
**같은 `C2220` 이 납니다**(§4). `C4996` 은 두 버전 모두에 있습니다.

---

## 4. 반증 — 양방향

### 고친 뒤 (프리셋으로 새로 생성)

```
===POST_CFG=0===  ===POST_BUILD=0===  ===POST_CTEST=0===
  100% tests passed, 0 tests failed out of 895
===AI_CFG=0===    ===AI_BUILD=0===    ===AI_CTEST=0===
  100% tests passed, 0 tests failed out of 259     (XPE_AI_EXPECT_ONNX=1)
```

### `getenv` 를 도로 넣으면 — 같은 실패가 **로컬에서** 재현

```
주입 확인: grep -c "FALSIFY-169A"  ->  1
===FX_BUILD=1===

test_alert_ai_processed.cpp(86): error C2220: 다음 경고는 오류로 처리됩니다.
test_alert_ai_processed.cpp(86): warning C4996: 'getenv': This function or
                                 variable may be unsafe...
```

**CI 가 낸 것과 같은 두 줄입니다.** 원복 후 재확인: `FALSIFY-169A` **0건**,
`getenv_s` 있음.

---

## 5. 검증 — 그리고 **190건이 돌아왔습니다**

```
ci-post   (프리셋 재생성)  895 / 895 통과      <- 직전 보고는 705
ci-ai     (프리셋 재생성)  259 / 259 통과
ci-post 안의 AI 시험      83줄 관측 (직전 0)
```

**`705 → 895`, 차이 190 이 `xpe_ai_tests` 입니다** — `QA-B-168` 의 풀 빌드에서
제가 센 190건과 같은 수입니다. 그동안 `ci-post` 를 보고할 때마다 AI 는 빠져
있었습니다.

종료 코드는 전부 파이프 없이. 스크립트: `_run.bat`, `_falsify.bat`.
건드린 것: `test_alert_ai_processed.cpp` **한 함수**. `QA-B-169` 는 착수하지
않았습니다(카드 §하지 말 것).

## 6. 미검증 · 잔여 위험

- **`ci-post` 프리셋 자체는 처음부터 옳았습니다.** 고칠 것이 없습니다 — 문제는
  **제 로컬 캐시**였고, 그것은 커밋되지 않는 것이라 **이 커밋으로는 재발을 막지
  못합니다.** 무엇을 세울지는 §7
- **다른 레인·다른 세션의 오래된 빌드 디렉터리는 확인하지 않았습니다.** 같은
  함정이 `xpe-pre`·`xpe-gui` 쪽에도 있는지 모릅니다
- **CI 의 실제 실패 로그를 직접 읽지 않았습니다.** 카드가 인용한 두 줄과 제가
  로컬에서 재현한 두 줄이 같다는 것까지가 제 관측입니다
- **`ci-ai` 는 왜 안 깨졌는지 따로 확인하지 않았습니다.** `259/259` 초록인데,
  그 잡이 이 파일을 `/WX` 로 짓는지 아니면 다른 이유인지 구별하지 않았습니다
  — 리더의 CI 로그 쪽에서 보이는 사실입니다

## 7. 재발을 막으려면 (리더 판단 사항 — 저는 손대지 않았습니다)

제가 고친 한 줄은 **이번 것만** 막습니다. 구조적으로 남는 것은 둘입니다:

1. **레인이 "전체 통과" 를 보고할 때 쓰는 명령이 프리셋이어야 합니다.**
   `cmake --build <기존 디렉터리>` 는 그 디렉터리가 무엇으로 설정됐는지 묻지
   않습니다. `cmake --preset` 로 재설정하면 캐시와 프리셋이 어긋날 수 없습니다
2. **시험 수가 대조군입니다.** `705` 와 `895` 는 눈으로 구별되는데, 저는
   `705` 를 이전 보고와 비교하지 않았습니다. `QA-B-154` 때 `575→731` 로
   같은 함정을 한 번 잡았던 형태입니다 — **그때는 셌고 이번에는 안 셌습니다**

---

Refs #130
