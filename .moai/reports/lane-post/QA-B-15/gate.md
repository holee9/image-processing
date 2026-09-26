# QA-B-15 — ai 우선순위 회귀 가드 확대 (#119)

**레인**: Lane B (`dev/postprocess`)
**목적**: QA-B-13 이 교정한 5개 진입점 중 `xpe_dl_denoise` 만 고정돼 있던 공백을 메운다.
**결과**: 8건 추가(4함수 × 2), `AiErrorPrecedenceTest` 총 10건. ai 110→**118**, ci-ai 168→**176**.

---

## 1. 주장 (Claim)

1. 나머지 4개 진입점에 "미초기화+널 → `-1` / 같은 상태 유효 인자 → `-6`" 쌍을 추가했다.
2. 8건 모두 통과하며, **실효성 프로브로 가드가 실제로 무는 것을 관측**했다(§3).
3. 프로브는 **되돌린 함수의 케이스만** 실패시켰다 — 케이스가 함수별로 분리돼 있다(§3.1).
4. ai 소스는 **변경하지 않았다**(테스트만 추가). 프로브용 임시 수정은 원복 확인.
5. 재실측: `-R "Ai|AI"` 118/118, ci-ai 전체 176/176.

---

## 2. 추가한 8건

`tests/ai_tests/test_ai_fallback.cpp` — 기존 `AiErrorPrecedenceTest` 픽스처를 그대로 쓴다
(SetUp 에서 `xpe_ai_shutdown()`, TearDown 에서 `xpe_ai_init(...)` 복구).

| 진입점 | 널 인자 케이스 | 유효 인자 케이스 |
|---|---|---|
| `xpe_bodypart_recognize` | `(nullptr, nullptr, 0, nullptr)` → `-1` | `(&img, label, sizeof(label), &conf)` → `-6` |
| `xpe_stitch_images` | `(nullptr, 2, nullptr, nullptr)` → `-1` | `(parts[2], 2, &stitched, nullptr)` → `-6` |
| `xpe_bone_suppress` | `(nullptr, nullptr, nullptr)` → `-1` | `(&img, &soft, nullptr)` → `-6` |
| `xpe_ai_get_model_card` | `(nullptr, nullptr, 0)` → `-1` | `("denoise", buf, sizeof(buf))` → `-6` |

유효 인자 케이스는 **널 검사를 통과하도록** 구성했다. `xpe_stitch_images` 는 널 검사가
`!parts || partCount < 2 || !stitchedOut` 한 문장이라 `partCount = 2` 와 원소 2개 배열이
필요하고, 널 케이스에서는 `partCount = 2` 를 그대로 두어 **널 때문에 걸린다는 것**이
분명해지도록 했다(0 을 주면 범위 조건과 구분이 안 된다).

버퍼는 파일에 이미 있는 `makeTestBuffer()` 헬퍼를 재사용했다.

### 2.1 실행

```
[==========] Running 10 tests from 1 test suite.
[       OK ] AiErrorPrecedenceTest.NullArgumentOutranksNotInitialized
[       OK ] AiErrorPrecedenceTest.ValidArgumentsReachNotInitialized
[       OK ] AiErrorPrecedenceTest.BodypartRecognize_NullArgumentOutranksNotInitialized
[       OK ] AiErrorPrecedenceTest.BodypartRecognize_ValidArgumentsReachNotInitialized
[       OK ] AiErrorPrecedenceTest.StitchImages_NullArgumentOutranksNotInitialized
[       OK ] AiErrorPrecedenceTest.StitchImages_ValidArgumentsReachNotInitialized
[       OK ] AiErrorPrecedenceTest.BoneSuppress_NullArgumentOutranksNotInitialized
[       OK ] AiErrorPrecedenceTest.BoneSuppress_ValidArgumentsReachNotInitialized
[       OK ] AiErrorPrecedenceTest.GetModelCard_NullArgumentOutranksNotInitialized
[       OK ] AiErrorPrecedenceTest.GetModelCard_ValidArgumentsReachNotInitialized
===RUN_EXIT=0===
```
로그: `_green.log`

---

## 3. 실효성 프로브 (카드: 1개 함수)

통과는 "잡아낸다" 를 증명하지 않는다. `xpe_bone_suppress` 의 검사 순서를 **교정 전 상태로
일시 되돌려** 실행했다:

```
    Which is: -6
[  FAILED  ] AiErrorPrecedenceTest.BoneSuppress_NullArgumentOutranksNotInitialized
[==========] 10 tests from 1 test suite ran.
 1 FAILED TEST
===RUN_EXIT=1===
```
로그: `_guard_probe.log`

### 3.1 프로브가 함께 증명한 것 — 케이스 분리

실패한 것은 **되돌린 함수의 널 케이스 1건뿐**이다. 나머지 9건은 통과했다.

- 케이스가 함수별로 독립적이며 서로를 대신 검증하지 않는다.
- 즉 4개 함수 각각에 대해 실제 회귀 방지가 성립한다 — 1건이 우연히 전체를
  커버하는 구조가 아니다.
- 같은 함수의 **유효 인자 케이스는 통과**했다(순서를 되돌려도 유효 인자는 여전히 `-6`).
  이것이 쌍으로 둔 이유다: 널 케이스만 있으면 "초기화 가드가 통째로 사라진 회귀" 를
  놓치고, 유효 케이스만 있으면 "순서 역전" 을 놓친다.

### 3.2 원복 확인

`git diff --stat modules/ai/src/ai.cpp` → 빈 출력. 이 카드의 커밋은 ai 소스를 건드리지 않는다.

---

## 4. 재실측

| 대상 | 결과 | 로그 |
|---|---|---|
| `ctest -R "Ai\|AI"` (ci-ai) | **118 passed / 0 failed** | `_verify.log` |
| `ctest` 전체 (ci-ai) | **176 passed / 0 failed**, `AI_ALL_EXIT=0` | `_verify.log` |

## 5. baseline 귀속

| 항목 | baseline (출처) | 현재 | 판정 |
|---|---|---|---|
| `ci-ai -R "Ai\|AI"` | 110 (`QA-B-14/gate.md` §5) | **118** | +8 = 신규 케이스 정확히 일치 |
| `ci-ai` 전체 | 168 (`QA-B-14/gate.md` §5) | **176** | +8 = 동일 |
| `AiErrorPrecedenceTest` 케이스 수 | 2 (QA-B-14) | **10** | +8 |
| ai 소스 | a26b781 상태 | 무변경 | `git diff` 빈 출력 |

`ci-post` 는 `BUILD_AI=OFF` 이고 이 카드는 ai 테스트만 건드리므로 돌리지 않았다(§6).

## 6. 미검증 (Gaps)

- **`ci-post` 재실행 안 함.** 변경 파일이 `tests/ai_tests/test_ai_fallback.cpp` 하나이고
  이 소스는 `ci-post` 구성에 들어가지 않는다(`BUILD_AI=OFF`). 영향 없음을 **구성으로
  판단**했고 실행으로 확인하지 않았다.
- **실효성 프로브는 `xpe_bone_suppress` 1개에서만 했다**(카드 지시). 나머지 3개
  (`bodypart_recognize`, `stitch_images`, `get_model_card`)의 케이스는 통과만 확인했고
  "되돌리면 실패한다" 를 개별 관측하지 않았다. §3.1 의 분리 관측이 간접 근거이나
  각 함수를 직접 되돌려 본 것은 아니다.
- **`xpe_ai_set_fallback_mode` 와 `xpe_stitch_estimate_size` 는 대상이 아니다.**
  전자는 포인터 인자가 없고 후자는 초기화 검사가 없다(QA-B-13 §2) — 우선순위가
  성립하지 않아 가드를 만들 수 없다.
- **ai 는 stub 빌드(`XPE_AI_STUB_BUILD=ON`)로만 확인.** ONNX 실경로는 이 레인에서
  계속 미검증이다.
- **비-MSVC / Linux 미검증.**

## 7. 잔여 위험 (Residual risk)

- 유효 인자 케이스는 `NOT_INITIALIZED` 를 기대한다. 향후 이 함수들이 stub 를 벗어나
  실제 처리를 하게 되면 **미초기화 상태에서의 반환값은 그대로여야** 하며, 그렇지
  않으면 이 8건이 먼저 깨진다. 의도된 동작이지만 stub → 실구현 전환 시 소음이 될 수 있다.
- `xpe_stitch_images` 의 널 케이스는 `partCount = 2` 를 주어 널로만 걸리게 했으나,
  구현이 범위 검사를 널보다 앞으로 옮기면 이 케이스는 여전히 `-1` 이라 **통과한다**.
  즉 이 케이스는 "널이 초기화보다 먼저" 는 잡지만 "널이 범위보다 먼저" 는 잡지 않는다.
  계약 ①이 후자를 요구하지 않으므로 공백은 아니다.

---

## 8. 합격 조건 대조

| 조건 | 결과 | 근거 |
|---|---|---|
| 케이스 8건 추가 diff | PASS | §2 (4함수 × 2), `AiErrorPrecedenceTest` 2 → 10 |
| 실효성 프로브 로그 1건 | PASS | §3 `_guard_probe.log` (bone_suppress 되돌림 → 해당 케이스만 실패) |
| 재실측 | PASS | §4 118/118 · 176/176, §5 baseline 대조 |
| footer `Refs #119` | PASS | 커밋 메시지 |
