# QA-A-72 — 보호하지 못하는 가드를 지운다 (#160)

**카드**: `.moai/lanes/pre/inbox/QA-A-72.md` · **브랜치**: `dev/preprocess` · **기계**: Intel Core i7-12700

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | `xpe_gain_has_avx2()` 와 호출부 분기를 지웠다. **AVX2 경로가 유일한 경로**가 됐고 **스칼라는 남았다** |
| C2 | **카드가 제안한 단언은 이 변경에 대해 아무것도 말하지 않는다.** `--regress` 도 A-71 다이제스트도 `gain_correct` 를 지나지 않는다 — 확인했다 |
| C3 | 그래서 **지나는 경로에서 재는 단언**을 만들었다: 배송 경로 대 스칼라 기준 구현 비교 |
| C4 | 그 단언으로 **변경 전/후가 같음**을 보였다 — 양쪽 다 `differing=0, worst_ulp=0` |
| C5 | 반증: AVX2 커널을 틀리게 하면 새 단언은 터지고 **기존 "parity" 두 건은 통과한다** |
| C6 | 같은 형태가 `offset_correct.cpp` 에 **둘 더** 있다. 고치지 않고 올린다. 다른 모듈에는 **없다** |
| C7 | 전체 642/69 통과, 경고 0 |

---

## 2. 증거 (Evidence)

### C1 — 무엇을 지웠고 무엇이 남았는가

호출부를 먼저 읽었다. 분기였다:

```cpp
// Apply: AVX2 when available, scalar fallback
if (xpe_gain_has_avx2())
    apply_gain_avx2(...);
else
    apply_gain_correction_scalar(...);
```

프로브와 분기를 지우고 `apply_gain_avx2(...)` 만 남겼다. 자리에는 **왜 지웠는지**를 적었다 — 프로브 자체는 제대로 쓰였으나(CPUID leaf 1·7, OSXSAVE, XGETBV) **모듈 전체가 `/arch:AVX2`(CMakeLists:71) 로 컴파일되므로 그 검사에 도달하는 코드에서도 AVX2 명령이 나올 수 있고**, 도달 자체가 보호 대상에 의존하는 가드는 가드가 아니다.

**스칼라는 지우지 않았다.** 다만 분기를 없애면 파일-지역 `static` 함수가 **참조 0** 이 되고, `/W4 /WX` 에서 그것은 경고가 아니라 **에러**다. 그래서 선택지는 둘뿐이었다 — 지우거나, 부를 사람을 만들거나. §4.6 이 스칼라를 기준 구현으로 지명하므로 **후자**를 택했다:

- 퍼-화소 규칙과 전체-버퍼 기준 구현을 `xpe_preprocess_internal.h` 로 옮기고 **inline** 으로 두었다.
- **내보내기(export)를 늘리지 않았다.** A-61 이 세운 경계다 — 내보내기는 거두기 어렵다. inline 이면 라이브러리와 테스트가 **같은 소스를 각자 컴파일**하므로 테스트를 위해 ABI 를 넓히지 않아도 된다.
- 그 결과 기준 구현은 **폴백이기를 그만두고 검사가 됐다.**

### C2 — 카드의 단언이 이 변경을 보지 못한다 (먼저 확인했다)

카드가 `--regress diff` 와 A-71 다이제스트를 제안하면서 **"그 경로가 `gain_correct` 를 지나는지 먼저 확인하라"**고 했다. 확인했고, **둘 다 지나지 않는다.**

- `--regress` 는 `xpe_struct_frames` 의 모드이고, 그 함수(`:696`)는 `ComputeGlobalSigma` 와 검출 규칙만 부른다. 파일 전체에서 "gain" 은 무관한 주석 6곳뿐이고 `xpe_gain_correct` 호출은 **0** 이다.
- A-71 의 `[build-parity]` 다이제스트는 `DetectRowRange` — 결함 검출이다. 게인 보정을 지나지 않는다.

**A-62 와 같은 자리이고, 이번에는 착수 전에 걸렀다.** 그 단언들을 근거로 썼다면 "변경이 안전하다"를 **측정하지 않은 채** 주장하게 됐을 것이다.

(참고로 두 단언 모두 이번에도 통과한다 — `[build-parity] digest=85e344b5a355de5d` 로 A-71 과 동일. 다만 **이 변경에 대한 증거가 아니다.**)

### C3 · C4 — 지나는 경로에서 잰다

기존 `test_gain_correct_avx2_parity.cpp` 가 무엇을 단언하는지 읽었다: **반복 호출이 서로 같다**(결정성)와 stride 케이스. **독립적인 것과 비교하지 않는다** — 일관되게 틀린 경로도, 아예 안 도는 경로도 통과한다. (검출기 쪽에서 A-42 가 같은 형태를 발견해 파일 이름을 바꿨다.)

그래서 새 단언을 넣었다 — `ShippedPathMatchesTheScalarReference`: 1024×768, 0.5~1.999 로 변화하는 게인 맵(상수 맵이면 곱셈이 망가져도 맞아 보인다), 공개 API 로 배송 경로를 돌리고 같은 입력으로 스칼라 기준을 돌려 **원시 비트 거리(ULP)** 를 센다. 허용치는 **AC-GAIN-004 의 1 ULP** 이고, 이 카드가 고른 수가 아니라 **선언된 계약**이다(`gain_correct.cpp` `MAX_ULP_DIFFERENCE`). 관측값은 **가정하지 않고 출력**한다.

**변경 전** (프로브 있음, 기준 구현만 먼저 노출한 상태):

```
[gain-parity] 1024x768  differing=0 of 786432  worst_ulp=0
```

**변경 후** (프로브·분기 제거):

```
[gain-parity] 1024x768  differing=0 of 786432  worst_ulp=0
```

**786,432 화소 전부가 비트까지 같다.** 두 경로는 애초에 1 ULP 를 쓰지도 않았다 — 커널이 `_mm256_mul_ps` 하나이고 rcp 근사도 FMA 축약도 없다. 계약이 1 ULP 를 허용한다는 것과 구현이 그것을 쓴다는 것은 다른 말이라 **둘 다 적는다.**

그리고 프로브는 AVX2 있는 CPU 에서 **항상 참**을 반환했으므로 같은 경로가 돈다 — 위 숫자가 그것을 보인다.

### C5 — 반증

AVX2 커널의 `_mm256_mul_ps` 를 `_mm256_add_ps` 로 바꿨다. `BUILD_EXIT=0`:

```
[gain-parity] 1024x768  differing=786429 of 786432  worst_ulp=1073284857
AC-GAIN-004 allows 1 ULP; worst was 1073284857 at index 400514
  (shipped 1.9455252885818481 vs reference 0)
[  FAILED  ] GainCorrectAVX2ParityTest.ShippedPathMatchesTheScalarReference
[       OK ] GainCorrectAVX2ParityTest.MultipleCallsAreBitIdentical
[       OK ] GainCorrectAVX2ParityTest.ParityWithNonMultipleStride
```

**새 단언만 터지고, 기존 "parity" 두 건은 통과한다.** 결정성은 일관되게 틀린 경로를 구분하지 못한다 — 추론이 아니라 관측이다. 원복 후 3/3 통과.

### C6 — 같은 형태 세기 (고치지 않음)

**찾은 범위**: `modules/preprocess/**` 의 모든 `.cpp`/`.h`, 패턴 `__cpuid` · `__cpuidex` · `_xgetbv` · `__builtin_cpu_supports`. 그리고 비교를 위해 저장소 전체의 `.cpp` 도 같은 패턴으로 훑었다.

**`modules/preprocess/src/offset_correct.cpp` 에 둘 있다:**

| 함수 | 줄 | 링크 | 호출 |
|---|---|---|---|
| `xpe_has_avx2()` | 171 | 외부(static 아님) | **1곳**(`:333`), 같은 파일 |
| `xpe_has_avx512f()` | 201 | 외부 | **0곳 — 아무도 부르지 않는다** |

- 두 함수 모두 **이 파일 밖에서 쓰이지 않는다**(`modules/` 전체 grep, 이 파일 제외 시 0건). `static` 이 아니라 심볼은 나가 있다.
- `xpe_has_avx2()` 의 호출부(`:333`)는 `gain_correct` 와 **같은 형태의 분기**이고, 같은 이유로 보호하지 못한다 — 같은 모듈, 같은 `/arch:AVX2`.
- `xpe_has_avx512f()` 는 다르다: **AVX-512 는 `/arch:AVX2` 가 함의하지 않으므로** 그 프로브는 원리적으로는 의미가 있을 수 있다. 다만 **호출되지 않으므로 지금은 아무것도 하지 않는다.**

**고치지 않았다** — 카드가 삭제 범위를 `gain_correct` 로 한정했다.

**다른 모듈에는 없다.** `modules/` 전체에서 `__cpuidex|__builtin_cpu_supports` 를 가진 `.cpp` 는 preprocess 의 두 파일뿐이다(하나는 이번에 제거). 올릴 목록이 없다.

### C7 — 회귀

```
PRE_BUILD_EXIT=0 / COMMON_BUILD_EXIT=0
100% tests passed, 0 tests failed out of 642
100% tests passed, 0 tests failed out of 69
```

642 = A-71 의 641 + 이번 1건. 경고 0.

---

## 3. baseline 귀속 (Baseline-attribution)

- 전/후 `[gain-parity]` 두 줄은 **같은 세션, 같은 기계**에서 났고, 유일한 차이가 프로브·분기 제거다. 기준 구현 노출은 **전 측정에도 이미 들어 있었다** — 그래야 전/후가 한 가지만 다르다.
- 반증 실행은 `BUILD_EXIT=0` 확인 후의 결과다.
- `--regress` 와 A-71 다이제스트가 `gain_correct` 를 지나지 않는다는 것은 **소스를 읽어** 확인했다(호출 0건).
- 프로브 수 세기의 검색 범위는 §C6 에 적었다.

---

## 4. 미검증 (Gaps)

- **AVX2 없는 CPU 에서는 확인할 수 없다.** 이 변경은 그런 CPU 의 동작을 바꾸지 않는다(이전에도 프로브 앞에서 죽었다). 확인할 기계가 없다.
- **`worst_ulp=0` 은 이 게인 맵·이 입력에 대한 것이다.** 1024×768, 0.5~1.999 게인, 0~4000 입력 한 조합이다. 계약은 1 ULP 이고 그보다 넓은 입력에서 0 이 유지된다고 주장하지 않는다.
- **`offset_correct.cpp` 의 둘은 그대로다.** 그 경로의 파리티 단언도 만들지 않았다 — 카드 범위 밖.
- **`MAX_ULP_DIFFERENCE` 상수가 `gain_correct.cpp` 안에서 쓰이는지 확인하지 않았다.** 테스트가 쓰는 값은 그 상수가 아니라 리터럴 1 이고, 출처를 주석에 적었다.
- **Doxygen 로컬 확인 불가** — 여전(#159). 내부 헤더에 함수 둘을 추가했으므로 **이번에도 CI 가 말해 줄 것**이다.

---

## 5. 잔여 위험 (Residual-risk)

- **스칼라 기준을 살린 것이 테스트 하나에 달려 있다.** `ShippedPathMatchesTheScalarReference` 가 지워지면 기준 구현은 다시 호출자 0이 되고, 이번에는 inline 이라 **경고조차 나지 않는다**(미사용 inline 함수는 경고 대상이 아니다). 조용히 죽는다.
- **`offset_correct.cpp` 의 같은 형태가 남아 있다.** 이 카드가 "보호하지 못하는 가드는 없는 가드보다 나쁘다"를 이유로 하나를 지웠으므로, 남은 둘은 **같은 이유로 남아 있는 상태**다.
- **내부 헤더가 inline 구현을 담기 시작했다.** `DetectRowRange` 계열이 이미 그랬지만, 이번에 게인 쪽도 그렇게 됐다. 헤더를 포함하는 모든 TU 가 이 코드를 컴파일한다.

---

Refs #160
