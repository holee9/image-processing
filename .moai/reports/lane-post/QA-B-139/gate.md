# QA-B-139 (#162) — 셋 중 둘은 이미 고쳐져 있고, `step_size` 는 **잇지 않습니다**

## 1. 주장 (Claim)

1. **카드 표 셋 중 둘은 현재 코드에 없습니다.** 항목 2·3 은 QA-B-122 에서 고쳐졌고 단언도
   있습니다. 카드는 그 이전 상태를 적고 있습니다.
2. **`step_size` 는 잇지 않습니다.** 코드가 "이 값이 무엇을 바꿔야 하는가" 에 답하지
   않습니다 — 근거는 §3.
3. 대신 **값이 살아 있는 척하던 배선을 걷어냈습니다**: 파싱·클램프·기본상수·로그 필드.
   키 자체는 **known 으로 남깁니다** — 그래야 미지 키 경고가 겹치지 않고, 무효 키 경고가
   **왜 무효인지까지** 말하는 단일 신호가 됩니다. (리더가 제안한 "known 에서 빼기" 를
   택하지 않은 이유는 §6.)
4. **제 반증이 처음에 안 터졌고, 원인은 시험의 눈먼 단언이었습니다.** `ParserRecognises`
   가 두 경고를 이름으로만 구별해 실패했고, 같은 설정 문자열의 앞선 호출이 중복 억제를
   먼저 먹었습니다. **대조군과 함께 다시 썼고 이제 터집니다.**
5. **기존 빨강 1건을 찾았습니다** — 제 변경과 무관하며 HEAD 에서도 동일하게 실패합니다.

## 2. 카드 전제 정정 — 항목 2·3

| # | 카드가 적은 상태 | 현재 코드 | 근거 |
|---|---|---|---|
| 2 | `levels` 가 `num_levels` **뒤**에 기록되어 `num_levels` 가 사라짐 | **고쳐짐.** `levels` 를 **먼저** 읽어 `num_levels` 가 덮어씀 | `enhance_advanced_helpers.cpp:229-238` (주석에 "Read FIRST so that ...") |
| 3 | 분기가 `num_levels` 의존, 2면 `edge_gain` 무효 | **고쳐짐.** level 0 을 **먼저** 판정 | `mfp_scalar.cpp:137-142` |

단언도 이미 있습니다:

- `ConfigValueDependency.NumLevelsWinsOverLegacyLevels` — 측정: `num_levels=5 + levels=2`
  가 `num_levels` 단독과 `maxdiff=0.000000000`, `levels` 단독과는 `10.797485`
- `ConfigValueDependency.KnownDivergence_LowLevelCountSilencesGains` — 측정:
  `edge@2 = 150.909546` (문턱 0.080585) 로 **움직입니다**

**남은 `texture_gain` 무효(≤3단)는 결함이 아니라 설계**입니다 — 중간 대역이 없습니다.
그 사실은 무효 키 경고가 말합니다(`multiscale_process.cpp:102-107`).

## 3. `step_size` — 코드가 답하지 않습니다

### 무엇을 바꿔야 하는가?

| 후보 의미 | 코드가 말하는 것 | 결론 |
|---|---|---|
| GL 정의의 보폭 `h` | 헤더가 `D^α f(x) = Σ (−1)^k C(α,k) f(x − k·h)` 라 적지만(`detail/fractional_derivative.h:52`), `computeFractionalMask` 는 **계수만** 만들고(`fractional_derivative.cpp:102-122`) 합성곱은 **정수 화소 간격**으로 샘플링합니다. `h` 를 담는 것도, 비정수 `h` 를 위한 보간도 **없습니다** | 새 코드 없이는 불가 |
| 미분 기여의 배율 | 그 자리에 이미 `gain = min(1 + 0.5*order, 2.0)` 이 있고(`fractional_derivative.cpp:402`), **`order` 에서 유도**되며 주석이 상한 이유를 명시합니다 — *"clamped so that overshoot limiting (SAF-100) still has room to clip cleanly"* | 안전 요구의 여유분과 충돌 |
| 반복 사이의 이완 계수 | 반복은 `fractional_process.cpp:133-138` 에서 **전량 적용 N회**이고 혼합 계수가 없습니다 | 새 거동 (배선 아님) |

**세 후보 모두 새 설계 결정이고, 하나는 안전 요구(SAF-100)의 여유분을 건드립니다.**
QA 가 정할 것이 아닙니다. → **잇지 않습니다.**

### 요구사항·호출자 확인 (`#196` 의 반대 방향 점검)

- **요구사항 없음**: `REQ-ADV-*` 어디에도 `step_size` 가 없습니다. 나오는 곳은 설계
  작업문서 2곳과 SDD 설정 표뿐입니다.
- **호출자 없음**: `gui/`, `clients/` 전체 검색에 **0건**.
- SDD 는 이미 실측 주석을 달고 있습니다 — `docs/project/sdd_adv.md:227`
  *"`step_size` 는 파싱·클램프되지만 출력에 닿지 않습니다"* (QA-B-78).

따라서 `#196` 처럼 "근거가 있는데 걷어내는" 경우가 **아닙니다.**

### 걷어낸 것

| 위치 | 걷어낸 것 | 왜 |
|---|---|---|
| `internal.h:57` | `XPE_FRAC_DEFAULT_STEP = 0.25f` | 읽는 곳이 파서 기본값 하나뿐 |
| `internal.h:105` | `parse_fractional_config` 의 `float& outStepSize` | 소비자 없음 |
| `enhance_advanced_helpers.cpp` | `step_size` 파싱·`[0.01,1.0]` 클램프 | 클램프 결과를 아무도 안 씀 — 코드가 효과를 주장함 |
| `fractional_process.cpp` | `float stepSize;` 와 로그의 `step={:.2f}` | **로그를 읽는 사람이 보폭이 적용됐다고 읽습니다** |

**남긴 것**: `kKnown` 의 `step_size`, 그리고 무효 키 경고와 그 사유 문장.

## 4. 단언 (양방향)

| 방향 | 단언 | 위치 |
|---|---|---|
| 안 움직인다 | `step_size` 0.01↔1.0 `maxdiff = 0.000000000` (문턱 0.080585), 그리고 **범위 밖 99.0 도 동일** (클램프가 사라졌으므로) | `FractionalStepSizeIsRecognisedAndReportedInert_162` |
| 도달한다 | 미지 키 경고가 `step_size` 를 **안 부른다** + **대조군**: 진짜 모르는 키는 부른다 | 같은 시험 |
| 말은 한다 | 무효 키 경고가 100프레임에 **정확히 1회** `step_size` 를 부른다 | `ConfigWarningOnce.InertKey_FractionalStepSizeWarns` |

세 단언이 **결정을 세 방향으로** 지킵니다: 잇으면 1번이, known 에서 빼면 2번이,
경고를 없애면 3번이 빨강입니다.

## 5. 반증 — **처음에 안 터졌고, 그게 이 카드의 수확입니다**

`kKnown` 에서 `step_size` 를 빼고(위반 주입) 빌드 `===BUILD=0===` 확인 후 시험을 돌렸더니
**통과했습니다.** 가드가 눈멀어 있었습니다.

**원인 둘, 둘 다 구조적입니다:**

1. `ParserRecognises` 는 알림에서 **키 이름**만 찾았습니다. 그런데 QA-B-122 이후 키를
   이름으로 부르는 경고가 **둘**입니다 — 미지 키(`... is not read by this entry point ...`)
   와 무효 키(`... is recognised but has no effect: ...`). 이름으로는 못 가릅니다.
2. `warn_unconsumed_keys_once` 는 미지 키 **집합**으로 스레드별 중복 억제를 하는데,
   같은 시험이 **같은 설정 문자열**을 앞서 호출해 경고를 이미 써 버렸습니다.

**고친 뒤**: 미지 키 **문장**을 찾도록 바꾸고, 이 시험에서만 쓰는 설정 문자열을 쓰고,
**존재 대조군**(진짜 모르는 키는 부른다)을 앞에 두었습니다. 같은 위반을 다시 주입하니
`===BUILD=0===` 에 **빨강이 떴고**, 위반을 되돌리니 초록으로 돌아왔습니다.

> 이름을 찾는 단언은 경고가 하나일 때만 맞습니다. 경고가 둘이 된 순간 조용히 눈멀었고,
> 그 사이에 아무 신호도 없었습니다.

## 6. 리더 제안과 다르게 한 곳 — 판단 요청

카드 §3 은 *"걷어내면 미지 키 경고의 침묵이 끝나야 한다"* 고 했습니다. **그렇게 하지
않았습니다.** 이유:

- `warn_inert_keys_once` 는 **`kKnown` 과 무관하게** 키가 JSON 에 있으면 발화합니다.
  `kKnown` 에서 빼면 **두 경고가 같은 키에 대해 동시에** 나옵니다.
- 두 문장 중 무효 키 쪽이 **더 많이 말합니다** — *"recognised but has no effect: the
  fractional derivative uses a fixed step; no requirement defines this value's effect"*.
  미지 키 쪽은 `not read by this entry point` 까지입니다.
- SDD 표를 보고 이 키를 쓴 사람에게 `unknown key` 는 **틀린 설명**입니다. 키는 알려져
  있고, 효과가 정의되지 않은 것입니다.

**되돌리기 쉽습니다** — `kKnown` 에서 한 항목입니다. 리더가 "침묵이 끝나야 한다" 를
고수하시면 그렇게 바꾸겠습니다.

## 7. 기존 빨강 1건 (제 것 아님)

`ConfigWarningOnce.InertKey_NestedTextureGainWarns` 가 **같은 프로세스에서 전체를 돌릴 때**
실패합니다(`CountMentioning(...) = 0`). 단독 실행은 통과합니다.

**귀속 확인**: 네 파일을 `git show HEAD:` 로 되돌려 재빌드(`===BUILD=0===`)하고 같은
전체 실행을 했더니 **똑같이 이 1건만 실패**했습니다. 제 변경과 무관합니다.

**왜 ctest 에서 안 보이는가**: `gtest_discover_tests` 가 시험마다 별도 프로세스로
등록하므로, 스레드별 중복 억제 상태가 시험 사이에 이월되지 않습니다. **순서 의존은
ctest 가 구조적으로 못 봅니다.**

별도 카드감으로 남깁니다 — 이 카드 범위 밖이고, 고치려면 경고 중복 억제의 수명을
바꿔야 합니다.

## 8. baseline 귀속 · 미검증 · 잔여 위험

**귀속**: 트리 `dev/postprocess`, `origin/main a9174e3` 병합 후.
빌드 `===BUILD=0===`(`_build4.log`), ctest ci-post 결론은 §9.

**미검증**

- `step_size` 를 **거부**(오류 반환)하는 선택지는 재지 않았습니다. 호출자가 0이라 안전해
  보이지만, 문서화된 키를 오류로 만드는 것은 API 계약 변경이고 요구사항 근거가 없습니다.
- SDD 표(`docs/project/sdd_adv.md:218,225`)가 여전히 `step_size`/기본값 0.25 를 설정
  항목으로 싣습니다. **docs 는 리더 소유**라 제가 안 고쳤습니다 — §10 에 블록을 둡니다.
- 무효 키 경고의 **중복 억제 수명**은 안 건드렸습니다(§7).

**잔여 위험**

- 제 변경은 **출력이 안 바뀌는 것이 목적**이라, "되돌리면 빨강" 식 반증이 성립하지
  않습니다. 대신 §4 의 세 단언이 결정을 지킵니다. 그 셋 중 하나라도 지워지면 이 결정은
  다시 조용해집니다.
- `ReportedAsUnknown` 은 경고 **문장**에 결합돼 있습니다. 문구가 바뀌면 다시 눈멉니다 —
  대조군이 함께 빨강이 되므로 **이번엔 조용하지는 않습니다.**

## 9. 검증

```
===BUILD=0===          (_build4.log, 타깃 없는 cmake --build build\ci-post)
```
ctest ci-post: `_verify.log`

## 10. 리더께 — SDD 표 수정 블록 (docs 는 리더 소유)

`docs/project/sdd_adv.md` 의 `step_size` 행에 적용할 내용입니다. 제가 옮기지 않았습니다.

```markdown
| step_size | float | (해당 없음) | (해당 없음) |

> **실측 주석 (2026-09-19, QA-B-139, #162).** `step_size` 는 **파싱되지 않습니다.**
> QA-B-78 이 "파싱·클램프되지만 출력에 닿지 않는다" 를 기록한 뒤, QA-B-139 가 그
> 파싱·클램프·기본값(0.25)·로그 필드를 걷어냈습니다 — 값이 적용되는 것처럼 보이게
> 하던 배선이었습니다. 키 이름은 계속 인식되며, `xpe_fractional_process` 가 무효 키
> 경고로 **왜 무효인지**까지 보고합니다.
>
> 잇지 않은 이유: 코드가 이 값의 의미를 정하지 않습니다. GL 정의의 보폭 `h` 는 표현이
> 없고(정수 화소 간격 샘플링, 보간 없음), 미분 기여를 조절하는 유일한 배율은 `order`
> 에서 유도되며 SAF-100 의 클리핑 여유분을 남기려고 상한이 걸려 있습니다.
```

---

Refs #162, #145
