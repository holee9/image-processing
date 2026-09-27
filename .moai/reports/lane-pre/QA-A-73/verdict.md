# QA-A-73 — `offset_correct.cpp` 의 둘, 그리고 조용히 죽는 기준 구현 (#160)

**카드**: `.moai/lanes/pre/inbox/QA-A-73.md` · **브랜치**: `dev/preprocess` · **기계**: Intel Core i7-12700

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | `xpe_has_avx2()` 를 지웠다 — **보호할 수 없어서**. 그리고 그 몸통이 **배송 구성에서는 컴파일 타임 `true`** 였다는 것을 확인했다 |
| C2 | `xpe_has_avx512f()` 를 지웠다 — **쓰이지 않아서**. 근거를 카드가 지정한 대로 구분해 적었다 |
| C3 | 둘 다 `static` 이 아니지만 **파일 밖 사용 0건**이라 ABI 를 좁히지 않는다. 재확인했다 |
| C4 | **`xpe_offset_correct` 를 지나는 경로를 먼저 찾았다** — A-72 와 달리 기존 테스트 10개 파일이 지난다 |
| C5 | **기존 offset 파리티도 결정성만 본다.** gain 과 같은 형태였고, 같은 손이 썼다는 추정이 맞았다 |
| C6 | 허용치를 **offset 쪽에서 가져왔다**: `REQ-P1A-010` 이 "bit-identical" 이라고 말한다. AC-GAIN-004 를 쓰지 않았다 |
| C7 | 조용히 죽는 기준 구현에 **코드가 말하게** 했다. 기계적 가드는 만들지 않았다 |
| C8 | 전체 643/69 통과, 경고 0 |

---

## 2. 증거 (Evidence)

### C1 · C2 — 둘을 다른 근거로 지웠다

**`xpe_has_avx2()`** — `gain_correct` 와 같은 이유다. 모듈이 `/arch:AVX2` 로 컴파일되므로 그 검사에 도달하는 코드에서도 AVX2 명령이 나올 수 있다.

**그리고 읽어 보니 더 분명했다.** 몸통이 이렇게 시작한다:

```cpp
bool xpe_has_avx2() noexcept
{
#if defined(__AVX2__)
    // Compile-time known: AVX2 is available
    return true;
#elif defined(_MSC_VER)
    ... __cpuidex / _xgetbv ...
```

**`/arch:AVX2` 는 `__AVX2__` 를 정의한다.** 즉 배송 구성에서 이 함수는 **런타임 검출이 아니라 컴파일 타임 `true`** 였다. CPUID 를 부르는 코드는 그 빌드에서 **도달조차 하지 않는다.** A-72 의 `xpe_gain_has_avx2` 는 그 단축이 없어 실제로 CPUID 를 돌렸는데, 이쪽은 한 단계 더 비어 있었다.

**`xpe_has_avx512f()`** — 카드가 지정한 대로 **다른 근거**로 지웠다. AVX-512 는 `/arch:AVX2` 가 함의하지 않으므로 이 프로브는 **원리적으로는 보호할 수 있었다.** 지우는 이유는 **호출자가 0** 이라는 것이다. 호출자 없는 검출 함수는 나중에 AVX-512 경로를 쓸 사람이 **그대로 믿고 쓸 위험**이 있고, 그때 필요한 것은 그 시점 요구에 맞춰 새로 쓴 프로브다. **A-61 의 기준과 같다 — 존재하지 않는 소비자를 근거로 살아남는 것은 없다.**

두 근거를 코드 주석에 나눠 적었다. 지운 자리에 남긴 문단이 **"왜 지웠는가"를 두 갈래로** 말한다.

**그리고 같이 정리된 것 하나**: 디스패치의 `#if defined(__aarch64__) { scalar } #else { scalar }` 두 갈래가 **같은 함수를 부르고 있었다.** 아무 말도 하지 않는 분기라 프로브와 함께 없앴다. 남은 것은 플랫폼별 컴파일 타임 선택 하나다.

### C3 — ABI

둘 다 `static` 이 아니어서 심볼에 외부 링크가 있었다. **파일 밖 사용 0건** 은 A-72 에서 `modules/` 전체를 훑어 확인했고 이번에 다시 확인했다:

```
$ grep -rn "xpe_has_avx2\|xpe_has_avx512f" --include=*.cpp --include=*.h modules/ | grep -v offset_correct.cpp
modules/preprocess/tests/test_offset_correct_avx2_parity.cpp:136:  (이번에 쓴 주석 한 줄)
```

즉 코드 참조는 0이다. 그리고 DLL 은 이 심볼을 내보내지 않으므로(`__declspec(dllexport)` 없음) **밖에서 링크할 수도 없었다.** 지우는 것이 ABI 를 좁히지 않는다.

### C4 — 지나는 경로를 먼저 찾았다 (A-72 의 실수를 반복하지 않음)

A-72 에서 `--regress` 가 `xpe_gain_correct` 를 한 번도 안 부른다는 것을 착수 전에 걸렀다. 이번에도 먼저 물었다 — **이쪽은 다르다.** `xpe_offset_correct` 를 부르는 테스트 파일이 **10개** 있다:

```
test_boundary.cpp  test_calibration_roundtrip.cpp  test_error_precedence.cpp
test_golden_reference.cpp  test_integration.cpp  test_offset_correct.cpp
test_offset_correct_avx2_parity.cpp  test_preprocess_degraded.cpp
test_xpe_preprocess.cpp  test_xpe_preprocess_correction.cpp
```

그래서 경로를 만들 필요가 없었고, 그중 파리티 파일에 단언을 더했다.

### C5 — 기존 offset 파리티도 결정성만 본다

카드의 추정("같은 손이 썼을 가능성")이 맞았다. 두 테스트 다 **두 번 돌려 서로 비교**한다:

```cpp
TEST_F(OffsetCorrectAVX2ParityTest, MultipleCallsAreBitIdentical) {
    ASSERT_EQ(XPE_OK, xpe_offset_correct(&input1, &output1, &metadata));
    ASSERT_EQ(XPE_OK, xpe_offset_correct(&input2, &output2, &metadata));
    ...outputPixels1[i] == outputPixels2[i]...
```

독립적인 것과 비교하지 않으므로 **일관되게 틀린 커널도, 아예 안 도는 커널도 통과한다.** gain 쪽에서 A-72 가 이것을 측정으로 보였고, 이번에 반증이 다시 보인다(§C6 끝).

### C6 — 허용치는 offset 쪽에서

**AC-GAIN-004 를 그대로 쓰지 않았다.** 연산이 다르다 — gain 은 float 곱이고 1 ULP 를 허용하지만, offset 은 float 로 빼고 클램프한 뒤 **uint16 으로 내보낸다.**

`offset_correct.cpp` 전체에 **ULP 상수도 `AC-OFFSET-*` 도 없다.** 대신 요구 자체가 말한다:

```
REQ-P1A-010: AVX2 implementation (branch-free saturating subtract)
  ... Bit-identical to scalar version — verified by test suite.
```

그래서 허용치는 **정확 일치**이고, 그 출처를 테스트 주석에 적었다. **그리고 그 문장의 뒷부분("verified by test suite")은 지금까지 참이 아니었다** — 스위트가 보던 것은 결정성뿐이었다. 이번에 참이 됐다.

**측정:**

```
[offset-parity] 1024x768  differing=0 of 786432  worst_gap=0
[       OK ] OffsetCorrectAVX2ParityTest.ShippedPathMatchesTheScalarReference
```

오프셋 맵은 −500 … 6998.5 로 만들어 **세 갈래를 모두 지나게** 했다 — 그냥 빼는 값, 0 아래로 내려가 클램프되는 값, `+0.5` 반올림 경계. 상수 맵이면 클램프가 안 걸린다.

**반증**: AVX2 커널의 `round_bias` 를 `0.5f` → `0.0f`. `BUILD_EXIT=0`:

```
[offset-parity] 1024x768  differing=131245 of 786432  worst_gap=1
131245 pixels differ ... first at index 1 (shipped 3278 vs reference 3279);
  REQ-P1A-010 requires bit-identical
[  FAILED  ] ShippedPathMatchesTheScalarReference
[       OK ] MultipleCallsAreBitIdentical      <-- 통과한다
[       OK ] ParityWithNonMultipleStride       <-- 통과한다
```

**새 단언만 터진다.** 그리고 진단이 결함의 성격까지 말한다 — `worst_gap=1` 이 13만 화소에 걸쳐 있으면 **반올림·클램프 차이**이지 커널이 망가진 것이 아니다. 원복 후 3/3 통과.

### C7 — 조용히 죽는 기준 구현: 코드가 말하게 했다

카드의 지시대로 **기계적 가드를 만들지 않았다.** "이 함수가 호출되는지 검사하는 검사" 는 이 저장소가 반복해 만난 형태를 하나 더 만든다.

대신 **이름과 주석이 말하게** 했다. 두 기준 구현(gain·offset) 모두 `xpe_*_apply_scalar_reference` 라는 이름을 갖고, 옆에 이렇게 적었다:

> **NOT A FALLBACK. THE COMPARAND.** … DELETING THAT TEST DELETES THE ONLY INDEPENDENT CHECK the AVX2 kernel has — the other tests in that file compare repeated calls with each other, which a consistently wrong kernel passes. It is also the only caller: with the test gone this is an unused inline, which no compiler warns about, so nothing would say the check had been lost.

**지우려는 사람이 읽을 자리에 있다** — 함수 선언 바로 위, 그리고 테스트 파일 머리. 지우는 행위 자체는 보이는 것이고, 문제는 **무엇을 함께 지우는지 모르는 것**이므로, 그 답을 양쪽에 놓았다.

### C8 — 회귀

```
PRE_BUILD_EXIT=0 / COMMON_BUILD_EXIT=0
100% tests passed, 0 tests failed out of 643
100% tests passed, 0 tests failed out of 69
```

643 = A-72 의 642 + 이번 1건. 경고 0. `xpe_has_avx*` 는 주석 세 줄로만 남았다.

---

## 3. baseline 귀속 (Baseline-attribution)

- `[offset-parity]` 두 줄(정상·반증)은 같은 세션, 같은 기계, 각각 `BUILD_EXIT=0` 확인 후의 결과다.
- 호출자 0건·파일 밖 사용 0건은 **이번에 다시 실행한 grep** 결과이며, A-72 의 기억이 아니다.
- `REQ-P1A-010` 인용은 `offset_correct.cpp:52` 원문이다.
- 지나는 경로 10개 파일 목록은 `grep -rln "xpe_offset_correct"` 결과다.

---

## 4. 미검증 (Gaps)

- **비-x86 경로는 여전히 미검증이다.** 디스패치가 이제 `#if defined(__AVX2__) || defined(_MSC_VER)` 로 갈리고, 그 밖의 갈래는 이 레인에서 빌드되지 않는다. A-71 의 구분이 그대로 남는다 — **"벡터를 안 쓰는 구성"과 "AVX2 없는 기계"는 다르다.**
- **`worst_gap=0` 은 이 오프셋 맵·이 입력 한 조합**에 대한 것이다. 1024×768, 오프셋 −500…6998.5, 입력 0…4000.
- **`offset_correct.cpp` 에 호출자 0인 커널이 더 있다**: `offset_correct_scalar`(uint16 in-place), `offset_correct_avx512`, `offset_correct_neon` 은 정의만 있고 부르는 곳이 없다(`offset_correct_avx2` 는 테스트가 부른다). **이번에 손대지 않았다** — 카드가 프로브 둘로 범위를 한정했다. §5 에 올린다.
- **Doxygen 로컬 확인 불가** — 여전(#159). 내부 헤더에 또 함수를 추가했으므로 CI 가 말해 줄 것이다.

---

## 5. 올리는 관찰 — 호출자 0인 커널 셋

`xpe_has_avx512f()` 를 "쓰이지 않아서" 지웠는데, **같은 판단이 걸리는 것이 세 개 더 있다**:

| 함수 | 줄 | 호출 |
|---|---|---|
| `offset_correct_scalar` (uint16 in-place) | 34 | 0 |
| `offset_correct_avx512` | 68 | 0 |
| `offset_correct_neon` | 243 | 0 |

셋 다 정의만 있고 부르는 곳이 없다(`modules/` 전체 grep). `offset_correct_avx512` 는 `#if defined(__AVX512F__)` 안이라 이 빌드에서는 컴파일도 되지 않고, `offset_correct_neon` 은 aarch64 용이다.

**`xpe_has_avx512f()` 와 성격이 같은지 다른지는 제가 정하지 않습니다.** 프로브는 "나중 사람이 믿고 쓸 위험" 이 근거였는데, 커널은 플랫폼 이식의 흔적일 수 있습니다. **판정을 요청합니다.**

---

## 6. 잔여 위험 (Residual-risk)

- **기준 구현 둘이 각각 테스트 하나에 달려 있다.** 코드가 그렇게 말하도록 했지만, 그것은 읽는 사람에게 거는 것이지 기계가 막는 것이 아니다. 카드가 지금은 그것이 맞다고 했고 동의하지만, **실제로 지워진 사례가 나오면 근거가 바뀐다.**
- **내부 헤더가 인라인 구현을 계속 받아들이고 있다.** 이제 검출·게인·오프셋 셋이다. 헤더를 포함하는 모든 TU 가 이 코드를 컴파일한다.
- **"bit-identical" 이라는 요구가 이제 실제로 검사된다** — 그래서 다음에 커널을 손대면 이 테스트가 막는다. 그것이 목적이지만, **반올림을 의도적으로 바꿀 이유가 생기면 요구부터 고쳐야 한다.**

---

Refs #160
