# QA-A-188 (#218) — FUNC-019 합성 오라클 시험(상시) + pipeline SNR 주석

시작 HEAD `a3de4607` (`evidence/00_head.txt`, 리더의 로컬 main 을 `git merge --ff-only main` 으로 받음).

## 한눈에

| 항목 | 결과 |
|---|---|
| ① 오라클 시험 | `DefectOracle.*` 4건 상시. **오늘 코드가 정본 합격선을 그대로 만족한다** (결함 발견 없음) |
| 반증 | 제품을 네 가지로 망가뜨린 팔 4개 모두 recall/FPR/GoodPixelDeltaP99 단언이 빨강 (한 결함만 놓친 팔 포함) |
| ② SNR 주석 | `xpe_verify_pipeline` 의 2.0 dB 판정 옆에 "요구 근거 없음" 주석, 동작 불변 (주석만 변경 증명) |

전체 시험 1회: **725 통과, 0 실패** (종료 코드 0, `08_full_suite.txt`), 셔플(시드 4242) 종료 0. `ctest -N`: 총계 842, DISABLED 40. 프리셋 점검 OK. 내보내기 이름 53 = 53 동일.

## ① FUNC-019 합성 오라클 시험

### 무엇을 단언하나 (정본 합격선 그대로)

`Preprocessing-E2E-Automated-Evaluation-Protocol.md` §5.5 의 정의식과 합격선, `SRS-CALIB-001:150` ("합성 BPM 오라클 케이스는 결함 recall 100%, 오탐률 0.001% 미만"):

| 지표 | 정의 | 단언 |
|---|---|---|
| DefectRecall | `TP / max(TP+FN, 1)` | `FN == 0` 이고 recall `== 1.0` |
| DefectFPR | `FP / max(FP+TN, 1)` | `100 × FPR < 0.001` (%) |
| GoodPixelDeltaP99 | 정상 화소에서 `percentile99(abs(결함 단계 출력 − 결함 단계 입력))` | `<= 1 ADU` |

시험 `DefectOracle.RecallFprAndGoodPixelDeltaMeetTheCanonicalLines`: 시드 3개(1, 2, 3)를 모두 단언한다.

### 체인과 합성 프레임

체인은 GUI 가 돌리는 것과 같다: `xpe_bpm_generate`(합성 dark 8장 + bright 12장, 기본 설정)로 결함 맵을 예측하고, 그 맵을 `xpe_calib_load_defect_map` 으로 올려 `xpe_defect_correct` 를 돌린다.

프레임 256×256, 고정 시드. 정답 결함 130개를 알려진 위치에 심는다. 배치를 섞었다 (`evidence/02_first_run.txt`: isolated 59, cluster 45, line 16, edge 10):

- 고립 화소: 5종 × 12개 (서로 5×5 안에 다른 결함이 없는 위치)
- 3×3 군집 5개 (종류마다 하나)
- 1×8 선분 2개 (죽은 열 조각, 뜨거운 행 조각)
- 프레임 네 모서리와 가장자리 (종류 5가지 모두)

결함 종류 5가지: 죽은 화소(0), 뜨거운 화소(dark·bright 둘 다 +3000), 고착(30000), 낮은 응답(bright 에서 −12%, 문턱 7% 보다 약간 큼), 높은 응답(+12%).

### 오늘 코드 결과 (세 시드 모두)

```
seed=1 truth=130 TP=130 FN=0 FP=0 recall=100.0000% FPR=0.000000% GoodPixelDeltaP99=0.0000 ADU
seed=2 truth=130 TP=130 FN=0 FP=0 recall=100.0000% FPR=0.000000% GoodPixelDeltaP99=0.0000 ADU
seed=3 truth=130 TP=130 FN=0 FP=0 recall=100.0000% FPR=0.000000% GoodPixelDeltaP99=0.0000 ADU
```

정본 합격선을 오늘 코드가 만족한다. 시험을 느슨하게 만든 곳은 없다. 단 프레임이 65536 화소라 FPR < 0.001% 는 오탐 0개를 뜻한다 (한 개가 0.0015%).

### 반증 팔 — 제품을 망가뜨려 오라클 시험이 빨개지는지

`xpe_defect_gen.cpp` 한 곳만 바꾸고 빌드한 뒤 `RecallFprAndGoodPixelDeltaMeetTheCanonicalLines` 를 돌렸다. 매 팔마다 `BUILD_EXIT` 를 읽었고, 끝에 원복했다 (`git diff` 0줄). `evidence/05_arms_summary.txt`.

| 팔 | 망가뜨린 곳 | seed 1 결과 | 빨개진 단언 |
|---|---|---|---|
| A | bright 문턱을 3배로 둔감하게 | TP=85 **FN=45** recall 65.38% | recall (`FN == 0`, `recall == 1.0`) |
| B | 왼쪽 프레임 가장자리(열 0)를 절대 표시하지 않음 | TP=127 **FN=3** recall 97.69% | recall |
| D | **결함 정확히 하나** (128, 20) 만 놓침 | TP=129 **FN=1** recall 99.23% | recall |
| C2 | bright 문턱을 0.5% 로 예민하게 | **FP=20642** FPR 31.56% | FPR, GoodPixelDeltaP99(322.7 ADU) |

팔 D 가 요청한 "한 결함이라도 놓친 팔"이다: 130개 중 하나만 놓쳐도 recall 단언이 빨개진다.

**팔 C 의 첫 시도는 무효였다.** `tolerance_pct` 가 쓰이지 않게 되어 `/WX` 로 빌드가 실패(`BUILD_EXIT=1`)했고, 그 실행은 팔 A 의 낡은 바이너리 결과를 찍었다. 빌드 결과를 읽어서 잡았고, 변수를 계속 쓰도록 고친 C2 로 다시 쟀다. C2 의 수치만 위 표에 썼다.

### 시험 지표 자체의 대조군 (제품과 무관)

- `ControlOneMissedDefectBreaksRecall`: 완벽한 예측에서 결함 하나를 지우면 recall < 1.0 이 된다.
- `ControlOneFalseAlarmShowsInFpr`: 정상 화소 하나를 표시하면 FPR ≥ 0.001% 가 된다.
- `ControlFlaggingManyGoodPixelsBreaksGoodPixelDelta`: 정상 화소의 약 14%를 표시하면 GoodPixelDeltaP99 > 1 ADU 가 된다.

### 시험 시간

오라클 시험 1건이 약 7.6 초(시드 3개, 시드당 BPM 생성 약 2.5 초)다. 전체 시험 1회는 51.5 초(`08_full_suite.txt`)이고 이 시험이 그중 약 7.5 초다.

### 이 오라클이 보지 않는 것 (Gaps)

- **노이지 화소**(시간 분산이 큰 화소, SRS FUNC-003 의 종류 4)는 심지 않았다. `xpe_bpm_generate` 는 프레임 평균으로 판정하므로 노이지 화소는 평균이 정상일 수 있고, 그 검출은 런타임 검출(FUNC-010)의 몫이다. 이 오라클이 그것을 비껴간다는 뜻이지 검출된다는 뜻이 아니다.
- 낮은/높은 응답을 ±12% 한 값만 쟀다 (문턱 7% 근처 ±1~5% 의 검출 한계는 훑지 않았다).
- 합성 잡음 모델 한 가지(가우시안 + 화소 고정 패턴), 크기 256×256 한 가지. 3072×3072 실프레임으로는 안 쟀다 (GUI 의 오라클 측정은 별도).
- `DefectResidualADU`(정본에 합격선 없음)는 이 시험의 단언이 아니다.
- 시험은 기본 설정(`XpeBpmConfig` nullptr)만 쓴다.

### 결정에 필요한 것

없음 — 정본 합격선이 오늘 코드에서 만족되고 시험이 상시 지키며, 반증이 4팔 모두 빨갛다. 열린 것은 "노이지 화소를 어느 경로의 오라클에 넣을지"와 GUI 합격선 정렬(QA-A-187 보고)뿐이다.

## ② `xpe_verify_pipeline` SNR 판정 주석

`xpe_verify_metrics.cpp` 의 `overall_pass = (snr_improvement_db >= SNR_IMPROVE_MIN_DB)` 바로 위에 주석을 달았다: 정본(Protocol 의 SNR 은 PSNR 기준 이미지 비교뿐), SRS FUNC-015/-021 어디에도 "SNR 개선" 지표나 2.0 dB 선이 없고, 중심이 중앙값(+RMS)이라는 점, QA-A-187 이 문턱 직하에서 판정이 갈림을 쟀다는 점, 값과 선은 그대로 둔다는 점 (#218).

`evidence/06_comment_only_check.txt`: 주석을 지운 뒤 HEAD 와 동일, 대조군(코드 토큰 하나 변경)은 감지됨. 제품 `src` diff 는 이 주석 9줄뿐이다.

## Gaps / Residual-risk

- 위 "이 오라클이 보지 않는 것"을 따른다.
- 잔여 위험: 합성 프레임에서 단언이 모두 만족하는 것은 실검출기 프레임에서의 recall/FPR 을 말해 주지 않는다. 오라클은 정답 맵이 있는 입력에서의 하한 증거다.
