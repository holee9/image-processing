# QA-A-76 — `simd_dispatch.cpp` 삭제, 그리고 `405/405` 이 어디서 나왔는지 (#160)

**카드**: `.moai/lanes/pre/inbox/QA-A-76.md` · **브랜치**: `dev/preprocess` · **기계**: Intel Core i7-12700

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | `modules/preprocess/src/simd_dispatch.cpp` 를 지웠다 |
| C2 | TODO 를 정정했다. **하나가 아니라 둘이었다** — 옆줄의 NEON TODO 도 같은 형태다 |
| C3 | **git 히스토리가 답을 준다: 이 파일은 한 번도 컴파일된 적이 없다.** CMake 목록에 오른 커밋이 0건이고, `XPE_EXPORT` 는 정의된 커밋이 0건이다 |
| C4 | **`405/405` 은 그 파일을 추가한 바로 그 커밋의 메시지에 있다.** 둘이 같이 태어났다 |
| C5 | 그런데 그 파일에 **CI 경고 수정 커밋이 하나 있다.** 빌드된 적 없는 파일에 대한 컴파일러 경고를 고쳤다 |
| C6 | 삭제 전/후 `xpe_simd_` 를 같은 범위로 셌다. **수는 안 줄었고, 뜻이 바뀌었다** — 그리고 **남는 실마리 둘**을 올린다 |
| C7 | 전체 643/69 통과, 경고 0, `BUILD_EXIT=0` |

---

## 2. 증거 (Evidence)

### C1 · C2 — 삭제와 TODO 정정

파일을 `git rm` 했다. 카드 지시대로 삭제 지점에 주석을 남기지 않았다(파일 전체가 사라지므로 둘 자리가 없다) — 기록은 커밋 메시지다.

**TODO 는 둘이었다.** 카드는 `:356` 만 지목했는데, 바로 아래 `:357` 이 같은 형태다:

```
# TODO SPEC-SIMD-001: test_offset_correct_avx512_parity.cpp — blocked by xpe_simd_force_scalar API
# TODO SPEC-SIMD-001: test_offset_correct_neon_parity.cpp   — blocked by NEON dispatch API (ARM only)
```

둘 다 **막혀 있지 않았다**:
- 두 테스트 파일 다 **존재하지 않는다**(A-75 가 `*avx512*` 를, 이번에 `*neon*` 을 워크트리에서 찾았고 소스는 0건 — `.agents/skills/.../neon.md` 한 건은 무관한 문서다). 기다리던 작업물이 없다.
- 두 API 다 **철회됐다**(§4.6 + `f1b963e`). NEON 쪽은 A-74 가 `offset_correct_neon` 을 지우면서 대상 커널도 없어졌다.

**둘 다 고쳤다.** 정정 문구에 옛 문장을 **인용으로** 남겨 두었다 — 무엇이 서 있었는지가 기록의 요점이고, 카드가 `acceptance.md` 체크를 "행은 남기고 해제" 한 것과 같은 처리다.

### C3 — 한 번도 컴파일된 적이 없다

카드의 4번 질문(“오타인가, 이름이 바뀐 잔해인가”)에 대한 답이다. **히스토리 전체를 두 축으로 봤다:**

```
$ git log --oneline -S "simd_dispatch" --all -- '*CMakeLists.txt'
(0건)

$ git log --oneline -S "define XPE_EXPORT" --all
(0건)

$ git log --oneline -S "XPE_EXPORT" --all
f1b963e  (리더의 문서 정정, 인용)
60dd828  (이 파일을 추가한 커밋)

$ git log --oneline -S "generate_export_header" --all
(0건)
```

- **어떤 커밋에서도 CMake 목록에 오른 적이 없다.**
- **어떤 커밋에서도 `XPE_EXPORT` 가 정의된 적이 없다.** 매크로를 만들어 줄 `generate_export_header` 도 쓰인 적이 없다.
- `XPE_EXPORT` 라는 토큰이 등장하는 커밋은 **딱 둘**이고, 하나는 이 파일을 만든 커밋, 하나는 리더가 그것을 인용한 이번 문서 커밋이다.

**그러므로 오타 쪽이다** — 이름이 `XPE_API` 로 바뀌면서 남은 잔해가 아니라, 처음부터 정의되지 않은 이름으로 쓰였다. 그리고 **컴파일했다면 A-75 가 본 `error C2143` 이 났을 것이므로, 성공적으로 컴파일된 적이 있을 수 없다.**

### C4 — `405/405` 의 출처

`60dd828` 의 커밋 메시지 본문:

```
### SIMD Dispatch & Parity
- simd_dispatch.cpp: AVX2/FMA/SSE4.1/scalar 런타임 선택
- SIMD parity harness: 405/405 checks passing
```

**같은 커밋이다.** 컴파일된 적 없는 파일과 "405/405 통과" 가 **한 커밋에서 같이 선언됐다.** 리더가 A-76 카드에서 "존재하지 않는 하네스를 센 수치" 라고 한 것과 맞고, 이제 **언제 어디서 생겼는지**까지 특정된다.

### C5 — 빌드된 적 없는 파일의 컴파일러 경고를 고친 커밋

이 파일을 건드린 커밋은 **둘뿐**이다:

```
b3ee1b2  fix(ci): simd_dispatch.cpp unsigned long long & 0xE6/0x06 C4365 수정
60dd828  feat(preprocess): ... SIMD 구현 (squash merge dev/preprocess)
```

`b3ee1b2` 의 내용:

```
MSVC /W4: unsigned long long & int 리터럴은 C4365 발생.
0xE6 → 0xE6ULL, 0x06 → 0x06ULL 로 변경.

-    return (xcr_mask & 0xE6) == 0xE6;
+    return (xcr_mask & 0xE6ULL) == 0xE6ULL;
```

**C4365 는 컴파일러가 이 파일을 컴파일해야 나오는 경고다.** 그런데 §C3 이 보이듯 이 파일은 어떤 커밋에서도 빌드에 없었고, 있었더라도 `C2143` 에서 먼저 죽는다. **그러니 저 경고는 이 저장소의 빌드가 이 파일에 대해 낸 것일 수 없다.**

무엇이 실제로 일어났는지는 **모른다** — 다른 CI 설정, 다른 트리, 또는 경고 목록을 파일명으로 추정해 고친 것일 수 있다. **추정하지 않는다.** 확실한 것은 **"CI 수정" 이라는 이름의 커밋이 빌드에 없는 파일을 고쳤다**는 것이고, 그것이 `405/405` 과 같은 계열의 흔적이다.

### C6 — 삭제 전/후 `xpe_simd_` (같은 명령·같은 범위)

**범위: 이 워크트리 전체, `build/`·`.git/` 제외.** 제 레인 보고서(A-75/A-76)는 증거 기록이므로 따로 셌다.

| | 전 | 후 |
|---|---|---|
| 전체 | 20 | 20 |
| 레인 보고서 제외 | 7 + 파일 자신 7 = 14 | **7** |
| 그중 살아 있던 코드 | 7 (파일 자신) | **0** |

**수는 줄지 않았고 뜻이 바뀌었다.** 카드가 "줄어야 한다"고 했는데 그렇게 되지 않은 이유를 그대로 적는다 — 제가 정정 문구 안에 **옛 TODO 두 줄을 인용으로 남겼기** 때문이다(§C2). 인용이 없었다면 6이 됐을 것이다. 인용을 남긴 것은 의도이고, 대신 **그것이 살아 있는 표시가 아니라는 것**을 여기 적는다.

**남은 7건의 내역과, 그중 실마리 둘:**

| 위치 | 상태 |
|---|---|
| `SPEC-XPE-P2-ADV/plan.md:62`, `:211`, `tasks.md:123` | (b) 확정된 다른 모듈 계획. **정상** |
| `SPEC-XPE-P1A/plan.md:172` | M5-1 취소선 + "철회". **정상** |
| `modules/preprocess/CMakeLists.txt:359` | 제 정정 문구 안의 **인용**. 살아 있는 TODO 아님 |
| **`SPEC-XPE-P1A/plan.md:65`** | `# never created at this path; see simd_dispatch.cpp` — **이제 없는 파일을 가리킨다** |
| **`SPEC-XPE-P1A/spec.md:433`** | `the function lives in modules/preprocess/src/simd_dispatch.cpp, which is absent from the CMake source list and does not compile` — **현재형으로 없는 파일을 서술한다** |

**뒤의 둘이 카드가 말한 "다음 사람이 집어들 실마리" 다.** 둘 다 리더가 어제 쓴 문장이고 그때는 참이었다 — 이제 파일이 없으므로 "see simd_dispatch.cpp" 는 가리킬 곳이 없고, "does not compile" 은 과거형이어야 한다. **문서는 리더 소유라 고치지 않았다.**

### C7 — 빌드와 전체

```
PRE_BUILD_EXIT=0 / COMMON_BUILD_EXIT=0
100% tests passed, 0 tests failed out of 643
100% tests passed, 0 tests failed out of 69
```

**카드가 경고한 대로 이 통과는 증거가 아니다** — 빌드에 없던 파일을 지웠으니 당연하다. 증거는 §C6 의 전/후 계수다. 빌드 결과를 적는 이유는 **삭제가 다른 것을 건드리지 않았다**는 것(예: 아무도 include 하지 않았다)을 보이기 위해서다.

---

## 3. baseline 귀속 (Baseline-attribution)

- 히스토리 계수는 **이번에 실행한 `git log -S`** 결과이고, 검색어를 §C3 에 그대로 적었다.
- 전/후 grep 은 **같은 명령·같은 범위**이며 제외 대상을 명시했다.
- `b3ee1b2` 의 diff 는 `git show` 원문이다.
- `60dd828` 의 커밋 메시지 인용은 `git show --stat` 출력에서 왔다.

---

## 4. 미검증 (Gaps)

- **`b3ee1b2` 의 C4365 경고가 실제로 어디서 나왔는지 모른다.** 다른 CI 설정인지, 다른 트리인지, 추정으로 고친 것인지 — 조사하지 않았고 추정하지 않는다.
- **`-S` 는 문자열 추가/삭제가 있는 커밋만 잡는다.** `define XPE_EXPORT` 가 다른 표기(예: 줄바꿈이 낀 형태)로 존재했다면 놓쳤을 수 있다. 그래서 토큰 자체(`XPE_EXPORT`)로도 훑어 커밋 둘뿐임을 확인했고, `generate_export_header` 도 봤다.
- **삭제한 340줄의 논리는 여전히 검사하지 않았다** — A-75 의 그대로다. 컴파일되지 않는 코드는 검사할 수 없다.
- **`405/405` 이 어떤 절차로 산출됐는지**는 커밋 메시지에 있다는 것까지만 확인했다. 그 수치를 만든 하네스가 어딘가에 있었는지는 조사하지 않았다.
- **Doxygen 로컬 확인 불가** — 여전(#159). 이번에 헤더를 바꾸지 않았다.

---

## 5. 잔여 위험 (Residual-risk)

- **문서 둘이 없는 파일을 가리킨다**(§C6). 지금은 방금 지웠다는 맥락이 살아 있지만, 시간이 지나면 "see simd_dispatch.cpp" 가 **찾다가 못 찾는 지시**가 된다.
- **커밋 메시지가 유일한 기록이다.** 카드 지시대로 코드에 주석을 남기지 않았으므로, 이 파일이 왜 없어졌는지는 `git log` 와 이 보고서에만 있다.
- **같은 형태가 더 있을 수 있다** — 빌드에 없는 소스 파일. 이번에 `modules/preprocess/` 의 CMake 목록과 실제 `src/` 를 대조하지 않았다. 셀 가치가 있어 보이지만 카드 범위 밖이다.

---

Refs #160
