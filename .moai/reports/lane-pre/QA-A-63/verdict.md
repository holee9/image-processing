# QA-A-63 — `DetectFrame` 가 맵 길이를 받는다

**카드**: `.moai/lanes/pre/inbox/QA-A-63.md` · **브랜치**: `dev/preprocess` · **기계**: Intel Core i7-12700

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | `DetectFrame` 이 `size_t mapCount` 를 받는다. 단위는 **ELEMENTS** 이며 gsvg 선례(`srcCount`/`dstCount`/`gainCount`, #152)의 이름·단위 규약을 따른다 |
| C2 | `mapCount < width * height` 이면 **쓰기 전에 거절**한다. 잘라 쓰지 않는다 — memset 앞에 검사가 온다 |
| C3 | 호출자 12곳(파리티 테스트 11, 실험 도구 1)이 모두 갱신됐고 남은 3인자 호출은 없다 |
| C4 | 가드 페이지 반증이 **"넘겨준 길이 밖을 안 건드린다"** 를 본다. 검사를 약화시키면 그 단언이 실패한다 |
| C5 | 전체 스위트 628/69 통과, 경고 0, 성능 게이트 ratio 7.193 (limit 10.000) |

---

## 2. 증거 (Evidence)

### C1 · C2 — 시그니처와 검사 순서

`modules/preprocess/include/runtime_detection.h:972`

```cpp
inline void DetectFrame(const XpeImageBuffer* img,
                        RuntimeDetectionConfig config,
                        uint8_t* map,
                        size_t mapCount) {
    if (img == nullptr || img->data == nullptr || map == nullptr) return;
    const uint32_t T = RuntimeDetection_NormalizeThreads(config.threadCount);
    const uint32_t w = img->width;
    const uint32_t h = img->height;

    // QA-A-63: before the memset, not after. See the note above.
    const size_t needed = static_cast<size_t>(w) * static_cast<size_t>(h);
    if (mapCount < needed) return;

    // QA-A-62: clear it here rather than trusting a comment. See the note above.
    std::memset(map, 0, needed);
```

`@param mapCount` 는 "Number of uint8 ELEMENTS @p map points at" 로, gsvg 와 같은 문구 형태를 쓴다. 맵 원소가 `uint8_t` 라 여기서는 요소 수와 바이트 수가 우연히 같지만, 단위를 ELEMENTS 로 못 박은 이유를 헤더에 적었다 — 검사가 비교하는 대상이 `width * height` 이고, 저장소에 이 인자에 대한 이름 규약이 이미 하나 있으며(그쪽은 원소가 1바이트가 아니다), 나중에 맵 원소 타입이 바뀌어도 숫자의 뜻이 조용히 달라지지 않는다.

거절이 `void` 반환인 이유도 적었다 — 이 함수에는 오류 채널이 없고, 바로 위 NULL 가드가 이미 같은 방식으로 거절한다.

### C3 — 호출자

```
$ grep -rn "DetectFrame(" modules/preprocess/tests modules/preprocess/tools | grep -v "size()"
(출력 없음)
```

갱신: `test_runtime_detection_thread_parity.cpp` 11곳, `xpe_detect_experiment.cpp` 1곳. 모두 자기 버퍼 크기를 알고 있어 `map.size()` 를 그대로 넘긴다.

### C4 — 반증 (가드 페이지)

새 파일 `modules/preprocess/tests/test_runtime_detection_map_length.cpp`. 픽스처는 QA-B-53 이 gsvg 에서 쓴 것과 같은 형태 — `VirtualAlloc` 로 잡은 영역 뒤에 `PAGE_NOACCESS` 페이지를 붙이고, 맵의 끝이 그 경계에 정확히 닿게 배치한다. 약속한 길이를 한 바이트라도 넘어 쓰면 접근 위반이 나고 `__except` 가 그것을 신호로 바꾼다.

단언이 둘이다. **넘어 썼는가**(가드 페이지)와 **일부라도 건드렸는가**(버퍼를 `0xCD` 로 채우고 변한 바이트를 센다). 두 번째가 있어야 "자르기"를 거절과 구분한다.

**현재 코드 (검사 있음)** — `BUILD_EXIT=0`, `Linking CXX executable bin\xpe_preprocess_tests.exe`:

```
[       OK ] MapLengthTest.ExactLengthMapIsFilledAndDoesNotFault (0 ms)
[       OK ] MapLengthTest.ShortMapIsRejectedWithoutBeingWritten (0 ms)
[       OK ] MapLengthTest.ShortMapIsRejectedOnTheThreadedPathToo (0 ms)
[       OK ] MapLengthTest.SurplusBeyondWidthTimesHeightIsNotTouched (0 ms)
GTEST_EXIT=0
```

**검사를 약화시킨 뒤** (`if (mapCount < needed)` → 항상 통과하는 `if (mapCount < weakened)`, `weakened = 0`) — 재빌드 `BUILD_EXIT=0`, 즉 낡은 바이너리가 아니다:

```
[ RUN      ] MapLengthTest.ShortMapIsRejectedWithoutBeingWritten
...(168): error: Value of: CallFaulted(&img, cfg, map.data(), shortCount)
wrote past the 2048 elements it was given (frame needs 4096)
...(174): error: Expected equality of these values:
2016 of 2048 bytes changed: the call was truncated rather than rejected
[  FAILED  ] MapLengthTest.ShortMapIsRejectedWithoutBeingWritten (0 ms)
[  FAILED  ] MapLengthTest.ShortMapIsRejectedOnTheThreadedPathToo (0 ms)
 2 FAILED TESTS
GTEST_EXIT=1
```

두 단언이 모두 깨졌다. 그리고 `2016 of 2048` 이라는 숫자가 이 카드가 막으려는 실패 모양을 그대로 보여준다 — memset 이 2048바이트 버퍼에 4096바이트를 쓰다가 2016바이트째에 페이지 경계를 만나 죽었다. 가드 페이지가 없는 실제 호출자였다면 그 뒤 2048바이트는 힙의 남의 메모리였을 것이고, 죽지도 않았을 것이다.

검사는 즉시 복원했다 (`runtime_detection.h:1012`).

### C5 — 전체 스위트

```
PRE_BUILD_EXIT=0
COMMON_BUILD_EXIT=0
100% tests passed, 0 tests failed out of 628
PRE=0
100% tests passed, 0 tests failed out of 69
COMMON=0
```

경고 0 (`/WX` 이므로 빌드 성공이 곧 경고 0).

성능 게이트:

```
[perf-gate] 3072x3072 single thread, warm, min of 5: 692.7 ms  (samples 692.7 / 718.4 / 900.9 / 988.7 / 1126.3)
[perf-gate] reference kernel: 96.3 ms
[perf-gate-ratio] 3072 ratio=7.193 limit=10.000
```

---

## 3. baseline 귀속 (Baseline-attribution)

- **테스트 수**: 직전 QA-A-62 커밋 시점 624(ci-preprocess) / 69(ci-common) → 이번 628 / 69. 증가분 4는 이번에 추가한 `MapLengthTest` 4건이고, 기존 624건 중 실패는 없다.
- **성능**: ratio 7.193 은 이번 실행에서 측정한 값이며, 같은 기계에서 A-60~A-62 가 기록한 7.1~7.4 범위 안이다. 길이 검사는 프레임당 비교 1회라 측정 분산(이 실행의 표본 폭만 봐도 692.7~1126.3) 아래에 있다 — **"검사가 공짜"라고 주장하지 않는다. 이 측정으로는 분간되지 않는다**가 관측된 것이다.
- 모든 수치는 이 워크트리, 이 실행에서 나온 것이다.

---

## 4. 미검증 (Gaps)

- **거짓말하는 호출자**: `mapCount == w*h` 를 넘기면서 실제 버퍼가 더 짧은 경우. 함수 안에서 잡을 방법이 없다 — 길이는 호출자가 자기 메모리에 대해 하는 진술이다. 테스트 파일 상단에 명시했다.
- **비-Windows 경로**: 가드 페이지 픽스처가 `VirtualAlloc`/`VirtualProtect` 를 쓴다. 다른 플랫폼에서는 `GTEST_SKIP` 이다. 이 프로젝트는 MSVC 전용이지만, 스킵이 통과로 읽히지 않도록 적어 둔다.
- **Doxygen 로컬 확인 불가**: 여전히 로컬에 Doxygen 이 없다. 카드대로 리더가 CI 에서 읽는다(#159).
- **`--regress diff` 비교 없음**: A-62 에서 확인했듯 그 도구 경로는 `DetectFrame` 을 부르지 않는다. 이번 변경의 동작 불변성은 기존 624건 통과로만 뒷받침된다.
- **공개 진입점**: `xpe_defect_detect_runtime` 의 시그니처는 건드리지 않았다. `DetectFrame` 은 내부 함수이고 내보내기는 그대로다.

---

## 5. 잔여 위험 (Residual-risk)

- **한 인자 더 늘어난 내부 함수**: 새 호출자가 길이를 틀리게 넘길 여지가 생겼다. 현재 호출자 12곳은 모두 `.size()` 를 그대로 쓰므로 틀릴 구석이 없지만, 상수를 직접 적는 호출자가 생기면 그때는 검사가 아니라 관행이 방어선이다.
- **거절이 조용하다**: 반환형이 `void` 라 거절이 호출자에게 보이지 않는다. NULL 가드와 같은 수준이고 이 함수의 기존 계약과 일치하지만, 길이를 잘못 준 호출자는 "맵이 전부 0" 을 보게 된다 — 이는 "결함 없음" 과 구분되지 않는다. 공개 진입점처럼 오류 코드를 돌려주는 형태가 필요해지면 그때 열어야 할 결정이다(지금은 소비자가 없다).
- **성능 게이트 표본 폭**: 이번 실행의 3072² 표본이 692.7~1126.3 으로 넓다. 게이트는 min 을 쓰고 기준 커널로 나누므로 통과했지만, 이 기계가 다른 작업으로 바쁠 때 찍힌 수치라는 뜻이다.

---

Refs #144 #143
