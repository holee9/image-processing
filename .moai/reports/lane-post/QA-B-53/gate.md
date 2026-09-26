# QA-B-53 게이트 보고서 — `xpe_gsvg_process` 버퍼 길이 인자

**카드**: QA-B-53 (#152 #142)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-53/`
**커밋 1건**: `22ee974`
**선행**: `git merge origin/main` 완료 (B-52 병합분 포함)

---

## 1. 시그니처와 단위 선택

### 1.1 확장된 시그니처

```c
XPE_API XpeErrorCode xpe_gsvg_process(void* handle,
                                      const uint16_t* src,  size_t srcCount,
                                      uint16_t*       dst,  size_t dstCount,
                                      int width, int height,
                                      const float*    gainMap, size_t gainCount);
```

**길이를 끝에 몰아 붙이지 않고 각 버퍼 옆에 뒀다.** 인자 순서를 틀리기 어렵게 하려는
것이고, 외부 호출자가 하나뿐이라 가독성을 택할 여유가 있었다.

### 1.2 단위 — **요소 수(elements), 바이트 아님**

| 근거 | 내용 |
|---|---|
| 타입이 둘이다 | `src`·`dst` 는 `uint16_t`, `gainMap` 은 `float`. 바이트로 통일하면 호출자가 **인자마다 맞는 `sizeof` 를 골라야** 하고, 짝이 어긋난 값은 **검증을 통과하면서 여전히 틀린다** |
| 비교 대상과 단위가 같다 | 각 길이를 `width * height` 와 직접 비교한다 — `width`·`height` 가 이미 쓰고 있는 단위다. 변환이 없으니 변환 실수도 없다 |
| 호출부에서 맞는 쪽이 짧다 | C++ 호출자는 `buf.size()` 를 넘기면 맞다. 바이트 형식은 `buf.size() * sizeof(...)` 가 되어 **틀리기 쉬운 쪽이 더 길다** |

반대 논거도 적어 둔다: 요소 수는 "개수인지 바이트인지" 를 이름만으로 구분해야 하므로
`...Count` 접미사에 의존한다. 그래서 헤더에 `@par` 단락으로 사유를 명시했다 —
판단을 코드 밖에 두지 않는다.

### 1.3 `gainMap` 이 NULL 일 때 — 길이 0 허용

**허용한다.** 존재하지 않는 버퍼에 의미 있는 길이는 없고, 요구하면 vignette 를 쓰지 않는
**모든** 호출자가 숫자를 지어내야 한다. NULL 이 아닌데 짧으면 거절한다.

**검사 순서**: NULL·0 먼저, 부족은 그 다음 — api-spec 의 출력 버퍼 규칙(2026-09-11 결정)과
같은 순서다. 없는 버퍼와 작은 버퍼는 다른 사건이고, 순서가 그 구분을 유지한다.

---

## 2. RED → GREEN — **반환 코드만으로 보지 않았다**

첫 접근 뒤에 놓인 검사는 **올바른 코드를 돌려주면서도 넘겨 읽는다.** 그래서 가드 페이지로
`over-read` 가 0 이 되는 것까지 확인했다(카드 3항).

| 케이스 | 변경 전 (B-52) | 변경 후 (`_green.log`, BUILD=0) |
|---|---|---|
| `src` 절반 (`srcCount=2048`, 약속 4096) | `rc=0`, **overread=YES** | **`rc=-1`, overread=no** |
| `gainMap` 절반 (vignette 켬) | `rc=0`, **overread=YES** | **`rc=-1`, overread=no** |
| 전 길이 대조군 | `rc=0`, overread=no | `rc=0`, overread=no (불변) |
| `gainMap=NULL`, `gainCount=0` | — | **`rc=0`** (계약대로 성공) |

**B-52 의 `KnownDivergence_` 2건은 틀렸던 게 아니라 대체됐다.** 길이 인자가 없던 동안에는
검증 대상 자체가 없어 **고칠 수도 없었고**, 시그니처가 바뀌자 기대가 확정됐다. 경위를
주석에 남기고 정상 기대로 되돌렸다(B-41·B-49·B-51 방식).

### 2.1 고쳐지지 않은 경계 — 새 `KnownDivergence_` 1건

```
gsvg misstated-length claimed=4096 actually_mapped=2048 rc=0 overread=YES
```

호출자가 **길이를 거짓으로 말하면**(절반 버퍼에 전체 개수를 주장) 여전히 넘겨 읽는다.
**이것은 검사의 구멍이 아니라 길이 인자가 할 수 있는 일의 한계다** — 함수는 들은 것을
믿는 것 말고 할 수 있는 일이 없다. 산문이 아니라 **스위트에** 남겨, 나중에 이것까지 잡는
경계 검사 기법이 들어오면 변경으로 드러나게 했다.

---

## 3. 호출자 — 재grep 실측

리더의 grep 도 한 번의 관측이므로 다시 쟀다(`git`·`build` 산출물 제외):

| 위치 | 건수 | 조치 |
|---|---|---|
| **모듈 밖** `tests/e2e_post_pipeline/test_e2e_full_pipeline.cpp:170` | **1** | 갱신 — `u16.size()` 를 src·dst 양쪽에, `gainCount=0` |
| 모듈 안 테스트 6파일 | 35 | 전부 갱신 |
| `clients/`·`gui/` C# (`--include=*.cs`) | **0** | 확인만 |
| `.moai/project/api-spec.md` · `docs/project/api-spec.md` | 2 (문서) | §5 참조 — **미갱신, 소유 밖** |

**리더의 관측과 일치한다.** 외부 호출자는 하나뿐이고 C# 소비자는 없다.

---

## 4. Export 대조 (`_exports.log`)

```
1    0 xpe_gsvg_init
2    1 xpe_gsvg_process
3    2 xpe_gsvg_shutdown
4    3 xpe_gsvg_version
```

**4개 유지, 이름 불변.** 다만 **A-39 교훈대로 이름 목록이 같다고 ABI 가 같은 것은 아니다** —
`xpe_gsvg_process` 의 시그니처는 3개 인자만큼 바뀌었고, **그것이 이 카드가 의도한 변경이다.**
바이너리 호환은 깨진다. C# 소비자가 없고 외부 호출자가 하나라서 감당 가능하다고 판단한
리더 결정의 전제가 §3 에서 재확인됐다.

---

## 5. 반증 (`_falsify.log`)

두 가드를 `* 4u <` 로 **약화**(삭제 아님):

```
===BUILD=0===                    <- 빌드 성공
gsvg short-src      rc=0 overread=YES   [FAILED]
gsvg short-gainMap  rc=0 overread=YES   [FAILED]
[  OK  ] FullLengthBuffersProcessNormally
[  OK  ] NullGainMapWithZeroLengthIsAccepted
[  OK  ] KnownDivergence_MisstatedLengthIsStillReadPastItsEnd
```

**대상 2건만 실패하고 나머지 3건은 통과한다** — 새 가드가 정확히 그 둘을 잡고 있으며,
대조군·NULL 허용·오기 한계에는 영향이 없다는 것이 한 로그에 있다. 실패 모습이 B-52 의
변경 전 관측과 **같은 형태**(rc=0 + overread=YES)로 복귀하는 것도 확인점이다.

약화 형태를 고른 이유는 B-49~B-52 와 같다(조건 삭제 → 인자 미사용 → `/WX` 파손 →
낡은 바이너리가 통과를 찍음). 반증 뒤 원복했다.

---

## 6. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| GREEN | BUILD=0, 5/5, **over-read 0** | `_green.log` |
| 반증 | BUILD=0, 대상 2건만 재실패 = **재현됨** | `_falsify.log` |
| export | 4개, 이름 불변 | `_exports.log` (dumpbin) |
| 외부 호출자 | 1건 (갱신), C# 0건 | §3 재grep |
| 이전 ctest | 472 / 211 / 173 | QA-B-52 `_verify.log` |
| 현재 ctest | **474 / 211 / 173** (gsvg +2) | `_verify.log` |
| 빌드 경고 | 0 (`grep -c "warning C"`) | `_verify.log` |
| 변경 파일 | 9 (헤더·구현·테스트 6·e2e 1) | `git show --stat` |

---

## 7. 미검증 (Gaps)

- **api-spec 두 부(`.moai/project/`·`docs/project/`)의 §12.1 시그니처가 낡았다.**
  소유 밖이라 고치지 않았다. 같은 줄에서 **별도 오류 1건**도 보인다 — 4개 export 를
  `init/process/process_ex/shutdown` 으로 적어 두었는데, dumpbin 실측은
  `init/process/shutdown/**version**` 이고 `xpe_gsvg_process_ex` 는 **헤더에도 소스에도
  존재하지 않는다.** **리더 처리 필요.**
- **가드 페이지 확인은 Windows 전용**이다. 다른 플랫폼에서는 프로브 전체가 비활성이라
  `over-read 0` 이 검증되지 않는다(반환 코드만 남는다).
- **`dst` 를 짧게 준 경우는 재지 않았다.** 가드는 `src`·`dst` 를 대칭으로 보므로 같을 것으로
  **판독**되지만, 측정한 것은 `src` 쪽이다.
- **길이가 `count` 보다 **큰** 경우는 그대로 허용**한다(`<` 비교). 여분 버퍼는 B-49 의
  "여분 바이트는 패딩" 판정과 같은 방향이지만, 이 카드에서 단언으로 고정하지 않았다.
- **`width * height` 오버플로는 보지 않았다.** 둘 다 `int` 라 큰 값에서 `size_t` 곱이
  넘칠 수 있는지 이 카드가 확인하지 않았다.

---

## 8. 잔여 위험 (Residual-risk)

- **ABI 가 깨진다.** 이름은 같은데 시그니처가 달라, 옛 `gsvg.dll` 을 쓰는 바이너리가 있다면
  **조용히 잘못된 스택으로 호출**된다. C# 소비자가 없고 외부 호출자가 하나여서 감당
  가능하다고 판단했지만, **재빌드되지 않은 소비자가 어딘가 있다면 그 형태가 가장 나쁘다.**
- **거짓 길이는 여전히 막지 못한다**(§2.1). 길이 인자는 호출자의 주장을 검증할 뿐,
  주장 자체를 검사하지 못한다.
- **호출자 갱신 35건 중 다수가 기계 변환이다.** 빌드와 474건 통과가 근거이지만,
  길이 표현이 의미상 맞는지(예: `(gainMaps[i] ? kCount : 0u)`)는 **컴파일러가 봐 주지 않는다.**
- 커밋은 push 전까지 미푸시 유일본이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` | 환경 + 세 프리셋 재빌드·전체 ctest |
| `_build1.log` ~ `_build3.log` | 호출자 갱신 단계별 (BUILD=1 → 1 → **0**) |
| `_b53.bat` / `_green.log` | GREEN — BUILD=0, 5/5, over-read 0 |
| `_falsify.log` | 가드 약화 — BUILD=0, 대상 2건만 재실패(**재현됨**) |
| `_exports.bat` / `_exports.log` | dumpbin — export 4개, 이름 불변 |
| `_verify.log` | 최종 474 / 211 / 173, 경고 0 |
