[2026-10-14 21:50 KST] [xpe-pre]

# QA-A-249 — #242 `xpe_verify_*` 판정 문턱 3개의 출처 조사

조사만 했다. 문턱 값, 판정, 이름은 바꾸지 않았다. 처분은 리더가 정한다.

## 1. 결과

| 문턱 | 쓰이는 곳 | 출처 | 근거 문서 | 실데이터에서 |
|---|---|---|---|---|
| `PRNU_IMPROVE_MIN_DB` 3.0 | `xpe_verify_gain` 의 `snr_improved` (`xpe_verify_metrics.cpp:643`) | 도입 커밋 `b6c19b8a` (2026-04-26) 한 개. 당시 주석 "gain correction should improve PRNU" | **없음** (§3). 같은 식을 `FPN_Reduction_dB` 라는 이름으로 적은 문서는 10 dB 와 20 dB 를 말한다 (§5) | 여섯 평탄 영상 중 어느 것의 판정도 이 문턱이 정하지 않았다 (§6) |
| `GAIN_COVERAGE_MIN` 0.99 | `xpe_verify_gain` 의 `coverage_ok` (`:642`) | 같은 커밋. 주석 "99% of gain values must be valid" | **없음** | 측정값 1.000000 (유효 = 유한하고 > 0). 판정에 걸리지 않았다 |
| `SNR_IMPROVE_MIN_DB` 2.0 | `xpe_verify_pipeline` 의 `overall_pass` (`:887`) | 같은 커밋. 주석 "minimum SNR improvement" | **없음** | 일곱 프레임에서 2.0 과 3.0 의 판정이 같다. 실제 손목 영상은 −5.574 dB 로 불합격이다 (§6) |

세 가지가 카드의 전제와 다르다.

1. **세 문턱 모두 도입 때부터 근거 문서가 없다.** 도입 커밋은 문서를 하나도 바꾸지 않았고 (`.moai/specs`·`docs/` 변경 0개), 코드 머리말이 인용한 "SPEC-XPE-P1A (Phase 10: Verification Metrics)" 라는 항목은 SPEC 에 없다 (§3).
2. **`PRNU_IMPROVE_MIN_DB` 는 이슈 본문 설명과 달리 PRNU 개선에 걸린다.** `xpe_verify_gain` 에서 `snr_improvement_db` 의 값은 `20·log10(prnu_before/prnu_after)` 이다 (§4). 이름이 어긋나는 쪽은 필드 이름(`snr_…`)이다.
3. **운영자 앱에서 이 판정을 부르는 코드를 찾지 못했다.** `main` 과 `dev/gui` 의 `gui/`, `clients/`, `tools/` 어디에도 `xpe_verify_*` 호출이 없다 (§7). 카드의 "앱이 `overall_pass` 를 보여 준다"는 전제는 이 범위에서 확인되지 않았다.

## 2. 기준 트리와 범위

- 기준: `dev/preprocess` `5c94d377`. 이 브랜치는 `origin/main` 보다 474 커밋 뒤에 있고 병합하지 않았다. 병합은 카드가 지시하지 않았고, 조사 대상 소스 `xpe_verify_metrics.cpp` 는 `HEAD` 와 `origin/main` 이 **바이트 동일**하다 (`git diff --quiet HEAD origin/main -- …` 통과). 문서 검색은 두 트리 모두에서 했고 두 결과가 같거나(`3 dB`, `0.99`, `PRNU`, 대조군) 한쪽이 한두 줄 많았다 (`99 %` 22 대 24, `2 dB` 2 대 3, `SNR improvement` 20 대 21). 늘어난 줄은 아래 분류에서 모두 후처리 문서였다.
- 실데이터 측정은 `HEAD` 의 로컬 `ci-preprocess` 빌드 (`xpe_preprocess.dll` sha256 `c2524c57…4d32`)로 했다. CI 산출물을 내려받은 것이 아니다.

## 3. 출처 조사 — 명령, 범위, 대조군

### 3.1 git 이력

| 명령 | 결과 (`evidence/10_`, `11_`, `12_`) |
|---|---|
| `git log -S"PRNU_IMPROVE_MIN_DB" --all --reverse` (세 상수 이름 각각) | 처음 나온 커밋은 세 상수 모두 `b6c19b8a` (2026-04-26 `feat(calibration): 7건 결함 수정 + FUNC-026/022~025/검증메트릭 구현`). 이후 커밋은 주석을 붙이거나 다른 문턱을 고친 것이다 (`4bcad451` QA-A-152, `c59263db` #242 보고 등) |
| `git log --all --reverse -- modules/preprocess/src/xpe_verify_metrics.cpp` | 첫 커밋 역시 `b6c19b8a`. 그 앞에 이 파일의 이력이 없다 |
| `git show --stat b6c19b8a` | 33개 파일, `docs/`·`.moai/specs/` 변경 **0개** (`13_`) |
| `git show b6c19b8a:…/xpe_verify_metrics.cpp` | 상수 옆 주석은 한 구절씩이다: "gain correction should improve PRNU", "99% of gain values must be valid", "minimum SNR improvement". 함수 머리말은 `REQ-P1A-XXX` 자리표시이고 파일 머리말은 "SPEC-XPE-P1A (Phase 10: Verification Metrics)" 라고 적는다. 커밋 메시지는 "검증 메트릭 API 구현 (DSNU, PRNU, SNR, defect density)" 한 줄이다 |
| 리터럴 값 `-G` 검색 (이름이 붙기 전에 3.0 등이 코드에 있었는지) | 해당 없음. 이름이 처음부터 붙어 도입되었다 (`10_` 끝 항목 비어 있음) |

같은 커밋에서 들어온 `DARK_BIAS_MAX = 5.0` 은 나중에 SRS-CALIB-FUNC-016 의 "abs(DarkBias) ≤ 5 ADU" 와 값이 같은 것으로 확인되었다. 도입 때의 주석은 "corrected dark should be near zero" 로 역시 출처를 적지 않았다. 즉 이 파일의 상수는 값이 맞았다고 해서 근거가 있었다는 뜻은 아니다.

### 3.2 문서 검색 (`evidence/01_doc_search.py.txt` 가 실행, 결과 `20_`~`23_`)

공통 범위: `git grep -n -I -i -E <패턴> <트리> -- docs .moai/specs .moai/project`, 트리는 `HEAD` 와 `origin/main`. `docs/quality-eval/` 는 포함했다 (소스 주석은 이 폴더를 요구 출처에서 뺀다. README 가 "생산 라인 QA 도구, 진단 소프트웨어 아님"이라고 적기 때문이다. 아래에서 이 폴더의 히트는 따로 센다).

| 패턴 | 찾는 것 | 히트 (HEAD / origin·main) | 분류 |
|---|---|---|---|
| `\b3(\.0)? ?dB\b` | 3.0 dB | 13 / 13 (9개 파일) | **보정과 무관.** 후처리 노이즈 제거 필터의 PSNR·SNR 이득 (`SPEC-XPE-P1B-ENH` AC-02, `SRS-ENHANCE-ADV-001` FR-1900, `XPE-VVP-001`, `sprint-plan` 등) |
| `0\.99\b` | 0.99 | 16 / 16 | 결함 보정 재현율 ≥ 99 %, 비선형성 다항식 R² > 0.99. **게인 유효 비율 아님** |
| `\b99 ?%` | 99 % | 22 / 24 | 같은 종류 (결함 recall·TPR, 이상치 제거율). 게인 유효 비율 아님 |
| `\b2(\.0)? ?dB\b` | 2.0 dB | 2 / 3 | 후처리 시험 계획 (`XPE-STP-001` UT-2.2-001). 보정 쪽 유일한 히트는 `RTM-CALIB-001:285` 인데 이 줄은 `xpe_verify_pipeline` 이 하는 일을 설명하고 "2.0 dB 문턱도 근거 없음 (#218·#242)" 이라고 적는다. **근거가 아니라 같은 조사의 기록이다** |
| `PRNU` | 개념 | 126 / 126 | 보정 문서의 PRNU 기준은 모두 **절대 잔차**다: `FlatResidualPct ≤ 1.0 %`, `PRNU_CV < 5 %` (TDS 예시), `PRNU_CV ≤ 10 %` (후처리 문서). "PRNU 개선 ≥ N dB" 를 요구하는 줄은 없다 (`23_`) |
| `gain[ _-]?coverage\|coverage of (the )?gain\|valid gain` | 게인 유효 비율 | 3 / 3 | 모두 "유효한 게인 맵을 적재한다" 시험 항목이나 오류 처리 서술. 비율 기준 아님 |
| `SNR[ _]?improve\|improvement … SNR\|snr_improvement` | SNR 개선 | 20 / 21 | 후처리 필터 문서가 대부분. 보정 쪽은 `RTM-CALIB-001:285` 뿐 |
| `62220` | IEC 62220 계열 | 325 / 325 (그중 196줄이 `quality-eval`) | 보정 문서에서 62220 을 인용하는 줄은 "평탄장 정규화가 필요하다"(`xpe-algorithm-spec-deepsync.md:124`), "Flat-field, Dark frame 사양"(`IAP-CALIB-001:1075`), 기본 모델 일치 확인(`cross-verification-report.md:45`)이다. 세 문턱의 값이나 PRNU 개선 기준을 62220 에서 가져왔다는 줄은 없다 |

**대조군** (같은 검색 모양이 알려진 근거를 찾아내는지):

| 패턴 | 알려진 근거 | 히트 |
|---|---|---|
| `\b5 ?ADU\b` | `DARK_BIAS_MAX`, SRS-CALIB-FUNC-016 | 24줄. 그중에 `SRS-CALIB-001…:148` (FUNC-016) 이 있다 |
| `FlatResidualPct *(<=\|<\|=) *1(\.0)?` | `FLAT_RESIDUAL_MAX_PCT`, SRS-CALIB-FUNC-017 | 5줄. `SRS-CALIB-001…:149` 가 있다 |
| `\b10 ?dB\b` | `DARK_REDUCTION_MIN_DB` | 9줄 |

대조군은 같은 범위·같은 명령 모양으로 알려진 상수의 근거 문장을 찾아냈다. 따라서 세 문턱에 대한 0건·무관 결과는 "검색이 읽지 못해서"가 아니다. 이 대조군이 증명하는 것은 검색이 이 문서들을 읽는다는 것까지다. 값이 다른 표기(예: "세 데시벨")로 적힌 근거는 이 패턴들로 잡히지 않는다. 개념 패턴(`PRNU`, 게인 유효 비율, SNR 개선)을 따로 둔 이유다.

SPEC: `git grep … HEAD -- .moai/specs/SPEC-XPE-P1A` 에서 `Phase 10|Verification Metrics|xpe_verify|verify_gain|overall_pass` 는 **0줄** (`origin/main` 도 0줄). 대조군으로 같은 SPEC 에서 `REQ-P1A-011` 은 27줄 나온다 (`13_`).

검색하지 않은 곳: `.moai/reports/`(우리 보고서라 요구 출처가 아님), 외부 표준 원문(IEC 62220 전문은 저장소에 없다), 이슈 #242 외의 다른 이슈 본문. 이슈 #242 에는 코멘트가 없다.

## 4. `PRNU_IMPROVE_MIN_DB` 는 무엇에 걸리는가 — 코드 인용

`xpe_verify_metrics.cpp` (이름과 `grep -n` 으로 확인한 줄):

- `:617` `gain_coverage = valid_gain_count / pixel_count`. 유효한 게인은 `std::isfinite(gain[i]) && gain[i] > 0.0f` (루프, 약 `:566`).
- `:632` `snr_improvement_db = 20.0 * log10(prnu_before / prnu_after)`. 이 줄을 감싼 조건은 `if (prnu_before > 0.0 && prnu_after > 0.0)`. 그렇지 않고 `prnu_before > 0.0 && after_perfect` 이면 `ZERO_SPREAD_DB`(200) 이고, 나머지는 `0.0`. `prnu_*` 는 보정 전·후 프레임의 `std/mean·100` 이다.
- `:640` `prnu_improved = prnu_after < prnu_before || (둘 다 < 0.01)`.
- `:643` `snr_improved = snr_improvement_db >= PRNU_IMPROVE_MIN_DB || (prnu_before < 0.01 && prnu_after < 0.01)`.
- `:678` `overall_pass = before_measurable && after_measurable && prnu_improved && coverage_ok && snr_improved && flat_residual_ok`.

확정한 것: `PRNU_IMPROVE_MIN_DB` 는 `xpe_verify_gain` 에서 **PRNU 의 개선량**(`snr_improvement_db` 라는 이름의 필드)에 걸린다. 헤더도 같은 말을 한다 ("xpe_verify_gain: 20*log10(prnu_before/prnu_after), the PRNU improvement", `preprocess_api.h:1544`). 이슈 #242 본문의 "이름은 PRNU 인데 실제로는 SNR 개선에 걸린다"는 이 함수에서는 사실과 다르다. SNR 이름이 붙은 **같은 필드** 가 `xpe_verify_pipeline` 에서는 `SNR_final − SNR_raw` 이고 (`:872`) 거기에 `SNR_IMPROVE_MIN_DB` 가 걸린다 (`:887`).

두 함수의 값이 같은 양이라는 소스 주석의 말은 식이 같다는 뜻이다. 같은 입력을 넣은 값은 같지 않다. 실데이터 평탄 6번은 `xpe_verify_gain` 에서 4.300 dB, `xpe_verify_pipeline` 에서 21.481 dB 다 (§6). 앞은 게인 단계만, 뒤는 오프셋·게인·결함 단계 전체와 오프셋 바탕값(원시 평균 4699, 최종 평균 2196)을 포함한 프레임 쌍이다.

덧붙여, `snr_improved`(3 dB 이상 개선)는 `prnu_improved`(조금이라도 개선)를 함의한다. 개선 3 dB 면 `prnu_after < prnu_before` 이기 때문이다 (둘 다 같은 "이미 평탄" 예외 조항을 가진다). 판정에서 `prnu_improved` 는 `snr_improved` 가 있는 한 추가로 걸러내는 것이 없다.

## 5. 문서에 있는 가장 가까운 기준

세 문턱의 값은 아니지만, **같은 식**이 요구 쪽 문서에 다른 이름으로 있다.

| 문서 | 정의 | 기준 |
|---|---|---|
| `Preprocessing-E2E-Automated-Evaluation-Protocol.md` §5.3 (`:220`, `:263`) | `FPN_Reduction_dB = 20·log10(std(R_flat_roi) / max(std(Y_flat_roi), ε))` — 표준편차 비율, 평탄 ROI | `≥ 10 dB` ("for real fixture evidence when raw non-uniformity exists", 승인된 수락 기본값) |
| `XPE-GUI-CALIB-001` 지표 표 (`:168`) | `20·log10(PRNU_CV_before / PRNU_CV_after)` — 변동계수 비율. 문서 상태 "Controlled Draft", 2026-04-19, "QA/Test Tool" | `≥ 20 dB` |
| `TDS-CALIB-001` (`:1497`) | 예시 출력 값 12.5 | (기준 아님, 예시) |

- `FPN_Reduction_dB` 는 **`modules/preprocess` 코드에 구현이 없다** (`git grep -E 'FPN_Reduction|fpn_reduction' -- modules/preprocess/src modules/preprocess/include` 0줄). 1단계 B6 측정에서는 하니스가 계산했다.
- `xpe_verify_gain` 의 `snr_improvement_db` 는 `GUI-CALIB-001` 의 식(변동계수 비율)과 같고, 프로토콜의 식(표준편차 비율)과는 평균 변화만큼 다르다. 게인 보정은 평균을 거의 그대로 두므로 두 값은 가깝지만 같지는 않다. 이 차이를 실데이터로 분리해 재지는 않았다.
- 두 문서가 요구하는 값이 서로 다르다 (10 대 20). 어느 쪽도 3.0 이 아니다.

## 6. 실데이터 값과 판정 영향

입력: `calib-real-v2`(offset `0f888eb6…`, gain `004056fd…`, defect `ff9df014…`)와 `CalData_6` 평탄 6장, 손목 영상 `wrist_lat_3072x3072.raw`. 평탄 4·5·6 은 이 게인 맵을 만든 표본이고 1·2·3 은 맵에 쓰지 않은 프레임이다. 하니스는 `A241Measure.DISABLED_A249_VerifyThresholdsOnRealData` (소스 사본 `evidence/02_harness_a249.cpp.txt`, 원값 `32_`, 예측 `30_`).

**예측과 결과.** 실행 전에 적은 예측 (`30_`) 중 틀린 것이 있다. 게인 판정의 `prnu_after` 를 약 0.8 % 로 예상했으나 6.5~7.8 % 였다. 원인 가설(범위 밖 게인 화소를 유효로 센다)을 세워 마스크 변형으로 확인했고, 맞았다 (아래). 손목의 파이프라인 판정이 불합격일 것이라는 예측은 맞았다.

### 6.1 게인 맵

| 항목 | 값 |
|---|---|
| 유한하고 > 0 (= `xpe_verify_gain` 의 "유효") | 9,437,183 / 9,437,184 = **1.000000** |
| 제품 범위 [0.1, 10.0] 안 | 9,393,131 = 0.995332 |
| 범위 밖의 종류 | 유한하지 않음 0 · 유한하고 ≤ 0 이 1 · 양수이고 0.1 미만 **44,051** · 10 초과 1 |

`xpe_verify_gain` 의 유효 조건(유한하고 > 0)은 제품이 결함으로 분류하는 범위(0.1~10)보다 넓다. 그래서 이 맵에서 0.99 문턱은 한 번도 걸리지 않는다 (측정값 1.000000).

### 6.2 `xpe_verify_gain`, 평탄 한 장씩

게인 단계는 모듈의 `xpe_gain_correct` 로 돌렸다. 평탄은 이미 오프셋 보정된 프레임이다. NORMALIZED 는 평탄 한도 0.5 %, UNKNOWN 은 1.0 % 를 쓴다.

**(a) 맵 그대로 (범위 밖 게인 화소 44,053개를 유효로 센다)**

| k | 구분 | 평균 | prnu_before % | prnu_after % | coverage | improve dB | ≥ 3.0 | 판정 NORM / UNK | 두 임시 문턱을 뺀 판정 NORM / UNK |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 표본 밖 | 2883.2 | 11.5004 | 7.8464 | 1.000000 | 3.321 | 예 | 0 / 0 | 0 / 0 |
| 2 | 표본 밖 | 5014.6 | 11.1195 | 7.3419 | 1.000000 | 3.606 | 예 | 0 / 0 | 0 / 0 |
| 3 | 표본 밖 | 350.6 | 16.7214 | 19.2244 | 1.000000 | **−1.212** | 아니오 | 0 / 0 | 0 / 0 |
| 4 | 표본 | 867.4 | 11.1281 | 7.0196 | 1.000000 | 4.002 | 예 | 0 / 0 | 0 / 0 |
| 5 | 표본 | 1395.7 | 10.8801 | 6.7301 | 1.000000 | 4.172 | 예 | 0 / 0 | 0 / 0 |
| 6 | 표본 | 2196.2 | 10.7309 | 6.5407 | 1.000000 | 4.300 | 예 | 0 / 0 | 0 / 0 |

**(b) 범위 밖 게인 화소를 무효(0.0)로 넘긴 변형.** 프로토콜 §5.3 이 FlatResidualPct 에서 이 화소를 빼는 방식을 따른 것이다. 제품 경로가 아니라 이 조사가 만든 변형이다.

| k | 구분 | prnu_before % | prnu_after % | coverage | improve dB | ≥ 3.0 | 판정 NORM / UNK | 두 임시 문턱을 뺀 판정 NORM / UNK |
|---|---|---|---|---|---|---|---|---|
| 1 | 표본 밖 | 9.4811 | 4.4171 | 0.995332 | 6.634 | 예 | 0 / 0 | 0 / 0 |
| 2 | 표본 밖 | 9.0032 | 3.4099 | 0.995332 | 8.433 | 예 | 0 / 0 | 0 / 0 |
| 3 | 표본 밖 | 14.2245 | 17.0773 | 0.995332 | −1.588 | 아니오 | 0 / 0 | 0 / 0 |
| 4 | 표본 | 8.6624 | **0.8710** | 0.995332 | 19.953 | 예 | 0 / **1** | 0 / 1 |
| 5 | 표본 | 8.5537 | **0.6247** | 0.995332 | 22.729 | 예 | 0 / **1** | 0 / 1 |
| 6 | 표본 | 8.4990 | **0.4327** | 0.995332 | 25.863 | 예 | **1** / 1 | 1 / 1 |

읽는 법:

- **(a) 의 큰 잔차(6.5~7.8 %)는 범위 밖 게인 화소 때문이다.** 그 화소를 유효로 세는 `xpe_verify_gain` 은 보정되지 않은 값을 평균·표준편차에 넣는다. (b) 에서 표본 평탄 4~6 의 잔차가 0.87 / 0.62 / 0.43 % 로 내려가는 것이 가설을 지지한다. 1단계 B6 가 보고한 평탄 6번 0.844 % 는 다른 방식(결함 단계까지 거친 최종 영상, 별도 마스크)으로 잰 값이라 (b) 의 0.4327 과 같은 양이 아니다. 두 값은 맞춰 보지 않았다.
- **어느 문턱이 판정을 정했는가.** 12번의 평가(평탄 6장 × (a)(b)) 어디서도 "두 임시 문턱(0.99, 3.0)을 뺀 판정"과 모듈이 돌려준 판정이 갈리지 않았다. 판정을 정한 것은 평탄 잔차 한도(0.5 % / 1.0 %)와, 평탄 3번에서는 개선 자체가 음수(`prnu_improved` 거짓)였다. NORMALIZED 열은 스크립트로 12줄 모두 대조했고 (`33_`) UNKNOWN 열은 `prnu_after ≤ 1.0` 과 눈으로 맞춰 보았다.
- 3.0 선과의 거리는 (a) 에서 표본 밖 평탄 1·2 가 0.32·0.61 dB 로 가깝다. 이 영상들은 잔차 한도에서 이미 불합격이므로 판정은 바뀌지 않았다.
- 평탄 3번(평균 350.6 ADU, 저선량)은 어느 변형에서도 개선이 음수다. 이 영상에서 `prnu_before` 는 거의 화소 잡음이다.

### 6.3 `xpe_verify_pipeline`, 앱 구성의 `pipeline_out` 결과

| 프레임 | 원시 평균 | 최종 평균 | improve dB | ≥ 2.0 | ≥ 3.0 | `overall_pass` |
|---|---|---|---|---|---|---|
| **손목 (실제 영상)** | 4705.5 | 2198.2 | **−5.574** | 아니오 | 아니오 | **거짓** |
| 암전류 + 평탄 1 | 5386.3 | 2886.5 | 3.079 | 예 | 예 | 참 |
| 암전류 + 평탄 2 | 7517.7 | 5018.8 | 6.629 | 예 | 예 | 참 |
| 암전류 + 평탄 3 | 2853.7 | 350.4 | 0.081 | 아니오 | 아니오 | 거짓 |
| 암전류 + 평탄 4 | 3370.5 | 867.4 | 10.696 | 예 | 예 | 참 |
| 암전류 + 평탄 5 | 3898.9 | 1395.7 | 16.047 | 예 | 예 | 참 |
| 암전류 + 평탄 6 | 4699.4 | 2196.3 | 21.481 | 예 | 예 | 참 |

- 일곱 프레임에서 2.0 과 3.0 의 판정이 같다. 문턱 값이 갈라 놓은 프레임이 없다.
- **손목 영상은 −5.574 dB 로 불합격이다.** 이 값 이하가 아닌 어떤 문턱도(−5.5 보다 큰 값은 모두) 불합격을 만든다. 문턱을 바꿔서는 이 판정이 달라지지 않는다. 식은 원시 프레임의 변동계수(바탕값이 평균에 들어 있다)와 최종 프레임의 변동계수(바탕값 제거 뒤)를 비교하는데, 평균이 4705.5 에서 2198.2 로 줄어든 영상에서는 해부 구조가 만드는 퍼짐 때문에 이 비율이 나빠지는 쪽으로 움직인다. 표준편차는 출력하지 않았으므로 이 설명은 식과 평균 두 값에서 읽은 것이고 직접 재지는 않았다.
- 한 장의 실제 영상 결과다. 다른 영상에서의 부호는 재지 않았다.

## 7. 앱이 이 판정을 쓰는가

| 범위 | 결과 (`evidence/14_`) |
|---|---|
| `main` (`1b128f7d`) 의 `gui/`, `clients/`, `tools/` | `xpe_verify`·`overall_pass`·`VerifyGain`·`VerifyPipeline` 호출 0건. 히트는 무관한 두 가지뿐이다: 에이전트 스킬 문서의 `overall_pass_threshold` 와 clang-tidy 기준선 파일 |
| `dev/gui` (`29de1cee`) | 같은 결과 |
| `XpeCalibrationMetrics`·`snr_improvement_db`·`gain_coverage` | `gui/`, `clients/` 에 0건 (기준선 파일만) |
| 대조군: 같은 명령 모양으로 `xpe_preprocess_pipeline_out` | 두 트리 모두 `clients/` 에서 여러 파일이 나온다 |
| 앱의 `CalibrationMetricsGrid` | `CalibrationLoads`(적재 결과)에 묶여 있다. 검증 지표가 아니다 |
| `XPE-GUI-CALIB-001` (검증 시험 GUI 명세) | `xpe_verify`·`overall_pass` 를 언급하지 않는다. 이 명세의 `FPN_Reduction_dB ≥ 20 dB` 는 §5 |

따라서 지금 저장소에서 이 세 문턱이 사용자에게 보이는 판정을 정한다는 것은 **관찰되지 않았다.** 호출하는 곳은 `modules/preprocess/tests/` 의 시험 8개뿐이다. 저장소 밖의 호출자(다른 저장소, 이미 배포된 앱)는 검색하지 못했다.

## 8. 처분 후보 — 근거와 영향만, 정하는 것은 리더

| 문턱 | 후보 | 근거와 실데이터 영향 |
|---|---|---|
| 3.0 `PRNU_IMPROVE_MIN_DB` | 그대로 둔다 (임시값 유지) | 근거 없음은 그대로. 평탄 6장에서 판정을 정한 적이 없으므로 현재 데이터로는 실사용 영향이 관찰되지 않는다. 데이터 밖(다른 패널, 다른 선량)은 모른다 |
| | 요구가 있는 쪽으로 바꾼다: `FPN_Reduction_dB ≥ 10 dB` (프로토콜 §5.3) | 문서 근거가 있는 가장 가까운 값. 그러나 식이 다르다(§5). 바꾸면 (b) 변형 기준 표본 평탄 4·5·6(19.95 / 22.7 / 25.9)은 통과, 표본 밖 1·2(6.6 / 8.4)는 불합격이고, 맵 그대로의 (a) 는 여섯 장 모두 불합격이다. 다만 어느 쪽도 최종 판정은 평탄 잔차 한도가 정한다. 요구 문구를 바꾸는 일이라 REQ-CHANGE-LOG 대상이다 |
| | 판정에서 빼고 참고값으로 보고한다 | 12번의 평가에서 최종 판정이 바뀌지 않는다 (§6.2). 이름은 `snr_improvement_db` 필드로 남는다 |
| 0.99 `GAIN_COVERAGE_MIN` | 그대로 둔다 | 실데이터 측정값 1.000000, 걸린 적 없음. 유효 조건이 "유한하고 > 0" 이라 제품이 결함으로 분류하는 범위(0.1~10, 최대 5 % 허용)보다 넓다. 제품이 허용하는 맵에 비양수·NaN 이 1 % 넘게 있으면 제품은 받아들이는데 이 선에서 불합격한다 (이런 맵은 이번 데이터에 없다) |
| | 유효 조건을 제품 범위로 맞춘다 | 이 맵의 값은 0.995332 로 여전히 0.99 이상이다. 범위 밖 화소가 1 % 를 넘는 맵이 불합격이 된다. 판정 의미가 바뀌는 일이다 |
| | 판정에서 빼고 참고값으로 보고한다 | 실데이터에서 판정이 바뀌지 않는다 |
| 2.0 `SNR_IMPROVE_MIN_DB` | 그대로 둔다 | 근거 없음. 일곱 프레임에서 2.0 과 3.0 의 판정이 같다. 실제 손목 영상은 문턱과 무관하게 불합격(−5.574 dB)이다 |
| | 판정에서 빼고 참고값으로 보고한다 | `xpe_verify_pipeline` 의 `overall_pass` 는 이 한 조건으로만 정해지므로 빼면 판정 자체가 없어진다 (함수가 판정을 내려야 하는지는 별도 결정) |
| (두 함수 공통) | 3.0 과 2.0 을 같게 맞춘다 | 같은 식이어도 입력 프레임 쌍이 달라 값이 같지 않다(평탄 6번 4.300 대 21.481). 맞추는 근거가 되지 않는다는 점만 사실로 적는다 |

"근거 있음 → 요구로 세우고 이름 정정" 에 해당하는 문턱은 이번 조사에서 하나도 나오지 않았다.

## 9. 못 본 것과 남는 위험

- **저장소 밖 근거.** 도입 당시의 설계 대화, 외부 메신저, 다른 저장소의 문서는 보지 못했다. 이 조사의 "근거 없음"은 "저장소 안 문서와 이력에 없음"이다.
- **데이터 폭.** 한 패널의 평탄 6장과 손목 한 장이다. 다른 패널·선량에서 문턱이 판정을 정하는지는 모른다.
- **(b) 변형은 제품 경로가 아니다.** 범위 밖 게인 화소를 무효로 넘기는 것은 이 조사가 만든 구성이다. 앱이나 제품이 그렇게 호출하지 않는다.
- **`FPN_Reduction_dB` 두 정의의 차이.** 표준편차 비율과 변동계수 비율의 차이를 실데이터로 분리해 재지 않았다.
- **−5.574 dB 의 원인.** 식과 두 평균에서 읽은 설명이다. 표준편차를 출력해 직접 확인하지 않았다.
- **로컬 빌드.** 측정은 로컬 `ci-preprocess` 빌드의 값이다. CI 러너에서의 값은 보지 않았다.
- **B6 값과의 대조.** 1단계 B6 의 평탄 6번 0.844 % 와 (b) 의 0.4327 % 를 같은 조건으로 맞추지 않았다.

## 10. 실행 범위와 종료 확인

카드의 범위 제한(조사와 실데이터 지표 계산, 스위트 전체·전체 재빌드 금지)에 따라 다음만 실행했다.

- 검색: `git log -S`, `git grep`, `gh issue view 242`, `git show` (읽기 전용).
- 빌드: `xpe_preprocess_tests` 한 타깃만, 증분 빌드 (측정 하니스 한 개를 추가한 `test_zz_a241_measure.cpp` 하나가 다시 컴파일되고 링크된다). 마스크 변형을 추가한 뒤 문자열 오류로 실패한 빌드가 세 번 있었고 네 번째에 성공했다. 전체 재생성은 하지 않았다.
- 실행: `A241Measure.DISABLED_A249_VerifyThresholdsOnRealData` 하나 (가설 확인을 위해 마스크 변형을 넣고 한 번 더 돌려 총 두 번, 각 약 10초). 시험 스위트 전체, ctest, clang-tidy, doxygen 은 돌리지 않았다.
- 종료 확인 (2026-10-14 21:50): `xpe_*tests`, `xpe_calib_fixture_gen`, `clang-tidy`, `ctest`, `cl`, `link`, `ninja`, `cmake`, `python`, `doxygen` 프로세스 **0개**.

## 11. 변경 파일

`modules/preprocess/tests/test_zz_a241_measure.cpp` (`DISABLED_A249_VerifyThresholdsOnRealData` 추가, CI 에서 돌지 않는 측정 하니스). 제품 코드 변경 없음. 증거: `.moai/reports/lane-pre/QA-A-249/evidence/`.

## Card Cross-Check

| milestone | card |
|---|---|
| #242 처분 "결정 전에 조사" — 세 문턱의 출처, 실데이터 통과/실패, `PRNU_IMPROVE_MIN_DB` 의 실제 대상 | QA-A-249 (이 보고서). 처분 결정은 리더 |
