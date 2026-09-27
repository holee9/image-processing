# QA-A-65 — 화소 루프 AVX2

**카드**: `.moai/lanes/pre/inbox/QA-A-65.md` · **브랜치**: `dev/preprocess` · **기계**: Intel Core i7-12700

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | AVX2 경로는 스칼라 경로와 **비트 동일**하다. ulp 허용이 아니라 동일이다 |
| C2 | 비결합성이 개입하지 않는다는 A-61 의 문장은 AVX2 에서도 참이다 — 수평 합·FMA·역수 근사를 **쓰지 않았다**. 다만 **min/max 피연산자 순서**가 새로 생긴 함정이고, 그것을 코드와 테스트로 못 박았다 |
| C3 | 반증 2건이 **서로 다른 것을 잡는다**. 피연산자 순서를 바꾸면 비트 테스트만 실패하고 프레임 테스트는 **통과한다**; 이웃 하나를 잘못 읽으면 프레임 테스트만 실패하고 비트 테스트는 통과한다 |
| C4 | 3072² 단일 스레드 **692.7 → 163.7 ms**, 게이트 비율 **7.193 → 1.644** (상한 10.000 은 **그대로 두었다**) |
| C5 | 화소 루프만 보면 **525.5 → 16.6 ms**. σ 몫이 21.0% → **89.8%** 로 뒤집혔다 |
| C6 | AVX2 런타임 검출: 이 모듈에 **하나 있으나 재사용 불가**하고, 애초에 모듈 전체가 `/arch:AVX2` 로 빌드된다. **새로 만들지 않았다** — §6 에 보고 |
| C7 | 전체 633/69 통과, 경고 0 |

---

## 2. 증거 (Evidence)

### C1 · C2 — 설계가 곧 동일성이고, 새 함정은 피연산자 순서다

배치를 뒤집었다. 한 화소의 이웃 8개를 한 레지스터에 넣으면 정렬망 단계마다 permute 가 필요하다. **레지스터 하나당 이웃 위치 하나, 레인 하나당 화소 하나**로 놓으면 19개 비교·교환이 전부 shuffle 없는 min/max 한 번이 되고, gather 는 화소 8개당 unaligned load 8회가 된다.

연산별 대응 (`runtime_detection.h`, `MedianSortCE8` / `DetectEightPixelsAvx2`):

| 스칼라 | 벡터 | 동일한 이유 |
|---|---|---|
| `lo = (b<a)?b:a` / `hi = (b<a)?a:b` | `_mm256_min_ps(b,a)` / `_mm256_max_ps(a,b)` | MINPS(x,y)=(x<y)?x:y, MAXPS(x,y)=(x>y)?x:y — 둘 다 "순서 비교, 아니면 둘째 피연산자" |
| `(a3+a4)*0.5f` | `add` 후 `mul` | FMA 로 합쳐지지 않는다 (인트린식을 쓰는 이유가 이것이다) |
| `std::abs(v-m)` | `sub` 후 `andnot`(부호 비트) | `-0.0` → `+0.0` 까지 같다 |
| `if (floor>s) s=floor` | `_mm256_max_ps(floor, s)` | 같은 삼항식 |
| `if (cap>0 && s>cap) s=cap` | `cap>0` 은 스칼라 분기, `_mm256_min_ps(cap, s)` | 같은 삼항식 |
| `<`, `>` | `_CMP_LT_OQ`, `_CMP_GT_OQ` | ordered — NaN 에서 false, 스칼라와 같다 |

**새 함정.** 위 첫 줄의 피연산자 순서는 **대칭이 아니다**. `a=+0.0, b=-0.0` 이면 비교가 양쪽 다 거짓이라 스칼라는 `hi = b = -0.0` 을 내는데, `_mm256_max_ps(b,a)` 는 `+0.0` 을 낸다. **같은 입력, 다른 비트.** NaN 이 어느 피연산자를 살리는지도 같은 비대칭이 정한다. 이유를 헤더 주석에 적었다.

### C3 — 반증 두 건, 서로의 사각을 덮는다

**반증 A — `_mm256_max_ps(a,b)` → `(b,a)`.** 재빌드 `BUILD_EXIT=0`:

```
[  FAILED  ] Avx2ParityTest.SignedZeroOrderingMatchesTheScalarNetwork
  lane 0: scalar bits 0x0 vector bits 0x80000000 -- both are zero and compare equal,
          so only the bits catch this. Check the min/max operand order in MedianSortCE8.
[  FAILED  ] Avx2ParityTest.PerLaneInputsAgreeBitForBit
  trial 1 lane 4: scalar 0x0 vector 0x80000000
[       OK ] Avx2ParityTest.FrameMapsAreIdenticalPixelForPixel        <-- 통과했다
[       OK ] Avx2ParityTest.ShippedEntryPointAgreesWithTheScalarRule  <-- 통과했다
```

**프레임 단위 비교만 있었으면 이 결함을 못 잡는다.** `+0.0` 과 `-0.0` 은 비교가 같아서 판정도 같고, 맵이 한 픽셀도 안 달라진다. 비트를 보는 단언만이 잡는다. #156 이 `> 0` 임계값에서 1 ulp 를 "도달함"으로 읽은 것과 같은 자리다.

**반증 B — `n[5]` 가 잘못된 이웃을 읽게 함** (`rowDn + x - 1` → `rowDn + x`). 재빌드 `BUILD_EXIT=0`:

```
[       OK ] Avx2ParityTest.SignedZeroOrderingMatchesTheScalarNetwork  <-- 통과했다
[       OK ] Avx2ParityTest.PerLaneInputsAgreeBitForBit                <-- 통과했다
[  FAILED  ] Avx2ParityTest.FrameMapsAreIdenticalPixelForPixel
  noise (64x48): 2 pixels differ, first at row 15 col 10
  realistic (640x480): 38 pixels differ, first at row 4 col 593
  narrowest frame with one vector run (10x10): 1 pixels differ, first at row 6 col 7
[  FAILED  ] Avx2ParityTest.ShippedEntryPointAgreesWithTheScalarRule
  12 of 49152 pixels differ from the scalar rule
```

**정확히 반대다.** 비트 테스트는 망 자체만 보므로 이웃을 잘못 읽는 것을 못 보고, 프레임 테스트는 그것만 본다. 둘 중 하나를 뺐으면 절반만 본다 — A-63 에서 가드 페이지와 센티넬이 서로 다른 것을 본 것과 같은 구조다.

두 반증 모두 원복했고, 원복 후 5/5 통과한다.

### C4 · C5 — 측정

**성능 게이트** (단일 스레드, warm, min of 5). 상한은 **건드리지 않았다**:

| | A-64 (전) | A-65 (후) |
|---|---|---|
| 3072² | 692.7 ms | **163.7 ms** |
| 기준 커널 | 96.3 ms | 99.5 ms |
| **ratio** | **7.193** | **1.644** (limit 10.000) |
| SPEC 60 ms 대비 | 11.5배 | **2.7배** |
| 1024² | 81.3 ms | 18.2 ms |

**단계 분해** (`--decompose-threads`, 3072²):

| threads | TOTAL | σ | σ 몫 | 화소 루프 | 루프 몫 |
|---|---|---|---|---|---|
| 1 (전) | 665.5 | 139.9 | 21.0% | 525.5 | 79.0% |
| **1 (후)** | **165.3** | 148.4 | **89.8%** | **16.6** | 10.0% |
| 16 (전) | 108.2 | 33.3 | 30.8% | 74.5 | 68.9% |
| **16 (후)** | **32.5** | 28.1 | **86.5%** | **4.2** | 12.9% |

**화소 루프 31.7배.** σ 는 손대지 않았으므로 절대값이 그대로다(139.9 → 148.4, 측정 분산 범위).

**A-56 하한과의 대조**: A-56 이 잰 AVX2 네트워크 단독 하한이 15.97 ms 였고, 이번 실측 화소 루프 전체가 **16.6 ms** 다. gather·판정·맵 쓰기까지 포함해 하한 바로 위에 붙었다 — A-64 에서 median 실측이 스칼라 하한에 붙어 있던 것과 같은 자리에, 이번엔 AVX2 하한에서 붙었다.

**GAP%** 는 -0.1% / +1.2% / +5.2%. A-64 의 -11.0~+12.0% 보다 좁아졌는데, 이유는 코드가 아니라 **도구를 고쳤기 때문**이다 — §C5 각주 참조.

**메모리**: 16스레드 1.14 ms, 전체 32.5 ms 의 3.5%. 전체 시간이 3분의 1로 줄었는데도 여전히 병목이 아니다.

> **도구 수정 (A-64 의 것)**: `--decompose-threads` 의 복제 루프는 A-64 에서 행 루프를 다시 써 놓은 것이었다. 두 루프가 다 스칼라일 때는 충실한 복제였지만, 배송 루프가 AVX2 를 갖는 순간 복제가 아니게 됐다 — 첫 측정에서 `repl/x 31.84x`, `GAP -321.1%` 가 찍혔다. **그 크기의 숫자는 발견이 아니라 서로 다른 두 프로그램을 비교했다는 뜻이다.** 복제를 없애고 배송 경로와 같은 `DetectRowRange` 를 부르게 고쳤고, 위 표는 고친 뒤의 값이다.

### C7 — 회귀

```
PRE_BUILD_EXIT=0 / COMMON_BUILD_EXIT=0
100% tests passed, 0 tests failed out of 633
100% tests passed, 0 tests failed out of 69
```

633 = A-64 의 628 + 이번 5건. 경고 0 (`/WX`).

---

## 3. baseline 귀속 (Baseline-attribution)

- 전/후 수치는 **같은 기계, 같은 세션**에서 잰 것이다. "전" 은 A-64 보고서의 실측값이고 이번에 재측정하지 않았다 — 게이트 692.7 ms 와 분해 665.5 ms 는 A-64 실행분이다.
- A-56 의 15.97 ms 는 **물려받은 값**이며 이번에 재측정하지 않았다. 대조용으로만 인용한다.
- 기준 커널이 96.3 → 99.5 ms 로 3.3% 움직였다. 비율 게이트가 기계 상태를 흡수하라고 존재하는 부분이 이것이다.
- 테스트 수 628 → 633, 증가분 5는 전부 `Avx2ParityTest` 다.

---

## 4. 미검증 (Gaps)

- **AVX2 없는 CPU 에서의 동작.** 확인하지 않았고 확인할 기계도 없다. 이 모듈은 이미 `/arch:AVX2` 로 빌드되므로 그런 CPU 에서는 이 변경 이전에도 동작하지 않았다(§6).
- **비-MSVC 컴파일러.** `XPE_DETECT_HAS_AVX2` 는 `_MSC_VER || __AVX2__` 로 열리고, GCC/Clang 에서 `-mavx2` 없이 이 헤더를 포함하는 타깃이 있으면 스칼라로 떨어진다 — **그 경로를 빌드해 보지 않았다.**
- **NaN 입력**. A-54 가 스칼라 망에 대해 적어 둔 그대로다: NaN 에서는 `std::nth_element` 의 전제가 깨져 스칼라 경로 자체가 미정의다. 비트 파리티 주장은 **옛 경로가 정의된 입력**으로 한정된다. 벡터 쪽이 NaN 에서 스칼라와 같은 비트를 내는지는 재지 않았다.
- **σ 는 손대지 않았다** — 카드 범위 밖. 이제 89.8% 다.
- **`pixel-stages` 마지막 델타가 -0.8%** 로 찍힌다. A-64 의 -21.4% 와 달리 구조적 결함이 아니라 별도로 타이밍한 두 루프 사이의 실행 간 잡음으로 보이지만, **그렇다고 확인하지는 않았다**(반복 수를 늘려 보지 않았다).
- **Doxygen 로컬 확인 불가** — 여전히. `89aade5` CI 는 리더가 읽는다(#159).

---

## 5. 잔여 위험 (Residual-risk)

- **피연산자 순서는 리뷰로 지켜지지 않는다.** `_mm256_max_ps(a,b)` 와 `(b,a)` 는 눈으로 구분이 안 되고, 잘못 써도 프레임 테스트는 통과한다(§C3). 방어선은 `SignedZeroOrderingMatchesTheScalarNetwork` 단 하나다. 그 테스트가 지워지면 조용해진다.
- **경로가 둘이다.** 테두리·창 크기 5×5·폭 10 미만 프레임은 스칼라로 간다. 파리티 테스트가 그 경계(9×10, 10×10, 17×17)를 덮지만, **두 구현이 있는 한 갈라질 여지는 남는다.**
- **게이트가 이제 매우 헐겁다.** 비율 1.644 에 상한 10.000 이라 6배 회귀까지 통과한다. 카드 지시대로 조이지 않았고, 조이는 시점은 리더 판단이다. 다만 **지금 게이트가 잡을 수 있는 회귀의 크기는 실질적으로 사라졌다**는 것을 적어 둔다.
- **최적화가 σ 로 문제를 밀었다.** 전체는 4.2배 빨라졌지만 남은 시간의 90%가 한 곳에 몰렸다. 다음 작업의 위험은 σ 를 건드릴 때 **A-61 이 세운 비트 파리티**가 깨지는 것이다 — 그쪽은 히스토그램 정수 합이라 성질이 다르다.

---

## 6. AVX2 런타임 검출 — 있는 것을 보고합니다 (만들지 않았습니다)

카드 지시대로 **확인만 하고 만들지 않았습니다.**

**하나 있습니다.** `modules/preprocess/src/gain_correct.cpp:98` 의 `xpe_gain_has_avx2()` — `__cpuidex` + `_xgetbv` 로 OSXSAVE·AVX·XCR0·AVX2 비트를 보는 제대로 된 검출이고, 같은 파일 301행에서 경로를 가릅니다. 다만 **`static` 이라 그 TU 밖에서 쓸 수 없습니다.**

**그리고 이 모듈에서는 그 검출이 이미 무의미합니다.** `modules/preprocess/CMakeLists.txt:71` 이 모듈 전체를 `/arch:AVX2` 로 컴파일합니다(비-MSVC 는 `-mavx2`). 즉 **DLL 안 어디서든 컴파일러가 AVX2 명령을 낼 수 있고**, AVX2 없는 CPU 는 `xpe_gain_has_avx2()` 가 무엇을 반환하든 그 전에 죽습니다. 검출이 가리는 것은 "이 함수의 AVX2 경로"뿐이고 "이 DLL 이 AVX2 를 요구하는가"가 아닙니다.

그래서 **제 판단**: 이번 변경은 그 상황을 **바꾸지 않습니다** — 모듈은 이전에도 AVX2 를 요구했고 지금도 요구합니다. 새 검출을 만들면 "고쳤다"는 인상만 주고 실제 요구사항은 그대로입니다. 다만 **`gain_correct.cpp` 의 검출이 보호하는 것처럼 보이는데 실제로는 못 보호한다**는 것은 별개의 발견이고, 이 카드에서 고치지 않았습니다. 판정이 필요하면 올립니다.

---

Refs #144 #143
