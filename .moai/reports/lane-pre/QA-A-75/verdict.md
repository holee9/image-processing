# QA-A-75 — 340줄이 컴파일조차 안 되는 이유, 그리고 그보다 나쁜 것 (#160)

**카드**: `.moai/lanes/pre/inbox/QA-A-75.md` · **브랜치**: `dev/preprocess` · **코드 변경 없음(조사 카드)**

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | 그 파일은 **빌드에서 빠진 것이 아니라 컴파일이 안 된다.** `XPE_EXPORT` 가 저장소 어디에도 정의되어 있지 않다 — 컴파일해서 확인했다 |
| C2 | `xpe_simd_*` 7개의 소비자는 **0건**이다. 같은 명령·같은 범위의 대조군은 97·91 을 낸다 |
| C3 | `:356` TODO 가 가리키는 `test_offset_correct_avx512_parity.cpp` 는 **존재하지 않는다** |
| C4 | §4 (a)/(b) — **두 SPEC 이 각자 자기 모듈 밑에 한 벌씩 계획**했고, **계획된 경로는 둘 다 존재하지 않는다.** 실제 파일은 내용상 P1A 쪽이다 |
| C5 | 어긋난 것이 파일 하나가 아니다 — **P1A 가 계획한 `simd/` 하위 트리 전체가 없고**, 그중 M5-2 의 서술이 **A-74 에서 지운 커널과 일치**한다 |
| C6 | 더 나쁜 것을 찾았다: `.moai/docs/acceptance.md:129` 가 **없는 기능을 체크 표시**하고 있다 |

**결정은 하지 않았다.** §7 에 판정 요청으로 정리한다.

---

## 2. 증거 (Evidence)

### C1 — 빌드 밖이 아니라, 넣어도 깨진다

파일은 실재한다: `modules/preprocess/src/simd_dispatch.cpp`, 10,260 바이트. CPUID·XGETBV·XCR0 확인까지 제대로 쓰여 있고 `XPE_EXPORT` 가 7개 붙어 있다.

CMake 목록에 없다는 것은 카드가 맞다(`set(XPE_PREPROCESS_SOURCES ...)` 는 글롭이 아니라 명시 목록이고 `simd_dispatch` 가 없다). **그런데 한 단계 더 있다:**

```
$ grep -rn "XPE_EXPORT" --exclude-dir=build --exclude-dir=.git .
./modules/preprocess/src/simd_dispatch.cpp:242  (사용)
./modules/preprocess/src/simd_dispatch.cpp:251  (사용)
... 7건 전부 이 파일의 사용 ...
```

**정의가 0건이다.** `XPE_API` 는 정의가 있는데(`codemaps/api-boundaries.md` 에 `__declspec(dllexport)` 로 문서화) `XPE_EXPORT` 는 이 파일 안에서만, 쓰이기만 한다.

추론으로 끝내지 않고 **모듈과 같은 플래그로 컴파일해 봤다**:

```
cl /c /O2 /arch:AVX2 /DNDEBUG /EHsc /std:c++17 /W4
   /I modules\preprocess\include /I modules\common\include
   modules\preprocess\src\simd_dispatch.cpp

simd_dispatch.cpp(242): error C2143: 구문 오류: ';'이(가) 'const' 앞에 없습니다.
simd_dispatch.cpp(242): error C4430: 형식 지정자가 없습니다. int로 가정합니다.
```

**첫 `XPE_EXPORT` 줄에서 깨진다.** 즉 "CMake 목록에 한 줄 추가하면 살아난다"가 **아니다** — 추가하면 **빌드가 깨진다.**

이것이 카드가 말한 "컴파일 통과 ≠ 동작" 의 한 단계 위라는 지적과 맞고, 한 단계 더 나아간다: **컴파일조차 안 되는데 소스는 완성되어 보인다.** 340줄 중 CPUID 부분은 실제로 정확하다. 틀린 곳은 함수 일곱 개의 맨 앞 단어뿐이다.

### C2 — 소비자 0건, 그리고 대조군

카드가 요구한 대로 **부재 주장에 검색 범위를 같은 문장에 적는다.**

**범위: 이 워크트리 전체(`.`), `build/` 와 `.git/` 제외.** 정의 파일(`simd_dispatch.cpp`) 자신은 집계에서 뺐다.

| 심볼 | 호출 |
|---|---|
| `xpe_simd_get_arch_name` | 0 |
| `xpe_simd_is_avx512` | 0 |
| `xpe_simd_is_avx2` | 0 |
| `xpe_simd_is_neon` | 0 |
| `xpe_simd_is_scalar` | 0 |
| `xpe_simd_force_scalar` | 0 |
| `xpe_simd_get_capabilities` | 0 |
| **대조군** `xpe_offset_correct` | **97** |
| **대조군** `xpe_gain_correct` | **91** |

대조군은 **같은 명령, 같은 범위**로 셌다. 97·91 이 나오므로 위의 0 은 "도구가 아무것도 못 읽어서 나온 0" 이 아니다.

**괄호 없는 이름으로도 훑었다**(`DllImport` 문자열·`.def`·문서를 잡기 위해). `xpe_simd_` 전체 6건이고 **전부 문서·TODO 다**:

```
.moai/specs/SPEC-XPE-P1A/plan.md:65, :172
.moai/specs/SPEC-XPE-P2-ADV/plan.md:62, :211
.moai/specs/SPEC-XPE-P2-ADV/tasks.md:123
modules/preprocess/CMakeLists.txt:356
```

`.def` 파일은 저장소에 **하나도 없다**(`find . -name "*.def"` → 0건). C# 쪽 `DllImport` 도 위 6건에 없으므로 0이다.

**A-73 의 실수를 되풀이하지 않으려고** 이번에는 `이름(` 으로 세고 정의 줄을 제외했으며, 파일 이름에 걸리는 경우를 따로 확인했다 — 위 6건은 전부 파일 *내용* 이지 파일 *이름* 이 아니다.

### C3 — TODO 가 가리키는 테스트는 없다

```
$ find . -name "*avx512*" --(build/.git 제외)
(출력 없음)
```

`test_offset_correct_avx512_parity.cpp` 는 **존재하지 않는다.** 그러므로 `:356` 의 TODO 는 "만들어 둔 테스트가 API 때문에 빌드에서 빠져 있다"가 아니라 **"쓰지 않은 테스트를 쓰려는데 막혔다"** 이다. 카드가 "막힌 이유가 다르면 푸는 비용도 다르다"고 했는데, **비용이 한 번 더 다르다**: API 를 빌드에 넣는 것으로는 안 되고(넣으면 깨진다), 테스트도 새로 써야 한다.

### C4 — (a)/(b): 판정 재료

**두 SPEC 이 각자 자기 모듈 밑에 한 벌씩 계획했다.** 서로 다른 파일 둘이다:

| | SPEC-XPE-P1A | SPEC-XPE-P2-ADV |
|---|---|---|
| 계획 경로 | `.../preprocess 트리/simd/xpe_simd_dispatch.cpp` (plan.md:65) | `modules/enhance_advanced/src/simd/xpe_simd_dispatch.cpp` (tasks.md:123) |
| 항목 | M5-1 (plan.md:172) | T-100, `REQ-ADV-040`, **pending** |
| 실재 여부 | **없음** — `modules/preprocess/src/simd` 디렉터리 자체가 없다 | **없음** — `modules/enhance_advanced/src/simd` 도 없다 |

**실제 파일**은 `modules/preprocess/src/simd_dispatch.cpp` 이고, 머리에 이렇게 적혀 있다:

```
* SPEC: SPEC-XPE-P1A-SIMD-PARITY v2.0.0  IEC 62304 Class B
* REQ-P1A-040: CPUID-based dispatch with hierarchical fallback
```

**`REQ-P1A-040` 이다 — `REQ-ADV-040` 이 아니다.** 내용상 P1A 쪽 항목(M5-1)에 대응하고, P2-ADV 의 T-100 과는 요구 번호가 다르다.

그래서 관측은 이렇게 갈린다 — **제가 단정하지 않고 그대로 올린다**:

- **P1A M5-1 에 대해서는 (a) 쪽으로 읽힌다**: 이름·경로는 다르지만 요구 번호와 내용이 같은 파일이 존재한다. 다만 **컴파일되지 않으므로 "작업이 되어 있다"고 말하기도 어렵다** — 세 번째 상태다.
- **P2-ADV T-100 에 대해서는 (b) 다**: 다른 모듈의 다른 요구(`REQ-ADV-040`)이고, 그 파일은 없다. `pending` 이 맞다.

### C5 — 어긋난 것은 파일 하나가 아니다

P1A plan.md:61-65 가 계획한 `simd/` 하위 트리 **전체가 없다**:

```
simd/
    xpe_offset_avx2.cpp     <- 없음
    xpe_gain_avx2.cpp       <- 없음
    xpe_defect_avx2.cpp     <- 없음
    xpe_simd_dispatch.cpp   <- 없음 (다른 이름·다른 위치에 비슷한 것이 있음)
```

실제 구현은 AVX2 커널을 **각 보정 `.cpp` 안에 인라인**으로 뒀다(`offset_correct.cpp` 의 `offset_correct_float_avx2` 등). **레이아웃이 통째로 다르다.**

**그리고 M5-2 를 읽다가 걸리는 것**:

```
| M5-2 | `xpe_offset_avx2.cpp` 구현 (`_mm256_subs_epu16` 활용) | M2-1, M5-1 |
```

`_mm256_subs_epu16` 은 **A-74 에서 제가 지운 `offset_correct_avx2`** 가 쓰던 명령이다. 즉 지운 그 넷은 **이 계획대로 쓰였다가 float 설계로 바뀌면서 남은 잔해**로 보인다. A-74 의 판정(“지금 쓰는 연산이 아니다”)은 그대로 유효하지만, **왜 거기 있었는지**가 이제 설명된다 — 그리고 **SPEC 은 아직 옛 설계를 적고 있다.**

### C6 — 체크 표시된 채로 사실이 아닌 것

`REQ-P1A-040` 의 수용 기준을 읽다가 찾았다.

- **AC-SIMD-001~004**: 스칼라 대 AVX2 파리티(uint16 비트 동일 / float 1 ULP). **A-72·A-73 이 이 의도를 다른 방법으로 이미 달성했다** — 런타임 스위치 없이 **인라인 스칼라 기준 구현과 비교**하는 방식으로.
- **AC-SIMD-005**: *"Given the module initialized with config `{"force_scalar": true}`"* — **init 설정 플래그**를 요구한다. `xpe_simd_force_scalar()` 함수가 아니다. **세 번째 메커니즘**이고 구현되어 있지 않다(`force_scalar` 는 코드에서 오직 저 컴파일 안 되는 파일 안에만 있다. 범위: 워크트리 전체, `build/`·`.git/` 제외, `.moai/specs` 제외한 코드·문서 7건이 전부이고 그중 5건이 그 파일).

**그리고:**

```
.moai/docs/acceptance.md:129
- [x] Config flag override: `force_scalar: true` in init config works
```

**체크되어 있다. 동작할 수 없다.** 그 설정을 읽는 코드가 없고, 유일한 구현은 컴파일되지 않는 파일 안에 있다. **이 저장소가 이미 만난 형태(체크표)이고, 문서는 리더 소유라 고치지 않았다.**

---

## 3. baseline 귀속 (Baseline-attribution)

- 컴파일 오류는 **이번에 실제로 실행한 `cl`** 의 출력이다(모듈과 같은 플래그).
- 모든 계수는 **이번 실행의 grep** 이고, 범위를 각 문장에 적었다.
- 대조군 97·91 은 **같은 명령·같은 범위**의 결과다.
- SPEC 인용은 원문 줄 번호와 함께 적었다.
- **파일 크기 10,260 바이트는 `ls -l` 값이다. 카드의 "340줄" 은 제가 세지 않았다.**

---

## 4. 미검증 (Gaps)

- **`XPE_EXPORT` 가 과거에 정의되어 있었는지 조사하지 않았다.** git 히스토리를 보지 않았고, 이름이 `XPE_API` 로 바뀌면서 이 파일만 남은 것인지 처음부터 오타였는지 모른다.
- **340줄 전체의 정확성은 검증하지 않았다.** CPUID 시퀀스가 "정확하게 쓰여 있다"는 것은 카드의 표현이고, 저는 **컴파일되지 않는다**는 것만 확인했다. 컴파일되지 않는 코드의 논리는 검사할 수 없다.
- **`.moai/docs/acceptance.md` 의 다른 체크 항목은 세지 않았다.** 하나가 거짓이면 다른 것도 볼 값이 있지만, 문서 소유가 다르고 카드 범위 밖이다.
- **P2-ADV 쪽 `enhance_advanced` 모듈의 실제 상태는 보지 않았다** — 경로 부재만 확인했다. 소유가 다른 모듈이다.
- **AC-SIMD-001~004 의 요구(100 프레임 × 3 형상 = 300건)와 A-72·A-73 이 만든 것(1 프레임씩)의 차이는 재지 않았다.** "의도를 달성했다"는 것은 메커니즘에 대한 말이지 **커버리지 수량에 대한 말이 아니다.**

---

## 5. 잔여 위험 (Residual-risk)

- **읽는 사람이 속는다는 것이 이 건의 핵심 피해다.** 소스에 `XPE_EXPORT bool xpe_simd_is_avx2()` 가 있으면 DLL 에 있다고 믿는다. 없다. 그리고 `.moai/docs/acceptance.md` 의 체크가 그 믿음을 **보강**한다.
- **TODO 가 잘못된 비용을 암시한다.** `:356` 은 "API 만 있으면 된다"로 읽히지만 실제로는 (1) 그 파일이 컴파일되게 고치고 (2) 빌드에 넣고 (3) 테스트를 새로 쓰는 셋이다.
- **SPEC 이 옛 설계를 담고 있다.** M5-1~M5-4 의 `simd/` 트리는 실제 레이아웃과 다르고, M5-2 는 A-74 가 지운 잔해를 서술한다. 그대로 두면 다음 사람이 `simd/` 를 만들려 할 것이다.

---

## 6. 판정 요청 (결정하지 않았습니다)

1. **`simd_dispatch.cpp` 의 처분** — 세 갈래로 보입니다:
   (i) 고쳐서 빌드에 넣는다(`XPE_EXPORT` → `XPE_API`, CMake 목록 추가, 그리고 **소비자를 만든다**),
   (ii) 지운다(소비자 0, 컴파일 불가, A-72·A-73 이 같은 의도를 다른 방법으로 달성),
   (iii) 그대로 둔다.
   **(iii) 은 지금 상태이고, 이 보고서가 그 비용을 적었습니다.**
2. **`REQ-P1A-040` / AC-SIMD-005 의 메커니즘** — init 설정 플래그(`force_scalar`)가 요구인데 구현이 없고, A-72·A-73 은 **스위치 없이** 파리티를 달성했습니다. 요구를 바꿀지, 스위치를 만들지.
3. **`.moai/docs/acceptance.md:129` 의 체크 해제** — 문서 소유가 리더입니다.
4. **SPEC 의 `simd/` 트리와 M5-1~M5-4** — 실제 레이아웃과 다릅니다. 그리고 **M5-2 는 A-74 가 지운 커널을 서술합니다.**
5. **P2-ADV T-100** — `pending` 이 맞아 보이지만(다른 모듈·다른 요구), 판정은 리더 몫입니다.

---

Refs #160
