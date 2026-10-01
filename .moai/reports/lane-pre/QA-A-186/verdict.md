# QA-A-186 (#220) — FlatResidualPct 의 분모: 평균 대 중앙값 (측정, 코드 변경 없음)

시작 HEAD `a4753fdc` (`evidence/00_head.txt`). 이 카드는 제품 코드를 바꾸지 않았다. 추가한 것은 `DISABLED_` 프로브 1건(`test_zz_a186_probe.cpp`)과 CMake 등록 1줄뿐이다.

## 먼저 — 이 질문은 QA-A-155/156 에서 이미 답이 났고, 코드는 이미 평균으로 바뀌어 있다

이슈 #220 본문(§1)은 쓰인 시점의 상태다. 오늘의 코드:

- `xpe_verify_gain` 의 `prnu_before`/`prnu_after` 는 `compute_mean`(산술평균)으로 계산한다 (`xpe_verify_metrics.cpp:535`, `:541`). 주석에 "QA-A-156 (#220): was compute_robust_mean (median)" 이 남아 있다.
- 그 변경은 `edc53cbc`(QA-A-156)이고 현재 HEAD 의 조상이다 (`git merge-base --is-ancestor edc53cbc HEAD` 성공).
- `compute_robust_mean`(중앙값)은 아직 파일에 있고 호출처는 **한 곳**이다: `xpe_verify_pipeline` 의 `mean_raw`/`mean_final` (`:753`, `:756`). FlatResidualPct 와 무관한 `snr` 계산이다 (`grep -n compute_robust_mean` 결과 정의 1 + 호출 2).
- `dark_bias`·`dsnu` 쪽(`:411`, `:429`)도 같은 QA-A-156 에서 `compute_mean` 으로 바뀌어 있다.

그래서 ①의 답은 "**아니오, 오늘은 쓰지 않는다**"이다. 다만 아래 ②는 카드가 요청한 대로 오늘 코드에서 다시 쟀다.

---

## ① 오늘 코드가 FlatResidualPct 분모로 무엇을 쓰는가

### 증거

```
:534  // QA-A-156 (#220): was compute_robust_mean (median).
:535  double mean_before = compute_mean(before_vals);
:541  double mean_after = compute_mean(after_vals);
:542  double std_after = compute_std(after_vals, mean_after);
:543  metrics->prnu_after = (mean_after > 0.0) ? (std_after / mean_after) * 100.0 : 0.0;
:587  bool flat_residual_ok = (metrics->prnu_after <= FLAT_RESIDUAL_MAX_PCT);
```

호출처 목록: `evidence/` 의 빌드·프로브 출력과 위 줄 인용은 `grep -n "compute_robust_mean\|compute_mean(" modules/preprocess/src/xpe_verify_metrics.cpp` 로 얻었다.

### 결정에 필요한 것

없음 — #220 §1 은 이미 코드에서 해소됐다. 이슈 본문의 "순서 1번"은 닫아도 된다(리더 판단).

---

## ② 같은 입력에서 평균 분모 대 중앙값 분모

### 방법

한 `after` 프레임마다 네 수를 낸다 (`evidence/02_probe.txt`, 128×128 합성, 고정 시드; 실픽스처는 3072×3072):

- `today` — `xpe_verify_gain` 이 돌려준 `prnu_after`
- `arith` — 같은 식을 프로브 안에서 다시 계산 (평균 중심 표준편차 ÷ 산술평균). `today` 와 같아야 하며, 같으면 재계산이 제품 식이라는 대조군이다 — **전 행에서 "= today"**
- `median` — QA-A-156 이전 코드: 중앙값 중심 RMS ÷ 중앙값
- `denom` — 분모만 중앙값으로 바꾼 값 (평균 중심 표준편차 ÷ 중앙값)

문턱은 `<= 1.0` 통과. `SPLIT` 은 `today` 와 판정이 다르다는 표시.

### 판정이 갈리는 입력 — 있다 (존재 증명)

화소 10% 를 `+amp%` 올린 프레임:

| amp | today (평균) | median (옛 코드) | denom (분모만) |
|---|---|---|---|
| +3.1% | 0.9273 PASS | 0.9805 PASS | 0.9302 PASS |
| **+3.2%** | **0.9571 PASS** | **1.0121 FAIL (SPLIT)** | 0.9602 PASS |
| **+3.3%** | **0.9869 PASS** | **1.0438 FAIL (SPLIT)** | 0.9902 PASS |
| +3.4% | 1.0167 FAIL | 1.0754 FAIL | 1.0202 FAIL |

`median`(옛 코드)은 +3.2%, +3.3% 에서 `today` 와 판정이 갈린다. 옛 코드가 평균식으로는 통과할 패널을 떨어뜨렸다는 QA-A-155 의 결과가 오늘 코드 위에서 재현됐다. `denom`(분모만 교체)은 갈리지 않는다 — 갈림의 원인이 분모가 아니라 중심 교체(중앙값 중심 RMS)임이 이 열로 분리된다.

### 갈리지 않는 입력

| 입력 | today | median | denom | 갈림 |
|---|---|---|---|---|
| 평탄장 잡음 0.3% / 0.8% / 1.2% | 0.2980 / 0.7974 / 1.2008 | 0.2980 / 0.7974 / 1.2007 | 같음 | 없음 |
| 비네팅 모서리 1·2·3·4% + 잡음 0.3% | 0.3698 / 0.5207 / 0.7073 / 0.9075 PASS | 0.3699 / 0.5213 / 0.7081 / 0.9091 PASS | 0.3698~0.9069 PASS | 없음 (4% 에서 0.9075 대 0.9091, 문턱 아래) |
| 불량(0값) 화소 2·8·16·24·28·40 개 | 1.1443 … 4.9562 전부 FAIL | 전부 FAIL | 전부 FAIL | 없음 (최소 개수 2 개에서 이미 1.14 로 FAIL) |
| 과열 화소(3×) 1·4·16 개 | 1.5909 / 3.1373 / 6.2423 FAIL | 전부 FAIL | 전부 FAIL | 없음 |

16384 화소 중 불량 화소 2개만으로 `today` 가 이미 1.1443 FAIL 이라, 이 입력군에서는 불량 화소가 문턱을 넘기는 지점이 갈림이 생길 만한 구간(0.9~1.1) 아래에 있다. 불량 화소 개수 1개일 때(문턱을 가로지르는 지점)는 훑지 않았다.

### 실픽스처 (`CalData_6`, 읽기 전용)

경로 `D:/workspace-github/image-processing/tests/test_data/CalData_6/` (이 워크트리에는 없음 — `.gitignore` 가 `tests/test_data/*.raw` 를 제외; QA-A-156 이 같은 이유로 원본 체크아웃에서 읽었다).

| 입력 | today | median | denom | 갈림 |
|---|---|---|---|---|
| bright01 ~ bright06 원 프레임 | 10.73 ~ 16.72 FAIL | 10.70 ~ 16.52 FAIL | 10.52 ~ 16.42 FAIL | 없음 |
| bright05 를 bright06 의 게인으로 평탄화 | 9.0330 FAIL | 9.0330 FAIL | 9.0330 FAIL | 없음 |

실프레임 값은 전부 문턱 1.0 의 9배 이상이라 이 입력들은 문턱 판정을 가를 힘이 없다 — "안 갈렸다"는 이 입력에서는 약한 근거다. 이 수치는 QA-A-156 보고(9.0330)와 같다.

### 기준선 귀속

명령: `xpe_preprocess_tests.exe --gtest_also_run_disabled_tests --gtest_filter='A186Probe.*'`, 이 트리(HEAD `a4753fdc` + 프로브). 종료 코드 0. 모든 행에서 `arith == today` (대조군 통과).

### 미검증

- 합성 입력만으로 존재를 보였다. 실검출기 평탄장 중 갈리는 것이 있는지는 모른다 (CalData_6 은 문턱의 9배 이상).
- 갈림 구간은 "치우친 분포" 한 족에서만 찾았다 (QA-A-155 와 같은 결과). 다른 형태(예: 이봉 분포)의 갈림 구간은 훑지 않았다.
- 전 구간 스윕이 아니다 (amp 2.8~3.6 만 조밀하게).

### 결정에 필요한 것

없음 — 코드는 이미 정본(산술평균)이다. 옛 중앙값 식은 +3.2% 입력에서 판정을 갈랐고(이번 재측정으로 재확인), 그래서 바꾼 것이 맞았다.

---

## ③ §2 — `Y_flat_roi` 의 ROI 정의

### 검색 범위

`docs/` 전체와 `.moai/specs/` 에서 `Y_flat_roi`, `flat`+`ROI` 같은 줄, ROI 와 중앙/가장자리/배제/% 같은 줄을 찾았다 (`evidence/03_roi_search.txt`). 대조군: "ROI" 가 `docs/`+`.moai/specs/` 109 개 파일에 잡히고, `Y_dark_roi` 는 7 개 파일에 잡혔다 — 검색은 눈멀지 않았다.

### 결과 — 정의 없음

`Y_flat_roi` 는 4 줄에만 나온다, 전부 수식의 피연산자:

- `docs/project/Preprocessing-E2E-Automated-Evaluation-Protocol.md:218`, `:220`
- `docs/project/Algorithm-Evaluation-Protocol.md:68`, `:69`

어느 영역인지 정하는 문장은 정본·SRS·SPEC 어디에도 없다. 비교용 사실:

- **다크 쪽에는 규칙이 있다.** `SRS-CALIB-FUNC-035` 가 `Y_dark_roi` 를 "가장 어두운 10분위(`raw < percentile10(raw)`), 최소 화소 수 = 화소 수의 1%" 로 못박았다 (SRS 466행). 평탄장 쪽에 해당하는 항목은 없다.
- 다른 문서에 인접한 말이 있으나 `Y_flat_roi` 정의는 아니다: `XPE-ALG-001…Specification.md:1147` (GainMean 은 "ROI 내 `(Flood - Offset)` 의 spatial mean"), `:1174` (주석 "avoid detector edge artefacts", 사각형 ROI 인자) — 게인 맵을 만들 때 쓰는 평균이고 검증 지표의 `Y_flat_roi` 가 아니다. `xray-detector-calibration-prd.md:2066` 은 NPS 용 ROI(`roi_size=256, n_rois=5`).
- 구현은 오늘 유효 화소 전체를 쓴다 (`xpe_verify_gain` 의 `gain[i]` 유한·양수 화소, `:515-523`).

### 미검증

`docs/` 밖(이슈·PR 코멘트·외부 문서)에 정의가 있는지는 보지 않았다.

### 결정에 필요한 것

"평탄장 지표의 `Y_flat_roi` 를 어떻게 선택할지(전체 유효 화소 유지 / 중앙 N% / 가장자리 배제 등) 규칙을 사용자가 정할지" — 정하면 `FUNC-035` 처럼 SRS 항목으로 적고 코드가 그 규칙을 따르게 한다.

---

## ④ §3 (`0.5%` 조건부 문턱)

카드 지시대로 조사하지 않았다 (ABI 결정, 리더 몫).

---

## Gaps / Residual-risk

- 위 각 항목의 "미검증"을 따른다. 특히 실검출기 평탄장과 ROI 규칙은 이 저장소에 데이터·정의가 없다.
- 잔여 위험: `compute_robust_mean`(중앙값) 이 `xpe_verify_pipeline` 에 한 곳 남아 있다. FlatResidualPct 와 무관해 이번 카드 범위 밖이지만, 같은 "정본은 평균" 질문이 그 `snr` 계산에도 걸리는지는 확인하지 않았다.
