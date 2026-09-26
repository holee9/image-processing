# QA-B-66 게이트 보고서 — 두 분기가 같은 식이다

**카드**: QA-B-66 (#156) · **레인**: Lane B (`xpe-post`, `dev/postprocess`)
**커밋 1건**: `2103ea4` · **증거**: `.moai/reports/lane-post/QA-B-66/`

---

## 1. 주장 (Claim)

| # | 항목 | 결론 |
|---|---|---|
| 1 | 두 분기가 같은 식인가 | **그렇다.** 코드에서 직접 확인, 실측 maxdiff **5.96046e-08**(1 ulp) |
| 2 | 반대 극성 단언 | 추가, **오늘 실패**(`DISABLED_`). 반증으로 차이를 본다는 것 확인 |
| 3 | `LINEAR_EXACT` 쓰는 기존 테스트 | **2곳뿐, 둘 다 결함을 못 본다** |
| 4 | 요구 번호 오기 | **8건** — 한 건이 아니라 블록 전체가 한 칸 밀려 있었다 |
| 5 | 식 자체 | **바꾸지 않았다** — DICOM 원문 대기 |

---

## 2. 대수 — 리더 메시지가 아니라 코드에서 확인

`modules/display/src/voi_lut.cpp`:

```cpp
:38  case XPE_VOI_LINEAR:        const float lo = center - width * 0.5f;
:41                              float val = (px[i] - lo) / width * range + minOut;

:47  case XPE_VOI_LINEAR_EXACT:
:50                              float val = ((px[i] - center) / width + 0.5f) * range + minOut;
```

전개:

```
(px - (center - width/2)) / width * range + minOut
  = ((px - center) + width/2) / width * range + minOut
  = ((px - center)/width + 0.5) * range + minOut          ← LINEAR_EXACT 와 동일
```

**두 분기는 같은 함수를 계산한다.** 남는 차이는 부동소수 연산 순서뿐이고, 측정값이
그것을 확인한다 — `[0,1]` 출력에서 **5.96046e-08**, 딱 1 ulp.

---

## 3. 반대 극성 단언 — 오늘 실패하는 것이 맞다

### 3.1 왜 극성이 문제인가

기존 `KnownDivergence_VoiLinearExactEqualsVoiLinear` 는 **지금 동작을 고정**한다.
그것만 있으면 훗날 REQ-DISP-010 을 구현하는 사람이 **빨간 테스트**를 보게 되고,
빨간 테스트의 가장 싼 해석은 **"내가 뭘 깨뜨렸다"** 다. 고침이 되돌려질 위험이 있다.

그래서 요구가 가리키는 방향으로도 단언한다 — **두 모드는 달라야 한다.**

### 3.2 `DISABLED_` 로 둔 이유 — 미해결이 아니라 막힘

무엇을 **대신** 계산해야 하는지는 DICOM PS3.3 C.11.2.1.3 원문 없이 쓸 수 없다.
지어내면 **검증 불가능한 숫자가 "표준 준수" 라는 이름으로** 들어간다. #156 이 외부 입력
대기인 이유가 그것이고, 이 카드는 그 선을 넘지 않았다.

주석에 "표준이 오면 DISABLED_ 를 떼고 위쪽 KnownDivergence_ 를 은퇴시켜라" 를 적었다.

### 3.3 임계값 — `> 0` 이 아닌 이유

두 분기는 오늘 **1 ulp(~6e-08)** 만큼 다르다. `> 0` 단언은 **반올림으로 초록이 되어
고쳐지지 않은 결함을 고쳐졌다고 보고한다.** 기존 파일의 `kMeaningful = 1e-4`(같은
`[0,1]` 스케일)를 그대로 썼다 — **세 자릿수 간격**이 "모드가 다르다" 와 "덧셈 순서가
다르다" 를 가른다.

### 3.4 RED (`_red.log`) — 의도된 실패

```
===BUILD=0===
[  INFO ] LINEAR vs LINEAR_EXACT maxdiff=5.96046e-08 (one ulp ~6e-08)
[       OK ] ParameterDependency.KnownDivergence_VoiLinearExactEqualsVoiLinear
[  INFO ] LINEAR vs LINEAR_EXACT maxdiff=5.96046e-08 (threshold 0.0001; ...)
[  FAILED  ] ParameterDependency.DISABLED_VoiLinearExactMustDifferFromLinear
===EXIT=1===
```

---

## 4. 반증 — 카드가 지시한 확인 (`_falsify.log`)

`LINEAR_EXACT` 의 `+ 0.5f` 를 `+ 0.0f` 로 바꾸고 재실행:

```
===BUILD=0===
[  INFO ] LINEAR vs LINEAR_EXACT maxdiff=0.5
[  FAILED  ] ParameterDependency.KnownDivergence_VoiLinearExactEqualsVoiLinear   ← 빨강으로
[  INFO ] LINEAR vs LINEAR_EXACT maxdiff=0.5 (threshold 0.0001; ...)
[       OK ] ParameterDependency.DISABLED_VoiLinearExactMustDifferFromLinear     ← 초록으로
===EXIT=1===
```

**카드가 요구한 대로 초록으로 바뀐다.** maxdiff 가 5.96e-08 → **0.5** 로 뛰므로 단언이
두 모드의 차이를 실제로 보고 있다.

**덤으로 나온 것**: 두 극성이 **같이 뒤집힌다.** 표준이 오면 이 쌍이 그대로 신호가 된다 —
하나가 초록이 되고 하나가 빨강이 되는 것이 같은 사실의 양면이다.

원복했다(`grep -c FALSIFICATION` → 0).

---

## 5. `LINEAR_EXACT` 를 쓰는 기존 테스트 — 2곳뿐, 둘 다 결함을 못 본다

카드가 "셋 다 결정론만 보고 있을 가능성이 높다" 고 했다. 세어 보니 **둘**이고, 결정론이
아니라 **두 식 모두 통과하는 단언**이었다.

| 위치 | 단언 | 결함을 보는가 |
|---|---|---|
| `test_voi_lut.cpp:101` `LinearExact_CenterValue` | `center=40, width=80` 입력 40 → 출력 `127.5 ± 1.0` | **아니다.** LINEAR 도 center 를 중점으로 보낸다 — **두 식 모두 통과** |
| `test_parameter_dependency.cpp` `KnownDivergence_…` | `maxdiff < 1e-6` (같다는 것) | **아니다.** 결함을 고정한다(의도된 특성화) |

**이 결함은 통과하는 테스트 아래에서 살아 있었다.** `LinearExact_CenterValue` 는 이름이
`LINEAR_EXACT` 를 말하지만, 실제로는 **두 모드가 공유하는 성질**만 확인한다 —
`LINEAR_EXACT` 를 `LINEAR` 로 바꿔도 통과한다.

---

## 6. 요구 번호 오기 — 한 건이 아니라 블록 전체

SPEC(`SPEC-XPE-P1B-DISP/spec.md:299-305`)의 매핑:

| 번호 | 내용 |
|---|---|
| REQ-DISP-009 | LINEAR |
| REQ-DISP-010 | LINEAR_EXACT |
| REQ-DISP-011 | SIGMOID |
| REQ-DISP-012 | clamp to `[minOut, maxOut]` |

**010 이후 인용이 한 칸씩 밀려 있었다.** 카드는 `voi_lut.cpp:56` 한 건을 지목했지만
전수하니 **8건**이다:

| 파일 | 위치 | 옛 인용 | 고침 |
|---|---|---|---|
| `voi_lut.cpp` | :56 SIGMOID 분기 | 012 | **011** |
| `test_voi_lut.cpp` | :66 clamp to minOut | 010 | **012** |
| | :82 clamp to maxOut | 010 | **012** |
| | :98 LINEAR_EXACT 절 제목 | 011 | **010** |
| | :102 LINEAR_EXACT 케이스 | 011 | **010** |
| | :119 SIGMOID 절 제목 | 012 | **011** |
| | :123 SIGMOID 케이스 | 012 | **011** |
| | :141 SIGMOID clamp | 010 | **012** |

**옛 문구를 이웃에 남기지 않았다**: B-59 가 `test_parameter_dependency.cpp` 에 남긴
"(EXACT 분기 주석이 011 을 인용한다. 고치지 않음)" 메모는 그 건이 이미 고쳐졌으므로
**현재 상태로 갱신**했다 — 정정 옆에 갓 검증된 것처럼 읽히는 옛 문장이 남지 않게.

---

## 7. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| LINEAR vs LINEAR_EXACT maxdiff | **5.96046e-08** (1 ulp) | `_red.log` |
| 임계값 | **1e-4** (기존 `kMeaningful`, 같은 스케일) | `_red.log` |
| 신규 케이스 현재 상태 | **FAILED** (의도) | `_red.log`, `BUILD_EXIT=1` |
| 반증 후 maxdiff | **0.5** | `_falsify.log` |
| 반증 후 신규 케이스 | **OK** (초록) | `_falsify.log` |
| 반증 후 기존 특성화 | **FAILED** (빨강) | `_falsify.log` |
| `LINEAR_EXACT` 사용 테스트 | **2곳** | §5 |
| 요구 번호 정정 | **8건** | §6 |
| 이전 ctest | 533 / 224 / 177 | QA-B-65 `_verify.log` |
| 현재 ctest (기본 실행) | **533 / 224 / 177** | `_verify.log` |
| 빌드 경고 | 0 | `_verify.log` |

신규 케이스는 `DISABLED_` 이므로 기본 실행의 통과 수를 늘리지 않는다 — 의도한 동작이고,
gtest 는 매 실행 끝에 `YOU HAVE 1 DISABLED TEST` 를 찍어 상기시킨다.

---

## 8. 미검증 (Gaps)

- **`LINEAR_EXACT` 가 무엇이어야 하는지 모른다.** DICOM PS3.3 C.11.2.1.3 원문을 보지
  않았고, 추측하지 않았다. 카드가 그은 선이다.
- **`DISABLED_` 는 기본 실행에서 돌지 않는다.** 표준이 와서 식이 고쳐져도 **누군가
  `--gtest_also_run_disabled_tests` 를 돌리거나 접두사를 떼야** 초록이 보인다.
  gtest 의 꼬리 경고가 유일한 상시 신호다 — 더 강한 장치(CI 에서 disabled 를 세는 등)는
  이 카드 범위 밖이다.
- **다른 모드 쌍은 보지 않았다.** SIGMOID vs LINEAR 는 기존 테스트가 다르다는 것을
  단언하고 있지만(`:132`), SIGMOID 자체가 요구 식과 맞는지는 이 카드에서 재지 않았다.
- **`LinearExact_CenterValue` 를 고치지 않았다.** 두 식 모두 통과하는 단언이라는 것은
  적었지만, 변별력 있는 단언으로 바꾸는 것은 **무엇이 옳은지 모르는 상태에서는 불가능**
  하다 — 표준이 오면 같이 처리할 자리다.
- **요구 번호 전수는 `voi_lut` 계열 2파일에 한정했다.** `display` 모듈의 다른 파일에
  같은 밀림이 있는지는 보지 않았다.

---

## 9. 잔여 위험 (Residual-risk)

- **이 카드는 결함을 고치지 않았다.** `LINEAR_EXACT` 를 선택한 호출자는 여전히 `LINEAR`
  을 받는다. 바뀐 것은 그 사실이 **테스트로 고정되고, 고쳐지면 신호가 난다**는 것뿐이다.
- **`DISABLED_` 는 잊히기 쉽다.** 이름에 이유가 적혀 있고 주석이 #156 을 가리키지만,
  **꺼진 테스트는 조용하다** — 이 세션에서 여러 번 본 "있는데 일하지 않는" 계열에 이
  장치 자체가 해당할 수 있다.
- **요구 문장과 코드가 여전히 어긋난다.** REQ-DISP-010 은 `without the half-value offset`
  이라고 쓰고 코드에는 `+ 0.5f` 가 있다. 어느 쪽을 고칠지는 원문이 정한다.
- **8건의 번호 정정은 문서를 바꾸지 않았다** — 코드 주석만이다. SPEC 쪽은 리더 소유다.
- 커밋은 push 전까지 미푸시 유일본이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` / `_verify.log` | 세 프리셋, 533 / 224 / 177, 경고 0 |
| `_b66.bat` / `_red.log` | `BUILD=0`, 신규 케이스 **의도된 FAILED**, maxdiff 5.96e-08 |
| `_falsify.log` | `+0.5f` → `+0.0f`: maxdiff **0.5**, 신규 초록 / 기존 빨강 |
