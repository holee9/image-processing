# QA-A-71 — Doxygen 1건, 그리고 스칼라 빌드가 실제로 도는가

**카드**: `.moai/lanes/pre/inbox/QA-A-71.md` · **브랜치**: `dev/preprocess` · **기계**: Intel Core i7-12700

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | Doxygen 오류의 원인은 A-68 주석 안의 `#if` 세 번이고, **문구만 고쳐 뜻을 유지**했다 |
| C2 | 스칼라 빌드(`XPE_DETECT_HAS_AVX2=0`)로 전체 스위트를 돌렸다. **632건 중 631건 통과, 1건 실패** — 실패는 성능 게이트이고 **게이트가 제 일을 한 것**이다 |
| C3 | AVX2 빌드와 스칼라 빌드가 3072² 에서 **같은 맵을 낸다**. 플래그 수와 다이제스트가 **완전히 일치**한다 |
| C4 | 그 단언이 스칼라 빌드를 **실제로 보고 있다**는 것을 반증으로 확인했다 |
| C5 | 다만 그 단언이 스칼라 빌드에서 **볼 수 없는 것**이 있다. 그것을 적고, 대신 무엇이 그 자리를 덮는지도 적는다 |
| C6 | AVX2 빌드 641/69 통과, 경고 0 |

---

## 2. 증거 (Evidence)

### C1 — Doxygen

```
runtime_detection.h:37: error: explicit link request to 'if' could not be resolved
```

원인은 A-68 이 쓴 `@def` 본문의 `#if` · `#ifdef` 세 번이다. Doxygen 은 `#단어` 를 링크 요청으로 읽는다. **문서화된 이스케이프 `\#`** 를 썼고, 렌더링 결과는 같다.

내용은 유지하되 **한 문장 더 분명하게** 했다 — 왜 `#ifdef` 가 틀리는지를 결과까지 적었다:

```
 * It is always defined -- both arms of the \#if below set it -- so
 * `\#if XPE_DETECT_HAS_AVX2` is the correct test and `\#ifdef` would be WRONG:
 * the macro is defined in the scalar build too, with the value 0, so an
 * existence test is true there and selects the vector branch that was not
 * compiled. Test the VALUE, not the existence.
```

같은 함정이 이 헤더에 또 있는지 확인했다 — 주석 안의 다른 `#단어` 는 없다(`grep "[ (\`]#[a-zA-Z]"` 결과 0건; `#144` 같은 숫자 참조는 이 오류를 내지 않는다).

### C2 — 스칼라 빌드 전체 스위트

`XPE_DETECT_HAS_AVX2` 를 0 으로 강제하고 두 프리셋을 빌드·실행했다. **`PRE_BUILD_EXIT=0`, `COMMON_BUILD_EXIT=0`** — 낡은 바이너리가 아니다.

```
99% tests passed, 1 tests failed out of 632        (ci-preprocess)
100% tests passed, 0 tests failed out of 69        (ci-common)
```

**AVX2 빌드는 641건, 스칼라 빌드는 632건.** 차이 9건의 내역:

- `Avx2ParityTest` 는 AVX2 빌드에서 **10건**이다.
- 스칼라 빌드에서는 그중 9건이 `#if` 로 **컴파일조차 되지 않아 등록되지 않고**, 남은 1건 `NoAvx2PathCompiledIn` 이 등록되어 **SKIP** 한다.
- 641 − 10 + 1 = 632. 맞는다.

**그래서 "SKIP 이 아닌 것들이 무엇을 보는가"** — 나머지 631건이 전부 돈다. 검출 규칙 자체를 보는 것들(`RuntimeDetectionFunctionalTest`, `...RatesTest`, `Median8ParityTest`, `RadixSelectParityTest`, `ThreadParityTest`, `MapLengthTest`)이 **스칼라 빌드에서 그대로 통과한다.** 스칼라 경로가 옳은 답을 낸다는 주장의 본체는 이쪽이다.

**실패 1건은 성능 게이트다:**

```
[perf-gate] 3072x3072 single thread, warm, min of 5: 598.7 ms
[perf-gate] reference kernel: 96.3 ms
[perf-gate-ratio] 3072 ratio=6.214 limit=0.850
619/633 Test #619: RuntimeDetectionPerformanceGateTest.Frame3072SquaredWithinMachineRatio ***Failed
```

**이것은 결함이 아니라 게이트가 제 일을 한 것이다.** A-67 이 상한 0.85 를 유도할 때 **회귀 대역으로 쓴 것이 바로 이 구성**이었다 — 그때 측정값이 P-코어 6.183, E-코어 6.490 이고 이번이 6.214 다. 즉 게이트는 "이 빌드는 회귀 대역에 있다"를 정확히 보고하고 있다.

**조정하지 않았다.** 카드가 "게이트가 움직이면 보고 대상이지 조정 대상이 아니다"라고 했고, 더 중요하게는 **게이트가 AVX2 구성을 재도록 유도된 값**이라 스칼라 빌드에서 통과시키려면 게이트의 의미를 버려야 한다.

### C3 — 두 빌드가 같은 답을 내는가

카드의 지적이 정확했다: **A-65 의 비트 파리티는 AVX2 빌드 안에서 두 경로를 비교한 것**이고, 스칼라 빌드 전체가 같은 답을 내는지는 **다른 주장**이다. 두 빌드는 두 바이너리라 한 테스트가 비교할 수 없다.

그래서 **양쪽 빌드에서 컴파일되는** 테스트를 하나 추가했다(`#if` 밖에 둔다). 3072² 고정 시드 프레임에서 `DetectRowRange` 와 스칼라 규칙 직접 루프를 비교하고, **맵 다이제스트와 플래그 수를 출력**한다. 출력 두 줄이 교차-빌드 증거다:

```
[build-parity] XPE_DETECT_HAS_AVX2=1  3072x3072  flagged=18890  digest=85e344b5a355de5d
[build-parity] XPE_DETECT_HAS_AVX2=0  3072x3072  flagged=18890  digest=85e344b5a355de5d
```

**플래그 수와 다이제스트가 완전히 일치한다.** 9,437,184 화소 중 18,890 이 플래그되고, 두 빌드의 맵이 바이트 단위로 같다.

**기대 다이제스트를 상수로 박지 않았다.** 그러면 이 저장소가 여러 번 당한 상수 비교(#140, A-34)가 되고 — 그 값은 어차피 어느 실행에서 나와야 하므로 — "옳은 답인가"가 아니라 "예전과 같은가"를 단언하게 된다. 교차-빌드 비교는 **두 실행을 돌린 사람이 수행하고, 그 쌍을 이 보고서가 기록한다.**

### C4 · C5 — 그 단언이 스칼라 빌드를 보고 있는가

카드가 요구한 반증: 스칼라 빌드에서 결과를 일부러 틀리게 만든다. `DetectRowRange` 의 비-벡터 분기를 `scalarSpan(y, 0u, w)` → `scalarSpan(y, 1u, w)` 로 바꿔 **0열을 아예 판정하지 않게** 했다. `BUILD_EXIT=0`:

```
[build-parity] XPE_DETECT_HAS_AVX2=0  3072x3072  flagged=18884  digest=78d4cc8656f9b651
6 of 9437184 pixels differ, first at row 54 col 0
[  FAILED  ] DetectBuildParityTest.RowRangeMatchesTheScalarRuleAtRealFrameSize
```

**터진다.** 그리고 다이제스트도 바뀌므로 **교차-빌드 비교 쪽으로도 잡힌다** — 플래그 수가 18,890 → 18,884 로 6밖에 안 움직이는데도 다이제스트는 완전히 달라진다.

**그러나 이 단언이 스칼라 빌드에서 볼 수 없는 것이 있다.** 스칼라 빌드에서는 비교의 양쪽이 **같은 `DetectDefectivePixel`** 을 부른다. 그래서 규칙 자체가 틀리면 양쪽이 똑같이 틀리고 비교는 통과한다 — **A-69 에서 프레임 파리티가 경계 오류를 못 잡았던 것과 같은 자리**다.

이 테스트가 스칼라 빌드에서 보는 것은 **행-범위 배선**(테두리 스팬, 행 분할)이고, 그것이 그 빌드에서 틀릴 수 있는 부분이다. **규칙 자체는 §C2 의 631건이 덮는다** — 그래서 "스칼라 경로가 옳은 답을 낸다"는 주장은 이 테스트 하나가 아니라 **631건 + 교차-빌드 다이제스트 일치**가 함께 지탱한다. 하나만으로는 부족하다는 것을 적어 둔다.

### C6 — 복원 후 AVX2 빌드

```
PRE_BUILD_EXIT=0 / COMMON_BUILD_EXIT=0
100% tests passed, 0 tests failed out of 641
100% tests passed, 0 tests failed out of 69
[build-parity] XPE_DETECT_HAS_AVX2=1 ... digest=85e344b5a355de5d
```

다이제스트가 주입 전 값으로 돌아왔다. 641 = A-70 의 640 + 이번 1건.

---

## 3. baseline 귀속 (Baseline-attribution)

- 두 빌드 수치는 **이번 세션, 같은 기계, 같은 소스**에서 나왔다. 유일한 차이는 `XPE_DETECT_HAS_AVX2` 이고, 그 외 플래그(`/O2 /arch:AVX2` 포함)는 양쪽 동일하다 — 카드 지시대로 `/arch:AVX2` 를 건드리지 않았다.
- A-67 의 회귀 대역 6.183 / 6.490 은 인용이며 재측정하지 않았다. 이번 6.214 가 그 사이에 든다는 것만 관측했다.
- 다이제스트는 FNV-1a 이며 암호학적 용도가 아니다 — 실행 신원용이다.

---

## 4. 미검증 (Gaps)

- **`/arch:AVX2` 를 뗀 빌드는 하지 않았다.** 이번 스칼라 빌드는 **매크로만** 0 이고 컴파일러는 여전히 AVX2 명령을 낼 수 있다. 즉 이것은 "벡터 경로를 안 쓰는 빌드"이지 **"AVX2 없는 CPU 에서 도는 빌드"가 아니다.** 카드가 `/arch:AVX2` 를 건드리지 말라고 했고 그것이 #160 의 선택지 B 이므로 그대로 두었다 — **그러나 이 구분은 남는다.** 진짜 non-AVX2 CPU 에서의 동작은 여전히 미검증이다.
- **비-MSVC 컴파일러는 여전히 안 봤다.** `XPE_DETECT_HAS_AVX2` 의 0 갈래가 열리는 실제 조건(`_MSC_VER` 도 `__AVX2__` 도 없음)은 이 레인에 없다.
- **교차-빌드 비교는 사람이 한다.** 두 다이제스트를 맞춰 보는 것은 CI 가 아니라 이 보고서다. 자동으로 잡히지 않는다.
- **프레임 한 종류·한 시드**에서만 비교했다. 3072² 잡음 프레임 하나다.
- **Doxygen 로컬 확인 불가** — 여전(#159). 이번 수정이 맞는지는 **CI 결과로만 확인된다.** 그리고 이번 오류 자체가 그 사실 때문에 났다.

---

## 5. 잔여 위험 (Residual-risk)

- **스칼라 빌드는 성능 게이트를 통과할 수 없다.** 의도된 것이지만, 누가 스칼라 구성을 CI 에 넣으면 그 잡은 항상 빨갛다. 게이트를 구성별로 나눌지는 판정 사항이고 이번에 하지 않았다.
- **다이제스트 비교는 기록이지 가드가 아니다.** 다음에 두 빌드가 갈라져도 아무것도 실패하지 않는다 — 누군가 다시 두 번 돌려 비교해야 한다.
- **이번 Doxygen 오류가 만든 것**: A-68 의 수정이 A-71 의 오류를 만들었다. 리더가 #159 에서 "안 썼다"와 "쓴 것이 틀렸다"를 나눠 세기로 한 그 자리다. 이 회차는 후자이고, **레인이 쓴 주석을 레인이 확인할 수 없다**는 것이 원인 그대로다.

---

Refs #159 #160
