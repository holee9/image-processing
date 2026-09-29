# QA-A-153 (#217) — 단위 오류를 고쳤습니다. **아무도 이 게이트를 검증하지 않고 있었습니다**

`DEFECT_DENSITY_MAX` 를 `0.05` → `5.0` 으로. 카드 §3 의 두 갈래 중 **둘째**가 답입니다.

---

## 1. 고친 것 — 방향 (a)

```cpp
constexpr double DEFECT_DENSITY_MAX = 5.0;   // %. SRS-CALIB-FUNC-003:
                                             // "Maximum 5% defect density tolerance"
```

지표(`defect_density`)는 **퍼센트 그대로** 두었습니다 — 소비자가 시험·보고·GUI 이고,
틀린 것은 **비교 한 줄**이었습니다. 상수 옆에 요구 ID 와 경위를 적었습니다.

리더 정정에 동의합니다: **합격선을 옮기는 결정이 아닙니다.** 요구(`FUNC-003`)도 주석(`// 5% max`)도
5% 를 말했고, 값 하나만 어긋나 있었습니다. 고친 상태가 준수이고 고치기 전이 위반입니다.

---

## 2. 반증 — 1% 케이스가 갈렸습니다

새 시험 `VerifyDefect_GateMatchesTheFivePercentRequirement` 를 **고치기 전에 먼저** 돌렸습니다.

**수정 전** (`DEFECT_DENSITY_MAX = 0.05`):

```
test_verify_metrics.cpp(378): error: Expected equality of these values:
    Which is: true
    Which is: false
at 1% density: 1% is inside the 5% the requirement allows (reported density 1)
[  FAILED  ] VerifyMetricsTest.VerifyDefect_GateMatchesTheFivePercentRequirement
```

**수정 후**:

```
[       OK ] VerifyMetricsTest.VerifyDefect_GateMatchesTheFivePercentRequirement (0 ms)
```

| 결함 밀도 | 수정 전 | 수정 후 | 카드 기대 |
|---|---|---|---|
| 0.01 % | pass | pass | pass ✔ |
| **1 %** | **fail** | **pass** | pass ✔ |
| 10 % | fail | fail | fail ✔ |

**세 행 모두 카드가 예측한 대로**입니다. 결함은 실재했습니다.

> 이 시험의 첫 판은 제 잘못으로 다른 이유로 빨갰습니다 — 공유 픽스처가 32×32 라 0.01% 가
> **1픽셀 미만(0개)** 이 됐습니다(1픽셀 = 0.098%). 이 시험만 200×200 을 쓰게 고쳐 0.01% 를
> 정확히 4픽셀로 만들었습니다. 사유를 시험 안에 적었습니다.

---

## 3. 기존 시험 의존 — **둘째 갈래: 아무도 검증하지 않고 있었습니다**

`ctest` 전체에서 **깨진 시험이 0건**입니다. 이유를 찾았습니다.

`test_verify_metrics.cpp` 에서 `overall_pass` 를 단언하는 곳은 **넷**인데, **defect 경로는 없습니다**:

| 줄 | 시험 |
|---|---|
| `:161` | `VerifyOffset_PerfectCorrection` |
| `:250` | `VerifyGain_PerfectFlatField` |
| `:291` | `VerifyGain_DetectsBadGain` |
| `:368` | `VerifyPipeline_SNRImprovement` |

유일한 defect 시험 `VerifyDefect_CountsDefects`(`:297`)는 `defect_count` 와 `defect_density` 만
보고 **`overall_pass` 를 보지 않습니다**.

**그래서 이 게이트는 검증 없이 돌고 있었고, 그것이 단위 오류가 살아남은 경로입니다.**
`#212` 에서 본 형태와 같습니다 — 단언이 없는 것이 아니라, **있는 단언이 그 자리를 안 보는 것**.

새 시험이 그 공백을 메웁니다. 요구가 허용하는 경계(5%)의 안팎을 고정하므로, 상수를 되돌리면
다시 빨개집니다.

---

## 검증

| 항목 | 결과 |
|---|---|
| 빌드 | `BUILD_EXIT=0` |
| 전체 ctest | **758 / 758** (`CTEST_EXIT=0`, 직전 757 + 신규 1) |
| 한 프로세스 네 순서 (기본 + seed 1/2/9) | 각각 `ran=689`, `PASSED 681`, 실패 0 |
| 깨진 기존 시험 | **0건** (§3 — 검증이 없었기 때문) |

## 미검증 / 잔여 위험

- **이 고침이 실제 사용에서 무엇을 바꾸는지는 재지 않았습니다.** 0.05 % ~ 5 % 구간의 패널이
  이제 통과합니다. 그 구간이 현장에서 어느 정도 빈도인지는 이 저장소에서 알 수 없습니다.
- 새 시험은 **밀도만** 봅니다. `xpe_verify_defect` 가 함께 계산하는 `correction_error` 는
  `overall_pass` 에 들어가지 않으므로(`:436` 이 밀도 하나만 봄) 시험도 보지 않습니다 —
  그것이 옳은지는 `#218` 의 질문입니다(요구가 정한 기준은 recall·FPR 입니다).
- 나머지 다섯 문턱(`DSNU_MAX_PCT`·`PRNU_IMPROVE_MIN_DB`·`GAIN_COVERAGE_MIN`·
  `SNR_IMPROVE_MIN_DB`·`DARK_BIAS_MAX`)은 이 카드에서 건드리지 않았습니다.
- `:199` 무조건 합격도 카드 §4 대로 범위 밖입니다.

🗿 MoAI
