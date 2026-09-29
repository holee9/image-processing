# QA-A-74 — 죽은 것은 이식의 흔적이 아니라 다른 연산이었다 (#160)

**카드**: `.moai/lanes/pre/inbox/QA-A-74.md` · **브랜치**: `dev/preprocess` · **기계**: Intel Core i7-12700

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | 커널 넷을 지웠다. 호출 0을 **리더 숫자를 받지 않고 직접 세어** 확인했다 |
| C2 | **A-73 에서 제가 하나를 빠뜨린 원인을 찾았다** — grep 패턴이 **파일 이름**에 걸렸고 그것을 호출로 읽었다 |
| C3 | 삭제 근거를 카드가 정한 대로 적었다: "안 쓰여서"가 아니라 **"지금 쓰는 연산이 아니어서"** |
| C4 | 넷 모두 `fan_in = 2` 라고 적혀 있었고 **실측 0** 이다. 같은 파일에 남은 `fan_in >= 3` 하나는 **확인했고 참이다** |
| C5 | 지나는 테스트 전부 통과. 리더 판정이 틀렸다는 신호는 없다 |
| C6 | 전체 643/69 통과, 경고 0, `BUILD_EXIT=0` |

---

## 2. 증거 (Evidence)

### C1 — 직접 세었다

리더가 넷이라고 했고, 제 A-73 목록은 셋이었다. **받지 않고 다시 셌다** — 정의와 호출을 따로 집계하고, 정의 줄 자체를 호출 집계에서 제외했다:

```
offset_correct_scalar              defs=1 calls=0
offset_correct_avx2                defs=1 calls=0
offset_correct_avx512              defs=1 calls=0
offset_correct_neon                defs=1 calls=0
offset_correct_float_avx2          defs=1 calls=1     <- 살아 있는 것
xpe_offset_apply_scalar_reference  defs=1 calls=3     <- 살아 있는 것
```

**리더가 맞다. 넷이다.**

### C2 — 제가 왜 빠뜨렸는가 (원인을 찾았다)

A-73 에서 저는 이렇게 적었다:

> `offset_correct_avx2` … refs=1 files=… **test_offset_correct_avx2_parity.cpp**

그리고 그것을 "테스트가 부른다"로 읽었다. **틀렸다. 걸린 것은 호출이 아니라 파일 이름이다** — `test_offset_correct_avx2_parity.cpp` 라는 문자열 안에 `offset_correct_avx2` 가 들어 있다. 제 grep 이 파일 목록을 찍었고, 저는 그 목록을 호출 목록으로 읽었다.

**"이름으로 세지 말라"는 이 저장소의 교훈을 제가 이번에 다시 밟았다.** 이번에는 `함수명(` 으로 세고 정의 줄을 빼는 형태로 고쳤고, 그 차이가 결과를 바꿨다.

### C3 — 지운 근거

카드의 판정을 그대로 받아 적었다. 제가 세운 프레임("플랫폼 이식의 흔적일 수 있다")이 틀렸고, 서명을 나란히 놓으면 보인다:

| | 살아 있는 경로 | 지운 넷 |
|---|---|---|
| 서명 | `(const uint16_t* src, const float* off, uint16_t* dst, n)` | `(uint16_t* dst, const uint16_t* off, n)` — in-place |
| 오프셋맵 | **float** | **uint16** |
| 연산 | 실수 뺄셈 → 0 하한 → 65535 상한 → `+0.5` 반올림 | 정수 포화 뺄셈 |

**NEON·AVX-512 판은 살아 있는 코드의 이식판이 아니라 죽은 코드의 이식판이었다.** 훗날 aarch64 를 하게 되면 옮겨야 할 대상은 float 경로이고, 이 넷을 남겨두면 그때 사람이 집어드는 것이 **틀린 출발점**이다. 그 문장을 삭제 지점에 남겼다.

같이 없어진 것: `#if defined(__AVX512F__)` 블록 전체, `#if defined(__aarch64__)` 블록 전체와 그 안의 `#include <arm_neon.h>`. 그리고 살아남은 `#if` 위의 머리 주석이 **지워진 커널을 설명하고 있어서**(`_mm256_subs_epu16` 운운) 남는 것을 설명하도록 다시 썼다.

### C4 — `fan_in = 2` 는 넷 다 거짓이었다

지운 넷이 전부 이렇게 달고 있었다:

```
// @MX:ANCHOR: [AUTO] offset_correct_avx2 — AVX2 saturating subtraction kernel
// @MX:REASON: Performance-critical hot path; processes 16 pixels per iteration; fan_in = 2
```

**실측 0 이다.** `@MX:ANCHOR` 는 호출자가 많은 함수에 붙는 표시인데, 여기서는 **호출자가 없다는 사실을 가리는 표시**로 기능했다 — 읽는 사람이 "앵커니까 중요한 것"으로 지나간다.

`[AUTO]` 표시가 붙어 있으니 도구가 붙인 것으로 보인다. **정의를 보고 숫자를 지어냈다면 같은 오류가 다른 파일에도 있다** — 카드가 범위 밖이라고 했으므로 세지 않았다.

**같은 파일에 하나 남았고, 그것은 확인했다.** 진입점 `xpe_offset_correct`(:142)에 `fan_in >= 3` 이 붙어 있다:

```
$ grep -rn "xpe_offset_correct(" --include=*.cpp --include=*.h modules/ | (선언 제외) | wc -l
44
```

**44 건. 참이다.** 남긴 이유는 "안 건드려서"가 아니라 **세어 봤더니 맞아서**다.

### C5 — 반증 (리더 판정이 틀렸는지)

카드가 "하나라도 깨지면 내 판정이 틀린 것이니 멈추고 보고하라"고 했다. 깨지지 않았다:

```
[offset-parity] 1024x768  differing=0 of 786432  worst_gap=0
[       OK ] OffsetCorrectAVX2ParityTest.ShippedPathMatchesTheScalarReference
[       OK ] OffsetCorrectAVX2ParityTest.MultipleCallsAreBitIdentical
[       OK ] OffsetCorrectAVX2ParityTest.ParityWithNonMultipleStride
[       OK ] OffsetCorrectTest.* (3건)
[  PASSED  ] 16 tests.
```

그리고 `xpe_offset_correct` 를 지나는 10개 파일이 전체 실행에서 전부 통과한다(§C6).

**주의해서 읽을 것**: 이 통과는 "지운 것이 안 쓰였다"를 보이지, "지운 것이 필요 없었다"를 보이지 않는다. 후자는 §C3 의 서명 비교가 말한다.

### C6 — 빌드와 전체

```
PRE_BUILD_EXIT=0 / COMMON_BUILD_EXIT=0
100% tests passed, 0 tests failed out of 643
100% tests passed, 0 tests failed out of 69
```

643 은 A-73 과 같다 — 이번에 테스트를 더하거나 빼지 않았다. 경고 0.

**낡은 바이너리가 아니다**: `BUILD_EXIT` 을 읽었고, 두 프리셋 모두 0이다. (이 절차가 이 레인에서 다섯 번 구제했다.)

---

## 3. baseline 귀속 (Baseline-attribution)

- 호출 수는 **이번 실행의 grep 결과**이며 A-73 의 숫자도 리더의 숫자도 아니다.
- `44` 는 `xpe_offset_correct(` 에서 선언 줄을 뺀 수다.
- `[offset-parity]` 줄은 A-73 이 추가한 테스트의 이번 실행 출력이다.
- CI 결과(`7b29995` Documentation Generation = success)는 **리더가 읽어 전달한 값**이며 내가 확인할 수 없다.

---

## 4. 미검증 (Gaps)

- **`_avx512` · `_neon` 은 이 빌드에서 컴파일되지 않던 코드다.** 지워도 이 기계에서는 아무 차이가 없고, 그래서 **"지워서 안전한가"를 컴파일로 확인할 수 없는 부분**이 남는다. 확인한 것은 x86 빌드가 그대로 돈다는 것뿐이다.
- **다른 파일의 `fan_in` 주석은 세지 않았다** — 카드 범위 밖. 다만 `[AUTO]` 도구가 같은 방식으로 붙였다면 같은 오류가 더 있을 것이라는 점은 §C4 에 적었다.
- **aarch64 빌드는 여전히 이 레인에 없다.** NEON 블록을 지운 것이 그 플랫폼에서 무엇을 뜻하는지 빌드로 확인하지 않았다 — 지운 함수가 호출 0이었다는 사실에만 근거한다.
- **Doxygen 로컬 확인 불가** — 여전(#159). 이번에는 헤더를 바꾸지 않았다.

---

## 5. 잔여 위험 (Residual-risk)

- **`@MX:ANCHOR` 가 가리는 힘이 있다는 것이 이번 사례로 드러났다.** 넷 다 "중요한 함수" 표시를 달고 호출 0이었다. 그 태그를 신뢰 신호로 읽으면 다음에도 같은 일이 난다 — 태그는 확인의 대상이지 확인의 결과가 아니다.
- **파일이 이제 float 경로 하나만 담는다.** 단순해진 대신, 다른 연산이 필요해지면 처음부터 써야 한다. 그것이 이번 판정의 내용이므로 위험이라기보다 결과이지만, 적어 둔다.
- **내 grep 습관이 한 번 틀렸다**(§C2). 같은 형태를 다음에도 쓸 수 있으므로, 부재/호출 수를 셀 때는 **`이름(` 으로 세고 정의 줄을 빼는** 형태를 쓴다.

---

Refs #160
