# QA-A-223 — #242 문턱 셋의 출처 조사 + #216 RTM 줄에만 이름이 있는 7개 읽기 (Refs #242 #216)

코드 변경 없음, 보고서만이다. 증거: `evidence/`. 기준 트리: `origin/main` `f9803071`의 문서 블롭과, 이 워크트리(`dev/preprocess` `1d32bf27`)의 코드·DLL.

## 결론

**#242 (문턱 셋)**

| 상수 | 값 | 출처 | 결론 |
|---|---|---|---|
| `PRNU_IMPROVE_MIN_DB` | 3.0 dB | 도입 커밋 `b6c19b8a` 하나. 근거 기록 없음 | **근거 없음.** 같은 값의 기준이 다른 모듈(노이즈 저감)에 있으나 다른 양이라 출처로 못 쓴다 |
| `SNR_IMPROVE_MIN_DB` | 2.0 dB | 같은 커밋 | **근거 없음.** 같은 사정 |
| `GAIN_COVERAGE_MIN` | 0.99 | 같은 커밋 | **근거 없음.** 요구 쪽에 게인 화소 비율 허용치가 하나(5%) 있으나 경로·의미가 다르다 |

그리고 이 조사가 이슈·QA-A-152 의 전제 둘을 바꿨다.

1. **두 문턱(3.0 / 2.0)은 같은 양에 걸려 있다.** `xpe_verify_gain` 의 `snr_improvement_db` 는 `20·log10(prnu_before/prnu_after)` 이고, `xpe_verify_pipeline` 의 같은 이름 필드는 `SNR_final − SNR_raw`(SNR = `20·log10(mean/std)`)다. 변동계수 CV 에 대해 두 식은 대수적으로 같고(수치 확인, 차이 ≤ 8e-15 dB), 다른 것은 추정기뿐이다. 그래서 "`PRNU_IMPROVE_MIN_DB` 가 PRNU 가 아니라 SNR 에 걸려 이름이 오용됐다" 는 전제는 반만 맞다: gain 함수 안에서 비교되는 값은 **진짜 PRNU 개선(dB)** 이라 상수 이름이 맞고, 어긋난 것은 **필드 이름** `snr_improvement_db` 다(ABI 잠금 FUNC-037 대상).
2. **`xpe_verify_gain` 은 완벽히 보정된 패널을 불합격시킨다**(실제 DLL 로 확인, §1.4). 문턱을 정하기 전에 알아야 할 동작이다.

**#216 (RTM 줄에만 이름이 있는 7개)**: SRS 본문이 그 함수가 하는 일을 서술하는지 읽었다.

| 함수 | SRS 본문이 서술하나 |
|---|---|
| `xpe_nonlinearity_correct` | **능력은 서술, API 계약 일부는 없음** (FUNC-006, -006-EXT) |
| `xpe_calib_generate_nonlin_lut` | **서술 있음** (-006-EXT 6a 절차 6단계) — 단 헤더가 낡았다 |
| `xpe_calib_load_nonlin_lut` | **일부** (검증 규칙은 있고, 적재해 활성화한다는 계약은 없음) |
| `xpe_calib_unload_nonlin_lut` | **서술 없음** |
| `xpe_bpm_generate` | **일부** (FUNC-022·023 검출 매개변수) — 병합·반사 패딩·프레임 수는 없고, 헤더·RTM 이 붙인 FUNC-024·025 는 다른 요구 |
| `xpe_verify_defect` | **서술 없음** (FUNC-019 는 이 함수가 하지 않는 일을 서술) |
| `xpe_verify_pipeline` | **서술 없음** (FUNC-015·021 도 이 함수가 하지 않는 일) |

## 0. 방법과 범위

- **문서 검색**: `git grep` 을 `origin/main` `f9803071` 의 `docs/`·`.moai/specs`·`.moai/project` 에 적용(보관 폴더와 JSON 연구 기록 포함, 전수). 값으로, 그리고 개념으로. **대조군**: 양성 `DarkBias.{0,80}5 ADU` → `SRS-CALIB-FUNC-016` 과 `PRIOR-ART-BPM-ALGORITHM.md:382` 가 나옴, 음성(지어낸 지표) → 0건(`evidence/20`). 개념 검색의 양성 대조는 `DarkBias` 7파일.
- **이력 검색**: `git log --all -S` 로 세 식별자와 원래 주석 문구(`evidence/10`·`26`).
- **안 한 것**: 외부 표준 원문(IEC 62220-1-1 등)은 읽지 않았다(유료). 저장소가 그 표준을 인용한 줄은 확인했고(`evidence/22`), 인용에 PRNU 개선 dB·게인 커버리지 문턱을 적은 줄은 없다. 웹 검색도 하지 않았다. `.moai/reports` 는 우리 자신의 보고서라 제외했다.
- **셸 함정 재발 방지**: 이번에도 `git show 리비전:경로` 는 쓰지 않고 `git grep`·`git cat-file blob` 을 썼다.

## 1. #242 — 문턱 셋은 어디서 왔나

### 1.1 도입 이력 (`evidence/10`·`11`·`12`)

세 상수는 **한 커밋에 한꺼번에** 들어왔다: `b6c19b8a`(2026-04-26, "feat(calibration): 7건 결함 수정 + FUNC-026/022~025/검증메트릭 구현", MEDIUM 수정 항목 한 줄 "검증 메트릭 API 구현 (DSNU, PRNU, SNR, defect density)"). 그 뒤 값은 한 번도 안 바뀌었다(각 상수의 대입 문장을 바꾼 커밋이 정확히 1개). 커밋은 문서를 건드리지 않았고(`docs/`·`.md`·spec 0개), diff 안 다른 곳에도 3 dB·99%·2 dB 가 없다. 커밋 메시지는 문턱의 근거를 적지 않는다. 원래 주석은 "gain correction should improve PRNU", "99% of gain values must be valid", "minimum SNR improvement" 가 전부다.

### 1.2 값·개념 검색 (`evidence/20`~`23`)

요구 쪽에서 이 세 문턱을 정하는 문장은 **없다.** 값이 겹치는 기준은 둘 있지만 모두 **다른 양**이다.

| 값 | 겹치는 곳 | 무엇을 재나 |
|---|---|---|
| 3 dB | `SPEC-XPE-P1B-ENH` AC-02(`spec.md:395`, `acceptance.md:161`), `XPE-VVP-001` ST-011 | **bilateral 노이즈 저감**의 SNR 이득. 전처리 게인 보정이 아니다 |
| 2 dB | `XPE-STP-001` UT-2.2-001 | bilateral 시험의 SNR 이득. 같은 사정 |

(같은 숫자, 다른 양: #155·QA-A-152 의 `DSNU` 와 같은 형태다. 값이 맞아서 채택하면 안 된다. 이 값들이 전처리 문턱의 출발점이었다는 증거도 없다 — 도입 커밋이 문서를 건드리지 않았고 메시지에도 없다.)

배경 문서는 둘 있다. `xpe-algorithm-spec-deepsync.md:95` 는 Ranger 2014("교정이 SNR 변동을 줄인다")를 근거로 **SNR 개선이라는 지표의 종류**를 뒷받침하되 수치 문턱은 주지 않는다. `:498` 은 스스로 **"Gap: No runtime QC metric computation specified"**(중요도 Medium)라고 적는다 — 설계 문서가 런타임 QC 지표와 그 합격선이 명세되지 않았다고 기록한 것이다.

### 1.3 중복 — 같은 양에 문턱이 둘 (`evidence/23`·`24`)

- 코드: `xpe_verify_gain` `:574` `snr_improvement_db = 20*log10(prnu_before/prnu_after)`, 여기서 `prnu = std/mean*100`. `xpe_verify_pipeline` `:785-788` `snr_improvement_db = snr_final − snr_raw`, `snr = 20*log10(mean/std)`.
- 대수: `20·log10(CV_before/CV_after) = SNR_after − SNR_before`. 수치로 5회(`evidence/24`): 두 식 값이 소수 아홉 자리까지 같고 차이 ≤ 8e-15 dB. **이 수치는 같은 추정기로 두 식을 재구현해 확인한 것이고, 실제 두 함수에 같은 영상을 넣어 비교한 것은 아니다**(§5).
- 실제 함수 사이의 차이는 추정기다: gain 은 산술평균과 유효 게인 화소(QA-A-156), pipeline 은 중앙값 중심·RMS·전체 화소. QA-A-187 이 이 차이로 2.0 dB 근방에서 판정이 갈리는 것을 이미 쟀다.
- 결과적 불일치: 2.5 dB 개선은 `xpe_verify_pipeline`(2.0) 은 통과, `xpe_verify_gain`(3.0) 은 불합격이다. **두 문턱이 왜 다른지는 어디에도 기록돼 있지 않다**(같은 커밋에서 같이 쓰였고 설명 없음).
- 파생 관찰: gain 의 `prnu_improved`(after < before)는 `snr_improved`(≥ 3 dB)가 참이면 항상 참이다(비율 ≥ 1.41 이므로). 코드를 읽고 도출한 것이고 따로 시험하지는 않았다.

### 1.4 완벽한 보정을 불합격시킨다 (`evidence/40`, 실제 DLL)

`xpe_verify_gain` `:573-577` 은 `prnu_before > 0 && prnu_after > 0` 일 때만 dB 를 계산하고 아니면 `0.0` 을 쓴다. 그러면 `snr_improved` 가 거짓이 된다. 실제 `xpe_preprocess.dll` 에 64×64 영상을 넣어 쟀다(게인 맵 전부 1.0, 의미 UNKNOWN, 플랫 잔차 선 1.0%):

| 사례 | `prnu_before` → `prnu_after` | `snr_improvement_db` | `overall_pass` |
|---|---|---|---|
| A 보정 후 **정확히 평탄**(상수) | 4.96% → **0.000000%** | **0.0** | **False** |
| B 5% → 0.5% 잡음 | 4.96% → 0.49% | 20.07 | True |
| C 5% → 0.0001% 잡음(아주 작지만 0 아님) | 4.96% → 0.000102% | 93.72 | True |
| D 5% → 3.5% 잡음(3.1 dB, 1% 선 위) | 4.96% → 3.46% | 3.13 | False |
| E 대조: 균일 → 균일 | 0 → 0 | 0.0 | True |

A 는 개선이 가장 큰 경우인데 불합격이고, C 는 거의 같은 상황인데 통과한다(대조 B·C·E 가 같은 호출 경로에서 정상 동작하므로 프로브 오류가 아니다). 실제 영상에서는 표준편차가 정확히 0 이 될 일이 거의 없어 영향은 합성 픽스처(게인 맵을 정확히 알고 나눈 경우)에 국한된다. 그래도 "개선이 무한대일 때 합격"이어야 하는 자리에서 거짓이다. 이 카드는 보고서만이라 고치지 않았다.

### 1.5 `GAIN_COVERAGE_MIN` (`evidence/25`)

- 코드의 "유효 게인" 정의는 `isfinite(g) && g > 0`(`xpe_verify_metrics.cpp:537`)이다.
- 제품의 정의는 다르다. `SRS-CALIB-FUNC-002`: 값은 `[0.1, 10.0]`, 범위 밖은 `XPE_ERR_INVALID_CALIB_DATA`. SPEC 1.3.3 REQ-P1A-011(`:168`): 범위 밖 화소는 게인 1.0 으로 보정하고 결함 단계에 넘기며, **다항 게인은 프레임의 5% 초과가 범위 밖일 때만** `XPE_ERR_CONFIG_INVALID`.
- 그래서 검증의 "유효"는 제품의 "유효"보다 넓다(게인 0.001 이나 50 도 유효로 센다). 그리고 요구 쪽에 게인 화소 비율 허용치가 **하나** 있다: 5%. 다만 그것은 파이프라인의 입력 거부 기준(다항 경로)이고 `0.99` 는 검증 게이트라 **출처가 아니라 비교할 후보**다.

### 1.6 결론 칸 (검색 범위를 같은 문장에)

- `PRNU_IMPROVE_MIN_DB` 3.0: **근거 없음** — 값·개념 검색(`origin/main` `f9803071`, `docs/`·`.moai/specs`·`.moai/project` 전수, 대조군 통과)과 `git log --all -S`(도입 커밋 1개, 문서 변경 0)에서 전처리 게인 보정의 문턱으로 쓰인 줄이 없다. 값이 같은 기준 둘(P1B-ENH AC-02 등)은 다른 모듈의 다른 양이다.
- `SNR_IMPROVE_MIN_DB` 2.0: **근거 없음** — 같은 범위, 값이 같은 기준 하나(STP UT-2.2-001)는 bilateral 시험.
- `GAIN_COVERAGE_MIN` 0.99: **근거 없음** — 같은 범위. 후보는 SPEC 5%(의미 다름).
- 외부 표준은 원문을 읽지 않았으므로 "표준에도 없다"는 말은 하지 않는다. 확인한 것은 저장소의 인용 줄에 이 문턱이 없다는 것까지다.

### 1.7 결정 재료 (결정은 리더 몫)

- 문턱이 둘인 것은 사실이고 이유는 기록이 없다. 같은 양이므로 하나로 합치거나, 하나를 판정에서 빼 참고값으로 낮출 수 있다.
- 합치기 전에 §1.4 를 알아야 한다: 개선이 `prnu_after = 0` 일 때의 처리가 문턱 값과 별개로 거짓이다.
- 어느 쪽을 택해도 `FUNC-017` 의 절대 기준(`flat_residual_ok`)은 독립이라 영향이 없다.
- 필드 `snr_improvement_db` 는 gain 에서 PRNU 개선, pipeline 에서 SNR 차이를 담는다. 같은 양이지만 이름이 하나뿐이라 문서화 방법(주석·헤더)과 개명(ABI 잠금)의 선택이 남는다.

## 2. #216 — RTM 에만 이름이 있는 7개

SRS 원문은 `evidence/30`·`31`·`32`. SPEC 1.3.3 본문에는 이 7개 중 6개의 이름이 **0줄**이고, `xpe_nonlinearity_correct` 는 요구가 아니라 범위 표의 한 줄(`:1090` "Separate SWU … SPEC-XPE-P1D", 그 문서는 존재하지 않음)뿐이다(`evidence/37`).

### 2.1 함수별

**① `xpe_nonlinearity_correct`** — FUNC-006: "LUT 또는 단조 다항식으로 **게인 보정 전에** 비선형 보정을 적용, `I_lin = f_nonlin(I_raw)`, LUT 256개 이상, 다항 차수 ≤ 5, 검출기 프로파일의 `panel.linear` 가 켜고 끈다". -006-EXT 6a(LUT 조회 `I_lin = LUT[I_raw]`, 4096/65536 항목, 단조 검사)·6b(다항). → **능력은 서술.** 함수 헤더(`preprocess_api.h:817` 근처)가 **스스로** 적고 있다: "No LUT and no coefficients: no-op with an alert, XPE_OK … **No requirement states this sentence**", 그리고 SPEC 은 비선형을 범위 밖(PRE-08 "별도 SPEC", `spec.md:57`, SPEC-XPE-P1D 는 없음)으로 둔다. 요구에 없는 계약: 설정 키(`panel.linear` 외 `panel.nonlin_poly_c0..c4`·`panel.adc_max`), `XPE_ERR_CALIB_NOT_LOADED` 조건, 설정 JSON 오류 시 프레임 불변, 빈 상태의 무동작.

**② `xpe_calib_generate_nonlin_lut`** — -006-EXT 6a: LUT 생성 절차 6단계(선량 N ≥ 10 에서 평탄 영상, `S_meas`·`D_ref`, `S_ideal = G_nominal·D` 적합, `LUT[S_meas] = S_ideal`, Fritsch-Carlson 보간, 경계 조건)와 2026-09-18 정정(상단 항등 고정 폐기, ≤ 0.3% 는 측정 구간 안에서만). → **서술 있음.** 요구에 없는 것: 입력 형식(평탄 프레임·선량 배열), 출력 파일 형식(`XCAL_TYPE_NONLIN_LUT`), 오류 코드. **헤더가 낡았다**: `preprocess_api.h:446-452` 근처가 아직 "boundary conditions `LUT[0] = 0` and `LUT[ADC_max] = ADC_max`" 와 "cannot reach the identity endpoint" 를 적는데, SRS 정정은 상단 항등을 폐기했고 코드(`xpe_calib_generate_nonlin_lut.cpp:263` 주석, `:238`)도 상단 매듭을 만들지 않으며 오류 사유는 "측정 최대값이 `adc_max` 이상"이다.

**③ `xpe_calib_load_nonlin_lut`** — 6a: "단조 검사는 강제(비단조 LUT = `XPE_ERR_INVALID_CALIB_DATA`)", LUT 크기 4096/65536, FUNC-006 "교정 프로파일에 저장". → **일부.** 검증 규칙은 있으나 "파일을 읽어 저장소의 활성 비선형 교정으로 만든다"는 계약, 설정 블록 오류(`XPE_ERR_CONFIG_INVALID`), 확장 경계(`xcal_nonlin_extension_start`) 검사는 요구에 없다.

**④ `xpe_calib_unload_nonlin_lut`** — **서술 없음.** FUNC-038 은 수명 함수로 `xpe_calib_state_release` 만 이름으로 묶고 이 함수는 묶지 않는다. 헤더는 "검출기 프로파일을 바꿀 때 이전 패널의 표가 다른 검출기에 적용되지 않게" 쓰라고 적는다 — 그 이유가 요구에는 없다.

**⑤ `xpe_bpm_generate`** — FUNC-022: 어두운 영상 BPM, 마스크 창 ≥ 32×32, RMM(λ=8.0), 종래 MC 의 256×7 비대칭 마스크를 적응형으로 대체. FUNC-023: 밝은 영상, 창 ≥ 128×128, 허용도 마스크 평균의 5~9%. → **일부.** 서술이 없는 것: 어두운·밝은 결과를 **`max` 로 병합**(값 0/1/2/3), **반사(reflect) 패딩**, 최소 프레임 수, 출력 UINT8 규약. **번호 불일치**: 헤더는 "FUNC-024: BPM Merging", "FUNC-025: Reflect Padding" 이라 적고 RTM(`:244`)도 `FUNC-022..025` 를 이 함수에 매핑하지만, SRS 의 FUNC-024 는 **게인 보정 프레임 수 등급과 단일 프레임 게인 표지**, FUNC-025 는 **BPM 보정 뒤 `LineArtifactScore` < 10%** 다. SRS 에서 병합·반사를 서술한 줄은 없다(`reflect` 는 다른 문장의 동사 하나, `merge` 는 런타임 결함 맵 병합 — `evidence/38`). 또 `LineArtifactScore` 는 `modules/` 에 0건, GUI 클라이언트(`clients/ImageProcTest/Services/MetricsComputationService.cs`)에만 있다.

**⑥ `xpe_verify_defect`** — FUNC-019: "`DefectRecall`·`DefectFPR`·`DefectResidualADU`·`GoodPixelDeltaP99` 를 계산, 합성 BPM 오라클은 100% 재현율·FPR < 0.001% 요구". → **서술은 있으나 이 함수가 하는 일이 아니다.** 함수는 결함 수·밀도·보정 오차를 계산하고 `defect_density < 5%` 로 판정한다(`:693-700`). FUNC-019 는 리더 결정(2026-09-28)대로 시험 쪽 오라클(`test_defect_oracle.cpp`)로 구현됐고 CI 에서 통과한다(QA-A-222). RTM `:242`(`FUNC-019 / REQ-P1A-012` ↔ `xpe_verify_defect`·`xpe_defect_correct`, 증거 "Defect AVX2 parity tests")는 이 함수를 설명하지 않는다. 이 함수에 해당하는 SRS 문장은 `SRS-CALIB-FUNC-036`(측정 가능 여부 구분)과, 5% 의 출처로 QA-A-152 가 찾은 `FUNC-003` 의 "Maximum 5% defect density tolerance"(BPM **적재** 요구의 한 구절)뿐이다.

**⑦ `xpe_verify_pipeline`** — FUNC-015: E2E 보고서 스키마 `xpe-pre-e2e-report-v1` 발행(SHA-256, 단계 시간, 게이트 …). FUNC-021: 보정 효과 점수(CES) 계산. → **서술은 있으나 이 함수가 하는 일이 아니다.** 함수는 `snr_improvement_db` 를 계산하고 `≥ 2.0` 으로 판정한다(`:785-801`). `xpe-pre-e2e-report-v1` 은 `modules/`·`clients/`·`gui/`·`tools/` 에 0건(문서 셋에만), `CES` 는 whole-word 로 `modules/` 의 주석 한 줄에만 있다(`evidence/33`·`34`). 즉 FUNC-015·021 이 말하는 보고서·점수는 이 함수에 구현돼 있지 않다. 이 함수에 해당하는 SRS 문장은 FUNC-036(측정 가능 여부)뿐이다.

### 2.2 요약

| 함수 | SRS 본문 서술 | SPEC 문안 초안 |
|---|---|---|
| `xpe_nonlinearity_correct` | 능력 있음, 계약 일부 없음 | §2.3 D1 |
| `xpe_calib_generate_nonlin_lut` | 있음 | §2.3 D2 |
| `xpe_calib_load_nonlin_lut` | 일부 | §2.3 D3 |
| `xpe_calib_unload_nonlin_lut` | 없음 | 없음 (함수가 하는 일은 헤더에만) |
| `xpe_bpm_generate` | 일부(022·023), 병합·반사 없음 | §2.3 D4 |
| `xpe_verify_defect` | 이 함수에 대한 서술 없음 | 없음 |
| `xpe_verify_pipeline` | 이 함수에 대한 서술 없음 | 없음 |

### 2.3 SPEC 에 옮길 문안 초안 (리더가 옮김)

**전제**: 아래는 **현재 헤더의 계약**을 요구 문체(`**When** … **shall** …`)로 옮긴 초안이다. 코드 경로의 판정 순서는 이 카드에서 읽지 않았고(`xpe_nonlinearity_correct` 의 D1 두 조건은 헤더 그대로 옮기되 우선순위는 미확인), 비선형 보정이 SPEC 범위 안인지(PRE-08 "별도 SPEC")는 리더 결정이다. 번호는 자리표시자다.

- **D1** `REQ-P1A-1xx`: **When** `xpe_nonlinearity_correct(img, configJsonOrNull)` is called with a non-NULL UINT16 `img`, the module **shall** linearize the frame with the loaded nonlinearity LUT (SRS-CALIB-FUNC-006-EXT 6a) or, from the configured polynomial coefficients, with the polynomial (6b); **when** the configuration text is not one valid JSON object or gives a top-level key twice, it **shall** return `XPE_ERR_CONFIG_INVALID` and leave the frame untouched; **when** the panel is declared non-linear and no LUT is loaded it **shall** return `XPE_ERR_CALIB_NOT_LOADED`; **when** neither a LUT nor coefficients are present it **shall** leave the frame unchanged, post an alert and return `XPE_OK`; a NULL `img` **shall** return `XPE_ERR_INVALID_INPUT`. (The last-but-one clause is the sentence the function header says no requirement states.)
- **D2** `REQ-P1A-1xx`: **When** `xpe_calib_generate_nonlin_lut(flat_frames, dose_levels, num_levels, dark_reference, lut_entries, output_path, metadata_json)` is called with at least 10 strictly increasing dose levels and `lut_entries` of 4096 or 65536, the module **shall** write an `XCAL_TYPE_NONLIN_LUT` file whose entries follow SRS-CALIB-FUNC-006-EXT 6a as corrected on 2026-09-18 (`LUT[0] = 0`; no upper identity pin; the fit line extended above the highest measured level, the start of the extension recorded in the file); it **shall** return `XPE_ERR_INVALID_INPUT` for a NULL argument, fewer than 10 levels, an unsupported entry count or non-increasing doses, `XPE_ERR_INVALID_CALIB_DATA` when the measured response is not strictly increasing or its largest value reaches `adc_max`, and `XPE_ERR_IO_FAILED` on a write failure.
- **D3** `REQ-P1A-1xx`: **When** `xpe_calib_load_nonlin_lut(filepath)` is called with a `.xcal` LUT file, the module **shall** make that table the active nonlinearity calibration used by the pipeline's nonlinearity stage; it **shall** return `XPE_ERR_INVALID_CALIB_DATA` when the entry count is not 4096 or 65536, the table is not non-decreasing or the recorded extension boundary lies outside the table, `XPE_ERR_CONFIG_INVALID` when the file's configuration block is not one valid JSON object or repeats `xcal_nonlin_extension_start` at the top level, and `XPE_ERR_INVALID_INPUT` for a NULL path.
- **D4** `REQ-P1A-1xx`: **When** `xpe_bpm_generate(dark_frames, num_dark, bright_frames, num_bright, cfg, bpm_out)` is called, the module **shall** detect dark defects per SRS-CALIB-FUNC-022 (window ≥ 32×32, RMM λ = 8.0) and bright defects per SRS-CALIB-FUNC-023 (window ≥ 128×128, tolerance 5–9% of the window mean), merge the two maps by taking the larger value per pixel (0 good, 1 dead/stuck, 2 hot/noisy, 3 both) and extend windows at the image border by reflection. **The merge rule and the reflection are not in the SRS** (the function header labels them "FUNC-024"/"FUNC-025", which the SRS defines as other requirements): they would be new requirement text, not a transcription.

(D2 의 "reaches adc_max" 와 D3 의 확장 경계는 코드 `:238` 과 헤더에서 옮겼고 이 카드에서 시험으로 확인하지 않았다.)

### 2.4 그 밖에 나온 것 (리더가 카드로 만들지 판단)

- **헤더 낡음 둘**: `xpe_calib_generate_nonlin_lut`(상단 항등 문구, §2.1 ②), `xpe_bpm_generate`(FUNC-024/025 라벨, §2.1 ⑤).
- **RTM 매핑 셋**: `:242` `xpe_verify_defect` ↔ FUNC-019, `:243` `xpe_verify_pipeline` ↔ FUNC-015/021, `:244` `xpe_bpm_generate` ↔ FUNC-022..025(024 는 게인). 앞의 둘은 매핑된 요구가 함수가 하는 일과 다르다. 이는 #216 이 말한 "이름은 있으나 서술은 있나" 의 답이다.
- **코드 주석**: `xpe_verify_metrics.cpp:97-99` 가 "compared against snr_improvement_db (:348), not PRNU" 라 적는데 §1.3 에 따르면 gain 함수 안에서 그 값은 PRNU 개선이다(줄 번호 `:348` 도 지금은 `:583`).

## 3. 이 결과가 #216 에 미치는 것

RTM 줄에만 이름이 있던 7개 가운데, **SRS 본문이 그 함수의 일을 서술하는 것**은 비선형 셋과 `xpe_bpm_generate`(일부)다. **서술이 없는 것**은 `xpe_calib_unload_nonlin_lut` 하나이고, `xpe_verify_defect`·`xpe_verify_pipeline` 은 매핑된 요구가 **다른 일**을 서술한다. 이름 축의 "요구가 전혀 없는 함수 0개"는 맞지만, 위 셋에 대해서는 "요구 줄에 이름이 있다"가 "그 함수를 요구가 서술한다"가 아님이 이제 확인됐다. 이 구분이 닫을지 말지의 기준이다.

## 4. 이 카드가 건드리지 않은 것

코드·헤더·SPEC·SRS·RTM·이슈. 발견한 헤더 낡음·RTM 매핑·§1.4 동작은 보고만 한다.

## 5. 미검증 (Gaps)

- 두 함수의 `snr_improvement_db` 가 같은 양이라는 것은 **같은 추정기로 두 식을 재구현해 확인**했다(`evidence/24`). 실제 두 함수에 같은 영상을 넣어 비교한 것이 아니다. 추정기 차이(산술평균 대 중앙값, ROI)가 값에 주는 크기도 이 카드에서 재지 않았다.
- §1.4 는 프로브 한 번(5개 입력, 64×64, 시드 하나)이다. `prnu_after > 0` 분기가 원인이라는 것은 코드를 읽어 도출했고, 그 분기를 바꿨을 때 결과가 바뀌는지는 시험하지 않았다(코드 변경 없음).
- 문서 검색은 `git grep` 이고 PDF·이미지·바이너리는 보지 않는다. 외부 표준은 읽지 않았다.
- D1 의 두 조건(`CALIB_NOT_LOADED`, 무동작 `XPE_OK`)의 우선순위, D2·D3 의 오류 조건은 헤더와 `:238` 줄에서 옮긴 것이고 구현 전체를 읽지 않았다.
- `xpe_verify_pipeline` 에서 `std_raw = 0` 일 때 `mean/std` 가 0 으로 나눠지는 처리는 보지 않았다(요청 범위 밖).

## 6. 잔여 위험

- §1.3 은 두 함수가 **같은 양**을 잰다고 말하지만 추정기가 달라 **같은 값**을 내지는 않는다. 문턱을 합칠 때 이 구분을 놓치면 한쪽 판정이 조용히 바뀐다.
