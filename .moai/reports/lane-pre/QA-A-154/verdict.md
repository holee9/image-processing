# QA-A-154 (#218) — **같은 양입니다.** 필요한 것은 계산이 아니라 게이트였습니다

카드 §1 의 답: `FlatResidualPct` 는 코드가 **이미 계산 중인 `prnu_after` 와 같은 양**입니다.
`alias` 라는 GUI 문서의 말이 맞았지만, **그 문서가 근거는 아닙니다** — 정의식은 다른 곳에 있었습니다.

---

## §1 답 — 무엇을 읽고 그렇게 판단했는가

### 정의식 (원천)

`docs/project/Preprocessing-E2E-Automated-Evaluation-Protocol.md:218-219`:

```text
PRNU_CV         = std(Y_flat_roi) / max(mean(Y_flat_roi), epsilon)
FlatResidualPct = 100 * PRNU_CV
```

즉 **`PRNU_CV` 는 분수, `FlatResidualPct` 는 그것의 퍼센트 표기**입니다. 같은 양의 두 단위입니다.

### 코드

`xpe_verify_metrics.cpp:370`:

```cpp
metrics->prnu_after = (mean_after > 0.0) ? (std_after / mean_after) * 100.0 : 0.0;
```

`std/mean × 100` — **`FlatResidualPct` 그 자체**입니다. 이름은 `prnu_after` 인데 담긴 값은
`PRNU_CV` 가 아니라 **`100 × PRNU_CV`** 입니다.

→ **새 계산이 필요 없습니다. 지표는 이미 있었고 게이트만 없었습니다.**

### 문서 넷을 나란히 — 셋이 어긋납니다

| 출처 | 말하는 것 | 판정 |
|---|---|---|
| `Preprocessing-E2E-…:218-219` | `FlatResidualPct = 100 × PRNU_CV` | **정의식. 이것이 근거입니다** |
| `SRS-CALIB-001:148` `FUNC-017` | *"compute … `PRNU_CV`, `FlatResidualPct`, …"* 로 **넷을 나열**하고 `FlatResidualPct <= 1.0%` 를 요구 | 별개 항목처럼 읽히나, 같은 양의 두 단위를 둘 다 보고하라는 것과 모순 아님 |
| `XPE-GUI-CALIB-001:167` | `FlatResidualPct` = *"same as PRNU_CV (alias)"*, 그리고 같은 표에서 `PRNU_CV = stdev/mean × 100` | **100배를 흘렸습니다.** 이 표 안에서는 둘이 완전히 같은 수가 되어, 정의식과 어긋납니다 |
| `TDS-CALIB-001:1481-82` | 같은 레코드에 `"FlatResidualPct": 0.8` 과 `"PRNU_CV": 0.035` | **정의식과 맞지 않습니다.** `100 × 0.035 = 3.5 ≠ 0.8` |

**카드가 경고한 함정을 실제로 만났습니다.** `#217` 의 `DSNU_MAX_PCT` 는 *"같은 숫자, 다른 분모"*
였고, 여기는 *"같은 양, 다른 배율(100×)"* 입니다. GUI 문서만 읽고 alias 를 그대로 받았다면
`PRNU_CV` 를 게이트에 쓸 뻔했는데, 정의식 기준으로 그것은 **분수**라 `<= 1.0` 이 `100%` 를
허용하는 게이트가 됐을 것입니다.

### 그리고 이름이 닮은 함정이 하나 더

`flatness_pct`(`:373`)는 `FlatResidualPct` 가 **아닙니다**. `compute_flatness` 는
**히스토그램 엔트로피**(`:103-121`)이고 *"모든 값이 같으면 1.0 → 100%"*, 즉 **높을수록 좋은**
값입니다(시험이 `> 90.0` 을 기대 — `:244`). `FlatResidualPct` 는 **낮을수록 좋은** 잔차입니다.
이름만으로 골랐다면 반대 방향의 게이트를 달았을 자리입니다.

---

## §2 구현 — 게이트 한 줄 + 문서화

```cpp
constexpr double FLAT_RESIDUAL_MAX_PCT = 1.0;   // SRS-CALIB-FUNC-017
...
bool flat_residual_ok = (metrics->prnu_after <= FLAT_RESIDUAL_MAX_PCT);
metrics->overall_pass = prnu_improved && coverage_ok && snr_improved && flat_residual_ok;
```

**상대 축을 걷어내지 않았습니다.** 개선 검사(`prnu_improved`·`snr_improved`)를 빼면 *이미 평탄한데
보정이 안 된* 패널이 통과하고, 이 검사를 빼면 *나쁘게 보정된* 패널이 통과합니다 — **서로 다른 것을
막습니다.** 그래서 AND 로 더했습니다.

`prnu_after` **필드 이름은 바꾸지 않았습니다** — 공개 구조체 멤버라 ABI 변경입니다. 대신 헤더의 그
자리에 *"이 필드가 `FlatResidualPct` 이고 프로토콜 정의로 `100 × PRNU_CV` 다"* 를 적었습니다.

---

## §3 반증 — **게이트 없이 통과하던 입력이 실재합니다**

카드가 요구한 것: *"`FlatResidualPct > 1.0%` 인데 `snr_improvement_db >= 3.0` 이라 현재 통과하는
입력"*. 만들었습니다 — `±10%` 비균일 원본을 `±5%` 로 보정한 경우입니다.

**게이트를 런타임-거짓으로 무력화한 대조군**:

```
test_verify_metrics.cpp(380): error: Value of: m.overall_pass
SRS-CALIB-FUNC-017 requires FlatResidualPct <= 1.0%;
residual was 5.0024431958457791% with 6.0205999132796242 dB improvement
[  FAILED  ] VerifyMetricsTest.VerifyGain_ImprovedButStillAboveOnePercentFails
```

**잔차 5.00%, 개선 6.02 dB 로 통과합니다.** 요구가 허용하는 것의 5배인데 상대 축은 전부 만족합니다.

→ **두 축은 같은 것을 재고 있지 않습니다.** 게이트는 중복이 아닙니다.
(`BUILD_EXIT=0`, DLL 타임스탬프 20:35:17 갱신 확인, 확인 후 제거 — `grep -c` 0건)

> 시험 첫 판은 `rc = -7`(`UNSUPPORTED_FORMAT`)로 빨갰습니다. `before` 버퍼를 FLOAT32 로 만들었는데
> 이 API 는 **UINT16** 을 받습니다(기존 시험 `:224` 가 그 형태). 제 시험의 오류였고 고쳤습니다 —
> "빨갛다" 만 보고 결함을 확인했다고 적을 뻔한 자리라 적어 둡니다.

### 카드 §2 의 둘째 요구 — `overall_pass` 단언에 걸리는가

`#217` 에서 *"단언하지 않는 경로는 게이트가 무엇을 하든 조용하다"* 를 봤으므로 확인했습니다.
게인 경로의 기존 단언 둘은 **살아 있고 이 게이트를 탑니다**:

| 시험 | 단언 | 새 게이트 영향 |
|---|---|---|
| `VerifyGain_PerfectFlatField:250` | `EXPECT_TRUE(overall_pass)` | 완전 평탄이라 잔차 ≈ 0 → 여전히 통과 ✔ |
| `VerifyGain_DetectsBadGain:291` | `EXPECT_FALSE(overall_pass)` | 이미 불합격 ✔ |
| **신규** `:~350` | `EXPECT_FALSE(overall_pass)` | **이 게이트가 유일한 불합격 사유** |

즉 새 게이트는 **관측되는 자리**에 있습니다 — `#217` 의 결함(게이트가 있는데 아무도 안 봄)을
반복하지 않습니다.

---

## 검증

| 항목 | 결과 |
|---|---|
| 빌드 | `BUILD_EXIT=0` |
| 전체 ctest | **759 / 759** (`CTEST_EXIT=0`, 직전 758 + 신규 1) |
| 한 프로세스 네 순서 (기본 + seed 1/2/9) | 각각 `ran=690`, `PASSED 682`, 실패 0 |
| 깨진 기존 시험 | 0건 |

## 하지 않은 것 (카드 §3)

- 출처 없는 상수 넷(`DSNU_MAX_PCT`·`PRNU_IMPROVE_MIN_DB`·`GAIN_COVERAGE_MIN`·`SNR_IMPROVE_MIN_DB`)
  **손대지 않았습니다**
- `FUNC-019`(recall/FPR) — 별건
- `xpe_verify_offset` 무조건 합격(`#219`) — 별건

## 미검증 / 잔여 위험

- **평균 추정기가 다릅니다.** 프로토콜은 `mean(Y_flat_roi)` 인데 코드는 `compute_robust_mean`
  (중앙값 기반)을 씁니다. 같은 양의 다른 추정기이고, 균일에 가까운 평탄 영상에서는 차이가 작지만
  **같다고 확인한 것은 아닙니다.** 요구가 어느 추정기를 뜻하는지는 문서에 없습니다.
- 프로토콜은 `Y_flat_roi`(평탄장 ROI)를 말하는데 코드는 **전체 유효 화소**를 씁니다. ROI 개념이
  구현에 없습니다.
- `release-hardening target <= 0.5%` 는 **넣지 않았습니다** — `FUNC-017` 이 Phase 1 을 `1.0%` 로
  정하고 `0.5%` 는 "target … where gain semantics are known" 이라는 조건부입니다. 그 조건을
  코드가 알 방법이 없습니다.
- `TDS-CALIB-001:1481-82` 의 레코드가 정의식과 맞지 않는 것은 **보고만** 했습니다 — 그 문서는
  제 소유가 아닙니다. `XPE-GUI-CALIB-001:167` 의 100배 누락도 같습니다.
- 이 게이트가 실제 픽스처에서 무엇을 떨어뜨리는지는 재지 않았습니다.

🗿 MoAI
