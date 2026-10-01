# QA-A-84 — `#144` 판단 재료: "60 ms" 는 어느 기계 기준인가 (기존 증거만)

**카드**: `.moai/lanes/pre/inbox/QA-A-84.md` · **브랜치**: `dev/preprocess` HEAD `2ec9509` · **재측정 없음 · 코드 변경 없음**

---

## 1. 주장 (Claim)

| # | 주장 |
|---|---|
| C1 | **60 ms 목표의 정의 줄(`spec.md:195`)에는 기계가 적혀 있지 않다.** 적힌 조건은 AVX2 · 단일 스레드 · 3072x3072 · FLOAT32 넷이다. 빌드 구성도 없다 |
| C2 | 그 수치의 유도 근거(`:197`, `:208`)는 **개발 기계 측정 하한**이다 |
| C3 | 현재 값: 개발 기계 **62.4~63.9 ms**(QA-A-69). CI 는 **절대 ms 기록이 제 보고서에 없고**, SPEC 에 "1.6x on the CI runner"(A-67 코드)로만 있다 |
| C4 | **같은 SPEC 파일 안에서 `:195` 는 "regression gate <= 810 ms" 를 유지하고, `:235` 는 그 절대 게이트가 비율 게이트로 바뀌었다고 적는다** |
| C5 | `#144` 본문 네 항목 각각에 반영된 변경과 근거 카드를 표로 정리했다 |
| C6 | `#144` 에 사실 코멘트를 달았고, 게시 후 본문을 재조회했다 |

**카드 2번 질문의 답**: 정의가 가리키는 기계가 정해져 있지 않다. 그러므로 "충족/미충족" 을 이 정의로 가를 수 없다 — 이것은 SPEC 원문에서 읽은 사실이고, 닫을지는 리더 판정이다.

---

## 2. 증거 (Evidence)

### C1 · C2 — 정의와 유도 근거 (원문)

`spec.md:195`:

```
- **Performance** (redefined 2026-09-12, #144 — see the note below): regression gate **<= 810 ms**
  and improvement target **<= 60 ms (AVX2, single thread)** for a 3072x3072 FLOAT32 frame.
```

`spec.md:197`:

```
> **Why the old numbers were replaced.** Both sat **below the measured lower bound**, so no
> implementation could reach them (QA-A-56, this machine, 3072x3072):
```

`spec.md:208` (발췌):

```
> ... The improvement target is ~2.2x the 27.1 ms AVX2 lower bound, leaving room for neighbour
> gather (excluded from that bound), map stores, and that same spread.
```

27.1 ms 는 `:197` 표의 "this machine" 측정값이다. **정의 줄은 그 기계를 이어받는다고 적지 않는다.**

### C3 — 현재 값의 출처

| 값 | 출처 원문 |
|---|---|
| 62.4 ~ 63.9 ms (개발 기계) | QA-A-69 `:106` "이 기계에서 **1.0~1.1배**(62.4~63.9 ms). **"도달"이라고 쓰지 않는다** — 실행에 따라 60 ms 위아래이고, CI 는 A-68 시점에 1.6배였다." |
| 62.4 ~ 67.8 ms (같은 실행 게이트 표본 폭) | QA-A-69 `:104` |
| 0.552 (CI 비율, A-67 코드, 런 `f19321a`) | QA-A-69 `:125` |
| 1.6x (CI) | `spec.md:273` "(1.6x on the CI runner)" — QA-A-67 시점, 리더 작성 |
| CI 측 A-69 코드 성능 | **기록 없음** — QA-A-69 `:136` "CI 측 성능은 재지 않았다" |

**CI 의 검출 절대 시간(ms)은 제 보고서 어디에도 없다.** 1.6x 는 SPEC 에 적힌 배율이고, 0.552 는 기준 커널 대비 비율이지 ms 가 아니다. 역산하지 않았다.

### C4 — 같은 파일의 두 서술

`spec.md:195` "regression gate **<= 810 ms**"

`spec.md:235`:

```
> **The gate is now a machine-relative ratio, confirmed on both machines (resolved 2026-09-16, QA-A-60).**
> The former absolute gate of 810 ms came from this development machine only; the first CI run to
> execute it measured **1340.9 ms**, **1.91x slower**, and failed.
```

테스트 파일의 현재 상한:

```
modules/preprocess/tests/test_runtime_detection_performance_gate.cpp:188:
constexpr double kRatio3072Limit = 0.85;
```

`:235` 의 원문이 **810 ms 는 개발 기계에서만 나온 값이었다**고 적는다 — 같은 SPEC 이 절대 수치의 기계 의존성을 이미 기록하고 있다. 60 ms 에는 같은 서술이 없다.

### C5 — 네 항목

| 항목 | 반영된 것 | 근거 |
|---|---|---|
| 예산 재정의 | 35 ms/12 ms → 810 ms/60 ms (2026-09-12). 옛 수치는 측정 하한 아래 | QA-A-56, `spec.md:195`·`:197` |
| 비율 게이트 | 절대 810 ms → 기준 커널 대비 비율, 상한 10.00 → 2.20 → 0.85 | QA-A-60·A-66·A-67, `spec.md:235`·`:255`·`:273`, 테스트 `:188` |
| AVX2 | 출하 진입점 픽셀 루프 AVX2, 비트 동일, 692.7 → 163.7 ms | QA-A-65, `spec.md:255`·`:461` |
| 시그마 | 전역 σ 137.8 → 46.7 ms, 검출 163.7 → 64.4 ms | QA-A-67, `spec.md:273` |

이후 QA-A-68(스칼라 경계 측정, 수정 안 함)·QA-A-69(행 꼬리 제거, 2.149 → 0.985 ms). `spec.md:299`·`:303`.

### C6 — 코멘트

```
https://github.com/holee9/image-processing/issues/144#issuecomment-5706588490
check: n=8 has208=true has209=false
```

**게시 전에 인용 행 번호를 대조해 하나를 고쳤다.** 초안이 "새 수치 선정 방식" 을 `:209` 로 적었는데 원문은 `:208` 이었다. `grep -n` 으로 확인하고 고친 뒤 올렸고, 게시 후 재조회로 `:208` 이 있고 `:209` 가 없음을 확인했다.

---

## 3. baseline 귀속 (Baseline-attribution)

- SPEC 인용은 `origin/main` 병합 후 HEAD `2ec9509` 의 파일에서 `grep -n` / `sed -n` 로 읽은 원문이다.
- 성능 수치는 전부 QA-A-65~A-69 보고서와 SPEC 에 이미 적혀 있던 값이다. **이번에 실행한 측정은 없다.**
- 행 번호는 게시 직전에 다시 대조했다.

---

## 4. 미검증 (Gaps)

- **`research.md` 는 읽지 않았다.** `spec.md:206` 에 따르면 옛 수치(35 ms/12 ms)의 출처였으나, 새 60 ms 의 기계 정의와는 무관하다고 보고 범위에서 뺐다.
- **SPEC 다른 절(수용 기준, `acceptance.md`)에 60 ms 가 기계와 함께 다시 적혀 있는지는 찾지 않았다.** 이번 계수는 `spec.md` 만이다.
- **CI 측 A-69 코드의 검출 시간은 없다.** 재지 말라는 카드 지시대로 재지 않았다.
- **`#144` 기존 코멘트 7건은 첫 줄만 읽었다.** 내용이 이번 코멘트와 겹치는지 확인하지 않았다.

---

## 5. 잔여 위험 (Residual-risk)

- **정의에 기계가 없으면, 어느 쪽으로 판정해도 한쪽 기계의 사실과 어긋난다.** 개발 기계로 읽으면 CI 에서 1.6배가 남고, CI 로 읽으면 유도 근거(개발 기계 하한)와 맞지 않는다. 810 ms 가 이미 같은 경로(`:235`)를 한 번 밟았다.
- **개발 기계 값은 60 ms 선에 걸쳐 있다**(62.4~67.8 ms 표본 폭). 실행 하나로 도달 여부를 말할 수 없다 — QA-A-69 가 같은 이유로 "도달"을 쓰지 않았다.
- **`:195` 의 810 ms 문구**는 남아 있는 한 읽는 사람이 절대 게이트가 살아 있다고 볼 수 있다.

---

Refs #144
