# QA-B-58 게이트 보고서 — 모델이 답에 도달하는지 전수: 의존성을 단언한다

**카드**: QA-B-58 (#155 #154 #142)
**레인**: Lane B (`xpe-post`, `dev/postprocess`)
**증거 경로**: `.moai/reports/lane-post/QA-B-58/`
**커밋 1건**: `e9a13f7` — **제품 코드 변경 0**
**선행**: `git merge origin/main` 완료 (B-57 병합 `1d7e5af`)

---

## 0. 단언의 모양을 바꿨다

#154·#155 를 통과시킨 것은 테스트의 **모양**이었다. 단조성·범위·"곡선이 이렇게 생겼다" 는
모델이 상쇄돼도 그대로 통과한다 — **특성화 테스트는 사라진 입력이 만들었어야 할 출력을 박아
두므로, 그 입력이 무의미해져도 똑같이 단단하게 통과한다.**

이 카드가 쓰는 단언: **하나만 바꾸고 나머지를 고정한 뒤 출력이 움직이기를 요구한다.**
임계값은 `> 0` 이 아니라 **1e-4** 다 — 1 ulp 차이는 파라미터가 답이 아니라 **반올림**에
도달했다는 뜻이고, §2.1 이 정확히 그 함정이다.

---

## 1. 후보 전수

카드가 준 세 신호로 골랐다. 우선순위는 **출력이 임상적으로 읽히는 것부터**.

| # | 지점 | 후보인 이유 |
|---|---|---|
| 1 | `display/voi_lut.cpp:36-54` | 모드 선택이 `switch` 로 갈리는데 세 분기가 같은 입력을 쓴다 |
| 2 | `display/presentation_lut.cpp:131-146` | **#155 확정분** — 모델이 분자·분모에 동시 등장 |
| 3 | `display/modality_lut.cpp` | slope·intercept 가 한 식에 함께 |
| 4 | `enhance_basic/contrast_enhance.cpp:69` | `clip_limit` 이 `tile_area / NUM_BINS` 와 곱해져 정수로 잘린다 — 소실 가능 |
| 5 | `enhance_basic/noise_reduce.cpp:17` | 주석이 "separable **approximation**" |
| 6 | `enhance_basic/edge_enhance.cpp` | `threshold` 가 조건 분기에만 쓰이면 무반응 가능 |
| 7 | `enhance_advanced/exposure_index.cpp:145,165` | 주석이 "**simplified** model" ×2 — #154 의 이웃 |
| 8 | `enhance_advanced/collimation_detect.cpp:177,204` | 두 폴백이 **전체 범위**를 돌려준다 — 입력 무반응처럼 보일 수 있다 |

---

## 2. 측정 결과 — 19개 파라미터

### 2.1 display (`_disp.log`, BUILD=0)

| 함수 | 파라미터 | maxdiff | 판정 |
|---|---|---|---|
| `xpe_apply_modality_lut` | `rescaleSlope` 1→2 | **1000** | 도달 |
| | `rescaleIntercept` 0→100 | **100** | 도달 |
| `xpe_apply_voi_lut` | `center` 500→700 | **0.2** | 도달 |
| | `width` 1000→400 | **0.2996** | 도달 |
| | `minOut/maxOut` 1→255 | **254** | 도달 |
| | `XPE_VOI_SIGMOID` | **0.1189** | 도달 |
| | **`XPE_VOI_LINEAR_EXACT`** | **5.96e-08** | **미도달 — §2.2** |
| `xpe_voi_preset_create` | `bodyPart` | BONE 500/2000, LUNG −600/1600, HEAD 40/80 | 도달 |
| `xpe_apply_presentation_lut` | `lutData` 반전 | **1024/1024 화소** | 도달 |
| `xpe_gsdf_calibrate` | `luminanceValues` | **최대 1 (반올림)** | **미도달 — #155 기확정** |

### 2.2 새 발견 — `XPE_VOI_LINEAR_EXACT` 가 `XPE_VOI_LINEAR` 과 같다

**세 번째 인스턴스이고 가장 명확하다.** 모드를 읽고 `switch` 가 갈라지는데 두 분기가 같은
식을 계산한다:

```
LINEAR (voi_lut.cpp:38-41)   lo = center - width*0.5
                             (x - lo)/width*range   ==   ((x - center)/width + 0.5)*range
EXACT  (voi_lut.cpp:48-50)   ((x - center)/width + 0.5)*range
```

대수적으로 동일하다. 측정 차이 **5.96e-08** 은 [0,1] 출력에서 **1 ulp** — 부동소수 결합
순서 차이뿐이다.

**REQ-DISP-010 은 정반대를 요구한다**: "the full window maps exactly from minOut to maxOut
**without the half-value offset**". 그 오프셋(`+ 0.5`)이 **양쪽 분기에 다 있다.**

부수: EXACT 분기의 주석이 **REQ-DISP-011**(sigmoid)을 인용한다 — 실제로는 REQ-DISP-010 이다.
B-57 에서 본 것과 같은 오인용. **기록만 하고 고치지 않았다.**

### 2.3 enhance_basic (`_eb.log`, BUILD=0)

| 함수 | 파라미터 | maxdiff | 판정 |
|---|---|---|---|
| `xpe_log_transform` | `normFactor` 1000→4000 | **9479.6** | 도달 |
| `xpe_log_inverse` | `normFactor` 1000→4000 | **6.09e-4** | 도달 |
| `xpe_noise_reduce` | `sigma_space` 3→8 | **25.68** | 도달 |
| | `sigma_range` 50→200 | **39.24** | 도달 |
| | `mode` BILATERAL→NLM | **72.00** | 도달 |
| | `search_window` 21→7 | **2.118** | 도달 |
| | `patch_size` 7→3 | **5.752** | 도달 |
| | `h_param` 10→40 | **91.41** | 도달 |
| `xpe_contrast_enhance` | `clip_limit` 3→40 | **65.92** | 도달 (§3.1) |
| | 타일 8×8→2×2 | **14.65** | 도달 |
| `xpe_edge_enhance` | `amount` 0.5→2 | **15** | 도달 |
| | `radius` 2→6 | **10** | 도달 |
| | `threshold` 10→300 | **5.0** | 도달 |

### 2.4 enhance_advanced (`_ea.log`, BUILD=0)

| 함수 | 파라미터 | 결과 | 판정 |
|---|---|---|---|
| `xpe_fractional_process` | `order` 0.3→0.9 | **637.96** | 도달 |
| `xpe_multiscale_process` | `edge_gain` | **329.1** | 도달 |
| | `flat_gain` | **117.6** | 도달 |
| | `levels` | **150.8** | 도달 |
| `xpe_adv_calc_exposure_index` | `kVp` 80→120 | EI 76548.7 → **172234** | 도달 |
| | `mAs` 10→40 | EI 76548.7 → **306195** | 도달 |
| | `bodyPart` → **EI** | CHEST 76548.7 = SKULL 76548.7 | **무반응 — 이것이 옳다** |
| | `bodyPart` → **DI** | CHEST 24.86 / SKULL **21.85** | **도달 — 이것도 옳다** |
| `xpe_detect_collimation` | 영상 | `[0,0,255,255]` vs **`[48,47,207,207]`** | 도달 (§3.2) |

**advanced 경로에는 #154 결함이 없다.** EI 는 목표에 무반응이고 DI 는 부위에 반응한다 —
basic 과 정반대이고, IEC 가 의도한 방향이다. #154 는 basic 쪽 요구의 문제이지 두 모듈
공통의 문제가 아니라는 것이 여기서 실측으로 확인된다.

---

## 3. 픽스처가 만든 가짜 발견 2건 — 스스로 걸렀다

**두 건 모두 처음에는 "파라미터가 도달하지 않는다" 로 보였다.** 올렸다면 둘 다
**픽스처가 만든 결함 주장**이었을 것이다. B-52 의 대조군 교훈이 반대편에서 온 형태다 —
거기서는 실행되지 않은 가드에 거절을 credit 할 뻔했고, 여기서는 작동할 기회를 못 얻은
코드에 무반응을 credit 할 뻔했다.

### 3.1 CLAHE `clip_limit` — 처음 측정 **maxdiff 0**

`clip_count = clip_limit × tile_area / NUM_BINS` 이고 **`NUM_BINS = 4096`**
(`contrast_enhance.cpp:14, :69`). 공용 64×64 픽스처에 8×8 격자면 타일이 64화소, 4096빈 →
`clip_count` 가 `clip_limit` 3 이든 10 이든 **0.75/2.5 → 1 로 클램프**되고, 퍼져 있는
픽스처에서는 어떤 빈도 그 값에 닿지 않는다. **자를 것이 없으니 차이가 0이다.**

256×256(타일 32×32=1024화소)에 값을 몇 개로 몰아 넣어 히스토그램 봉우리를 높이니
**maxdiff 65.92** 로 움직인다. `clip_limit` 은 도달한다.

### 3.2 `xpe_detect_collimation` — 처음 측정 **두 입력 같은 박스**

64×64에 8px 테두리로 쟀더니 둘 다 `[0,0,63,63]`. 로그를 보면 이유가 적혀 있다:

```
[warning] ROI area ratio (0.014) below minimum (0.050). Using full-image extent.
```

**저신뢰·최소면적 미달 시 전체 범위를 돌려주는 문서화된 폴백**(`:177-192, :204-210`) 아래였다.
256×256에 48px 테두리를 주니 `[0,0,255,255]` 대 **`[48,47,207,207]`** — 테두리를 거의 정확히
찾아낸다.

**"박스가 안 움직였다" 는 픽스처가 검출기 자신의 바닥을 넘었을 때만 발견이다.**

---

## 4. 반증 (`_falsify.log`)

VOI 의 center 영향을 `center * 1e-6f` 로 **약화**(삭제 아님 — 변수는 계속 쓰이므로 `/WX`
가 깨지지 않는다):

```
===BUILD=0===
voi center maxdiff=2.98e-07 width=0.2999 range=254 sigmoid=0.5
center does not reach the output
[  FAILED  ] ParameterDependency.VoiLut_EveryParameterReachesTheOutput
[  FAILED  ] ParameterDependency.KnownDivergence_VoiLinearExactEqualsVoiLinear
```

**임계값을 1e-4 로 둔 이유가 여기서 보인다** — 약화된 center 도 2.98e-07 만큼은 움직인다.
`> 0` 이었다면 **통과했을 것이고**, 그것이 §2.2 를 놓치는 방식이다. 두 번째 실패는 예상된
것이다(EXACT 기록은 두 분기가 같다는 전제 위에 서 있고, 한쪽만 약화하면 깨진다).
반증 뒤 원복했다.

---

## 5. Baseline 귀속 (Baseline-attribution)

| 항목 | 값 | 근거 |
|---|---|---|
| 측정 파라미터 | **19개** (display 10 · basic 13 항목 · advanced 9) | §2 |
| 미도달 | **2건** — `LINEAR_EXACT`(신규), GSDF 광도(#155 기확정) | §2.1 |
| 가짜 발견 | **2건 자체 기각** | §3 |
| 반증 | BUILD=0, 약화한 파라미터만 실패 | `_falsify.log` |
| 이전 ctest | 481 / 211 / 173 | QA-B-57 `_verify.log` |
| 현재 ctest | **498 / 211 / 173** (신규 17건) | `_verify.log` |
| 빌드 경고 | 0 | `_verify.log` |
| 제품 코드 변경 | **0** | `git status` = 테스트 3개 + CMakeLists 3개 |

---

## 6. 미검증 (Gaps)

- **ai·gsvg·dicom 은 이 사전에서 제외했다.** 카드가 "출력이 임상적으로 읽히는 것부터" 라
  했고 LUT·지표·보정 계수까지 갔다. ai 는 stub 이라 파라미터 의존성 자체가 의미가 다르고,
  gsvg 의 config(`vignette_correction`·`grid_suppression`)는 **미측정**이다.
- **`xpe_noise_estimate_sigma`·`xpe_calc_exposure_index`(basic)는 파라미터가 없거나
  #154 로 이미 다뤄져 제외했다.**
- **`texture_gain`·`noise_threshold`(multiscale)는 측정하지 않았다** — `edge_gain`·
  `flat_gain`·`levels` 로 config 경로가 도달함을 보였고 나머지는 같은 파서를 지난다는
  **판독**이다.
- **도달한다는 것이 올바르다는 뜻은 아니다.** 이 사전은 "영향이 있는가" 만 본다.
  값이 요구의 식과 맞는지는 별개이고, #154 가 정확히 그 구분에서 나왔다.
- **`XPE_VOI_LINEAR_EXACT` 가 DICOM PS3.3 C.11.2.1.3 을 만족하는지 판정하지 않았다** —
  표준 원문이 저장소에 없다. 측정된 것은 "두 모드가 같은 값을 낸다" 와
  "REQ-DISP-010 의 문구와 코드가 어긋난다" 까지다.

---

## 7. 잔여 위험 (Residual-risk)

- **`LINEAR_EXACT` 를 고르는 호출자는 자기가 고른 것을 받지 못한다.** DICOM 뷰어가
  PS3.3 의 정확 선형을 요청하면 half-value 오프셋이 든 결과를 받는다. 폭이 좁을수록
  차이가 커지는 종류의 어긋남이다.
- **이 사전은 한 번의 관측이다.** 파라미터가 특정 입력 영역에서만 영향을 갖는 경우
  (§3 의 두 건이 그 형태였다) 다른 픽스처에서는 다르게 나올 수 있다.
- **임계값 1e-4 는 판단이다.** 출력 스케일이 다른 함수에 같은 절대값을 쓰고 있고,
  아주 미세하지만 실재하는 의존성을 "미도달" 로 읽을 위험이 남는다.
- **기록 테스트는 위험을 없애지 않는다** — 표류를 막을 뿐이다.
- 커밋은 push 전까지 미푸시 유일본이다.

---

## 부록 — 증거 파일

| 파일 | 내용 |
|---|---|
| `_env.bat` / `_verify.bat` | 환경 + 세 프리셋 재빌드·전체 ctest |
| `_b58.bat` / `_disp.log` | display 6건 |
| `_b58eb.bat` / `_eb.log` | enhance_basic 6건 (CLAHE 픽스처 정정 포함) |
| `_b58ea.bat` / `_ea.log` | enhance_advanced 5건 (collimation 픽스처 정정 포함) |
| `_falsify.log` | VOI center 약화 — BUILD=0, 해당 단언만 실패 |
| `_verify.log` | 최종 498 / 211 / 173, 경고 0 |
