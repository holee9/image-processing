# QA-A-187 (#218) — 오늘 상태 확인 (측정, 제품 코드 변경 없음)

시작 HEAD `a420cd26` (`evidence/00_head.txt`). 추가한 것은 `DISABLED_` 프로브 1건(`test_zz_a187_probe.cpp`)과 CMake 등록 1줄이다. `modules/preprocess/src`·`include` diff 는 0줄이다.

이슈 #218 의 제목("FUNC-017 FlatResidualPct 0건, FUNC-019 recall/FPR 0건")은 2026-09-28 시점이다. 오늘 상태는 아래와 같다.

| 항목 | 오늘 | 해소 커밋 |
|---|---|---|
| FUNC-017 FlatResidualPct | 게이트가 있다 (해소) | `fa3da63b` (QA-A-154) + 분모 평균화 `edc53cbc` (QA-A-156) |
| FUNC-019 recall/FPR | `xpe_verify_*` 에 없다 (이슈 코멘트가 이미 "시험 쪽 요구"로 판정) | — |
| pipeline SNR 의 중앙값 | 정본에 대응하는 지표가 없다 (맞다/틀리다를 가릴 대상이 없음) | — |

---

## ① FUNC-017 — 오늘 코드에서 확인

### 증거

```
xpe_verify_metrics.cpp:541  double mean_after = compute_mean(after_vals);
:543  metrics->prnu_after = (mean_after > 0.0) ? (std_after / mean_after) * 100.0 : 0.0;
:587  bool flat_residual_ok = (metrics->prnu_after <= FLAT_RESIDUAL_MAX_PCT);     // 1.0, FUNC-017
:589  metrics->overall_pass = prnu_improved && coverage_ok && snr_improved && flat_residual_ok;
```

- `fa3da63b`, `edc53cbc` 모두 현재 HEAD 의 조상이다 (`evidence/04_ancestry.txt`).
- 게이트를 단언하는 시험 4건이 통과한다 (`VerifyMetricsTest.VerifyGain_PerfectFlatField`, `_DetectsBadGain`, `_FlatResidualUsesTheArithmeticMean`, `_ImprovedButStillAboveOnePercentFails`): 마지막 것이 "개선(6 dB)은 됐지만 잔차 5%" 인 패널이 이 게이트 때문에만 떨어지는 경우다.
- 이름 census: modules 에서 `FlatResidualPct` 를 포함하는 파일 4개(헤더·구현·시험 2). 첫 시도에서 `grep -w` 로 세어 0 이 나왔으나 `-w` 는 `FlatResidualPct` 를 `FlatResidual` 로 못 잡는 도구 오류였고, 부분 문자열로 다시 세어 정정했다 (`evidence/01_token_census.txt`; 대조군 `prnu_after`·`gain_coverage`·`defect_density`·`snr_improvement_db`·`dark_bias` 가 같은 검색에서 4·3·3·3·4 개 파일로 잡힘).

### 아직 열려 있는 부분 (이 이슈가 아니라 #220)

- release-hardening `<= 0.5%` 는 코드에 없다 (조건부 "where gain semantics are known", #220 §3).
- `Y_flat_roi` 의 ROI 규칙은 정의가 없다 (#220 §2, QA-A-186).

### 결정에 필요한 것

없음 — #218 의 FUNC-017 부분은 해소됐다. 이슈 제목 갱신은 리더 판단.

---

## ② FUNC-019 — `xpe_verify_*` 가 오늘 판정하는가: 아니오

### 요구 원문과 오늘의 지표

요구 원문 (SRS-CALIB-001:150): "System shall compute defect-correction metrics: `DefectRecall`, `DefectFPR`, `DefectResidualADU`, and `GoodPixelDeltaP99`. Synthetic BPM oracle cases shall require 100% defect recall and false-positive rate below 0.001%." 정의식과 합격 기본값은 `Preprocessing-E2E-Automated-Evaluation-Protocol.md` §5.5.

| 요구가 이름 지은 지표 | 정본 정의·합격 (Protocol §5.5) | `xpe_verify_defect` 오늘 | 비고 |
|---|---|---|---|
| `DefectRecall` | `TP / max(TP+FN, 1)`, 합성 BPM 오라클에서 = 100% | 없음 | 정답 BPM(어느 화소가 진짜 결함인지)이 필요한데 함수 인자는 (보정 이미지, 사용한 BPM) 둘뿐 |
| `DefectFPR` | `FP / max(FP+TN, 1)`, 합성에서 < 0.001% | 없음 | 위와 같은 이유 |
| `DefectResidualADU` | `mean(abs(Y(결함 화소) − 이웃 모델))` | `correction_error` 가 닮은 값: 결함 화소에서 보정값과 이웃 평균의 절대차 평균 (이웃 평균이 0 이하이면 표본에서 제외) | 같은 정의인지는 확인 안 함; 합격 문턱이 없음(요구도 정본도 문턱을 안 정함) |
| `GoodPixelDeltaP99` | `percentile99(abs(Y(정상 화소) − Y(결함 단계 없는 경우)))`, 기본 ≤ 1 ADU | 없음 | 결함 단계를 거치지 않은 이미지가 인자에 없음 |
| (요구에 없음) | — | `defect_density < 5.0%` 로 `overall_pass` 결정 (`DEFECT_DENSITY_MAX`, SRS FUNC-003 의 "5% 결함 밀도 허용") | QA-A-153 (#217) 이 단위를 고침 |

`xpe_verify_defect` 의 `overall_pass` 는 오늘도 결함 밀도 하나로 결정된다 (`xpe_verify_metrics.cpp:675`). recall/FPR 은 그 함수에 없다.

### recall/FPR 이 오늘 어디서 재어지는가

- **제품 모듈 안**: 없다. 단 런타임 검출(FUNC-010)의 FPR < 0.001% 는 `runtime_detection.h` 주석과 그 시험(QA-A-43)에서 다룬다 — 다른 요구(FUNC-010)이고 FUNC-019 의 보정 검증 지표가 아니다. `test_defect_correct.cpp:305` 의 `CorrectionRecallOnSyntheticDefects` 는 REQ-P1A-012 의 "보정된 결함 수 / 결함 수 ≥ 99%" 이며, 정답 BPM 대비 검출 recall 이 아니다.
- **GUI 클라이언트**: `clients/ImageProcTest/Services/MetricsComputationService.cs` 가 네 지표를 모두 계산·표시한다 (예측 BPM 이 선택된 경우). 그 합격선이 정본과 다르다:

| 지표 | 정본 합격 (Protocol §5.5) | GUI 클라이언트 합격선 (`MetricsComputationService.cs:163-169`) |
|---|---|---|
| DefectRecall | = 100% (합성 오라클) | `>= 95%` |
| DefectFPR | < 0.001% | `<= 0.001%` (경계 포함 여부만 다름) |
| DefectResidualADU | 문턱 없음 | `<= 2 ADU` |
| GoodPixelDeltaP99 | `<= 1 ADU` | `<= 0 ADU` |

이 표는 보고용이다. clients 레인 소유 코드라 확인만 했고 고치지 않았다.

### 이전 판정과의 관계

이슈 #218 의 두 번째 코멘트(2026-09-28)는 이미 "FUNC-019 는 요구 본문이 '합성 오라클 케이스'로 범위를 정했으므로 라이브러리 런타임 게이트가 아니라 시험 하네스 요구"라고 판정했다. 오늘 상태는 그 판정과 일치한다 (게이트로 구현되지 않았고, GUI 가 오라클이 있을 때 계산한다). 이후 그 "시험 쪽" 구현을 맡을 카드가 세워졌는지는 확인하지 못했다.

### 결정에 필요한 것

"FUNC-019 의 합성 BPM 오라클 시험(정답 BPM 으로 recall 100%, FPR < 0.001% 를 단언)을 누가·언제 세울지" 와 "GUI 의 합격선(95%, 0 ADU)을 정본(100%, 1 ADU)에 맞출지(clients 레인 카드)".

---

## ③ `xpe_verify_pipeline` 의 SNR 중앙값 사용이 정본과 맞는가

### 먼저 — 대응하는 정본 지표가 없다

오늘 코드 (`xpe_verify_metrics.cpp:753-767`): raw 와 final 각각에서 중앙값을 중심으로 한 RMS 편차로 `20·log10(중앙값/표준편차)` 를 내고, 둘의 차를 `snr_improvement_db` 로 두어 `>= 2.0 dB`(`SNR_IMPROVE_MIN_DB`, 소스 주석 "NO REQUIREMENT FOUND")이면 합격으로 판정한다.

정본에는 그에 해당하는 식이 없다:

- `Preprocessing-E2E-Automated-Evaluation-Protocol.md` 의 "SNR" 은 `PSNR(A,B)` 한 줄(`:194`)과 보고서 스키마의 `psnr_reference_db`(`:376`)뿐이고, 둘 다 기준 이미지와 비교하는 값이다.
- `SRS-CALIB-FUNC-015`(보고서 스키마), `FUNC-021`(CES 점수)에도 "SNR 개선 dB" 지표와 합격선이 없다 (`docs/calibration`·`docs/project`·`.moai/specs` 의 SNR 줄 전부를 봄, `evidence/05_snr_docs_search.txt`; 대조군 `PRNU` 6개 파일 / `SNR` 30개 파일).
- 다른 SNR 은 있으나 모두 다른 양이다: 화소별 SNR(런타임 결함 검출, FUNC-010: `|x−μ|/σ`), 획득 합격 기준(IAP-CALIB:820 `σ/μ < 5%`), PRD §7.3.3 의 절대 SNR 목표(0.2/1.0/5.0 mR 에서 > 45/90/170, 23 mm ROI·10,000 화소), 임상 PSNR > 40 dB (TDS).
- `RTM-CALIB-001:243` 은 `xpe_verify_pipeline` 을 FUNC-015/021 에 매핑하지만 그 둘은 위와 같이 SNR 개선을 요구하지 않는다.

그래서 "중앙값 사용이 정본과 맞는가"의 답은 "맞다/틀리다를 가릴 정본 식이 없다"이다. 이 함수의 합격선(2.0 dB)과 지표 자체가 요구에 근거하지 않는다.

### 그럼에도 중앙값이냐 평균이냐가 판정을 가르는가 (재측정)

QA-A-186 이 FlatResidualPct 에서 쓴 방식으로 재었다. `today`(= `xpe_verify_pipeline`), `median`(프로브가 같은 식으로 재계산, 전 행 `= today` — 대조군 25/25), `mean`(같은 식에서 중심만 산술평균으로 바꾼 값). `evidence/03_probe_pipeline_snr.txt`.

| 입력 | today | mean | 판정 |
|---|---|---|---|
| 대칭 잡음 1.20× / 1.26× / 1.30× / 2.00× 감소 | 1.6084 / 1.9806 / 2.2212 / 5.8998 | 1.6077 / 1.9798 / 2.2212 / 5.8998 | 같음 |
| raw 에 밝은 치우침 2·5·10·20% (+1500), final 깨끗 | 11.66 / 15.28 / 18.02 / 21.00 | 11.59 / 15.08 / 17.61 / 20.11 | 같음 (문턱보다 훨씬 위) |
| raw 에 불량 화소 8·40·160 개, final 보정 | 13.09 / 19.92 / 25.93 | 13.10 / 19.93 / 25.97 | 같음 |
| final 이 치우침(10% 화소 +300) | 3.4234 PASS | 3.4922 PASS | 같음 |
| final 이 치우침, +360 ~ +390 | 2.61 ~ 2.20 PASS | 2.72 ~ 2.33 PASS | 같음 |
| **final 이 치우침, +400** | **1.9248 FAIL** | **2.0518 PASS** | **갈림** |
| **final 이 치우침, +410** | **1.9277 FAIL** | **2.0628 PASS** | **갈림** |
| final 이 치우침, +420 ~ +460 | 1.70 ~ 1.09 FAIL | 1.85 ~ 1.25 FAIL | 같음 |

판정이 갈리는 입력이 있다 (final 이 치우친 입력에서 2.0 dB 문턱 바로 아래 좁은 띠). 두 값의 차는 대체로 0.0~0.2 dB 이고 "raw 가 치우친" 입력에서는 최대 0.9 dB(20% 치우침)까지 벌어지지만 그 입력들은 문턱보다 훨씬 위라 판정은 같다.

여기서 `mean` 은 정본이 아니라 "중앙값이 아닌 쪽"의 비교 대상이다. 정본에 이 지표가 없으므로 어느 쪽이 옳은지는 이 측정이 정하지 못한다. 확인된 사실은 두 가지다: 이 함수는 QA-A-155 가 비판한 "중앙값 중심 + RMS 산포" 조합(알려진 추정기 쌍이 아님)을 쓴다, 그리고 그 조합과 평균 조합은 문턱 근처에서 판정이 갈릴 수 있다.

### 기준선 귀속

명령: `xpe_preprocess_tests.exe --gtest_also_run_disabled_tests --gtest_filter='A187Probe.*'`, 이 트리(HEAD `a420cd26` + 프로브), 합성 128×128, 고정 시드.

### 미검증

- 실픽스처로는 재지 않았다 (이 지표가 어떤 실프레임에서 문턱 근처인지 모름). 이 카드의 프로브는 합성만.
- 갈림 구간은 "final 이 치우침" 한 족에서만 찾았다.
- 이 지표를 게이트에서 빼거나 정본에 정의를 세우면 갈림은 의미가 없어진다.

### 결정에 필요한 것

"`xpe_verify_pipeline` 의 SNR 개선 지표와 2.0 dB 합격선을 (a) 정본에 정의를 세워 유지할지(그때 중심을 평균으로 할지 정함), (b) 판정에서 빼고 보고만 할지, (c) 지금 그대로 두고 '근거 없음'을 주석으로 남길지."

---

## 검증

전체 시험 1회와 `ctest -N`, 프리셋 점검, 린트는 아래 증거 파일에 있다 (`06_*`~`08_*`).

## Gaps / Residual-risk

- 위 각 항목의 "미검증"을 따른다.
- `XpeCalibrationMetrics` 의 `overall_pass` 가 어느 SPEC/요구에서 왔는지는 파이프라인 쪽은 추적되지 않는다 (RTM:243 은 FUNC-015/021 로 매핑하나 그 요구에 SNR 판정이 없음).
