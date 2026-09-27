# QA-A-134 (#140) — (c) 전제가 틀렸습니다. 멈추고 보고합니다

Lane A (pre), `dev/preprocess`, `origin/main 2ba12f5` 병합. **코드 변경 없음 — 배선하지 않았습니다.**
빌드는 안 돌렸고(코드 미변경), **관련 시험만 실행**했습니다.

## 결론

**`#140` 의 네 주장 중 셋이 더는 참이 아닙니다. 요구하신 배선은 이미 돼 있습니다.**

그리고 **카드가 경계하라고 한 "상수 비교" 함정도 이미 막혀 있습니다** — QA-A-70 이 R² 가 자료에 반응하는지를 단조성으로 고정해 뒀습니다.

## (a) 네 주장 확인

| `#140` 의 주장 | 지금 | 근거 |
|---|---|---|
| `xpe_calib_get_quality_meta` 가 **초기값만 돌려준다** | **아니오** | 아래 |
| 메타를 **채우는 코드가 없다** | **아니오 — 있습니다** | `xpe_calib_mode.cpp:252` `xpe_calib_record_quality_meta()`; **생성 경로 양쪽이 호출**(`xpe_calib_generate_gain.cpp:316` 단일점, `:616` 다항식) |
| `R2QualityGate_*`·`PreviousCalibration_*` 4건이 **삭제됐다** | **아니오 — 되살아났습니다** | `test_calib_quality_meta.cpp:191` `R2QualityGate_Pass_WhenAboveThreshold`, `:252` `R2QualityGate_Fail_WhenBelowThreshold`, `:279` *"These two cases were named PreviousCalibration_Comparison_* until QA-A-87"* |
| **FUNC-033 이 살아 있는 요구다** | **예 — 참입니다** | 아래 |

### FUNC-033 은 재번호 뒤에도 같은 요구입니다

`SRS-CALIB-001:240` 원문을 읽었습니다(`#140` 본문이 아니라):

> **SRS-CALIB-FUNC-033** | **Calibration Quality Metadata Recording.** … (1) **Mandatory Metadata Fields**: Every generated XCal gain file shall include: `calibration_mode`, `actual_dose_levels`, `polynomial_degree`, `fit_r_squared`, `max_residual_pct`, `mean_residual_pct`, `acquisition_duration_s`, `detector_temperature_c`. (2) **Quality Gate**: If `fit_r_squared < 0.999` … `XPE_WARN_CALIB_POOR_FIT` … Gate constant `XPE_CALIB_R_SQUARED_GATE = 0.999`.

**제목도 내용도 `#140` 이 말하는 그 요구입니다.** `#188` 의 옛 057(CRC-32 → SHA-256 흡수)이나 `#195` 의 RTM 블록처럼 다른 요구로 바뀌지 않았습니다.

**대조군**: `SRS-CALIB-*` 는 `REQ-P1A-*` 와 **다른 번호 체계**입니다. `#197` 의 재번호는 `SPEC-XPE-P1A/spec.md` 의 `REQ-P1A-*` 를 바꾼 것이고, `SRS-CALIB-FUNC-*` 는 `docs/calibration/SRS-CALIB-001…md` 의 별개 체계입니다. **재번호의 영향권이 아닙니다.**

### 주장 1·2 — 메타는 채워지고, 실제로 돌아갑니다

`xpe_calib_generate_gain.cpp` 양쪽 경로가 적합 결과를 `quality` 구조체에 담아 `xpe_calib_record_quality_meta()` 를 호출하고, 그 반환값으로 게이트 통과 여부를 받습니다:

```
:316   const bool gate_passed = xpe_calib_record_quality_meta(quality);   // 단일점
:616   const bool gate_passed = xpe_calib_record_quality_meta(quality);   // 다항식
```

기록 함수(`xpe_calib_mode.cpp:252-268`)가 이전 값 보존·타임스탬프·게이트 판정을 합니다:

```
:259   g_quality_meta.previous_r_squared = ...
:263   g_quality_meta.calibration_timestamp = ...
:267   const bool passed = (g_quality_meta.r_squared >= XPE_CALIB_R_SQUARED_GATE);
:268   g_quality_meta.calibration_pass = passed ? 1u : 0u;
```

그리고 **파일에도 실립니다** — `config_json` 에 `fit_r_squared`·`max_residual_pct`·`mean_residual_pct`·`calibration_pass`·`polynomial_degree`·`actual_dose_levels`·`calibration_mode` 가 들어갑니다(`:676-686`). 적재 시 `xpe_calib_apply_quality_meta_json()`(`xpe_calib_mode.cpp:272`)이 되돌립니다(`xpe_calib_load_gain.cpp:127` 근처).

**즉 카드 (b) 가 요구한 "R² 를 계산해 메타를 채우고 XCal 게인 파일에 기록" 이 전부 이미 있습니다.**

## 카드 2번 — "상수 비교 함정" 도 이미 막혀 있습니다

카드가 *"메타가 채워지는지가 아니라 적합이 나쁜 자료와 좋은 자료에서 R² 가 서로 다르게 나오는지를 단언하라"* 고 했습니다.

**`test_calib_quality_meta.cpp` 가 그 지적을 이미 적어 두고 그 시험을 갖고 있습니다**(`:204-212`):

> **QA-A-70 (#140): R2 MOVES WITH FIT QUALITY, monotonically.**
> *"The two gate tests below assert one side each, with different inputs. Neither on its own says the number RESPONDS to the data — **a function returning 1.0 for good input and 0.5 for bad input would satisfy both while being a lookup table.** This test holds everything fixed … and varies exactly one thing: how far the series departs from a straight line. R2 must fall, step by step."*

**카드가 우려한 함정을 그 시험이 이름까지 대어 막고 있습니다.** 그리고 선형 적합을 쓴 이유까지 적혀 있습니다 — 2차 적합이면 곡선형 편차가 적합에 흡수돼 잔차로 안 나타나기 때문입니다.

**양방향 단언도 있습니다**: `R2QualityGate_Pass_WhenAboveThreshold`(`:191`)와 `R2QualityGate_Fail_WhenBelowThreshold`(`:252`).

### 실행 결과

```
[----------] 15 tests from CalibQualityMetaTest (146 ms total)
[  PASSED  ] 15 tests.
```

명령: `xpe_preprocess_tests.exe --gtest_filter='CalibQualityMetaTest.*'` (현 빌드, 코드 변경 전).

## SRS 가 `#140` 으로 추적하던 한 가지도 해소돼 있습니다

`SRS-CALIB-001:240` 의 레이아웃 주석(2026-09-17 / QA-A-86)이 이렇게 적습니다:

> *"on the polynomial path the file's `polynomial_degree` records the requested cap `max_degree` while the in-memory quality struct records the actually fitted `highest_degree` — the SRS says "fitted", so the file value is the one that disagrees; **tracked in #140**."*

**지금은 둘 다 `highest_degree` 입니다:**

| 자리 | 값 |
|---|---|
| 파일 `config_json` 의 `polynomial_degree` (`:683`) | `static_cast<int>(**highest_degree**)` |
| 파일의 `max_polynomial_degree` (`:684`) | `static_cast<int>(max_degree)` — **별도 키** |
| 메모리 `quality.polynomial_degree` (`:612` 부근) | `static_cast<uint8_t>(**highest_degree**)` |

**요청 상한이 `max_polynomial_degree` 라는 자기 키를 갖게 되면서 불일치가 사라졌습니다.** SRS 의 그 주석이 낡았습니다 — `docs/` 는 리더 소유라 **보고만 합니다.**

## 그러면 `#140` 에 무엇이 남았나 — **FUNC-033 의 (1) 일부·(3)·(4)**

카드가 옮긴 네 주장은 해소됐지만, **FUNC-033 원문의 다른 항목들은 미구현이고 코드가 그 사실을 적어 두고 있습니다.**

| FUNC-033 항목 | 상태 | 근거 |
|---|---|---|
| (1) 여섯 필드(`calibration_mode`·`actual_dose_levels`·`polynomial_degree`·`fit_r_squared`·`max_residual_pct`·`mean_residual_pct`) | **구현** | `config_json`(`:676-682`) + `quality` 구조체 |
| (1) `acquisition_duration_s`·`detector_temperature_c` | **미구현** | `xpe_calib_generate_gain.cpp:308-310` — *"are also named by FUNC-033 (1) and are **NOT written here** -- neither value reaches this function through its current signature. **Recorded as residual on #140.**"* |
| (2) R² 게이트 | **구현** | `xpe_calib_mode.cpp:267-268`, 양방향 시험 |
| (3) 비교 항목 3종(`dark_bias_delta`·`prnu_delta_pct`·`defect_count_delta`) | **미구현** | `test_calib_quality_meta.cpp:281-283` — *"**none of those is implemented.** Renamed so the name no longer reads as coverage of (3)"* |
| (4) 모드별(`gain_uncertainty`, `per_point_r_squared[]`) | **미구현** | 검색 0건 — `modules/preprocess/` 전체의 `.cpp`·`.h`. **대조군**: 같은 검색으로 (1)·(3) 의 키워드는 위 두 줄에서 잡힙니다 |

**(1) 의 두 필드가 미구현인 이유도 코드가 적습니다** — *"neither value reaches this function through its current signature"*. 즉 **API 서명을 바꿔야 하는 일**이고, 메타 기록 배선과는 크기가 다릅니다.

**그래서 `#140` 은 "배선이 없다" 가 아니라 "FUNC-033 의 일부 항목이 남았다" 입니다.** 남은 셋은 서로 성격이 다릅니다 — (1) 두 필드는 서명 변경, (3) 은 이전 교정과의 비교 계산, (4) 는 모드별 추가 계산.

**어느 것을 할지는 리더 판단입니다.** 카드의 (b) 가 지시한 것("R² 계산해 메타 채우고 파일에 기록")은 이미 돼 있으므로, 그대로 착수하면 헛일입니다.

## (c) — 멈췄습니다

카드 지시대로 **짓지 않았습니다.** `#140` 의 전제 넷 중 셋이 더는 참이 아니고, 요구된 배선과 단언이 모두 존재하며 통과합니다.

**남은 것이 있다면 제가 못 본 것일 수 있어, 아래를 미검증으로 답니다.**

## 미검증

- ~~(1) 두 필드·(3)·(4) 미확인~~ → **확인했고 위 절에 적었습니다.** 셋 다 미구현이며 코드가 그 사실을 기록해 두고 있습니다.
- **`#140` 본문에서 남은 항목을 찾지 못했습니다.** `acquisition`·`detector_temp`·`dark_bias`·`per_point`·`gain_uncertainty` 로 본문을 검색해 **0건**입니다 — 본문이 그 항목들을 언급하지 않습니다. 즉 위 "남은 일" 표는 **이슈가 아니라 SRS 원문과 코드 주석에서** 얻은 것입니다.
- **전체 ctest 를 돌리지 않았습니다**(코드 변경이 없어서). `CalibQualityMetaTest` 15건만 실행했습니다.
- ~~`XPE_WARN_CALIB_POOR_FIT` 알림 미확인~~ → **확인했습니다. 구현·시험 둘 다 있습니다** — `xpe_calib_generate_gain.cpp:626` 이 알림을 밀고, `test_calib_quality_meta.cpp:263` 이 `alertQueueMentions("XPE_WARN_CALIB_POOR_FIT")` 로 단언합니다. FUNC-033 (2) 는 완전히 구현·검증돼 있습니다.

## 잔여 위험

- **SRS `:240` 의 QA-A-86 주석이 낡았습니다**(`polynomial_degree` 불일치가 해소됨). 그대로 두면 다음 사람이 없는 결함을 쫓습니다 — 오늘 반복해서 만난 형태입니다. `docs/` 소유라 보고만 합니다.
- **`#140` 이 열린 뒤 여러 카드(QA-A-35·70·86·87)가 이 영역을 건드렸습니다.** 이슈 본문이 그 이력을 반영하지 않으면, 같은 전제 오류가 또 납니다.
