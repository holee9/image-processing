# QA-B-62 게이트 보고서 — 읽히고 버려지는 config 값

**카드**: QA-B-62 (#145) · **레인**: Lane B (`xpe-post`, `dev/postprocess`)
**커밋 1건**: `3c9228f` · **증거**: `.moai/reports/lane-post/QA-B-62/`
**전수표**: 같은 디렉터리의 `census.md` (정적 census, 이 보고서의 §1 이 그 결론)

---

## 1. 주장 (Claim)

**`xpe_fractional_process` 의 `step_size` 는 읽히고 클램프된 뒤 버려진다.**
그리고 **B-61 의 경고는 이것에 대해 침묵한다** — 알려진 키이기 때문이다.
B-60·B-61 두 보고서가 연속으로 적은 잔여 위험("경고가 없다 ≠ 설정이 적용됐다")의 실물이다.

부수로 두 가지가 더 나왔고, 셋 다 **파싱·클램프는 정상이고 출력만 안 움직인다.**

| # | 발견 | 경고가 잡나 |
|---|---|---|
| **A** | `step_size` — 담길 필드가 `FractionalConfig` 에 **없다**(멤버는 `float order` 하나). 로그 문자열에서 끝난다 | **아니오** (알려진 키) |
| **B** | `levels`(레거시)가 `num_levels` **뒤에** 같은 출력 변수에 기록 → 둘 다 주면 `num_levels` 가 조용히 사라진다 | **아니오** (둘 다 알려진 키) |
| **C** | 레벨별 게인 분기가 `num_levels` 에 의존 → 3 이면 `texture_gain`, 2 면 `edge_gain`·`texture_gain` **둘 다** 무효 | **아니오** |

**ai 쪽 4건은 정적 사실로만 적는다**(§4) — stub 경계 때문에 런타임 무반응과 섞지 않는다.

---

## 2. 증거 (Evidence)

### 2.1 GREEN — 7/7, BUILD=0 (`_green.log`)

```
===BUILD=0===
[  INFO ] multiscale edge_gain        maxdiff=150.909668 (threshold 0.080585)
[  INFO ] multiscale texture_gain     maxdiff=56.201782  (threshold 0.080585)
[  INFO ] multiscale flat_gain        maxdiff=42.214478  (threshold 0.080585)
[  INFO ] multiscale noise_threshold  maxdiff=102.016541 (threshold 0.080585)
[  INFO ] multiscale num_levels       maxdiff=5.628662   (threshold 0.080585)
[  INFO ] gain reach by level count: tex@3=0.000000 edge@2=0.000000 tex@2=0.000000
[  INFO ] fractional iterations maxdiff=494.171753 (threshold 0.080585)
[  INFO ] fractional step_size 0.01 vs 1.0 maxdiff=0.000000000 -- read, clamped, discarded
[  INFO ] collimation base=[48,47,207,207] margin16(below edge)=[48,47,207,207]
          margin64(above edge)=[64,64,191,191] gated=[0,0,255,255]
[  INFO ] collimation sensitivity 0.0=[48,47,207,207] 1.0=[48,47,207,207]
7 tests from 1 test suite ran. ===EXIT=0===
```

**`step_size` 는 0.01 과 1.0 에서 비트 동일**이다 — 임계값 미만이 아니라 `maxdiff == 0`.
단언도 그렇게 적었다(`EXPECT_EQ(d, 0.0f)`): "작게 움직인다" 와 "안 움직인다" 는 다른 주장이다.

### 2.2 임계값 — `> 0` 이 아닌 이유와 정한 방법

#156 이 함정이다: 1 ulp(단위 스케일에서 5.96e-08)가 `> 0` 이면 "도달함" 으로 읽힌다.
B-58 은 절대 1e-4 를 썼고, **출력 스케일이 다르면 그 절대값이 안 맞는다는 것을 스스로
Gap 으로 적었다.** 그래서 여기서는 **기준선의 동적 범위에 대한 상대값**으로 잡았다:

```
moveThreshold = 1e-4 x (max - min)(기준선 출력)   →  이 픽스처에서 0.080585
```

기준선 범위 ~806 에서 float 반올림은 ~1e-4 수준이므로 **임계값이 반올림보다 약 3~4 자릿수
위**다. 양방향 오판이 불가능하다. **collimation 은 예외** — 정수 픽셀 좌표를 반환하므로
반올림 띠가 없고, 한 픽셀 차이가 곧 실제 변화다.

### 2.3 픽스처 바닥 확인

| 대상 | 바닥 | 넘겼다는 증거 |
|---|---|---|
| multiscale | 여러 스케일의 디테일이 있어야 레벨별 게인이 곱할 것이 생긴다 | 256×256 에 램프+계단+리플. `num_levels=4` 에서 게인 3개 전부 출력을 움직인다(§2.1) |
| collimation | ROI 면적비가 최소치를 넘어야 검출 경로로 간다 | `base=[48,47,207,207]` ≠ 전체 범위. 테스트가 `ASSERT_FALSE(base == full)` 로 **먼저 막는다** |
| fractional | 기울기가 있어야 미분이 반응 | `iterations` maxdiff 494.17 |

### 2.4 픽스처 함정에 **세 번째로 걸렸고, 측정이 아니라 코드를 읽어 잡았다**

`border_margin=16` 으로 두고 무반응(`[48,47,207,207]` 동일)을 관측했다. **이것을 발견으로
보고할 뻔했다.** 코드를 읽으니 `border_margin` 은 인셋이 아니라 좌표 **클램프**다:

```cpp
*x0Out = std::max(borderMargin, rect.x0);          // collimation_detect.cpp:198
*x1Out = std::min(width - 1 - borderMargin, rect.x1);
```

검출 경계가 48 인데 margin 16 이면 `max(16,48) = 48` — **설계상 무효**다. 없는 결함을
보고할 뻔했다. 64 로 바꾸니 `[64,64,191,191]` 로 움직인다. 두 값을 **모두** 단언으로 박았다:
경계 미만은 무변, 경계 초과는 변화. 경위는 테스트 파일에 남겼다.

B-58 이 두 번, 여기서 세 번째다. 이번에 다른 점은 **로그가 아니라 코드가 답을 줬다는 것**이다.

### 2.5 무반응의 도달 증거 — 두 가지 방식

무반응을 발견으로 적으려면 그 값이 코드에 닿았다는 증거가 필요하다. 두 가지를 썼다:

1. **같은 키가 이웃 설정에서는 움직인다.** `edge_gain`·`texture_gain` 은 `num_levels=4`
   에서 출력을 움직이므로(150.91 / 56.20), 2·3 에서의 무반응은 파싱 실패가 아니라 분기다.
   `num_levels` 도 단독으로는 움직이므로, `levels` 가 이긴다는 주장이 공허하지 않다.
2. **B-61 의 미지 키 경고가 그 이름에 대해 침묵한다.** 모르는 키였다면 경고가 이름을
   부른다. 침묵은 **파서가 이름을 안다는 양성 증거**다. `step_size` 에 이것을 썼다
   (`ParserRecognises()`), 더해 config 가 `XPE_ERR_CONFIG_INVALID` 가 아니라 `XPE_OK` 를
   받는 것도 함께 단언했다.

### 2.6 단언 diff — 특성화가 아니라 의존성

이 파일은 **출력값을 하나도 박지 않는다.** 전부 "하나만 바꾸고 나머지 고정 → 움직여라"
또는 "→ 안 움직인다(+도달 증거)" 형태다. 현재 출력을 박는 특성화 테스트는 입력이
무의미해져도 똑같이 통과하므로, 찾으려는 결함을 구조적으로 못 본다.

### 2.7 반증 2건 — 둘 다 BUILD=0

**반증 1 — `step_size` 를 실제로 소비하게 한다** (`_falsify_stepsize.log`).
최소 배선: `FractionalConfig` 에 `stepSize` 필드 추가 → 호출부에서 채움 → `gain` 에 곱함.

```
===BUILD=0===
[  INFO ] fractional step_size 0.01 vs 1.0 maxdiff=287.940002441
[  FAILED  ] ConfigValueDependency.KnownDivergence_FractionalStepSizeIsReadAndDiscarded
[       OK ] 나머지 6건
1 FAILED TEST
```

**해당 단언 하나만 실패한다** — 단언이 이 키를 특정해서 보고 있다는 증거다. 원복했다.

**반증 2 — 움직임 단언이 실제로 일하는지** (`_falsify_disable.log`).
`parse_mfp_config` 의 `edge_gain` 읽기를 `if (false && ...)` 로 무력화:

```
===BUILD=0===
[  INFO ] multiscale edge_gain maxdiff=0.000000 (threshold 0.080585)
[  FAILED  ] ConfigValueDependency.MultiscaleGainsAndThresholdAllMoveTheOutput
[  FAILED  ] ConfigValueDependency.KnownDivergence_LowLevelCountSilencesGains
2 FAILED TESTS
```

두 번째 실패가 설계의 핵심이다 — **도달 증거 가드가 함께 무너졌다.** "2레벨에서
`edge_gain` 이 안 움직인다" 는 주장은 "4레벨에서는 움직인다" 를 전제하고, 그 전제가
사라지면 **주장이 스스로 무효화된다.** 무반응 보고가 조용히 살아남지 못하게 하는 장치다.
원복했다.

---

## 3. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| config 를 받는 진입점 | **9개** (헤더 기계 census) | `census.md` §0 |
| 이번에 런타임 단언한 키 | **9개** (mfp 6 · fractional 2 · collimation 3, 중복 제외) | `_green.log` |
| 신규 테스트 | **7건**, BUILD=0 | `_green.log` |
| `step_size` 0.01 vs 1.0 | **maxdiff 0.000000000** (비트 동일) | `_green.log` |
| 임계값 | **0.080585** = 1e-4 × 기준선 동적 범위 | `_green.log` (런타임 산출) |
| 반증 1 (step_size 배선) | BUILD=0, **해당 1건만 FAILED**, maxdiff 287.94 | `_falsify_stepsize.log` |
| 반증 2 (edge_gain 무력화) | BUILD=0, **2건 FAILED**(도달 가드 포함) | `_falsify_disable.log` |
| 이전 ctest | 522 / 222 / 177 | QA-B-61 `_verify.log` |
| 현재 ctest | **529 / 222 / 177** (신규 7건) | `_verify.log` |
| 빌드 경고 | 0 | `_verify.log` |

---

## 4. 미검증 (Gaps)

- **ai 의 4개 키는 런타임으로 측정하지 않았다.** `confidence_threshold`·`fallback_mode` 는
  **리포지토리 전체에 읽는 코드가 0곳**이고 getter 도 없는 쓰기 전용 필드이며,
  `execution_provider`·`timeout_ms` 는 init 로그 줄에만 도달한다 — 이것은 **코드 사실이지
  런타임 관측이 아니다.** ai 추론 경로가 stub 이므로 "출력이 안 움직인다" 를 측정해도
  stub 조기 반환과 구별되지 않는다. **둘을 섞지 않으려고 측정하지 않았다.**
- **ai per-call 3개**(`xpe_stitch_images`·`xpe_bone_suppress`·`xpe_dl_denoise`)는 config 를
  `(void)` 로 버린다. 파싱 자체가 없어 B-61 경고도 없다. 같은 이유로 미측정.
- **gsvg 2개 키는 이번 전수에 넣지 않았다.** `vignette_correction` 은 `gainMap != nullptr`
  일 때만 소비되는 조건부 경로다(B-59 가 짚은 기본값 함정과 같은 계열). 별도 모듈 타깃이라
  다음 카드 대상으로 남긴다.
- **collimation `sensitivity` 는 단언하지 않고 기록만 했다.** 0.0 과 1.0 이 같은 정수 ROI 를
  냈는데(`[48,47,207,207]`), 이것이 픽스처의 에지가 너무 강해서인지 실제 무반응인지
  가르지 못했다. **픽스처 의존 단언을 박는 것이 B-58 함정의 반대 방향**이라 측정만 남겼다.
- **`num_levels` 의 maxdiff 5.63 은 다른 키들보다 한 자릿수 작다.** 임계값(0.0806)보다는
  두 자릿수 위라 판정은 안전하지만, 왜 작은지는 보지 않았다.
- **죽은 파서 2개**(`MfpConfig::fromJson`·`FractionalConfig::fromJson`, 호출처 0곳)는
  살아 있는 파서와 **클램프 범위가 다르다**(`[1,6]` vs `XPE_MFP_MIN/MAX_LEVELS`).
  리더가 따로 짚기로 했고 이 카드에서는 손대지 않았다.

---

## 5. 잔여 위험 (Residual-risk)

- **전수는 "소비되는 키" 에만 걸려 있다.** 파서가 아예 이름을 모르는 키는 B-61 경고가
  보고, 이 단언들이 보는 것은 "이름은 아는데 값이 안 닿는" 쪽이다. **두 장치가 서로 다른
  것을 본다.** 어느 쪽에도 안 걸리는 세 번째 형태(예: 타입 불일치로 조용히 기본값)는
  카드 범위 밖이고 여전히 열려 있다.
- **B·C 는 결함이 아니라 기록이다.** `levels` 우선순위와 레벨별 게인 붕괴는 현재 동작을
  `KnownDivergence_` 로 고정한 것이고, 바꾸면 출력 화소가 바뀌므로 #154·#155·#156 선례대로
  별도 결정이다. **이 테스트가 통과한다는 것이 이 동작이 옳다는 뜻은 아니다.**
- **임계값이 픽스처에서 산출된다.** 픽스처를 바꾸면 임계값도 바뀐다 — 의도이지만,
  누가 픽스처를 평평하게 만들면 `ASSERT_GT(moveThreshold_, 0)` 가 막는 것 외에는
  임계값이 조용히 작아진다.
- **`num_levels=2·3` 의 게인 붕괴는 임상적으로 중요할 수 있다.** 레벨 수를 줄인 호출자는
  게인 두 개를 설정했다고 믿지만 하나만 적용된다. 경고도 없다. 고치지 않았다.
- 커밋은 push 전까지 미푸시 유일본이다.

---

## 6. 부수 정정 — QA-B-61 요구 번호

`REQ-ADV-032` 는 NaN/Inf 요구이고, B-61 이 인용한 문구는 **`REQ-ADV-090` (Thread Safety)**
였다. 3곳을 고쳤다(B-61 `gate.md`, `enhance_advanced_helpers.cpp`, `test_config_warning_once.cpp`).

**번호만 고치면 이웃에 옛 문구가 남아 갓 검증된 것처럼 읽힌다.** 두 주석이 인용하던
개정 전 문구("No global mutable state shall be modified")도 함께 갱신해, 개정 사실
(`d52a5da`)과 좁은 허용 범위(뮤텍스로 보호된 모듈 전역은 해당 없음)를 인용하게 했다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `census.md` | 정적 전수표 — 9개 진입점, 키별 소비처, 후보 A~E |
| `_env.bat` / `_verify.bat` | 환경 + 세 프리셋 재빌드·전체 ctest |
| `_b62.bat` / `_green.log` | 7건 BUILD=0 |
| `_falsify_stepsize.log` | step_size 최소 배선 → 해당 1건만 FAILED |
| `_falsify_disable.log` | edge_gain 무력화 → 움직임 단언 + **도달 가드** FAILED |
| `_verify.log` | 최종 529 / 222 / 177, 경고 0 |
