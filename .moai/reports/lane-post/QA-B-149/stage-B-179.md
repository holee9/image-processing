# QA-B-149 — `#179` 회귀 게이트 배선

커밋 `aac7700`. **푸시하지 않았습니다** — §5 에 이유를 적었습니다.

## 1. 배선한 것

`BenchmarkFreeze_ADV061_FractionalMeasure3072`
→ **`BenchmarkFreeze_ADV061_FractionalPerformanceRegressionGate3072`**

```cpp
constexpr long long kRegressionBudgetUs = 700000;   // 700 ms
const long long medUs = us[us.size() / 2];
EXPECT_LE(medUs, kRegressionBudgetUs) << ...
```

문턱 근거는 **CI 실측 8회**(`_ci_adv061.txt`), 이 기계가 아닙니다:

```
med (ms): 428.4  436.4  438.8  441.4  453.9  468.3  545.3  585.9
max (ms): 453.6  461.4  461.6  474.1  481.5  502.7  612.3  632.7
```

`700 = 585.9 × 1.19`. 관측된 러너 편차 **1.37배**는 통과하고, 두 배 느려지는
회귀는 잡습니다.

**median 을 잽니다.** max 는 공유 러너의 꼬리라 454..633 으로 흔들려, 코드가
아니라 스케줄링에서 빨개집니다.

## 2. 이름이 곧 라우팅 — 대조로 확인했습니다

시간 게이트가 **ci-post 잡에서 돌면 안 됩니다.** `ci.yml:289` 가 벽시계 시험을
의도적으로 빼는 이유("공유 러너는 기계를 잰다")가 그대로 이 게이트에 적용됩니다.

그래서 이름에 `Performance` 를 넣어 `ci.yml` 의 `-E` 에 걸리게 하고,
`BenchmarkFreeze` 를 유지해 `benchmark-regression.yml` 의 `-R` 이 고르게 했습니다.

**읽기로 끝내지 않고 두 정규식에 대조했습니다:**

| 패턴 | 결과 |
|---|---|
| `ci.yml -E "Performance\|Within[0-9]+ms\|PerformanceBudget\|LargeImagePerformance"` | **제외됨** (기대대로) |
| `benchmark -R "…\|BenchmarkFreeze\|…"` | **선택됨** (기대대로) |

### 판단이 뒤집힌 것을 기록합니다

옛 주석은 *"`Performance` 를 **일부러 피했다**"* 고 적습니다 — 측정만 할 때는
ci-post 에서도 도는 게 나았기 때문입니다. **단언이 붙으면서 이유가 반대로
뒤집혔습니다.** 잃는 것은 없습니다: 그 수치를 읽는 잡은 벤치마크 워크플로뿐입니다.

## 3. 초록이 무엇을 뜻하지 **않는가** — 실패 메시지에 박았습니다

> THIS IS A REGRESSION GUARD, NOT A REQ-ADV-061 CONFORMANCE VERDICT.

`REQ-ADV-061` 의 400 ms 를 CI 러너가 **한 번도** 지킨 적이 없습니다 — 8회 중
가장 빠른 단일 실행이 **417.5 ms**. 400 은 기준 하드웨어 주장이고(기록된
302/320 ms 는 개발용 i7-12700), 이 러너는 약 1.4배 느리며 **그 기계가 아닙니다.**

리더가 SPEC(`04e9f30`)에 두 행으로 박았고, 실패 메시지가 그것을 가리킵니다.

## 4. 반증

예산을 로컬 median 아래(**100 ms**)로 낮추니:

```
median 217.11 ms exceeds the 100 ms CI REGRESSION budget (#179).
THIS IS A REGRESSION GUARD, NOT A REQ-ADV-061 CONFORMANCE VERDICT. …
[  FAILED  ] …FractionalPerformanceRegressionGate3072
```

빨개지고 **의도한 문장이 그대로** 나옵니다. 되돌린 뒤 `700000` 이 하나인 것을
확인했습니다.

### 빌드 함정 하나 (재발)

첫 시도가 `===BUILD=1===` 였습니다 — 실패 메시지의 `\n` 이 파일에 **실제
줄바꿈**으로 들어가 `error C2001`. 파이썬 heredoc 으로 C++ 문자열을 쓸 때
되풀이되는 함정이라 Edit 도구로 고쳤습니다. **`===BUILD=` 를 먼저 읽지 않았으면
낡은 바이너리의 초록을 결과로 읽었을 자리입니다.**

## 5. 푸시하지 않은 이유

리더가 *"푸시해도 됩니다 — `ci.yml:14` 가 `group: xpe-ci-${{ github.ref }}` 라
ref 로 키가 갈려 main 실행을 취소하지 않습니다"* 라고 확인해 주셨고, 기술적
근거도 읽었습니다.

**그래도 제가 푸시하지 않습니다.** 이 워크트리의 `CLAUDE.local.md`(사용자 파일)가
`push` 금지를 조건 없이 적고, 별도로 기록된 위임은 *"PASS 판정 뒤 병합·push 는
**리더가** 처리"* 입니다. 즉 푸시 권한은 레인이 아니라 리더에게 위임돼 있습니다.
동료 세션의 허가로 사용자 파일의 금지를 제가 해제할 수는 없습니다.

**브랜치는 준비돼 있습니다** — `dev/postprocess` `aac7700`(+ main 병합).
리더가 푸시해 주시면 레인 CI 결과를 제가 읽고 판정하겠습니다. 사용자가 직접
지시하면 그때 제가 푸시하겠습니다.

## 6. 검증

```
===BUILD=0===
ctest ci-post: 698/698 통과      (_verify_179.log, ===CTEST=0===, main 병합 후)
한 프로세스 전체: 12 바이너리 0 실패
로컬 med: 221.6 ms               (참고용 — 문턱 근거 아님)
반증: 예산 100 ms → 빨강         (§4)
라우팅: 두 워크플로 정규식 대조   (§2)
```

## 7. 미검증 · 잔여 위험

- **레인 CI 에서 실제로 돌려 보지 않았습니다.** 게이트가 벤치마크 워크플로에서
  선택되는지는 **정규식 대조**까지이고, 실행 관측이 아닙니다. 푸시 뒤 확인해야
  닫힙니다.
- **8회는 하루치입니다**(2026-09-19). 러너 세대가 바뀌면 분포가 옮겨갑니다.
- **AVX2 120 ms 조항**은 손대지 않았습니다 — SPEC 이 "명시적 SIMD 없음"으로
  이미 적었고 판단은 SPEC 쪽입니다.
- 게이트는 `order=1.2` 한 점만 봅니다. SPEC 측정 조건은 1.0 도 기록하라고
  하는데, 그쪽은 여전히 **측정만** 되고 게이트되지 않습니다.

---

Refs #179
